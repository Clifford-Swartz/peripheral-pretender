# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The avr64ea48-cnano twin against an AVR64EA48 Curiosity Nano.

golden/avr64ea48-b1/ holds the virtual COM port output that an EV66E56A
(AVR64EA48, silicon revision B1) gave for firmware/avr-hello on
2026-10-14 (<app>-vcom.bin). The twin must reproduce it byte for byte
and access nothing it does not model. Build the firmware first:
make -C firmware/avr-hello (MPLAB XC8 avr-gcc, AVR-Ex DFP).
"""

from pathlib import Path

import pytest

from tools.pp import RunOptions, run
from tools.pp.qemu import QemuNotFound, find_qemu, list_machines
from tools.pp.runner import decode_uart

pytestmark = pytest.mark.qemu

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "golden" / "avr64ea48-b1"
MACHINE = "avr64ea48-cnano"
BUILD = ROOT / "firmware" / "avr-hello" / "build"
APPS = sorted(p.name[:-len("-vcom.bin")] for p in GOLDEN.glob("*-vcom.bin"))


@pytest.fixture(scope="module")
def qemu_avr() -> Path:
    try:
        qemu = find_qemu(target="avr")
    except QemuNotFound as exc:
        pytest.skip(str(exc))
    if MACHINE not in {m.name for m in list_machines(qemu)}:
        pytest.skip(f"{qemu} has no {MACHINE} machine")
    return qemu


@pytest.mark.parametrize("app", APPS)
def test_vcom_matches_silicon(qemu_avr, app):
    elf = BUILD / f"{app}.elf"
    if not elf.is_file():
        pytest.skip(f"{elf} not built (make -C firmware/avr-hello)")
    result = run(RunOptions(elf=elf, machine=MACHINE, qemu=qemu_avr,
                            timeout_s=20))
    expected = decode_uart((GOLDEN / f"{app}-vcom.bin").read_bytes())
    assert result.firmware_exit == 0, result.detail
    assert result.uart == expected
    assert not result.unimplemented_summary.get("total"), \
        result.unimplemented_summary


def test_adc_checks_pass(qemu_avr):
    """firmware/avr-hello adc.c: the ADC's internal inputs, references and
    modes (its own checks; codes follow the supply, so no byte golden)."""
    elf = BUILD / "adc.elf"
    if not elf.is_file():
        pytest.skip(f"{elf} not built (make -C firmware/avr-hello APP=adc)")
    result = run(RunOptions(elf=elf, machine=MACHINE, qemu=qemu_avr,
                            timeout_s=20))
    assert result.firmware_exit == 0, result.uart
    assert "FAIL" not in result.uart, result.uart
    assert not result.unimplemented_summary.get("total"), \
        result.unimplemented_summary
