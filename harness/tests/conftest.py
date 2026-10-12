# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Shared fixtures for the harness tests.  Nothing here touches hardware."""
import copy
import json
import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

FIXTURES = Path(__file__).resolve().parent / "fixtures"


def pytest_configure(config):
    config.addinivalue_line("markers", "qemu: launches qemu-system-arm (skipped if missing)")


def regdef(name, offset, size, *, count=1, reset=0, access="RW", flags=(),
           side_effect=False, provenance=()):
    """One register definition as generator/mchpgen/jsongen.py writes it."""
    return {"name": name, "offset": offset, "size": size, "count": count, "stride": size,
            "reset": reset, "access": access, "flags": list(flags),
            "read_side_effect": side_effect, "provenance": [list(p) for p in provenance]}


def view(registers, mode=None, overrides=(), reset_from=()):
    return {"mode": mode, "registers": list(registers),
            "reset_overrides": list(overrides), "reset_from": list(reset_from)}


def instance(name, module, ip, ip_id, base):
    return {"name": name, "module": module, "ip": ip, "ip_id": ip_id, "base": base}


def sercom_view(mode, baud_size, data_size):
    return view([regdef("CTRLA", 0x0, 4),
                 regdef("BAUD", 0xC, baud_size),
                 regdef("DATA", 0x28, data_size, flags=["HOOK_PRE_READ"], side_effect=True)],
                mode=mode)


DEVICE = {
    "schema": "pp-device/1",
    "memory": [
        {"name": "FLASH", "base": 0x0, "size": 0x80000, "type": "flash"},
        {"name": "CAL", "base": 0x800000, "size": 8, "type": "fuses"},
        {"name": "USER_PAGE", "base": 0x804000, "size": 0x100, "type": "user_page"},
        {"name": "DRAM", "base": 0x20000000, "size": 0x10000, "type": "ram"},
    ],
    "nvm_rows": {"fill": 0xFF,
                 "words": [{"addr": 0x804000, "value": 0xB15088FB, "what": "USER word 0"}],
                 "serial_addrs": [0x80A00C], "serial_default": [0x50500000]},
    "instances": [
        instance("GCLK", "GCLK", "gclk_u2122", "U2122", 0x40001C00),
        instance("WDT", "WDT", "wdt_u2251", "U2251", 0x40002000),
        instance("SERCOM4", "SERCOM", "sercom_u2201", "U2201", 0x42001400),
        instance("SERCOM5", "SERCOM", "sercom_u2201", "U2201", 0x42001800),
        instance("MPU", "MPU", "mpu", "", 0xE000ED90),
        instance("FUSES", "FUSES", "fuses_03929", "03929", 0x802000),
    ],
    "ips": {
        "gclk_u2122": {"module": "GCLK", "default_view": "", "views": [view(
            [regdef("SYNCBUSY", 0x4, 4, access="R", flags=["DYNAMIC", "READ_ONLY"]),
             regdef("GENCTRL", 0x20, 4, count=2,
                    provenance=[("reset", "overlay", "reset_overrides GENCTRL[0]")])],
            overrides=[{"key": "GENCTRL[0]", "offset": 0x20, "size": 4, "value": 0x106}])]},
        "wdt_u2251": {"module": "WDT", "default_view": "", "views": [view(
            [regdef("CTRLA", 0x0, 1), regdef("CLEAR", 0xC, 1, access="W", flags=["WRITE_ONLY"])],
            reset_from=[{"register_field": "CTRLA.ENABLE", "reg_offset": 0x0,
                         "fuse": "USER_FUSES.USER_WORD_0.WDT_ENABLE"}])]},
        "sercom_u2201": {"module": "SERCOM", "default_view": "USART_EXT",
                         "views": [sercom_view("USART_EXT", 2, 2), sercom_view("SPIM", 1, 4)]},
        "mpu": {"module": "MPU", "default_view": "", "views": [view(
            [regdef("TYPE", 0x0, 4, access="R", flags=["READ_ONLY"],
                    provenance=[("reset", "heuristic", "no ATDF initval: 0")])])]},
        "fuses_03929": {"module": "FUSES", "default_view": None, "views": []},
    },
}


@pytest.fixture
def device_doc() -> dict:
    return copy.deepcopy(DEVICE)


@pytest.fixture
def device_json(tmp_path, device_doc) -> Path:
    path = tmp_path / "device.json"
    path.write_text(json.dumps(device_doc), encoding="utf-8")
    return path
