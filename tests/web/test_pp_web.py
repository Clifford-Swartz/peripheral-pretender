# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The web board view: board description and the twin session behind it."""

import asyncio
import json
import math
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
CLICKS_DEMO = BUILD / "clicks.elf"
JH01_BLINKY = (ROOT / "firmware" / "hello" / "build" / "pic32cm_jh01_cpro"
               / "blinky.elf")
AVR_BUILD = ROOT / "firmware" / "avr-hello" / "build"


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
        boxes = [b for c in CLICKS.values() for b in _stack_boxes(hd, c)]
        for x0, y0, x1, y1 in boxes:
            assert vx <= x0 and x1 <= vx + vw, (hd["id"], x0, x1)
            assert vy - vh <= y0 and y1 <= vy, (hd["id"], y0, y1)
        spans.append((min(b[1] for b in boxes), max(b[3] for b in boxes)))
    spans.sort()
    assert spans[0][1] <= spans[1][0], spans    # any two Clicks fit together
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


def _socket_box(header: dict, click: dict) -> tuple[float, ...]:
    """Board-mm box of a Click on a header that is a mikroBUS socket."""
    sock = header["socket"]
    hc = click.get("header_center_mm", [11.43, 8.89])
    kx = click["origin_mm"][0] + click["size_mm"][0] / 2 - hc[0]
    ky = click["origin_mm"][1] - click["size_mm"][1] / 2 - hc[1]
    r = math.radians(sock["rotation"])
    cx = sock["at_mm"][0] + kx * math.cos(r) + ky * math.sin(r)
    cy = sock["at_mm"][1] - kx * math.sin(r) + ky * math.cos(r)
    cw, ch = click["size_mm"]
    if sock["rotation"] % 180:
        cw, ch = ch, cw
    return (cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2)


def test_board_description_ev66e56a():
    """AVR64EA48 Curiosity Nano on a Curiosity Nano Base: one image of both
    (compose_carrier.py), pins from the user guides DS50003494A and
    DS50002839B, three mikroBUS sockets as Click slots."""
    board = json.loads((BOARDS / "ev66e56a" / "board.json").read_text())
    assert board["machine"] == "avr64ea48-cnano"
    x0, y0 = board["origin_mm"]
    w, h = board["size_mm"]
    for part in board["leds"] + board["buttons"] + board["headers"]:
        x, y = part["at_mm"]
        assert x0 <= x <= x0 + w and y0 - h <= y <= y0, part["id"]
    led = board["leds"][0]
    assert (led["id"], led["pin"], led["active_low"]) == ("LED0", "PB3", True)
    assert {hd["slot"]: hd["mikrobus"] for hd in board["headers"]} == {
        "mb1": {"rst": "PD6", "pwm": "PD0", "int": "PD3", "cs": "PA7"},
        "mb2": {"rst": "PF3", "pwm": "PD1", "int": "PF2", "cs": "PE2"},
        "mb3": {"rst": "PE1", "pwm": "PD2", "int": "PE0", "cs": "PE3"}}
    assert _png_size(BOARDS / "ev66e56a" / board["image"]) == tuple(
        round(v * board["px_per_mm"]) for v in board["size_mm"])
    # Every Click fits the stage on every socket; neighbours do not overlap
    (vx, vy), (vw, vh) = board["view_mm"]["origin"], board["view_mm"]["size"]
    spans = []
    for hd in board["headers"]:
        boxes = [_socket_box(hd, c) for c in CLICKS.values()]
        for bx0, by0, bx1, by1 in boxes:
            assert vx <= bx0 and bx1 <= vx + vw, (hd["id"], bx0, bx1)
            assert vy - vh <= by0 and by1 <= vy, (hd["id"], by0, by1)
        spans.append((min(b[1] for b in boxes), max(b[3] for b in boxes)))
    spans.sort()
    assert all(a[1] <= b[0] + 0.01 for a, b in zip(spans, spans[1:])), spans


def test_pin_groups():
    from tools.pp_web.server import pin_groups
    for bid, groups in (("ev15n46a", ("a", "b")),
                        ("ev81x90a", ("a", "b", "c")),
                        ("ev66e56a", ("a", "b", "d", "e", "f"))):
        board = json.loads((BOARDS / bid / "board.json").read_text())
        assert pin_groups(board) == groups, bid


