# Handoff: moving to a new machine (2026-10-12; updated 2026-10-13)

**2026-10-13:** set up on the personal PC and finished QEMU part B. The twin now runs `hello.elf`
to `PP:EXIT 0` with an empty model backlog. See "Part B result" below.

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
| Tools (`pp` runner, MCP server) | done: the full Python suite (241 tests) passes with nothing skipped, including the PIC32CM end-to-end tests; the MCP server works with `mcp` 1.x and 2.x | `docs/handoff/agent-reports/tools.md` |
| Silicon harness | done: 72 tests pass; no board was connected, so nothing has touched hardware | `docs/handoff/agent-reports/silicon-harness.md`, `docs/silicon-harness.md` |
| QEMU part A | done: `pic32cm-jh01-cpro` boots `hello.elf` through the full Harmony clock bring-up and reaches the first UART poll with the CPU at 48 MHz; 12/12 board qtests; microbit, stm32l4x5, device-introspect, qom-test and test-hmp all still pass | `docs/handoff/agent-reports/qemu-core-a.md`, patch 0004 |
| QEMU part B (SERCOM, PORT, LED0/SW0, end to end) | done: acceptance in spec §6.6 met; see "Part B result" | `docs/handoff/agent-progress/qemu-core-b.md`, patch 0005 |

## Setting up the new machine

Notes from the 2026-10-13 setup on Windows 11 (personal PC):
- MSYS2 came from `winget install MSYS2.MSYS2`. Running `pacman -Syu` the first time updates the core
  and exits the shell, so run it twice. Also install `mingw-w64-ucrt-x86_64-meson`,
  `-pkgconf` and `-gdb-multiarch`.
- The repo can live in a path with spaces (the firmware Makefile now quotes the pack paths), but the
  QEMU fork cannot, and a user directory with a space rules out `../pp-qemu`. Build it with
  `qemu/setup.sh /c/pp/pp-qemu`, then set `PP_QEMU=C:/pp/pp-qemu/build/qemu-system-arm.exe`,
  `PP_QEMU_SRC=C:/pp/pp-qemu` and `PP_MSYS2_BIN=C:/msys64/ucrt64/bin`.
- `bash -lc` from a non-MSYS2 shell does not see `USERPROFILE`, so the firmware Makefile falls back
  to `$HOME`. Pass `PACKS="C:/Users/<you>/.mchp_packs"` to make.
- `generator/out/` is gitignored, so `test_checked_in_outputs_are_current` fails on a fresh clone
  until the generator is run once (the command is in the assertion message).

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
7. `python -m tools.pp run firmware/hello/build/hello.elf`: prints the transcript and exits 0.

Environment variables: `PP_PACKS` (pack root), `PP_QEMU` (QEMU binary for tools/harness),
`PP_QEMU_SRC` (pp-qemu source root for generator tests), `PP_GCC`, `PP_ARM_GCC`, and on Windows
`PP_MSYS2_BIN` (QEMU's DLL directory).

## Part B result (2026-10-13)

- SoC: `{ "PORT", TYPE_MCHP_PORT }` and `{ "SERCOM", TYPE_MCHP_SERCOM }` in `pic32cm_jh_models[]`;
  `PIC32CM_JH_SOC` selects `MCHP_PORT`/`MCHP_SERCOM`, the board selects `LED`.
- Board: LED0 is a QEMU LED (active low, yellow) on `port-out[69]` (PC05); machine property
  `sw0-pressed` drives `port-in[51]` (PB19) to 0, or releases it (-1). It can be set at run time
  with `qom-set /machine sw0-pressed true`.
- Trace events `mchp_sercom_tx`, `mchp_sercom_tx_blocked`, `mchp_sercom_rx`, `mchp_port_out`,
  `mchp_port_in`. `-trace led_set_intensity` shows the three LED0 blinks of `hello.elf`.
- Qtests: `pic32cm-jh-sercom-test` (6 cases: reset values, TX to a socket, W1C leaves DRE/RXC alone,
  INTENSET/INTENCLR alias with NVIC line 13, RX to RXC to IRQ, debug read does not pop RX, RX disable
  flushes, STATUS W1C, enable protection, SWRST) and `pic32cm-jh-port-test` (7 cases: reset values,
  SET/CLR/TGL, PINCFG/PMUX bytes and WRCONFIG, IOBUS alias, IN with pulls and external drivers, SW0,
  gpio-out levels). Both pass, as do `pic32cm-jh-test` 12/12, microbit 6/6, stm32l4x5
  exti/syscfg/rcc/gpio/usart 9/10/5/14/7, device-introspect 6/6, qom-test 79/79, test-hmp 79/79.
- Debug reads in tests: HMP `xp` reads with unspecified memory attributes, so it is not a
  debugger access and **does** pop SERCOM DATA. HMP `x` and the gdbstub use debug attributes.
- `tools/pp_mcp`: `mcp` 2.x renamed FastMCP to MCPServer and hides the message of any exception
  but `ToolError`. The server imports either, and maps runner errors to `ToolError`.

## Next steps, in order

1. **Independent review**, three lenses: datasheet/Harmony semantics, QEMU robustness (reset,
   VMState, debug reads, logging), and end to end. Then fix what the reviewers find.
2. **Fact-check the walkthrough** with `docs/handoff/workflows/walkthrough-factcheck.js` (five claim
   clusters, primary sources only), and correct it before sharing.
3. **First silicon session** (needs the owner's go-ahead at each step; follow the checklist in
   `docs/silicon-harness.md`):
   - Connect the Curiosity Pro.
   - Read DSU.DID and CPUID with pyOCD in attach mode, which is read-only.
   - Decide on the first programming. It erases BOOTPROT and the factory bootloader.
   - Dump golden reset values, run `hello.elf` on both targets, and diff.
4. **Tier 1 models** (EIC, TC, RTC, EVSYS), plus a `cortex-m0plus` CPU type in target/arm so the
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
