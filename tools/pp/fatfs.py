# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""FAT12/FAT16 file systems in flash images: the CIRCUITPY drive.

CircuitPython keeps its CIRCUITPY drive as a FAT file system at the start
of the board's external flash (Curiosity CircuitPython: an SST26VF032B,
whose image the twin loads with -drive if=mtd). This module formats such
an image the way CircuitPython does and reads and writes files in it, so
tools can put code.py and libraries on the twin's drive without USB mass
storage.

    py -m tools.pp.fatfs build circuitpy.img --from <folder>
    py -m tools.pp.fatfs ls circuitpy.img [path]
    py -m tools.pp.fatfs get circuitpy.img code.py
    py -m tools.pp.fatfs put circuitpy.img code.py ./code.py

Long file names (VFAT) are read and written; there is no FAT32 (CIRCUITPY
drives on QSPI flash are FAT12 or FAT16). Stdlib only.
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path

SECTOR = 512
ATTR_RO, ATTR_HIDDEN, ATTR_SYSTEM, ATTR_VOLUME, ATTR_DIR, ATTR_ARCHIVE = (
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20)
ATTR_LFN = 0x0F

# The CircuitPython layout for flash of @size bytes: the last 4 KiB erase
# sector is a staging area, the rest is blocks of 512 bytes; FatFs f_mkfs
# with one FAT, 512 root entries, no partition table (measured on the twin
# running CircuitPython 10.1.4: 8184 sectors, 4 per cluster, FAT12).
STAGING = 4096


class FatError(Exception):
    pass


@dataclass
class Entry:
    name: str           # long name if any, else the 8.3 name
    short: bytes        # 11-byte 8.3 name
    attr: int
    cluster: int
    size: int
    offset: int         # byte offset of the short entry in the image
    first: int          # byte offset of its first (LFN) entry

    @property
    def is_dir(self) -> bool:
        return bool(self.attr & ATTR_DIR)


def _parts(path: str) -> list[str]:
    """Components of a drive path ("lib/x.py", "/lib/x.py", "" = root)."""
    return [p for p in path.replace("\\", "/").split("/") if p not in ("", ".")]


