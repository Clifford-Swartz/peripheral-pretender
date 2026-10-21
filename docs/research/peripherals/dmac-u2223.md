# DMAC (Direct Memory Access Controller, IP U2223) on the PIC32CM MC00: research notes

Gathered 2026-10-20 to plan a QEMU model of the PIC32CM1216MC00048 DMAC and a silicon check on the
bare EV15N46A. The twin already models the SAM D5x DMAC (U2503) in `hw/dma/mchp_dmac.c`. The main
question here is what changes when that model also has to cover U2223. No hardware was touched.
Sources, most authoritative first:

1. **PIC32CM MC00 Family Data Sheet DS60001638E** (the PDF from microchip.com, read with
   pdftotext): chapter 24 "Direct Memory Access Controller (DMAC)", pp. 289-348. Also §9.4 (bus
   matrix, SRAM ports), Table 10-1, §28 (EVSYS generator and user tables), §30.6.4.1 (USART DMA),
   §33.6.4 (TC DMA), §34.6.5.1 (TCC DMA), §36.6.4 (ADC), §37.6.4 (SDADC), §39.6.3 (DAC) and §40.5.3
   (TSENS). Below, "§24.x" means this document.
2. **Errata DS80000905F**. Its sections are NVMCTRL, SERCOM I2C, OSCCTRL, AC, PDEC, SERCOM USART and
   OSC48M. **It has no DMAC erratum.**
3. DFP PIC32CM-MC_DFP 1.4.67: `PIC32CM1216MC00048.atdf` (module DMAC, U2223 version "2f0": instance
   at lines 193-211, module at 1650-1996) and `include/component/dmac.h`.
4. Repo and twin: `devices/pic32cm1216mc00048.json`, pp-qemu `hw/dma/mchp_dmac.c` and
   `include/hw/dma/mchp_dmac.h` (the U2503 model), `hw/arm/pic32cm_soc.c`, the generated
   `include/hw/mchp/gen/dmac_u2223.h`, `dmac_u2503.h` and `include/hw/arm/pic32cm1216mc00048_gen.h`,
   and `overlays/dmac_u2503.yaml`.
5. Harmony CSP `peripheral/dmac_u2223/` on master (`templates/plib_dmac.c.ftl`, `plib_dmac.h.ftl`,
   `config/dmac.py`), fetched 2026-10-20.
6. Secondary evidence (same IP on other parts): CircuitPython `adafruit/samd-peripherals`
   `samd/dma.c` and `samd/samd21/dma.c` (SAM D21, U2223 1.0.0), and the DMAC errata in the SAM L21
   and PIC32CX-BZ errata summaries, found through the Microchip docs MCP.

Search warning: the Microchip docs MCP returns the **SAM D21/DA1** data sheet for the query title "Low-Power,
32-bit Cortex-M0+ MCU with Advanced Analog and PWM" (OSC8M, DFLL48M, I2S, USB, PTC, "The SAM D21 implements
the ARM Cortex-M0+"). Its DMAC chapter (20) also describes U2223, but it is not the MC00 authority, so these notes
use the DS60001638E PDF.

Twin today: no model. `pic32cm_models[]` has `{ "DMAC", TYPE_MCHP_DMAC, "U2503" }` only, so on the
MC00 (and on the JH01) the DMAC is a plain `mchp-regblock` with the generated register table. The
channel registers do not follow CHID, ENABLE never clears, CHINTFLAG never sets and CRCBUSY never
sets. Harmony's `DMAC_ChannelTransfer()` therefore leaves the channel "busy" for good, and
`DMAC_CRCCalculate()` spins forever on CRCBUSY.

## 0. Summary

- **Same engine, different register front end.** The descriptor format, the end-address convention, the
  block actions, linked descriptors, write-back and the trigger actions (BLOCK, BEAT, TRANSACTION) are
  the same as U2503. What differs: the channel registers sit behind a **CHID window**, fields have
  moved (TRIGSRC, TRIGACT, CMD, LVL and the event controls all live in CHCTRLB), there are 12 channels,
  **one NVIC line**, no BURSTLEN or THRESHOLD, a QOSCTRL register, and CTRL.CRCENABLE with a simpler CRC
  engine (no CRCMODE).
- **Trigger numbering trap.** DS Table 24-2 numbers the triggers 0x00-0x27 with no gaps (TCC0 OVF is
  0x0A, TC0 OVF 0x15, ADC0 0x24, DAC 0x27). The ATDF per-instance `DMAC_ID_*` parameters,
  `devices/*.json dmac_triggers`, the generated SoC trigger table and Harmony agree with it. The ATDF
  value-group `DMAC_CHCTRLB__TRIGSRC`, the `DMAC_CHCTRLB_TRIGSRC_*_Val` macros in dmac.h and the
  generated `MCHP_DMAC_CHCTRLB_TRIGSRC_*` constants carry **SAM C21 numbers** from 0x0A up
  (SERCOM4/5, TCC0_OVF = 0x10, TC0_OVF = 0x1B, ...). They are wrong for this part.
- **The data sheet contradicts itself** in places silicon has to settle (§4). Is an invalid descriptor
  TERR or SUSP? Is TCMPL set with BLOCKACT NOACT? What does BTCNT = 0 do? Does CTRL.SWRST reset
  DBGCTRL? The BTCNT formula in §24.7.3/4 is also wrong (it gives 3 bytes for a word beat). The Events
  section §24.5.5 is copied from U2503 (it names CHEVCTRL and INCPRI). The register is called both
  PRICTRL and PRICTRL0.
- **The CRC engine has to be modelled**: Harmony's I/O-mode CRC waits for CRCBUSY to read 1 after each
  CRCDATAIN write.
- **One model covers both PIC32CM targets.** The JH01 has the same U2223 register map ("2.4.0", 12
  channels). Only the PRICTRL0 name and its 63 triggers differ.
- Evidence suggests that U2223 relatives have silicon bugs with linked descriptors (SAM L21 errata
  15670/15683, PIC32CX-BZ). The MC00 errata lists none. That is open question O10.

## 1. Instance facts

| | PIC32CM1216MC00048 (MC00) | Source |
|---|---|---|
| Base, IRQ | 0x41006000 (HPB1, bridge B), **NVIC line 7, one line for all channels** | §24.4 p.291, ATDF, JSON |
| Channels | 12 (CH_NUM 12, CH_BITS 4) | ATDF, §24.2 |
| Priority levels | 4 (LVL_NUM 4) | ATDF, §24.5.2.4 |
| Peripheral triggers | 40 ids, 0x00-0x27 (TRIG_NUM 40, TRIG_BITS 6) | ATDF, Table 24-2 p.336 |
| Event inputs | CH0-CH3 = EVSYS users 5-8. Table 28-3 (p.433) lists asynchronous, synchronous and resynchronized paths (the text extraction is interleaved, so read the PDF before relying on it) | §24.4, Table 28-3, JSON users 5-8 |
| Event outputs | CH0-CH3 = EVSYS generators 0x1F-0x22 (31-34) | §24.4, EVGEN table p.431, JSON |
| Clocks | AHB clock MCLK.AHBMASK.DMAC (bit 7), **on at reset**. No APB mask, no GCLK | Table 10-1 p.37, §24.4 |
| PAC | INSTANCE_ID 35 (bridge B index 3), not protected at reset | §24.4, ATDF |
| Bus | Data goes through the bus matrix (flash, APB bridges, SRAM port "DMAC Data"). Descriptor fetch and write-back use **direct SRAM ports** only | §9.4 Table 9-6 p.35 |
| Sleepwalking | yes. CHCTRLA.RUNSTDBY per channel. In standby it cannot write some ADC/RTC/TC/TCC/SDADC registers | §24.4, §24.5.6 p.310 |
| QoS reset | QOSCTRL 0x2A (DQOS, FQOS and WRBQOS all MEDIUM) | §24.6.7 p.322, ATDF params |

U2503 (SAM D5x) for comparison: 32 channels, five NVIC lines (channels 0-3, then the rest), TRIGSRC
7 bits, burst and threshold control, CRC monitor and generate modes.

## 2. Register map, U2223 (MC00) against U2503 (SAM D5x)

Sources: dmac.h, ATDF lines 1650-1801, §24.6.1-24.6.17 (pp. 314-333). U2503 is from pp-qemu
`include/hw/mchp/gen/dmac_u2503.h`.

### 2.1 Common registers

