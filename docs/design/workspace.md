# The workspace: boards, Clicks and parts on one bench

Status: built (2026-10-22). Owner's ask:
- open where you left off;
- start from an empty bench that invites you to add a chip;
- more than one chip at a time;
- a searchable, draggable palette instead of dropdowns;
- Click boards you can wire as you like.

## 1. Model

A **workspace** is a bench in millimetres (x right, y down) holding:

- **boards**, each a twin with its own QEMU: a key (`b1`, `b2`…), the board type, where its view's
  top-left sits on the bench (`at`), its Clicks on mikroBUS headers, and the firmware it ran last;
- the **parts bench**: parts, Click boards wired pin by pin (part type `click`), pad and rail
  tags, and wires.

A pad or header hole names its board: `pin:b2/PA05`, `hole:b2/J801.6`. Ends without a key (from
before boards had keys) are the first board's. Rails (`3V3`, `5V`, `GND`) are shared.

The engine keeps the workspace in a file: `workspace.json` in `PP_WEB_BENCH_DIR` (the extension's
global storage), `~/.pp_web`, or `--workspace FILE`. It looks like this:

```json
{"version": 2,
 "boards": [{"key": "b1", "type": "ev15n46a", "at": [0, 0],
             "clicks": {"ext1": {"id": "oledc", "values": {}}},
             "firmware": ".../firmware/hello.elf"}],
 "bench": {"parts": [], "wires": [], "anchors": {}},
 "selected": "b1",
 "watched": {"folder": "...", "build": [".../out/app/default.elf", 1792700000000000000, 34792]}}
```

An uploaded ELF, or a build the watcher ran, is copied beside the file (`firmware/`), so it runs
again next time. A board with no firmware recorded runs its demo. Templates
(`tools/pp_web/templates.json`) set up one board, its Clicks, a bench and firmware.

`watched` is the newest build of the watched folder (`--watch`, the VS Code folder) when the
engine last looked: path, mtime in ns, size. At start-up every board runs what it ran last; only
a build that differs from `watched` (made while the bench was closed) takes the selected board.
A workspace that has never recorded one treats existing builds as old, except a brand-new
workspace, which runs one.

## 2. Engine (`tools/pp_web/server.py`)

- `App` is the workspace: twins, the shared bench, persistence, the page's WebSocket. With one
  board it answers for it (`session`, `run`, `plug`…), so single-board code and tests read as
  before.
- `Twin` is one board: Clicks, firmware, QEMU session, DGI, console. Its messages carry
  `"twin": key`.
- **Bench.** The bench is solved jointly (DC) from every board's pads, and each board gets its
  inputs (`pX-bench`) and ADC voltages. Boards wired together are therefore coupled at the
  engine's frame rate (about 30 Hz): right for buttons, LEDs and slow signals. UART, SPI or
  timing-exact links between boards would need the boards in lockstep (the `pp-cosim` port, as
  for ngspice); that is a later step. ngspice runs the bench only when one board is on it.
- **Page → engine:**
  - `add-board {board, at}`, `remove-board`, `move-board {twin, at}`, `select {twin}`;
  - `template {id}`; `select-board {board}` replaces the selected board (null: none);
  - board-scoped requests (`button`, `uart`, `click`, `control`, `stick`, `save-file`, `dgi`)
    carry `twin`.
- **HTTP:**
  - `POST /api/firmware?twin=` (default: the selected board);
  - `POST /api/board {"board", "add"}`;
  - `GET /api/state`: the selected board, and all of them.

## 3. Click boards wired pin by pin

A Click part has the 16 mikroBUS pins as terminals, laid out from the catalog's header geometry.
The engine follows each signal pin's wire to a pad, and checks two things:
- **Power:** 3V3 or 5V, and GND.
- **Buses:** SPI on one SERCOM's DOPO pairs, I2C on PAD0/PAD1, using the device's signal table.

A Click that passes is plugged into its board's QEMU with `-M ...,wired=click1=oledc:cs.PA17:...`;
a change of wiring restarts that board.

In pp-qemu, `pic32cm_soc_wire_click()` builds a mikroBUS socket from the pads:
- **GPIO:** inputs listen to the pads through `mchp_port_pad_listen()`, so they can share a pad
  with a board LED.
- **AN:** the ADC inputs on that pad.
- **SPI and I2C:** a private bus (`pp-bus-hub`), with a tap on every SERCOM that can serve those
  pads. A tap passes traffic only while the PORT has its pad muxed to that SERCOM. UART Clicks
  still plug into headers.

## 4. Page (`tools/pp_web/static/`)

- `workspace.js`: the canvas (pan, zoom, fit), the palette (search, click or drag to add; drop
  zones on mikroBUS headers), the empty state with templates.
- `app.js`: a `TwinView` per board (picture, LEDs, buttons, Clicks, title bar to drag, select or
  remove), wired Clicks' pictures, and the side panel for the selected board.
- `bench.js`: parts drawn to scale (0.2 mm per drawing unit), wires, tags, and the Click part's
  pins.

## 5. VS Code

Showing the Twin view opens the bench on the last workspace (`--board` only from the
`peripheralPretender.board` setting). Clicking a board in the sidebar puts it on the bench in
place of the selected one, without restarting the engine. **Run on Twin** asks for a board when
the bench has none of the ELF's architecture.

## 6. Next

The timeline, faults, exact replay and the assistants' bench are in `timeline.md`.

- Boards in lockstep for UART, SPI and timing links between them.
- Bare chips on a breadboard (a package drawing from the device's pinout).
- Wired UART Clicks.
- `pp run --workspace FILE` for CI.
- Undo.
