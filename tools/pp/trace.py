# Copyright (c) 2026 Clifford Swartz.
"""What the twin's event recorder saw.

pp-qemu's recorder (the machine's "trace=<chardev>", hw/misc/pp_trace.c)
streams every pad's level, the SERCOMs' traffic and the exceptions, each
stamped with virtual time. Parser turns the stream into records, Store
keeps the latest of them, and the decoders read a Window of them the way a
logic analyzer would: pad edges, UART text, SPI transfers, I2C transactions
and interrupts. The web page's timeline and the MCP tools share them.

A record is (t, kind, src, a, b): t in virtual ns, src a source id that
Store.names names ("PORT", "NVIC", "sercom2", "usart1"), a and b as the
kind says (include/hw/misc/pp_trace.h in pp-qemu). The stream's HELLO says
how pads are numbered (32 a group on Arm parts, PA05; 8 on AVR, PB3) and
interrupts (Cortex-M exception numbers, or AVR vectors).
"""

from __future__ import annotations

import array
import bisect
import functools
import hashlib
import struct
import subprocess
from collections import Counter
from pathlib import Path

REC = struct.Struct("<QBBHI")

(PAD, PAD_BUSY, UART_TX, UART_RX, SPI, I2C_START, I2C_BYTE, I2C_HOST_NACK,
 I2C_STOP, IRQ_PEND, IRQ_ENTER, IRQ_EXIT, RESET, INPUT,
 INPUT_ERROR) = range(1, 16)
HELLO, NAME, LOST, TIME = 0xF0, 0xF1, 0xF2, 0xF3

WINDOW_NS = 1_000_000           # a PAD_BUSY record's window (pp_trace.c)
# The stream is in time order but for busy-pad records, stamped with their
# window, which the recorder's 20 ms tick may close late
DISORDER_NS = 50_000_000
LEAD_NS = 50_000_000            # a window also reads this much before it
CHUNK = 8192                    # records a chunk of the store
SRC_PORT, SRC_NVIC = 0, 1

# Cortex-M system exceptions, by exception number
SYSTEM_EXCEPTIONS = {2: "NMI", 3: "HardFault", 11: "SVCall", 14: "PendSV",
                     15: "SysTick"}


GROUPS = "ABCDEF"
VECTORS = 1 << 8                # HELLO: interrupts are AVR vectors


def pad_name(pin: int, group: int = 32) -> str:
    """5 -> "PA05", 42 -> "PB10"; with 8 pads a group 11 -> "PB3" """
    if group > 10:
        return f"P{GROUPS[pin // group]}{pin % group:02d}"
    return f"P{GROUPS[pin // group]}{pin % group}"


def pad_number(name: str, group: int = 32) -> int:
    """"PA05" -> 5, "PB10" -> 42; with 8 pads a group "PB3" -> 11"""
    return GROUPS.index(name[1].upper()) * group + int(name[2:])


@functools.lru_cache(maxsize=None)
def _machine_help(qemu: str, mtime: float, machine: str, env: tuple) -> str:
    try:
        return subprocess.run([qemu, "-M", f"{machine},help"],
                              capture_output=True, text=True, timeout=30,
                              env=dict(env)).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def machine_has_trace(qemu: Path, machine: str, env: dict) -> bool:
    """Whether @machine of the QEMU build @qemu has the recorder ("trace=")"""
    try:
        mtime = qemu.stat().st_mtime
    except OSError:
        return False
    return "trace=" in _machine_help(str(qemu), mtime, machine.split(",")[0],
                                     tuple(sorted(env.items())))


