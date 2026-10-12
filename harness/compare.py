# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Compare twin and silicon results: UART transcripts or register captures.

  py -m harness.compare uart A B [--from REGEX] [--masks FILE]
  py -m harness.compare regs A B [--masks FILE] [--core] [--by-instance]
                                 [--emit-overrides FILE [--golden a|b]]
  py -m harness.compare reset A [--masks FILE] [--by-instance] [--emit-overrides FILE]

A and B are transcripts (raw VCOM bytes, text, or the JSON of `pp run --json`
and `hw_run --json`) or pp-regs/1 captures (dump_regs, gdb_snapshot).  `reset`
checks the first event of A against the reset values its register list
documents (the generated tables), so it needs no twin.

Rules (docs/silicon-harness.md, "Comparison rules"): events are compared by
order and value, never by time; registers are matched by (address, size) and
only registers read on both sides are compared; masked bits are ignored; a
read fault on one side only is a difference; RSTC.RCAUSE is compared only when
both captures came from the same kind of reset.  Exit status: 0 same,
1 different, 2 bad input.
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from harness import snapshot
from harness.regs import HARNESS_DIR, fmt_hex, name_pattern, parse_int
from harness.vcom import normalize

DEFAULT_MASKS = HARNESS_DIR / "masks" / "default.yaml"
CORE_MODULES = {"SYSTEMCONTROL", "SYSTICK", "NVIC", "MPU"}     # QEMU's own models
_ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

Place = tuple[int, int]                 # (address, size)


@dataclass
class Masks:
    registers: list[tuple[re.Pattern[str], int, str]] = field(default_factory=list)
    core: list[tuple[re.Pattern[str], int, str]] = field(default_factory=list)
    uart: list[tuple[re.Pattern[str], str]] = field(default_factory=list)

    def ignored(self, names: list[str], table: str = "registers") -> tuple[int, str]:
        """OR of the masks whose glob matches any of names, and their reasons."""
        bits, reasons = 0, []
        for pattern, mask, reason in getattr(self, table):
            if any(pattern.fullmatch(n) for n in names):
                bits |= mask
                reasons.append(reason)
        return bits, "; ".join(reasons)

    def plus(self, name: str, mask: int, reason: str) -> "Masks":
        return Masks(self.registers + [(name_pattern(name), mask, reason)],
                     self.core, self.uart)


def load_masks(paths: list[str], defaults: bool = True) -> Masks:
    masks = Masks()
    for path in ([DEFAULT_MASKS] if defaults else []) + [Path(p) for p in paths]:
        doc = _load_mapping(path)
        for entry in doc.get("registers") or []:
            masks.registers.append((name_pattern(entry["name"]),
                                    parse_int(entry["mask"]), entry.get("reason", "")))
        for entry in doc.get("core") or []:
            masks.core.append((name_pattern(entry["name"]),
                               parse_int(entry["mask"]), entry.get("reason", "")))
        for entry in doc.get("uart") or []:
            masks.uart.append((re.compile(entry["pattern"]), entry.get("repl", "<masked>")))
    return masks


def _load_mapping(path: Path) -> dict[str, Any]:
    text = Path(path).read_text(encoding="utf-8")
    if Path(path).suffix.lower() in (".yaml", ".yml"):
        import yaml                         # PyYAML; only needed for YAML mask files
        return yaml.safe_load(text) or {}
    return json.loads(text)


# --- UART transcripts -----------------------------------------------------------

def load_transcript(path: str | Path) -> str:
    raw = Path(path).read_bytes()
    if raw.lstrip()[:1] == b"{":
        try:
            doc = json.loads(raw)
        except ValueError:
            doc = None
        if isinstance(doc, dict) and isinstance(doc.get("uart"), str):
            return doc["uart"]
    return normalize(raw)


def transcript_lines(text: str, masks: Masks, start: str | None = None) -> list[str]:
    """Normalized, masked lines.  Leading NUL/garbage bytes (pin set-up glitches)
    are dropped; with start, so is everything before the first matching line."""
    text = _ANSI.sub("", text).lstrip("\x00�")
    lines = [line.rstrip() for line in text.split("\n")]
    if start:
        first = next((i for i, line in enumerate(lines) if re.search(start, line)), None)
        lines = lines[first:] if first is not None else []
    for pattern, repl in masks.uart:
        lines = [pattern.sub(repl, line) for line in lines]
    while lines and not lines[-1]:
        lines.pop()
    return lines


