# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The web board view: board description and the twin session behind it."""

import asyncio
import json
import math
import struct
import time
from pathlib import Path

import pytest

from tools.pp_web.server import ADAPTERS, BOARDS, CLICKS, slot_watch
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
def test_run_for_the_debugger(qemu_path, qemu_machines):
    """run(debug_port=): the twin starts paused at reset with its gdbstub
    on that port (the VS Code extension's debug sessions attach there)."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro")
    if not BLINKY.is_file():
        pytest.skip("blinky.elf not built")
    from tools.pp_web import server
    from tools.pp_web.session import free_port

    async def scenario():
        app = server.App("ev15n46a", None)
        port = free_port()
        assert await app.run(BLINKY, debug_port=port)
        try:
            reader, writer = await asyncio.open_connection("127.0.0.1", port)

            async def packet(cmd: bytes) -> bytes:
                """One GDB remote packet out, the reply's payload back."""
                writer.write(b"$%s#%02x" % (cmd, sum(cmd) & 0xFF))
                await writer.drain()
                buf = b""
                while not (b"$" in buf and b"#" in buf.split(b"$", 1)[1]
                           and len(buf.split(b"#")[-1]) >= 2):
                    buf += await asyncio.wait_for(reader.read(4096), 10)
                writer.write(b"+")
                return buf.split(b"$", 1)[1].split(b"#")[0]
            stop = await packet(b"?")               # why are you stopped?
            regs = await packet(b"g")               # pc is r15
            writer.close()
        finally:
            await app.session.stop()
        return stop, regs

    stop, regs = asyncio.run(scenario())
    assert stop[:3] in (b"T05", b"S05"), stop               # SIGTRAP: paused
    pc = int.from_bytes(bytes.fromhex(regs[15 * 8:16 * 8].decode()), "little")
    assert pc != 0, regs                                    # at Reset_Handler


def test_default_firmware(tmp_path, monkeypatch):
    """A board runs its demo until given firmware; the PyKit, which has
    none, its stock CircuitPython, fetched once (here: a stub)."""
    from tools.pp import circuitpython
    from tools.pp_web import server
    mc00 = server.App("ev15n46a", None)
    demo = ROOT / mc00.board["demo_elf"]
    if demo.is_file():
        assert asyncio.run(mc00.default_firmware()) == demo
    monkeypatch.setenv("PP_CACHE", str(tmp_path))
    fetched = []

    def fetch(version, board):
        fetched.append((version, board))
        elf = circuitpython.elf_path(version, board)
        elf.parent.mkdir(parents=True, exist_ok=True)
        elf.write_bytes(b"\x7fELF")
        return elf
    monkeypatch.setattr(circuitpython, "fetch", fetch)
    pykit = server.App("pykit", None)
    first = asyncio.run(pykit.default_firmware())
    assert first == circuitpython.elf_path(
        pykit.board["circuitpython"]["version"],
        pykit.board["circuitpython"]["board_id"])
    assert asyncio.run(pykit.default_firmware()) == first
    assert len(fetched) == 1                    # cached after the first


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
                    emit, watch=[slot_watch("ext2", "outputs")])
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
                    watch=[slot_watch("mb1", CLICKS["8x8g"]["display"]["property"])])
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
                    watch=[slot_watch("ext1", *p) for p in watched(CLICKS["buzz"])])
        await s.start()
        try:
            await asyncio.sleep(2.5)
            tune = list(tones)
            await s.press("sw0", True)
            # The firmware looks at SW0 between notes; under load that can
            # take a while, so wait for the A4 rather than sampling once
            held = (0, 0)
            for _ in range(40):
                await asyncio.sleep(0.1)
                held = tones[-1]
                if held[0] == 440:
                    break
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
                    watch=[slot_watch("ext1", *p) for p in watched(CLICKS["oledc"])])
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
            # LED0 lights at the next sample; under load a frame may still
            # straddle the change, so wait for a whole lit frame
            led = 0.0
            for _ in range(30):
                await asyncio.sleep(0.1)
                led = app.session.pins.get("a", [[0, 0]] * 32)[24][1]
                if led > 0.9:
                    break
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
                    watch=[slot_watch("ext1", *p) for p in watched(CLICKS["rtc6"])])
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
                    watch=[slot_watch("mb1", *p) for p in watched(CLICKS[matrix])])
        await m.start()
        try:
            await asyncio.sleep(1.5)
        finally:
            await m.stop()
        return "".join(uart), states

    uart, states = asyncio.run(scenario())
    assert "frame: 3C 42 A9 85 85 A9 42 3C" in uart, uart
    assert len({f for f in states if f}) >= 3


