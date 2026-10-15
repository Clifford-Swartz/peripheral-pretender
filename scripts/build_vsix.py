# Copyright (c) 2026 Clifford Swartz.
"""Build the platform VSIX (win32-x64): the VS Code extension with the
engine bundled, so installing it is all a user does.

vscode/engine/ (staged here, not in git):
  python/   the python.org embeddable Python, with aiohttp
  app/      the project root the engine needs: tools/pp, tools/pp_web,
            devices/*.json and each board's demo firmware (if built)
  qemu/     qemu-system-arm and -avr (stripped) and their MSYS2 DLLs
  gdb/      gdb-multiarch (Arm, AVR) with its Python and data directory

  py scripts/build_vsix.py [--qemu C:/pp/pp-qemu] [--skip-package]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXT = ROOT / "vscode"
STAGE = EXT / "engine"
CACHE = Path.home() / ".cache" / "pp-vsix"
PYTHON = "3.12.10"
PY_TAG = "312"
WHEELS = ["aiohttp==3.13.5"]          # as the test suite runs with
MSYS2 = Path(os.environ.get("PP_MSYS2_ROOT", "C:/msys64"))
# Not needed at run time: art and catalog tools, notes
SKIP = shutil.ignore_patterns("__pycache__", "*.pyc", "make_art.py",
                              "render_click.py", "dxf_pads.py",
                              "compose_carrier.py", "recolor_board.py",
                              "*.md")


def log(msg: str) -> None:
    print(f"[vsix] {msg}", flush=True)


def fetch(url: str) -> Path:
    CACHE.mkdir(parents=True, exist_ok=True)
    dest = CACHE / url.rsplit("/", 1)[1]
    if not dest.is_file():
        log(f"download {url}")
        tmp = dest.with_suffix(".part")
        with urllib.request.urlopen(url) as r, open(tmp, "wb") as f:
            shutil.copyfileobj(r, f)
        tmp.replace(dest)
    return dest


def stage_python() -> None:
    out = STAGE / "python"
    zip_path = fetch(f"https://www.python.org/ftp/python/{PYTHON}/"
                     f"python-{PYTHON}-embed-amd64.zip")
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(out)
    # The embeddable Python ignores PYTHONPATH and site-packages unless
    # its ._pth file names them
    (out / f"python{PY_TAG}._pth").write_text(
        f"python{PY_TAG}.zip\n.\nLib/site-packages\n../app\nimport site\n",
        encoding="ascii")
    site = out / "Lib" / "site-packages"
    log("install " + " ".join(WHEELS))
    subprocess.run([sys.executable, "-m", "pip", "install", "--quiet",
                    "--target", str(site), "--platform", "win_amd64",
                    "--python-version", PYTHON.rsplit(".", 1)[0],
                    "--implementation", "cp", "--only-binary=:all:",
                    *WHEELS], check=True)


def stage_app() -> None:
    app = STAGE / "app"
    (app / "tools").mkdir(parents=True)
    shutil.copy2(ROOT / "tools" / "__init__.py", app / "tools")
    for pkg in ("pp", "pp_web"):
        shutil.copytree(ROOT / "tools" / pkg, app / "tools" / pkg,
                        ignore=SKIP)
    (app / "devices").mkdir()
    for f in (ROOT / "devices").glob("*.json"):
        shutil.copy2(f, app / "devices")
    for board in sorted((ROOT / "tools" / "pp_web" / "boards")
                        .glob("*/board.json")):
        demo = json.loads(board.read_text(encoding="utf-8")).get("demo_elf")
        if not demo:
            continue
        src = ROOT / demo
        if src.is_file():
            (app / demo).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, app / demo)
            log(f"demo {demo}")
        else:
            log(f"warning: {demo} not built; {board.parent.name} starts "
                "without firmware")


def msys(cmd: str) -> str:
    env = dict(os.environ, MSYSTEM="UCRT64", CHERE_INVOKING="1")
    return subprocess.run([str(MSYS2 / "usr" / "bin" / "bash.exe"), "-lc",
                           cmd], env=env, check=True, capture_output=True,
                          text=True).stdout


def stage_qemu(src: Path) -> None:
    out = STAGE / "qemu"
    out.mkdir()
    ucrt = MSYS2 / "ucrt64" / "bin"
    dlls: set[str] = set()
    for build, exe in (("build", "qemu-system-arm.exe"),
                       ("build-avr", "qemu-system-avr.exe")):
        binary = src / build / exe
        if not binary.is_file():
            raise SystemExit(f"{binary} not built")
        subprocess.run([str(ucrt / "strip.exe"), "-o", str(out / exe),
                        str(binary)], check=True)
        posix = binary.as_posix()
        for line in msys(f"ldd '{posix}'").splitlines():
            m = re.match(r"\s*(\S+\.dll) => /ucrt64/bin/", line, re.I)
            if m:
                dlls.add(m.group(1))
    for dll in sorted(dlls):
        shutil.copy2(ucrt / dll, out / dll)
    log(f"qemu: 2 binaries, {len(dlls)} DLLs")


def ucrt_dlls(binary: Path) -> list[str]:
    out = []
    for line in msys(f"ldd '{binary.as_posix()}'").splitlines():
        m = re.match(r"\s*(\S+\.dll) => /ucrt64/bin/", line, re.I)
        if m:
            out.append(m.group(1))
    return out


def stage_gdb() -> None:
    """MSYS2's gdb-multiarch (Arm and AVR) in its own prefix: gdb finds
    its Python (lib/python3.14) and data directory (share/gdb) relative to
    bin/, so the layout must stay."""
    ucrt = MSYS2 / "ucrt64"
    out = STAGE / "gdb"
    (out / "bin").mkdir(parents=True)
    exe = ucrt / "bin" / "gdb-multiarch.exe"
    subprocess.run([str(ucrt / "bin" / "strip.exe"), "-o",
                    str(out / "bin" / exe.name), str(exe)], check=True)
    dlls = ucrt_dlls(exe)
    for dll in dlls:
        shutil.copy2(ucrt / "bin" / dll, out / "bin" / dll)
    pyver = next(d for d in dlls if d.startswith("libpython"))[9:-4]
    shutil.copytree(ucrt / "lib" / f"python{pyver}",
                    out / "lib" / f"python{pyver}",
                    ignore=shutil.ignore_patterns(
                        "test", "tests", "idlelib", "tkinter", "turtledemo",
                        "ensurepip", "site-packages", "lib2to3",
                        "__pycache__", "config-*", "pydoc_data", "*.a"))
    shutil.copytree(ucrt / "share" / "gdb", out / "share" / "gdb")
    log(f"gdb: gdb-multiarch, {len(dlls)} DLLs, Python {pyver}")


def smoke() -> None:
    """The staged engine imports, and its QEMU starts, with no MSYS2 or
    system Python in sight."""
    env = {k: v for k, v in os.environ.items()
           if k.upper() not in ("PATH", "PYTHONPATH", "PP_QEMU",
                                "PP_QEMU_AVR", "PP_MSYS2_BIN")}
    env["PATH"] = os.environ.get("SystemRoot", "C:/Windows") + "/System32"
    qemu = STAGE / "qemu"
    env.update(PP_QEMU=str(qemu / "qemu-system-arm.exe"),
               PP_QEMU_AVR=str(qemu / "qemu-system-avr.exe"),
               PP_MSYS2_BIN=str(qemu), PYTHONNOUSERSITE="1")
    code = ("import aiohttp, tools.pp_web.server as s; "
            "from tools.pp.qemu import find_qemu, qemu_env; "
            "import subprocess; "
            "q = find_qemu(None, 'arm'); "
            "r = subprocess.run([str(q), '-machine', 'help'], env=qemu_env(), "
            "capture_output=True, text=True); "
            "assert 'pic32cm-mc00-cpro' in r.stdout, r.stderr; "
            "q = find_qemu(None, 'avr'); "
            "r = subprocess.run([str(q), '-machine', 'help'], env=qemu_env(), "
            "capture_output=True, text=True); "
            "assert 'avr64ea48-cnano' in r.stdout, r.stderr; print('ok')")
    subprocess.run([str(STAGE / "python" / "python.exe"), "-c", code],
                   cwd=STAGE / "app", env=env, check=True)
    gdb = subprocess.run(
        [str(STAGE / "gdb" / "bin" / "gdb-multiarch.exe"), "-q", "-batch",
         "-ex", "python import gdb; print('gdb', gdb.VERSION)",
         "-ex", "set architecture avr", "-ex", "set architecture arm"],
        env=env, capture_output=True, text=True)
    assert gdb.returncode == 0 and "gdb " in gdb.stdout, gdb.stderr
    log("smoke test: engine, both QEMUs and gdb start standalone")


def package() -> Path:
    npm = shutil.which("npm") or "npm"
    if not (EXT / "node_modules").is_dir():
        subprocess.run([npm, "install", "--no-audit", "--no-fund"], cwd=EXT,
                       check=True, shell=sys.platform == "win32")
    subprocess.run([npm, "run", "compile"], cwd=EXT, check=True,
                   shell=sys.platform == "win32")
    subprocess.run([npm, "run", "package"], cwd=EXT, check=True,
                   shell=sys.platform == "win32")
    version = json.loads((EXT / "package.json").read_text())["version"]
    return EXT / f"peripheral-pretender-win32-x64-{version}.vsix"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--qemu", type=Path,
                    default=Path(os.environ.get("PP_QEMU_SRC") or
                                 ROOT.parent / "pp-qemu"),
                    help="pp-qemu checkout with build/ and build-avr/")
    ap.add_argument("--skip-package", action="store_true",
                    help="stage and smoke-test the engine only")
    args = ap.parse_args(argv)
    if sys.platform != "win32":
        raise SystemExit("only the win32-x64 VSIX is built so far")
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    stage_python()
    stage_app()
    stage_qemu(args.qemu)
    stage_gdb()
    smoke()
    if not args.skip_package:
        vsix = package()
        log(f"{vsix} ({vsix.stat().st_size / 2**20:.1f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
