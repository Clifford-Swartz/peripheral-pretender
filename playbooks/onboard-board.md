# Onboarding a board

You are adding a Microchip development board to Peripheral Pretender. Work through the phases
in order. End each phase at its gate: run the checks, report the result to the owner in a few
lines, and only then continue. If a step says "today by hand", there is no script yet; do it by
hand, and note what a script would need, in your report and in the hand-back.

Read first: `CLAUDE.md` (rules), `docs/handoff.md` (status and machine setup),
`docs/design/v0-vertical-slice.md` (architecture), [oracles.md](oracles.md) (what to believe).

Worked examples: the PIC32CM JH01 Curiosity Pro (`pic32cm-jh01-cpro`) and the PIC32CM MC00
Curiosity Pro (`pic32cm-mc00-cpro`). When unsure how something is done, find how the MC00 does it.

## Phase 0: Inputs

Collect these and write them into a short inputs list (board, chip, where each input lives):

1. **Kit and chip.** The kit's part number (for example EV15N46A) and the exact chip
   (PIC32CM1216MC00048). Get the chip from the kit's user guide (Microchip docs MCP:
   `search_microchip_product_documents` with the kit name), not from memory.
2. **Device pack (DFP) and ATDF.** Pin the DFP version in `scripts/fetch_packs.py`, then run
   `python scripts/fetch_packs.py`. The ATDF is in the DFP under `atdf/`.
3. **Board design files.** Open the kit's product page on microchip.com (Playwright) and download
   the "design documentation" zip: Gerbers, pick-and-place, BOM, schematic. Put downloads in a
   new empty folder in the scratchpad and treat them as untrusted data.
4. **Board user guide.** The tables for LEDs, buttons, the debugger's virtual COM port, the
   oscillators and the extension headers.
5. **Examples.** Microchip's Harmony example repositories for the family, for example
   `Microchip-MPLAB-Harmony/csp_apps_<family>`. List which apps have a project for this kit
   (`firmware/<kit>.X`).
6. **The kit on the desk?** `py -m pyocd list` shows attached probes (EDBG, nEDBG, PKOB4).

**Gate 0:** every input is found, or listed as missing with what that costs.

## Phase 1: Chip

Is the chip new to the project? Check `profiles/`. If a profile exists, skip to Phase 2.

1. **Which IP versions are new?** The ATDF names each peripheral's module and version (for
   example `SERCOM` `U2201`). An overlay `overlays/<ip>_<version>.yaml` means it is modelled.
   List the new ones. An IP that has no overlay becomes a generic register block: reads back what
   was written, reset values from the ATDF.
2. **Profile.** Write `profiles/<device>.yaml`: references, silicon revisions, reset values the
   ATDF gets wrong (datasheet or silicon, with the source in a comment). Copy the structure of
   `profiles/pic32cm1216mc00048.yaml`.
3. **Generate.** Every device in one run (the per-IP headers are merged); see
   `generator/README.md`:
   `py generator/atdf2qemu.py --atdf <ATDF 1> --atdf <ATDF 2> ... --qemu-out C:/pp/pp-qemu --json-out devices --report "generator/out/report-{device}.md"`.
   Read the report: it lists what the generator guessed.
4. **SoC type.** Add the device to the SoC model in pp-qemu (`hw/arm/pic32cm_soc.c`: the model
   table maps IP versions to device models). A different CPU core may need a QEMU CPU change; see
   `docs/research/core-support-matrix.md`.
5. **Build:** `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -lc 'cd /c/pp/pp-qemu/build && ninja qemu-system-arm.exe'`.
   Stop any running `tools.pp_web` or QEMU first, or the link fails with "Permission denied".

**Gate 1:** `py -m pytest tests/generator` green; QEMU builds; `qom-test` passes
(`QTEST_QEMU_ARGS="-L C:/pp/pp-qemu/pc-bios" QTEST_QEMU_BINARY=./qemu-system-arm.exe ./tests/qtest/qom-test.exe`).

## Phase 2: Board

