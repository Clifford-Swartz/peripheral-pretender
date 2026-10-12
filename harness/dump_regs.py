# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Golden register dump: read a register list once, on silicon or the twin.

  --twin            QEMU halted at power-on reset (-S), read via its gdbstub
  --gdb HOST:PORT   any halted gdbserver (QEMU -gdb, pyOCD gdbserver)
  --pyocd           the EDBG directly.  By default pyOCD attaches without
                    halting and only always-safe locations are read (flash and
                    NVM rows, DSU, System Control Space); the rest is skipped.
      --reset-halt     run the DFP reset-and-halt sequence first (needs
                       --allow-halt): the state before the first instruction
      --unmask-clocks  with --reset-halt: open every MCLK AHB/APB gate for
                       the dump, then restore the masks

The result is a pp-regs/1 capture with a single event (see snapshot.py);
compare captures with `py -m harness.compare regs`.

  py -m harness.dump_regs --twin --out twin-reset.json
  py -m harness.dump_regs --pyocd --out identify.json        (read-only)
  py -m harness.dump_regs --pyocd --reset-halt --allow-halt --unmask-clocks --out golden.json
"""
from __future__ import annotations

import argparse
import contextlib
import sys
from dataclasses import replace
from typing import Iterator

from harness import gdb_snapshot, regs, silicon, twin
from harness.gdbmi import GdbMi, MiError, MiTimeout
from harness.regs import Reg, fmt_hex
from harness.snapshot import (CORE_REGS, GdbTarget, ReadFault, make_event, new_capture,
                              read_values, save)

# Readable with the core running: no clock gate, no read side effects.
ATTACH_SAFE = (
    (0x00000000, 0x00810000),           # flash, data flash, NVM rows
    (0x41002000, 0x41004000),           # DSU and its CoreSight ROM table
    (0xE000E000, 0xE000F000),           # System Control Space
)
MCLK_MASKS = (0x40000810, 0x40000814, 0x40000818, 0x4000081C, 0x40000820)


def attach_split(to_read: list[Reg]) -> tuple[list[Reg], list[Reg]]:
    safe, unsafe = [], []
    for reg in to_read:
        inside = any(lo <= reg.address and reg.address + reg.size <= hi
                     for lo, hi in ATTACH_SAFE)
        (safe if inside else unsafe).append(reg)
    return safe, unsafe


def refusal(args: argparse.Namespace) -> str:
    if args.reset_halt and not args.pyocd:
        return "--reset-halt applies to --pyocd"
    if args.unmask_clocks and not args.reset_halt:
        return "--unmask-clocks needs --reset-halt (the core must be halted)"
    halts = args.reset_halt or (args.gdb and args.side == "silicon")
    if halts and not args.allow_halt:
        return ("this halts the silicon core: rerun with --allow-halt once the user "
                "approved halting (and, for --reset-halt, resetting) the board")
    return ""


def dump_gdb(args: argparse.Namespace, to_read: list[Reg]) -> tuple[str, dict]:
    with contextlib.ExitStack() as stack:
        target = args.gdb or stack.enter_context(
            twin.halted(args.machine, args.kernel, args.qemu))
        mi = stack.enter_context(GdbMi(args.gdb_path))
        gdb_snapshot.connect(mi, args.kernel, target, None)
        reader = GdbTarget(mi)
        core = reader.core()
        values, errors = read_values(reader.read_reg, to_read)
        with contextlib.suppress(MiError, MiTimeout):
            mi.command("-target-detach")
    reason = "qemu-cold" if args.twin else "as-found"
    return target, make_event(0, "dump", reason, values, errors, pc=core.get("pc", ""),
                              core=core)


@contextlib.contextmanager
def unmasked_clocks(target, enabled: bool) -> Iterator[dict[int, int]]:
    """Open every MCLK gate while the body runs; yields the original masks."""
    originals = {addr: target.read32(addr) for addr in MCLK_MASKS} if enabled else {}
    try:
        for addr in originals:
            target.write32(addr, 0xFFFFFFFF)
        yield originals
    finally:
        for addr, value in originals.items():
            target.write32(addr, value)


def pyocd_reader(target):
    from pyocd.core.exceptions import TransferError

    def read(reg: Reg) -> int:
        try:
            return silicon.read_reg(target, reg.address, reg.size)
        except TransferError as exc:
            raise ReadFault(str(exc)) from None

    return read


def dump_pyocd(args: argparse.Namespace, to_read: list[Reg]) -> tuple[str, dict, dict]:
    pac = [r for r in to_read if r.instance == "PAC" and r.element.startswith("INTFLAG")]
    extra = {}
    with silicon.session(args.probe, args.pack, frequency=args.frequency,
                         allow_halt=args.reset_halt) as sess:
        read = pyocd_reader(sess.target)
        if args.reset_halt:
            silicon.reset_and_halt_dfp(sess.target, allow_halt=args.allow_halt)
        with unmasked_clocks(sess.target, args.unmask_clocks) as originals:
            values, errors = read_values(read, to_read)
            pac_after, _ = read_values(read, pac)
        for reg in to_read:                     # report masks as found at reset
            if reg.address in originals and reg.size == 4:
                values[reg.name] = fmt_hex(originals[reg.address], 4)
        core = None
        if args.reset_halt:
            raw = sess.target.read_core_registers_raw(list(CORE_REGS))
            core = {name: fmt_hex(v, 4) for name, v in zip(CORE_REGS, raw)}
        target = f"pyocd:{sess.probe.unique_id}"
    changed = {n: [values.get(n), v] for n, v in pac_after.items() if values.get(n) != v}
    if changed:
        extra["pac_flags_changed_by_dump"] = changed
        print(f"dump_regs: warning: the dump itself set PAC error flags: {changed}",
              file=sys.stderr)
    if originals:
        extra["mclk_unmasked_for_dump"] = True
    reason = "dfp-reset-halt" if args.reset_halt else "attach"
    return target, make_event(0, "dump", reason, values, errors,
                              pc=(core or {}).get("pc", ""), core=core), extra


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="py -m harness.dump_regs", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    where = p.add_mutually_exclusive_group(required=True)
    where.add_argument("--twin", action="store_true", help="QEMU halted at reset")
    where.add_argument("--gdb", metavar="HOST:PORT", help="a halted gdbserver")
    where.add_argument("--pyocd", action="store_true", help="the EDBG through pyOCD")
    p.add_argument("--side", choices=("twin", "silicon"), default="silicon",
                   help="what --gdb is connected to (default silicon)")
    regs.add_selection_args(p)
    p.add_argument("--kernel", help="twin: ELF to load (symbols for --gdb)")
    p.add_argument("--qemu", help=f"QEMU binary (default {twin.QEMU} or $PP_QEMU)")
    p.add_argument("--machine", default=twin.MACHINE)
    p.add_argument("--gdb-path", help="gdb-multiarch path")
    p.add_argument("--probe", help="probe unique id (EDBG serial number)")
    p.add_argument("--pack", help=f"DFP directory or .pack (default {silicon.DFP_DIR})")
    p.add_argument("--frequency", type=int, default=silicon.FREQUENCY)
    p.add_argument("--reset-halt", action="store_true", help="DFP reset-and-halt first")
    p.add_argument("--unmask-clocks", action="store_true")
    p.add_argument("--allow-halt", action="store_true", help="user approved halt/reset")
    p.add_argument("--out", help="write the capture JSON here")
    args = p.parse_args(argv)
    if args.twin:
        args.side = "twin"
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    problem = refusal(args)
    if problem:
        print(f"dump_regs: refusing: {problem}", file=sys.stderr)
        return 2
    try:
        listed = regs.load_selected(args)
        to_read, skipped = regs.partition(listed)
        extra = {}
        if args.pyocd:
            if not args.reset_halt:
                to_read, running = attach_split(to_read)
                skipped += [replace(r, sensitive="core running (attach): needs --reset-halt")
                            for r in running]
            target, event, extra = dump_pyocd(args, to_read)
        else:
            target, event = dump_gdb(args, to_read)
    except (MiError, MiTimeout, twin.TwinError, silicon.SiliconError,
            OSError, ValueError) as exc:
        print(f"dump_regs: {exc}", file=sys.stderr)
        return 2
    capture = new_capture(args.side if not args.pyocd else "silicon", "dump_regs", target,
                          to_read, skipped, reset=event["reason"],
                          register_source=str(regs.source_path(args.regs)),
                          views=args.view, **extra)
    capture["events"], capture["end"] = [event], "dump"
    if args.out:
        save(capture, args.out)
    errors = event.get("errors", {})
    print(f"{capture['side']} {target}: {len(event['values'])} registers read, "
          f"{len(errors)} read errors, {len(skipped)} skipped")
    for name, msg in list(errors.items())[:10]:
        print(f"  error {name}: {msg}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
