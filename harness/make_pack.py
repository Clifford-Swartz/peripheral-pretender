# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Zip an installed device family pack (DFP) folder into a CMSIS .pack file.

pyOCD 0.45 reads an expanded pack folder directly (CmsisPack in
pyocd/target/pack/cmsis_pack.py), so the harness passes the DFP folder as is
and needs no .pack.  Build one for tools that only take the archive, or to
pin a single versioned artifact for CI machines:

    py -m harness.make_pack [--dfp DIR] [--out FILE] [--verify]

Entries are stored in sorted order with fixed timestamps, so the same folder
always gives byte-identical output.  The .pdsc lands at the archive root, as
the CMSIS-Pack format requires.
"""
from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

from harness import silicon

FIXED_TIME = (2020, 1, 1, 0, 0, 0)


class PackError(Exception):
    pass


def find_pdsc(dfp: Path) -> Path:
    found = sorted(dfp.glob("*.pdsc"))
    if len(found) != 1:
        raise PackError(f"{dfp}: expected one .pdsc at the top level, found {len(found)}")
    return found[0]


def pack_name(pdsc: Path) -> str:
    """<vendor>.<name>.<version>.pack; the first <release> is the newest."""
    root = ET.parse(pdsc).getroot()
    release = root.find("releases/release")
    vendor, name = root.findtext("vendor"), root.findtext("name")
    if not vendor or not name or release is None or not release.get("version"):
        raise PackError(f"{pdsc}: no vendor, name or release version")
    return f"{vendor}.{name}.{release.get('version')}.pack"


def make_pack(dfp: Path, out: Path) -> int:
    """Write the archive; return the number of files stored."""
    files = sorted((p for p in dfp.rglob("*") if p.is_file()),
                   key=lambda p: p.relative_to(dfp).as_posix())
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            info = zipfile.ZipInfo(path.relative_to(dfp).as_posix(), FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, path.read_bytes())
    return len(files)


def verify(pack: Path, target: str) -> None:
    """Check that pyOCD can parse the pack and finds target in it."""
    from pyocd.target.pack.cmsis_pack import CmsisPack

    parts = {dev.part_number.lower() for dev in CmsisPack(str(pack)).devices}
    if target.lower() not in parts:
        raise PackError(f"{pack}: pyOCD finds no {target} (parts: {sorted(parts)})")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="py -m harness.make_pack", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dfp", type=Path, default=silicon.DFP_DIR,
                        help="expanded DFP folder (default %(default)s)")
    parser.add_argument("--out", type=Path,
                        help="output file or directory (default: <vendor>.<name>.<version>.pack "
                             "in the current directory)")
    parser.add_argument("--verify", action="store_true",
                        help=f"load the result with pyOCD and look for {silicon.TARGET}")
    parser.add_argument("--target", default=silicon.TARGET, help="part --verify looks for")
    args = parser.parse_args(argv)
    try:
        name = pack_name(find_pdsc(args.dfp))
        out = args.out or Path(name)
        if out.is_dir():
            out = out / name
        count = make_pack(args.dfp, out)
        print(f"{out}: {count} files, {out.stat().st_size} bytes")
        if args.verify:
            verify(out, args.target)
            print(f"pyOCD finds {args.target} in {out.name}")
    except (PackError, OSError, ET.ParseError) as exc:
        print(f"make_pack: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
