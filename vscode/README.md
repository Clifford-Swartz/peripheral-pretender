# Peripheral Pretender

Run your firmware on a digital twin of a Microchip board, inside VS Code. The board appears in a
panel: LEDs light as your pins drive them, buttons click, the virtual COM port shows up as a
console, and you can plug Click boards and everyday parts (resistors, LEDs, pots, a voltmeter…)
into its headers.

Everything the twin needs comes with the extension: QEMU with the Microchip models and the
engine that runs it. Nothing else to install.

## Boards

- PIC32CM MC00 Curiosity Pro (EV15N46A)
- PIC32CM JH01 Curiosity Pro (EV81X90A)
- AVR64EA48 Curiosity Nano (EV66E56A) on a Curiosity Nano Base
- Curiosity PyKit Explorer (ATSAME51J20A, stock CircuitPython, downloaded on first use)

## Use

- Click the **chip icon** in the activity bar (the left edge). The bench opens beside your editor
  as you left it: its boards, Click boards, parts, wires and firmware. The first time it is empty
  and offers templates, such as a rotary angle on an OLED or text scrolling on an 8x8 matrix.
- The **palette** on the left of the bench holds everything you can add: templates, boards,
  Click boards, parts and pin tags. Press **/** to search it. Click an entry to add it, or drag it
  onto the bench:
  - **A Click board:** drop it on a mikroBUS header to plug it in. Drop it anywhere else to
    wire it pin by pin (click a pin, then a header hole); the bench checks that the SPI or I2C
    pins are ones a SERCOM can drive.
  - **A part:** dropped on a header, its legs go into the holes.
  - **Another board:** it joins the bench, each board with its own firmware. Wire one board's
    pins to another's.
- Pan the bench by dragging its background; zoom with the wheel, **Fit** (or **F**) shows
  everything. Drag a board by its title bar, click it to select it; × takes it off.
- The side panel follows the selected board: drop its firmware there (or on the board itself),
  its Click boards' controls, its console.
- Build your project: with MPLAB for VS Code (`out/**/*.elf`) or MPLAB X (`dist/**/*.elf`), each
  new build in the workspace runs on the selected board by itself (`peripheralPretender.watch`),
  and the board keeps it. Reopening the bench brings back what each board ran last, unless you
  built while it was closed.
- Right-click any `.elf` in the Explorer: **Run on Twin** (on an empty bench it asks which
  board).
- The sidebar lists the boards too: clicking one puts it on the bench in place of the selected
  board. **Twin: Stop** ends the twin; **Twin: Show Engine Log** shows what the engine did.

## Timeline, faults and replay

- **Timeline** (the waveform button, top right): the selected board's pins, its buses decoded
  (UART text, SPI bytes, I2C transactions such as `0x18 W 05 | 0x18 R 01 90`) and its
  interrupts, in the twin's own time. Every board has it: the Curiosity Pros, the PyKit and
  the AVR64EA48 Nano.
  - Live, it follows the board.
  - The wheel zooms onto what is under the cursor, dragging goes back in time, and double-click
    goes live again.
  - Hover for the detail; **+ Row** adds a pad or a bus.
- **Faults** (side panel): test your error handling.
  - Short a pad to GND or VDD.
  - Make an I2C Click stop answering.
  - Make an SPI device's MISO stick.

  The board's title bar shows a ⚡ while any is set. Faults are not saved.
- **Record** (timeline bar) restarts the firmware and records a run that replays exactly: every
  button, slider and console line, kept with the instant it took effect. **Replay** runs it
  again and says whether everything happened the same, or where it first differed.

## Assistants (MCP)

The extension registers an MCP server, **Peripheral Pretender**, with VS Code (1.101 or later).
Assistants in VS Code can then use your bench:
- read the boards, LEDs and console;
- run firmware;
- press buttons and move Click controls;
- read the timeline;
- inject faults;
- record and replay.

They drive the same bench you see. Outside VS Code (Claude Code, for one), run
`py -m tools.pp_mcp` from a source checkout: it finds a running bench, or starts its own.

## Debug on the twin

Breakpoints, stepping, variables, the call stack and memory, on the twin instead of a board,
with the board view live beside the debugger. Arm (PIC32CM, SAM E51) and AVR firmware.

- Press **F5** in a workspace with a build and pick **Peripheral Pretender Twin**: the newest
  `out/**/*.elf` or `dist/**/*.elf` runs on the twin and stops at `main`.
- Or add it to `launch.json` (**Add Configuration… → Peripheral Pretender: Debug on the twin**):

  ```jsonc
  {
    "type": "twin",
    "request": "launch",
    "name": "Debug on the twin",
    "program": "${workspaceFolder}/out/${workspaceFolderBasename}/default.elf",
    "board": "ev15n46a",          // optional: else the board showing, or ask
    "stopAtEntry": true           // optional: stop at main (default)
  }
  ```

When the session ends the firmware runs on, undebugged. Debugging uses the C/C++ extension's
debugger (`ms-vscode.cpptools`, part of the MPLAB extension pack) with the gdb that comes with
this extension. Build with debug information (`-g`, MPLAB's debug configuration) for source
lines.

## Settings

| Setting | |
|---|---|
| `peripheralPretender.board` | board id to open without asking (`ev15n46a`, `ev81x90a`, `ev66e56a`, `pykit`) |
| `peripheralPretender.watch` | run new builds of the workspace (default on) |
| `peripheralPretender.gdbPath` | the gdb to debug with (default: the bundled gdb-multiarch) |
| `peripheralPretender.engineRoot`, `.python` | development: run a source checkout instead of the bundled engine |

## Building the extension

```
py scripts/build_vsix.py            # stages vscode/engine, smoke-tests it, packages the VSIX
cd vscode && npm test               # end to end in a downloaded VS Code
code --install-extension vscode/peripheral-pretender-win32-x64-0.6.3.vsix
```

Windows x64 only so far. The bundle holds QEMU (GPL-2.0-or-later) built from the pp-qemu fork,
whose patches are in `qemu/patches/` of the source repository.
