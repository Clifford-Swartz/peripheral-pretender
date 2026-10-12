# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Run a firmware ELF on the twin and collect what a test needs.

The result holds the firmware's ``PP:EXIT`` code, the UART transcript and a
summary of the unimplemented and guest-error accesses QEMU logged (the
"model backlog").

UART transport: the runner listens on an ephemeral loopback TCP port and
QEMU connects to it as a client chardev (``-chardev socket,...`` plus
``-serial chardev:...``). Measured on Windows against ``-serial stdio``
with pipes, where QEMU drops input bytes whenever the guest UART cannot
take them and strips CR (chardev/char-win-stdio.c), and against QEMU as the
TCP server, which needs a free-port guess and a connect retry loop. TCP
gives the guest backpressure, so no byte is lost in either direction.

Input is written once the firmware has printed something (or *input_after*
text has appeared, or after a short silence). Bytes that reach QEMU before
the guest enables its receiver stay queued until the UART model calls
qemu_chr_fe_accept_input(), and models that do not do so on receiver enable
(CMSDK APB UART) would leave them queued forever.
"""

from __future__ import annotations

import dataclasses
import os
import select
import socket
import subprocess
import sys
import tempfile
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Sequence

from .protocol import ExitScanner, process_exit_status
from .qemu import DEFAULT_MACHINE, find_qemu, qemu_env
from .unimp import summarize_log

EXIT_TIMEOUT = 124      # no PP:EXIT before the timeout (as GNU timeout)
EXIT_NO_EXIT_LINE = 125  # QEMU ended without printing PP:EXIT

STATUS_EXIT = "exit"
STATUS_TIMEOUT = "timeout"
STATUS_QEMU_EXITED = "qemu-exited"

LOG_ITEMS = ("unimp", "guest_errors")
UART_CHARDEV = "pp_uart0"

POLL_S = 0.05
QUIET_EXIT_S = 0.2       # accept an unterminated exit line after this
DRAIN_S = 0.05           # keep reading this long after the exit line
SILENT_INPUT_S = 2.0     # send input anyway if the firmware stays silent
STDERR_TAIL_LINES = 40

_WIN_DLL_NOT_FOUND = 0xC0000135     # NTSTATUS STATUS_DLL_NOT_FOUND


class RunnerError(RuntimeError):
    """The run could not be set up (missing ELF, unusable options)."""


@dataclass
class RunOptions:
    elf: str | os.PathLike[str]
    machine: str = DEFAULT_MACHINE
    qemu: str | os.PathLike[str] | None = None
    timeout_s: float | None = 10.0
    icount_shift: int | None = None
    gdb_port: int | None = None
    trace: Sequence[str] = ()
    input_text: str | bytes | None = None
    input_after: str | None = None
    extra_args: Sequence[str] = ()
    log_path: str | os.PathLike[str] | None = None
    on_uart: Callable[[bytes], None] | None = None


@dataclass
class RunResult:
    exit_code: int
    status: str
    firmware_exit: int | None
    uart: str
    unimplemented_summary: dict
    trace: dict | None
    qemu_stderr_tail: str
    qemu_returncode: int | None
    duration_s: float
    machine: str
    elf: str
    command: list[str] = field(default_factory=list)
    log_path: str | None = None

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)


def run(options: RunOptions) -> RunResult:
    """Run *options.elf* on QEMU until PP:EXIT, QEMU exit or timeout."""
    elf = Path(options.elf).resolve()
    if not elf.is_file():
        raise RunnerError(f"ELF not found: {elf}")
    _check_options(options)
    qemu = find_qemu(options.qemu)
    trace_patterns = _trace_patterns(options.trace, options.extra_args)

    with tempfile.TemporaryDirectory(prefix="pp-run-",
                                     ignore_cleanup_errors=True) as tmp:
        log_path = Path(options.log_path or Path(tmp) / "qemu.log").resolve()
        with _UartLink() as link:
            cmd = build_command(qemu, elf, options, link.qemu_args(), log_path)
            outcome = _launch_and_pump(cmd, link, options)
        summary = summarize_log(log_path, trace_patterns)

    if outcome.status == STATUS_EXIT:
        exit_code = process_exit_status(outcome.firmware_exit)
    elif outcome.status == STATUS_TIMEOUT:
        exit_code = EXIT_TIMEOUT
    else:
        exit_code = EXIT_NO_EXIT_LINE
    return RunResult(
        exit_code=exit_code,
        status=outcome.status,
        firmware_exit=outcome.firmware_exit,
        uart=decode_uart(outcome.transcript),
        unimplemented_summary=summary.as_dict(),
        trace=summary.trace_dict() if trace_patterns else None,
        qemu_stderr_tail=outcome.stderr_tail,
        qemu_returncode=outcome.returncode,
        duration_s=round(outcome.duration_s, 3),
        machine=options.machine,
        elf=str(elf),
        command=cmd,
        log_path=str(log_path) if options.log_path else None,
    )


def decode_uart(raw: bytes) -> str:
    """UART bytes as text: UTF-8 (undecodable bytes as \\xNN), CRLF -> LF."""
    return raw.decode("utf-8", "backslashreplace").replace("\r\n", "\n")


def build_command(qemu: Path, elf: Path, options: RunOptions,
                  uart_args: Sequence[str], log_path: Path) -> list[str]:
    extra_args, extra_items = _split_log_items(options.extra_args)
    log_items = list(LOG_ITEMS)
    log_items += [i for i in extra_items if i not in log_items]
    cmd = [str(qemu), "-M", options.machine,
           "-display", "none", "-monitor", "none", *uart_args,
           "-d", ",".join(log_items), "-D", str(log_path),
           "-kernel", str(elf)]
    if options.icount_shift is not None:
        cmd += ["-icount", f"shift={options.icount_shift},sleep=off"]
    if options.gdb_port is not None:
        cmd += ["-gdb", f"tcp:127.0.0.1:{options.gdb_port}", "-S"]
    for pattern in options.trace:
        cmd += ["-trace", pattern]
    return cmd + extra_args


def _check_options(options: RunOptions) -> None:
    if options.timeout_s is not None and options.timeout_s <= 0:
        raise RunnerError("timeout must be positive")
    shift = options.icount_shift
    if shift is not None and not 0 <= shift <= 10:
        raise RunnerError("icount shift must be between 0 and 10")


def _split_log_items(args: Sequence[str]) -> tuple[list[str], list[str]]:
    """Pull "-d ITEMS" out of extra QEMU args: a second -d would replace
    the runner's own, so the items are merged into one option instead."""
    rest: list[str] = []
    items: list[str] = []
    args = list(args)
    i = 0
    while i < len(args):
        if args[i] == "-d" and i + 1 < len(args):
            items += [x for x in args[i + 1].split(",") if x]
            i += 2
        else:
            rest.append(args[i])
            i += 1
    return rest, items


