# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""py -m tools.pp_accept build|run|suite; see tools/pp_accept/__init__.py."""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path

import yaml

from .build import build_all
from .scenario import parse_steps, play, to_json


def check(expect: dict, res) -> list[str]:
    """Failed expectations of one run (empty = all met)."""
    fails = []
    for led, (lo, hi) in expect.get("led_toggles", {}).items():
        n = res.toggles(led)
        if not lo <= n <= hi:
            fails.append(f"{led} toggled {n} times, expected {lo}..{hi}")
    for text in expect.get("uart", []):
        if text not in res.uart:
            fails.append(f"UART lacks {text!r}")
    total = res.backlog.get("total", 0)
    if "backlog_max" in expect and total > expect["backlog_max"]:
        fails.append(f"backlog {total} > {expect['backlog_max']}")
    if res.stopped is not None:
        fails.append(f"QEMU stopped: {res.stopped[-200:]}")
    return fails


def cmd_build(a) -> int:
    results = build_all(Path(a.clone), a.kit, Path(a.out), a.force, a.only)
    for b in results:
        print(f"{'ok  ' if b.ok else 'FAIL'} {b.app}" + ("" if b.ok else f"  ({b.log})"))
    return 0 if all(b.ok for b in results) else 1


def cmd_run(a) -> int:
    res = asyncio.run(play(Path(a.elf), a.machine, parse_steps(a.steps)))
    if a.json:
        print(to_json(res))
    else:
        for led, trace in res.leds.items():
            print(f"{led}: {res.toggles(led)} toggles {trace[:20]}")
        print("uart:", res.uart[-1500:])
        print("backlog:", res.backlog.get("total", 0), res.backlog.get("instances", {}))
    return 0


def cmd_suite(a) -> int:
    suite = yaml.safe_load(Path(a.suite).read_text(encoding="utf-8"))
    elves, rows, out = Path(a.elves), [], {}
    names = a.only or list(suite["examples"])
    for name in names:
        ex = suite["examples"][name]
        elf = elves / f"{name}.elf"
        if not elf.is_file():
            rows.append((name, "not built", ex.get("verdict", "")))
            continue
        res = asyncio.run(play(elf, suite["machine"],
                               parse_steps(ex.get("steps", "wait 2"))))
        if "expect" in ex:
            fails = check(ex["expect"], res)
            result = "PASS" if not fails else "FAIL: " + "; ".join(fails)
        else:
            result = (f"ran (no expectations; backlog "
                      f"{res.backlog.get('total', 0)})")
        rows.append((name, result, ex.get("verdict", "")))
        out[name] = {"result": result, "leds": res.leds,
                     "uart": res.uart[-1500:], "backlog": res.backlog}
        print(f"{name}: {result}", flush=True)
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1), encoding="utf-8")
    passed = sum(r[1] == "PASS" for r in rows)
    failed = [r for r in rows if r[1].startswith("FAIL")]
    print(f"\n{passed} expectations met, {len(failed)} failed, "
          f"{len(rows) - passed - len(failed)} unchecked or not built")
    return 1 if failed else 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="pp_accept")
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="build a kit's example projects with XC32")
    b.add_argument("clone", help="Harmony examples clone (has apps/)")
    b.add_argument("--kit", required=True, help="project name, e.g. pic32cm_jh01_curiosity_pro")
    b.add_argument("--out", required=True, help="folder for <app>.elf and logs")
    b.add_argument("--force", action="store_true", help="rebuild cached ELFs")
    b.add_argument("--only", nargs="*", help="app names")
    r = sub.add_parser("run", help="play one scenario")
    r.add_argument("elf")
    r.add_argument("--machine", required=True)
    r.add_argument("--steps", default="wait 2")
    r.add_argument("--json", action="store_true")
    s = sub.add_parser("suite", help="play a kit's acceptance file")
    s.add_argument("suite", help="acceptance/<kit>.yaml")
    s.add_argument("--elves", required=True, help="folder with <app>.elf")
    s.add_argument("--only", nargs="*")
    s.add_argument("--json", help="write results here")
    a = ap.parse_args(argv)
    return {"build": cmd_build, "run": cmd_run, "suite": cmd_suite}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
