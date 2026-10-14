# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The web board view: board description and the twin session behind it."""

import asyncio
import json
import time
from pathlib import Path

import pytest

from tools.pp_web.server import BOARDS
from tools.pp_web.session import Session

ROOT = Path(__file__).resolve().parents[2]
BLINKY = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / "blinky.elf"


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


def _toggles(updates: list[tuple[float, dict]]) -> list[float]:
    """Times at which PA24 (LED0) changed level while driven."""
    times, last = [], None
    for t, pads in updates:
        out, mask = pads["a"]
        if mask >> 24 & 1:
            level = out >> 24 & 1
            if last is not None and level != last:
                times.append(t)
            last = level
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
            if msg["type"] == "pads":
                updates.append((time.monotonic(), msg["pads"]))
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
