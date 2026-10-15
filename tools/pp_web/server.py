# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Web board view: serves the page, runs a twin per upload, and streams its
state over a WebSocket.

    py -m tools.pp_web [--port 8080] [--elf firmware.elf] [--board ev15n46a]
                       [--watch <project folder or ELF>]

--watch follows a firmware project: whenever a newer ELF appears (an
MPLAB for VS Code build in out/, an MPLAB X build in dist/, or the file
itself), the twin restarts with it.

Messages to the page: {"type": "status"|"pins"|"uart"|"board"|"clicks",
...}; "pins" maps a PORT group ("a", "b") to 32 [driven-high, driven-low]
fractions of the last frame, "levels" each group's pad levels as a
32-bit mask (whoever drives them); "clickstate" maps "<slot>.<property>" to
the value of each property a plugged Click shows (display, sound); "board" carries the board, the Click
catalog and the adapters; "clicks" maps each header slot ("ext1") to its Click and control
values, or null.
Messages from the page: {"type": "select-board", "board": "<id>"}
(switches boards), {"type": "button", "id": "sw0"|"reset",
"pressed": bool}, {"type": "uart", "data": str}, {"type": "click",
"slot": "ext1", "click": "thermo8"|null} (plugs or unplugs a Click: the
twin restarts with it, like a power cycle) and {"type": "control",
"slot": "ext1", "id": "temperature", "value": 31.5} (live) and
{"type": "stick", "slot": "ext1", "id": "stick", "x": .5, "y": -.2,
"pressed": false} (a joystick or a tilt, live; see stick_values()). POST
/api/firmware with the ELF as the body (re)starts the twin.

The parts bench (bench.py): the page sends {"type": "bench", "bench":
{...}} (the whole bench, replaced) and {"type": "bench-prop", "id": "sw1",
"prop": "pressed", "value": true} (one property, live); it gets "bench"
(the bench as stored) and "benchstate" (the solved circuit: parts, pads,
nets, warnings). Each board's bench is kept in ~/.pp_web/bench-<board>.json
(PP_WEB_BENCH_DIR elsewhere); --bench loads one from a file.

Local, single-user prototype: one twin shared by every open page.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import math
import os
import re
import shutil
import tempfile
from pathlib import Path

from aiohttp import WSMsgType, web

from . import bench as benchlib
from .dgiview import DgiView
from .session import PORT_PATH, Session
from tools.pp import circuitpython, fatfs

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
BOARDS = HERE / "boards"
MAX_ELF = 4 * 1024 * 1024
ROOT = HERE.parents[1]
WATCH_S = 1.0
# Build output: any .elf under an out/ (MPLAB for VS Code) or dist/
# (MPLAB X) folder of the watched project
WATCH_DIRS = {"out", "dist"}
WATCH_SKIP = {"node_modules", "__pycache__", "venv"}
DEVICES = ROOT / "devices"


def bench_dir() -> Path:
    return Path(os.environ.get("PP_WEB_BENCH_DIR") or
                Path.home() / ".pp_web")


def device_pads(device: str | None) -> tuple[list[str], dict]:
    """The pads of @device (devices/<device>.json), and per pad the ADC
    channels on it: {pad: [(QOM path of the ADC, channel)]}."""
    path = DEVICES / f"{device}.json"
    if not device or not path.is_file():
        return [], {}
    dev = json.loads(path.read_text(encoding="utf-8"))
    pads, ain = set(), {}
    for sig in dev["signals"]:
        if not re.fullmatch(r"P[A-Z]\d+", sig["pad"]):
            continue                    # RESET_N, VDD...
        pads.add(sig["pad"])
        if sig["instance"].startswith("ADC") and \
                sig["group"] in ("AIN", "AINP"):
            ain.setdefault(sig["pad"], []).append(
                (f"/machine/soc/{sig['instance'].lower()}", sig["index"]))
        elif sig["instance"] == "SDADC" and sig["group"] in ("INN", "INP"):
            # The SDADC's "ain": 2n is INNn, 2n + 1 is INPn
            ain.setdefault(sig["pad"], []).append(
                ("/machine/soc/sdadc",
                 2 * sig["index"] + (sig["group"] == "INP")))
    key = lambda p: (p[1], int(p[2:]))                  # noqa: E731
    return sorted(pads, key=key), ain


