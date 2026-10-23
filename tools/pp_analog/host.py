# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The analog host: a circuit in ngspice, in lockstep with a twin's virtual
time over pp-qemu's co-simulation port (pp_cosim.c has the protocol).

QEMU always speaks first and waits for the reply. The transient runs on
ngspice's background thread, and the protocol lives in its sync callback,
called before every time step:

- while the twin has asked for nothing, it reads the twin's next message
  (QEMU runs meanwhile; ngspice waits);
- pad changes in a message become breakpoints, and each step is
  shortened to land on the next one, so ngspice lands on every edge (a
  driven pad's edge ramps over RAMP_S) even when the step was chosen
  before the change arrived;
- a request names a virtual time: ngspice steps up to it, never past it
  (each step is shortened to land on it), and the answer comes from that
  accepted time point. A request for a time ngspice has already passed (a
  tick overtook a conversion) is interpolated from recent points.

So QEMU and ngspice take turns, and the result does not depend on host
timing. If ngspice stops (a convergence failure), the host keeps answering
with the last voltages, so the twin never hangs, and reports the error.

For a live view (the engine's bench), set_control() changes a "*pp
control" value while the circuit runs, and on_state, when set, gets every
vector's average over the virtual time since the last call, about
state_hz times a second of host time.
"""

from __future__ import annotations

import bisect
import heapq
import socket
import struct
import threading
import time
from collections import deque
from dataclasses import dataclass, field

from . import circuit as circ
from .ngspice import NgSpice

EPS_S = 0.5e-9          # a request within this of ngspice's time is met
HYST_HI, HYST_LO = 0.6, 0.4     # input thresholds, fractions of VDD
DRIVEN = "hl"


class LinkClosed(Exception):
    pass


class Link:
    """Length-prefixed text messages on a socket."""

    def __init__(self, sock: socket.socket, stop: threading.Event,
                 log=None):
        self.sock, self.stop, self.log = sock, stop, log
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(0.2)

    def _exact(self, n: int) -> bytes:
        buf = b""
        while len(buf) < n:
            if self.stop.is_set():
                raise LinkClosed
            try:
                chunk = self.sock.recv(n - len(buf))
            except socket.timeout:
                continue
            except OSError as e:
                raise LinkClosed from e
            if not chunk:
                raise LinkClosed
            buf += chunk
        return buf

    def recv(self) -> list[str]:
        n = struct.unpack("<I", self._exact(4))[0]
        lines = self._exact(n).decode(errors="replace").splitlines()
        if self.log:
            self.log.writelines(f"> {ln}\n" for ln in lines)
        return lines

    def send(self, lines: list[str]) -> None:
        data = "".join(f"{ln}\n" for ln in lines).encode()
        if self.log:
            self.log.writelines(f"< {ln}\n" for ln in lines)
        try:
            self.sock.sendall(struct.pack("<I", len(data)) + data)
        except OSError as e:
            raise LinkClosed from e


@dataclass
class Stats:
    samples: int = 0
    ticks: int = 0
    events: int = 0
    steps: int = 0
    errors: list[str] = field(default_factory=list)


class Host:
    """Serves one twin with @circuit. @tick_ns: how often the twin brings
    the circuit up to date for its input pads (0: never)."""

    def __init__(self, circuit: circ.Circuit, tick_ns: int = 1_000_000,
                 tmax: float = circ.TMAX_S, log=None, on_state=None,
                 state_hz: float = 30.0):
        self.circuit = circuit
        self.controls = dict(circuit.controls)
        self.on_state = on_state        # (t, {vector: average}) -> None
        self.state_s = 1 / state_hz
        self.state_due = 0.0
        self.area: dict[str, float] = {}
        self.area_t = 0.0               # ngspice time the areas start at
        self.log = log              # a text file: the messages, both ways
        self.tick_ns = tick_ns
        self.tmax = tmax
        self.stop = threading.Event()
        self.stats = Stats()
        self.vdd = 3.3
        self.t0 = 0                 # virtual ns at ngspice's time 0
        self.now = 0.0              # ngspice's last accepted time (s)
        self.values: dict[str, float] = {}
        self.recent: deque[tuple[float, dict[str, float]]] = deque(maxlen=64)
        self.pads: dict[str, list[tuple[int, str]]] = {
            p: [(0, "z")] for p in circuit.pads}
        self.times: dict[str, list[int]] = {p: [0] for p in circuit.pads}
        self.pad_of = {n: p for p, n in circuit.node.items()}
        self.levels: dict[str, int] = {}
        self.probes: dict[int, str] = {}        # id -> node
        self.demand: tuple[str, int, int] | None = None     # kind, t, id
        self.edges: list[float] = []    # ngspice times of coming edges
        self.link: Link | None = None
        self.ng = NgSpice(self._on_sync, self._on_vsrc, self._on_data)
        # Load now, so a bad netlist fails before any twin starts
        self.ng.load(circuit.lines(tmax=tmax))

    # -- the twin's side --------------------------------------------------

    def config(self, machine: str) -> list[str]:
        """The reply to the twin's hello: the pads to watch, the ADC inputs
        the circuit drives, the tick."""
        lines = [f"watch {p}" for p in self.circuit.pads]
        ain = circ.adc_inputs(circ.machine_device(machine))
        for pad in self.circuit.pads:
            for path, ch in ain.get(pad, []):
                pid = len(self.probes)
                self.probes[pid] = self.circuit.node[pad]
                lines.append(f"probe {pid} {path} {ch}")
        return lines + [f"tick {self.tick_ns}", "ok"]

    def serve(self, sock: socket.socket) -> None:
        """Serve the twin connected on @sock until it goes away."""
        self.link = link = Link(sock, self.stop, self.log)
        try:
            hello = link.recv()
            words = hello[-1].split() if hello else []
            if len(words) < 4 or words[0] != "hello":
                raise LinkClosed
            self.vdd = int(words[3]) / 1000
            link.send(self.config(words[2]))
            msg = link.recv()
            words = msg[-1].split() if msg else []
            if len(words) != 2 or words[0] != "init":
                raise LinkClosed
            self.t0 = int(words[1])
            for line in msg[:-1]:           # the states at t0
                w = line.split()
                if w[0] == "e" and w[2] in self.pads:
                    self.pads[w[2]] = [(self.t0, w[3])]
            self.times = {p: [self.t0] for p in self.pads}
            link.send(["ok"])
        except (LinkClosed, IndexError, ValueError):
            return
        self.ng.start()
        while not self.stop.is_set():
            if self.ng.ended.wait(0.1):
                break
        if self.stop.is_set():
            self.ng.halt()
            return
        # ngspice stopped by itself: report it, keep the twin running
        self.stats.errors += self.ng.errors() or ["ngspice stopped"]
        self._frozen()

    def _frozen(self) -> None:
        try:
            while True:
                if self.demand is None:
                    self._read()
                self._answer(self.demand, self.now)
                self.demand = None
        except LinkClosed:
            pass

    def _read(self) -> None:
        """The twin's next message: its pad changes, and its request."""
        for line in self.link.recv():
            w = line.split()
            if not w:
                continue
            if w[0] == "e":
                self._event(int(w[1]), w[2], w[3])
            elif w[0] == "s":
                self.demand = ("s", int(w[1]), int(w[2]))
            elif w[0] in ("t", "f"):
                self.demand = (w[0], int(w[1]), 0)

    def _event(self, t_ns: int, pad: str, state: str) -> None:
        if pad not in self.pads:
            return
        self.stats.events += 1
        if len(self.times[pad]) > 1024:
            self._prune(pad)
        prev = self.pads[pad][-1][1]
        self.pads[pad].append((t_ns, state))
        self.times[pad].append(t_ns)
        te = (t_ns - self.t0) * 1e-9
        if te > self.now + EPS_S:
            times = [te]
            if prev in DRIVEN and state in DRIVEN:
                times.append(te + circ.RAMP_S)
            for tb in times:
                self.ng.breakpoint(tb)
                heapq.heappush(self.edges, tb)

    def _prune(self, pad: str) -> None:
        """Forget @pad's changes before the one in force at ngspice's
        time (ngspice never steps back)."""
        i = bisect.bisect_right(self.times[pad],
                                self.t0 + self.now * 1e9) - 1
        if i > 1:
            del self.pads[pad][:i - 1]
            del self.times[pad][:i - 1]

    def _answer(self, demand: tuple[str, int, int], t: float) -> None:
        kind, t_ns, pid = demand
        if kind == "s":
            self.stats.samples += 1
            v = self._value_at(self.probes.get(pid, ""),
                               (t_ns - self.t0) * 1e-9)
            self.link.send([f"v {round(v * 1e6)}"])
        elif kind == "t":
            self.stats.ticks += 1
            self.link.send(self._inputs() + ["ok"])
        else:
            self.link.send(["ok"])

    def _value_at(self, node: str, t: float) -> float:
        """@node's voltage at @t: the last accepted point, or between two
        recent ones for a time ngspice has passed."""
        if t >= self.now - EPS_S or not self.recent:
            return self.values.get(node, 0.0)
        pts = self.recent
        if t <= pts[0][0]:
            return pts[0][1].get(node, 0.0)
        for (ta, va), (tb, vb) in zip(pts, list(pts)[1:]):
            if ta <= t <= tb:
                a, b = va.get(node, 0.0), vb.get(node, 0.0)
                return a + (b - a) * (t - ta) / (tb - ta) if tb > ta else b
        return self.values.get(node, 0.0)

    def _inputs(self) -> list[str]:
        """The level each pad the MCU does not drive reads, per group."""
        groups: dict[str, list[int]] = {}
        for pad in self.circuit.pads:
            g = groups.setdefault(pad[1].lower(), [0, 0])
            if self.pads[pad][-1][1] in DRIVEN:
                continue
            v = self.values.get(self.circuit.node[pad], 0.0)
            prev = self.levels.get(pad, 0)
            lvl = 1 if v > HYST_HI * self.vdd else \
                0 if v < HYST_LO * self.vdd else prev
            self.levels[pad] = lvl
            bit = 1 << int(pad[2:])
            g[0] |= bit
            g[1] |= bit if lvl else 0
        return [f"in {g} {m:x} {lv:x}" for g, (m, lv) in sorted(groups.items())]

    # -- ngspice's callbacks (its thread) ------------------------------------

    def _on_sync(self, t: float, delta: float) -> float | None:
        self.stats.steps += 1
        if self.stop.is_set():
            return None
        try:
            while True:
                if self.demand is None:
                    self._read()
                    continue
                kind, t_ns, _ = self.demand
                target = (t_ns - self.t0) * 1e-9
                if kind == "f" or t >= target - EPS_S:
                    self._answer(self.demand, t)
                    self.demand = None
                    continue
                while self.edges and self.edges[0] <= t + EPS_S:
                    heapq.heappop(self.edges)
                if self.edges:
                    target = min(target, self.edges[0])
                return min(delta, target - t)
        except LinkClosed:
            self.stop.set()
            return None

    def set_control(self, name: str, value: float) -> None:
        """A live control's new value (from the next time step on)."""
        self.controls[name.lower()] = value

    def _on_vsrc(self, name: str, t: float) -> float:
        if name == "v_pp_vdd":
            return self.vdd
        if name.startswith("vx_"):
            return self.controls.get(name[3:], 0.0)
        pad = self.pad_of.get(name[3:])
        if pad is None:
            return 0.0
        t_ns = self.t0 + t * 1e9
        i = bisect.bisect_right(self.times[pad], t_ns) - 1
        hist = self.pads[pad]
        te, state = hist[max(i, 0)]
        if name[1] == "g":
            return 1 / circ.R_PAD if state in DRIVEN else \
                1 / circ.R_PULL if state in "ud" else 0.0
        v = self.vdd if state in "hu" else 0.0
        if i > 0 and state in DRIVEN and hist[i - 1][1] in DRIVEN:
            ramp = (t_ns - te) * 1e-9 / circ.RAMP_S
            if ramp < 1:
                prev = self.vdd if hist[i - 1][1] == "h" else 0.0
                v = prev + (v - prev) * max(ramp, 0.0)
        return v

    def _on_data(self, values: dict[str, float]) -> None:
        t = values.get("time", self.now)
        if self.on_state:
            self._accumulate(t, values)
        self.now = t
        self.values = values
        self.recent.append((self.now, values))

    def _accumulate(self, t: float, values: dict[str, float]) -> None:
        """Integrate every vector over virtual time (trapezoids), and hand
        the averages to on_state when it is due."""
        dt = t - self.now
        if dt > 0 and self.values:
            area, prev = self.area, self.values
            for k, v in values.items():
                area[k] = area.get(k, 0.0) + (v + prev.get(k, v)) * 0.5 * dt
        wall = time.monotonic()
        if wall >= self.state_due and t > self.area_t:
            span = t - self.area_t
            avg = {k: a / span for k, a in self.area.items()} or \
                dict(values)
            self.on_state(t, avg)
            self.area, self.area_t = {}, t
            self.state_due = wall + self.state_s
