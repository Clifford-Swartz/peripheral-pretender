# Copyright (c) 2026 Clifford Swartz.
"""The live bench, for the MCP tools: the engine (tools/pp_web) that the
VS Code extension runs, found through the file each engine leaves in
~/.pp_web/engines, else one started here (on PP_WEB_BENCH_DIR's workspace;
the extension then shows that one, and it stops with this server).
PP_BENCH_URL names an engine outright. Plain HTTP on 127.0.0.1; the calls
block, so the tools run them in a thread."""

from __future__ import annotations

import atexit
import json
import os
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

from ..pp.qemu import PROJECT_ROOT

START_S = 40.0


def engines_dir() -> Path:
    """Where engines say where they listen (tools/pp_web/server.py)"""
    return Path(os.environ.get("PP_WEB_ENGINES") or
                Path.home() / ".pp_web" / "engines")


class BenchError(Exception):
    pass


def _free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Bench:
    def __init__(self):
        self.url: str | None = None
        self.proc: subprocess.Popen | None = None
        self.started_here = False

    # -- finding the engine ------------------------------------------------

    def _alive(self, url: str) -> bool:
        try:
            with urllib.request.urlopen(f"{url}/api/state", timeout=2):
                return True
        except (OSError, ValueError):
            return False

    def _announced(self) -> str | None:
        """The newest engine that says where it listens and answers"""
        try:
            files = sorted(engines_dir().glob("*.json"),
                           key=lambda p: p.stat().st_mtime, reverse=True)
        except OSError:
            return None
        for f in files:
            try:
                url = json.loads(f.read_text(encoding="utf-8"))["url"]
            except (OSError, ValueError, KeyError):
                continue
            if self._alive(url):
                return url
            f.unlink(missing_ok=True)       # an engine that is gone
        return None

    def _start(self) -> str:
        """An engine of our own on the default workspace (~/.pp_web)"""
        port = _free_port()
        self.proc = subprocess.Popen(
            [sys.executable, "-m", "tools.pp_web", "--port", str(port)],
            cwd=PROJECT_ROOT, stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        url = f"http://127.0.0.1:{port}"
        deadline = time.monotonic() + START_S
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise BenchError("the engine (py -m tools.pp_web) exited at "
                                 "once; run it by hand to see why")
            if self._alive(url):
                self.started_here = True
                return url
            time.sleep(0.25)
        raise BenchError("the engine did not answer in time")

    def base(self) -> str:
        if self.url and self._alive(self.url):
            return self.url
        self.url = os.environ.get("PP_BENCH_URL") or self._announced() or \
            self._start()
        return self.url

    def close(self) -> None:
        """Stop an engine started here, with its QEMUs"""
        if self.proc and self.proc.poll() is None:
            if sys.platform == "win32":
                subprocess.run(["taskkill", "/T", "/F", "/PID",
                                str(self.proc.pid)], capture_output=True)
            else:
                self.proc.terminate()
        self.proc = None

    # -- talking to it --------------------------------------------------------

    def call(self, method: str, path: str, body=None, *, raw: bytes | None = None,
             timeout: float = 60.0):
        url = self.base() + path
        data, headers = None, {}
        if raw is not None:
            data = raw
        elif body is not None:
            data = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(url, data=data, method=method,
                                     headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as f:
                return json.loads(f.read() or b"null")
        except urllib.error.HTTPError as e:
            raise BenchError(e.read().decode(errors="replace") or
                             f"HTTP {e.code}") from None
        except OSError as e:
            raise BenchError(f"the engine at {self.base()} did not answer: "
                             f"{e}") from None

    def state(self) -> dict:
        return self.call("GET", "/api/bench")

    def request(self, msg: dict) -> dict:
        """A message as the page sends it; the workspace after it"""
        return self.call("POST", "/api/request", msg)

    def board(self, twin: str | None) -> dict:
        """The summary of board @twin (the selected one without)"""
        st = self.state()
        boards = st["boards"]
        if not boards:
            raise BenchError("no board on the bench: add one "
                             "(bench_add_board) or set up a template")
        key = twin or st["selected"]
        b = next((b for b in boards if b["key"] == key), None)
        if not b:
            raise BenchError(f"no board {key!r}; the bench has "
                             f"{[b['key'] for b in boards]}")
        return b


BENCH = Bench()
atexit.register(BENCH.close)
