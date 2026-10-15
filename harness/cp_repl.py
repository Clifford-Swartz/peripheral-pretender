# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Read-only REPL probe of a CircuitPython board: the silicon side of a
CircuitPython twin check.

CircuitPython kits have no debugger to program or halt; their console is
the USB CDC port, and the board's CIRCUITPY drive holds someone's code.
This tool only talks to the REPL: Ctrl-C stops code.py, Enter opens the
REPL, each line is sent and its output captured, and then, always (even
after an error or Ctrl-C here), Ctrl-D soft-reboots the board so that its
code.py runs again. It never writes files, never touches the drive, never
reflashes. Lines that write files (open(..., "w"), storage.remount) are
refused.

The twin side is `py -m tools.pp.circuitpython run <drive> --lines ...`:
send the same lines to both and compare.

  py -m harness.cp_repl --port COM9 "import board" "print(dir(board))"
  py -m harness.cp_repl --list            # CircuitPython ports (VID 239A/04D8)
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from typing import Callable

# Lines that could change the board: refused
WRITES = re.compile(r"""open\s*\(.*['"][wax+]|storage\s*\.\s*(remount|erase)|"""
                    r"""os\s*\.\s*(remove|rename|rmdir|mkdir|unlink)|"""
                    r"""microcontroller\s*\.\s*(reset|on_next_reset)|"""
                    r"""nvm\s*\[|supervisor\s*\.\s*(reload|set_next_code_file)""")
CIRCUITPY_VIDS = {0x239A, 0x04D8}


def refused(line: str) -> bool:
    return bool(WRITES.search(line))


def probe(ser, lines: list[str], out: Callable[[str], None],
          line_wait: float = 5.0) -> None:
    """Run @lines in the REPL of the board on @ser (a pyserial-like object
    with read/write/reset_input_buffer); Ctrl-D at the end, always."""
    def read_until(token: bytes | None, timeout: float) -> str:
        buf = b""
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            d = ser.read(4096)
            if d:
                buf += d
                if token and token in buf:
                    break
        return buf.decode("utf-8", "replace")

    try:
        ser.reset_input_buffer()
        for _ in range(2):
            ser.write(b"\x03")
            time.sleep(0.3)
        text = read_until(b"Press any key", 4)
        ser.write(b"\r")
        text += read_until(b">>> ", 4)
        out(text)
        if ">>> " not in text:
            ser.write(b"\x03\r")
            out(read_until(b">>> ", 4))
        for line in lines:
            if refused(line):
                out(f"\n[cp_repl] refused (could write to the board): {line}\n")
                continue
            ser.write(line.encode() + b"\r")
            out(read_until(b">>> ", line_wait))
    finally:
        ser.write(b"\x04")          # soft reboot: the board's code.py runs again
        out(read_until(b"soft reboot", 3))
        out("\n[cp_repl] sent Ctrl-D\n")


def circuitpython_ports() -> list[str]:
    from serial.tools import list_ports
    return [p.device for p in list_ports.comports() if p.vid in CIRCUITPY_VIDS]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", help="the board's USB serial port (COM9)")
    ap.add_argument("--list", action="store_true",
                    help="list serial ports with a CircuitPython VID")
    ap.add_argument("--line-wait", type=float, default=5.0)
    ap.add_argument("--log", help="also write the transcript here")
    ap.add_argument("lines", nargs="*")
    args = ap.parse_args(argv)
    if args.list:
        print("\n".join(circuitpython_ports()) or "none")
        return 0
    if not args.port:
        ap.error("--port is required (see --list)")
    import serial

    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    log = open(args.log, "w", encoding="utf-8") if args.log else None

    def out(text: str) -> None:
        sys.stdout.write(text)
        if log:
            log.write(text)

    with serial.Serial(args.port, 115200, timeout=0.1) as ser:
        try:
            probe(ser, args.lines, out, args.line_wait)
        finally:
            if log:
                log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
