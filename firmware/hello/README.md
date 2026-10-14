# hello: bring-up image for the PIC32CM Curiosity Pro boards

One test image, built per board with `make BOARD=<board>`:

| `BOARD` | kit | device | twin machine |
|---|---|---|---|
| `pic32cm_jh01_cpro` (default) | PIC32CM JH01 Curiosity Pro (EV81X90A) | PIC32CM5164JH01100 | `pic32cm-jh01-cpro` |
| `pic32cm_mc00_cpro` | PIC32CM MC00 Curiosity Pro (EV15N46A) | PIC32CM1216MC00048 | `pic32cm-mc00-cpro` |

Peripheral tests share the board bring-up and console: `make APP=<test>
BOARD=<board>` builds `tests/<test>.c` into `build/<board>/<test>.elf`.

| `APP` | checks |
|---|---|
| `hello` (default) | the Harmony bring-up, identity, interrupt-driven print, LED0 |
| `eic` | EIC edges, levels, flag clearing, interrupt, and the clock needed by synchronous edges and their CTRLA synchronization; stimulus: the free test pin's pull resistor (`TEST_PIN` in `board.h`) |
| `tc` | TC counting, READSYNC, commands, one-shot, down counting, PER/CC buffering, compare match, MFRQ, counting rate, COUNT32 pairing, overflow interrupt, synchronization without the clock |
| `tcprobe` | raw TC observations for behaviour the data sheet leaves open (prints, checks nothing) |
| `rtc` | RTC (MC00) counting rate, periodic flags, compare with clear-on-match, MODE1, calendar across a leap day, alarm, interrupt, synchronization without the RTC clock |
| `evsys` | EVSYS (MC00) paths with software events and generators (TC0, EIC, RTC), edge selection, the channel clock rule, CHSTATUS and OVR/EVD with and without ready users, the interrupt, register masks, TC1 event actions (COUNT, RETRIGGER, START, TCINV) and PORT event actions |
| `tcc` | TCC (MC00): output state and flags in NPWM, NFRQ and dual slope, one-shot, STOP/RETRIGGER, buffers and LUPD, CNTSEL, events in and out through EVSYS, counter width, interrupt, synchronization without the clock |
| `tccprobe` | raw TCC observations, in rounds (`-DPROBE_ROUNDS=`): waveform modes, CNTSEL/STOP/buffers, register writes, event actions, the waveform pin (duty and drive) |
| `sercom` | SERCOM I2C host (bus states, ACK/NACK flags, stop, scan, general call, the EDBG data gateway at 0x28) and SPI host (pad loopback, open MISO, overflow, receiver off) |
| `sercomprobe` | raw SERCOM I2C/SPI observations (prints, checks nothing) |
| `breathe` | demo for the web board view: LED0 dimmed by a TCC1 PWM on PA24 (function F), duty rising and falling every 2 s; SW0 held for full brightness |
| `blinky` | demo for the web board view (`tools/pp_web`): LED0 at 2 Hz, 8 Hz while SW0 is held, presses logged on VCOM; runs forever |
| `mpu` | Cortex-M0+ MPU: MPU_TYPE, register read-back, read-only, XN, subregion and background faults (HardFault on ARMv6-M), unprivileged mode against a privileged-only region |
| `evprobe` | raw EVSYS observations for behaviour the data sheet leaves open (prints, checks nothing) |

Their transcripts are deterministic (timing checks have tolerances; run the
twin with `--icount 5`); silicon captures live in `golden/`.

