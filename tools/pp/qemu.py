# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Locate qemu-system-arm (or -avr), build its environment and query its
machines.

Each CPU family runs on its own QEMU binary, built from the same pp-qemu
checkout into its own build folder: build/ for Arm (qemu-system-arm),
build-avr/ for 8-bit AVR (qemu-system-avr). The target comes from the
firmware's ELF header (elf_target) or the machine name (machine_target).
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

DEFAULT_MACHINE = "pic32cm-jh01-cpro"
DEFAULT_AVR_MACHINE = "avr64ea48-cnano"

PROJECT_ROOT = Path(__file__).resolve().parents[2]

TARGETS = ("arm", "avr")

# ELF e_machine of each target
EM_ARM = 40
EM_AVR = 83
_EM_TARGET = {EM_ARM: "arm", EM_AVR: "avr"}

# Machines of 8-bit AVR boards (everything else is Arm)
AVR_MACHINES = ("avr64ea48-cnano",)


def exe_name(target: str = "arm") -> str:
    name = f"qemu-system-{target}"
    return name + ".exe" if sys.platform == "win32" else name


def build_dir_name(target: str = "arm") -> str:
    """The pp-qemu build folder of @target: build/ (Arm), build-avr/."""
    return "build" if target == "arm" else f"build-{target}"


def env_var(target: str = "arm") -> str:
    """$PP_QEMU names the Arm binary, $PP_QEMU_AVR the AVR one."""
    return "PP_QEMU" if target == "arm" else f"PP_QEMU_{target.upper()}"


_EXE = exe_name("arm")

# The fork is checked out next to the project because QEMU's configure
# rejects paths with spaces (docs/design/v0-vertical-slice.md section 2).
DEFAULT_QEMU = PROJECT_ROOT.parent / "pp-qemu" / "build" / _EXE


def default_qemu(target: str = "arm") -> Path:
    if target == "arm":
        return DEFAULT_QEMU
    return (PROJECT_ROOT.parent / "pp-qemu" / build_dir_name(target) /
            exe_name(target))


def elf_target(path: str | os.PathLike[str]) -> str | None:
    """"arm" or "avr" from an ELF header, None for anything else."""
    try:
        with open(path, "rb") as f:
            header = f.read(20)
    except OSError:
        return None
    if header[:4] != b"\x7fELF" or len(header) < 20:
        return None
    machine = int.from_bytes(header[18:20],
                             "big" if header[5] == 2 else "little")
    return _EM_TARGET.get(machine)


def machine_target(machine: str | None) -> str:
    """The target a machine runs on: AVR boards by name, else Arm."""
    name = (machine or "").split(",")[0]
    return "avr" if name in AVR_MACHINES else "arm"

DEFAULT_MSYS2_BIN = Path("C:/msys64/ucrt64/bin")


class QemuNotFound(RuntimeError):
    pass


@dataclass(frozen=True)
class Machine:
    name: str
    description: str


def find_qemu(explicit: str | os.PathLike[str] | None = None,
              target: str = "arm") -> Path:
    """Return the QEMU binary for @target ("arm", "avr"): explicit path,
    then $PP_QEMU ($PP_QEMU_AVR), then the pp-qemu build next to this
    project, then qemu-system-<target> on PATH.

    The explicit path and the variable may also name a pp-qemu checkout or
    its build directory. For AVR, a $PP_QEMU that names the Arm binary,
    build folder or checkout also finds build-avr/ beside it.
    """
    var = env_var(target)
    for candidate, origin in ((explicit, "--qemu"),
                              (os.environ.get(var), var)):
        if candidate:
            path = _binary_at(Path(candidate), target)
            if path is None:
                raise QemuNotFound(
                    f"{origin} names no QEMU binary: {candidate}")
            return path
    if target != "arm" and os.environ.get("PP_QEMU"):
        path = _sibling_build(Path(os.environ["PP_QEMU"]), target)
        if path:
            return path
    default = default_qemu(target)
    if default.is_file():
        return default
    found = shutil.which(f"qemu-system-{target}")
    if found:
        return Path(found)
    raise QemuNotFound(
        f"qemu-system-{target} not found (looked at {default} and PATH); "
        f"build pp-qemu, set {var} or pass --qemu")


def _binary_at(path: Path, target: str = "arm") -> Path | None:
    exe = exe_name(target)
    if path.is_file():
        return path
    for candidate in (path / exe, path / build_dir_name(target) / exe):
        if candidate.is_file():
            return candidate
    return None


def _sibling_build(arm: Path, target: str) -> Path | None:
    """<checkout>/build-<target>/qemu-system-<target> beside the Arm
    binary, build folder or checkout that $PP_QEMU names."""
    exe, folder = exe_name(target), build_dir_name(target)
    for checkout in (arm, arm.parent, arm.parent.parent):
        candidate = checkout / folder / exe
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


def machines_info(explicit: str | os.PathLike[str] | None = None,
                  target: str = "arm") -> dict:
    """The QEMU binary in use and the machines it offers."""
    qemu = find_qemu(explicit, target)
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
