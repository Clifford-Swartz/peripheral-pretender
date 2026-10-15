# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The parts bench: the DC solver, and a bench wired to a running twin."""

import asyncio
import json
import time
from pathlib import Path

import pytest

from tools.pp_web.bench import Bench, Pad, clean, new_part, pad_drives
from tools.pp_web.server import App, device_pads

ROOT = Path(__file__).resolve().parents[2]
BLINKY = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "blinky.elf"


def part(ptype, pid, **props):
    return {**new_part(ptype, pid), **props}


def test_divider_and_meter():
    b = Bench({"parts": [part("resistor", "r1", ohms=1000),
                         part("resistor", "r2", ohms=2000),
                         part("voltmeter", "vm")],
               "wires": [["rail:3V3", "r1.a"], ["r1.b", "r2.a"],
                         ["r2.b", "rail:GND"], ["vm.p", "r2.a"],
                         ["vm.n", "rail:GND"]]})
    assert b.solve({}).parts["vm"]["volts"] == pytest.approx(2.2, abs=0.005)


def test_led_current_follows_drive_and_duty():
    b = Bench({"parts": [part("resistor", "r1", ohms=330),
                         part("led", "d1", color="red")],
               "wires": [["pin:PA05", "r1.a"], ["r1.b", "d1.a"],
                         ["d1.k", "rail:GND"]]})
    on = b.solve({"PA05": Pad(high=1)})
    # (3.3 V - 1.9 V - the pad's drop) / 330 ohm: about 4 mA
    assert 3.5e-3 < on.parts["d1"]["amps"] < 4.5e-3
    assert on.pads["PA05"]["amps"] == pytest.approx(on.parts["d1"]["amps"],
                                                    rel=0.01)
    half = b.solve({"PA05": Pad(high=0.5, low=0.5)})
    assert half.parts["d1"]["amps"] == pytest.approx(
        on.parts["d1"]["amps"] / 2, rel=0.02)
    assert b.solve({"PA05": Pad(low=1)}).parts["d1"]["amps"] < 1e-6
    assert not on.warnings


def test_led_without_resistor_warns():
    b = Bench({"parts": [part("led", "d1")],
               "wires": [["pin:PA05", "d1.a"], ["d1.k", "rail:GND"]]})
    res = b.solve({"PA05": Pad(high=1)})
    assert "warn" in res.parts["d1"] and "warn" in res.pads["PA05"]


def test_button_reads_through_the_pads_pull():
    b = Bench({"parts": [part("button", "sw")],
               "wires": [["pin:PA25", "sw.a"], ["sw.b", "rail:GND"]]})
    assert b.solve({"PA25": Pad(pull=1)}).inputs == {"PA25": 1}
    b.data["parts"][0]["pressed"] = True
    res = b.solve({"PA25": Pad(pull=1)})
    assert res.inputs == {"PA25": 0}
    assert pad_drives(res.inputs, "ab") == {"a": (1 << 25, 0), "b": (0, 0)}
    # A pad the MCU drives is not driven by the bench
    assert b.solve({"PA25": Pad(high=1)}).inputs == {}


def test_pot_motor_transistor_and_servo():
    b = Bench({"parts": [part("pot", "p", position=0.25),
                         part("motor", "m"), part("nmos", "q"),
                         part("diode", "d"), part("servo", "s")],
               "wires": [["rail:3V3", "p.a"], ["p.b", "rail:GND"],
                         ["p.w", "pin:PB08"],
                         ["rail:5V", "m.a"], ["m.b", "q.d"],
                         ["q.s", "rail:GND"], ["q.g", "pin:PA10"],
                         ["d.a", "m.b"], ["d.k", "rail:5V"],
                         ["rail:5V", "s.vcc"], ["s.gnd", "rail:GND"],
                         ["s.sig", "pin:PA11"]]})
    res = b.solve({"PA10": Pad(high=1), "PA11": Pad(high=0.075, low=0.925)})
    assert res.volts["PB08"] == pytest.approx(2.475, abs=0.01)
    assert res.parts["m"]["rpm"] > 4500 and "warn" not in res.parts["m"]
    assert res.parts["s"]["angle"] == pytest.approx(90, abs=0.5)
    off = b.solve({"PA10": Pad(low=1), "PA11": Pad(low=1)})
    assert abs(off.parts["m"]["rpm"]) < 5


