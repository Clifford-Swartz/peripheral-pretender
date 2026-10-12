# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""v0 end to end: firmware/hello on the pic32cm-jh01-cpro twin.

Skipped, with the reason, until both the machine (pp-qemu) and the ELF
(make -C firmware/hello) exist. Acceptance per
docs/design/v0-vertical-slice.md section 6.6: ``py -m tools.pp run
firmware/hello/build/hello.elf`` returns 0 with the full transcript.
"""

import subprocess
import sys
from pathlib import Path

import pytest

from tools.pp import DEFAULT_MACHINE, RunOptions, run

pytestmark = pytest.mark.qemu

PROJECT_ROOT = Path(__file__).resolve().parents[2]
HELLO_ELF = PROJECT_ROOT / "firmware" / "hello" / "build" / "hello.elf"


@pytest.fixture(scope="module")
def hello_elf(qemu_machines):
    if DEFAULT_MACHINE not in qemu_machines:
        pytest.skip(f"QEMU build has no {DEFAULT_MACHINE} machine yet")
    if not HELLO_ELF.is_file():
        pytest.skip(f"{HELLO_ELF.relative_to(PROJECT_ROOT)} not built "
                    "(make -C firmware/hello)")
    return HELLO_ELF


def test_hello_passes_on_the_twin(hello_elf):
    result = run(RunOptions(hello_elf, timeout_s=30))
    backlog = result.unimplemented_summary
    print(result.uart)
    print(f"model backlog: {backlog['total']} lines; "
          f"instances {backlog['instances']}")
    assert result.status == "exit", (
        f"{result.status}: {result.qemu_stderr_tail}\n{result.uart}")
    assert result.exit_code == 0
    assert result.uart.rstrip().endswith("PP:EXIT 0")


def test_hello_through_the_cli(hello_elf):
    proc = subprocess.run(
        [sys.executable, "-m", "tools.pp", "run",
         str(hello_elf.relative_to(PROJECT_ROOT)), "--timeout", "30"],
        cwd=PROJECT_ROOT, capture_output=True, text=True, timeout=90)
    assert proc.returncode == 0, proc.stderr
    assert "PP:EXIT 0" in proc.stdout