| Off | U2223 register and fields | Reset | U2503 difference | Notes |
|---|---|---|---|---|
| 0x00 | **CTRL** (16): SWRST 0, DMAENABLE 1, **CRCENABLE 2**, LVLEN0-3 8-11 | 0x0000 (DS heading prints "0x00X0") | no CRCENABLE | SWRST only acts with DMAENABLE = CRCENABLE = 0, otherwise "the DMAC will return an access error" (§24.6.1). DMAENABLE and CRCENABLE read 1 until the ongoing burst (CRC: CRCBUSY) is done |
| 0x02 | **CRCCTRL** (16): CRCBEATSIZE 1:0, CRCPOLY 3:2 (0 CRC16, 1 CRC32), CRCSRC 13:8 (0 NOACT, 1 IO, 0x20+n = channel n, 0x02-0x1F reserved) | 0 | + CRCMODE 15:14, CRCSRC up to CHN31 | Enable-protected by CTRL.CRCENABLE (§24.5.2.1). The CRCPOLY text in §24.6.2 is pasted from CRCBEATSIZE |
| 0x04 | **CRCDATAIN** (32) | 0 | same | I/O-mode input |
| 0x08 | **CRCCHKSUM** (32) | 0 | same | Enable-protected (CRCENABLE). CRC-32 with CRCBUSY = 0 reads **bit-reversed and complemented**. Upper 16 bits read 0 for CRC-16 (§24.6.4 p.319) |
| 0x0C | **CRCSTATUS** (8): CRCBUSY 0 (R/W, W1C in I/O mode), CRCZERO 1 (R) | 0 | + CRCERR 2 | §24.6.5 p.320, see §3.8 |
| 0x0D | **DBGCTRL** (8): DBGRUN | 0 | same | |
| 0x0E | **QOSCTRL** (8): WRBQOS 1:0, FQOS 3:2, DQOS 5:4 | **0x2A** | absent (per-level QOS in PRICTRL0) | storage only |
| 0x10 | **SWTRIGCTRL** (32): SWTRIG0-11 | 0 | SWTRIG0-31 | the bit converts to CHSTATUS.PEND at once, see §3.5 |
| 0x14 | **PRICTRL** (ATDF and dmac.h name). DS §24.6.9 and the JH01 ATDF call it **PRICTRL0**. Fields: LVLPRIx 4 bits at 0/8/16/24, RRLVLENx at 7/15/23/31 | 0 | PRICTRL0: LVLPRIx 5 bits, + QOSx 2 bits | Address it by offset, not by name. Harmony picks the name from the ATDF |
| 0x20 | **INTPEND** (16): ID 3:0 (R/W), TERR 8, TCMPL 9, SUSP 10 (W1C into channel ID), FERR 13, BUSY 14, PEND 15 (R) | 0 | ID 4:0, + CRCERR 12 | Not PAC-protected |
| 0x24 | **INTSTATUS**: CHINT0-11 (flag AND enable, per §24.6.11) | 0 | 0-31 | R |
| 0x28 | **BUSYCH**: 0-11 | 0 | 0-31 | R |
| 0x2C | **PENDCH**: 0-11 | 0 | 0-31 | R |
| 0x30 | **ACTIVE**: LVLEX0-3 3:0, ID 12:8 (5 bits even with 12 channels), ABUSY 15, BTCNT 31:16 | 0 | same layout | R |
| 0x34 | **BASEADDR** | 0 | same | Enable-protected by DMAENABLE. "Must be 128-bit aligned" |
| 0x38 | **WRBADDR** | 0 | same | as above |
| 0x3F | **CHID** (8): ID 3:0 | 0 | absent | selects the channel seen at 0x40-0x4F. Not PAC-protected |

### 2.2 Channel registers

U2223 has **one window at 0x40-0x4F for the channel in CHID.ID**. U2503 has 32 blocks at
0x40 + 0x10 × n.

| U2223 window | Fields | U2503 per-channel block |
|---|---|---|
| 0x40 **CHCTRLA** (8) | SWRST 0, ENABLE 1, RUNSTDBY 6 | +0x00 CHCTRLA (32): SWRST, ENABLE, RUNSTDBY, **TRIGSRC 14:8 (7 b), TRIGACT 21:20, BURSTLEN 27:24, THRESHOLD 29:28** |
| 0x44 **CHCTRLB** (32) | EVACT 2:0, EVIE 3, EVOE 4, LVL 6:5, **TRIGSRC 13:8 (6 b), TRIGACT 23:22, CMD 25:24** | +0x04 CHCTRLB (8): CMD 1:0. +0x05 CHPRILVL: PRILVL 1:0. +0x06 CHEVCTRL: EVACT 2:0, EVOMODE 5:4, EVIE 6, EVOE 7 |
| 0x4C CHINTENCLR, 0x4D CHINTENSET | TERR 0, TCMPL 1, SUSP 2 | +0x0C, +0x0D, same |
| 0x4E **CHINTFLAG** | TERR, TCMPL, SUSP (W1C) | +0x0E, same |
| 0x4F **CHSTATUS** (R) | PEND 0, BUSY 1, FERR 2 | +0x0F, + CRCERR 3 |

- TRIGACT (§24.6.19 p.335): 0 BLOCK, 1 reserved, 2 **BEAT**, 3 TRANSACTION. U2503 has 2 = BURST of
  BURSTLEN+1 beats, so U2223 BEAT behaves like U2503 BURST with BURSTLEN = 0.
- EVACT: 0 NOACT, 1 TRIG, 2 CTRIG, 3 CBLOCK, 4 SUSPEND, 5 RESUME, 6 SSKIP, **7 reserved** (U2503: 7 =
  INCPRI). EVIE, EVOE and EVACT only exist on channels 0-3 (§24.6.19).
- Enable protection (§24.5.2.1 p.293): CHCTRLB, **except CMD and LVL**, can be written only while that
  channel's CHCTRLA.ENABLE = 0. CHCTRLA.SWRST "will be ignored as long as ENABLE = 1" (§24.6.18).
- CHCTRLA.ENABLE written 0 during a transfer reads 1 until the ongoing burst is done (§24.6.18).
- PAC write protection (the "Property" lines): CTRL, CRCCTRL, CRCDATAIN, CRCCHKSUM, CRCSTATUS,
  DBGCTRL, QOSCTRL, SWTRIGCTRL, PRICTRL0, BASEADDR, WRBADDR, CHCTRLA, CHCTRLB, CHINTENCLR and
  CHINTENSET. Not protected: INTPEND, INTSTATUS, BUSYCH, PENDCH, ACTIVE, CHID, CHINTFLAG and
  CHSTATUS. (`dmac_u2503.yaml` only marks CTRL, BASEADDR and WRBADDR.)
- What CHID values 12-15 do is not stated (O8).

### 2.3 Descriptor (the same in both IPs)

The descriptor is 16 bytes. Each channel's first descriptor is at BASEADDR + 16 × ch, and its ongoing copy at
WRBADDR + 16 × ch. Further descriptors are linked through DESCADDR (§24.5.2.3 p.294, Figure 24-3,
§24.7 pp. 342-348).

| Off | Field | Bits |
|---|---|---|
| 0x0 | BTCTRL (16) | VALID 0. EVOSEL 2:1 (0 DISABLE, 1 BLOCK, 2 reserved, 3 BEAT; U2503 calls 3 "BURST"). BLOCKACT 4:3 (0 NOACT, 1 INT, 2 SUSPEND, 3 BOTH). BEATSIZE 9:8 (0 byte, 1 halfword, 2 word, 3 reserved). SRCINC 10, DSTINC 11. STEPSEL 12 (0 DST, 1 SRC). STEPSIZE 15:13 (X1 ... X128). Mask 0xFF1F |
| 0x2 | BTCNT (16) | beats in the block, counted down per beat |
| 0x4 | SRCADDR (32) | end address when SRCINC = 1 (see below) |
| 0x8 | DSTADDR (32) | end address when DSTINC = 1 |
| 0xC | DESCADDR (32) | next descriptor. 0 = last |

- **End-address convention** (§24.5.2.7 p.300): with xINC = 1 the address field holds the address *after*
  the last beat, ADDR = START + BTCNT × bytes_per_beat × (2^STEPSIZE if STEPSEL selects this side, else 1).
  STEPSIZE only applies to the side STEPSEL selects, and the other side steps one beat. §24.7.3 and
  §24.7.4 instead write "BTCNT × (BEATSIZE + 1) × 2^STEPSIZE", which gives 3 bytes per word beat: a DS
  error. §24.5.2.7 itself defines BEATSIZE as "the configured number of bytes in a beat". Harmony
  writes SRCADDR = src + blockSize with BTCNT = blockSize >> BEATSIZE, and the U2503 twin model uses
  the same convention. §24.5.2.7 also has a typo: it gives the destination register as "(SRCADDR)".
- With xINC = 0 the field is the fixed address ("the last beat transfer address", §24.7.3).
- BTCNT: "1 to 64k beats" (§24.5.1.1). The features list says "From 1 KB to 256 KB data transfer in a
  single block", a typo, presumably for 1 byte. What BTCNT = 0 does is not stated (O3).
- At the end of a block **VALID is cleared, then the whole descriptor is written back** (§24.5.2.5
  p.298, §24.7.1). BTCNT is written back whenever the channel loses arbitration, is suspended or is
  disabled (§24.7.2 p.345).
- Alignment: the DS says BASEADDR, WRBADDR and DESCADDR "must be 128-bit aligned" (§24.6.15,
  §24.6.16, §24.7.5). The ATDF register-group `DMAC_DESCRIPTOR` says `aligned="8"`, Harmony declares
  its sections `__ALIGNED(8)`, and CircuitPython (SAM D21) uses 16-byte alignment (O6).
