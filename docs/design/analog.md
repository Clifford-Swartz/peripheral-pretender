# Analog co-simulation

Status (2026-10-21): phase 1 works on the twin; the silicon check waits for an RC wired on the
MC00. Phase 2 works for the parts bench; the PWM demo is still to do. See §6. Owner's ask: make
analog circuits part of the backend, so it feels seamless. Customers draw or import a circuit; the twin's pads drive it; the ADC, the
comparators and the input buffers read it. Nobody configures a simulator.

## 0. Why

The parts bench (`tools/pp_web/bench.py`) solves the board's parts as a DC circuit 30 times a
second, between QMP polls. That covers LEDs, buttons, pots and resistor dividers. It cannot do
anything that changes in time: an RC filter on an ADC pin, a PWM filtered into a voltage, an
inductive kick, a motor spinning up, a sensor's output stage. Its answers also arrive at a 30 Hz
host rate, not at the instant the firmware samples. A conversion ought to read the voltage at
its own virtual instant.

## 1. Principles

1. **Customers describe circuits, not simulations.** Parts on the bench, a Click's analog front
   end, or a KiCad netlist. The engine picks the solver, the step and the sync points.
2. **One clock.** The analog solver runs in lockstep with QEMU's virtual time. A pad edge reaches
   the circuit at its virtual timestamp. An ADC sample reads the circuit at its virtual sampling
   instant. With `-icount` and no `sleep`, runs are deterministic, so CI can assert on them.
3. **The cheapest solver that is right.** Digital stays digital. Resistive nets keep the DC bench.
   Nets with capacitors, inductors or dynamic parts go to ngspice's transient solver. PWM much
   faster than a filter's corner can be averaged (later).
4. **Models travel with parts.** The part library carries a SPICE model for every part. A Click
   carries its analog section. Vendor models (op-amps, sensors) load as subcircuits.
5. **One timeline.** Pad edges with timestamps feed the circuit, the logic analyzer and VCD export
   alike. Analog nodes become scope traces on the same time axis.
6. **Checked on silicon.** Every claim about a peripheral's analog behaviour gets a probe on a
   real board, as for the digital models.

## 2. Architecture

```
 pp-qemu (virtual time)                        analog host (Python, tools/pp_analog)
 ┌───────────────────────────┐   socket,       ┌──────────────────────────────────────┐
 │ PORT ── pad observer ──┐  │   length-       │ link: the lockstep protocol          │
 │ ADC ── sample hook ────┼─►│ pp-cosim ◄─────►│ pads: Norton sources (B + external)   │
 │ PORT pX-bench ◄─ inputs┘  │   framed text   │ ngspice shared library, bg thread,   │
 └───────────────────────────┘                 │ held at the twin's frontier by its   │
                                               │ sync callback                         │
                                               └──────────────────────────────────────┘
        ▲ started by: tools.pp run --circuit, tools.pp_web (the bench), MCP, CI
```

The analog side is a separate process from QEMU, for three reasons:
- it keeps pp-qemu free of a SPICE dependency;
- the part library and netlist generation already live in Python;
- `pp-cosim` is a general port: an out-of-process Click model, a logic analyzer recorder or a VCD
  writer can use it too.

The engine stays headless. `tools.pp run`, the VS Code extension's engine (`tools.pp_web`), MCP
and CI all start the same host.

### 2.1 The co-simulation port (pp-qemu `pp-cosim`)

A machine property names a chardev: `-M pic32cm-mc00-cpro,cosim=analog`, with
`-chardev socket,id=analog,host=127.0.0.1,port=N` (QEMU is the client; the host listens first).
Every message, in both directions, is a 4-byte little-endian length followed by ASCII lines.
QEMU always speaks first and always waits for the reply. The host never sends unprompted, so
there are no asynchronous reads and no races.

| QEMU sends | Meaning | Host replies |
|---|---|---|
| `hello 1 <machine> <vdd_mv>` | at connection | `watch <pad>`…, `probe <id> <qom-path> <channel>`…, `tick <ns>`, `ok` |
| `init <t>` | the watched pads' states follow as `e` lines | `ok` (the host starts its solver here) |
| `e <t> <pad> <state>` | a watched pad changed, batched before the next request | — |
| `s <t> <id>` | sample probe `id` at virtual time `t` (ns) | `v <microvolts>` |
| `t <t>` | a tick: bring the analog side up to `t` | `in <group> <mask> <level>`…, `ok` |
| `f <t>` | the event batch is large: flush | `ok` |

