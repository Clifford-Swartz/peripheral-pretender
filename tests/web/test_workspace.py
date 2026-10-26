# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The workspace: an empty start, templates, and the board, its Clicks, the
bench and the firmware coming back next time."""

import asyncio
import json
from pathlib import Path

import pytest

from tools.pp_web.server import App, bench_dir, template_list

ROOT = Path(__file__).resolve().parents[2]
BLINKY = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "blinky.elf"


def test_starts_empty():
    app = App(None, None)
    assert app.board is None and app.clicks == {}
    msg = app.board_message()
    assert msg["board"] is None
    assert {b["id"] for b in msg["boards"]} >= {"ev15n46a", "ev66e56a"}
    assert all(b["image"].startswith("/boards/") for b in msg["boards"])
    assert {t["id"] for t in msg["templates"]} >= {"mc00-rotary-oled",
                                                   "nano-scroll", "mc00-rc"}
    assert app.status_message()["text"] == "add a board to start"
    # Parts without a board: rails only, solved as before
    app.bench.set({"parts": [{"id": "r1", "type": "resistor", "ohms": 1000},
                             {"id": "led1", "type": "led"}],
                   "wires": [["rail:3V3", "r1.a"], ["r1.b", "led1.a"],
                             ["led1.k", "rail:GND"]]})
    res = app.bench.solve({})
    assert res.parts["led1"]["amps"] > 0.001


def test_templates_name_real_boards_and_clicks():
    from tools.pp_web.server import CLICKS
    for t in template_list():
        assert (ROOT / "tools" / "pp_web" / "boards" / t["board"]).is_dir()
        assert all(c in CLICKS for c in t["clicks"].values()), t["id"]


def test_workspace_comes_back(qemu_path, qemu_machines):
    """A template sets up the board, Clicks, bench and firmware; a new
    engine on the same workspace file starts where this one stopped."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BLINKY.is_file():
        pytest.skip("blinky.elf not built")

    async def scenario():
        app = App(None, None)
        app.emit = lambda m: None
        await app.apply_template("mc00-rc")
        try:
            assert app.board["id"] == "ev15n46a"
            assert {p["id"] for p in app.bench.data["parts"]} == {"r1", "c1"}
            await app.plug("ext1", "thermo8")
            app.keep_firmware(BLINKY)
        finally:
            if app.session:
                await app.session.stop()
            app._stop_analog()
        saved = json.loads((bench_dir() / "workspace.json")
                           .read_text(encoding="utf-8"))
        again = App(None, None)
        return saved, again

    saved, again = asyncio.run(scenario())
    assert saved["boards"][0]["type"] == "ev15n46a"
    assert saved["boards"][0]["clicks"]["ext1"]["id"] == "thermo8"
    assert again.board["id"] == "ev15n46a"
    assert again.clicks["ext1"]["id"] == "thermo8"
    assert {p["id"] for p in again.bench.data["parts"]} == {"r1", "c1"}
    assert again.kept_elf and again.kept_elf.name == "blinky.elf"
    assert again.kept_elf.parent == bench_dir() / "firmware"


def test_reopens_on_its_own_firmware_not_an_old_build(tmp_path, monkeypatch):
    """A board that ran its demo comes back on its demo though the watched
    project has a build the workspace has seen; a build made while the bench
    was closed takes the selected board."""
    import os
    import time
    from tools.pp_web import server

    build = tmp_path / "proj" / "dist" / "default" / "production" / "app.elf"
    build.parent.mkdir(parents=True)
    build.write_bytes(b"\x7fELF app")
    ws = bench_dir() / "workspace.json"
    ws.parent.mkdir(parents=True, exist_ok=True)
    # From before the workspace kept "watched": a board, no firmware of its own
    ws.write_text(json.dumps({"version": 2, "boards": [
        {"key": "b1", "type": "ev15n46a", "at": [0, 0], "clicks": {},
         "firmware": None}]}), encoding="utf-8")

    def startup(app):
        started, fresh = [], []

        async def start(t, elf=None):
            started.append(t.key)

        async def watch(f=None):
            fresh.append(f)
        monkeypatch.setattr(server.Twin, "start", start)
        app._watch = watch

        async def go():
            await app.on_startup(None)
            await asyncio.sleep(0)              # the watcher task
        asyncio.run(go())
        return started, fresh

    app = App(None, None, tmp_path / "proj")
    assert startup(app) == (["b1"], [None])     # its demo, not app.elf
    app.saw_build(build)                        # as the watcher records it
    os.utime(build, (time.time() + 5, time.time() + 5))
    again = App(None, None, tmp_path / "proj")
    assert startup(again) == ([], [build])      # the new build takes b1