def test_clean_drops_bad_input():
    data = clean({"parts": [{"id": "r1", "type": "resistor", "ohms": "x"},
                            {"id": "r1", "type": "led"},
                            {"id": "z", "type": "flux-capacitor"}],
                  "wires": [["r1.a", "pin:PA05"], ["r1.q", "rail:GND"],
                            ["r1.b", "rail:12V"], ["r1.a", "r1.a"]],
                  "anchors": {"pin:PA05": [1, 2], "rail:9V": [0, 0]}})
    assert [p["id"] for p in data["parts"]] == ["r1"]
    assert data["parts"][0]["ohms"] == 330.0
    assert data["wires"] == [["r1.a", "pin:PA05"]]
    assert data["anchors"] == {"pin:PA05": [1.0, 2.0]}


def test_device_pads_and_adc_channels():
    pads, ain = device_pads("atsame51j20a")
    assert "PB08" in pads and "RESET_N" not in pads
    assert ("/machine/soc/adc0", 2) in ain["PB08"]


@pytest.mark.qemu
def test_bench_button_and_led_on_the_mc00(qemu_path, qemu_machines,
                                          tmp_path, monkeypatch):
    """blinky reads SW0 (PA25, the PORT's pull-up) and blinks LED0 (PA24,
    active low): a bench button from PA25 to GND speeds it up, and a bench
    LED from 3V3 through 330 ohm into PA24 blinks with it."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BLINKY.is_file():
        pytest.skip("blinky.elf not built")
    monkeypatch.setenv("PP_WEB_BENCH_DIR", str(tmp_path))

    async def scenario():
        app = App("ev15n46a", BLINKY)
        states, uart = [], []
        app.emit = lambda m: (states.append((time.monotonic(), m))
                              if m["type"] == "benchstate" else
                              uart.append(m.get("data", ""))
                              if m["type"] == "uart" else None)
        await app.run(BLINKY)
        try:
            await app.set_bench({
                "parts": [part("button", "sw1"), part("resistor", "r1"),
                          part("led", "d1")],
                "wires": [["pin:PA25", "sw1.a"], ["sw1.b", "rail:GND"],
                          ["rail:3V3", "r1.a"], ["r1.b", "d1.a"],
                          ["d1.k", "pin:PA24"]]})
            await asyncio.sleep(2.0)
            slow = len(states)
            pull = str(await app.session.qmp.execute(
                "qom-get", path="/machine/soc/port", property="pa-pull"))
            await app.bench_prop("sw1", "pressed", True)
            await asyncio.sleep(2.0)
            bench = str(await app.session.qmp.execute(
                "qom-get", path="/machine/soc/port", property="pa-bench"))
            await app.bench_prop("sw1", "pressed", False)
            await asyncio.sleep(0.3)
            released = str(await app.session.qmp.execute(
                "qom-get", path="/machine/soc/port", property="pa-bench"))
        finally:
            await app.session.stop()
        return states, slow, pull, bench, released, "".join(uart)

    states, slow, pull, bench, released, uart = asyncio.run(scenario())
    mask, up = (int(x, 16) for x in pull.split())
    assert mask >> 25 & 1 and up >> 25 & 1         # SW0's pull-up, on
    assert bench == f"{1 << 25:x} 0"                # pressed: PA25 held low
    # Released, the net settles at the pull-up's level, which the bench keeps
    assert released == f"{1 << 25:x} {1 << 25:x}"
    assert "SW0 pressed" in uart

    def lit(m):
        return m["parts"]["d1"]["brightness"] > 0.05

    def flips(seq):
        return sum(1 for a, b in zip(seq, seq[1:]) if lit(a[1]) != lit(b[1]))
    # 2 Hz then 8 Hz: the bench LED follows LED0's pad both ways
    assert flips(states[:slow]) >= 4, [lit(m) for _, m in states[:slow]]
    assert flips(states[slow:]) > 2 * flips(states[:slow])
    assert json.dumps(states[-1][1])                # serialisable