def test_board_description_pykit():
    """The Curiosity PyKit Explorer: a CircuitPython board whose own parts
    (TFT, NeoPixels, IMU) the page shows; no Click headers."""
    from tools.pp import fatfs
    from tools.pp_web.server import part_watch, pin_groups
    board = json.loads((BOARDS / "pykit" / "board.json").read_text(
        encoding="utf-8"))
    assert _png_size(BOARDS / "pykit" / board["image"]) == tuple(
        round(v * board["px_per_mm"]) for v in board["size_mm"])
    x0, y0 = board["origin_mm"]
    w, h = board["size_mm"]
    spots = [p["at_mm"] for p in board["leds"] + board["buttons"]]
    for part in board["parts"]:
        spots += [part.get("display", {}).get("at_mm", [x0, y0])]
        spots += part.get("pixels", {}).get("at_mm", [])
        spots += [st["at_mm"] for st in part.get("sticks", [])]
        spots += [part["audio"]["at_mm"]] if "audio" in part else []
    for x, y in spots:
        assert x0 <= x <= x0 + w and y0 - h <= y <= y0, (x, y)
    assert board["machine"] == "curiosity-circuitpython"
    assert board["leds"][0]["pin"] == "PB23"
    assert pin_groups(board) == ("a", "b")
    assert part_watch(board) == [
        ("lcd.frame", "/machine/lcd", "frame", "frame-gen"),
        ("neopixels.pixels", "/machine/neopixels", "pixels", "gen"),
        ("speaker.audio", "/machine/soc/dac", "audio")]
    files = fatfs.folder_files(BOARDS / "pykit" / board["circuitpython"]["drive"])
    assert "code.py" in files


PYKIT_ELF = (Path.home() / ".pp_cache" / "circuitpython"
             / "microchip_curiosity_circuitpython-10.1.4.elf")


@pytest.mark.qemu
def test_session_pykit_circuitpython(tmp_path, qemu_path, qemu_machines):
    """Stock CircuitPython runs the board's demo code.py from the CIRCUITPY
    image: the TFT shows the bubble level, the NeoPixels follow a tilt of
    the IMU, and USER turns them green."""
    from tools.pp import fatfs
    from tools.pp_web.server import part_watch, stick_values
    if "curiosity-circuitpython" not in qemu_machines:
        pytest.skip("QEMU build has no curiosity-circuitpython machine")
    if not PYKIT_ELF.is_file():
        pytest.skip("CircuitPython not fetched (py -m tools.pp.circuitpython "
                    "fetch)")
    board = json.loads((BOARDS / "pykit" / "board.json").read_text(
        encoding="utf-8"))
    drive = tmp_path / "circuitpy.img"
    drive.write_bytes(fatfs.build_image(fatfs.folder_files(
        BOARDS / "pykit" / board["circuitpython"]["drive"])))
    tilt = next(p for p in board["parts"] if p["id"] == "imu")["sticks"][0]

    async def scenario():
        uart, state = [], {}

        def emit(msg):
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate":
                state.update(msg["state"])

        async def until(cond, timeout):
            end = time.monotonic() + timeout
            while time.monotonic() < end and not cond():
                await asyncio.sleep(0.2)
            return cond()

        s = Session(board["machine"], PYKIT_ELF, emit,
                    watch=part_watch(board), drive=drive)
        await s.start()
        try:
            # Wait for each expected state, not just any change: under load the
            # first NeoPixel write can land after the first "pitch" line
            await until(lambda: "pitch" in "".join(uart), 60)
            await until(lambda: state.get("neopixels.pixels")
                        == " ".join(["000020"] * 5), 10)
            level = dict(state)
            for prop, value in stick_values(tilt, 0.5, 0).items():
                await s.set_prop("/machine/imu", prop, value)
            await until(lambda: state.get("neopixels.pixels", "")
                        .startswith("007820"), 10)
            leaning = dict(state)
            await s.press("user", True, "user-pressed")
            await until(lambda: state.get("neopixels.pixels", "")
                        .startswith("400000"), 10)
            pressed = dict(state)
        finally:
            await s.stop()
        return "".join(uart), level, leaning, pressed

    uart, level, leaning, pressed = asyncio.run(scenario())
    assert "\x1b" not in uart and "pitch" in uart, uart[-500:]
    assert level["neopixels.pixels"] == " ".join(["000020"] * 5)
    # Tipped by 30 degrees along the IMU's x: pitch 30, green 4 * 30 = 120
    assert leaning["neopixels.pixels"].split()[0] == "007820", leaning
    assert pressed["neopixels.pixels"] == " ".join(["400000"] * 5)
    # The bubble level on the TFT: the dark blue background, the ring
    frame = level["lcd.frame"]
    seen = {_rgb565(frame, x, 10, 240) for x in range(0, 240, 8)}
    assert 0x0108 in seen, [hex(v) for v in seen]


