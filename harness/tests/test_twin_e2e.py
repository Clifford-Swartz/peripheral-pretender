# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The GDB/MI snapshot path end to end against QEMU's gdbstub.

Runs the m0demo fixture on QEMU's stock microbit machine, so it proves the
harness before the PIC32CM machine exists; the same commands drive pyOCD's
gdbserver on silicon.
"""
import shutil
import subprocess
from pathlib import Path

import pytest

from harness import compare, dump_regs, gdb_snapshot, snapshot, twin
from harness.gdbmi import find_gdb

M0DEMO = Path(__file__).resolve().parent / "fixtures" / "m0demo"
REGS = M0DEMO / "regs.txt"
GCC = shutil.which("arm-none-eabi-gcc") or "C:/msys64/ucrt64/bin/arm-none-eabi-gcc.exe"
MACHINE = "microbit"

pytestmark = pytest.mark.qemu


@pytest.fixture(scope="module")
def demo_elf(tmp_path_factory) -> Path:
    try:
        qemu = twin.qemu_binary()
        find_gdb()
    except (twin.TwinError, FileNotFoundError) as exc:
        pytest.skip(str(exc))
    if not Path(GCC).exists():
        pytest.skip("arm-none-eabi-gcc not found")
    machines = subprocess.run([str(qemu), "-M", "help"], capture_output=True, text=True,
                              env=twin.qemu_env()).stdout
    if MACHINE not in machines.split():
        pytest.skip(f"{qemu} has no {MACHINE} machine")
    elf = tmp_path_factory.mktemp("m0demo") / "demo.elf"
    subprocess.run([GCC, "-mcpu=cortex-m0", "-mthumb", "-O0", "-g", "-nostdlib",
                    "-ffreestanding", "-T", str(M0DEMO / "demo.ld"), str(M0DEMO / "demo.c"),
                    "-o", str(elf)], check=True, env=twin.qemu_env())
    return elf


def snapshot_run(elf: Path, out: Path) -> int:
    return gdb_snapshot.main([str(elf), "--twin", "--machine", MACHINE, "--regs", str(REGS),
                              "--break", "step", "--stop-at", "done", "--out", str(out)])


def test_breakpoint_snapshots(demo_elf, tmp_path):
    out = tmp_path / "twin.json"
    assert snapshot_run(demo_elf, out) == 0
    capture = snapshot.load(out)
    assert [e["label"] for e in capture["events"]] == ["start", "step", "step", "step", "done"]
    assert [e["values"]["DEMO.counter"] for e in capture["events"]] == [
        "0x00000000", "0x00000000", "0x00000001", "0x00000003", "0x00000006"]
    assert capture["events"][-1]["values"]["DEMO.marks[3]"] == "0x00000006"
    assert capture["events"][-1]["func"] == "done"
    assert [s["name"] for s in capture["skipped"]] == ["DEMO.fifo"]
    assert capture["end"] == "stop-at"


def test_two_runs_compare_equal(demo_elf, tmp_path):
    a, b = tmp_path / "a.json", tmp_path / "b.json"
    assert snapshot_run(demo_elf, a) == 0
    assert snapshot_run(demo_elf, b) == 0
    assert compare.main(["regs", str(a), str(b), "--core"]) == 0


def test_reset_dump_matches_documented_values(demo_elf, tmp_path):
    out = tmp_path / "dump.json"
    assert dump_regs.main(["--twin", "--machine", MACHINE, "--kernel", str(demo_elf),
                           "--regs", str(REGS), "--out", str(out)]) == 0
    dump = snapshot.load(out)
    assert dump["side"] == "twin" and dump["reset"] == "qemu-cold"
    assert dump["events"][0]["values"]["FICR.CODEPAGESIZE"] == "0x00000400"
    assert compare.main(["reset", str(out)]) == 0
