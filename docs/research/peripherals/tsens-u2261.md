# TSENS (Temperature Sensor, IP U2261): research notes for the twin

Gathered 2026-10-20 for the PIC32CM1216MC00048 (PIC32CM MC00 Curiosity Pro, EV15N46A). Among the
project's targets only the MC00 has a TSENS: the PIC32CM5164JH01100 ATDF has no TSENS instance
(the PIC32CM JH data sheet has a TSENS chapter, 43; which JH parts carry it was not checked) and
the SAM E51 measures temperature
through its ADC (PTAT/CTAT, modelled in hw/adc/mchp_adc.c). Nothing here was run on hardware.
"(guess)" marks inferences that no source states.

Sources, in order of authority (silicon outranks all; none has been taken yet for TSENS):

- **DS**: PIC32CM MC00 data sheet **DS60001638E** (PDF in the scratchpad
  `research/ac/dl/ds60001638.pdf`, text extracted with pypdf): TSENS is chapter 40 (PDF pages
  845-865); NVM temperature calibration area 8.5; electrical 43.20. The onlinedocs edition reached
  through the Microchip MCP ("... - 5-Volt Motor Control MCU" pages, GUID-0D44A543-..., en-US-5)
  has the same text but loses the formula and the VALUE example. MCP queries titled "Low-Power,
  32-bit Cortex-M0+ MCU with Advanced Analog and PWM" return the SAM D21/DA1 data sheet; nothing
  below comes from it.
- **Errata**: DS80000905F (rev F, 03/2023): **no TSENS item** (its summary lists NVMCTRL,
  SERCOM I2C, OSCCTRL, AC, PDEC, SERCOM USART and OSC48M issues).
- **ATDF**: `PIC32CM-MC_DFP/1.4.67/PIC32CM1216MC00048/atdf/PIC32CM1216MC00048.atdf` lines 950-958
  (instance), 5594-5670 (module), 2978-2988 (TEMP_LOG_FUSES), 1061 and 1121 (events), 1952 (DMA
  trigger); `include/component/tsens.h`, `include/component/fuses.h` lines 31-51.
- **devices/pic32cm1216mc00048.json**, profiles/pic32cm1216mc00048.yaml (silicon NVM words).
- **Harmony**: `csp/peripheral/tsens_u2261` (config/tsens.py, templates/plib_tsens.c.ftl,
  plib_tsens_common.h.ftl, master) and `csp_apps_pic32cm_mc00/apps/tsens` (main.c, plib_clock.c,
  plib_tsens.c, plib_evsys.c, plib_tc0.c, master). Copies in the scratchpad
  `research/freqm_tsens/dl/`.
- Twin: `C:\pp\pp-qemu` hw/arm/pic32cm_soc.c, hw/misc/mchp_regblock.c, hw/adc/mchp_adc.c,
  hw/adc/mchp_adc_u2247.c, hw/adc/avr8x_adc.c, hw/timer/mchp_tc.c,
  include/hw/arm/pic32cm1216mc00048_gen.h.

## 1. At a glance

| Item | Value | Source |
|---|---|---|
| Base | 0x40003000 (HPB0 / APB-A) | DS 40.4, ATDF 952 |
| IRQ | line 5, own vector | DS 40.4, ATDF 1007 |
| APB clock at reset | **off**: MCLK.APBAMASK resets to 0x00000FFF, TSENS_ is bit 12 | DS 40.4 ("N"), ATDF 2706-2719 |
| Generic clock | GCLK_TSENS = PCHCTRL index 5; factory calibration assumes undivided OSC48M (48 MHz) | DS 40.4, 40.5.1 note, Table 12-9 |
| PAC | id 12, not protected at reset | DS 40.4 |
| Event user | TSENS_START = EVSYS USER[0] | ATDF 1121; DS 40.5.5 |
| Event generator | TSENS_WINMON = EVSYS generator 30 | ATDF 1061; DS 40.5.5 |
| DMA trigger | TSENS RESRDY = DMAC trigger 1 | ATDF 1952 (DMAC_ID_RESRDY 1); DS 40.5.3 |
| SleepWalking | not listed | DS 40.4 |
| Accuracy | -11.3/+6.2 C over 0-60 C; -14.6/+10.5 C over -40-85 C; "not suggested for automotive applications" | DS 43.20 TSENS_1 |