def newest_elf(target: Path) -> Path | None:
    """The newest build output of a project folder, or @target itself."""
    if target.is_file():
        return target
    best, best_mtime = None, -1.0
    for top, dirs, files in os.walk(target):
        # Not build output, and slow to walk (a workspace with an extension
        # in it has node_modules and a downloaded VS Code)
        dirs[:] = [d for d in dirs if d not in WATCH_SKIP and
                   not d.startswith(".")]
        rel = Path(top).relative_to(target).parts
        if not set(rel) & WATCH_DIRS:
            continue
        for name in files:
            if name.endswith(".elf"):
                try:
                    mtime = os.stat(os.path.join(top, name)).st_mtime
                except OSError:
                    continue            # being rewritten
                if mtime > best_mtime:
                    best, best_mtime = Path(top) / name, mtime
    return best
CLICKS = json.loads((HERE / "clicks.json").read_text(encoding="utf-8"))
ADAPTERS = json.loads((HERE / "adapters.json").read_text(encoding="utf-8"))


def watched(click: dict) -> list[tuple[str, ...]]:
    """QOM properties of a catalog Click that the page shows: its display,
    its sound (frequency, duty). A display panel's picture comes as
    (property, generation property): it is fetched only when the
    generation moves."""
    props = []
    if "display" in click:
        d = click["display"]
        props.append((d["property"], d["gen_property"]) if "gen_property" in d
                     else (d["property"],))
    if "sound" in click:
        props += [(click["sound"]["property"],),
                  (click["sound"]["duty_property"],)]
    return props


def stick_values(stick: dict, x: float, y: float) -> dict[str, int | str]:
    """QOM property values for a catalog stick at (@x, @y) in the unit
    circle. "stick": the axes scaled to "range". "tilt": the Click leans
    toward (x, y) by asin(r): an accelerometer at rest reads the opposite
    of gravity, so the axis that dips reads negative, and z (up from the
    Click) the rest of 1 g; "range" is 1 g in the properties' units. With
    "xyz_property" the three go in one "x y z" string, so the sensor
    never samples a half-updated vector."""
    full = stick["range"]
    if stick.get("mode") == "tilt":
        z = math.sqrt(max(0.0, 1 - x * x - y * y))
        g = [round(-x * full), round(-y * full), round(z * full)]
        if "xyz_property" in stick:
            return {stick["xyz_property"]: " ".join(map(str, g))}
        return {stick["x_property"]: g[0], stick["y_property"]: g[1],
                stick["z_property"]: g[2]}
    return {stick["x_property"]: round(x * full),
            stick["y_property"]: round(y * full)}


def slot_watch(slot: str, *prop: str) -> tuple[str, ...]:
    """The Session watch entry of a Click property (property[, generation
    property]) on @slot."""
    return (f"{slot}.{prop[0]}", f"/machine/{slot}-click", *prop)


def part_watch(board: dict) -> list[tuple[str, ...]]:
    """(key, path, property[, generation]) of what the board's own parts
    show or play (board.json "parts": a display panel, NeoPixels, a
    speaker)."""
    out = []
    for part in board.get("parts", []):
        for kind in ("display", "pixels", "audio"):
            d = part.get(kind)
            if d:
                out.append((f"{part['id']}.{d['property']}", part["path"],
                            d["property"],
                            *([d["gen_property"]] if "gen_property" in d
                              else [])))
    return out


def board_list() -> list[dict]:
    """Every board the page can show: id and name, from boards/*/board.json."""
    boards = []
    for path in sorted(BOARDS.glob("*/board.json")):
        b = json.loads(path.read_text(encoding="utf-8"))
        boards.append({"id": b["id"], "name": b["name"]})
    return boards


def pin_groups(board: dict) -> tuple[str, ...]:
    """PORT groups ("a", "b", ...) the page shows: those of the board's
    LEDs and header INT pins, and always A and B."""
    pins = [p["pin"] for p in board["leds"]]
    pins += [h["int_pin"] for h in board["headers"] if "int_pin" in h]
    pins += [p for h in board["headers"] for p in h.get("mikrobus", {}).values()]
    return tuple(sorted({"a", "b"} | {p[1].lower() for p in pins}))


