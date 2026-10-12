# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Every path that halts, resets or writes silicon refuses without approval."""
import pytest

from harness import dump_regs, gdb_snapshot, silicon
from harness.regs import Reg


@pytest.fixture(autouse=True)
def no_hardware(monkeypatch):
    def forbidden(*args, **kwargs):
        raise AssertionError("test reached a hardware entry point")
    monkeypatch.setattr(silicon, "session", forbidden)


@pytest.mark.parametrize("argv, reason", [
    (["--pyocd", "--reset-halt"], "--allow-halt"),
    (["--gdb", "localhost:3333"], "--allow-halt"),
    (["--twin", "--reset-halt"], "--reset-halt applies to --pyocd"),
    (["--pyocd", "--unmask-clocks", "--allow-halt"], "--unmask-clocks needs --reset-halt"),
])
def test_dump_regs_refusals(capsys, argv, reason):
    assert dump_regs.main(argv) == 2
    assert reason in capsys.readouterr().err


def test_attach_reads_only_always_safe_ranges():
    flash = Reg("NVM.USER_PAGE_WORD_0", 0x00804000, 4)
    did = Reg("DSU.DID", 0x41002018, 4)
    cpuid = Reg("SystemControl.CPUID", 0xE000ED00, 4)
    gclk = Reg("GCLK.GENCTRL[0]", 0x40001C20, 4)
    assert dump_regs.attach_split([flash, did, cpuid, gclk]) == ([flash, did, cpuid], [gclk])


@pytest.mark.parametrize("argv, reason", [
    (["--target", "localhost:3333"], "--allow-halt"),
    (["--target", "localhost:3333", "--allow-halt", "--include-sensitive"], "twin only"),
])
def test_gdb_snapshot_refusals(capsys, argv, reason):
    assert gdb_snapshot.main(["hello.elf"] + argv) == 2
    assert reason in capsys.readouterr().err


def test_breakpoints_limited_to_the_m0plus_bpu():
    with pytest.raises(gdb_snapshot.SnapshotError, match="only 4 hardware breakpoints"):
        gdb_snapshot.insert_breakpoints(None, ["a", "b", "c", "d", "e"], 4)
