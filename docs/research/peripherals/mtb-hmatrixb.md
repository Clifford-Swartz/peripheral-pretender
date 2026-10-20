# MTB and HMATRIXB on the PIC32CM MC00: do they need a model?

Status: research notes, 2026-10-20. Nothing was run on hardware for these notes. Device:
PIC32CM1216MC00048 (rev A1 on the EV15N46A). Twin state as of pp-qemu on QEMU 11.1.2. Both
peripherals are register-only today: the generic `mchp_regblock` with the ATDF tables, plus
`overlays/mtb_u2002.yaml`, `overlays/hmatrixb_i7638.yaml` and the HMATRIXB reset values in
`profiles/pic32cm1216mc00048.yaml`. The primary source for MC00 facts is the data sheet
DS60001638E (PDF). The errata DS80000905F (rev F, 03/2023) has no MTB or HMATRIX entries and no
data sheet clarifications. No SAM D21 text was used for MC00 facts.

Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

Markers: **[TRM]**, **[DS]**, **[ATDF]** and **[silicon]** cite sources (listed at the end).
**(inference)** means reasoned from sources but not stated in them. **(guess)** means unverified.

## 1. Recommendations

| peripheral | verdict | why | what to change now |
|---|---|---|---|
| MTB (U2002) | **Keep register-only for now.** Model trace later, only if a use case shows up. | Trace off is exactly register-only, and that is what almost all firmware does. Firmware that turns trace on still runs correctly on the twin. It only loses the contents of its own trace buffer, and POSITION never moves. QEMU already gives users richer execution trace than the MTB. | (1) MASTER reset 0x00000080 (SFRWPRIV resets to 1 per TRM; silicon reads 0x8D). (2) Once confirmed on silicon: make POSITION/FLOW bits at and above AWIDTH, and ITCTRL/CLAIM*/LOCKACCESS, read-as-zero/write-ignored. (3) Make the "write not modelled" note explicit when MASTER.EN or TSTARTEN is set. (4) Optional, cheap: wire EVSYS users 42/43 (MTB_START/STOP) to MASTER.EN. |
| HMATRIXB (I7638) | **Register-only is the correct and complete model.** | Every field changes bus arbitration, burst splitting, default-master latency or SRAM QoS: timing only. QEMU has no bus arbitration and no access latency. Nothing else is visible to firmware except reading values back. MRCR (remap) is the one unknown, and nothing writes it. | Nothing functional. The generic regblock already logs each write as `LOG_UNIMP ... not modelled`. Optional: add overlay `note:`s ("timing only"). Optional silicon check of implemented-bit masks and of the PRS[4..7] gap. |

## 2. MTB: Micro Trace Buffer (Arm CoreSight MTB-M0+, Microchip IP U2002 1.0.0)

### 2.1 On this device

- Registers at 0x41008000, a 4 KB slot on HPB1 (APB bridge B) **[DS Table 9-2, 9.3.2]**. PAC
  peripheral id 36 (bridge B bit 4), not write-protected at reset. No interrupt, and no AHB/APB
  clock enable ("N/A"); the MCLK has no mask bit for it **[DS Table 10-1, Table 19-1; ATDF]**. The
  CoreSight ROM table in the DSU points to it: ENTRY1 0x5002 (ATDF) gives 0x41003000 + 0x5000 =
  0x41008000; silicon reads 0x5003 (present) **[ATDF, silicon]**.
- **Trace SRAM = the system SRAM.** BASE reads 0x20000000 **[silicon]**. HSRAM is
  0x20000000-0x20003FFF (16 KB) **[ATDF]**. There is no dedicated trace RAM: software puts a
  2^(MASK+4)-byte circular buffer anywhere in SRAM, aligned to its own size, and must keep
  variables, stack and heap out of it **[TRM 3.4.2; MPLAB X UG 9.7]**. The buffer can be 16 B to
  16 KB if AWIDTH = 14 (guess, see 2.2).
- Trace writes use their own SRAM port 9 ("Direct"). The CPU reaches SRAM through the bus matrix
  (port 0). Figure 9-1 lists the MTB, DMAC write-back and DMAC fetch as "privileged FlexRAM-access
  hosts". The MTB has a fixed QoS level of HIGH **[DS 9.4.2 Figure 9-1, Table 9-6; 9.4.3]**.
- External start and stop: EVSYS users 42 START and 43 STOP **[DS Table 10-1; ATDF `MTB_START`,
  `MTB_STOP`]**. These drive the MTB's TSTART and TSTOP inputs (inference from the names and the
  TRM interface). On JH01
  they are users 43/44 **[ATDF JH01]**. The Arm DWT-based start/stop is an NXP Kinetis vendor
  block (MTB_DWT). The ARMv6-M DWT defines no trace-trigger function (inference), so on Microchip
  parts EVSYS is the external start/stop path.
- The M0+ on MC00 (r0p1) has unprivileged/privileged support, an 8-region MPU, 2 watchpoint and 4
  breakpoint comparators, and halting debug **[DS 9.1.1 Table 9-1]**. The
  silicon MASTER reset value has SFRWPRIV = 1, and that bit is RAZ/WI unless the MTB's
  user/privileged option is built in **[TRM 3.4.2]**. So this MTB has privilege support
  (inference from silicon 0x8D).

