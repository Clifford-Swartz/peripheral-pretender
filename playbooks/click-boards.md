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
3. **Board table.** Add an entry to the board's Click table (`hw/arm/pic32cm_mc00_cpro.c`:
   `pic32cm_mc00_cpro_clicks`). Each entry gives:
   - the device type (or none: the Relay Click has only pins) and its bus (I2C address, or
     SPI on the header's SERCOM),
   - `outs`: device outputs onto mikroBUS signals (RST, PWM, INT, CS),
   - `ins`: mikroBUS signals into device inputs (the 74HC595's LATCH from CS),
   - `pulls`: every pull resistor on the Click, from its schematic. An output behind a pull
     lets go with -1 and the pull sets the pin.
   Generic parts exist for pushbuttons (`click-button`) and shift registers (`74hc595`). The
   table is per board today.
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
     bits, each digit has a `shift`, `enable_pin` dims it),
   - `signals`: lights for pins the Click drives.
7. **Demo and tests.** A firmware demo (`firmware/hello/tests/<name>.c`) and a web session test
   that drives the Click through `Session.set_click` and checks the result.

## Adapter

Render its Gerbers like a board (`recolor_board.py --parts`), list its parts from the
pick-and-place, and add it to `tools/pp_web/adapters.json`: where its socket meets the board's
header (`header_at_mm`) and how it holds the Click (`socket`: centre and rotation). Map the
Click's AN/PWM pins to the adapter's labelled rows to get the rotation right. Check the pin
mapping against the adapter's user guide; the twin's wiring must follow it (for example INT on
extension pin 9).