Pad states are `h` and `l` (driven), `u` and `d` (pulled up or down, not driven), and `z`
(floating). The PORT reports them through a pad observer (`mchp_port_observe()`), called wherever
the PORT recomputes its pads, with the virtual time of the change. With icount, MMIO happens at
an exact instruction count, so edge times are exact.

- **Samples.** The ADC asks at the end of its sampling phase (conversion end minus the bit
  cycles), through `pp_cosim_sample()`. The answer takes precedence over the `ain` property.
- **Inputs.** Pads whose level comes from the circuit get it as the PORT's `pX-bench` layer, at
  each tick. Input crossings are therefore exact to the tick quantum, which the host chooses.
  There are no ticks unless some input pad is fed by a dynamic net.

### 2.2 The analog host (`tools/pp_analog`)

- **ngspice** as a shared library (`libngspice-0.dll`, `.so.0` on Linux), through ctypes.
- **Pads.** Each watched pad is a Norton source:
  `b_<pad> 0 <pad> i = v(g_<pad>) * (v(t_<pad>) - v(<pad>))`. Target and conductance are ngspice
  `external` sources, answered by the host's `GetVSRCData` callback from the pad's history:
  - `h`/`l`: VDD or 0 behind 40 Ω;
  - `u`/`d`: behind 40 kΩ;
  - `z`: open.

  A 1 GΩ leak keeps every pad node defined. A change between `h` and `l` ramps over 5 ns.
- **Lockstep.** The transient runs on ngspice's background thread. The sync callback (`GetSyncData`,
  called before every step) is where the protocol lives:
  - Until the twin has asked for something, it blocks reading the socket.
  - It never lets ngspice step past the twin's latest request. It shortens each step to land
    exactly on the next pad edge (also registered as a breakpoint with `ngSpice_SetBkpt`) or on a
    sample time.
  - At a sample time it answers from the values of that accepted point.

  ngspice and QEMU take turns, so the result does not depend on host timing. A sample earlier
  than ngspice's time (a tick overtook a conversion) is interpolated from a short history of
  accepted points.
- **Memory.** `.save none` (shared mode) keeps every vector at length 1. Values come through the
  `SendData` callback, so a session runs for hours at a flat memory footprint.
- **Failure.** If ngspice stops (a convergence failure, a bad model), the host keeps answering
  with the last voltages, so the twin never hangs. The error is reported as it is: the engine shows
  it, and `pp run` prints it and fails.

### 2.3 Circuits

- **Netlists.** Plain SPICE. A node named like a pad (`PA05`) is that pad; `VDD` and `0` are the
  board's supply and ground. The host adds the pad sources, so a customer's netlist is just the
  board-side circuit.
