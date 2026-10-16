# pp_web: the twin on a picture of its board

A web page that shows the board, runs your firmware on the twin, and updates the board as the
firmware runs: LEDs light as the pins drive them, SW0 and RESET respond to clicks, and the
VCOM UART appears in a console. This is the first slice of the endgame in
`docs/design/endgame.md`.

```
make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=blinky     # or APP=breathe (PWM)
py -m tools.pp_web --elf firmware/hello/build/pic32cm_mc00_cpro/blinky.elf
# open http://127.0.0.1:8080/ ; drop another ELF on the page to switch firmware
```

You need a pp-qemu build (`PP_QEMU`, as for `tools.pp`) and `aiohttp`.

## With MPLAB for VS Code (or MPLAB X)

Point the page at a firmware project and every build shows up on the board view:

```
py -m tools.pp_web --watch <project folder>
```

The watcher looks for the newest `out/**/*.elf` (MPLAB for VS Code writes
`out/<project>/<configuration>.elf`) or `dist/**/*.elf` (MPLAB X), waits until the file stops
changing, copies it and restarts the twin with it. Plugged Clicks and their settings carry over.
`--watch` also takes a single ELF.

Checked with Microchip's own example for this board, `csp_apps_pic32cm_mc00`
`apps/port/port_led_on_off_polling` (an MCC/Harmony project built with XC32 v4.45). Unchanged,
it runs on the twin (LED0 follows SW0), and the same hex programmed the EV15N46A through
MPLAB IPE over the on-board EDBG:

```
ipecmd -P32CM1216MC00048 -TS<EDBG serial> -F<project>.hex -M -OL
```

In VS Code, install the "MPLAB" extension pack (`Microchip.mplab-extension-pack`), open the
project folder and import the `.X` project when asked. Build, Program and Debug then come from
MPLAB, and with `--watch` on that folder the page follows every build.

## How it works

- `session.py` starts QEMU with the ELF under icount paced to the host clock
  (`-icount shift=auto,sleep=on`), so it runs in real time with timer events at exact virtual
  times. It connects to the QMP monitor and to the VCOM UART, a socket chardev on SERCOM0.
  Each frame (30 Hz) it reads the PORT's `pa-stats`/`pb-stats`, the total driven-high and
  driven-low time of every pad, and sends each pad's high and low fraction of the frame. An
  LED therefore shows its PWM duty cycle as brightness. Buttons map to
  `qom-set /machine sw0-pressed` and `system_reset`.
- `server.py` (aiohttp) serves the page, accepts `POST /api/firmware` with an ELF, and relays
  messages over `/ws`.
- `static/` is the page. It places the board's LEDs and buttons from `board.json`.
- Click boards: `clicks.json` is the catalog (see `playbooks/click-boards.md` for its entries). Picking one for a
  header restarts the twin with `-M <machine>,ext1=<click>`, like plugging it in with the power
  off. Its controls (the temperature) are `qom-set`s on `/machine/ext1-click` while it runs.
  Its signals (ALERT on INT) light from the PORT's `pa-levels`/`pb-levels`, which include
  external drivers and pulls.

  ```
  make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=thermo
  py -m tools.pp_web --elf firmware/hello/build/pic32cm_mc00_cpro/thermo.elf
  # pick "Thermo 8 Click" for EXT1, then move the temperature above 30 C
  ```

## The parts bench

Everyday parts plug into the board itself: resistors, LEDs (and an RGB LED), a diode, a push
button, a switch, a potentiometer, a DC motor, a logic-level N-MOSFET, a servo and a voltmeter.
They sit on or beside the board picture, and jumper wires run from their legs into the holes of
the board's headers (`board.json` `"pin_headers"`: the Arduino, EXT and power headers on the
MC00, J9 on the PyKit), each tied to its pad or to GND, 3V3 or 5V:

- **+ part…** (the toolbar under the board) adds a part beside the board. Drag it onto a
  header and drop it: its legs plug into the holes nearest them (green while you drag), bent
  in like a real part pushed into a header, so an LED dropped on D13 and GND sits in those two
  pins. **R** turns the selected part (to put the anode where you want it); drag it away to
  unplug it; right-click a leg to pull just that one out.
