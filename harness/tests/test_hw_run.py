# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""hw_run and silicon: image checks, NVM protection rules, approval gates.

No test reaches a probe or a COM port: the hardware entry points are
replaced by stubs that fail the test if anything calls them.
"""
import struct
from pathlib import Path

import pytest

from harness import hw_run, silicon, vcom
from harness.silicon import NvmProtection, Preflight

PRODUCTION_ROW = (0xB15088FB, 0xFFFFF8BB)       # DS60001632H Table 10-3
HELLO_ELF = (Path(__file__).resolve().parents[2] / "firmware" / "hello" / "build"
             / "pic32cm_jh01_cpro" / "hello.elf")


def elf_bytes(segments, entry=0x00000101, machine=40):
    """A minimal ELF32 little-endian image: one PT_LOAD per (paddr, filesz)."""
    ident = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)
    header = ident + struct.pack("<HHIIIIIHHHHHH", 2, machine, 1, entry, 52, 0, 0,
                                 52, 32, len(segments), 40, 0, 0)
    phdrs = b"".join(struct.pack("<8I", 1, 0, paddr, paddr, size, size, 5, 4)
                     for paddr, size in segments)
    return header + phdrs


@pytest.fixture
def elf(tmp_path):
    def make(segments, **kwargs):
        path = tmp_path / "image.elf"
        path.write_bytes(elf_bytes(segments, **kwargs))
        return str(path)
    return make


@pytest.fixture(autouse=True)
def no_hardware(monkeypatch):
    def forbidden(*args, **kwargs):
        raise AssertionError("test reached a hardware entry point")
    monkeypatch.setattr(silicon, "session", forbidden)
    monkeypatch.setattr(vcom, "open_port", forbidden)
    monkeypatch.setattr(vcom, "pick_port", forbidden)


def test_read_image_sorts_loadable_segments(elf):
    image = hw_run.read_image(elf([(0x1000, 0x200), (0x0, 0x100)], entry=0x1101))
    assert image.segments == ((0x0, 0x100), (0x1000, 0x1200))
    assert image.entry == 0x1101


def test_read_image_rejects_other_machines(elf):
    with pytest.raises(hw_run.HwRunError, match="not an Arm ELF"):
        hw_run.read_image(elf([(0x0, 4)], machine=3))


def test_never_programs_outside_main_flash(elf, capsys):
    assert hw_run.main([elf([(0x0, 0x100), (0x804000, 8)]), "--dry-run"]) == 2
    assert "outside main flash" in capsys.readouterr().err


def test_dry_run_prints_the_plan(elf, capsys):
    assert hw_run.main([elf([(0x1000, 0x100)]), "--dry-run"]) == 0
    out = capsys.readouterr().out
    assert "DTR asserted" in out and "never chip erase" in out


@pytest.mark.parametrize("extra, reason", [
    ([], "--allow-program"),
    (["--programmer", "ipecmd"], "--allow-program"),
    (["--programmer", "none"], "--allow-reset"),
])
def test_board_actions_need_explicit_approval(elf, capsys, extra, reason):
    assert hw_run.main([elf([(0x1000, 0x100)])] + extra) == 2
    assert reason in capsys.readouterr().err


def test_button_reset_needs_no_probe_approval():
    args = hw_run.parse_args(["x.elf", "--programmer", "none", "--reset", "button"])
    assert hw_run.approval_missing(args) == ""


def test_ipecmd_programs_program_memory_only():
    cmd = hw_run.ipecmd_command("ipecmd.exe", "a.hex", "EDBG")
    assert "-MP" in cmd
    assert not {"-M", "-E", "-MC"} & set(cmd)


# --- NVM protection (silicon.py) -----------------------------------------------------

def test_production_row_protects_the_factory_bootloader():
    prot = NvmProtection(PRODUCTION_ROW, secured=False)
    assert prot.bootprot_bytes == 4096 and prot.locked_regions == []
    problems = prot.conflicts([(0x0, 0x800)])
    assert len(problems) == 1 and "BOOTPROT=0xB" in problems[0]
    assert prot.conflicts([(0x1000, 0x1800)]) == []


def test_locked_region_and_security_bit():
    region0_locked = (PRODUCTION_ROW[0] | 0xF, PRODUCTION_ROW[1] & ~(1 << 16))
    prot = NvmProtection(region0_locked, secured=True)
    problems = prot.conflicts([(0x1000, 0x1800)])
    assert any("region 0" in p for p in problems)
    assert any("secured" in p for p in problems)


def test_preflight_names_the_silicon_revision():
    lines = Preflight(probe="ATML0000", did=0x11060200, statusb=0x02).describe()
    assert "DSU.DID      0x11060200 (revision C0)" in lines
    assert "DSU.STATUSB  0x02 (PROT=0, DBGPRES=1)" in lines


@pytest.mark.skipif(not HELLO_ELF.exists(), reason=f"{HELLO_ELF} not built")
def test_hello_needs_bootprot_cleared_on_a_production_part():
    image = hw_run.read_image(HELLO_ELF)
    assert image.segments[0][0] == 0x0
    assert NvmProtection(PRODUCTION_ROW, False).conflicts(list(image.segments))
