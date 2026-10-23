# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""MCP server (stdio transport) exposing the twin runner to AI assistants.

Tools: ``twin_run`` runs an ELF on the twin through the same engine as
``py -m tools.pp run``; ``twin_machines`` lists what the QEMU build offers.
The ``bench_*`` tools drive the live bench the user sees (bench.py finds
its engine): boards, Click boards, firmware, buttons, the console, the
timeline of pins, buses and interrupts, and injected faults.
Nothing here writes to stdout: that is the protocol channel.
"""

import re
from pathlib import Path
from typing import Any, TypedDict

import anyio

try:                    # mcp 2.x renamed FastMCP to MCPServer
    from mcp.server.mcpserver import MCPServer as FastMCP
    from mcp.server.mcpserver.exceptions import ToolError
except ImportError:     # mcp 1.x
    from mcp.server.fastmcp import FastMCP
    from mcp.server.fastmcp.exceptions import ToolError

from ..pp import (DEFAULT_MACHINE, QemuNotFound, RunnerError, RunOptions,
                  machines_info, run)
from ..pp.qemu import DEFAULT_AVR_MACHINE, PROJECT_ROOT, elf_target
from .bench import BENCH, BenchError

MAX_TIMEOUT_S = 600.0
# The transcript goes into the assistant's context: keep its head and tail.
MAX_UART_CHARS = 64 * 1024
MAX_TRACE_LINES = 200

INSTRUCTIONS = f"""\
Peripheral Pretender runs Microchip MCU firmware on a QEMU digital twin.
twin_machines shows which machines the QEMU build offers; the default board
is {DEFAULT_MACHINE} (PIC32CM5164JH01100 Curiosity Pro, UART = SERCOM4 VCOM)
for Arm ELFs and {DEFAULT_AVR_MACHINE} (AVR64EA48 Curiosity Nano, UART =
USART1 VCOM) for AVR ELFs, which run on qemu-system-avr.
twin_run boots an ELF and waits for the firmware to print 'PP:EXIT <n>':
exit_code is n, 124 on timeout, 125 if QEMU ended first (status
cpu-lockup for a CPU lockup; see detail and qemu_stderr_tail). On timeout,
detail says where the CPU was (e.g. stuck in HardFault). unimplemented_summary
groups the register accesses the twin does not model yet, by instance and
register: use it to tell firmware bugs from model gaps. A long uart
transcript is cut in the middle (uart_truncated, uart_length).

