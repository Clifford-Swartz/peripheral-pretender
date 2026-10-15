# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""UF2 firmware files (CircuitPython, Arduino) to ELF for the twin.

A UF2 file is a list of 512-byte blocks, each carrying up to 476 bytes
for a flash address (https://github.com/microsoft/uf2). The twin loads
ELF files (QEMU's -kernel), so convert: one PT_LOAD segment per run of
contiguous blocks, entry = the reset vector of the image's vector table.

    py -m tools.pp.uf2 firmware.uf2 firmware.elf

Stdlib only.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_NOT_MAIN_FLASH = 0x00000001
UF2_FLAG_FAMILY = 0x00002000

# Family IDs this project knows (uf2families.json)
FAMILIES = {0x55114460: "SAMD51", 0x68ED2B88: "SAMD21"}


class Uf2Error(ValueError):
    pass


def parse(data: bytes) -> tuple[dict[int, bytes], set[int]]:
    """{address: payload} of the main-flash blocks, and the family IDs."""
    if len(data) % 512:
        raise Uf2Error("not a UF2 file: size is not a multiple of 512")
    blocks: dict[int, bytes] = {}
    families: set[int] = set()
    for off in range(0, len(data), 512):
        blk = data[off:off + 512]
        m0, m1, flags, addr, size, _, _, fam = struct.unpack("<8I", blk[:32])
        end, = struct.unpack("<I", blk[508:512])
        if m0 != UF2_MAGIC0 or m1 != UF2_MAGIC1 or end != UF2_MAGIC_END:
            raise Uf2Error(f"block at 0x{off:X}: bad UF2 magic")
        if flags & UF2_FLAG_NOT_MAIN_FLASH:
            continue
        if size > 476:
            raise Uf2Error(f"block at 0x{off:X}: payload of {size} bytes")
        if flags & UF2_FLAG_FAMILY:
            families.add(fam)
        blocks[addr] = blk[32:32 + size]
    if not blocks:
        raise Uf2Error("no main-flash blocks")
    return blocks, families


def segments(blocks: dict[int, bytes]) -> list[tuple[int, bytes]]:
    """Contiguous (address, bytes) runs, gaps between blocks filled 0xFF."""
    out: list[tuple[int, bytearray]] = []
    for addr in sorted(blocks):
        data = blocks[addr]
        if out and out[-1][0] + len(out[-1][1]) == addr:
            out[-1][1].extend(data)
        else:
            out.append((addr, bytearray(data)))
    return [(a, bytes(d)) for a, d in out]


def to_elf(segs: list[tuple[int, bytes]], entry: int | None = None) -> bytes:
    """A little-endian ARM ELF32 executable with one PT_LOAD per segment."""
    if entry is None:
        base, first = segs[0]
        entry = struct.unpack("<I", first[4:8])[0] if len(first) >= 8 else base
    ehsize, phentsize = 52, 32
    phoff = ehsize
    data_off = phoff + phentsize * len(segs)
    phdrs, payload = b"", b""
    for addr, data in segs:
        off = data_off + len(payload)
        # PT_LOAD, R+X, 4-byte alignment
        phdrs += struct.pack("<8I", 1, off, addr, addr, len(data), len(data),
                             5, 4)
        payload += data + b"\0" * (-len(data) % 4)
    ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + b"\0" * 8
    # ET_EXEC, EM_ARM, EV_CURRENT, EABI version 5
    ehdr = ident + struct.pack("<HHIIIIIHHHHHH", 2, 40, 1, entry, phoff, 0,
                               0x05000000, ehsize, phentsize, len(segs),
                               40, 0, 0)
    return ehdr + phdrs + payload


def uf2_to_elf(data: bytes) -> bytes:
    blocks, _ = parse(data)
    return to_elf(segments(blocks))


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("uf2", type=Path)
    ap.add_argument("elf", type=Path)
    args = ap.parse_args(argv)
    try:
        data = args.uf2.read_bytes()
        blocks, families = parse(data)
    except (OSError, Uf2Error) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    segs = segments(blocks)
    args.elf.write_bytes(to_elf(segs))
    fam = ", ".join(FAMILIES.get(f, f"0x{f:08X}") for f in sorted(families))
    for addr, d in segs:
        print(f"0x{addr:08X}-0x{addr + len(d):08X} ({len(d)} bytes)")
    print(f"family: {fam or 'not given'}; wrote {args.elf}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
