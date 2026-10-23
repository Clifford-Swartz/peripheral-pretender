# The timeline, faults, exact replay and the assistants' bench

Status: built (2026-10-22). Owner's ask, from "what would make this useful":
- things a board on the desk can't do: a timeline of pins, buses and interrupts, injected
  faults, exact replay;
- an AI assistant driving the bench over MCP, testing its own firmware.

## 1. The event recorder (pp-qemu)

`hw/misc/pp_trace.c` (`include/hw/misc/pp_trace.h` has the stream). The machine property
`trace=<chardev>` creates it, on every board: MC00, JH01, the PyKit (`curiosity-circuitpython`)
and the AVR64EA48 Nano (`avr64ea48-cnano`, in `qemu-system-avr`). Everything it reports carries
`QEMU_CLOCK_VIRTUAL`, the firmware's own time under `-icount`.

The recorder knows no device. The SoC glue (`pic32cm_soc_trace()`, `avr8x_soc_trace()`) wires
the sources, then attaches it.

**Sources:**
- **Pads:** the PORT model calls `pp_trace_pad()` whenever a pad's level changes, whoever drives
  it, and registers how to read every pad (`pp_trace_pads()`) for a reader that connects. Arm
  parts number pads 32 a group (PA05), AVR 8 (PB3); the stream's `HELLO` says which.
- **Serial models:** `pp_trace_event()` at their trace points: UART bytes each way, SPI
  transfers (out and in), I2C start, byte, host NACK and stop. On Arm these are the SERCOMs; on
  AVR the USARTs, SPI0 and TWI0.
- **Interrupts:** on Arm an observer hook in `armv7m_nvic.c` (`armv7m_nvic_observe()`) for
  pend, entry and return, by exception number. On AVR one in `target/avr`
  (`avr_cpu_observe_irqs()`): a source asserting its line, the CPU taking the vector, the RETI
  that ends its level. These are by vector, and `HELLO` says so.
- **Resets.**

**Stream:** 16-byte little-endian records (`t`, kind, source, `a`, `b`):
- names follow `NAME` records;
- a `TIME` heartbeat every 20 ms;
- `LOST` counts what was dropped if the reader fell behind. The buffer holds 8 MiB and QEMU never
  blocks.
- A pad toggling more than 64 times a millisecond switches to one `PAD_BUSY` record per ms
  (edges and high time) until it calms.

**The engine:**
- `tools/pp_web/session.py` starts every machine that has the recorder with it; the probe is
  `machine_has_trace()`, which looks for `trace=` in `-M <machine>,help`.
- `tools/pp/trace.py` parses the stream into a `Store`: columns in 8192-record chunks, 2 M
  records, each chunk with every pad's level at its start.

## 2. Decoders and the timeline

`tools/pp/trace.py` decodes a `Window` (records read once, 50 ms of lead for what began
earlier):
- **pads:** edges, and spans faster than a pixel as busy with their duty;
- **UART:** runs of text, split at a 2 ms pause or a newline;
- **SPI:** transfers, split at a 20 µs pause or a CS rise;
- **I2C:** transactions with repeated starts, `0x18 W 05 | 0x18 R 01 90`, NACKs marked;
- **interrupts:** spans, nested;
- **wide views:** bursts.

`tools/pp_web/timeline.py` suggests rows:
- the board's LEDs and buttons, and its Clicks' pins;
- the buses that talked (the VCOM first: SERCOM0 on the MC00, USART1 on the AVR Nano);
- other pads that changed;
- the exceptions taken, named from the device JSON (`RTC`, `TCB0_INT`).

The page (`static/timeline.js`) is a drawer under the bench:
- **Live:** follows the twin at 200 ms. The wheel zooms, snapping to the transfer, handler or
  edge under the cursor; zooming in pauses. Dragging pans, and double-click goes live again.
- **Hover** shows the decoded detail; rows can be hidden, or added from the suggestions or by pad
  name.
- **Requests** go to the engine over the page's WebSocket, and `POST /api/timeline` takes the
  same request.

Transfers are instants on the twin: the serial models move a whole character at once, with no
bus timing. A transaction is about a microsecond wide, so its label is drawn beside it.

## 3. Injected faults

| Fault | Where | Effect |
|---|---|---|
| `short` (pad, level) | PORT `pX-short` (Arm and AVR) | the pad reads GND or VDD over every driver, the PORT's too; LEDs see it |
| `nack` (addr, or slot: its I2C Click) | `fault-nack` of every I2C host: SERCOMs, AVR TWI0 | that address NACKs before any device sees the start |
| `miso` (host, value) | `fault-miso` of that SPI host: `sercom1`, AVR `spi0` | the SPI host reads that value whatever the device sends |

`bus_hosts()` (in `tools/pp_web/server.py`) finds the hosts in the device JSON. A `miso` fault
written with `sercom` instead of `host` still works.

Faults are a test's, not the board's:
- they aren't saved or migrated;
- a QEMU restart keeps them, since the engine applies them again;
- the side panel's Faults section sets them;
- each board's title bar shows a ⚡ count;
- the page message is `{"type": "faults", "faults": [...]}`, validated by `Twin.check_fault()`.