- All descriptors must be in SRAM (§24.5.2.3). Fetch and write-back only have direct SRAM ports (Table
  9-6), so a descriptor in flash is undefined (O7).
- The section size is 16 × (highest channel used + 1) bytes. BASEADDR may equal WRBADDR. Then the
  first descriptor is overwritten during the transfer and must be re-armed before every transfer
  (§24.5.2.3).

### 2.4 Triggers on this device (DS Table 24-2, p.336)

| TRIGSRC | Source | Request set / cleared (DS) | Twin today (SoC wiring) |
|---|---|---|---|
| 0x00 | DISABLE: software and event triggers only | | software trigger works |
| 0x01 | TSENS RESRDY | set when a result is ready, cleared when VALUE is read (§40.5.3) | level: INTFLAG.RESRDY at base + 0x06 |
| 0x02 / 0x03 | SERCOM0 RX / TX | USART RX: set while the RX FIFO has data, cleared by a DATA read. TX: set while the TX buffer is empty, cleared by a DATA write (§30.6.4.1 p.454) | level: INTFLAG.RXC / DRE at base + 0x18 |
| 0x04 / 0x05 | SERCOM1 RX / TX | same | same |
| 0x06 / 0x07 | SERCOM2 RX / TX | same | same |
| 0x08 / 0x09 | SERCOM3 RX / TX | same | same |
| 0x0A-0x0E | TCC0 OVF, MC0, MC1, MC2, MC3 | OVF: one request per update (CTRLA.DMAOS = 0) or after the DMAOS command. MCx: on compare match. Cleared on DMA acknowledge (§34.6.5.1 p.662) | pulses (`MCHP_TCC_OVF_DMA`, `MCHP_TCC_MC_DMA`): OVF per update or retrigger, MCx per compare match or capture into CCx |
| 0x0F-0x11 | TCC1 OVF, MC0, MC1 | same | same |
| 0x12-0x14 | TCC2 OVF, MC0, MC1 | same | same |
| 0x15-0x17 | TC0 OVF, MC0, MC1 | OVF: set on overflow, underflow or re-trigger, cleared on DMA acknowledge. MCx compare: set on match, cleared on acknowledge. MCx capture: set while CCx holds data, cleared by the CCx read (§33.6.4 p.568) | OVF as a pulse (`MCHP_TC_OVF_DMA`), MCx not wired |
| 0x18-0x1A | TC1 OVF, MC0, MC1 | same | same |
| 0x1B-0x1D | TC2 OVF, MC0, MC1 | same | same |
| 0x1E-0x20 | TC3 OVF, MC0, MC1 | same | same |
| 0x21-0x23 | TC4 OVF, MC0, MC1 | same | same |
| 0x24 / 0x25 | ADC0 / ADC1 RESRDY | set when a result (or average) is ready, cleared by the RESULT read (§36.6.4 p.737) | level: INTFLAG.RESRDY at base + 0x06, with a DMAC kick |
| 0x26 | SDADC RESRDY | same (§37.6.4) | level: INTFLAG.RESRDY at base + 0x07, with a DMAC kick |
| 0x27 | DAC EMPTY | set when DATABUF moves into the DAC, cleared by a DATABUF write or by writing INTFLAG.EMPTY. Requested as soon as the DAC is enabled (§39.6.3) | level: INTFLAG.EMPTY at base + 0x06, with a DMAC kick (silicon: every enable sets EMPTY, `dac-u2214.md`) |
| 0x28-0x3F | reserved | | |

The ATDF value-group `DMAC_CHCTRLB__TRIGSRC` (and so dmac.h's `DMAC_CHCTRLB_TRIGSRC_*_Val`) matches
this table only up to 0x09. From 0x0A it holds SERCOM4/5 RX/TX at 0x0A-0x0D, TCC0_OVF at 0x10 and so
on up to TC4_MC1 at 0x29, then ADC0 0x2A, ADC1 0x2B, SDADC 0x2C and DAC 0x2D. That is the SAM C21
list without its CAN and PTC entries, and the MC00 has no SERCOM4/5. The ATDF's own `DMAC_ID_*`
instance parameters and `TRIG_NUM = 40` agree with DS Table 24-2, and so do
`devices/pic32cm1216mc00048.json` `dmac_triggers`, `pic32cm1216mc00048_dmac_triggers[]` and Harmony
(`dmac.py` builds its list from the `DMAC_ID_*` parameters). The merged generated header
`include/hw/mchp/gen/dmac_u2223.h` has `MCHP_DMAC_CHCTRLB_TRIGSRC_*` constants from the JH01 and MC00
value-groups merged together. A model must take trigger ids from the device's `dmac_triggers` and
never from those constants. Firmware that uses dmac.h's macros (e.g. `DMAC_CHCTRLB_TRIGSRC_TC0_OVF` =
0x1B) would select TC2 OVF according to Table 24-2. Silicon can confirm this cheaply (E15). This
belongs in the known-ATDF-errors list in `docs/research/microchip-data.md`; that file was not edited
here.

### 2.5 Reset values

All DMAC registers reset to 0 except QOSCTRL = 0x2A (§24.6, ATDF). The write-back and descriptor
sections are plain SRAM and have no reset value. CTRL.SWRST resets "all registers in the DMAC except
DBGCTRL" (§24.5.2.2, §24.6.1). §24.6.6 instead says DBGRUN "is reset by a software reset" (O9).
CHCTRLA.SWRST resets that channel's registers (§24.5.2.2).

## 3. Behaviour

### 3.1 Data transfer, arbitration and write-back

- A trigger on an **enabled, not suspended** channel sends a request to the arbiter and sets
  PENDCH.n and CHSTATUS.PEND. The grant clears PENDCH.n. The first granted burst of a request sets
  BUSYCH.n, which stays 1 while the channel goes back into the queue for more bursts. It is cleared when
  the channel waits for a new trigger, is suspended or is disabled (§24.5.2.4 p.295, §24.6.12/13).
- Arbitration runs after every burst (§24.5.2.5). U2223 has no BURSTLEN, so a burst is presumably
  one beat (guess. The text is shared with U2503. E17 measures the interleaving).
- Priority: a higher CHCTRLB.LVL wins. Requests at a level with CTRL.LVLENx = 0 are ignored. Within a
  level, PRICTRL.RRLVLENx = 0 means static priority, lowest channel number first, which can starve high
  numbers. RRLVLENx = 1 means round-robin: the last granted channel (kept in LVLPRIx) gets the lowest
  priority next time (§24.5.2.4, Figures 24-5/24-6, §24.6.9). ACTIVE.LVLEXx is set while a level-x
  request executes or is pending (§24.6.14).
- When a channel is granted, its descriptor is fetched into the active channel: from BASEADDR for a new
  block, from WRBADDR for an ongoing block. So the **first fetch happens at the first grant, not at
  ENABLE** (§24.5.2.5). When another channel is granted, BTCNT is written back first.
- Block end: VALID is cleared and the whole descriptor written back. Then come TCMPL/SUSP and the
  block event. If DESCADDR is not 0, the next descriptor is fetched and **written to the write-back
  section before the arbiter picks the next channel**. Firmware can see progress there (§24.5.2.5).
- **Last block** (DESCADDR = 0): "the DMA channel will either be suspended or disabled, depending on
  ... BTCTRL.BLOCKACT" (§24.5.2.5). Resuming a channel that then fetches DESCADDR = 0 suspends it again,
  with SUSP and FERR set (§24.5.2.8).
- If firmware edits the write-back copy of a suspended channel (for example its DESCADDR, as in the
  list-editing recipes of §24.5.3.1.2/3), the channel uses the edit after RESUME, because ongoing
  blocks are re-fetched from WRBADDR.

### 3.2 Trigger actions (§24.5.2.6 p.299, Figure 24-7)

- BLOCK (0): one trigger per block. With a single descriptor, the channel disables itself at the end of
  the block. With a list it waits for the next trigger after each block and disables itself after the last.
- BEAT (2): one trigger per beat. TRANSACTION (3): one trigger runs every block of the list.
- **One pending request per channel**. Triggers that arrive while one is pending are lost. Triggers
  during an ongoing transfer stay pending (CHSTATUS.PEND = 1) and start when it ends.
- CHSTATUS.BUSY "is set when the DMA channel starts a DMA transfer" and cleared "when the channel
  trigger action is completed, when a bus error is detected or when the channel is disabled" (§24.6.23).
  So with BEAT and BLOCK, BUSY is **0 while the channel waits for its next trigger** (to be confirmed,
  E14).
- Peripheral requests: SERCOM, ADC, SDADC, TSENS and DAC requests are levels that stay up until
  firmware or the DMA services the register. TC and TCC overflow and compare requests are cleared "on
  DMA acknowledge", that is, one request per event (§2.4).

### 3.3 When CHINTFLAG.TCMPL sets

- §24.5.4: "Transfer Complete (TCMPL): Indicates that a block transfer is completed". §24.6.22: set
  "when a block transfer is completed **and the corresponding interrupt block action is enabled**"
  (BLOCKACT INT or BOTH). §24.6.20 adds, under the CHINTENCLR.TCMPL value 0: "When block action is set
  to none, the TCMPL flag will not be set when a block transfer is completed."
