# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Minimal GDB/MI client: drive gdb-multiarch against a remote gdbserver.

The same MI commands work against QEMU's gdbstub and pyOCD's gdbserver,
which is what lets one snapshot script run against the twin and silicon.
Only the parts of MI the harness uses are implemented: result and async
records, stream records, tokens, and the value syntax (c-strings, tuples,
lists).  See "GDB/MI Output Syntax" in the GDB manual.
"""
from __future__ import annotations

import itertools
import queue
import re
import shutil
import subprocess
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

GDB_CANDIDATES = ("gdb-multiarch", "arm-none-eabi-gdb", "C:/msys64/ucrt64/bin/gdb-multiarch.exe")
_KINDS = {"^": "result", "*": "exec", "+": "status", "=": "notify",
          "~": "console", "@": "target", "&": "log"}
_PREFIX = re.compile(r"(\d*)([\^*+=~@&])")
_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}


class MiError(Exception):
    """GDB answered ^error."""


class MiTimeout(Exception):
    """No answer from GDB in time."""


@dataclass
class Record:
    kind: str                   # result, exec, status, notify, console, target, log, raw
    token: int | None
    cls: str                    # "done", "stopped", ... ("" for streams)
    payload: Any                # dict of results, or text for streams
    console: str = ""           # console output collected while waiting for a result


# --- parsing ------------------------------------------------------------------

def parse_line(line: str) -> Record | None:
    """Parse one line of MI output; None for the "(gdb)" prompt or blank lines."""
    line = line.rstrip("\r\n")
    if not line or line.strip() == "(gdb)":
        return None
    m = _PREFIX.match(line)
    if not m:
        return Record("raw", None, "", line)
    token = int(m.group(1)) if m.group(1) else None
    kind = _KINDS[m.group(2)]
    rest = line[m.end():]
    if kind in ("console", "target", "log"):
        return Record(kind, token, "", _Parser(rest).cstring())
    cls, comma, tail = rest.partition(",")
    return Record(kind, token, cls, _Parser(tail).results() if comma else {})


class _Parser:
    def __init__(self, text: str):
        self.s, self.i = text, 0

    def peek(self) -> str:
        return self.s[self.i] if self.i < len(self.s) else ""

    def expect(self, char: str) -> None:
        if self.peek() != char:
            raise ValueError(f"MI parse error at {self.i}: expected {char!r} in {self.s!r}")
        self.i += 1

    def results(self, end: str = "") -> dict[str, Any]:
        out: dict[str, Any] = {}
        while self.peek() and self.peek() != end:
            name, value = self.result()
            out[name] = value
            if self.peek() == ",":
                self.i += 1
        return out

    def result(self) -> tuple[str, Any]:
        eq = self.s.index("=", self.i)
        name = self.s[self.i:eq]
        self.i = eq + 1
        return name, self.value()

    def value(self) -> Any:
        char = self.peek()
        if char == '"':
            return self.cstring()
        if char == "{":
            self.i += 1
            value = self.results("}")
            self.expect("}")
            return value
        if char == "[":
            return self.list()
        raise ValueError(f"MI parse error at {self.i}: unexpected {char!r} in {self.s!r}")

    def list(self) -> list[Any]:
        """A list of values; for a list of results the names are dropped."""
        self.expect("[")
        items = []
        while self.peek() and self.peek() != "]":
            items.append(self.value() if self.peek() in '"{[' else self.result()[1])
            if self.peek() == ",":
                self.i += 1
        self.expect("]")
        return items

    def cstring(self) -> str:
        self.expect('"')
        out = bytearray()
        while True:
            char = self.peek()
            if not char:
                raise ValueError(f"MI parse error: unterminated string in {self.s!r}")
            self.i += 1
            if char == '"':
                return out.decode("utf-8", errors="replace")
            if char != "\\":
                out += char.encode("utf-8")
                continue
            esc = self.peek()
            self.i += 1
            if esc in "01234567":
                digits = re.match(r"[0-7]{1,3}", self.s[self.i - 1:]).group(0)
                self.i += len(digits) - 1
                out.append(int(digits, 8) & 0xFF)
            else:
                out += _ESCAPES.get(esc, esc).encode("utf-8")


def mi_quote(text: str) -> str:
    """Quote a command argument as an MI c-string."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