The DS 40.4 table shows "-" for the event user, the event generator and DMA, while 40.5.3 and
40.5.5 describe all three and the ATDF numbers them: a table omission; the ATDF and Harmony's
example (which starts conversions through EVSYS USER[0]) are right.

Firmware must set MCLK.APBAMASK bit 12 before touching TSENS (Harmony's example writes
APBAMASK = 0x1FFF). The twin does not model MCLK clock masks, so it will not catch firmware that
forgets.

## 2. Register map

| Off | Register | Size | Reset | Access | Properties (DS 40.6.x) |
|---|---|---|---|---|---|
| 0x00 | CTRLA: SWRST(0), ENABLE(1), RUNSTDBY(6) | 8 | 0x00 | RW | PAC; SWRST and ENABLE write-synchronized; RUNSTDBY enable-protected, not synchronized; SWRST resets everything **except GAIN, OFFSET, CAL and DBGCTRL** and wins over the other bits of the same write |
| 0x01 | CTRLB: START(0) | 8 | 0x00 | W | PAC write protection blocks the write "but will not trigger a PAC interrupt" |
| 0x02 | CTRLC: WINMODE(2:0), FREERUN(4) | 8 | 0x00 | RW | PAC, enable-protected |
| 0x03 | EVCTRL: STARTEI(0), STARTINV(1), WINEO(2) | 8 | 0x00 | RW | PAC, enable-protected |
| 0x04 | INTENCLR: RESRDY(0), OVERRUN(1), WINMON(2), OVF(3) | 8 | 0x00 | RW | PAC |
| 0x05 | INTENSET: same bits | 8 | 0x00 | RW | PAC |
| 0x06 | INTFLAG: same bits | 8 | 0x00 | RW, W1C | RESRDY and WINMON also cleared by reading VALUE |
| 0x07 | STATUS: OVF(0) | 8 | 0x00 | R | writes have no effect |
| 0x08 | SYNCBUSY: SWRST(0), ENABLE(1) | 32 | 0 | R | |
| 0x0C | VALUE(23:0) | 32 | 0 | R | two's complement |
| 0x10 | WINLT(23:0) | 32 | 0 | RW | PAC, enable-protected, two's complement |
| 0x14 | WINUT(23:0) | 32 | 0 | RW | PAC, enable-protected, two's complement |
| 0x18 | GAIN(23:0) | 32 | 0 | RW | PAC, enable-protected, not reset by SWRST; "the number of GCLK_TSENS periods that will be used for a measurement cycle" |
| 0x1C | OFFSET: OFFSETC(23:0) | 32 | 0 | RW | PAC, enable-protected, not reset by SWRST, two's complement |
| 0x20 | CAL: FCAL(5:0), TCAL(13:8) | 32 | 0 | RW | PAC, enable-protected, not reset by SWRST; "must be copied only, and must not be changed" |
| 0x24 | DBGCTRL: DBGRUN(0) | 8 | 0x00 | RW | reset by a system reset, not by SWRST; DBGRUN = 0 halts TSENS while the CPU is halted (an ongoing measurement completes) |

Sources: DS60001638E 40.6 Register Summary, 40.6.1-40.6.16, 40.5.2.1 (enable protection), 40.5.7
(synchronization); ATDF 5594-5670; tsens.h. Silicon reset state
(golden/pic32cm1216mc00048-a1/reset.json): every readable TSENS register reads 0. Whether that dump
read TSENS with APBAMASK bit 12 set is not recorded in the file (docs/silicon-harness.md 6.3
describes the gated and unmasked dumps).