- Against that: CircuitPython on the SAM D21 (the same IP) builds descriptors with BLOCKACT NOACT,
  never enables channel interrupts, and waits for `CHINTFLAG & (TCMPL | TERR)` to end every SPI DMA
  transfer (`samd/dma.c` `shared_dma_transfer_finished`). That code ships and works. So TCMPL very likely
  **does** set at the end of the transaction with NOACT (guess for the MC00). The U2503 twin model sets
  TCMPL at the end of the transaction whatever BLOCKACT is, because CircuitPython on the SAM D51 needs it.
- Middle blocks with NOACT: probably no TCMPL (guess). E8 tests the full matrix.

### 3.4 Errors (§24.5.2.8 p.302)

- **Bus error during a beat**: the channel is disabled, CHINTFLAG.TERR is set, BTCNT is *not*
  decremented and is written back, and BUSYCH/PENDCH/CHSTATUS BUSY and PEND are cleared (§24.6.12,
  §24.6.13, §24.6.23).
- **Invalid descriptor** (VALID = 0), or a resume that fetches DESCADDR = 0: §24.5.2.8 and §24.5.3.2
  say the channel is **suspended** with CHINTFLAG.SUSP and CHSTATUS.FERR set (the channel stays
  enabled). §24.6.22 says SUSP is set "when an invalid descriptor is fetched". But §24.5.4 and the
  CHINTFLAG.TERR description (§24.6.22) say TERR is set "when the DMAC fetches an invalid
  descriptor". FERR is cleared by a software RESUME command (§24.6.23). The SAM L21 data sheet has the
  same suspend wording. The U2503 twin model raises TERR and FERR and disables the channel. E10 tests it.
- Writing VALID = 0 in the descriptor or write-back memory is the documented way to stop a list at a
  given point (§24.7.1).

### 3.5 Software trigger (§24.6.8 p.323)

Writing SWTRIGx = 1 with CHSTATUS.PEND = 0 makes the channel pending: "CHSTATUS.PEND will be set and
SWTRIGx will remain cleared", so the bit reads back 0. If PEND is already 1, SWTRIGx stays set,
presumably as a queued second request (guess). Writing 0 clears it. The DS says nothing about a
disabled channel (O4). Harmony and CircuitPython both set the bit with `SWTRIGCTRL |= bit`, a
read-modify-write. CircuitPython also clears it with `&= ~bit` when it configures a channel.

### 3.6 Suspend and resume (§24.5.3.2-3, pp. 303-304)

- CMD = SUSPEND: the channel suspends after the ongoing burst. CMD clears itself and SUSP is set.
  Suspending a disabled channel is ignored.
- BLOCKACT SUSPEND or BOTH: the channel suspends after the block. It stays enabled and still takes
  triggers (PEND), but leaves the arbitration. Per §24.5.2.4, PENDCH.n stays set while it is suspended.
- CMD = RESUME continues where the channel stopped. A RESUME issued *before* the suspend makes the
  channel skip the next suspend (Figure 24-10). Event actions: SUSPEND, RESUME (which "is acknowledged
  ... and the Channel Suspend Interrupt Flag is cleared", so a resume event seems to clear SUSP) and
  SSKIP (§24.5.3.4).

### 3.7 Events (§24.5.3.4-5, pp. 304-307)

- Inputs (channels 0-3, CHCTRLB.EVIE plus EVACT): TRIG with TRIGSRC = DISABLE is a plain transfer
  trigger. TRIG with a peripheral TRIGSRC is "conditional transfer on strobe": the peripheral request
  only counts when an event coincides. CTRIG latches the peripheral trigger on an event. CBLOCK needs one
  event before each block, acknowledged at the end of the block. SUSPEND, RESUME and SSKIP act as in
  §3.6. Events that arrive while the channel is pending are lost.
- Outputs (channels 0-3, CHCTRLB.EVOE): BTCTRL.EVOSEL BLOCK pulses at the end of each block, BEAT
  after each beat. The pulse is one AHB clock wide. A transaction-complete event means BLOCK in the
  last descriptor only (§24.5.3.5).
- DS error: §24.5.5 describes "CHEVCTRLx.EVOE/EVIE" and an INCPRI action, which are U2503 features.
  The MC00 register map (§24.6.19) has the bits in CHCTRLB and EVACT 7 reserved.

### 3.8 CRC engine (§24.5.3.7 pp. 307-309, §24.6.2-24.6.5)

- Polynomials: CRC-16 CCITT (0x1021) and CRC-32 IEEE 802.3 (0x04C11DB7). The engine works a byte at a
  time.
- Sources (CRCCTRL.CRCSRC): **I/O** (0x01) means the CPU writes CRCDATAIN with CRCBEATSIZE bytes, and
  "a 32-bit write takes four cycles". **DMA channel n** (0x20 + n) means every beat of channel n is fed
  in at that channel's BEATSIZE. A DMA channel can also write into CRCDATAIN with the source set to I/O.
- **CRCBUSY in I/O mode**: "The CRC complete is signaled by a set CRCBUSY bit ... New data can be
  written only when CRCBUSY flag is not set" (§24.5.3.7), and firmware clears it by writing 1
  (§24.6.5). Harmony's `DMAC_CRCCalculate()` follows exactly that: write CRCDATAIN, **wait until CRCBUSY
  = 1**, write 1 to clear it, repeat. §24.6.5 also says the bit "is set when a source configuration is
  selected and as long as the source is using the CRC module", which contradicts this (E18). In
  channel mode CRCBUSY follows the channel's ENABLE and firmware cannot clear it.
- **CRCCHKSUM**: the seed is written while CRCENABLE = 0 (enable-protected). For CRC-32 with CRCBUSY = 0
  the read returns the bit-reversed and complemented value, while busy it returns the raw register.
  CRC-16 reads raw, upper half 0 (§24.6.4). CRCZERO: cleared when a new source is selected, set when
  generation completes with a zero checksum. The CRC-32 residue over data plus its LE checksum is
  0x2144DF1C (§24.6.5).
- CTRL.CRCENABLE written 0 takes effect once CRCBUSY is clear, and reads 1 until then (§24.6.1). Harmony
  writes CRCCTRL right after clearing CRCENABLE. That works because its loop leaves CRCBUSY clear.
- Expected values, which are **guesses** until E18. With seed 0xFFFF, a non-reflected CRC-16 gives
  0x29B1 for "123456789" (CRC-16/CCITT-FALSE). Seed 0 gives 0x31C3. With seed 0xFFFFFFFF, a CRC-32
  with reflected input and the documented bit-reverse plus complement gives 0xCBF43926 (zlib).

| Buffer | CRC-16 seed 0 | CRC-16 seed 0xFFFF | CRC-32 seed 0xFFFFFFFF, standard (reflected input) | CRC-32 seed 0xFFFFFFFF, MSB-first input, then reversed and complemented |
|---|---|---|---|---|
| "123456789" | 0x31C3 | 0x29B1 | 0xCBF43926 | 0x1898913F |
| bytes 00..0F | 0x513D | 0x3B37 | 0xCECEE288 | 0x4D00A16A |

  (A reflected-input CRC-16 would give 0x9184 / 0x89F6 and 0x023D / 0x6837.)

### 3.9 Interrupts (§24.5.4 p.309, §24.6.10-11)

- An interrupt request is "flag AND enable" per channel, ORed into the one NVIC line 7. INTSTATUS.n =
  channel n has an enabled flag set.
- INTPEND.ID: the "lowest channel number with pending interrupts". It is refreshed when a lower channel
  becomes pending or when firmware clears the current one, and reads 0 when nothing is pending.
  Writing ID gives "indirect access" to that channel's flags, so INTPEND.TERR/TCMPL/SUSP written 1 clear
  them in channel ID. Whether disabled flags count as "pending" for INTPEND is open (O5).
- Harmony's handler (MC00, one vector): read INTPEND.ID, save CHID, write CHID, read CHINTFLAG, clear
  TCMPL and TERR by writing CHINTFLAG, call back, restore CHID.

### 3.10 Aborting (§24.5.3.6 p.307)

Clearing a channel's ENABLE stops it after the ongoing beat, once the write-back is updated. ENABLE
reads 1 until then, so Harmony polls `while (CHCTRLA & ENABLE)`. Clearing CTRL.DMAENABLE aborts
everything: the active channel after its beat, and "all other enabled channels will be disabled in
the next clock cycle", with each channel's **CHCTRLA.ENABLE cleared** (guess that this is literal,
E12).

## 4. What Harmony and CircuitPython do (firmware-visible requirements)

Harmony `plib_dmac.c.ftl` (U2223):

