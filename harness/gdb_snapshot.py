# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Register snapshots at breakpoints through GDB/MI, for the twin and silicon.

One script and one MI command sequence for both sides:
  twin     --twin starts QEMU halted at reset (-S) with the ELF loaded; gdbstub
           reads carry attrs.debug, so models take no read side effects
  silicon  --target localhost:3333 attaches to a pyOCD gdbserver on the EDBG;
           --reset-cmd "monitor reset halt" restarts the program.  A GDB
           connection halts the core, so silicon needs --allow-halt (approval).

The script snapshots once at connection, then at every breakpoint: stop
location, core registers, and each readable register of the list
(read-sensitive registers are never read).  Breakpoints are hardware
breakpoints, at most 4 (the Cortex-M0+ BPU), so both sides use the same set.

  py -m harness.gdb_snapshot hello.elf --twin --break main --stop-at done --out twin.json
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import sys
from pathlib import Path
from typing import Any

from harness import regs, twin
from harness.gdbmi import GdbMi, MiError, MiTimeout, mi_quote
from harness.snapshot import GdbTarget, hex32, make_event, new_capture, read_values, save

M0PLUS_HW_BREAKPOINTS = 4


class SnapshotError(Exception):
    pass


def connect(mi: GdbMi, elf: str | None, target: str, reset_cmd: str | None) -> None:
    for setting in ("confirm off", "pagination off", "mi-async on", "remotetimeout 10"):
        mi.command(f"-gdb-set {setting}")
    if elf:
        mi.command(f"-file-exec-and-symbols {mi_quote(Path(elf).as_posix())}")
    mi.command(f"-target-select remote {target}", timeout=30)
    if reset_cmd:
        mi.console(reset_cmd, timeout=30)


def insert_breakpoints(mi: GdbMi, locations: list[str], limit: int) -> dict[str, str]:
    """Insert hardware breakpoints; return {breakpoint number: location}."""
    if len(locations) > limit:
        raise SnapshotError(f"{len(locations)} breakpoints, but only {limit} hardware "
                            "breakpoints exist (Cortex-M0+ BPU)")
    labels = {}
    for location in locations:
        rec = mi.command(f"-break-insert -h {mi_quote(location)}")
        labels[rec.payload["bkpt"]["number"]] = location
    return labels


def take(reader: GdbTarget, to_read: list[regs.Reg], seq: int, label: str,
         reason: str, frame: dict, with_core: bool) -> dict[str, Any]:
    core = reader.core() if with_core else None
    values, errors = read_values(reader.read_reg, to_read)
    pc = frame.get("addr") or (core or {}).get("pc", "")
    return make_event(seq, label, reason, values, errors, pc=hex32(pc) if pc else "",
                      func=frame.get("func", ""), core=core)


def run(mi: GdbMi, args: argparse.Namespace, target: str,
        to_read: list[regs.Reg]) -> tuple[list[dict], str]:
    """Connect, snapshot at start and at each stop; return (events, end reason)."""
    reader = GdbTarget(mi)
    connect(mi, args.elf, target, args.reset_cmd)
    locations = args.breaks + ([args.stop_at] if args.stop_at else [])
    labels = insert_breakpoints(mi, locations, args.hw_breakpoints)
    events = [take(reader, to_read, 0, "start", "connected", {}, not args.no_core)]
    end = "max-events"
    while len(events) < args.max_events:
        mi.clear_stops()
        mi.command("-exec-continue")
        stop = mi.wait_stopped(args.timeout)
        if stop is None:
            mi.command("-exec-interrupt")
            stop = mi.wait_stopped(5.0) or {}
            label, end = "timeout", "timeout"
        else:
            label = labels.get(stop.get("bkptno", ""), stop.get("reason", "stopped"))
        events.append(take(reader, to_read, len(events), label, stop.get("reason", ""),
                           stop.get("frame") or {}, not args.no_core))
        if end == "timeout":
            break
        if args.stop_at and label == args.stop_at:
            end = "stop-at"
            break
        if stop.get("reason", "").startswith("exited"):
            end = "exited"
            break
    with contextlib.suppress(MiError, MiTimeout):
        mi.command("-target-detach")            # silicon resumes, as found
    return events, end