### 2.2 Register map: ATDF vs TRM vs silicon vs twin

Silicon column: `golden/pic32cm1216mc00048-a1/reset.json` (pyOCD, after SYSRESETREQ, 2026-10-13).

| off | register | ATDF | TRM reset / behaviour | silicon | twin now (`mtb_u2002.h`) | gap |
|---|---|---|---|---|---|---|
| 0x000 | POSITION | RW, no initval; WRAP 0x4, POINTER 0xFFFFFFF8 | POINTER[31:3], WRAP[2] UNKNOWN; **bits >= AWIDTH are RAZ/WI**; hardware increments POINTER and sets WRAP | 0x00003FF8 | reset 0, impl/wmask 0xFFFFFFFC | twin keeps all 30 bits. Silicon probably keeps only [13:2] (AWIDTH 14, guess from the reset value and 16 KB SRAM) |
| 0x004 | MASTER | RW, initval 0; MASK 0x1F, TSTARTEN 0x20, TSTOPEN 0x40, SFRWPRIV 0x80, RAMPRIV 0x100, HALTREQ 0x200, EN bit 31 | EN 0, HALTREQ 0, RAMPRIV 0, **SFRWPRIV 1**, TSTOPEN 0, TSTARTEN 0, MASK UNKNOWN; hardware updates EN and HALTREQ | 0x0000008D | reset 0, impl 0x800003FF | **reset should be 0x80** (ATDF initval is wrong). MASK width unknown |
| 0x008 | FLOW | RW, initval 0; AUTOSTOP 0x1, AUTOHALT 0x2, WATERMARK 0xFFFFFFF8 | WATERMARK UNKNOWN (same format as POINTER), AUTOHALT 0, AUTOSTOP 0 | 0x000027F0 | reset 0, impl 0xFFFFFFFB | WATERMARK width unknown (probably AWIDTH-limited too, guess) |
| 0x00C | BASE | R, no initval | SRAMBASEADDR tie-off | 0x20000000 | 0x20000000 (overlay) | none |
| 0xF00 | ITCTRL | RW, no fields | **not in the TRM's CoreSight register table** | 0 | 32-bit storage | probably RAZ/WI (guess) |
| 0xFA0 | CLAIMSET | RW, `set:CLAIMSET` | **not in the TRM table** | 0 | set-alias, 32-bit storage | CoreSight: CLAIMSET reads the implemented claim bits, so 0 means no claim tags and RAZ/WI (inference) |
| 0xFA4 | CLAIMCLR | RW, `clear:CLAIMCLR` | not in the TRM table | 0 | clear-alias of CLAIMSET | as CLAIMSET |
| 0xFB0 | LOCKACCESS | RW | 0x00000000 | 0 | 32-bit storage | LAR is write-only in CoreSight, and with LOCKSTATUS = 0 there is no lock. Reads probably return 0 (guess) |
| 0xFB4 | LOCKSTATUS | R | 0 (no lock mechanism) | 0 | 0 | none |
| 0xFB8 | AUTHSTATUS | R | [3]=1, [2]=NIDEN or DBGEN, [1]=1, [0]=DBGEN | 0xF (debugger attached) | 0xF | constant in the twin. Silicon may read 0xA without a debugger (guess) |
| 0xFBC | DEVARCH | R | 0x47700A31 | 0x47700A31 | 0x47700A31 | none |
| 0xFC8 | DEVID | R | 0 | 0 | 0 | none |
| 0xFCC | DEVTYPE | R | 0x31 (basic trace router, trace sink) | 0x31 | 0x31 | none |
| 0xFD0-0xFDC | PID4-PID7 | R | 0x04, 0, 0, 0 | same | same | none |
| 0xFE0-0xFEC | PID0-PID3 | R | 0x32, 0xB9, 0x1B (r0p1), ECOREVNUM << 4 | 0x32, 0xB9, 0x1B, 0 | same | none (part 0x932, Arm JEP106 0x3B; pyOCD names it "MTB M0+") |
| 0xFF0-0xFFC | CID0-CID3 | R | 0x0D, 0x90, 0x05, 0xB1 | same | same | none |

The TRM requires 32-bit access to the SFRs; byte or halfword access is UNPREDICTABLE **[TRM 2.2.2]**.
`tests/golden/test_mc00_silicon.py` exempts POSITION, MASTER and FLOW as "trace state, UNKNOWN at
reset". That is right for POINTER, WRAP, MASK and WATERMARK, but MASTER bit 7 is defined. The
exemption could become a mask (compare MASTER & ~0x1F).

### 2.3 Behaviour (TRM DDI 0486B)

- **Packets.** When enabled, every non-sequential PC change writes a two-word packet at
  POSITION.POINTER, which then advances by 8 **[TRM 2.3.1, 3.4.2]**:
  - even word = source address[31:1] | **A** (bit 0). A = 0: the branch came from an
    instruction, and the source is the branch instruction's address. A = 1: the branch came from
    an exception (or a PC write in Debug state), and the source is the exception's preferred
    return address. For example, after an SVC the source is the next instruction.
  - odd word = destination[31:1] | **S** (bit 0). S = 1 marks the first packet after trace
    (re)starts, including after DBGEN/NIDEN come back high.
  - An exception return makes two packets: (BX/POP address -> EXC_RETURN, A=0) then (EXC_RETURN ->
    resume address, A=1).
  - "The processor can cause a trace packet to be generated for any instruction" **[TRM 2.3.1]**.
    Which non-branch instructions do this on the M0+ is not specified (open; see the test plan).
