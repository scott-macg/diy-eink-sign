import machine
import time
import math
import sys
import network
import socket
import json

# ============================================================================
# Hardware & Configuration
# ============================================================================
ADC_PIN = 0               # A0 / GPIO 0 (Xiao D0 - Battery Sense)
BUZZER_PIN = 16           # D6 / GPIO 16 (Audio NPN Driver Pin)
LED_PIN = 15              # Onboard User LED (Xiao ESP32-C6 GPIO 15)
BAT_DIVIDER_RATIO = 2.0   # 1:2 resistor divider (2x 100k)

FULL_CHARGE_VOLTAGE = 4.14 # Voltage threshold to consider battery full
CUTOFF_VOLTAGE = 3.20      # Safe low-voltage shutdown threshold (Volts)
SAMPLE_INTERVAL_SEC = 30   # Take a sample every 30 seconds
LOG_FILE = "battery_curve.csv"
CREDENTIALS_FILE = "wifi_credentials.json"

# Configure Hardware
adc = machine.ADC(machine.Pin(ADC_PIN))
try:
    adc.atten(machine.ADC.ATTN_11DB)
except AttributeError:
    pass

try:
    led = machine.Pin(LED_PIN, machine.Pin.OUT)
    led.value(1) # Default OFF (Active Low on Xiao ESP32-C6)
except Exception:
    led = None

# ============================================================================
# Visual LED Indicator Helpers
# ============================================================================
def pulse_led(on_ms=50):
    if led is None: return
    led.value(0) # ON
    time.sleep_ms(on_ms)
    led.value(1) # OFF

def heartbeat_led():
    pulse_led(40)
    time.sleep_ms(70)
    pulse_led(40)

def sample_taken_led():
    for _ in range(3):
        pulse_led(30)
        time.sleep_ms(50)

# ============================================================================
# Audio Helper Functions (Zelda Chimes)
# ============================================================================
def play_tone(freq, duration_ms):
    if freq <= 0:
        time.sleep_ms(duration_ms)
        return
    try:
        pwm = machine.PWM(machine.Pin(BUZZER_PIN), freq=freq, duty=512)
        time.sleep_ms(duration_ms)
        pwm.deinit()
    except Exception:
        pass
    time.sleep_ms(30)

def play_zelda_item_get():
    print("\n🎶 Playing Zelda Item Fanfare (Full Charge!)...")
    notes = [(784, 130), (831, 130), (880, 130), (932, 450)]
    for freq, duration in notes:
        play_tone(freq, duration)

def play_zelda_lost_life():
    print("\n🎶 Playing Zelda Lost Life Ditty (Test Stopped/Complete!)...")
    notes = [(494, 160), (466, 160), (440, 160), (415, 400), (392, 500)]
    for freq, duration in notes:
        play_tone(freq, duration)

# ============================================================================
# Battery ADC Reading
# ============================================================================
def read_battery_voltage(num_samples=16):
    total_uv = 0
    for _ in range(num_samples):
        try:
            total_uv += adc.read_uv()
        except AttributeError:
            total_uv += (adc.read_u16() >> 4) * (3300000 / 4095)
        time.sleep_us(100)
    
    avg_uv = total_uv / num_samples
    v_adc = (avg_uv / 1000000.0)
    v_bat = v_adc * BAT_DIVIDER_RATIO
    return v_adc, v_bat

def run_synthetic_load(duration_ms=500):
    """Runs a synthetic CPU math loop to simulate active ESP32 load draw."""
    start = time.ticks_ms()
    x = 1.0001
    while time.ticks_diff(time.ticks_ms(), start) < duration_ms:
        x = math.sin(x) + math.cos(x) + 1.0001

