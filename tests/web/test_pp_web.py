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
BUILD = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro"
BLINKY = BUILD / "blinky.elf"
BREATHE = BUILD / "breathe.elf"


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