def _fat_time(t: float | None = None) -> tuple[int, int]:
    lt = time.localtime(t)
    date = ((max(lt.tm_year, 1980) - 1980) << 9) | (lt.tm_mon << 5) | lt.tm_mday
    tm = (lt.tm_hour << 11) | (lt.tm_min << 5) | (lt.tm_sec // 2)
    return date, tm


def _short_checksum(short: bytes) -> int:
    s = 0
    for b in short:
        s = (((s & 1) << 7) + (s >> 1) + b) & 0xFF
    return s


class FatImage:
    """A FAT12/16 volume inside @data (a bytearray of the whole image)."""

    def __init__(self, data: bytearray, offset: int = 0):
        self.data = data
        self.base = offset
        b = bytes(data[offset:offset + SECTOR])
        if b[510:512] != b"\x55\xaa":
            raise FatError("no FAT boot sector (blank or foreign image)")
        (self.bps, self.spc, self.rsv, self.nfats, self.nroot, tot16,
         _media, self.fatsz, _spt, _heads, _hidden, tot32) = struct.unpack(
            "<HBHBHHBHHHII", b[11:36])
        if self.bps != SECTOR or not self.fatsz:
            raise FatError("unsupported FAT geometry (FAT32?)")
        self.total = tot16 or tot32
        self.fat_start = offset + self.rsv * SECTOR
        self.root_start = self.fat_start + self.nfats * self.fatsz * SECTOR
        root_bytes = self.nroot * 32
        self.data_start = self.root_start + -(-root_bytes // SECTOR) * SECTOR
        first_data = (self.data_start - offset) // SECTOR
        self.nclusters = (self.total - first_data) // self.spc
        self.bits = 12 if self.nclusters < 4085 else 16
        self.csize = self.spc * SECTOR

    # FAT

    def fat_get(self, n: int) -> int:
        if self.bits == 16:
            return struct.unpack_from("<H", self.data, self.fat_start + 2 * n)[0]
        off = self.fat_start + n * 3 // 2
        v = self.data[off] | self.data[off + 1] << 8
        return v >> 4 if n & 1 else v & 0xFFF

    def fat_set(self, n: int, value: int) -> None:
        for f in range(self.nfats):
            base = self.fat_start + f * self.fatsz * SECTOR
            if self.bits == 16:
                struct.pack_into("<H", self.data, base + 2 * n, value)
                continue
            off = base + n * 3 // 2
            v = self.data[off] | self.data[off + 1] << 8
            v = (v & 0x000F) | (value << 4) if n & 1 else (v & 0xF000) | value
            self.data[off], self.data[off + 1] = v & 0xFF, v >> 8

    @property
    def eoc(self) -> int:
        return 0xFFF if self.bits == 12 else 0xFFFF

    def chain(self, first: int) -> list[int]:
        out, n = [], first
        while 2 <= n < self.nclusters + 2 and len(out) <= self.nclusters:
            out.append(n)
            n = self.fat_get(n)
        return out

    def alloc(self, count: int) -> list[int]:
        free = [n for n in range(2, self.nclusters + 2)
                if self.fat_get(n) == 0][:count]
        if len(free) < count:
            raise FatError("CIRCUITPY drive is full")
        for a, b in zip(free, free[1:] + [None]):
            self.fat_set(a, b if b is not None else self.eoc)
        return free

    def free_chain(self, first: int) -> None:
        for n in self.chain(first):
            self.fat_set(n, 0)

    def cluster_off(self, n: int) -> int:
        return self.data_start + (n - 2) * self.csize

    def free_bytes(self) -> int:
        return sum(1 for n in range(2, self.nclusters + 2)
                   if self.fat_get(n) == 0) * self.csize

    # directories

    def _dir_slots(self, cluster: int) -> list[int]:
        """Byte offsets of every 32-byte slot of a directory."""
        if cluster == 0:
            return [self.root_start + 32 * i for i in range(self.nroot)]
        return [self.cluster_off(c) + 32 * i for c in self.chain(cluster)
                for i in range(self.csize // 32)]

    def listdir(self, cluster: int = 0) -> list[Entry]:
        out, lfn, first = [], {}, None
        for off in self._dir_slots(cluster):
            e = bytes(self.data[off:off + 32])
            if e[0] == 0:
                break
            if e[0] == 0xE5:
                lfn, first = {}, None
                continue
            if e[11] == ATTR_LFN:
                if first is None:
                    first = off
                seq = e[0] & 0x1F
                chars = e[1:11] + e[14:26] + e[28:32]
                lfn[seq] = chars.decode("utf-16-le", "replace")
                continue
            if e[11] & ATTR_VOLUME:
                lfn, first = {}, None
                continue
            short = e[:11]
            if lfn:
                name = "".join(lfn[k] for k in sorted(lfn))
                name = name.split("\0")[0]
            else:
                base = short[:8].decode("latin-1").rstrip()
                ext = short[8:].decode("latin-1").rstrip()
                if e[12] & 0x08:
                    base = base.lower()
                if e[12] & 0x10:
                    ext = ext.lower()
                name = base + ("." + ext if ext else "")
            cl = struct.unpack_from("<H", e, 26)[0]
            size = struct.unpack_from("<I", e, 28)[0]
            if short[:1] != b".":
                out.append(Entry(name, short, e[11], cl, size, off,
                                 first if first is not None else off))
            lfn, first = {}, None
        return out

    def lookup(self, path: str) -> Entry | None:
        parts = _parts(path)
        cluster, found = 0, None
        for i, part in enumerate(parts):
            found = next((e for e in self.listdir(cluster)
                          if e.name.lower() == part.lower()), None)
            if found is None:
                return None
            if i < len(parts) - 1:
                if not found.is_dir:
                    return None
                cluster = found.cluster
        return found

    def _dir_cluster(self, path: str, create: bool) -> int:
        parts = _parts(path)
        cluster = 0
        for part in parts:
            e = next((x for x in self.listdir(cluster)
                      if x.name.lower() == part.lower()), None)
            if e is None:
                if not create:
                    raise FatError(f"no directory {path}")
                e = self._mkdir_in(cluster, part)
            elif not e.is_dir:
                raise FatError(f"{part} is a file")
            cluster = e.cluster
        return cluster

    def _short_name(self, cluster: int, name: str) -> tuple[bytes, bool, int]:
        """8.3 name for @name, whether LFN entries are needed, and the NT
        case flags (FatFs stores "code.py" as CODE.PY with lower-case
        flags and no LFN, as Windows does)."""
        allowed = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$%'-_@~`!(){}^#&")
        base, dot, ext = name.rpartition(".")
        if not dot or not base:
            base, ext = name, ""

        def plain(part, maxlen):
            return (0 < len(part) <= maxlen or (part == "" and maxlen == 3))                 and set(part.upper()) <= allowed                 and (part == part.upper() or part == part.lower())

        if plain(base, 8) and plain(ext, 3):
            flags = (0x08 if base != base.upper() else 0) |                     (0x10 if ext != ext.upper() else 0)
            short = (base.upper().ljust(8) + ext.upper().ljust(3)).encode()
            if short not in {e.short for e in self.listdir(cluster)}:
                return short, False, flags
        ok = "".join(c for c in base.upper() if c in allowed)
        okx = "".join(c for c in ext.upper() if c in allowed)
        existing = {e.short for e in self.listdir(cluster)}
        for n in range(1, 1000000):
            tail = f"~{n}"
            short = (ok[:8 - len(tail)] + tail).ljust(8) + okx[:3].ljust(3)
            if short.encode("ascii") not in existing:
                return short.encode("ascii"), True, 0
        raise FatError("no free short name")

    def _free_slots(self, cluster: int, count: int) -> list[int]:
        slots = self._dir_slots(cluster)
        run = []
        for off in slots:
            if self.data[off] in (0x00, 0xE5):
                run.append(off)
                if len(run) == count:
                    return run
            else:
                run = []
        if cluster == 0:
            raise FatError("root directory is full")
        # Grow the directory by one cluster
        last = self.chain(cluster)[-1]
        new = self.alloc(1)[0]
        self.fat_set(last, new)
        off = self.cluster_off(new)
        self.data[off:off + self.csize] = bytes(self.csize)
        return self._free_slots(cluster, count)

    def _add_entry(self, cluster: int, name: str, attr: int, first: int,
                   size: int) -> None:
        short, need_lfn, nt = self._short_name(cluster, name)
        lfn_entries = []
        if need_lfn:
            u = name.encode("utf-16-le")
            chars = [u[i:i + 2] for i in range(0, len(u), 2)] + [b"\0\0"]
            while len(chars) % 13:
                chars.append(b"\xff\xff")
            csum = _short_checksum(short)
            groups = [chars[i:i + 13] for i in range(0, len(chars), 13)]
            for seq, g in enumerate(groups, 1):
                e = bytearray(32)
                e[0] = seq | (0x40 if seq == len(groups) else 0)
                e[1:11] = b"".join(g[0:5])
                e[11], e[12], e[13] = ATTR_LFN, 0, csum
                e[14:26] = b"".join(g[5:11])
                e[28:32] = b"".join(g[11:13])
                lfn_entries.append(bytes(e))
            lfn_entries.reverse()
        slots = self._free_slots(cluster, len(lfn_entries) + 1)
        for off, e in zip(slots, lfn_entries):
            self.data[off:off + 32] = e
        date, tm = _fat_time()
        e = bytearray(32)
        e[0:11] = short
        e[11], e[12] = attr, nt
        struct.pack_into("<HHHHHHHI", e, 14, tm, date, date, first >> 16,
                         tm, date, first & 0xFFFF, size)
        self.data[slots[-1]:slots[-1] + 32] = e

    def _mkdir_in(self, parent: int, name: str) -> Entry:
        c = self.alloc(1)[0]
        off = self.cluster_off(c)
        self.data[off:off + self.csize] = bytes(self.csize)
        date, tm = _fat_time()
        for i, (n, cl) in enumerate(((b".".ljust(11), c),
                                     (b"..".ljust(11), parent))):
            e = bytearray(32)
            e[0:11], e[11] = n, ATTR_DIR
            struct.pack_into("<HHHHHHHI", e, 14, tm, date, date, 0, tm, date,
                             cl, 0)
            self.data[off + 32 * i:off + 32 * i + 32] = e
        self._add_entry(parent, name, ATTR_DIR, c, 0)
        return next(e for e in self.listdir(parent) if e.name == name)

    # files

    def read(self, path: str) -> bytes:
        e = self.lookup(path)
        if e is None or e.is_dir:
            raise FatError(f"no file {path}")
        out = bytearray()
        for c in self.chain(e.cluster):
            out += self.data[self.cluster_off(c):self.cluster_off(c) + self.csize]
        return bytes(out[:e.size])

    def remove(self, path: str) -> None:
        e = self.lookup(path)
        if e is None:
            raise FatError(f"no file {path}")
        if e.is_dir:
            for child in self.listdir(e.cluster):
                self.remove(f"{path}/{child.name}")
        if e.cluster:
            self.free_chain(e.cluster)
        for off in range(e.first, e.offset + 32, 32):
            self.data[off] = 0xE5

    def write(self, path: str, content: bytes) -> None:
        parts = _parts(path)
        if not parts:
            raise FatError("empty file name")
        if self.lookup(path) is not None:
            self.remove(path)
        parent = self._dir_cluster("/".join(parts[:-1]), create=True)
        n = -(-len(content) // self.csize)
        chain = self.alloc(n) if n else []
        for i, c in enumerate(chain):
            chunk = content[i * self.csize:(i + 1) * self.csize]
            off = self.cluster_off(c)
            self.data[off:off + self.csize] = chunk.ljust(self.csize, b"\0")
        self._add_entry(parent, parts[-1], ATTR_ARCHIVE,
                        chain[0] if chain else 0, len(content))

    def mkdir(self, path: str) -> None:
        self._dir_cluster(path, create=True)

    def walk(self, cluster: int = 0, prefix: str = ""):
        """(path, Entry) of every file and directory, depth first."""
        for e in self.listdir(cluster):
            path = f"{prefix}{e.name}"
            yield path, e
            if e.is_dir:
                yield from self.walk(e.cluster, path + "/")


def format_image(size: int = 4 << 20, label: str = "CIRCUITPY") -> bytearray:
    """An erased flash image of @size bytes holding an empty FAT volume laid
    out as CircuitPython formats it."""
    data = bytearray(b"\xff" * size)
    total = (size - STAGING) // SECTOR
    spc = 1
    while total // spc > 4084 and spc < 64:     # FatFs picks FAT12 cluster
        spc *= 2                                # sizes this way for small
    nroot = 512                                 # volumes
    nclus = total // spc
    bits = 12 if nclus < 4085 else 16
    fatsz = -(-((nclus + 2) * bits // 8) // SECTOR)
    rsv = 1
    first_data = rsv + fatsz + nroot * 32 // SECTOR
    b = bytearray(SECTOR)
    b[0:3] = b"\xeb\xfe\x90"
    b[3:11] = b"MSDOS5.0"
    struct.pack_into("<HBHBHHBHHHII", b, 11, SECTOR, spc, rsv, 1, nroot,
                     total if total < 0x10000 else 0, 0xF8, fatsz, 63, 255,
                     1, total if total >= 0x10000 else 0)
    serial = int(time.time()) & 0xFFFFFFFF
    struct.pack_into("<BBBI", b, 36, 0x80, 0, 0x29, serial)
    b[43:54] = label.upper().ljust(11)[:11].encode("ascii")
    b[54:62] = (b"FAT12   " if bits == 12 else b"FAT16   ")
    b[510:512] = b"\x55\xaa"
    data[0:SECTOR] = b
    # FAT, root directory and data area start out zeroed
    end = first_data * SECTOR
    data[SECTOR:end] = bytes(end - SECTOR)
    fat = FatImage(data)
    fat.fat_set(0, 0xFF8 if bits == 12 else 0xFFF8)
    fat.fat_set(1, fat.eoc)
    e = bytearray(32)
    e[0:11] = label.upper().ljust(11)[:11].encode("ascii")
    e[11] = ATTR_VOLUME | ATTR_ARCHIVE
    date, tm = _fat_time()
    struct.pack_into("<HH", e, 22, tm, date)
    data[fat.root_start:fat.root_start + 32] = e
    # Clear the data area CircuitPython would see as directories
    data[end:end + fat.csize * 2] = bytes(fat.csize * 2)
    return data


def build_image(files: dict[str, bytes], size: int = 4 << 20,
                label: str = "CIRCUITPY") -> bytearray:
    data = format_image(size, label)
    fat = FatImage(data)
    for path in sorted(files):
        fat.write(path, files[path])
    return data


def folder_files(folder: Path, skip=("System Volume Information",)) -> dict[str, bytes]:
    out = {}
    for p in sorted(folder.rglob("*")):
        rel = p.relative_to(folder).as_posix()
        if p.is_file() and not any(rel.startswith(s) for s in skip):
            out[rel] = p.read_bytes()
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="new image (optionally from a folder)")
    b.add_argument("image", type=Path)
    b.add_argument("--from", dest="src", type=Path)
    b.add_argument("--size", type=lambda s: int(s, 0), default=4 << 20)
    ls = sub.add_parser("ls")
    ls.add_argument("image", type=Path)
    g = sub.add_parser("get")
    g.add_argument("image", type=Path)
    g.add_argument("path")
    pt = sub.add_parser("put")
    pt.add_argument("image", type=Path)
    pt.add_argument("path")
    pt.add_argument("src", type=Path)
    rm = sub.add_parser("rm")
    rm.add_argument("image", type=Path)
    rm.add_argument("path")
    args = ap.parse_args(argv)
    try:
        if args.cmd == "build":
            files = folder_files(args.src) if args.src else {}
            args.image.write_bytes(build_image(files, args.size))
            print(f"wrote {args.image} ({len(files)} files)")
            return 0
        data = bytearray(args.image.read_bytes())
        fat = FatImage(data)
        if args.cmd == "ls":
            for path, e in fat.walk():
                print(f"{'d' if e.is_dir else '-'} {e.size:8d} {path}")
            print(f"free: {fat.free_bytes()} bytes")
        elif args.cmd == "get":
            sys.stdout.buffer.write(fat.read(args.path))
        elif args.cmd == "put":
            fat.write(args.path, args.src.read_bytes())
            args.image.write_bytes(data)
        elif args.cmd == "rm":
            fat.remove(args.path)
            args.image.write_bytes(data)
    except (OSError, FatError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
