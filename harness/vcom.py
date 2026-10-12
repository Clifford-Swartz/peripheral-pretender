# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""EDBG virtual COM port: find it, open it with DTR asserted, capture a run.

The Curiosity Pro's EDBG bridges SERCOM4 (PB10 TX, PB11 RX) to a USB CDC port
and forwards data only while the host asserts DTR.  A capture ends at the
firmware's final "PP:EXIT <n>" line (docs/design/v0-vertical-slice.md 5.6).
The port's USB serial number equals the pyOCD probe unique id, so --probe
picks the COM port that belongs to a given board.

    py -m harness.vcom list
    py -m harness.vcom capture [--port COM5 | --probe ATML...] [--timeout 20]
"""
from __future__ import annotations

import argparse
import codecs
import json
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

# USB ids of Microchip on-board debuggers with a CDC virtual COM port.
DEBUGGER_IDS = {
    (0x03EB, 0x2111): "EDBG",
    (0x03EB, 0x2145): "mEDBG",
    (0x03EB, 0x2175): "nEDBG",
}
EXIT_TIMEOUT = 124      # same convention as `pp run`
_EXIT_LINE = re.compile(rb"(?:^|\n)PP:EXIT (\d+)(\r?\n)?")


class VcomError(Exception):
    pass


@dataclass(frozen=True)
class VcomPort:
    device: str             # "COM5"
    kind: str               # "EDBG"
    serial_number: str      # debugger USB serial = pyOCD probe unique id
    description: str


@dataclass
class Capture:
    data: bytes
    exit_code: int | None   # None when no complete PP:EXIT line arrived
    duration_s: float

    @property
    def text(self) -> str:
        return normalize(self.data)


def normalize(data: bytes) -> str:
    """Decode a raw UART stream and convert CRLF/CR line ends to LF."""
    text = data.decode("utf-8", errors="replace")
    return text.replace("\r\n", "\n").replace("\r", "\n")


def exit_code(data: bytes, idle: bool = False) -> int | None:
    """Return n from the first complete "PP:EXIT n" line, else None.

    A final line without a newline only counts once the line has gone idle,
    so "PP:EXIT 1" followed later by "2" is not mistaken for exit code 1.
    """
    for m in _EXIT_LINE.finditer(data):
        if m.group(2) or (idle and m.end() == len(data)):
            return int(m.group(1))
    return None


def list_all_ports() -> list[dict]:
    from serial.tools import list_ports

    ports = []
    for info in sorted(list_ports.comports(), key=lambda p: p.device):
        ports.append({"device": info.device,
                      "vid_pid": f"{info.vid:04X}:{info.pid:04X}" if info.vid else "",
                      "serial_number": info.serial_number or "",
                      "description": info.description or "",
                      "debugger": DEBUGGER_IDS.get((info.vid, info.pid), "")})
    return ports


def find_ports(serial_number: str | None = None) -> list[VcomPort]:
    """Debugger COM ports, optionally only the one of a given probe."""
    found = []
    for p in list_all_ports():
        if not p["debugger"]:
            continue
        if serial_number and p["serial_number"].upper() != serial_number.upper():
            continue
        found.append(VcomPort(p["device"], p["debugger"], p["serial_number"],
                              p["description"]))
    return found


def pick_port(port: str | None = None, probe: str | None = None) -> str:
    """Resolve --port/--probe to one COM port name, or explain why not."""
    if port:
        return port
    ports = find_ports(probe)
    if len(ports) == 1:
        return ports[0].device
    if not ports:
        which = f" for probe {probe}" if probe else ""
        raise VcomError(f"no EDBG virtual COM port found{which}; "
                        "is the board connected over its DEBUG USB port?")
    listing = ", ".join(f"{p.device} ({p.kind} {p.serial_number})" for p in ports)
    raise VcomError(f"several debugger COM ports: {listing}; pass --port or --probe")


def open_port(device: str, baudrate: int = 115200):
    """Open 8N1 with DTR asserted (EDBG drops data while DTR is low)."""
    import serial

    port = serial.Serial()
    port.port = device
    port.baudrate = baudrate
    port.timeout = 0.05
    port.dtr = True             # applied by open()
    try:
        port.open()
    except serial.SerialException as exc:
        raise VcomError(f"cannot open {device}: {exc}") from None
    if hasattr(port, "set_buffer_size"):        # Windows: room for output that
        port.set_buffer_size(rx_size=1 << 16)   # arrives while pyOCD disconnects
    port.reset_input_buffer()
    return port


def capture(port, timeout: float, until_exit: bool = True,
            echo: Callable[[bytes], None] | None = None,
            settle: float = 0.2) -> Capture:
    """Read until a PP:EXIT line (when until_exit) or until timeout seconds."""
    start = time.monotonic()
    last_rx = start
    buf = bytearray()
    while True:
        now = time.monotonic()
        if now - start >= timeout:
            return Capture(bytes(buf), None, now - start)
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            buf += chunk
            last_rx = now
            if echo:
                echo(chunk)
        if until_exit:
            code = exit_code(bytes(buf), idle=now - last_rx >= settle)
            if code is not None:
                return Capture(bytes(buf), code, time.monotonic() - start)


def console_echo() -> Callable[[bytes], None]:
    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")

    def echo(chunk: bytes) -> None:
        sys.stdout.write(decoder.decode(chunk).replace("\r\n", "\n"))
        sys.stdout.flush()

    return echo


def _cmd_list(args: argparse.Namespace) -> int:
    ports = list_all_ports()
    if args.json:
        print(json.dumps(ports, indent=2))
        return 0
    if not ports:
        print("no serial ports found")
    for p in ports:
        tag = f"[{p['debugger']} {p['serial_number']}]" if p["debugger"] else ""
        print(f"{p['device']:<8} {p['vid_pid']:<10} {p['description']} {tag}".rstrip())
    return 0


def _cmd_capture(args: argparse.Namespace) -> int:
    device = pick_port(args.port, args.probe)
    port = open_port(device, args.baud)
    try:
        echo = None if (args.json or args.quiet) else console_echo()
        result = capture(port, args.timeout, until_exit=not args.no_exit, echo=echo)
    finally:
        port.close()
    if args.out:
        Path(args.out).write_bytes(result.data)
    if args.json:
        print(json.dumps({"port": device, "exit_code": result.exit_code,
                          "duration_s": round(result.duration_s, 3),
                          "uart": result.text}, indent=2))
    if args.no_exit:
        return 0
    return EXIT_TIMEOUT if result.exit_code is None else result.exit_code


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="py -m harness.vcom", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list", help="list serial ports, marking debugger VCOM ports")
    p.add_argument("--json", action="store_true")
    p.set_defaults(func=_cmd_list)
    p = sub.add_parser("capture", help="capture the VCOM stream (read-only)")
    p.add_argument("--port", help="COM port, e.g. COM5 (default: the only EDBG port)")
    p.add_argument("--probe", help="debugger serial number / pyOCD probe unique id")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--timeout", type=float, default=20.0, help="seconds (default 20)")
    p.add_argument("--no-exit", action="store_true",
                   help="do not stop at PP:EXIT; capture for the whole timeout")
    p.add_argument("--out", help="write the raw bytes to this file")
    p.add_argument("--json", action="store_true", help="print a JSON summary")
    p.add_argument("--quiet", action="store_true", help="do not echo the stream")
    p.set_defaults(func=_cmd_capture)
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except VcomError as exc:
        print(f"vcom: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