- **POSITION, MASK, WRAP.** MASK sets the buffer size 2^(MASK+4). When the increment would pass
  that boundary, POINTER[MASK:0] (address bits [MASK+3:3]) clear, WRAP sets, and the higher
  POINTER bits stay. MASK values above AWIDTH-4 act as the maximum **[TRM 3.4.2]**. Debug agents
  read the write address as BASE + ((P + 2^AWIDTH - BASE mod 2^AWIDTH) mod 2^AWIDTH) **[TRM 3.4.1]**.
  **Discovery** (what debuggers do): write all ones to POINTER, read it back, AWIDTH = (number of
  ones) + 3 **[TRM B.1]**. On the twin today that gives AWIDTH = 32.
- **FLOW.** When POINTER == WATERMARK: AUTOSTOP clears MASTER.EN, and AUTOHALT sets
  MASTER.HALTREQ. A trace stopped by the watermark cannot restart until software moves POINTER or
  clears AUTOSTOP. TSTART does not restart it **[TRM 2.3.2, 3.4.3]**.
- **Start and stop.** Software writes to EN. TSTART (with TSTARTEN) sets EN, and TSTOP (with
  TSTOPEN) clears it; TSTART wins a tie. Priority order: watermark stop, then software write,
  then TSTART/TSTOP **[TRM 2.3.2]**. Software must set up POSITION and FLOW before enabling, or
  behaviour is UNPREDICTABLE **[TRM 3.3.1]**.
- **HALTREQ.** HALTREQ drives EDBGRQ (a debug halt request) when DBGEN is high **[TRM 3.4.2, table
  2-1]**. It only halts the core if halting debug is enabled (a debugger attached) (inference).
- **Privilege.** With SFRWPRIV = 1 (the reset value), unprivileged writes to the MTB SFRs are
  ignored; reads are always allowed. With RAMPRIV = 1, unprivileged accesses to the MTB SRAM
  read as zero and writes are ignored **[TRM 3.4.2]**. On MC00 the CPU reaches SRAM through the
  bus matrix (SRAM port 0), while the MTB has its own port 9 **[DS 9.4.2]**. So CPU SRAM
  accesses may not pass through the MTB's AHB-Lite SRAM bridge at all, and RAMPRIV may have no
  effect here (inference, open).
- **Reset domain.** The TRM resets the MTB on RESETn: the integrator ties it to power-on reset
  if trace must survive a CPU reset, otherwise to HRESETn **[TRM A.2]**. CircuitPython's SAMD21
  port clears MASTER on every boot because "switching between debug and non-debug builds can
  leave it set over reset and wreak havoc" (`ports/atmel-samd/supervisor/port.c`). So on SAMD21
  the MTB survives at least some warm resets, and trace keeps writing into SRAM. MC00 is unknown
  (open); the non-zero POSITION/MASK/FLOW values after SYSRESETREQ fit either way.
- **Timing.** A trace write takes priority over a CPU SRAM access in the same cycle and can add
  wait states **[TRM 2.2.2]**. Each branch costs 8 bytes, about 875 instructions per KB
  (Dhrystone) **[TRM 2.3.2]**. Timing only.

### 2.4 What register-only gets right and wrong

Right: with trace disabled (the reset state, and nearly all firmware), every MTB behaviour is
register storage. The CoreSight identification and the BASE discovery path are correct, and so
are ROM-table discovery through the DSU and all reset values except MASTER bit 7. The trace SRAM
is ordinary RAM in the twin, as on silicon.

Wrong or missing. Firmware can observe these, ordered by likelihood:

1. **POSITION never advances and the buffer is never written** when EN = 1. Firmware that
   records its own crash history (CircuitPython debug builds on SAMD21; the "flight recorder"
   pattern) runs normally but reads back an empty or stale trace. A GDB script that decodes the
   buffer through the twin's gdbstub (Adafruit `micro-trace-buffer.py`, the `mtb` command) shows
   nothing.
2. **FLOW.AUTOSTOP/AUTOHALT never fire**, so EN stays 1 and HALTREQ never sets.
3. **Implemented-bit masks.** The POSITION/FLOW high bits, ITCTRL, CLAIMSET/CLAIMCLR and
   LOCKACCESS read back what was written. Silicon probably returns 0. This breaks the TRM
   discovery algorithm (AWIDTH 32 instead of 14) and any absolute-address value written to FLOW,
   as CircuitPython's older code wrote.
4. **MASTER reset** 0x00 vs 0x80. Shows in a register dump, nothing else.
5. **EVSYS MTB_START/MTB_STOP** do not change MASTER.EN. The twin's EVSYS pulses an unconnected
   user output: the user counts as ready and nothing happens.
6. **SFRWPRIV.** Unprivileged writes are not ignored. Only matters for unprivileged code that
   writes the MTB, which is rare.
7. **Warm-reset retention** (if silicon keeps the MTB across SYSRESETREQ/WDT): the twin resets
   the registers with the system.

### 2.5 Who uses the MTB

