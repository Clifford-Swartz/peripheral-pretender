# How a QEMU-based digital twin of a Microchip MCU works

Written 2026-10-09 for Microchip engineers evaluating the idea: people who know MCUs, MPLAB and the
device-support files well, but not necessarily QEMU's internals. Every factual claim here comes from
the research notes in `docs/research/` (cited, checked against the QEMU 11.1 source tree and the
PIC32CM-JH DFP 1.3.221). The note names are given in brackets so you can check details.

---

## 1. What we are building, in one picture

```
 ┌──────────────── front ends (all drive the same engine) ────────────────┐
 │  VS Code extension      CI (CLI / container)      AI assistant (MCP)   │
 └───────────────────────────────┬────────────────────────────────────────┘
                                 │ run firmware, inject stimulus, read state, collect traces
 ┌───────────────────────────────▼────────────────────────────────────────┐
 │ ENGINE  (Microchip-maintained QEMU fork + launcher)                      │
 │                                                                          │
 │  Board layer:  board file ─ part models ─ pad routing ─ external parts   │
 │  ─────────────────────────────────────────────────────────────────────  │
 │  Chip layer:   Cortex-M CPU (QEMU TCG)  + NVIC/SysTick (QEMU armv7m)     │
 │                + peripheral models: clocks, PORT, SERCOM, TC, ADC, ...   │
 │                  (register tables generated from ATDF + behavior code)   │
 └──────────────────────────────────────────────────────────────────────────┘
```

- **Chip layer.** It runs the customer's unmodified ELF/HEX. The CPU is QEMU's, and the peripherals are ours.
- **Board layer.** It connects the chip's pins and buses to models of whatever the customer wired up.
- **Front ends.** VS Code, CI and the AI assistant all call the same headless engine. Nothing essential
  lives only in an IDE.

**What to call it.** "Digital twin" usually implies a board- or product-level model. In automotive,
the unmodified production binary running on a simulated MCU *plus the ECU hardware model* is a
formally defined "Level 4 V-ECU" (prostep ivip SmartSE recommendation, 2025). In everyday conversation
"virtual prototype" or "virtual platform" is the common vendor term [board-level-prior-art].

## 2. How QEMU runs a microcontroller

QEMU translates guest instructions to host code just in time ("TCG"). Everything else is a *machine*
assembled from *device models* in C:

| layer | in QEMU | for PIC32CM JH |
|---|---|---|
| Board (machine) | `MachineClass` in C: crystals, the SoC, on-board parts, firmware loading (`armv7m_load_kernel`) | `pic32cm-jh01-cpro`: 32 MHz + 32.768 kHz crystals, LED0 on PC05, SW0 on PB19, VCOM on SERCOM4 |
| SoC | a device that owns the memory map and all peripherals | flash 512 KB @0, data flash 8 KB @0x00400000, NVM rows @0x00800000, SRAM 64 KB @0x20000000, peripheral bridges HPB0-3, IOBUS alias @0x60000000 |
| CPU + NVIC + SysTick | `TYPE_ARMV7M` container, already in QEMU | `cortex-m0` used as a stand-in for the M0+ (below) |
| Peripherals | `SysBusDevice`s: an MMIO region with read/write callbacks, IRQ outputs, Clock inputs, character back ends for UARTs | 55 instances from the ATDF |

[qemu-internals]

**CPU fit.** QEMU has no `cortex-m0plus` type, but `cortex-m0` is a good stand-in [core-support-matrix]:
- VTOR is implemented on every M-profile CPU, so relocatable vector tables and bootloaders work.
- The Armv6-M NVIC caps at 32 IRQs with 2 priority bits, which is exactly the JH configuration.
- SysTick is always present.
- The M0+ MPU can be enabled through the SoC's `mpu-ns-regions` property.

Three real gaps remain:
- CPUID reads as a Cortex-M0.
- There is no unprivileged thread mode.
- QEMU has no single-cycle I/O port (IOBUS), which we model as a memory alias.

The Cortex-M23 parts (PIC32CM LE/LS, SAM L10/L11) would need a new CPU type. M3/M4F/M7/M33 families
(SAM D5x/E5x, E70/V71, PIC32CK/CZ/CX) are covered at the CPU level today. PIC32 MIPS and AVR-Dx would
need CPU work. 8/16-bit PIC and dsPIC have no QEMU target at all; MPLAB SIM covers those.

