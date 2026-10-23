# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""One running twin: QEMU with a firmware image, its QMP monitor and its
VCOM UART, plus the board-level state the web view shows.

QEMU runs with icount paced to the host clock (shift=auto, sleep=on), so
a 2 Hz blink looks like 2 Hz and timer events land at exact virtual
times. Each frame the PORT model's "pX-stats" give every pad's driven-high
and driven-low time; their differences are the duty cycles the page shows
(a PWM-dimmed LED at its brightness). Where the machine has pp-qemu's event
recorder, its stream (every pad edge, SERCOM character and exception, in
virtual time) fills a trace store for the timeline.

A deterministic session replays exactly: a fixed icount (shift 4,
sleep=off) paced to real time by the recorder, started paused until go()
so its starting settings are in place, and every input (a button, a Click
control, the console) sent through the recorder, which applies it at a
virtual time and reports which. recording() gives those back; a session
given a recording repeats them at their times and takes no live input.
"""

from __future__ import annotations

import asyncio
import codecs
import contextlib
import json
import re
import socket
from dataclasses import dataclass, field
from pathlib import Path
from typing import Awaitable, Callable

from tools.pp.qemu import elf_target, find_qemu, machine_target, qemu_env
from tools.pp.trace import Store, machine_has_trace

POLL_S = 1 / 30
QMP_LINE_LIMIT = 4 * 1024 * 1024
DETERMINISTIC_ICOUNT = "shift=4,sleep=off"      # 16 ns an instruction
STOP_S = 3.0                    # the most stopping QEMU may take per step
PORT_PATH = "/machine/soc/port"
# Terminal control sequences (CircuitPython sets the window title with
# OSC and moves the cursor with CSI); the page shows plain text
TERM_ESC = re.compile(r"\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)"
                      r"|\x1b\[[0-9;?]*[ -/]*[@-~]")
TERM_PARTIAL = re.compile(r"\x1b(?:\][^\x07\x1b]*\x1b?"
                          r"|\[[0-9;?]*[ -/]*)?$")


def plain_text(pending: str, chunk: str) -> tuple[str, str]:
    """@pending + @chunk without terminal control sequences; a sequence
    cut at the end of the chunk is held back (the second value)."""
    text = TERM_ESC.sub("", pending + chunk)
    cut = TERM_PARTIAL.search(text)
    if cut:
        return text[:cut.start()], text[cut.start():]
    return text, ""


def qom_text(value) -> str:
    """@value as QOM parses a property from text (true, 35500, "1000 0")"""
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Qmp:
    """Minimal asyncio QMP client."""

    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self.reader, self.writer = reader, writer
        self.lock = asyncio.Lock()

    @classmethod
    async def connect(cls, port: int, timeout: float = 20.0) -> "Qmp":
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            try:
                # A panel's frame is one long line (a 240x135 TFT: 86 KiB)
                reader, writer = await asyncio.open_connection(
                    "127.0.0.1", port, limit=QMP_LINE_LIMIT)
                break
            except OSError:
                if loop.time() > deadline:
                    raise
                await asyncio.sleep(0.1)
        qmp = cls(reader, writer)
        await qmp._recv()                       # greeting
        await qmp.execute("qmp_capabilities")
        return qmp

    async def _recv(self) -> dict:
        while True:
            line = await self.reader.readline()
            if not line:
                raise ConnectionError("QMP closed")
            msg = json.loads(line)
            if "event" not in msg:
                return msg

    async def execute(self, cmd: str, **args) -> object:
        async with self.lock:
            body = {"execute": cmd}
            if args:
                body["arguments"] = args
            self.writer.write((json.dumps(body) + "\n").encode())
            await self.writer.drain()
            msg = await self._recv()
        if "error" in msg:
            raise RuntimeError(f"{cmd}: {msg['error'].get('desc')}")
        return msg.get("return")

    def close(self) -> None:
        self.writer.close()


@dataclass
class Session:
    """A twin running @elf on @machine; @emit(dict) gets state updates."""

    machine: str
    elf: Path
    emit: Callable[[dict], None]
    groups: tuple[str, ...] = ("a", "b")
    extra_args: tuple[str, ...] = ()        # more QEMU arguments (-d, -D)
    # Properties the page shows: (key, QOM path, property) or (key, path,
    # property, generation property), the last read only when the
    # generation changes; key is "<slot or part>.<property>"
    watch: list[tuple[str, ...]] = field(default_factory=list)
    # The board's external flash image (-drive if=mtd): a CIRCUITPY drive
    drive: Path | None = None
    # Called each frame with {"pins", "pulls"} while the board's parts
    # bench has anything on it (the bench solves and drives pads)
    bench: Callable[[dict], Awaitable[None]] | None = None
    bench_active: bool = False
    bench_groups: tuple[str, ...] = ()      # PORT groups of the bench's pads
    # Peripherals that drive pads with a voltage (board.json "analog_out"):
    # (pad, QOM path, property in mV, < 0 while not driving)
    bench_analog: tuple[tuple[str, str, str], ...] = ()
    # The debugger's data gateway (the machine's "dgi=dgi" chardev): the
    # TCP port its DGI host side listens on once started
    dgi: bool = False
    dgi_port: int | None = None
    # The event recorder, if the machine has one: what it saw
    trace: bool = True
    trace_store: Store | None = None
    # A run that replays exactly (needs the recorder), paced to real time
    # or as fast as it goes; a recording to repeat
    deterministic: bool = False
    pace: bool = True
    replay: dict | None = None
    vcom: str | None = None             # its VCOM ("sercom0", "usart1")
    initial: list = field(default_factory=list)  # [path, prop, value]
    running: bool = False               # a deterministic one: go() was
    _actions: dict = field(default_factory=dict)  # id -> action
    _next_id: int = 1
    click_state: dict[str, object] = field(default_factory=dict)
    proc: asyncio.subprocess.Process | None = None
    qmp: Qmp | None = None
    uart_writer: asyncio.StreamWriter | None = None
    trace_writer: asyncio.StreamWriter | None = None
    tasks: list[asyncio.Task] = field(default_factory=list)
    pins: dict[str, list[list[float]]] = field(default_factory=dict)
    levels: dict[str, int] = field(default_factory=dict)
    _stats: dict[str, list[int]] = field(default_factory=dict)
    _gens: dict[str, object] = field(default_factory=dict)

    async def start(self) -> None:
        qmp_port, uart_port = free_port(), free_port()
        self.dgi_port = free_port() if self.dgi else None
        # Arm firmware runs on qemu-system-arm, AVR on qemu-system-avr
        target = elf_target(self.elf) or machine_target(self.machine)
        qemu = find_qemu(target=target)
        self.deterministic = self.deterministic or bool(self.replay)
        trace_port = free_port() if (self.trace or self.deterministic) and \
            await asyncio.to_thread(machine_has_trace, qemu, self.machine,
                                    qemu_env()) else None
        if self.deterministic and not trace_port:
            raise RuntimeError("this QEMU build has no event recorder for "
                               f"{self.machine.split(',')[0]}: it cannot "
                               "record or replay")
        machine = self.machine + (",trace=pptrace" if trace_port else "")
        # QEMU waits for the VCOM client, then the recorder's, then runs
        # (a deterministic run: when go() says)
        args = [str(qemu), "-M", machine,
                "-kernel", str(self.elf),
                "-display", "none", "-monitor", "none",
                "-icount", DETERMINISTIC_ICOUNT if self.deterministic
                else "shift=auto,sleep=on",
                *(["-S"] if self.deterministic else []),
                "-chardev", f"socket,id=vcom,host=127.0.0.1,port={uart_port},"
                            "server=on,wait=on",
                *(["-chardev", f"socket,id=pptrace,host=127.0.0.1,"
                               f"port={trace_port},server=on,wait=on"]
                  if trace_port else []),
                "-serial", "chardev:vcom",
                "-qmp", f"tcp:127.0.0.1:{qmp_port},server=on,wait=off",
                *(["-drive", f"if=mtd,format=raw,file={self.drive}"]
                  if self.drive else []),
                *(["-chardev", f"socket,id=dgi,host=127.0.0.1,"
                               f"port={self.dgi_port},server=on,wait=off"]
                  if self.dgi else []),
                *self.extra_args]
        self.proc = await asyncio.create_subprocess_exec(
            *args, env=qemu_env(), stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE)
        # QEMU waits for the UART client before running the firmware
        reader, self.uart_writer = await self._connect(uart_port)
        self.tasks = [asyncio.create_task(self._uart(reader))]
        if trace_port:
            treader, self.trace_writer = await self._connect(trace_port)
            self.trace_store = Store()
            self.tasks.append(asyncio.create_task(self._trace(treader)))
        self.qmp = await Qmp.connect(qmp_port)
        self.tasks += [asyncio.create_task(self._poll()),
                       asyncio.create_task(self._watch())]
        if not self.deterministic:
            self.emit({"type": "status", "state": "running",
                       "elf": self.elf.name})

    async def go(self) -> None:
        """Run a deterministic session from its starting settings: a
        replay's first, then its inputs wait for their times."""
        if self.replay:
            for path, prop, value in self.replay.get("initial", []):
                await self.qmp.execute("qom-set", path=path, property=prop,
                                       value=value)
            for t, *action in self.replay.get("inputs", []):
                self._send_at(t, tuple(action))
        self._trace_line(f"pace {1 if self.pace else 0}")
        await self.qmp.execute("cont")
        self.running = True
        self.emit({"type": "status", "state": "running",
                   "elf": self.elf.name})

    def _trace_line(self, line: str) -> None:
        self.trace_writer.write((line + "\n").encode())

    def _send_at(self, t: int, action: tuple) -> None:
        """@action ("set", path, prop, value) or ("reset",) through the
        recorder, at virtual time @t (0: now)"""
        i = self._next_id
        self._next_id += 1
        self._actions[i] = action
        if action[0] == "reset":
            self._trace_line(f"at {t} {i} reset")
        else:
            self._trace_line(f"at {t} {i} set {action[1]} {action[2]} "
                             f"{qom_text(action[3])}")

    async def _act(self, path: str, prop: str, value) -> None:
        """Set a property of the board: through the recorder in a running
        deterministic session, kept as a starting setting before go(), and
        never in a replay (it has its own inputs)"""
        if self.replay:
            return
        if self.deterministic and self.running:
            self._send_at(0, ("set", path, prop, value))
            return
        if self.deterministic:
            self.initial.append([path, prop, value])
        await self.qmp.execute("qom-set", path=path, property=prop,
                               value=value)

    def recording(self) -> dict:
        """What a replay needs: the starting settings and each input with
        the virtual time it took effect"""
        inputs = [[t, *self._actions[i]]
                  for t, i, ok in (self.trace_store.inputs
                                   if self.trace_store else [])
                  if ok and i in self._actions]
        return {"initial": list(self.initial), "inputs": inputs}

    async def _connect(self, port: int):
        loop = asyncio.get_running_loop()
        deadline = loop.time() + 20
        while True:
            try:
                return await asyncio.open_connection("127.0.0.1", port)
            except OSError:
                if loop.time() > deadline or self.proc.returncode is not None:
                    raise RuntimeError("QEMU did not open the VCOM socket")
                await asyncio.sleep(0.1)

    async def _uart(self, reader: asyncio.StreamReader) -> None:
        # UTF-8 (CircuitPython's banner has an emoji), control sequences
        # dropped
        dec = codecs.getincrementaldecoder("utf-8")(errors="replace")
        pending = ""
        while data := await reader.read(4096):
            text, pending = plain_text(pending, dec.decode(data))
            if text:
                self.emit({"type": "uart", "data": text})

    async def _trace(self, reader: asyncio.StreamReader) -> None:
        while data := await reader.read(1 << 16):
            self.trace_store.feed(data)

    async def _poll(self) -> None:
        """Per-pad [driven-high, driven-low] fractions and every pad's
        level (external drivers and pulls too), each sent on change."""
        while True:
            pins, levels = {}, {}
            for g in self.groups:
                levels[g] = int(await self.qmp.execute(
                    "qom-get", path=PORT_PATH, property=f"p{g}-levels"))
                text = await self.qmp.execute("qom-get", path=PORT_PATH,
                                              property=f"p{g}-stats")
                stats = [int(v) for v in str(text).split()]
                prev = self._stats.get(g)
                self._stats[g] = stats
                if not prev or stats[0] <= prev[0]:
                    continue
                dt = stats[0] - prev[0]
                # 32 pads a group on Arm parts, 8 on AVR
                pins[g] = [[round((stats[1 + 2 * n] - prev[1 + 2 * n]) / dt, 2),
                            round((stats[2 + 2 * n] - prev[2 + 2 * n]) / dt, 2)]
                           for n in range((len(stats) - 1) // 2)]
            if pins and pins != self.pins:
                self.pins = pins
                self.emit({"type": "pins", "pins": pins})
            if levels != self.levels:
                self.levels = levels
                self.emit({"type": "levels", "levels": levels})
            if self.bench and self.bench_active:
                pulls = {}
                for g in self.bench_groups:
                    mask, up = str(await self.qmp.execute(
                        "qom-get", path=PORT_PATH,
                        property=f"p{g}-pull")).split()
                    pulls[g] = (int(mask, 16), int(up, 16))
                analog = {}
                for pad, path, prop in self.bench_analog:
                    with contextlib.suppress(RuntimeError):
                        analog[pad] = int(await self.qmp.execute(
                            "qom-get", path=path, property=prop))
                await self.bench({"pins": self.pins, "pulls": pulls,
                                  "analog": analog})
            state = {}
            for key, path, prop, *gen in self.watch:
                if gen:
                    g = await self.qmp.execute("qom-get", path=path,
                                               property=gen[0])
                    if key in self.click_state and self._gens.get(key) == g:
                        state[key] = self.click_state[key]
                        continue
                    self._gens[key] = g
                state[key] = await self.qmp.execute("qom-get", path=path,
                                                    property=prop)
            if state != self.click_state:
                self.click_state = state
                self.emit({"type": "clickstate", "state": state})
            await asyncio.sleep(POLL_S)

    async def _watch(self) -> None:
        err = await self.proc.stderr.read()
        code = await self.proc.wait()
        self.emit({"type": "status", "state": "stopped", "code": code,
                   "stderr": err.decode(errors="replace")[-2000:]})

    async def press(self, button: str, pressed: bool,
                    prop: str | None = None) -> None:
        """Press or release a board button: "reset", or a machine bool
        property (@prop; "sw0" means "sw0-pressed")."""
        if button == "reset":
            if not pressed or self.replay:
                return
            if self.deterministic and self.running:
                self._send_at(0, ("reset",))
            else:
                await self.qmp.execute("system_reset")
            return
        prop = prop or ("sw0-pressed" if button == "sw0" else None)
        if prop:
            await self._act("/machine", prop, pressed)

    async def set_prop(self, path: str, prop: str,
                       value: int | bool | str) -> None:
        """Set QOM property @prop of @path (a part on the board)."""
        await self._act(path, prop, value)

    async def set_click(self, slot: str, prop: str,
                        value: int | bool | str) -> None:
        """Set a QOM property of the Click board in @slot ("ext1")."""
        await self._act(f"/machine/{slot}-click", prop, value)

    async def uart_send(self, text: str) -> None:
        data = text.encode("latin-1", errors="replace")
        if self.deterministic and self.vcom:
            # Into the VCOM's receiver, at a time the recorder knows
            await self._act(f"/machine/soc/{self.vcom}", "rx-inject",
                            data.hex())
        elif self.uart_writer:
            self.uart_writer.write(data)
            await self.uart_writer.drain()

    async def stop(self) -> None:
        for t in self.tasks:
            t.cancel()
        if self.trace_writer:
            self.trace_writer.close()
        # Bounded: a QEMU that does not answer must not stall a board switch
        if self.qmp:
            with contextlib.suppress(Exception):
                await asyncio.wait_for(self.qmp.execute("quit"), STOP_S)
            self.qmp.close()
        if self.proc and self.proc.returncode is None:
            with contextlib.suppress(ProcessLookupError):
                self.proc.kill()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(self.proc.wait(), STOP_S)