def refusal(args: argparse.Namespace) -> str:
    if args.side == "silicon" and not args.allow_halt:
        return ("a GDB connection halts the silicon core: rerun with --allow-halt "
                "once the user approved halting the board")
    if args.include_sensitive and args.side != "twin":
        return "--include-sensitive changes silicon state; it is for the twin only"
    return ""


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="py -m harness.gdb_snapshot", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("elf", help="ELF with symbols (the image running on the target)")
    where = p.add_mutually_exclusive_group(required=True)
    where.add_argument("--target", help="gdbserver HOST:PORT (pyOCD gdbserver: 3333)")
    where.add_argument("--twin", action="store_true", help="start QEMU halted at reset")
    p.add_argument("--side", choices=("twin", "silicon"),
                   help="what --target is (default: silicon; --twin implies twin)")
    p.add_argument("--qemu", help=f"QEMU binary (default {twin.QEMU} or $PP_QEMU)")
    p.add_argument("--machine", default=twin.MACHINE)
    p.add_argument("--break", dest="breaks", action="append", default=[],
                   metavar="LOCATION", help="breakpoint: function, file:line or *ADDR")
    p.add_argument("--stop-at", metavar="LOCATION", help="final breakpoint")
    regs.add_selection_args(p)
    p.add_argument("--no-core", action="store_true", help="do not record core registers")
    p.add_argument("--max-events", type=int, default=50)
    p.add_argument("--timeout", type=float, default=10.0, help="seconds per stop")
    p.add_argument("--hw-breakpoints", type=int, default=M0PLUS_HW_BREAKPOINTS)
    p.add_argument("--reset-cmd", help='e.g. "monitor reset halt" (pyOCD gdbserver)')
    p.add_argument("--allow-halt", action="store_true", help="user approved halting silicon")
    p.add_argument("--include-sensitive", action="store_true",
                   help="twin only: also read read-sensitive registers")
    p.add_argument("--gdb", help="gdb-multiarch path")
    p.add_argument("--out", help="write the capture JSON here")
    args = p.parse_args(argv)
    args.side = "twin" if args.twin else (args.side or "silicon")
    return args


def summary(capture: dict[str, Any]) -> str:
    lines = [f"{capture['side']} {capture['target']}: {len(capture['events'])} events, "
             f"{len(capture['registers'])} registers read, {len(capture['skipped'])} "
             f"skipped, end={capture['end']}"]
    for ev in capture["events"]:
        errs = f", {len(ev['errors'])} read errors" if ev.get("errors") else ""
        lines.append(f"  #{ev['seq']:<3} {ev['label']:<20} pc={ev['pc'] or '?':<10} "
                     f"{ev['func']}{errs}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    problem = refusal(args)
    if problem:
        print(f"gdb_snapshot: refusing: {problem}", file=sys.stderr)
        return 2
    try:
        listed = regs.load_selected(args)
        to_read, skipped = regs.partition(listed, include_sensitive=args.include_sensitive)
        with contextlib.ExitStack() as stack:
            target = args.target or stack.enter_context(
                twin.halted(args.machine, args.elf, args.qemu))
            mi = stack.enter_context(GdbMi(args.gdb))
            events, end = run(mi, args, target, to_read)
    except (SnapshotError, MiError, MiTimeout, twin.TwinError, OSError, ValueError) as exc:
        print(f"gdb_snapshot: {exc}", file=sys.stderr)
        return 2
    capture = new_capture(
        args.side, "gdb_snapshot", target, to_read, skipped,
        elf=str(args.elf), elf_sha256=hashlib.sha256(Path(args.elf).read_bytes()).hexdigest(),
        reset="qemu-cold" if args.twin else (args.reset_cmd or "as-found"),
        register_source=str(regs.source_path(args.regs)), views=args.view,
        breakpoints=args.breaks, stop_at=args.stop_at)
    capture["events"], capture["end"] = events, end
    if args.out:
        save(capture, args.out)
    print(summary(capture))
    return 3 if end == "timeout" else 0


if __name__ == "__main__":
    sys.exit(main())
