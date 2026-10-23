# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""ngspice as a shared library (libngspice), through ctypes.

ngspice keeps one circuit per process (its state is global), so there is
one NgSpice per process; the analog host runs as its own process for that
reason. The library is found from PP_NGSPICE (a path to the library), else
MSYS2's UCRT64 build on Windows, else the system's (libngspice.so.0).

The transient runs on ngspice's background thread ("bg_run"). The caller's
callbacks run there too: on_sync before every time step (it may shorten the
step), on_vsrc for each "external" voltage source at each evaluation, and
on_data after every accepted time point with all vectors' values.
"""

from __future__ import annotations

import ctypes as C
import ctypes.util
import os
import sys
import threading
from pathlib import Path
from typing import Callable

MSYS2_DLL = Path("C:/msys64/ucrt64/bin/libngspice-0.dll")


class NgSpiceError(RuntimeError):
    """ngspice is missing, or rejected the circuit."""


class _VecValues(C.Structure):
    _fields_ = [("name", C.c_char_p), ("creal", C.c_double),
                ("cimag", C.c_double), ("is_scale", C.c_bool),
                ("is_complex", C.c_bool)]


class _VecValuesAll(C.Structure):
    _fields_ = [("veccount", C.c_int), ("vecindex", C.c_int),
                ("vecsa", C.POINTER(C.POINTER(_VecValues)))]


_SendChar = C.CFUNCTYPE(C.c_int, C.c_char_p, C.c_int, C.c_void_p)
_SendStat = C.CFUNCTYPE(C.c_int, C.c_char_p, C.c_int, C.c_void_p)
_ControlledExit = C.CFUNCTYPE(C.c_int, C.c_int, C.c_bool, C.c_bool, C.c_int,
                              C.c_void_p)
_SendData = C.CFUNCTYPE(C.c_int, C.POINTER(_VecValuesAll), C.c_int, C.c_int,
                        C.c_void_p)
_SendInitData = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int, C.c_void_p)
_BGThreadRunning = C.CFUNCTYPE(C.c_int, C.c_bool, C.c_int, C.c_void_p)
_GetVSRCData = C.CFUNCTYPE(C.c_int, C.POINTER(C.c_double), C.c_double,
                           C.c_char_p, C.c_int, C.c_void_p)
_GetSyncData = C.CFUNCTYPE(C.c_int, C.c_double, C.POINTER(C.c_double),
                           C.c_double, C.c_int, C.c_int, C.c_int, C.c_void_p)


def library_path() -> Path | None:
    """Where libngspice is, or None."""
    env = os.environ.get("PP_NGSPICE")
    if env:
        return Path(env)
    if sys.platform == "win32":
        return MSYS2_DLL if MSYS2_DLL.is_file() else None
    found = ctypes.util.find_library("ngspice")
    return Path(found) if found else None


def available() -> bool:
    path = library_path()
    return bool(path) and (path.is_file() or not path.is_absolute())


class NgSpice:
    """The process's ngspice. @on_sync(t, delta) returns a new delta (or
    None to keep it), @on_vsrc(name, t) a voltage, @on_data(values) takes
    {vector: value} at each accepted point."""

    def __init__(self, on_sync: Callable[[float, float], float | None],
                 on_vsrc: Callable[[str, float], float],
                 on_data: Callable[[dict[str, float]], None]):
        path = library_path()
        if not path:
            raise NgSpiceError("ngspice not found (set PP_NGSPICE to "
                               "libngspice's path, or install MSYS2's "
                               "mingw-w64-ucrt-x86_64-ngspice)")
        if sys.platform == "win32" and path.is_absolute():
            os.add_dll_directory(str(path.parent))
        try:
            self.lib = C.CDLL(str(path))
        except OSError as e:
            raise NgSpiceError(f"cannot load {path}: {e}") from e
        self.output: list[str] = []         # ngspice's messages
        self.running = threading.Event()
        self.ended = threading.Event()      # the bg thread finished
        self._on_sync, self._on_vsrc, self._on_data = \
            on_sync, on_vsrc, on_data
        # Keep the callbacks referenced: ngspice holds raw pointers
        self._cbs = (_SendChar(self._send_char), _SendStat(self._send_stat),
                     _ControlledExit(self._exit), _SendData(self._send_data),
                     _SendInitData(self._send_init),
                     _BGThreadRunning(self._bg_running),
                     _GetVSRCData(self._get_vsrc), _GetSyncData(self._sync))
        lib = self.lib
        lib.ngSpice_Init.argtypes = [_SendChar, _SendStat, _ControlledExit,
                                     _SendData, _SendInitData,
                                     _BGThreadRunning, C.c_void_p]
        lib.ngSpice_Init_Sync.argtypes = [_GetVSRCData, C.c_void_p,
                                          _GetSyncData, C.POINTER(C.c_int),
                                          C.c_void_p]
        lib.ngSpice_Command.argtypes = [C.c_char_p]
        lib.ngSpice_Circ.argtypes = [C.POINTER(C.c_char_p)]
        lib.ngSpice_SetBkpt.argtypes = [C.c_double]
        lib.ngSpice_SetBkpt.restype = C.c_bool
        self._ident = C.c_int(0)
        lib.ngSpice_Init(*self._cbs[:6], None)
        lib.ngSpice_Init_Sync(self._cbs[6], None, self._cbs[7],
                              C.byref(self._ident), None)
        self.command("set num_threads=1")
        # The output-memory estimate assumes every time point is kept;
        # with ".save none" only the latest is (the run is open-ended)
        self.command("set no_mem_check")

    # -- callbacks (ngspice's threads) ---------------------------------------

    def _send_char(self, text, ident, user):
        self.output.append(text.decode(errors="replace"))
        return 0

    def _send_stat(self, text, ident, user):
        return 0

    def _exit(self, status, unload, quit_, ident, user):
        self.output.append(f"stderr ngspice exited ({status})")
        return 0

    def _send_data(self, vals, count, ident, user):
        v = vals.contents
        self._on_data({v.vecsa[i].contents.name.decode():
                       v.vecsa[i].contents.creal for i in range(v.veccount)})
        return 0

    def _send_init(self, info, ident, user):
        return 0

    def _bg_running(self, noruns, ident, user):
        if noruns:
            self.running.clear()
            self.ended.set()
        else:
            self.ended.clear()
            self.running.set()
        return 0

    def _get_vsrc(self, pv, t, name, ident, user):
        pv[0] = self._on_vsrc(name.decode().lower(), t)
        return 0

    def _sync(self, t, pdelta, olddelta, redostep, ident, loc, user):
        # loc 0: before a step (after breakpoint handling); loc 1: after
        # it, to accept (0) or redo (1). Only the first chooses the step.
        if loc == 0:
            delta = self._on_sync(t, pdelta[0])
            if delta is not None:
                pdelta[0] = delta
        return 0

    # -- calls ------------------------------------------------------------------

    def command(self, cmd: str) -> None:
        self.lib.ngSpice_Command(cmd.encode())

    def errors(self) -> list[str]:
        return [line[7:] for line in self.output
                if line.startswith("stderr") and
                ("rror" in line or "fatal" in line.lower())]

    def load(self, lines: list[str]) -> None:
        """Load a circuit (its lines, title first, ".end" last)."""
        before = len(self.output)
        arr = (C.c_char_p * (len(lines) + 1))(*[ln.encode() for ln in lines],
                                              None)
        rc = self.lib.ngSpice_Circ(arr)
        errs = [line[7:] for line in self.output[before:]
                if line.startswith("stderr") and "rror" in line]
        if rc or errs:
            raise NgSpiceError("; ".join(errs) or "ngspice rejected the "
                               "circuit")

    def breakpoint(self, t: float) -> None:
        self.lib.ngSpice_SetBkpt(C.c_double(t))

    def start(self) -> None:
        """Run the circuit's transient on ngspice's background thread."""
        self.ended.clear()
        self.command("bg_run")

    def halt(self) -> None:
        if not self.ended.is_set():
            self.command("bg_halt")
