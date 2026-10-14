# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The web board view: board description and the twin session behind it."""

import asyncio
import json
import struct
import time
from pathlib import Path

import pytest

from tools.pp_web.server import ADAPTERS, BOARDS, CLICKS
from tools.pp_web.session import Session

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro"
BLINKY = BUILD / "blinky.elf"
BREATHE = BUILD / "breathe.elf"
THERMO = BUILD / "thermo.elf"
JH01_BLINKY = (ROOT / "firmware" / "hello" / "build" / "pic32cm_jh01_cpro"
               / "blinky.elf")


def test_board_description():
    board = json.loads((BOARDS / "ev15n46a" / "board.json").read_text())
    assert (BOARDS / "ev15n46a" / board["image"]).is_file()
    x0, y0 = board["origin_mm"]
    w, h = board["size_mm"]
    for part in board["leds"] + board["buttons"] + board["headers"]:
        x, y = part["at_mm"]
        assert x0 <= x <= x0 + w and y0 - h <= y <= y0, part["id"]
    led = board["leds"][0]
    assert (led["id"], led["ref"], led["pin"]) == ("LED0", "D700", "PA24")
    slots = {h["slot"]: h["int_pin"] for h in board["headers"]}
    assert slots == {"ext1": "PB02", "ext2": "PA28"}
    # Adapter and Click stacks are drawn inside the stage and never overlap
    (vx, vy), (vw, vh) = board["view_mm"]["origin"], board["view_mm"]["size"]
    spans = []
    for hd in board["headers"]:
        boxes = _stack_boxes(hd, CLICKS["thermo8"])
        for x0, y0, x1, y1 in boxes:
            assert vx <= x0 and x1 <= vx + vw, (hd["id"], x0, x1)
            assert vy - vh <= y0 and y1 <= vy, (hd["id"], y0, y1)
        spans.append((min(b[1] for b in boxes), max(b[3] for b in boxes)))
    spans.sort()
    assert spans[0][1] <= spans[1][0], spans
    assert _png_size(BOARDS / "ev15n46a" / board["image"]) == tuple(
        round(v * board["px_per_mm"]) for v in board["size_mm"])


def test_board_description_ev81x90a():
    """PIC32CM JH01 Curiosity Pro: parts inside the image, pins from the
    user guide (DS70005482B) and schematic R4."""
    board = json.loads((BOARDS / "ev81x90a" / "board.json").read_text())
    assert board["machine"] == "pic32cm-jh01-cpro"
    x0, y0 = board["origin_mm"]
    w, h = board["size_mm"]
    for part in board["leds"] + board["buttons"] + board["headers"]:
        x, y = part["at_mm"]
        assert x0 <= x <= x0 + w and y0 - h <= y <= y0, part["id"]
    led = board["leds"][0]
    assert (led["id"], led["ref"], led["pin"], led["active_low"]) == (
        "LED0", "D700", "PC05", True)
    assert {b["id"]: b["action"] for b in board["buttons"]} == {
        "SW0": "sw0", "RESET": "reset"}
    assert {hd["id"]: hd["int_pin"] for hd in board["headers"]} == {
        "EXT1": "PA21", "EXT2": "PC24", "EXT3": "PA28"}
    # The machine has no Click slots yet, so no header offers one
    assert not any("slot" in hd for hd in board["headers"])
    assert _png_size(BOARDS / "ev81x90a" / board["image"]) == tuple(
        round(v * board["px_per_mm"]) for v in board["size_mm"])


def test_pin_groups():
    from tools.pp_web.server import pin_groups
    for bid, groups in (("ev15n46a", ("a", "b")),
                        ("ev81x90a", ("a", "b", "c"))):
        board = json.loads((BOARDS / bid / "board.json").read_text())
        assert pin_groups(board) == groups, bid


def _stack_boxes(header: dict, click: dict) -> list[tuple[float, ...]]:
    """Board-mm boxes (x0, y0, x1, y1) of the adapter and Click on @header."""
    ad = ADAPTERS[header["adapter"]]
    dx = header["at_mm"][0] - ad["header_at_mm"][0]
    dy = header["at_mm"][1] - ad["header_at_mm"][1]
    (ox, oy), (w, h) = ad["origin_mm"], ad["size_mm"]
    cx, cy = ad["socket"]["at_mm"][0] + dx, ad["socket"]["at_mm"][1] + dy
    cw, ch = click["size_mm"]
    if ad["socket"]["rotation"] % 180:
        cw, ch = ch, cw
    return [(ox + dx, oy - h + dy, ox + w + dx, oy + dy),
            (cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2)]