class Parser:
    """The recorder's stream in, records out; keeps the names, the lost
    count and the latest virtual time it has seen."""

    def __init__(self):
        self.buf = bytearray()
        self.names: dict[int, str] = {}
        self.now = 0
        self.lost = 0
        self.version: int | None = None
        self.group_pins = 32            # pads a group (HELLO)
        self.vectors = False            # interrupts are AVR vectors

    def feed(self, data: bytes) -> list[tuple[int, int, int, int, int]]:
        self.buf += data
        out = []
        buf, i, n = self.buf, 0, len(self.buf)
        while i + 16 <= n:
            t, kind, src, a, b = REC.unpack_from(buf, i)
            if kind == NAME:
                size = 16 + ((a + 15) & ~15)
                if i + size > n:
                    break
                self.names[src] = bytes(buf[i + 16:i + 16 + a]).decode(
                    "utf-8", "replace")
                i += size
                continue
            i += 16
            if kind != PAD_BUSY and t > self.now:
                self.now = t
            if kind == HELLO:
                self.version = a
                self.group_pins = b & 0xFF or 32
                self.vectors = bool(b & VECTORS)
            elif kind == LOST:
                self.lost += b
            elif kind != TIME:
                out.append((t, kind, src, a, b))
        del buf[:i]
        return out


class _Chunk:
    """Up to CHUNK records in columns, and every pad's level at its start"""

    __slots__ = ("t", "kind", "src", "a", "b", "tmin", "tmax", "levels")

    def __init__(self, levels: dict[int, int]):
        self.t = array.array("Q")
        self.kind = array.array("B")
        self.src = array.array("B")
        self.a = array.array("H")
        self.b = array.array("I")
        self.tmin, self.tmax = 1 << 64, 0
        self.levels = dict(levels)

    def add(self, t: int, kind: int, src: int, a: int, b: int) -> None:
        self.t.append(t)
        self.kind.append(kind)
        self.src.append(src)
        self.a.append(a)
        self.b.append(b)
        self.tmin, self.tmax = min(self.tmin, t), max(self.tmax, t)

    def __len__(self) -> int:
        return len(self.t)