- **MPLAB X IDE**: an "MTB Trace" tab for Cortex-M0+ parts. The trace buffer is user-configured
  in device RAM, and the debugger reads the frames when the program stops **[MPLAB X UG 9.7]**.
  Real hardware only. MPLAB X cannot attach to the twin.
- **IAR C-SPY, SEGGER, iSYSTEM winIDEA, NXP MCUXpresso/LPCXpresso**: MTB trace views. Same
  limitation.
- **pyOCD**: identifies the component during ROM-table discovery (`coresight/component_ids.py`:
  part 0x932, devtype 0x31, archid 0x0A31 -> "MTB M0+") but has no trace driver.
- **Firmware**: CircuitPython `atmel-samd`, `ENABLE_MICRO_TRACE_BUFFER` (SAMD21 debug builds).
  It uses a 1 KB buffer (`mtb[256]`) aligned to its size, POSITION = buffer offset, MASTER =
  EN | 6, and turns trace off in `HardFault_Handler`. This is not relevant to the PyKit's SAM E51,
  which is a Cortex-M4 with no MTB. Nothing in this repository (firmware/, examples/, Harmony
  output) touches the MTB or HMATRIX.

### 2.6 Can QEMU see every branch cheaply?

Yes. In TCG every non-sequential PC change is an explicit code-generation site, and every taken
branch ends a translation block (TB). Three ways in:

| option | how | cost while tracing | cost with trace off | effort |
|---|---|---|---|---|
| A. TCG plugin (no fork changes) | A per-TB exec callback records the last instruction of each TB at translation time; a non-sequential next TB is a packet. `qemu_plugin_register_vcpu_discon_cb` (QEMU 11.1 plugin API v7) reports exceptions and interrupts with from/to PC. M-profile calls it from `arm_v7m_cpu_do_interrupt`. Packets go out with `qemu_plugin_write_memory_hwaddr`. | one callback per TB (guess: 1.5-3x slower) | the same: callbacks are registered whether or not EN is set. The plugin cannot read device state, so it would have to poll MASTER through `qemu_plugin_read_memory_hwaddr` | ~300 lines outside QEMU. Needs `--enable-plugins`: the pp-qemu build has `CONFIG_PLUGIN` off. The exception-return two-packet form needs EXC_RETURN detection. Awkward as a "twin model" |
| B. In-tree model (recommended if trace is ever modelled) | A `mchp_mtb` device owns POSITION/MASTER/FLOW. A TB flag (in the M-profile hflags) is set while MASTER.EN = 1. With the flag set, the Thumb translator emits `helper_mtb_branch(src, dst)` on the taken path of B/B<cond>/BL (`gen_jmp_tb`), BX/BLX/POP {pc}/LDR pc (`gen_bx`, `gen_bx_excret`) and writes to the PC (`store_reg(s, 15, …)`). `v7m_exception_taken` and `do_v7m_exception_exit` (`m_helper.c`) add the A=1 packets. The helper stores 8 bytes through `address_space_stl_le`, so TBs held in SRAM are invalidated correctly, advances POINTER with MASK/WRAP, and applies the watermark. HALTREQ -> `qemu_system_debug_request()` when a GDB client is attached. A change to EN rebuilds hflags (`arm_rebuild_hflags`). | one helper call per taken branch (guess: 1.3-2x slower on branchy code) | **zero** (TBs translated without the flag) | ~400-600 lines (guess): the device plus a target/arm patch of about 100 lines. Deterministic under icount. Fork maintenance cost in target/arm |
| C. Turn off TB chaining while EN = 1 and compare each TB's start with the previous TB's end in `cpu_exec` | Hook in `accel/tcg` | disabling chaining costs 2-3x (guess) | zero | small, but touches generic accel code, and recovering the last instruction's address needs Thumb decoding. Not recommended |

Is a minimal model acceptable? Yes. Trace disabled = register-only is exact. With trace enabled,
the twin's only failure is the trace content itself, which no functional firmware path depends on.
For users who want execution trace, QEMU already offers more than the MTB: `-d exec,nochain`,
`-d in_asm`, the gdbstub, or a stock plugin such as `execlog` (needs plugins enabled). The MCP
server or VS Code extension could expose that as a branch trace without modelling the MTB.
Option B becomes worth doing in two cases: customer firmware reads its own MTB buffer (a crash
recorder, or a self-test that compares control flow), or the harness wants twin-vs-silicon
branch traces from the same firmware.

### 2.7 Concrete twin changes (MTB), in order

1. Overlay `mtb_u2002.yaml`: `reset_overrides: MASTER: 0x00000080` [TRM]. Narrow the golden-test
   exemption for MASTER to the MASK bits.
2. After the silicon check (2.8 R1): implemented bits per device. AWIDTH depends on the SRAM size:
   MC00 16 KB, probably 14; JH01 64 KB, probably 16 (guess). So it belongs in the device profile.
   The overlay schema can only drop whole fields from `wmask` (`ro:`). It needs a new key (for
   example a profile `impl_overrides: {MTB: {POSITION: 0x00003FFC, FLOW: 0x00003FFB}}`), or a small
   `mchp_mtb` model that masks the values. Make ITCTRL/CLAIMSET/CLAIMCLR/LOCKACCESS RAZ/WI if
   silicon confirms. `read_only: true` would do it, but it logs a guest error on every claim write.
