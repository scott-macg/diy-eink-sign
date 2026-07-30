import machine
import time
import math
import sys

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
FLUSH_EVERY_N = 1         # Flush to LittleFS on EVERY sample to prevent data loss!
LOG_FILE = "battery_curve.csv"

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
    """Turns LED on for specified milliseconds (Active Low)."""
    if led is None:
        return
    led.value(0) # ON
    time.sleep_ms(on_ms)
    led.value(1) # OFF

def heartbeat_led():
    """Flashes a double-pulse heartbeat: lub-dub."""
    pulse_led(40)
    time.sleep_ms(70)
    pulse_led(40)

def sample_taken_led():
    """Flashes 3 rapid bursts when a battery sample is recorded."""
    for _ in range(3):
        pulse_led(30)
        time.sleep_ms(50)

# ============================================================================
# Audio Helper Functions (Zelda Chimes)
# ============================================================================
def play_tone(freq, duration_ms):
    """Plays a single tone frequency on BUZZER_PIN via PWM."""
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
    """Legend of Zelda - Item Discovery / Get Fanfare."""
    print("\n🎶 Playing Zelda Item Fanfare (Full Charge!)...")
    notes = [
        (784, 130),  # G5
        (831, 130),  # G#5
        (880, 130),  # A5
        (932, 450)   # A#5
    ]
    for freq, duration in notes:
        play_tone(freq, duration)

def play_zelda_lost_life():
    """Legend of Zelda - Lost Life / Game Over Ditty."""
    print("\n🎶 Playing Zelda Lost Life Ditty (Test Stopped/Complete!)...")
    notes = [
        (494, 160),  # B4
        (466, 160),  # Bb4
        (440, 160),  # A4
        (415, 400),  # Ab4
        (392, 500)   # G4
    ]
    for freq, duration in notes:
        play_tone(freq, duration)

# ============================================================================
# Battery Reading & Heartbeat Helper Functions
# ============================================================================
def read_battery_voltage(num_samples=16):
    """Takes 16 rapid ADC samples and returns calculated V_bat in Volts."""
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

def responsive_sleep(seconds, status_prefix=""):
    """
    Sleeps for N seconds in 0.25s intervals, pulsing the onboard LED 
    heartbeat every 2 seconds so the user can visually confirm the 
    battery test is running untethered without USB connected.
    """
    spinner = ["|", "/", "-", "\\"]
    start = time.time()
    idx = 0
    while time.time() - start < seconds:
        rem = max(0, int(seconds - (time.time() - start)))
        char = spinner[idx % len(spinner)]
        sys.stdout.write(f"\r{status_prefix} [{char}] Next update in {rem:2d}s (Ctrl-C to stop)  ")
        try:
            sys.stdout.flush()
        except AttributeError:
            pass
        
        # Pulse LED heartbeat every 2 seconds (8 x 0.25s)
        if idx % 8 == 0:
            heartbeat_led()
        else:
            time.sleep(0.25)
            
        idx += 1
        
    sys.stdout.write("\r" + " " * 75 + "\r")
    try:
        sys.stdout.flush()
    except AttributeError:
        pass

def run_synthetic_load(duration_ms=2000):
    """Runs a synthetic CPU load loop to simulate continuous MCU power draw."""
    start = time.ticks_ms()
    x = 1.0001
    while time.ticks_diff(time.ticks_ms(), start) < duration_ms:
        x = math.sin(x) + math.cos(x) + 1.0001

# ============================================================================
# Main State Machine
# ============================================================================
try:
    print("==========================================")
    print("   Zelda Battery Discharge Profiler      ")
    print("==========================================")
    print(" -> Heartbeat LED on GPIO 15 active.")
    print(" -> Data flushed to LittleFS after EVERY sample.")
    print(" -> Press Ctrl-C at any time to stop execution gracefully.\n")

    # --- Phase 1: Wait for Full Charge (Plugged into USB) ---
    print("[PHASE 1] Monitoring Charge... Connect USB to charge battery.")
    print(" -> Unplug USB at any time to automatically start discharge test!")
    last_announcement = 0
    peak_voltage = 0.0

    while True:
        v_adc, v_bat = read_battery_voltage()
        print(f" -> Current Voltage: {v_bat:.3f}V (Target: >={FULL_CHARGE_VOLTAGE}V)")
        
        if v_bat > peak_voltage:
            peak_voltage = v_bat
        
        if v_bat >= FULL_CHARGE_VOLTAGE:
            now = time.time()
            if now - last_announcement >= 60:
                play_zelda_item_get()
                last_announcement = now
                print(" -> [FULL CHARGE] Unplug USB cable to automatically start discharge test!")
        
        # Transition to Phase 2 automatically if:
        # 1. Full charge reached and USB unplugged (voltage drops slightly below full charge threshold)
        # 2. USB was unplugged during monitoring (voltage drops by >0.03V from peak)
        # 3. Started already untethered on battery power (voltage below full charge threshold)
        if (last_announcement > 0 and v_bat < (FULL_CHARGE_VOLTAGE - 0.04)) or \
           (peak_voltage > 3.8 and v_bat < (peak_voltage - 0.03)) or \
           (peak_voltage == 0.0 and v_bat < FULL_CHARGE_VOLTAGE):
            print("\n⚡ USB Disconnected / Battery Power Detected! Starting discharge test...")
            break
            
        responsive_sleep(5, status_prefix=" 🔌 Charging...")

    # --- Phase 2: Discharge Profiling & Logging ---
    print("\n[PHASE 2] Profiling Battery Discharge Curve...")
    print(f"Logging to: {LOG_FILE} every {SAMPLE_INTERVAL_SEC}s (Safety Cutoff: {CUTOFF_VOLTAGE}V)")

    try:
        with open(LOG_FILE, "r") as f:
            pass
    except OSError:
        with open(LOG_FILE, "w") as f:
            f.write("timestamp_sec,v_adc,v_bat\n")

    start_time = time.time()
    buffer = []
    sample_count = 0

    while True:
        elapsed = int(time.time() - start_time)
        v_adc, v_bat = read_battery_voltage()
        
        print(f"[{elapsed:6d}s] Sample #{sample_count+1:04d} | V_ADC: {v_adc:.3f}V | V_BAT: {v_bat:.3f}V")
        sample_taken_led() # 3 rapid blinks when sample is taken
        
        buffer.append(f"{elapsed},{v_adc:.4f},{v_bat:.4f}\n")
        sample_count += 1
        
        if v_bat <= CUTOFF_VOLTAGE:
            print(f"\n[CUTOFF] Voltage reached {v_bat:.2f}V! Ending test.")
            break
            
        # Flush every sample immediately to LittleFS so no data is lost if power drops!
        if len(buffer) >= FLUSH_EVERY_N:
            with open(LOG_FILE, "a") as f:
                f.writelines(buffer)
            buffer.clear()

        # Run synthetic CPU load & pause remainder of interval
        run_synthetic_load(duration_ms=3000)
        remaining_sleep = max(1, SAMPLE_INTERVAL_SEC - 3)
        responsive_sleep(remaining_sleep, status_prefix=" 🔋 Profiling...")

except KeyboardInterrupt:
    print("\n\n[INFO] Execution interrupted by user (Ctrl-C).")

finally:
    # Save remaining data in buffer before exiting
    if 'buffer' in locals() and buffer:
        with open(LOG_FILE, "a") as f:
            f.writelines(buffer)
        print(" -> Final data buffer saved to LittleFS.")
        
    play_zelda_lost_life()
    print("\n==========================================")
    print(" Profiling Stopped / Complete!")
    print("==========================================")
