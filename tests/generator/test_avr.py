# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""8-bit AVR (AVR8X) devices: the AVR64EA48 through the generator."""

import json

import pytest

from gen_support import ATDF_AVR, OVERLAYS, PROFILES
from mchpgen import atdf, avrgen, device, jsongen, overlay, profile


@pytest.fixture(scope="module")
def avr():
    if not ATDF_AVR.exists():
        pytest.skip(f"ATDF not found: {ATDF_AVR}")
    dev = atdf.parse(str(ATDF_AVR))
    return device.build(dev, overlay.load_dir(str(OVERLAYS)),
                        profile.find(str(PROFILES), dev.name))


def test_avr_device_level(avr):
    assert avr.device.is_avr
    assert len(avr.irqs) == 44 and avr.irqs[-1].index == 44
    usart1 = avr.instance("USART1")
    assert (usart1.base, usart1.slot_size, usart1.vectors) == \
        (0x820, 0x10, [29, 30, 31])
    assert usart1.irq == 29
    # A vector with two names (one per timer mode) is one line
    tca0 = next(l for l in avr.irqs if l.index == 8)
    assert tca0.name == "TCA0_LUNF" and "TCA0_OVF" in tca0.caption
    assert "CORE" in avr.instance("CPU").flags
    assert avr.instance("VPORTB").slot_size == 4


def test_avr_intflags_w1c_and_overlays(avr):
    assert avr.ip("PORT").views[0].reg("INTFLAGS").w1c == 0xFF
    status = avr.ip("USART").views[0].reg("STATUS")
    assert status.w1c == 0x5A and status.wmask == 0x01
    assert avr.ip("USART").overlay_status == "applied"


def test_avr_headers_and_json(avr):
    soc = avrgen.soc_header(avr)
    assert "#define AVR64EA48_SIGNATURE1 0x96" in soc
    assert '{ .name = "USART1_DRE", .instance = "USART1", .vector = 30 }' in soc
    dev = avrgen.device_header(avr)
    assert '{ .name = "B1", .revision = 0x21 }' in dev
    assert ".signature = { 0x1E, 0x96, 0x1E }" in dev
    assert all(len(line) <= 80 for line in (soc + dev).splitlines())
    doc = json.loads(jsongen.device_json(avr))
    assert doc["core"]["type"] == "avrxt"
    assert doc["core"]["signature"] == [0x1E, 0x96, 0x1E]
    # The only data row: the desk kit's temperature sensor calibration
    assert "nvm_rows" not in doc
    rows = [(r["addr"], r["bytes"]) for r in doc["data_rows"]]
    assert rows == [(0x1104, [0xCE, 0xFD, 0x7E, 0xEC])]
