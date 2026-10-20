# FREQM (Frequency Meter, IP U2257): research notes for the twin

Gathered 2026-10-20 for the PIC32CM1216MC00048 (PIC32CM MC00 Curiosity Pro, EV15N46A). The same IP
is on the PIC32CM5164JH01100 (U2257 1.0.1) and the ATSAME51J20A (U2257 1.1.0), so one model can
serve all three SoCs. Nothing here was run on hardware. "(guess)" marks inferences that no source
states.

Sources, in order of authority (silicon outranks all; none has been taken yet for FREQM):

- **DS**: PIC32CM MC00 data sheet **DS60001638E** (PDF in the scratchpad
  `research/ac/dl/ds60001638.pdf`, text extracted with pypdf): FREQM is chapter 41 (PDF pages
  866-878), the clock limits 43.10, the oscillators 43.13-43.15, the NVM calibration area 8.4.
  The onlinedocs edition reached through the Microchip MCP has the same text; its page titles end
  in "5-Volt Motor Control MCU" (GUID-0D44A543-..., en-US-5). MCP queries titled "Low-Power,
  32-bit Cortex-M0+ MCU with Advanced Analog and PWM" return the **SAM D21/DA1** data sheet
  instead; nothing below comes from it.
- **Errata**: DS80000905F (rev F, 03/2023). It lists NVMCTRL, SERCOM I2C, OSCCTRL (1.3.1 FDPLL
  spurious unlocks, 1.3.2 FDPLL ONDEMAND), AC, PDEC, SERCOM USART and OSC48M (1.7.1 start-up)
  issues, and **nothing for FREQM**.
- **ATDF**: `PIC32CM-MC_DFP/1.4.67/PIC32CM1216MC00048/atdf/PIC32CM1216MC00048.atdf` lines 298-307
  (instance), 2439-2471 (module); `include/component/freqm.h`.
- **devices/pic32cm1216mc00048.json** (generated from the ATDF): instance, IRQ, GCLK channels.
- **Harmony**: `csp/peripheral/freqm_u2257` (config/freqm.py, templates/plib_freqm.c.ftl, master)
  and `csp_apps_pic32cm_mc00/apps/freqm/freqm_measure_clock_freq` (main.c, plib_clock.c,
  plib_freqm.c, master). Copies in the scratchpad `research/freqm_tsens/dl/`.
- Same-IP data sheets, secondary, for details the MC00 DS omits (the synchronization error, the
  ripple counter, DIVREF on the newer IP): SAM D5x/E5x (DS60001507, chapter 30), PIC32CM SG00
  (DS60001839C, chapter 24/26), SAM L10/L11 (26.5.1).
- Board: PIC32CM MC00 Curiosity Pro user guide DS70005440A (1.1, 4.3.3).
- Twin: `C:\pp\pp-qemu` hw/misc/mchp_clock.c, hw/misc/mchp_regblock.c, hw/timer/mchp_tc.c,
  hw/arm/pic32cm_soc.c, hw/arm/pic32cm_mc00_cpro.c.

## 1. At a glance

| | PIC32CM MC00 | PIC32CM JH01 | SAM E51 |
|---|---|---|---|
| IP version | 1.0.1 | 1.0.1 | 1.1.0 |
| Base | 0x40002C00 | 0x40002C00 | 0x40002C00 |
| IRQ line | 4 (own vector) | 4 | 28 |
| GCLK_FREQM_MSR (PCHCTRL index) | 3 | 3 | 5 |
| GCLK_FREQM_REF (PCHCTRL index) | 4 | 4 | 6 |
| PAC id | 11, not protected at reset | 11 | 11 |
| APB clock at reset | on (MCLK.APBAMASK 0x0FFF has FREQM_ = bit 11) | | |
| Events, DMA, SleepWalking | none | | |

Sources: DS 41.4 Peripheral Dependencies (IRQ 4, APB clock enabled at reset "Y", generic clocks
"3: Measure, 4: Reference", PAC 11 "N", no events/DMA/SleepWalking); ATDF 298-307 (GCLK_ID_MSR 3,
GCLK_ID_REF 4, INSTANCE_ID 11) and 2706-2719 (APBAMASK initval 0x00000FFF); device JSONs of all
three parts.

