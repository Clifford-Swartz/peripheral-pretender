# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Build every MPLAB X project of one kit in an examples folder.

Two layouts:
- Harmony example repositories (Microchip-MPLAB-Harmony/csp_apps_<family>)
  hold one project per kit: apps/<group>/<app>/firmware/<kit>.X, built in
  the configuration named after the kit. Output: <out>/<group>__<app>.elf.
- One repository per example (github.com/microchip-pic-avr-examples, the
  8-bit kits, e.g. avr64ea48-breathing-led-mplab-melody/<repo>.X), cloned
  side by side into the folder (no apps/), each built in its project's
  first configuration ("free"). Output: <out>/<repo>.elf; --kit is then
  only a label.

MPLAB X's prjMakefilesGenerator writes the makefiles, then MPLAB X's own
make builds with the compiler the project names (languageToolchain): the
newest installed XC32 or XC8 (PP_XC32, PP_XC8 override). Packs the
projects pin are looked up in MPLAB X's packs folder and in ~/.mchp_packs;
a project whose pack is missing fails with "Device pack missing" in its
.build.log.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

MPLABX = Path(os.environ.get("PP_MPLABX",
                             r"C:\Program Files\Microchip\MPLABX\v6.20"))
XC32_ROOT = Path(r"C:\Program Files\Microchip\xc32")
XC8_ROOT = Path(r"C:\Program Files\Microchip\xc8")


@dataclass
class Built:
    app: str
    elf: Path | None
    log: Path
    ok: bool


def _newest_bin(root: Path, var: str) -> Path:
    if os.environ.get(var):
        return Path(os.environ[var])
    versions = sorted(root.glob("v*"), key=lambda p: [
        int(x) for x in p.name[1:].split(".") if x.isdigit()])
    if not versions:
        raise FileNotFoundError(f"no compiler under {root} (set {var})")
    return versions[-1] / "bin"


def xc32_bin() -> Path:
    """The newest installed XC32 (PP_XC32 overrides)."""
    return _newest_bin(XC32_ROOT, "PP_XC32")


def xc8_bin() -> Path:
    """The newest installed XC8 (PP_XC8 overrides)."""
    return _newest_bin(XC8_ROOT, "PP_XC8")


def project_conf(proj: Path) -> tuple[str, str]:
    """(first configuration, its toolchain) from nbproject/configurations.xml."""
    path = proj / "nbproject" / "configurations.xml"
    text = path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""
    conf = re.search(r'<conf name="([^"]+)"', text)
    tool = re.search(r"<languageToolchain>([^<]+)<", text)
    return (conf.group(1) if conf else "default",
            tool.group(1).strip() if tool else "XC32")


def projects(clone: Path, kit: str) -> list[tuple[str, Path]]:
    """(app name, project folder): every <kit>.X under apps/ (Harmony), or
    the .X of every example repository in @clone."""
    apps = clone / "apps"
    if not apps.is_dir():
        # <repo>/<repo>.X, or several projects: <repo>/<sub>/<sub>.X
        found = []
        for repo in sorted(p for p in clone.iterdir() if p.is_dir()):
            for proj in sorted(repo.glob("*.X")) + sorted(repo.glob("*/*.X")):
                sub = proj.parent.relative_to(repo).parts
                found.append(("__".join((repo.name, *sub)), proj))
        return found
    found = []
    for proj in sorted(apps.rglob(f"{kit}.X")):
        rel = proj.relative_to(apps).parent      # <group>/<app>/firmware
        name = "__".join(rel.parts[:-1] if rel.name == "firmware" else rel.parts)
        found.append((name, proj))
    return found


def build_one(proj: Path, kit: str, log: Path) -> Path | None:
    gen = MPLABX / "mplab_platform" / "bin" / "prjMakefilesGenerator.bat"
    make = MPLABX / "gnuBins" / "GnuWin32" / "bin" / "make.exe"
    conf, tool = project_conf(proj)
    if proj.name == f"{kit}.X":
        conf = kit                      # Harmony: one configuration per kit
    compiler = xc8_bin() if tool.upper() == "XC8" else xc32_bin()
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(make.parent), str(compiler), env["PATH"]])
    with log.open("w", encoding="utf-8", errors="replace") as f:
        r = subprocess.run(["cmd", "/c", str(gen), str(proj)], stdout=f,
                           stderr=subprocess.STDOUT, env=env)
        if r.returncode != 0:
            return None
        r = subprocess.run([str(make), "-f", f"nbproject/Makefile-{conf}.mk",
                            "SUBPROJECTS=", ".build-conf"], cwd=proj,
                           stdout=f, stderr=subprocess.STDOUT, env=env)
    elf = proj / "dist" / conf / "production" / f"{proj.name}.production.elf"
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
