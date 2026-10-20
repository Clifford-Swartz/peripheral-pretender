# PDEC (Position Decoder, IP U2263 1.0.0) on the PIC32CM MC00: research notes

Gathered 2026-10-20 to plan a QEMU model of the PDEC and a silicon check on the bare EV15N46A. The
model would also serve the PIC32CM JH01 and the SAM E51. No hardware was touched. Sources, most
authoritative first:

1. **PIC32CM MC00 Family Data Sheet DS60001638E** (August 2023, the PDF from microchip.com, read
   with pdftotext): chapter 42 "Position Decoder (PDEC)" (pp. 879-917), 43.29 (PDEC AC
   characteristics), 4.3 (pinout), 25.6 (EIC), 27.6 (PORT), 28.5/28.6 (EVSYS), 35.6 (CCL). Below,
   "§42.x" and "Fig. 42-n" refer to this document.
2. **Errata DS80000905F** (rev F, 03/2023), §1.5.1-1.5.4 (PDEC), affecting silicon A1 and A2.
3. **SAM D5x/E5x Family Data Sheet DS60001507N**, chapter 53 "PDEC" (same IP; the PDF), cited as
   "E5x §53.x". **PIC32CM JH01 data sheet** chapter 42 (onlinedocs GUID-76938A18 rev 6, via the
   Microchip docs MCP), cited as "JH01 §42.x".
4. DFPs: `PIC32CM-MC_DFP/1.4.67/.../PIC32CM1216MC00048.atdf` (PDEC module lines 3491-3661,
   instance 483-500, events 1113-1169), `include/component/pdec.h`, plus the JH01 (1.5.236) and
   SAME51 (3.9.267) ATDFs for comparison.
5. `devices/{pic32cm1216mc00048,pic32cm5164jh01100,atsame51j20a}.json`, pp-qemu's generated
   `include/hw/mchp/gen/*/pdec_u2263.h`, `tools/pp_web/boards/ev15n46a/board.json`.
6. Harmony CSP `peripheral/pdec_u2263` (master): `templates/plib_pdec_{qdec,hall,counter}.c.ftl`
   and `config/pdec.py`. These show how real firmware drives the block. They are not a spec.

Search warning: in the Microchip docs MCP, the query title "Low-Power, 32-bit Cortex-M0+ MCU with
Advanced Analog and PWM" returns the **SAM D21** data sheet. The MC00 data sheet is indexed as
"5-Volt Motor Control MCU" (GUID-0D44A543, rev 5). Its register field tables come back with only
the header, so these notes use the PDF. Related slip: `hw/timer/mchp_tcc.c` and
`overlays/tcc_u2213.yaml` cite "DS60001638E chapter 31" for the TCC. In DS60001638E the TCC is
chapter **34**; chapter 31 is SERCOM SPI.

Tags used below: **(inference)** = follows from the text but is not stated. **(guess)** = my
expectation, no source. **SILICON?** = an open point that only the board can settle (collected
in §6).

Twin today: no model. `pic32cm_models[]` in `hw/arm/pic32cm_soc.c` has no PDEC entry, so the PDEC
is a plain `mchp-regblock`: SYNCBUSY never sets, the commands do nothing, and COUNT never moves.

## 1. Instance facts

| | PIC32CM1216MC00048 (MC00) | PIC32CM5164JH01100 (JH01) | ATSAME51J20A (SAM E51) |
|---|---|---|---|
| Base | 0x42006800 | 0x42006800 | 0x42001C00 |
| NVIC | 26 (single line) | 29 (single line) | **115 PDEC_OTHER (OVF, ERR, DIR, VLC), 116 PDEC_MC0, 117 PDEC_MC1** |
| GCLK_PDEC (PCHCTRL index) | 34 | 41 | 31 |
| APB clock | MCLK.APBCMASK bit 26, **off at reset** (§42.5) | APBC | APBC bit 7 (E5x) |
| PAC | INSTANCE_ID 90 (bridge C index 26), not protected at reset | 90 | 71 |
| EVSYS generators | 83 OVF, 84 ERR, 85 DIR, 86 VLC, 87 MC0, 88 MC1 (Table 28-2: 0x53-0x58) | 96-101 | 97-102 |
| EVSYS users | 44 EVU0, 45 EVU1, 46 EVU2, **asynchronous path only** (Table 28-3) | 48-50 | 52-54 |
| Pads (ATDF group QDI, DS calls them PDEC[2:0]) | QDI0/1/2 = PA08/PA09/PA10, function **J**. Or PA16/PA17/PA18, function **G** | same as MC00 | PA24 QDI0, PA25 QDI1, PB22 QDI2, PB23 QDI0, function G |
| GCLK_PDEC max | FCLK_29. Table 43-13 is garbled in the PDF: 48 or 96 MHz (SILICON?/DS) | FCLK_41 | |
| Sleepwalking | yes (§42.5) | | |

