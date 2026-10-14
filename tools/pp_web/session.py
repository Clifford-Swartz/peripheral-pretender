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
import contextlib
import json
import socket
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from tools.pp.qemu import elf_target, find_qemu, machine_target, qemu_env

POLL_S = 1 / 30
PORT_PATH = "/machine/soc/port"


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
                reader, writer = await asyncio.open_connection("127.0.0.1", port)
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
    watch: list[tuple[str, str]] = field(default_factory=list)  # Click props
    click_state: dict[str, object] = field(default_factory=dict)
    proc: asyncio.subprocess.Process | None = None
    qmp: Qmp | None = None
    uart_writer: asyncio.StreamWriter | None = None
    tasks: list[asyncio.Task] = field(default_factory=list)
    pins: dict[str, list[list[float]]] = field(default_factory=dict)
    levels: dict[str, int] = field(default_factory=dict)
    _stats: dict[str, list[int]] = field(default_factory=dict)

    async def start(self) -> None:
        qmp_port, uart_port = free_port(), free_port()
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
        while data := await reader.read(4096):
            self.emit({"type": "uart", "data": data.decode("latin-1")})

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
            state = {}
            for slot, prop in self.watch:
                state[f"{slot}.{prop}"] = await self.qmp.execute(
                    "qom-get", path=f"/machine/{slot}-click", property=prop)
            if state != self.click_state:
                self.click_state = state
                self.emit({"type": "clickstate", "state": state})
            await asyncio.sleep(POLL_S)

    async def _watch(self) -> None:
        err = await self.proc.stderr.read()
        code = await self.proc.wait()
        self.emit({"type": "status", "state": "stopped", "code": code,
                   "stderr": err.decode(errors="replace")[-2000:]})

    async def press(self, button: str, pressed: bool) -> None:
        if button == "sw0":
            await self.qmp.execute("qom-set", path="/machine",
                                   property="sw0-pressed", value=pressed)
        elif button == "reset" and pressed:
            await self.qmp.execute("system_reset")

    async def set_click(self, slot: str, prop: str, value: int | bool) -> None:
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