def compare_uart(args: argparse.Namespace, masks: Masks) -> int:
    a = transcript_lines(load_transcript(args.a), masks, args.start)
    b = transcript_lines(load_transcript(args.b), masks, args.start)
    if a == b:
        print(f"same: {len(a)} lines")
        return 0
    sys.stdout.writelines(line + "\n" for line in
                          difflib.unified_diff(a, b, args.a, args.b, lineterm=""))
    return 1


# --- register captures -------------------------------------------------------------

@dataclass
class RegDiff:
    seq: int
    label: str
    name: str
    address: int
    size: int
    a: str | None
    b: str | None
    note: str = ""

    def line(self) -> str:
        diff = ""
        if self.a and self.b:
            diff = f" diff={fmt_hex(int(self.a, 16) ^ int(self.b, 16), self.size)}"
        return (f"#{self.seq:<3} {self.label:<14} {self.name:<30} @0x{self.address:08X} "
                f"A={self.a or '-':<10} B={self.b or '-':<10}{diff} {self.note}").rstrip()


def read_places(capture: dict[str, Any]) -> dict[Place, dict[str, Any]]:
    """(address, size) -> register-table entry, for every register read."""
    return {(int(e["address"], 16), e["size"]): e for e in capture["registers"]}


def _names(entry: dict[str, Any]) -> list[str]:
    return [entry["name"]] + entry.get("aliases", [])


def event_values(event: dict[str, Any],
                 table: dict[Place, dict]) -> dict[Place, tuple[str | None, str | None]]:
    """(address, size) -> (value, read error); exactly one of them is set."""
    place_of = {e["name"]: place for place, e in table.items()}
    out = {place_of[name]: (value, None) for name, value in event.get("values", {}).items()}
    out.update({place_of[name]: (None, error)
                for name, error in event.get("errors", {}).items()})
    return out


def _difference(a_val: str | None, a_err: str | None, b_val: str | None,
                b_err: str | None, ignore: int) -> str | None:
    """None when equal after masking, else a short note."""
    if a_err or b_err:
        if a_err and b_err:
            return None
        return f"read fault on {'A' if a_err else 'B'}: {a_err or b_err}"
    if (int(a_val, 16) ^ int(b_val, 16)) & ~ignore:
        return f"(masked 0x{ignore:X})" if ignore else ""
    return None


def diff_event(seq: int, ea: dict, eb: dict, ta: dict, tb: dict, masks: Masks,
               stats: Counter) -> list[RegDiff]:
    va, vb = event_values(ea, ta), event_values(eb, tb)
    diffs = []
    for place in sorted(va.keys() & vb.keys()):
        entry = ta[place]
        instance = entry["name"].split(".", 1)[0]
        stats[(instance, "compared")] += 1
        ignore, why = masks.ignored(_names(entry) + _names(tb[place]))
        (a_val, a_err), (b_val, b_err) = va[place], vb[place]
        note = _difference(a_val, a_err, b_val, b_err, ignore)
        if note is None:
            stats[(instance, "masked" if why and a_val != b_val else "same")] += 1
            continue
        stats[(instance, "different")] += 1
        diffs.append(RegDiff(seq, ea.get("label", ""), entry["name"], *place,
                             a_val, b_val, note))
    return diffs


def diff_core(seq: int, ea: dict, eb: dict, masks: Masks) -> list[RegDiff]:
    ca, cb = ea.get("core") or {}, eb.get("core") or {}
    diffs = []
    for name in sorted(set(ca) & set(cb)):
        ignore, _ = masks.ignored([name], "core")
        if (int(ca[name], 16) ^ int(cb[name], 16)) & ~ignore:
            diffs.append(RegDiff(seq, ea.get("label", ""), f"core.{name}", 0, 4,
                                 ca[name], cb[name]))
    return diffs


