import time
from machine import Pin, SPI

cs   = Pin(1,  Pin.OUT, value=1)
dc   = Pin(2,  Pin.OUT, value=1)
rst  = Pin(21, Pin.OUT, value=1)
busy = Pin(22, Pin.IN)

spi = SPI(1, baudrate=4000000, polarity=0, phase=0, sck=Pin(19), mosi=Pin(18))

def send_cmd(cmd):
    dc.value(0)
    cs.value(0)
    spi.write(bytes([cmd]))
    cs.value(1)

def send_data(data):
    dc.value(1)
    cs.value(0)
    if isinstance(data, int):
        spi.write(bytes([data]))
    else:
        spi.write(data)
    cs.value(1)

def wait_busy(timeout_ms=5000):
    start = time.ticks_ms()
    while busy.value() == 1:
        time.sleep_ms(10)
        if time.ticks_diff(time.ticks_ms(), start) > timeout_ms:
            break

def render_image(bw_filename="bw.raw", red_filename="red.raw"):
    print("1. Hardware Reset...")
    rst.value(0)
    time.sleep_ms(20)
    rst.value(1)
    time.sleep_ms(200)

    print("2. SW Reset...")
    send_cmd(0x12)
    wait_busy()

    print("3. Streaming Black/White RAM...")
    send_cmd(0x24)
    with open(bw_filename, "rb") as f:
        while True:
            chunk = f.read(256)
            if not chunk: break
            send_data(chunk)

    print("4. Streaming Red RAM...")
    send_cmd(0x26)
    with open(red_filename, "rb") as f:
        while True:
            chunk = f.read(256)
            if not chunk: break
            send_data(chunk)

    print("5. Triggering 3-Color Display Refresh...")
    send_cmd(0x20)
    wait_busy()
    print("✨ Display render complete!")

render_image()