DS synchronization rule (40.5.7): writing a write-synchronized bit while its SYNCBUSY bit is set
discards the write and raises a bus error. The twin synchronizes instantly, so it never happens
there.

What the generated table (`ips.tsens_u2261` in the device JSON) lacks, because no overlay exists:
enable protection on RUNSTDBY, CTRLC, EVCTRL, WINLT, WINUT, GAIN, OFFSET, CAL (all enprot 0); the
VALUE read side effects; SWRST retention of GAIN/OFFSET/CAL/DBGCTRL; PAC flags. Proposed
`overlays/tsens_u2261.yaml` (version 1.0.1): CTRLA `{pac_protected, sync: {SWRST: SWRST, ENABLE:
ENABLE}, self_clearing: [SWRST], enable_protected: [RUNSTDBY], hooks: [post_write]}`; CTRLB
`{pac_protected, hooks: [post_write]}`; CTRLC, EVCTRL, WINLT, WINUT, GAIN, OFFSET, CAL
`{pac_protected, enable_protected: all}`; INTENCLR/INTENSET alias pair; INTFLAG `{w1c: all,
irq_source: {enable: INTENSET}}`; STATUS read-only; SYNCBUSY `{dynamic: true}`; VALUE
`{hooks: [pre_read]}`. The overlay vocabulary has no "kept across SWRST" key; the model has to do
that (4.3).

## 3. Factory calibration (NVM Temperature Calibration Area, 0x00806030)

| Bits | Field | Goes to | Mask in fuses.h |
|---|---|---|---|
| 5:0 | TSENS TCAL | CAL.TCAL (13:8) | TEMP_LOG_WORD_0 0x0000003F |
| 11:6 | TSENS FCAL | CAL.FCAL (5:0) | TEMP_LOG_WORD_0 0x00000FC0 |
| 35:12 | TSENS GAIN | GAIN (23:0) | WORD_0 0xFFFFF000 (bits 19:0) + WORD_1 0x0000000F (bits 23:20) |
| 59:36 | TSENS OFFSET | OFFSET (23:0) | WORD_1 0x0FFFFFF0 |
| 63:60 | reserved | | |

