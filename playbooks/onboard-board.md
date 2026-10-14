# Onboarding a board

You are adding a Microchip development board to Peripheral Pretender. Work through the phases
in order. End each phase at its gate: run the checks, report the result to the owner in a few
lines, and only then continue. If a step says "today by hand", there is no script yet; do it by
hand, and note what a script would need, in your report and in the hand-back.

Read first: `CLAUDE.md` (rules), `docs/handoff.md` (status and machine setup),
`docs/design/v0-vertical-slice.md` (architecture), [oracles.md](oracles.md) (what to believe).

Worked examples: the PIC32CM MC00 Curiosity Pro (EV15N46A, `pic32cm-mc00-cpro`) and the PIC32CM
JH01 Curiosity Pro (EV81X90A, `pic32cm-jh01-cpro`); for 8-bit AVR, the AVR64EA48 Curiosity Nano on
a Curiosity Nano Base (EV66E56A, `avr64ea48-cnano`). A chip that is not a Cortex-M0+ PIC32CM: read
"8-bit AVR and other non-Arm families" at the end first. When unsure how something is done, find how
those two do it. The JH01 was onboarded by a fresh agent from this playbook; its acceptance file
`acceptance/pic32cm_jh01_curiosity_pro.yaml` is the pattern for Phase 3.

## Before you start

- **Windows checkout.** Overlays, profiles and `devices/*.json` must keep LF line endings: the
  generator hashes their bytes (`.gitattributes` enforces this). If `tests/generator` says the
  checked-in outputs are stale right after a checkout, check line endings before anything else.
- **Shared pp-qemu.** The fork lives outside the repository (`C:/pp/pp-qemu`, see
  `docs/handoff.md`) and other sessions may use it. Ask before editing or rebuilding it if
  another session is active. A rebuild needs every QEMU and `tools.pp_web` process stopped, or
  the link fails with "Permission denied" (stop only your own: check the process path).
- **Parallel sessions** work in git worktrees of both repositories
  (`git worktree add <dir> -b <branch>` in the repository and in `C:/pp/pp-qemu`), with their own
  builds in the pp-qemu worktree: copy the downloaded meson subprojects
  (`subprojects/keycodemapdb` and the other folders) from `C:/pp/pp-qemu`, then configure
  `build/` and `build-avr/` with the arguments `qemu/setup.sh` uses and run `ninja` there.
  The "cannot symlink ... keymaps" warnings on Windows are harmless. Point `PP_QEMU`,
  `PP_QEMU_SRC` and `PP_QEMU_AVR` at the worktree's builds.
- **Generator reports** (`generator/out/report-*.md`) are gitignored, but
  `tests/generator/test_outputs.py` wants them: in a fresh checkout or worktree, generate once
  with the reports into the repository and the QEMU files into a scratch folder:
  `py generator/atdf2qemu.py --atdf <JH01> --atdf <MC00> --atdf <AVR64EA48> --qemu-out <scratch> --json-out <scratch> --report "generator/out/report-{device}.md"`
  (the ATDFs: `~/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf`,
  `.../PIC32CM-MC_DFP/1.4.67/PIC32CM1216MC00048/atdf/PIC32CM1216MC00048.atdf`,
  `.../AVR-Ex_DFP/2.8.189/atdf/AVR64EA48.atdf`). To check that the generated files are current
  without touching pp-qemu, generate into a scratch folder and compare.
- **Firmware builds** run in an MSYS2 UCRT64 shell with the packs folder named:
  `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -lc 'cd <repo>/firmware/hello && make BOARD=<board> APP=<app> PACKS="C:/Users/<you>/.mchp_packs"'`.
  Tests skip when their ELF is missing, so build every app before the gate: MC00
  `hello blinky breathe thermo clicks relay eic evsys mpu rtc sercom tc tcc` and the Click demos
  `buzz oledc thumbstick accel rtc6`; JH01 `hello blinky`; `firmware/avr-hello` (all apps).