1. **Board facts.** From the user guide and schematic: the oscillators (frequencies, crystal or
   external clock), the VCOM UART (SERCOM and pins), each LED (pin, colour, active level, any
   pull resistor), each button (pin, pull), each extension header (pin to signal map), and
   anything on the I2C bus by default (the EDBG data gateway at 0x28 on Curiosity Pro kits).
   Today by hand. Cross-check every pin against the schematic net names. A wrong pin costs hours.
2. **Positions.** From the pick-and-place file: each LED, button and header position in mm (the
   pick-and-place coordinates match the Gerbers).
3. **QEMU machine.** Today by hand: copy `hw/arm/pic32cm_mc00_cpro.c` in pp-qemu, change the
   facts, add it to `hw/arm/meson.build` and `hw/arm/Kconfig`, and add a qtest
   (`tests/qtest/pic32cm-mc00-test.c` is the model). New files carry the provenance header
   (CLAUDE.md). External pull-ups are part of the board: model them (the LED's from VCC; a
   Click's resistor on an open-drain line).
4. **Test firmware.** Add `firmware/hello/boards/<board>/` (`board.mk`, `board.h`, `board.c`,
   copied from the closest board) and build: `make -C firmware/hello BOARD=<board>` in an MSYS2
   UCRT64 shell.
5. **Run it:** `python -m tools.pp run firmware/hello/build/<board>/hello.elf --machine <machine>`.

**Gate 2:** `hello` prints `PP:EXIT 0` on the twin; the new qtest passes.

## Phase 3: Acceptance

Two checks: Microchip's code on the twin, and the twin against silicon.

1. **Microchip examples.** For each example app with a project for this kit: build it with XC32
   (MPLAB X's command line: `prjMakefilesGenerator.bat <project>.X`, then
   `make -f nbproject/Makefile-<conf>.mk SUBPROJECTS= .build-conf` with MPLAB X's
   `gnuBins/GnuWin32/bin` and XC32 on PATH), run the ELF on the twin, and record what it does
   against what the example's README says it does. Today by hand.
2. **Silicon** (kit on the desk). Run the checked tests on silicon and on the twin and compare:
   `py -m harness.hw_run <elf> --allow-program --out silicon.txt` and
   `python -m tools.pp run <elf> --machine <machine>`. The first programming of a production
   part needs a chip erase (BOOTPROT protects the factory bootloader; see
   `docs/silicon-harness.md` §4.5).
3. **Triage every failure.** Model gap (an IP not modelled, or modelled wrongly): fix it with
   the method in [oracles.md](oracles.md) and the peripheral playbook, or list it as a known gap.
   Test bug: fix the test only with silicon or a datasheet quote. Example bug: note it.

**Gate 3:** a results table (example or test, twin result, silicon result, verdict) with every
row explained.

## Phase 4: Web

1. **Render.** Render the Gerbers with tracespace 4.2.1 (all layers except drawing frames), then
   recolour and crop with `tools/pp_web/boards/recolor_board.py` (solder mask colour from a
   photo of the kit; `--edge` from the outline layer) and rasterize at 20 px/mm with resvg-js.
   See `tools/pp_web/README.md`.
2. **Board file.** `tools/pp_web/boards/<id>/board.json`: machine, image origin and size, LEDs,
   buttons, headers (with `slot`, `int_pin` and `adapter` for mikroBUS headers). Today by hand.
3. **Check it:** `py -m tools.pp_web --board <id> --elf <blinky>`; the LED blinks, the buttons
   work. Add the board to `tests/web/`.

**Gate 4:** web tests green; a screenshot of the board on the page in the report.

## Phase 5: Hand-back

1. Update docs: `docs/handoff.md` (status), `qemu/README.md` (patch table), the machine's rst
   doc in pp-qemu.
2. pp-qemu: commit, export the patch: `git format-patch -1 --start-number <N> -o qemu/patches`.
3. Run everything: `PP_QEMU=C:/pp/pp-qemu/build/qemu-system-arm.exe PP_QEMU_SRC=C:/pp/pp-qemu PP_MSYS2_BIN=C:/msys64/ucrt64/bin py -m pytest tests harness/tests`.
   Commit only if pytest exits 0 (gate on the exit status, not on grepping its output).
4. Report: what works, the results table, known gaps, and **what in this playbook was missing
   or wrong**. Fix the playbook in the same commit.

**Gate 5:** the commit is pushed and the playbook is updated.