Clock limits (DS 43.10, MC00): FREQM Measure max 96 MHz (FCLK_13, fGCLK_FREQM_MSR), FREQM
Reference max 48 MHz (FCLK_15, fGCLK_FREQM_REF); GCLK generator outputs max 96 MHz for
generators 0-2 (FCLK_6a, fGCLKGEN[0:2]) and 66 MHz for 3-8 (FCLK_6b). DS 43.27 points FM_7
(Reference) at FCLK_17 and FM_9 (Measure) at FCLK_15, parameter numbers that in the MC00 table
belong to EVSYS and FREQM Reference: stale cross-references, use 43.10. DS 41.5.2.1: "The
reference clock must be slower than the measurement clock." DS 12 Table 12-9: PCHCTRL 3 =
GCLK_FREQM_MSR, 4 = GCLK_FREQM_REF.

Sleep (DS 41.5.4): FREQM keeps running in Idle sleep while its clocks run; DONE is a synchronous
wake-up source (41.5.3). Disable it before sleeping for the lowest power.

## 2. Register map (MC00, also JH01/E51)

| Off | Register | Size | Reset | Access | Sync | Notes |
|---|---|---|---|---|---|---|
| 0x00 | CTRLA: SWRST(0), ENABLE(1) | 8 | 0x00 | RW, PAC | write-synchronized (both) | SWRST resets every register and disables; a SWRST write discards the other bits of the same write (41.6.1) |
| 0x01 | CTRLB: START(0) | 8 | 0x00 | W | - | write 1 starts a measurement (41.6.2). "Reading the START bit will result in a PAC error when PAC protection is enabled for the FREQM" (41.6.2 note) |
| 0x02 | CFGA: REFNUM(7:0) | 16 | 0x0000 | RW, PAC | - | enable-protected (41.6.3); 0x01-0xFF, "must be a non-zero value" (41.6.3, 41.5.2.1). Bits 15:8 unimplemented (3.5) |
| 0x08 | INTENCLR: DONE(0) | 8 | 0x00 | RW, PAC | - | |
| 0x09 | INTENSET: DONE(0) | 8 | 0x00 | RW, PAC | - | |
| 0x0A | INTFLAG: DONE(0) | 8 | 0x00 | RW, W1C | - | "set when the STATUS.BUSY bit has a one-to-zero transition" (41.6.6) |
| 0x0B | STATUS: BUSY(0) R, OVF(1) R/W | 8 | 0x00 | BUSY hardware-owned; OVF sticky, write 1 to clear (41.6.7) | - | |
| 0x0C | SYNCBUSY: SWRST(0), ENABLE(1) | 32 | 0 | R | - | 41.6.8 |
| 0x10 | VALUE: VALUE(23:0) | 32 | 0 | R | - | "Result from measurement" (41.6.9) |

Sources: DS60001638E 41.6 Register Summary and 41.6.1-41.6.9, 41.5.2.1-41.5.2.3, 41.5.5 (only
CTRLA.SWRST and CTRLA.ENABLE are write-synchronized); ATDF 2441-2470; freqm.h.

Silicon reset state (golden/pic32cm1216mc00048-a1/reset.json, A1 part, 2026-10-13): every readable
FREQM register reads 0, matching the table.

What the generated table (devices/pic32cm1216mc00048.json, `ips.freqm_u2257`) gets wrong, all
because no overlay exists yet:

- STATUS is plain RW (wmask 0x3, no W1C, no read-only bits). DS: BUSY is hardware-owned, OVF is
  write-1-to-clear. The ATDF says rw="RW" for the whole register.
- CFGA has enprot 0; DS says enable-protected.
- CTRLB has no hook, so a START write is stored and logged, nothing more.

Overlay to add (`overlays/freqm_u2257.yaml`, versions 1.0.1 and 1.1.0), in the existing vocabulary:
CTRLA `{pac_protected, sync: {SWRST: SWRST, ENABLE: ENABLE}, self_clearing: [SWRST], hooks:
[post_write]}`; CTRLB `{strobe/hooks: [post_write]}`; CFGA `{pac_protected, enable_protected:
all}`; INTENCLR/INTENSET alias pair; INTFLAG `{w1c: all, irq_source: {enable: INTENSET}}`; STATUS
`{ro: [BUSY], w1c: [OVF]}`; SYNCBUSY `{dynamic: true}`; VALUE read-only.

CTRLB read and PAC: on the JH01 it is an erratum (DS80001000H 2.6.1, cited in
docs/research/boot-path.md finding 28: reading FREQM CTRLB raises a PAC error). On the MC00 it is
documented behaviour instead, limited to a PAC-protected FREQM (DS60001638E 41.6.2 note; the
revision history lists "FREQM: Updated the CTRLB Register with a new note"). The MC00 errata
DS80000905F has no FREQM item. The silicon plan checks both cases (E9). The twin's harness already
treats FREQM.CTRLB as write-only (docs/silicon-harness.md 6.4).

