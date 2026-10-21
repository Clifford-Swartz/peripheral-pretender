# Modelling a peripheral

You are turning a register-only peripheral (or a model with a gap) into behaviour that matches the
silicon on the desk. Work through the phases in order. Each ends at a gate; report it to the owner
in a few lines before going on. `py -m tools.periph` does the mechanical steps. You do the
judgement: what to ask the silicon, how to read its answers, what to model, what to leave out.

Read first: [oracles.md](oracles.md) (silicon outranks the datasheet, which outranks the ATDF),
the peripheral's playbook in [peripherals/](peripherals/) if there is one, and its research note
in `docs/research/peripherals/` if there is one.

Worked examples, all on the MC00 (PIC32CM1216MC00048): AC U2245, TSENS U2261, FREQM U2257,
DMAC U2223. Each has a research note with "Answers from silicon", a probe, a checked app, a
golden, a model and a qtest. When unsure how something is done, find how those four do it.

## Before you start

- **The desk board.** Record it once per machine:
  `py -m tools.periph desk mc00 --probe <EDBG serial> --port <COMn>`. Only the owner may add
  `--allow-program`; `silicon`, `probe` and `check` refuse without it. Approvals are in
  [README.md](README.md#hardware).
- **Where the gaps are.** `py -m tools.periph status mc00` lists every instance as
  `REGISTER-ONLY`, a model type, or core/NVM, plus the apps that already have a silicon golden.
  A gap inside an existing model (the WDT time-out, NVMCTRL programming) does not show there:
  read the model's header comment ("Not modelled: ...").
- **Shared pp-qemu.** Other sessions may use `C:/pp/pp-qemu`. `qemu`, `qtest` and `gate` build
  QEMU, and on Windows a build first stops every running QEMU (a running exe cannot be relinked),
  theirs included. So never build QEMU while a gate runs: it kills the gate's QEMU and fails
  tests for no reason (a web session test once died of it). `twin` and `probe` no longer stop
  anything.

## Phase 1: Research

```
py -m tools.periph scaffold mc00 <INSTANCE> <name>                  # new model
py -m tools.periph scaffold mc00 <INSTANCE> <name> --firmware-only  # model exists
```

`scaffold` writes the research note with the instance facts filled in (base, NVIC line, GCLK
channel, event generators and users, pads), the probe and checked-app skeletons, and for a new
model the model, header, qtest and overlay skeletons, wired into `hw/misc/meson.build`,
`tests/qtest/meson.build` and `pic32cm_models[]` in `hw/arm/pic32cm_soc.c`. It never
overwrites a file that exists.

Fill in the note's sections 2 and 3 from the datasheet chapter (the Microchip docs MCP), the
errata, and the existing overlay. In section 4, list every behaviour you are not sure of as an
experiment, with what the datasheet predicts. Good things to put to silicon:

- reset values and the writable bits of every register (write all ones, read back)
- which bits are enable-protected or write-synchronised, and what reads return while a sync is
  busy
- what a disable does to a running operation, a pending flag and an output
- what happens at the edges: zero counts, maximum counts, overflow, a command while busy, a
  command while disabled
- timings: start-up, conversion or transfer time, as functions of the clocks
- interrupts and events: when a flag sets relative to the event, whether a flag that is already
  set pulses again

**Gate:** sections 1–4 of the note are filled in; every experiment has a datasheet prediction
(or "not stated").

## Phase 2: Ask the silicon

Write `firmware/hello/tests/<name>probe.c`: one experiment per line, `E<n> <label> key=value ...`,
raw values only, no checks. Then:

```
py -m tools.periph probe mc00 <name>probe
```

This builds it, runs it on the desk board, runs it on the twin, and diffs the two field by field
(`E7 enable:  silicon -> twin` followed by each `key: a -> b` that differs). Before a model
exists the twin answers like a register file; that diff is the work list.

Write what silicon said into section 5 of the note ("Answers from silicon"), with the experiment
number. Where silicon contradicts the datasheet, say so, and add a numbered finding to
`docs/research/microchip-data.md` when Microchip's documentation is wrong. A second probe
(`<name>probe2.c`) for the questions the first one raised is normal.

**Gate:** every experiment in section 4 has an answer in section 5, or is listed in section 6
(open questions) with the reason.

## Phase 3: Model

Write the model in pp-qemu (`hw/<class>/mchp_<name>_<ip>.c`). The register file comes from the
generator; the overlay (`overlays/<module>_<ip>.yaml`) says which registers are `dynamic`
(`pre_read` runs on every read), which have `hooks` (`post_write`), which are `ro`, `w1c`,
`enable_protected`, and which fields the ATDF lacks (`extra_fields`). After you edit an overlay:

```
py -m tools.periph regen        # every device in one run
py -m tools.periph qemu         # build qemu-system-arm
```

Rules for the model:

- Event-driven on `QEMU_CLOCK_VIRTUAL`: arm a timer for the next thing that happens, never tick.
  Work out the state lazily in `pre_read` when the firmware looks, from the time an operation
  started.
- Timings come from the clock tree (`mchp_regblock` clock periods, GCLK channel or APB clock),
  not from constants that assume one clock.
- QEMU style: `/* */` comments, 4-space indent, 80 columns, QEMU 11.1 idioms. The file starts
  with the provenance header from the template, a summary of what silicon showed and what the
  model does, and a "Not modelled:" line.
- Every field that changes on its own (status, busy, counters) goes in the VMState.
- A CTRL register whose reset field reads as zero makes the generator assume a generic soft
  reset. When the model resets itself, say `self_clearing: []` in the overlay.

Then run the probe again: `py -m tools.periph twin mc00 <name>probe` followed by
`py -m tools.periph diff mc00 <name>probe`.

**Gate:** the diff shows only what varies between parts, supplies or temperatures (analog
values, RC oscillator rates) and UART pacing. Name each remaining difference in the note.

## Phase 4: The checked app

Write `firmware/hello/tests/<name>.c` with `check.h`. It encodes the answers from section 5.
Check behaviour exactly (`put_checked`), and part-, supply- or temperature-dependent values
within a range (`put_range`, `put_near`). End each line with `end_line()`; `test_exit()` prints
`PP:EXIT <failures>`.

```
py -m tools.periph check mc00 <name>
```

`check` builds the app and runs it on the twin and on the silicon. Both must print `PP:EXIT 0`.
The silicon transcript then becomes `golden/<device>/<name>-silicon.txt`, and
`tests/golden/test_mc00_silicon.py` runs the app on the twin in every gate from then on.

When the silicon fails, the check is wrong: it encodes a belief that the probe did not establish.
Fix the check and, where the model shares the belief, the model, and re-probe if needed. When
only the twin fails, fix the model.

**Gate:** `check` passes. Run it twice on silicon when timings are near a limit.

## Phase 5: The qtest

Fill in `tests/qtest/pic32cm-<name>-test.c` for what firmware cannot reach well: the board side
(GPIO inputs and outputs of the model, properties such as the supply or the temperature),
migration of a running operation, and exact timings on stepped virtual time (`qtest_clock_step`).

```
py -m tools.periph qtest pic32cm-<name>-test
```

**Gate:** the qtest passes.

## Phase 6: Hand-back

1. Docs:
   - the machine's `docs/system/arm/pic32cm-*.rst` in pp-qemu (the device list and what is
     modelled)
   - the peripheral playbook in [peripherals/](peripherals/), for lessons that apply to other
     models
   - `docs/handoff.md`