Sources: DS 8.5 Table 8-4; ATDF memory segment TEMP_LOG at 0x00806030, 8 bytes, and register
group TEMP_LOG_FUSES (2978-2988); fuses.h 31-51. (DS 40.6.15 says CAL comes from the "NVM software
calibration row"; Table 8-4, the ATDF and Harmony agree it is the temperature calibration area.)

This board's part (A1, serial F79A74AF51524B5939202020FF190418), from the words read on silicon
2026-10-13 and stored as profile defaults (profiles/pic32cm1216mc00048.yaml lines 81-82;
docs/handoff.md, "Silicon check (2026-10-13)"): word 0 = 0x14421ADF, word 1 = 0xF0024420, so

| Field | Value |
|---|---|
| TCAL | 0x1F (31) |
| FCAL | 0x2B (43) |
| GAIN | 0x014421 (82,977) |
| OFFSET | 0x002442 (+9,282) |
| bits 63:60 | 0xF |

Harmony's `TSENS_Initialize()` (plib_tsens.c.ftl, and the generated MC00 example) reads the area
as one uint64 and writes:

    CAL    = TCAL(lo & 0x3F) | FCAL((lo & 0xFC0) >> 6)
    GAIN   = ((hi & 0xF) << 20) | ((lo & 0xFFFFF000) >> 12)
    OFFSET = hi >> 4                    (bits 63:60 included: 0x0F002442 on this part)

so the register's 24-bit width has to drop the reserved nibble; the twin's write mask (0xFFFFFF)
does. Silicon read-back of OFFSET after that write is in the plan (T2).

## 4. Conversion

### 4.1 Principle and transfer function (DS 40.5.1, 40.6.13)

TOSC, a temperature-dependent oscillator, is measured twice, first in its "min" and then in its
"max" configuration. GAIN sets the number of GCLK_TSENS periods of each measurement; a counter
counts up in the first phase and down in the second; the signed result is corrected by OFFSET.
Figure 40-1 shows TOSC (trimmed by FCAL), GCLK_TSENS, GAIN, a TIME AMPLIFIER, the COUNTER and
OFFSET. The DS formula (40.5.1; the PDF text extraction loses the layout, the grouping below is
the only dimensionally consistent reading):

    VALUE = OFFSET + GAIN * (f_TOSCMIN / f_GCLK - f_TOSCMAX / f_GCLK)

Notes in 40.5.1: GAIN and OFFSET are factory programmed "to give a specific temperature slope when
using the undivided internal 48 MHz oscillator (OSC48M) as the GCLK_TSENS source"; other clocks
need GAIN or the expected slope scaled; the calibration values must be copied into GAIN and OFFSET
to reach the specified accuracy.

Scale (DS 40.6.10): with GAIN and OFFSET from the NVM area "the TSENS resolution is set at 100",
i.e. VALUE is hundredths of a degree Celsius in 24-bit two's complement: 25 C reads 2500 =
0x0009C4, -25 C reads -2500 = 0xFFF63C. Firmware converts with

    int32_t v = (int32_t)(TSENS_REGS->TSENS_VALUE << 8) >> 8;   /* sign-extend 24 bits */
    temperature_c = v / 100.0;

Harmony's example (`apps/tsens/firmware/src/main.c`) divides the unsigned 32-bit VALUE by
`TEMP_RESOLUTION` (100) as a float without sign extension, so it misprints temperatures below 0 C.

Derived for this part (guess, from the formula): at 25 C, GAIN * (f_TOSCMIN - f_TOSCMAX) / 48 MHz
= 2500 - 9282 = -6782, so f_TOSCMAX exceeds f_TOSCMIN by about 3.9 MHz, and the slope of 100
counts per degree means the difference shrinks by about 58 kHz per degree.

### 4.2 Starting, results, flags

- Starts (40.5.2.3): CTRLB.START, the START event (40.5.5: rising edge by default; STARTINV
  inverts the input, so a falling edge starts), or CTRLC.FREERUN = 1 (a new measurement starts
  when the previous one completes, no trigger needed). START needs ENABLE.
- Each result overwrites VALUE and sets INTFLAG.RESRDY (40.5.2.3). RESRDY clears by writing 1 or
  by reading VALUE (40.6.7). RESRDY "will not set if an overflow occurs during the conversion".
- OVERRUN: "set if a valid VALUE is updated before the previous valid value has been read by the
  CPU" (40.6.7).
- OVF: INTFLAG.OVF sets "when the conversion result requires more than 24 bits and overflows the
  VALUE register" (40.6.7); 40.5.4 says "more than 16 bits" (DS conflict; the register is 24
  bits). STATUS.OVF (read-only) says whether the result in VALUE is valid; how STATUS.OVF clears
  is not stated (guess: on the next valid result).
- DMA (40.5.3): the RESRDY request sets with each result, independent of the window monitor, and
  clears when VALUE is read.
- 40.5.2.3 recommends averaging 10 measurements.

### 4.3 Window monitor (40.5.2.4, 40.6.3, 40.6.11-12)

WINLT, WINUT and VALUE are two's complement, so comparisons are signed (guess; the DS says only
that the values are two's complement).

| WINMODE | Name | Condition (DS 40.6.3) |
|---|---|---|
| 0 | DISABLE | - |
| 1 | ABOVE | VALUE > WINLT |
| 2 | BELOW | VALUE < WINUT |
| 3 | INSIDE | WINLT < VALUE < WINUT |
| 4 | OUTSIDE | DS: "WINUT < VALUE < WINLT"; ATDF: VALUE < WINLT or VALUE > WINUT (the DS line is a typo, guess) |
| 5 | HYST_ABOVE | VALUE > WINUT, with hysteresis to WINLT |
| 6 | HYST_BELOW | VALUE < WINLT, with hysteresis to WINUT (ATDF caption misspells WINLT as "WINLST") |
| 7 | reserved | |