The three ATDF modules are identical except for two points. EVCTRL.EVACT value 2 = COUNT ("Count
on event") exists in the JH01 and SAME51 ATDFs and is absent from the MC00 ATDF. §42.7.4 marks it
"Reserved". Also, the SAME51 ATDF drops `access="WSYNC"` on CTRLA. The merged generated header
`include/hw/mchp/gen/pdec_u2263.h` already defines `MCHP_PDEC_EVCTRL_EVACT_COUNT`.

SAM E51 caveats: the E5x pins come in IOSETs. PDEC pins must all come from one IOSET (E5x 6.2.8.6,
Table 6-33: IOSET3 = PA24/PA25/PB22, IOSET4 = PB23/PB24/PB25). On the 64-pin J20A only PA24, PA25,
PB22 and PB23 exist. On the Curiosity CircuitPython board, PA24/PA25 are USB D-/D+ (the REPL), so
only event inputs are usable there.

## 2. Register map and access

Offsets, resets and bits come from §42.7 and agree with the ATDF. "Sync" names the SYNCBUSY bit
(§42.6.7, §42.7.10). "EP" means enable-protected (§42.6.2.1). "PAC" means PAC write protection
(from each register's property line).

| Off | Register | Size | Reset | Fields | Sync | EP | PAC |
|---|---|---|---|---|---|---|---|
| 0x00 | CTRLA | 32 | 0x00000000 | SWRST 0 (W), ENABLE 1, MODE[3:2] (0 QDEC, 1 HALL, 2 COUNTER), RUNSTDBY 6, CONF[10:8] (0 X4, 1 X4S, 2 X2, 3 X2S, 4 AUTOC), ALOCK 11, SWAP 14, PEREN 15, PINEN[18:16], PINVEN[22:20], ANGULAR[26:24], MAXCMP[31:28] | SWRST, ENABLE only. Every other field is "not synchronized" | all except SWRST, ENABLE | yes |
| 0x04 | CTRLBCLR | 8 | 0x00 | LUPD 1, CMD[7:5] | CTRLB (R- and W-sync) | no | yes |
| 0x05 | CTRLBSET | 8 | 0x00 | LUPD 1, CMD[7:5]: 0 NONE, 1 RETRIGGER, 2 UPDATE, 3 READSYNC, 4 START, 5 STOP | CTRLB | no | yes |
| 0x06 | EVCTRL | 16 | 0x0000 | EVACT[1:0] (0 OFF, 1 RETRIGGER, 2 reserved on MC00 / COUNT on JH01, E5x), EVINV[4:2], EVEI[7:5], OVFEO 8, ERREO 9, DIREO 10, VLCEO 11, MCEO0 12, MCEO1 13 | none | **whole register** | yes |
| 0x08 | INTENCLR | 8 | 0x00 | OVF 0, ERR 1, DIR 2, VLC 3, MC0 4, MC1 5 | | | yes |
| 0x09 | INTENSET | 8 | 0x00 | same | | | yes |
| 0x0A | INTFLAG | 8 | 0x00 | same, W1C | | | no |
| 0x0C | STATUS | 16 | **0x0040** | QERR 0, IDXERR 1, MPERR 2, WINERR 4, HERR 5 (RW, write 1 to clear), STOP 6 (R, reset 1), DIR 7 (R), PRESCBUFV 8, FILTERBUFV 9, CCBUFV0 12, CCBUFV1 13 | STATUS (R- and W-sync) | | no |
| 0x0F | DBGCTRL | 8 | 0x00 | DBGRUN 0. Not reset by SWRST | | | yes |
| 0x10 | SYNCBUSY | 32 | 0 | SWRST 0, ENABLE 1, CTRLB 2, STATUS 3, PRESC 4, FILTER 5, COUNT 6, CC0 7, CC1 8 | | | RO |
| 0x14 | PRESC | 8 | 0x00 | PRESC[3:0]: 0-10 = DIV1..DIV1024 (powers of 2) | PRESC | | no |
| 0x15 | FILTER | 8 | 0x00 | FILTER[7:0] | FILTER | | no |
| 0x18 | PRESCBUF | 8 | 0x00 | PRESCBUF[3:0] | PRESC (§42.7.13) | | no |
| 0x19 | FILTERBUF | 8 | 0x00 | FILTERBUF[7:0] | FILTER | | no |
| 0x1C | COUNT | 32 | 0 | COUNT[15:0] | COUNT (W). Read needs READSYNC | | yes |
| 0x20, 0x24 | CC0, CC1 | 32 | 0 | CC[15:0] | CCx (R- and W-sync) | | no |
| 0x30, 0x34 | CCBUF0, CCBUF1 | 32 | 0 | CCBUF[15:0] | CCx | | no |

Generated-table masks (pp-qemu): CTRLA impl 0xF777CF4F, EVCTRL 0x3FFF, STATUS 0x33F7, SYNCBUSY
0x1FF, COUNT/CC/CCBUF 0xFFFF. The generator marks every STATUS bit writable, but STOP and DIR are
read-only. An overlay must fix that (§7).

Enable protection (§42.6.2.1): enable-protected CTRLA bits may be written together with
ENABLE = 1, but not together with ENABLE = 0. SWRST resets everything except DBGCTRL and
disables the PDEC. An SWRST write discards every other bit of the same write (§42.7.1). The PDEC
"should be disabled before reset to avoid undefined behavior" (§42.6.2.2). PAC exemptions in
E5x §53.5.8: INTFLAG, FILTER, PRESC, CCx, CCBUFx, STATUS. These match the MC00 property lines,
which also show no PAC on PRESCBUF/FILTERBUF.

Copy-paste errors in DS60001638E chapter 42, to ignore:
- STATUS.FILTERBUFV is "set when a new value is written to the PRESCALERBUF register", and
  PRESCBUFV "...to the PRESC register" (§42.7.8). §42.6.3.3 says the buffer registers:
  FILTERBUF, PRESCBUF.
- The STATUS bit table marks the buffer-valid bits "R", while the text says "cleared by writing a
  '1'" (§42.7.8). Treat them as W1C (inference). The TCC has the same pattern.
- INTFLAG.DIR "clears the Velocity transition interrupt flag". INTFLAG.OVF is set "on the next
  CLK_TC_CNT cycle" (text copied from the TC chapter).
- CTRLB.LUPD values mention "CCx and PER registers" (copied from the TCC). The PDEC has no PER.
- Fig. 42-2 lists the prescaler as {1,2,4,8,64,256,1024}. The register is DIV1..DIV1024 in powers
  of 2 (§42.7.11, ATDF).
- CTRLA.MAXCMP "has no effect outside AUTOC" (§42.7.1). §42.6.2.6.5 implies the same.

### Synchronization, commands, READSYNC

- Write-synchronized: CTRLA.SWRST/ENABLE, CTRLBCLR/SET, STATUS, PRESC/PRESCBUF, CCx/CCBUFx,
  FILTER/FILTERBUF and COUNT (§42.6.7). SYNCBUSY.x is set while a write crosses into the
  GCLK_PDEC domain (§42.7.10).
- **COUNT is read-synchronized on demand**. Writing CTRLBSET.CMD = READSYNC copies the internal
  counter into COUNT, and a later read returns that copy. "To read the most updated counter value,
  the READSYNC software command must be applied first" (§42.7.15, §42.6.3.4.3). Harmony's
  `PDECx_QDECPositionGet()` writes READSYNC, waits for SYNCBUSY.CTRLB = 0, then for
  CTRLBSET.CMD = 0, then reads COUNT. The model must clear SYNCBUSY.CTRLB and read CMD back as 0
  once the command has run. A COUNT read without READSYNC presumably returns the last copy, as on
  the TCC (inference, TCC precedent in `mchp_tcc.c`). SILICON?
- Commands "are executed on the next prescaled GCLK_PDEC clock cycle. When a command has been
  executed, the CMD bit group will read back zero" (§42.7.2/3). A write to CTRLBCLR.CMD "will
  clear the corresponding pending command" (§42.7.2), so it cancels a command still pending
  (inference). The values 6 and 7 are undefined.
- RETRIGGER (§42.6.3.4.1): during counting, the counter is "reloaded or cleared, depending on the
  counting direction". When stopped, counting "will resume ... from the value in the COUNT
  register".
- UPDATE (§42.6.3.4.2): copies the buffered registers now. Whether it obeys LUPD is SILICON?
- START/STOP: "In QDEC or HALL operation modes, PDEC decoding is enabled writing a START command
  ... disabled writing a STOP command" (§42.6.2.2). STATUS.STOP resets to 1 (§42.7.8), so after
  ENABLE alone the decoder stays stopped until START (inference). STATUS.STOP reads 0 in COUNTER
  mode, so COUNTER probably runs on ENABLE alone (inference; Harmony still sends START in every
  mode). SILICON?
- Harmony start sequence: `CTRLA |= ENABLE; CTRLBSET = START;` back to back, with no wait for
  SYNCBUSY.ENABLE in between, then wait for SYNCBUSY = 0. Stop sequence: `CTRLBSET = STOP;
  CTRLA &= ~ENABLE;` then wait. The init code writes PRESC, FILTER, CC0 (and CC1) while the PDEC
  is **disabled**, then waits for `SYNCBUSY == 0`. The model must clear those SYNCBUSY bits
  without ENABLE (with GCLK_PDEC on).
- The COUNTER-mode text says "The COUNT value will always be zero when starting the PDEC, unless a
  different value has been written to it, or the PDEC has been disabled at a value other than
  zero" (§42.6.3.2). So disabling keeps COUNT.

## 3. Operation

### 3.1 Inputs, polarity, filter, prescaler (§42.6.2.3-4, Fig. 42-1/42-3)

- Three control signals, 0-2. Signal n comes from pad PDEC[n] when CTRLA.PINENn = 1 ("0: Event
  line is the signal input", §42.7.1), or from EVSYS user EVUn when EVCTRL.EVEIn = 1. PINVENn
  inverts the pad and EVINVn the event. With neither PINENn nor EVEIn set, the input is disabled.
  For signal 2 that means "Index disabled" (§42.6.2.6.1). The behaviour with both PINENn and EVEIn
  set is SILICON?
- CTRLA.SWAP swaps signals 0 and 1 (not 2). It has no effect in COUNTER mode.
- Each input passes a synchronizer, then the filter. The filter output is valid only after the
  input has been stable for (FILTER + 1) · t_GCLK_PDEC (§42.6.2.4, formula revised in rev E).
  Fig. 42-3 draws the filter on the "Prescaled Clock", and §42.6.2.3 says "the prescaler clock is
  also enabled when the input filtering is required". So t may be the prescaled period. SILICON?
- Prescaler: GCLK_PDEC / 2^PRESC. "If the prescaler value is higher than 0x01, the counter update
  condition is executed on the next prescaled clock pulse" (§42.6.2.3). JH01 §42.6.2.3 adds: "If
  the counter is set to count events, the internal prescaler is bypassed".
- AC limits (Table 43-41): input high and low time ≥ 2/f_GCLK_PDEC, period ≥ 4/f, filter
  recognition ≥ 4/f.

### 3.2 QDEC mode (MODE = 0; §42.6.2.6, Fig. 42-4/42-5)

- Signals: X4, X4S and AUTOC use S0 = phase A, S1 = phase B, S2 = index. In X2 and X2S, "Signal 0
  and Signal 1 ... refer to count/direction", and S2 is still the index (§42.6.2.6). Conflicts: the
  same chapter says "In X2 and X2S configuration, a valid index is detected when the two inputs
  (Count and Index) are at low level" (§42.6.2.6.1), and Harmony's `pdec.py` relabels S1 as
  "Index" in X2/X2S and hides S2. The X2 input roles and counts per cycle are SILICON?
- **COUNT split** (§42.7.1, Table 42-1): angular = COUNT[ANGULAR+8:0] (ANGULAR + 9 bits) and
  revolution = COUNT[15:ANGULAR+9]. ANGULAR = 0 gives 9 angular + 7 revolution bits. ANGULAR = 7
  gives a 16-bit angular count and no revolution counter. Harmony's angular-bits option runs 9-16
  and writes `ANGULAR(bits - 9)`. Its revolution read is `COUNT & REVOLUTION_MASK`, masked in place
  and not shifted.
- **TOP**: with PEREN = 0, each part's TOP is all ones. With PEREN = 1, the angular TOP is the
  angular-width LSB part of CC0 and the revolution TOP is its MSB part (§42.6.2.6.1). Harmony
  writes `CC0 = pulses_per_rev | (revs << angular_bits)`, where "Quadrature Pulses per Revolution"
  is the angular TOP. The general PEREN text (§42.6.2.5) says that counting up the counter clears
  after it reaches CC0, and counting down it reloads CC0 after ZERO.
- Angular counting: "the 'Signal 0' and 'Signal 1' edge detections ... increment or decrement the
  Angular counter". X4 decoding counts every edge of A and B, 4 per quadrature cycle (inference
  from the name X4, the "each QDEC transition" wording and Fig. 42-5). Which phase order counts
  up, and the STATUS.DIR polarity, are SILICON?
- Angular wrap: counting up at TOP, the next tick reloads ZERO and sets INTFLAG.OVF. Counting
  down at ZERO, the next tick reloads TOP and sets OVF (§42.6.2.6.1). "When counting-up and its
  TOP value is reached, ... INTFLAG.MC0 will be set. When counting-down and ZERO is reached,
  INTFLAG.MC0 will be set." Which counter (angular or the whole COUNT) these OVF/MC0 sentences
  mean is ambiguous. Fig. 42-5 labels the overflow trace "Angle OVF". SILICON?
- Index, valid only with "the three inputs (PhaseA, PhaseB and Index) ... at low level" in X4/X4S
  (§42.6.2.6.1). "A valid Index is qualified with the two other inputs at low level". Read
  literally, the index is active low. SILICON?
  - X2, X4 with the index enabled: a valid index reloads the angular counter (ZERO counting up,
    TOP counting down). The revolution counter then counts on each index and not on the angular
    wrap. Without an index, the revolution counter counts on each angular wrap.
  - X2S, X4S ("secure", §42.6.2.6.2, Fig. 42-6): the index must be enabled. The first valid index
    after enabling reloads ZERO, and "the first error generation is masked". After that, an index
    that does not coincide with an angular wrap, or a full angular revolution without an index,
    sets STATUS.IDXERR and INTFLAG.ERR. The revolution counter counts on each angular wrap.
- **Errors**: in X4, "a simultaneous transition on Phase A and Phase B will cause a QDEC error
  detection (STATUS.QERR)" plus INTFLAG.ERR (§42.6.2.6). Whether COUNT moves on such a
  transition is SILICON?
- **Direction** (§42.6.2.6.3, Fig. 42-7): STATUS.DIR reads anytime, 0 = clockwise and 1 =
  counter-clockwise (§42.7.8). "The polarity ... depends on the input signal swap and active level
  configuration". INTFLAG.DIR and the DIR event fire on a direction change, "only on the second
  edge confirming the direction change". The DIR event output is a level: "changes the level when
  the rotation direction changes" (§42.7.4 DIREO).
- **Velocity**: "Velocity output event is generated on each QDEC transition except when the
  direction changes" (§42.6.2.6.3). VLCEO: "an event level will be generated for each change on
  the qualified PDEC phases". INTFLAG.VLC: "set if a velocity transition occurs". Whether the VLC
  event is a pulse per transition (guess) or a toggling level is SILICON? It feeds TC/TCC speed
  measurement (§42.6.2.6.4, Fig. 42-8: a TC counts VLC events, a TCC captures).
- **AUTOC** (CONF = 4, §42.6.2.6.5, X4 only): a single missing pulse on one phase is detected and
  corrected in the angular count. There is no correction when both phases are hit at the same
  place. MAXCMP sets the limit of consecutive missing pulses, and at the limit STATUS.MPERR and
  INTFLAG.ERR set. MAXCMP = 0 never sets MPERR. The correction algorithm is not described.
  SILICON?
- Errata: §1.5.1, "When the INTFLAG.ERR is rising the associated STATUS.xERR is set after a
  delay". Workaround: poll STATUS. §1.5.2, STATUS.DIR is set after a delay relative to INTFLAG.DIR.
  "It is not recommended to use [them] in conjunction". A twin can set both at once, but a test
  must not compare STATUS right after an ERR/DIR interrupt.

### 3.3 HALL mode (MODE = 1; §42.6.3.1, Fig. 42-9 to 42-11)

- S0, S1 and S2 = Hall phases A, B, C. "When positive rotation is detected, STATUS.DIR = 1",
  negative 0.
- On each update of the filter output (§42.6.3.1.1, MC00 text):
  - an invalid code sets STATUS.HERR (and ERR). The codes 000 and 111 are invalid for 120° sensors
    (guess). Fig. 42-11 shows ERR at state 000;
  - "The OVF Interrupt Flag is set if CC0[2:0] matches the filter output value, stored in LSB
    part of the COUNTER" (the hall code is in COUNT[2:0]; Harmony `HALLPatternGet()` returns
    `COUNT & 7` after READSYNC, and `HALLPatternSet()` writes CCBUF0[2:0]);
  - the window counter is checked to lie between the "MSB part" of CC0 (min) and of CC1 (max),
    then reset. Outside the window, STATUS.WINERR sets;
  - the delay counter starts. MC0 fires when it matches CC0's MSB part, and MC1 when it matches
    CC1's MSB part.
- **Conflict**: JH01 §42.6.3.1.1 and E5x §53.6.3.1.1 say "**MC0** ... is set if CC0[2:0]
  matches", and "the delay counter ... MC0 ... when the delay counter matches CC0[LSB]". They do
  not mention MC1. Fig. 42-9 (same in all three) labels CC0[2:0] "Hall Code Trigger", CC1[2:0]
  "(Unused)", COUNT(MSB) "Window Counter" and COUNT(LSB) "Delay Counter". So a code match in
  COUNT LSB and a delay counter in COUNT LSB both appear. Where "MSB part" and "LSB part" split a
  16-bit register is not stated. Harmony's `HALLTimeWindowSet()` writes `CCBUF0 = low << 16` and
  `CCBUF1 = high << 16`, above the 16-bit field in the ATDF. Either CC is wider in HALL mode or
  Harmony is wrong. SILICON?
- Errata §1.5.3: a spurious WINERR can appear at START or on leaving standby. §1.5.4: several
  WINERR interrupts can occur at very low speed or standstill.
- The window and delay counters presumably run on the prescaled GCLK_PDEC (inference).

### 3.4 COUNTER mode (MODE = 2; §42.6.3.2, Fig. 42-2)

- A 16-bit up-counter "incremented at each counter clock input". Fig. 42-2 draws the clock as
  GCLK_PDEC/PRESC, or EVENT selected through EVACT. At TOP, the next clock wraps to 0 and sets
  INTFLAG.OVF (and the OVF event). TOP is 0xFFFF, or CC0 with PEREN = 1 (§42.6.2.5). Features
  list: "16-bit counter with two compare channels; one of the compare channels can be configured
  with period settings; overflow and compare match interrupt/event". MC0 = COUNT==CC0 and MC1 =
  COUNT==CC1 (inference). The MC0 behaviour when CC0 is the period is SILICON?
- Harmony COUNTER: the option "Select TOP as Period" (default on) leaves PEREN = 0. Off, it sets
  PEREN and CC0 = period.
- "In COUNTER mode, only PINEN[0] [and PINVEN[0]] is significant" (§42.7.1). What pad 0 does in
  COUNTER mode (gate, count source, retrigger) is not stated. SILICON?
- Event action (§42.6.5, §42.7.4: "These bits have an effect only when COUNTER operation mode is
  selected"). With RETRIGGER, an incoming event restarts the counter. Per §42.6.5 there is one
  event input "for each of the three PDEC channels". MC00 §42.6.3.4.1 note: "When re-trigger event
  action is enabled, enabling the counter will start the counter. The counter will be reset on the
  next incoming event and restart on any following event". **E5x §53.6.3.4.1 says the opposite**:
  "enabling the counter will not start the counter. The counter will start on the next incoming
  event". JH01 and E5x also have EVACT = 2 COUNT, "When an event is received, the counter
  increments the value" (E5x §53.6.3.4.2). On the MC00 the value is reserved. SILICON? (both)
- STATUS.DIR and STOP read 0. INTFLAG.VLC and DIR never set. FILTER has no effect, FILTERBUFV
  reads 0 and SYNCBUSY.FILTER reads 0.
- The UPDATE condition for buffers in COUNTER mode is "the overflow/underflow or re-trigger event
  detection" (§42.6.3.3).

### 3.5 Double buffering and lock (§42.6.3.3)

PRESC, FILTER, CC0 and CC1 have buffers PRESCBUF, FILTERBUF, CCBUF0 and CCBUF1. A buffer write
sets STATUS.xBUFV. On the UPDATE condition, valid buffers are copied and their flags cleared. In
QDEC and HALL, the UPDATE condition is "the next filter transition", that is, the next input edge
(inference). In COUNTER mode it is overflow or retrigger. CTRLBSET.LUPD blocks the copies and
CTRLBCLR.LUPD unblocks them. With CTRLA.ALOCK = 1, hardware sets LUPD when an UPDATE condition
happens. CMD UPDATE forces the copy. Two TCC behaviours are open for the PDEC (SILICON?): on
MC00 silicon, TCC CCx reads show CCBUFx while CCBUFVx is set, and SYNCBUSY.CCx stays set until
the copy (`mchp_tcc.c` header).

### 3.6 Interrupts, events, debug, sleep

- INTFLAG sources: OVF, ERR, DIR, VLC, MC0, MC1. VLC and DIR exist in QDEC and HALL only (§42.6.4,
  §42.7.7). The IRQ stays asserted while flag & enable is nonzero. On the SAM E51 the request is
  split: line 115 = OVF|ERR|DIR|VLC, 116 = MC0, 117 = MC1 (device JSON; Harmony has
  `PDECx_InterruptHandler`, `_MC0_InterruptHandler`, `_MC1_InterruptHandler`). The regblock's
  default multi-line mapping (flag bit k → line k, the rest on the last line) would be wrong. The
  model needs an `irq_lines` hook.
- Event outputs, gated by EVCTRL.xxEO: OVF, ERR, DIR (level), VLC, MC0, MC1. Event inputs: EVU0-2
  (asynchronous path only). EVSYS.USERm.CHANNEL = channel + 1 (§28.6.8).
- DBGCTRL.DBGRUN = 0 halts the PDEC when the CPU halts in debug. It is not reset by SWRST and
  "should not be changed while the PDEC is enabled" (§42.7.9).
- RUNSTDBY: with 1, the PDEC keeps running in standby (§42.6.6). It can wake the device from any
  sleep mode.

## 4. Inputs on the bare EV15N46A and ways to feed the PDEC without wiring

Pads (§4.3 pinout, ATDF): PDEC[0..2] = PA08/PA09/PA10 (PMUX J = 0x9) or PA16/PA17/PA18 (PMUX
G = 0x6). Board use (`tools/pp_web/boards/ev15n46a/board.json`):
- PA16 = Arduino D12 MISO and EXT1/EXT2 SPI MISO. PA17 = EXT1 SS_A (mikroBUS CS via the adapter).
  PA18 = Arduino D11 MOSI and EXT SPI MOSI. With nothing plugged in, all three are free. PA16 is
  already the `TEST_PIN` of the EIC tests. Function A on these pads = EIC EXTINT0/1/2.
- PA08/PA09 = Arduino A4/A5 and EXT ADC+/ADC- (free; PA08 function A is NMI, PA09 is EXTINT9).
  **PA10 is EDBG DGI GPIO0** (and EXT2 GPIO1/mikroBUS RST): avoid it. Its DGI side is unknown, and
  memory `dgi-usb-hazard` warns against DGI sessions.
- So use **PA16 = A, PA17 = B, PA18 = index/Hall C**.

Ways to drive the inputs with no wire, best first:

1. **Pull toggling on the PDEC pads (pin path, PINEN = 1). Recommended.** Set PA16-18 to
   PINCFG = PMUXEN | INEN | PULLEN, DIR = 0, PMUX = G. Writing OUT then moves the pull resistor up
   or down and the floating pad follows. This is the technique of `firmware/hello/tests/eic.c` on
   silicon (200 µs settle). The PORT model records "input functions (EIC) keep the pull
   (silicon)". DS PINCFG.INEN (§27.7.14): with PMUXEN = 1, IN still shows the pad when INEN = 1, so
   every step can print IN as a stimulus check. Open: whether the pull also holds when the pad is
   muxed to the PDEC rather than the EIC (very likely, SILICON? E0b checks it). One OUTSET or OUTTGL
   write moves several pads at once. The pull RC (guess τ ≈ 1 µs with ~40 kΩ and pad plus header
   capacitance) makes the edges slow ramps. Set FILTER to a few µs (for example FILTER = 63 at
   48 MHz ≈ 1.3 µs) so a ramp cannot give a double transition.
2. **Pull toggling on the EIC pads, through EVSYS (event path, EVEI = 1).** PA16-18 use function A
   (EXTINT0-2), CONFIG SENSE = HIGH with no filter (asynchronous level detection, §25.6.3) and
   EVCTRL.EXTINTEOn. EVSYS channels 0-2 use PATH = ASYNCHRONOUS with EVGEN = EIC_EXTINT_0/1/2 (14,
   15, 16) and USER[44/45/46].CHANNEL = ch + 1. The PDEC then sees the pad levels as events.
   SENSE = LOW, or EVINVn, gives the inverted level. The pp-qemu EIC model already passes the level
   ("high while a HIGH line is high", from silicon), and its EVSYS async path passes levels.
3. **One event into two inputs**, for an exact **simultaneous A/B edge** (QERR): route one channel
   (from option 2) to both USER[44] and USER[45]. EVINV1 can make B the inverse. Each pad toggle
   then changes A and B in the same instant. Pull toggling of two pads in one write gives edges
   within about a µs, which a 48 MHz GCLK_PDEC sees as two transitions. With option 1, a slow
   GCLK_PDEC (32 kHz) also makes them look simultaneous most of the time, but not
   deterministically.
4. **EVSYS software events** (SWEVT, §28.5.2.12) on an async channel: a pulse, not a level (the
   pp-qemu EVSYS notes from `evprobe.c`: "a software event is a generator pulse"). Good for COUNTER
   RETRIGGER and for short-pulse and filter limits. Useless as a quadrature phase, because a pulse
   is two edges. Its width relative to 2/f_GCLK_PDEC is SILICON?
5. **PORT event actions on a pulled pad**: PORT EVCTRL EVACT = TGL (§27.6.4), fed by TC/TCC
   MC0/MC1/OVF event pulses, toggles OUT and so the pull of a PDEC pad. Two TCC compare events half
   a period apart, toggling PA16 and PA17, give quadrature with no CPU involvement, limited by the
   pull RC (tens of kHz). This needs the PORT, TCC and EVSYS models, which exist.
6. **CCL with internal TC waveforms**: LUT0 with INSEL = TC (TC0 WO[0]) and LUT1 with INSEL = TC
   (TC1 WO[0]) (§35.6.2.x "Timer/Counter Inputs"). Run both TCs in NFRQ at the same period, with
   TC1's COUNT preset to half the period: the two toggles are a quarter cycle apart. LUTEO feeds
   generators CCL_LUTOUT_0/1 (77, 78) on async channels to EVU0/1. This is fast (MHz) and uses no
   pads. It suits velocity and filter-limit tests. The **twin has no CCL model** (CCL is a generic
   regblock), so these transcripts cannot be compared until a CCL model exists.
7. Not possible: a TCC/TC waveform pad cannot be the PDEC input at the same time (one PMUX
   function per pad), and TC/TCC events are pulses, not waveforms.

Most reliable: option 1 for the pin path and option 2 for the event path. Both run on today's
twin, apart from the PDEC itself. Option 3 covers QERR.

## 5. Silicon test plan (bare EV15N46A, VCOM transcript, no wiring)

Proposed `firmware/hello/tests/pdecprobe.c`, in the style of `tccprobe.c` and `evprobe.c`: raw
deterministic prints, then a checked `pdec.c` once the twin matches. Common setup: `sys_initialize()`
(GCLK0 = 48 MHz); MCLK.APBCMASK |= bit 26; GCLK.PCHCTRL[34] = GEN0 | CHEN (wait for CHEN). Pads
PA16/17/18 as in option 1, all pulled low. PDEC SWRST, then wait for SYNCBUSY = 0. Helpers:
- `step(a, b, i)`: write OUT, wait 200 µs.
- `snap()`: CTRLBSET = READSYNC, wait for SYNCBUSY.CTRLB = 0 and CMD = 0, then print COUNT,
  STATUS, INTFLAG and PORT IN[18:16]. Clear INTFLAG and the STATUS error bits (W1C), then wait
  for SYNCBUSY.STATUS.
- To route PDEC output events into a counter (`evprobe.c` pattern): TC1 COUNT16 with
  EVCTRL.EVACT = COUNT and TCEI, EVSYS channel n on the resynchronized path with EDGSEL = RISING
  (BOTH for the DIR level), EVGEN = 83-88, USER[24] = n + 1. Read TC1 COUNT through its READSYNC.

Predictions come from the data sheet. "?" marks the cases where the text does not decide.

| # | Experiment | DS prediction |
|---|---|---|
| E0a | Reset values, then all-ones writes and read-back per register (PDEC disabled; CTRLA without SWRST/ENABLE, then MODE/CONF fields one by one); then CC0 = 0xFFFFFFFF and read CC0 | CTRLA 0, STATUS 0x0040, rest 0. Read-back masks as in §2. CC0 reads 0x0000FFFF? (HALL `<< 16` question). PRESC 11-15 read back? |
| E0b | Stimulus check: toggle OUT of PA16-18 with PMUX G, print IN | IN follows OUT (pull kept under PMUX G)? |
| E1 | SYNCBUSY: write CC0, PRESC, FILTER and COUNT with GCLK_PDEC off, then on, PDEC disabled. SWRST with GCLK off | Bits set while the clock is off and clear once it runs, enabled or not (Harmony relies on this) |
| E2 | QDEC X4, PINEN = 0b011, ENABLE without START: 4 steps, snap; then START, 4 steps, snap | STOP = 1 and COUNT 0 before START (inference); after START COUNT = ±4 and STOP = 0 |
| E3 | Direction, counts per cycle: from 00, A-leading cycles 00→10→11→01→00 three times (snap after every step of the first cycle), then the reverse order three times | ±1 per step, 4 per cycle, 12 after three cycles, back to 0. Sign and STATUS.DIR? VLC on each step except the first after the reversal; DIR flag on the 2nd edge after the reversal |
| E4 | SWAP = 1, then PINVEN0 = 1 (each a re-enable), same A-leading sequence | Count sign flips in both cases. STATUS.DIR flips too |
| E5 | READSYNC: 5 steps, read COUNT without READSYNC, then with READSYNC. Write COUNT = 0x0100 while running, read back with and without READSYNC | Stale value (last READSYNC copy?) then 5. COUNT write lands (SYNCBUSY.COUNT) and later steps count from 0x100 |
| E6 | Angular/revolution with PEREN: ANGULAR = 0, CC0 = 3 \| (2 << 9) (angular TOP 3, revolution TOP 2), no index. 14 steps up, snap after each, then 14 down | Angular 0..3 wraps. Revolution +1 per wrap, 0..2 then wraps to 0. Which of OVF and MC0 fire at the angular wrap and which at the revolution wrap? Down: 0 → angular 3, revolution −1 |
| E7 | PEREN = 0, ANGULAR = 7: COUNT = 0xFFFE, 3 up, then 3 down | 0xFFFF, 0x0000 (OVF), 0x0001, then back through 0x0000 to 0xFFFF (OVF) |
| E8 | Index X4: PINEN = 0b111. Index pulses (PA18 high then low, and the inverse via PINVEN2) at A=B=0, then at A=1 B=0; counting up and down | Valid only at A=B=0, active level? Up: angular → 0, revolution +1. Down: angular → TOP, revolution −1. No effect at A=1 |
| E9 | X4S: CC0 angular TOP 7, PEREN. Index once per 8 steps at the wrap, then an extra index mid-revolution, then a revolution with no index | First index zeroes the count, no error. Extra index → IDXERR + ERR. Missing index → IDXERR after one revolution. Poll STATUS after ERR (errata 1.5.1) |
| E10 | QERR: option 3 (one event to EVU0 and EVU1), toggling. Also option 1 with a single OUTSET of A+B, at 48 MHz and with GCLK_PDEC = 32 kHz (GCLK gen 1 from OSCULP32K) | QERR + ERR, COUNT unchanged? Option 1 at 48 MHz probably counts +1/−1 with no QERR |
| E11 | X2 and X2S: the A-leading and reverse sequences. Then toggle S0 with S1 held at 0 and at 1. Index as in E8 | Counts per cycle (2 if X2 counts the edges of S0 only)? Which signal is direction? Index rule with 2 inputs? |
| E12 | AUTOC: drop one A transition (B continues), then 2, then 3 consecutive, with MAXCMP = 0, 1, 2 | Single miss corrected (count as if undamaged). MPERR + ERR at MAXCMP. MAXCMP = 0 never sets it |
| E13 | Filter vs prescaler: GCLK gen 1 = OSC48M / 48 = 1 MHz for PDEC, FILTER = 99. A pulses of 50, 90, 110 and 500 µs (A up then down, B steady), each with PRESC = DIV1 and DIV8 | Pulses shorter than 100 µs (DIV1) are ignored: no VLC flag, COUNT unchanged. Longer ones give 2 transitions. With DIV8 the window becomes 800 µs if the filter runs on the prescaled clock? |
| E14 | Buffers: write CCBUF0, read CC0, STATUS.CCBUFV0 and SYNCBUSY.CC0. Then 1 step. Then LUPD = 1, write CCBUF0, 1 step, then CMD UPDATE. Then ALOCK. Same for PRESCBUF and FILTERBUF | V = 1 until the step copies it (UPDATE condition = filter transition). LUPD blocks it. UPDATE forces it (despite LUPD?). ALOCK sets LUPD at the first copy. CC0 reads CCBUF while V (TCC-like)? |
| E15 | STOP/START/RETRIGGER in QDEC: 5 steps, STOP, 2 steps, START, 2 steps; RETRIGGER counting up and down; disable then re-enable | Steps ignored while stopped. COUNT kept or cleared on STOP (the TCC clears it on silicon)? RETRIGGER → 0 counting up, TOP counting down? COUNT kept across re-enable (§42.6.3.2) |
| E16 | Event path (option 2) for all three signals; EVINVn; PINENn and EVEIn both set | Same transcripts as E3/E8 through events. Inversion as PINVEN. Which input wins? |
| E17 | Event outputs counted by TC1: N steps with VLCEO, DIREO, OVFEO, MC0EO, MC1EO, ERREO in turn | VLC count = transitions minus direction changes. DIR = a level edge per confirmed reversal. OVF/MC0 as in E6. MC1 at COUNT = CC1? |
| E18 | COUNTER: PRESC DIV1024 (46.875 kHz). (a) PEREN = 0: COUNT deltas over 10 ms SysTick windows, with and without START. (b) PEREN = 1, CC0 = 4687, CC1 = 1000: OVF every 100 ms, MC1 at about 21 ms, MC0? (c) RETRIGGER and STOP commands. (d) EVACT RETRIGGER, EVEI0, SWEVT on an async channel to EVU0: COUNT → 0 per event, and state just after ENABLE. (e) EVACT = 2 on the MC00. (f) PINEN0 = 1 with pad 0 toggled | (a) about 468.75 per 10 ms; runs without START? (b) OVF period 100.0 ms. (c) → 0 and stop. (d) MC00 text: runs from enable, reset per event (E5x: waits for the first event). (e) reserved: counts events like the JH01/E5x? (f) undocumented |
| E19 | HALL: PINEN = 0b111; step the codes 001→011→010→110→100→101→001 and back, then 000, 111 and non-adjacent jumps. CC0[2:0] = a code. Window values in CC0/CC1 bits [15:8] and also [31:16] | COUNT[2:0] = code, DIR = 1 one way. HERR on 000/111 (non-adjacent?). Code match → OVF (MC00 text) or MC0 (JH01/E5x)? WINERR behaviour (errata 1.5.3: spurious at START) |
| E20 | SWRST with DBGCTRL.DBGRUN = 1 | DBGCTRL keeps 1, everything else resets |
| E21 (optional, needs a debugger halt; the owner allows halting) | COUNTER mode, halt for 1 s with DBGRUN 0 and 1 | Count frozen while halted with 0, runs with 1 |

Order the probe so that each experiment starts from SWRST, and print only deterministic values
(booleans for SYNCBUSY, not poll counts). Then the twin and silicon transcripts can be compared
byte for byte, as with the EIC and TCC probes.

## 6. Open questions only silicon can answer

1. In X4, which phase order counts up, and the STATUS.DIR value for it. How SWAP and PINVEN change
   both.
2. Do QDEC/HALL count before the first START (STATUS.STOP = 1 at reset)? Does COUNTER need START?
   Does STOP keep or clear COUNT? What does RETRIGGER do in QDEC?
3. Is a COUNT read without READSYNC stale (the last copy) or zero? When does SYNCBUSY.COUNT/CTRLB
   clear?
4. Which counter do OVF and MC0 refer to in QDEC: the angular wrap, the revolution wrap, or both?
   Is MC1 a full-COUNT match with CC1?
5. Index active level ("at low level") and the qualification with A = B = 0. Does an index in
   X2/X4 also set OVF?
6. X2/X2S: the roles of S0/S1/S2 (DS: count/direction/index; Harmony: count/index) and counts per
   cycle.
7. QERR: does COUNT move? How close must the edges be?
8. Is the filter time base GCLK_PDEC or the prescaled clock? Is the "+1" exact?
9. The AUTOC correction rule and MAXCMP counting.
10. Buffers: does CCx read show CCBUFx while CCBUFVx is set? Does SYNCBUSY.CCx stay set until the
    copy? Does CMD UPDATE override LUPD? What exactly is the "filter transition" UPDATE condition?
11. HALL: is the code match OVF (MC00 text) or MC0 (JH01/E5x text)? Where are the MSB/LSB split
    and the window/delay counters? Are CC bits 31:16 implemented (Harmony writes `<< 16`)? Which
    codes are invalid and which sequence is positive?
12. COUNTER: the role of pad 0 (PINEN0/PINVEN0), MC0 with PEREN = 1, EVACT = 2 on the MC00, and
    the RETRIGGER-at-enable behaviour (MC00 and E5x texts disagree).
13. Both PINENn and EVEIn set: which input wins?
14. VLC event: a pulse per transition or a level? Its width?
15. The minimum event and pad pulse width the PDEC sees (SWEVT pulse vs 2/f_GCLK_PDEC).
16. Does the pull resistor stay on when the pad is muxed to function G (PDEC)? (E0b. Almost
    certainly yes.)
17. GCLK_PDEC maximum (48 or 96 MHz; Table 43-13 garbled).
18. The size of the errata 1.5.1/1.5.2 delays: how long STATUS.xERR and STATUS.DIR lag INTFLAG.

## 7. Notes for the twin model (suggestions, not facts)

- `mchp-pdec` (`hw/timer/mchp_pdec.c`), a subclass of `mchp-regblock` like `mchp-tcc`. Add
  `{ "PDEC", TYPE_MCHP_PDEC }` to `pic32cm_models[]`. QDEC and HALL are **input-edge driven**: run
  the state machine on each filtered input change. Time only matters for the filter (commit a
  change after (FILTER + 1) ticks of stability, using a QEMU_CLOCK_VIRTUAL timer), the HALL
  window/delay counters, and COUNTER mode. COUNTER mode can reuse the TCC's event-driven tick
  arithmetic (prescaled GCLK, TOP, CC0/CC1 matches, next-event timer).
- COUNT reads return the READSYNC copy (TCC pattern). SYNCBUSY clears at once while GCLK_PDEC
  runs, and stays pending without it, whether enabled or not. CMD reads back 0 after execution.
  STATUS reset is 0x0040.
- Events: GPIO outputs `ovf-event`, `err-event`, `dir-event` (level), `vlc-event`, `mc-event`[2],
  and GPIO input `evu-event-in`[3]. These names match what `pic32cm_connect_events()` derives from
  the ATDF signals OVF/ERR/DIR/VLC/MC[n] and EVU[n].
- **Pad inputs need SoC and PORT work.** Today only function A reaches a peripheral input (PORT
  `func-a` → EIC, `pic32cm_connect_eic()`), and `pic32cm_connect_pads()` handles outputs only. The
  PDEC needs the pad level for functions G and J. Suggestion: a PORT output "pmux-level"[pin * 16
  + f] (the pad level while the pin is muxed to f, else 0), and a generic SoC pass that wires it to
  a model's GPIO input `pad-<group>-in`[index] (here `pad-qdi-in`[0..2]). Pads carrying the same
  signal (PA08 and PA16 for QDI0) are OR-ed as `pic32cm_connect_eic()` does. Generalize that
  function rather than adding a PDEC special case, since a TC/TCC capture input would need the same
  path.
- SAM E51: an `irq_lines` hook for the OTHER/MC0/MC1 split.
- Overlay `overlays/pdec_u2263.yaml` (sketch): CTRLA `enable_protected: {all_except: [SWRST,
  ENABLE]}`, `sync: {SWRST: SWRST, ENABLE: ENABLE}`, `self_clearing: [SWRST]`. CTRLBCLR/SET as
  aliases of CTRLBSET with `sync: CTRLB` and hooks. EVCTRL `enable_protected: all`. INTFLAG
  `w1c: all`. STATUS `w1c: [QERR, IDXERR, MPERR, WINERR, HERR, PRESCBUFV, FILTERBUFV, CCBUFV0,
  CCBUFV1]` and `ro: [STOP, DIR]` with `sync: STATUS`. PRESC/PRESCBUF `sync: PRESC`. FILTER and
  FILTERBUF `sync: FILTER`. COUNT `sync: COUNT` with pre_read/post_write hooks. CC/CCBUF
  `sync: "CC{n}"`. DBGCTRL kept across SWRST. `versions: ["1.0.0"]` covers all three devices.
  EVACT COUNT is "reserved" on the MC00, so gate it by device, or log it as LOG_GUEST_ERROR there.
- Until the open points are settled, follow the data sheet where it is clear and log LOG_UNIMP for
  AUTOC correction and the HALL window/delay counters, as `mchp_tcc.c` does for its unmodelled
  features.