def _stack_boxes(header: dict, click: dict) -> list[tuple[float, ...]]:
    """Board-mm boxes (x0, y0, x1, y1) of the adapter and Click on @header."""
    ad = ADAPTERS[header["adapter"]]
    dx = header["at_mm"][0] - ad["header_at_mm"][0]
    dy = header["at_mm"][1] - ad["header_at_mm"][1]
    (ox, oy), (w, h) = ad["origin_mm"], ad["size_mm"]
    hc = click.get("header_center_mm", [11.43, 8.89])
    kx = click["origin_mm"][0] + click["size_mm"][0] / 2 - hc[0]
    ky = click["origin_mm"][1] - click["size_mm"][1] / 2 - hc[1]
    r = math.radians(ad["socket"]["rotation"])
    cx = ad["socket"]["at_mm"][0] + dx + kx * math.cos(r) + ky * math.sin(r)
    cy = ad["socket"]["at_mm"][1] + dy - kx * math.sin(r) + ky * math.cos(r)
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
        size = _png_size(image)
        assert all(abs(n - v * 20) <= 1 for n, v in zip(size, click["size_mm"])), size
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


@pytest.mark.qemu
def test_session_button_g_and_7seg(qemu_path, qemu_machines):
    """clicks.elf: Button G Click presses on EXT1 count up on the 7seg Click
    on EXT2 (two 74HC595 over SPI), and the button's LED follows it."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not CLICKS_DEMO.is_file():
        pytest.skip("clicks.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=clicks)")

    async def scenario():
        uart, pins = [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "pins":
                pins.append(msg["pins"])

        s = Session("pic32cm-mc00-cpro,ext1=buttong,ext2=7seg", CLICKS_DEMO,
                    emit, watch=[("ext2", "outputs")])
        await s.start()
        try:
            await asyncio.sleep(0.8)
            for _ in range(3):
                await s.set_click("ext1", "pressed", True)
                await asyncio.sleep(0.25)
                await s.set_click("ext1", "pressed", False)
                await asyncio.sleep(0.25)
            await s.set_click("ext1", "pressed", True)
            await asyncio.sleep(0.4)
            held = pins[-1]["b"][10][0]           # PB10: the button's LED
            await s.set_click("ext1", "pressed", False)
            await asyncio.sleep(0.4)
            state = dict(s.click_state)
        finally:
            await s.stop()
        return "".join(uart), held, state

    uart, held, state = asyncio.run(scenario())
    assert "press count=0x04" in uart, uart
    assert held > 0.9
    # U1 (bits 7:0) is the left digit: 0; U2 the right digit: 4
    assert state["ext2.outputs"] == 0xCA << 8 | 0x7E, hex(state["ext2.outputs"])


def test_board_list_and_demos():
    """The page's board picker: every board, and each demo_elf names a
    firmware build for that board's machine."""
    from tools.pp_web.server import board_list
    ids = [b["id"] for b in board_list()]
    assert {"ev15n46a", "ev81x90a", "ev66e56a"} <= set(ids)
    for bid in ids:
        board = json.loads((BOARDS / bid / "board.json").read_text(encoding="utf-8"))
        if "demo_elf" in board:
            assert board["demo_elf"].startswith("firmware/"), board["demo_elf"]


def _avr_twin():
    """Skip unless an AVR pp-qemu build with avr64ea48-cnano is around."""
    from tools.pp.qemu import QemuNotFound, find_qemu, list_machines
    try:
        qemu = find_qemu(target="avr")
    except QemuNotFound as exc:
        pytest.skip(str(exc))
    if "avr64ea48-cnano" not in {m.name for m in list_machines(qemu)}:
        pytest.skip(f"{qemu} has no avr64ea48-cnano machine")


