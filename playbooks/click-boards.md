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
   `pic32cm_mc00_cpro_clicks`): name, type, I2C address, INT output, and `int_pullup` when the
   Click has a pull-up on INT. Today the table is per board.
4. **qtest.** Identity registers, data encoding, the property, the INT line with and without the
   pad's own pull (`tests/qtest/pic32cm-click-test.c`).
5. **Art.** `py tools/pp_web/clicks/render_click.py <dxf> <parts.json> out.svg`, then rasterize at
   20 px/mm with a transparent background. `parts.json` lists part bodies by position (the DXF
   has none); find them from the pad groups and the schematic.
6. **Catalog.** `tools/pp_web/clicks.json`: image, origin and size, controls (with the QOM
   property and scale), LEDs, signals and their pin positions on the Click.

## Adapter

Render its Gerbers like a board (`recolor_board.py --parts`), list its parts from the
pick-and-place, and add it to `tools/pp_web/adapters.json`: where its socket meets the board's
header (`header_at_mm`) and how it holds the Click (`socket`: centre and rotation). Map the
Click's AN/PWM pins to the adapter's labelled rows to get the rotation right. Check the pin
mapping against the adapter's user guide; the twin's wiring must follow it (for example INT on
extension pin 9).
