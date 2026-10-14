# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Play a scenario on the twin and record what the board shows.

Examples never print PP:EXIT, and many need a button or UART input, so
the twin runs paced to the host clock (as in the web view) while a
scenario presses SW0, sends text or resets. Recorded: each LED's on/off
transitions with times, the UART output, and the model backlog (QEMU's
unimp/guest-error log). Always read the backlog before calling a run a
pass: a peripheral that only stores its registers can make an example
print "Success".

Steps, separated by ";":  wait <s> | press | release | send <text> | reset
(<text> takes \\r and \\n escapes).
"""

from __future__ import annotations

import asyncio
import json
import tempfile
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

from tools.pp.unimp import summarize_log
from tools.pp_web.session import Session

BOARDS = Path(__file__).resolve().parents[1] / "pp_web" / "boards"
LOG_ITEMS = "unimp,guest_errors,invalid_mem"


@dataclass
class Led:
    id: str
    pin: str                    # "PC05"
    active_low: bool


@dataclass
class ScenarioResult:
    machine: str
    elf: str
    steps: list[tuple[float, str]] = field(default_factory=list)
    leds: dict[str, list[tuple[float, bool]]] = field(default_factory=dict)
    uart: str = ""
    backlog: dict = field(default_factory=dict)
    stopped: str | None = None  # QEMU exited early: its last stderr

    def toggles(self, led: str) -> int:
        return max(0, len(self.leds.get(led, [])) - 1)


def parse_steps(text: str) -> list[tuple[str, str]]:
    steps = []
    for part in text.split(";"):
        kind, _, arg = part.lstrip(" \t\n").partition(" ")
        if kind != "send":
            kind, arg = kind.strip(), arg.strip()
        if not kind:
            continue
        if kind not in ("wait", "press", "release", "send", "reset"):
            raise ValueError(f"unknown step {part!r}")
        if kind == "wait":
            float(arg)
        steps.append((kind, arg.encode().decode("unicode_escape")
                      if kind == "send" else arg))
    return steps


def board_leds(machine: str) -> list[Led]:
    """The LEDs of the web board file that uses @machine."""
    base = machine.split(",")[0]
    for path in sorted(BOARDS.glob("*/board.json")):
        board = json.loads(path.read_text(encoding="utf-8"))
        if board["machine"] == base:
            return [Led(l["id"], l["pin"], l["active_low"]) for l in board["leds"]]
    return []


async def play(elf: Path, machine: str, steps: list[tuple[str, str]],
               leds: list[Led] | None = None) -> ScenarioResult:
    leds = board_leds(machine) if leds is None else leds
    groups = tuple(sorted({"a", "b"} | {l.pin[1].lower() for l in leds}))
    res = ScenarioResult(machine, str(elf), leds={l.id: [] for l in leds})
    uart: list[str] = []
    t0 = time.monotonic()
    now = lambda: round(time.monotonic() - t0, 3)

    def emit(msg: dict) -> None:
        if msg["type"] == "pins":
            for led in leds:
                g = msg["pins"].get(led.pin[1].lower())
                if not g:
                    continue
                on = g[int(led.pin[2:])][1 if led.active_low else 0] > 0.5
                trace = res.leds[led.id]
                if not trace or trace[-1][1] != on:
                    trace.append((now(), on))
        elif msg["type"] == "uart":
            uart.append(msg["data"])
        elif msg["type"] == "status" and msg["state"] == "stopped":
            res.stopped = msg.get("stderr", "")[-500:]

    with tempfile.TemporaryDirectory() as d:
        log = Path(d) / "qemu.log"
        s = Session(machine, elf, emit, groups=groups,
                    extra_args=("-d", LOG_ITEMS, "-D", str(log)))
        await s.start()
        try:
            for kind, arg in steps:
                res.steps.append((now(), f"{kind} {arg}".strip()))
                if kind == "wait":
                    await asyncio.sleep(float(arg))
                elif kind in ("press", "release"):
                    await s.press("sw0", kind == "press")
                elif kind == "send":
                    await s.uart_send(arg)
                elif kind == "reset":
                    await s.press("reset", True)
            await asyncio.sleep(0.2)            # the last frame
        finally:
            await s.stop()
        res.backlog = summarize_log(log).as_dict()
    res.uart = "".join(uart).replace("\r", "")
    return res


def to_json(res: ScenarioResult) -> str:
    return json.dumps(asdict(res), indent=1)
