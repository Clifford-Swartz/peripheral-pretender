# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Shared pytest setup: project root on sys.path and fixtures for tests
that launch QEMU. Fixtures skip (with the reason) when QEMU, a machine or
the cross toolchain is missing; tools are imported lazily so a broken tools
package cannot break collection of unrelated suites."""

import importlib.util
import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

MPS2_MACHINE = "mps2-an385"
MPS2_BUILD_SCRIPT = PROJECT_ROOT / "tests" / "assets" / "mps2" / "build.py"


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "qemu: launches qemu-system-arm (skipped if missing)")


@pytest.fixture(scope="session")
def qemu_path() -> Path:
    from tools.pp.qemu import QemuNotFound, find_qemu
    try:
        return find_qemu()
    except QemuNotFound as exc:
        pytest.skip(str(exc))


@pytest.fixture(scope="session")
def qemu_machines(qemu_path) -> set:
    from tools.pp.qemu import list_machines
    return {m.name for m in list_machines(qemu_path)}


def _load_mps2_build():
    spec = importlib.util.spec_from_file_location("mps2_build",
                                                  MPS2_BUILD_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture(scope="session")
def mps2_images(qemu_machines) -> dict:
    """{name: ELF path} for the mps2-an385 runner test images."""
    if MPS2_MACHINE not in qemu_machines:
        pytest.skip(f"QEMU build has no {MPS2_MACHINE} machine")
    build = _load_mps2_build()
    try:
        return build.build_all()
    except build.ToolchainMissing as exc:
        pytest.skip(str(exc))
