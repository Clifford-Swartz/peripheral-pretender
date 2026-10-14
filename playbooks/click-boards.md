# Adding a Click board or an adapter

Worked examples: Thermo 8 Click (MCP9808 on I2C, ALERT on INT) and the mikroBUS Xplained Pro
adapter (ATMBUSADAPTER-XPRO). One of each kind of Click the page can show:

| Click | Part model | What the page does | Pattern for |
|---|---|---|---|
| Thermo 8 | `mchp-mcp9808` (I2C) | temperature slider, ALERT light | a sensor with a value |
| Button G/R/Y | `click-button` | press the button, LED on PWM | GPIO-only Clicks |
| 7seg, 8x8 G/R/Y/B | `74hc595`, `max7219` (SPI, latch on CS) | digits, LED matrix | latch-style SPI parts |
| Relay | none (pins only) | relay state, click sound | Clicks without a chip |
| Buzz | `click-buzzer` (times the PWM pin) | plays the tone (Web Audio) | `sound` |
| OLED C | `ssd1351` + `pp_panel` (SPI with CS) | live picture on a canvas | display controllers (`panel`) |
| Thumbstick | `click-thumbstick` (an `mchp-mcp3204`) | drag the stick, push it | `sticks` (`stick`), chip + extra part |
| Accel | `adxl345` (I2C) | tilt the Click | `sticks` (`tilt`), atomic multi-value set |
| RTC 6 | `mchp-mcp79411` (I2C at two addresses) | the chip's time on the Click | display `text`, timers on the virtual clock |

## Click board

1. **Inputs.**
   - The product page is `https://www.mikroe.com/<slug>`; the slug is not always
     `<name>-click` (RTC 6 is `rtc6-click`). mikroe.com answers `curl` with a Cloudflare
     challenge page: open one product page with Playwright, then list the download links of any
     product page from inside it:
     `browser_evaluate`: `async () => [...new Set((await (await fetch('/<slug>')).text()).match(/https:\/\/download\.mikroe\.com\/[^"' <>]+/g))]`.
     The files themselves (`download.mikroe.com`) download with `curl` into a new scratch folder.
   - You want the "2D and 3D files" zip (a DXF and a STEP), the schematic PDF and the chip's
     datasheet. Mikroe mirrors datasheets at
     `download.mikroe.com/documents/datasheets/<PART>_datasheet.pdf` (analog.com does not answer
     `curl`). Microchip parts: the Microchip docs MCP or the link on the page.
   - Several versions: pages list v100 and v101, ClickID and no-ID files and schematics. Model
     the current one (the ClickID version where there is one) and compare the versions: the
     Accel Click's address jumper defaults to 0x1D on v100 and to 0x53 on v101. Write the
     version into the catalog comment and parts.json.
   - The schematics are vector drawings without extractable text: render them
     (`pdftoppm -r 110 -png`, from poppler; MSYS2 has no `pdftotext`, `pypdf` reads datasheets)
     and read them; render a crop at 300-400 dpi for resistor networks and jumpers
     (`pdftoppm -r 400 -x .. -y .. -W .. -H ..`). Note every pull resistor, its value and the pin,
     the jumpers' default positions and which mikroBUS pins are used.
   - MikroElektronika's own driver (`github.com/MikroElektronika/mikrosdk_click_v2`,
     `clicks/<name>/lib_<name>/`) shows the conventions the datasheet leaves to the board: which
     ADC channel is which joystick axis, the OLED C's 96x96 window in the SSD1351's 128x128
     RAM, the default I2C address (which may disagree with the current schematic: say so).
