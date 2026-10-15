# Handoff: moving to a new machine (2026-10-12; updated 2026-10-13)

**2026-10-13:** set up on the personal PC and finished QEMU part B. The twin now runs `hello.elf`
to `PP:EXIT 0` with an empty model backlog. See "Part B result" below.

**2026-10-13, later:** second target, the **PIC32CM MC00 Curiosity Pro** (EV15N46A,
PIC32CM1216MC00048, machine `pic32cm-mc00-cpro`). Its `hello.elf` reaches `PP:EXIT 0` with an
empty backlog. See "Second target: PIC32CM MC00" below.

**2026-10-14:** first non-Arm board, the **AVR64EA48 Curiosity Nano** (EV66E56A, machine
`avr64ea48-cnano`, `qemu-system-avr` built in `pp-qemu/build-avr`), on a Curiosity Nano Base for
Click boards with the 8x8 G Click. `firmware/avr-hello` prints byte for byte what the kit prints
(`golden/avr64ea48-b1/`); the MPLAB simulator differs only in SIGROW and REVID. See "AVR64EA48
Curiosity Nano" below and the playbook section "8-bit AVR and other non-Arm families".

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
| First 8-bit target: AVR64EA48 Curiosity Nano | done: AVRxt CPU, generic AVR8X SoC, board, Nano Base Click slots, 8x8 G Click, web view; checked on silicon and in MPLAB SIM | "AVR64EA48 Curiosity Nano" below, patches 0021-0025 |
| Click batch 1 | done on the twin (no physical Click checked): Buzz (tone in the browser), OLED C (SSD1351, live picture; `pp_panel` for future display controllers), Thumbstick (MCP3204, drag the stick), Accel (ADXL345, tilt), RTC 6 (MCP79411, time and alarms), 8x8 R/Y/B, Button R/Y; AVR8X TWI host so I2C Clicks work on the Nano Base; demos and web tests on the MC00 | `playbooks/click-boards.md`, `tools/pp_web/README.md`, patches 0026-0031 |

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
3. `python scripts/fetch_packs.py`: downloads the PIC32CM-JH DFP 1.3.221, PIC32CM-MC DFP 1.4.67,
   AVR-Ex DFP 2.8.189 and CMSIS 6.1.0 into
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

## Endgame and the web board view

The owner's target (2026-10-14) is a web page, later also a VS Code extension, showing the
real board in 3D. Users plug in Mikroe Click boards and run their own (or AI-written) firmware
on the twin, live. See `docs/design/endgame.md` for the layers and roadmap. First slice done:
`tools/pp_web`. It shows the EV15N46A rendered from Microchip's Gerbers, with LED0 following
PA24, clickable SW0/RESET, the VCOM console and ELF upload (`py -m tools.pp_web --elf ...`).
PWM brightness is done too (patch 0016): peripheral outputs drive pads through the pin mux,
PORT accounts the duty cycle per pad, and the `breathe` demo dims LED0 with TCC1. Next in that
roadmap: SERCOM SPI/I2C, done too (patch 0017: host modes, checked on silicon against the
EDBG data gateway at 0x28 and an SPI pad loopback). The first Click board is in too (patch
0018): a Thermo 8 Click (MCP9808) on EXT1/EXT2 via `-M pic32cm-mc00-cpro,ext1=thermo8`, with a
Click picker and a live temperature slider on the page and the `thermo` demo. It is modelled
from the data sheet only; no physical Click has been checked.

**2026-10-14: the PIC32CM JH01 Curiosity Pro in the web view** (`--board ev81x90a`), rendered
from the R4 Gerbers; LED0 (PC05) and SW0 (PB19) work (`tests/web`). Microchip's 38
`csp_apps_pic32cm_jh00_jh01` examples for this kit build with XC32 v4.45 (they ask for v4.60,
PIC32CM-JH_DFP 1.5.236 and CMSIS 5.9.0, fetched into `~/.mchp_packs`). On the twin, port, EIC,
EVSYS, SysTick, TC timer, clock, DIVAS, USART echo, PM/EIC wake-up and RSTC external reset
behave as the code says. Gaps they show on the JH01: RTC (`rtc_03608`, no overlay; the MC00's
RTC is `rtc_u2250`), WDT time-out, DMAC, DSU CRC32, NVMCTRL programming, MCRAMC/SMBIST, FREQM,
ADC, AC, DAC, CCL, CAN, TC capture, TCC dead time, SERCOM I2C client and LIN. Not checked on
silicon (no board on the desk).

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
4. ~~Tier 1 models (EIC, TC, RTC, EVSYS)~~ **done 2026-10-13**, checked on silicon.
   ~~`cortex-m0plus` CPU type~~ **done 2026-10-13** (patch 0014): 8-region ARMv6-M MPU and
   unprivileged mode, checked on silicon with the `mpu` app (MPU_TYPE 0x800; no TEX field;
   RBAR.ADDR [31:8]). Also fixes a QEMU PMSAv7 bug where a disabled subregion cached a whole
   TLB page.
