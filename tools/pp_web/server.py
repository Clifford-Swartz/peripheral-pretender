# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Web board view: serves the page, runs a twin per upload, and streams its
state over a WebSocket.

    py -m tools.pp_web [--port 8080] [--elf firmware.elf] [--board ev15n46a]
                       [--watch <project folder or ELF>] [--workspace FILE]

The workspace is what the page shows: a board (or none yet), its Click
boards, its firmware and the parts bench. It is kept in a file
(~/.pp_web/workspace.json, PP_WEB_BENCH_DIR or --workspace), so the engine
starts where it left off; --board picks a board instead. Templates
(templates.json) set up a board, its Clicks, a bench and firmware at once.

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
Messages from the page: {"type": "select-board", "board": "<id>"|null}
(switches boards; null leaves the workspace without one), {"type":
"template", "id": "<template>"}, {"type": "button", "id": "sw0"|"reset",
"pressed": bool}, {"type": "uart", "data": str}, {"type": "click",
"slot": "ext1", "click": "thermo8"|null} (plugs or unplugs a Click: the
twin restarts with it, like a power cycle) and {"type": "control",
"slot": "ext1", "id": "temperature", "value": 31.5} (live) and
{"type": "stick", "slot": "ext1", "id": "stick", "x": .5, "y": -.2,
"pressed": false} (a joystick or a tilt, live; see stick_values()). POST
/api/firmware with the ELF as the body (re)starts the twin, and the
workspace keeps a copy to run next time; POST /api/board {"board": id|null}
switches boards and GET /api/state says which board and firmware run (the
VS Code extension's sidebar).

The parts bench (bench.py): the page sends {"type": "bench", "bench":
{...}} (the whole bench, replaced) and {"type": "bench-prop", "id": "sw1",
"prop": "pressed", "value": true} (one property, live); it gets "bench"
(the bench as stored) and "benchstate" (the solved circuit: parts, pads,
nets, warnings, and "solver": "dc" or "ngspice"). The bench belongs to the
workspace: it stays when the board changes (its wires into the old board's
header holes go). --bench loads one from a file. A bench with a capacitor or an inductor changes in
time: when ngspice is installed and the machine has the co-simulation
port, it runs in the analog host (tools/pp_analog) in lockstep with the
twin, which restarts with it when the circuit changes; live properties (a
button, a switch, a pot) reach it without a restart.

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
import time
from pathlib import Path

from aiohttp import WSMsgType, web

from . import bench as benchlib
from . import timeline as tl
from .dgiview import DgiView
from tools.pp_analog import ngspice as ngspice_lib
from tools.pp_analog.launch import COSIM_MACHINES, HostError, HostProcess
from .session import DETERMINISTIC_ICOUNT, PORT_PATH, Session
from tools.pp import circuitpython, fatfs
from tools.pp import trace as tr

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
BOARDS = HERE / "boards"
MAX_ELF = 4 * 1024 * 1024
ROOT = HERE.parents[1]
WATCH_S = 1.0
FAULT_KINDS = ("short: pad, level (0 GND, 1 VDD); nack: addr (7-bit) or "
               "slot (its I2C Click); miso: host (an SPI host: sercom1, "
               "spi0), value")
# Build output: any .elf under an out/ (MPLAB for VS Code) or dist/
# (MPLAB X) folder of the watched project
WATCH_DIRS = {"out", "dist"}
WATCH_SKIP = {"node_modules", "__pycache__", "venv"}
DEVICES = ROOT / "devices"
TEMPLATES = json.loads((HERE / "templates.json").read_text(encoding="utf-8"))
# A wired Click's pins the twin routes (pp-qemu "wired"); RX/TX are not
WIRED_ROLES = ("an", "rst", "cs", "sck", "miso", "mosi", "pwm", "int",
               "scl", "sda")
BOARD_IMAGE = {".png", ".jpg", ".jpeg", ".svg", ".webp"}


def bench_dir() -> Path:
    return Path(os.environ.get("PP_WEB_BENCH_DIR") or
                Path.home() / ".pp_web")


def engines_dir() -> Path:
    """Where each running engine says where it listens (the MCP server
    finds it there): PP_WEB_ENGINES, else ~/.pp_web/engines"""
    return Path(os.environ.get("PP_WEB_ENGINES") or
                Path.home() / ".pp_web" / "engines")


def sercom_pads(device: str | None) -> dict[str, dict[str, int]]:
    """Per SERCOM, its PAD index on each pad: {"SERCOM1": {"PA16": 0}}."""
    path = DEVICES / f"{device}.json"
    if not device or not path.is_file():
        return {}
    out: dict[str, dict[str, int]] = {}
    for sig in json.loads(path.read_text(encoding="utf-8"))["signals"]:
        if sig["instance"].startswith("SERCOM") and sig["group"] == "PAD":
            out.setdefault(sig["instance"], {})[sig["pad"]] = sig["index"]
    return out


def bus_hosts(device: str | None) -> dict[str, list[str]]:
    """The models that host each bus, as the QOM names them: {"spi":
    ["sercom0", ...], "i2c": [...]} (SERCOMs host both; on AVR SPIn and
    TWIn)."""
    path = DEVICES / f"{device}.json"
    out: dict[str, list[str]] = {"spi": [], "i2c": []}
    if not device or not path.is_file():
        return out
    for inst in json.loads(path.read_text(encoding="utf-8"))["instances"]:
        name = inst["name"]
        if re.fullmatch(r"SERCOM\d+", name):
            out["spi"].append(name.lower())
            out["i2c"].append(name.lower())
        elif re.fullmatch(r"SPI\d+", name):
            out["spi"].append(name.lower())
        elif re.fullmatch(r"TWI\d+", name):
            out["i2c"].append(name.lower())
    return out


def wiring_fits(bus: str, pins: dict[str, str],
                sercoms: dict[str, dict[str, int]]) -> list[str]:
    """The SERCOMs that can drive a Click's SPI or I2C on @pins (as
    pp-qemu's pic32cm_soc_wire_click() looks for them)."""
    fits = []
    for inst, pads in sorted(sercoms.items()):
        if bus == "SPI":
            do, sck = pads.get(pins.get("mosi")), pads.get(pins.get("sck"))
            di = pads.get(pins.get("miso")) if "miso" in pins else -2
            if (do, sck) in ((0, 1), (2, 3), (3, 1), (0, 3)) and \
                    di is not None and di not in (do, sck):
                fits.append(inst)
        elif bus == "I2C":
            if pads.get(pins.get("sda")) == 0 and \
                    pads.get(pins.get("scl")) == 1:
                fits.append(inst)
    return fits


def click_i2c_address(click: str) -> int | None:
    """The 7-bit address of an I2C Click (the catalog's "I2C 0x18")"""
    m = re.match(r"I2C\s+0x([0-9a-fA-F]+)", CLICKS.get(click, {}).get("bus", ""))
    return int(m.group(1), 16) if m else None


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


def build_stamp(elf: Path) -> list | None:
    """Which build @elf is: its path, time and size (as JSON keeps them)"""
    try:
        st = elf.stat()
    except OSError:
        return None
    return [str(elf), st.st_mtime_ns, st.st_size]


CLICKS =json.loads((HERE / "clicks.json").read_text(encoding="utf-8"))
ADAPTERS = json.loads((HERE / "adapters.json").read_text(encoding="utf-8"))


def watched(click: dict) -> list[tuple[str, ...]]:
    """QOM properties of a catalog Click that the page shows: its display,
    its sound (frequency, duty). A display panel's picture comes as
    (property, generation property): it is fetched only when the
    generation moves."""
    props = []
    # Toggles and monitors in the Click's panel (a BLE module's link and
    # what it sent the phone) follow their properties
    props += [(x["property"],) for x in click.get("toggles", [])]
    props += [(x["property"],) for x in click.get("monitors", [])]
    # A swatch shows the Click's own controls: nothing to poll
    if "display" in click and "property" in click["display"]:
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
        boards.append({"id": b["id"], "name": b["name"],
                       "device": b.get("device"), "machine": b["machine"],
                       "image": f"/boards/{b['id']}/{b['image']}"})
    return boards


def template_list() -> list[dict]:
    """The templates the page offers, each with whether its firmware is
    built here (else the board's own firmware runs)."""
    return [{**t, "built": bool(t.get("firmware")) and
             (ROOT / t["firmware"]).is_file()}
            for t in TEMPLATES if (BOARDS / t["board"] / "board.json").is_file()]


def pin_groups(board: dict) -> tuple[str, ...]:
    """PORT groups ("a", "b", ...) the page shows: those of the board's
    LEDs and header INT pins, and always A and B."""
    pins = [p["pin"] for p in board["leds"]]
    pins += [h["int_pin"] for h in board["headers"] if "int_pin" in h]
    pins += [p for h in board["headers"] for p in h.get("mikrobus", {}).values()]
    return tuple(sorted({"a", "b"} | {p[1].lower() for p in pins}))


def _bare(pad: str) -> tuple[str, str]:
    """"b2/PA05" -> ("b2", "PA05"); "PA05" -> ("", "PA05")."""
    key, _, bare = pad.rpartition("/")
    return key, bare


class Twin:
    """One board on the workspace: its Click boards, its firmware and the
    QEMU running it. Its pads and header holes are the bench's
    "pin:<key>/PA05" and "hole:<key>/J801.3"; its messages to the page
    carry "twin": <key>."""

    def __init__(self, app: "App", key: str, board: str,
                 at: list[float] | tuple[float, float] = (0.0, 0.0)):
        self.app, self.key = app, key
        self.board_dir = BOARDS / board
        self.board = json.loads((self.board_dir / "board.json").read_text(
            encoding="utf-8"))
        self.at = [float(at[0]), float(at[1])]
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
        self.sercoms = sercom_pads(self.board.get("device"))
        self.hosts = bus_hosts(self.board.get("device"))
        self.session: Session | None = None
        self.elf: Path | None = None
        self.kept_elf: Path | None = None   # an upload, run again next time
        self.uart_log = ""
        self.uart_total = 0             # characters ever, a mark to wait from
        self.lock = asyncio.Lock()
        self.dgi: DgiView | None = None
        self.dgi_wanted: list[int] = []
        self.frame: dict = {}           # the last bench frame of its session
        self.bench_applied: dict[str, str] = {}     # PORT group -> value
        self.ain_applied: dict[str, dict[int, int]] = {}    # ADC -> ch: mV
        self.wired_running: str | None = None
        # Injected faults (not saved: a test's, not the board's)
        self.faults: list[dict] = []
        self.short_groups: set[str] = set()     # PORT groups shorted
        # Recording (since when, wall clock) and replaying a recording
        self.recording_since: float | None = None
        self.replaying: dict | None = None

    # -- where it is ----------------------------------------------------

    @property
    def view(self) -> dict:
        b = self.board
        return b.get("view_mm") or {"origin": b["origin_mm"],
                                    "size": b["size_mm"]}

    def to_bench(self, at_mm: list[float]) -> list[float]:
        """Board millimetres (y up) to the bench's (y down)."""
        v = self.view
        return [round(at_mm[0] - v["origin"][0] + self.at[0], 3),
                round(v["origin"][1] - at_mm[1] + self.at[1], 3)]

    def holes(self) -> dict[str, dict]:
        """Its header holes on the bench: {end: {"net", "label", "at_mm"
        (board), "at" (bench)}}."""
        out = {}
        for end, h in benchlib.header_holes(self.board).items():
            net = h["net"]
            if net and net.startswith("pin:"):
                net = f"pin:{self.key}/{net[4:]}"
            out[f"hole:{self.key}/{end[5:]}"] = {
                "net": net, "label": h["label"], "at_mm": h["at_mm"],
                "at": self.to_bench(h["at_mm"])}
        return out

    def bench_pins(self) -> list[dict]:
        """Its pads, as the palette offers them for wiring, with their
        board names."""
        names: dict[str, list[str]] = {}
        for label, pad in self.board.get("pin_labels", {}).items():
            names.setdefault(pad, []).append(label)
        for led in self.board["leds"]:
            names.setdefault(led["pin"], []).append(led["id"])
        return [{"pad": f"{self.key}/{p}", "label": " ".join(names.get(p, [])),
                 "adc": p in self.ain, "twin": self.key} for p in self.pads]

    def describe(self) -> dict:
        return {"key": self.key, "board": self.board, "at": self.at,
                "name": self.board["name"], "spi_hosts": self.hosts["spi"]}

    def emit(self, msg: dict) -> None:
        if msg["type"] == "uart":
            self.uart_log = (self.uart_log + msg["data"])[-20000:]
            self.uart_total += len(msg["data"])
        self.app.emit({**msg, "twin": self.key})

    # -- Click boards -----------------------------------------------------

    def wired(self) -> dict[str, dict]:
        """The Clicks on the bench wired to its pads"""
        return {p: w for p, w in self.app.wired.items()
                if w.get("twin") == self.key}

    def wired_option(self) -> str:
        """pp-qemu's "wired": name=click:role.PAD:... per plugged Click"""
        return "/".join(
            f"{pid}={w['id']}:" + ":".join(f"{r}.{pad}" for r, pad in
                                           sorted(w["pins"].items()))
            for pid, w in sorted(self.wired().items()) if w["plugged"])

    def clicks_message(self) -> dict:
        return {"type": "clicks", "clicks": self.clicks,
                "wired": self.wired(),
                "wired_warnings": self.app.wired_warnings,
                "twin": self.key}

    def machine(self) -> str:
        opts = [f"{slot}={c['id']}" for slot, c in self.clicks.items() if c]
        if self.board.get("dgi"):
            opts.append("dgi=dgi")
        if self.wired_option():
            opts.append(f"wired={self.wired_option()}")
        return ",".join([self.board["machine"], *opts])

    def click_at(self, slot: str) -> dict | None:
        """The Click in mikroBUS @slot, or the wired one with part id
        @slot (it is /machine/<slot>-click either way)."""
        c = self.clicks.get(slot)
        if c:
            return c
        w = self.wired().get(slot)
        return w if w and w["plugged"] else None

    def click_watch(self) -> list[tuple[str, ...]]:
        """(key, path, property[, generation property]) of what each
        plugged Click and the board's own parts show, polled live."""
        plugged = {**{s: c for s, c in self.clicks.items() if c},
                   **{s: w for s, w in self.wired().items() if w["plugged"]}}
        return [slot_watch(slot, *prop) for slot, c in plugged.items()
                for prop in watched(CLICKS[c["id"]])] + \
            part_watch(self.board)

    async def plug(self, slot: str, click: str | None) -> None:
        if slot not in self.clicks or (click and click not in CLICKS):
            return
        self.clicks[slot] = click and {
            "id": click,
            "values": {c["id"]: c["default"] for c in CLICKS[click]["controls"]}}
        self.app.save_workspace()
        self.emit(self.clicks_message())
        if self.elf:
            await self.run(self.elf)

    async def control(self, slot: str, cid: str, value: float) -> None:
        c = self.click_at(slot)
        ctl = c and next((x for x in CLICKS[c["id"]]["controls"]
                          if x["id"] == cid), None)
        btn = c and next((x for x in CLICKS[c["id"]].get("buttons", []) +
                          CLICKS[c["id"]].get("toggles", [])
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
        self.emit(self.clicks_message())
        if self.session and self.session.qmp:
            try:
                await self.session.set_click(slot, ctl["property"],
                                             round(value * ctl["scale"]))
            except (RuntimeError, ConnectionError):
                pass                    # restarting; run() applies it

    async def click_text(self, slot: str, iid: str, text: str) -> None:
        """A text input of the Click's panel ("inputs": what a phone sends
        the BLE module): the string to its property."""
        c = self.click_at(slot)
        inp = c and next((x for x in CLICKS[c["id"]].get("inputs", [])
                          if x["id"] == iid), None)
        if inp and self.session and self.session.qmp:
            try:
                await self.session.set_click(slot, inp["property"], text)
            except (RuntimeError, ConnectionError):
                pass

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
        c = self.click_at(slot)
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

    # -- firmware -----------------------------------------------------------

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

    def keep_firmware(self, elf: Path) -> None:
        """Keep a copy of an uploaded @elf beside the workspace file: the
        workspace runs it again next time."""
        try:
            d = self.app.workspace_file.parent / "firmware"
            d.mkdir(parents=True, exist_ok=True)
            dest = d / (elf.name if len(self.app.twins) < 2 else
                        f"{self.key}-{elf.name}")
            if elf.resolve() != dest.resolve():
                shutil.copyfile(elf, dest)
            self.kept_elf = dest
        except OSError:
            return
        self.app.save_workspace()

    def files_message(self) -> dict:
        """The CIRCUITPY drive for the page: every file, and code.py."""
        if not self.drive:
            return {"type": "files", "files": None, "twin": self.key}
        try:
            fat = fatfs.FatImage(bytearray(self.drive.read_bytes()))
            files = [{"path": p, "size": e.size, "dir": e.is_dir}
                     for p, e in fat.walk()]
            code = fat.read("code.py").decode("utf-8", "replace") \
                if fat.lookup("code.py") else ""
        except (OSError, fatfs.FatError) as e:
            return {"type": "files", "files": [], "error": str(e),
                    "twin": self.key}
        return {"type": "files", "files": files, "code": code,
                "twin": self.key}

    async def save_file(self, path: str, text: str) -> None:
        """Write @path on the CIRCUITPY drive; the twin restarts with it
        (the twin holds the image while it runs)."""
        if not self.drive or not path:
            return
        await self.stop()
        data = bytearray(self.drive.read_bytes())
        fatfs.FatImage(data).write(path, text.encode("utf-8"))
        self.drive.write_bytes(data)
        self.emit(self.files_message())
        if self.elf:
            await self.run(self.elf)

    def status_message(self) -> dict:
        if self.session:
            return {"type": "status", "state": "running",
                    "elf": self.session.elf.name, "twin": self.key}
        return {"type": "status", "state": "idle",
                "text": "no firmware: drop an ELF built for this board",
                "twin": self.key}

    # -- the bench -------------------------------------------------------

    def bench_groups(self) -> tuple[str, ...]:
        """PORT groups of its pads the bench uses"""
        return tuple(sorted({benchlib.pad_ref(p)[0]
                             for p in self.app.bench.pads
                             if _bare(p)[0] == self.key}))

    def bench_session(self) -> None:
        """Tell its session what the bench needs polled."""
        if self.session:
            groups = self.bench_groups()
            self.session.bench_active = bool(groups)
            self.session.bench_groups = groups
            self.session.groups = tuple(sorted(set(self.session.groups) |
                                               set(groups)))

    async def bench_apply(self, res: benchlib.Result) -> None:
        """Its input pads read the level the bench settles them at; its
        ADC channels read their pad's voltage."""
        if not (self.session and self.session.qmp):
            return
        inputs = {b: lvl for p, lvl in res.inputs.items()
                  for k, b in [_bare(p)] if k == self.key}
        groups = set(self.bench_groups()) | set(self.bench_applied)
        for g, (mask, level) in benchlib.pad_drives(inputs, groups).items():
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
            key, bare = _bare(pad)
            if key != self.key:
                continue
            for path, ch in self.ain.get(bare, []):
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

    # -- injected faults ---------------------------------------------------

    def check_fault(self, f: dict) -> dict:
        """@f as a fault of this board, or ValueError: {"kind": "short",
        "pad", "level"} (shorted to VDD or GND), {"kind": "nack", "addr"}
        (or "slot": its I2C Click; that address NACKs), {"kind": "miso",
        "host", "value"} (what that SPI host reads; "sercom" names it
        too)."""
        kind = f.get("kind")
        if kind == "short":
            pad = str(f.get("pad", "")).upper()
            if pad not in self.pads:
                raise ValueError(f"{self.board['name']} has no pad {pad}")
            level = f.get("level", 0)
            return {"kind": "short", "pad": pad,
                    "level": 1 if level in (1, True, "1", "vdd", "high")
                    else 0}
        if kind == "nack":
            slot, addr = f.get("slot"), f.get("addr")
            if addr is None and slot:
                c = self.click_at(str(slot))
                addr = click_i2c_address(c["id"]) if c else None
                if addr is None:
                    raise ValueError(f"no I2C Click in {slot}")
            try:
                addr = int(addr, 16) if isinstance(addr, str) else int(addr)
            except (TypeError, ValueError):
                raise ValueError("nack needs addr or slot") from None
            if not 0 <= addr < 128:
                raise ValueError(f"0x{addr:x} is not a 7-bit address")
            return {"kind": "nack", "addr": addr,
                    **({"slot": str(slot)} if slot else {})}
        if kind == "miso":
            host = str(f.get("host") or f.get("sercom") or "").lower()
            if host not in self.hosts["spi"]:
                raise ValueError(f"{self.board['name']} has no SPI host "
                                 f"{host!r} ({', '.join(self.hosts['spi'])})")
            value = f.get("value", 0xFF)
            try:
                value = int(value, 16) if isinstance(value, str) \
                    else int(value)
            except ValueError:
                raise ValueError(f"miso value {value!r}") from None
            # A SERCOM's characters have up to 9 bits, an AVR SPI's 8
            mask = 0x1FF if host.startswith("sercom") else 0xFF
            return {"kind": "miso", "host": host, "value": value & mask}
        raise ValueError(f"unknown fault {kind!r} ({FAULT_KINDS})")

    async def set_faults(self, faults: list) -> None:
        """Replace its faults (ValueError: one is not a fault; none change)"""
        self.faults = [self.check_fault(f) for f in faults]
        await self.apply_faults()
        self.emit(self.faults_message())

    def faults_message(self) -> dict:
        return {"type": "faults", "faults": self.faults}

    async def apply_faults(self) -> None:
        """Its faults on its QEMU: shorts on the PORT, NACKed addresses on
        the I2C hosts, stuck MISO lines on the SPI hosts"""
        s = self.session
        if not (s and s.qmp):
            return
        shorts: dict[str, tuple[int, int]] = {}
        for f in self.faults:
            if f["kind"] == "short":
                g, bit = f["pad"][1].lower(), int(f["pad"][2:])
                mask, level = shorts.get(g, (0, 0))
                shorts[g] = (mask | 1 << bit, level | f["level"] << bit)
        sets = [(PORT_PATH, f"p{g}-short", "%x %x" % shorts.get(g, (0, 0)))
                for g in sorted(set(shorts) | self.short_groups)]
        self.short_groups = set(shorts)
        nack = ",".join(f"{f['addr']:x}" for f in self.faults
                        if f["kind"] == "nack")
        for name in self.hosts["i2c"]:
            sets.append((f"/machine/soc/{name}", "fault-nack", nack))
        for name in self.hosts["spi"]:
            miso = next((f"{f['value']:x}" for f in self.faults
                         if f["kind"] == "miso" and f["host"] == name), "")
            sets.append((f"/machine/soc/{name}", "fault-miso", miso))
        for path, prop, val in sets:
            try:
                await s.set_prop(path, prop, val)
            except (RuntimeError, ConnectionError):
                pass                    # an older QEMU, or restarting

    # -- recording and replaying ---------------------------------------------

    def recordings_dir(self) -> Path:
        return self.app.workspace_file.parent / "recordings"

    def recordings(self) -> list[dict]:
        """Its board type's recordings, the newest first"""
        out = []
        files = sorted(self.recordings_dir().glob("*.json"),
                       key=lambda p: p.stat().st_mtime, reverse=True) \
            if self.recordings_dir().is_dir() else []
        for f in files:
            try:
                rec = json.loads(f.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            if rec.get("board") == self.board["id"]:
                out.append({k: rec.get(k) for k in (
                    "name", "firmware", "duration_ns", "events", "saved")} |
                    {"inputs": len(rec.get("inputs", []))})
        return out

    def recorder_message(self) -> dict:
        s = self.session
        return {"type": "recorder", "twin": self.key,
                "recording_since": self.recording_since,
                "deterministic": bool(s and s.deterministic),
                "replaying": self.replaying,
                "recordings": self.recordings()}

    async def record_start(self) -> None:
        """Run its firmware again from reset, so that the run replays
        exactly: each input is kept with the virtual time it took effect"""
        if not self.elf:
            raise ValueError("no firmware to record")
        if await self.run(self.elf, deterministic=True):
            self.recording_since = time.time()
        self.emit(self.recorder_message())

    def record_stop(self, name: str | None = None) -> dict:
        """Save what was recorded (the firmware, the machine, the starting
        settings, the inputs and digests of all that happened) under
        @name, else the board and the time; its summary"""
        s = self.session
        if not (s and s.deterministic and self.recording_since and
                s.trace_store):
            raise ValueError("not recording")
        name = re.sub(r"[^A-Za-z0-9_.-]+", "-", name or "").strip("-.") or \
            f"{self.board['id']}-{time.strftime('%Y%m%d-%H%M%S')}"
        store, d = s.trace_store, self.recordings_dir()
        end = store.now
        d.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(s.elf, d / f"{name}.elf")
        digests = tr.digests(store, end)
        rec = {"version": 1, "name": name, "board": self.board["id"],
               "machine": s.machine, "icount": DETERMINISTIC_ICOUNT,
               "firmware": s.elf.name, "elf": f"{name}.elf",
               "duration_ns": end, **s.recording(), "digests": digests,
               "events": sum(b[2] for b in digests),
               "saved": time.strftime("%Y-%m-%dT%H:%M:%S")}
        (d / f"{name}.json").write_text(json.dumps(rec), encoding="utf-8")
        self.recording_since = None
        self.emit(self.recorder_message())
        return {k: rec[k] for k in ("name", "duration_ns", "events")} | \
            {"inputs": len(rec["inputs"]), "path": str(d / f"{name}.json")}

    async def replay(self, name: str, pace: bool = True) -> None:
        """Run recording @name again: its firmware and machine from reset,
        its inputs at their virtual times; then say whether everything
        happened as recorded"""
        path = self.recordings_dir() / f"{Path(name).stem}.json"
        try:
            rec = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            raise ValueError(f"no recording {name}") from None
        if rec.get("board") != self.board["id"]:
            raise ValueError(f"{name} was recorded on {rec.get('board')}")
        self.replaying = {"name": rec["name"], "end": rec["duration_ns"],
                          "pace": pace, "state": "running"}
        if not await self.run(self.recordings_dir() / rec["elf"],
                              replay=rec, pace=pace):
            self.replaying = None
            return
        self.emit(self.recorder_message())
        asyncio.create_task(self._judge(self.session, rec))

    async def _judge(self, s: Session, rec: dict) -> None:
        """Once replay @s passes the recording's end, compare digests"""
        last, still = -1, 0.0
        while s is self.session and s.trace_store.now < rec["duration_ns"]:
            await asyncio.sleep(0.1)
            still = 0.0 if s.trace_store.now != last else still + 0.1
            last = s.trace_store.now
            if still > 15:
                self.replaying["state"] = "stalled"
                self.emit(self.recorder_message())
                return
        if s is not self.session:
            return
        verdict = tr.compare(rec["digests"],
                             tr.digests(s.trace_store, rec["duration_ns"]))
        self.replaying.update(state="same" if verdict["same"] else "differs",
                              verdict=verdict)
        self.emit(self.recorder_message())

    # -- running -----------------------------------------------------------

    async def _stop_dgi(self) -> None:
        if self.dgi:
            await self.dgi.stop()
        self.dgi = None

    async def stop(self) -> None:
        """Stop its QEMU (and the circuit simulated with it)."""
        async with self.lock:
            await self._stop_dgi()
            if self.session:
                await self.session.stop()
                self.session = None
            if self.app.analog_twin is self:
                self.app._stop_analog()

    async def run(self, elf: Path, debug_port: int | None = None, *,
                  deterministic: bool = False, replay: dict | None = None,
                  pace: bool = True) -> bool:
        """Run @elf (False if it did not start). With @debug_port, QEMU
        starts paused with its gdbstub on that port. @deterministic: a run
        that can be recorded; @replay: a recording to repeat (its machine
        and inputs), paced to real time or not."""
        if not replay:
            self.replaying = None
        async with self.lock:
            await self._stop_dgi()
            if self.session:
                await self.session.stop()
            if self.app.analog_twin is self:
                self.app._stop_analog()
            self.uart_log = ""
            self.emit({"type": "reset"})
            self.elf = elf
            self.bench_applied, self.ain_applied = {}, {}
            self.frame = {}
            groups = self.bench_groups()
            analog_args = self.app._start_analog(self)
            self.wired_running = self.wired_option()
            self.recording_since = None
            self.session = Session(replay["machine"] if replay
                                   else self.machine(), elf, self.emit,
                                   deterministic=deterministic,
                                   replay=replay, pace=pace,
                                   vcom=tl.vcom_source(self.board),
                                   groups=tuple(sorted(
                                       set(pin_groups(self.board)) |
                                       set(groups))),
                                   watch=self.click_watch(),
                                   drive=self.drive,
                                   bench=lambda frame: self.app.bench_frame(
                                       frame, self),
                                   bench_active=bool(groups),
                                   bench_groups=groups,
                                   bench_analog=tuple(
                                       (a["pad"], a["path"], a["property"])
                                       for a in self.board.get("analog_out",
                                                               [])),
                                   dgi=bool(self.board.get("dgi")),
                                   extra_args=(*analog_args, *(
                                       () if debug_port is None else (
                                           "-gdb",
                                           f"tcp:127.0.0.1:{debug_port}",
                                           "-S"))))
            try:
                await self.session.start()
                if self.session.dgi_port:
                    self.dgi = DgiView(self.session.dgi_port, self.emit)
                    await self.dgi.start()
                    self.dgi.set_wanted(self.dgi_wanted)
                for slot in [*self.clicks, *self.wired()]:
                    c = self.click_at(slot)
                    for ctl in CLICKS[c["id"]]["controls"] if c else ():
                        await self.session.set_click(
                            slot, ctl["property"],
                            round(c["values"][ctl["id"]] * ctl["scale"]))
                self.short_groups = set()
                await self.apply_faults()
                if self.session.deterministic:
                    await self.session.go()
            except Exception as e:      # report to the page, keep serving
                self.emit({"type": "status", "state": "error", "error": str(e)})
                return False
            if debug_port is not None:
                self.emit({"type": "status", "state": "running",
                           "elf": f"{elf.name} (debugger on {debug_port})"})
            return True

    async def start(self, elf: Path | None = None) -> None:
        """Run @elf, else what it ran last (an upload), else its default"""
        self.elf = elf or self.kept_elf or await self.default_firmware()
        if self.elf:
            await self.run(self.elf)
        elif not self.board.get("circuitpython"):
            self.emit({"type": "status", "state": "idle",
                       "text": "no firmware: drop an ELF built for this board"})

    def summary(self, uart_tail: int = 2000) -> dict:
        """What an assistant needs to know about it (the MCP tools)"""
        s = self.session
        levels = s.levels if s else {}

        def level(pad: str) -> int | None:
            g = pad[1].lower()
            return (levels[g] >> int(pad[2:])) & 1 if g in levels else None

        leds = []
        for led in self.board["leds"]:
            lvl = level(led["pin"])
            leds.append({"id": led["id"], "pin": led["pin"],
                         "on": None if lvl is None else
                         bool(lvl) != bool(led.get("active_low"))})
        store = s.trace_store if s else None
        return {
            "key": self.key, "board": self.board["id"],
            "name": self.board["name"], "machine": self.board["machine"],
            "running": bool(s), "firmware": str(self.elf) if self.elf else None,
            "clicks": {slot: c["id"] for slot, c in self.clicks.items() if c},
            "click_slots": list(self.clicks),
            "click_values": {slot: c["values"]
                             for slot, c in self.clicks.items() if c},
            "wired": {p: {"click": w["id"], "plugged": w["plugged"],
                          "pins": w["pins"]}
                      for p, w in self.wired().items()},
            "buttons": [b["action"] for b in self.board["buttons"]],
            "leds": leds, "faults": self.faults,
            "timeline": bool(store),
            "virtual_time_ns": store.now if store else None,
            "uart_tail": self.uart_log[-uart_tail:],
            "uart_mark": self.uart_total,
            "recording": bool(self.recording_since),
            "replaying": self.replaying,
            "recordings": [r["name"] for r in self.recordings()]}

    def snapshot(self) -> list[str]:
        """What a page that just connected needs about it"""
        msgs = [self.clicks_message(), self.files_message(),
                self.status_message(), self.faults_message(),
                self.recorder_message()]
        if self.dgi and self.dgi.info:
            msgs.append({**self.dgi.info, "twin": self.key})
        if self.session:
            msgs += [{"type": "uart", "data": self.uart_log},
                     {"type": "pins", "pins": self.session.pins},
                     {"type": "levels", "levels": self.session.levels},
                     {"type": "clickstate",
                      "state": self.session.click_state}]
        return [json.dumps({**m, "twin": self.key}) for m in msgs]


class App:
    """The workspace: the boards on it (each a Twin), the parts bench they
    share, and the page's WebSocket. With one board, the App answers for
    it (session, run, plug...) as it did before workspaces had several."""

    def __init__(self, board: str | None, elf: Path | None,
                 watch: Path | None = None, bench: Path | None = None,
                 workspace: Path | None = None, port: int | None = None):
        self.port = port                # where it listens, to announce
        self.workspace_file = workspace or bench_dir() / "workspace.json"
        self.first_run = not self.workspace_file.is_file()
        saved = self._read_workspace()
        # The watched project's newest build when the workspace last looked:
        # {"folder", "build": build_stamp}
        self.watch_seen = saved.get("watched") \
            if isinstance(saved.get("watched"), dict) else None
        self.clients: set[web.WebSocketResponse] = set()
        self.watch = watch
        self.tmp = Path(tempfile.mkdtemp(prefix="pp_web_"))
        self.lock = asyncio.Lock()
        self.twins: list[Twin] = []
        entries = [e for e in saved.get("boards") or [] if isinstance(e, dict)]
        if board is not None:
            # --board: that board alone (its Clicks if it was there)
            entries = [next((e for e in entries if e.get("type") == board),
                            {"type": board})]
        for e in entries:
            if not (BOARDS / str(e.get("type")) / "board.json").is_file():
                continue
            key = e.get("key") if e.get("key") and \
                not self.twin(e.get("key")) else self._next_key()
            t = Twin(self, key, e["type"], e.get("at") or [0.0, 0.0])
            if not e.get("at") and self.twins:
                t.at = self._free_at(t)
            for slot, c in (e.get("clicks") or {}).items():
                if slot in t.clicks and c and c.get("id") in CLICKS:
                    t.clicks[slot] = {"id": c["id"], "values": {
                        k["id"]: c.get("values", {}).get(k["id"], k["default"])
                        for k in CLICKS[c["id"]]["controls"]}}
            kept = e.get("firmware")
            t.kept_elf = Path(kept) if kept and Path(kept).is_file() else None
            self.twins.append(t)
        if elf and not self.twins:
            # --elf on an empty workspace: a board for its architecture
            from tools.pp.qemu import elf_target
            arch = elf_target(elf) if elf.is_file() else None
            self.twins.append(Twin(self, "b1", "ev66e56a" if arch == "avr"
                                   else "ev15n46a"))
        if elf and self.twins:
            self.twins[0].elf = elf
        sel = saved.get("selected")
        self.selected = sel if self.twin(sel) else \
            (self.twins[0].key if self.twins else None)
        # The bench: the workspace's, else the board's from before the
        # workspace file existed (bench-<board>.json)
        data = saved.get("bench")
        if data is None and self.twins:
            try:
                data = json.loads((bench_dir() /
                                   f"bench-{self.twins[0].board['id']}.json")
                                  .read_text(encoding="utf-8"))
            except (OSError, ValueError):
                data = None
        self.bench = benchlib.Bench({}, holes={})
        self.bench_view = ""
        self.wired: dict[str, dict] = {}
        self.wired_warnings: list[str] = []
        # The bench in ngspice (a dynamic bench, one board): the host, the
        # netlist it runs (without the live controls' values), the controls
        # as sent, and its latest averages
        self.analog: HostProcess | None = None
        self.analog_twin: Twin | None = None
        self.analog_bench = benchlib.Analog(self.bench)
        self.analog_key: str | None = None
        self.analog_controls: dict[str, float] = {}
        self.analog_avg: dict[str, float] | None = None
        self.load_bench(data or {})
        if bench:
            self.load_bench(json.loads(bench.read_text(encoding="utf-8")))

    # -- the boards ---------------------------------------------------------

    def twin(self, key: str | None) -> Twin | None:
        return next((t for t in self.twins if t.key == key), None)

    @property
    def primary(self) -> Twin | None:
        """The board the page has selected (else the first)"""
        return self.twin(self.selected) or (self.twins[0] if self.twins
                                            else None)

    def _next_key(self) -> str:
        n = 1
        while self.twin(f"b{n}"):
            n += 1
        return f"b{n}"

    def _free_at(self, t: Twin) -> list[float]:
        """Right of the boards already there, tops lined up"""
        if not self.twins:
            return [0.0, 0.0]
        right = max(o.at[0] + o.view["size"][0] for o in self.twins)
        return [round(right + 20, 1), 0.0]

    def owner(self, slot: str) -> Twin | None:
        """The board with mikroBUS @slot or a wired Click @slot"""
        w = self.wired.get(slot)
        if w and w.get("twin"):
            return self.twin(w["twin"])
        if self.primary and slot in self.primary.clicks:
            return self.primary
        return next((t for t in self.twins if slot in t.clicks), None)

    # The first (selected) board, as before a workspace had several
    board = property(lambda self: self.primary and self.primary.board)
    session = property(lambda self: self.primary and self.primary.session)
    uart_log = property(lambda self: self.primary.uart_log
                        if self.primary else "")
    clicks = property(lambda self: self.primary.clicks if self.primary
                      else {})
    kept_elf = property(lambda self: self.primary and self.primary.kept_elf)
    drive = property(lambda self: self.primary and self.primary.drive)

    @property
    def elf(self) -> Path | None:
        return self.primary and self.primary.elf

    @elf.setter
    def elf(self, value: Path | None) -> None:
        if self.primary:
            self.primary.elf = value

    def machine(self) -> str:
        return self.primary.machine() if self.primary else ""

    async def run(self, elf: Path, debug_port: int | None = None) -> bool:
        if not self.primary:
            self.emit({"type": "status", "state": "error",
                       "error": "add a board first"})
            return False
        return await self.primary.run(elf, debug_port)

    async def plug(self, slot: str, click: str | None) -> None:
        t = self.owner(slot)
        if t:
            await t.plug(slot, click)

    async def control(self, slot: str, cid: str, value: float) -> None:
        t = self.owner(slot)
        if t:
            await t.control(slot, cid, value)

    async def click_text(self, slot: str, iid: str, text: str) -> None:
        t = self.owner(slot)
        if t:
            await t.click_text(slot, iid, text)

    async def stick(self, slot: str, sid: str, x: float, y: float,
                    pressed: bool | None) -> None:
        t = self.owner(slot)
        if t:
            await t.stick(slot, sid, x, y, pressed)

    async def default_firmware(self) -> Path | None:
        return await self.primary.default_firmware() if self.primary else None

    def keep_firmware(self, elf: Path) -> None:
        if self.primary:
            self.primary.keep_firmware(elf)

    def status_message(self) -> dict:
        if self.primary:
            return self.primary.status_message()
        return {"type": "status", "state": "idle",
                "text": "add a board to start"}

    def clicks_message(self) -> dict:
        return self.primary.clicks_message() if self.primary else \
            {"type": "clicks", "clicks": {}, "wired": {},
             "wired_warnings": self.wired_warnings}

    # -- the workspace file ------------------------------------------------

    def _read_workspace(self) -> dict:
        try:
            data = json.loads(self.workspace_file.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}
        return data if isinstance(data, dict) else {}

    def save_workspace(self) -> None:
        """The boards (each with its Clicks, place and firmware) and the
        bench, to the workspace file."""
        bench = {"parts": [{k: v for k, v in p.items() if k != "pressed"}
                           for p in self.bench.data["parts"]],
                 "wires": self.bench.data["wires"],
                 "anchors": self.bench.data["anchors"]}
        boards = [{"key": t.key, "type": t.board["id"], "at": t.at,
                   "clicks": {s: c for s, c in t.clicks.items() if c},
                   "firmware": str(t.kept_elf) if t.kept_elf else None}
                  for t in self.twins]
        try:
            self.workspace_file.parent.mkdir(parents=True, exist_ok=True)
            self.workspace_file.write_text(json.dumps(
                {"version": 2, "boards": boards, "bench": bench,
                 "selected": self.selected, "watched": self.watch_seen},
                indent=1), encoding="utf-8")
        except OSError:
            pass

    save_bench = save_workspace

    # -- the parts bench -----------------------------------------------------

    def _holes(self) -> dict[str, dict]:
        return {k: v for t in self.twins for k, v in t.holes().items()}

    def _qualify(self, end: str) -> str | None:
        """A bench end with its board's key: one from before boards had
        keys ("pin:PA05", "hole:J801.3") is the first board's; one of a
        board that has gone is None"""
        for kind in ("pin:", "hole:"):
            if end.startswith(kind):
                key, bare = _bare(end[len(kind):])
                if not key:
                    return f"{kind}{self.twins[0].key}/{bare}" \
                        if self.twins else None
                return end if self.twin(key) else None
        return end

    def load_bench(self, data: dict) -> None:
        """Replace the bench (its ends qualified with board keys), for the
        boards on the workspace now."""
        data = dict(data or {})
        wires = []
        for w in data.get("wires") or []:
            if isinstance(w, (list, tuple)) and len(w) == 2:
                q = [self._qualify(str(e)) for e in w]
                if all(q):
                    wires.append(q)
        anchors = {}
        for end, at in (data.get("anchors") or {}).items():
            q = self._qualify(str(end))
            if q:
                anchors[q] = at
        data.update(wires=wires, anchors=anchors)
        first = self.twins[0].board if self.twins else {}
        self.bench.vdd = first.get("vdd", 3.3)
        self.bench.pad_max = first.get("pad_max_ma", 20.0) / 1000
        self.bench.holes = self._holes()
        self.bench.set(data)
        self.update_wired()

    def bench_message(self) -> dict:
        return {"type": "bench", "bench": self.bench.data}

    async def set_bench(self, data: dict) -> None:
        self.load_bench(data)
        self.save_workspace()
        self.emit(self.bench_message())
        for t in self.twins:
            t.bench_session()
            t.emit(t.clicks_message())
        await self._sync()
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
            self.save_workspace()
        self.emit(self.bench_message())
        if self.update_wired():
            for t in self.twins:
                t.emit(t.clicks_message())
        await self._sync()
        await self.bench_frame(None)

    async def _sync(self) -> None:
        """After a bench change: restart each board whose wired Clicks
        changed, or the one with the circuit if that changed (or went in
        or out of ngspice); send changed live controls."""
        want = self._analog_wanted()
        key = self._analog_key(want[0]) if want else None
        for t in self.twins:
            analog = t is (self.twins[0] if len(self.twins) == 1 else None) \
                and key != self.analog_key
            if t.session and t.elf and \
                    (analog or t.wired_option() != t.wired_running):
                await t.run(t.elf)
        if self.analog and want and key == self.analog_key:
            for name, value in want[1].items():
                if self.analog_controls.get(name) != value:
                    self.analog.set_control(name, value)
                    self.analog_controls[name] = value

    async def bench_frame(self, frame: dict | None,
                          twin: Twin | None = None) -> None:
        """Solve the bench for the boards' pads (@frame: @twin's session's
        latest; else the last ones), drive the input pads and ADC channels
        it feeds, and send the page what changed."""
        if twin is not None and frame is not None:
            twin.frame = frame
        pads = {}
        for pad in self.bench.pads:
            key, bare = _bare(pad)
            t = self.twin(key)
            fr = t.frame if t and t.session else {}
            pins, pulls = fr.get("pins", {}), fr.get("pulls", {})
            analog = fr.get("analog", {})
            ohms = {a["pad"]: a.get("ohms", 100.0)
                    for a in (t.board if t else {}).get("analog_out", [])}
            g, bit = benchlib.pad_ref(bare)
            group = pins.get(g) or []
            hi, lo = group[bit] if bit < len(group) else (0, 0)
            mask, up = pulls.get(g, (0, 0))
            pull = (1 if up >> bit & 1 else -1) if mask >> bit & 1 else 0
            mv = analog.get(bare, -1)
            pads[pad] = benchlib.Pad(high=hi, low=lo, pull=pull,
                                     analog_mv=mv if mv >= 0 else None,
                                     analog_ohms=ohms.get(bare, 100.0))
        if self.analog:
            # ngspice drives the board's pads and ADC itself; the page
            # shows its averages (the DC solve until the first arrive)
            res = self.analog_bench.result(self.analog_avg, pads) \
                if self.analog_avg else self.bench.solve(pads)
            solver = "ngspice"
        else:
            res = self.bench.solve(pads)
            for t in self.twins:
                await t.bench_apply(res)
            solver = "dc"
        view = json.dumps({"type": "benchstate", "parts": res.parts,
                           "pads": res.pads, "nets": res.nets,
                           "warnings": res.warnings + self.wired_warnings,
                           "solver": solver})
        if view != self.bench_view:
            self.bench_view = view
            self.emit(json.loads(view))

    # -- Click boards wired on the bench -----------------------------------

    def update_wired(self) -> bool:
        """The Clicks on the bench and what their pins are wired to
        (through jumpers, header holes and tags), with warnings for one
        no board can plug in. True if that changed."""
        b, old = self.bench, self.wired
        wired, warn = {}, []
        roots: dict[str, set[str]] = {}
        for e in b.ends.up:
            roots.setdefault(b.ends.find(e), set()).add(e)
        for p in b.data["parts"]:
            if p["type"] != "click" or not p.get("click"):
                continue
            pid, cid = p["id"], p["click"]
            net = lambda t: roots.get(b.ends.find(f"{pid}.{t}"), set()) \
                # noqa: E731
            pins, keys = {}, set()
            for role in WIRED_ROLES:
                pads = sorted(e[4:] for e in net(role)
                              if e.startswith("pin:"))
                if pads:
                    key, pins[role] = _bare(pads[0])
                    keys.add(key)
            twin = self.twin(next(iter(keys))) if len(keys) == 1 else None
            power = bool({"rail:3V3", "rail:5V"} & (net("3v3") | net("5v")))
            ground = "rail:GND" in net("gnd") | net("gnd2")
            name = CLICKS[cid]["name"]
            bus = CLICKS[cid].get("bus", "").split()[0] if \
                CLICKS[cid].get("bus") else ""
            ok = power and ground
            if not ok:
                warn.append(f"{pid} ({name}): wire 3V3 or 5V and GND to "
                            "power it")
            elif len(keys) > 1:
                warn.append(f"{pid} ({name}): its pins go to two boards")
                ok = False
            elif bus == "UART":
                warn.append(f"{pid} ({name}): a UART Click goes on a "
                            "mikroBUS header")
                ok = False
            elif bus in ("SPI", "I2C"):
                need = ("sck", "mosi") if bus == "SPI" else ("sda", "scl")
                if not all(r in pins for r in need):
                    warn.append(f"{pid} ({name}): wire its "
                                f"{' and '.join(r.upper() for r in need)} "
                                f"for {bus}")
                elif not wiring_fits(bus, pins, twin.sercoms if twin else {}):
                    warn.append(f"{pid} ({name}): no SERCOM has {bus} on "
                                + ", ".join(f"{r.upper()} {pins[r]}"
                                            for r in need))
            values = (old.get(pid) or {}).get("values") \
                if (old.get(pid) or {}).get("id") == cid else None
            wired[pid] = {"id": cid, "pins": pins, "plugged": ok and
                          twin is not None, "twin": twin and twin.key,
                          "values": values or {
                              c["id"]: c["default"]
                              for c in CLICKS[cid]["controls"]}}
        changed = wired != old or warn != self.wired_warnings
        self.wired, self.wired_warnings = wired, warn
        return changed

    # -- the bench in ngspice ------------------------------------------------

    def _analog_wanted(self) -> tuple[str, dict[str, float]] | None:
        """(netlist, controls) when the bench should run in ngspice: it
        changes in time, and there is one board, which has the port"""
        if not self.bench.dynamic or len(self.twins) != 1 or \
                self.twins[0].board["machine"] not in COSIM_MACHINES or \
                not ngspice_lib.available():
            return None
        return self.analog_bench.netlist()

    @staticmethod
    def _analog_key(netlist: str) -> str:
        return "\n".join(ln for ln in netlist.splitlines()
                         if not ln.startswith("*pp control"))

    def _start_analog(self, twin: Twin) -> list[str]:
        """Start the analog host for a dynamic bench, for @twin: QEMU's
        arguments to reach it (none, and the DC bench, without one)."""
        want = self._analog_wanted() if twin is self.twins[0] else None
        if twin is self.twins[0] or self.analog_twin is twin:
            self.analog_key = self._analog_key(want[0]) if want else None
            self.analog_avg = None
        if not want:
            return []
        path = self.tmp / "bench.cir"
        path.write_text(want[0], encoding="utf-8")
        loop = asyncio.get_running_loop()

        def on_state(t: float, avg: dict) -> None:
            loop.call_soon_threadsafe(self._analog_state, avg)
        try:
            self.analog = HostProcess(path, on_state=on_state)
        except HostError as e:
            self.analog_key = None
            twin.emit({"type": "status", "state": "warning",
                       "text": f"bench in DC only: {e}"})
            return []
        self.analog_twin = twin
        self.analog_controls = dict(want[1])
        return self.analog.qemu_args()

    def _analog_state(self, avg: dict) -> None:
        self.analog_avg = avg

    def _stop_analog(self) -> None:
        if self.analog:
            self.analog.kill()
        self.analog = None
        self.analog_twin = None
        self.analog_avg = None

    # -- changing the workspace -------------------------------------------------

    def workspace_message(self) -> dict:
        """Everything the page draws the workspace from"""
        return {"type": "workspace",
                "twins": [t.describe() for t in self.twins],
                "selected": self.selected,
                "board": self.board,
                "boards": board_list(), "templates": template_list(),
                "catalog": CLICKS, "adapters": ADAPTERS,
                "bench_lib": {t: {"name": d["name"],
                                  "terminals": d["terminals"],
                                  "props": d["props"]}
                              for t, d in benchlib.PARTS.items()},
                "bench_pins": [p for t in self.twins for p in t.bench_pins()],
                "bench_holes": self.bench.holes}

    board_message = workspace_message

    def _changed(self) -> None:
        """The boards changed: the bench follows, the page redraws"""
        self.load_bench(self.bench.data)
        self.save_workspace()
        self.emit(self.workspace_message())
        self.emit(self.bench_message())
        for t in self.twins:
            t.emit(t.clicks_message())
            t.emit(t.files_message())
        if not self.twins:
            self.emit(self.status_message())

    async def add_board(self, board: str, at: list[float] | None = None,
                        clicks: dict[str, str] | None = None,
                        elf: Path | None = None) -> Twin | None:
        """Put @board on the workspace (at @at, bench mm; else right of
        the others) with @clicks ({slot: Click}), and start it on @elf
        (else its default firmware)."""
        if not (BOARDS / str(board) / "board.json").is_file():
            return None
        async with self.lock:
            t = Twin(self, self._next_key(), board)
            t.at = [float(at[0]), float(at[1])] if at else self._free_at(t)
            for slot, cid in (clicks or {}).items():
                if slot in t.clicks and cid in CLICKS:
                    t.clicks[slot] = {"id": cid, "values": {
                        c["id"]: c["default"]
                        for c in CLICKS[cid]["controls"]}}
            if elf and elf.is_file():
                t.kept_elf = elf
            self.twins.append(t)
            self.selected = t.key
            if len(self.twins) == 2:
                # A second board: the circuit runs in DC now
                for o in self.twins:
                    if o is self.analog_twin and o.elf:
                        await o.run(o.elf)
            self._changed()
        await t.start(elf)
        return t

    async def remove_board(self, key: str) -> None:
        t = self.twin(key)
        if not t:
            return
        await t.stop()
        async with self.lock:
            self.twins.remove(t)
            if self.selected == key:
                self.selected = self.twins[0].key if self.twins else None
            self._changed()
        await self.bench_frame(None)

    async def move_board(self, key: str, at: list[float]) -> None:
        t = self.twin(key)
        if not t:
            return
        t.at = [float(at[0]), float(at[1])]
        self._changed()

    async def select_board(self, board: str | None) -> None:
        """Make @board the selected board's (None: no boards at all); the
        bench stays."""
        if board is None:
            for t in list(self.twins):
                await t.stop()
            self.twins, self.selected = [], None
            self._changed()
            await self.bench_frame(None)
            return
        cur = self.primary
        if cur and cur.board["id"] == board:
            self.emit(self.workspace_message())
            self.emit(self.status_message())
            return
        if not (BOARDS / board / "board.json").is_file():
            return
        if not cur:
            await self.add_board(board)
            return
        print(f"[pp_web] board {cur.key}: {cur.board['id']} -> {board}",
              flush=True)
        await cur.stop()
        async with self.lock:
            t = Twin(self, cur.key, board, cur.at)
            self.twins[self.twins.index(cur)] = t
            self.selected = t.key
            self._changed()
        await t.start()

    async def apply_template(self, tid: str) -> None:
        """Set the workspace up as template @tid says: one board, its
        Clicks, the bench (replacing the workspace's) and firmware (the
        board's own if that is not built)."""
        tpl = next((x for x in TEMPLATES if x["id"] == tid), None)
        if not tpl or not (BOARDS / tpl["board"] / "board.json").is_file():
            return
        fw = ROOT / tpl["firmware"] if tpl.get("firmware") else None
        for t in list(self.twins):
            await t.stop()
        self.twins, self.selected = [], None
        self.load_bench({})
        t = Twin(self, "b1", tpl["board"])
        for slot, cid in (tpl.get("clicks") or {}).items():
            if slot in t.clicks and cid in CLICKS:
                t.clicks[slot] = {"id": cid, "values": {
                    c["id"]: c["default"] for c in CLICKS[cid]["controls"]}}
        t.kept_elf = fw if fw and fw.is_file() else None
        self.twins, self.selected = [t], t.key
        self.load_bench(tpl.get("bench") or {})
        self._changed()
        await self.bench_frame(None)
        await t.start(t.kept_elf)

    # -- the page -------------------------------------------------------------

    def emit(self, msg: dict) -> None:
        data = json.dumps(msg)
        for ws in list(self.clients):
            asyncio.create_task(self._send(ws, data))

    async def _send(self, ws: web.WebSocketResponse, data: str) -> None:
        try:
            await ws.send_str(data)
        except ConnectionError:
            self.clients.discard(ws)

    async def index(self, request: web.Request) -> web.StreamResponse:
        return web.FileResponse(STATIC / "index.html")

    async def board_file(self, request: web.Request) -> web.StreamResponse:
        """The selected board's files (?board=: that board's)"""
        name = request.match_info["name"]
        board = request.query.get("board") or (self.board or {}).get("id")
        if not board:
            raise web.HTTPNotFound()
        root = (BOARDS / board).resolve()
        path = (root / name).resolve()
        if root.parent != BOARDS.resolve() or path.parent != root or \
                not path.is_file():
            raise web.HTTPNotFound()
        return web.FileResponse(path)

    async def board_picture(self, request: web.Request) -> web.StreamResponse:
        """Any board's picture (the palette shows them all)."""
        board = request.match_info["board"]
        path = (BOARDS / board / request.match_info["name"]).resolve()
        if path.parent.parent != BOARDS.resolve() or \
                path.suffix.lower() not in BOARD_IMAGE or not path.is_file():
            raise web.HTTPNotFound()
        return web.FileResponse(path)

    async def api_board(self, request: web.Request) -> web.Response:
        """{"board": id or null}: the selected board becomes that one (null:
        no boards); with "add": true, another board joins the workspace."""
        try:
            req = await request.json()
        except ValueError:
            raise web.HTTPBadRequest(text="expected JSON") from None
        board = req.get("board")
        if board is not None and \
                not (BOARDS / str(board) / "board.json").is_file():
            raise web.HTTPBadRequest(text=f"no board {board}")
        if board and req.get("add"):
            await self.add_board(str(board))
        else:
            await self.select_board(board and str(board))
        return web.json_response(self.state())

    def state(self) -> dict:
        t = self.primary
        return {"board": t and t.board["id"], "name": t and t.board["name"],
                "machine": t and t.board["machine"],
                "elf": t.session.elf.name if t and t.session else None,
                "twins": [{"key": o.key, "board": o.board["id"],
                           "name": o.board["name"],
                           "elf": o.session.elf.name if o.session else None}
                          for o in self.twins]}

    async def api_state(self, request: web.Request) -> web.Response:
        return web.json_response(self.state())

    async def firmware(self, request: web.Request) -> web.Response:
        data = await request.content.read(MAX_ELF + 1)
        if len(data) > MAX_ELF or data[:4] != b"\x7fELF":
            raise web.HTTPBadRequest(text="expected an ELF file up to 4 MiB")
        # ?twin=: the board it is for (else the selected one); ?name=: the
        # file name the page shows; ?debug=<port>: start paused with the
        # gdbstub on 127.0.0.1:<port> (the VS Code extension's debugging)
        t = self.twin(request.query.get("twin")) or self.primary
        if not t:
            raise web.HTTPBadRequest(text="no board on the workspace")
        name = Path(request.query.get("name") or "firmware.elf").name
        elf = self.tmp / t.key / (name if name.endswith(".elf")
                                  else "firmware.elf")
        elf.parent.mkdir(exist_ok=True)
        elf.write_bytes(data)
        debug = request.query.get("debug")
        if debug is not None and not (debug.isdigit() and
                                      0 < int(debug) < 65536):
            raise web.HTTPBadRequest(text="debug must be a TCP port")
        if debug is None:
            t.keep_firmware(elf)
        ok = await t.run(elf, int(debug) if debug else None)
        if not ok:
            raise web.HTTPInternalServerError(text="the twin did not start")
        return web.json_response({"ok": True})

    async def ws(self, request: web.Request) -> web.WebSocketResponse:
        ws = web.WebSocketResponse(heartbeat=20)
        await ws.prepare(request)
        self.clients.add(ws)
        await ws.send_str(json.dumps(self.workspace_message()))
        await ws.send_str(json.dumps(self.bench_message()))
        if self.bench_view:
            await ws.send_str(self.bench_view)
        if not self.twins:
            await ws.send_str(json.dumps(self.status_message()))
        for t in self.twins:
            for m in t.snapshot():
                await ws.send_str(m)
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                continue
            req = json.loads(msg.data)
            if req.get("type") == "timeline":
                # The asking page's own answer, worked out off the loop
                reply = await asyncio.to_thread(self.timeline_message, req)
                await self._send(ws, json.dumps(reply))
                continue
            await self.request(req)
        self.clients.discard(ws)
        return ws

    async def request(self, req: dict) -> str | None:
        """One message from the page; what was wrong with it, if anything"""
        kind = req.get("type")
        t = self.twin(req.get("twin"))
        if kind == "select-board":
            b = req.get("board")
            await self.select_board(str(b) if b else None)
        elif kind == "add-board":
            at = req.get("at")
            await self.add_board(str(req.get("board")),
                                 at if isinstance(at, list) and
                                 len(at) == 2 else None)
        elif kind == "remove-board":
            await self.remove_board(str(req.get("twin")))
        elif kind == "move-board":
            at = req.get("at")
            if isinstance(at, list) and len(at) == 2:
                await self.move_board(str(req.get("twin")), at)
        elif kind == "select":
            if t:
                self.selected = t.key
                self.save_workspace()
        elif kind == "template":
            await self.apply_template(str(req.get("id")))
        elif kind == "bench":
            await self.set_bench(req.get("bench") or {})
        elif kind == "bench-prop":
            await self.bench_prop(str(req.get("id")), str(req.get("prop")),
                                  req.get("value"))
        elif kind in ("click", "control", "click-text") or \
                (kind == "stick" and not req.get("part")):
            slot = str(req.get("slot"))
            t = t if t and (slot in t.clicks or slot in t.wired()) \
                else self.owner(slot)
            if not t:
                return
            if kind == "click":
                await t.plug(slot, req.get("click"))
            elif kind == "control":
                await t.control(slot, str(req.get("id")),
                                req.get("value", 0))
            elif kind == "click-text":
                await t.click_text(slot, str(req.get("id")),
                                   str(req.get("text", ""))[:200])
            else:
                await t.stick(slot, str(req.get("id")),
                              float(req.get("x", 0)), float(req.get("y", 0)),
                              req.get("pressed"))
        else:
            t = t or self.primary
            if not t:
                return
            if kind == "stick":
                await t.part_stick(str(req.get("part")), str(req.get("id")),
                                   float(req.get("x", 0)),
                                   float(req.get("y", 0)))
            elif kind == "save-file":
                await t.save_file(str(req.get("path", "")),
                                  str(req.get("text", "")))
            elif kind == "dgi":
                t.dgi_wanted = [int(i) for i in req.get("enable", [])]
                if t.dgi:
                    t.dgi.set_wanted(t.dgi_wanted)
            elif kind == "button" and t.session:
                btn = next((b for b in t.board["buttons"]
                            if b["action"] == req.get("id")), {})
                await t.session.press(str(req.get("id")),
                                      bool(req.get("pressed")),
                                      btn.get("property"))
            elif kind == "uart" and t.session:
                await t.session.uart_send(str(req.get("data", "")))
            elif kind == "faults":
                try:
                    await t.set_faults(list(req.get("faults") or []))
                except ValueError as e:
                    t.emit({**t.faults_message(), "error": str(e)})
                    return str(e)
            elif kind in ("record", "replay"):
                try:
                    if kind == "replay":
                        await t.replay(str(req.get("name", "")),
                                       bool(req.get("pace", True)))
                    elif req.get("action") == "stop":
                        t.record_stop(req.get("name"))
                    else:
                        await t.record_start()
                except ValueError as e:
                    t.emit({**t.recorder_message(), "error": str(e)})
                    return str(e)
        return None

    # -- the timeline and the assistants' view ------------------------------

    def timeline_message(self, req: dict) -> dict:
        """Board @req["twin"]'s timeline (the selected board's without):
        @req["rows"] (else the suggested ones) from t1 - span to t1 (ns;
        t1 None: now), for a view @req["width"] px wide. "suggest": true
        adds the rows worth showing."""
        t = self.twin(req.get("twin")) or self.primary
        base = {"type": "timeline", "twin": t.key if t else None}
        if not t:
            return {**base, "available": False, "rows": []}
        store = t.session.trace_store if t.session else None
        suggested = tl.suggest(t.board, t.clicks, t.wired(), store)
        if not store:
            return {**base, "available": False, "rows": [],
                    "suggested": suggested}
        now = store.now
        t1 = now if req.get("t1") is None else min(int(req["t1"]), now)
        span = max(1000, int(req.get("span") or 2_000_000_000))
        t0 = max(0, t1 - span)
        rows = req.get("rows") or suggested
        return {**base, "available": True, "now": now, "start": store.start,
                "t0": t0, "t1": t1, "lost": store.lost,
                "rows": tl.build(store, rows, t0, t1,
                                 int(req.get("width") or 1000)),
                **({"suggested": suggested} if req.get("suggest") else {})}

    def bench_state(self) -> dict:
        """The workspace for an assistant: each board's summary, the parts
        on the bench, what can be added"""
        return {"selected": self.selected,
                "boards": [t.summary() for t in self.twins],
                "parts": [{k: v for k, v in p.items() if k != "pressed"}
                          for p in self.bench.data["parts"]],
                "wires": self.bench.data["wires"],
                "board_types": sorted(p.parent.name for p in
                                      BOARDS.glob("*/board.json")),
                "clicks": sorted(CLICKS),
                "templates": [t["id"] for t in TEMPLATES],
                "faults_help": FAULT_KINDS}

    async def api_request(self, request: web.Request) -> web.Response:
        """Any message the page sends ({"type": ...}), from a script or an
        assistant; the reply is the workspace as bench_state() has it"""
        try:
            req = await request.json()
        except ValueError:
            raise web.HTTPBadRequest(text="expected JSON") from None
        if not isinstance(req, dict) or not req.get("type"):
            raise web.HTTPBadRequest(text="expected {\"type\": ...}")
        err = await self.request(req)
        if err:
            raise web.HTTPBadRequest(text=err)
        return web.json_response(self.bench_state())

    async def api_bench(self, request: web.Request) -> web.Response:
        return web.json_response(self.bench_state())

    async def api_timeline(self, request: web.Request) -> web.Response:
        """POST the timeline request as JSON (see timeline_message())"""
        try:
            req = await request.json() if request.can_read_body else {}
        except ValueError:
            raise web.HTTPBadRequest(text="expected JSON") from None
        msg = await asyncio.to_thread(self.timeline_message, req)
        return web.json_response(msg)

    async def api_recordings(self, request: web.Request) -> web.Response:
        """?twin=: the recordings of a board's type"""
        t = self.twin(request.query.get("twin")) or self.primary
        return web.json_response({"twin": t.key if t else None,
                                  "recordings": t.recordings() if t else []})

    async def api_uart(self, request: web.Request) -> web.Response:
        """?twin=&tail=: the console of a board, its last characters;
        ?since=<mark>: what came after that mark (as much as is kept).
        "mark": the mark now (characters ever received)."""
        t = self.twin(request.query.get("twin")) or self.primary
        if not t:
            return web.json_response({"twin": None, "text": "", "mark": 0})
        since = request.query.get("since")
        if since is not None:
            n = max(0, t.uart_total - int(since))
            text = t.uart_log[-n:] if n else ""
        else:
            text = t.uart_log[-int(request.query.get("tail") or 4000):]
        return web.json_response({"twin": t.key, "text": text,
                                  "mark": t.uart_total})

    def engine_file(self) -> Path | None:
        return engines_dir() / f"{os.getpid()}.json" if self.port else None

    def announce(self) -> None:
        """Say where this engine listens, for the MCP server"""
        path = self.engine_file()
        if not path:
            return
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps({
                "pid": os.getpid(), "port": self.port,
                "url": f"http://127.0.0.1:{self.port}",
                "workspace": str(self.workspace_file),
                "watch": str(self.watch) if self.watch else None}),
                encoding="utf-8")
        except OSError:
            pass

    # -- a watched project ------------------------------------------------------

    def build_is_new(self, elf: Path) -> bool:
        """Whether @elf, the watched project's newest build, is new to the
        workspace: built since it last looked (a first workspace has seen
        nothing; a workspace that looked at another folder keeps its own)"""
        seen = self.watch_seen
        if not seen or seen.get("folder") != str(self.watch):
            return self.first_run
        return seen.get("build") != build_stamp(elf)

    def saw_build(self, elf: Path) -> None:
        self.watch_seen = {"folder": str(self.watch),
                           "build": build_stamp(elf)}
        self.save_workspace()

    async def _watch(self, fresh: Path | None = None) -> None:
        """Run each new build of the watched project once it is complete
        (unchanged for one poll); a copy, so the linker can overwrite it.
        @fresh: the newest build, new to the workspace, runs first (unless
        the board was given --elf)."""
        seen = pending = None
        # The walk can take a while on a big workspace: off the event loop
        first = await asyncio.to_thread(newest_elf, self.watch)
        if first:
            seen = (first, first.stat().st_mtime_ns, first.stat().st_size)
            if first == fresh and self.elf is None:
                await self._load(first)
            else:
                self.saw_build(first)
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
        self.saw_build(elf)
        dest = self.tmp / "watch" / elf.name
        dest.parent.mkdir(exist_ok=True)
        shutil.copyfile(elf, dest)
        if self.primary:
            self.primary.emit({"type": "status", "state": "idle",
                               "text": f"new build: {elf.name}"})
        await self.run(dest)
        self.keep_firmware(elf)         # the board runs it again next time

    async def on_startup(self, app: web.Application) -> None:
        self.announce()
        await self.bench_frame(None)
        if not self.twins:
            self.emit(self.status_message())
        # Every board starts where it left off, except that the selected
        # board takes a watched project's build that is new to the
        # workspace (built while the bench was closed): the watcher runs it
        build = self.watch and await asyncio.to_thread(newest_elf, self.watch)
        fresh = build if build and self.build_is_new(build) else None
        for t in self.twins:
            if t is self.primary and fresh and t.elf is None:
                continue
            await t.start(t.elf)
        if self.watch:
            self.watcher = asyncio.create_task(self._watch(fresh))

    async def on_cleanup(self, app: web.Application) -> None:
        path = self.engine_file()
        if path:
            path.unlink(missing_ok=True)
        if getattr(self, "watcher", None):
            self.watcher.cancel()
        for t in self.twins:
            await t.stop()
        self._stop_analog()


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


def make_app(board: str | None = None, elf: Path | None = None,
             watch: Path | None = None, bench: Path | None = None,
             workspace: Path | None = None,
             port: int | None = None) -> web.Application:
    state = App(board, elf, watch, bench, workspace, port)
    app = web.Application(client_max_size=MAX_ELF + 1024,
                          middlewares=[revalidate])
    app.add_routes([
        web.get("/", state.index),
        web.get("/board/{name}", state.board_file),
        web.post("/api/firmware", state.firmware),
        web.post("/api/board", state.api_board),
        web.get("/api/state", state.api_state),
        web.get("/api/bench", state.api_bench),
        web.post("/api/request", state.api_request),
        web.post("/api/timeline", state.api_timeline),
        web.get("/api/uart", state.api_uart),
        web.get("/api/recordings", state.api_recordings),
        web.get("/boards/{board}/{name}", state.board_picture),
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
    ap.add_argument("--board", default=None,
                    help="board to open (default: the workspace's)")
    ap.add_argument("--elf", type=Path, help="firmware to start with")
    ap.add_argument("--watch", type=Path, metavar="PATH",
                    help="project folder (or ELF) whose new builds to run")
    ap.add_argument("--bench", type=Path, metavar="JSON",
                    help="parts bench to start with (see bench.py)")
    ap.add_argument("--workspace", type=Path, metavar="JSON",
                    help="workspace file (default: workspace.json in "
                         "PP_WEB_BENCH_DIR or ~/.pp_web)")
    args = ap.parse_args(argv)
    web.run_app(make_app(args.board, args.elf, args.watch, args.bench,
                         args.workspace, args.port),
                host=args.host, port=args.port)
    return 0
