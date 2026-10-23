# Rotary angle on a colour OLED (MPLAB project)

An MPLAB X project for the PIC32CM MC00 Curiosity Pro (EV15N46A) with two Click boards: turn a
**Rotary R Click** on EXT2 and an **OLED C Click** on EXT1 shows the angle, as big digits and as
a dial with a needle; the Rotary's LED ring points the same way. Push the knob to set it back to
0 degrees. A sibling of `rotary_oled_angle` (Rotary G on EXT1, OLED B on EXT2).

It is Microchip's Harmony CSP example `port_led_on_off_polling` for this board with a new
`firmware/src/main.c`: Harmony's generated clock, port and start-up code are unchanged, and the
application drives SERCOM0 (VCOM), SERCOM1 (SPI, shared by both Clicks) and the EIC itself.

| | |
|---|---|
| OLED C | SSD1351, 96x96 RGB, on SERCOM1 SPI: CS on CS (PA17), D/C on PWM (PB10), RES# on RST (PA20), the 15 V supply's EN on INT (PB02) |
| Encoder | A on PWM (PB22, EXTINT6), B on AN (PA08: its EIC line is the NMI, so read as an input); counted on both edges of A, 30 detents a turn, 12 degrees each |
| Push switch | INT (PA28): back to 0, LED0 while held |
| LED ring | two 74HC595 on the same SPI, LATCH on CS (PA15), MR# on RST (PA10) |
| VCOM | SERCOM0, 115200 8N1: `angle <n>` whenever it changes |

## Build

- **MPLAB X:** open `firmware/pic32cm_mc00_curiosity_pro.X` and build (XC32 v4.45 or later,
  PIC32CM-MC_DFP).
- **MPLAB for VS Code:** open this folder; accept the import of the `.X` project; build.
- **Command line** (MPLAB X's make, from the `.X` folder):

  ```
  make -f nbproject/Makefile-pic32cm_mc00_curiosity_pro.mk CONF=pic32cm_mc00_curiosity_pro build
  ```

The ELF lands in `firmware/pic32cm_mc00_curiosity_pro.X/dist/pic32cm_mc00_curiosity_pro/production/`.

## Run it without the board

- **VS Code with the Peripheral Pretender extension:** open this folder (or the repository) and
  build; the MC00 board from the Twin sidebar runs each new build in `dist/` by itself. In the
  board view, plug **OLED C Click** into EXT1 and **Rotary R Click** into EXT2; turn the Knob
  slider, click the shaft to push it.
- **Web board view:** `py -m tools.pp_web --watch examples/mplab/rotary_oledc_angle`, then plug
  the two Clicks as above.
- **QEMU:** `qemu-system-arm -M pic32cm-mc00-cpro,ext1=oledc,ext2=rotaryr -kernel <the ELF>
  -serial stdio -display none`, turning with `qom-set /machine/ext2-click position <n>`.

On the board itself, fit both Clicks through mikroBUS Xplained Pro adapters and program the
hex from `dist/` with MPLAB X or MPLAB IPE.

## Notes

- The 16 KB of RAM has no room for a 96 x 96 x 16-bit frame buffer: the picture is drawn in
  SSD1351 windows (a filled rectangle per glyph pixel, a 1 x 1 window per dial point), and a
  turn redraws only the digits and the needle.
- The OLED set-up is MikroElektronika's oledc driver's: columns 16-111 and rows 0-95 are the
  panel, RGB565 high byte first.
- The 74HC595s shift every byte on the shared bus, OLED traffic included; only the 16 bits sent
  just before the LATCH edge show.
- The Harmony example's sources keep their Microchip licence headers.
