# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Run a hex file on a Curiosity Nano: drag-and-drop program, capture VCOM.

The silicon counterpart of `py -m tools.pp run` for kits whose nEDBG offers
drag-and-drop programming (the kit's USB drive, label CURIOSITY): the image
is copied onto the drive, the debugger programs and resets the target, and
the virtual COM port is captured until "PP:EXIT n". Same exit codes as the
twin runner (n, 124 on timeout). Used for 8-bit kits (AVR64EA48 Curiosity
Nano), where pyOCD does not apply.

Nothing touches the board without --allow-program. The image is refused
unless every byte lies in the program flash: fuses, lock bits and the
signature rows (avr-gcc's 0x820000+ sections) are never written, so the
device cannot be locked from here.

  py -m harness.nano_run firmware/avr-hello/build/hello.hex --check
  py -m harness.nano_run firmware/avr-hello/build/hello.hex --allow-program --out silicon.bin
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import string
import sys
from dataclasses import dataclass
from pathlib import Path

from harness import vcom

LABEL = "CURIOSITY"
EXIT_TIMEOUT = vcom.EXIT_TIMEOUT


class NanoRunError(Exception):
    pass


@dataclass(frozen=True)
class Kit:
    drive: Path                 # root of the kit's USB drive
    name: str                   # "AVR64EA48 Curiosity Nano"
    device: str                 # "AVR64EA48"
    serial_number: str          # USB serial of the nEDBG


def parse_kit_info(text: str) -> dict[str, str]:
    """KIT-INFO.TXT: "Key:   value" lines."""
    out = {}
    for line in text.splitlines():
        key, sep, value = line.partition(":")
        if sep:
            out[key.strip()] = value.strip()
    return out


def _drives() -> list[Path]:
    if os.name == "nt":
        return [Path(f"{d}:/") for d in string.ascii_uppercase
                if Path(f"{d}:/KIT-INFO.TXT").is_file()]
    roots = [Path("/media"), Path("/run/media"), Path("/Volumes")]
    return [p for r in roots if r.is_dir() for p in r.rglob("KIT-INFO.TXT")
            if p.parent.name == LABEL]


def find_kits() -> list[Kit]:
    kits = []
    for root in _drives():
        root = root if root.is_dir() else root.parent
        info = parse_kit_info((root / "KIT-INFO.TXT").read_text(
            encoding="ascii", errors="replace"))
        if "Device" in info:
            kits.append(Kit(root, info.get("Kit name", ""), info["Device"],
                            info.get("Kit USB serial number", "")))
    return kits


def pick_kit(drive: str | None, device: str | None) -> Kit:
    kits = find_kits()
    if drive:
        kits = [k for k in kits
                if str(k.drive).upper().startswith(drive.upper().rstrip("/\\"))]
    if device:
        kits = [k for k in kits if k.device.upper() == device.upper()]
    if len(kits) != 1:
        found = ", ".join(f"{k.drive} ({k.device}, {k.serial_number})"
                          for k in find_kits()) or "none"
        raise NanoRunError(f"need exactly one matching Curiosity kit drive; "
                           f"found: {found}")
    return kits[0]


def kit_port(kit: Kit, port: str | None) -> str:
    """The virtual COM port of the kit's debugger (same USB serial)."""
    if port:
        return port
    ports = vcom.find_ports(kit.serial_number)
    if len(ports) != 1:
        raise NanoRunError(f"no single virtual COM port for {kit.serial_number}"
                           f" ({[p.device for p in ports]}); pass --port")
    return ports[0].device


def hex_ranges(text: str) -> list[tuple[int, int]]:
    """[start, end) address ranges of the data records of an Intel hex."""
    base, ranges = 0, []
    for n, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise NanoRunError(f"line {n}: not Intel hex")
        raw = bytes.fromhex(line[1:])
        if sum(raw) & 0xFF:
            raise NanoRunError(f"line {n}: checksum error")
        length, addr, kind = raw[0], int.from_bytes(raw[1:3], "big"), raw[3]
        if kind == 0x00:
            ranges.append((base + addr, base + addr + length))
        elif kind == 0x02:
            base = int.from_bytes(raw[4:6], "big") << 4
        elif kind == 0x04:
            base = int.from_bytes(raw[4:6], "big") << 16
        elif kind == 0x01:
            break
    return ranges


def check_image(path: Path, flash_size: int) -> list[tuple[int, int]]:
    """Refuse anything outside the program flash (fuses, lock bits)."""
    if path.suffix.lower() != ".hex":
        raise NanoRunError(f"{path}: drag-and-drop takes Intel hex (.hex)")
    ranges = hex_ranges(path.read_text(encoding="ascii"))
    if not ranges:
        raise NanoRunError(f"{path}: no data records")
    bad = [r for r in ranges if r[1] > flash_size]
    if bad:
        raise NanoRunError(f"{path}: data outside the {flash_size // 1024} KB "
                           f"flash at 0x{bad[0][0]:X} (fuses or lock bits?); "
                           "refused")
    return ranges


# Program flash per device (ATDF prog address space PROGMEM)
FLASH_SIZE = {"AVR64EA48": 0x10000}


def run(args: argparse.Namespace) -> int:
    kit = pick_kit(args.drive, args.device)
    flash = FLASH_SIZE.get(kit.device.upper())
    if not flash:
        raise NanoRunError(f"{kit.device}: flash size unknown; add it to "
                           "FLASH_SIZE")
    image = Path(args.hex)
    ranges = check_image(image, flash)
    port_name = kit_port(kit, args.port)
    plan = {"kit": kit.name, "device": kit.device, "drive": str(kit.drive),
            "serial_number": kit.serial_number, "port": port_name,
            "image": str(image), "bytes": sum(e - s for s, e in ranges)}
    if args.check or not args.allow_program:
        print(json.dumps(plan, indent=1))
        if not args.check:
            print("nano_run: pass --allow-program to program the kit",
                  file=sys.stderr)
        return 0
    port = vcom.open_port(port_name, args.baud)
    try:
        shutil.copyfile(image, kit.drive / image.name)
        cap = vcom.capture(port, args.timeout,
                           echo=None if args.quiet else vcom.console_echo())
    finally:
        port.close()
    status = (kit.drive / "STATUS.TXT")
    if args.out:
        Path(args.out).write_bytes(cap.data)
    if status.is_file():
        plan["status"] = status.read_text(errors="replace").splitlines()[0]
    print(f"\n[nano] {plan}", file=sys.stderr)
    return cap.exit_code if cap.exit_code is not None else EXIT_TIMEOUT


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="py -m harness.nano_run",
                                 description=__doc__.splitlines()[0])
    ap.add_argument("hex", help="Intel hex image (program flash only)")
    ap.add_argument("--drive", help="kit drive (default: the one CURIOSITY "
                                    "drive)")
    ap.add_argument("--device", help="refuse kits with another device, e.g. "
                                     "AVR64EA48")
    ap.add_argument("--port", help="virtual COM port (default: the kit's)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=20.0)
    ap.add_argument("--out", help="write the raw VCOM bytes here")
    ap.add_argument("--check", action="store_true",
                    help="print the plan only (read-only)")
    ap.add_argument("--allow-program", action="store_true",
                    help="copy the image onto the kit drive (programs it)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    try:
        return run(args)
    except (NanoRunError, vcom.VcomError, OSError) as exc:
        print(f"nano_run: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
