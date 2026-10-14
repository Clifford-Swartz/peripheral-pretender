# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Run an AVR ELF in the MPLAB simulator (MDB, Hwtool SIM) and recover the
characters it writes to a USART.

The MPLAB X v6.20 simulator runs AVR64EA48 code but its USARTs are "beta":
UART I/O to a file or the output window (uartNio.*) produces nothing for
AVR8X parts. So this tool breaks on every instruction that stores to the
USART's TXDATAL (found by disassembling the ELF with avr-objdump), prints
the stored register at each stop and continues; the stores, in order, are
the transcript. Simulator warnings (W0035 unimplemented memory, W0102
missing peripherals) are reported alongside.

  py -m harness.mdb_sim firmware/avr-hello/build/hello.elf --device AVR64EA48 --txdata 0x0822 --out sim.bin
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

MPLABX = Path("C:/Program Files/Microchip/MPLABX/v6.20")
XC8 = Path("C:/Program Files/Microchip/xc8/v2.50")

_STS = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*sts\s+0x([0-9a-f]+), "
                  r"r(\d+)", re.IGNORECASE)
# "Stop at" is followed by "\taddress:0xa0" (a word address) on its own line
_STOP = re.compile(r"^\s*address:0x([0-9a-f]+)\s*$", re.IGNORECASE)
_REG = re.compile(r"^r(\d+)=(\d+)\s*$")


class MdbError(Exception):
    pass


@dataclass
class SimResult:
    data: bytes
    stops: int
    warnings: list[str] = field(default_factory=list)
    log: str = ""


def store_sites(elf: Path, objdump: Path, address: int) -> dict[int, int]:
    """Word address of each "sts <address>, rN" -> N."""
    out = subprocess.run([str(objdump), "-d", str(elf)], capture_output=True,
                         text=True, check=True).stdout
    sites = {}
    for line in out.splitlines():
        m = _STS.match(line)
        if m and int(m.group(2), 16) == address:
            sites[int(m.group(1), 16) // 2] = int(m.group(3))
    return sites


def command_file(device: str, elf: Path, sites: dict[int, int],
                 max_chars: int, wait_ms: int) -> str:
    regs = sorted(set(sites.values()))
    lines = [f"device {device}", "hwtool SIM", f"program {elf.as_posix()}"]
    lines += [f"break *0x{addr:x}" for addr in sorted(sites)]
    lines.append("run")
    for i in range(max_chars):
        lines.append(f"wait {wait_ms}")
        lines += [f"print r{r}" for r in regs]
        lines.append("continue")
    lines += ["halt", "quit"]
    return "\n".join(lines) + "\n"


def parse(log: str, sites: dict[int, int]) -> SimResult:
    data = bytearray()
    stops = 0
    pc = None
    regs: dict[int, int] = {}
    warnings = sorted(set(re.findall(r"^(W\d{4}-\w+: .*)$", log, re.M)))

    def flush():
        if pc in sites and sites[pc] in regs:
            data.append(regs[sites[pc]] & 0xFF)

    for line in log.splitlines():
        m = _STOP.search(line)
        if m:
            flush()
            pc, regs = int(m.group(1), 16), {}
            stops += 1
            continue
        m = _REG.match(line.strip())
        if m:
            # The first print after a stop; later ones (waits that did not
            # stop again because the program ended) are stale
            regs.setdefault(int(m.group(1)), int(m.group(2)))
    flush()
    return SimResult(bytes(data), stops, warnings, log)


def run(elf: Path, device: str, txdata: int, max_chars: int = 400,
        wait_ms: int = 5000, mplabx: Path = MPLABX,
        objdump: Path | None = None) -> SimResult:
    objdump = objdump or XC8 / "avr" / "bin" / "avr-objdump.exe"
    mdb = mplabx / "mplab_platform" / "bin" / "mdb.bat"
    if not mdb.is_file():
        raise MdbError(f"MDB not found: {mdb}")
    sites = store_sites(elf, objdump, txdata)
    if not sites:
        raise MdbError(f"no 'sts 0x{txdata:04x}, rN' in {elf}")
    with tempfile.TemporaryDirectory(prefix="pp-mdb-") as tmp:
        cmd = Path(tmp) / "cmd.txt"
        cmd.write_text(command_file(device, elf.resolve(), sites, max_chars,
                                    wait_ms))
        proc = subprocess.run(["cmd.exe", "/c", str(mdb), str(cmd)],
                              capture_output=True, text=True,
                              errors="replace", timeout=1800)
    return parse(proc.stdout + proc.stderr, sites)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="py -m harness.mdb_sim",
                                 description=__doc__.splitlines()[0])
    ap.add_argument("elf")
    ap.add_argument("--device", required=True, help="e.g. AVR64EA48")
    ap.add_argument("--txdata", type=lambda s: int(s, 0), required=True,
                    help="data address of the USART TXDATAL (USART1 on "
                         "AVR64EA48: 0x0822)")
    ap.add_argument("--max-chars", type=int, default=400)
    ap.add_argument("--out", help="write the recovered bytes here")
    ap.add_argument("--log", help="keep the MDB log here")
    args = ap.parse_args(argv)
    try:
        res = run(Path(args.elf), args.device, args.txdata, args.max_chars)
    except (MdbError, subprocess.SubprocessError, OSError) as exc:
        print(f"mdb_sim: {exc}", file=sys.stderr)
        return 2
    if args.out:
        Path(args.out).write_bytes(res.data)
    if args.log:
        Path(args.log).write_text(res.log)
    sys.stdout.write(res.data.decode("latin-1").replace("\r\n", "\n"))
    for w in res.warnings:
        print(f"[sim] {w}", file=sys.stderr)
    print(f"[sim] {res.stops} stops, {len(res.data)} characters",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