2. **Part model.** If the chip on the Click has no QEMU model, write one in pp-qemu
   (`hw/sensor/mchp_mcp9808.c` is the pattern): an I2C or SSI target with the registers from its
   datasheet, the physical quantity as a QOM property settable at run time (in integer units,
   such as milli-degrees or mg), and its outputs as GPIO. Open-drain outputs drive 0 or -1
   (released); push-pull outputs drive 0/1. Say in the file comment that no physical Click
   checked it, and list what is not modelled (logged with `LOG_UNIMP` when used).
   - **Time** comes from `QEMU_CLOCK_VIRTUAL` (timers for sample rates, RTC ticks, EEPROM write
     times; edge timing for a PWM pin). The web view runs with icount paced to the host clock,
     so virtual time is real time on the page.
   - **SPI parts with a real chip select** (`SSI_CS_LOW`) get the socket's CS through `ins`
     (`SSI_GPIO_CS`); latch-style parts (74HC595, MAX7219) use `SSI_CS_NONE` and a latch input.
     Each socket's SPI Click gets the socket index as its SSI `cs` index (mikrobus.c does this:
     two SPI Clicks share a bus).
   - **Initial levels:** `mikrobus_plug()` sets the pulls before it connects `ins`, so a device
     does not see the pulled level at start: reset it to the idle state of its pulled pins (CS
     high, RES# released, panel supply off).
   - **A chip plus another part** (a joystick and its switch): a catalog entry has one device,
     so make a subclass of the chip model that adds the part (`click-thumbstick` is an
     `mchp-mcp3204` with x/y/pressed and a switch output).
   - **Two I2C addresses** (MCP79411: 0x6F RTCC, 0x57 EEPROM): override `match_and_add` and
     remember which address was matched. QEMU does not rescan the bus on a repeated START, so a
     switch of address needs a STOP. The catalog's address check knows only the first address.
   - **Values that change together** (an accelerometer's x, y, z): add one property that sets
     them at once (`xyz`, "x y z"), or the firmware can sample a half-updated vector.
   - **Displays:** keep the controller's RAM and settings in the model and give it a `PpPanel`
     (`include/hw/display/pp_panel.h`): it composes the visible picture on demand and adds
     `frame` (base64 RGB565) and `frame-gen` (bumped on every change). An ST7789 or other
     controller only needs its command set and a render callback. Where the panel sits in the
     controller's RAM (segments, commons) is a property with the Click's values as defaults.
   - New files: `hw/<area>/<part>.c` and `include/hw/<area>/<part>.h`, built with
     `when: 'CONFIG_MIKROBUS'` in that area's `meson.build`.
3. **Catalog entry.** Add the Click to the shared catalog `mikrobus_clicks[]` in pp-qemu
   `hw/misc/mikrobus.c` (every board with mikroBUS sockets uses it). Each entry gives:
   - the device type (or none: the Relay Click has only pins) and its bus (I2C address, or
     SPI on the header's SERCOM),
   - `outs`: device outputs onto mikroBUS signals (RST, PWM, INT, CS),
   - `ins`: mikroBUS signals into device inputs (the 74HC595's LATCH from CS, the SSD1351's D/C
     from PWM, an SSI chip select), up to four,
   - `pulls`: every pull resistor on the Click, from its schematic, up to four. An output
     behind a pull lets go with -1 and the pull sets the pin. A transistor's base resistor
     (Relay, Buzz) counts as a pull-down. A pull on a bus line (SCL/SDA) is not listed.
   Generic parts exist for pushbuttons (`click-button`), buzzers (`click-buzzer`), shift
   registers (`74hc595`) and the MAX7219 LED driver (`max7219`); colour variants (8x8 R/Y/B,
   Button R/Y) are new entries with the same device. A board describes its sockets
   (`MikrobusSocket`: the pads behind RST/PWM/INT/CS, the I2C and SPI buses) and calls
   `mikrobus_plug()`; a socket without the bus a Click needs refuses it. Board option help texts
   take the list from `mikrobus_click_names()`, so they need no change. Document the Click in
   the boards' rst files (`docs/system/arm/pic32cm-mc00-cpro.rst` describes each Click,
   `docs/system/avr/avr64ea48-cnano.rst` lists them).
4. **qtest.** Identity registers, data encoding, the property, the INT line with and without the
   pad's own pull (`tests/qtest/pic32cm-click-test.c`); timing with `qtest_clock_step` (keep
   the arithmetic in 64 bits: `SCALE_MS` is an `int`). Test the Click on the AVR64EA48 Nano Base
   too (`tests/qtest/avr64ea48-cnano-test.c`: SPI0, TWI0 helpers). Build and run in the build
   folders:
   `ninja qemu-system-arm.exe tests/qtest/pic32cm-click-test.exe`, then
   `QTEST_QEMU_ARGS="-L <pp-qemu>/pc-bios" QTEST_QEMU_BINARY=./qemu-system-arm.exe ./tests/qtest/pic32cm-click-test.exe`
   (`build-avr`, `qemu-system-avr.exe`, `avr64ea48-cnano-test.exe` for AVR); run `qom-test`
   on both, since it creates and finalizes every new device type.
5. **Art.** `py tools/pp_web/clicks/make_art.py <dxf> <parts.json> clicks/<id>/top.png
   --resvg <scratch folder with node_modules/@resvg/resvg-js>` renders (`render_click.py`),
   rasterizes at 20 px/mm with a transparent background and fixes resvg's rounding; it prints
   `origin_mm` and `size_mm` for clicks.json. DXFs in inches are scaled automatically.
   - `parts.json` lists part bodies by position (the DXF has none). Render once with
     `{"parts": []}`, read the silkscreen labels, and get pad positions from
     `py tools/pp_web/clicks/dxf_pads.py <dxf>` (a body sits over its pad pair). Kinds: ic,
     resistor, capacitor, led, zero_ohm, button, relay, terminal, digit, buzzer, oled (a display
     module with `active_mm`/`active_at_mm`), joystick, battery, crystal, silk_label.
   - Parts not in the DXF: display modules (the OLED C's panel: draw it from the panel's
     specification), parts on the bottom side (the Thumbstick's ADC). Jumpers: draw only the
     fitted position.
   - Check the DXF really is the Click's: the 7seg Click's page links the 7SEG RGB Click's files,
     the 8x8 B Click's page the 8x8 R Click's. For a colour variant of the same board, render the
     sibling's DXF and cover its letter with a `silk_label` part. Without a usable DXF, pass `-`
     and give `outline_mm`, `header` and `texts` in parts.json.
   - Write the source (zip and DXF names, versions, what was estimated) into parts.json.
6. **Catalog.** `tools/pp_web/clicks.json`: image, origin and size (as printed), and what the
   page draws on the Click, positions in the Click's mm (y up; `at_mm` is a centre):
   - `controls`: sliders for a QOM property (with scale),
   - `buttons`: areas that hold a boolean QOM property while pressed,
   - `leds`: with `pin` (a mikroBUS signal) they glow at that pin's duty; without, while powered,
   - `relays`: state from a pin, with a click sound,
   - `display`: a seven-segment display from a polled QOM property (`segments` maps names to
     bits, each digit has a `shift`, `enable_pin` dims it), an LED matrix (`"type": "matrix"`:
     `rows_mm`, `cols_mm`, `row_bits`, `col_digits` over a "d0 ... d7" hex property), a display
     panel (`"type": "panel"`: `property` "frame", `gen_property` "frame-gen", `width`,
     `height`, `at_mm`, `size_mm` of the active area) or a text readout (`"type": "text"`: a
     string property, `color`),
   - `sound`: a buzzer (`property` frequency in Hz, `duty_property` per mille, `at_mm`,
     `size_mm`); the page plays it with Web Audio after the first click on the page,
   - `sticks`: a knob to drag (`id`, `label`, `at_mm`, `size_mm`, `range`): `"mode": "stick"`
     sets `x_property`/`y_property` (springs back; `press_property` while held still),
     `"mode": "tilt"` leans the Click and sets the reading at rest (`xyz_property`, or
     `x_`/`y_`/`z_property`); see `server.stick_values()`,
   - `signals`: lights for pins the Click drives (INT only).
   What the page polls comes from `server.watched()` (display, sound); add new kinds there.
   A long Click may not fit a board's stage: `test_board_description*` checks every Click on
   every header; widen that board's `view_mm`.
7. **Demo and tests.** A firmware demo (`firmware/hello/tests/<name>.c` for the MC00; the I2C
   ones share `tests/ext1_i2c.h`) and a web session test. Prefer driving the Click through the
   server (`server.App(board, elf)`, `await app.plug(slot, id)`, then `app.control` /
   `app.stick` and `app.uart_log`), which also tests what the page sends; `Session.set_click`
   tests the model alone. Build the demo, add it to the list of apps you build before the gate
   (`make BOARD=pic32cm_mc00_cpro APP=<name>`): without the ELF the test skips.
8. **Look at it.** `py -m tools.pp_web --elf <demo> --port <free port>`, plug the Click from the
   page's picker, and check it on every board with sockets (the Click is turned 90 degrees on
   the adapter and on the Nano Base). Playwright saves screenshots only inside the main
   checkout's `.playwright-mcp` folder (gitignored): delete yours when done.
9. **Hand-back.** One pp-qemu commit per Click (`git format-patch -1 --start-number <N> -o
   qemu/patches`), a row in `qemu/README.md`, a paragraph in `tools/pp_web/README.md`, and the
   full gate (`playbooks/onboard-board.md` Phase 5) before each repository commit.

## AVR boards: what a Click needs from the chip

The Nano Base's sockets bring SPI0, TWI0 (I2C), and RST/PWM/INT/CS pins. A Click needs the AVR
peripheral behind its bus: SPI (`avr8x-spi`) and TWI host (`avr8x-twi`) exist; a new one needs an
overlay (`overlays/<ip>_<id>.yaml`), a regeneration of every device, the model in pp-qemu, a
line in `avr8x_models[]` and the new overlay in `EXPECTED` of `tests/generator/test_overlays.py`.
PWM from TCA is not modelled yet: a Buzz Click on the Nano Base sounds only if the firmware
toggles its PWM pin from a timer interrupt.

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
