# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""A Data Gateway Interface (DGI) client for the twin's EDBG: the command
sequence a host tool sends a debugger's DGI, over the chardev the twin
offers it on (`-M <machine>,dgi=<chardev>`; the web view and tools.pp use a
TCP socket).

    async with DgiClient.connect(port) as dgi:
        print(await dgi.sign_on(), await dgi.interfaces())
        await dgi.enable(DGI_I2C)
        data = await dgi.poll(DGI_I2C)

Framing (the same bytes as over a debugger's USB DGI endpoints): a command
is its code, a 16-bit big-endian payload length and the payload; a reply
echoes the code, then 0x80 (OK) or 0xA0 (data).
"""

from __future__ import annotations

import asyncio
import struct

DGI_TIMESTAMP, DGI_SPI, DGI_USART, DGI_I2C, DGI_GPIO = 0x00, 0x20, 0x21, \
    0x22, 0x30
NAMES = {DGI_TIMESTAMP: "Timestamp", DGI_SPI: "SPI", DGI_USART: "USART",
         DGI_I2C: "I2C", DGI_GPIO: "GPIO"}
OK, DATA = 0x80, 0xA0


class DgiError(RuntimeError):
    pass


class DgiClient:
    def __init__(self, reader: asyncio.StreamReader,
                 writer: asyncio.StreamWriter):
        self.reader, self.writer = reader, writer
        self.lock = asyncio.Lock()

    @classmethod
    async def connect(cls, port: int, host: str = "127.0.0.1",
                      timeout: float = 10.0) -> "DgiClient":
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            try:
                return cls(*await asyncio.open_connection(host, port))
            except OSError:
                if loop.time() > deadline:
                    raise
                await asyncio.sleep(0.1)

    async def __aenter__(self) -> "DgiClient":
        return self

    async def __aexit__(self, *exc) -> None:
        try:
            await self.sign_off()
        except (OSError, DgiError, asyncio.IncompleteReadError):
            pass
        self.close()

    def close(self) -> None:
        self.writer.close()

    async def _cmd(self, code: int, payload: bytes = b"") -> tuple[int, bytes]:
        """Send a command; the reply's status and what follows it (the
        caller knows its shape and reads the rest)."""
        self.writer.write(bytes([code]) + struct.pack(">H", len(payload)) +
                          payload)
        await self.writer.drain()
        head = await self.reader.readexactly(2)
        if head[0] != code:
            raise DgiError(f"reply to {code:#04x} was {head.hex(' ')}")
        return head[1], b""

    async def _ok(self, code: int, payload: bytes = b"") -> None:
        async with self.lock:
            st, _ = await self._cmd(code, payload)
        if st != OK:
            raise DgiError(f"command {code:#04x} failed ({st:#04x})")

    async def sign_on(self) -> str:
        async with self.lock:
            st, _ = await self._cmd(0x00)
            n = struct.unpack(">H", await self.reader.readexactly(2))[0]
            name = await self.reader.readexactly(n)
        return name.rstrip(b"\0").decode(errors="replace")

    async def sign_off(self) -> None:
        await self._ok(0x01)

    async def version(self) -> tuple[int, int]:
        async with self.lock:
            await self._cmd(0x02)
            v = await self.reader.readexactly(2)
        return v[0], v[1]

    async def interfaces(self) -> list[int]:
        async with self.lock:
            await self._cmd(0x08)
            n = (await self.reader.readexactly(1))[0]
            return list(await self.reader.readexactly(n))

    async def config(self, iface: int) -> dict[int, int]:
        async with self.lock:
            st, _ = await self._cmd(0x13, bytes([iface]))
            if st != DATA:
                raise DgiError(f"no config for {iface:#04x}")
            n = struct.unpack(">H", await self.reader.readexactly(2))[0]
            body = await self.reader.readexactly(n)
        return {struct.unpack(">H", body[o:o + 2])[0]:
                struct.unpack(">I", body[o + 2:o + 6])[0]
                for o in range(1, len(body) - 5, 6)}

    async def enable(self, iface: int, timestamps: bool = False) -> None:
        await self._ok(0x10, bytes([iface, 2 if timestamps else 1]))

    async def disable(self, iface: int) -> None:
        await self._ok(0x10, bytes([iface, 0]))

    async def send(self, iface: int, data: bytes) -> None:
        await self._ok(0x14, bytes([iface]) + data)

    async def poll(self, iface: int) -> bytes:
        async with self.lock:
            st, _ = await self._cmd(0x15, bytes([iface]))
            if st != DATA:
                raise DgiError(f"poll of {iface:#04x} failed ({st:#04x})")
            hdr = await self.reader.readexactly(3)
            n = struct.unpack(">H", hdr[1:3])[0]
            return await self.reader.readexactly(n)


def data_stream(buf: bytes) -> tuple[list[bytes], bytes]:
    """Data Visualizer's Data Stream packets in @buf: (payloads, the
    unfinished rest). A packet is 0x03, the payload, 0xFC (~0x03); bytes
    outside packets are skipped, as Data Visualizer does."""
    out, i = [], 0
    while True:
        start = buf.find(b"\x03", i)
        if start < 0:
            return out, b""
        end = buf.find(b"\xfc", start + 1)
        if end < 0:
            return out, buf[start:]
        out.append(buf[start + 1:end])
        i = end + 1