- **Hardware.** `CLAUDE.md` makes the boards read-only unless the owner approves. Approvals are
  listed in [README.md](README.md#hardware).
- **Python on Windows** is `py`.
- **Firmware already in the repo?** Look for `firmware/*` folders for the board (first-light
  firmware and its golden transcript in `golden/<device>-<rev>/`) before writing new test firmware.

## Phase 0: Inputs

Collect these and write them into a short inputs list (board, chip, where each input lives):

1. **Kit and chip.** The kit's part number (for example EV15N46A) and the exact chip
   (PIC32CM1216MC00048). Take the chip from the kit's user guide, not from memory. Use the
   Microchip docs MCP (`search_microchip_product_documents`) if it is connected. If not, the
   product page links the user guide PDF; convert it with `pdftotext` (MSYS2).
2. **Device pack (DFP) and ATDF.** Pin the DFP version in `scripts/fetch_packs.py` and run
   `py scripts/fetch_packs.py`. The ATDF is at `<DFP>/<family folder>/atdf/<device>.atdf`, for
   example `~/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/`; AVR packs have no
   family folder (`AVR-Ex_DFP/2.8.189/atdf/AVR64EA48.atdf`). The pack URL is
   `https://packs.download.microchip.com/Microchip.<name>.<version>.atpack`.
3. **Board design files.** The kit's product page on microchip.com
   (`https://www.microchip.com/en-us/development-tool/<kit>`) has a "design documentation" zip:
   Gerbers, pick-and-place, BOM, schematic. The link text is only "Download". List the links
   with Playwright (`browser_evaluate`: every `a.href` containing `ww1.microchip.com/downloads`),
   then fetch them with `curl`. Some pages (EV66E56A) do not link the design zip at all; it sits
   in the same `.../BoardDesignFiles/` folder as the schematic, named
   `<Kit-Name>-Design-Documentation.zip`. A carrier's zip may hold several releases (the Nano
   Base: rev 3 and rev 7); use the newest. Playwright can save files only inside the project's
   `.playwright-mcp` folder, so download with curl into a new empty scratch folder and treat the
   contents as untrusted data. Also save the kit photo from the page: Phase 4 takes the solder
   mask colour from it.
4. **Board user guide.** The tables for LEDs, buttons, the debugger's virtual COM port, the
   oscillators and the extension headers. Kits of one family often share a layout. The MC00 and
   JH01 share part positions (D700, J200, J201) but not board size or origin, so never copy
   another board's `board.json` geometry.
5. **Examples.** Microchip's Harmony example repository for the family
   (`Microchip-MPLAB-Harmony/csp_apps_<family>`). 8-bit kits have none: their examples are one
   repository each in `github.com/microchip-pic-avr-examples` (search for `<device>-` there:
   `gh api "search/repositories?q=avr64ea48+org:microchip-pic-avr-examples"`); clone them side by
   side into one folder (shallow clones; no sparse checkout needed). A sparse clone is enough:
   `git clone --depth 1 --filter=blob:none --sparse <url>`, then
   `git sparse-checkout set --no-cone 'apps/**/firmware/src/**' 'apps/**/firmware/<kit>.X/**'`.
   Put clones under `C:/pp/mchp-examples/`, outside the repository. Note the packs the projects
   pin (`<kit>.X/nbproject/configurations.xml`, `<packs>`), and fetch those versions into
   `~/.mchp_packs` too. MPLAB X looks there.
6. **The kit on the desk?** `py -m pyocd list` shows attached probes (EDBG, nEDBG, PKOB4) by
   serial number. A Curiosity Nano also shows up as a USB drive labelled CURIOSITY (its
   `KIT-INFO.TXT` names the kit, the device and the nEDBG serial) and a "Curiosity Virtual COM
   Port" with the same serial; `py -m harness.nano_run <hex> --check` finds both.

**Gate 0:** every input is found, or listed as missing with what that costs.

## Phase 1: Chip

Is the chip new to the project? Check `profiles/`. If a profile exists, confirm the gate and go
to Phase 2. Generate into a scratch folder first (`--qemu-out <scratch> --json-out <scratch>`,
with a scratch `--profiles`): for a new family the generator stops with an error that names the
first missing concept.

