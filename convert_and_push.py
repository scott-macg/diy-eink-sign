import sys
import os
from PIL import Image

def convert_png_to_3color_raw(png_path, out_bw_path="bw.raw", out_red_path="red.raw", flip_180=False):
    if not os.path.exists(png_path):
        print(f"Error: File '{png_path}' not found.")
        sys.exit(1)
        
    print(f"Loading image '{png_path}'...")
    img = Image.open(png_path).convert('RGB')
    
    # Target resolution: 296x128 landscape (or 128x296 portrait)
    # The SSD1680 panel controller native RAM orientation is 128 wide x 296 high.
    if img.width == 296 and img.height == 128:
        print("Rotating landscape 296x128 image to match native 128x296 panel orientation...")
        img = img.rotate(270, expand=True) # 270 deg / 90 deg counter-clockwise
    elif (img.width, img.height) != (128, 296):
        print(f"Resizing image from {img.size} to (128, 296)...")
        img = img.resize((128, 296))

    if flip_180:
        print("Flipping image 180 degrees...")
        img = img.rotate(180)

    # Define 3-Color Palette: 0=Black, 1=White, 2=Red
    pal = Image.new("P", (1, 1))
    pal.putpalette([
        0, 0, 0,        # Index 0: Black
        255, 255, 255,  # Index 1: White
        255, 0, 0       # Index 2: Red
    ] + [0, 0, 0] * 253)

    print("Applying Floyd-Steinberg Error Diffusion Dithering...")
    quantized = img.quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)
    
    # Save a preview PNG for visual inspection on computer
    preview_path = "preview_dithered.png"
    quantized.save(preview_path)
    print(f"Dithered preview saved to '{preview_path}'.")

    pixels = list(quantized.get_flattened_data() if hasattr(quantized, "get_flattened_data") else quantized.getdata())  # 128 * 296 pixels
    total_bytes = (128 * 296) // 8
    
    bw_bytes = bytearray(total_bytes)
    red_bytes = bytearray(total_bytes)
    
    for i, p in enumerate(pixels):
        byte_idx = i // 8
        bit_idx = 7 - (i % 8) # MSB first
        
        if p == 0:   # Black: BW bit = 0, Red bit = 0
            pass
        elif p == 1: # White: BW bit = 1, Red bit = 0
            bw_bytes[byte_idx] |= (1 << bit_idx)
        elif p == 2: # Red: BW bit = 1, Red bit = 1
            bw_bytes[byte_idx] |= (1 << bit_idx)
            red_bytes[byte_idx] |= (1 << bit_idx)

    with open(out_bw_path, "wb") as f:
        f.write(bw_bytes)
        
    with open(out_red_path, "wb") as f:
        f.write(red_bytes)
        
    print(f"Successfully generated '{out_bw_path}' ({len(bw_bytes)} bytes) and '{out_red_path}' ({len(red_bytes)} bytes).")
    return out_bw_path, out_red_path

def generate_micropython_player(out_script="play_image.py"):
    script_content = '''import time
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
'''
    with open(out_script, "w") as f:
        f.write(script_content)
    print(f"Generated MicroPython player script '{out_script}'.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 convert_and_push.py <path_to_png_image> [--flip180]")
        sys.exit(1)
        
    png_path = sys.argv[1]
    flip = "--flip180" in sys.argv or "-r" in sys.argv or "--rotate180" in sys.argv
    convert_png_to_3color_raw(png_path, flip_180=flip)
    generate_micropython_player()