## 3. The measurement

### 3.1 Principle and formula

FREQM counts periods of GCLK_FREQM_MSR during REFNUM periods of GCLK_FREQM_REF ("The
measurement is done for a period of REFNUM/fCLK_REF") and stores the count in VALUE (DS 41.5.1,
41.5.2.3). Figure 41-1 shows a TIMER on CLK_REF (compared with REFNUM) gating a COUNTER on
CLK_MSR; the E5x DS 30.5.3 calls the latter a ripple counter. The MC00 DS gives (41.5.1 and again
41.5.2.3)

    f_CLK_MSR = VALUE / REFNUM * f_CLK_REF

with no error term. The same IP's text in the SAM L10/L11 and PIC32CM SG00 data sheets adds
"Error represents the error introduced by the synchronization mechanism. The error can be maximum
two measured clock cycles."

Harmony's `FREQM_FrequencyGet()` uses two formulas by IP version (config/freqm.py lines 180-193,
plib_freqm.c.ftl lines 163-185):

- **v1** (U2257 1.0.1 or 1.1.0: our three parts): `f = (VALUE * f_REF) >> shift`, where `shift`
  is log2(REFNUM). The MCC menu only offers REFNUM = 1, 2, 4, ... 128, so the shift is exact.
- other versions: `f = (VALUE + 1) * f_REF / (REFNUM + 1)` (later FREQM revisions, such as the
  PIC32CM SG00 one with DIVREF/MSRSEL, free-running and event modes).

The MC00 example (`freqm_measure_clock_freq`) measures GCLK0 = OSC48M (48 MHz) against GCLK1 =
XOSC32K (crystal mode, `XTALEN=1`), REFNUM = 128, interrupt mode, and prints
`VALUE * 32768 >> 7`. Expected VALUE 187500, measurement 3.906 ms.

### 3.2 Sequence and flags

1. Configure and enable both GCLK channels (PCHCTRL[3], PCHCTRL[4]) (41.5.2.1).
2. Write CFGA.REFNUM (non-zero) while disabled; enable; wait SYNCBUSY (Harmony waits until
   SYNCBUSY == 0).
3. Write CTRLB.START. STATUS.BUSY sets at the start and clears at the end; INTFLAG.DONE sets on
   BUSY's one-to-zero transition (41.6.6) and the IRQ fires if INTENSET.DONE. 41.5.2.3 words it
   as "When INTENSET.DONE is 1 and a measurement is finished, INTFLAG.DONE will be set", but the
   register description makes DONE independent of the enable; the silicon plan confirms (E10).
4. Read VALUE. Check STATUS.OVF: when the count does not fit 24 bits, OVF sets and VALUE is invalid;
   reduce REFNUM or use a faster reference, clear OVF by writing 1, start again (41.5.2.3).

Harmony's `FREQM_MeasurementStart()` starts only if BUSY is clear and clears DONE first;
`FREQM_ErrorGet()` returns STATUS.OVF and writes 1 to clear it.

### 3.3 Duration and range

Duration = REFNUM / f_REF (plus synchronization, a few reference cycles; guess). VALUE is 24 bits:
the largest count is 16,777,215, so REFNUM * f_MSR / f_REF must stay below 2^24. Examples:

| MSR | REF | REFNUM | VALUE | window |
|---|---|---|---|---|
| OSC48M 48 MHz | XOSC32K 32,768 Hz | 128 | 187,500 | 3.9 ms |
| OSC48M 48 MHz | XOSC32K | 255 | 373,535 | 7.8 ms |
| FDPLL96M 96 MHz | XOSC32K | 255 | 747,070 | 7.8 ms |
| 32,768 Hz source | XOSC 12 MHz / 12000 = 1 kHz | 255 | 8,355 | 255 ms |
| OSC48M | XOSC32K / 64 = 512 Hz | 255 | 23,906,250: overflows | 0.5 s |

### 3.4 When a clock is missing (the DS is silent; all guesses)

- REF stopped: the reference timer never reaches REFNUM, so BUSY stays set and DONE never comes.
- MSR stopped: the measurement ends after REFNUM reference periods with VALUE = 0 (or a few counts
  from the synchronizer).
- Disabling (ENABLE = 0) during a measurement aborts it and clears BUSY; since DONE is defined as
  BUSY's one-to-zero transition (41.6.6), DONE may set as well, with a partial VALUE.
- START while BUSY: ignored (the later FREQM IP documents exactly that; Harmony avoids it).
- REFNUM = 0: undefined ("must be a non-zero value, i.e. 0x01 (one cycle) to 0xFF (255
  cycles)", 41.6.3).

