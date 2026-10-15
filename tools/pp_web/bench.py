# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The parts bench: everyday parts wired to the board's pads, solved as a DC
circuit each frame.

A bench is JSON: {"parts": [{"id", "type", "x", "y", <props>}], "wires":
[[end, end], ...], "anchors": {end: [x, y]}}. An end is a part terminal
("r1.a"), a pad ("pin:PA05"), a rail ("rail:3V3", "rail:5V", "rail:GND")
or a hole in one of the board's headers ("hole:J801.6", which the board
ties to a pad or rail: see header_holes()). Wires join ends into nets;
anchors are where the page draws a pad or rail tag. Positions (parts,
anchors) are millimetres on the page's stage, from its top-left corner.

Each frame the twin's pads become sources: a pad the MCU drives is VDD or
0 V behind its output resistance (a PWM pad is solved high and low and the
results weighted by its duty cycle); an input pad is open, with its pull
resistor if the PORT has it on. The solver is nodal analysis with Newton
steps for the diodes and LEDs. The results go two ways:

- to the page: LED currents (brightness), motor speed, servo angle, meter
  readings, pad currents and warnings (an LED with no resistor, a motor on
  a bare pad, a short);
- to the twin: each input pad wired to the bench reads the level its net
  settles at (the PORT's "pX-bench" layer, with hysteresis), and each pad
  with an ADC channel reads its net's voltage (the ADC's "ain").