INTFLAG.WINMON "is set on the next cycle after a match", clears by writing 1 or reading VALUE;
EVCTRL.WINEO emits the WINMON event (generator 30). Hysteresis (guess): HYST_ABOVE matches from the
first result above WINUT until a result falls below WINLT; HYST_BELOW mirrors it. Whether WINMON
sets on every matching result or only on entering the window, and how the event is shaped, is for
silicon.

### 4.4 Timing, sleep, debug

- Conversion time: not given anywhere in DS60001638E. From the formula, two phases of GAIN
  GCLK_TSENS periods each: 2 * 82,977 / 48 MHz = about 3.5 ms per conversion on this part, scaling
  with GAIN / f_GCLK (guess; if GAIN counts the whole cycle it is half that). Measure (T3).
- Sleep (40.5.6 Table 40-1): disabled when ENABLE = 0; RUNSTDBY = 0 runs in every sleep mode but
  Standby (on request unless FREERUN); RUNSTDBY = 1 also in Standby.
- Debug (40.6.16): DBGRUN = 0 halts TSENS while the CPU is halted (an ongoing measurement
  completes).

### 4.5 How Harmony uses it (MC00 example `apps/tsens`)

1. CLOCK_Initialize: CAL48M from OTP5, OSC48MDIV = 0, GCLK0 = OSC48M / 1; PCHCTRL[5] (TSENS),
   PCHCTRL[6] (EVSYS channel 0), PCHCTRL[19] (SERCOM0), PCHCTRL[25] (TC0/TC1) all on GCLK0;
   MCLK.APBAMASK = 0x1FFF, APBCMASK = 0x1003.
2. TSENS_Initialize: CTRLA = SWRST, wait SYNCBUSY.SWRST; CAL, GAIN, OFFSET from 0x00806030 as in 3;
   CTRLC = WINMODE(0); INTFLAG = 0x0F; INTENSET = RESRDY; EVCTRL = STARTEI; wait SYNCBUSY = 0.
3. EVSYS: CHANNEL[0] = EVGEN 52 (TC0_OVF) | PATH 2 (asynchronous) | EDGSEL 1; USER[0] (TSENS_START)
   = channel 0 + 1.
4. TC0: COUNT16, DIV1024, MPWM with CC0 = 23436, EVCTRL.OVFEO: an overflow every
   23437 * 1024 / 48 MHz = 0.5 s, so a conversion every 0.5 s.
5. main: TSENS_Enable() (CTRLA |= ENABLE, wait), TC0 start, callback. The ISR reads INTFLAG, writes
   1 to the enabled flags, calls back; the callback reads VALUE; main prints
   `TSENS Count = 0x%x    Temperature = %d.%02d celcius`.

The Harmony configurator offers trigger "Free Run" (default), "SW Trigger" or "HW Event Trigger",
window thresholds 0..2^24 (defaults WINUT 1024, WINLT 512) and the RESRDY/WINMON interrupts
(config/tsens.py). `TSENS_ConversionStatusGet()` (polled mode) checks RESRDY and clears it by
writing 1.

## 5. The twin

### 5.1 Today

TSENS is a plain mchp-regblock (no `pic32cm_models[]` entry). Its "gclk" input is wired to
pch[5] (`gclk_core` 5, pic32cm_soc.c 443-453), but nothing uses it: START is stored and logged,
RESRDY never sets, VALUE stays 0. The MC00 `apps/tsens` example would run but never print a
reading. Event lines are left open (`pic32cm_connect_events()` connects only lines the model
declares), and the DMAC trigger is unused.

### 5.2 The "temperature" property