def _trace_patterns(trace: Sequence[str],
                    extra_args: Sequence[str]) -> list[str]:
    patterns = list(trace)
    args = list(extra_args)
    for flag, value in zip(args, args[1:]):
        if flag == "-trace":
            if value.startswith("enable="):
                patterns.append(value[len("enable="):])
            elif "=" not in value:
                patterns.append(value)
    return patterns


@dataclass
class _Outcome:
    status: str
    firmware_exit: int | None
    transcript: bytes
    stderr_tail: str
    returncode: int | None
    duration_s: float


class _QemuGone(Exception):
    pass


class _DeadlineReached(Exception):
    pass


class _UartLink:
    """Loopback listener that QEMU joins as the backend of -serial 0."""

    def __init__(self) -> None:
        self._listener = socket.create_server(("127.0.0.1", 0))
        self.port = self._listener.getsockname()[1]
        self.conn: socket.socket | None = None

    def __enter__(self) -> "_UartLink":
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()

    def qemu_args(self) -> list[str]:
        return ["-chardev",
                f"socket,id={UART_CHARDEV},host=127.0.0.1,port={self.port}",
                "-serial", f"chardev:{UART_CHARDEV}"]

    def accept(self, proc: subprocess.Popen,
               deadline: float | None) -> socket.socket:
        self._listener.settimeout(POLL_S)
        while self.conn is None:
            try:
                self.conn, _ = self._listener.accept()
            except TimeoutError:
                if proc.poll() is not None:
                    raise _QemuGone from None
                if deadline is not None and time.monotonic() >= deadline:
                    raise _DeadlineReached from None
        self._listener.close()
        self.conn.setblocking(False)
        return self.conn

    def close(self) -> None:
        self._listener.close()
        if self.conn is not None:
            self.conn.close()


