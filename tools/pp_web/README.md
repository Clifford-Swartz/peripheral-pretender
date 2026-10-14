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

It is a local, single-user prototype: one twin is shared by every open page. Hosting
(one sandboxed twin per visitor) is a later step.

## Board assets: `boards/ev15n46a/`

`top.png` is rendered from Microchip's production Gerbers for the PIC32CM MC00 Curiosity Pro
(design files R3, from the EV15N46A product page: "PIC32CM MC00 Curiosity Pro Evaluation Kit
Design Documentation"). It was made with tracespace 4.2.1: every layer except the Mechanical 2
drawing frame, cropped to the board edge, and rasterized at 20 px/mm with resvg.

Part positions in `board.json` come from the pick-and-place file of the same release. Its
coordinates are the Gerber coordinates in mm, checked against the paste pads of U300 and
D700. `origin_mm` is the image's top-left corner in those coordinates (y points up on the
board, down in the image). Designators: LED0 = D700 (PA24, active low), SW0 = SW700
(PA25), RESET = SW301, EXT1 = J200, EXT2 = J201.
