# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Command line for the twin runner: ``py -m tools.pp run|machines``.

``run`` streams the UART transcript to stdout and prints its verdict and
the model backlog to stderr; with ``--json`` stdout carries one JSON object
(the same fields as the MCP tool ``twin_run``) and nothing else. The exit
status is the firmware's ``PP:EXIT`` code, 124 on timeout, 125 when QEMU
ended without an exit line (including a CPU lockup), 2 for usage errors and
127 when QEMU is missing.
"""

from __future__ import annotations

import argparse
import codecs
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Sequence, TextIO

from .qemu import DEFAULT_MACHINE, QemuNotFound, machines_info
from .runner import (STATUS_CPU_LOCKUP, STATUS_EXIT, STATUS_TIMEOUT,
                     RunnerError, RunOptions, RunResult, describe_returncode,
                     run)

EXIT_USAGE = 2
EXIT_QEMU_MISSING = 127
DEFAULT_TIMEOUT_S = 10.0
DEFAULT_GDB_PORT = 1234
MAX_TRACE_LINES_SHOWN = 200

_ESCAPE = re.compile(rb"\\(x[0-9a-fA-F]{2}|.)", re.DOTALL)
_SIMPLE_ESCAPES = {b"n": b"\n", b"r": b"\r", b"t": b"\t", b"0": b"\0",
                   b"\\": b"\\"}


def unescape(text: str) -> bytes:
    """Bytes for an --input value: \\n \\r \\t \\0 \\\\ and \\xNN are
    expanded, anything else is kept as written."""
    def expand(m: re.Match[bytes]) -> bytes:
        esc = m.group(1)
        if esc[:1] == b"x" and len(esc) == 3:
            return bytes([int(esc[1:], 16)])
        return _SIMPLE_ESCAPES.get(esc, m.group(0))
    return _ESCAPE.sub(expand, text.encode("utf-8"))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="py -m tools.pp",
        description="Peripheral Pretender twin runner.")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser(
        "run", help="run a firmware ELF on the twin",
        description="Run an ELF until it prints 'PP:EXIT <n>' and exit "
                    "with n (124 on timeout, 125 if QEMU ends first).")
    p.add_argument("elf", help="firmware image (ELF)")
    p.add_argument("-M", "--machine", default=DEFAULT_MACHINE,
                   help=f"QEMU machine (default {DEFAULT_MACHINE})")
    p.add_argument("--qemu", metavar="PATH",
                   help="qemu-system-arm binary (default $PP_QEMU, then "
                        "../pp-qemu/build, then PATH)")
    p.add_argument("--timeout", type=float, metavar="S",
                   help=f"host seconds before giving up (default "
                        f"{DEFAULT_TIMEOUT_S:g}; none with --gdb)")
    p.add_argument("--icount", type=int, metavar="SHIFT",
                   help="deterministic virtual time: -icount "
                        "shift=SHIFT,sleep=off (0-10)")
    p.add_argument("--gdb", type=int, nargs="?", const=DEFAULT_GDB_PORT,
                   metavar="PORT",
                   help="start paused with the gdbstub on 127.0.0.1:PORT "
                        f"(default {DEFAULT_GDB_PORT})")
    p.add_argument("--trace", action="append", default=[], metavar="PATTERN",
                   help="enable QEMU trace events matching PATTERN "
                        "(repeatable)")
    p.add_argument("--input", metavar="TEXT",
                   help="send TEXT to the UART; \\n \\r \\t \\0 \\\\ \\xNN "
                        "are expanded")
    p.add_argument("--input-after", metavar="TEXT",
                   help="send --input once TEXT appears on the UART "
                        "(default: after the first output)")
    p.add_argument("--log", metavar="FILE",
                   help="keep QEMU's -d log in FILE")
    p.add_argument("--json", action="store_true",
                   help="print one JSON object instead of the live "
                        "transcript and summary")
    p.add_argument("--extra-args", nargs=argparse.REMAINDER, default=[],
                   metavar="ARG",
                   help="all remaining arguments go to QEMU (-d is merged "
                        "with the runner's, -D replaces --log)")

    m = sub.add_parser("machines",
                       help="list the machines of the QEMU binary")
    m.add_argument("--qemu", metavar="PATH", help="qemu-system-arm binary")
    m.add_argument("--json", action="store_true", help="print JSON")
    return parser


def options_from_args(args: argparse.Namespace) -> RunOptions:
    timeout = args.timeout
    if timeout is None and args.gdb is None:
        timeout = DEFAULT_TIMEOUT_S
    return RunOptions(
        elf=args.elf,
        machine=args.machine,
        qemu=args.qemu,
        timeout_s=timeout,
        icount_shift=args.icount,
        gdb_port=args.gdb,
        trace=args.trace,
        input_text=None if args.input is None else unescape(args.input),
        input_after=args.input_after,
        extra_args=args.extra_args,
        log_path=args.log,
    )


class LiveEcho:
    """Writes UART bytes to a text stream as they arrive, CRLF -> LF."""

    def __init__(self, stream: TextIO) -> None:
        self._stream = stream
        self._decoder = codecs.getincrementaldecoder("utf-8")(
            "backslashreplace")
        self._held_cr = False

    def __call__(self, data: bytes) -> None:
        self._write(self._decoder.decode(data))

    def close(self) -> None:
        self._write(self._decoder.decode(b"", final=True), final=True)

    def _write(self, text: str, final: bool = False) -> None:
        if self._held_cr:
            text = "\r" + text
        self._held_cr = text.endswith("\r") and not final
        if self._held_cr:
            text = text[:-1]
        if text:
            self._stream.write(text.replace("\r\n", "\n"))
            self._stream.flush()


def verdict(result: RunResult) -> str:
    if result.status == STATUS_EXIT:
        what = f"PP:EXIT {result.firmware_exit}"
    elif result.status == STATUS_TIMEOUT:
        what = "timeout, no PP:EXIT"
    elif result.status == STATUS_CPU_LOCKUP:
        what = "CPU lockup, QEMU aborted before PP:EXIT"
    else:
        code = result.qemu_returncode
        what = "QEMU exited" + ("" if code is None else
                                f" with status {describe_returncode(code)}")
        what += " before PP:EXIT"
    line = (f"[pp] exit {result.exit_code} ({what}) after "
            f"{result.duration_s:.2f} s on {result.machine}")
    if result.detail and result.status != STATUS_EXIT:
        line += f"\n[pp] {result.detail}"
    return line


def format_summary(summary: dict) -> list[str]:
    counts = summary["counts"]
    if not summary["total"]:
        return ["[pp] model backlog: no unimplemented or guest-error accesses"]
    parts = ", ".join(f"{k} {v}" for k, v in counts.items() if v)
    lines = [f"[pp] model backlog: {summary['total']} log lines ({parts})"]
    rows = [(str(g["count"]), g["kind"], g["instance"], g["register"],
             _rw(g)) for g in summary["groups"]]
    header = ("count", "kind", "instance", "register", "r/w")
    widths = [max(len(r[i]) for r in rows + [header]) for i in range(5)]
    for row in [header] + rows:
        cells = [row[0].rjust(widths[0])]
        cells += [c.ljust(w) for c, w in zip(row[1:], widths[1:])]
        lines.append("    " + "  ".join(cells).rstrip())
    return lines


def _rw(group: dict) -> str:
    parts = []
    if group["reads"]:
        parts.append(f"{group['reads']}r")
    if group["writes"]:
        parts.append(f"{group['writes']}w")
    text = " ".join(parts) or "-"
    # QEMU stopped logging writes to the register: the count is a minimum
    return text + "+" if group.get("truncated") else text


def report(result: RunResult, err: TextIO) -> None:
    for warning in result.warnings:
        print(f"[pp] warning: {warning}", file=err)
    print(verdict(result), file=err)
    for line in format_summary(result.unimplemented_summary):
        print(line, file=err)
    if result.trace is not None:
        _report_trace(result.trace, err)
    if result.status not in (STATUS_EXIT, STATUS_TIMEOUT) and \
            result.qemu_stderr_tail:
        print("[pp] QEMU output:", file=err)
        for line in result.qemu_stderr_tail.splitlines():
            print("    " + line, file=err)
    if result.log_path and Path(result.log_path).is_file():
        print(f"[pp] QEMU log kept at {result.log_path}", file=err)


def _report_trace(trace: dict, err: TextIO) -> None:
    events = ", ".join(f"{k} {v}" for k, v in trace["events"].items())
    print(f"[pp] trace: {trace['count']} events" +
          (f" ({events})" if events else ""), file=err)
    shown = trace["lines"][:MAX_TRACE_LINES_SHOWN]
    for line in shown:
        print("    " + line, file=err)
    hidden = trace["count"] - len(shown)
    if hidden > 0:
        print(f"    ... {hidden} more (use --log FILE for all)", file=err)


def shell_quote(arg: str) -> str:
    """Quote arg for the shell a user pastes the hint into (cmd/PowerShell
    on Windows, a POSIX shell elsewhere)."""
    if sys.platform == "win32":
        return subprocess.list2cmdline([arg])
    return shlex.quote(arg)


def cmd_run(args: argparse.Namespace) -> int:
    options = options_from_args(args)
    echo = None
    if not args.json:
        echo = options.on_uart = LiveEcho(sys.stdout)
        if args.gdb is not None:
            print(f"[pp] QEMU is paused; attach with: gdb-multiarch "
                  f"{shell_quote(args.elf)} -ex "
                  f"{shell_quote(f'target remote 127.0.0.1:{args.gdb}')}",
                  file=sys.stderr)
    result = run(options)
    if args.json:
        json.dump(result.to_dict(), sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        echo.close()
        if result.uart and not result.uart.endswith("\n"):
            sys.stdout.write("\n")
        sys.stdout.flush()
        report(result, sys.stderr)
    return result.exit_code


def cmd_machines(args: argparse.Namespace) -> int:
    info = machines_info(args.qemu)
    if args.json:
        json.dump(info, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0
    print(f"# {info['qemu']} ({info['qemu_version']})")
    width = max((len(m["name"]) for m in info["machines"]), default=0)
    for machine in info["machines"]:
        print(f"{machine['name'].ljust(width)}  {machine['description']}")
    if not info["default_machine_available"]:
        print(f"# default machine {info['default_machine']} is not in this "
              f"build", file=sys.stderr)
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="backslashreplace")
    args = build_parser().parse_args(argv)
    handler = cmd_run if args.command == "run" else cmd_machines
    try:
        return handler(args)
    except QemuNotFound as exc:
        print(f"pp: {exc}", file=sys.stderr)
        return EXIT_QEMU_MISSING
    except RunnerError as exc:
        print(f"pp: {exc}", file=sys.stderr)
        return EXIT_USAGE
    except KeyboardInterrupt:
        return 130
    except BrokenPipeError:
        # The reader of stdout went away (e.g. "| head"); silence the
        # flush at interpreter exit.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        return 1