### 3.5 No DIVREF on these parts

The MC00 DS Configuration Summary table (DS60001638E, front matter) lists "Frequency Meter (FREQM)
reference clock divider: Yes", but the register summary (41.6) and the CFGA description (41.6.3)
show bits 15:8 empty, and the MC00, JH01 and SAME51 ATDFs all define CFGA with REFNUM only.
Harmony shows its "Divide reference clock by 8" option only
when the ATDF has a DIVREF field (freqm.py lines 125-135), so no MC00/JH01/E51 project sets it.
DIVREF (bit 15, reference / 8) exists on the newer FREQM of the PIC32CM SG00/GC00 (DS60001839C
24.8.3). The twin should treat CFGA bit 15 as unimplemented; the silicon plan checks it (E8).

## 4. What the twin has and what the model needs

### 4.1 Today

FREQM is a plain mchp-regblock: no entry in `pic32cm_models[]` (pic32cm_soc.c 49-87), and its
`gclk_core` is -1 because the instance parameters are GCLK_ID_MSR/GCLK_ID_REF, not GCLK_ID, so
`pic32cm_connect_clocks()` (pic32cm_soc.c 443-453) connects no clock. A START write is stored and
logged; BUSY, DONE and VALUE never change. The JH01 Harmony example hangs on it
(acceptance/pic32cm_jh01_curiosity_pro.yaml, `freqm__freqm_measure_clock_freq`: "FREQM not
modelled; hangs"); the MC00 example would hang the same way.

### 4.2 How the clock tree looks to a FREQM model

Each mchp-regblock has one clock input, "gclk", with a `gclk_update` class hook
(mchp_regblock.c 465-486). The GCLK model (mchp_clock.c 512-728) exposes `gen[n]` and `pch[m]`
outputs; `pch[m]` carries `gen[GEN]` while PCHCTRLm.CHEN is set, else 0 Hz. A FREQM model needs a
second input. Proposal: keep "gclk" for GCLK_FREQM_MSR and add "gclk-ref" with its own update
callback, then wire both in `pic32cm_connect_clocks()` with the existing helper, as for EVSYS:

    pic32cm_gclk_channel(s, "FREQM", "GCLK_ID_MSR") -> freqm "gclk"
    pic32cm_gclk_channel(s, "FREQM", "GCLK_ID_REF") -> freqm "gclk-ref"

That covers all three SoCs (MC00/JH01 channels 3/4, E51 5/6) from the generated tables.

Frequencies the twin produces (all exact, nominal, ready instantly):

| Source | Twin value | Condition in the twin | Silicon (DS, MC00) |
|---|---|---|---|
| OSC48M | 48 MHz / (OSC48MDIV.DIV + 1), integer division | OSC48MCTRL.ENABLE (reset DIV = 11: 4 MHz) | 46.6-49.5 MHz over -40..85 C (43.14) |
| OSC32K | 32,768 Hz | OSC32K.ENABLE and EN32K; CALIB ignored | +-1.5 % at 25 C, 5 V; +-10 % over 2.7-5.5 V at 25 C (43.13) |
| OSCULP32K | 32,768 Hz always | EN32K/EN1K ignored for the GCLK source | -4.4/+2.4 % at 25 C, 5 V; -25/+21 % overall (43.15) |
| XOSC32K | board clock, 32,768 Hz | ENABLE and EN32K; XTALEN, ONDEMAND ignored | the board's 32.768 kHz oscillator |
| XOSC | board clock, 12 MHz | XOSCCTRL.ENABLE; XTALEN, ONDEMAND ignored | the board's 12 MHz oscillator |
| FDPLL96M | f_ckr * (LDR + 1 + LDRFRAC/16) >> PRESC | locked as soon as enabled with a reference | PLL jitter |
| GCLK gen n | src / DIV (DIV 0, 1: undivided), or src >> (DIV + 1) with DIVSEL | GENEN; only gen 1 has 16 DIV bits | same math |
| GCLK_IO input ("gclk-in") | 0 Hz (not driven) | | external |

Sources: mchp_clock.c 123-185 (XOSC, OSC48M, FDPLL reference XOSC32K / XOSC/(2(DIV+1)) / GCLK),
337-421 (32 kHz outputs), 562-618 (generators, channels); pic32cm_mc00_cpro.c 7-9, 46-47, 261-263
(12 MHz on XIN, 32,768 Hz on XIN32). The board user guide lists "32.768 kHz oscillator mounted" and
"12 MHz oscillator mounted" (DS70005440A 1.1) but 4.3.3 calls the 12 MHz part a footprint
(Table 4-15 "External 12 MHz Oscillator Footprint", PA14 XIN / PA15 XOUT shared with EXT2). The
silicon plan settles whether XOSC runs.

So the twin's FREQM will report nominal ratios. On silicon, ratios between clocks from one
oscillator (generator dividers, FDPLL against its own reference) are exact up to the 2-count
synchronization error, MEMS-against-MEMS ratios are within ppm, and RC oscillators deviate by
their tolerance. That split is what makes FREQM a good validator of the twin's clock tree (5.3).

### 4.3 Behaviour to model (spec draft)

- START (CTRLB write with bit 0, ENABLE set, BUSY clear): BUSY = 1; arm a QEMU_CLOCK_VIRTUAL timer
  for REFNUM reference periods (`clock_ticks_to_ns(ref, REFNUM)` plus the silicon-measured
  latency). With f_REF = 0 do not arm: BUSY stays set (guess, E5).
- Completion: count = floor(REFNUM * f_MSR / f_REF) (+ the bias silicon shows in R1/R2); compute
  from the Hz values (muldiv64) so that 48 MHz / 32,768 Hz gives exactly 187,500 for REFNUM 128.
  If count > 0xFFFFFF: STATUS.OVF = 1 (sticky) and VALUE = what silicon shows (wrapped, saturated
  or frozen; E7). Then BUSY = 0 and, on that falling edge, INTFLAG.DONE = 1 whatever INTENSET
  says (41.6.6; confirm with E10); update the IRQ.
- CTRLB reads return 0; with FREQM PAC-protected they also raise the PAC error (41.6.2), if the
  twin's PAC model can express read errors.
- A clock change during a measurement (either input's update callback): count the elapsed part at
  the old rates, like `mchp_tc_gclk_update()` (mchp_tc.c 686-692), or simply restart the window
  (simpler; the error matters only to firmware that reconfigures clocks mid-measurement).
- SWRST during a measurement: cancel the timer; every register returns to 0 (41.6.1).
  ENABLE = 0 during a measurement: cancel the timer, BUSY = 0; DONE and VALUE as E4 shows (until
  then: no DONE, VALUE unchanged; guess).
- SYNCBUSY instant (as every twin peripheral). DBGRUN does not exist on FREQM.
- Deterministic: no random +-2 count error; if silicon shows a constant bias, model the bias.

## 5. Silicon test plan (bare EV15N46A; needs the owner's go-ahead per CLAUDE.md)

### 5.1 Firmware

A probe in the existing style: `firmware/hello/tests/freqmprobe.c`, built with
`make APP=freqmprobe BOARD=pic32cm_mc00_cpro`, using `sys_initialize()` (GCLK0 = OSC48M/1 = 48 MHz
CPU, SERCOM0 VCOM), `console_*`, `put_field()`, `delay_ms()`, ending with `test_exit()` (PP:EXIT).
It prints raw observations and checks nothing; the same ELF runs on the twin for comparison.

Conventions for every row:

- REF on **GCLK1** (the only generator with a 16-bit divider), MSR on **GCLK2** (generators 0-2
  may run up to 96 MHz). PCHCTRL[4] = GEN1 | CHEN, PCHCTRL[3] = GEN2 | CHEN; wait for CHEN.
- CFGA.REFNUM = 255 unless stated (best resolution); clear INTFLAG and STATUS.OVF; START; poll
  BUSY with a SysTick timeout (default 2 s); record VALUE, STATUS, INTFLAG and the SysTick ticks
  from START to BUSY = 0.
- Print one line per row: `fm <tag> v=<VALUE> st=<STATUS> if=<INTFLAG> t=<ticks>` plus the implied
  frequency `f=<VALUE * f_REF / REFNUM>` computed with the nominal f_REF.
- Rows that change OSC48MDIV slow the CPU and break the UART baud: run them last, store the
  results in RAM, restore DIV = 0, then print.
- Reference clocks: XOSC32K as an external clock (OSC32KCTRL.XOSC32K = ENABLE | EN32K, XTALEN = 0,
  wait STATUS.XOSC32KRDY), and XOSC as an external clock (XOSCCTRL = ENABLE with ONDEMAND cleared,
  XTALEN = 0, wait STATUS.XOSCRDY). Clear ONDEMAND on every oscillator used (XOSCCTRL, XOSC32K,
  OSC32K and DPLLCTRLA all reset with ONDEMAND = 1) so that RDY flags reflect a running clock.
  Keeping OSC48M always on (ONDEMAND = 0) is also the workaround of errata DS80000905F 1.7.1
  (rare OSC48M restart failures); hello's replica of Harmony sets ONDEMAND = 1.
- FDPLL96M rows: set DPLLCTRLB.LBYPASS = 1 (errata 1.3.1: below 25 C spurious unlocks halt the
  DPLL output; LBYPASS keeps the clock running), wait DPLLSTATUS.CLKRDY, then select it in GCLK.
  Record DPLLSTATUS.LOCK before and after each measurement.

### 5.2 Rows

Twin values below are floor(REFNUM * f_MSR / f_REF) with the twin's nominal clocks.

**Clock sources (validate the twin's oscillator table):**

| Tag | MSR (GCLK2) | REF (GCLK1) | REFNUM | Twin VALUE | Silicon expectation |
|---|---|---|---|---|---|
| S1 | OSC48M, DIV 0 | XOSC32K | 255 | 373,535 | 362,640-385,208 (DS range) |
| S2.d | OSC48M, OSC48MDIV.DIV = d, d = 1..15 | XOSC32K | 255 | 186,767 / 124,511 / 93,383 / 74,707 / 62,255 / 53,362 / 46,691 / 41,503 / 37,353 / 33,957 / 31,127 / 28,733 / 26,681 / 24,902 / 23,345 | S1 / (d+1), within 2 counts |
| S3 | XOSC (12 MHz) | XOSC32K | 255 | 93,383 | MEMS vs MEMS: within ~+-200 ppm (guess at part tolerance) |
| S4 | OSC32K, CALIB = reset 0x3F | XOSC / 12000 (1 kHz) | 255 | 8,355 | uncalibrated: wide |
| S5 | OSC32K, CALIB = NVM value (OTP5 bits 18:12; this part 0x4F) | XOSC / 12000 | 255 | 8,355 | +-1.5 % at 5 V; the board runs at 3.3 V |
| S6 | OSCULP32K | XOSC / 12000 | 255 | 8,355 | -4.4/+2.4 % at 5 V |
| S6b | OSCULP32K with OSCULP32K.EN32K cleared | XOSC / 12000 | 255 | 8,355 (twin ignores EN32K) | does EN32K gate the GCLK source? |
| S7 | XOSC32K | XOSC / 12000 | 255 | 8,355 | ppm (cross-check of S3) |
| S7b | XOSC32K with XTALEN = 1 (crystal mode, as Harmony configures it) | XOSC / 12000 | 255 | 8,355 | does the oscillator output drive the crystal amplifier? |
| S8 | FDPLL96M, REFCLK XOSC32K, LDR 2928, LDRFRAC 11, PRESC 0 (96.000 MHz) | XOSC32K | 255 | 747,070 | exact ratio 2929.6875 * 255 = 747,070.3 within 2 counts + jitter |
| S9 | FDPLL96M as S8 with PRESC 1, 2 | XOSC32K | 255 | 373,535 / 186,767 | exact |
| S10 | FDPLL96M, REFCLK XOSC, DPLLCTRLB.DIV 5 (1 MHz), LDR 95 | XOSC32K | 255 | 747,070 | S3 ratio * 8 |
| S11 | FDPLL96M, REFCLK GCLK (PCHCTRL[0] = GEN3 = OSC48M / 48), LDR 95 | XOSC32K | 255 | 747,070 | S1 * 2 |

**Generator math (exact on silicon; validate GCLK DIV/DIVSEL handling):**

| Tag | MSR | REF | Twin VALUE |
|---|---|---|---|
| G1 | GEN2 = OSC48M, DIV 3 (odd divider) | XOSC32K | 124,511 |
| G2.n | GEN2 = OSC48M, DIVSEL 1, DIV n = 0..4 (/2 .. /32) | XOSC32K | 186,767 / 93,383 / 46,691 / 23,345 / 11,672 |
| G3 | GEN2 = OSC48M, DIV 0 and DIV 1 (both undivided) | XOSC32K | 373,535 |
| G4 | GEN2 = OSC48M | GEN1 = OSC48M, DIV 300 (16-bit divider in use) | 76,500 |

**Ratio and bias (one oscillator on both sides: pins the twin's exact formula):**

| Tag | MSR | REF | REFNUM | Twin VALUE |
|---|---|---|---|---|
| R1 | GEN2 = OSC48M / 1024 | GEN1 = OSC48M / 1024 | 255, 128, 1 | 255, 128, 1 |
| R1b | PCHCTRL[3] and PCHCTRL[4] both on GEN1 | | 255 | 255 |
| R2 | GEN2 = OSC48M / 512 | GEN1 = OSC48M / 1024 | 255, 1 | 510, 2 |
| R3 | GEN2 = OSC48M / 8 | GEN1 = OSC48M / 1024 | 255 | 32,640 |
| R4 | GEN2 = OSC48M / 1024 | GEN1 = OSC48M / 512 (REF faster than MSR, outside the DS rule) | 255 | 127 |

**Behaviour (flags must match the twin exactly):**

- E1 REFNUM = 0: BUSY, DONE, VALUE after 2 s.
- E2 START again while BUSY (REF = 1 kHz, REFNUM 255: 255 ms window): total time and VALUE
  (restart vs ignore); VALUE read three times during the window (previous result, live counter or
  0).
- E3 START with ENABLE = 0: BUSY, DONE.
- E4 ENABLE = 0 halfway through a 255 ms window: BUSY, DONE, VALUE.
- E5 REF channel off (PCHCTRL[4].CHEN = 0) before START: BUSY after 2 s; then CHEN = 1 again: does
  the pending measurement finish?
- E6 MSR channel off (PCHCTRL[3].CHEN = 0): VALUE, DONE, time.
- E7 Overflow: MSR OSC48M, REF XOSC32K / 64 (512 Hz), REFNUM 255 (nominal 23,906,250): VALUE
  (wrapped would be about 7,129,034, saturated 0xFFFFFF), STATUS.OVF, DONE. Then REFNUM 170
  (15,937,500, fits) without clearing OVF: does OVF stay (sticky) and does START clear it? Then
  write STATUS = 0x02: OVF clears? Also write STATUS = 0x01: BUSY unchanged?
- E8 CFGA = 0x80FF written while disabled: read back (bit 15 kept means DIVREF-like hardware: then
  re-run S1 and compare the window and VALUE); CFGA written while enabled: ignored?
- E9 8-bit read of CTRLB, first with FREQM not PAC-protected, then after PAC WRCTRL protects it
  (PAC id 11): value read, PAC.INTFLAGA bit 11 (FREQM_) before and after, and whether the read
  faults. DS 41.6.2 predicts a PAC error only in the protected case; the JH01 erratum
  (DS80001000H 2.6.1) does not mention the condition. Unprotect again afterwards.
- E10 INTENSET.DONE = 0: does INTFLAG.DONE still set (41.6.6 says yes, 41.5.2.3 suggests not)?
  With INTENSET.DONE = 1: NVIC line 4 pending.
- E11 SYNCBUSY: loop count from the ENABLE write until SYNCBUSY = 0, at REF = 32 kHz and 1 kHz
  (which clock it synchronizes to).
- E12 SWRST halfway through a window: every register 0 afterwards? BUSY?
- E13 Timing: ticks from START to BUSY = 0 for REFNUM 1, 16, 255 at REF 32,768 Hz; subtract
  REFNUM / f_REF to get the start/stop latency in reference cycles (input for the twin's timer).

### 5.3 Comparing with the twin

Run the same ELF with `python -m tools.pp run ... --machine pic32cm-mc00-cpro`. Classify each row:
**exact** (R*, G*, S2 relative to S1, S8-S9 relative to the reference): twin and silicon within
2 counts, after the bias found in R1 is applied to the twin; **MEMS** (S3, S7, S10): within a few
hundred ppm; **RC** (S1, S4-S6, S11): within the DS tolerance, and the measured silicon value is
recorded for this part (it can become a board property later); **behaviour** (E*): identical
flags and the same VALUE semantics. A twin mismatch in an exact row is a clock-tree bug in
mchp_clock.c, not a FREQM bug.

## 6. Open questions only silicon can answer

1. Constant bias of VALUE (R1, R2): floor, n - 1, n + 1, and the spread over repeated runs.
2. Overflow value (wrap, saturate, freeze), stickiness of OVF across START (E7).
3. Missing REF or MSR clock, disable or SWRST mid-measurement, START while busy (E2-E6, E12).
4. Does DONE set with INTENSET.DONE = 0 (E10; the DS contradicts itself, 41.6.6 vs 41.5.2.3)?
5. Start/stop latency in reference cycles (E13) and which clock SYNCBUSY waits for (E11).
6. CFGA bit 15 (DIVREF) really absent on the MC00 (E8), despite the DS Configuration Summary.
7. Does a CTRLB read raise a PAC error only when FREQM is PAC-protected (41.6.2), or always as the
   JH01 erratum reads (E9)?
8. Is the 12 MHz XOSC fitted (S3); does XOSC32K work with XTALEN = 1 on the board's oscillator
   (S7b); does OSCULP32K.EN32K gate the GCLK source (S6b)?
9. This part's RC frequencies (S1, S5, S6) at 3.3 V and room temperature.
10. REFNUM = 0 (E1); REF faster than MSR (R4).

## 7. Side findings

- MC00 OTP5 (0x00806020, DS 8.4): OSC32K CAL is bits 18:12 and CAL48M bits 40:19; on this part
  (profile words 0x1904FFBE / 0xC2064108) OSC32K CAL = 0x4F, CAL48M = 0x210320. Bits 63:41 are
  "Reserved" in the MC00 DS but hold 0x610320, which looks like a second CAL48M copy (0x210320 in
  bits 62:41, as the JH DS 9.4 "CAL48M 3V3" field) with bit 63 set. Harmony and hello load only
  CAL48M; nobody loads OSC32K CAL, and the silicon reset dump shows OSC32K.CALIB = 0x3F (not
  0x4F), so OSC32K runs uncalibrated in Harmony projects (S4 vs S5 quantifies it).
- golden/pic32cm1216mc00048-a1/reset.json: OSCCTRL.OSC48MDIV = 0x0B at reset (OSC48M at 4 MHz),
  OSCCTRL.CAL48M = 0x210320.

## 8. Answers from silicon (2026-10-20)

The desk EV15N46A ran `firmware/hello/tests/freqmprobe.c` (three rounds); the model
(`hw/misc/mchp_freqm.c`) follows these answers and `firmware/hello/tests/freqm.c` checks them on
both (`golden/pic32cm1216mc00048-a1/freqm-silicon.txt`). By question in section 6:

1. No bias: VALUE = REFNUM * f_MSR / f_REF exactly for clocks from one oscillator (48 MHz against
   OSC48M / 1024, REFNUM 255: 261,120; every GCLK divider and DIVSEL row exact, twice). MSR and REF
   on the same generator read REFNUM - 1 (254); the FDPLL96M against OSC48M showed +-1 between runs.
2. Overflow keeps the low 24 bits (33,423,360 reads 0xFE0000) and sets OVF; disabling clears OVF
   (read right after the disable).
3. A missing reference clock: BUSY stays; when it returns, a whole window follows with the exact
   count. A missing measured clock: VALUE 0, DONE at the usual time. Disabling mid-window: BUSY 0,
   VALUE 0, no DONE; re-enabling starts nothing. SWRST mid-window clears everything. START while
   busy is ignored (the count runs on); START while disabled is ignored.
4. DONE sets whatever INTENSET says; the NVIC line only with INTENSET.DONE.
5. A measurement lasts REFNUM + 2 to 3 reference periods (the phase of START against the
   reference); VALUE reads the live count during it. SYNCBUSY lasts about 3 reference periods after
   ENABLE, 2 after SWRST (not modelled).
6. CFGA bits 15:8 do not exist (0xFFFF reads 0x00FF); CFGA ignores writes while enabled.
7. Not probed (a PAC-protected CTRLB read may fault).
8. The 12 MHz XOSC is fitted and runs (11.93 MHz against OSC48M, so OSC48M runs about 0.55 % fast
   on this part), although the user guide calls it a footprint. **XOSC32K gives no clock on the desk
   board**, in external-clock mode or in crystal mode with the longest start-up: a GCLK generator
   fed from it never finishes synchronizing, though XOSC32KRDY may read 1 (the register survives
   every reset but power-on). The user guide lists a mounted 32.768 kHz oscillator with zero-ohm
   jumpers; the owner found no jumper. The twin keeps the oscillator; the board option
   `xosc32k=off` (patch 0062) matches the desk board.
9. Against OSC48M / 1024: OSCULP32K 31,985-32,169 Hz, OSC32K 35,294 Hz with CALIB 0x3F (reset) and
   31,433 Hz with the OTP value 0x4F, the FDPLL96M (1 MHz GCLK reference, LDR 95) 96.000 MHz. The
   twin runs all 32 kHz sources at 32,768 Hz and ignores CALIB.
10. REFNUM = 0 counts as 512 (a 515 ms window at 1 kHz, 24,576,000 wrapped to 0x770000 with OVF).
    A reference faster than the measured clock still measures correctly (REF at 93,750 Hz).

Side finding: generators 2-8 have 8-bit DIV fields, so DIV = 1024 or 512 written there reads as
0 (undivided); the twin already masks them. The twin's GCLK used to divide integer Hz (48 MHz /
2^17 became 366 Hz, so FREQM counted 0xFE4B3E instead of 0xFE0000); generators now multiply the
source's period.
