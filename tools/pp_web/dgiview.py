# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The page's Data Visualizer panel: the web server as a DGI host on the
twin's debugger (tools.pp.dgi), like Data Visualizer on a real one.

Once the twin runs, the server signs on and tells the page what the kit's
DGI offers ({"type": "dgiinfo"}). The page turns interfaces on and off
({"type": "dgi", "enable": [ids]}); the server keeps those on (GPIO with
timestamps) and polls them 20 times a second, sending {"type": "dgidata",
"iface": id, "data": <base64>} for byte interfaces and {"type":
"dgigpio", "edges": [[t_us, levels], ...]} for GPIO (t_us from the
debugger's timestamp clock, since GPIO was turned on).
"""

from __future__ import annotations

import asyncio
import base64
import contextlib
import struct
from typing import Callable

from tools.pp.dgi import (DGI_GPIO, DGI_TIMESTAMP, NAMES, DgiClient,
                          DgiError)

POLL_S = 0.05


class DgiView:
    def __init__(self, port: int, emit: Callable[[dict], None]):
        self.port, self.emit = port, emit
        self.client: DgiClient | None = None
        self.want: set[int] = set()
        self.on: set[int] = set()
        self.ifaces: list[int] = []
        self.tick_hz = 2_000_000
        self.info: dict | None = None
        self.task: asyncio.Task | None = None

    async def start(self) -> None:
        self.client = await DgiClient.connect(self.port)
        name = await self.client.sign_on()
        major, minor = await self.client.version()
        self.ifaces = await self.client.interfaces()
        if DGI_TIMESTAMP in self.ifaces:
            self.tick_hz = (await self.client.config(DGI_TIMESTAMP)).get(
                1, self.tick_hz) or self.tick_hz
        self.info = {"type": "dgiinfo", "name": name,
                     "version": f"{major}.{minor}",
                     "interfaces": [{"id": i, "name": NAMES.get(i, hex(i))}
                                    for i in self.ifaces
                                    if i != DGI_TIMESTAMP]}
        self.emit(self.info)
        self.task = asyncio.create_task(self._loop())

    def set_wanted(self, ids) -> None:
        self.want = {int(i) for i in ids if int(i) in self.ifaces}

    async def _loop(self) -> None:
        c = self.client
        while True:
            for i in self.want - self.on:
                await c.enable(i, timestamps=(i == DGI_GPIO))
                self.on.add(i)
            for i in self.on - self.want:
                await c.disable(i)
                self.on.discard(i)
            for i in sorted(self.on):
                data = await c.poll(i)
                if not data:
                    continue
                if i == DGI_GPIO:
                    edges = [[struct.unpack(">I", data[o:o + 4])[0] * 1e6 /
                              self.tick_hz, data[o + 4]]
                             for o in range(0, len(data) - 4, 5)]
                    self.emit({"type": "dgigpio", "edges": edges})
                else:
                    self.emit({"type": "dgidata", "iface": i,
                               "data": base64.b64encode(data).decode()})
            await asyncio.sleep(POLL_S)

    async def stop(self) -> None:
        if self.task:
            self.task.cancel()
            with contextlib.suppress(asyncio.CancelledError, Exception):
                await self.task
        if self.client:
            # Bounded: the twin's QEMU may be on its way out
            with contextlib.suppress(OSError, DgiError, asyncio.TimeoutError,
                                     asyncio.IncompleteReadError):
                await asyncio.wait_for(self.client.sign_off(), 2.0)
            self.client.close()