# ============================================================================
# Wi-Fi Setup Helper
# ============================================================================
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    ssid = ""
    password = ""
    
    # Try loading cached credentials if available
    try:
        with open(CREDENTIALS_FILE, "r") as f:
            creds = json.load(f)
            ssid = creds.get("ssid", "")
            password = creds.get("password", "")
    except Exception:
        pass
        
    if ssid:
        print(f"\n Found saved Wi-Fi credentials for: '{ssid}'")
        use_saved = input(" Use saved Wi-Fi credentials? (Y/n): ").strip().lower()
        if use_saved in ["n", "no"]:
            ssid = ""
            
    if not ssid:
        print("\n==========================================")
        print("   Wi-Fi Configuration Prompt            ")
        print("==========================================")
        ssid = input(" Enter Wi-Fi SSID: ").strip()
        password = input(" Enter Wi-Fi Password: ").strip()
        
        if ssid:
            try:
                with open(CREDENTIALS_FILE, "w") as f:
                    json.dump({"ssid": ssid, "password": password}, f)
                print(" -> Wi-Fi credentials saved to device.")
            except Exception:
                pass

    if not ssid:
        print("❌ No SSID provided. Running offline mode.")
        return None

    print(f"\nConnecting to '{ssid}'...", end="")
    wlan.connect(ssid, password)
    
    timeout = 15
    while not wlan.isconnected() and timeout > 0:
        time.sleep(1)
        print(".", end="")
        timeout -= 1
        
    if not wlan.isconnected():
        print("\n❌ Wi-Fi Connection failed! Running in offline mode.")
        return None
        
    ip = wlan.ifconfig()[0]
    print(f"\n✅ Connected! Device IP: {ip}")
    print("==================================================")
    print(f" 🌐 Web Dashboard:  http://{ip}/")
    print(f" 📊 CSV Raw Data:   http://{ip}/csv")
    print(f" ⚙️ C++ Code Table:  http://{ip}/code")
    print("==================================================\n")
    return ip

