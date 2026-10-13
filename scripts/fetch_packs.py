# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Download and unpack the device packs this project builds against.

The packs land in the layout the MPLAB VS Code extensions use,
<root>/<Vendor>/<Name>/<version>/, so the generator, firmware Makefile and
harness find them without extra configuration.  Root: --packs-dir, else
$PP_PACKS, else ~/.mchp_packs.  Stdlib only; runs on Windows, Linux, macOS.
"""
from __future__ import annotations

import argparse
import io
import os
import sys
import urllib.request
import zipfile
from pathlib import Path

# Pinned to the versions the generator and firmware were verified with.
PACKS = [
    {
        "path": ("Microchip", "PIC32CM-JH_DFP", "1.3.221"),
        "url": "https://packs.download.microchip.com/"
               "Microchip.PIC32CM-JH_DFP.1.3.221.atpack",
        "check": "PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf",
    },
    {
        "path": ("Microchip", "PIC32CM-MC_DFP", "1.4.67"),
        "url": "https://packs.download.microchip.com/"
               "Microchip.PIC32CM-MC_DFP.1.4.67.atpack",
        "check": "Microchip.PIC32CM-MC_DFP.pdsc",
    },
    {
        "path": ("ARM", "CMSIS", "6.1.0"),
        "url": "https://github.com/ARM-software/CMSIS_6/releases/download/"
               "v6.1.0/ARM.CMSIS.6.1.0.pack",
        "check": "CMSIS/Core/Include/core_cm0plus.h",
    },
]


def default_root() -> Path:
    return Path(os.environ.get("PP_PACKS", Path.home() / ".mchp_packs"))


def fetch(pack: dict, root: Path, force: bool) -> Path:
    dest = root.joinpath(*pack["path"])
    if (dest / pack["check"]).is_file() and not force:
        print(f"ok       {dest}")
        return dest
    print(f"download {pack['url']}")
    with urllib.request.urlopen(pack["url"], timeout=120) as resp:
        data = resp.read()
    dest.mkdir(parents=True, exist_ok=True)
    # Windows: the IAR/Keil folders nest deep enough to pass MAX_PATH, so
    # extract through an extended-length path.
    target = Path("\\\\?\\" + str(dest.resolve())) if os.name == "nt" else dest
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        zf.extractall(target)
    if not (dest / pack["check"]).is_file():
        raise SystemExit(f"unpacked {dest} but {pack['check']} is missing")
    print(f"unpacked {dest}")
    return dest


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--packs-dir", type=Path, default=default_root(),
                    help="pack root (default: $PP_PACKS or ~/.mchp_packs)")
    ap.add_argument("--force", action="store_true",
                    help="download again even if the pack is present")
    args = ap.parse_args(argv)
    for pack in PACKS:
        fetch(pack, args.packs_dir, args.force)
    return 0


if __name__ == "__main__":
    sys.exit(main())
