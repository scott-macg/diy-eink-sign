import time
from machine import Pin, PWM

pcm_pwm = PWM(Pin(16), freq=62500)

def play_audio(filename, sample_rate=8000):
    delay_us = int(1000000 / sample_rate)
    with open(filename, 'rb') as f:
        buf = bytearray(256)
        n = f.readinto(buf)
        while n > 0:
            for i in range(n):
                pcm_pwm.duty_u16(buf[i] * 257)
                time.sleep_us(delay_us)
            n = f.readinto(buf)
    pcm_pwm.duty_u16(0)

print("Playing chime.raw over speaker...")
play_audio('chime.raw', sample_rate=8000)
print("Finished playing chime!")