2. `py -m tools.periph gate` runs the Python suite and every pp qtest; it must pass. Gate on its
   exit status, not on grepping its output.
3. Commit pp-qemu with the provenance trailer, then `py -m tools.periph export` writes the next
   `qemu/patches/NNNN-*.patch`. Add its row to `qemu/README.md`.
4. Commit the repository and push.
5. Engine code changed, so the VS Code extension's bundled QEMU is stale. Bump
   `vscode/package.json`, build the VSIX (`py scripts/build_vsix.py --qemu C:/pp/pp-qemu`),
   run `cd vscode && npm test` (from a terminal inside VS Code, with `ELECTRON_RUN_AS_NODE`
   unset: `env -u ELECTRON_RUN_AS_NODE npm test`, or Code.exe rejects `--user-data-dir`),
   update the version in `SETUP.md`, and publish a release. When
   several peripherals go through the process in one session, one release after the last of
   them will do, but never report the batch to the owner before it exists.

**Gate:** the gate passes; the commit is pushed; the release exists.

## Lessons from silicon

These cost a probe run each to learn. Check new probes against them.

- **Silicon disproves confident hypotheses.** TSENS START rules and DMAC fetch timing both turned
  out unlike the datasheet's reading. Never write a check from the datasheet alone: probe first.