- `DMAC_Initialize`: BASEADDR = `descriptor_section[]` and WRBADDR = `write_back_section[]`, both
  `static ... __ALIGNED(8)`, placed in `.lpram` when the part has LPRAM (the MC00 has none). Writes
  PRICTRL (or PRICTRL0) with LVLPRIx and RRLVLENx. For each used channel: CHID = n,
  `CHCTRLA |= RUNSTDBY` (if selected), CHCTRLB = TRIGACT | TRIGSRC (from `DMAC_ID_*`) | LVL (+ EVACT |
  EVIE, EVOE for channels below 4), descriptor BTCTRL = **BLOCKACT_INT** | BEATSIZE | VALID |
  SRCINC/DSTINC (+ EVOSEL), CHINTENSET = TERR | TCMPL. Finally CTRL = DMAENABLE | LVLEN0-3.
- `DMAC_ChannelTransfer`: saves CHID and selects the channel. If CHINTFLAG has TCMPL or TERR, or the
  channel is not busy: clears both flags, writes SRCADDR/DSTADDR as end addresses (+blockSize when
  incrementing), BTCNT = blockSize >> BEATSIZE, `CHCTRLA |= ENABLE`, and **then**
  `SWTRIGCTRL |= 1 << ch` if TRIGSRC = 0 and EVIE = 0. Restores CHID.
- `DMAC_ChannelIsBusy` and `ChannelTransferStatusGet`: read CHINTFLAG through CHID.
- `DMAC_ChannelDisable` and `ChannelSettingsSet`: clear ENABLE, then poll until it reads 0.
- `DMAC_ChannelGetTransferredCount`: descriptor BTCNT minus **write-back** BTCNT. Mid-transfer, this is
  only correct if the write-back BTCNT is current (O2).
- Linked lists: `LinkedListDescriptorSetup` and `ChannelLinkedListTransfer` copy the first descriptor
  into the descriptor section, enable the channel and software-trigger it.
- Suspend and resume: CHCTRLB = (CHCTRLB & ~CMD) | CMD_SUSPEND or CMD_RESUME. Because LVL is not
  enable-protected, this read-modify-write is legal while the channel is enabled.
- CRC: `ChannelCRCSetup` disables the CRC, writes CRCCHKSUM = seed, CRCCTRL = CRCPOLY | CRCSRC(0x20 + ch),
  and sets CTRL.CRCENABLE. `CRCRead` returns CRCCHKSUM. `CRCCalculate` is the I/O-mode loop in §3.8, and
  it picks the beat size from the buffer length.

CircuitPython on the SAM D21 (`samd/samd21/dma.c`, `samd/dma.c`; secondary evidence):

- Configure: CHID, ENABLE = 0, `CHCTRLA = SWRST`, `SWTRIGCTRL &= ~bit`, CHCTRLB = LVL0 | TRIGSRC |
  **TRIGACT_BEAT** (| EVOE for audio). Enable: `CHINTFLAG = 0x07`, then ENABLE. Resume: CMD_RESUME,
  then `CHINTFLAG = SUSP`.
- Descriptors use BLOCKACT NOACT and no channel interrupt. Completion is `CHINTFLAG & 3` (TCMPL or
  TERR). `dma_channel_free()` tests `CHSTATUS == 0`, so the CHSTATUS BUSY and PEND semantics are
  firmware-visible.

## 5. Model plan: extending `mchp_dmac.c` to U2223

Recommendation: keep one engine and add a U2223 subclass, `TYPE_MCHP_DMAC_U2223` derived from
`TYPE_MCHP_DMAC`. Then the SoC's `object_dynamic_cast(..., TYPE_MCHP_DMAC)` trigger wiring
(`pic32cm_dmac_triggers`, `pic32cm_dmac_pulses`) works unchanged, and the pattern matches the
ADC/U2247 entries in `pic32cm_models[]`. A separate model would duplicate the roughly 300 lines of
descriptor and trigger engine (load, store, fetch, beat, block_end, serve, run, timer, pulses), which
are IP-neutral except for how they read channel configuration. These are suggestions, not facts:

1. **Engine/front-end split.** Have the engine read a normalized per-channel configuration (enabled,
   trigsrc, trigact, beats per trigger, lvl, evie/evact/evoe, intenset, intflag) through class hooks
   instead of `dmac_reg(DMAC_CH(ch, ...))`. U2503 fills it from its per-channel regfile blocks, as
   today. U2223 fills it from shadow arrays. Per IP: channel count (32 or 12), IRQ lines (5 or 1),
   trigger-action encoding (U2503 BURST uses BURSTLEN+1. U2223 BEAT = 1 beat).
2. **CHID window.** CHID at 0x3F is plain storage (mask 0x0F). Reads of 0x40-0x4F return
   `shadow[CHID]`. CHSTATUS and the CHINTENCLR/CHINTENSET pair are computed. Writes update
   `shadow[CHID]` and restore the regfile's single window copy. Writes implement CHCTRLA.SWRST (only
   while ENABLE = 0), ENABLE rising and falling, CHCTRLB.CMD (then self-clearing) and CHINTFLAG W1C into
   the shadow. CHID 12-15 per silicon (E22).
3. **Enable protection.** The regfile's generic `enprot` uses one `is_enabled()` per block. The DMAC has
   three different enables: DMAENABLE for BASEADDR/WRBADDR, CRCENABLE for CRCCTRL/CRCCHKSUM, and
   per-channel ENABLE for CHCTRLB except CMD and LVL. So either enforce them in `post_write` (restore
   `old` bits) or give the regfile a per-register enable callback.
4. **SWTRIGCTRL** (`post_write`): move each written bit into a per-channel software-pending flag and
   clear it in the register, unless the channel is already pending (§3.5).
5. **CTRL**: SWRST only with DMAENABLE = CRCENABLE = 0. It resets the 12 shadow channels and the CRC
   too (DBGCTRL per E21). It is an access error otherwise: `MEMTX_ERROR` if E21 shows a fault. Clearing
   DMAENABLE disables every channel and clears its ENABLE (E12).
6. **Arbitration** (helps both IPs): honour CHCTRLB.LVL, CTRL.LVLENx and PRICTRL.RRLVLENx, and update
   LVLPRIx for round-robin levels. Today the engine serves the lowest channel first and runs a whole
   trigger action at once. Beat-level interleaving only matters for overlapping channels (E17).
7. **CRC engine**: I/O mode (each CRCDATAIN write feeds CRCBEATSIZE bytes, LSB first (guess, E18),
   then sets CRCBUSY, which firmware clears by W1C) and channel mode (feed every beat of channel n.
   CRCBUSY = channel n enabled). The CRCCHKSUM read is reversed and complemented for CRC-32 when CRCBUSY
   = 0. Also CRCZERO. Without this, Harmony's `DMAC_CRCCalculate()` hangs.
8. **IRQ**: `irq_lines` = bit 0 if any channel has flag AND enable.
9. **Events**: inputs `ch-event-in`[0..3] (EVSYS users 5-8; `pic32cm_connect_events` connects
   `<sig>-event-in` by name): TRIG = trigger, SUSPEND, RESUME (also clears SUSP?), SSKIP. CTRIG and
   CBLOCK can come later. Outputs: add EVOSEL BEAT pulses (today only BLOCK).
10. **Overlay** `overlays/dmac_u2223.yaml` (sketch, keys as in `dmac_u2503.yaml` and `eic_u2254.yaml`.
    Run the generator for every device in one go):

    ```yaml
    ip: DMAC
    id: U2223
    versions: ["2f0", "2.4.0"]        # MC00, JH01
    registers:
      CTRL: {pac_protected: true, self_clearing: [SWRST], hooks: [post_write]}
      CRCCTRL: {pac_protected: true, hooks: [post_write]}     # enable-protected by CRCENABLE (model)
      CRCDATAIN: {pac_protected: true, hooks: [post_write]}   # feeds the CRC
      CRCCHKSUM: {pac_protected: true, hooks: [pre_read, post_write]}
      CRCSTATUS: {pac_protected: true, dynamic: true, hooks: [post_write]}
      DBGCTRL: {pac_protected: true}
      QOSCTRL: {pac_protected: true}
      SWTRIGCTRL: {pac_protected: true, hooks: [post_write]}
      PRICTRL: {pac_protected: true, optional: true}           # MC00 name
      PRICTRL0: {pac_protected: true, optional: true}          # JH01 name
      INTPEND: {dynamic: true, hooks: [post_write]}
      INTSTATUS: {dynamic: true}
      BUSYCH: {dynamic: true}
      PENDCH: {dynamic: true}
      ACTIVE: {dynamic: true}
      BASEADDR: {pac_protected: true, hooks: [post_write]}     # enable-protected by DMAENABLE
      WRBADDR: {pac_protected: true, hooks: [post_write]}
      CHID: {}
      CHCTRLA: {pac_protected: true, dynamic: true, hooks: [post_write]}  # window on shadow[CHID]
      CHCTRLB: {pac_protected: true, dynamic: true, hooks: [post_write]}
      CHINTENCLR: {pac_protected: true, dynamic: true, hooks: [post_write]}
      CHINTENSET: {pac_protected: true, dynamic: true, hooks: [post_write]}
      CHINTFLAG: {dynamic: true, hooks: [post_write]}  # no regfile w1c: the shadow is cleared
      CHSTATUS: {dynamic: true}
    ```