3. Make the existing generic note explicit: the regblock already logs `MTB.MASTER: write 0x…
   not modelled` (`LOG_UNIMP`, at most 8 per register). A specific "MTB trace not modelled: the
   trace buffer stays unchanged" when EN or TSTARTEN goes to 1 tells a user why their trace is
   empty.
4. Optional and cheap, if a small `mchp_mtb` model is written for step 2: connect EVSYS user
   outputs 42/43 to set or clear EN (TSTARTEN/TSTOPEN), and ignore unprivileged writes while
   SFRWPRIV = 1 (`attrs.user`).
5. Only if a use case appears: option B. Do not model RAMPRIV until silicon shows it does
   anything (2.8 R6).

### 2.8 Silicon test plan (bare EV15N46A, VCOM only)

A probe app `firmware/hello/tests/mtbprobe.c` that prints and checks nothing, as `tccprobe` and
`evprobe` do. It is built for both the twin and silicon, and the silicon transcript is kept in
`golden/`. It needs the owner's go-ahead to program the board (CLAUDE.md). Rounds run in order of
risk; each restores MASTER = 0x80 at the end.

- **R1 Discovery and masks (safe).** Print POSITION/MASTER/FLOW as found. Then write and read
  back each of these, with EN = 0 and HALTREQ never set:
  - POSITION = 0xFFFFFFF8. Prediction: 0x00003FF8, AWIDTH 14 (guess).
  - FLOW = 0xFFFFFFF8 (AUTOSTOP/AUTOHALT clear).
  - MASTER = 0x000001FF: MASK width, TSTART/STOPEN, SFRWPRIV, and RAMPRIV implemented?
  - ITCTRL = 1, CLAIMSET = 0xFFFFFFFF (read CLAIMSET and CLAIMCLR), LOCKACCESS = 0xC5ACCE55 (read
    it and LOCKSTATUS). Prediction: all 0. Clean up with CLAIMCLR = ~0 and ITCTRL = 0.
  - AUTHSTATUS read by the firmware with no debugger session. 0xF, or 0xA if DBGEN/NIDEN follow
    the debugger (guess).
- **R2 Known loop (safe).** Use `static uint32_t buf[64] __attribute__((aligned(256)))` (MASK = 4)
  with POSITION = (uint32_t)buf - BASE and FLOW = 0. One asm block keeps the compiler out:

  ```
  str  r1, [r2, #4]      @ MASTER = EN | SFRWPRIV | 4
  bl   loop3             @ loop3: movs r0,#3; 1: subs r0,#1; bne 1b; bx lr
  str  r3, [r2, #4]      @ MASTER = SFRWPRIV | 4
  ```

  Dump POSITION and every packet as (src - &loop3, dst - &loop3, A, S). Prediction: 4 packets:
  BL -> loop3 with S=1, bne -> 1b twice, bx lr -> the return address. POSITION advances 32 and
  WRAP = 0.
- **R3 Wrap (safe).** As R2 with 40 iterations, which overflows the 32-packet buffer. Expect
  WRAP = 1, POINTER = start + (41 mod 32) * 8, high POINTER bits unchanged.
- **R4 Watermark (safe).** FLOW = (start + 0x40) | AUTOSTOP, 40 iterations. Expect EN = 0 and
  POSITION = start + 0x40. Then write EN = 1 again without touching POSITION/FLOW: does it stay
  0? The TRM says trace "cannot restart" until software moves the pointer.
- **R5 Exceptions and odd cases (safe).** Pend PendSV (`ICSR.PENDSVSET`) inside the traced
  block, with a `bx lr` handler. Expect (preferred return -> handler, A=1), (bx lr -> EXC_RETURN,
  A=0), (EXC_RETURN -> resume, A=1). Also trace `b .+2` (a branch to the next instruction), `isb`,
  `svc`, and a tail-chained pair of exceptions to see which give packets.
- **R6 EVSYS start/stop (safe).** Set TSTARTEN and route an EVSYS software event to USER[42]
  (follow the clock and path rules found by evprobe), EN = 0 beforehand. Expect EN = 1 after the
  event. Then TSTOPEN with USER[43]: expect EN = 0. Print CHSTATUS/INTFLAG too, to see whether the
  MTB counts as a "ready" user.
- **R7 Privilege (moderate).** Reuse `mpu.c`'s unprivileged entry and SVC exit:
  - With SFRWPRIV = 1, write FLOW unprivileged and read it back privileged. Prediction: ignored.
    Repeat with SFRWPRIV = 0.
  - RAMPRIV only as an optional sub-round. With RAMPRIV = 1 an unprivileged stack push or pop
    could read as zero and write nothing. The test would then need a stack-free asm sequence and
    an SVC handler that rebuilds the exception frame. Skip it unless the result matters.
- **R8 Warm-reset retention (moderate).** Write POSITION = 0x120, FLOW = 0x208, MASTER = 0x83
  (EN = 0), set a `.noinit` magic word and SYSRESETREQ. On the next boot print the three
  registers. Are they kept or reset? Only if they are kept, repeat with EN = 1 and the buffer in
  `.noinit`, to see whether trace runs through the reset and startup.

## 3. HMATRIXB: high-speed bus matrix (instance HMATRIXHS, IP I7638 2.1.3)

### 3.1 On this device

