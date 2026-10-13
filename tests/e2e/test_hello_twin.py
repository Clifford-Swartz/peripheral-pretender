# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""End to end: firmware/hello on each board's twin.

Skipped, with the reason, until both the machine (pp-qemu) and the ELF
(make -C firmware/hello BOARD=<board>) exist. Acceptance per
docs/design/v0-vertical-slice.md section 6.6: ``py -m tools.pp run
firmware/hello/build/<board>/hello.elf --machine <machine>`` returns 0 with
the full transcript.
"""

import subprocess
import sys
from pathlib import Path

import pytest

from tools.pp import RunOptions, run

pytestmark = pytest.mark.qemu

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD = PROJECT_ROOT / "firmware" / "hello" / "build"

# firmware board directory -> QEMU machine, and the banner it prints
BOARDS = {
    "pic32cm_jh01_cpro": ("pic32cm-jh01-cpro", "PIC32CM5164JH01100"),
    "pic32cm_mc00_cpro": ("pic32cm-mc00-cpro", "PIC32CM1216MC00048"),
}


@pytest.fixture(params=sorted(BOARDS), scope="module")
def board(request, qemu_machines):
    name = request.param
    machine, device = BOARDS[name]
    elf = BUILD / name / "hello.elf"
    if machine not in qemu_machines:
        pytest.skip(f"QEMU build has no {machine} machine yet")
    if not elf.is_file():
        pytest.skip(f"{elf.relative_to(PROJECT_ROOT)} not built "
                    f"(make -C firmware/hello BOARD={name})")
    return elf, machine, device


def test_hello_passes_on_the_twin(board):
    elf, machine, device = board
    result = run(RunOptions(elf, machine=machine, timeout_s=30))
    backlog = result.unimplemented_summary
    print(result.uart)
    print(f"model backlog: {backlog['total']} lines; "
          f"instances {backlog['instances']}")
    assert result.status == "exit", (
        f"{result.status}: {result.qemu_stderr_tail}\n{result.uart}")
    assert result.exit_code == 0
    assert result.uart.startswith(f"PP hello: {device} on ")
    assert result.uart.rstrip().endswith("PP:EXIT 0")
    assert backlog["total"] == 0, backlog


def test_hello_through_the_cli(board):
    elf, machine, _ = board
    proc = subprocess.run(
        [sys.executable, "-m", "tools.pp", "run",
         str(elf.relative_to(PROJECT_ROOT)), "--machine", machine,
         "--timeout", "30"],
        cwd=PROJECT_ROOT, capture_output=True, text=True, timeout=90)
    assert proc.returncode == 0, proc.stderr
    assert "PP:EXIT 0" in proc.stdout
