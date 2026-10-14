# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Web board view: serves the page, runs a twin per upload, and streams its
state over a WebSocket.

    py -m tools.pp_web [--port 8080] [--elf firmware.elf] [--board ev15n46a]

Messages to the page: {"type": "status"|"pins"|"uart"|"board"|"clicks",
...}; "pins" maps a PORT group ("a", "b") to 32 [driven-high, driven-low]
fractions of the last frame, "levels" each group's pad levels as a
32-bit mask (whoever drives them); "board" carries the board and the Click
catalog; "clicks" maps each header slot ("ext1") to its Click and control
values, or null.
Messages from the page: {"type": "button", "id": "sw0"|"reset",
"pressed": bool}, {"type": "uart", "data": str}, {"type": "click",
"slot": "ext1", "click": "thermo8"|null} (plugs or unplugs a Click: the
twin restarts with it, like a power cycle) and {"type": "control",
"slot": "ext1", "id": "temperature", "value": 31.5} (live). POST
/api/firmware with the ELF as the body (re)starts the twin.

Local, single-user prototype: one twin shared by every open page.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import tempfile
from pathlib import Path

from aiohttp import WSMsgType, web

from .session import Session

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
BOARDS = HERE / "boards"
MAX_ELF = 4 * 1024 * 1024
CLICKS = json.loads((HERE / "clicks.json").read_text(encoding="utf-8"))


class App:
    def __init__(self, board: str, elf: Path | None):
        self.board_dir = BOARDS / board
        self.board = json.loads((self.board_dir / "board.json").read_text(encoding="utf-8"))
        self.clients: set[web.WebSocketResponse] = set()
        self.session: Session | None = None
        self.elf = elf
        self.tmp = Path(tempfile.mkdtemp(prefix="pp_web_"))
        self.uart_log = ""
        self.lock = asyncio.Lock()
        self.slots = [h["slot"] for h in self.board["headers"] if "slot" in h]
        # slot -> {"id": click, "values": {control: value}} or None
        self.clicks: dict[str, dict | None] = {s: None for s in self.slots}

    def machine(self) -> str:
        opts = [f"{slot}={c['id']}" for slot, c in self.clicks.items() if c]
        return ",".join([self.board["machine"], *opts])

    async def plug(self, slot: str, click: str | None) -> None:
        if slot not in self.clicks or (click and click not in CLICKS):
            return
        self.clicks[slot] = click and {
            "id": click,
            "values": {c["id"]: c["default"] for c in CLICKS[click]["controls"]}}
        self.emit({"type": "clicks", "clicks": self.clicks})
        if self.elf:
            await self.run(self.elf)

    async def control(self, slot: str, cid: str, value: float) -> None:
        c = self.clicks.get(slot)
        ctl = c and next((x for x in CLICKS[c["id"]]["controls"]
                          if x["id"] == cid), None)
        if not ctl:
            return
        value = min(max(float(value), ctl["min"]), ctl["max"])
        c["values"][cid] = value
        self.emit({"type": "clicks", "clicks": self.clicks})
        if self.session and self.session.qmp:
            try:
                await self.session.set_click(slot, ctl["property"],
                                             round(value * ctl["scale"]))
            except (RuntimeError, ConnectionError):
                pass                    # restarting; run() applies it

    def emit(self, msg: dict) -> None:
        if msg["type"] == "uart":
            self.uart_log = (self.uart_log + msg["data"])[-20000:]
        data = json.dumps(msg)
        for ws in list(self.clients):
            asyncio.create_task(self._send(ws, data))

    async def _send(self, ws: web.WebSocketResponse, data: str) -> None:
        try:
            await ws.send_str(data)
        except ConnectionError:
            self.clients.discard(ws)

    async def run(self, elf: Path) -> None:
        async with self.lock:
            if self.session:
                await self.session.stop()
            self.uart_log = ""
            self.emit({"type": "reset"})
            self.elf = elf
            self.session = Session(self.machine(), elf, self.emit)
            try:
                await self.session.start()
                for slot, c in self.clicks.items():
                    for ctl in CLICKS[c["id"]]["controls"] if c else ():
                        await self.session.set_click(
                            slot, ctl["property"],
                            round(c["values"][ctl["id"]] * ctl["scale"]))
            except Exception as e:      # report to the page, keep serving
                self.emit({"type": "status", "state": "error", "error": str(e)})

    # -- handlers ------------------------------------------------------

    async def index(self, request: web.Request) -> web.StreamResponse:
        return web.FileResponse(STATIC / "index.html")

    async def board_file(self, request: web.Request) -> web.StreamResponse:
        name = request.match_info["name"]
        path = (self.board_dir / name).resolve()
        if path.parent != self.board_dir.resolve() or not path.is_file():
            raise web.HTTPNotFound()
        return web.FileResponse(path)

    async def firmware(self, request: web.Request) -> web.Response:
        data = await request.content.read(MAX_ELF + 1)
        if len(data) > MAX_ELF or data[:4] != b"\x7fELF":
            raise web.HTTPBadRequest(text="expected an ELF file up to 4 MiB")
        elf = self.tmp / "firmware.elf"
        elf.write_bytes(data)
        await self.run(elf)
        return web.json_response({"ok": True})

    async def ws(self, request: web.Request) -> web.WebSocketResponse:
        ws = web.WebSocketResponse(heartbeat=20)
        await ws.prepare(request)
        self.clients.add(ws)
        await ws.send_str(json.dumps({"type": "board", "board": self.board,
                                      "catalog": CLICKS}))
        await ws.send_str(json.dumps({"type": "clicks", "clicks": self.clicks}))
        if self.session:
            await ws.send_str(json.dumps({"type": "status", "state": "running",
                                          "elf": self.session.elf.name}))
            await ws.send_str(json.dumps({"type": "uart", "data": self.uart_log}))
            await ws.send_str(json.dumps({"type": "pins",
                                          "pins": self.session.pins}))
            await ws.send_str(json.dumps({"type": "levels",
                                          "levels": self.session.levels}))
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                continue
            req = json.loads(msg.data)
            if req.get("type") == "click":
                await self.plug(str(req.get("slot")), req.get("click"))
                continue
            if req.get("type") == "control":
                await self.control(str(req.get("slot")), str(req.get("id")),
                                   req.get("value", 0))
                continue
            if not self.session:
                continue
            if req.get("type") == "button":
                await self.session.press(str(req.get("id")),
                                         bool(req.get("pressed")))
            elif req.get("type") == "uart":
                await self.session.uart_send(str(req.get("data", "")))
        self.clients.discard(ws)
        return ws

    async def on_startup(self, app: web.Application) -> None:
        if self.elf:
            await self.run(self.elf)

    async def on_cleanup(self, app: web.Application) -> None:
        if self.session:
            await self.session.stop()


def make_app(board: str = "ev15n46a", elf: Path | None = None) -> web.Application:
    state = App(board, elf)
    app = web.Application(client_max_size=MAX_ELF + 1024)
    app.add_routes([
        web.get("/", state.index),
        web.get("/board/{name}", state.board_file),
        web.post("/api/firmware", state.firmware),
        web.get("/ws", state.ws),
        web.static("/static", STATIC),
    ])
    app.on_startup.append(state.on_startup)
    app.on_cleanup.append(state.on_cleanup)
    app["state"] = state
    return app


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="pp_web", description=__doc__.split("\n")[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--board", default="ev15n46a")
    ap.add_argument("--elf", type=Path, help="firmware to start with")
    args = ap.parse_args(argv)
    web.run_app(make_app(args.board, args.elf), host=args.host, port=args.port)
    return 0