def test_board_off_keeps_the_parts():
    async def scenario():
        app = App("ev15n46a", None)
        app.emit = lambda m: None
        app.load_bench({"parts": [{"id": "r1", "type": "resistor"}],
                        "wires": [["r1.a", "hole:J803.1"],
                                  ["r1.b", "rail:GND"]]})
        await app.select_board(None)
        return app
    app = asyncio.run(scenario())
    assert app.board is None
    assert [p["id"] for p in app.bench.data["parts"]] == ["r1"]
    # The wire into the board's header is gone with the board
    assert app.bench.data["wires"] == [["r1.b", "rail:GND"]]


def test_board_pictures_route():
    from aiohttp.test_utils import TestClient, TestServer
    from tools.pp_web.server import make_app

    async def scenario():
        async with TestClient(TestServer(make_app(None))) as c:
            ok = await c.get("/boards/ev15n46a/top.png")
            bad = await c.get("/boards/ev15n46a/board.json")
            state = await (await c.get("/api/state")).json()
            return ok.status, bad.status, state
    ok, bad, state = asyncio.run(scenario())
    assert ok == 200 and bad == 404
    assert state["board"] is None


# -- Click boards wired on the bench ---------------------------------------

def _oledc_bench(**over):
    wires = {"3v3": "hole:EXT1.20", "gnd": "hole:EXT1.19",
             "cs": "hole:EXT1.15", "mosi": "hole:EXT1.16",
             "miso": "hole:EXT1.17", "sck": "hole:EXT1.18",
             "rst": "hole:EXT1.5", "pwm": "hole:EXT1.7", "int": "hole:EXT1.9"}
    wires.update(over)
    return {"parts": [{"id": "click1", "type": "click", "click": "oledc",
                       "x": 110, "y": 40}],
            "wires": [[f"click1.{k}", v] for k, v in wires.items() if v]}


def test_wired_click_resolution():
    app = App("ev15n46a", None)
    app.load_bench(_oledc_bench())
    app.update_wired()
    assert app.wired["click1"]["plugged"]
    assert app.wired["click1"]["pins"] == {
        "cs": "PA17", "mosi": "PA18", "miso": "PA16", "sck": "PA19",
        "rst": "PA20", "pwm": "PB10", "int": "PB02"}
    assert app.wired_warnings == []
    assert "wired=click1=oledc:cs.PA17:int.PB02:miso.PA16:mosi.PA18:" \
        "pwm.PB10:rst.PA20:sck.PA19" in app.machine()
    # No power: on the bench, but not plugged into the twin
    app.load_bench(_oledc_bench(**{"3v3": None}))
    app.update_wired()
    assert not app.wired["click1"]["plugged"]
    assert "power" in app.wired_warnings[0] and "wired=" not in app.machine()
    # SCK and MOSI on pads no SERCOM pairs for SPI
    app.load_bench(_oledc_bench(mosi="hole:EXT1.7", pwm=None))
    app.update_wired()
    assert "no SERCOM has SPI" in app.wired_warnings[0]


THERMO = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "thermo.elf"


def test_wired_thermo8_answers(qemu_path, qemu_machines):
    """A Thermo 8 wired to PA12/PA13 (SERCOM2's I2C pads), not plugged
    into a header: the thermo demo finds it and reads it."""
    from tools.pp import RunOptions, run
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file():
        pytest.skip("thermo.elf not built")
    result = run(RunOptions(
        elf=THERMO, machine="pic32cm-mc00-cpro", qemu=qemu_path,
        timeout_s=6, icount_shift=5,
        extra_args=["-machine", "wired=t1=thermo8:sda.PA12:scl.PA13:int.PB02"]))
    assert "Thermo 8 Click found" in result.uart, result.uart
    assert "T=+25.00 C" in result.uart


OLED_ELF = ROOT / "examples" / "mplab" / "rotary_oledc_angle" / "firmware" / \
    "pic32cm_mc00_curiosity_pro.X" / "dist" / "pic32cm_mc00_curiosity_pro" / \
    "production" / "pic32cm_mc00_curiosity_pro.X.production.elf"


