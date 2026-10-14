# Handoff: moving to a new machine (2026-10-12; updated 2026-10-13)

**2026-10-13:** set up on the personal PC and finished QEMU part B. The twin now runs `hello.elf`
to `PP:EXIT 0` with an empty model backlog. See "Part B result" below.

**2026-10-13, later:** second target, the **PIC32CM MC00 Curiosity Pro** (EV15N46A,
PIC32CM1216MC00048, machine `pic32cm-mc00-cpro`). Its `hello.elf` reaches `PP:EXIT 0` with an
empty backlog. See "Second target: PIC32CM MC00" below.

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
| Second target: PIC32CM MC00 Curiosity Pro | done: generic SoC, MC00 device and board, hello, qtests | "Second target: PIC32CM MC00" below, patch 0007 |

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
3. `python scripts/fetch_packs.py`: downloads the PIC32CM-JH DFP 1.3.221, PIC32CM-MC DFP 1.4.67 and
   CMSIS 6.1.0 into
   `~/.mchp_packs` (verified byte-identical to the MPLAB-installed copies).
4. `qemu/setup.sh`: clones QEMU v11.1.2 to `../pp-qemu`, applies `qemu/patches/`, builds.
   It ends by printing the `pic32cm-jh01-cpro` machine line.
5. `make -C firmware/hello` and `make -C firmware/hello BOARD=pic32cm_mc00_cpro`, then optionally
   `make check` with the same `BOARD`.
6. `python -m pytest tests harness/tests`. The generator tests also need gcc and the pp-qemu
   source; set `PP_QEMU_SRC` if pp-qemu is not at `../pp-qemu`.
7. `python -m tools.pp run firmware/hello/build/pic32cm_jh01_cpro/hello.elf`: prints the transcript and exits 0.

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

## Second target: PIC32CM MC00 (2026-10-13)

Board facts (primary sources, collected in this session): kit EV15N46A, user guide DS70005440A;
data sheet DS60001638E; errata DS80000905G; Harmony `csp_apps_pic32cm_mc00`, configuration
`pic32cm_mc00_curiosity_pro`. EDBG VCOM on SERCOM0 (PA06 TX PAD2, PA07 RX PAD3, function D, IRQ9);
LED0 PA24 and SW0 PA25, both active low, no external pull-up on SW0; 12 MHz MEMS oscillator on XIN
and 32.768 kHz on XIN32 (external clocks, XTALEN = 0); EXT1/EXT2 USART on SERCOM3 (PA22/PA23).
DSU.DID 0x11070006 (A1) / 0x11070106 (A2). VDD defaults to 3.3 V.

What changed to support a second device:
- **Generator:** several `--atdf` in one run. Per-IP constant headers are merged over all devices
  (wider registers and fields win; fields at different positions and clashing value names are
  dropped; different offsets are errors). Register tables are per device in
  `include/hw/mchp/gen/<device>/`. New `hw/arm/<device>_device.h` with the `MchpDeviceInfo`
  descriptor. Overlays gained `optional: true` (register absent on some devices) and
  `optional_fields`; five overlays list the MC00's IP versions (register-identical).
- **Profiles** (`profiles/<device>.yaml`): device-specific reset overrides (DSU.DID, NVMCTRL.PARAM,
  PAC.STATUSC on the JH01; SUPC.VREG and MCLK.AHBMASK on the MC00, where the ATDF disagrees with the
  data sheet), silicon revisions and the NVM rows image. The JH01 values moved out of the overlays
  and `nvmrows.py`; its generated tables are unchanged.
- **QEMU:** `pic32cm-soc` (abstract) with one subtype per device; each device's generated tables
  are compiled in their own file (`hw/arm/<device>.c`), because the JH01 and MC00 have different
  RTC IPs with the same constant names. `mchp-port` and `mchp-gclk` take their group/channel
  counts from the device table. New board `pic32cm-mc00-cpro` and qtest `pic32cm-mc00-test`.
- **Firmware:** `firmware/hello` builds per board (`make BOARD=...`, output `build/<board>/`);
  `boards/<board>/board.c` replays that board's Harmony sequence.
- **Tests:** generator tests check outputs for both devices; the e2e test runs hello on both
  boards. Regression qtests (qom-test, test-hmp: 80 machines) pass.

Silicon check (2026-10-13, EV15N46A on the desk, pyOCD attach mode, reads only, core never
halted): DSU.DID 0x11070006 (**rev A1**), CPUID 0x410CC601, user row 0xB15088FF / 0xFFFFF8BB and
NVMCTRL.PARAM 0x04030800 all **confirmed**. OTP1, OTP5 (calibration, CAL48M 0x210320, matching
what the running firmware had loaded) and TEMP_LOG were read and are now the profile's defaults.
Serial number of this part: F79A74AF 51524B59 39202020 FF190418 (pass it as the SoC's
`serial-number` to make the twin's transcript match the board's byte for byte).