- At 0x4100A000 on HPB1. PAC id 37 (bridge B bit 5). APB clock MCLK.APBBMASK bit 5, AHB clock
  AHBMASK bit 4 **[ATDF]**. Silicon reads APBBMASK = 0x7 (the HMATRIXHS APB clock is *off*), yet
  the debugger read all HMATRIXHS registers with their non-zero values **[silicon]**. Reads
  therefore work without the APB clock. Writes are untested. The SAM C21 data sheet says "The
  APBB.HMATRIXHS bit need to be set first" before writing the CPU QoS **[SAM C2x DS 10.4.3]**.
- Masters: 0 CM0+, 1 DSU, 2 DMAC (data). Slaves: 0 flash, 1 HSRAM CM0+ port, 2 HSRAM DSU port,
  3 HPB1, 4 HPB0, 5 HPB2, 6 DMAC SRAM (LPRAMDMAC), 7 DIVAS **[ATDF parameters; DS 9.4.2]**. The DMAC
  fetch and write-back paths and the MTB reach SRAM directly, not through the matrix **[DS 9.4.2]**.
- SRAM QoS: DISABLE, LOW, MEDIUM, HIGH. DISABLE and LOW add at least one cycle of SRAM latency.
  Ties go to the lowest SRAM port id. MTB fixed HIGH, DSU fixed LOW. "The CPU QoS level can be
  written/read at address 0x4100A110, bits [1:0]. Its reset value is 0x0" **[DS 9.4.3]**. That is
  SFR[0]. Silicon: SFR[0] = 0 but **SFR[1] (0x4100A114) = 0x3**. The SAM C21 data sheet puts the
  CPU QoS at 0x4100A114 with reset 0x3. MC00 derives from SAM C2x, so the MC00 data sheet may name
  the wrong SFR (open, see 3.6 H4).
- The MC00 data sheet has no HMATRIX register chapter. Field meanings below come from the same
  Atmel HMATRIX IP as documented in the AT32UC3B/C/L data sheets (MCFG/SCFG/PRAS/PRBS) and the
  ATDF value groups.

### 3.2 Register map

| off | register | ATDF (fields, initval) | silicon (A1) | twin | meaning |
|---|---|---|---|---|---|
| 0x000 + 4m | MCFG[0..15] | ULBT 0x7; 0x2 | [0..2] = 2, [3..15] = 0 | same (profile) | how this master's undefined-length (INCR) bursts are split for re-arbitration: 0 infinite, 1 single, 2/3/4 = 4/8/16 beats. Masters 3-15 do not exist |
| 0x040 + 4s | SCFG[0..15] | SLOT_CYCLE 0xFF, DEFMSTR_TYPE 0x30000, FIXED_DEFMSTR 0x3C0000, ARBT 0x1000000; 0x10 | [0] 0x20010, [1] 0x20010, [2] 0x60010, [3..5] 0x10, [6] 0xA0010, [7] 0x20010, [8..15] 0 | same | per slave: maximum burst cycles before a break (16), default master (none / last / fixed: flash, SRAM-CPU and DIVAS default to the CPU, SRAM-DSU to the DSU, DMAC-SRAM to the DMAC, the bridges have none), round-robin vs fixed priority |
| 0x080 + 8s | PRS[0..3].PRAS | M0PR..M7PR nibbles; 0 | 0x333, 0x003, 0x030, 0x303 | same | per-slave master priority, used with ARBT = fixed. The non-zero nibbles match the connectivity: the flash serves all three masters, SRAM-CPU only the CPU, SRAM-DSU only the DSU, HPB1 the CPU and DMAC (inference). Priority 3 as a maximum suggests 2-bit fields (guess) |
| 0x084 + 8s | PRS[0..3].PRBS | M8PR..M15PR; 0 | 0 | same | masters 8-15 do not exist |
| 0x0A0-0x0BF | PRS[4..7] | **absent**: the ATDF has `count="4"` but SLAVE_NUM = 8 | not captured | unmapped: reads 0 and logs a guest error | **coverage gap** for slaves 4-7 (HPB0, HPB2, LPRAMDMAC, DIVAS). Read on silicon |
| 0x100 | MRCR | RCB0..RCB15; 0 | 0 | 0 | per-master "remapped address decoding". No remap is documented for MC00. Unknown, so do not write it in tests |
| 0x110 + 4n | SFR[0..15] | 32-bit; 0 | [1] = 0x3, others 0 | same | chip-specific. CPU QoS in SFR[0] or SFR[1] (see 3.1) |

Silicon matches the twin on every HMATRIXHS register: the profile overrides carry the values, and
`test_mc00_silicon.py` has no exemption here.

### 3.3 What changes, and what firmware can see

Each field changes **when** a master gets a slave: arbitration type and priority, default-master
parking (saves one cycle on the next access by that master), burst breaking (ULBT, SLOT_CYCLE),
and SRAM QoS (one cycle of latency per access at DISABLE/LOW). None of them changes **what** an
access returns or whether it succeeds. The documented semantics are all latency and throughput
**[AT32UC3 HMATRIX chapters; DS 9.4.3]**. Firmware can only see them through timing: SysTick or TC
cycle counts, DMA-vs-CPU contention, or the jitter of a control loop. That matters for a
motor-control part with DMA, but it sits in the timing tier, which the twin does not model
(icount, `docs/silicon-harness.md` §T0).