@pytest.mark.qemu
def test_session_pykit_audio(tmp_path, qemu_path, qemu_machines):
    """audioio on the DAC: a looping 20-sample sine at 8 kHz (a TC paces
    the DMA into DATABUF and the DAC's START events) comes out of the
    speaker part as a 400 Hz waveform; synthio (double-buffered, refilled
    on the DMAC's block events through EVSYS) plays A4 at 440 Hz."""
    import base64
    from tools.pp import fatfs
    if "curiosity-circuitpython" not in qemu_machines:
        pytest.skip("QEMU build has no curiosity-circuitpython machine")
    if not PYKIT_ELF.is_file():
        pytest.skip("CircuitPython not fetched (py -m tools.pp.circuitpython "
                    "fetch)")
    code = (
        "import array, math, time, board, audioio, audiocore, synthio\n"
        "dac = audioio.AudioOut(board.DAC)\n"
        "sine = array.array('H', [int(32768 + 30000 * math.sin(2 * math.pi"
        " * i / 20)) for i in range(20)])\n"
        "dac.play(audiocore.RawSample(sine, sample_rate=8000), loop=True)\n"
        "print('sine')\n"
        "time.sleep(4)\n"
        "syn = synthio.Synthesizer(sample_rate=22050)\n"
        "dac.play(syn)\n"
        "syn.press(69)\n"
        "print('synth')\n"
        "while True:\n"
        "    time.sleep(1)\n")
    drive = tmp_path / "circuitpy.img"
    drive.write_bytes(fatfs.build_image({"code.py": code.encode()}))

    async def scenario():
        uart, chunks = [], {"sine": [], "synth": []}

        def emit(msg):
            text = "".join(uart)
            phase = "synth" if "synth" in text else "sine" if "sine" in text \
                else None
            if msg["type"] == "uart":
                uart.append(msg["data"])
            elif msg["type"] == "clickstate" and phase and \
                    msg["state"].get("spk.audio"):
                chunks[phase].append(msg["state"]["spk.audio"])

        s = Session("curiosity-circuitpython", PYKIT_ELF, emit, drive=drive,
                    watch=[("spk.audio", "/machine/soc/dac", "audio")])
        await s.start()
        try:
            end = time.monotonic() + 90
            while time.monotonic() < end and \
                    len(chunks["synth"]) < 40:
                await asyncio.sleep(0.2)
        finally:
            await s.stop()
        return "".join(uart), chunks

    def pitch(chunks):
        pcm, rate = b"", 0
        for c in chunks[5:]:            # a phase's first chunks may mix
            r, _, b64 = c.split(" ", 2)
            rate, pcm = int(r), pcm + base64.b64decode(b64)
        v = struct.unpack("<%dh" % (len(pcm) // 2), pcm)
        mean = sum(v) / len(v)
        ups = sum(1 for a, b in zip(v, v[1:]) if a < mean <= b)
        return ups * rate / len(v), min(v), max(v)

    uart, chunks = asyncio.run(scenario())
    assert "synth" in uart, uart[-500:]
    hz, lo, hi = pitch(chunks["sine"][:-3])
    assert abs(hz - 400) < 8 and lo < -25000 and hi > 25000, (hz, lo, hi)
    hz, lo, hi = pitch(chunks["synth"])
    assert abs(hz - 440) < 8, hz


POT_DEMO = BUILD / "pot.elf"


@pytest.mark.qemu
def test_session_pot(qemu_path, qemu_machines):
    """pot.elf with a POT Click on EXT1, turned through the server's control
    path (what the page's slider sends): the knob's voltage reaches ADC0
    AIN2 through the socket's AN pin, 0-2.048 V; LED0 lights above half."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not POT_DEMO.is_file():
        pytest.skip("pot.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=pot)")

    def readings(uart):
        return [int(line.split()[1]) for line in uart.splitlines()
                if line.startswith("pot ") and line.endswith(" mV")]

    async def scenario():
        app = server.App("ev15n46a", POT_DEMO)
        await app.plug("ext1", "pot")
        try:
            await asyncio.sleep(0.8)
            mid = readings(app.uart_log)
            await app.control("ext1", "position", 100)
            await asyncio.sleep(0.5)
            led = app.session.pins["a"][24][1]      # driven low: lit
            top = readings(app.uart_log)
            await app.control("ext1", "position", 10)
            await asyncio.sleep(0.5)
        finally:
            await app.session.stop()
        return mid, top, readings(app.uart_log), led

    mid, top, low, led = asyncio.run(scenario())
    assert mid and abs(mid[0] - 1024) <= 8, mid             # default 50 %
    assert abs(top[-1] - 2048) <= 8, top
    assert abs(low[-1] - 205) <= 8, low
    assert led > 0.9


GNSS_DEMO = BUILD / "gnss.elf"


def _nmea_ok(sentence: str) -> bool:
    body, _, cs = sentence[1:].partition("*")
    x = 0
    for c in body:
        x ^= ord(c)
    return sentence.startswith("$") and cs == f"{x:02X}"


@pytest.mark.qemu
def test_session_gnss(qemu_path, qemu_machines):
    """gnss.elf with a GNSS Click on EXT1 (a UART Click: the L86 chardev on
    SERCOM3), through the server's control path: the module boots, takes
    the firmware's PMTK314, sends checksummed NMEA, follows the position
    sliders and loses its fix below 4 satellites."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not GNSS_DEMO.is_file():
        pytest.skip("gnss.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=gnss)")

    async def scenario():
        app = server.App("ev15n46a", GNSS_DEMO)
        await app.plug("ext1", "gnss")
        try:
            await asyncio.sleep(2.5)
            await app.control("ext1", "lat", -33.8568)        # Sydney
            await app.control("ext1", "lon", 151.2153)
            await app.control("ext1", "alt", 12)
            await asyncio.sleep(1.5)
            await app.control("ext1", "sats", 2)
            await asyncio.sleep(1.5)
        finally:
            await app.session.stop()
        return app.uart_log

    uart = asyncio.run(scenario())
    lines = uart.splitlines()
    assert "module: $PMTK010,001*2E" in lines, uart
    assert "module: $PMTK010,002*2D" in lines, uart
    assert "ack: $PMTK001,314,3*36" in lines, uart
    rmc = next(line[5:] for line in lines if line.startswith("rmc: "))
    assert rmc.startswith("$GNRMC,") and _nmea_ok(rmc), rmc
    assert ",A,3317.7900,N,11153.8200,W," in rmc, rmc
    assert "fix lat 3317.7900 N lon 11153.8200 W alt 370.0 m sats 8" in lines
    assert "fix lat 3351.4080 S lon 15112.9180 E alt 12.0 m sats 8" in lines
    assert lines[-1] == "waiting for a fix", uart


ROTARY_DEMO = BUILD / "rotary.elf"


@pytest.mark.qemu
def test_session_rotary(qemu_path, qemu_machines):
    """rotary.elf with a Rotary G Click on EXT1, through the server's
    control and button paths: turning the knob makes quadrature on PWM and
    AN (as a digital pin) that the firmware counts per detent, in both
    directions; its LED follows on the 74HC595 ring; pushing it lights the
    whole ring."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not ROTARY_DEMO.is_file():
        pytest.skip("rotary.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=rotary)")

    async def ring():
        return int(await app.session.qmp.execute(
            "qom-get", path="/machine/ext1-click", property="outputs"))

    async def scenario():
        nonlocal app
        app = server.App("ev15n46a", ROTARY_DEMO)
        await app.plug("ext1", "rotaryg")
        try:
            await asyncio.sleep(0.6)
            await app.control("ext1", "position", 3)
            await asyncio.sleep(0.4)
            at3 = await ring()
            await app.control("ext1", "position", -2)
            await asyncio.sleep(0.4)
            at_m2 = await ring()
            await app.control("ext1", "push", 1)
            await asyncio.sleep(0.3)
            pushed = await ring()
            await app.control("ext1", "push", 0)
            await asyncio.sleep(0.3)
        finally:
            await app.session.stop()
        return app.uart_log, at3, at_m2, pushed

    app = None
    uart, at3, at_m2, pushed = asyncio.run(scenario())
    lines = uart.splitlines()
    for n in ("position 1", "position 2", "position 3", "position -1",
              "position -2"):
        assert n in lines, uart
    assert at3 == 1 << 3 and at_m2 == 1 << 14, (hex(at3), hex(at_m2))
    assert pushed == 0xFFFF
    assert "pressed" in lines and "released" in lines, uart


RN4871_DEMO = BUILD / "rn4871.elf"


@pytest.mark.qemu
def test_session_rn4871(qemu_path, qemu_machines):
    """rn4871.elf with an RN4871 Click on EXT1 (a UART Click on SERCOM3):
    MikroElektronika's start-up sequence ("$", a pause, "$$$" each time)
    reaches command mode, SF,1 and SS,C0 hold across R,1; the phone in the
    Click's panel (toggle, text input, monitor) connects, sends a line and
    reads the firmware's echo."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not RN4871_DEMO.is_file():
        pytest.skip("rn4871.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=rn4871)")

    async def scenario():
        app = server.App("ev15n46a", RN4871_DEMO)
        await app.plug("ext1", "rn4871")
        try:
            for _ in range(60):
                await asyncio.sleep(0.1)
                if "waiting for a phone" in app.uart_log:
                    break
            await app.control("ext1", "connected", 1)
            await asyncio.sleep(0.3)
            await app.click_text("ext1", "send", "hello twin\r\n")
            await asyncio.sleep(0.3)
            rx = await app.session.qmp.execute(
                "qom-get", path="/machine/ext1-click", property="central-rx")
            await app.control("ext1", "connected", 0)
            await asyncio.sleep(0.3)
        finally:
            await app.session.stop()
        return app.uart_log, rx

    uart, rx = asyncio.run(scenario())
    lines = uart.splitlines()
    assert lines.count("module: %REBOOT%") == 3, uart       # reset, SF, R,1
    assert "module: RN4871 V1.40 7/9/2019 (c)Microchip Technology Inc" in lines
    assert "module: Reboot after Factory Reset" in lines, uart
    assert "module: Name=RN4871-0001" in lines and "module: Services=C0" in lines
    assert "module: Err" not in lines, uart
    assert "status: CONNECT,0,D88039FA0001" in lines, uart
    assert "status: STREAM_OPEN" in lines and "phone: hello twin" in lines
    assert "echo: hello twin" in rx, rx
    assert "status: DISCONNECT" in lines, uart


EEPROM3_DEMO = BUILD / "eeprom3.elf"


@pytest.mark.qemu
def test_session_eeprom3(qemu_path, qemu_machines):
    """eeprom3.elf with an EEPROM 3 Click on EXT1: its checks (blank part,
    a write with ACK polling through the write cycle, current-address
    reads, page wrap, A17:A16 in the device address, wrap-around reads)
    pass, and the panel's memory view shows what it wrote."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not EEPROM3_DEMO.is_file():
        pytest.skip("eeprom3.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=eeprom3)")

    async def scenario():
        app = server.App("ev15n46a", EEPROM3_DEMO)
        await app.plug("ext1", "eeprom3")
        try:
            for _ in range(50):
                await asyncio.sleep(0.1)
                if "PP:EXIT" in app.uart_log:
                    break
            await app.click_text("ext1", "at", "0x100")
            view = await app.session.qmp.execute(
                "qom-get", path="/machine/ext1-click", property="view")
        finally:
            await app.session.stop()
        return app.uart_log, view

    uart, view = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert view.startswith("00100: A3 A4 6B 72 6F 65"), view   # wrapped
    assert "..kroe" in view, view


COLOR2_DEMO = BUILD / "color2.elf"


@pytest.mark.qemu
def test_session_color2(qemu_path, qemu_machines):
    """color2.elf with a Color 2 Click on EXT1: MikroElektronika's set-up,
    the conversion-done interrupt and its clearing pass, the default light
    reads RED, and turning the page's sliders to pure green makes the
    firmware (classifying as MikroE's driver does) report GREEN."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not COLOR2_DEMO.is_file():
        pytest.skip("color2.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=color2)")

    async def scenario():
        app = server.App("ev15n46a", COLOR2_DEMO)
        await app.plug("ext1", "color2")
        try:
            for _ in range(50):
                await asyncio.sleep(0.1)
                if "PP:EXIT" in app.uart_log:
                    break
            await app.control("ext1", "red", 0)
            await app.control("ext1", "green", 255)
            for _ in range(50):
                await asyncio.sleep(0.1)
                if "GREEN" in app.uart_log:
                    break
        finally:
            await app.session.stop()
        return app.uart_log

    uart = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert "B=1 hue=120 GREEN" in uart, uart


WEATHER_DEMO = BUILD / "weather.elf"


@pytest.mark.qemu
def test_session_weather(qemu_path, qemu_machines):
    """weather.elf with a Weather Click on EXT1: MikroElektronika's set-up
    reads back, the data sheet's compensation gives the page's defaults
    exactly (25.00 C, 1013.25 hPa, 50.00 %RH), and a warmer slider shows
    in the readings, eased by the IIR filter (16) toward it."""
    import re
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not WEATHER_DEMO.is_file():
        pytest.skip("weather.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=weather)")

    def temps(log):
        return [float(x) for x in re.findall(r"T=(-?[\d.]+) C", log)]

    async def scenario():
        app = server.App("ev15n46a", WEATHER_DEMO)
        await app.plug("ext1", "weather")
        try:
            for _ in range(50):
                await asyncio.sleep(0.1)
                if "PP:EXIT" in app.uart_log:
                    break
            await app.control("ext1", "temperature", 30)
            for _ in range(150):
                await asyncio.sleep(0.1)
                if temps(app.uart_log)[-1] > 25.5:
                    break
        finally:
            await app.session.stop()
        return app.uart_log

    uart = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert "T=25.00 C P=1013.25 hPa H=50.00 %RH" in uart, uart
    assert 25.5 < temps(uart)[-1] < 30.0, uart


FAN_DEMO = BUILD / "fan.elf"


@pytest.mark.qemu
def test_session_fan(qemu_path, qemu_machines):
    """fan.elf with a Fan Click on EXT1: the EMC2301's identity, a fan at
    rest reading the 960 rpm floor, Direct Setting and the RPM algorithm
    pass; the page's fan turns (rpm polled) and holding it stops it."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not FAN_DEMO.is_file():
        pytest.skip("fan.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=fan)")
    assert ("rpm",) in server.watched(server.CLICKS["fan"])

    async def rpm(app):
        return await app.session.qmp.execute(
            "qom-get", path="/machine/ext1-click", property="rpm")

    async def scenario():
        app = server.App("ev15n46a", FAN_DEMO)
        await app.plug("ext1", "fan")
        try:
            for _ in range(300):
                await asyncio.sleep(0.1)
                if "PP:EXIT" in app.uart_log:
                    break
            spinning = await rpm(app)
            await app.control("ext1", "blocked", 1)
            held = await rpm(app)
        finally:
            await app.session.stop()
        return app.uart_log, spinning, held

    uart, spinning, held = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert spinning > 2500, spinning
    assert held == 0


LR2_DEMO = BUILD / "lr2.elf"


@pytest.mark.qemu
def test_session_lr2(qemu_path, qemu_machines):
    """lr2.elf with an LR 2 Click on EXT1: the RN2903's boot line after a
    reset pulse, keys_not_init, an OTAA join, an uplink and
    MikroElektronika's point-to-point packet pass; then the page, as the
    other radio, sends a line the firmware receives, and as the network
    sends a downlink that arrives with the next uplink."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not LR2_DEMO.is_file():
        pytest.skip("lr2.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=lr2)")

    async def scenario():
        app = server.App("ev15n46a", LR2_DEMO)
        await app.plug("ext1", "lr2")
        try:
            for _ in range(300):
                await asyncio.sleep(0.1)
                if "PP:EXIT" in app.uart_log:
                    break
            await app.click_text("ext1", "downlink", "hi node")
            await app.click_text("ext1", "peer", "hello twin")
            for _ in range(600):
                await asyncio.sleep(0.1)
                if "downlink: hi node" in app.uart_log:
                    break
            air = await app.session.qmp.execute(
                "qom-get", path="/machine/ext1-click", property="air")
        finally:
            await app.session.stop()
        return app.uart_log, air

    uart, air = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert "received: hello twin" in uart, uart
    assert "downlink: hi node" in uart, uart
    assert "P2P out: MikroE" in air, air


DCMOTOR_DEMO = BUILD / "dcmotor.elf"


@pytest.mark.qemu
def test_session_dcmotor(qemu_path, qemu_machines):
    """dcmotor.elf with a DC Motor Click on EXT1: a round of steps with no
    fault, then the motor runs forward at full drive (TCC0 PWM at 100 %,
    SEL1 = SEL2 = 0) and backward at half (SEL2 high), and the fault
    toggle stops the bridge."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not DCMOTOR_DEMO.is_file():
        pytest.skip("dcmotor.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=dcmotor)")

    async def get(app, prop):
        return await app.session.qmp.execute(
            "qom-get", path="/machine/ext1-click", property=prop)

    async def until(app, text, count, rpms=None):
        for _ in range(400):
            await asyncio.sleep(0.05)
            if rpms is not None:
                rpms.append(await get(app, "rpm"))
            if app.uart_log.count(text) >= count:
                return

    async def scenario():
        app = server.App("ev15n46a", DCMOTOR_DEMO)
        await app.plug("ext1", "dcmotor")
        try:
            fwd, rev = [], []
            await until(app, "PP:EXIT", 1)
            await until(app, "forward 100 %", 2)
            await until(app, "brake", 2, fwd)
            await until(app, "reverse 50 %", 2)
            await until(app, "coast", 2, rev)
            fwd, rev = max(fwd), min(rev)
            await app.control("ext1", "fault", 1)
            await asyncio.sleep(0.3)
            state = await get(app, "state")
        finally:
            await app.session.stop()
        return app.uart_log, fwd, rev, state

    uart, fwd, rev, state = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert fwd > 2500, fwd
    assert -1600 < rev < -1300, rev
    assert state == "coast"


SERVO_DEMO = BUILD / "servo.elf"


@pytest.mark.qemu
def test_session_servo(qemu_path, qemu_machines):
    """servo.elf with a Servo Click on EXT1: MikroElektronika's set-up
    (EXTCLK left clear), 30 Hz, the OFF count read back and servo 1's
    current through the LTC2497 pass; servos 3 and 4 then hold 45 and 135
    degrees and servo 1 sweeps, as the page's angles show."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not SERVO_DEMO.is_file():
        pytest.skip("servo.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=servo)")
    assert ("angles",) in server.watched(server.CLICKS["servo"])

    async def angles(app):
        a = await app.session.qmp.execute(
            "qom-get", path="/machine/ext1-click", property="angles")
        return [int(x) for x in a.split()]

    async def scenario():
        app = server.App("ev15n46a", SERVO_DEMO)
        await app.plug("ext1", "servo")
        try:
            for _ in range(100):
                await asyncio.sleep(0.1)
                if "angle 30" in app.uart_log:
                    break
            seen = set()
            for _ in range(20):
                a = await angles(app)
                seen.add(a[0])
                await asyncio.sleep(0.05)
        finally:
            await app.session.stop()
        return app.uart_log, a, seen

    uart, a, seen = asyncio.run(scenario())
    assert "PP:EXIT 0" in uart, uart
    assert "FAIL" not in uart, uart
    assert a[2] == 45 and a[3] == 135, a
    assert a[4:] == [-1] * 12, a
    assert len(seen) > 3, seen                      # servo 1 moving


OLEDW_DEMO = BUILD / "oledw.elf"


@pytest.mark.qemu
def test_session_oledw(qemu_path, qemu_machines):
    """oledw.elf with an OLED W Click on EXT1: MikroElektronika's set-up
    (MUX 40, C8h, DAh 12h, charge pump on) and page-mode writes put the
    frame on the edges of the 96x39 glass (screen row y = RAM line y + 1,
    column x = RAM column 95 - x), and the ball moves."""
    import base64
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not OLEDW_DEMO.is_file():
        pytest.skip("oledw.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=oledw)")

    async def frame():
        b = await app.session.qmp.execute(
            "qom-get", path="/machine/ext1-click", property="frame")
        raw = base64.b64decode(b)
        return [int.from_bytes(raw[i:i + 2], "little") for i in
                range(0, len(raw), 2)]

    async def scenario():
        nonlocal app
        app = server.App("ev15n46a", OLEDW_DEMO)
        await app.plug("ext1", "oledw")
        try:
            await asyncio.sleep(1.0)
            first = await frame()
            await asyncio.sleep(0.3)
            second = await frame()
        finally:
            await app.session.stop()
        return first, second

    app = None
    first, second = asyncio.run(scenario())
    w, h = 96, 39
    assert len(first) == w * h
    lit = lambda f, x, y: f[y * w + x] != 0            # noqa: E731
    for x in (0, 47, 95):
        assert lit(first, x, 0) and lit(first, x, h - 1), x
    for y in (0, 19, 38):
        assert lit(first, 0, y) and lit(first, w - 1, y), y
    inside = sum(lit(first, x, y) for x in range(2, w - 2)
                 for y in range(2, h - 2))
    assert 0 < inside <= 25                            # the 5x5 ball
    assert first != second                             # it moves


OLEDB_DEMO = BUILD / "oledb.elf"


@pytest.mark.qemu
def test_session_oledb(qemu_path, qemu_machines):
    """oledb.elf with an OLED B Click on EXT1: the same picture as the OLED
    W's over I2C (commands to 0x3C with D/C low, data to 0x3D with D/C
    high, as MikroElektronika's driver moves SA0), in blue."""
    import base64
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not OLEDB_DEMO.is_file():
        pytest.skip("oledb.elf not built (make -C firmware/hello "
                    "BOARD=pic32cm_mc00_cpro APP=oledb)")

    async def scenario():
        app = server.App("ev15n46a", OLEDB_DEMO)
        await app.plug("ext1", "oledb")
        try:
            await asyncio.sleep(1.2)
            b = await app.session.qmp.execute(
                "qom-get", path="/machine/ext1-click", property="frame")
        finally:
            await app.session.stop()
        raw = base64.b64decode(b)
        return [int.from_bytes(raw[i:i + 2], "big") for i in
                range(0, len(raw), 2)]

    px = asyncio.run(scenario())
    w, h = 96, 39
    lit = [v for v in px if v]
    assert len(px) == w * h
    assert set(lit) == {0x2CBF}                         # blue
    for x, y in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
        assert px[y * w + x], (x, y)
    assert len(lit) == 2 * w + 2 * (h - 2) + 25         # frame and ball


ANGLE_ELF = (ROOT / "examples" / "mplab" / "rotary_oled_angle" / "firmware" /
             "pic32cm_mc00_curiosity_pro.X" / "dist" /
             "pic32cm_mc00_curiosity_pro" / "production" /
             "pic32cm_mc00_curiosity_pro.X.production.elf")


@pytest.mark.qemu
def test_mplab_rotary_oled_angle(qemu_path, qemu_machines):
    """The MPLAB example (Harmony project, XC32 build) with a Rotary G Click
    on EXT1 and an OLED B Click on EXT2: the EIC counts the encoder while
    the OLED redraws, the push sets 0, the ring follows."""
    from tools.pp_web import server
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not ANGLE_ELF.is_file():
        pytest.skip("the MPLAB example is not built (examples/mplab/"
                    "rotary_oled_angle/README.md)")

    async def scenario():
        app = server.App("ev15n46a", ANGLE_ELF)
        app.clicks["ext2"] = {"id": "oledb", "values": {}}
        await app.plug("ext1", "rotaryg")
        seen = []
        try:
            await asyncio.sleep(1.0)
            for pos in (5, 12, -3, 30):
                await app.control("ext1", "position", pos)
                await asyncio.sleep(0.8)
                ring = await app.session.qmp.execute(
                    "qom-get", path="/machine/ext1-click", property="outputs")
                seen.append((app.uart_log.splitlines()[-1], ring))
            await app.control("ext1", "push", 1)
            await asyncio.sleep(0.3)
            await app.control("ext1", "push", 0)
            await app.control("ext1", "position", 28)
            await asyncio.sleep(0.8)
            seen.append((app.uart_log.splitlines()[-1], 0))
        finally:
            await app.session.stop()
        return seen

    seen = asyncio.run(scenario())
    assert [s[0] for s in seen] == ["angle 60", "angle 144", "angle 324",
                                    "angle 0", "angle 336"], seen
    assert [s[1] for s in seen[:4]] == [1 << 11, 1 << 14, 1 << 6, 1 << 8]