class _OutputTail:
    """Drains QEMU's stdout+stderr on a thread, keeping the last lines."""

    def __init__(self, stream, max_lines: int = STDERR_TAIL_LINES) -> None:
        self._lines: deque[str] = deque(maxlen=max_lines)
        self._thread = threading.Thread(target=self._drain, args=(stream,),
                                        daemon=True)
        self._thread.start()

    def _drain(self, stream) -> None:
        with stream:
            for raw in iter(stream.readline, b""):
                self._lines.append(
                    raw.decode("utf-8", "replace").rstrip("\r\n"))

    def text(self, wait_s: float = 5.0) -> str:
        self._thread.join(wait_s)
        return "\n".join(self._lines)


def _launch_and_pump(cmd: list[str], link: _UartLink,
                     options: RunOptions) -> _Outcome:
    start = time.monotonic()
    deadline = None if options.timeout_s is None else start + options.timeout_s
    try:
        proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, env=qemu_env())
    except OSError as exc:
        raise RunnerError(f"cannot start {cmd[0]}: {exc}") from exc
    tail = _OutputTail(proc.stdout)
    pump = _Pump(_encode_input(options.input_text), options.input_after,
                 options.on_uart)
    status = STATUS_QEMU_EXITED
    followed = False
    try:
        conn = link.accept(proc, deadline)
        status = pump.run(conn, proc, deadline)
        if status == STATUS_EXIT and options.gdb_port is not None:
            # The session belongs to the debugger: stopping QEMU now would
            # pull the target out from under it.
            followed = pump.follow(conn, proc, deadline)
    except _QemuGone:
        status = STATUS_QEMU_EXITED
    except _DeadlineReached:
        status = STATUS_TIMEOUT
    finally:
        # QEMU closes the UART just before it exits; give it a moment so
        # its own exit status is reported rather than our kill.
        grace = 1.0 if status == STATUS_QEMU_EXITED or followed else 0.0
        exited_by_itself = _stop(proc, grace)
    stderr_tail = tail.text()
    if exited_by_itself and proc.returncode == _WIN_DLL_NOT_FOUND:
        stderr_tail += ("\n[pp] QEMU could not load its DLLs; set "
                        "PP_MSYS2_BIN to the MSYS2 ucrt64/bin directory")
    firmware_exit = pump.scanner.exit_code if status == STATUS_EXIT else None
    return _Outcome(
        status=status,
        firmware_exit=firmware_exit,
        transcript=bytes(pump.transcript),
        stderr_tail=stderr_tail.strip("\n"),
        returncode=proc.returncode if exited_by_itself else None,
        duration_s=time.monotonic() - start,
    )


def _encode_input(text: str | bytes | None) -> bytes:
    if text is None:
        return b""
    return text if isinstance(text, bytes) else text.encode("utf-8")