def compare_captures(ca: dict, cb: dict, masks: Masks, core: bool = False
                     ) -> tuple[list[str], list[str], list[RegDiff], Counter]:
    """Structural problems, notes, register differences, per-instance counts."""
    problems, notes, diffs, stats = [], [], [], Counter()
    if ca.get("reset") != cb.get("reset"):
        masks = masks.plus("RSTC.RCAUSE", 0xFF, "captures come from different resets")
    la = [e.get("label", "") for e in ca["events"]]
    lb = [e.get("label", "") for e in cb["events"]]
    if la != lb:
        problems.append("event order differs:\n" + "\n".join(
            difflib.unified_diff(la, lb, "A", "B", lineterm="")))
    ta, tb = read_places(ca), read_places(cb)
    only_a, only_b = len(ta.keys() - tb.keys()), len(tb.keys() - ta.keys())
    if only_a or only_b:
        notes.append(f"not compared: {only_a} registers read only in A, "
                     f"{only_b} only in B")
    for seq, (ea, eb) in enumerate(zip(ca["events"], cb["events"])):
        if ea.get("pc") and eb.get("pc") and ea["pc"] != eb["pc"]:
            problems.append(f"event #{seq} ({ea.get('label')}): pc A={ea['pc']} B={eb['pc']}")
        diffs += diff_event(seq, ea, eb, ta, tb, masks, stats)
        if core:
            diffs += diff_core(seq, ea, eb, masks)
    return problems, notes, diffs, stats


def documented(capture: dict[str, Any]) -> dict[str, Any]:
    """A one-event capture holding the reset values capture's list documents."""
    table = [e for e in capture["registers"] if "reset" in e]
    label = capture["events"][0].get("label", "") if capture["events"] else ""
    event = snapshot.make_event(0, label, "documented",
                                {e["name"]: e["reset"] for e in table}, {})
    return {"format": snapshot.FORMAT, "side": "documented", "reset": "documented",
            "target": capture.get("register_source", ""), "registers": table,
            "skipped": [], "events": [event], "end": "dump"}


def instance_table(stats: Counter) -> list[str]:
    instances = sorted({inst for inst, _ in stats})
    lines = [f"{'instance':<16} {'compared':>8} {'same':>6} {'masked':>6} {'different':>9}"]
    for inst in instances:
        lines.append(f"{inst:<16} {stats[(inst, 'compared')]:>8} {stats[(inst, 'same')]:>6} "
                     f"{stats[(inst, 'masked')]:>6} {stats[(inst, 'different')]:>9}")
    return lines


# --- feeding the overlays -----------------------------------------------------------

_NOT_OVERRIDES = {
    "nvm": "NVM rows: seed the twin's NVM image (generator/mchpgen/nvmrows.py)",
    "fuses": "loaded from the NVM user row at reset: compare NVM.USER_PAGE_WORD_0/1 first",
    "dynamic": "computed by the model: fix the model's behavior, not the reset table",
    "core": "Arm core registers: QEMU's CPU/NVIC model, no overlay",
}


def _overlay_file(entry: dict[str, Any]) -> str:
    module = entry.get("module", "?").lower()
    return f"overlays/{module}_{entry.get('ip_id', '<ip-id>').lower()}.yaml"


def _kind(entry: dict[str, Any]) -> str:
    if entry.get("module") == "NVM":
        return "nvm"
    if entry.get("module", "").upper() in CORE_MODULES:
        return "core"
    if entry.get("reset_from"):
        return "fuses"
    return "dynamic" if entry.get("dynamic") else "override"


