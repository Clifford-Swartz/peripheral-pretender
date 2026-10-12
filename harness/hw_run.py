# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Run an ELF on the Curiosity Pro: program, reset, capture VCOM until PP:EXIT.

The silicon counterpart of `py -m tools.pp run`: same exit protocol and exit
codes (n from "PP:EXIT n", 124 on timeout) and a comparable transcript.

Nothing touches the board without an explicit flag:
  --dry-run        print the plan only
  --check          read-only pre-flight (pyOCD attach, no halt): DID, CPUID,
                   security bit, BOOTPROT and region locks against the image
  --allow-program  program main flash (sector erase only), reset, capture
  --allow-reset    with --programmer none: reset through the probe

  py -m harness.hw_run firmware/hello/build/hello.elf --check
  py -m harness.hw_run firmware/hello/build/hello.elf --allow-program --out silicon.txt
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from harness import silicon, vcom

PART_IPE = "32CM5164JH01100"            # MPLAB IPE part name (Zephyr board.cmake)
MSYS2_BIN = Path("C:/msys64/ucrt64/bin")


class HwRunError(Exception):
    pass


@dataclass(frozen=True)
class Image:
    path: Path
    sha256: str
    entry: int
    segments: tuple[tuple[int, int], ...]   # [start, end) load addresses with file data

    def describe(self) -> str:
        spans = ", ".join(f"0x{s:08X}-0x{e - 1:08X}" for s, e in self.segments)
        return f"{self.path} (sha256 {self.sha256[:16]}..., entry 0x{self.entry:08X}; {spans})"


