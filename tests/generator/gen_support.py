# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Paths and helpers shared by the generator tests.

Environment overrides: PP_PACKS (pack root, default ~/.mchp_packs),
PP_ATDF (device .atdf), PP_QEMU_SRC (pp-qemu source root, default ../pp-qemu
next to this repo; PP_QEMU is the QEMU binary for tools/ and harness/),
PP_GCC (gcc for C checks; default gcc on PATH, then MSYS2 UCRT64).
"""

import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "generator") not in sys.path:
    sys.path.insert(0, str(ROOT / "generator"))

PACKS = Path(os.environ.get("PP_PACKS", Path.home() / ".mchp_packs"))
ATDF = Path(os.environ.get(
    "PP_ATDF", PACKS / "Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf"
                       / "PIC32CM5164JH01100.atdf"))
# Second device: the generator merges per-IP constants over both
ATDF_MC00 = PACKS / ("Microchip/PIC32CM-MC_DFP/1.4.67/PIC32CM1216MC00048/atdf/"
                     "PIC32CM1216MC00048.atdf")
# Third device, 8-bit AVR (AVR8X): its own device-level outputs
ATDF_AVR = PACKS / "Microchip/AVR-Ex_DFP/2.8.189/atdf/AVR64EA48.atdf"
# SAM E51 (Curiosity CircuitPython board of the PyKit Explorer); the SAME51
# DFP has no family folder
ATDF_SAME51 = PACKS / "Microchip/SAME51_DFP/3.9.267/atdf/ATSAME51J20A.atdf"
QEMU = Path(os.environ.get("PP_QEMU_SRC", ROOT.parent / "pp-qemu"))
OVERLAYS = ROOT / "overlays"
PROFILES = ROOT / "profiles"
SHIM = Path(__file__).resolve().parent / "shim"
SPEC = ROOT / "docs" / "design" / "v0-vertical-slice.md"
RESEARCH = ROOT / "docs" / "research" / "microchip-data.md"


def find_gcc() -> str | None:
    explicit = os.environ.get("PP_GCC")
    if explicit and Path(explicit).exists():
        return explicit
    msys2_gcc = Path("C:/msys64/ucrt64/bin/gcc.exe")
    if sys.platform == "win32" and msys2_gcc.exists():
        return str(msys2_gcc)
    return shutil.which("gcc")


def gcc_env(gcc: str) -> dict:
    """gcc's own bin directory must be on PATH for its DLLs and cc1."""
    env = dict(os.environ)
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")
    return env
