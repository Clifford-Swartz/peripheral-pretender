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

A second loopback socket carries a QMP monitor. It is only used when a run
times out, to ask QEMU where the CPU is ("info registers") before it is
killed.
"""

from __future__ import annotations

import dataclasses
import fnmatch
import hashlib
import json
import os
import re
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
from .qemu import (DEFAULT_MACHINE, EM_ARM, EM_AVR, find_qemu,
                   machine_target, qemu_env, trace_events)
from .unimp import summarize_log

EXIT_TIMEOUT = 124      # no PP:EXIT before the timeout (as GNU timeout)
EXIT_NO_EXIT_LINE = 125  # QEMU ended without printing PP:EXIT

STATUS_EXIT = "exit"
STATUS_TIMEOUT = "timeout"
STATUS_QEMU_EXITED = "qemu-exited"
STATUS_CPU_LOCKUP = "cpu-lockup"   # QEMU aborted: "qemu: fatal: Lockup..."

# invalid_mem (QEMU 9.1+) carries "Invalid read/write at addr ...": bus
# faults to addresses no device claims.
LOG_ITEMS = ("unimp", "guest_errors", "invalid_mem")
UART_CHARDEV = "pp_uart0"
QMP_TIMEOUT_S = 1.0

POLL_S = 0.05
QUIET_EXIT_S = 0.2       # accept an unterminated exit line after this
DRAIN_S = 0.05           # keep reading this long after the exit line
SILENT_INPUT_S = 2.0     # send input anyway if the firmware stays silent
STDERR_TAIL_LINES = 40

_WIN_DLL_NOT_FOUND = 0xC0000135     # NTSTATUS STATUS_DLL_NOT_FOUND
_NTSTATUS_NAMES = {
    0xC0000005: "STATUS_ACCESS_VIOLATION",
    0xC00000FD: "STATUS_STACK_OVERFLOW",
    0xC0000135: "STATUS_DLL_NOT_FOUND",
    0xC000013A: "STATUS_CONTROL_C_EXIT",
    0xC0000409: "STATUS_STACK_BUFFER_OVERRUN, e.g. abort()",
}


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
    detail: str | None = None       # why the run ended, where the CPU was
    warnings: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)


def run(options: RunOptions) -> RunResult:
    """Run *options.elf* on QEMU until PP:EXIT, QEMU exit or timeout."""
    elf = Path(options.elf).resolve()
    target = check_elf(elf)
    _check_options(options)
    extra = split_extra_args(options.extra_args)
    machine = extra.machine or options.machine
    if target != machine_target(machine):
        raise RunnerError(f"{elf.name} is {target} firmware; machine "
                          f"{machine} runs {machine_target(machine)}")
    kept_log = _kept_log(options.log_path, extra.log_file)
    qemu = find_qemu(options.qemu, target)
    trace_patterns = _trace_patterns(options.trace, options.extra_args)
    warnings = unknown_trace_patterns(qemu, trace_patterns)

    with tempfile.TemporaryDirectory(prefix="pp-run-",
                                     ignore_cleanup_errors=True) as tmp:
        log_path = kept_log or (Path(tmp) / "qemu.log").resolve()
        with _UartLink() as link, _Monitor() as monitor:
            cmd = build_command(qemu, elf, options,
                                link.qemu_args() + monitor.qemu_args(),
                                log_path)
            outcome = _launch_and_pump(cmd, link, monitor, options)
        summary = summarize_log(log_path, trace_patterns)

    status, detail = outcome.status, outcome.detail
    fatal = outcome.fatal or summary.fatal
    if status == STATUS_QEMU_EXITED and fatal:
        if fatal.startswith("Lockup"):
            status = STATUS_CPU_LOCKUP
            reason = fatal[len("Lockup"):].lstrip(": ")
            where = cpu_location(outcome.fatal_dump)
            detail = f"CPU lockup ({reason})" + (f"; {where}" if where else "")
        else:
            detail = f"QEMU fatal error: {fatal}"
    if status == STATUS_EXIT:
        exit_code = process_exit_status(outcome.firmware_exit)
    elif status == STATUS_TIMEOUT:
        exit_code = EXIT_TIMEOUT
    else:
        exit_code = EXIT_NO_EXIT_LINE
    return RunResult(
        exit_code=exit_code,
        status=status,
        firmware_exit=outcome.firmware_exit,
        uart=decode_uart(outcome.transcript),
        unimplemented_summary=summary.as_dict(),
        trace=summary.trace_dict() if trace_patterns else None,
        qemu_stderr_tail=outcome.stderr_tail,
        qemu_returncode=outcome.returncode,
        duration_s=round(outcome.duration_s, 3),
        machine=extra.machine or options.machine,
        elf=str(elf),
        command=cmd,
        log_path=str(log_path) if kept_log else None,
        detail=detail,
        warnings=warnings,
    )


def check_elf(path: Path) -> str:
    """Refuse anything but an Arm or AVR ELF before QEMU sees it (QEMU
    would load a raw file as a kernel image and run garbage). Returns the
    target, "arm" or "avr"."""
    if path.is_dir():
        raise RunnerError(f"{path} is a directory, expected an ELF file")
    if not path.is_file():
        raise RunnerError(f"ELF not found: {path}")
    with path.open("rb") as f:
        header = f.read(20)
    if header[:4] != b"\x7fELF":
        raise RunnerError(f"not an ELF file: {path}")
    if len(header) < 20:
        raise RunnerError(f"truncated ELF header: {path}")
    machine = int.from_bytes(header[18:20],
                             "big" if header[5] == 2 else "little")
    if machine == EM_AVR:
        return "avr"
    if machine != EM_ARM:
        raise RunnerError(f"not an Arm or AVR ELF (e_machine {machine}, "
                          f"expected {EM_ARM} or {EM_AVR}): {path}")
    return "arm"


def describe_returncode(code: int) -> str:
    """QEMU's exit status for people: Windows NTSTATUS values in hex with
    their name, POSIX signals by number."""
    if code < 0 and sys.platform != "win32":
        return f"signal {-code}"
    if code < 0 or code >= 0x80000000:
        code &= 0xFFFFFFFF
        name = _NTSTATUS_NAMES.get(code)
        return f"0x{code:08X}" + (f" ({name})" if name else "")
    return str(code)


_EXCEPTIONS = {1: "Reset", 2: "NMI", 3: "HardFault", 4: "MemManage",
               5: "BusFault", 6: "UsageFault", 7: "SecureFault",
               11: "SVCall", 12: "DebugMonitor", 14: "PendSV",
               15: "SysTick"}
_CPU_REG = re.compile(r"\b(R15|XPSR)=([0-9a-fA-F]{8})\b")


def cpu_location(dump: str) -> str | None:
    """"CPU in HardFault (IPSR=3) at PC=0x..." from an M-profile register
    dump (HMP "info registers", or the dump after "qemu: fatal:")."""
    regs = dict(_CPU_REG.findall(dump))
    if "R15" not in regs:
        return None
    where = ""
    if "XPSR" in regs:
        ipsr = int(regs["XPSR"], 16) & 0x1FF
        if ipsr == 0:
            where = " thread mode"
        else:
            name = _EXCEPTIONS.get(ipsr) or (
                f"IRQ {ipsr - 16}" if ipsr >= 16 else "exception")
            where = f" {name} (IPSR={ipsr})"
    return f"CPU in{where} at PC=0x{int(regs['R15'], 16):08X}"


def decode_uart(raw: bytes) -> str:
    """UART bytes as text: UTF-8 (undecodable bytes as \\xNN), CRLF -> LF."""
    return raw.decode("utf-8", "backslashreplace").replace("\r\n", "\n")


def build_command(qemu: Path, elf: Path, options: RunOptions,
                  uart_args: Sequence[str], log_path: Path) -> list[str]:
    extra = split_extra_args(options.extra_args)
    log_items = list(LOG_ITEMS)
    log_items += [i for i in extra.log_items if i not in log_items]
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
    return cmd + extra.rest


def _check_options(options: RunOptions) -> None:
    if options.timeout_s is not None and options.timeout_s <= 0:
        raise RunnerError("timeout must be positive")
    shift = options.icount_shift
    if shift is not None and not 0 <= shift <= 10:
        raise RunnerError("icount shift must be between 0 and 10")


@dataclass
class ExtraArgs:
    rest: list[str]             # passed to QEMU after the runner's own
    log_items: list[str]        # from -d, merged into the runner's -d
    log_file: str | None        # from -D, used as the runner's log file
    machine: str | None         # machine type named by -M/-machine


def split_extra_args(args: Sequence[str]) -> ExtraArgs:
    """Pull "-d ITEMS" and "-D FILE" out of extra QEMU args: a second -d or
    -D would replace the runner's own (and empty the backlog), so the items
    are merged and the file becomes the runner's log. -M/-machine stay
    (QEMU merges repeated -machine options, so "-machine sw0-pressed=on"
    adds to --machine); a machine type named there is what runs."""
    extra = ExtraArgs([], [], None, None)
    args = list(args)
    i = 0
    while i < len(args):
        flag = args[i]
        value = args[i + 1] if i + 1 < len(args) else None
        if flag == "-d" and value is not None:
            extra.log_items += [x for x in value.split(",") if x]
            i += 2
            continue
        if flag == "-D" and value is not None:
            extra.log_file = value
            i += 2
            continue
        if flag in ("-M", "-machine", "--machine") and value is not None:
            extra.machine = _machine_type(value) or extra.machine
        extra.rest.append(flag)
        i += 1
    return extra


def _machine_type(value: str) -> str | None:
    """Type in a -machine value: "type=X" or a leading bare word; None for
    option-only values such as "sw0-pressed=on"."""
    for n, part in enumerate(value.split(",")):
        if part.startswith("type="):
            return part[len("type="):]
        if n == 0 and part and "=" not in part:
            return part
    return None


def _kept_log(log_path: str | os.PathLike[str] | None,
              extra_log: str | None) -> Path | None:
    """The log file to keep (--log, or -D in the extra args) with its
    directory created; None means a temporary log."""
    if log_path and extra_log and \
            Path(log_path).resolve() != Path(extra_log).resolve():
        raise RunnerError("--log and -D in the extra QEMU args name "
                          "different files; give one")
    chosen = log_path or extra_log
    if not chosen:
        return None
    path = Path(chosen).resolve()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise RunnerError(f"cannot create the log directory "
                          f"{path.parent}: {exc}") from exc
    return path


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


def unknown_trace_patterns(qemu: Path, patterns: Sequence[str]) -> list[str]:
    """Warnings for trace patterns that match no event of this QEMU build
    (a misspelt --trace otherwise just reports 0 events)."""
    if not patterns:
        return []
    try:
        events = _cached_trace_events(qemu)
    except (OSError, RuntimeError, subprocess.SubprocessError):
        return []
    return [f"trace pattern {p!r} matches no trace event of this QEMU "
            f"(see '{qemu.name} -trace help')"
            for p in patterns if not fnmatch.filter(events, p)]


def _cached_trace_events(qemu: Path) -> list[str]:
    """``qemu -trace help``, cached per binary (path, size, mtime) in the
    temporary directory: listing the events takes about 0.2 s."""
    stat = qemu.stat()
    key = hashlib.sha1(f"{qemu.resolve()}|{stat.st_size}|{stat.st_mtime_ns}"
                       .encode()).hexdigest()[:16]
    cache = Path(tempfile.gettempdir()) / f"pp-trace-events-{key}.txt"
    try:
        events = cache.read_text(encoding="utf-8").split()
        if events:
            return events
    except OSError:
        pass
    events = trace_events(qemu)
    try:
        cache.write_text("\n".join(events), encoding="utf-8")
    except OSError:
        pass
    return events


@dataclass
class _Outcome:
    status: str
    firmware_exit: int | None
    transcript: bytes
    stderr_tail: str
    returncode: int | None
    duration_s: float
    detail: str | None = None
    fatal: str | None = None        # message of "qemu: fatal: ..."
    fatal_dump: str = ""            # the register dump that follows it


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


class _Monitor:
    """Loopback listener for a QMP monitor. QEMU connects while it starts
    (the connection waits in the listen backlog); it is accepted only to
    ask where the CPU is when a run times out."""

    def __init__(self) -> None:
        self._listener = socket.create_server(("127.0.0.1", 0))
        self.port = self._listener.getsockname()[1]

    def __enter__(self) -> "_Monitor":
        return self

    def __exit__(self, *exc: object) -> None:
        self._listener.close()

    def qemu_args(self) -> list[str]:
        # -qmp rather than -chardev + -mon: QEMU 11 deprecates -mon.
        return ["-qmp", f"tcp:127.0.0.1:{self.port}"]

    def registers(self, timeout_s: float = QMP_TIMEOUT_S) -> str | None:
        """HMP "info registers" through QMP, or None if QEMU does not
        answer in time. It reads CPU state only, no device registers."""
        deadline = time.monotonic() + timeout_s
        try:
            self._listener.settimeout(timeout_s)
            conn, _ = self._listener.accept()
        except OSError:
            return None
        with conn:
            try:
                buf = b""
                for request in ({"execute": "qmp_capabilities"},
                                {"execute": "human-monitor-command",
                                 "arguments": {
                                     "command-line": "info registers"}}):
                    reply, buf = _qmp(conn, request, buf, deadline)
            except (OSError, ValueError):
                return None
        ret = reply.get("return")
        return ret if isinstance(ret, str) else None


def _qmp(conn: socket.socket, request: dict, buf: bytes,
         deadline: float) -> tuple[dict, bytes]:
    """Send one QMP command; return its reply (the greeting and events are
    skipped) and the bytes read beyond it."""
    conn.sendall(json.dumps(request).encode() + b"\r\n")
    while True:
        while b"\n" not in buf:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError("no QMP reply")
            conn.settimeout(left)
            data = conn.recv(65536)
            if not data:
                raise ConnectionError("QMP connection closed")
            buf += data
        line, buf = buf.split(b"\n", 1)
        message = json.loads(line) if line.strip() else {}
        if "return" in message or "error" in message:
            return message, buf


class _OutputTail:
    """Drains QEMU's stdout+stderr on a thread, keeping the last lines and
    the "qemu: fatal:" message with the register dump that follows it."""

    FATAL = "qemu: fatal: "
    MAX_DUMP_LINES = 64

    def __init__(self, stream, max_lines: int = STDERR_TAIL_LINES) -> None:
        self._lines: deque[str] = deque(maxlen=max_lines)
        self.fatal: str | None = None
        self._dump: list[str] = []
        self._thread = threading.Thread(target=self._drain, args=(stream,),
                                        daemon=True)
        self._thread.start()

    def _drain(self, stream) -> None:
        with stream:
            for raw in iter(stream.readline, b""):
                line = raw.decode("utf-8", "replace").rstrip("\r\n")
                self._lines.append(line)
                if self.fatal is None and line.startswith(self.FATAL):
                    self.fatal = line[len(self.FATAL):].strip()
                elif (self.fatal is not None
                      and len(self._dump) < self.MAX_DUMP_LINES):
                    self._dump.append(line)

    def text(self, wait_s: float = 5.0) -> str:
        self._thread.join(wait_s)
        return "\n".join(self._lines)

    def fatal_dump(self) -> str:
        return "\n".join(self._dump)


def _launch_and_pump(cmd: list[str], link: _UartLink, monitor: _Monitor,
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
    detail = None
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
        if status == STATUS_TIMEOUT and proc.poll() is None:
            dump = monitor.registers()
            where = cpu_location(dump) if dump else None
            if where:
                detail = f"timeout: {where}"
        # QEMU closes the UART just before it exits; give it a moment so
        # its own exit status is reported rather than our kill.
        grace = 1.0 if status == STATUS_QEMU_EXITED or followed else 0.0
        exited_by_itself = _stop(proc, grace)
    stderr_tail = tail.text()
    if exited_by_itself and proc.returncode == _WIN_DLL_NOT_FOUND:
        stderr_tail += ("\n[pp] QEMU could not load its DLLs; set "
                        "PP_MSYS2_BIN to the MSYS2 ucrt64/bin directory")
    if exited_by_itself and status == STATUS_QEMU_EXITED and \
            proc.returncode:
        detail = f"QEMU exit status {describe_returncode(proc.returncode)}"
    firmware_exit = pump.scanner.exit_code if status == STATUS_EXIT else None
    return _Outcome(
        status=status,
        firmware_exit=firmware_exit,
        transcript=bytes(pump.transcript),
        stderr_tail=stderr_tail.strip("\n"),
        returncode=proc.returncode if exited_by_itself else None,
        duration_s=time.monotonic() - start,
        detail=detail,
        fatal=tail.fatal,
        fatal_dump=tail.fatal_dump(),
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