def read_image(path: str | Path) -> Image:
    """Load addresses (LMA) of every PT_LOAD segment that carries file data."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4:6] != b"\x01\x01":
        raise HwRunError(f"{path}: not a 32-bit little-endian ELF")
    (machine,) = struct.unpack_from("<H", data, 18)
    if machine != 40:
        raise HwRunError(f"{path}: not an Arm ELF (e_machine {machine})")
    entry, phoff = struct.unpack_from("<II", data, 24)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    segments = []
    for i in range(phnum):
        p_type, _, _, paddr, filesz = struct.unpack_from("<5I", data, phoff + i * phentsize)
        if p_type == 1 and filesz:              # PT_LOAD
            segments.append((paddr, paddr + filesz))
    if not segments:
        raise HwRunError(f"{path}: no loadable segments")
    return Image(Path(path), hashlib.sha256(data).hexdigest(), entry, tuple(sorted(segments)))


def outside_main_flash(image: Image) -> list[str]:
    lo, hi = silicon.FLASH
    return [f"0x{s:08X}-0x{e - 1:08X}" for s, e in image.segments if s < lo or e > hi]


def find_tool(explicit: str | None, name: str) -> str | None:
    if explicit:
        return explicit
    found = shutil.which(name)
    if found:
        return found
    local = MSYS2_BIN / f"{name}.exe"
    return str(local) if local.exists() else None


def ipecmd_command(ipecmd: str, hex_path: str, tool: str) -> list[str]:
    """MPLAB IPE command line: program memory only, never erase.

    Zephyr's mplab_ipe runner passes -M (all memories) and, with --erase,
    -E; the harness narrows -M to -MP so configuration memory (the user row
    with the factory BODCORE calibration) is never rewritten.  -OL releases
    the part from reset afterwards.  Unverified: IPE is not installed here.
    """
    return [ipecmd, f"-P{PART_IPE}", f"-TP{tool}", f"-F{hex_path}", "-MP", "-OL"]


def plan(args: argparse.Namespace, image: Image) -> list[str]:
    who = args.port or f"the EDBG port of probe {args.probe or '(the only probe)'}"
    steps = [f"image    {image.describe()}",
             f"vcom     open {who} at {args.baud} 8N1, DTR asserted, flush"]
    if args.programmer == "pyocd":
        steps += [f"connect  pyOCD target {silicon.TARGET}, pack {args.pack or silicon.DFP_DIR}, "
                  f"{args.frequency} Hz, halt on connect",
                  "check    security bit, BOOTPROT, region locks vs the image (abort on conflict)",
                  "program  main flash with the DFP flash algorithm: sector erase, smart "
                  "flash; never chip erase, never the user row",
                  "flush    VCOM input",
                  f"reset    {args.reset} reset, then disconnect (core keeps running)"]
    elif args.programmer == "ipecmd":
        cmd = ipecmd_command(args.ipecmd or "ipecmd.exe", "<image>.hex", args.ipe_tool)
        steps += [f"convert  arm-none-eabi-objcopy -O ihex {image.path} <image>.hex",
                  f"program  {' '.join(cmd)}  (releases reset when done)"]
    elif args.reset == "button":
        steps.append("reset    wait for the user to press RESET on the board")
    else:
        steps.append(f"reset    {args.reset} reset through pyOCD (attach, no halt)")
    steps.append(f"capture  until 'PP:EXIT <n>' or {args.timeout:g} s "
                 "(exit code n; 124 on timeout)")
    return steps


def check(args: argparse.Namespace, image: Image) -> int:
    """Read-only pre-flight; exit 0 if the image can be programmed as is."""
    with silicon.session(args.probe, args.pack, frequency=args.frequency) as sess:
        info = silicon.preflight(sess)
    print("\n".join(info.describe()))
    if not info.protection:
        print("cannot judge programmability: user row or STATUSB unreadable")
        return 3
    problems = info.protection.conflicts(list(image.segments))
    for problem in problems:
        print(f"conflict     {problem}")
    print("programmable as is" if not problems else "NOT programmable as is")
    return 3 if problems else 0


def program_pyocd(args: argparse.Namespace, image: Image, port) -> dict:
    from pyocd.core.target import Target
    from pyocd.flash.file_programmer import FileProgrammer

    resets = {"system": Target.ResetType.SYSTEM, "hardware": Target.ResetType.HARDWARE}
    with silicon.session(args.probe, args.pack, frequency=args.frequency,
                         allow_halt=True) as sess:
        info = silicon.preflight(sess)
        problems = (info.protection.conflicts(list(image.segments)) if info.protection
                    else [f"user row unreadable: {'; '.join(info.errors)}"])
        if problems:
            raise HwRunError("refusing to program:\n  " + "\n  ".join(problems))
        FileProgrammer(sess, chip_erase="sector", smart_flash=True).program(
            str(image.path), file_format="elf")
        port.reset_input_buffer()
        sess.target.reset(resets[args.reset])
    return {"probe": info.probe, "did": f"0x{info.did:08X}" if info.did is not None else None}


def program_ipecmd(args: argparse.Namespace, image: Image) -> dict:
    ipecmd = find_tool(args.ipecmd, "ipecmd")
    objcopy = find_tool(args.objcopy, "arm-none-eabi-objcopy")
    if not ipecmd or not objcopy:
        raise HwRunError("--programmer ipecmd needs ipecmd (MPLAB IPE) and "
                         "arm-none-eabi-objcopy; pass --ipecmd/--objcopy")
    with tempfile.TemporaryDirectory() as tmp:
        hex_path = str(Path(tmp) / (image.path.stem + ".hex"))
        subprocess.run([objcopy, "-O", "ihex", str(image.path), hex_path], check=True)
        subprocess.run(ipecmd_command(ipecmd, hex_path, args.ipe_tool), check=True)
    return {"probe": args.probe}


def reset_only(args: argparse.Namespace) -> dict:
    if args.reset == "button":
        print("press RESET on the board now", file=sys.stderr)
        return {"probe": args.probe}
    from pyocd.core.target import Target

    resets = {"system": Target.ResetType.SYSTEM, "hardware": Target.ResetType.HARDWARE}
    with silicon.session(args.probe, args.pack, frequency=args.frequency) as sess:
        sess.target.reset(resets[args.reset])
        return {"probe": sess.probe.unique_id}


def approval_missing(args: argparse.Namespace) -> str:
    if args.programmer != "none" and not args.allow_program:
        return ("programming changes the board's flash: rerun with --allow-program "
                "once the user approved flashing this image")
    if args.programmer == "none" and args.reset != "button" and not args.allow_reset:
        return "a probe reset needs --allow-reset (or use --reset button)"
    return ""


def run(args: argparse.Namespace, image: Image) -> int:
    device = vcom.pick_port(args.port, args.probe)
    port = vcom.open_port(device, args.baud)
    try:
        if args.programmer == "pyocd":
            meta = program_pyocd(args, image, port)
        elif args.programmer == "ipecmd":
            meta = program_ipecmd(args, image)
        else:
            meta = reset_only(args)
        echo = None if args.json else vcom.console_echo()
        result = vcom.capture(port, args.timeout, echo=echo)
    finally:
        port.close()
    if args.out:
        Path(args.out).write_bytes(result.data)
    if args.json:
        print(json.dumps({"target": "silicon", "exit_code": result.exit_code,
                          "uart": result.text,
                          "duration_s": round(result.duration_s, 3),
                          "port": device, "programmer": args.programmer,
                          "elf": str(image.path), "elf_sha256": image.sha256,
                          **meta}, indent=2))
    return vcom.EXIT_TIMEOUT if result.exit_code is None else result.exit_code


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="py -m harness.hw_run", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("elf", help="firmware ELF linked for main flash")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="print the plan only")
    mode.add_argument("--check", action="store_true", help="read-only pre-flight")
    parser.add_argument("--allow-program", action="store_true",
                        help="user approved programming this image")
    parser.add_argument("--allow-reset", action="store_true",
                        help="user approved resetting the board (--programmer none)")
    parser.add_argument("--programmer", choices=("pyocd", "ipecmd", "none"), default="pyocd",
                        help="none: already programmed (MCC MCP mplab_program_device, "
                             "MPLAB X/IPE); only reset and capture")
    parser.add_argument("--reset", choices=("system", "hardware", "button"), default="system")
    parser.add_argument("--probe", help="probe unique id (EDBG serial number)")
    parser.add_argument("--port", help="VCOM port (default: the probe's EDBG port)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--pack", help=f"DFP directory or .pack (default {silicon.DFP_DIR})")
    parser.add_argument("--frequency", type=int, default=silicon.FREQUENCY, help="SWD Hz")
    parser.add_argument("--timeout", type=float, default=20.0, help="capture seconds")
    parser.add_argument("--out", help="write the raw VCOM bytes here")
    parser.add_argument("--json", action="store_true", help="print a JSON summary")
    parser.add_argument("--ipecmd", help="path to ipecmd.exe")
    parser.add_argument("--ipe-tool", default="EDBG", help="IPE -TP tool name")
    parser.add_argument("--objcopy", help="path to arm-none-eabi-objcopy")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        image = read_image(args.elf)
        bad = outside_main_flash(image)
        if bad:
            raise HwRunError(f"image writes outside main flash ({', '.join(bad)}); the "
                             "harness never programs the user row, fuses or data flash")
        if args.dry_run:
            print("\n".join(plan(args, image)))
            return 0
        if args.check:
            return check(args, image)
        missing = approval_missing(args)
        if missing:
            print("\n".join(plan(args, image)))
            print(f"hw_run: refusing: {missing}", file=sys.stderr)
            return 2
        return run(args, image)
    except (HwRunError, vcom.VcomError, silicon.SiliconError) as exc:
        print(f"hw_run: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