11. **SoC**: add `{ "DMAC", TYPE_MCHP_DMAC_U2223, "U2223" }` to `pic32cm_models[]`. Trigger ids already
    come from `dmac_triggers` (correct, §2.4), and `MCHP_DMAC_MAX_TRIGGERS` = 128 covers 6-bit TRIGSRC.
    All of §2.4's triggers are wired.
12. **Places where the U2503 model and the U2223 data sheet disagree.** Check the SAM D5x data sheet
    before changing the shared engine, since its wording is probably the same:
    1. Invalid descriptor: the model raises TERR and FERR and disables. The DS says SUSP and FERR, channel
       still enabled (with a contradiction; E10).
    2. First descriptor fetch: the model fetches at ENABLE. The DS fetches at the first grant (E5).
    3. VALID: the model leaves it set in the write-back copy at the end of a block. The DS clears it
       (§24.5.2.5).
    4. Last block with BLOCKACT SUSPEND or BOTH: the model disables the channel and sets TCMPL. The DS
       suspends it (E9).
    5. TCMPL with NOACT at the end of a transaction: the model sets it. The DS says no. CircuitPython says
       yes (E8).
    6. CHSTATUS.BUSY: the model reports BUSY while the channel holds a descriptor. The DS reports it only
       during a trigger action (E14).
    7. Bus errors: `dma_memory_read/write` results are ignored. The DS sets TERR, disables the channel and
       writes BTCNT back (E11).
    8. Pulsed triggers that arrive while the channel is suspended: the model drops them. The DS keeps them
       pending (E13).
    9. SWTRIGCTRL readback (E6). ACTIVE always reads 0 and LVLPRI is never updated (low priority).
    10. Write-back BTCNT: the model writes it after every beat. The DS writes it only on loss of
        arbitration, suspend or disable (E5, O2).

## 6. Silicon test plan (bare EV15N46A, no wiring)

One probe image in the style of `tcprobe.c`: `firmware/hello/tests/dmacprobe.c`, built with
`make -C firmware/hello APP=dmacprobe BOARD=pic32cm_mc00_cpro`. It runs the same on the twin
(`--machine pic32cm-mc00-cpro`) and on silicon, and prints over the VCOM (SERCOM0, PA06 TX / PA07 RX,
115200 8N1, 48 MHz from `boards/pic32cm_mc00_cpro/board.c`). It ends with `PP:EXIT 0`. Common scaffolding:

- `desc[12]` and `wb[12]` aligned to 16 in SRAM. `wb[]` is pre-filled with 0xCCCCCCCC before every
  experiment, so it is visible what the DMAC writes and when. Destination buffers are pre-filled with
  0xA5 and have guard words on both ends.
- `dmac_restart()`: CTRL = 0, wait for DMAENABLE = 0, CTRL = SWRST, BASEADDR, WRBADDR, CTRL =
  DMAENABLE | LVLEN0-3. The DMAC NVIC line stays **disabled** so flags stay visible (except E25).
- `dump(ch)` prints one line: CHID-selected CHCTRLA, CHCTRLB, CHINTFLAG and CHSTATUS, then BUSYCH,
  PENDCH, INTPEND, INTSTATUS, ACTIVE and the 4 words of `wb[ch]`.
- Cycle counts come from SysTick (24-bit, CPU clock) around a transfer, so the twin can later get timing.
- Pacing needs no wiring: TC0 overflow (TRIGSRC 0x15; TC0 at 0x42003000, its GCLK channel and
  MCLK.APBCMASK bit from the DFP header) at roughly 1 kHz, or SERCOM0 TX DRE (TRIGSRC 0x03, DATA at
  0x42000428), which paces one byte per about 87 µs at 115200.
- Run order: E21 (part 2, the access-error case) goes last, behind a HardFault handler that records
  the fault and skips the faulting store. DBGRUN stays 0. Do not halt in the debugger mid-experiment,
  because a halt freezes the DMAC (§24.6.6).