class App:
    def __init__(self, board: str, elf: Path | None,
                 watch: Path | None = None, bench: Path | None = None):
        self.load_board(board)
        if bench:
            self.bench.set(json.loads(bench.read_text(encoding="utf-8")))
        self.clients: set[web.WebSocketResponse] = set()
        self.session: Session | None = None
        self.elf = elf
        self.watch = watch
        self.tmp = Path(tempfile.mkdtemp(prefix="pp_web_"))
        self.uart_log = ""
        self.lock = asyncio.Lock()
        self.dgi: DgiView | None = None
        self.dgi_wanted: list[int] = []

    def load_board(self, board: str) -> None:
        """Make @board (a folder under boards/) current; no Clicks plugged."""
        self.board_dir = BOARDS / board
        self.board = json.loads((self.board_dir / "board.json").read_text(encoding="utf-8"))
        self.slots = [h["slot"] for h in self.board["headers"] if "slot" in h]
        # slot -> {"id": click, "values": {control: value}} or None
        self.clicks: dict[str, dict | None] = {s: None for s in self.slots}
        self.drive = None               # the CIRCUITPY image, if any
        cp = self.board.get("circuitpython")
        if cp:
            self.drive = Path(tempfile.mkdtemp(prefix="pp_drive_")) / \
                "circuitpy.img"
            fatfs_files = fatfs.folder_files(self.board_dir / cp["drive"])
            self.drive.write_bytes(fatfs.build_image(fatfs_files))
        self.pads, self.ain = device_pads(self.board.get("device"))
        saved = bench_dir() / f"bench-{board}.json"
        try:
            data = json.loads(saved.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = {}
        self.holes = benchlib.header_holes(self.board)
        self.bench = benchlib.Bench(data, vdd=self.board.get("vdd", 3.3),
                                    pad_max_ma=self.board.get("pad_max_ma",
                                                              20.0),
                                    holes=self.holes)
        self.bench_view = ""
        self.bench_frame_last: dict = {}
        self.bench_applied: dict[str, str] = {}     # PORT group -> value
        self.ain_applied: dict[str, dict[int, int]] = {}    # ADC -> ch: mV

    # -- the parts bench ----------------------------------------------

    def bench_pins(self) -> list[dict]:
        """The pads the page offers for wiring, with their board names."""
        names: dict[str, list[str]] = {}
        for label, pad in self.board.get("pin_labels", {}).items():
            names.setdefault(pad, []).append(label)
        for led in self.board["leds"]:
            names.setdefault(led["pin"], []).append(led["id"])
        return [{"pad": p, "label": " ".join(names.get(p, [])),
                 "adc": p in self.ain} for p in self.pads]

    def bench_groups(self) -> tuple[str, ...]:
        return tuple(sorted({benchlib.pad_ref(p)[0] for p in self.bench.pads}))

    def bench_message(self) -> dict:
        return {"type": "bench", "bench": self.bench.data}

    def save_bench(self) -> None:
        try:
            d = bench_dir()
            d.mkdir(parents=True, exist_ok=True)
            data = {"parts": [{k: v for k, v in p.items() if k != "pressed"}
                              for p in self.bench.data["parts"]],
                    "wires": self.bench.data["wires"],
                    "anchors": self.bench.data["anchors"]}
            (d / f"bench-{self.board['id']}.json").write_text(
                json.dumps(data, indent=1), encoding="utf-8")
        except OSError:
            pass

    def _bench_session(self) -> None:
        """Tell the running twin what the bench needs polled."""
        if self.session:
            groups = self.bench_groups()
            self.session.bench_active = not self.bench.empty
            self.session.bench_groups = groups
            self.session.groups = tuple(sorted(set(self.session.groups) |
                                               set(groups)))

    async def set_bench(self, data: dict) -> None:
        self.bench.set(data)
        self.save_bench()
        self.emit(self.bench_message())
        self._bench_session()
        await self.bench_frame(None)

    async def bench_prop(self, pid: str, prop: str, value) -> None:
        """One property of a bench part, live (a button held, a knob)."""
        part = next((p for p in self.bench.data["parts"] if p["id"] == pid),
                    None)
        if not part or prop not in benchlib.PARTS[part["type"]]["props"]:
            return
        part[prop] = value
        self.bench.set(self.bench.data)
        if prop != "pressed":
            self.save_bench()
        self.emit(self.bench_message())
        await self.bench_frame(None)

    async def bench_frame(self, frame: dict | None) -> None:
        """Solve the bench for the twin's pads (@frame from the session,
        or the last one), drive the input pads and ADC channels it feeds,
        and send the page what changed."""
        if frame is None:
            frame = self.bench_frame_last if self.session else {}
        self.bench_frame_last = frame
        pins, pulls = frame.get("pins", {}), frame.get("pulls", {})
        analog = frame.get("analog", {})
        ohms = {a["pad"]: a.get("ohms", 100.0)
                for a in self.board.get("analog_out", [])}
        pads = {}
        for pad in self.bench.pads:
            g, bit = benchlib.pad_ref(pad)
            group = pins.get(g) or []
            hi, lo = group[bit] if bit < len(group) else (0, 0)
            mask, up = pulls.get(g, (0, 0))
            pull = (1 if up >> bit & 1 else -1) if mask >> bit & 1 else 0
            mv = analog.get(pad, -1)
            pads[pad] = benchlib.Pad(high=hi, low=lo, pull=pull,
                                     analog_mv=mv if mv >= 0 else None,
                                     analog_ohms=ohms.get(pad, 100.0))
        res = self.bench.solve(pads)
        if self.session and self.session.qmp:
            await self._bench_apply(res)
        view = json.dumps({"type": "benchstate", "parts": res.parts,
                           "pads": res.pads, "nets": res.nets,
                           "warnings": res.warnings})
        if view != self.bench_view:
            self.bench_view = view
            self.emit(json.loads(view))

    async def _bench_apply(self, res: benchlib.Result) -> None:
        """Input pads read the level the bench settles them at; ADC
        channels read their pad's voltage."""
        groups = set(self.bench_groups()) | set(self.bench_applied)
        for g, (mask, level) in benchlib.pad_drives(res.inputs,
                                                    groups).items():
            val = f"{mask:x} {level:x}"
            if self.bench_applied.get(g, "0 0") == val:
                continue
            self.bench_applied[g] = val
            try:
                await self.session.set_prop(PORT_PATH, f"p{g}-bench", val)
            except (RuntimeError, ConnectionError):
                pass                    # no such group, or restarting
        want: dict[str, dict[int, int]] = {}
        for pad, volts in res.volts.items():
            for path, ch in self.ain.get(pad, []):
                want.setdefault(path, {})[ch] = round(volts * 1000)
        for path in set(want) | set(self.ain_applied):
            new, old = want.get(path, {}), self.ain_applied.get(path, {})
            if new == old:
                continue
            items = {**{ch: -1 for ch in old}, **new}
            self.ain_applied[path] = new
            try:
                await self.session.set_prop(
                    path, "ain", ",".join(f"{c}={mv}"
                                          for c, mv in sorted(items.items())))
            except (RuntimeError, ConnectionError):
                pass                    # no ADC model on this device

    def circuitpython_elf(self) -> Path | None:
        """The board's stock CircuitPython (from the cache; fetched once
        with py -m tools.pp.circuitpython fetch)."""
        cp = self.board.get("circuitpython")
        if not cp:
            return None
        elf = circuitpython.elf_path(cp["version"], cp["board_id"])
        return elf if elf.is_file() else None

    async def default_firmware(self) -> Path | None:
        """What the board runs until the user gives it firmware: its demo
        (board.json "demo_elf", if built), else its stock CircuitPython,
        downloaded once into the cache."""
        demo = self.board.get("demo_elf")
        if demo and (ROOT / demo).is_file():
            return ROOT / demo
        cp = self.board.get("circuitpython")
        if not cp or self.circuitpython_elf():
            return self.circuitpython_elf()
        self.emit({"type": "status", "state": "idle",
                   "text": f"downloading CircuitPython {cp['version']} "
                           "(once)..."})
        try:
            return await asyncio.get_running_loop().run_in_executor(
                None, circuitpython.fetch, cp["version"], cp["board_id"])
        except (OSError, ValueError) as err:
            self.emit({"type": "status", "state": "idle",
                       "text": f"no CircuitPython: download failed ({err})"})
            return None

    def files_message(self) -> dict:
        """The CIRCUITPY drive for the page: every file, and code.py."""
        if not self.drive:
            return {"type": "files", "files": None}
        try:
            fat = fatfs.FatImage(bytearray(self.drive.read_bytes()))
            files = [{"path": p, "size": e.size, "dir": e.is_dir}
                     for p, e in fat.walk()]
            code = fat.read("code.py").decode("utf-8", "replace") \
                if fat.lookup("code.py") else ""
        except (OSError, fatfs.FatError) as e:
            return {"type": "files", "files": [], "error": str(e)}
        return {"type": "files", "files": files, "code": code}

    async def save_file(self, path: str, text: str) -> None:
        """Write @path on the CIRCUITPY drive; the twin restarts with it
        (the twin holds the image while it runs)."""
        if not self.drive or not path:
            return
        async with self.lock:
            await self._stop_dgi()
            if self.session:
                await self.session.stop()
                self.session = None
            data = bytearray(self.drive.read_bytes())
            fatfs.FatImage(data).write(path, text.encode("utf-8"))
            self.drive.write_bytes(data)
        self.emit(self.files_message())
        if self.elf:
            await self.run(self.elf)

    def board_message(self) -> dict:
        return {"type": "board", "board": self.board, "boards": board_list(),
                "catalog": CLICKS, "adapters": ADAPTERS,
                "bench_lib": {t: {"name": d["name"],
                                  "terminals": d["terminals"],
                                  "props": d["props"]}
                              for t, d in benchlib.PARTS.items()},
                "bench_pins": self.bench_pins(),
                "bench_holes": self.holes}

    async def select_board(self, board: str) -> None:
        """Switch boards: the twin stops, the page redraws, and the board's
        demo firmware (board.json "demo_elf", if built) starts."""
        if board == self.board["id"] or not (BOARDS / board / "board.json").is_file():
            # Nothing to switch: the page still waits for an answer
            self.emit(self.board_message())
            self.emit(self.status_message())
            return
        t0 = asyncio.get_running_loop().time()

        def step(what: str) -> None:
            print(f"[pp_web] board {board}: {what} "
                  f"({asyncio.get_running_loop().time() - t0:.2f} s)",
                  flush=True)
        step("switching")
        async with self.lock:
            await self._stop_dgi()
            step("DGI stopped")
            if self.session:
                await self.session.stop()
                self.session = None
            step("twin stopped")
            self.load_board(board)
            self.uart_log = ""
            self.elf = None
        self.emit({"type": "reset"})
        self.emit(self.board_message())
        self.emit({"type": "clicks", "clicks": self.clicks})
        self.emit(self.files_message())
        self.emit(self.bench_message())
        await self.bench_frame(None)
        self.elf = await self.default_firmware()
        step(f"starting {self.elf.name if self.elf else 'no firmware'}")
        if self.elf:
            await self.run(self.elf)
            step("running")
        elif self.board.get("circuitpython"):
            pass                        # default_firmware said why
        else:
            self.emit({"type": "status", "state": "idle",
                       "text": "no firmware: drop an ELF built for this board"})

    def status_message(self) -> dict:
        if self.session:
            return {"type": "status", "state": "running",
                    "elf": self.session.elf.name}
        return {"type": "status", "state": "idle",
                "text": "no firmware: drop an ELF built for this board"}

    def machine(self) -> str:
        opts = [f"{slot}={c['id']}" for slot, c in self.clicks.items() if c]
        if self.board.get("dgi"):
            opts.append("dgi=dgi")
        return ",".join([self.board["machine"], *opts])

    def click_watch(self) -> list[tuple[str, ...]]:
        """(key, path, property[, generation property]) of what each
        plugged Click and the board's own parts show, polled live."""
        return [slot_watch(slot, *prop) for slot, c in self.clicks.items()
                if c for prop in watched(CLICKS[c["id"]])] + \
            part_watch(self.board)

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
        btn = c and next((x for x in CLICKS[c["id"]].get("buttons", [])
                          if x["id"] == cid), None)
        if btn and self.session and self.session.qmp:
            try:                        # a button on the Click: held or not
                await self.session.set_click(slot, btn["property"], bool(value))
            except (RuntimeError, ConnectionError):
                pass
            return
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

    async def part_stick(self, pid: str, sid: str, x: float,
                         y: float) -> None:
        """A stick on a board part (board.json "parts") moved: the IMU's
        tilt goes to the part's QOM path and its "also" paths."""
        part = next((p for p in self.board.get("parts", [])
                     if p["id"] == pid), None)
        st = part and next((k for k in part.get("sticks", [])
                            if k["id"] == sid), None)
        if not st or not (self.session and self.session.qmp):
            return
        r = math.hypot(x, y)
        if r > 1:
            x, y = x / r, y / r
        for path in [part["path"], *part.get("also", [])]:
            for prop, value in stick_values(st, x, y).items():
                try:
                    await self.session.set_prop(path, prop, value)
                except (RuntimeError, ConnectionError):
                    pass                # absent (an optional breakout)

    async def stick(self, slot: str, sid: str, x: float, y: float,
                    pressed: bool | None) -> None:
        """A stick on the Click in @slot moved to (@x, @y), both -1..1 (y
        up, inside the unit circle): a joystick's axes, or, in "tilt"
        mode, which way the Click leans (gravity on its x, y, z)."""
        c = self.clicks.get(slot)
        st = c and next((k for k in CLICKS[c["id"]].get("sticks", [])
                         if k["id"] == sid), None)
        if not st or not (self.session and self.session.qmp):
            return
        r = math.hypot(x, y)
        if r > 1:
            x, y = x / r, y / r
        values = stick_values(st, x, y)
        if pressed is not None and "press_property" in st:
            values[st["press_property"]] = bool(pressed)
        try:
            for prop, value in values.items():
                await self.session.set_click(slot, prop, value)
        except (RuntimeError, ConnectionError):
            pass

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

    async def _stop_dgi(self) -> None:
        if getattr(self, "dgi", None):
            await self.dgi.stop()
        self.dgi = None

    async def run(self, elf: Path) -> None:
        async with self.lock:
            await self._stop_dgi()
            if self.session:
                await self.session.stop()
            self.uart_log = ""
            self.emit({"type": "reset"})
            self.elf = elf
            self.bench_applied, self.ain_applied = {}, {}
            self.bench_frame_last = {}
            groups = self.bench_groups()
            self.session = Session(self.machine(), elf, self.emit,
                                   groups=tuple(sorted(
                                       set(pin_groups(self.board)) |
                                       set(groups))),
                                   watch=self.click_watch(),
                                   drive=self.drive,
                                   bench=self.bench_frame,
                                   bench_active=not self.bench.empty,
                                   bench_groups=groups,
                                   bench_analog=tuple(
                                       (a["pad"], a["path"], a["property"])
                                       for a in self.board.get("analog_out",
                                                               [])),
                                   dgi=bool(self.board.get("dgi")))
            try:
                await self.session.start()
                if self.session.dgi_port:
                    self.dgi = DgiView(self.session.dgi_port, self.emit)
                    await self.dgi.start()
                    self.dgi.set_wanted(self.dgi_wanted)
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
        await ws.send_str(json.dumps(self.board_message()))
        await ws.send_str(json.dumps({"type": "clicks", "clicks": self.clicks}))
        await ws.send_str(json.dumps(self.files_message()))
        await ws.send_str(json.dumps(self.bench_message()))
        if getattr(self, "dgi", None) and self.dgi.info:
            await ws.send_str(json.dumps(self.dgi.info))
        if self.bench_view:
            await ws.send_str(self.bench_view)
        if self.session:
            await ws.send_str(json.dumps(self.status_message()))
            await ws.send_str(json.dumps({"type": "uart", "data": self.uart_log}))
            await ws.send_str(json.dumps({"type": "pins",
                                          "pins": self.session.pins}))
            await ws.send_str(json.dumps({"type": "levels",
                                          "levels": self.session.levels}))
            await ws.send_str(json.dumps({"type": "clickstate",
                                          "state": self.session.click_state}))
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                continue
            req = json.loads(msg.data)
            if req.get("type") == "select-board":
                await self.select_board(str(req.get("board")))
                continue
            if req.get("type") == "click":
                await self.plug(str(req.get("slot")), req.get("click"))
                continue
            if req.get("type") == "stick" and req.get("part"):
                await self.part_stick(str(req.get("part")), str(req.get("id")),
                                      float(req.get("x", 0)),
                                      float(req.get("y", 0)))
                continue
            if req.get("type") == "stick":
                await self.stick(str(req.get("slot")), str(req.get("id")),
                                 float(req.get("x", 0)), float(req.get("y", 0)),
                                 req.get("pressed"))
                continue
            if req.get("type") == "save-file":
                await self.save_file(str(req.get("path", "")),
                                     str(req.get("text", "")))
                continue
            if req.get("type") == "dgi":
                self.dgi_wanted = [int(i) for i in req.get("enable", [])]
                if getattr(self, "dgi", None):
                    self.dgi.set_wanted(self.dgi_wanted)
                continue
            if req.get("type") == "bench":
                await self.set_bench(req.get("bench") or {})
                continue
            if req.get("type") == "bench-prop":
                await self.bench_prop(str(req.get("id")), str(req.get("prop")),
                                      req.get("value"))
                continue
            if req.get("type") == "control":
                await self.control(str(req.get("slot")), str(req.get("id")),
                                   req.get("value", 0))
                continue
            if not self.session:
                continue
            if req.get("type") == "button":
                btn = next((b for b in self.board["buttons"]
                            if b["action"] == req.get("id")), {})
                await self.session.press(str(req.get("id")),
                                         bool(req.get("pressed")),
                                         btn.get("property"))
            elif req.get("type") == "uart":
                await self.session.uart_send(str(req.get("data", "")))
        self.clients.discard(ws)
        return ws

    async def _watch(self) -> None:
        """Run each new build of the watched project once it is complete
        (unchanged for one poll); a copy, so the linker can overwrite it."""
        seen = pending = None
        # The walk can take a while on a big workspace: off the event loop
        first = await asyncio.to_thread(newest_elf, self.watch)
        if self.elf is None and first:
            seen = (first, first.stat().st_mtime_ns, first.stat().st_size)
            await self._load(first)
        elif first:
            seen = (first, first.stat().st_mtime_ns, first.stat().st_size)
        while True:
            await asyncio.sleep(WATCH_S)
            elf = await asyncio.to_thread(newest_elf, self.watch)
            try:
                now = elf and (elf, elf.stat().st_mtime_ns, elf.stat().st_size)
            except OSError:
                continue                # being rewritten
            if not now or now == seen:
                pending = None
            elif now == pending:
                seen, pending = now, None
                await self._load(elf)
            else:
                pending = now

    async def _load(self, elf: Path) -> None:
        if elf.read_bytes()[:4] != b"\x7fELF":
            return
        dest = self.tmp / "watch" / elf.name
        dest.parent.mkdir(exist_ok=True)
        shutil.copyfile(elf, dest)
        self.emit({"type": "status", "state": "idle",
                   "text": f"new build: {elf.name}"})
        await self.run(dest)

    async def on_startup(self, app: web.Application) -> None:
        await self.bench_frame(None)
        # A watched project's existing build wins (the watcher runs it);
        # otherwise the board's default firmware
        if self.elf is None and not (
                self.watch and await asyncio.to_thread(newest_elf, self.watch)):
            self.elf = await self.default_firmware()
        if self.elf:
            await self.run(self.elf)
        if self.watch:
            self.watcher = asyncio.create_task(self._watch())

    async def on_cleanup(self, app: web.Application) -> None:
        if getattr(self, "watcher", None):
            self.watcher.cancel()
        await self._stop_dgi()
        if self.session:
            await self.session.stop()


@web.middleware
async def revalidate(request: web.Request, handler) -> web.StreamResponse:
    """The page and its scripts are checked on every load (a 304 when they
    have not changed), so an updated engine never runs a stale page."""
    resp = await handler(request)
    if request.path == "/" or request.path.startswith("/static/"):
        resp.headers["Cache-Control"] = "no-cache"
    return resp


async def part_image(request: web.Request) -> web.StreamResponse:
    """A Click's or adapter's rendered image (clicks/<id>/top.png)."""
    root = (HERE / request.match_info["kind"]).resolve()
    path = (root / request.match_info["id"] / request.match_info["name"]).resolve()
    if path.parent.parent != root or path.suffix != ".png" or not path.is_file():
        raise web.HTTPNotFound()
    return web.FileResponse(path)


def make_app(board: str = "ev15n46a", elf: Path | None = None,
             watch: Path | None = None,
             bench: Path | None = None) -> web.Application:
    state = App(board, elf, watch, bench)
    app = web.Application(client_max_size=MAX_ELF + 1024,
                          middlewares=[revalidate])
    app.add_routes([
        web.get("/", state.index),
        web.get("/board/{name}", state.board_file),
        web.post("/api/firmware", state.firmware),
        web.get("/ws", state.ws),
        web.static("/static", STATIC),
        web.get("/{kind:clicks|adapters}/{id}/{name}", part_image),
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
    ap.add_argument("--watch", type=Path, metavar="PATH",
                    help="project folder (or ELF) whose new builds to run")
    ap.add_argument("--bench", type=Path, metavar="JSON",
                    help="parts bench to start with (see bench.py)")
    args = ap.parse_args(argv)
    web.run_app(make_app(args.board, args.elf, args.watch, args.bench),
                host=args.host, port=args.port)
    return 0