Non-timing effects, all unknown or generic:

- MRCR remap: undocumented on MC00, nothing writes it (inference: no effect, or not implemented).
- Writes with the APB clock off: may be dropped on silicon. The twin accepts them (the generic
  regblock does not model clock gating).
- PAC write-protection of HMATRIXHS (PAC id 37): `MCHP_F_PAC` is defined in `mchp_regfile.h` but
  no model uses it. This is a generic twin gap, not an HMATRIX one.

### 3.4 QEMU and bus arbitration

QEMU has none. TCG runs one vCPU at a time. A memory access is a function call that completes
instantly through the `AddressSpace` dispatch. DMA models (`mchp_dmac`) copy through
`address_space_rw` in device callbacks, between TBs. Nothing ever contends, waits or is
re-ordered by priority. The twin's only time base is icount, at 2^N ns per instruction. A
register-only HMATRIX is therefore exactly as faithful as the rest of the twin's timing. Any model
would need a cycle-level bus model first, which is out of scope and against the project's
instruction-accurate design.

### 3.5 Concrete twin changes (HMATRIXB)

- None needed for function. The `LOG_UNIMP` on writes that change timing already exists: the
  generic regblock logs `HMATRIXHS.<REG>: write 0x… not modelled` for every register, at most 8
  times each.
- Optional: overlay `note:` on SCFG, PRAS/PRBS, MCFG and SFR, for example "arbitration/QoS: timing
  only, not modelled". The note goes into `devices/<device>.json` and the report, so the VS Code
  extension and MCP server can show it.
- Optional after 3.6: masks for absent masters and slaves (MCFG[3..15], SCFG[8..15], probably
  RAZ/WI), field widths, PRS[4..7] (`count` 8 instead of the ATDF's 4, with silicon reset
  values), and which SFR is the CPU QoS.
- Side note: the **JH01 ATDF has no HMATRIXHS instance** (only the clock and PAC bits), so on the
  `pic32cm-jh01-cpro` twin 0x4100A000 is inside the HPB1 unimplemented-device range and reads 0.
  The JH data sheets document the CPU QoS at 0x4100A110 **[JH00/JH01 DS 11.4.3]**. Same verdict
  (timing only), but a JH01 profile entry would be needed for read-back fidelity.

### 3.6 Optional silicon checks (in the same probe app, low priority)

- **H1** Read 0x4100A000-0x4100A14C, including PRS[4..7] at 0xA0-0xBF and 0xC0-0xFC. This gives
  the reset values and the slave connectivity for slaves 4-7.
- **H2** Implemented bits: save, write, read back, restore. MCFG[n] = 7. SCFG[n] =
  0xFFFEFFFF (DEFMSTR_TYPE 2, FIXED_DEFMSTR all ones; avoids reserved type 3). PRAS/PRBS = ~0.
  SFR[0], SFR[1] = ~0. Do not write MRCR or SFR[2..15] (unknown function).
- **H3** Repeat one H2 write with APBBMASK.HMATRIXHS = 0 and then = 1: does the write need the
  APB clock?
- **H4** CPU QoS: SysTick-time 1000 SRAM loads with SFR[0] = 0 vs 3, then SFR[1] = 0 vs 3. The
  register that changes the count (by about one cycle per load) is the CPU QoS. Timing results
  are printed for the record only; the twin cannot match them.

## 4. ATDF and overlay coverage

- **MTB**: the ATDF has all registers the TRM lists, plus three it does not (ITCTRL, CLAIMSET,
  CLAIMCLR; silicon reads 0). ATDF errors: MASTER initval 0 (TRM 0x80); no initvals for BASE and
  the CoreSight IDs (the overlay supplies them, matching silicon); LOCKACCESS marked RW (write-only
  in CoreSight); POSITION/FLOW masks not limited to AWIDTH. Overlay status: all silicon reset
  values reproduced except the UNKNOWN fields and MASTER bit 7. Missing: MASTER 0x80, and the
  implemented masks once measured.
- **HMATRIXB**: the ATDF lists MCFG[16], SCFG[16], PRS[4], MRCR and SFR[16]. Errors and gaps:
  PRS count 4 for 8 slaves; initvals for the absent masters and slaves and for PRAS/SFR[1] are
  wrong (the profile fixes them from silicon); no field widths for 3 masters (FIXED_DEFMSTR is
  4 bits in the ATDF, but log2(3) rounded up is 2 per the UC3 HMATRIX description). Profile
  status: every captured register matches silicon.

## 5. Other findings

- **Data sheet title in the Microchip MCP**: "Low-Power, 32-bit Cortex-M0+ MCU with Advanced
  Analog and PWM" returns the **SAM D21/DA1** data sheet (GUID-22527069…; its MTB is at
  0x41006000). The memory note now says so too. The MC00 data sheet is indexed in the MCP as
  **"5-Volt Motor Control MCU"** (GUID-0D44A543…, v5). Its 9.1.1, 9.3, 9.4.2 and 9.4.3 text matches
  DS60001638E word for word. The PDF (`C:\pp\datasheets\DS60001638E-PIC32CM-MC00.pdf`) remains the
  primary source.
- The pp-qemu build has `CONFIG_PLUGIN` undefined. TCG plugins (option A, `execlog`) need
  `--enable-plugins` in `qemu/setup.sh`.