## 4. Exact replay

A deterministic run has three parts:
1. **Fixed instruction timing:** `-icount shift=4,sleep=off`. `shift=auto` (the live default)
   adapts to host speed and differs between runs by tens of ns.
2. **The same start:** QEMU starts paused (`-S`). Click values and faults go in by QMP, then
   `cont`.
3. **Inputs at known instants:** every input goes through the recorder's scheduler,
   `at <t> <id> set <path> <prop> <value>` and `at <t> <id> reset` (0 is now). It applies each on
   a `QEMU_CLOCK_VIRTUAL` timer and reports `INPUT` with the instant it took effect.
   - Inputs are buttons, Click controls, faults, bench drives, and the console through the
     VCOM's `rx-inject` (a SERCOM, or the AVR's USART1).
   - A replay schedules them at those instants. The guest sees each at the same instruction, so
     the run repeats event for event.
   - Inputs for the same instant apply in the order they came. Two inputs often land on one
     nanosecond (a button release and console text sent together). A queue that put the newer
     first turned them around on replay.

**Pacing:** `align=on` is not allowed with `sleep=off`. So `pace 1` has the recorder pause the VM
while virtual time leads real time. A paused VM's virtual clock does not move, so the guest
cannot tell.

**A recording** (`<workspace dir>/recordings/<name>.json`, with the ELF beside it):
- the machine string, the starting settings and the timed inputs;
- a hash of all events every 10 ms (`digests()`).

**A replay:**
- reports `same`, or `differs` with the first 10 ms that differs (`compare()`), or `stalled`;
- takes no live input.

**AVR interrupts and SEI.** Upstream QEMU's AVR target dropped the CPU loop's interrupt check
when SREG.I was clear, and SEI or a write to SREG didn't re-arm it. An interrupt that went
pending during a `cli` section then waited for the CPU loop's next exit, which pacing and the
monitor move. A recording took TCB0's handler 0.54 ms late; its replay took it at once. Now
SEI and SREG writes re-arm the check, as `cpu_interrupt()` does from the CPU thread
(`helper_sei`). SEI also ends the TB after the next instruction, so the interrupt comes where
silicon takes it: after the instruction that follows SEI.

**Checked** (`tests/web/test_timeline.py`, `tests/mcp_server/test_mcp_bench.py`):
- the Thermo 8 demo with a slider move, SW0, console text and a reset replays every event (708
  to 712 per run);
- the same recording with one input moved 1 ms replays as `differs`;
- the AVR Nano's 8x8 G demo with SW0 and console text replays every event (about 4000 a run);
  its SPI0 frames, TCB0's vector, the MISO fault and a short on LED0 show;
- the PyKit's CircuitPython: an I2C scan on SERCOM5, interrupts named.

Not covered:
- the DGI (EDBG data gateway) socket;
- the analog co-simulation;
- several boards wired together (each records alone).

## 5. The bench for assistants (MCP)

**Engine HTTP:**
- `GET /api/bench`: every board's summary (Clicks, LEDs lit, faults, console tail and mark,
  recordings);
- `POST /api/request`: any page message;
- `POST /api/timeline`;
- `GET /api/uart?since=<mark>`;
- `GET /api/recordings`.

Each engine writes `~/.pp_web/engines/<pid>.json` (port, workspace); `PP_WEB_ENGINES` moves it.

`tools/pp_mcp` (`bench.py` finds the engine; `server.py` has the tools):
- **Board and Clicks:** `bench_status`, `bench_add_board`, `bench_remove_board`,
  `bench_template` and `bench_plug`.
- **Running and acting:** `bench_run` (an ELF), `bench_press`, `bench_control`, and `bench_uart`
  (send, then wait for a regex after a mark).
- **Seeing:** `bench_timeline`: rows compacted to ms from the window's start, 120 events a row.
- **Faults:** `bench_faults`.
- **Replay:** `bench_record`, `bench_replay` (waits for the verdict) and `bench_recordings`.

Console marks count characters received. `bench_press`, `bench_control` and `bench_run` return
the mark from before they acted, so waiting from it cannot miss what the firmware printed at
once.

**Where the bench comes from:**
- With no engine announced, the MCP server starts one on `PP_WEB_BENCH_DIR`'s workspace, and
  stops it on exit.
- The VS Code extension shows an engine already running on its workspace rather than starting a
  second.
- The extension registers the server with VS Code (`mcpServerDefinitionProviders`, VS Code
  1.101+) with its bundled Python. Claude Code:
  `claude mcp add pp -e PYTHONPATH=<repo> -- py -m tools.pp_mcp`.

## 6. Next

- Bus timing in the serial models, so transfers take their real time.
- The AVR models' remaining sources: TCA/TCB waveform pads are traced (they drive pads), but
  the ADC, the event system and CCL report nothing of their own.
- Board-to-board links and the analog host inside a deterministic run.
- A scenario file (stimuli and expectations) that runs headless in CI, built on the scheduler.