5. **TCC** done 2026-10-13 (patch 0015), checked on silicon with the `tcc` app (byte-identical)
   after four `tccprobe` rounds. Datasheet sections came from Microchip's MCP server
   (`claude mcp add --transport http microchip https://api.microchip.com/mcp/resources`,
   tool `search_microchip_product_documents`). Silicon facts the data sheet does not give: a
   buffer write keeps SYNCBUSY set until it is copied (so a SYNCBUSY wait lasts to the end of
   the period, forever with LUPD); STOP clears COUNT; COUNTEV/INC stop the clock from moving
   the counter; the CNT rules per CNTSEL. Open: DSCRITICAL waveform, faults, dead time,
   pattern generation, dithering, capture, pins. Next candidates: ADC, then DMAC.

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


## AVR64EA48 Curiosity Nano (2026-10-14)

- **QEMU:** `target/avr` gains an `avrxt` CPU (patch 0021); `hw/avr/avr8x_soc.c` builds the SoC
  from the generated `hw/avr/avr64ea48_device.h` like `pic32cm-soc` does (0023). Modelled: PORT/
  VPORT, USART, SPI host, TCB periodic interrupt, CLKCTRL, CPUINT, NVMCTRL FLMAP, SIGROW/SYSCFG/
  USERROW/FUSE/LOCK, TWI (host mode, patch 0029); everything else is a logging regblock (TCA, ADC, RTC, EVSYS, ...).
  Machine options `mb1=`..`mb3=` plug Clicks into the Nano Base's mikroBUS sockets; the Click
  catalog is now shared (`hw/misc/mikrobus.c`).
- **Build:** `qemu/setup.sh` builds `build/` (Arm) and `build-avr/` (AVR). Tools find the AVR
  binary from the ELF: `$PP_QEMU_AVR`, else `build-avr/` beside `$PP_QEMU`, else
  `../pp-qemu/build-avr`.
- **Firmware:** `make -C firmware/avr-hello` (MPLAB XC8 v2.50 avr-gcc; `PACKS=` for the fetched DFP):
  `hello` (golden), `blinky` (web demo, TCB0 interrupt), `matrix` (8x8 G Click on mikroBUS 1).
- **Silicon:** `py -m harness.nano_run <hex> --allow-program` programs by drag and drop onto the kit's
  CURIOSITY drive and captures the VCOM; it refuses images with fuses or lock bits.
- **Simulator:** `py -m harness.mdb_sim <elf> --device AVR64EA48 --txdata 0x0822` recovers the MPLAB
  simulator's USART output (its UART I/O is silent for AVR8X).
- **Results:** hello: twin = silicon = golden; simulator: SIGROW reads 0 (W0035 unimplemented memory),
  REVID 0. matrix: twin = silicon VCOM. Microchip's examples: `acceptance/avr64ea48_curiosity_nano.yaml`
  (3 pass, 5 gaps: TCA, ADC, RTC, NVMCTRL commands; 1 does not link with XC8 v2.50).
- **Open:** TCA, ADC, RTC, EVSYS, NVM writes, CCP enforcement,
  NMI/compact vector table, cycle timing (busy-wait delays under `-icount shift=auto` run fast);
  SIGROW serial number and TEMPSENSE from silicon into `profiles/avr64ea48.yaml` `data_rows`;
  the 8x8 G Click's physical LED order (the schematic's is used).

## Parts bench with a DC solver (2026-10-15)

The web page has a bench below the board: resistors, LEDs, RGB LED, diode, button, switch,
potentiometer, DC motor, N-MOSFET, servo and voltmeter, wired to the board's pads and rails
(`tools/pp_web/bench.py`, `static/bench.js`; usage in `tools/pp_web/README.md`). Parts sit on
the board picture and jumper wires plug into the holes of its headers (`board.json`
`pin_headers`: MC00 Arduino/EXT/power headers from the user guide's tables, PyKit J9 from the
schematic; positions measured on the board art). Dropped on a header, a part plugs in: its legs
go into the nearest holes as bent leads (R turns it); `APP=shield` is the MC00 demo for it. Every frame
it is solved as a DC circuit from the twin's pads (drive, PWM duty, pulls), and the results go
back in: input pads read their net's level through the PORT's new `pX-bench` layer (patch
0036, both PORT models), and ADC pads read their net's voltage through `ain`. Checked on the
PyKit (stock CircuitPython reads a bench button and pot; the firmware lights a bench LED) and
the MC00 (`blinky` speeds up from a bench button on SW0's pad; a bench LED on LED0's pad blinks
with it, `tests/web/test_bench.py`).

- **Open:** analog (ngspice: RC, inductive kick, motor inertia); an ADC model for the PIC32CM
  U2247 and the AVR ADC so pots work there too; a logic analyzer view; servo period from the
  timer instead of an assumed 50 Hz; `pin_headers` for the JH01 and the AVR64EA48 Nano (and its
Nano Base).

## PIC32CM ADC (2026-10-15)

