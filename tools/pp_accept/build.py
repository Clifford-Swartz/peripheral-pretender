# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Build every MPLAB X project of one kit in a Harmony examples clone.

Harmony example repositories (Microchip-MPLAB-Harmony/csp_apps_<family>)
hold one project per kit: apps/<group>/<app>/firmware/<kit>.X. MPLAB X's
prjMakefilesGenerator writes the makefiles, then MPLAB X's own make builds
the configuration named after the kit with XC32. Packs the projects pin
are looked up in MPLAB X's packs folder and in ~/.mchp_packs; a project
whose pack is missing fails with "Device pack missing" in its .gen.log.
The output is copied to <out>/<group>__<app>.elf.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

MPLABX = Path(os.environ.get("PP_MPLABX",
                             r"C:\Program Files\Microchip\MPLABX\v6.20"))
XC32_ROOT = Path(r"C:\Program Files\Microchip\xc32")


@dataclass
class Built:
    app: str
    elf: Path | None
    log: Path
    ok: bool


def xc32_bin() -> Path:
    """The newest installed XC32 (PP_XC32 overrides)."""
    if os.environ.get("PP_XC32"):
        return Path(os.environ["PP_XC32"])
    versions = sorted(XC32_ROOT.glob("v*"), key=lambda p: [
        int(x) for x in p.name[1:].split(".") if x.isdigit()])
    if not versions:
        raise FileNotFoundError(f"no XC32 under {XC32_ROOT} (set PP_XC32)")
    return versions[-1] / "bin"


def projects(clone: Path, kit: str) -> list[tuple[str, Path]]:
    """(app name, project folder) for every <kit>.X under apps/."""
    apps = clone / "apps"
    found = []
    for proj in sorted(apps.rglob(f"{kit}.X")):
        rel = proj.relative_to(apps).parent      # <group>/<app>/firmware
        name = "__".join(rel.parts[:-1] if rel.name == "firmware" else rel.parts)
        found.append((name, proj))
    return found


def build_one(proj: Path, kit: str, log: Path) -> Path | None:
    gen = MPLABX / "mplab_platform" / "bin" / "prjMakefilesGenerator.bat"
    make = MPLABX / "gnuBins" / "GnuWin32" / "bin" / "make.exe"
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(make.parent), str(xc32_bin()), env["PATH"]])
    with log.open("w", encoding="utf-8", errors="replace") as f:
        r = subprocess.run(["cmd", "/c", str(gen), str(proj)], stdout=f,
                           stderr=subprocess.STDOUT, env=env)
        if r.returncode != 0:
            return None
        r = subprocess.run([str(make), "-f", f"nbproject/Makefile-{kit}.mk",
                            "SUBPROJECTS=", ".build-conf"], cwd=proj,
                           stdout=f, stderr=subprocess.STDOUT, env=env)
    elf = proj / "dist" / kit / "production" / f"{kit}.X.production.elf"
    return elf if r.returncode == 0 and elf.is_file() else None


def build_all(clone: Path, kit: str, out: Path, force: bool = False,
              only: list[str] | None = None) -> list[Built]:
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for name, proj in projects(clone, kit):
        if only and name not in only:
            continue
        dest, log = out / f"{name}.elf", out / f"{name}.build.log"
        if dest.is_file() and not force:
            results.append(Built(name, dest, log, True))
            continue
        elf = build_one(proj, kit, log)
        if elf:
            shutil.copyfile(elf, dest)
        results.append(Built(name, dest if elf else None, log, bool(elf)))
    return results