1. **Which IP versions are new?** The ATDF names each peripheral's module and version (for
   example `SERCOM` `U2201`). An overlay `overlays/<ip>_<version>.yaml` means it is modelled.
   List the new ones. An IP that has no overlay becomes a generic register block: reads back what
   was written, reset values from the ATDF. Two chips can share an IP name with different
   versions (the MC00 RTC is U2250, the JH01's is 03608); a model for one is not a model for the
   other.
2. **Profile.** Write `profiles/<device>.yaml`: references, silicon revisions, reset values the
   ATDF gets wrong (datasheet or silicon, with the source in a comment). Copy the structure of
   `profiles/pic32cm1216mc00048.yaml`.
3. **Generate.** Every device in one run (the per-IP headers are merged); see
   `generator/README.md`:
   `py generator/atdf2qemu.py --atdf <ATDF 1> --atdf <ATDF 2> ... --qemu-out C:/pp/pp-qemu --json-out devices --report "generator/out/report-{device}.md"`.
   Read the report: it lists what the generator guessed.
4. **SoC type.** Add the device to the SoC model in pp-qemu (`hw/arm/pic32cm_soc.c`, or
   `hw/avr/avr8x_soc.c` for AVR8X: the model table maps modules to device models, the type list
   at the end gets `<device>-soc`, and `hw/<arch>/<device>.c` holds the generated descriptor). A
   different CPU core may need a QEMU CPU change; see `docs/research/core-support-matrix.md` and
   [peripherals/core.md](peripherals/core.md).
5. **Build:** `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash.exe -lc 'cd /c/pp/pp-qemu/build && ninja qemu-system-arm.exe'`.

**Gate 1:** `py -m pytest tests/generator` green; QEMU builds; `qom-test` passes
(in the build folder: `QTEST_QEMU_ARGS="-L C:/pp/pp-qemu/pc-bios" QTEST_QEMU_BINARY=./qemu-system-arm.exe ./tests/qtest/qom-test.exe`;
`build-avr` and `qemu-system-avr.exe` for AVR). qom-test starts every machine without firmware, so
a machine must not exit when `-kernel` is missing.

## Phase 2: Board

1. **Board facts.** From the user guide and schematic: the oscillators (frequencies, crystal or
   external clock), the VCOM UART (SERCOM and pins), each LED (pin, colour, active level, any
   pull resistor), each button (pin, pull), each extension header (pin to signal map), and
   anything on the I2C bus by default (the EDBG data gateway at 0x28 on Curiosity Pro kits).
   Today by hand. Check every pin against the schematic net names. A wrong pin costs hours.
2. **Positions.** From the pick-and-place file: each LED, button and header position in mm (the
   pick-and-place coordinates match the Gerbers).
3. **QEMU machine.** Today by hand: copy `hw/arm/pic32cm_mc00_cpro.c` in pp-qemu, change the
   facts, add it to `hw/arm/meson.build` and `hw/arm/Kconfig`, and add a qtest
   (`tests/qtest/pic32cm-mc00-test.c` is the model). New files carry the provenance header
   (CLAUDE.md). External pull-ups are part of the board: model them (the LED's from VCC; a
   Click's resistor on an open-drain line). For mikroBUS Click slots, give the machine
   `ext1=`/`ext2=` properties and a Click table, as the MC00 has.
4. **Test firmware.** Add `firmware/hello/boards/<board>/` (`board.mk`, `board.h`, `board.c`,
   copied from the closest board). `firmware/hello` is Arm only (arm-none-eabi, CMSIS); 8-bit AVR
   boards use `firmware/avr-hello` (XC8 avr-gcc, `APP=hello|blinky|matrix`). `board.h` names LED0 and SW0 (`LED0_GROUP`, `LED0_MASK`,
   `SW0_GROUP`, `SW0_PIN`); `blinky` uses them. Demos written for one board (`breathe`, `thermo`)
   stop with `#error` elsewhere. Build `hello` and `blinky` (see "Before you start").
5. **Run it:** `py -m tools.pp run firmware/hello/build/<board>/hello.elf --machine <machine>`.
   The runner picks `qemu-system-arm` or `qemu-system-avr` from the ELF; Click options go in the
   machine string (`-M avr64ea48-cnano,mb1=8x8g`).

**Gate 2:** `hello` prints `PP:EXIT 0` with an empty model backlog; the new qtest passes.

## Phase 3: Acceptance

Two checks: Microchip's code on the twin, and the twin against silicon.

1. **Build Microchip's examples:**
   `py -m tools.pp_accept build <examples clone> --kit <kit project name> --out <elves folder>`.
   Given a folder of per-example repositories (8-bit kits), the tool builds each project's first
   configuration with the toolchain the project names (XC8 or XC32; `PP_XC8`, `PP_XC32`); `--kit`
   is then only a label. The 8-bit examples pin older packs: fetch each version they name.
   The kit project name is the `.X` folder name without `.X` (for example
   `pic32cm_jh01_curiosity_pro`); it is also the build configuration. The tool runs MPLAB X's
   `prjMakefilesGenerator` and make with the newest XC32 (`PP_MPLABX`, `PP_XC32` override). A
   project that pins a newer XC32 builds with the installed one. A "Device pack missing" in
   `<app>.build.log` means a pack from Phase 0.5 is not in `~/.mchp_packs`.
2. **Write the acceptance file** `acceptance/<kit>.yaml` (copy the JH01's). For each example:
   - `should`: what it does. Take it from its `main.c` and plib configuration (periods, edges,
     event wiring). The `readme.md` files only link to onlinedocs.
   - `steps`: the scenario (`wait <s>; press; release; send <text>; reset`). Examples never print
     `PP:EXIT`; a scenario that ends while the example still runs is normal.
3. **Run them:** `py -m tools.pp_accept suite acceptance/<kit>.yaml --elves <folder> --json <out>`
   plays each scenario on the twin (paced to the host clock) and records LED transitions, the UART
   and the model backlog. One example by hand:
   `py -m tools.pp_accept run <elf> --machine <machine> --steps "wait 1; press; wait .5; release"`.
4. **Triage every example** and record `verdict` (pass, gap, nt) and `note`:
   - **Always read the backlog first.** An example can print "Success" because an unmodelled
     peripheral just stores its registers (the JH01 CAN examples did). Backlog entries for the
     peripheral under test mean a gap, whatever the UART says.
   - pass: add an `expect` (`led_toggles`, `uart` substrings, `backlog_max`) so the suite checks
     it from now on.
   - gap: name the missing model. Fix it with the method in [oracles.md](oracles.md) and the
     peripheral playbook, or leave it listed.
   - nt (not testable): needs a part, a second board or a host the twin does not have.
5. **Silicon** (kit on the desk, owner approval per README). Run the checked tests on silicon
   and on the twin and compare: `py -m harness.hw_run <elf> --allow-program --out silicon.txt`
   and `py -m tools.pp run <elf> --machine <machine>`. Curiosity Nano kits (8-bit parts have no
   pyOCD target): `py -m harness.nano_run <hex> --allow-program --out silicon.bin` programs by
   drag and drop. It refuses hex files with fuses or lock bits; MCC examples carry fuses, so strip
   them (`avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures`).
   `--baud` for examples that do not use 115200.
6. **Simulator** where it is mature (8-bit PIC and AVR, see [oracles.md](oracles.md)):
   `py -m harness.mdb_sim <elf> --device <device> --txdata <TXDATAL address>`. The first programming of a production
   part needs a chip erase (BOOTPROT protects the factory bootloader; see
   `docs/silicon-harness.md` §4.5).

**Gate 3:** `tools.pp_accept suite` exits 0 (every `expect` met), and every example has a verdict
and note.

## Phase 4: Web

1. **Render.** Render the Gerbers with tracespace 4.2.1 and rasterize with resvg-js 2.6.2. One-time
   install in a scratch folder: `npm install --ignore-scripts @tracespace/cli@4.2.1 @resvg/resvg-js@2.6.2`.
   - Layers: copper, solder mask, silkscreen, paste, outline, and the drill files. Leave out
     drawing frames and binary drill drawings: some releases name the real drill files
     `*-RoundHoles.TXT` and `*-SlotHoles.TXT` while the `.DRL` is a drawing.
   - Then `py tools/pp_web/boards/recolor_board.py <top.svg> out.svg --mask "<colour>" --edge <x,y,w,h>`.
     The mask colour comes from the kit photo. `--edge` is the board outline in Gerber units;
     with no outline layer, use the silkscreen frame or the board size from the user guide. The
     y flip is read from the SVG.
   - Rasterize at 20 px/mm: `new Resvg(svg, {fitTo: {mode: "width", value: <mm * 20>}})`.
   - A board that always sits on a carrier (a Curiosity Nano on a Nano Base): render both, then
     `py tools/pp_web/boards/compose_carrier.py <carrier.svg> W,H <board.svg> w,h --place X,Y,ROT`
     puts the board into the carrier's socket; `--point u,v` converts board positions to carrier
     mm. See `boards/ev66e56a` in `tools/pp_web/README.md`.
   - resvg may round a size up by one pixel; the web tests want `round(mm * 20)`, so resize.
2. **Board file.** `tools/pp_web/boards/<id>/board.json` (copy EV81X90A's; EV66E56A's for a
   carrier with mikroBUS sockets, which use `socket` instead of `adapter`): machine, `vcom`
   label, image origin and size, LEDs, buttons, headers, and `demo_elf` (a firmware build the page
   starts when someone picks this board in its board picker; the blinky demo is a good choice). A header gets `slot` (and `adapter`)
   only if the QEMU machine has a Click slot for it; otherwise give `int_pin` and leave `slot`
   out, and the page hides the Click section. The server polls every PORT group that the LEDs
   and INT pins use.
3. **Check it:** `py -m tools.pp_web --board <id> --elf firmware/hello/build/<board>/blinky.elf --port <free port>`.
   The default port 8080 may be taken; stop your server when done. The LED blinks at 2 Hz, and
   holding SW0 gives 8 Hz and "SW0 pressed".
4. **Web tests.** Add the board to `tests/web/test_pp_web.py` like EV81X90A: a board description
   test and a blinky session test.

**Gate 4:** web tests green; a screenshot of the board on the page in the report.

## Phase 5: Hand-back

1. Update docs: `docs/handoff.md` (status), `qemu/README.md` (patch table), the machine's rst
   doc in pp-qemu, `tools/pp_web/README.md`.
2. pp-qemu: commit, export the patch: `git format-patch -1 --start-number <N> -o qemu/patches`.
3. Run everything: `PP_QEMU=C:/pp/pp-qemu/build/qemu-system-arm.exe PP_QEMU_SRC=C:/pp/pp-qemu PP_MSYS2_BIN=C:/msys64/ucrt64/bin py -m pytest tests harness/tests`
   (add `PP_QEMU_AVR=<checkout>/build-avr/qemu-system-avr.exe` when that build is not beside
   `PP_QEMU`'s; the AVR tests skip when no AVR build has the machine).
   Commit only if pytest exits 0 (gate on the exit status, not on grepping its output).
4. Report: what works, the acceptance results, known gaps, and **what in this playbook was
   missing or wrong**. Fix the playbook in the same commit.

**Gate 5:** the commit is pushed and the playbook is updated.

## 8-bit AVR and other non-Arm families

Learned on the AVR64EA48 Curiosity Nano (2026-10-14). Everything above applies; this is what
differs.

- **CPU.** QEMU's AVR target models the classic cores (avr5/51/6) only. AVR Dx/Ex, tinyAVR 0/1/2
  and megaAVR 0 are AVRxt: use the `avrxt` CPU type (I/O at data address 0, no register file in
  the data space, CPUINT levels). Parts with more than 64 KB of flash (AVR128Dx) also need
  RAMPZ/ELPM on that type; XMEGA (AVRxm) shares the data-space layout (`AVR_FEATURE_IO_DATA`)
  but not CPUINT. avr-gcc names its architectures by memory size (`avrxmega2`), not by core;
  `hw/avr/boot.c` maps them. Other families (PIC16/18, dsPIC, PIC32 MIPS) have no usable QEMU
  target: stop and ask the owner (`docs/research/core-support-matrix.md`).
- **One QEMU binary per family.** Build `pp-qemu/build-avr` (`--target-list=avr-softmmu`) beside
  `build/`; `qemu/setup.sh` does both. Every tool takes the binary from the ELF
  (`tools/pp/qemu.py`: `find_qemu(target=...)`, `$PP_QEMU_AVR`). A new board's machine name goes
  into `AVR_MACHINES` there.
- **Generator.** ATDF architecture `AVR8X` selects `generator/mchpgen/avrgen.py`: no NVIC, NVM rows
  or APB slots; several vectors per instance. Module ids such as `avrea` stand where Arm parts
  have IP versions, and modules carry no version (overlays use `versions: [""]`). Profiles have
  no `nvm_rows`; `silicon_revisions` are SYSCFG.REVID values; `data_rows` carry silicon bytes
  (SIGROW serial number, TEMPSENSE) when known.
- **Models.** AVR peripherals are different IPs from the SAM ones with the same names (PORT, RTC,
  NVMCTRL). The generic register block logs everything not modelled, so the backlog lists what
  an example needs. `avr8x-*` models exist for PORT/VPORT, USART, SPI, TCB, CLKCTRL, CPUINT,
  NVMCTRL (FLMAP), TWI (host) and the NVM-backed rows; the examples want TCA, ADC and RTC next.
- **Timing.** TCG has no cycle counts: `_delay_ms` busy loops run at host speed under
  `-icount shift=auto` (the web view). Demos for the page take time from a timer interrupt
  (TCB0), as `firmware/avr-hello/blinky.c` does.
- **Silicon.** Curiosity Nano kits program by drag and drop (`harness.nano_run`). Never write
  fuses or lock bits from the playbook: the tool refuses such images.
- **Simulator.** MPLAB SIM runs avr-gcc ELFs directly but its UART I/O stays silent for AVR8X;
  `harness.mdb_sim` reads the USART stores instead. It does not implement SIGROW (reads 0) or REVID.
