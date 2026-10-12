# Handoff: moving to a new machine (2026-10-12)

Work started on a locked-down corporate Windows workstation and moves to a personal PC because
background work there kept raising admin prompts. The prompts were most likely Windows Firewall
dialogs: some agent scratch scripts started QEMU with `-s`, which listens on all interfaces. The
committed tools use `-gdb tcp:127.0.0.1:<port>`; keep it that way.

## Where everything stands

| piece | status | details |
|---|---|---|
| Research (10 cited notes) | done | `docs/research/` |
| Walkthrough | draft; the fact-check was interrupted by a usage limit and has not run | `docs/walkthrough.md`, `docs/handoff/workflows/walkthrough-factcheck.js` |
| v0 build contract | done | `docs/design/v0-vertical-slice.md` (§2 paths refer to the original workstation) |
| Generator + 15 overlays | done: 82 tests pass, 34 generated headers compile | `docs/handoff/agent-reports/generator.md` |
| Firmware `hello` | done: warning-free, 11/11 static checks, reaches `PP:EXIT 0` on a Unicorn mock; not yet run on the twin or on silicon | `docs/handoff/agent-reports/firmware.md`, `firmware/hello/mock/run_mock.py` |
| Tools (`pp` runner, MCP server) | done: 85 tests pass, 2 skipped (the PIC32CM end-to-end tests wait for part B) | `docs/handoff/agent-reports/tools.md` |
| Silicon harness | done: 72 tests pass; no board was connected, so nothing has touched hardware | `docs/handoff/agent-reports/silicon-harness.md`, `docs/silicon-harness.md` |
| QEMU part A | done: `pic32cm-jh01-cpro` boots `hello.elf` through the full Harmony clock bring-up and reaches the first UART poll with the CPU at 48 MHz; 12/12 board qtests; microbit, stm32l4x5, device-introspect, qom-test and test-hmp all still pass | `docs/handoff/agent-reports/qemu-core-a.md`, patch 0004 |
| QEMU part B (SERCOM, PORT, LED0/SW0, end to end) | **in progress**: SERCOM and PORT models written but not wired into the SoC or tested | `docs/handoff/agent-progress/qemu-core-b.md`, patch 0005 |

## Setting up the new machine

1. Clone this repo to a path **without spaces** (e.g. `~/src/peripheral-pretender`); the QEMU fork
   goes next to it and QEMU's configure rejects spaces.
2. Install the build dependencies listed in `qemu/README.md` for your OS, then
   `python -m pip install --user -r requirements.txt`.
3. `python scripts/fetch_packs.py`: downloads the PIC32CM-JH DFP 1.3.221 and CMSIS 6.1.0 into
   `~/.mchp_packs` (verified byte-identical to the MPLAB-installed copies).
4. `qemu/setup.sh`: clones QEMU v11.1.2 to `../pp-qemu`, applies `qemu/patches/`, builds.
   It ends by printing the `pic32cm-jh01-cpro` machine line.
5. `make -C firmware/hello`, then optionally `make -C firmware/hello check`.
6. `python -m pytest tests harness/tests`. The generator tests also need gcc and the pp-qemu
   source; set `PP_QEMU_SRC` if pp-qemu is not at `../pp-qemu`.
7. `python -m tools.pp run firmware/hello/build/hello.elf`: until part B lands, this times out
   at the first UART poll. That is expected.

Environment variables: `PP_PACKS` (pack root), `PP_QEMU` (QEMU binary for tools/harness),
`PP_QEMU_SRC` (pp-qemu source root for generator tests), `PP_GCC`, `PP_ARM_GCC`, and on Windows
`PP_MSYS2_BIN` (QEMU's DLL directory).

## Next steps, in order

1. **Finish QEMU part B.** The part A report's "PART B" section lists the exact integration steps:
   - Add two entries to `pic32cm_jh_models[]`.
   - Add the Kconfig `select` lines.
   - Wire LED0 to `port-out[69]` and SW0 to `port-in[51]`.
   - Connect the chardev, and call `qemu_chr_fe_accept_input()` when RX is enabled (the tools report
     explains why).

   The part B progress log has its checklist and design decisions. Acceptance (spec §6.6):
   `python -m tools.pp run firmware/hello/build/hello.elf` exits 0 with the transcript in
   `firmware/hello/README.md`, LED0 toggles, `twin_run` over MCP matches, and the SERCOM/PORT qtests pass.
2. **Independent review**, three lenses: datasheet/Harmony semantics, QEMU robustness (reset,
   VMState, debug reads, logging), and end to end. Then fix what the reviewers find.
3. **Fact-check the walkthrough** with `docs/handoff/workflows/walkthrough-factcheck.js` (five claim
   clusters, primary sources only), and correct it before sharing.
4. **First silicon session** (needs the owner's go-ahead at each step; follow the checklist in
   `docs/silicon-harness.md`):
   - Connect the Curiosity Pro.
   - Read DSU.DID and CPUID with pyOCD in attach mode, which is read-only.
   - Decide on the first programming. It erases BOOTPROT and the factory bootloader.
   - Dump golden reset values, run `hello.elf` on both targets, and diff.
5. **Tier 1 models** (EIC, TC, RTC, EVSYS), plus a `cortex-m0plus` CPU type in target/arm so the
   M0+ MPU works (Zephyr needs it).

## Known issues

- MPU_TYPE reads 0 on QEMU's `cortex-m0`, because QEMU only creates MPU regions for Armv7 cores.
  Firmware that enables the MPU (Zephyr with `CONFIG_ARM_MPU`) fails until a `cortex-m0plus` type exists.
- The generated calibration row (`0xFFFFFF00_1103F924`) differs from the firmware README's example
  (`0x0000010011040249`). The README marks that line as variable; align both once silicon is dumped.
- DSU CRC32 is not modeled (STATUSA.DONE never sets), so Harmony's `DSU_CRCCalculate` would hang.
- Windows only: `qom-test` and `test-hmp` need `QTEST_QEMU_ARGS="-L <pp-qemu>/pc-bios"`, because the
  bundle symlinks are skipped.
- The generator report lists 15 heuristic warnings: WSYNC registers with no SYNCBUSY mapping.
- Scripts under `docs/handoff/scripts/` are the agents' scratch tools and still contain the original
  Windows paths; adapt them before use.

## Reusing the agent workflows (Claude Code)

`docs/handoff/workflows/pp-v0-build.js` is the build workflow that produced this state. The paths at
its top (`ROOT`, `QEMU`, `SCRATCH`) refer to the original workstation and must be updated. To finish
part B, reuse its `COREB` prompt and pass in the reports from `docs/handoff/agent-reports/`.