- Or run a jumper: click a leg, then a header hole (hover shows the pin's name, pad and
  voltage); Esc cancels. A leg can also go to another leg, and a hole to another hole.
- **+ pin tag…** puts a pad with no header hole (or a rail) on the page as a tag to wire to.
- Drag parts and tags. Select a part to edit its values in the toolbar; right-click (or select
  and Delete) to remove things. Wires in use light their holes (red while the pad is high).
- While the twin runs: hold a button, drag a pot's knob, double-click a switch. LEDs glow with
  their current, a motor spins at its speed, a servo's horn follows its pulse width (a 50 Hz
  frame is assumed), the meter reads, wires are coloured by voltage and hovering shows it.
  Warnings: an LED with no resistor, a motor on a bare pad, a pad over its current, a short.

`bench.py` solves the circuit as DC each frame: nodal analysis with Newton steps for the
diodes. A pad the firmware drives is VDD or 0 V behind about 40 ohm; a PWM pad is solved high
and low and weighted by its duty cycle. An input pad is open, with its pull resistor if the
PORT has one on (`pX-pull`). The results go back into the twin: an input pad wired to the
bench reads the level its net settles at (`pX-bench`, a drive layer over the Clicks' and
buttons'; hysteresis at 40/60 % of VDD), and a pad with an ADC channel reads its net's voltage
(the ADC's `ain`: the PyKit's SAM D5x ADC and the MC00/JH01's PIC32CM ADC). Pads a peripheral
drives with a voltage (`board.json` `"analog_out"`: the MC00's DAC on PA02, J700) are sources
behind their output resistance, so a meter or an LED on DAC OUT follows the firmware. Not modelled: anything
that changes in time (capacitors, inductors, motor inertia). That is the analog step, with
ngspice, later.

`firmware/hello` has a demo for it on the MC00: `make BOARD=pic32cm_mc00_cpro APP=shield`
blinks D13 at 2 Hz, at 8 Hz while D2 is pulled low. Plug an LED into D13/GND and a button into
D2 with its other leg on GND.

Each board's bench is kept in `~/.pp_web/bench-<board>.json` (or `PP_WEB_BENCH_DIR`), and
`--bench <file>` starts with one, so a bench can live in a project and run headless. Part
positions are millimetres from the top-left corner of the page's stage (the board's `view_mm`). Example,
on the PyKit with a `code.py` that reads D0 and A4 and lights D1:

```json
{"parts": [{"id": "sw1", "type": "button", "x": 190, "y": 52},
           {"id": "r1", "type": "resistor", "x": 220, "y": 52, "ohms": 330},
           {"id": "led1", "type": "led", "x": 245, "y": 52, "color": "green"},
           {"id": "pot1", "type": "pot", "x": 270, "y": 52, "position": 0.25}],
 "wires": [["sw1.a", "hole:J9B.17"], ["sw1.b", "hole:J9B.20"],
           ["r1.a", "hole:J9B.16"], ["r1.b", "led1.a"], ["led1.k", "hole:J9B.9"],
           ["pot1.a", "hole:J9T.25"], ["pot1.b", "hole:J9T.11"],
           ["pot1.w", "hole:J9T.19"]]}
```

## Data Visualizer panel

On a board whose debugger has a data gateway (`board.json` `"dgi"`: the MC00 Curiosity Pro so far)
the twin's EDBG speaks the DGI command protocol on a socket, the same bytes a real debugger
exchanges over USB with MPLAB Data Visualizer (`hw/misc/mchp_edbg_dgi.c`, `tools/pp/dgi.py` is a
client). The server signs on and the side panel shows what the kit's DGI offers: tick SPI, I2C
or GPIO and the server enables and polls them.

- Byte streams are decoded as Data Visualizer's Data Stream packets (0x03, fields, 0xFC) with
  the fields typed in the box (`u16`, `u8,i16,f32`, ...) and plotted; raw bytes show below.
- GPIO lines (MC00: GPIO0-3 on PA10, PA11, PA20, PA21) are drawn from the debugger's timestamps
  (2 MHz), with each line's period.

`make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=dgi` is the demo: a counter as Data Stream
packets to the DGI over I2C every 10 ms, and GPIO0 toggling. MPLAB Data Visualizer itself finds
DGI tools over USB only; letting it use the twin's socket is a change on its side.

It is a local, single-user prototype: one twin is shared by every open page. Hosting
(one sandboxed twin per visitor) is a later step.

## Board assets: `boards/ev15n46a/`

`top.png` is rendered from Microchip's production Gerbers for the PIC32CM MC00 Curiosity Pro
(design files R3, from the EV15N46A product page: "PIC32CM MC00 Curiosity Pro Evaluation Kit
Design Documentation"). It was made with tracespace 4.2.1: every layer except the Mechanical 2
drawing frame. `recolor_board.py` then gives it the board's red solder mask (copper showing
through as traces, gold pads) and crops it to the board edge, and resvg rasterizes it at
20 px/mm.

Part positions in `board.json` come from the pick-and-place file of the same release. Its
coordinates are the Gerber coordinates in mm, checked against the paste pads of U300 and
D700. `origin_mm` is the image's top-left corner in those coordinates (y points up on the
board, down in the image). Designators: LED0 = D700 (PA24, active low), SW0 = SW700
(PA25), RESET = SW301, EXT1 = J200, EXT2 = J201.

## Board assets: `boards/ev81x90a/`

The PIC32CM JH01 Curiosity Pro (`py -m tools.pp_web --board ev81x90a --elf
firmware/hello/build/pic32cm_jh01_cpro/blinky.elf`). `top.png` is rendered from
Microchip's Gerbers in "PIC32CM_JH01_Curiosity_Pro_R4.zip" (design
documentation R4, from the EV81X90A product page; inside it, "PIC32CM JH0_R4_
Gerber Files.zip"). tracespace 4.2.1 took the copper (GTL, G1, G2, GBL), mask,
paste and silkscreen layers and the two NC drill files (`-RoundHoles.TXT`,
`-SlotHoles.TXT`); `.DRL` is a binary drill drawing and is left out. This
release has no outline layer: the board edge is the 60 x 100 mm rectangle the
top silkscreen draws from (0, 0), and copper text above y = 100 mm is outside
the board. The mask is red like the EV15N46A's (product photo), so the
defaults apply:

```
py tools/pp_web/boards/recolor_board.py PIC32CMJH_Curiosity_Pro_R4.top.svg out.svg     --edge 0,0,60000,100000 --flip 108446
```

`--flip` is the y of tracespace's `translate(0,<flip>) scale(1,-1)` (its
viewBox height less 100). Positions come from the R4 pick-and-place file.
Designators: LED0 = D700 (PC05, active low), SW0 = SW700 (PB19), RESET = SW301,
EXT1 = J200, EXT2 = J201, EXT3 = J202 (INT pins PA21, PC24, PA28; user guide
DS70005482B tables 4-4 to 4-6). The `pic32cm-jh01-cpro` machine has no Click
slots yet, so the headers carry no `slot`. LED0 is on port C: the server
polls every PORT group a board's LEDs and INT pins use (`pin_groups`).