**What a peripheral model is.** A register file plus behavior:
- It decodes each MMIO read/write.
- It applies the hardware's rules (write-1-to-clear, enable protection, sync).
- It runs side effects (a DATA write transmits a byte).
- It keeps its interrupt line equal to `INTFLAG & INTEN`.
- It schedules future events on QEMU's virtual clock (timers, baud pacing).
- It exposes "knobs" for the outside world (UART bytes, pin levels, ADC input values).

**Timing.** By default QEMU runs instructions as fast as the host allows, and virtual time follows host
time. `-icount` makes time deterministic: every instruction costs a fixed power-of-two nanoseconds,
and `sleep=off` skips idle time. That enables record/replay and reverse debugging. It is
instruction-accurate, **not cycle-accurate**. Wait states, bus contention and analog behavior are out
of reach and must be documented per peripheral [qemu-internals, validation-build].

## 3. Anatomy of one model: SERCOM in USART mode

These are the rules firmware actually relies on. Harmony and Zephyr drivers hang or misbehave if any
of them is wrong [boot-path, microchip-data]:

| register | rule the model must implement |
|---|---|
| CTRLA (0x00) | MODE selects which register layout is active (USART/SPI/I2C share one base). SWRST resets and self-clears. ENABLE takes effect immediately. Most other bits are *enable-protected*: they can only change while ENABLE=0. |
| CTRLB (0x04) | TXEN/RXEN may change while enabled; the rest is enable-protected; LINCMD is a strobe that reads 0 |
| INTENCLR/INTENSET (0x14/0x16) | two views of one hidden INTEN register |
| INTFLAG (0x18) | TXC/RXS/CTSIC/RXBRK/ERROR are write-1-to-clear, **but DRE and RXC are not**: DRE clears only by writing DATA, RXC only by reading DATA or disabling RX. The ATDF marks the whole register W1C, which is wrong for these two. |
| STATUS (0x1A) | PERR/FERR/BUFOVF/ISF/COLL/TXE are W1C (the ATDF does not say so); CTS is a read-only pin level |
| SYNCBUSY (0x1C) | reads 0 in an "instant sync" model (every Harmony/Zephyr wait tolerates that) |
| DATA (0x28) | write = transmit; read = pop the 2-deep RX buffer. A debugger read must *not* pop it. |

Two consequences matter for the whole project:

1. **Register maps are generatable; behavior is not, at least not from the ATDF alone.** ATDF gives
   offsets, sizes, reset values, modes, IRQs and pin-mux tables. Behavior lives in the datasheet's
   per-register "Property:" lines and bit descriptions. There are 482 such lines for this family.
2. **A model that is plausible but wrong is worse than a missing one.** Treating INTFLAG as fully W1C
   makes a DRE-driven TX interrupt stall; treating STATUS as plain RW makes Harmony's error-clear path
   latch errors instead of clearing them [microchip-data].

## 4. Where the models come from: ATDF + behavior overlays + generator

```
ATDF (per device) ──┐
                    ├─► generator ─► register tables + reset values + IRQ/instance map + pad tables
overlay (per IP) ───┘                     │
  (W1C/RO bits, aliases, enable           ▼
   protection, sync, hooks,          QEMU models (one per IP version)  ◄─ hand-written behavior code
   reset exceptions)
```

**Why the ATDF is the right primary input.** It is the only file with the full memory map (NVM rows
and IOBUS aliases included). It is also the only one with all instances and their IP id/version, all
32 IRQs *including the 6 shared vectors*, register modes, pin-mux signals, event-system wiring, and
per-instance PAC/GCLK/DMA ids. The SVD and EDC carry identical register data; the SVD silently drops
the shared vectors, PTC and FUSES [microchip-data].

**The ATDF also has traps the generator must correct:**

| trap | consequence | correction |
|---|---|---|
| GCLK GENCTRL0 initval 0 | the datasheet says 0x00000106 (generator 0 on, from OSC48M) | overlay `reset_overrides` |
| User row default 0xFFFFFFFF | would mean an always-on watchdog and BODVDD off | twin preloads production row 0xB15088FB / 0xFFFFF8BB |
| NVMCTRL.PARAM rw=RW, initval 0 | the datasheet says read-only 0x08032000 | overlay |
| Status bits OSCCTRL.STATUS / NVMCTRL.INTFLAG initval 0 | momentary reset values, not steady state | model computes "ready" from enables |
| Serial number, calibration fields | absent from ATDF/SVD/EDC | separately supplied NVM image; dump real values from silicon |