Most of this README describes the JH01 build; the MC00 build differs only
where its board does (see [PIC32CM MC00 Curiosity Pro](#pic32cm-mc00-curiosity-pro)).


Bare-metal test image for the PIC32CM JH01 Curiosity Pro. It brings the chip
up exactly the way MPLAB Harmony does, reports what the hardware returns over
the EDBG virtual COM port, blinks LED0 three times and ends with
`PP:EXIT <n>`, where `n` is the number of failed checks (0 = pass). The same
ELF is meant to run unchanged on silicon and on the twin
(`-M pic32cm-jh01-cpro`), so the two transcripts can be compared line by line.

Contract: `docs/design/v0-vertical-slice.md` §5.6 and §6.2. Register
sequence: `docs/research/boot-path.md`, table "Pre-main register touches",
steps 3-8.

Status (v0): builds warning-free; `make check` passes; the image runs to
`PP:EXIT 0` on a throwaway functional mock. It has not yet run on the twin
(the machine is still being built) or on silicon (programming the board needs
the user's approval).

## Files

| path | purpose |
|---|---|
| `boards/<board>/board.c` | Harmony `SYS_Initialize` replica for the board (the same registers, values, access widths, order and busy-waits as the generated code), and the read-back report of each step |
| `boards/<board>/board.h` | Board wiring: device header, VCOM SERCOM and its handler, LED0 pin, banner |
| `boards/<board>/board.mk` | Device, DFP directory and VCOM vector for the Makefile and `make check` |
| `src/console.c` | VCOM output: blocking DRE-polled TX, hex/decimal formatting, the interrupt-driven print and `VCOM_Handler` (`SERCOM4_Handler`, IRQ13, on the JH01) |
| `src/main.c` | Test sequence: identity data, IRQ print, SysTick-timed LED0 blinks, exit line |
| `dfp/` | Startup files and flash linker scripts copied unmodified from the DFPs: `startup_pic32cm5164jh01100.c`, `pic32cm5164jh01100_flash.ld` (PIC32CM-JH_DFP 1.3.221, `PIC32CM-JH01/gcc/gcc/`), `startup_pic32cm1216mc00048.c`, `pic32cm1216mc00048_flash.ld` (PIC32CM-MC_DFP 1.4.67, `PIC32CM1216MC00048/gcc/gcc/`); Apache-2.0 headers intact |
| `check-elf.sh` | Static checks on the ELF (`make check`) |

## Build

Needs `arm-none-eabi-gcc` with newlib-nano (16.1 tested) and GNU make; on this
workstation both come from MSYS2 UCRT64. The DFP headers and CMSIS 6.1.0 are
read from `%USERPROFILE%\.mchp_packs`.

From PowerShell, in the repository root:

```powershell
$env:MSYSTEM = 'UCRT64'; $env:CHERE_INVOKING = '1'
& C:\msys64\usr\bin\bash.exe -lc 'make -C firmware/hello'
& C:\msys64\usr\bin\bash.exe -lc 'make -C firmware/hello check'
```

From an MSYS2 UCRT64 shell: `make -C firmware/hello` and
`make -C firmware/hello check`. If the packs are somewhere else, pass
`PACKS=<dir holding Microchip/ and ARM/>`, or set `DFP=` and `CMSIS=`
individually.

Add `BOARD=pic32cm_mc00_cpro` for the MC00 board. Outputs in
`build/<board>/`: `hello.elf`, `hello.hex`, `hello.map` and `hello.lst`
(`objdump -h -S`). The build ends by printing `arm-none-eabi-size`:

```
   text    data     bss     dec     hex filename
   2604       0    1544    4148    1034 build/pic32cm_jh01_cpro/hello.elf
```

(bss includes the linker script's 0x200 heap and 0x400 stack.)

Compiler flags: `-mcpu=cortex-m0plus -mthumb -std=gnu11 -Os -g
-ffunction-sections -fdata-sections -Wall -Wextra`. Linking uses the DFP
linker script with `--gc-sections`, a map file and `--entry=Reset_Handler`
(the script has no `ENTRY()`).

The DFP startup replaces newlib's `crt0`, so the image links with
`-nostartfiles`. The startup still calls `__libc_init_array()`: that comes
from newlib-nano (`--specs=nano.specs`), and the `_init()` it calls comes from
the toolchain's `crti.o`/`crtn.o`, which the Makefile links explicitly.
`nosys.specs` is not used, because nothing references a syscall. With the
default start files, `crt0`'s `exit()` drags in stdio, and libnosys then
prints "not implemented" link warnings.

`make check` verifies:
- the vector table (`exception_table`, section `.vectors`) sits at 0x0
- word 0 is `_estack`, the reset vector and the ELF entry are `Reset_Handler`, and the VCOM vector is the strong handler (`board.mk`: IRQ13 `SERCOM4_Handler` on the JH01, IRQ9 `SERCOM0_Handler` on the MC00)
- HardFault and SysTick go to the DFP's `Dummy_Handler`, which must be an endless `b .`
- there are no BKPT or SVC instructions and no semihosting symbols

QEMU calls `cpu_abort` on lockup, which a BKPT inside a fault handler
triggers. On silicon without a debugger attached, a BKPT is a HardFault.

## What it does

1. **Harmony `SYS_Initialize` (steps 3-8)**, run unchanged before anything is
   printed. The accesses are listed under
   [Peripheral accesses before the first byte](#peripheral-accesses-before-the-first-byte).
   1. NVMCTRL.CTRLB = RWS(3).
   2. Route PB10/PB11 to SERCOM4 PAD2/PAD3 (function D) and make PC05 an output.
   3. Load CAL48M from bits 40:19 of a 64-bit read of the software
      calibration row at 0x00806020.
   4. Set OSC48MDIV = 0 and wait for OSC48MSYNCBUSY, then wait for
      OSC48MRDY. Set OSC48MCTRL |= ONDEMAND, OSC32K = 0 and RTCCTRL = 0.
   5. Set GENCTRL0 = OSC48M/1 and wait for SYNCBUSY.GENCTRL0. Set
      PCHCTRL[22] = GCLK0 | CHEN and wait for CHEN to read back.
   6. Set APBCMASK = 0x08000020 and NVMCTRL.CTRLB = 0x84.
   7. Configure SERCOM4 for 115200 8N1 (CTRLA 0x40310104, BAUD 63019,
      CTRLB 0x30000), with a SYNCBUSY wait both before and after ENABLE.
   8. NVIC: `cpsie i`, IRQ13 at priority 3, enabled.
2. **Report**: one line per step. Each value is read back after initialization
   finishes, so the sequence itself stays identical to Harmony's. Then DSU.DID,
   SCB->CPUID, the four serial-number words and the calibration row.
3. **One interrupt-driven print**: INTENSET.DRE is set, and `SERCOM4_Handler`
   sends one byte per DRE interrupt and writes INTENCLR.DRE after the last
   byte. Because the line is level-sensitive, the handler re-enters once per
   byte. The main loop waits at most 100 ms of SysTick time.
4. **LED0 blinks three times**: PC05 is active low; each blink is 200 ms on and
   200 ms off. SysTick runs from the 48 MHz CPU clock (CLKSOURCE = 1, a 1 ms
   reload) and is polled via COUNTFLAG, with no SysTick interrupt.
5. **Exit**: `PP:EXIT <failures>`, a wait for TXC so the last stop bit is on
   the wire, then `WFI` forever.

All output uses the blocking path except step 3. Lines end in CRLF.

## Expected UART transcript

115200 8N1 on SERCOM4 (EDBG VCOM). Lines end in `\r\n`. These are the values
the twin should produce with the default NVM image of design §4.3:

```
PP hello: PIC32CM5164JH01100 on PIC32CM JH01 Curiosity Pro (EV81X90A)
step 3 nvmctrl: CTRLB <- RWS(3)
step 4 port: PB.PINCFG10=0x01 PB.PINCFG11=0x01 PB.PMUX5=0x33 PC.DIR=0x00000020
step 5 clock: CAL48M=0x00200220 OSC48MDIV=0x00 GENCTRL0=0x00010106 PCHCTRL22=0x00000040 APBCMASK=0x08000020
step 6 nvmctrl: CTRLB=0x00000084
step 7 sercom4: CTRLA=0x40310106 BAUD=0xF62B CTRLB=0x00030000
step 8 nvic: ISER=0x00002000 IPR3=0x0000C000
dsu: DID=0x11060100
scb: CPUID=0x410CC200
serial: 0x50500000 0x00000001 0x00000002 0x00000003
calrow: 0x0000010011040249
irq13: sent by SERCOM4_Handler, one byte per DRE interrupt
led0: blink 1/3
led0: blink 2/3
led0: blink 3/3
PP:EXIT 0
```

Every other line must be byte-identical on the twin and on silicon. These
lines legitimately differ:

| line | twin | silicon | regex for comparisons |
|---|---|---|---|
| `step 5 clock:` (CAL48M field only) | bits 40:19 of the twin's calibration row (0x00200220 if the default row uses the ATDF reset value), masked to 0x003F033F | the factory CAL48M from the chip's calibration row | `CAL48M=0x[0-9A-F]{8}` |
| `dsu: DID=` | 0x11060100 (SoC property `silicon-rev`, B0 by default) | 0x11060X00, X = die revision (1 = B0, 2 = C0) | `DID=0x11060[0-9A-F]00` |
| `scb: CPUID=` | 0x410CC200 (QEMU `cortex-m0`), or 0x410CC601 if the SoC overrides MIDR | expected 0x410CC601 (Cortex-M0+ r0p1; not yet read from silicon) | `CPUID=0x410CC[0-9A-F]{3}` |
| `serial:` | SoC property `serial-number` (design §4.3 example shown) | unique per chip | `serial:( 0x[0-9A-F]{8}){4}` |
| `calrow:` | the twin's default calibration row (example shown: ADC bias 0x249, OSC32K CAL 0x40, CAL48M 0x00200220) | factory values, to be dumped from a board | `calrow: 0x[0-9A-F]{16}` |

The calibration row is printed as one 64-bit number, word 1 (0x00806024) then
word 0 (0x00806020). The serial-number words are those at 0x0080A00C,
0x0080A040, 0x0080A044 and 0x0080A048 (DS60001632H §10.3; the DFP does not
define them). BAUD 0xF62B is 63019.

### Exit code and failure lines

`n` in `PP:EXIT n` counts failed checks:
- every read-back in the step 4-8 lines except CAL48M, against the values in
  `docs/research/boot-path.md`
- DSU.DID with the revision bits masked, against the DFP's `CHIP_DSU_DID`
- completion of the interrupt-driven print

A failed field is annotated in place, for example
`CTRLA=0x40310104(expected 0x40310106)`. If IRQ13 never runs, the line
`irq13: FAIL, interrupt-driven print stalled` replaces the irq13 line. If the
print stops part-way, the partial line is terminated and the FAIL line
follows. Either way the exit code is at least 1.

### When there is no output

The Harmony waits are unbounded, exactly as in the generated code:
OSC48MSYNCBUSY, OSC48MRDY, GCLK SYNCBUSY.GENCTRL0, PCHCTRL[22].CHEN, the two
SERCOM4 SYNCBUSY waits, DRE before each byte and TXC at the end. A model that
never sets one of these bits therefore hangs before or during the first line,
and the runner reports a timeout. To find the loop, run the twin with `-s -S`
and attach `gdb-multiarch build/pic32cm_jh01_cpro/hello.elf` (`target remote :1234`, `continue`,
Ctrl-C, then `bt` or `info symbol $pc`). The functions involved are
`oscctrl_initialize`, `gclk0_initialize`, `clock_initialize`,
`sercom4_usart_initialize`, `console_putc` and `console_flush`. A model that
never sets TXC only spins in `console_flush` after `PP:EXIT` has been sent,
which a runner never notices.

Timing: the blinks take 1.2 s of SysTick time. On the twin that is virtual
time and needs the SoC to drive the armv7m `cpuclk` at 48 MHz after step 5.
If `cpuclk` stays at 4 MHz, the blinks take 12 times longer; if it is 0 Hz,
SysTick never counts.

## Peripheral accesses before the first byte

This is the expected bus sequence on any correct model: the mock's MMIO trace
matched the disassembly. Each wait loop repeats its read until the condition
holds; on the twin that happens on the first read.

```
W32 0x41004004 = 0x00000006   NVMCTRL.CTRLB          step 3
W8  0x410000CA = 0x01         PB PINCFG[10]          step 4
W8  0x410000CB = 0x01         PB PINCFG[11]
W8  0x410000B5 = 0x33         PB PMUX[5]
W32 0x41000100 = 0x00000020   PC DIR
W8  0x41000145 = 0x00         PC PINCFG[5]
W8  0x41000132 = 0x00         PC PMUX[2]
R32 0x00806024, 0x00806020    calibration row (ROM)  step 5
W32 0x40001038 = CAL48M       OSCCTRL.CAL48M
W8  0x40001015 = 0x00         OSCCTRL.OSC48MDIV
R32 0x40001018 until bit 2=0  OSCCTRL.OSC48MSYNCBUSY
R32 0x4000100C until bit 4=1  OSCCTRL.STATUS (OSC48MRDY)
R8  0x40001014, W8 |= 0x80    OSCCTRL.OSC48MCTRL (ONDEMAND)
W32 0x40001418 = 0x00000000   OSC32KCTRL.OSC32K
W32 0x40001410 = 0x00000000   OSC32KCTRL.RTCCTRL
W32 0x40001C20 = 0x00010106   GCLK.GENCTRL[0]
R32 0x40001C04 until bit 2=0  GCLK.SYNCBUSY
W32 0x40001CD8 = 0x00000040   GCLK.PCHCTRL[22]
R32 0x40001CD8 until bit 6=1  (CHEN read-back)
W32 0x4000081C = 0x08000020   MCLK.APBCMASK
W32 0x41004004 = 0x00000084   NVMCTRL.CTRLB          step 6
W32 0x42001400 = 0x40310104   SERCOM4.CTRLA          step 7
W16 0x4200140C = 0xF62B       SERCOM4.BAUD
W32 0x42001404 = 0x00030000   SERCOM4.CTRLB
R32 0x4200141C until 0        SERCOM4.SYNCBUSY
R32 0x42001400, W32 |= 0x2    SERCOM4.CTRLA (ENABLE)
R32 0x4200141C until 0        SERCOM4.SYNCBUSY
dmb; cpsie i; IPR3 |= 0xC000 (RMW 0xE000E40C); ISER = 0x2000   step 8
R8  0x42001418 until bit 0=1  SERCOM4.INTFLAG (DRE), then W16 0x42001428 = 'P'
```

Before `main`, the DFP `Reset_Handler` only touches SRAM (it copies `.relocate`
and zeroes `.bss`) and writes VTOR = 0.

## Running it

Twin, once `pic32cm-jh01-cpro` exists: `py -m tools.pp run
firmware/hello/build/pic32cm_jh01_cpro/hello.elf` (design §6.3). The raw equivalent is
`qemu-system-arm -M pic32cm-jh01-cpro -kernel firmware/hello/build/pic32cm_jh01_cpro/hello.elf
-display none -serial stdio -d unimp,guest_errors`, stopped once
`PP:EXIT` appears.

Silicon: program `build/<board>/hello.hex` with MPLAB IPE or pyOCD, which needs the
user's approval (design §6.4). Then open the EDBG virtual COM port at
115200 8N1 with DTR asserted and press reset. LED0 lights at PORT init and
then blinks three times.

## PIC32CM MC00 Curiosity Pro

`make BOARD=pic32cm_mc00_cpro`, run with `py -m tools.pp run
firmware/hello/build/pic32cm_mc00_cpro/hello.elf --machine pic32cm-mc00-cpro`.
`boards/pic32cm_mc00_cpro/board.c` replays what Harmony generates for the
`csp_apps_pic32cm_mc00` usart_echo_interrupt example (configuration
`pic32cm_mc00_curiosity_pro`). The differences from the JH01 build:

- EDBG VCOM on SERCOM0 (PA06 TX = PAD2, PA07 RX = PAD3, function D; IRQ9),
  core clock on GCLK channel 19, APBCMASK = 0x2.
- LED0 on PA24 (active low). PORT_Initialize writes DIR and OUT of group 0
  directly (PA24 high, LED off) instead of the JH01's PC05 DIR write.
- DSU.DID is 0x11070X06 (X = revision: A1 = 0, A2 = 1).
- The board has a 12 MHz oscillator on XIN and a 32.768 kHz one on XIN32;
  like Harmony, the image uses neither.

Transcript on the twin (default NVM image, whose calibration rows come from
a real rev A1 part; the default revision is A2, so DID reads 0x11070106 unless
`-global pic32cm1216mc00048-soc.silicon-rev=A1` is given):

```
PP hello: PIC32CM1216MC00048 on PIC32CM MC00 Curiosity Pro (EV15N46A)
step 3 nvmctrl: CTRLB <- RWS(3)
step 4 port: PA.PINCFG6=0x01 PA.PINCFG7=0x01 PA.PMUX3=0x33 PA.DIR=0x01000000
step 5 clock: CAL48M=0x00210320 OSC48MDIV=0x00 GENCTRL0=0x00010106 PCHCTRL19=0x00000040 APBCMASK=0x00000002
step 6 nvmctrl: CTRLB=0x00000084
step 7 sercom0: CTRLA=0x40310106 BAUD=0xF62B CTRLB=0x00030000
step 8 nvic: ISER=0x00000200 IPR2=0x0000C000
dsu: DID=0x11070106
scb: CPUID=0x410CC601
serial: 0x50500000 0x00000001 0x00000002 0x00000003
calrow: 0xC20641081904FFBE
irq9: sent by SERCOM0_Handler, one byte per DRE interrupt
led0: blink 1/3
led0: blink 2/3
led0: blink 3/3
PP:EXIT 0
```

## Notes on Harmony fidelity

- The sequence follows the board's csp_apps `usart_echo_interrupt` example.
  That example was generated by a Harmony version that still loads CAL48M from
  the calibration row. Current `clk_sam_c20_c21` templates skip that write on
  PIC32CM JH, so field images exist in both variants (boot-path finding 7).
- Literals are written with DFP macros. The disassembly was checked to produce
  the same values and access widths as the generated code. Harmony's
  `== MSK`/`!= MSK` loop tests become `!= 0`/`== 0` here; the masks are single
  bits, so the bus behaviour is identical.
- Harmony's first print is interrupt-driven: `SERCOM4_USART_Write` primes DATA
  while DRE is set, then enables INTENSET.DRE. Here all output is polled except
  one line, which the handler sends a byte at a time without priming. On an
  instant-TX model, that exercises IRQ re-pending more than Harmony's priming
  loop would.

## Provenance

`src/`, `Makefile`, `check-elf.sh` and this file: Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
AI-assisted.

`dfp/`: unmodified Microchip files under Apache-2.0. Open questions are kept
visible per design §3: how Apache-2.0 DFP files combine with the GPLv2 QEMU
fork, and binary-distribution obligations. SHA-256:

```
776e701cd9021f7131501c0c1c4c9e33414fdb7ccb4bd0465ac0cb9591572620  startup_pic32cm5164jh01100.c
84b4aac6c70d9ec767b541e7a4a618d76ca1b6f21bd99a5907ae314e066b2693  pic32cm5164jh01100_flash.ld
```