- QEMU's NVIC has no DHCSR/halting-debug model. EDBGRQ (HALTREQ/AUTOHALT) could only map to a
  gdbstub stop.

## 6. Open questions (answered by the silicon rounds above)

1. AWIDTH and the implemented POSITION/FLOW/MASK bits (R1).
2. ITCTRL/CLAIM*/LOCKACCESS: RAZ/WI? (R1)
3. Does AUTHSTATUS change without a debugger? (R1)
4. Which non-branch instructions make packets; tail-chain packets (R5).
5. Do EVSYS users 42/43 start and stop trace; is the MTB a "ready" user? (R6)
6. Does SFRWPRIV work through the APB bridge; does RAMPRIV do anything with the MTB on its own
   SRAM port? (R7)
7. Does the MTB survive SYSRESETREQ/WDT? (R8)
8. PRS[4..7], implemented HMATRIX bits, the APB-clock write rule, CPU QoS in SFR[0] or SFR[1]
   (H1-H4).

## Sources

- **[TRM]** Arm, *CoreSight MTB-M0+ Technical Reference Manual* r0p1, ARM DDI 0486B (2012):
  1.4.3, 1.5, 2.2.2, 2.2.6 (table 2-1), 2.3.1, 2.3.2, 3.1-3.4, A.2, B.1-B.3. PDF from
  documentation-service.arm.com/static/5e9065e1c8052b160876034f (the HTML at
  developer.arm.com/documentation/ddi0486/b is rendered by script and did not load).
- **[DS]** Microchip *PIC32CM MC00 Family Data Sheet* DS60001638E (2023), PDF: 9.1.1 Table 9-1
  (Cortex-M0+ configuration, p. 31), 9.1.3 Table 9-2 (address map, p. 32), 9.3 MTB (p. 34), 9.4.2
  Figure 9-1 and Tables 9-4 to 9-6 (bus matrix hosts, clients, SRAM ports, p. 35-36), 9.4.3 Table
  9-7 (SRAM QoS, p. 36), Table 10-1 (peripheral summary, MTB row, p. 37), Table 19-1 (PAC
  peripheral ids, p. 163-164), MCLK AHBMASK/APBBMASK. The same text is in the Microchip MCP as
  "5-Volt Motor Control MCU" (onlinedocs GUID-0D44A543-8D6E-4F72-814C-6F4857061C73 v5).
- **[Errata]** *PIC32CM MC00 Family Silicon Errata and Data Sheet Clarifications* DS80000905F
  (03/2023), via the Microchip MCP. It covers NVMCTRL, SERCOM I2C/USART, OSCCTRL, AC, PDEC and OSC48M:
  nothing on the MTB, the bus matrix or QoS, and "There are no data sheet clarifications to report".
- **[SAM C2x DS 10.4.3]** "32-bit Arm Cortex-M0+ with 5V Support, CAN-FD, PTC, and Advanced
  Analog": CPU QoS at 0x4100A114, reset 0x3, APBB.HMATRIXHS first. **[JH00/JH01 DS 11.4.3]**:
  0x4100A110, reset 0.
- **[ATDF]** `PIC32CM1216MC00048.atdf` (PIC32CM-MC_DFP 1.4.67): lines 351-371 (HMATRIXHS
  parameters), 399-406 (MTB instance), 1162-1163 (EVSYS users), 2049-2056 (DSU ROM table),
  2543-2640 (HMATRIXB module), 2721-2726 (APBBMASK), 2761-2820 (MTB module). JH01:
  `PIC32CM5164JH01100.atdf` (PIC32CM-JH_DFP 1.3.221).
- **[silicon]** `golden/pic32cm1216mc00048-a1/reset.json` (EV15N46A rev A1, 2026-10-13).
- AT32UC3B/C/L complete data sheets (Atmel 32059L, 32117D, 32099J): the HMATRIX chapters (MCFG
  ULBT, SCFG, PRAS/PRBS, arbitration), via the Microchip MCP.
- MPLAB X IDE User's Guide 9.7 "Micro Trace Buffer (MTB) Trace", via the Microchip MCP.
- CircuitPython `ports/atmel-samd/supervisor/port.c` (main branch, fetched 2026-10-20), and Adafruit
  "Debugging the SAMD21 with GDB: Micro Trace Buffer"
  (learn.adafruit.com/debugging-the-samd21-with-gdb/micro-trace-buffer).
- pyOCD `pyocd/coresight/component_ids.py` (GitHub, fetched 2026-10-20).
- pp-qemu: `hw/misc/mchp_regblock.c` (write logging), `mchp_regfile.c`, `mchp_evsys.c` (user
  outputs and readiness), `include/hw/mchp/gen/pic32cm1216mc00048/mtb_u2002.h`,
  `include/plugins/qemu-plugin.h` (API v7: discontinuity callbacks, hwaddr memory write),
  `target/arm/tcg/translate.c` (`gen_jmp_tb`, `gen_bx`, `gen_bx_excret`, `store_reg`),
  `target/arm/tcg/m_helper.c` (`v7m_exception_taken`, `do_v7m_exception_exit`,
  `arm_v7m_cpu_do_interrupt`), `build/config-host.h` (`CONFIG_PLUGIN` off).