[microchip-data, boot-path]

**Why per-IP overlays scale across the catalog.** IP blocks are reused with stable id/version tags.
PIC32CM JH and SAM C21 share id *and* version for 11 modules, SERCOM U2201/3.1.1 included, and share
the id alone for 14 more. The same SERCOM overlay applied to the SAM C21 ATDF produced an identical
table. SAM D21 differs more (SERCOM U2201/2.0.0, older clock IP), and the generator's version guard
rejects it until someone reviews the differences. Harmony's CSP is organized by the same IP ids. So the
unit of work is "one IP version", not "one device"; a new device built from known IP versions is
mostly a generated configuration [microchip-data].

**A question worth asking internally.** The datasheet "Property:" lines (Enable-Protected,
Write-Synchronized, PAC write-protection) and the ATDF clearly come from an internal register
database. If that database can export per-bit W1C/RO/enable-protect/sync/strobe metadata directly, the
overlays become generated too [microchip-data]. Likewise, MPLAB SIM's pic32sim engine already encodes
behavior for these devices; whether its models can be mined or reused is worth checking [boot-path].

## 5. What firmware needs before it prints anything

Getting to `main()` needs almost nothing. The DFP startup files copy `.data`, zero `.bss`, write VTOR
and call `main`; SystemInit is a no-op [boot-path]. The real work is between `main()` and the first
UART byte. For the Curiosity Pro's VCOM example, Harmony's `SYS_Initialize` does the following:

1. NVMCTRL wait states.
2. PORT pin mux for PB10/PB11 to SERCOM4.
3. A 64-bit read of the calibration row at 0x00806020.
4. OSC48M divider = 0 → wait for sync → **wait for OSC48MRDY**.
5. GCLK generator 0 → wait for SYNCBUSY.
6. PCHCTRL[22] = CHEN → **wait until CHEN reads back 1**.
7. MCLK mask.
8. SERCOM4 setup and enable.
9. NVIC.

[boot-path]

Two lessons follow:
- **A read-as-zero stub is not enough.** QEMU's stub for unmodelled devices satisfies "wait while
  SYNCBUSY != 0" but hangs forever on OSC48MRDY and on the CHEN read-back. Even QEMU's own nRF51 model
  needs a "return 1" clock stub. Tier 0 therefore means real register files with ready-bit behavior,
  not stubs.
- **The NVM rows must be mapped.** Harmony reads the calibration row and Zephyr reads the 128-bit
  serial number at 0x0080A00C/40/44/48. Unmapped, those reads fault before the first print.