def test_adapter_catalog():
    for ad in ADAPTERS.values():
        image = BOARDS.parent / "adapters" / ad["image"]
        assert _png_size(image) == tuple(round(v * 20) for v in ad["size_mm"])
        (ox, oy), (w, h) = ad["origin_mm"], ad["size_mm"]
        for x, y in (ad["header_at_mm"], ad["socket"]["at_mm"]):
            assert ox <= x <= ox + w and oy - h <= y <= oy


def _png_size(path: Path) -> tuple[int, int]:
    return struct.unpack(">II", path.read_bytes()[16:24])


def test_click_catalog():
    for click in CLICKS.values():
        for ctl in click["controls"]:
            assert ctl["min"] <= ctl["default"] <= ctl["max"], ctl
        for sig in click["signals"]:
            assert sig["pin"] == "int", sig
        # The image is rendered at 20 px/mm over the outline
        image = BOARDS.parent / "clicks" / click["image"]
        assert _png_size(image) == tuple(round(v * 20)
                                         for v in click["size_mm"])
        (ox, oy), (w, h) = click["origin_mm"], click["size_mm"]
        for part in click.get("leds", []) + click["signals"]:
            x, y = part["at_mm"]
            assert ox <= x <= ox + w and oy - h <= y <= oy, part


def _led0(pins: dict) -> float:
    """LED0 (PA24, active low): the fraction of the frame driven low."""
    return pins["a"][24][1]


def _toggles(updates: list[tuple[float, dict]]) -> list[float]:
    """Times at which LED0 went on or off."""
    times, last = [], None
    for t, pins in updates:
        on = _led0(pins) > 0.5
        if last is not None and on != last:
            times.append(t)
        last = on
    return times


@pytest.mark.qemu
def test_session_blinks_and_reads_sw0(qemu_path, qemu_machines):
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BLINKY.is_file():
        pytest.skip("blinky.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=blinky)")

    async def scenario():
        updates, uart = [], []

        def emit(msg):
            if msg["type"] == "pins":
                updates.append((time.monotonic(), msg["pins"]))
            elif msg["type"] == "uart":
                uart.append(msg["data"])

        s = Session("pic32cm-mc00-cpro", BLINKY, emit)
        await s.start()
        try:
            await asyncio.sleep(2.0)
            slow = _toggles(updates)
            n = len(updates)
            await s.press("sw0", True)
            await asyncio.sleep(1.0)
            await s.press("sw0", False)
            fast = _toggles(updates[n:])
            await asyncio.sleep(0.3)
        finally:
            await s.stop()
        return slow, fast, "".join(uart)

    slow, fast, uart = asyncio.run(scenario())
    assert "PP blinky" in uart and "SW0 pressed" in uart
    # 2 Hz: a toggle every 250 ms; 8 Hz while SW0 is held (sampled at 30 Hz)
    assert 5 <= len(slow) <= 10, slow
    assert len(fast) >= 7, fast


@pytest.mark.qemu
def test_session_blinks_jh01(qemu_path, qemu_machines):
    """EV81X90A: LED0 on PC05 (PORT group C) blinks and SW0 (PB19) speeds
    it up, through the same page path (pin_groups) the server uses."""
    from tools.pp_web.server import pin_groups
    if "pic32cm-jh01-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-jh01-cpro machine")
    if not JH01_BLINKY.is_file():
        pytest.skip("blinky.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_jh01_cpro APP=blinky)")
    board = json.loads((BOARDS / "ev81x90a" / "board.json").read_text())

    def led0(pins):
        return pins["c"][5][1]

    def toggles(updates):
        times, last = [], None
        for t, pins in updates:
            on = led0(pins) > 0.5
            if last is not None and on != last:
                times.append(t)
            last = on
        return times

    async def scenario():
        updates, uart = [], []

        def emit(msg):
            if msg["type"] == "pins":
                updates.append((time.monotonic(), msg["pins"]))
            elif msg["type"] == "uart":
                uart.append(msg["data"])

        s = Session(board["machine"], JH01_BLINKY, emit,
                    groups=pin_groups(board))
        await s.start()
        try:
            await asyncio.sleep(2.0)
            slow = toggles(updates)
            n = len(updates)
            await s.press("sw0", True)
            await asyncio.sleep(1.0)
            await s.press("sw0", False)
            fast = toggles(updates[n:])
            await asyncio.sleep(0.3)
        finally:
            await s.stop()
        return slow, fast, "".join(uart)

    slow, fast, uart = asyncio.run(scenario())
    assert "PP blinky" in uart and "EV81X90A" in uart, uart
    assert "SW0 pressed" in uart, uart
    assert 5 <= len(slow) <= 10, slow
    assert len(fast) >= 7, fast


