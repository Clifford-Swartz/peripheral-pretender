# NVMCTRL (03929) on the PIC32CM1216MC00048: research notes

Gathered for the twin. Sources, most authoritative first: silicon (section 5), the data sheet
(DS60001638E chapter 26, electrical 43.28), the DFP headers, the twin's code
(`hw/misc/mchp_sysctrl.c`, `hw/arm/pic32cm_soc.c`).

## 1. Instance facts

| | |
|---|---|
| Base | 0x41004000 |
| NVIC line | 6 |
| GCLK channel | none |
| Events | none |
| Pads | none |
| Arrays | main 0x00000000 (128 KiB, 16 regions of 8 KiB), data flash 0x00400000 (4 KiB), NVM rows 0x00800000 (user row 0x00804000) |
| Geometry | 64-byte pages, 4 pages to a 256-byte row (PARAM 0x04030800: 2048 pages, PSZ 64, 64 data flash pages) |

## 2. Register map

| Offset | Register | Bits | Notes |
|---|---|---|---|
| 0x00 | CTRLA | 16 | CMD (6:0), CMDEX (15:8, key 0xA5, reads 0) |
| 0x04 | CTRLB | 32 | RWS, MANW (reset 1), SLEEPPRM, READMODE, CACHEDIS |
| 0x08 | PARAM | 32 | read-only geometry |
| 0x0C | INTENCLR | 32 | READY, ERROR |
| 0x10 | INTENSET | 32 | |
| 0x14 | INTFLAG | 32 | READY (read-only), ERROR (W1C) |
| 0x18 | STATUS | 16 | PRM, LOAD, PROGE, LOCKE, NVME (W1C), SB |
| 0x1C | ADDR | 32 | halfword address; 16 bits on this part |
| 0x20 | LOCK | 16 | region lock bits (1 = unlocked), from the user row |
| 0x28 | PBLDATA0 | 32 | the 64-bit page buffer load register |
| 0x2C | PBLDATA1 | 32 | |

## 3. Behaviour (data sheet)

- Writes to an array load the page buffer (one page, shared by both arrays) through PBLDATA, 64
  bits at a time; crossing a 64-bit boundary resets PBLDATA to ones. Only 32-bit writes are
  allowed; 8- and 16-bit writes cause a system exception (26.5.4.3).
- ER/DFER erase a row, WP/DFWP write the page buffer to a page, EAR/WAP do so for the user row,
  LR/UR lock and unlock a region until reset, SPRM/CPRM set and clear power reduction mode
  (cleared by any AHB access too), PBC clears the page buffer, SSB sets the security bit, SCEHL
  the permanent chip-erase hard lock, INVALL invalidates the cache (26.6.1).
- A command needs CMDEX = 0xA5 and READY; otherwise PROGE. Locked regions refuse with LOCKE.
  ADDR "uses 16-bit addressing". MANW = 0 writes a page when its last word is loaded.
- BOOTPROT (user row word 0, bits 2:0; 7 = none, 6 = 512 bytes, ... 0 = 32 KiB) protects the
  start of the main array from the next reset on.
- Times (43.28, maxima): write page 2.5 ms, erase row 6 ms, chip erase 260 ms.

## 4. Silicon test plan (nvmctrlprobe.c, nvmctrlprobe2.c)

| # | Experiment | Data sheet prediction |
|---|---|---|
| E0 | Reset values | PARAM per the memory sizes |
| E1 | No key, reserved command, CTRLA readback | PROGE |
| E2 | Page buffer loads: PBLDATA, ADDR, LOAD; read before the write; DFER/DFWP | 26.5.4.3 example |
| E3 | Random-access loads | 26.5.4.3 second example |
| E4 | Writing over programmed bits | not stated |
| E5 | PBC | LOAD cleared |
| E6 | MANW = 0, full and partial page | write at the last word |
| E7 | A command while busy | PROGE |
| E8 | Repeat DFER/DFWP times | ≤ 6 ms / 2.5 ms |
| E9 | Main array ER/WP on the last row; ADDR after a load | |
| E10 | LR, ER and WP on a locked region, UR | LOCKE |
| E11 | SPRM | PRM, cleared by an AHB access |
| E12, E14 | 8- and 16-bit loads | system exception |
| E13 | ADDR written by hand, its width | 22 bits |
| P2 E1–E3 | PRM with the cache on, off, from RAM | |
| P2 E4–E6 | A command right after DFER, from flash and RAM, gaps of 0–64 cycles | PROGE |

