# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""QEMU discovery, environment and machine list parsing."""

import sys

import pytest

from tools.pp import qemu
from tools.pp.qemu import QemuNotFound, find_qemu, parse_machine_list, qemu_env

MACHINE_HELP = """\
Supported machines are:
mps2-an385           ARM MPS2 with AN385 FPGA image for Cortex-M3
none                 empty machine
pic32cm-jh01-cpro    Microchip PIC32CM JH01 Curiosity Pro (PIC32CM5164JH01100)
virt                 QEMU 11.1 ARM Virtual Machine (alias of virt-11.1)
"""


def test_parse_machine_list():
    machines = parse_machine_list(MACHINE_HELP)
    assert [m.name for m in machines] == ["mps2-an385", "none",
                                          "pic32cm-jh01-cpro", "virt"]
    assert machines[2].description == \
        "Microchip PIC32CM JH01 Curiosity Pro (PIC32CM5164JH01100)"


def test_find_qemu_precedence(tmp_path, monkeypatch):
    explicit = tmp_path / "explicit-qemu"
    from_env = tmp_path / "env-qemu"
    explicit.write_bytes(b"")
    from_env.write_bytes(b"")
    monkeypatch.setenv("PP_QEMU", str(from_env))
    assert find_qemu(explicit) == explicit
    assert find_qemu() == from_env


def test_find_qemu_accepts_checkout_or_build_dir(tmp_path, monkeypatch):
    monkeypatch.delenv("PP_QEMU", raising=False)
    binary = tmp_path / "build" / qemu._EXE
    binary.parent.mkdir()
    binary.write_bytes(b"")
    assert find_qemu(tmp_path) == binary
    assert find_qemu(tmp_path / "build") == binary
    monkeypatch.setenv("PP_QEMU", str(tmp_path))
    assert find_qemu() == binary


def test_find_qemu_rejects_missing_paths(tmp_path, monkeypatch):
    monkeypatch.delenv("PP_QEMU", raising=False)
    with pytest.raises(QemuNotFound, match="--qemu"):
        find_qemu(tmp_path / "missing")
    monkeypatch.setenv("PP_QEMU", str(tmp_path / "missing"))
    with pytest.raises(QemuNotFound, match="PP_QEMU"):
        find_qemu()


def test_find_qemu_falls_back_to_path(tmp_path, monkeypatch):
    monkeypatch.delenv("PP_QEMU", raising=False)
    monkeypatch.setattr(qemu, "DEFAULT_QEMU", tmp_path / "no-build")
    monkeypatch.setattr(qemu.shutil, "which", lambda name: None)
    with pytest.raises(QemuNotFound, match="build pp-qemu"):
        find_qemu()


@pytest.mark.skipif(sys.platform != "win32",
                    reason="MSYS2 DLLs are Windows only")
def test_qemu_env_prepends_msys2_bin(tmp_path):
    env = qemu_env({"PATH": "C:\\Windows", "PP_MSYS2_BIN": str(tmp_path)})
    assert env["PATH"].split(";")[0] == str(tmp_path)
    env = qemu_env({"PATH": "C:\\Windows",
                    "PP_MSYS2_BIN": str(tmp_path / "missing")})
    assert env["PATH"] == "C:\\Windows"
