# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""A board-side circuit: the customer's SPICE netlist, with the twin's pads
added as sources.

A netlist names pads as nodes ("PA05", any case) and the board's supply as
"VDD"; ground is 0. Everything else is plain SPICE: elements, .model,
.subckt, .param, .include. The analysis is the host's: .tran, .op, .end,
.save, .print and .control blocks in the netlist are dropped.

Each pad becomes a Norton source whose target voltage and conductance are
ngspice "external" sources, answered from the pad's state in the twin:

    vt_pa05 t_pa05 0 dc 0 external     target voltage
    vg_pa05 g_pa05 0 dc 0 external     conductance (siemens, as volts)
    b_pa05 0 pa05 i = v(g_pa05) * (v(t_pa05) - v(pa05))
    rleak_pa05 pa05 0 1e9              keeps the node defined when open

and VDD, unless the netlist drives it, is "v_pp_vdd vdd 0 dc 0 external".

Live controls (a pot's position, a pressed button) are external sources
too: the netlist declares "*pp control NAME VALUE" (a SPICE comment) and
uses "vx_NAME x_NAME 0 dc 0 external"; v(x_NAME) is then VALUE, which the
host changes while it runs without reloading the circuit.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEVICES = ROOT / "devices"
BOARDS = ROOT / "tools" / "pp_web" / "boards"

PAD_NODE = re.compile(r"^p[a-d]\d{1,2}$", re.IGNORECASE)
R_PAD = 40.0            # a driven pad's output resistance (typical)
R_PULL = 40e3           # a pull resistor (typical)
R_LEAK = 1e9
RAMP_S = 5e-9           # a driven pad's edge
# The largest time step: ngspice's error control lets an RC with a 10 ms
# time constant drift by about 0.6 mV with 1 ms steps (most of an ADC
# LSB at 3.3 V); 100 us steps track it within a few microvolts
TMAX_S = 100e-6
# Analysis lines the host supplies itself
DROPPED = (".tran", ".op", ".ac", ".dc", ".end", ".save", ".print", ".plot",
           ".probe", ".four", ".meas", ".measure", ".width", ".backanno")


class CircuitError(ValueError):
    """The netlist cannot be used."""


@dataclass
class Circuit:
    title: str
    body: list[str]                 # elements and models, as given
    pads: list[str] = field(default_factory=list)       # "PA05", sorted
    node: dict[str, str] = field(default_factory=dict)  # pad -> its node
    drives_vdd: bool = False        # the netlist has its own VDD source
    uses_vdd: bool = False
    controls: dict[str, float] = field(default_factory=dict)

    def lines(self, tstep: float = 1e-6, tmax: float = TMAX_S,
              tstop: float = 1e6) -> list[str]:
        """The circuit as ngspice loads it, with the pad sources and the
        host's transient."""
        out = [self.title, *self.body]
        for pad in self.pads:
            p = self.node[pad]
            out += [f"vt_{p} t_{p} 0 dc 0 external",
                    f"vg_{p} g_{p} 0 dc 0 external",
                    f"b_{p} 0 {p} i = v(g_{p}) * (v(t_{p}) - v({p}))",
                    f"rleak_{p} {p} 0 {R_LEAK:g}"]
        if self.uses_vdd and not self.drives_vdd:
            out.append("v_pp_vdd vdd 0 dc 0 external")
        out += [".save none", f".tran {tstep:g} {tstop:g} 0 {tmax:g}",
                ".end"]
        return out


def _logical_lines(text: str) -> list[str]:
    """Netlist lines with "+" continuations joined, comments dropped."""
    out: list[str] = []
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].rstrip()
        if not line.strip() or line.lstrip().startswith("*"):
            continue
        if line.lstrip().startswith("+") and out:
            out[-1] += " " + line.lstrip()[1:].strip()
        else:
            out.append(line.strip())
    return out


def parse(text: str, name: str = "circuit") -> Circuit:
    """@text, a SPICE netlist (its first line the title, as in SPICE)."""
    raw = text.splitlines()
    title = raw[0].strip() if raw and raw[0].strip() else name
    controls = {}
    for line in raw[1:]:
        w = line.split()
        if len(w) == 4 and w[:2] == ["*pp", "control"]:
            try:
                controls[w[2].lower()] = float(w[3])
            except ValueError:
                raise CircuitError(f"bad control line: {line}") from None
    body, spell = [], {}
    depth, control, drives_vdd, uses_vdd = 0, False, False, False
    for line in _logical_lines("\n".join(raw[1:])):
        low = line.lower()
        word = low.split()[0]
        if word == ".control":
            control = True
            continue
        if word == ".endc":
            control = False
            continue
        if control or word in DROPPED:
            continue
        if word == ".subckt":
            depth += 1
        elif word == ".ends":
            depth = max(0, depth - 1)
        body.append(line)
        if depth or word.startswith("."):
            continue
        # A top-level element: its pads and VDD (nodes are the tokens
        # that look like them; values and models never do)
        tokens = re.split(r"[\s(),=]+", low)[1:]
        for t in tokens:
            if PAD_NODE.match(t):
                spell.setdefault(f"P{t[1].upper()}{int(t[2:]):02d}",
                                 set()).add(t)
        if "vdd" in tokens:
            uses_vdd = True
            if word[0] == "v" and tokens[0] == "vdd":
                drives_vdd = True
    for pad, names in spell.items():
        if len(names) > 1:
            raise CircuitError(f"{pad} is spelled {' and '.join(sorted(names))}"
                               ": SPICE takes those as different nodes")
    key = lambda p: (p[1], int(p[2:]))                  # noqa: E731
    return Circuit(title=title, body=body, pads=sorted(spell, key=key),
                   node={pad: names.pop() for pad, names in spell.items()},
                   drives_vdd=drives_vdd, uses_vdd=uses_vdd,
                   controls=controls)


def load(path: str | Path) -> Circuit:
    path = Path(path)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as e:
        raise CircuitError(f"cannot read {path}: {e}") from e
    circuit = parse(text, path.stem)
    if not circuit.pads:
        raise CircuitError(f"{path.name}: no node is named like a pad "
                           "(PA05)")
    return circuit


def machine_device(machine: str) -> str | None:
    """The device a machine type simulates (from the boards' board.json)."""
    for board in sorted(BOARDS.glob("*/board.json")):
        try:
            data = json.loads(board.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if data.get("machine") == machine:
            return data.get("device")
    return None


def adc_inputs(device: str | None) -> dict[str, list[tuple[str, int]]]:
    """Per pad, the ADC inputs on it: {pad: [(QOM path, channel)]}. The
    SDADC's channel 2n is INNn, 2n + 1 INPn."""
    path = DEVICES / f"{device}.json"
    if not device or not path.is_file():
        return {}
    dev = json.loads(path.read_text(encoding="utf-8"))
    ain: dict[str, list[tuple[str, int]]] = {}
    for sig in dev["signals"]:
        if not re.fullmatch(r"P[A-Z]\d+", sig["pad"]):
            continue
        if sig["instance"].startswith("ADC") and \
                sig["group"] in ("AIN", "AINP"):
            ain.setdefault(sig["pad"], []).append(
                (f"/machine/soc/{sig['instance'].lower()}", sig["index"]))
        elif sig["instance"] == "SDADC" and sig["group"] in ("INN", "INP"):
            ain.setdefault(sig["pad"], []).append(
                ("/machine/soc/sdadc",
                 2 * sig["index"] + (sig["group"] == "INP")))
    return ain