Follow the two existing temperature inputs, mchp_adc.c (SAM E51 PTAT/CTAT) and avr8x_adc.c:
an int32 QOM property **"temperature" in millidegrees Celsius, default 25000**, settable at run
time (`qom-set /machine/soc/tsens temperature 31500`, as tools/pp_web's `set_prop()` does for
board parts). Firmware that loads the factory calibration and runs GCLK_TSENS at 48 MHz must read

    VALUE = round(temperature / 10)  as 24-bit two's complement
    (25000 -> 0x0009C4, -25000 -> 0xFFF63C, 31500 -> 0x000C4E)

For any other register or clock setting, apply the DS formula with a TOSC difference D(T) chosen
so that the factory calibration gives the scale above:

    D(T)  = (temperature/10 - OFFSET_nvm) * 48 MHz / GAIN_nvm      [= f_TOSCMIN - f_TOSCMAX]
    VALUE = OFFSET + GAIN * D(T) / f_GCLK                          (signed; OFFSET sign-extended)

with GAIN_nvm, OFFSET_nvm read from the NVM rows at 0x00806030 at conversion time (mchp_adc.c
already reads its TEMP_LOG this way). That reproduces the DS note that another GCLK_TSENS
frequency or GAIN rescales the result, and gives VALUE = OFFSET when GAIN = 0. CAL (FCAL/TCAL)
trims the analog TOSC and cannot be modelled from the DS: ignore it (or log a guest error when it
differs from the NVM values). If the NVM row is erased (GAIN_nvm = 0xFFFFFF, no profile data),
fall back to the nominal VALUE = temperature/10. Whether this matches silicon is what plan T14
tests.

### 5.3 Behaviour to model (spec draft)

- New type (e.g. "mchp-tsens") in `pic32cm_models[]` for module TSENS, IP U2261.
- START (CTRLB bit 0 with ENABLE), a start event edge (input "start-event-in"[0]; rising unless
  STARTINV; only with STARTEI) or FREERUN: begin a conversion on QEMU_CLOCK_VIRTUAL lasting the
  silicon-measured time (first guess 2 * GAIN / f_GCLK). With f_GCLK = 0 (PCHCTRL[5] off) the
  conversion does not complete (guess, T9). START during a conversion: whatever T6 shows.
- Completion: compute VALUE as in 5.2. Outside the 24-bit range: INTFLAG.OVF and STATUS.OVF,
  no RESRDY. Otherwise: OVERRUN if the previous valid VALUE was not read, store VALUE, RESRDY,
  STATUS.OVF = 0 (guess), window compare (signed, hysteresis state kept in the model) setting
  WINMON and pulsing output "winmon-event"[0] when WINEO. FREERUN schedules the next conversion.
- VALUE read (pre_read with side effects): clear RESRDY, WINMON and the "unread" state.
- DMA: DMAC trigger 1 paced like the SERCOM triggers in `pic32cm_dmac_triggers()`:
  `mchp_dmac_set_trigger(dmac, 1, base + 0x06, 0x01)` (INTFLAG.RESRDY), since the DMAC's VALUE
  read clears RESRDY just as the DS request clears.
- SWRST: keep GAIN, OFFSET, CAL and DBGCTRL (40.6.1). In mchp-regblock the write path calls the
  CTRLA post_write hook before `mchp_regblock_soft_reset()` (mchp_regblock.c 345-364), so the
  model can save them there and restore them in reset_hold; a system reset clears them.
- Event user readiness (output "start-event-busy"): unknown; T12 reads EVSYS CHSTATUS.USRRDY.
- Not modelled at first: RUNSTDBY/sleep, DBGRUN, the sync bus error, PAC.

## 6. Silicon test plan (bare EV15N46A; needs the owner's go-ahead per CLAUDE.md)

