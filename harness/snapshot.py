# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Register captures: one JSON format for golden dumps and breakpoint snapshots.

    {"format": "pp-regs/1", "side": "twin" | "silicon", "tool": ..., "target": ...,
     "reset": how the state was reached ("qemu-cold", "sysresetreq", "attach", ...),
     "registers": [{name, address, size, module?, ip_id?, reset?, aliases?}],  # read
     "skipped":   [{name, address, size, reason}],                             # never read
     "events":    [{seq, label, reason, pc, func, core?: {...},
                    values: {name: "0x..."}, errors?: {name: "..."}}],
     "end": "dump" | "stop-at" | "max-events" | "timeout" | "exited"}

A golden dump is a capture with one event.  compare.py matches events by
position and registers by (address, size), so names may differ by source.
"""
from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Sequence

from harness.gdbmi import GdbMi, MiError
from harness.regs import Reg, fmt_hex, is_forbidden

FORMAT = "pp-regs/1"
CORE_REGS = tuple(f"r{i}" for i in range(13)) + (
    "sp", "lr", "pc", "xpsr", "msp", "psp", "primask", "control")


class ReadFault(Exception):
    """A register read failed (bus fault, unreadable address, short read)."""


def new_capture(side: str, tool: str, target: str, read: Sequence[Reg],
                skipped: Sequence[Reg], **meta: Any) -> dict[str, Any]:
    return {"format": FORMAT, "side": side, "tool": tool, "target": target,
            "created": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            **meta,
            "registers": [r.to_json() for r in read],
            "skipped": [{**r.to_json(), "reason": is_forbidden(r) or r.sensitive}
                        for r in skipped],
            "events": [], "end": ""}


def read_values(read: Callable[[Reg], int],
                regs: Sequence[Reg]) -> tuple[dict[str, str], dict[str, str]]:
    """Read every register once, in list order; faults are recorded, not raised."""
    values, errors = {}, {}
    for reg in regs:
        try:
            values[reg.name] = fmt_hex(read(reg), reg.size)
        except ReadFault as exc:
            errors[reg.name] = str(exc)
    return values, errors


def make_event(seq: int, label: str, reason: str, values: dict[str, str],
               errors: dict[str, str], pc: str = "", func: str = "",
               core: dict[str, str] | None = None) -> dict[str, Any]:
    event: dict[str, Any] = {"seq": seq, "label": label, "reason": reason,
                             "pc": pc, "func": func}
    if core is not None:
        event["core"] = core
    event["values"] = values
    if errors:
        event["errors"] = errors
    return event


def save(capture: dict[str, Any], path: str | Path) -> None:
    Path(path).write_text(json.dumps(capture, indent=1) + "\n", encoding="utf-8")


def load(path: str | Path) -> dict[str, Any]:
    doc = json.loads(Path(path).read_text(encoding="utf-8"))
    if doc.get("format") != FORMAT:
        raise ValueError(f"{path}: not a {FORMAT} capture")
    return doc


def hex32(text: str) -> str:
    return fmt_hex(int(text, 0), 4)


class GdbTarget:
    """Memory and core registers of a halted gdbserver target, via GDB/MI."""

    def __init__(self, mi: GdbMi):
        self.mi = mi
        self._names: list[str] | None = None

    def read_reg(self, reg: Reg) -> int:
        """One read of the register's size; QEMU performs it as one access."""
        try:
            rec = self.mi.command(f"-data-read-memory-bytes 0x{reg.address:08X} {reg.size}")
        except MiError as exc:
            raise ReadFault(str(exc)) from None
        blocks = rec.payload.get("memory", [])
        data = bytes.fromhex("".join(b.get("contents", "") for b in blocks))
        if len(data) != reg.size:
            raise ReadFault(f"read {len(data)} of {reg.size} bytes")
        return int.from_bytes(data, "little")

    def core(self) -> dict[str, str]:
        if self._names is None:
            self._names = self.mi.command("-data-list-register-names").payload["register-names"]
        rec = self.mi.command("-data-list-register-values --skip-unavailable x")
        out = {}
        for item in rec.payload["register-values"]:
            name = self._names[int(item["number"])]
            if name in CORE_REGS:
                out[name] = hex32(item["value"])
        return out