Units: volts, amps, ohms. Not modelled: anything that changes in time
(capacitors, inductors, motor inertia); that is the analog step later.
"""

from __future__ import annotations

import itertools
import math
from dataclasses import dataclass, field

R_RAIL = 0.01           # a supply rail's output resistance
R_PAD = 40.0            # an MCU output pad's output resistance (typical)
R_PULL = 40e3           # an MCU pull resistor (typical)
G_MIN = 1e-9            # every net to ground, so floating nets solve
R_SWITCH = 0.05         # a closed button or switch
V_T = 0.05              # LED/diode n*Vt: 20 mA at Vf, about 3 decades/0.35 V
I_LED_RATED = 0.020     # full brightness
I_LED_MAX = 0.030       # above this an LED is overdriven
PWM_STATES = 5          # pads solved high/low; more pads use their average
SERVO_PERIOD = 0.020    # 50 Hz frame assumed for a servo signal
HYST_HI, HYST_LO = 0.6, 0.4     # input thresholds, fractions of VDD

# LED forward voltage at rated current, by colour
LED_VF = {"red": 1.9, "yellow": 2.0, "orange": 2.0, "green": 2.1,
          "blue": 2.9, "white": 2.9}
LED_RGB = {"red": "#ff2a1a", "yellow": "#ffd21a", "orange": "#ff8c1a",
           "green": "#2aff4a", "blue": "#2a7bff", "white": "#ffffff"}

# The part library: terminals, editable properties and their defaults
PARTS = {
    "resistor": {"name": "Resistor", "terminals": ["a", "b"],
                 "props": {"ohms": 330.0}},
    "led": {"name": "LED", "terminals": ["a", "k"],
            "props": {"color": "red"}},
    "rgb_led": {"name": "RGB LED (common cathode)",
                "terminals": ["r", "g", "b", "k"], "props": {}},
    "diode": {"name": "Diode", "terminals": ["a", "k"], "props": {}},
    "button": {"name": "Push button", "terminals": ["a", "b"],
               "props": {"pressed": False}},
    "switch": {"name": "Switch", "terminals": ["a", "b"],
               "props": {"on": False}},
    "pot": {"name": "Potentiometer", "terminals": ["a", "w", "b"],
            "props": {"ohms": 10e3, "position": 0.5}},
    "motor": {"name": "DC motor", "terminals": ["a", "b"],
              "props": {"ohms": 20.0, "rpm_per_volt": 1000.0}},
    "nmos": {"name": "N-MOSFET (logic level)", "terminals": ["g", "d", "s"],
             "props": {"vth": 1.5, "ron": 0.1}},
    "servo": {"name": "Servo", "terminals": ["sig", "vcc", "gnd"],
              "props": {}},
    "voltmeter": {"name": "Voltmeter", "terminals": ["p", "n"], "props": {}},
}
RAILS = {"3V3": None, "5V": 5.0, "GND": 0.0}      # 3V3 follows the board VDD


def pad_ref(pad: str) -> tuple[str, int]:
    """"PA05" -> ("a", 5)."""
    return pad[1].lower(), int(pad[2:])


def new_part(ptype: str, pid: str, x: float = 0, y: float = 0) -> dict:
    return {"id": pid, "type": ptype, "x": x, "y": y,
            **PARTS[ptype]["props"]}


def header_holes(board: dict) -> dict[str, dict]:
    """The holes of the board's headers (board.json "pin_headers"): {end:
    {"net", "label", "at_mm"}}, net a pad or rail end, at_mm in board
    coordinates. A header gives its first hole, the step to the next pin,
    the pins in order as [net, label] (net a pad name, GND, 3V3, 5V, or
    null for no connection), optionally "cols" (2: pins alternate between
    two columns "col_mm" apart) and "also_mm" (more holes per pin, offsets
    from the first: a header with a breakout row)."""
    holes = {}
    for h in board.get("pin_headers", []):
        cols = h.get("cols", 1)
        for i, (net, label) in enumerate(h["pins"]):
            if net in ("GND", "3V3", "5V"):
                end = f"rail:{net}"
            elif net:
                end = f"pin:{net}"
            else:
                end = None
            row, col = divmod(i, cols)
            x = h["first_mm"][0] + row * h["step_mm"][0] +                 col * h.get("col_mm", [0, 0])[0]
            y = h["first_mm"][1] + row * h["step_mm"][1] +                 col * h.get("col_mm", [0, 0])[1]
            for k, (dx, dy) in enumerate([(0, 0)] + h.get("also_mm", [])):
                key = f"hole:{h['id']}.{i + 1}" + ("" if not k else
                                                    "abcdefgh"[k - 1])
                holes[key] = {"net": end, "label": label,
                              "at_mm": [round(x + dx, 3), round(y + dy, 3)]}
    return holes


def clean(bench: dict, holes: dict | None = None) -> dict:
    """@bench with unknown parts, properties and dangling wires dropped,
    and every property of the right type (it comes from the page). Hole
    ends must be among @holes and connected."""
    holes = holes or {}
    parts, ids = [], set()
    for p in bench.get("parts", []):
        spec = PARTS.get(p.get("type"))
        pid = str(p.get("id", ""))
        if not spec or not pid or pid in ids or "." in pid or ":" in pid:
            continue
        ids.add(pid)
        part = {"id": pid, "type": p["type"],
                "x": float(p.get("x", 0)), "y": float(p.get("y", 0))}
        for k, default in spec["props"].items():
            v = p.get(k, default)
            if isinstance(default, bool):
                part[k] = bool(v)
            elif isinstance(default, float):
                try:
                    part[k] = float(v)
                except (TypeError, ValueError):
                    part[k] = default
            else:
                part[k] = v if isinstance(v, str) and v in LED_VF else default
        if part["type"] == "pot":
            part["position"] = min(1.0, max(0.0, part["position"]))
        for k in ("ohms", "ron", "rpm_per_volt"):
            if k in part:
                part[k] = max(part[k], 1e-3)
        parts.append(part)

    def ok(end: str) -> bool:
        if end.startswith("hole:"):
            return bool(holes.get(end, {}).get("net"))
        if end.startswith("pin:"):
            pad = end[4:]
            return len(pad) >= 3 and pad[0] == "P" and pad[1].isalpha() and \
                pad[2:].isdigit()
        if end.startswith("rail:"):
            return end[5:] in RAILS
        pid, _, term = end.partition(".")
        part = next((p for p in parts if p["id"] == pid), None)
        return bool(part) and term in PARTS[part["type"]]["terminals"]

    wires = []
    for w in bench.get("wires", []):
        if isinstance(w, (list, tuple)) and len(w) == 2:
            a, b = str(w[0]), str(w[1])
            if a != b and ok(a) and ok(b) and [a, b] not in wires and \
                    [b, a] not in wires:
                wires.append([a, b])
    anchors = {}
    for end, at in (bench.get("anchors") or {}).items():
        if (end.startswith("pin:") or end.startswith("rail:")) and ok(end)                 and isinstance(at, (list, tuple)) and len(at) == 2:
            try:
                anchors[end] = [float(at[0]), float(at[1])]
            except (TypeError, ValueError):
                pass
    return {"parts": parts, "wires": wires, "anchors": anchors}


@dataclass
class Pad:
    """What the twin says about one pad this frame."""
    high: float = 0.0       # fraction of the frame driven high
    low: float = 0.0        # fraction driven low
    pull: int = 0           # 0: none, 1: pull-up, -1: pull-down

    @property
    def driven(self) -> bool:
        return self.high + self.low >= 0.5

    @property
    def duty(self) -> float:
        d = self.high + self.low
        return self.high / d if d else 0.0


@dataclass
class Result:
    nets: dict[str, float] = field(default_factory=dict)     # end -> volts
    parts: dict[str, dict] = field(default_factory=dict)
    pads: dict[str, dict] = field(default_factory=dict)      # pad -> amps...
    # pad -> 0/1: the level the bench drives an input pad to
    inputs: dict[str, int] = field(default_factory=dict)
    volts: dict[str, float] = field(default_factory=dict)    # pad -> V
    warnings: list[str] = field(default_factory=list)


class _Nets:
    """Union-find over wire ends."""

    def __init__(self):
        self.up: dict[str, str] = {}

    def find(self, x: str) -> str:
        self.up.setdefault(x, x)
        while self.up[x] != x:
            self.up[x] = self.up[self.up[x]]
            x = self.up[x]
        return x

    def join(self, a: str, b: str) -> None:
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.up[ra] = rb


def _solve_linear(a: list[list[float]], b: list[float]) -> list[float]:
    """Gaussian elimination with partial pivoting (the matrices are tiny)."""
    n = len(b)
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(a[r][c]))
        if abs(a[p][c]) < 1e-30:
            continue
        a[c], a[p] = a[p], a[c]
        b[c], b[p] = b[p], b[c]
        for r in range(c + 1, n):
            f = a[r][c] / a[c][c]
            if f:
                row, top = a[r], a[c]
                for k in range(c, n):
                    row[k] -= f * top[k]
                b[r] -= f * b[c]
    x = [0.0] * n
    for c in range(n - 1, -1, -1):
        s = b[c] - sum(a[c][k] * x[k] for k in range(c + 1, n))
        x[c] = s / a[c][c] if abs(a[c][c]) > 1e-30 else 0.0
    return x


class Bench:
    """A bench and the state it carries between frames (input hysteresis)."""

    def __init__(self, bench: dict | None = None, vdd: float = 3.3,
                 pad_max_ma: float = 20.0, holes: dict | None = None):
        self.vdd = vdd
        self.holes = holes or {}
        self.pad_max = pad_max_ma / 1000
        self.levels: dict[str, int] = {}
        self.set(bench or {})

    def set(self, bench: dict) -> None:
        self.data = clean(bench, self.holes)
        nets = _Nets()
        nets.find("rail:GND")
        for a, b in self.data["wires"]:
            nets.join(a, b)
            for end in (a, b):              # a hole is its pad or rail
                if end.startswith("hole:"):
                    nets.join(end, self.holes[end]["net"])
        for p in self.data["parts"]:
            for t in PARTS[p["type"]]["terminals"]:
                nets.find(f"{p['id']}.{t}")
        self.ends = nets
        roots = sorted({nets.find(e) for e in nets.up})
        gnd = nets.find("rail:GND")
        # Node 0 is ground; the others are numbered
        others = [r for r in roots if r != gnd]
        self.node = {gnd: 0, **{r: i + 1 for i, r in enumerate(others)}}
        self.nnodes = len(others)
        # Pads wired to the bench, and the other ends on each pad's net
        self.pads = sorted({e[4:] for e in nets.up if e.startswith("pin:")})
        self.rails = {e[5:] for e in nets.up if e.startswith("rail:")}

    def n(self, end: str) -> int:
        return self.node[self.ends.find(end)]

    @property
    def empty(self) -> bool:
        return not self.data["parts"] and not self.pads

    def _pad_attached(self, pad: str) -> bool:
        """The pad's net has something other than pads on it."""
        root = self.ends.find(f"pin:{pad}")
        return any(self.ends.find(e) == root and not e.startswith("pin:")
                   for e in self.ends.up)

    # -- one operating point ------------------------------------------

    def _operating_point(self, states: dict[str, float],
                         pads: dict[str, Pad]) -> tuple[list[float], dict]:
        """Node voltages with each driven pad at @states[pad] (0..1: its
        averaged level) and the currents of interest."""
        size = self.nnodes
        guess = [0.0] * (size + 1)
        diodes = []             # (anode node, cathode node, Vf, key)
        for p in self.data["parts"]:
            pid, t = p["id"], p["type"]
            if t == "led":
                diodes.append((self.n(f"{pid}.a"), self.n(f"{pid}.k"),
                               LED_VF[p["color"]], pid))
            elif t == "diode":
                diodes.append((self.n(f"{pid}.a"), self.n(f"{pid}.k"), 0.7,
                               pid))
            elif t == "rgb_led":
                for c, vf in (("r", 1.9), ("g", 2.9), ("b", 2.9)):
                    diodes.append((self.n(f"{pid}.{c}"), self.n(f"{pid}.k"),
                                   vf, f"{pid}.{c}"))
        v = guess
        for _ in range(200):
            a = [[0.0] * size for _ in range(size)]
            b = [0.0] * size

            def g(n1: int, n2: int, cond: float) -> None:
                if n1:
                    a[n1 - 1][n1 - 1] += cond
                if n2:
                    a[n2 - 1][n2 - 1] += cond
                if n1 and n2:
                    a[n1 - 1][n2 - 1] -= cond
                    a[n2 - 1][n1 - 1] -= cond

            def src(n1: int, volts: float, ohms: float) -> None:
                """A source of @volts behind @ohms, node to ground."""
                if n1:
                    a[n1 - 1][n1 - 1] += 1 / ohms
                    b[n1 - 1] += volts / ohms

            def cur(n1: int, n2: int, amps: float) -> None:
                """@amps flowing from n1 to n2 through the element."""
                if n1:
                    b[n1 - 1] -= amps
                if n2:
                    b[n2 - 1] += amps

            for i in range(size):
                a[i][i] += G_MIN
            self._stamp_linear(g, src, v, states, pads)
            for na, nk, vf, _ in diodes:
                vd = min(v[na] - v[nk], vf + 1.0)
                i_s = I_LED_RATED * math.exp(-vf / V_T)
                e = math.exp(vd / V_T)
                gd = i_s * e / V_T + 1e-12
                ieq = i_s * (e - 1) - gd * vd
                g(na, nk, gd)
                cur(na, nk, ieq)
            new = [0.0] + _solve_linear(a, b)
            # Limit each step so the diode exponentials stay tame
            step = max((abs(x - y) for x, y in zip(new, v)), default=0)
            if step > 0.3:
                k = 0.3 / step
                new = [y + (x - y) * k for x, y in zip(new, v)]
            v = new
            if step < 1e-7:
                break
        currents = {}
        for na, nk, vf, key in diodes:
            vd = v[na] - v[nk]
            i_s = I_LED_RATED * math.exp(-vf / V_T)
            currents[key] = i_s * (math.exp(min(vd, vf + 1.0) / V_T) - 1)
        return v, currents

    def _stamp_linear(self, g, src, v, states, pads) -> None:
        vdd = self.vdd
        for rail in self.rails:
            volts = vdd if RAILS[rail] is None else RAILS[rail]
            if rail != "GND":
                src(self.n(f"rail:{rail}"), volts, R_RAIL)
        for pad in self.pads:
            n = self.n(f"pin:{pad}")
            st = pads.get(pad, Pad())
            if pad in states:
                src(n, states[pad] * vdd, R_PAD)
            elif st.pull:
                src(n, vdd if st.pull > 0 else 0.0, R_PULL)
        for p in self.data["parts"]:
            pid, t = p["id"], p["type"]
            end = lambda term: self.n(f"{pid}.{term}")      # noqa: E731
            if t in ("resistor", "motor"):
                g(end("a"), end("b"), 1 / p["ohms"])
            elif t == "button" and p["pressed"] or t == "switch" and p["on"]:
                g(end("a"), end("b"), 1 / R_SWITCH)
            elif t == "pot":
                ra = max(p["ohms"] * p["position"], 0.5)
                rb = max(p["ohms"] * (1 - p["position"]), 0.5)
                g(end("a"), end("w"), 1 / ra)
                g(end("w"), end("b"), 1 / rb)
            elif t == "nmos":
                vgs = v[end("g")] - v[end("s")]
                # Smoothly on above Vth: keeps the Newton steps continuous
                on = 1 / (1 + math.exp(-(vgs - p["vth"]) / 0.08))
                g(end("d"), end("s"), on / p["ron"] + 1e-9)
            elif t == "servo":
                g(end("vcc"), end("gnd"), 1 / 50.0)     # idle draw, ~66 mA

    # -- a frame ---------------------------------------------------------

    def solve(self, pads: dict[str, Pad]) -> Result:
        """Solve the bench for the twin's pads this frame."""
        res = Result()
        if self.empty:
            return res
        driven = {p: pads[p] for p in self.pads
                  if p in pads and pads[p].driven}
        pwm = sorted((p for p, s in driven.items() if 0.02 < s.duty < 0.98),
                     key=lambda p: -min(driven[p].duty, 1 - driven[p].duty))
        switching, steady = pwm[:PWM_STATES], pwm[PWM_STATES:]
        base = {p: (1.0 if s.duty >= 0.98 else 0.0) for p, s in driven.items()}
        for p in steady:
            base[p] = driven[p].duty            # too many: their average
        acc_v = [0.0] * (self.nnodes + 1)
        acc_i: dict[str, float] = {}
        acc_pad: dict[str, float] = {}
        peak_i: dict[str, float] = {}
        for combo in itertools.product((0, 1), repeat=len(switching)):
            w = 1.0
            states = dict(base)
            for p, hi in zip(switching, combo):
                d = driven[p].duty
                w *= d if hi else 1 - d
                states[p] = float(hi)
            if w < 1e-6:
                continue
            v, currents = self._operating_point(states, pads)
            for i, x in enumerate(v):
                acc_v[i] += w * x
            for k, x in currents.items():
                acc_i[k] = acc_i.get(k, 0.0) + w * x
                peak_i[k] = max(peak_i.get(k, 0.0), x)
            for p, lvl in states.items():
                vp = v[self.n(f"pin:{p}")]
                acc_pad[p] = acc_pad.get(p, 0.0) + w * (lvl * self.vdd - vp) \
                    / R_PAD
            self._part_extras(res, v, w)
        volt = lambda end: acc_v[self.n(end)]               # noqa: E731
        for e in self.ends.up:
            res.nets[e] = round(volt(e), 4)
        self._report(res, acc_i, peak_i, acc_pad, pads, volt)
        return res

    def _part_extras(self, res: Result, v: list[float], w: float) -> None:
        """Weighted per-state quantities that are not plain averages of
        node voltages: motor current needs the state's own voltages."""
        for p in self.data["parts"]:
            if p["type"] == "motor":
                d = res.parts.setdefault(p["id"], {"amps": 0.0})
                va = v[self.n(f"{p['id']}.a")] - v[self.n(f"{p['id']}.b")]
                d["amps"] += w * va / p["ohms"]

    def _report(self, res, acc_i, peak_i, acc_pad, pads, volt) -> None:
        vdd = self.vdd
        for p in self.data["parts"]:
            pid, t = p["id"], p["type"]
            d = res.parts.setdefault(pid, {})
            if t in ("led", "diode"):
                i = acc_i.get(pid, 0.0)
                d.update(amps=round(i, 6),
                         brightness=round(min(1.0, max(0.0, i / I_LED_RATED)),
                                          3))
                if t == "led" and peak_i.get(pid, 0) > I_LED_MAX:
                    d["warn"] = "overdriven: add a series resistor"
                    res.warnings.append(f"{pid}: LED at "
                                        f"{peak_i[pid] * 1000:.0f} mA")
            elif t == "rgb_led":
                rgb = [min(1.0, max(0.0, acc_i.get(f"{pid}.{c}", 0.0) /
                                    I_LED_RATED)) for c in "rgb"]
                d["rgb"] = [round(x, 3) for x in rgb]
                if any(peak_i.get(f"{pid}.{c}", 0) > I_LED_MAX for c in "rgb"):
                    d["warn"] = "overdriven: add series resistors"
                    res.warnings.append(f"{pid}: RGB LED overdriven")
            elif t == "motor":
                va = volt(f"{pid}.a") - volt(f"{pid}.b")
                d["volts"] = round(va, 3)
                d["amps"] = round(d.get("amps", 0.0), 4)
                d["rpm"] = round(va * p["rpm_per_volt"])
            elif t == "voltmeter":
                d["volts"] = round(volt(f"{pid}.p") - volt(f"{pid}.n"), 3)
            elif t == "pot":
                d["volts"] = round(volt(f"{pid}.w") - volt(f"{pid}.b"), 3)
            elif t == "servo":
                d.update(self._servo(pid, pads, volt))
        for pad in self.pads:
            st = pads.get(pad, Pad())
            vp = volt(f"pin:{pad}")
            res.volts[pad] = round(vp, 4)
            info = {"volts": round(vp, 3), "driven": st.driven}
            if pad in acc_pad:
                amps = acc_pad[pad]
                info["amps"] = round(amps, 5)
                if abs(amps) > self.pad_max:
                    info["warn"] = f"{abs(amps) * 1000:.0f} mA: over the " \
                                   f"pad's {self.pad_max * 1000:.0f} mA"
                    res.warnings.append(f"{pad}: {info['warn']}")
            elif self._pad_attached(pad):
                # An input wired to the bench reads its net's level
                prev = self.levels.get(pad, 1 if st.pull > 0 else 0)
                lvl = 1 if vp > HYST_HI * vdd else 0 if vp < HYST_LO * vdd \
                    else prev
                self.levels[pad] = lvl
                res.inputs[pad] = lvl
                info["reads"] = lvl
            res.pads[pad] = info
        for rail in self.rails - {"GND"}:
            volts = vdd if RAILS[rail] is None else RAILS[rail]
            amps = (volts - volt(f"rail:{rail}")) / R_RAIL
            if amps > 1.0:
                res.warnings.append(f"{rail} shorted ({amps:.1f} A)")
        # Motors straight on a pad: a real pad cannot run one
        for p in self.data["parts"]:
            d = res.parts[p["id"]]
            if p["type"] == "motor" and any(
                    self.ends.find(f"{p['id']}.{t}") ==
                    self.ends.find(f"pin:{pad}")
                    for t in ("a", "b") for pad in acc_pad):
                d["warn"] = "on a bare pad: drive it through a transistor"

    def _servo(self, pid: str, pads: dict[str, Pad], volt) -> dict:
        """The angle of a servo whose signal comes from a PWM pad: pulse
        width over an assumed 50 Hz frame, 0.5-2.5 ms for 0-180 degrees."""
        powered = volt(f"{pid}.vcc") - volt(f"{pid}.gnd") > 3.0
        root = self.ends.find(f"{pid}.sig")
        src = next((p for p in self.pads
                    if self.ends.find(f"pin:{p}") == root and
                    p in pads and pads[p].driven), None)
        if not powered:
            return {"angle": None, "warn": "no power on vcc/gnd"}
        if not src:
            return {"angle": None}
        pulse = pads[src].duty * SERVO_PERIOD
        if not 0.0004 <= pulse <= 0.0026:
            return {"angle": None, "pulse_ms": round(pulse * 1000, 2)}
        angle = (min(max(pulse, 0.0005), 0.0025) - 0.0005) / 0.002 * 180
        return {"angle": round(angle, 1), "pulse_ms": round(pulse * 1000, 2)}


def pad_drives(inputs: dict[str, int], groups) -> dict[str, tuple[int, int]]:
    """{group: (mask, level)} for the PORT's "pX-bench" from the input
    levels of a Result; every group in @groups appears (so a pad the bench
    lets go of is released)."""
    out = {g: (0, 0) for g in groups}
    for pad, lvl in inputs.items():
        g, bit = pad_ref(pad)
        mask, level = out.get(g, (0, 0))
        out[g] = (mask | 1 << bit, level | (lvl << bit))
    return out
