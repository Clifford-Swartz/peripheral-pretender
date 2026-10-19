# Peripheral Pretender

A QEMU-based digital twin of Microchip MCU peripherals, growing toward board-level simulation that
customers can scale to their own boards. Boards: the PIC32CM JH01 Curiosity Pro (EV81X90A,
machine `pic32cm-jh01-cpro`), the PIC32CM MC00 Curiosity Pro (EV15N46A, `pic32cm-mc00-cpro`),
the AVR64EA48 Curiosity Nano (EV66E56A, `avr64ea48-cnano`) on a Curiosity Nano Base, and the
Curiosity CircuitPython board of the Curiosity PyKit Explorer (ATSAME51J20A,
`curiosity-circuitpython`), with 31 MikroElektronika Click boards. A web board view and a
VS Code extension show the board running your firmware.

**New machine? Read [`SETUP.md`](SETUP.md)** (written for AI coding agents as much as for people:
the VS Code extension in five minutes, or the full development setup, each step with a check).

A personal side project, not affiliated with or endorsed by Microchip Technology Inc. or
MikroElektronika; built with heavy AI assistance. MIT License (see [`LICENSE`](LICENSE)); the QEMU
fork's patches are GPL-2.0-or-later.

## Layout

| path | purpose |
|---|---|
| `SETUP.md` | **set up a machine**: run the extension, or build everything |
| `docs/handoff.md` | **then read**: status, open items, next steps |
| `playbooks/` | adding a board or a Click board |
| `vscode/` | the VS Code extension |
| `examples/` | MPLAB projects that run on the twin |
| `docs/walkthrough.md` | how the twin works, end to end (draft) |
| `docs/design/v0-vertical-slice.md` | current build contract (architecture, interfaces, acceptance criteria) |
| `qemu/` | the pp-qemu fork as patches on QEMU v11.1.2, plus `setup.sh` |
| `scripts/fetch_packs.py` | fetches the pinned device packs into `~/.mchp_packs` |
| `docs/research/` | cited research notes behind the design (QEMU internals, ATDF data, boot path, prior art, validation) |
| `generator/`, `overlays/`, `profiles/` | ATDFs + per-IP behavior overlays + per-device profiles → generated QEMU register tables and device descriptions |
| `devices/` | generated device descriptions consumed by tools |
| `firmware/` | test firmware that runs unchanged on the twin and on silicon, one build per board |
| `tools/` | `pp` CLI runner and MCP server |
| `harness/` | silicon comparison harness (EDBG/pyOCD) |
| `tests/` | pytest suites |

The QEMU fork lives next to this repo at `../pp-qemu` (branch `pp/main`), because QEMU's configure
script rejects paths containing spaces. `qemu/setup.sh` recreates and builds it.
