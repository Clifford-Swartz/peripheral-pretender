# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Stock CircuitPython on the twin: fetch the official firmware, build the
CIRCUITPY drive image, boot and talk to the REPL.

    py -m tools.pp.circuitpython fetch [--version 10.1.4]
    py -m tools.pp.circuitpython run <folder with code.py | image> \\
        [--lines "print(1)" ...] [--seconds 20] [--machine ...]

The firmware is Adafruit's unmodified build for the board
(downloads.circuitpython.org, MIT licence), cached as UF2 and as the ELF
the twin loads under $PP_CACHE (default ~/.pp_cache/circuitpython). The
drive is a flash image (tools.pp.fatfs) the machine's SST26VF032B loads.
The console is the USB CDC port, serial0 of curiosity-circuitpython.
"""

from __future__ import annotations

import argparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

from . import fatfs, uf2
from .qemu import find_qemu, qemu_env

BOARD_ID = "microchip_curiosity_circuitpython"
DEFAULT_VERSION = "10.1.4"
DEFAULT_MACHINE = "curiosity-circuitpython"
URL = ("https://downloads.circuitpython.org/bin/{board}/en_US/"
       "adafruit-circuitpython-{board}-en_US-{version}.uf2")


def cache_dir() -> Path:
    return Path(os.environ.get("PP_CACHE", Path.home() / ".pp_cache")) / \
        "circuitpython"


def elf_path(version: str = DEFAULT_VERSION, board: str = BOARD_ID) -> Path:
    return cache_dir() / f"{board}-{version}.elf"


def fetch(version: str = DEFAULT_VERSION, board: str = BOARD_ID) -> Path:
    """The ELF of the official UF2 (downloaded and converted once)."""
    elf = elf_path(version, board)
    if elf.is_file():
        return elf
    elf.parent.mkdir(parents=True, exist_ok=True)
    uf2_file = elf.with_suffix(".uf2")
    if not uf2_file.is_file():
        url = URL.format(board=board, version=version)
        with urllib.request.urlopen(url, timeout=120) as resp:
            data = resp.read()
        uf2.parse(data)                 # refuse anything that is not UF2
        uf2_file.write_bytes(data)
    elf.write_bytes(uf2.uf2_to_elf(uf2_file.read_bytes()))
    return elf


def image_from(src: Path, dest: Path) -> Path:
    """A CIRCUITPY image at @dest: a copy of @src if it is one, else built
    from the folder @src."""
    if src.is_file():
        shutil.copyfile(src, dest)
    else:
        dest.write_bytes(fatfs.build_image(fatfs.folder_files(src)))
    return dest


def _free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def run(elf: Path, image: Path, lines: list[str] | None = None,
        seconds: float = 20.0, machine: str = DEFAULT_MACHINE,
        extra: list[str] | None = None, icount: bool = True) -> str:
    """Boot @elf with the CIRCUITPY @image; with @lines, enter the REPL
    (after code.py) and send each line. Returns the console text."""
    port = _free_port()
    cmd = [str(find_qemu()), "-M", machine, "-kernel", str(elf),
           "-drive", f"if=mtd,format=raw,file={image}",
           "-display", "none", "-monitor", "none",
           "-serial", f"tcp:127.0.0.1:{port},server=on,wait=on"]
    if icount:
        cmd += ["-icount", "shift=auto,sleep=on"]
    cmd += extra or []
    proc = subprocess.Popen(cmd, env=qemu_env(), stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE)
    out = b""
    try:
        deadline = time.monotonic() + 20
        while True:
            try:
                sock = socket.create_connection(("127.0.0.1", port))
                break
            except OSError:
                if time.monotonic() > deadline or proc.poll() is not None:
                    err = proc.stderr.read().decode(errors="replace")
                    raise RuntimeError(f"QEMU did not start: {err}")
                time.sleep(0.1)
        sock.settimeout(0.2)

        def read_until(token: bytes | None, timeout: float) -> bytes:
            nonlocal out
            end, got = time.monotonic() + timeout, b""
            while time.monotonic() < end:
                try:
                    d = sock.recv(4096)
                except socket.timeout:
                    continue
                if not d:
                    break
                got += d
                if token and token in got:
                    break
            out += got
            return got

        if not lines:
            read_until(None, seconds)
        else:
            read_until(b"Press any key", seconds)
            sock.sendall(b"\r")
            read_until(b">>> ", 10)
            for line in lines:
                sock.sendall(line.encode() + b"\r")
                read_until(b">>> ", seconds)
        sock.close()
    finally:
        proc.kill()
        proc.wait()
    return out.decode("utf-8", "replace")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch", help="download and convert the firmware")
    f.add_argument("--version", default=DEFAULT_VERSION)
    r = sub.add_parser("run", help="boot the twin with a CIRCUITPY drive")
    r.add_argument("drive", type=Path, help="folder with code.py, or image")
    r.add_argument("--version", default=DEFAULT_VERSION)
    r.add_argument("--elf", type=Path, help="firmware ELF (default: cache)")
    r.add_argument("--machine", default=DEFAULT_MACHINE)
    r.add_argument("--seconds", type=float, default=20.0)
    r.add_argument("--lines", nargs="*", help="REPL lines after code.py")
    args = ap.parse_args(argv)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if args.cmd == "fetch":
        print(fetch(args.version))
        return 0
    elf = args.elf or fetch(args.version)
    with tempfile.TemporaryDirectory() as tmp:
        image = image_from(args.drive, Path(tmp) / "circuitpy.img")
        print(run(elf, image, args.lines, args.seconds, args.machine))
    return 0


if __name__ == "__main__":
    sys.exit(main())
