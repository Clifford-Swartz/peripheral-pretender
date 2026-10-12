# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Start the twin (QEMU) halted at reset with its gdbstub listening.

Snapshots of the twin go through the gdbstub, whose reads carry
attrs.debug=1, so device models take no read side effects.  Never use HMP
"xp" or QMP "pmemsave" for snapshots: they read with MEMTXATTRS_UNSPECIFIED
and do trigger side effects (docs/research/validation-build.md, finding 7).
"""
from __future__ import annotations

import contextlib
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Iterator, Sequence

_EXE = "qemu-system-arm.exe" if sys.platform == "win32" else "qemu-system-arm"
# The pp-qemu checkout lives next to this repo (its path must not contain spaces).
QEMU = Path(__file__).resolve().parents[1].parent / "pp-qemu" / "build" / _EXE
MACHINE = "pic32cm-jh01-cpro"
# Windows only: QEMU's runtime DLLs (glib, pixman) from MSYS2 UCRT64.
MSYS2_BIN = Path(os.environ.get("PP_MSYS2_BIN", "C:/msys64/ucrt64/bin"))


class TwinError(Exception):
    pass


def qemu_binary(explicit: str | Path | None = None) -> Path:
    path = Path(explicit or os.environ.get("PP_QEMU") or QEMU)
    if not path.is_file():
        raise TwinError(f"QEMU not found at {path}; pass --qemu or set PP_QEMU")
    return path


def qemu_env() -> dict[str, str]:
    env = dict(os.environ)
    if sys.platform == "win32" and MSYS2_BIN.is_dir():
        env["PATH"] = f"{MSYS2_BIN}{os.pathsep}{env.get('PATH', '')}"
    return env


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@contextlib.contextmanager
def halted(machine: str = MACHINE, kernel: str | Path | None = None,
           qemu: str | Path | None = None, extra: Sequence[str] = ()) -> Iterator[str]:
    """Run QEMU paused at reset (-S); yield the gdbstub address "host:port".

    gdb retries the connection for up to 15 s (tcp auto-retry), so the caller
    can connect at once.
    """
    port = free_port()
    cmd = [str(qemu_binary(qemu)), "-M", machine, "-S",
           "-gdb", f"tcp:127.0.0.1:{port}",
           "-display", "none", "-monitor", "none", "-serial", "null"]
    if kernel:
        cmd += ["-kernel", str(kernel)]
    cmd += list(extra)
    with tempfile.TemporaryFile() as stderr:
        proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=stderr, env=qemu_env())
        try:
            time.sleep(0.3)
            if proc.poll() is not None:
                stderr.seek(0)
                raise TwinError(f"QEMU exited with {proc.returncode}: "
                                f"{stderr.read().decode(errors='replace').strip()}")
            yield f"127.0.0.1:{port}"
        finally:
            proc.kill()
            proc.wait()