Never probed: SSB and SCEHL (SCEHL is permanent), EAR/WAP (the user row holds the desk board's
configuration), BOOTPROT (needs a user row write), chip erase.

## 5. Answers from silicon

Runs on the desk board (EV15N46A), 2026-10-21; times in CPU cycles at 48 MHz, from SysTick over
24 bits (a main array operation stalls the CPU's flash fetches).

1. **Command keys** (E1): no key or a reserved command sets STATUS.PROGE and INTFLAG.ERROR and
   leaves CTRLA as it was; a keyed command leaves CMD readable (0x0044 after PBC), CMDEX 0.
2. **Page buffer** (E2, E3): as the data sheet's examples, word for word. Reading the array
   before the write gives the old contents. LOAD is set by a load and cleared by WP/DFWP and PBC;
   PBLDATA reads ones after a write or PBC.
3. **ADDR** (E2, E9, E13): the halfword address **from the start of the array**: 0 for a load at
   0x00400000 (the data sheet suggests 0x200000), byte address / 2 in the main array (0xFF82 for
   0x0001FF04). **Only 16 bits** (writing 0xFFFFFFFF reads 0xFFFF): the data sheet's ADDR[21:16]
   do not exist on this part. DFER through ADDR alone erases the data flash row at offset
   ADDR × 2.
4. **Programming clears bits only** (E4): 0x11111111 written with 0x0F0F0F0F gives 0x01010101.
5. **MANW = 0** (E6): READY drops at the 16th word of a page and the page is written; four words
   of the next page stay in the buffer (LOAD = 1, page still erased).
6. **Busy** (E7, P2 E4–E6): READY is 0 right after the command. A command written while one runs
   sets PROGE and ERROR and is ignored (a PBC leaves LOAD set), back to back from flash and from
   RAM with gaps of 0 to 64 cycles. In one layout of `nvmctrl.c` a PBC written from flash 30
   cycles after a DFER set no PROGE at all; the checked app therefore issues the pair from RAM.
7. **Times** (E2, E8, E9, E6): DFER 36852–36938 cycles (768 µs), DFWP 6228–6346 (131 µs), ER
   37326–37360 (777 µs), WP 8200–8216 (171 µs); LR/UR about 100 cycles including the code around
   them. A MANW = 0 data flash write took 6263 cycles in one build and 8047–8063 in later ones
   (same data, different code layout).
8. **Locks** (E10): LR clears the region's LOCK bit (0x7FFF for region 15); ER and WP there set
   LOCKE and ERROR and change nothing; a refused WP leaves LOAD set. UR restores the bit.
9. **Narrow loads** (E12, E14): a **16-bit write loads** its halfword (the data sheet says it
   faults). An **8-bit write is a bus error (HardFault) and still loads** its byte on both lanes
   of its halfword: 0x5A at offset 1 gives 0xFFFF5A5A.
10. **PRM** (E11, P2 E1–E3): SPRM sets PRM a few cycles later (from RAM, the first read right
    after SPRM shows 0, the second 1); a data read of either array clears it. From flash code the
    first STATUS read after SPRM showed PRM in three code layouts out of five, later reads never.
11. **Reprogrammed code** (nvmctrl.c exec): code written into flash, erased and written again runs
    the new version: the commands invalidate the cache.
12. **A sticky PROGE breaks the next programming**: after one run the debugger's flash algorithm
    failed to erase sector 0 ("result code 0x1") with STATUS.PROGE set, because pyOCD halted the
    part without a reset. `harness/hw_run.py` now resets and halts before programming.

## 6. Open questions

- What sets the PROGE that broke programming in 5.12; leaving PRM set did not.
- Why the MANW = 0 write time moved with the build (5.7), and why one build's PBC-while-busy set
  no PROGE (5.6).
- EAR, WAP, SSB, SCEHL, BOOTPROT, and an out-of-range data flash ADDR (the model wraps it) are from
  the data sheet only.
- The user row's ADDR for EAR/WAP (the model takes the user row whatever ADDR says, and the page
  from ADDR's low bits).
