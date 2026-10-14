# The pp-qemu fork

The twin's QEMU side lives in a separate checkout, `pp-qemu`, next to this repo: upstream QEMU
**v11.1.2** plus the commits in `patches/`, on branch `pp/main`. This repo carries the fork as
patches rather than a full QEMU history. These patches are AI-assisted and must never be submitted
upstream, because QEMU declines AI-generated contributions (`docs/devel/code-provenance.rst`).

| patch | content |
|---|---|
| 0001 | `scripts/symlink-install-tree.py`: skip symlink failures on Windows (no Developer Mode/admin needed) |
| 0002 | generated per-IP register tables (`include/hw/mchp/`) |
| 0003 | generated device description and NVM-rows image (`include/hw/arm/pic32cm5164jh01100_gen.h`, `hw/arm/pic32cm5164jh01100_nvmrows.h`) |
| 0004 | part A: register-file helper, generic regblock, clock and system models, SoC, `pic32cm-jh01-cpro` board, qtest, board docs |
| 0005 | part B: SERCOM USART and PORT models, LED0 and SW0 on the board, SERCOM and PORT qtests |
| 0006 | review fixes: clock migration, SERCOM loadvm, ICSR.RETTOBASE on ARMv6-M, unimplemented-write log cap, WDT CONFIG/EWCTRL protection |
| 0007 | generic `pic32cm-soc` built from generated device descriptors; PIC32CM1216MC00048 and the `pic32cm-mc00-cpro` board; tables regenerated for both devices (shared constants, per-device tables); MC00 qtest and board docs |
| 0008 | reset values checked against PIC32CM MC00 silicon (golden reset dump): DSU ROM table, MTB IDs, OSC48MRDY flag, PA30 SWCLK, EVSYS, MC00 calibration and bus-matrix values |
| 0009 | EIC model (edges, levels, NMI, clock-dependent synchronization as on silicon); PORT routes pads muxed to function A to the EIC; EIC qtest |
| 0010 | TC model (COUNT8/16/32, commands, one-shot, buffering, pairing), behaviour checked on silicon; regblock gclk_update hook; TC qtest |
| 0011 | RTC model for the MC00 (three modes, PER flags, calendar, alarm); OSC32KCTRL CLK_RTC_OSC output; power-on-only reset for RTC and OSC32KCTRL (as on silicon); RTC qtest |
| 0012 | Per-instance counter width for TCC registers (TCC0/1 24-bit, TCC2 16-bit; ATDF SIZE), matching silicon reset values |
| 0013 | EVSYS model (channels, users, software events, CHSTATUS/OVR/EVD as on silicon); generated event table (MchpDeviceInfo.events) wired by the SoC; event outputs of EIC (levels for level senses), TC and RTC; event inputs of TC (COUNT, RETRIGGER, START) and PORT (OUT/SET/CLR/TGL); EVSYS qtest |
| 0014 | `cortex-m0plus` CPU type (M0+ r0p1 CPUID, 8-region ARMv6-M MPU in the PMSAv7 format, CONTROL.nPRIV); ARMv6-M MPU_RASR/RBAR masks as on silicon; fix for disabled MPU subregions caching a whole TLB page; PIC32CM SoC uses it; board docs brought up to date |
| 0015 | TCC model (single/dual-slope waveform modes, flags, STATUS.CMPx, one-shot, commands, buffers with silicon SYNCBUSY behaviour, CNTSEL, event inputs/outputs), checked against MC00 silicon; TCC qtest; board docs |
| 0016 | Peripheral outputs on pads: PORT `pmux-in`/`pmux-active`/`pad` lines and per-pad duty accounting (`pX-stats`); TCC waveform outputs WO[x] (pin level checked on silicon); SoC wiring from the signal table; LED pull-up on the boards; TC/TCC keep scheduling after a catch-up jump |
| 0017 | SERCOM SPI host and I2C host (on the QEMU SSI and I2C buses), checked against MC00 silicon; EDBG data gateway I2C target (0x28) on the MC00 board; qtests |

## Setting it up

The QEMU path must not contain spaces (QEMU's configure rejects them). Clone this repo somewhere
like `~/src/peripheral-pretender`, so the fork lands at `~/src/pp-qemu`.

```
qemu/setup.sh            # clone v11.1.2 to ../pp-qemu, apply patches, configure, build
qemu/setup.sh /path/qemu # or choose the location
```

Build dependencies:

- **Ubuntu/Debian:** `sudo apt install git build-essential ninja-build python3 python3-venv
  pkg-config libglib2.0-dev libpixman-1-dev libfdt-dev flex bison gcc-arm-none-eabi
  libnewlib-arm-none-eabi gdb-multiarch`
- **Windows (MSYS2, UCRT64 shell):** `pacman -S --needed base-devel git mingw-w64-ucrt-x86_64-toolchain
  mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pixman
  mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-dtc mingw-w64-ucrt-x86_64-arm-none-eabi-toolchain`.
  MSYS2 deprecated the MINGW64 environment in March 2026; QEMU's docs still mention it, but use UCRT64.
- **macOS:** `brew install ninja pkg-config glib pixman dtc` plus an Arm GNU toolchain.

Configure options used everywhere: `--target-list=arm-softmmu --without-default-features
--enable-fdt=system --disable-docs --disable-werror`. `-Werror` is off because GCC 16 is newer than
anything QEMU 11.1 was tested with.

## Working on the fork

Edit and commit in `pp-qemu` on `pp/main`, then refresh the patches here:

```
rm qemu/patches/*.patch
git -C ../pp-qemu format-patch v11.1.2..pp/main -o "$PWD/qemu/patches"
```

Once the fork grows, it may be easier to keep it as its own private GitHub repo (a fork of
`qemu/qemu` with a `pp/main` branch) and drop the patch files.

## Running

```
../pp-qemu/build/qemu-system-arm -M pic32cm-jh01-cpro -kernel firmware/hello/build/pic32cm_jh01_cpro/hello.elf \
    -display none -serial stdio -d unimp,guest_errors
python -m tools.pp run firmware/hello/build/pic32cm_jh01_cpro/hello.elf     # runner: UART transcript + exit code
```

For a debugger, use `-gdb tcp:127.0.0.1:1234 -S`, not `-s`. On Windows, `-s` listens on every
interface and triggers a Windows Firewall prompt (an admin dialog) for each QEMU binary location.
