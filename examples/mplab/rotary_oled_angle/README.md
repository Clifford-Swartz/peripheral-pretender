# Rotary angle on an OLED (MPLAB project)

An MPLAB X project for the PIC32CM MC00 Curiosity Pro (EV15N46A) with two Click boards: turn a
**Rotary G Click** on EXT1 and an **OLED B Click** on EXT2 shows the angle, as big digits and as
a dial with a needle; the Rotary's LED ring points the same way. Push the knob to set it back to
0 degrees.

It is Microchip's Harmony CSP example `port_led_on_off_polling` for this board with a new
`firmware/src/main.c`: Harmony's generated clock, port and start-up code are unchanged, and the
application drives SERCOM0 (VCOM), SERCOM1 (SPI), SERCOM2 (I2C) and the EIC itself.

| | |
|---|---|
| Encoder | A on PWM (PB10, EXTINT10), B on AN (PB08, EXTINT8): counted in the EIC interrupt on both edges, 30 detents a turn, 12 degrees each |
| Push switch | INT (PB02): back to 0, LED0 while held |
| LED ring | two 74HC595 on SERCOM1 SPI, LATCH on CS (PA17), MR# on RST (PA20) |
| OLED B | SSD1306 on SERCOM2 I2C: commands to 0x3C with D/C (PB22) low, data to 0x3D with D/C high; RES# on PA10 |
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

- **VS Code with the Peripheral Pretender extension:** open this folder and build; open the
  MC00 board from the Twin sidebar (it runs each new build in `dist/` by itself). In the board
  view, plug **Rotary G Click** into EXT1 and **OLED B Click** into EXT2; turn the Knob slider,
  click the shaft to push it. F5 (Peripheral Pretender Twin) debugs it on the twin.
- **Web board view:** `py -m tools.pp_web --watch examples/mplab/rotary_oled_angle`, then plug
  the two Clicks as above.
- **QEMU:** `qemu-system-arm -M pic32cm-mc00-cpro,ext1=rotaryg,ext2=oledb -kernel <the ELF>
  -serial stdio -display none`, turning with `qom-set /machine/ext1-click position <n>`.

On the board itself, fit both Clicks through mikroBUS Xplained Pro adapters and program the
hex from `dist/` with MPLAB X or MPLAB IPE.

## Notes

- The encoder is counted in an interrupt because a redraw of the OLED takes about 12 ms of I2C:
  a polling loop that draws would miss steps while the knob turns.
- The OLED set-up is MikroElektronika's oledb driver's; with it, screen row y is RAM line
  y + 1 and column x RAM column 95 - x, which `plot()` follows.
- The Harmony example's sources keep their Microchip licence headers.