| # | Experiment | What to print | DS prediction (or guess) | Settles |
|---|---|---|---|---|
| E0 | **Reset values and masks.** Read every DMAC register after reset, and the channel window for CHID 0-15. Then, with the DMAC disabled, write all-ones to CRCCTRL, PRICTRL, QOSCTRL, DBGCTRL, BASEADDR, WRBADDR, CHID, CHCTRLB (CMD = 0) and CHCTRLA.RUNSTDBY, read back, and restore | values and masks | all 0 except QOSCTRL 0x2A. Masks: CRCCTRL 0x3F0F (reserved CRCSRC 0x02-0x1F?), PRICTRL 0x8F8F8F8F, QOSCTRL 0x3F, CHID 0x0F, CHCTRLB 0x00C03F7F, BASEADDR/WRBADDR 0xFFFFFFFF or low bits forced 0? | generated table, O6, O8 |
| E1 | **Memory-to-memory copy** of 64 bytes, software trigger, TRIGACT BLOCK, SRCINC and DSTINC, for BEATSIZE byte, halfword and word. Also 1 KB word copy timing | destination match, `wb[ch]`, CHINTFLAG, CHSTATUS, ENABLE, SWTRIGCTRL read right after the write, cycles | the copy matches. Write-back = descriptor with VALID cleared and BTCNT 0. Addresses unchanged (guess). TCMPL only if BLOCKACT INT (E8). ENABLE = 0. SWTRIGCTRL reads 0 | basic engine, §3.5 |
| E2 | **STEPSEL/STEPSIZE matrix**: SRCINC with STEPSEL = SRC, STEPSIZE X2 and X4. DSTINC with STEPSEL = DST, X2. STEPSIZE X4 set on the non-incrementing side. All three beat sizes, BTCNT 8, end addresses computed with 2^BEATSIZE bytes | which source and destination bytes moved (index list) | ADDR = START + BTCNT × 2^BEATSIZE × 2^STEPSIZE on the selected side (§24.5.2.7). With the (BEATSIZE+1) formula, word beats would start 8 bytes off | the §24.7.3 formula error |
| E3 | **Fixed addresses**: SRCINC = 0 (fill from one word) and DSTINC = 0 (last value wins) | destination contents | the fixed address is the field value | §24.7.3 |
| E4 | **BTCNT = 0**: byte beats, SRCINC = DSTINC = 0 into a scratch word (harmless even at 65536 beats) | TCMPL, write-back BTCNT, cycles | not stated. Guess: 65536 beats (around 65536 × 2-3 cycles), since "256 KB in a single block" = 65536 word beats | O3 |
| E5 | **Write-back timing**: (a) enable a channel whose TC0 trigger is stopped and dump `wb[ch]`. (b) TRIGACT BEAT, BTCNT 8, three software triggers, dump `wb` and ACTIVE (ID, ABUSY, BTCNT). (c) run one block on another channel, dump `wb` of the first again. (d) finish, dump again | `wb[ch]` after each step, ACTIVE | (a) still 0xCC (fetch on the first grant, §24.5.2.5). (b) BTCNT maybe still stale at 8, while ACTIVE.BTCNT = 5 with ABUSY. (c) 5 after the switch. (d) VALID 0, BTCNT 0 | twin fetch timing, O2, Harmony transferred count |
| E6 | **Software trigger semantics**: (a) SWTRIG on a disabled channel, then enable. (b) Enabled channel with TRIGACT BEAT: one SWTRIG, read SWTRIGCTRL at once. (c) Two SWTRIGs back to back during a slow TC-paced transaction on the same channel. (d) SWTRIG on a suspended channel | SWTRIGCTRL, PENDCH, CHSTATUS, beats moved | (b) reads 0, PEND set and then cleared. (c) the second stays as SWTRIGx = 1 (queued) and extras are lost. (a) and (d) are not stated. Guess: (a) ignored, (d) PEND kept | §3.5, O4 |
| E7 | **Trigger actions with three linked descriptors** (4 beats each, BLOCKACT INT), TRIGACT BEAT, BLOCK and TRANSACTION, software triggers one at a time | triggers needed per block, TCMPL after each block, `wb` (next descriptor present after each block?) | BEAT: 12 triggers. BLOCK: 3. TRANSACTION: 1. TCMPL per block. The next descriptor appears in `wb` at once (§24.5.2.5) | engine, linked lists |
| E8 | **TCMPL matrix**: {BLOCKACT NOACT, INT} × {CHINTENSET.TCMPL 0, 1} × {single descriptor, middle block of two} | CHINTFLAG, ENABLE, INTSTATUS, INTPEND | DS: TCMPL only with INT (§24.6.20/22). Guess from CircuitPython: TCMPL also at the end of the transaction with NOACT and the interrupt off, but not for a middle block with NOACT | §3.3, O1 |
| E9 | **BLOCKACT SUSPEND** on a middle block and on the last block, then CMD RESUME. Also RESUME issued *before* a suspending block (skip). | SUSP, TCMPL, CHSTATUS (FERR), ENABLE, `wb` before and after resume | middle: suspended (SUSP), RESUME continues. Last: suspended (not disabled), RESUME gives SUSP + FERR (DESCADDR 0). Skip: no suspend | §3.1, §3.6, model item 12.4 |
| E10 | **Invalid descriptor**: (a) first descriptor VALID = 0: enable, dump, software trigger, dump. (b) second linked descriptor VALID = 0. (c) then fix VALID and issue CMD RESUME | CHINTFLAG (TERR vs SUSP), FERR, ENABLE, INTPEND, `wb`. Whether (a) flags at enable or at the trigger | DS conflict: SUSP + FERR with the channel enabled (§24.5.2.8) vs TERR (§24.6.22). RESUME clears FERR | §3.4, model item 12.1 |
| E11 | **Bus error**: source in unmapped space. First check, under a fault-catching handler, that the CPU faults on the same address (candidates 0x20004000 just past the 16 KB SRAM, 0x30000000). Also the destination unmapped | CHINTFLAG, ENABLE, BUSYCH, PENDCH, `wb` BTCNT | TERR, channel disabled, BTCNT not decremented (= initial) and written back (§24.5.2.8) | model item 12.7 |
| E12 | **Abort**: (a) clear CHCTRLA.ENABLE mid-transfer (TC-paced) and read ENABLE at once and after 10 µs. (b) Two channels enabled and waiting, clear CTRL.DMAENABLE, read both CHCTRLA.ENABLE and DMAENABLE | ENABLE readbacks, `wb` BTCNT, flags | (a) ENABLE stays 1 until the beat ends, BTCNT written back, no TCMPL. (b) both ENABLEs cleared (§24.5.3.6) | §3.10 |
| E13 | **Suspend command mid-transfer**: TC-paced BEAT transfer of 32 beats. CMD SUSPEND after about 8, wait 10 TC periods, CMD RESUME | SUSP, CHSTATUS, PENDCH while suspended, `wb` BTCNT at suspend, beats moved after resume | suspends after the ongoing beat, BTCNT written back. One overflow stays pending (PENDCH set) and runs after RESUME, the rest are lost. CMD reads 0 afterwards | §3.6, model item 12.8 |
| E14 | **Status while transferring**: record 64 snapshots (CHSTATUS, BUSYCH, PENDCH, ACTIVE, INTPEND) as fast as possible during (a) a SERCOM0-TX-paced BEAT transfer of 32 bytes, (b) the same with TRIGACT BLOCK, (c) a TC-paced TRANSACTION of 3 blocks. Print the snapshots afterwards | distinct values and how often each was seen | (a) BUSY mostly 0 between beats, PEND = 1 while DRE is up. (c) BUSY set only while a block runs (BLOCK and BEAT clear it between triggers, §24.6.23). ACTIVE.ID = channel and ABUSY while active, LVLEX for the level | model item 12.6, CircuitPython `CHSTATUS == 0` |
| E15 | **TC0 overflow trigger** (TRIGSRC 0x15): TC0 COUNT16 MFRQ about 1 kHz. A BEAT channel copies an incrementing SRAM table into a buffer, then a BLOCK channel copies 4-beat blocks. Afterwards try TRIGSRC 0x1B (dmac.h's "TC0_OVF") | beats moved after N ms, TC0 INTFLAG.OVF, whether 0x1B moves anything with TC0 running (TC2 off) | one beat (or block) per overflow. OVF in INTFLAG is not cleared by the DMA acknowledge. 0x1B (= TC2 OVF per Table 24-2) moves nothing | §2.4 numbering, the TC pulse wiring |
| E16 | **SERCOM0 TX via DMA** (TRIGSRC 0x03): a 40-byte string with TRIGACT BEAT, then the same with TRIGACT BLOCK, then CPU prints of the flags | what arrives on the VCOM | BEAT: the string is intact. BLOCK: one DRE request runs the whole block back to back, so only the first one or two characters (plus the last?) arrive. Whether the twin shows the same depends on its SERCOM TX model | trigger level vs whole-block serving |
| E17 | **Priority and arbitration**: channel A samples a word S (fixed source) into a 32-entry log. Channel B writes 1..32 into S (fixed destination), both TRIGACT TRANSACTION, started by one SWTRIGCTRL write of both bits. Cases: same LVL static, same LVL round-robin, B higher LVL, A higher LVL, B's level with LVLENx = 0. Repeat with channels 0 and 5 swapped | A's log (run lengths), PRICTRL afterwards, PENDCH/BUSYCH in the LVLEN = 0 case | static: lower channel first, A's log all 0 if A is lower. Round-robin: alternating 0,1,2,... (burst = beat?). Higher LVL first. LVLEN = 0: B never runs, PEND stays. LVLPRI = last granted channel under round-robin | arbitration, burst length, model item 6 |
| E18 | **CRC, I/O mode**: CRC-16 and CRC-32 on "123456789" (byte beats) and 00..0F (byte, halfword and word beats), seeds 0 / all-ones. After each CRCDATAIN write read CRCSTATUS (busy?), CRCCHKSUM (raw?), then W1C CRCBUSY and read CRCCHKSUM again. Also CRCSTATUS right after setting CRCENABLE, before any write | the CRCSTATUS sequence, raw and final checksums, CRCZERO, CTRL.CRCENABLE readback after clearing it with CRCBUSY = 1 | CRCBUSY = 1 after each write (Harmony relies on it). Checksums per the §3.8 table: guesses 0x29B1 and 0xCBF43926. Halfword and word beats give the byte result if bytes go in LSB first. CRCENABLE reads 1 until CRCBUSY is cleared | §3.8, O11 |
| E19 | **CRC on a DMA channel** (CRCSRC 0x22, channel 2): memory-to-memory copy of the E18 buffers with byte and word beats, polling CRCSTATUS during and after. Also a DMA channel writing into CRCDATAIN with CRCSRC = I/O | CRCBUSY during and after, CRCCHKSUM, `wb` | same checksum as I/O mode. CRCBUSY = 1 while channel 2 is enabled, 0 after. CRCCHKSUM reversed and complemented after (CRC-32) | channel CRC mode |
| E20 | **Enable protection**: with DMAENABLE = 1 write BASEADDR/WRBADDR. With CRCENABLE = 1 write CRCCTRL/CRCCHKSUM. With a channel enabled (waiting) write CHCTRLB.TRIGSRC, TRIGACT, EVACT, LVL and CHCTRLA.SWRST | readbacks, and whether a fault was taken | protected bits are ignored (no fault?). LVL and CMD writes take effect. SWRST is ignored | the protection model |
| E21 | **Software reset**: (1) set PRICTRL, QOSCTRL, DBGCTRL = 1, CRCCTRL, BASEADDR, channel 3's CHCTRLB, CHINTENSET and a CHINTFLAG (from a run). Then, with everything disabled, CTRL = SWRST and read all back. Also CHCTRLA.SWRST on one channel. (2) **last**: CTRL = SWRST with DMAENABLE = 1 | readbacks, whether SWRST reads 1 for a while, the fault record | everything reset. DBGCTRL kept (§24.5.2.2) or reset (§24.6.6)? (2): "access error", probably a HardFault (guess) | O9, CTRL write handling |
| E22 | **CHID window and INTPEND**: CHID = 12..15, read and write CHCTRLB/CHINTFLAG there. Channels 2 and 5 both with TCMPL set, channel 2's interrupt disabled: read INTPEND.ID and INTSTATUS. Write INTPEND = ID 5 + TCMPL, read back. Read INTPEND with ID written to a channel with no flags | values | CHID mask 0x0F, 12-15 not stated. INTSTATUS = flag AND enable. INTPEND.ID = lowest enabled pending (5?) or lowest flagged (2?) | O5, O8 |
| E23 | **Descriptor sections**: (a) BASEADDR ≡ 8 (mod 16) with a valid descriptor there. (b) WRBADDR ≡ 8 (mod 16). (c) BASEADDR = WRBADDR: run once, then re-enable without re-arming | did it run, where the write-back landed, flags on the re-run | (a)/(b): "must be 128-bit aligned", behaviour not stated (Harmony only guarantees 8). (c) the first descriptor is overwritten (VALID 0), so the re-run gets the invalid-descriptor behaviour of E10 | O6 |
| E24 | **Events**: (a) out: CH0 EVOE with EVOSEL BLOCK, then BEAT, routed through an EVSYS channel to TC1 (TC EVACT COUNT), 3 blocks of 4 beats. (b) in: EVSYS software event (SWEVT) on a channel routed to user 5 (DMAC CH0), EVACT TRIG with TRIGSRC 0 and TRIGACT BEAT, 5 events. Then EVACT SUSPEND/RESUME | TC1 COUNT. Beats moved per event, SUSP and CHSTATUS after the suspend and resume events | (a) 3 events for BLOCK, 12 for BEAT. (b) one beat per event. The SUSPEND event suspends, the RESUME event resumes and clears SUSP | event model |
| E25 | **Interrupt line**: enable NVIC 7 and CHINTENSET.TCMPL on channels 0 and 11, run both. The handler logs INTPEND (ID first), clears through CHID/CHINTFLAG and records order | the handler log | one vector for all channels, lowest ID served first | irq_lines (1 line) |

Twin comparison: run the same image on the twin and diff the outputs line by line. Each mismatch
points at a specific model decision in the last column.

## 7. Open questions only silicon can answer

- **O1** Is TCMPL set at the end of a transaction with BLOCKACT NOACT, with and without
  CHINTENSET.TCMPL? Is it set for middle blocks with NOACT? (E8. The DS says no, CircuitPython on the
  SAM D21 implies yes.)
- **O2** When is the write-back BTCNT updated? Every beat, or only on loss of arbitration, suspend
  or disable (§24.7.2)? This decides what Harmony's `ChannelGetTransferredCount()` returns
  mid-transfer (E5).
- **O3** What does BTCNT = 0 do (E4)?
- **O4** What does a software trigger do on a disabled or suspended channel? Does SWTRIGx read back 1
  when PEND was already set (E6)?
- **O5** Does INTPEND.ID count flags whose interrupt is disabled? What does INTPEND read right after
  firmware writes ID (E22)?
- **O6** BASEADDR, WRBADDR and DESCADDR alignment: are the low 4 bits ignored, used, or is it undefined?
  Harmony's `__ALIGNED(8)` sections depend on the answer (E0, E23).
- **O7** What happens when BASEADDR or DESCADDR points into flash or peripheral space, given that fetch
  and write-back only have SRAM ports (Table 9-6)? Optional, not in the table above.
- **O8** What do the CHID values 12-15 select (E0, E22)?
- **O9** Does CTRL.SWRST reset DBGCTRL (§24.5.2.2 vs §24.6.6)? Is SWRST with the DMAC enabled a bus
  fault (E21)?
- **O10** Do the U2223 linked-descriptor errata of relatives also hit the MC00? SAM L21 15670 ("fetch
  error ... when using more than one channel and one has linked descriptors") and 15683 ("enabling a
  lower-numbered channel while a channel with linked descriptors is active can give FERR"), plus the
  PIC32CX-BZ "corrupted write-back descriptor when another channel is disabled or suspended". To
  stress-test, run E7 on channel 5 while repeatedly enabling and disabling channel 1, and check for FERR
  or corrupted `wb`.
- **O11** CRC details: bit order (reflected or not) for each polynomial, byte order within halfword and
  word beats, what CRCBUSY does in I/O mode (set on completion, or set while selected), and the
  CRCZERO timing (E18, E19).
- **O12** Is an invalid descriptor TERR, or SUSP + FERR? Does the channel stay enabled? When is it
  flagged: at enable or at the first trigger (E10)?
- **O13** Last block with BLOCKACT SUSPEND: suspended or disabled? What does RESUME do then (E9)?
- **O14** BUSY and PEND as seen by firmware between BEAT/BLOCK triggers, and ACTIVE.ID/ABUSY/BTCNT
  after a transfer ends: stale or cleared (E14)?
- **O15** Arbitration granularity within a level (beat or block) and the round-robin start point (E17).
- **O16** Does a SERCOM TX trigger with TRIGACT BLOCK drop data on silicon (E16)? It sets how strictly
  the twin must pace level triggers.
- **O17** Do the dmac.h `TRIGSRC_*_Val` macros (SAM C21 numbering) really select different peripherals
  on silicon, as Table 24-2 implies (E15)?

## 8. Answers from silicon (2026-10-20)

The desk EV15N46A ran `firmware/hello/tests/dmacprobe.c` (E0-E25) and `dmacprobe2.c`; the model
(`hw/dma/mchp_dmac_u2223.c`) follows these answers and `firmware/hello/tests/dmac.c` checks them on
both (`golden/pic32cm1216mc00048-a1/dmac-silicon.txt`). Open questions of section 7:

- **O1** TCMPL is set at the end of a transaction whatever BLOCKACT says (NOACT included, interrupt
  enabled or not); a middle block sets it only with BLOCKACT INT. CircuitPython was right.
- **O2** The write-back BTCNT is updated when the channel loses the arbiter, is suspended, disabled
  or hits a bus error, and at the end of a block; not per beat (three beats in, it still read 8).
  ACTIVE.BTCNT has the live count.
- **O3** BTCNT = 0 is 65536 beats (6.8 ms for byte beats at 48 MHz).
- **O4** SWTRIGx on a disabled channel stays set and fires when the channel is enabled. On an enabled
  one it becomes the pending request and reads 0; written while a request is pending it stays set
  (queued); a third write is lost. A software request on a suspended channel stays pending and runs
  after a RESUME command.
- **O5** INTPEND.ID is the lowest channel with an *enabled* flag set (flags whose interrupt is off do
  not count); it reads 0 when none; writing flag bits clears them in channel ID.
- **O6** The low four bits of BASEADDR and WRBADDR are kept and used (a section at 8 mod 16 works).
- **O8** CHID 12-15 read channels 8-11 and ignore writes.
- **O9** CTRL.SWRST resets everything but DBGCTRL. With the DMAC enabled: not tried.
- **O11** CRC-16 is CCITT MSB first (seed 0: 0x31C3, seed 0xFFFF: 0x29B1 for "123456789"); CRC-32 is
  the IEEE reflected one (0xCBF43926). Halfword and word beats go in LSB first. While CRCBUSY is set
  CRCCHKSUM reads the CRC-32 register bit-reversed (0x1208C43E after "1"); idle, reversed and
  complemented. I/O mode: each CRCDATAIN write sets CRCBUSY, firmware clears it by writing 1; clearing
  CRCENABLE meanwhile is ignored (not deferred). Channel mode: CRCBUSY reads 1 while the channel is
  enabled. CRCZERO: not seen set.
- **O12** An invalid descriptor raises TERR *and* SUSP, sets FERR and leaves the channel enabled and
  suspended; at enable for the first descriptor (the write-back copy is not written), at the block end
  for a linked one. A RESUME command clears FERR and fetches the failing descriptor again.
- **O13** The last block with BLOCKACT SUSPEND suspends the channel (no TCMPL, BUSY stays 1 in a
  transaction); a RESUME then fetches DESCADDR 0: TERR + SUSP + FERR.
- **O14** CHSTATUS.BUSY is 1 while a trigger action runs (between BEAT or BLOCK triggers it is 0; a
  transaction suspended between blocks keeps it); ACTIVE.ABUSY stays 1 while the last granted channel
  is inside a block, and after a bus error.
- **O15** Arbitration is per beat: round robin alternates single beats, starting after LVLPRI (0 at
  reset: channel 1 first) and leaving LVLPRI at the last granted channel; static priority runs the
  lower channel to the end first; a higher level preempts; a disabled level's request stays pending.
- **O16** A SERCOM TX trigger with TRIGACT BLOCK sends the whole block back to back and the USART
  drops all but the first characters (the twin's USART, without TX pacing, drops none).
- **O17** TRIGSRC 0x1B (dmac.h's "TC0_OVF", SAM C21 numbering) moves nothing with TC0 running; TC0's
  overflow is 0x15 (Table 24-2). TC overflow requests do not clear TC INTFLAG.OVF.
- Others: the first descriptor is fetched at ENABLE (a load right after the write, all but the very
  next, sees it). Disabling a channel mid-transfer writes BTCNT back; ENABLE reads 1 for a moment
  (not modelled). Clearing CTRL.DMAENABLE leaves every channel's ENABLE set. A RESUME issued before a
  suspending block skips that suspend. A RESUME *event* resumed a suspended channel only while it had
  a request pending from its peripheral trigger (with none, or a software one, it did nothing): a
  hypothesis that fits five cases. RESUME (command or event) does not clear SUSP. A bus error raises
  TERR, disables the channel and writes back BTCNT undecremented. Event outputs: EVOSEL BLOCK pulses
  per block, BEAT per beat. One NVIC line, lowest INTPEND.ID first.
- Unexplained: with BASEADDR = WRBADDR the first run did not clear VALID and a second enable left the
  channel with FERR and no flags (not modelled; no firmware does this).
- Test firmware lesson: read memory a DMA writes through `volatile`; the compiler hoisted a load of
  the write-back copy above a DMAC register read and made the fetch look late.

### Peripheral triggers (2026-10-21)

`firmware/hello/tests/dmaprobe.c` (X1-X5), checked by `dmatrig.c`, one beat per trigger:

- TSENS RESRDY (1) and SDADC RESRDY (38), free running, move one result per conversion and complete.
  The TSENS probe first moved nothing: GAIN resets to 0, which silicon counts as 2^24 (0.7 s per
  conversion).
- TCC0 OVF (10) moves one beat per update (100 us period: incomplete after 250 us, done by 600 us;
  CCBUF0 fed from a table reaches CC0). MC1 (12) moves one beat per compare match. MC2 (13) on a
  capture channel moves each capture out of CC2, and the read clears INTFLAG.MC2.
- The write-back BTCNT reads the descriptor's count until the end (O2), so timed reads check
  CHINTFLAG.TCMPL instead.
- A level request already set when the channel is enabled (an SDADC result left unread) moves
  within 5 us, with no DMAC access meanwhile. The twin used to wait for its 20 us batch; it now
  kicks the DMAC at the enable.
- Twin difference: TCC timings run about one GCLK_TCC cycle early (the TCC's start latency is not
  modelled, `tcc-u2213.md` 4.13); captures read one count higher.
