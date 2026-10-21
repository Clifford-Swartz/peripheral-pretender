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

- Click the **chip icon** in the activity bar (the left edge): the **Boards** view lists the
  boards; click one to open it beside your editor, running its demo firmware (the running one has
  a green dot; click another to switch). Its title bar has **Debug on the Twin**, **Run on Twin**
  (pick an ELF), **Stop** and the engine log. The **Twin** item in the status bar opens a board
  too, or switches once one runs. **Twin: Open Board** in the Command Palette does the same.
- Build your project: with MPLAB for VS Code (`out/**/*.elf`) or MPLAB X (`dist/**/*.elf`), each
  new build in the workspace runs on the twin by itself (`peripheralPretender.watch`).
- Right-click any `.elf` in the Explorer: **Run on Twin**. Or drop an ELF on the board view.
- Switch boards from the dropdown at the top of the board view, or with **Twin: Switch Board**
  (also the **Twin** item in the status bar).
- **Twin: Stop** ends the twin; **Twin: Show Engine Log** shows what the engine did.

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
code --install-extension vscode/peripheral-pretender-win32-x64-0.4.7.vsix
```

Windows x64 only so far. The bundle holds QEMU (GPL-2.0-or-later) built from the pp-qemu fork,
whose patches are in `qemu/patches/` of the source repository.