def overrides_yaml(golden: dict, diffs: list[RegDiff], side: str) -> str:
    """reset_overrides suggestions per overlay, from the first event's differences.

    An overlay override applies to every instance of its IP, so each entry
    names the instances that showed the value; conflicting values are left as
    comments.  Differences reset_overrides cannot fix are listed separately.
    """
    table = read_places(golden)
    instances: dict[tuple[str, str], set[str]] = {}
    for entry in table.values():
        instance, element = entry["name"].split(".", 1)
        instances.setdefault((_overlay_file(entry), element), set()).add(instance)
    found: dict[str, dict[str, dict[str, list[str]]]] = {}
    other_kinds: dict[str, list[str]] = {kind: [] for kind in _NOT_OVERRIDES}
    for d in diffs:
        entry = table.get((d.address, d.size))
        value, other = (d.a, d.b) if side == "a" else (d.b, d.a)
        if d.seq != 0 or entry is None or not value:
            continue
        kind = _kind(entry)
        if kind != "override":
            fuses = f"; from {', '.join(entry['reset_from'])}" if kind == "fuses" else ""
            other_kinds[kind].append(f"#   {d.name}: {value} (other side {other or '-'}){fuses}")
            continue
        instance, element = d.name.split(".", 1)
        found.setdefault(_overlay_file(entry), {}).setdefault(element, {}) \
            .setdefault(value, []).append(instance)
    lines = [f"# reset_overrides suggested from the {golden.get('side')} capture "
             f"{golden.get('target')} ({golden.get('reset')}, {golden.get('created', '')}).",
             "# Review every entry before copying it into the overlay."]
    for overlay in sorted(found):
        lines += [f"{overlay}:", "  reset_overrides:"]
        for element, values in sorted(found[overlay].items()):
            lines += _override_lines(element, values, instances[(overlay, element)])
    for kind, entries in other_kinds.items():
        if entries:
            lines += [f"# Not reset_overrides; {_NOT_OVERRIDES[kind]}:"] + entries
    if len(lines) == 2:
        lines.append("# no reset differences")
    return "\n".join(lines) + "\n"


def _override_lines(element: str, values: dict[str, list[str]],
                    all_inst: set[str]) -> list[str]:
    if len(values) > 1:
        seen = "; ".join(f"{v} on {', '.join(sorted(i))}" for v, i in sorted(values.items()))
        return [f'    # "{element}": instances disagree ({seen})']
    (value, inst), = values.items()
    if set(inst) == all_inst:
        return [f'    "{element}": {value}    # {", ".join(sorted(inst))}']
    return [f'    # "{element}": {value} only on {", ".join(sorted(inst))} of '
            f'{len(all_inst)} instances; an IP-level override would change the others']


# --- commands -----------------------------------------------------------------------

def report(ca: dict, cb: dict, args: argparse.Namespace, masks: Masks,
           golden: dict, golden_side: str) -> int:
    problems, notes, diffs, stats = compare_captures(ca, cb, masks,
                                                     core=getattr(args, "core", False))
    for line in problems + notes:
        print(line)
    for d in diffs:
        print(d.line())
    if args.by_instance:
        print("\n".join(instance_table(stats)))
    if args.emit_overrides:
        Path(args.emit_overrides).write_text(overrides_yaml(golden, diffs, golden_side),
                                             encoding="utf-8")
    total = sum(v for (_, k), v in stats.items() if k == "compared")
    print(f"{'same' if not problems and not diffs else 'different'}: "
          f"{len(ca['events'])}/{len(cb['events'])} events, {total} register comparisons, "
          f"{len(diffs)} differences")
    return 1 if problems or diffs else 0


def compare_regs(args: argparse.Namespace, masks: Masks) -> int:
    ca, cb = snapshot.load(args.a), snapshot.load(args.b)
    golden = ca if args.golden == "a" else cb
    return report(ca, cb, args, masks, golden, args.golden)


def compare_reset(args: argparse.Namespace, masks: Masks) -> int:
    capture = snapshot.load(args.a)
    first = {**capture, "events": capture["events"][:1]}
    return report(first, documented(first), args, masks, first, "a")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="py -m harness.compare", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    for name, func in (("uart", compare_uart), ("regs", compare_regs),
                       ("reset", compare_reset)):
        p = sub.add_parser(name)
        p.add_argument("a")
        if name != "reset":
            p.add_argument("b")
        p.add_argument("--masks", action="append", default=[], help="extra mask file")
        p.add_argument("--no-default-masks", action="store_true")
        p.set_defaults(func=func)
        if name == "uart":
            p.add_argument("--from", dest="start", metavar="REGEX",
                           help="ignore lines before the first match (both sides)")
            continue
        p.add_argument("--by-instance", action="store_true", help="per-instance counts")
        p.add_argument("--emit-overrides", metavar="FILE",
                       help="write reset_overrides suggestions (first event)")
        if name == "regs":
            p.add_argument("--core", action="store_true", help="compare core registers too")
            p.add_argument("--golden", choices=("a", "b"), default="b",
                           help="which side is silicon truth (default b)")
    args = parser.parse_args(argv)
    try:
        masks = load_masks(args.masks, defaults=not args.no_default_masks)
        return args.func(args, masks)
    except (OSError, ValueError, KeyError) as exc:
        print(f"compare: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
