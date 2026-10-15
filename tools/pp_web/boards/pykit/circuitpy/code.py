# PyKit Explorer demo for Peripheral Pretender (built-in modules only).
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
#
# Tilt the board on the page: the bubble on the TFT follows the Ruler's
# ICM-20948, the NeoPixels show which way the board leans, the red LED
# blinks, and holding USER turns the bubble red and sounds a note on the
# speaker. Readings go to the console (the USB serial port).
import math
import time

import audioio
import board
import busdisplay
import digitalio
import displayio
import fourwire
import neopixel_write
import synthio
import vectorio

# ST7789 on the Ruler (same start-up sequence as adafruit_st7789)
INIT = bytes((
    0x01, 0x80, 0x96,           # SWRESET, 150 ms
    0x11, 0x80, 0xFF,           # SLPOUT, 255 ms
    0x3A, 0x81, 0x55, 0x0A,     # COLMOD 16 bit, 10 ms
    0x36, 0x01, 0x08,           # MADCTL
    0x21, 0x80, 0x0A,           # INVON, 10 ms
    0x13, 0x80, 0x0A,           # NORON, 10 ms
    0x36, 0x01, 0xC0,           # MADCTL
    0x29, 0x80, 0xFF,           # DISPON, 255 ms
))

displayio.release_displays()
backlight = digitalio.DigitalInOut(board.LCD_BL)
backlight.switch_to_output(value=False)          # low: lit
bus = fourwire.FourWire(board.LCD_SPI(), command=board.D4,
                        chip_select=board.LCD_CS)
display = busdisplay.BusDisplay(bus, INIT, width=240, height=135,
                                rotation=90, rowstart=40, colstart=53)

screen = displayio.Group()
back = displayio.Palette(1)
back[0] = 0x002244
screen.append(vectorio.Rectangle(pixel_shader=back, width=240, height=135))
lines = displayio.Palette(1)
lines[0] = 0x4488AA
screen.append(vectorio.Rectangle(pixel_shader=lines, width=240, height=1,
                                 y=67))
screen.append(vectorio.Rectangle(pixel_shader=lines, width=1, height=135,
                                 x=120))
ring = displayio.Palette(1)
ring[0] = 0x4488AA
screen.append(vectorio.Circle(pixel_shader=ring, radius=30, x=120, y=67))
inner = displayio.Palette(1)
inner[0] = 0x002244
screen.append(vectorio.Circle(pixel_shader=inner, radius=28, x=120, y=67))
bubble_colour = displayio.Palette(1)
bubble_colour[0] = 0x66FF66
bubble = vectorio.Circle(pixel_shader=bubble_colour, radius=10, x=120, y=67)
screen.append(bubble)
display.root_group = screen

# ICM-20948 at 0x69: wake it, read the accelerometer (+-2 g, 16384 LSB/g)
i2c = board.I2C()
while not i2c.try_lock():
    pass
i2c.writeto(0x69, bytes((0x7F, 0x00)))           # user bank 0
i2c.writeto(0x69, bytes((0x06, 0x01)))           # PWR_MGMT_1: awake
who = bytearray(1)
i2c.writeto_then_readfrom(0x69, bytes((0x00,)), who)
print("ICM-20948 WHO_AM_I: 0x%02X" % who[0])


def accel():
    raw = bytearray(6)
    i2c.writeto_then_readfrom(0x69, bytes((0x2D,)), raw)
    return [((raw[2 * i] << 8 | raw[2 * i + 1]) ^ 0x8000) - 0x8000
            for i in range(3)]


led = digitalio.DigitalInOut(board.LED)
led.switch_to_output()
user = digitalio.DigitalInOut(board.D3)
user.switch_to_input()                           # R20 pulls it up
pixels_pin = digitalio.DigitalInOut(board.NEOPIXEL)
pixels_pin.switch_to_output()

# The speaker: a synthesizer on the DAC, a note while USER is held
speaker = audioio.AudioOut(board.DAC)
synth = synthio.Synthesizer(sample_rate=22050)
speaker.play(synth)
sounding = False

last_print = 0
while True:
    x, y, z = accel()
    pitch = math.degrees(math.atan2(-x, math.sqrt(y * y + z * z)))
    roll = math.degrees(math.atan2(y, z))
    bubble.x = 120 + max(-100, min(100, int(roll * 2)))
    bubble.y = 67 + max(-55, min(55, int(pitch * 2)))
    pressed = not user.value
    if pressed != sounding:
        sounding = pressed
        (synth.press if pressed else synth.release)(72)     # C5
    bubble_colour[0] = 0xFF4040 if pressed else 0x66FF66
    # Five NeoPixels: the Curiosity board's, then the Ruler's four
    lean = min(255, int(abs(roll) * 4))
    tip = min(255, int(abs(pitch) * 4))
    g, r, b = (tip, lean, 32) if not pressed else (0, 64, 0)
    neopixel_write.neopixel_write(pixels_pin, bytes((g, r, b) * 5))
    led.value = (time.monotonic() % 1) < 0.5
    if time.monotonic() - last_print > 1:
        last_print = time.monotonic()
        print("pitch %5.1f roll %5.1f %s" % (pitch, roll,
                                             "USER" if pressed else ""))
    time.sleep(0.05)
