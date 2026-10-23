# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The analog host: netlists, and the lockstep protocol against ngspice
(the test plays the twin's side of pp-qemu's co-simulation port)."""

import math
import socket
import struct

import pytest

from tools.pp_analog import circuit as circ
from tools.pp_analog import ngspice
from tools.pp_analog.launch import HostProcess

RC = """rc on an ADC pin
* PA04 drives 10k into PA02 (ADC0 AIN0 on the PIC32CM MC00), 1u to ground
r1 PA04 PA02 10k
c1 PA02 0 1u
.tran 1u 10m
.end
"""
TAU = (10e3 + circ.R_PAD) * 1e-6


def test_parse_pads_and_vdd():
    c = circ.parse("t\nr1 pa5 out 1k\nr2 out VDD 2k\nd1 PB10 out dmod\n"
                   ".model dmod d\n.subckt x a pc01\nr1 a pc01 1\n.ends\n"
                   ".control\nrun\n.endc\n.tran 1u 1m\n")
    assert c.pads == ["PA05", "PB10"]           # PC01 is the subckt's own
    assert c.node == {"PA05": "pa5", "PB10": "pb10"}
    assert c.uses_vdd and not c.drives_vdd
    assert not any(ln.startswith((".tran", "run")) for ln in c.body)
    lines = c.lines()
    assert "b_pa5 0 pa5 i = v(g_pa5) * (v(t_pa5) - v(pa5))" in lines
    assert "v_pp_vdd vdd 0 dc 0 external" in lines
    assert lines[-1] == ".end" and lines[-3] == ".save none"


def test_parse_own_vdd_and_continuations():
    c = circ.parse("t\nvsup VDD 0 3.3\nr1 PA05\n+ VDD 1k\n")
    assert c.drives_vdd and c.pads == ["PA05"]
    assert "r1 PA05 VDD 1k" in c.body


def test_parse_rejects_two_spellings():
    with pytest.raises(circ.CircuitError, match="different nodes"):
        circ.parse("t\nr1 PA5 PA05 1k\n")


def test_adc_inputs_mc00():
    ain = circ.adc_inputs(circ.machine_device("pic32cm-mc00-cpro"))
    assert ("/machine/soc/adc0", 0) in ain["PA02"]


needs_ngspice = pytest.mark.skipif(not ngspice.available(),
                                   reason="ngspice not installed")


class Twin:
    """The twin's side of the port."""

    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(30)

    def ask(self, *lines):
        data = "".join(f"{ln}\n" for ln in lines).encode()
        self.sock.sendall(struct.pack("<I", len(data)) + data)
        n = struct.unpack("<I", self._exact(4))[0]
        return self._exact(n).decode().splitlines()

    def _exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            assert chunk, "host closed"
            buf += chunk
        return buf


@pytest.fixture
def rc_host(tmp_path):
    path = tmp_path / "rc.cir"
    path.write_text(RC, encoding="utf-8")
    host = HostProcess(path, tick_us=0)
    yield host
    host.kill()


@needs_ngspice
def test_lockstep_rc_step(rc_host):
    twin = Twin(rc_host.port)
    config = twin.ask("hello 1 pic32cm-mc00-cpro 3300")
    assert "watch PA02" in config and "watch PA04" in config
    assert "probe 0 /machine/soc/adc0 0" in config
    assert config[-2:] == ["tick 0", "ok"]
    assert twin.ask("e 0 PA02 z", "e 0 PA04 l", "init 0") == ["ok"]
    ms = 1_000_000
    v = twin.ask(f"e {ms} PA04 h", f"s {ms} 0")
    assert v == ["v 0"]
    for k in range(2, 11):
        uv = int(twin.ask(f"s {k * ms} 0")[0].split()[1])
        want = 3.3 * (1 - math.exp(-(k - 1) * 1e-3 / TAU))
        assert abs(uv / 1e6 - want) < 0.05e-3, (k, uv, want)
    # A time ngspice has passed: interpolated between recent points
    uv = int(twin.ask(f"s {int(9.5 * ms)} 0")[0].split()[1])
    want = 3.3 * (1 - math.exp(-8.5e-3 / TAU))
    assert abs(uv / 1e6 - want) < 5e-3
    # A tick: PA02 (not driven by the MCU) reads high past 0.6 VDD
    assert twin.ask(f"t {20 * ms}") == ["in a 4 4", "ok"]
    twin.sock.close()
    report = rc_host.finish()
    assert report["errors"] == []
    assert report["samples"] == 11 and report["ticks"] == 1


@needs_ngspice
def test_lockstep_edge_between_requests(rc_host):
    """An edge that arrives after ngspice chose its step still lands
    exactly (the real ADC's samples are not on the edge's grid)."""
    twin = Twin(rc_host.port)
    twin.ask("hello 1 pic32cm-mc00-cpro 3300")
    twin.ask("e 0 PA02 z", "e 0 PA04 l", "init 0")
    assert twin.ask("s 200033569 0") == ["v 0"]
    edge = 200042752
    for dt in (523265, 1022945, 5000000):
        uv = int(twin.ask(*([f"e {edge} PA04 h"] if dt == 523265 else []),
                          f"s {edge + dt} 0")[0].split()[1])
        want = 3.3 * (1 - math.exp(-dt * 1e-9 / TAU))
        assert abs(uv / 1e6 - want) < 0.05e-3, (dt, uv, want)
    twin.sock.close()
    assert rc_host.finish()["errors"] == []


@needs_ngspice
def test_bad_netlist_fails_at_start(tmp_path):
    path = tmp_path / "bad.cir"
    path.write_text("bad\nr1 PA04 0 notanumber\n", encoding="utf-8")
    with pytest.raises(RuntimeError, match="analog host"):
        HostProcess(path)