## Board assets: `boards/ev66e56a/`

The AVR64EA48 Curiosity Nano (EV66E56A) plugged into a Curiosity Nano Base for Click boards
(AC164162): `py -m tools.pp_web --board ev66e56a --elf firmware/avr-hello/build/blinky.elf`
(the twin runs on `qemu-system-avr`; see `tools/pp/qemu.py`). A Curiosity Nano always sits on
a carrier here, so `top.png` is one picture of both, in the base's coordinates:

```
tracespace -o nano <Nano Gerbers: PCBA rev 3, Gerber/ + NC Drill/*Holes.TXT>
tracespace -o base <Base Gerbers: PCBA rev 7, Gerber/ + NC Drill/*-RoundHoles*.TXT>
py tools/pp_web/boards/recolor_board.py nano/AVR64EA48_Curiosity_Nano.top.svg nano.svg --edge 0,0,83820,20320
py tools/pp_web/boards/recolor_board.py base/Curiosity_Nano_Base_for_Click_boards_.top.svg base.svg --edge 0,0,74930,107950
py tools/pp_web/boards/compose_carrier.py base.svg 74.93,107.95 nano.svg 83.82,20.32 --place 8.89,107.32,90 > top.svg
```

then resvg at 20 px/mm (resvg gave 2160 px for 107.95 mm; the PNG is resized to 2159).
`--place`: the Nano's pick-and-place origin lands at (8.89, 107.32) on the base, turned 90
degrees clockwise: its pins 1-28 sit on J17 (x = 11.43, pin 1 at y = 99.70) and 56-29 on J18
(x = 26.67). `compose_carrier.py --point u,v` converts Nano positions (LED0 D200 (74.22, 10.18),
SW0 SW200 (80.39, 10.16)) to base mm for `board.json`. The three mikroBUS sockets (J3/J4, J5/J6,
J7/J8, centres from the base's pick-and-place) are headers with `socket` instead of `adapter`:
the Click's header centre goes on the socket, turned 90 degrees (AN to the right, as the base's
silkscreen shows). Pins per socket: Nano Base user guide DS50002839B Figure 3-1 through the
Nano's edge pins (schematic sheet 2).

