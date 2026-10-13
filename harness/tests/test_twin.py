# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""QEMU discovery for the twin side: the same rules as the pp runner."""
import sys

import pytest

from harness import twin

EXE = "qemu-system-arm.exe" if sys.platform == "win32" else "qemu-system-arm"


@pytest.mark.parametrize("layout", ["", "build"])
def test_pp_qemu_may_name_a_checkout_or_build_dir(tmp_path, monkeypatch, layout):
    binary = tmp_path / layout / EXE
    binary.parent.mkdir(parents=True, exist_ok=True)
    binary.write_bytes(b"")
    monkeypatch.setenv("PP_QEMU", str(tmp_path))
    assert twin.qemu_binary() == binary
    assert twin.qemu_binary(str(binary)) == binary


def test_missing_qemu_is_a_twin_error(tmp_path, monkeypatch):
    monkeypatch.setenv("PP_QEMU", str(tmp_path / "nothing"))
    with pytest.raises(twin.TwinError, match="PP_QEMU"):
        twin.qemu_binary()
