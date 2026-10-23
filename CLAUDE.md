# Peripheral Pretender

QEMU-based digital twin of Microchip MCU peripherals, growing toward board-level simulation that
customers can scale to their own boards. Targets: PIC32CM5164JH01100 on the PIC32CM JH01
Curiosity Pro (EV81X90A, machine `pic32cm-jh01-cpro`) and PIC32CM1216MC00048 on the PIC32CM MC00
Curiosity Pro (EV15N46A, `pic32cm-mc00-cpro`); AVR64EA48 on the AVR64EA48 Curiosity Nano (EV66E56A,
`avr64ea48-cnano`, QEMU's AVR build) on a Curiosity Nano Base for Click boards; ATSAME51J20A on the
Curiosity CircuitPython board of the Curiosity PyKit Explorer (`curiosity-circuitpython`, running stock
CircuitPython). A personal side project by Clifford Swartz, not affiliated with or endorsed by
Microchip. MIT License (see LICENSE); the QEMU fork is GPL-2.0-or-later.

**Start here:** `docs/handoff.md` (status, setup, next steps), then `docs/design/v0-vertical-slice.md`
(the build contract: architecture, interfaces, acceptance criteria). Background:
`docs/walkthrough.md` and the cited research in `docs/research/`.

**Adding a board** (or a Click board): follow `playbooks/README.md`. Fix the playbook whenever it
falls short.

## Decisions (from the project owner)

- Customers: professional embedded teams. Delivery: a VS Code extension alongside Microchip's MPLAB
  extensions and AI coding assistant, plus CI (CLI/container) and MCP. The engine stays headless;
  nothing essential lives only in an IDE.
- Built in-house with heavy AI assistance, as a downstream QEMU fork.
  Upstream QEMU declines AI-generated contributions (`docs/devel/code-provenance.rst` in QEMU), so
  never prepare or submit any of this code upstream.
- Chip models first (validated against silicon), designed from day one to plug into a board layer
  (board file, pad routing from ATDF signals, part library, out-of-process extension SDK).

## Layout

- `generator/` + `overlays/` + `profiles/` — ATDFs + per-IP behavior overlays + per-device profiles
  → generated QEMU register tables, device descriptors and `devices/<device>.json`. Always generate
  every device in one run (the per-IP constant headers are merged); see `generator/README.md`.
- `qemu/` — the pp-qemu fork as patches on QEMU v11.1.2; `qemu/setup.sh` recreates it at
  `../pp-qemu`. QEMU's configure rejects paths with spaces.
- `firmware/hello/` — bare-metal test image that runs unchanged on twin and silicon; ends with a
  `PP:EXIT <n>` line. One image per board: `make BOARD=pic32cm_jh01_cpro|pic32cm_mc00_cpro`,
  output in `build/<board>/`.
- `tools/pp` (runner CLI; `trace.py` decodes pp-qemu's event recorder), `tools/pp_mcp` (MCP
  server: the runner and the live bench), `harness/` (silicon comparison via pyOCD/EDBG),
  `tests/`. Timeline, faults, exact replay: `docs/design/timeline.md`.
- `tools/pp_analog` — analog co-simulation: SPICE circuits in ngspice, in lockstep with the twin
  over pp-qemu's `pp-cosim` port (`docs/design/analog.md`).
- `scripts/fetch_packs.py` — pins and fetches the device packs (PIC32CM-JH, PIC32CM-MC, AVR-Ex, SAME51)
  and CMSIS into `~/.mchp_packs`.

## Rules

- New files in pp-qemu carry the provenance header from spec §3 (AI-assisted, not for upstream,
  GPL-2.0-or-later). Generated files say so and are regenerated, never hand-edited.
- QEMU code follows QEMU style: `/* */` comments, 4-space indent, 80 columns, QEMU 11.1 idioms
  (see the cheat sheet in `docs/research/qemu-internals.md`).
- Physical board: read-only unless the owner explicitly approves programming, erasing, resetting or
  halting it. A production part has BOOTPROT protecting 0x0–0xFFF (the factory bootloader), so the
  first programming of `hello.elf` needs an erase and the owner's approval.
- Debug QEMU with `-gdb tcp:127.0.0.1:<port>`, never `-s`. On Windows, `-s` binds all interfaces and
  raises Windows Firewall admin prompts.
- Datasheet facts outrank ATDF hints; the ATDF has known errors (see `docs/research/microchip-data.md`).
  Silicon outranks both.

## Common commands

```
python scripts/fetch_packs.py                 # device packs → ~/.mchp_packs
qemu/setup.sh                                 # pp-qemu fork → ../pp-qemu, built
make -C firmware/hello [BOARD=pic32cm_mc00_cpro]   # hello.elf (arm-none-eabi-gcc)
python -m pytest tests harness/tests          # Python test suites
python -m tools.pp run firmware/hello/build/pic32cm_jh01_cpro/hello.elf
python -m tools.pp run firmware/hello/build/pic32cm_mc00_cpro/hello.elf --machine pic32cm-mc00-cpro
python -m tools.pp run firmware/hello/build/pic32cm_mc00_cpro/rcstep.elf --machine pic32cm-mc00-cpro \
    --circuit firmware/hello/circuits/rcstep.cir   # an RC in ngspice, in lockstep (needs ngspice)
```
On Windows use `py` for Python and run the build steps in an MSYS2 UCRT64 shell.
