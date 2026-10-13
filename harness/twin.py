# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Start the twin (QEMU) halted at reset with its gdbstub listening.

Snapshots of the twin go through the gdbstub, whose reads carry
attrs.debug=1, so device models take no read side effects.  Never use HMP
"xp" or QMP "pmemsave" for snapshots: they read with MEMTXATTRS_UNSPECIFIED
and do trigger side effects (docs/research/validation-build.md, finding 7).
"""
from __future__ import annotations

import contextlib
import socket
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Iterator, Sequence

# QEMU is located and launched exactly as by the runner (tools/pp/qemu.py):
# --qemu / $PP_QEMU may name the binary or a pp-qemu checkout or build
# directory, and $PP_MSYS2_BIN supplies the DLLs on Windows.
from tools.pp.qemu import DEFAULT_MACHINE as MACHINE
from tools.pp.qemu import DEFAULT_QEMU as QEMU
from tools.pp.qemu import QemuNotFound, find_qemu, qemu_env

__all__ = ["MACHINE", "QEMU", "TwinError", "free_port", "halted",
           "qemu_binary", "qemu_env"]


class TwinError(Exception):
    pass


def qemu_binary(explicit: str | Path | None = None) -> Path:
    try:
        return find_qemu(explicit)
    except QemuNotFound as exc:
        raise TwinError(str(exc)) from exc


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