A probe `firmware/hello/tests/tsensprobe.c` (`make APP=tsensprobe BOARD=pic32cm_mc00_cpro`):
`sys_initialize()` gives GCLK0 = OSC48M / 1, then MCLK.APBAMASK |= bit 12, PCHCTRL[5] = GEN0 |
CHEN. Timing with SysTick at 48 MHz (or TC0 free-running at 48 MHz). Print raw hex plus
sign-extended decimal for every VALUE; end with `test_exit()`. Run the same ELF on the twin.

- **T0 calibration**: print 0x00806030/34 and the decoded TCAL, FCAL, GAIN, OFFSET, bits 63:60.
  Expected on this part: 31, 43, 0x014421, 0x002442, 0xF.
- **T1 reset state**: after setting APBAMASK bit 12, read every register (expect 0). The gated
  read (bit 12 clear) belongs to the harness's gated dump, not to firmware.
- **T2 Harmony init** (the sequence of 4.5 step 2), then read back CAL, GAIN, OFFSET (does OFFSET
  read 0x002442 or 0x0F002442?) and SYNCBUSY.
- **T3 single conversions**: 32 times START, poll RESRDY, record ticks and VALUE; print min, max,
  mean, VALUE / 100. Prediction: about 2500 at 25 C, about 3.5 ms each (guess).
- **T4 flag clearing**: after RESRDY read INTFLAG, read VALUE, read INTFLAG again (RESRDY and
  WINMON should clear on the VALUE read, 40.6.7).
- **T5 OVERRUN**: two conversions without reading VALUE in between: OVERRUN, VALUE updated?
- **T6 START during a conversion**: START, START again at once; total time, number of results.
- **T7 FREERUN**: disable, CTRLC.FREERUN = 1, enable; period between results (clearing RESRDY);
  then stop reading: OVERRUN.
- **T8 START with ENABLE = 0**: no conversion expected.
- **T9 no GCLK**: PCHCTRL[5].CHEN = 0, START, wait 1 s: RESRDY? Then CHEN = 1: does it finish?
- **T10 window**: V0 = mean of T3. For WINMODE 1-6 and the threshold pairs (WINLT, WINUT) =
  (V0-300, V0+300), (V0+300, V0+600), (V0-600, V0-300), one conversion each: WINMON. Signedness:
  WINMODE ABOVE with WINLT = 0xFFFF9C (-100): WINMON set means signed. Hysteresis: in HYST_ABOVE
  move the thresholds around V0 over several conversions and log WINMON per result. Repeat a
  matching conversion twice: does WINMON set on each matching result?
- **T11 WINMON event**: EVSYS channel n, EVGEN 30, PATH 2, a TC with EVACT COUNT as user; the TC
  count should step once per matching result.
- **T12 START event**: EVSYS channel 0 with USER[0] = 1, STARTEI = 1, SWEVT pulses: a conversion
  per pulse? With STARTINV = 1? Also the Harmony TC0 OVF path. CHSTATUS.USRRDY before ENABLE,
  while enabled, during a conversion.
- **T13 DMA**: a DMAC channel on trigger 1 moving VALUE to RAM, beat count after N conversions;
  RESRDY after each DMA read.
- **T14 transfer function** (8 conversions each, mean VALUE and time; disable before each change,
  the registers are enable-protected). Predictions from the DS formula with V0 measured at room
  temperature and OFFSET = 9282:
  - GAIN / 2: 9282 + (V0 - 9282) / 2, time halved;
  - GAIN * 2: 9282 + 2 (V0 - 9282), time doubled;
  - OFFSET = 0: V0 - 9282; OFFSET + 1000: V0 + 1000 (tells whether OFFSET adds or subtracts);
  - GCLK_TSENS = OSC48M / 2 (GEN3 DIV 2): 9282 + 2 (V0 - 9282), time doubled;
  - GCLK_TSENS = XOSC 12 MHz: 9282 + 4 (V0 - 9282);
  - CAL = 0, FCAL +-4, TCAL +-4 (record only; the twin ignores CAL).