class Store:
    """The latest records of one twin (at most @max_records, about 16 bytes
    each), searchable by time."""

    def __init__(self, max_records: int = 2_000_000):
        self.parser = Parser()
        self.max_chunks = max(2, max_records // CHUNK)
        self.chunks: list[_Chunk] = []
        self.levels: dict[int, int] = {}        # pad -> level now
        self.counts: Counter = Counter()        # (kind, src) -> records
        self.exceptions: set[int] = set()       # taken at least once
        self.pads: set[int] = set()             # changed after the start
        self.resets = 0
        # The reader's actions as they took effect: (t, id, ok)
        self.inputs: list[tuple[int, int, bool]] = []

    @property
    def names(self) -> dict[int, str]:
        return self.parser.names

    @property
    def now(self) -> int:
        return self.parser.now

    @property
    def lost(self) -> int:
        return self.parser.lost

    @property
    def group_pins(self) -> int:
        return self.parser.group_pins

    @property
    def vectors(self) -> bool:
        return self.parser.vectors

    def pad_name(self, pin: int) -> str:
        return pad_name(pin, self.group_pins)

    def pad_number(self, name: str) -> int:
        return pad_number(name, self.group_pins)

    @property
    def start(self) -> int:
        """The earliest time still kept"""
        return self.chunks[0].tmin if self.chunks else 0

    def source(self, name: str) -> int | None:
        """The id of source @name ("sercom2", "SERCOM2")"""
        name = name.lower()
        return next((i for i, n in self.names.items() if n.lower() == name),
                    None)

    def feed(self, data: bytes) -> None:
        for rec in self.parser.feed(data):
            self.add(*rec)

    def add(self, t: int, kind: int, src: int, a: int, b: int) -> None:
        first = not self.chunks
        if first or len(self.chunks[-1]) >= CHUNK:
            self.chunks.append(_Chunk(self.levels))
            if len(self.chunks) > self.max_chunks:
                del self.chunks[0]
        self.chunks[-1].add(t, kind, src, a, b)
        self.counts[(kind, src)] += 1
        if kind == PAD:
            if self.levels.get(a, b) != b:
                self.pads.add(a)
            self.levels[a] = b
        elif kind == PAD_BUSY:
            self.pads.add(a)
        elif kind == IRQ_ENTER:
            self.exceptions.add(a)
        elif kind == RESET:
            self.resets += 1
        elif kind in (INPUT, INPUT_ERROR):
            self.inputs.append((t, a | b << 16, kind == INPUT))

    def records(self, t0: int, t1: int) -> list[tuple[int, int, int, int, int]]:
        """The records from @t0 to @t1, in time order"""
        out = []
        for c in self.chunks:
            if c.tmax < t0 or c.tmin > t1:
                continue
            lo = bisect.bisect_left(c.t, t0 - DISORDER_NS)
            hi = bisect.bisect_right(c.t, t1 + DISORDER_NS)
            for i in range(lo, hi):
                t = c.t[i]
                if t0 <= t <= t1:
                    out.append((t, c.kind[i], c.src[i], c.a[i], c.b[i]))
        out.sort(key=lambda r: r[0])
        return out

    def level_at(self, pin: int, t: int) -> int | None:
        """The level of pad @pin at time @t (None: not known)"""
        k = next((k for k in range(len(self.chunks) - 1, -1, -1)
                  if self.chunks[k].tmin <= t), None)
        if k is None:
            return None
        c = self.chunks[k]
        level = c.levels.get(pin)
        for i in range(bisect.bisect_right(c.t, t + DISORDER_NS)):
            if c.kind[i] == PAD and c.a[i] == pin and c.t[i] <= t:
                level = c.b[i]
        return level

    def window(self, t0: int, t1: int) -> "Window":
        return Window(self, t0, t1)

    def activity(self) -> dict:
        """What has happened at all: the serial ports (SERCOMs, USARTs,
        SPIs, TWIs) by what they did, the exceptions taken, the pads that
        changed."""
        buses: dict[str, set] = {}
        for (kind, src), _ in self.counts.items():
            what = {UART_TX: "uart", UART_RX: "uart", SPI: "spi",
                    I2C_START: "i2c"}.get(kind)
            if what:
                buses.setdefault(self.names.get(src, f"src{src}"),
                                 set()).add(what)
        return {"buses": {n: sorted(k) for n, k in sorted(buses.items())},
                "exceptions": sorted(self.exceptions),
                "pads": [self.pad_name(p) for p in sorted(self.pads)]}


class Window:
    """The records from @t0 to @t1 (and LEAD_NS before, for what began
    earlier), read once for any number of decoders"""

    def __init__(self, store: Store, t0: int, t1: int):
        self.store, self.t0, self.t1 = store, t0, t1
        self.recs = store.records(max(0, t0 - LEAD_NS), t1)

    def of(self, src: int, kinds: tuple[int, ...]):
        return [r for r in self.recs if r[2] == src and r[1] in kinds]


# -- decoders ------------------------------------------------------------

def pad_trace(w: Window, pins: list[int], resolution: int = 0) -> dict:
    """Each pad's level at the window's start, its edges and where it
    toggled faster than @resolution ns: busy spans [t, end, high
    fraction]."""
    out = {}
    for pin in pins:
        level = w.store.level_at(pin, w.t0)
        edges, busy = [], []
        for t, kind, src, a, b in w.recs:
            if src != SRC_PORT or a != pin:
                continue
            if kind == PAD and t >= w.t0:
                edges.append([t, b])
            elif kind == PAD_BUSY and t + WINDOW_NS >= w.t0:
                busy.append([t, t + WINDOW_NS, round((b >> 24) / 255, 3)])
        if resolution:
            edges, merged = _squeeze(edges, resolution)
            busy = _merge_spans(busy + merged)
        out[w.store.pad_name(pin)] = {"level": level, "edges": edges,
                                      "busy": busy}
    return out


def _squeeze(edges: list[list[int]], resolution: int) -> tuple[list, list]:
    """Edges closer than @resolution become busy spans with their high
    fraction; the others stay edges."""
    keep, spans = [], []
    i, n = 0, len(edges)
    while i < n:
        j = i
        while j + 1 < n and edges[j + 1][0] - edges[j][0] < resolution:
            j += 1
        if j == i:
            keep.append(edges[i])
        else:
            start, end = edges[i][0], edges[j][0]
            high = sum(edges[k + 1][0] - edges[k][0]
                       for k in range(i, j) if edges[k][1])
            spans.append([start, end, round(high / max(1, end - start), 3)])
            keep.append([end, edges[j][1]])
        i = j + 1
    return keep, spans


def _merge_spans(spans: list[list]) -> list[list]:
    out: list[list] = []
    for s in sorted(spans):
        if out and s[0] <= out[-1][1]:
            last = out[-1]
            a, b = last[1] - last[0], s[1] - s[0]
            last[2] = round((last[2] * a + s[2] * b) / max(1, a + b), 3)
            last[1] = max(last[1], s[1])
        else:
            out.append(list(s))
    return out


def _text(data: bytes) -> str:
    return "".join(chr(c) if 32 <= c < 127 or c in (9, 10, 13)
                   else f"\\x{c:02x}" for c in data)


def uart(w: Window, src: int, gap: int = 2_000_000) -> list[dict]:
    """Runs of characters one way (tx: from the MCU, rx: to it), split at
    a pause longer than @gap ns or after a line feed."""
    runs: list[dict] = []
    cur = None
    for t, kind, s, a, b in w.of(src, (UART_TX, UART_RX)):
        if t < w.t0:
            continue
        way = "tx" if kind == UART_TX else "rx"
        if cur is None or cur["dir"] != way or t - cur["end"] > gap or \
                cur["_data"].endswith(b"\n"):
            cur = {"t": t, "end": t, "dir": way, "_data": bytearray()}
            runs.append(cur)
        cur["end"] = t
        cur["_data"].append(a & 0xFF)
    for r in runs:
        data = r.pop("_data")
        r["text"] = _text(data)
        r["bytes"] = len(data)
    return runs


def spi(w: Window, src: int, gap: int = 20_000,
        cs: int | None = None) -> list[dict]:
    """Transfers of the SPI host: characters out (mosi) and in (miso),
    split at a pause longer than @gap ns, or where pad @cs rises."""
    out: list[dict] = []
    cur = None
    for t, kind, s, a, b in w.recs:
        if t < w.t0:
            continue
        if cs is not None and kind == PAD and s == SRC_PORT and a == cs:
            if b:
                cur = None
            continue
        if s != src or kind != SPI:
            continue
        if cur is None or (cs is None and t - cur["end"] > gap):
            cur = {"t": t, "end": t, "mosi": [], "miso": []}
            out.append(cur)
        cur["end"] = t
        cur["mosi"].append(a)
        cur["miso"].append(b)
    return out


def i2c(w: Window, src: int) -> list[dict]:
    """Transactions of the I2C host, start to stop (end None: not stopped
    yet). Each is a list of segments, a repeated start beginning another,
    with the address, the direction, whether the address was NACKed, the
    data and the index of a data byte the device NACKed; host_nack: the
    host NACKed the last byte it read, as it should to end a read."""
    out: list[dict] = []
    cur = None
    for t, kind, s, a, b in w.of(src, (I2C_START, I2C_BYTE, I2C_HOST_NACK,
                                       I2C_STOP)):
        if kind == I2C_START:
            if cur is None:
                cur = {"t": t, "end": None, "segments": [],
                       "host_nack": False}
                out.append(cur)
            cur["segments"].append({"addr": a, "read": bool(b & 1),
                                    "nack": bool(b & 2), "data": [],
                                    "data_nack": None})
        elif cur is None:
            continue                    # began before what is kept
        elif kind == I2C_BYTE and cur["segments"]:
            seg = cur["segments"][-1]
            seg["data"].append(a)
            if b & 1 and seg["data_nack"] is None:
                seg["data_nack"] = len(seg["data"]) - 1
        elif kind == I2C_HOST_NACK:
            cur["host_nack"] = True
        elif kind == I2C_STOP:
            cur["end"] = t
            cur = None
    out = [tx for tx in out if (tx["end"] or w.t1) >= w.t0]
    for tx in out:
        tx["text"] = i2c_text(tx)
    return out


def i2c_text(tx: dict) -> str:
    """"0x18 W 05 | 0x18 R C1 90" for a transaction; NACK marks what was
    NACKed, "..." one that has not stopped."""
    parts = []
    for seg in tx["segments"]:
        p = f"0x{seg['addr']:02X} {'R' if seg['read'] else 'W'}"
        if seg["nack"]:
            p += " NACK"
        data = [f"{d:02X}" for d in seg["data"]]
        if seg["data_nack"] is not None:
            data[seg["data_nack"]] += " NACK"
        parts.append(" ".join([p, *data]))
    return " | ".join(parts) + ("" if tx["end"] else " ...")


def irqs(w: Window) -> dict[int, dict]:
    """Per exception number: the spans it ran [enter, exit] (exit None:
    still running) and how often it became pending."""
    out: dict[int, dict] = {}
    stack: list[tuple[int, int]] = []
    for t, kind, s, a, b in w.of(SRC_NVIC, (IRQ_PEND, IRQ_ENTER, IRQ_EXIT)):
        if kind == IRQ_ENTER:
            stack.append((a, t))
        elif kind == IRQ_EXIT:
            for k in range(len(stack) - 1, -1, -1):
                if stack[k][0] == a:
                    exc, start = stack.pop(k)
                    if t >= w.t0:
                        out.setdefault(exc, {"spans": [], "pends": 0})[
                            "spans"].append([max(start, w.t0), t])
                    break
        elif t >= w.t0:
            out.setdefault(a, {"spans": [], "pends": 0})["pends"] += 1
    for exc, start in stack:
        out.setdefault(exc, {"spans": [], "pends": 0})["spans"].append(
            [max(start, w.t0), None])
    return out


def exception_name(exc: int, device: dict | None,
                   vectors: bool = False) -> str:
    """"SysTick", "TC0" (an IRQ line the device names), or "IRQ 5"; with
    @vectors (AVR) @exc is the vector: "TCB0_INT", or "vector 13" """
    if vectors:
        line = exc
    elif exc < 16:
        return SYSTEM_EXCEPTIONS.get(exc, f"exception {exc}")
    else:
        line = exc - 16
    for irq in (device or {}).get("interrupts", []):
        if irq.get("line") == line:
            return irq["name"]
    return f"vector {line}" if vectors else f"IRQ {line}"


DIGEST_BUCKET_NS = 10_000_000


def digests(store: Store, t1: int,
            bucket: int = DIGEST_BUCKET_NS) -> list[list]:
    """Everything recorded up to @t1, @bucket ns at a time: [[bucket's
    start, hash, events]] for each bucket with events. Two runs that went
    the same way have the same digests; the first that differs says
    where they parted."""
    out: list[list] = []
    cur, h, n = None, None, 0
    for r in store.records(0, t1):
        b = r[0] // bucket
        if b != cur:
            if cur is not None:
                out.append([cur * bucket, h.hexdigest()[:16], n])
            cur, h, n = b, hashlib.sha1(), 0
        h.update(REC.pack(*r))
        n += 1
    if cur is not None:
        out.append([cur * bucket, h.hexdigest()[:16], n])
    return out


def compare(recorded: list[list], replayed: list[list]) -> dict:
    """{"same": bool, "events": compared, "from": t, "differs_at": the
    first bucket's start that differs (None: none)}, over the buckets both
    still have"""
    start = max(recorded[0][0] if recorded else 0,
                replayed[0][0] if replayed else 0)
    a = {b[0]: b for b in recorded if b[0] >= start}
    b = {x[0]: x for x in replayed if x[0] >= start}
    for t in sorted(set(a) | set(b)):
        if a.get(t) != b.get(t):
            return {"same": False, "from": start, "differs_at": t,
                    "events": sum(x[2] for x in a.values())}
    return {"same": True, "from": start, "differs_at": None,
            "events": sum(x[2] for x in a.values())}


def bursts(items: list[dict], limit: int) -> list[dict]:
    """At most @limit items: neighbours merge into {"t", "end", "count"}
    bursts (a wide view of a busy bus)."""
    if limit <= 0 or len(items) <= limit:
        return items
    per = -(-len(items) // limit)
    out = []
    for i in range(0, len(items), per):
        group = items[i:i + per]
        out.append({"t": group[0]["t"],
                    "end": max((g.get("end") or g["t"]) for g in group),
                    "count": len(group)})
    return out