Microchip already publishes free test firmware for this exact board:
- 38 Harmony `csp_apps` examples.
- The Harmony core, Class B and bootloader repos.
- The Zephyr `pic32cm_jh01_cpro` board, which Microchip itself upstreamed (PR #95480, merged 2025-09-13).
  At least 14 Zephyr test suites list this board.

[boot-path, core-support-matrix]

## 6. From chip to board: what QEMU gives you and what you build

What upstream QEMU provides [qemu-board-composition]:
- **Boards are compiled C.** There is no declarative board format and no devicetree-driven machine
  construction.
- **Some parts can be attached at startup** without recompiling: I2C targets and SPI flashes, via
  `-device part,bus=<named bus>,address=<n>`. Their state can then be changed at runtime with QMP
  `qom-set` (e.g. a temperature).
- **GPIO wiring cannot be expressed** from the command line (CS, INT, LED, button). Pin-mux and pad
  routing are not modeled anywhere.
- **No part can run outside the QEMU process** on an MCU-side bus.
- **Windows hosts lose** SocketCAN, vhost-user, multi-process QEMU and loadable modules.
- **Time is deterministic per instance only.** Nothing synchronizes several QEMUs.

So the board layer is ours to build:

| piece | what it does | why Microchip is well placed |
|---|---|---|
| Board file | declares the MCU part, crystals, and what is wired to which pads | MCC already records pin choices (`components/core.yml`, `pin_configurations.csv`); Microchip-authored Zephyr devicetrees describe its own boards |
| Pad routing | follows the firmware's PMUX/PINCFG writes at run time and connects SERCOM signals to whatever is wired to those pads | the ATDF has the full pad → function → SERCOM PAD table; SERCOM CTRLA captions give the PAD roles |
| Part library | functional models of off-chip parts | Microchip sells many of them: EEPROMs, temperature sensors, RTCCs, GPIO expanders, CAN controllers, PHYs. QEMU already models a few (AT24C, SST25/AT25, EMC1413/14, LAN9118). Most I2C/SPI parts are register files of 250–500 lines [microchip-board-inputs] |
| Extension SDK | customers model their own parts | an out-of-process protocol keeps customer code outside GPL QEMU (legal review needed) and lets them use Python or C |
| Time sync | keeps multi-board or plant co-simulation deterministic | needs a quantum/barrier protocol. QEMU's TCG-plugin time-control API (works on Windows) is a building block. Renode's global-quantum design is the reference. |

**Interface design.** Bus traffic should cross the board boundary as whole operations: an I2C
transfer, an SPI frame, a UART byte, a CAN frame, a pin edge, an analog sample. Pin-level signals are
only a fallback for bit-banged I/O.

QEMU's I2C API must answer inside the MMIO access. SERCOM, however, signals I2C completion through
flags (INTFLAG.MB/SB), so the SERCOM model can hand transfers to an external part and complete them
later in virtual time without blocking the CPU [qemu-board-composition].

**The competitive picture.** Antmicro's dts2repl already turns Zephyr devicetrees into Renode
platforms for about 1,170 boards; 1,063 targets pass Zephyr hello_world and 425 pass blinky. Its
PIC32CM JH01 platforms boot to the console only:
- Only the core, NVIC and one SERCOM UART are real models.
- Clock status registers are faked with constants labelled "UNK".
- PORT, EIC, the timers and the ADC are stubs.

Of 29 Microchip SAM/PIC32C boards on that dashboard, 21 pass hello_world and **0 pass blinky**
[board-level-prior-art]. Shallow third-party simulation of our parts will exist whatever we do; the
differentiator is depth. That means accurate clocks, PORT, EIC, TC/TCC, SERCOM I2C/SPI, ADC and DMAC,
plus accurate models of Microchip's own off-chip parts.

Microchip already ships one simulator in another division: Renode is integrated in SoftConsole for
PolarFire SoC and Mi-V [board-level-prior-art].

## 7. Where model code lives: integration options

| option | per-access cost | best for |
|---|---|---|
| Native QEMU device (C) | function call | everything boot-critical and common; this is the default |
| Out-of-process bridge (socket/shared memory) | ≈7.7 µs (Unix socket), ≈14 µs (TCP), ≈0.2 µs (spinning shared memory) per round trip, Linux benchmark | customer/confidential part models, plant models; low-rate buses |
| SystemC/TLM (AMD remote-port style) | as bridge | reusing SystemC models; keeps an RTL path open |
| Verilated RTL of a real IP block | ~0.1 MIPS full-SoC vs 170–1000 MIPS for functional simulation | *validating* models against the actual IP, pre-silicon; not the everyday twin |
| QEMU TCG plugins | observe only: callbacks fire after the access | MMIO tracing, register coverage, lockstep diffing; not modeling |

[cosim-options]

Recommended layering:
- **L0:** native models for boot-critical and common peripherals, with register skeletons generated
  from ATDF and behavior reviewed by people.
- **L1:** QEMU-native board I/O plus QMP.
- **L2:** a timestamped bridge for plant and confidential models, using a packet format compatible
  with remote-port.
- **L3:** optional internal Verilator co-simulation, used to validate L0.

Owning the RTL is a uniquely Microchip advantage for L3 [cosim-options].

## 8. Proving the twin matches silicon

The oracle is the chip, not the files describing it. Models generated from the same ATDF that MCC uses
will agree with MCC even when both are wrong. The DFP has provable errors too:
- SCB.CCR's reset value disagrees with its own bitfields.
- One JH00 part has a DID mismatch.
- TCC PER has the wrong width.

[validation-build]

| technique | catches | tooling |
|---|---|---|
| Golden reset dump from silicon vs generated reset values | wrong/missing initvals, fuse-loaded values | pyOCD on the on-board EDBG (CMSIS-DAP) or J-Link. Note: OpenOCD's at91samd driver does not recognize this DID series |
| Same ELF on board and twin, compare UART transcripts and GPIO activity | behavioral differences | VCOM capture (EDBG needs DTR), `PP:EXIT` protocol |
| Register snapshots at breakpoints, one GDB/MI script for both targets | state divergence | pyOCD gdbserver and QEMU gdbstub. QEMU's gdbstub reads without side effects; HMP `xp` triggers them. |
| `-d unimp,guest_errors` + MMIO trace events | unmodeled behavior the firmware touches | becomes the model backlog automatically |
| Silicon-validated test suites | the executable spec | tests run on silicon first; only passing tests define correct behavior; models iterate until they pass |
| Deterministic replay (`-icount`) | flaky timing in CI | per-instance |

[validation-build]

This is also what makes heavy AI-assisted development safe. Agents can write a large share of the
models and tests, but every model must converge against silicon-validated tests rather than against
the agent's reading of a datasheet.

**Safety-critical customers.** Using simulation results as ISO 26262 evidence makes the twin a
software tool under ISO 26262-8 Clause 11 (tool confidence and qualification). A documented
validation-against-silicon story is a prerequisite [board-level-prior-art]. Publish a per-peripheral
fidelity matrix (register-accurate? functional? timing? interrupts? DMA? known gaps), as Wokwi and
Renode do.

## 9. Constraints and open decisions

- **QEMU's AI policy.** Upstream QEMU declines contributions believed to include AI-generated content
  (`docs/devel/code-provenance.rst`; no exceptions listed). An AI-heavy build therefore means a
  Microchip-maintained fork. Mitigations:
  - Keep QEMU-facing glue small and template-driven, so API churn means "fix the template, regenerate".
  - Anything you ever want upstream (e.g. a `cortex-m0plus` CPU type) has to be written by people
    without AI help.
- **Licensing (legal review).**
  - The QEMU fork is GPLv2: shipping binaries carries source obligations.
  - ATDF data is Apache-2.0, which does not mix cleanly with GPLv2; Microchip owns the data, so this
    may be a relicensing question.
  - Out-of-process customer models are probably separate works.
  - AI-generated code has its own copyright questions.
- **Windows.**
  - It works without admin. We are building in the existing MSYS2 install's UCRT64 environment;
    MSYS2 deprecated MINGW64 in March 2026, though QEMU's docs still mention it.
  - QEMU's configure step needs symlinks; one small local patch removes that need.
  - SocketCAN (real CAN in the loop) is Linux-only, so WSL2 or Linux would be needed for that.
- **Customer demand is unmeasured.** AspenCore's 2023 study (n=655):
  - 11% use QEMU today.
  - 37% expect simulation to become more important.
  - "Virtual prototype available" was a weak processor-selection factor: 40%, against 85% for software
    tools.

  Field interviews would size board-level demand better than any survey found [microchip-board-inputs].

## 10. Effort and order

The size estimates below extrapolate from comparable in-tree QEMU models [boot-path]:

| tier | scope | size |
|---|---|---|
| 0 | boot to `main` and through Harmony init: clocks with ready bits and frequency propagation, NVMCTRL, SUPC, PM, RSTC, WDT, PAC, DSU, DIVAS, NVM rows, IOBUS alias, shared-vector OR gates, SoC + board | ≈2.2–3.5 kLOC |
| 1 | PORT, SERCOM USART, EIC, TC, RTC, EVSYS | ≈3–4.5 kLOC |
| 2 | SERCOM SPI/I2C (with board parts), ADC with injected inputs, DMAC, CAN-FD (M_CAN; no QEMU model exists), TCC, AC/DAC/CCL/PDEC | ≈6–9.5 kLOC |
| 3 | PTC (needs an internal spec), sleep/standby, timing fidelity, ECC fault injection, errata mode | open-ended |

Order, as agreed:
1. Chip twin plus the Curiosity Pro board, validated against silicon, with internal users first.
   The v0 vertical slice is in progress; see `docs/design/v0-vertical-slice.md`.
2. Board file plus 5–10 library parts.
3. Extension SDK and CI tooling.
4. Catalog scaling, multi-board, plant co-simulation.

## 11. Questions only Microchip can answer

1. Can the internal register database export per-bit W1C/RO/enable-protect/sync/strobe metadata?
2. Can MPLAB SIM's pic32sim behavior for these parts be mined (source access, licensing)?
3. Is peripheral RTL available for Verilator-based validation of selected IPs?
4. Is there an internal PTC register spec? Without one, QTouch firmware cannot run on the twin.
5. What are the real production contents of the CAL row, OTP rows, calibration row and TEMP_LOG on
   B0/C0 silicon? These can be dumped from an EV81X90A.
6. Does the PIC32CM JH Cortex-M0+ implement the unprivileged extension, and what is its exact CPUID?
7. Legal: ATDF Apache-2.0 vs GPLv2, binary distribution, AI-generated code, and the out-of-process
   boundary for customer models.
