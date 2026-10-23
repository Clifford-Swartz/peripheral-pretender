# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Firmware on the twin with a circuit simulated alongside (pp run
--circuit): firmware/hello APP=rcstep steps an RC (10 k, 1 u) on the
PIC32CM MC00's EXT1 pins and samples it with ADC0. The twin must give the
RC's analytic step response at each sample's instant. Build the firmware
first: py -m tools.periph build mc00 rcstep."""

import math
import re
from pathlib import Path

import pytest

from tools.pp import RunOptions, run
from tools.pp_analog import ngspice

pytestmark = [pytest.mark.qemu,
              pytest.mark.skipif(not ngspice.available(),
                                 reason="ngspice not installed")]

ROOT = Path(__file__).resolve().parents[2]
ELF = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / \
    "rcstep.elf"
CIRCUIT = ROOT / "firmware" / "hello" / "circuits" / "rcstep.cir"
MACHINE = "pic32cm-mc00-cpro"
TAU_US = (10e3 + 40) * 1e-6 * 1e6       # 10 k plus the pad's 40 ohms
# From a sample's timestamp to its sampling instant: the start (36 GCLK)
# and SAMPLEN + 1 = 16 CLK_ADC at 48 MHz / 32, and the few instructions
# between the timestamp and SWTRIG
SAMPLE_US = 12.8


def test_rcstep_matches_the_rc(qemu_path, qemu_machines):
    if MACHINE not in qemu_machines:
        pytest.skip(f"no {MACHINE} machine")
    if not ELF.is_file():
        pytest.skip(f"{ELF} not built (py -m tools.periph build mc00 rcstep)")
    result = run(RunOptions(elf=ELF, machine=MACHINE, qemu=qemu_path,
                            timeout_s=60, circuit=CIRCUIT))
    assert result.firmware_exit == 0, result.uart
    assert result.analog["errors"] == []
    vdd = int(re.search(r"vddana mv=0x([0-9A-F]+)", result.uart)[1], 16)
    samples = [(int(t), int(mv)) for t, mv in
               re.findall(r"rc t_us=(\d+) mv=(\d+)", result.uart)]
    assert len(samples) == 80
    for t_us, mv in samples:
        want = vdd * (1 - math.exp(-(t_us + SAMPLE_US) / TAU_US))
        # One ADC LSB (0.8 mV), the firmware's truncation to mV, and a
        # microsecond of timestamp rounding on the steepest part
        assert abs(mv - want) <= 2.0, (t_us, mv, want)
    assert result.analog["samples"] >= 80
