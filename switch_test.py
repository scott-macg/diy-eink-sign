from machine import Pin
import time

# Configure pins with internal pull-up resistors
sw_d7 = Pin(17, Pin.IN, Pin.PULL_UP)  # Switch 2 (D7 / GPIO 17)
sw_d9 = Pin(20, Pin.IN, Pin.PULL_UP) # Switch 1 (D9 / GPIO 20)

print("Testing microswitches on D7 and D9...")

while True:
    # 0 = Pressed (grounded), 1 = Released (pulled high)
    d7_val = sw_d7.value()
    d9_val = sw_d9.value()

    print(f"D7: {'PRESSED (0)' if d7_val == 0 else 'RELEASED (1)'} | "
        f"D9: {'PRESSED (0)' if d9_val == 0 else 'RELEASED (1)'}")

    time.sleep(0.2) # Small delay to prevent spamming terminal output