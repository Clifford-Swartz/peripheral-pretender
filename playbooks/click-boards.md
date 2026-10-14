# Adding a Click board or an adapter

Worked examples: Thermo 8 Click (MCP9808 on I2C, ALERT on INT) and the mikroBUS Xplained Pro
adapter (ATMBUSADAPTER-XPRO).

## Click board

1. **Inputs.** The product page on mikroe.com has "2D and 3D files" (a zip with a DXF and a STEP)
   and a schematic PDF. Read the schematic for pull-ups, address jumpers and which mikroBUS pins
   are used.
2. **Part model.** If the chip on the Click has no QEMU model, write one in pp-qemu
   (`hw/sensor/mchp_mcp9808.c` is the pattern): an I2C or SSI target with the registers from its
   datasheet, the physical quantity as a QOM property settable at run time (in integer units,
   such as milli-degrees), and its outputs as GPIO. Open-drain outputs drive 0 or -1 (released).
3. **Catalog entry.** Add the Click to the shared catalog `mikrobus_clicks[]` in pp-qemu
   `hw/misc/mikrobus.c` (every board with mikroBUS sockets uses it). Each entry gives:
   - the device type (or none: the Relay Click has only pins) and its bus (I2C address, or
     SPI on the header's SERCOM),
   - `outs`: device outputs onto mikroBUS signals (RST, PWM, INT, CS),
   - `ins`: mikroBUS signals into device inputs (the 74HC595's LATCH from CS),
   - `pulls`: every pull resistor on the Click, from its schematic. An output behind a pull
     lets go with -1 and the pull sets the pin.
   Generic parts exist for pushbuttons (`click-button`), shift registers (`74hc595`) and the
   MAX7219 LED driver (`max7219`). A board describes its sockets (`MikrobusSocket`: the pads
   behind RST/PWM/INT/CS, the I2C and SPI buses) and calls `mikrobus_plug()`; a socket without
   the bus a Click needs refuses it (no TWI model on the AVR64EA48 yet: no thermo8 there).
4. **qtest.** Identity registers, data encoding, the property, the INT line with and without the
   pad's own pull (`tests/qtest/pic32cm-click-test.c`).
5. **Art.** `py tools/pp_web/clicks/render_click.py <dxf> <parts.json> out.svg`, then rasterize at
   20 px/mm with a transparent background. DXFs in inches are scaled automatically.
   `parts.json` lists part bodies by position (the DXF has none: kinds ic, resistor,
   capacitor, led, zero_ohm, button, relay, terminal, digit); find them from the pad groups
   and the silkscreen labels (render once with an empty parts list). Check the DXF really is
   the Click's: the 7seg Click's page links the 7SEG RGB Click's files. Without a usable DXF,
   pass `-` and give `outline_mm`, `header` and `texts` in parts.json.
6. **Catalog.** `tools/pp_web/clicks.json`: image, origin and size (as printed by the
   renderer), and what the page draws on the Click, positions in the Click's mm:
   - `controls`: sliders for a QOM property (with scale),
   - `buttons`: areas that hold a boolean QOM property while pressed,
   - `leds`: with `pin` (a mikroBUS signal) they glow at that pin's duty; without, while powered,
   - `relays`: state from a pin, with a click sound,
   - `display`: a seven-segment display from a polled QOM property (`segments` maps names to
     bits, each digit has a `shift`, `enable_pin` dims it), or an LED matrix (`"type": "matrix"`:
     `rows_mm`, `cols_mm`, `row_bits`, `col_digits` over a "d0 ... d7" hex property),
   - `signals`: lights for pins the Click drives.
7. **Demo and tests.** A firmware demo (`firmware/hello/tests/<name>.c`) and a web session test
   that drives the Click through `Session.set_click` and checks the result.

## Carrier with mikroBUS sockets (Curiosity Nano Base)

The Click sits directly on the carrier, so there is no adapter: compose the board into the
carrier's image (`boards/compose_carrier.py`, see `tools/pp_web/README.md`) and give each socket
header a `socket` (`at_mm`: centre between the two 1x8 rows from the carrier's pick-and-place;
`rotation`: 90 when AN is to the right of the GND/+3.3V end of the row, as on the Nano Base) and
its `mikrobus` pins. The pins come from the carrier's user guide (socket signals to edge pins)
combined with the board's edge-pin table. On the machine side, name the sockets after the
carrier (`mb1=` .. `mb3=`).

## Adapter

Render its Gerbers like a board (`recolor_board.py --parts`), list its parts from the
pick-and-place, and add it to `tools/pp_web/adapters.json`: where its socket meets the board's
header (`header_at_mm`) and how it holds the Click (`socket`: centre and rotation). Map the
Click's AN/PWM pins to the adapter's labelled rows to get the rotation right. Check the pin
mapping against the adapter's user guide; the twin's wiring must follow it (for example INT on
extension pin 9).
