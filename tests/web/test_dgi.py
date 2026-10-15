# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The twin's EDBG data gateway: the DGI command protocol on its chardev,
and what the target sends it (dgi.elf: Data Stream packets over I2C every
10 ms, DGI GPIO0 toggled with them)."""

import asyncio
import socket
from pathlib import Path

import pytest

from tools.pp.dgi import (DGI_GPIO, DGI_I2C, DGI_SPI, DGI_TIMESTAMP,
                          DgiClient, data_stream)

ROOT = Path(__file__).resolve().parents[2]
DGI_ELF = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "dgi.elf"


def test_data_stream_packets():
    pk, rest = data_stream(b"\x00\x03\x01\x02\xfc\x03\x05\x00\xfc\x03\x07")
    assert pk == [b"\x01\x02", b"\x05\x00"] and rest == b"\x03\x07"


def _free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.mark.qemu
def test_dgi_on_the_mc00(qemu_path, qemu_machines):
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not DGI_ELF.is_file():
        pytest.skip("dgi.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=dgi)")
    from tools.pp.qemu import qemu_env

    async def scenario():
        port = _free_port()
        proc = await asyncio.create_subprocess_exec(
            str(qemu_path), "-M", "pic32cm-mc00-cpro,dgi=dgi",
            "-kernel", str(DGI_ELF), "-display", "none", "-monitor", "none",
            "-serial", "null", "-icount", "shift=auto,sleep=on",
            "-chardev", f"socket,id=dgi,host=127.0.0.1,port={port},"
                        "server=on,wait=off", env=qemu_env())
        try:
            async with await DgiClient.connect(port) as d:
                out = {"name": await d.sign_on(),
                       "version": await d.version(),
                       "ifaces": await d.interfaces(),
                       "ts": await d.config(DGI_TIMESTAMP)}
                # Nothing is taken before the host turns an interface on
                await asyncio.sleep(0.1)
                out["before"] = await d.poll(DGI_I2C)
                await d.enable(DGI_I2C)
                await d.enable(DGI_GPIO, timestamps=True)
                await asyncio.sleep(0.3)
                out["i2c"] = await d.poll(DGI_I2C)
                out["gpio"] = await d.poll(DGI_GPIO)
                out["spi"] = await d.poll(DGI_SPI)
            return out
        finally:
            if proc.returncode is None:
                proc.kill()
            await proc.wait()

    out = asyncio.run(scenario())
    assert out["name"] == "EDBG Data Gateway Interface"
    assert out["version"] == (4, 1)
    assert out["ifaces"] == [DGI_TIMESTAMP, DGI_SPI, DGI_I2C, DGI_GPIO]
    assert out["ts"] == {0: 1, 1: 2_000_000}
    assert out["before"] == b"" and out["spi"] == b""
    # Data Stream packets with a counter that steps by one
    packets, _ = data_stream(out["i2c"])
    counts = [int.from_bytes(p, "little") for p in packets]
    assert len(counts) >= 15
    assert all(b - a == 1 for a, b in zip(counts, counts[1:])), counts
    # GPIO: the levels at the start, then an edge every 10 ms (20000 ticks
    # of the 2 MHz timestamp clock)
    recs = [out["gpio"][o:o + 5] for o in range(0, len(out["gpio"]), 5)]
    ts = [int.from_bytes(r[:4], "big") for r in recs]
    assert ts[0] < 100 and len(recs) >= 15
    steps = [b - a for a, b in zip(ts[1:], ts[2:])]
    assert all(abs(s - 20000) <= 40 for s in steps), steps
    levels = [r[4] & 1 for r in recs[1:]]
    assert all(a != b for a, b in zip(levels, levels[1:]))