- **`volatile` for DMA memory.** The compiler hoisted a plain load of a DMA write-back
  descriptor above a volatile CHSTATUS read, and the probe read stale memory.
- **`delay_ms(n)` can last n − 1 ms.** A SysTick wrap may already be pending. `delay_ms(5)`
  measured 3.37 ms and broke an interrupt test. Use margins, or `clock_zero()`/`now_us()`.
- **Do not print while timing.** A UART print blocks for longer than a SysTick period; wraps
  are lost, and on the twin the print pacing differs from silicon.
- **A dead clock source hangs the sync.** A GCLK generator on a source that does not run (the
  desk board's XOSC32K) never finishes its sync, and PCHCTRL.CHEN never reads back on a
  disabled generator. Bound every wait, and give each doubtful source its own generator.
- **Know the desk board.** On the MC00 desk board, XOSC32K gives no clock (machine option
  `xosc32k=off`), the 12 MHz XOSC runs, and VCC is about 4.89 V (machine option `vcc=4890`).
- **Integer-Hz clocks round.** Generators are period-based (`mchp_regblock_set_clock_period`):
  an integer-Hz divider chain gave FREQM 0xFE4B3E where silicon gave 0xFE0000.
- **Probes that reset the part** keep their state in `keep_area()` (`check.h`), arm before the
  write that should reset, and finish the line on the next boot (`wdtprobe.c`, `wdt.c`). Arm
  every experiment that might reset, not only those expected to: the WDT reset on a wrong key
  while disabled, the unarmed probe started over, and looped until the run timed out.
- **Code layout changes silicon's answers** when timing or the NVM cache matters: adding a
  function after `check_busy` in `nvmctrl.c` changed whether a command written while busy set
  PROGE, and whether PRM read 1. When an unrelated edit makes a check fail on silicon, suspect
  fetch timing; run the timed sequence from RAM (`__attribute__((section(".ramfunc"),
  noinline, long_call))`) or leave the value unchecked and say why.
- **A probe can break the next programming.** `harness/hw_run.py` resets and halts the part
  before programming, because a sticky NVMCTRL error left by the last firmware made the DFP's
  flash algorithm fail ("flash erase sector failure (address 0x00000000; result code 0x1)").
  If programming fails like that, reset the part (pyOCD `reset_and_halt`) and try again.
- **Pins without wiring.** `firmware/hello/src/scope.h` records the edges of a peripheral's own
  output pins through PORT.IN (PINCFG.INEN), from RAM, timed by SysTick over 24 bits. For inputs,
  an EIC pin's pull resistor turned by PORT.OUT makes clean pulses (a pin muxed to a TC or TCC
  loses its pull: released, it is pulled down whatever PORT.OUT says). Silicon often starts
  outputs a clock tick after the enable: check periods and widths, not absolute times.
- **Schedule probe actions by absolute time.** A loop that waits 1 us per step and acts on some
  steps runs slower than 1 us per step and drifts by tens of microseconds. Put the stimuli and
  reads in a time-ordered list and wait for each one's time from the same reference
  (`tccprobe9.c`), or let `scope_record_stim` make them while it records.
- **Floating pins keep their charge** for a millisecond and more. "The output is kept" and "the
  output is released" read the same until a probe drives the opposite level or uses the pull
  (`tccprobe5..7`); board parts can pull a released pin too (the MC00's PA18/PA19 sit behind the
  DGI level converter).
- **Prints take milliseconds on silicon and nothing on the twin** (instant SERCOM TX): never let
  a timed behaviour run across a print, or the transcripts differ for reasons unrelated to the
  model (`tcprobe4.c`).
- **Line endings in pp-qemu.** pp-qemu keeps bytes as they are (no autocrlf), and Python's
  `write_text()` writes CRLF on Windows: read and write bytes in edit scripts. `export` refuses
  a commit that adds CRLF lines. Git Bash heredocs also mangle backslashes in C strings: write
  scripts with the Write tool, or edit with the Edit tool.
- **Twin runs use icount.** `twin` and the golden tests use `--icount 5`, so the twin's timing
  is deterministic. Leave timing checks a margin for the board's clock error anyway.