## Board assets: `boards/pykit/` (a CircuitPython board)

The Curiosity PyKit Explorer: the Curiosity CircuitPython board (SAME51J20A) on the PyKit Ruler,
machine `curiosity-circuitpython`. `py -m tools.pp_web --board pykit --port <port>`. The page
runs Adafruit's stock CircuitPython for the board (`board.json` `"circuitpython"`: version and
board id; fetch it once with `py -m tools.pp.circuitpython fetch`) with a CIRCUITPY drive built
from `circuitpy/` (`tools/pp/fatfs.py`; the twin's QSPI flash image, `-drive if=mtd`). The
CIRCUITPY panel shows the drive's files and an editor for `code.py`: "Save and restart" writes
the file into the image and restarts the twin (the twin holds the image while it runs). VCOM is
the USB CDC console (CircuitPython's REPL and `print`). Terminal control sequences (the title
CircuitPython sets) are dropped; the console is UTF-8.

`circuitpy/code.py` is the demo, built-in modules only: a bubble level on the TFT from the
ICM-20948, the NeoPixels follow the tilt, the LED blinks, and holding USER turns the bubble red
and plays a C5 on the speaker.

No design files (Gerbers) are published for either board, so `top.png` is composed from the
renders in the kit's workshop slides by `make_art.py` (its docstring has the scales and the
placement), 10 px/mm instead of 20. Positions come from those renders; pins from the schematics.

### Board parts (`"parts"` in `board.json`)

A board's own devices (not Clicks) that the page shows or drives. Each has an `id`, a `name`
and the QOM `path` of its device; `also` lists more paths a control writes to (the IMU tilt also
goes to an optional BNO085). Kinds, any combination:

| Key | What the page does | Polled as |
|---|---|---|
| `display` (`type: panel`) | draws a `pp_panel` frame (base64 RGB565) at `at_mm`, `size_mm` | `<id>.<property>`, re-read when `gen_property` changes |
| `pixels` | a glow per LED at each of `at_mm` from `"RRGGBB RRGGBB ..."` | `<id>.<property>` (with `gen_property`) |
| `sticks` | a stick control like a Click's (`mode: tilt`, `range`, `xyz_property`) | written with `qom-set` on `path` and `also` |
| `audio` | a speaker: each new chunk of `"<rate> <seq> <base64 int16>"` is queued in Web Audio, DC removed; click it to mute | `<id>.<property>` every frame |

Buttons may name a machine bool `property` (`"user-pressed"`); without one, `sw0` means
`sw0-pressed` and `reset` is `system_reset`.

## Click assets: `clicks/<id>/`

`top.png` is the Click's top side, rendered by `clicks/render_click.py` from MikroElektronika's
"2D and 3D files" for the board (Thermo 8 Click v100: `Thermo_8_click_v100.zip`, from the
Downloads section of the product page). The DXF gives the outline (GKO), the silkscreen (GTO)
and the mask openings (GTS). It has no part bodies, so `parts.json` lists them by position,
identified from their pads and the schematic. The DXF coordinates are mm. `origin_mm` and
`size_mm` in `clicks.json` place the image, and LED and signal positions (`at_mm`) are in the
same coordinates. The zip also holds a STEP model for the 3D view later.

