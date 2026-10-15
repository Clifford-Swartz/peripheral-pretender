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

- **Twin: Open Board** (Command Palette) picks a board and opens it beside your editor, running
  the board's demo firmware.
- Build your project: with MPLAB for VS Code (`out/**/*.elf`) or MPLAB X (`dist/**/*.elf`), each
  new build in the workspace runs on the twin by itself (`peripheralPretender.watch`).
- Right-click any `.elf` in the Explorer: **Run on Twin**. Or drop an ELF on the board view.
- **Twin: Stop** ends the twin; **Twin: Show Engine Log** shows what the engine did.

## Settings

| Setting | |
|---|---|
| `peripheralPretender.board` | board id to open without asking (`ev15n46a`, `ev81x90a`, `ev66e56a`, `pykit`) |
| `peripheralPretender.watch` | run new builds of the workspace (default on) |
| `peripheralPretender.engineRoot`, `.python` | development: run a source checkout instead of the bundled engine |

## Building the extension

```
py scripts/build_vsix.py            # stages vscode/engine, smoke-tests it, packages the VSIX
cd vscode && npm test               # end to end in a downloaded VS Code
code --install-extension vscode/peripheral-pretender-win32-x64-0.1.0.vsix
```

Windows x64 only so far. The bundle holds QEMU (GPL-2.0-or-later) built from the pp-qemu fork,
whose patches are in `qemu/patches/` of the source repository.
