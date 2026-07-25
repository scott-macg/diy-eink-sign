import machine
import time
import math

# ============================================================================
# Hardware & Configuration
# ============================================================================
ADC_PIN = 0               # A0 / GPIO 0 (Xiao D0 - Battery Sense)
BUZZER_PIN = 16           # D6 / GPIO 16 (Audio NPN Driver Pin)
BAT_DIVIDER_RATIO = 2.0   # 1:2 resistor divider (2x 100k)

FULL_CHARGE_VOLTAGE = 4.18 # Voltage threshold to consider battery full
CUTOFF_VOLTAGE = 3.20      # Safe low-voltage shutdown threshold (Volts)
SAMPLE_INTERVAL_SEC = 30   # Take a sample every 30 seconds
FLUSH_EVERY_N = 10        # Flush buffer to flash every 10 samples (5 mins)
LOG_FILE = "battery_curve.csv"

# Configure Hardware
adc = machine.ADC(machine.Pin(ADC_PIN))
try:
    adc.atten(machine.ADC.ATTN_11DB)
except AttributeError:
    pass

# ============================================================================
# Audio Helper Functions (Zelda Chimes)
# ============================================================================
def play_tone(freq, duration_ms):
    """Plays a single tone frequency on BUZZER_PIN via PWM."""
    if freq <= 0:
        time.sleep_ms(duration_ms)
        return
    pwm = machine.PWM(machine.Pin(BUZZER_PIN), freq=freq, duty=512)
    time.sleep_ms(duration_ms)
    pwm.deinit()
    time.sleep_ms(30) # Short silence gap between notes

def play_zelda_item_get():
    """Legend of Zelda - Item Discovery / Get Fanfare."""
    print("🎶 Playing Zelda Item Fanfare (Full Charge!)...")
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
    print("🎶 Playing Zelda Lost Life Ditty (Test Complete!)...")
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
# Battery Reading & Load Helper
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

def run_synthetic_load(duration_ms=2000):
    """Runs a synthetic CPU load loop to simulate continuous MCU power draw."""
    start = time.ticks_ms()
    x = 1.0001
    while time.ticks_diff(time.ticks_ms(), start) < duration_ms:
        x = math.sin(x) + math.cos(x) + 1.0001

# ============================================================================
# Main State Machine
# ============================================================================
print("==========================================")
print("   Zelda Battery Discharge Profiler      ")
print("==========================================")

# --- Phase 1: Wait for Full Charge (Plugged into USB) ---
print("\n[PHASE 1] Monitoring Charge... Connect USB to charge battery.")
last_announcement = 0

while True:
    v_adc, v_bat = read_battery_voltage()
    print(f" -> Current Voltage: {v_bat:.3f}V (Target: >={FULL_CHARGE_VOLTAGE}V)")
    
    if v_bat >= FULL_CHARGE_VOLTAGE:
        now = time.time()
        # Play Zelda item discovery chime every 60 seconds while fully charged
        if now - last_announcement >= 60:
            play_zelda_item_get()
            last_announcement = now
            print(" -> [FULL CHARGE] Unplug USB cable to automatically start discharge test!")
    
    # Detect USB disconnection (voltage drops slightly below charging potential)
    if last_announcement > 0 and v_bat < (FULL_CHARGE_VOLTAGE - 0.05):
        print("\n⚡ USB Disconnected! Battery fully charged. Starting discharge test...")
        break
        
    time.sleep(5)

# --- Phase 2: Discharge Profiling & Logging ---
print("\n[PHASE 2] Profiling Battery Discharge Curve...")
print(f"Logging to: {LOG_FILE} every {SAMPLE_INTERVAL_SEC}s (Safety Cutoff: {CUTOFF_VOLTAGE}V)")

# Prepare CSV file
try:
    with open(LOG_FILE, "r") as f:
        pass
except OSError:
    with open(LOG_FILE, "w") as f:
        f.write("timestamp_sec,v_adc,v_bat\n")

start_time = time.time()
buffer = []
sample_count = 0

try:
    while True:
        elapsed = int(time.time() - start_time)
        v_adc, v_bat = read_battery_voltage()
        
        print(f"[{elapsed:6d}s] Sample #{sample_count+1:04d} | V_ADC: {v_adc:.3f}V | V_BAT: {v_bat:.3f}V")
        buffer.append(f"{elapsed},{v_adc:.4f},{v_bat:.4f}\n")
        sample_count += 1
        
        # Check low-voltage cutoff threshold
        if v_bat <= CUTOFF_VOLTAGE:
            print(f"\n[CUTOFF] Voltage reached {v_bat:.2f}V! Ending test.")
            break
            
        # Flush buffer periodically to protect flash filesystem
        if len(buffer) >= FLUSH_EVERY_N:
            with open(LOG_FILE, "a") as f:
                f.writelines(buffer)
            buffer.clear()
            print(" -> Data buffer saved to LittleFS.")

        # Run synthetic CPU load & pause remainder of interval
        run_synthetic_load(duration_ms=3000)
        time.sleep(max(1, SAMPLE_INTERVAL_SEC - 3))

except KeyboardInterrupt:
    print("\n[INFO] Test manually stopped by user.")

finally:
    # Save remaining data
    if buffer:
        with open(LOG_FILE, "a") as f:
            f.writelines(buffer)
        print(" -> Final data buffer saved to LittleFS.")
        
    # --- Phase 3: Play Test Complete Ditty ---
    play_zelda_lost_life()
    print("\n==========================================")
    print(f" Profiling Complete! Total Samples: {sample_count}")
    print(f" Output File: {LOG_FILE}")
    print("==========================================")