**First silicon differential (2026-10-13, owner granted full control of this board):**
- **Golden reset dump**: reset-and-halt, all 811 register elements read and compared with the twin
  paused at reset. 73 differences before, 19 after the fixes in patch 0008; the rest are capture
  artifacts (warm-reset RCAUSE, power-on-only XOSC32K, pad levels, DBGPRES, DCC/DCFG, MTB trace
  state) or open (SUPC undocumented bits 3/5). The TCC per-instance counter width (TCC0/1 24
  bits, TCC2 16 bits) is fixed in patch 0012.
- **hello.elf on silicon** (`harness.hw_run --target pic32cm1216mc00048`): `PP:EXIT 0`, and the
  VCOM transcript is **byte-identical** to the twin's (rev A1, this part's serial number).
- Both are kept in `golden/pic32cm1216mc00048-a1/` and checked by `tests/golden/` with QEMU only.
- The factory demo is gone from the board; `hello.elf` is on it now.

**Tier 1 peripherals (2026-10-13), each checked against this board:** EIC (patch 0009), TC
(0010), RTC (0011) and EVSYS (0013, with event outputs/inputs in EIC, TC, RTC and PORT). Each has
a qtest and a `firmware/hello` test app whose silicon transcript is in `golden/` and must match
the twin byte for byte (`tests/golden/`). Behaviour the data sheet leaves open was settled with
probe apps (`tcprobe`, `evprobe`) and is written up in each model's header comment. EVSYS
differences left open on purpose: FALLING_EDGE with pulse generators loses some events on
silicon (the twin delivers all), CHBUSY after PORT OUT actions is not repeatable, and CHSTATUS
and the INTFLAG-to-NVIC path lag a few clock cycles on silicon (the twin is instant).

Open MC00 items:
- The twin defaults to rev A2; this board is A1 (`-global pic32cm1216mc00048-soc.silicon-rev=A1`).
- `tools.pp` still defaults to `pic32cm-jh01-cpro`; MC00 runs need `--machine pic32cm-mc00-cpro`.
- DS SERIES field inconsistency for DSU.DID (6 in the header and bit drawing, 7 in text, errata
  and signature); the twin uses 7.

## Next steps, in order

1. ~~Independent review~~ **done 2026-10-13** (three lenses). Fixed: clock state lost on
   migration, SERCOM loadvm, unbounded unimplemented-write logging, WDT CONFIG/EWCTRL protection,
   ICSR.RETTOBASE on ARMv6-M (patch 0006); runner: `invalid_mem` faults in the backlog, lockup
   status, timeout reports PC/IPSR, non-ELF rejection, `-D`/`-M` in `--extra-args`, MCP
   `machine_options`/`trace`/`input_after` and bounded `uart`, harness PP_QEMU and compare fixes.
   Open items are listed under "Known issues".
2. **Fact-check the walkthrough** with `docs/handoff/workflows/walkthrough-factcheck.js` (five claim
   clusters, primary sources only), and correct it before sharing.
3. **First silicon session** (needs the owner's go-ahead at each step; follow the checklist in
   `docs/silicon-harness.md`):
   - Connect the Curiosity Pro.
   - Read DSU.DID and CPUID with pyOCD in attach mode, which is read-only.
   - Decide on the first programming. It erases BOOTPROT and the factory bootloader.
   - Dump golden reset values, run `hello.elf` on both targets, and diff.
4. ~~Tier 1 models (EIC, TC, RTC, EVSYS)~~ **done 2026-10-13**, checked on silicon. Still to do:
   a `cortex-m0plus` CPU type in target/arm so the M0+ MPU works (Zephyr needs it).

## Known issues

Open findings from the 2026-10-13 review (datasheet items are from the shared SAM C21/JH IP and
still need checking against DS60001632H):
- SERCOM RX never overflows: the chardev holds input back (TCP backpressure), so STATUS.BUFOVF,
  INTFLAG.ERROR and lost bytes before RXEN never happen. Firmware RX error paths cannot be tested.
  Plan: an opt-in `rx-overrun` mode paced by the baud rate (2-deep FIFO + shift register).
- SERCOM TX is instant unless `pace-tx=on`: TXC is already set when a DATA write returns.
- SERCOM DRE is gated on CTRLB.TXEN; RXS is set once per chardev batch; DATA reads 0 when empty.
- WDT CTRLA.ALWAYSON is cleared by warm resets (SYST, WDT); silicon clears it only at power-on.
- GCLK PCHCTRL.WRTLOCK does not also lock the generator the channel uses.
- Oscillators with ONDEMAND=1 report ready without a clock request, so firmware that would hang on
  silicon boots on the twin.
- NVMCTRL accepts undefined command codes without setting STATUS.PROGE.
- `--icount` is deterministic only for output-only runs: UART input arrives in host time.
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