The bench_* tools drive the live bench instead: the workspace the user sees
in VS Code (or a browser), with its boards, Click boards and firmware, each
twin running on. bench_status first: it lists the boards (their keys,
"b1"...), what runs on them, the LEDs, the console's tail, and what can be
added. Then bench_run puts firmware on a board, bench_press and
bench_control act on it, bench_uart talks to its console and waits for
text (pass an action's uart_mark_before as since, so what the firmware
printed at once is not missed), bench_timeline shows what happened on its pins, buses (I2C
transactions, SPI transfers, UART text) and interrupts in the twin's
virtual time, and bench_faults injects faults (a pad shorted, an I2C device
not answering, a stuck MISO) to test error handling. bench_record and
bench_replay make a run repeatable: record a bug once, then replay it
exactly (same instruction timing, same inputs at the same instants) and
check that it went the same way. The user watches the same bench, so say
what you change."""


class TwinRunResult(TypedDict):
    exit_code: int
    status: str
    firmware_exit: int | None
    uart: str
    uart_truncated: bool
    uart_length: int
    unimplemented_summary: dict[str, Any]
    qemu_stderr_tail: str
    duration_s: float
    machine: str
    elf: str
    detail: str | None
    trace: dict[str, Any] | None
    warnings: list[str]


mcp = FastMCP("peripheral-pretender", instructions=INSTRUCTIONS,
              log_level="WARNING")


def resolve_elf(elf_path: str) -> Path:
    """Relative paths are tried against the working directory, then the
    project root (clients start servers in arbitrary directories)."""
    path = Path(elf_path).expanduser()
    if path.is_absolute() or path.exists():
        return path
    in_project = PROJECT_ROOT / path
    return in_project if in_project.exists() else path


def bound_uart(text: str, limit: int = MAX_UART_CHARS) -> tuple[str, bool]:
    """text, or its head and tail around an omission marker when longer
    than limit characters; and whether it was cut."""
    if len(text) <= limit:
        return text, False
    half = limit // 2
    omitted = len(text) - 2 * half
    return (text[:half] + f"\n[... {omitted} characters omitted ...]\n"
            + text[-half:]), True


def bound_trace(trace: dict | None) -> dict | None:
    if trace is None or len(trace["lines"]) <= MAX_TRACE_LINES:
        return trace
    return {**trace, "lines": trace["lines"][:MAX_TRACE_LINES],
            "truncated": True}


def machine_extra_args(machine_options: str | None) -> list[str]:
    """machine_options as "-machine OPTIONS" (QEMU merges it into -M); a
    machine type there would silently replace the machine argument."""
    if not machine_options:
        return []
    first = machine_options.split(",", 1)[0]
    if "=" not in first or first.startswith("type="):
        raise ToolError("machine_options takes only key=value options "
                        "(e.g. 'sw0-pressed=on'); name the machine in "
                        "'machine'")
    return ["-machine", machine_options]


@mcp.tool()
async def twin_run(elf_path: str, timeout_s: float = 10.0,
                   input_text: str | None = None,
                   icount_shift: int | None = None,
                   machine: str | None = None,
                   machine_options: str | None = None,
                   input_after: str | None = None,
                   trace: list[str] | None = None) -> TwinRunResult:
    """Run a firmware ELF on the digital twin and report the outcome.

    Args:
        elf_path: ELF file (absolute, or relative to the project root).
        timeout_s: host seconds to wait for 'PP:EXIT <n>' (max 600).
        input_text: text sent to the UART once the firmware has printed
            something; include the line endings it expects (e.g. "\\r\\n").
        icount_shift: deterministic virtual time, 2^shift ns per
            instruction (0-10); omit for real-time pacing. Only output-only
            runs are reproducible: UART input arrives in host time.
        machine: QEMU machine name (see twin_machines); omitted: the
            default board of the ELF's architecture (Arm or AVR).
        machine_options: extra machine properties, comma separated, e.g.
            "sw0-pressed=on" (button SW0 held) on pic32cm-jh01-cpro.
        input_after: send input_text once this text has appeared on the
            UART (e.g. a prompt) instead of after the first output.
        trace: QEMU trace event patterns (e.g. ["mchp_sercom_*"]); matching
            events are counted in trace (first 200 lines kept).

    Returns exit_code (firmware PP:EXIT value, 124 timeout, 125 QEMU ended
    first), status (exit, timeout, qemu-exited, cpu-lockup), detail, uart
    transcript (LF line endings; head and tail of at most 64 KB, see
    uart_truncated and uart_length), unimplemented_summary (model backlog),
    trace, warnings, qemu_stderr_tail, duration_s.
    """
    if not 0 < timeout_s <= MAX_TIMEOUT_S:
        raise ToolError(f"timeout_s must be in (0, {MAX_TIMEOUT_S:g}]")
    elf = resolve_elf(elf_path)
    if machine is None:
        machine = (DEFAULT_AVR_MACHINE if elf_target(elf) == "avr"
                   else DEFAULT_MACHINE)
    options = RunOptions(elf=elf, machine=machine,
                         timeout_s=timeout_s, icount_shift=icount_shift,
                         input_text=input_text, input_after=input_after,
                         trace=list(trace or ()),
                         extra_args=machine_extra_args(machine_options))
    # mcp 2.x hides the message of any exception but ToolError
    try:
        result = await anyio.to_thread.run_sync(run, options)
    except (RunnerError, QemuNotFound, ValueError) as exc:
        raise ToolError(str(exc)) from exc
    uart, uart_truncated = bound_uart(result.uart)
    return TwinRunResult(
        exit_code=result.exit_code,
        status=result.status,
        firmware_exit=result.firmware_exit,
        uart=uart,
        uart_truncated=uart_truncated,
        uart_length=len(result.uart),
        unimplemented_summary=result.unimplemented_summary,
        qemu_stderr_tail=result.qemu_stderr_tail,
        duration_s=result.duration_s,
        machine=result.machine,
        elf=result.elf,
        detail=result.detail,
        trace=bound_trace(result.trace),
        warnings=result.warnings,
    )


@mcp.tool()
async def twin_machines() -> dict[str, Any]:
    """List the QEMU binary in use and the machines it offers.

    default_machine_available tells whether the Peripheral Pretender board
    is built in.
    """
    return await anyio.to_thread.run_sync(machines_info)


# -- the live bench ----------------------------------------------------------

MAX_EVENTS = 120                # events a timeline row, the latest kept


async def _bench(fn, *args):
    try:
        return await anyio.to_thread.run_sync(fn, *args)
    except BenchError as exc:
        raise ToolError(str(exc)) from exc


def _board_brief(b: dict) -> dict:
    return {k: b[k] for k in ("key", "board", "name", "running", "firmware",
                              "clicks", "click_slots", "wired", "buttons",
                              "leds", "faults", "timeline", "uart_tail",
                              "uart_mark", "recording", "replaying",
                              "recordings")}


@mcp.tool()
async def bench_status(uart_tail: int = 1500) -> dict[str, Any]:
    """The live bench: each board (key, type, firmware, Click boards, LEDs
    lit, faults, the console's last characters), the selected board, the
    parts on the bench, and the board types, Click boards and templates
    that can be added. Also the engine's URL: the user can open it in a
    browser to watch, if VS Code is not showing the bench.
    """
    st = await _bench(BENCH.state)
    return {"engine": BENCH.url, "selected": st["selected"],
            "boards": [{**_board_brief(b),
                        "uart_tail": b["uart_tail"][-uart_tail:]}
                       for b in st["boards"]],
            "parts": st["parts"], "board_types": st["board_types"],
            "clicks": st["clicks"], "templates": st["templates"]}


@mcp.tool()
async def bench_add_board(board: str, replace: bool = False) -> dict[str, Any]:
    """Put a board on the bench: board is a type from bench_status
    (board_types, e.g. "ev15n46a" for the PIC32CM MC00 Curiosity Pro). It
    starts on its demo firmware. replace: instead of the selected board.
    """
    st = await _bench(BENCH.request, {"type": "select-board" if replace
                                      else "add-board", "board": board})
    return {"boards": [_board_brief(b) for b in st["boards"]]}


@mcp.tool()
async def bench_remove_board(twin: str) -> dict[str, Any]:
    """Take board @twin ("b2") off the bench; parts and wires stay."""
    st = await _bench(BENCH.request, {"type": "remove-board", "twin": twin})
    return {"boards": [_board_brief(b) for b in st["boards"]]}


@mcp.tool()
async def bench_template(template: str) -> dict[str, Any]:
    """Set the bench up from a template (bench_status lists them, e.g.
    "mc00-thermo"): it replaces every board, Click board and part."""
    st = await _bench(BENCH.request, {"type": "template", "id": template})
    return {"boards": [_board_brief(b) for b in st["boards"]]}


@mcp.tool()
async def bench_plug(slot: str, click: str | None,
                     twin: str | None = None) -> dict[str, Any]:
    """Plug Click board @click (an id from bench_status, e.g. "thermo8")
    into mikroBUS @slot ("ext1") of a board, or unplug it (click null).
    The board's twin restarts with it."""
    await _bench(BENCH.request, {"type": "click", "slot": slot,
                                 "click": click, **({"twin": twin}
                                                    if twin else {})})
    return _board_brief(await _bench(BENCH.board, twin))


@mcp.tool()
async def bench_run(elf_path: str, twin: str | None = None,
                    wait_s: float = 1.0) -> dict[str, Any]:
    """Run firmware on a board of the bench (the selected one without
    twin): the ELF goes to its twin, which restarts on it; the board keeps
    it for next time. Returns the board after @wait_s seconds, with the
    console's tail."""
    elf = resolve_elf(elf_path)
    try:
        data = elf.read_bytes()
    except OSError as exc:
        raise ToolError(f"cannot read {elf}: {exc}") from exc
    board = await _bench(BENCH.board, twin)
    query = f"?twin={board['key']}&name={elf.name}"
    await _bench(lambda: BENCH.call("POST", "/api/firmware" + query,
                                    raw=data))
    await anyio.sleep(max(0.0, min(wait_s, 30.0)))
    return {**_board_brief(await _bench(BENCH.board, board["key"])),
            "uart_mark_before": board["uart_mark"]}


@mcp.tool()
async def bench_press(button: str, action: str = "click", hold_ms: int = 150,
                      twin: str | None = None) -> dict[str, Any]:
    """Act on a board button (bench_status lists them: "sw0", "reset"):
    action "click" presses it for @hold_ms, "press" holds it down,
    "release" lets go."""
    if action not in ("click", "press", "release"):
        raise ToolError("action is click, press or release")
    board = await _bench(BENCH.board, twin)
    mark = board["uart_mark"]
    msg = {"type": "button", "id": button, "twin": board["key"]}
    if action in ("click", "press"):
        await _bench(BENCH.request, {**msg, "pressed": True})
    if action == "click":
        await anyio.sleep(max(0, min(hold_ms, 10000)) / 1000)
    if action in ("click", "release"):
        await _bench(BENCH.request, {**msg, "pressed": False})
    await anyio.sleep(0.2)
    return {**_board_brief(await _bench(BENCH.board, board["key"])),
            "uart_mark_before": mark}


@mcp.tool()
async def bench_control(slot: str, control: str, value: float,
                        twin: str | None = None) -> dict[str, Any]:
    """Set a Click board's control, as its slider would: the temperature a
    Thermo 8 measures, a potentiometer's position... bench_status gives the
    Click boards and their current values (click_values)."""
    board = await _bench(BENCH.board, twin)
    await _bench(BENCH.request, {"type": "control", "slot": slot,
                                 "id": control, "value": value,
                                 "twin": board["key"]})
    after = await _bench(BENCH.board, board["key"])
    return {"click_values": after["click_values"],
            "uart_mark_before": board["uart_mark"]}


async def _uart_mark(key: str) -> int:
    return (await _bench(BENCH.call, "GET",
                         f"/api/uart?twin={key}&tail=0"))["mark"]


@mcp.tool()
async def bench_uart(send: str | None = None, wait_for: str | None = None,
                     since: int | None = None, timeout_s: float = 5.0,
                     tail: int = 3000,
                     twin: str | None = None) -> dict[str, Any]:
    """A board's console (its virtual COM port): send @send (include the
    line ending it expects, e.g. "\\r\\n"), then wait up to @timeout_s for
    text matching the regex @wait_for after mark @since (default: the mark
    before sending). Marks count characters received: bench_status,
    bench_press, bench_control and bench_run return one (uart_mark,
    uart_mark_before), so text the firmware printed in between is not
    missed. Returns the console's last @tail characters, whether it
    matched, the text since the mark and the mark now."""
    board = await _bench(BENCH.board, twin)
    key = board["key"]
    if since is None:
        since = await _uart_mark(key)
    if send:
        await _bench(BENCH.request, {"type": "uart", "data": send,
                                     "twin": key})
    matched, new = None, ""
    if wait_for:
        try:
            pattern = re.compile(wait_for)
        except re.error as exc:
            raise ToolError(f"wait_for: {exc}") from exc
        matched = False
        deadline = anyio.current_time() + max(0.1, min(timeout_s, 120.0))
        while True:
            new = (await _bench(BENCH.call, "GET",
                                f"/api/uart?twin={key}&since={since}"))["text"]
            if pattern.search(new):
                matched = True
                break
            if anyio.current_time() >= deadline:
                break
            await anyio.sleep(0.1)
    reply = await _bench(BENCH.call, "GET", f"/api/uart?twin={key}&tail={tail}")
    return {"twin": key, "text": reply["text"], "matched": matched,
            "since_text": new[-tail:], "mark": reply["mark"]}


def _ms(t: int, t0: int) -> float:
    return round((t - t0) / 1e6, 4)


def _compact_row(r: dict, t0: int) -> dict:
    """A timeline row for an assistant: times in ms from the window's
    start, the latest MAX_EVENTS events"""
    out = {"label": r["label"], "kind": r["kind"]}
    d = r.get("data")
    if d is None:
        return {**out, "events": []}
    if r["kind"] == "pad":
        events = [[_ms(t, t0), v] for t, v in d["edges"]]
        out["level_at_start"] = d["level"]
        out["busy"] = [[_ms(a, t0), _ms(b, t0), duty]
                       for a, b, duty in d["busy"]][-MAX_EVENTS:]
    elif r["kind"] == "irq":
        events = [[_ms(a, t0), None if b is None else round((b - a) / 1e3, 3)]
                  for a, b in d["spans"]]
        out["runs"], out["pends"] = len(d["spans"]), d["pends"]
        out["note"] = "events: [start ms, duration us]"
    else:
        events = []
        for it in d["items"]:
            if it.get("count"):
                events.append([_ms(it["t"], t0), f"{it['count']} merged; "
                               "ask for a shorter window"])
            elif r["kind"] == "i2c":
                events.append([_ms(it["t"], t0), it["text"]])
            elif r["kind"] == "uart":
                events.append([_ms(it["t"], t0), it["dir"], it["text"]])
            else:
                events.append([_ms(it["t"], t0),
                               " ".join(f"{v:02X}" for v in it["mosi"]),
                               " ".join(f"{v:02X}" for v in it["miso"])])
    if len(events) > MAX_EVENTS:
        out["dropped"] = len(events) - MAX_EVENTS
        events = events[-MAX_EVENTS:]
    out["events"] = events
    return out


@mcp.tool()
async def bench_timeline(last_ms: float = 1000.0, kinds: list[str] | None = None,
                         pads: list[str] | None = None,
                         twin: str | None = None) -> dict[str, Any]:
    """What happened on a board over its last @last_ms milliseconds of
    virtual time, like a logic analyzer: its named pads (LEDs, buttons,
    Click pins: edges [ms, level] and busy spans [ms, end, high fraction]),
    each bus that talked (a SERCOM, or an AVR USART, SPI or TWI: I2C
    transactions "0x18 W 05 | 0x18 R 01 90" with NACKs marked, SPI
    transfers [ms, MOSI, MISO], UART text [ms, tx or rx, text]) and the
    interrupts taken (runs, durations). kinds narrows the rows ("pad",
    "i2c", "spi", "uart", "irq"); pads adds pads by name ("PA05", or
    "PB3" on AVR). Times are ms from the window's start (window_start_s,
    in the twin's virtual seconds); each row keeps its latest events."""
    board = await _bench(BENCH.board, twin)
    if not board["timeline"]:
        raise ToolError("this board has no timeline: its firmware is not "
                        "running, or its QEMU build has no event recorder")
    span = int(max(0.001, min(last_ms, 60000.0)) * 1e6)
    req = {"twin": board["key"], "span": span, "width": 2000, "suggest": True}
    msg = await _bench(BENCH.call, "POST", "/api/timeline", req)
    rows = msg["suggested"]
    if kinds:
        rows = [r for r in rows if r["kind"] in kinds]
    for pad in pads or []:
        pad = pad.strip().upper()
        if not any(r.get("pad") == pad for r in rows):
            rows.append({"id": f"pad:{pad}", "kind": "pad", "pad": pad,
                         "label": pad})
    msg = await _bench(BENCH.call, "POST", "/api/timeline",
                       {**req, "rows": rows, "suggest": False,
                        "t1": msg["t1"]})
    t0 = msg["t0"]
    return {"twin": board["key"], "window_start_s": t0 / 1e9,
            "window_ms": round((msg["t1"] - t0) / 1e6, 3),
            "events_lost": msg["lost"],
            "rows": [_compact_row(r, t0) for r in msg["rows"]]}


@mcp.tool()
async def bench_faults(faults: list[dict[str, Any]] | None = None,
                       clear: bool = False,
                       twin: str | None = None) -> dict[str, Any]:
    """Inject faults into a board's twin to test the firmware's error
    handling; they replace the board's faults (clear: remove them all;
    neither: just list them). Each fault is one of
      {"kind": "short", "pad": "PA24", "level": 0}  pad shorted to GND (1:
          to VDD), over every driver
      {"kind": "nack", "slot": "ext1"}  that slot's I2C Click stops
          answering (or "addr": "48" for a 7-bit address in hex)
      {"kind": "miso", "host": "sercom1", "value": "ff"}  that SPI host
          reads the value whatever the device sends.
    Faults last until changed (a restart keeps them) and are not saved."""
    board = await _bench(BENCH.board, twin)
    if clear or faults is not None:
        await _bench(BENCH.request, {"type": "faults", "twin": board["key"],
                                     "faults": [] if clear else faults})
        board = await _bench(BENCH.board, board["key"])
    return {"twin": board["key"], "faults": board["faults"]}


@mcp.tool()
async def bench_record(action: str = "start", name: str | None = None,
                       twin: str | None = None) -> dict[str, Any]:
    """Record a run that replays exactly. "start" restarts the board's
    firmware from reset in a deterministic mode (fixed instruction timing,
    paced to real time); from then on every input (bench_press,
    bench_control, bench_uart, faults, the user's clicks) is kept with
    the virtual time it took effect. "stop" saves it as @name (default:
    the board and the time) with digests of everything that happened.
    bench_replay runs it again."""
    if action not in ("start", "stop"):
        raise ToolError("action is start or stop")
    board = await _bench(BENCH.board, twin)
    await _bench(BENCH.request, {"type": "record", "twin": board["key"],
                                 "action": action,
                                 **({"name": name} if name else {})})
    board = await _bench(BENCH.board, board["key"])
    return {"twin": board["key"], "recording": board["recording"],
            "recordings": board["recordings"]}


@mcp.tool()
async def bench_replay(name: str, pace: bool = False, timeout_s: float = 120.0,
                       twin: str | None = None) -> dict[str, Any]:
    """Replay recording @name on its board: the same firmware and Click
    boards from reset, each recorded input at the instant it took effect.
    Waits until the replay passes the recording's end and says whether
    everything (pins, bus traffic, interrupts) happened exactly as
    recorded, or the first 10 ms window where it differs (differs_at, ns
    of virtual time). pace: at real-time speed (for watching); else as
    fast as it runs. Afterwards bench_timeline shows the replayed run."""
    board = await _bench(BENCH.board, twin)
    await _bench(BENCH.request, {"type": "replay", "twin": board["key"],
                                 "name": name, "pace": pace})
    deadline = anyio.current_time() + max(1.0, min(timeout_s, 600.0))
    while True:
        b = await _bench(BENCH.board, board["key"])
        r = b["replaying"] or {}
        if r.get("state") != "running" or anyio.current_time() > deadline:
            break
        await anyio.sleep(0.25)
    return {"twin": board["key"], "replay": r,
            "uart_tail": b["uart_tail"][-1500:]}


@mcp.tool()
async def bench_recordings(twin: str | None = None) -> dict[str, Any]:
    """The recordings saved for a board's type: name, firmware, length in
    virtual ns, number of inputs and events, when saved."""
    board = await _bench(BENCH.board, twin)
    msg = await _bench(BENCH.call, "GET",
                       f"/api/recordings?twin={board['key']}")
    return {"twin": board["key"], "recordings": msg["recordings"]}


def main() -> None:
    mcp.run("stdio")