# ============================================================================
# C++ Array Code Generator from CSV
# ============================================================================
def generate_cpp_code():
    raw_data = []
    try:
        with open(LOG_FILE, "r") as f:
            lines = f.readlines()
            for l in lines[1:]:
                parts = l.strip().split(",")
                if len(parts) >= 3:
                    t = int(parts[0])
                    v = float(parts[2])
                    raw_data.append((t, v))
    except Exception:
        return "// No CSV data logged yet."

    if not raw_data:
        return "// No calibration points available."

    # 1. Skip initial startup settling period (first 180 seconds)
    valid_data = [v for t, v in raw_data if t >= 180]
    if not valid_data:
        valid_data = [v for t, v in raw_data]

    # 2. Moving Median Filter (window size 9) to smooth Wi-Fi/CPU spike dips
    def moving_median(pts, w=9):
        res = []
        half = w // 2
        for i in range(len(pts)):
            s = max(0, i - half)
            e = min(len(pts), i + half + 1)
            sub = sorted(pts[s:e])
            res.append(sub[len(sub) // 2])
        return res

    smoothed = moving_median(valid_data, w=9)

    v_max = max(smoothed)
    v_min = min(smoothed)
    in_progress = (v_min > 3.40)

    cpp_lines = [
        "// Auto-generated LiPo Discharge Curve Array",
        "// Copy-paste into firmware/src/battery_curve.cpp"
    ]

    if in_progress:
        cpp_lines.append(f"// NOTE: Profiling IN PROGRESS! Battery is currently at {v_max:.3f}V (~98% capacity).")
        cpp_lines.append(f"// Full curve will populate automatically once discharged down to 3.30V.")
        
        # Standard LiPo Reference Curve while test is in progress
        standard_lipo = [
            (4150, 100), (4050, 90), (3950, 80), (3880, 70), (3820, 60),
            (3780, 50), (3740, 40), (3700, 30), (3620, 20), (3500, 10), (3300, 0)
        ]
        cpp_lines.append("static const VoltagePoint PROGMEM BATTERY_CURVE[] = {")
        for i, (mv, pct) in enumerate(standard_lipo):
            comma = "," if i < len(standard_lipo) - 1 else ""
            cpp_lines.append(f"    {{ {mv:4d}, {pct:3d} }}{comma}  // {mv/1000:.3f}V = {pct}% (Reference)")
        cpp_lines.append("};")
    else:
        # Full discharge complete - output actual empirical curve points
        cpp_lines.append("static const VoltagePoint PROGMEM BATTERY_CURVE[] = {")
        pcts = [100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0]
        num_points = len(smoothed)
        for i, pct in enumerate(pcts):
            idx = int(i * (num_points - 1) / 10)
            v_val = smoothed[idx]
            mv = int(v_val * 1000)
            comma = "," if i < len(pcts) - 1 else ""
            cpp_lines.append(f"    {{ {mv:4d}, {pct:3d} }}{comma}  // {v_val:.3f}V = {pct}%")
        cpp_lines.append("};")

    return "\n".join(cpp_lines)



# ============================================================================
# HTML Dashboard Page Generator
# ============================================================================
def build_html_dashboard(ip):
    return """<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>E-Ink Sign - Battery Profiler</title>
  <style>
    :root { --bg: #0f172a; --card: #1e293b; --accent: #38bdf8; --text: #f8fafc; --muted: #94a3b8; --green: #4ade80; }
    body { background: var(--bg); color: var(--text); font-family: -apple-system, system-ui, sans-serif; margin: 0; padding: 20px; }
    .container { max-width: 900px; margin: 0 auto; }
    h1 { color: var(--accent); display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; margin-bottom: 20px; }
    .card { background: var(--card); border-radius: 12px; padding: 20px; border: 1px solid #334155; }
    .val { font-size: 2rem; font-weight: bold; margin-top: 5px; color: var(--green); }
    .lbl { font-size: 0.85rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; }
    textarea { width: 100%; background: #020617; color: #38bdf8; border: 1px solid #334155; border-radius: 8px; padding: 12px; font-family: monospace; box-sizing: border-box; }
    button { background: var(--accent); color: #020617; border: none; padding: 10px 18px; font-weight: bold; border-radius: 6px; cursor: pointer; margin-right: 10px; margin-top: 10px; }
    button:hover { opacity: 0.9; }
    .status-badge { display: inline-block; padding: 4px 10px; border-radius: 20px; font-size: 0.8rem; background: #0284c7; color: white; margin-left: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔋 E-Ink Sign Battery Profiler <span class="status-badge" id="phase-badge">CONNECTING...</span></h1>
    
    <div class="grid">
      <div class="card"><div class="lbl">Battery Voltage</div><div class="val" id="v-bat">-- V</div></div>
      <div class="card"><div class="lbl">ADC Pin (D0)</div><div class="val" id="v-adc">-- V</div></div>
      <div class="card"><div class="lbl">Elapsed Time</div><div class="val" id="elapsed">0s</div></div>
      <div class="card"><div class="lbl">Samples Logged</div><div class="val" id="samples">0</div></div>
    </div>

    <div class="card" style="margin-bottom: 20px;">
      <div class="lbl">Synthetic Power Load Control</div>
      <p style="color: var(--muted); font-size: 0.9rem;">Simulates active ESP32 Wi-Fi & CPU power draw (~80mA - 120mA) to accelerate battery discharge profiling.</p>
      <button onclick="toggleLoad()" id="load-btn">Enable Synthetic Load</button>
      <button onclick="location.reload()">🔄 Refresh Dashboard</button>
    </div>

    <div class="card" style="margin-bottom: 20px;">
      <div class="lbl">Raw CSV Logged Data (battery_curve.csv)</div>
      <textarea id="csv-area" rows="8" readonly>Loading CSV data...</textarea>
      <button onclick="copyCSV()">📋 Copy Raw CSV Data</button>
    </div>

    <div class="card">
      <div class="lbl">Generated C++ Code Array (battery_curve.cpp)</div>
      <textarea id="code-area" rows="8" readonly>Generating C++ code...</textarea>
      <button onclick="copyCode()">📋 Copy C++ Array Code</button>
    </div>
  </div>

  <script>
    async function update() {
      try {
        const res = await fetch('/api/status');
        const d = await res.json();
        document.getElementById('v-bat').innerText = d.v_bat.toFixed(3) + ' V';
        document.getElementById('v-adc').innerText = d.v_adc.toFixed(3) + ' V';
        document.getElementById('elapsed').innerText = d.elapsed + 's';
        document.getElementById('samples').innerText = d.samples;
        document.getElementById('phase-badge').innerText = d.phase;
        document.getElementById('load-btn').innerText = d.load_active ? 'Disable Synthetic Load' : 'Enable Synthetic Load';
      } catch(e){}
    }

    async function loadCSV() {
      try {
        const r1 = await fetch('/csv');
        document.getElementById('csv-area').value = await r1.text();
        const r2 = await fetch('/code');
        document.getElementById('code-area').value = await r2.text();
      } catch(e){}
    }

    async function toggleLoad() {
      await fetch('/api/toggle_load', {method: 'POST'});
      update();
    }

    function copyCSV() {
      const a = document.getElementById('csv-area');
      a.select();
      navigator.clipboard.writeText(a.value);
      alert('CSV Data copied to clipboard!');
    }

    function copyCode() {
      const a = document.getElementById('code-area');
      a.select();
      navigator.clipboard.writeText(a.value);
      alert('C++ Code snippet copied to clipboard!');
    }

    setInterval(update, 3000);
    setInterval(loadCSV, 10000);
    update();
    loadCSV();
  </script>
</body>
</html>"""

# ============================================================================
# HTTP Request Dispatcher
# ============================================================================
def handle_http_request(conn, state):
    try:
        req = conn.recv(1024).decode('utf-8')
        if not req:
            conn.close()
            return
            
        first_line = req.split('\r\n')[0]
        parts = first_line.split(' ')
        if len(parts) < 2:
            conn.close()
            return
        method, path = parts[0], parts[1]
        
        if path == "/":
            body = build_html_dashboard(state['ip'])
            header = f"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
            conn.sendall(header.encode('utf-8') + body.encode('utf-8'))
            
        elif path == "/api/status":
            body = json.dumps({
                "v_bat": state["v_bat"],
                "v_adc": state["v_adc"],
                "elapsed": state["elapsed"],
                "samples": state["samples"],
                "phase": state["phase"],
                "load_active": state["load_active"]
            })
            header = f"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
            conn.sendall(header.encode('utf-8') + body.encode('utf-8'))
            
        elif path == "/csv":
            try:
                with open(LOG_FILE, "r") as f:
                    body = f.read()
            except Exception:
                body = "timestamp_sec,v_adc,v_bat\n"
            header = f"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
            conn.sendall(header.encode('utf-8') + body.encode('utf-8'))
            
        elif path == "/code":
            body = generate_cpp_code()
            header = f"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
            conn.sendall(header.encode('utf-8') + body.encode('utf-8'))
            
        elif path == "/api/toggle_load":
            state["load_active"] = not state["load_active"]
            body = json.dumps({"status": "ok", "load_active": state["load_active"]})
            header = f"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\nConnection: close\r\n\r\n"
            conn.sendall(header.encode('utf-8') + body.encode('utf-8'))
            
        else:
            conn.sendall(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            
    except Exception:
        pass
    finally:
        try:
            conn.close()
        except Exception:
            pass

# ============================================================================
# Main State Machine
# ============================================================================
try:
    print("==========================================")
    print("   Zelda Battery Discharge Profiler (Web) ")
    print("==========================================")
    
    # 1. Connect Wi-Fi
    ip = connect_wifi()
    
    # 2. Bind Socket
    server_socket = None
    if ip:
        try:
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_socket.bind(('', 80))
            server_socket.listen(3)
            server_socket.settimeout(0.05) # Non-blocking 50ms timeout
        except Exception as e:
            print(f"❌ Server socket failed: {e}")

    # Prepare CSV file
    try:
        with open(LOG_FILE, "r") as f:
            pass
    except OSError:
        with open(LOG_FILE, "w") as f:
            f.write("timestamp_sec,v_adc,v_bat\n")

    start_time = time.time()
    last_sample_time = 0
    last_heartbeat_time = 0
    sample_count = 0
    
    v_adc, v_bat = read_battery_voltage()
    
    state = {
        "ip": ip or "offline",
        "v_bat": v_bat,
        "v_adc": v_adc,
        "elapsed": 0,
        "samples": 0,
        "phase": "🔋 PROFILING",
        "load_active": True
    }

    print(" -> Discharge profiling active!")
    print(" -> Press Ctrl-C at any time to exit.\n")

    while True:
        now = time.time()
        state["elapsed"] = int(now - start_time)
        
        # A. Handle HTTP Requests
        if server_socket:
            try:
                conn, addr = server_socket.accept()
                handle_http_request(conn, state)
            except OSError:
                pass # Timeout on accept is normal
                
        # B. Run Synthetic Load if toggled ON
        if state["load_active"]:
            run_synthetic_load(duration_ms=100)

        # C. Periodic LED Heartbeat (every 2s)
        if now - last_heartbeat_time >= 2:
            heartbeat_led()
            last_heartbeat_time = now

        # D. Periodic Battery Sampling (every SAMPLE_INTERVAL_SEC)
        if now - last_sample_time >= SAMPLE_INTERVAL_SEC or last_sample_time == 0:
            v_adc, v_bat = read_battery_voltage()
            state["v_adc"] = v_adc
            state["v_bat"] = v_bat
            
            sample_count += 1
            state["samples"] = sample_count
            
            # Log sample to CSV
            row = f"{state['elapsed']},{v_adc:.4f},{v_bat:.4f}\n"
            with open(LOG_FILE, "a") as f:
                f.write(row)
                
            sample_taken_led()
            print(f"[{state['elapsed']:6d}s] Sample #{sample_count:04d} | V_ADC: {v_adc:.3f}V | V_BAT: {v_bat:.3f}V")
            
            # Check cutoff
            if v_bat <= CUTOFF_VOLTAGE:
                state["phase"] = "🛑 CUTOFF REACHED"
                print(f"\n[CUTOFF] Voltage reached {v_bat:.2f}V! Profiling complete.")
                break
                
            last_sample_time = now

        time.sleep(0.05)

except KeyboardInterrupt:
    print("\n\n[INFO] Execution interrupted by user (Ctrl-C).")

finally:
    play_zelda_lost_life()
    print("\n==========================================")
    print(" Profiling Stopped / Complete!")
    print("==========================================")