def _stop(proc: subprocess.Popen, grace_s: float = 0.0) -> bool:
    """Stop QEMU; return True if it exited by itself (within *grace_s*)."""
    try:
        proc.wait(timeout=grace_s)
        return True
    except subprocess.TimeoutExpired:
        pass
    if sys.platform != "win32":
        proc.terminate()
        try:
            proc.wait(timeout=2)
            return False
        except subprocess.TimeoutExpired:
            pass
    proc.kill()
    proc.wait()
    return False


class _Pump:
    """Moves UART bytes between the socket and the run until it ends."""

    def __init__(self, input_bytes: bytes, input_after: str | None,
                 on_uart: Callable[[bytes], None] | None) -> None:
        self.scanner = ExitScanner()
        self.transcript = bytearray()
        self._held_input = input_bytes
        self._input_after = (None if input_after is None
                             else input_after.encode())
        self._search_from = 0
        self._on_uart = on_uart

    def run(self, conn: socket.socket, proc: subprocess.Popen,
            deadline: float | None) -> str:
        connected = last_rx = time.monotonic()
        outgoing = b""
        while True:
            now = time.monotonic()
            if self._held_input and self._input_ready(now - connected):
                outgoing, self._held_input = self._held_input, b""
            if deadline is not None and now >= deadline:
                return STATUS_TIMEOUT
            wait = POLL_S if deadline is None else min(POLL_S, deadline - now)
            readable, writable, _ = select.select(
                [conn], [conn] if outgoing else [], [], wait)
            if readable:
                data = _recv(conn)
                if data == b"":
                    return STATUS_QEMU_EXITED   # QEMU closed the chardev
                if data:
                    last_rx = now
                    if self._receive(data):
                        self._drain(conn)
                        return STATUS_EXIT
            elif (self.scanner.pending_exit() is not None
                  and now - last_rx >= QUIET_EXIT_S):
                return STATUS_EXIT
            elif proc.poll() is not None:
                return STATUS_QEMU_EXITED
            if writable and outgoing:
                outgoing = outgoing[_send(conn, outgoing[:4096]):]

    def follow(self, conn: socket.socket, proc: subprocess.Popen,
               deadline: float | None) -> bool:
        """Keep collecting UART output until QEMU ends (True) or the
        deadline passes (False)."""
        while deadline is None or time.monotonic() < deadline:
            if select.select([conn], [], [], POLL_S)[0]:
                data = _recv(conn)
                if data == b"":
                    return True
                if data:
                    self._receive(data)
            elif proc.poll() is not None:
                return True
        return False

    def _receive(self, data: bytes) -> bool:
        self.transcript += data
        if self._on_uart is not None:
            self._on_uart(data)
        return self.scanner.feed(data) is not None

    def _drain(self, conn: socket.socket) -> None:
        """Collect what follows the exit line within DRAIN_S: an exit line
        ends at its CR, and the LF is usually still on the way."""
        end = time.monotonic() + DRAIN_S
        while (left := end - time.monotonic()) > 0:
            if not select.select([conn], [], [], left)[0]:
                return
            data = _recv(conn)
            if not data:
                return
            self._receive(data)

    def _input_ready(self, since_connect: float) -> bool:
        if self._input_after is None:
            return bool(self.transcript) or since_connect >= SILENT_INPUT_S
        found = self.transcript.find(self._input_after, self._search_from) >= 0
        self._search_from = max(
            0, len(self.transcript) - len(self._input_after))
        return found


def _recv(conn: socket.socket) -> bytes | None:
    """Received bytes, b"" once QEMU has gone, None if nothing is ready."""
    try:
        return conn.recv(65536)
    except BlockingIOError:
        return None
    except (ConnectionResetError, ConnectionAbortedError):
        return b""


def _send(conn: socket.socket, data: bytes) -> int:
    """Bytes consumed from *data*; all of them once QEMU has gone (the
    next receive then reports the end of the run)."""
    try:
        return conn.send(data)
    except BlockingIOError:
        return 0
    except OSError:
        return len(data)