The 8x8 G Click (`clicks/8x8g/`) shows an LED matrix: `display` `{"type": "matrix"}` reads the
MAX7219's `display` property ("d0 ... d7", hex) and lights dot (row r, column c) from bit
`row_bits[r]` of digit `col_digits[c]`. Rows and columns follow the schematic (DIG7 left, DP at
the top); the physical LED order is not confirmed. LED positions come from the DXF's pads.

The Buzz Click (`clicks/buzz/`) has a `sound` entry instead of a display: the page polls the
buzzer's `frequency` and `duty` properties (`server.watched()` lists what a Click entry polls)
and plays a square wave at that pitch through Web Audio, louder the closer the duty is to 50 %;
rings and the pitch show on the buzzer, and clicking it mutes the page. Browsers start audio
only after a click on the page. Demo: `make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=buzz`,
then pick "Buzz Click" for EXT1 (TCC0 plays C5 E5 G5 C6 on PB10; hold SW0 for A4).

The OLED C Click (`clicks/oledc/`) has a `display` of type `panel`: a canvas over the glass
(`at_mm`, `size_mm`) showing `width` x `height` pixels from the controller's `frame` property
(base64 RGB565). The session polls `gen_property` (`frame-gen`) every frame and fetches the
picture only when it changes. Any display controller that keeps a `PpPanel` in pp-qemu
(`include/hw/display/pp_panel.h`) works the same way, so a TFT (ST7789) needs only its model and
a catalog entry. The PSP27801 panel is not in Mikroe's DXF (only its FPC pads are): it is drawn
from the panel's specification (25.8 x 30.1 mm glass, 20.14 mm active area); where the active
area sits on the glass is an estimate. Demo: `make -C firmware/hello BOARD=pic32cm_mc00_cpro
APP=oledc`, then pick "OLED C Click" for EXT1 (colour bars and a bouncing square; SW0 inverts).
The long Click (57 mm) needed wider stages (`view_mm`) on EV15N46A and EV66E56A.

The Thumbstick Click (`clicks/thumbstick/`) has a `sticks` entry: a knob over the joystick
(`at_mm`, `size_mm`) to drag. The page turns the pointer's offset back by the Click's rotation on
the board and sends `{"type": "stick", "x", "y", "pressed"}` (unit circle, y up); the server
sets the Click's properties through `stick_values()` (`"mode": "stick"`: the axes times
`range`). The knob springs back when let go; holding it still for 250 ms pushes it
(`press_property`). Demo: `make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=thumbstick`,
then pick "Thumbstick Click" for EXT1 (the VCOM prints the axes and the direction; LED0 lights
while the stick is pushed).