# --- the client -----------------------------------------------------------------

def find_gdb(explicit: str | None = None) -> str:
    for candidate in ((explicit,) if explicit else GDB_CANDIDATES):
        found = shutil.which(candidate) or (candidate if Path(candidate).is_file() else None)
        if found:
            return found
    raise FileNotFoundError("no gdb-multiarch or arm-none-eabi-gdb found; pass --gdb")


@dataclass
class GdbMi:
    """A gdb process in MI mode.  Use as a context manager."""

    gdb: str | None = None
    timeout: float = 10.0
    proc: subprocess.Popen = field(init=False, repr=False)
    log: deque = field(default_factory=lambda: deque(maxlen=200), repr=False)

    def __post_init__(self) -> None:
        self.proc = subprocess.Popen(
            [find_gdb(self.gdb), "--interpreter=mi3", "--nx", "--quiet"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self._records: queue.Queue = queue.Queue()
        self._tokens = itertools.count(1)
        self._stops: deque = deque()
        threading.Thread(target=self._pump, args=(self.proc.stdout, True), daemon=True).start()
        threading.Thread(target=self._pump, args=(self.proc.stderr, False), daemon=True).start()

    def _pump(self, stream, is_mi: bool) -> None:
        for raw in iter(stream.readline, b""):
            line = raw.decode("utf-8", errors="replace")
            if not is_mi:
                self.log.append(("stderr", line.rstrip()))
                continue
            try:
                record = parse_line(line)
            except ValueError as exc:
                record = Record("raw", None, "", f"{line.rstrip()} ({exc})")
            if record:
                self._records.put(record)
        if is_mi:
            self._records.put(None)             # gdb exited

    def _next(self, deadline: float) -> Record:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise MiTimeout("gdb did not answer in time")
        try:
            record = self._records.get(timeout=remaining)
        except queue.Empty:
            raise MiTimeout("gdb did not answer in time") from None
        if record is None:
            raise MiError(f"gdb exited: {list(self.log)[-5:]}")
        if record.kind in ("console", "target", "log", "raw"):
            self.log.append((record.kind, record.payload))
        elif record.kind == "exec" and record.cls == "stopped":
            self._stops.append(record.payload)
        return record

    def command(self, command: str, timeout: float | None = None) -> Record:
        """Send one MI command and wait for its result record."""
        token = next(self._tokens)
        self.proc.stdin.write(f"{token}{command}\n".encode("utf-8"))
        self.proc.stdin.flush()
        deadline = time.monotonic() + (timeout or self.timeout)
        console = []
        while True:
            record = self._next(deadline)
            if record.kind == "console":
                console.append(record.payload)
            if record.kind == "result" and record.token == token:
                if record.cls == "error":
                    raise MiError(f"{command}: {record.payload.get('msg', record.payload)}")
                record.console = "".join(console)
                return record

    def console(self, command: str, timeout: float | None = None) -> str:
        """Run a CLI command (e.g. "monitor reset halt"); return its console output."""
        return self.command(f"-interpreter-exec console {mi_quote(command)}", timeout).console

    def wait_stopped(self, timeout: float) -> dict | None:
        """Payload of the next *stopped record, or None after timeout seconds."""
        deadline = time.monotonic() + timeout
        while not self._stops:
            try:
                self._next(deadline)
            except MiTimeout:
                return None
        return self._stops.popleft()

    def clear_stops(self) -> None:
        self._stops.clear()

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self.command("-gdb-exit", timeout=3)
            except (MiError, MiTimeout, OSError):
                pass
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

    def __enter__(self) -> "GdbMi":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