def test_wired_oledc_draws(qemu_path, qemu_machines):
    """An OLED C wired on the bench to EXT1's holes (SPI through SERCOM1)
    shows the rotary example's dial: the engine plugs it in by its pads."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not OLED_ELF.is_file():
        pytest.skip("the rotary_oledc_angle example is not built")

    import base64

    def lit(frame):
        raw = base64.b64decode(frame)
        return sum(1 for i in range(0, len(raw), 2) if raw[i] | raw[i + 1])

    async def scenario():
        app = App("ev15n46a", OLED_ELF)
        app.emit = lambda m: None
        await app.set_bench(_oledc_bench())
        await app.run(OLED_ELF)
        frame = None
        try:
            # The firmware clears the panel first, then draws the dial
            for _ in range(60):
                await asyncio.sleep(0.1)
                frame = app.session.click_state.get("click1.frame")
                if frame and lit(frame) > 100:
                    break
        finally:
            await app.session.stop()
        return frame
    frame = asyncio.run(scenario())
    assert frame, "no frame from the wired OLED C"
    assert lit(frame) > 100


def test_template_rotary_oled_draws(qemu_path, qemu_machines):
    """The "Rotary angle on an OLED C" template from the palette: its own
    firmware runs (not the board's blinky) and the OLED C on EXT1 draws
    the dial."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not OLED_ELF.is_file():
        pytest.skip("the rotary_oledc_angle example is not built")

    import base64

    def lit(frame):
        raw = base64.b64decode(frame)
        return sum(1 for i in range(0, len(raw), 2) if raw[i] | raw[i + 1])

    async def scenario():
        app = App(None, None)
        app.emit = lambda m: None
        await app.apply_template("mc00-rotary-oled")
        frame = None
        try:
            elf = app.kept_elf
            for _ in range(60):
                await asyncio.sleep(0.1)
                frame = app.session.click_state.get("ext1.frame")
                if frame and lit(frame) > 100:
                    break
        finally:
            await app.session.stop()
        return elf, frame

    elf, frame = asyncio.run(scenario())
    assert elf and elf.name == OLED_ELF.name, elf
    assert frame, "no frame from the OLED C"
    assert lit(frame) > 100


# -- several boards -------------------------------------------------------

MC00_BLINKY = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "blinky.elf"
AVR_BLINKY = ROOT / "firmware" / "avr-hello" / "build" / "blinky.elf"


def test_two_boards_wired_together(qemu_path, qemu_machines):
    """The AVR64EA48 Nano's LED0 pad (PB3, blinking at 2 Hz) wired to the
    MC00's SW0 pad (PA25): the MC00 reads it rise and fall."""
    from tools.pp.qemu import QemuNotFound, find_qemu
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    try:
        find_qemu(target="avr")
    except QemuNotFound as exc:
        pytest.skip(str(exc))
    if not (MC00_BLINKY.is_file() and AVR_BLINKY.is_file()):
        pytest.skip("blinky.elf not built for both boards")

    async def scenario():
        app = App("ev15n46a", MC00_BLINKY)
        app.emit = lambda m: None
        await app.run(MC00_BLINKY)
        b2 = await app.add_board("ev66e56a", elf=AVR_BLINKY)
        try:
            assert [t.key for t in app.twins] == ["b1", "b2"]
            assert b2.at[0] > 0                 # right of the MC00
            await app.set_bench({"parts": [], "anchors": {},
                                 "wires": [["pin:b2/PB3", "pin:b1/PA25"]]})
            seen = set()
            for _ in range(40):
                await asyncio.sleep(0.1)
                lv = app.twin("b1").session.levels.get("a")
                if lv is not None:
                    seen.add((lv >> 25) & 1)
            uart2 = app.twin("b2").uart_log
        finally:
            for t in app.twins:
                await t.stop()
        saved = json.loads((bench_dir() / "workspace.json")
                           .read_text(encoding="utf-8"))
        return seen, uart2, saved

    seen, uart2, saved = asyncio.run(scenario())
    assert seen == {0, 1}
    assert "PP blinky: AVR64EA48" in uart2
    assert [b["key"] for b in saved["boards"]] == ["b1", "b2"]
    assert saved["bench"]["wires"] == [["pin:b2/PB3", "pin:b1/PA25"]]
    again = App(None, None)
    assert [t.board["id"] for t in again.twins] == ["ev15n46a", "ev66e56a"]
