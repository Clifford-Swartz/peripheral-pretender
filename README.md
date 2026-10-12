# Peripheral Pretender

A QEMU-based digital twin of Microchip MCU peripherals, growing toward board-level simulation that
customers can scale to their own boards. First target: PIC32CM5164JH01100 on the PIC32CM JH01
Curiosity Pro (EV81X90A).

A personal side project; MIT License.

## Layout

| path | purpose |
|---|---|
| `docs/handoff.md` | **start here**: status, machine setup, next steps |
| `docs/walkthrough.md` | how the twin works, end to end (draft) |
| `docs/design/v0-vertical-slice.md` | current build contract (architecture, interfaces, acceptance criteria) |
| `qemu/` | the pp-qemu fork as patches on QEMU v11.1.2, plus `setup.sh` |
| `scripts/fetch_packs.py` | fetches the pinned device packs into `~/.mchp_packs` |
| `docs/research/` | cited research notes behind the design (QEMU internals, ATDF data, boot path, prior art, validation) |
| `generator/`, `overlays/` | ATDF + per-IP behavior overlays → generated QEMU register tables and device description |
| `devices/` | generated device descriptions consumed by tools |
| `firmware/` | test firmware that runs unchanged on the twin and on silicon |
| `tools/` | `pp` CLI runner and MCP server |
| `harness/` | silicon comparison harness (EDBG/pyOCD) |
| `tests/` | pytest suites |

The QEMU fork lives next to this repo at `../pp-qemu` (branch `pp/main`), because QEMU's configure
script rejects paths containing spaces. `qemu/setup.sh` recreates and builds it.
