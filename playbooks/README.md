# Playbooks: adding a board to Peripheral Pretender

Peripheral Pretender should simulate every Microchip development board. Adding one must be a
process that an agent can run, not something only the person who did the last board knows how to
do. These playbooks are that process. They follow the approach of
[zephyr-bsp-copilot](https://github.com/Clifford-Swartz/zephyr-bsp-copilot): Claude works through
fixed phases, scripts do the mechanical parts, Claude makes the judgement calls (schematic
reading, datasheet errata, model behaviour), and every phase ends with a gate that must pass
before the next one starts.

## How to run it

Open a Claude Code session in this repository and say:

> Add the board `<board name>` (`<Microchip part number of the kit>`) to Peripheral Pretender.
> Follow `playbooks/onboard-board.md`.

The agent works through the phases and reports at each gate. Run it in a fresh session (for
example a second VS Code window) with no other context. Wherever the agent gets stuck, asks or
guesses, the playbook is missing something, so fix the playbook, not just the board.

## Files

| File | What it is |
|---|---|
| [onboard-board.md](onboard-board.md) | The procedure: phases, steps, gates |
| [oracles.md](oracles.md) | What to believe when sources disagree: silicon, simulator, datasheet, ATDF |
| [peripherals/](peripherals/) | Lessons per peripheral model, learned against silicon |
| [click-boards.md](click-boards.md) | Adding a Click board or an adapter |

## Phases at a glance

| Phase | Output | Gate |
|---|---|---|
| 0. Inputs | Kit part number, chip, design files, packs, examples located | Every input found or listed as missing |
| 1. Chip | Generated device (registers, IRQs, signals, events) | `py -m pytest tests` green; QEMU builds; `qom-test` passes |
| 2. Board | Board render, board file, QEMU machine | The machine boots `hello` and prints `PP:EXIT 0` (CircuitPython boards: and the stock CircuitPython) |
| 3. Acceptance | `acceptance/<kit>.yaml`: Microchip's examples on the twin; silicon if the kit is on the desk (CircuitPython kits: the same REPL lines on kit and twin) | `tools.pp_accept suite` passes; every example has a verdict |
| 4. Web | The board in `tools/pp_web` | LEDs and buttons work on the page (web tests) |
| 5. Hand-back | Docs, patches, commit | Full test suite green; commit pushed |

## What the agent does and what the owner does

The agent does everything in the phases. The owner decides which board comes next, supplies
hardware, approves hardware access, and reviews the acceptance results. Nothing here is ever sent
to upstream QEMU (see `CLAUDE.md`).

## Hardware

`CLAUDE.md` makes the boards read-only unless the owner approves programming, erasing, resetting
or halting them. Approvals so far:

| Kit | Approved | Scope |
|---|---|---|
| EV15N46A (PIC32CM MC00 Curiosity Pro) | 2026-10-13 | everything |
| PIC32CM JH01 Curiosity Nano | 2026-10-14 | everything, including the first chip erase |
| EV66E56A (AVR64EA48 Curiosity Nano), on an AC164162 Curiosity Nano Base | 2026-10-14 | everything; never lock the device |
| Curiosity PyKit Explorer (EV78P89A on EV82H04A), CircuitPython | 2026-10-14 | **read-only REPL only** (`harness.cp_repl`: Ctrl-C, expressions, always Ctrl-D). Never modify the CIRCUITPY drive, never reflash (no UF2, no bootloader): it carries someone's project (backup: `C:/pp/pykit/device-backup-2026-10-14`) |

The PIC32CM JH01 Curiosity Pro (EV81X90A) is modelled but not on the desk: it has no silicon
checks. For any other kit, ask the owner first and add a row here. Even with approval, never set the
security bit or anything else that cannot be undone.

## Tools the playbook uses

| Tool | For |
|---|---|
| `generator/atdf2qemu.py` | Chip: ATDF + overlays to QEMU tables |
| `py -m tools.pp run` | Run an ELF that prints `PP:EXIT` (test firmware) |
| `py -m tools.pp_accept build / run / suite` | Microchip's examples: build with XC32, play scenarios, check expectations |
| `py -m harness.hw_run` | Silicon: program, reset, capture the VCOM |
| `py -m harness.nano_run` | Silicon on Curiosity Nano kits: drag-and-drop programming, VCOM capture |
| `py -m harness.mdb_sim` | MPLAB simulator: USART output of an ELF (8-bit parts) |
| `py -m harness.cp_repl` | A CircuitPython kit's REPL, read-only (always soft-reboots at the end) |
| `py -m tools.pp.circuitpython fetch / run` | Stock CircuitPython on the twin: UF2 to ELF, CIRCUITPY image, REPL lines |
| `tools/pp/fatfs.py`, `tools/pp/uf2.py` | CIRCUITPY images (build, list, get, put); UF2 to ELF |
| `tools/pp_web/boards/pykit/make_art.py` | Board art from renders when there are no Gerbers |
| `tools/pp_web/boards/compose_carrier.py` | A board in its carrier's socket, one image |
| `tools/pp_web/boards/recolor_board.py`, `clicks/render_click.py` | Board, adapter and Click art |
| `py -m tools.pp_web --watch` | The web view, following a firmware project's builds |
