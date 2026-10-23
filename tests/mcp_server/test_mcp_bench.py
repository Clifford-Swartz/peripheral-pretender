# Copyright (c) 2026 Clifford Swartz.
"""The bench_* MCP tools on a live engine: found through the file it
leaves, then status, the timeline, faults, a Click control and the
console."""

import json
import os
import socket
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

import anyio
import pytest
from mcp import ClientSession
from mcp.client.stdio import stdio_client

from test_mcp_server import PROJECT_ROOT, field, server_params

THERMO = PROJECT_ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" \
    / "thermo.elf"


def test_timeline_rows_compact_for_an_assistant(monkeypatch):
    from tools.pp_mcp import server
    monkeypatch.setattr(server, "MAX_EVENTS", 2)
    t0 = 5_000_000_000
    i2c = {"label": "SERCOM2 I2C", "kind": "i2c", "data": {"items": [
        {"t": t0 + 1_000_000, "end": t0 + 1_000_800, "text": "0x18 W 05"},
        {"t": t0 + 2_000_000, "end": t0 + 2_000_800, "text": "0x18 R 01 90"},
        {"t": t0 + 3_000_000, "end": None, "count": 40}]}}
    out = server._compact_row(i2c, t0)
    assert out["events"] == [[2.0, "0x18 R 01 90"],
                             [3.0, "40 merged; ask for a shorter window"]]
    assert out["dropped"] == 1
    pad = {"label": "LED0 PA24", "kind": "pad", "data": {
        "level": 1, "edges": [[t0 + 250_000_000, 0]],
        "busy": [[t0, t0 + 1_000_000, 0.5]]}}
    assert server._compact_row(pad, t0) == {
        "label": "LED0 PA24", "kind": "pad", "level_at_start": 1,
        "busy": [[0.0, 1.0, 0.5]], "events": [[250.0, 0]]}
    irq = {"label": "TC0", "kind": "irq",
           "data": {"spans": [[t0 + 10_000, t0 + 12_500]], "pends": 1}}
    out = server._compact_row(irq, t0)
    assert out["events"] == [[0.01, 2.5]] and out["runs"] == 1


def _port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.mark.qemu
def test_bench_tools_drive_a_live_engine(qemu_path, qemu_machines, tmp_path):
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file():
        pytest.skip("thermo.elf not built")
    engines = tmp_path / "engines"
    ws = tmp_path / "workspace.json"
    ws.write_text(json.dumps({"version": 2, "boards": [
        {"key": "b1", "type": "ev15n46a", "at": [0, 0],
         "clicks": {"ext1": {"id": "thermo8",
                             "values": {"temperature": 25}}},
         "firmware": str(THERMO)}]}), encoding="utf-8")
    env = {**os.environ, "PP_WEB_ENGINES": str(engines),
           "PP_QEMU": str(qemu_path)}
    port = _port()
    engine = subprocess.Popen([sys.executable, "-m", "tools.pp_web", "--port",
                               str(port), "--workspace", str(ws)],
                              cwd=PROJECT_ROOT, env=env,
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 60
        while not list(engines.glob("*.json")):
            assert time.monotonic() < deadline and engine.poll() is None
            time.sleep(0.2)
        while True:
            with urllib.request.urlopen(
                    f"http://127.0.0.1:{port}/api/bench") as f:
                if json.loads(f.read())["boards"][0]["timeline"]:
                    break
            assert time.monotonic() < deadline
            time.sleep(0.3)
        time.sleep(1.0)
        results = anyio.run(_session, env)
    finally:
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(engine.pid)],
                           capture_output=True)
        else:
            engine.terminate()
    (status, timeline, faulted, after, cleared, bad, uart, stopped,
     replay) = results
    assert status["engine"] == f"http://127.0.0.1:{port}"     # found it
    assert status["boards"][0]["clicks"] == {"ext1": "thermo8"}
    i2c = next(r for r in timeline["rows"] if r["kind"] == "i2c")
    assert i2c["label"] == "SERCOM2 I2C" and \
        any("0x18 R" in e[1] for e in i2c["events"])
    assert faulted["faults"] == [{"kind": "nack", "addr": 0x18,
                                  "slot": "ext1"}]
    assert after["rows"][0]["events"] and \
        all(e[1] == "0x18 W NACK" for e in after["rows"][0]["events"])
    assert cleared["faults"] == []
    assert "no pad PZ1" in bad
    assert uart["matched"] and "HIGH alert" in uart["text"]
    assert stopped["recordings"] == ["mcp-run"] and not stopped["recording"]
    assert replay["replay"]["state"] == "same", replay


async def _session(env):
    params = server_params()
    params.env.update({k: env[k] for k in ("PP_WEB_ENGINES", "PP_QEMU")})
    with anyio.fail_after(150):
        async with stdio_client(params) as streams:
            async with ClientSession(*streams) as c:
                await c.initialize()

                async def call(name, args):
                    r = await c.call_tool(name, args)
                    if field(r, "isError"):
                        return r.content[0].text
                    return field(r, "structuredContent")

                status = await call("bench_status", {})
                timeline = await call("bench_timeline",
                                      {"last_ms": 1000, "kinds": ["i2c"]})
                faulted = await call("bench_faults", {"faults": [
                    {"kind": "nack", "slot": "ext1"}]})
                await anyio.sleep(1.0)
                after = await call("bench_timeline",
                                   {"last_ms": 500, "kinds": ["i2c"]})
                cleared = await call("bench_faults", {"clear": True})
                bad = await call("bench_faults", {"faults": [
                    {"kind": "short", "pad": "PZ1"}]})
                # What the firmware prints right away is not missed: wait
                # from the mark before the control moved
                ctl = await call("bench_control", {"slot": "ext1",
                                                   "control": "temperature",
                                                   "value": 35})
                await anyio.sleep(1.0)
                uart = await call("bench_uart", {
                    "wait_for": r"T=\+35", "timeout_s": 8,
                    "since": ctl["uart_mark_before"]})
                # Record a run with the slider moved, then replay it
                await call("bench_record", {"action": "start"})
                await anyio.sleep(0.5)
                await call("bench_control", {"slot": "ext1",
                                             "control": "temperature",
                                             "value": 15})
                await anyio.sleep(0.5)
                stopped = await call("bench_record", {"action": "stop",
                                                      "name": "mcp-run"})
                replay = await call("bench_replay", {"name": "mcp-run"})
                return (status, timeline, faulted, after, cleared, bad, uart,
                        stopped, replay)