The JH01 and MC00 twins have an ADC (U2247, patch 0038, overlay `overlays/adc_u2247.yaml`): the
same `ain` interface as the SAM D5x ADC, so a pot on the web bench reaches the firmware
(`tests/web/test_bench.py`: a pot plugged into the MC00's J803 A0 hole reads 2475 mV then 330 mV).
`make BOARD=pic32cm_mc00_cpro APP=adc` checks the internal inputs against the data sheet and then
reports A0. **Open:** run `adc` on the MC00 silicon: the INTREF-against-INTVCC0 line settles
INTVCC0 (data sheet VDDANA/1.48, DFP comment 1/1.6); the AVR64EA48 ADC; SDADC; the DAC on the MC00.

## EDBG data gateway (DGI) and a Data Visualizer panel (2026-10-15)

The MC00 twin's debugger has a data gateway (patch 0037): the DGI command protocol on a chardev,
I2C target 0x28, SPI target on SERCOM1 (SS PA17), GPIO0-3 with 2 MHz timestamps. The command
layer (sign on, version, interface list, configuration, status) was checked against the AVR64EA48
Nano's nEDBG with `py -m harness.dgi_probe` (read-only; it must always stop interfaces and sign
off: an nEDBG left streaming dropped off USB and needed a replug). The web page has a Data
Visualizer panel (Data Stream graph, GPIO lanes, raw bytes); `APP=dgi` is the demo;
`tests/web/test_dgi.py`.

- **Open:** GPIO and timestamp record formats, and the setup GPIO needs, from Data Visualizer's
  DGI host code (the owner is getting it from the DV team). Do not probe DGI GPIO on hardware
  meanwhile: on the nEDBG, enabling GPIO answered 0x99, a poll of the interface that was then off
  got no reply, and the debugger dropped off USB (twice; replug needed). `APP=dgigpio` is ready
  for that comparison. DGI on the
  AVR64EA48 Nano (GPIO on PB2, needs an AVR PORT watch), the JH01 and the PyKit; USART and power
  interfaces; MPLAB Data Visualizer connecting to the twin's socket (on the DV side).

## Curiosity PyKit Explorer (CircuitPython, 2026-10-14)

The first CircuitPython kit and the first Cortex-M4F: the Curiosity CircuitPython board
(EV78P89A, ATSAME51J20A) on the PyKit Ruler (EV82H04A), machine `curiosity-circuitpython`
(patches 0032-0035). Research and the kit's facts: `docs/research/pykit-explorer.md`.

- **Chip:** SAME51_DFP 3.9.267 (`scripts/fetch_packs.py`), profile `profiles/atsame51j20a.yaml`,
  new overlays for the SAM D5x clocks, NVMCTRL, QSPI, TRNG, USB, DMAC, ADC, DAC and EVSYS. The
  generator learned instances with several NVIC lines, register-less instances (PUKCC), the M4
  core defines, several RAM segments, per-device `reset_from` and per-family GCLK sources.
- **Firmware:** `make -C firmware/hello BOARD=curiosity_circuitpython` (`hello`, `blinky`; console
  on the debug UART, so the e2e test runs `-M curiosity-circuitpython,console=debug`).
- **CircuitPython:** the stock build runs unmodified. `py -m tools.pp.circuitpython fetch` caches
  Adafruit's UF2 and its ELF (`tools/pp/uf2.py`); `... run <folder with code.py>` boots it with a
  CIRCUITPY image (`tools/pp/fatfs.py`) and talks to the REPL.
- **Console design:** CircuitPython has its REPL only on USB, so the USB device model carries a
  built-in host that enumerates the device and opens the first CDC ACM port (DTR); its data is
  QEMU's serial0. Prints made before the port opens are dropped, as with a real PC.
- **File system design:** no mass-storage host. The SST26VF032B flash image (`-drive if=mtd`)
  is built and read on the host side with `tools/pp/fatfs.py`, formatted the way CircuitPython
  formats it; the web page edits `code.py` in the image and restarts the twin.
- **Web:** `py -m tools.pp_web --board pykit` (see `tools/pp_web/README.md`): live TFT, the five
  NeoPixels, a tilt stick for the IMU (and the optional BNO085), USER/RESET, the speaker in the
  browser (DAC samples paced by a TC through DMA and EVSYS, as CircuitPython's `audioio` and
  `synthio` do), the CIRCUITPY editor.
- **Kit checks** (read-only REPL, `py -m harness.cp_repl`): identity, UID, 120 MHz, USB IDs, I2C
  scan, file system geometry, board names and the RNBD451's replies match the twin; the kit's
  own robot-arm `code.py` runs on the twin. The die temperature does not (25 C default vs 36 C).
- **Open:** BLE as a "phone" panel on the page (the RNBD451 model has `central-*` properties for
  it); BME680/APDS-9999 plug-ins; SD card (`ssi-sd`); CAN; capacitive touch; the TEMP_LOG
  calibration row from silicon; USB mass storage (a drive the host OS could mount); the BNO085
  is not on the schematics (the owner is asking).
