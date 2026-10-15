# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""One running twin: QEMU with a firmware image, its QMP monitor and its
VCOM UART, plus the board-level state the web view shows.

QEMU runs with icount paced to the host clock (shift=auto, sleep=on), so
a 2 Hz blink looks like 2 Hz and timer events land at exact virtual
times. Each frame the PORT model's "pX-stats" give every pad's driven-high
and driven-low time; their differences are the duty cycles the page shows
(a PWM-dimmed LED at its brightness).
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

POLL_S = 1 / 30
QMP_LINE_LIMIT = 4 * 1024 * 1024
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
    click_state: dict[str, object] = field(default_factory=dict)
    proc: asyncio.subprocess.Process | None = None
    qmp: Qmp | None = None
    uart_writer: asyncio.StreamWriter | None = None
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
        args = [str(find_qemu(target=target)), "-M", self.machine,
                "-kernel", str(self.elf),
                "-display", "none", "-monitor", "none",
                "-icount", "shift=auto,sleep=on",
                "-chardev", f"socket,id=vcom,host=127.0.0.1,port={uart_port},"
                            "server=on,wait=on",
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
        self.qmp = await Qmp.connect(qmp_port)
        self.tasks = [asyncio.create_task(self._uart(reader)),
                      asyncio.create_task(self._poll()),
                      asyncio.create_task(self._watch())]
        self.emit({"type": "status", "state": "running", "elf": self.elf.name})

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
            if pressed:
                await self.qmp.execute("system_reset")
            return
        prop = prop or ("sw0-pressed" if button == "sw0" else None)
        if prop:
            await self.qmp.execute("qom-set", path="/machine",
                                   property=prop, value=pressed)

    async def set_prop(self, path: str, prop: str,
                       value: int | bool | str) -> None:
        """Set QOM property @prop of @path (a part on the board)."""
        await self.qmp.execute("qom-set", path=path, property=prop,
                               value=value)

    async def set_click(self, slot: str, prop: str,
                        value: int | bool | str) -> None:
        """Set a QOM property of the Click board in @slot ("ext1")."""
        await self.qmp.execute("qom-set", path=f"/machine/{slot}-click",
                               property=prop, value=value)

    async def uart_send(self, text: str) -> None:
        if self.uart_writer:
            self.uart_writer.write(text.encode("latin-1", errors="replace"))
            await self.uart_writer.drain()

    async def stop(self) -> None:
        for t in self.tasks:
            t.cancel()
        if self.qmp:
            with contextlib.suppress(Exception):
                await self.qmp.execute("quit")
            self.qmp.close()
        if self.proc and self.proc.returncode is None:
            with contextlib.suppress(ProcessLookupError):
                self.proc.kill()
            await self.proc.wait()
