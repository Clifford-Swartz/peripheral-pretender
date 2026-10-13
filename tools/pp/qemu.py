# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Locate qemu-system-arm, build its environment and query its machines."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

DEFAULT_MACHINE = "pic32cm-jh01-cpro"

PROJECT_ROOT = Path(__file__).resolve().parents[2]

_EXE = "qemu-system-arm.exe" if sys.platform == "win32" else "qemu-system-arm"

# The fork is checked out next to the project because QEMU's configure
# rejects paths with spaces (docs/design/v0-vertical-slice.md section 2).
DEFAULT_QEMU = PROJECT_ROOT.parent / "pp-qemu" / "build" / _EXE

DEFAULT_MSYS2_BIN = Path("C:/msys64/ucrt64/bin")


class QemuNotFound(RuntimeError):
    pass


@dataclass(frozen=True)
class Machine:
    name: str
    description: str


def find_qemu(explicit: str | os.PathLike[str] | None = None) -> Path:
    """Return the QEMU binary: explicit path, then $PP_QEMU, then the
    pp-qemu build next to this project, then qemu-system-arm on PATH.

    The explicit path and $PP_QEMU may also name a pp-qemu checkout or its
    build directory.
    """
    for candidate, origin in ((explicit, "--qemu"),
                              (os.environ.get("PP_QEMU"), "PP_QEMU")):
        if candidate:
            path = _binary_at(Path(candidate))
            if path is None:
                raise QemuNotFound(
                    f"{origin} names no QEMU binary: {candidate}")
            return path
    if DEFAULT_QEMU.is_file():
        return DEFAULT_QEMU
    found = shutil.which("qemu-system-arm")
    if found:
        return Path(found)
    raise QemuNotFound(
        f"qemu-system-arm not found (looked at {DEFAULT_QEMU} and PATH); "
        "build pp-qemu, set PP_QEMU or pass --qemu")


def _binary_at(path: Path) -> Path | None:
    if path.is_file():
        return path
    for candidate in (path / _EXE, path / "build" / _EXE):
        if candidate.is_file():
            return candidate
    return None


def qemu_env(base: dict[str, str] | None = None) -> dict[str, str]:
    """Environment for launching QEMU.

    A QEMU built in MSYS2 UCRT64 needs that environment's DLLs. They are on
    PATH inside the MSYS2 shell but not for a process started from Windows
    Python, so the directory ($PP_MSYS2_BIN, default C:/msys64/ucrt64/bin)
    is prepended when it exists.
    """
    env = dict(os.environ if base is None else base)
    if sys.platform == "win32":
        msys2_bin = Path(env.get("PP_MSYS2_BIN") or DEFAULT_MSYS2_BIN)
        if msys2_bin.is_dir():
            env["PATH"] = str(msys2_bin) + os.pathsep + env.get("PATH", "")
    return env


def _query(qemu: Path, *args: str) -> str:
    proc = subprocess.run([str(qemu), *args], env=qemu_env(),
                          stdin=subprocess.DEVNULL, capture_output=True,
                          text=True, errors="replace", timeout=30)
    if proc.returncode != 0:
        raise RuntimeError(f"{qemu.name} {' '.join(args)} failed "
                           f"({proc.returncode}): {proc.stderr.strip()}")
    return proc.stdout


_MACHINE_LINE = re.compile(r"^(\S+)\s+(.*?)\s*$")


def parse_machine_list(text: str) -> list[Machine]:
    """Parse the output of ``qemu-system-arm -M help``."""
    machines = []
    for line in text.splitlines():
        if not line.strip() or line.startswith("Supported machines"):
            continue
        m = _MACHINE_LINE.match(line)
        if m:
            machines.append(Machine(m.group(1), m.group(2)))
    return machines


def list_machines(qemu: Path) -> list[Machine]:
    return parse_machine_list(_query(qemu, "-M", "help"))


def trace_events(qemu: Path) -> list[str]:
    """Names of the trace events the binary knows (``-trace help``)."""
    return [line.strip() for line in _query(qemu, "-trace", "help")
            .splitlines() if re.fullmatch(r"\s*[A-Za-z_]\w*\s*", line)]


def qemu_version(qemu: Path) -> str:
    """First line of ``qemu-system-arm --version``."""
    out = _query(qemu, "--version")
    return out.splitlines()[0].strip() if out else ""


def machines_info(explicit: str | os.PathLike[str] | None = None) -> dict:
    """The QEMU binary in use and the machines it offers."""
    qemu = find_qemu(explicit)
    machines = list_machines(qemu)
    return {
        "qemu": str(qemu),
        "qemu_version": qemu_version(qemu),
        "default_machine": DEFAULT_MACHINE,
        "default_machine_available":
            any(m.name == DEFAULT_MACHINE for m in machines),
        "machines": [{"name": m.name, "description": m.description}
                     for m in machines],
    }