- **The bench.** Parts become SPICE elements: resistor, capacitor, inductor, LED and diode (with
  `.model` cards fitted to the bench's Vf at 20 mA), pot, button, switch, MOSFET. Live properties
  are external sources too: a pot's position, a pressed button. Turning a knob then never reloads
  the circuit. Adding or removing a part restarts the transient from a fresh operating point at
  the current virtual time.
- **Solver per net.** In phase 1, a bench with any capacitor or inductor goes to ngspice whole.
  Otherwise it stays on the DC bench (faster, already checked). Splitting into connected
  components comes later.

### 2.4 Cost

Every accepted ngspice step crosses into Python a few times (sync, data, sources), about
50–100 µs per step. An RC step response takes a few hundred steps per time constant. A 1 kHz PWM
into an RC with 1 kHz sampling runs at about real time. Faster PWM needs the averaging solver,
and a C shim for the callbacks is the next step if needed. When the circuit is slower than real
time, virtual time slows down with it: QEMU waits, nothing is lost.

## 3. Phases

1. **Lockstep core.** Pad observer, `pp-cosim` port and ADC sample hook in pp-qemu (MC00/JH01
   first). `tools/pp_analog` with ngspice, the protocol and the pad model.
   `tools.pp run --circuit board.cir`. Demo `APP=rcstep`: a pad steps an RC, the ADC samples it
   every 500 µs, and the twin matches the analytic curve within a few LSB.
   Silicon check: the same image on the MC00 with a 10 kΩ / 1 µF RC wired between EXT1 pins.
2. **Bench integration.** Capacitor and inductor parts on the page. The server starts the host
   when the bench needs it and shows ngspice's voltages and currents. PWM-through-RC as a DAC
   demo, checked on the MC00.
3. **Scope and logic analyzer.** Pad edges and analog nodes on one timeline in the board view;
   VCD/CSV export; MCP and CI probes with assertions (`pp run --probe PA05 --expect ...`).
4. **Part library models.** SPICE models per part, analog sections of Clicks (e.g. a pot Click's
   divider, a sensor's output stage), the AC's continuous inputs, the DAC's output stage, the
   ADC's sample-and-hold load, AVR (avr64ea48) pads and ADC.
5. **Board import.** KiCad netlists (then Altium) as boards: the customer's own board becomes the
   circuit, with the chip twin in the middle.

## 4. Silicon checks

- The `rcstep` image runs on the MC00 with a real RC, and the twin's transcript is compared with
  the board's (`tools.periph diff`). Tolerance: the parts' tolerance (1 % resistor, 10 % capacitor)
  plus ADC INL.
- The ADC's sampling instant: a fast edge against the sampling window (SAMPLEN sweep) on silicon
  pins down when the twin should sample.
- The pad model: a pad's output resistance (40 Ω typical) and pull resistance (40 kΩ) against a
  known load, read by the ADC.

## 5. Risks and decisions

- **Licences.** ngspice is Modified BSD, with named exceptions: KLU (LGPLv2), OSDI (MPL 2.0),
  `tclspice.c` (LGPLv2). The MSYS2 build links FFTW, which is GPL. The VSIX already ships QEMU
  (GPL) with its sources offered. Before bundling, decide one of two options:
  - build ngspice without FFTW (it has its own FFT);
  - ship the MSYS2 build under the same terms as QEMU.

  Vendor SPICE models carry their own licences. Each model in the part library records its source.
- **Convergence.** Switching circuits and ideal parts can stall a transient. The defaults (pad
  ramps, leaks, smooth MOSFET models) avoid the usual traps, and failures are reported as they
  are, never hidden.
- **Speed.** See §2.4. Averaging and the C shim are the planned answers. The tick quantum is a
  knob, with a sensible default.
- **Not in scope:** AC/noise analyses inside a twin run, thermal, EMI, and analog parts inside the
  MCU beyond its pads, ADC, AC and DAC.

## 6. Findings from phase 1 and 2

- **Lockstep works as designed.** ngspice's `GetSyncData` runs on its background thread before
  every step and can shorten the step. The whole protocol lives there, and `ngSpice_SetBkpt`
  works mid-run (it calls `CKTsetBreak` once the transient has started).
- **An edge has to shorten the current step.** A breakpoint registered after ngspice chose its
  step comes too late for that step. Before the fix, an edge arriving with a sample request was
  smeared about 25 µs late. The host now also clamps each step to the next known edge.
- **Accuracy is set by `tmax`, not `reltol`.** On a 10 kΩ / 1 µF step, standalone ngspice with
  1 ms steps is 0.6 mV off at t = τ, whatever `reltol` is (1e-3 or 1e-4). With 100 µs it is
  exact to the printed digits. The host defaults to `tmax` = 100 µs: within 5 µV of the
  analytic curve to 10 ms. The ~30 µV left at 80 ms is the pads' 1 GΩ leak, which is physical.
- **`.save none` needs `set no_mem_check`.** ngspice estimates its output memory as
  tstop / tstep points and refuses an open-ended run, although in shared mode `.save none`
  keeps only the latest point.
- **The twin needs `-icount`.** Without it, `QEMU_CLOCK_VIRTUAL` follows the host clock while
  QEMU waits for the circuit. SysTick then wraps unseen (tau measured 4.4 ms instead of 10).
  `pp run --circuit` defaults to shift 5, and `pp-cosim` warns without icount. The engine's
  `shift=auto,sleep=on` counts instructions too.
- **Firmware timestamps on an M0+.** A software division between a sample's timestamp and its
  `SWTRIG` costs microseconds: on the twin, 7 µs at shift 5. `rcstep` stores raw cycle counts
  and divides when it prints.
- **Cost.** `rcstep` (40 ms of RC with 80 samples and 1 ms ticks) takes about 5,000 ngspice
  steps and 0.4 s of wall time.
- **Phase 2 on the bench:**
  - live properties are external sources (`*pp control` lines), and the host takes `set NAME
    VALUE` on stdin;
  - the page gets frame averages (`state` lines; trapezoid areas over virtual time), mapped
    back through `bench.Analog.result()`;
  - a driven pad's current is g · (target − V), exact from the averages while it drives.