@pytest.mark.qemu
def test_session_ev66e56a_blinky_and_8x8g():
    """AVR64EA48 Curiosity Nano: LED0 (PB3) blinks from the TCB0 interrupt
    and SW0 (PB2) speeds it up; the 8x8 G Click on mikroBUS 1 shows what
    the MAX7219 is told, through the display property the page polls."""
    from tools.pp_web.server import pin_groups
    _avr_twin()
    for app in ("blinky", "matrix"):
        if not (AVR_BUILD / f"{app}.elf").is_file():
            pytest.skip(f"{app}.elf not built (make -C firmware/avr-hello)")
    board = json.loads((BOARDS / "ev66e56a" / "board.json").read_text())

    def toggles(updates):
        times, last = [], None
        for t, pins in updates:
            on = pins["b"][3][1] > 0.5
            if last is not None and on != last:
                times.append(t)
            last = on
        return times

    async def scenario():
        updates, uart, states = [], [], []

        def emit(msg):
            if msg["type"] == "pins":
                updates.append((time.monotonic(), msg["pins"]))
            elif msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                states.append(msg["state"].get("mb1.display"))

        s = Session(board["machine"], AVR_BUILD / "blinky.elf", emit,
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
        m = Session(f"{board['machine']},mb1=8x8g", AVR_BUILD / "matrix.elf",
                    emit, groups=pin_groups(board),
                    watch=[("mb1", CLICKS["8x8g"]["display"]["property"])])
        await m.start()
        try:
            await asyncio.sleep(1.5)
        finally:
            await m.stop()
        return slow, fast, "".join(uart), states

    slow, fast, uart, states = asyncio.run(scenario())
    assert "PP blinky" in uart and "SW0 pressed" in uart, uart
    assert 5 <= len(slow) <= 10, slow
    assert len(fast) >= 7, fast
    assert "frame: 3C 42 A9 85 85 A9 42 3C" in uart, uart
    frames = {f for f in states if f}
    assert len(frames) >= 3, frames        # the text scrolls
    assert all(len(f.split()) == 8 for f in frames), frames


BUZZ_DEMO = BUILD / "buzz.elf"


@pytest.mark.qemu
def test_session_buzz(qemu_path, qemu_machines):
    """buzz.elf with a Buzz Click on EXT1: the page hears the tune TCC0
    plays on PB10 (the buzzer's frequency property, the property the page
    polls), silence between notes, and A4 while SW0 is held."""
    from tools.pp_web.server import watched
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BUZZ_DEMO.is_file():
        pytest.skip("buzz.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=buzz)")

    async def scenario():
        uart, tones = [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                tones.append((msg["state"]["ext1.frequency"],
                              msg["state"]["ext1.duty"]))

        s = Session("pic32cm-mc00-cpro,ext1=buzz", BUZZ_DEMO, emit,
                    watch=[("ext1", *p) for p in watched(CLICKS["buzz"])])
        await s.start()
        try:
            await asyncio.sleep(2.5)
            tune = list(tones)
            await s.press("sw0", True)
            await asyncio.sleep(1.0)
            held = tones[-1]
            await s.press("sw0", False)
            await asyncio.sleep(0.5)
            after = tones[-1]
        finally:
            await s.stop()
        return "".join(uart), tune, held, after

    uart, tune, held, after = asyncio.run(scenario())
    assert "note 523 Hz" in uart and "note 1047 Hz" in uart, uart
    heard = {f for f, _ in tune if f}
    for note in (523, 659, 784, 1047):
        assert any(abs(f - note) <= 1 for f in heard), (note, heard)
    assert all(abs(d - 500) <= 5 for f, d in tune if f), tune
    assert (0, 0) in tune                       # rests are silent
    assert held[0] == 440 and abs(held[1] - 500) <= 5, held
    assert after[0] != 440, after


OLEDC_DEMO = BUILD / "oledc.elf"


def _rgb565(frame: str, x: int, y: int, width: int = 96) -> int:
    import base64
    data = base64.b64decode(frame)
    return data[2 * (y * width + x)] << 8 | data[2 * (y * width + x) + 1]


@pytest.mark.qemu
def test_session_oledc(qemu_path, qemu_machines):
    """oledc.elf with an OLED C Click on EXT1: the panel picture the page
    polls (fetched when frame-gen moves) shows the colour bars and the
    moving square; SW0 inverts it."""
    from tools.pp_web.server import watched
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not OLEDC_DEMO.is_file():
        pytest.skip("oledc.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=oledc)")
    assert watched(CLICKS["oledc"]) == [("frame", "frame-gen")]

    async def scenario():
        uart, frames = [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                frames.append(msg["state"]["ext1.frame"])

        s = Session("pic32cm-mc00-cpro,ext1=oledc", OLEDC_DEMO, emit,
                    watch=[("ext1", *p) for p in watched(CLICKS["oledc"])])
        await s.start()
        try:
            await asyncio.sleep(2.0)
            normal = list(frames)
            await s.press("sw0", True)
            await asyncio.sleep(0.6)
            inverted = frames[-1]
            await s.press("sw0", False)
            await asyncio.sleep(0.3)
        finally:
            await s.stop()
        return "".join(uart), normal, inverted

    uart, normal, inverted = asyncio.run(scenario())
    assert "bars drawn" in uart and "inverse" in uart, uart
    last = normal[-1]
    # Bars 12 pixels wide: white, yellow, cyan, green, magenta, red, blue,
    # black; the square may cover part of a bar, so look down each one
    bars = [0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000]
    for b, color in enumerate(bars):
        seen = {_rgb565(last, 12 * b + 5, y) for y in range(0, 96, 4)}
        assert color in seen, (b, [hex(v) for v in seen])
    assert any(0xFD20 == _rgb565(last, x, y)
               for x in range(96) for y in range(96))       # the square
    assert len(set(normal)) >= 10                           # it moves
    white = {_rgb565(inverted, 5, y) for y in range(0, 96, 4)}
    black = {_rgb565(inverted, 89, y) for y in range(0, 96, 4)}
    assert 0x0000 in white and 0xFFFF not in white, white
    assert 0xFFFF in black, black


THUMBSTICK_DEMO = BUILD / "thumbstick.elf"


def test_stick_values():
    """A joystick's axes, and a tilt as gravity on x, y and z."""
    from tools.pp_web.server import stick_values
    joy = CLICKS["thumbstick"]["sticks"][0]
    assert stick_values(joy, 0.5, -1.0) == {"x": 500, "y": -1000}
    tilt = {"mode": "tilt", "range": 1000, "x_property": "x",
            "y_property": "y", "z_property": "z"}
    assert stick_values(tilt, 0, 0) == {"x": 0, "y": 0, "z": 1000}
    # Leaning right by 30 degrees: x dips, reads -sin(30) g
    assert stick_values(tilt, 0.5, 0) == {"x": -500, "y": 0, "z": 866}
    assert stick_values(CLICKS["accel"]["sticks"][0], 0, 0.5) == {
        "xyz": "0 -500 866"}


@pytest.mark.qemu
def test_session_thumbstick(qemu_path, qemu_machines):
    """thumbstick.elf with a Thumbstick Click on EXT1, through the server's
    stick path (what the page sends): the MCP3204 reads the stick, the
    push switch pulls INT low and LED0 follows it."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THUMBSTICK_DEMO.is_file():
        pytest.skip("thumbstick.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=thumbstick)")

    async def scenario():
        app = server.App("ev15n46a", THUMBSTICK_DEMO)
        await app.plug("ext1", "thumbstick")
        try:
            await asyncio.sleep(0.8)
            await app.stick("ext1", "stick", 0.0, 1.0, False)      # up
            await asyncio.sleep(0.4)
            await app.stick("ext1", "stick", 0.9, 0.0, False)      # right
            await asyncio.sleep(0.4)
            await app.stick("ext1", "stick", 0.0, 0.0, True)       # push
            await asyncio.sleep(0.4)
            led = app.session.pins["a"][24][1]
            await app.stick("ext1", "stick", 0.0, 0.0, False)
            await asyncio.sleep(0.4)
        finally:
            await app.session.stop()
        return app.uart_log, led

    uart, led = asyncio.run(scenario())
    assert "stick x=2048 y=2048 centre" in uart, uart
    assert "y=0 up" in uart, uart
    assert "x=3891 y=2048 right" in uart, uart
    assert "centre pushed" in uart, uart
    assert led > 0.9                                # LED0 on while pushed


ACCEL_DEMO = BUILD / "accel.elf"


@pytest.mark.qemu
def test_session_accel(qemu_path, qemu_machines):
    """accel.elf with an Accel Click on EXT1: tilting it on the page (the
    server's tilt mapping) reaches the ADXL345's samples; LED0 lights while
    it leans."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not ACCEL_DEMO.is_file():
        pytest.skip("accel.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=accel)")

    async def scenario():
        app = server.App("ev15n46a", ACCEL_DEMO)
        await app.plug("ext1", "accel")
        try:
            await asyncio.sleep(0.8)
            await app.stick("ext1", "tilt", 0.6, 0.0, None)    # lean right
            await asyncio.sleep(0.5)
            led = app.session.pins["a"][24][1]
            await app.stick("ext1", "tilt", 0.0, -0.8, None)   # bottom down
            await asyncio.sleep(0.5)
            await app.stick("ext1", "tilt", 0.0, 0.0, None)
            await asyncio.sleep(0.5)
        finally:
            await app.session.stop()
        return app.uart_log, led

    uart, led = asyncio.run(scenario())
    assert "Accel Click found" in uart, uart
    assert "x=+0 y=+0 z=+1000 mg flat" in uart, uart
    assert "x=-601 y=+0 z=+800 mg right side down" in uart, uart
    assert "y=+800 z=+601 mg bottom down" in uart, uart
    assert led > 0.9


RTC6_DEMO = BUILD / "rtc6.elf"


@pytest.mark.qemu
def test_session_rtc6(qemu_path, qemu_machines):
    """rtc6.elf with an RTC 6 Click on EXT1: the time the page shows (the
    MCP79411's "time") runs with the twin, alarm 0 pulls MFP (INT, PB02)
    low and the firmware sees it."""
    from tools.pp_web.server import watched
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not RTC6_DEMO.is_file():
        pytest.skip("rtc6.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=rtc6)")
    assert watched(CLICKS["rtc6"]) == [("time",)]

    async def scenario():
        uart, times, mfp = [], [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                times.append(msg["state"]["ext1.time"])
            elif msg["type"] == "levels":
                mfp.append((msg["levels"]["b"] >> 2) & 1)

        s = Session("pic32cm-mc00-cpro,ext1=rtc6", RTC6_DEMO, emit,
                    watch=[("ext1", *p) for p in watched(CLICKS["rtc6"])])
        await s.start()
        try:
            await asyncio.sleep(7.0)
        finally:
            await s.stop()
        return "".join(uart), times, mfp

    uart, times, mfp = asyncio.run(scenario())
    assert "EUI-48 00:04:A3:00:00:01" in uart and "SRAM ok" in uart, uart
    assert "2026-10-14 12:00:05 ALARM" in uart, uart
    assert "2026-10-14 12:00:00" in times and "2026-10-14 12:00:05" in times
    assert 0 in mfp and mfp[-1] == 1, mfp      # asserted, then cleared


@pytest.mark.qemu
@pytest.mark.parametrize("button", ["buttonr", "buttony"])
def test_session_button_variants(button, qemu_path, qemu_machines):
    """Button R and Y Clicks behave as Button G: clicks.elf counts their
    presses and lights their LED (on PWM, PB10) while held."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not CLICKS_DEMO.is_file():
        pytest.skip("clicks.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=clicks)")

    async def scenario():
        uart, pins = [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "pins":
                pins.append(msg["pins"])

        s = Session(f"pic32cm-mc00-cpro,ext1={button},ext2=7seg", CLICKS_DEMO,
                    emit)
        await s.start()
        try:
            await asyncio.sleep(0.6)
            await s.set_click("ext1", "pressed", True)
            await asyncio.sleep(0.4)
            held = pins[-1]["b"][10][0]
            await s.set_click("ext1", "pressed", False)
            await asyncio.sleep(0.2)
        finally:
            await s.stop()
        return "".join(uart), held

    uart, held = asyncio.run(scenario())
    assert "press count=0x01" in uart, uart
    assert held > 0.9


@pytest.mark.qemu
@pytest.mark.parametrize("matrix", ["8x8r", "8x8y", "8x8b"])
def test_session_8x8_variants(matrix):
    """8x8 R, Y and B Clicks behave as 8x8 G on the Nano Base: matrix.elf
    scrolls its text through the MAX7219's display property."""
    from tools.pp_web.server import pin_groups, watched
    _avr_twin()
    if not (AVR_BUILD / "matrix.elf").is_file():
        pytest.skip("matrix.elf not built (make -C firmware/avr-hello)")
    board = json.loads((BOARDS / "ev66e56a" / "board.json").read_text())

    async def scenario():
        uart, states = [], []

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                states.append(msg["state"].get("mb1.display"))

        m = Session(f"{board['machine']},mb1={matrix}",
                    AVR_BUILD / "matrix.elf", emit, groups=pin_groups(board),
                    watch=[("mb1", *p) for p in watched(CLICKS[matrix])])
        await m.start()
        try:
            await asyncio.sleep(1.5)
        finally:
            await m.stop()
        return "".join(uart), states

    uart, states = asyncio.run(scenario())
    assert "frame: 3C 42 A9 85 85 A9 42 3C" in uart, uart
    assert len({f for f in states if f}) >= 3
