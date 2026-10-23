# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Start the analog host for a twin: a subprocess ("python -m
tools.pp_analog serve"), since ngspice holds one circuit per process."""

from __future__ import annotations

import json
import subprocess
import sys
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
READY_S = 30.0
# Machines with the co-simulation port ("cosim" property)
COSIM_MACHINES = ("pic32cm-mc00-cpro", "pic32cm-jh01-cpro")


class HostError(RuntimeError):
    """The analog host did not start."""


class HostProcess:
    """The host serving @circuit; qemu_args() connects a twin to it. With
    @on_state, the host runs interactive: set_control() changes a live
    control, and on_state(t, {vector: average}) runs (on a reader thread)
    about 30 times a second."""

    def __init__(self, circuit: str | Path, tick_us: float = 1000.0,
                 tmax: float = 100e-6, on_state=None):
        self.on_state = on_state
        self.proc = subprocess.Popen(
            [sys.executable, "-m", "tools.pp_analog", "serve",
             str(Path(circuit).resolve()), "--tick-us", str(tick_us),
             "--tmax", str(tmax),
             *(["--interactive"] if on_state else [])],
            cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            stdin=subprocess.PIPE if on_state else subprocess.DEVNULL,
            text=True, encoding="utf-8", errors="replace")
        self.stderr: list[str] = []
        threading.Thread(target=self._drain, daemon=True).start()
        line = self._first_line()
        if not line.startswith("ready "):
            self.proc.kill()
            raise HostError("analog host: " + (
                "".join(self.stderr).strip() or line.strip() or
                "did not start"))
        self.port = int(line.split()[1])
        self.report: dict | None = None
        if on_state:
            threading.Thread(target=self._states, daemon=True).start()

    def _states(self) -> None:
        for line in self.proc.stdout:
            if line.startswith("state "):
                d = json.loads(line[6:])
                self.on_state(d["t"], d["avg"])
            elif line.startswith("done "):
                self.report = json.loads(line[5:])

    def set_control(self, name: str, value: float) -> None:
        try:
            self.proc.stdin.write(f"set {name} {value!r}\n")
            self.proc.stdin.flush()
        except (OSError, ValueError):
            pass                        # the host has gone

    def _drain(self) -> None:
        for line in self.proc.stderr:
            self.stderr.append(line)

    def _first_line(self) -> str:
        out: list[str] = []
        t = threading.Thread(target=lambda: out.append(
            self.proc.stdout.readline()), daemon=True)
        t.start()
        t.join(READY_S)
        return out[0] if out else ""

    def qemu_args(self, chardev: str = "cosim") -> list[str]:
        return ["-chardev", f"socket,id={chardev},host=127.0.0.1,"
                            f"port={self.port},nodelay=on",
                "-machine", f"cosim={chardev}"]

    def finish(self, timeout: float = 10.0) -> dict:
        """Wait for the host (the twin has gone) and return its report:
        {"samples", "ticks", "events", "steps", "errors"}."""
        if self.on_state:               # stdout is the reader's
            try:
                self.proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
            report = self.report or {"errors": []}
        else:
            try:
                out, _ = self.proc.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                out, _ = self.proc.communicate()
            report = {"errors": []}
            for line in out.splitlines():
                if line.startswith("done "):
                    report = json.loads(line[5:])
        if self.proc.returncode not in (0, 2) and not report["errors"]:
            report["errors"] = ["".join(self.stderr).strip() or
                                f"analog host exited {self.proc.returncode}"]
        return report

    def kill(self) -> None:
        if self.proc.poll() is None:
            self.proc.kill()