The Accel Click (`clicks/accel/`) uses the same stick in `"mode": "tilt"`: dragging leans the
Click toward the pointer (drawn with a CSS 3D tilt; it stays where it is let go, double-click
levels it). `stick_values()` turns the lean into what the ADXL345 measures at rest, the
opposite of gravity: the axis that dips reads negative, z the rest of 1 g (`range` mg). Axes
as on the Click's silkscreen (x right, y up). Demo: `make -C firmware/hello
BOARD=pic32cm_mc00_cpro APP=accel` (EXT1; prints the reading and which side is down, LED0
while it leans more than 30 degrees). I2C Clicks now plug into the Nano Base too (TWI0).

The RTC 6 Click (`clicks/rtc6/`) has a `display` of type `text`: the MCP79411's `time`
property, polled every frame, in a readout over the Click (`at_mm`, `size_mm`, `color`). Its
MFP lights the INT signal while low. Demo: `make -C firmware/hello BOARD=pic32cm_mc00_cpro
APP=rtc6` (EXT1; starts the stopped clock at 2026-10-14 12:00:00, prints the time each second
and an alarm every five seconds, LED0 with each alarm). The model starts from the chip's
power-on state like a Click without a battery; the firmware sets the time.

The POT Click (`clicks/pot/`) is the first Click on the analog pin AN: its `Knob` slider sets
the `click-pot`'s `position`, and the wiper's voltage (0-2.048 V, the MCP1501 reference) goes
to the ADC channels behind the socket's AN (EXT1 PB08 = ADC0 AIN2). Demo: `make -C
firmware/hello BOARD=pic32cm_mc00_cpro APP=pot` (EXT1; prints the voltage when it moves, LED0
above half travel). AN is not wired on the Nano Base yet, so the POT Click plugs into the MC00
only.

The GNSS Click (`clicks/gnss/`) is the first UART Click: its L86 module is a chardev on the
headers' UART (MC00 SERCOM3; Nano Base sockets 1 and 3, USART2), reachable at
`/machine/ext1-click` like any Click. Sliders set latitude, longitude, altitude and the
satellites in use (below 4 there is no fix). Demo: `make -C firmware/hello
BOARD=pic32cm_mc00_cpro APP=gnss` (EXT1; echoes the boot lines, sends PMTK314 and prints the
acknowledgement, the first RMC sentence, then each GGA's position, as MikroElektronika's
example parses it). One UART Click at a time: the MC00's EXT1 and EXT2 share their UART.

The Rotary G, R, B and Y Clicks (`clicks/rotary*/`) bring a `display` of type `bits`: LEDs at
`at_mm`, each lit by bit `bit` of a polled property (the 74HC595s' `outputs`; bit n is LED
n+1). The `Knob` slider turns the encoder to a detent position, one detent at a time; the
shaft is a button (push). The renderer now follows arcs in DXF outlines (the Rotary Click's
round top) and turns parts by `rotate_deg` (the ring's LEDs). Demo: `make -C firmware/hello
BOARD=pic32cm_mc00_cpro APP=rotary` (EXT1; counts detents as MikroElektronika's driver does and
lights the LED of the position, the whole ring while pushed). The Rotary O Click's files are
not downloadable, so it is not in the catalog.

Colour variants: 8x8 R, Y and B and Button R and Y share their device with 8x8 G and Button G
and have their own art and LED colours. The 8x8 B Click's page links the 8x8 R Click's 2D files,
so its art is the 8x8 R DXF with the silkscreen letter replaced (`silk_label`).
`clicks/make_art.py` renders and rasterizes a Click in one step, `clicks/dxf_pads.py` lists a
DXF's pads for placing part bodies.

## Adapter assets: `adapters/<id>/`

The Curiosity Pro's EXT1/EXT2 are Xplained Pro extension headers, so a Click needs an adapter:
the mikroBUS Xplained Pro (ATMBUSADAPTER-XPRO, user guide DS50002671A). Its edge socket J100
plugs face down onto the header, so the adapter lies flat beyond the board edge. The Click
sits on J103/J104 turned 90 degrees (AN and PWM toward the far end); a medium-size Click
overhangs the adapter. Mapping: INT on extension pin 9, I2C on 11/12, SPI on 15-18, UART on
13/14, RST on 5, PWM on 7, AN on 3.

`top.png` is rendered from the adapter's production Gerbers (design documentation rev4, from
the product page) with tracespace, recoloured blue with part bodies from `parts.json`
(pick-and-place positions):

```
py tools/pp_web/boards/recolor_board.py <tracespace top.svg> out.svg --mask "#1d4fa0" \
    --palette blue --opacity 0.85 --unit in --edge=-220.5,0,2189,1181.1 \
    --parts tools/pp_web/adapters/mikrobus-xpro/parts.json
```

`adapters.json` gives each adapter's image (`origin_mm`, `size_mm` in its pick-and-place
coordinates), the centre of the socket that goes on the board's header (`header_at_mm`), and
where and how it holds the Click (`socket`: where the Click's mikroBUS header centre goes, and
the rotation). A Click's header centre is (11.43, 8.89) in its own coordinates unless
`header_center_mm` says otherwise. A header in `board.json` names its `adapter`;
the page lines the socket up with the header (`at_mm`) and draws the Click on top. A header
without an adapter takes the Click at `click_at_mm`. `view_mm` widens the stage beyond the
board so the stack fits. Its PWR LED glows while the
twin runs, and a signal pin (ALERT on INT) glows while the pad is at its active level.