@pytest.mark.qemu
def test_session_pwm_brightness(qemu_path, qemu_machines):
    """breathe.elf: LED0 dims through intermediate brightness (TCC1 PWM)."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BREATHE.is_file():
        pytest.skip("breathe.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=breathe)")

    async def scenario():
        levels = []

        def emit(msg):
            if msg["type"] == "pins":
                levels.append(_led0(msg["pins"]))

        s = Session("pic32cm-mc00-cpro", BREATHE, emit)
        await s.start()
        try:
            await asyncio.sleep(4.0)    # two breaths
        finally:
            await s.stop()
        return levels

    levels = asyncio.run(scenario())
    assert any(0.1 < f < 0.9 for f in levels), levels
    assert max(levels) > 0.8 and min(levels) < 0.1, levels


@pytest.mark.qemu
def test_session_thermo8(qemu_path, qemu_machines):
    """thermo.elf with a Thermo 8 Click on EXT1: the temperature control
    reaches the firmware, ALERT pulls PB02 low and LED0 follows it."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file():
        pytest.skip("thermo.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=thermo)")

    async def scenario():
        uart, levels, pins = [], [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "levels":
                levels.append(msg["levels"])
            elif msg["type"] == "pins":
                pins.append(msg["pins"])

        s = Session("pic32cm-mc00-cpro,ext1=thermo8", THERMO, emit)
        await s.start()
        try:
            await asyncio.sleep(1.0)
            cool = (levels[-1]["b"] >> 2) & 1, _led0(pins[-1]) if pins else 0
            await s.set_click("ext1", "temperature", 35500)
            await asyncio.sleep(1.0)
            hot = (levels[-1]["b"] >> 2) & 1, _led0(pins[-1])
        finally:
            await s.stop()
        return "".join(uart), cool, hot

    uart, cool, hot = asyncio.run(scenario())
    assert "Thermo 8 Click found" in uart, uart
    assert "T=+25.00 C" in uart and "T=+35.50 C HIGH alert" in uart, uart
    assert cool == (1, 0) and hot[0] == 0 and hot[1] > 0.5, (cool, hot)


def test_watch_runs_each_new_build(tmp_path, monkeypatch):
    """--watch: the newest out/ or dist/ ELF runs once its build settles."""
    import os
    from tools.pp_web import server

    monkeypatch.setattr(server, "WATCH_S", 0.05)
    out = tmp_path / "out" / "app"
    out.mkdir(parents=True)
    old = out / "default.elf"
    old.write_bytes(b"\x7fELF old")
    assert server.newest_elf(tmp_path) == old
    assert server.newest_elf(old) == old

    async def scenario():
        app = server.App("ev15n46a", None, tmp_path)
        ran = []

        async def run(elf):
            ran.append(elf.read_bytes())
        app.run = run
        task = asyncio.create_task(app._watch())
        await asyncio.sleep(0.2)                # the existing build runs
        new = tmp_path / "dist" / "default" / "production" / "app.elf"
        new.parent.mkdir(parents=True)
        new.write_bytes(b"\x7fELF new")
        os.utime(new, (time.time() + 5, time.time() + 5))
        await asyncio.sleep(0.4)
        (tmp_path / "out" / "app" / "notes.txt").write_text("x")
        await asyncio.sleep(0.2)
        task.cancel()
        return ran

    assert asyncio.run(scenario()) == [b"\x7fELF old", b"\x7fELF new"]