- **T15 uncalibrated**: GAIN = OFFSET = CAL = 0: VALUE, OVF, time (prediction VALUE = 0).
- **T16 overflow**: GAIN = 0xFFFFFF (about 0.7 s per conversion at 48 MHz, guess; timeout 5 s):
  INTFLAG.OVF, STATUS.OVF, RESRDY, VALUE. Then a normal GAIN: does STATUS.OVF clear?
- **T17 SWRST retention**: write distinct GAIN, OFFSET, CAL, DBGCTRL, WINLT, CTRLC values; SWRST;
  read back (expect the first four kept, 40.6.1).
- **T18 enable protection**: with ENABLE = 1 write CTRLC, EVCTRL, WINLT, GAIN, OFFSET, CAL and
  CTRLA.RUNSTDBY: ignored? PAC flags?
- **T19 repeatability**: 100 conversions over 10 s at rest; spread in hundredths of a degree.
  Optional, manual: the owner warms the chip with a finger and records room temperature for a
  plausibility check (the DS accuracy is about +-10 C, so only gross errors show).

Twin comparison: T0-T2, T4-T8, T12, T15, T17, T18 must match exactly; T3 and T14 must match the
model's formula within the silicon spread once the twin's "temperature" is set to the silicon's
reading; T6, T9, T10, T11, T16 define behaviour the model then adopts.

## 7. Open questions only silicon can answer

1. Conversion time and its dependence on GAIN and f_GCLK (T3, T14).
2. Does the DS formula hold: OFFSET added (not subtracted), VALUE linear in GAIN and in 1/f_GCLK
   (T14)? What does CAL do (T14)?
3. Overflow threshold (24 bits per 40.6.7 or 16 bits per 40.5.4), VALUE after an overflow, when
   STATUS.OVF clears (T16).
4. Is the window comparison signed; hysteresis behaviour; does WINMON set on every matching result;
   event pulse shape (T10, T11).
5. START while converting, START with ENABLE = 0, conversions without GCLK_TSENS (T6, T8, T9).
6. SWRST really keeps GAIN, OFFSET, CAL, DBGCTRL (40.6.1 vs 40.5.2.2) (T17).
7. Width of OFFSET read-back after Harmony writes the reserved nibble (T2).
8. Event user readiness (USRRDY) and the falling-edge start with STARTINV (T12).
9. Room-temperature reading and noise of this part (T3, T19).
10. Does a read of the APB-gated TSENS return 0 or fault (the harness's gated dump)?

## 8. Discrepancies between sources

- DS 40.4 lists no events or DMA; the ATDF (user 0, generator 30, DMA trigger 1), DS 40.5.3/40.5.5
  and Harmony's example use them.
- DS 40.5.2.2 says SWRST resets all registers; 40.6.1, 40.6.13-40.6.15 exempt GAIN, OFFSET, CAL
  (and DBGCTRL).
- DS 40.5.4 says OVF means "more than 16 bits", 40.6.7 says "more than 24 bits"; VALUE is 24 bits.
- WINMODE OUTSIDE: DS "WINUT < VALUE < WINLT" vs ATDF "VALUE less than WINLT or VALUE greater than
  WINUT". HYST_BELOW: ATDF caption "WINLST" typo; Harmony names it
  `TSENS_WINMODE_HYST_LESS_THAN_WINLT`.
- DS 40.6.15 sources CAL from the "NVM software calibration row"; Table 8-4, the ATDF and Harmony
  use the temperature calibration area (0x00806030).
- TEMP_LOG bits 63:60 are reserved but read 0xF on this part; Harmony writes them into OFFSET
  bits 27:24.
- Harmony's example treats VALUE as unsigned (wrong below 0 C).
- The DS principle text says a counter clocked by GCLK_TSENS measures a pulse width, while the
  formula and the GAIN description make GAIN a count of GCLK_TSENS periods with TOSC cycles
  counted; the twin follows the formula.
