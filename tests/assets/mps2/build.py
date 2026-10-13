# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Build the mps2-an385 runner test images with arm-none-eabi-gcc.

Usage: py tests/assets/mps2/build.py [--force]

Images land in tests/assets/mps2/build/ (git-ignored) and are rebuilt only
when a source is newer than the ELF. The compiler is taken from PP_ARM_GCC,
then PATH, then the MSYS2 UCRT64 install.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD_DIR = HERE / "build"

CFLAGS = [
    "-mcpu=cortex-m3", "-mthumb", "-Os", "-g",
    "-ffreestanding", "-nostdlib", "-fno-tree-loop-distribute-patterns",
    "-Wall", "-Wextra", "-Werror",
]

# image name -> (main source, extra compiler flags)
IMAGES = {
    "exit": ("exit.c", []),
    "exit3": ("exit.c", ["-DEXIT_CODE=3"]),
    "echo": ("echo.c", []),
    "burst": ("burst.c", []),
    "fault": ("fault.c", []),
    "lockup": ("fault.c", ["-DLOCKUP"]),
}

COMMON_SOURCES = ["startup.c", "cmsdk_uart.h", "mps2.ld", "build.py"]

MSYS2_UCRT64_GCC = Path("C:/msys64/ucrt64/bin/arm-none-eabi-gcc.exe")


class ToolchainMissing(RuntimeError):
    pass


def find_gcc() -> Path:
    env = os.environ.get("PP_ARM_GCC")
    if env:
        return Path(env)
    found = shutil.which("arm-none-eabi-gcc")
    if found:
        return Path(found)
    if MSYS2_UCRT64_GCC.exists():
        return MSYS2_UCRT64_GCC
    raise ToolchainMissing(
        "arm-none-eabi-gcc not found (set PP_ARM_GCC or add it to PATH)")


def _is_stale(elf: Path, sources: list[Path]) -> bool:
    if not elf.exists():
        return True
    built = elf.stat().st_mtime
    return any(src.stat().st_mtime > built for src in sources)


def build_image(name: str, gcc: Path, force: bool = False) -> Path:
    main_src, extra = IMAGES[name]
    elf = BUILD_DIR / f"{name}.elf"
    sources = [HERE / s for s in COMMON_SOURCES + [main_src]]
    if not force and not _is_stale(elf, sources):
        return elf
    BUILD_DIR.mkdir(exist_ok=True)
    cmd = [str(gcc), *CFLAGS, *extra, "-T", str(HERE / "mps2.ld"),
           "-Wl,-Map=" + str(elf.with_suffix(".map")),
           str(HERE / "startup.c"), str(HERE / main_src), "-o", str(elf)]
    # The MSYS2 compiler passes need the DLLs next to the driver.
    env = dict(os.environ)
    env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
    subprocess.run(cmd, check=True, env=env)
    return elf


def build_all(force: bool = False) -> dict[str, Path]:
    """Build every image; return {name: elf path}."""
    gcc = find_gcc()
    return {name: build_image(name, gcc, force) for name in IMAGES}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--force", action="store_true",
                        help="rebuild even if the images are up to date")
    args = parser.parse_args()
    try:
        images = build_all(args.force)
    except ToolchainMissing as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode or 1
    for name, elf in images.items():
        print(f"{name:6} {elf} ({elf.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
