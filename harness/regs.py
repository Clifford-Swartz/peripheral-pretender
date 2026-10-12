# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Register lists for golden dumps and breakpoint snapshots.

A list comes from the generated device description (devices/<part>.json,
schema pp-device/1) or from a plain-text file in the format of
regsets/pic32cm5164jh01100-boot.txt.  Every register is classified before
anything reads it: read-sensitive registers (FIFO pops, clear-on-read flags),
write-only registers and address ranges a debug probe must not touch are
skipped on silicon and on the twin alike, so both sides of a comparison
perform the same accesses.

    py -m harness.regs [--regs SOURCE] [--only GLOB] [--view SERCOM4=USART_INT]
    py -m harness.regs --export regsets/custom.txt --only 'GCLK' --only 'NVM'
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, Iterable, Iterator

HARNESS_DIR = Path(__file__).resolve().parent
REPO_ROOT = HARNESS_DIR.parent
DEVICE_JSON = REPO_ROOT / "devices" / "pic32cm5164jh01100.json"
REGSETS = {"boot": HARNESS_DIR / "regsets" / "pic32cm5164jh01100-boot.txt"}

# (module, register) -> what a read does on silicon.
READ_SENSITIVE = {
    ("SERCOM", "DATA"): "read pops the receive buffer; in I2C smart mode it also "
                        "acknowledges (DS60001632H 30.7.8, 31.7.6, 32.6.3.2)",
    ("ADC", "RESULT"): "read clears INTFLAG.RESRDY and WINMON (DS60001632H 37.7.7)",
    ("CAN", "ECR"): "read resets ECR.CEL (DS60001632H 33.7.13)",
    ("CAN", "PSR"): "read sets LEC/DLEC to 7 and clears PXE/RFDF/RBRS/RESI "
                    "(DS60001632H 33.7.14)",
    ("DSU", "DCC"): "read clears STATUSB.DCCDx (DS60001632H 13.11.4)",
    ("SYSTICK", "CSR"): "read clears COUNTFLAG; QEMU clears it on debugger reads "
                        "too (hw/timer/armv7m_systick.c)",
}

# [start, end) ranges a probe must never access, even when forced.
FORBIDDEN = (
    (0x60000000, 0x60000400,
     "IOBUS alias: Cortex-M0+ routes probe accesses to AHB, where this range "
     "is unmapped (PAC.INTFLAGAHB, DS60001632H 12.5.2.7)"),
)

_MODULE_ALIASES = {"DSU_EXT": "DSU", "PORT_IOBUS": "PORT", "DIVAS_IOBUS": "DIVAS"}


@dataclass(frozen=True)
class Reg:
    """One register element at a fixed address."""

    name: str                   # "<instance>.<element>", e.g. "GCLK.PCHCTRL[22]"
    address: int
    size: int                   # bytes: 1, 2 or 4
    access: str = "RW"          # "R", "W" or "RW"
    reset: int | None = None    # documented (twin) reset value, when known
    module: str = ""            # IP name, e.g. "SERCOM"; inferred when empty
    ip_id: str = ""             # IP id, e.g. "U2201", when the source has one
    aliases: tuple[str, ...] = ()
    sensitive: str = ""         # why reading is unsafe; "" means safe
    dynamic: bool = False       # the model computes the value (not a stored reset)
    reset_from: tuple[str, ...] = ()    # NVM user-row fields loaded at reset

    @property
    def instance(self) -> str:
        return self.name.split(".", 1)[0]

    @property
    def element(self) -> str:
        return self.name.split(".", 1)[-1]

    def names(self) -> tuple[str, ...]:
        return (self.name,) + self.aliases

    def to_json(self) -> dict[str, Any]:
        entry: dict[str, Any] = {"name": self.name,
                                 "address": fmt_hex(self.address, 4),
                                 "size": self.size}
        if self.aliases:
            entry["aliases"] = list(self.aliases)
        if self.module:
            entry["module"] = self.module
        if self.ip_id:
            entry["ip_id"] = self.ip_id
        if self.reset is not None:
            entry["reset"] = fmt_hex(self.reset, self.size)
        if self.dynamic:
            entry["dynamic"] = True
        if self.reset_from:
            entry["reset_from"] = list(self.reset_from)
        return entry


def parse_int(value: Any) -> int | None:
    """Parse 17, "0x11" or "0b10001"; None, "" and "-" mean unknown."""
    if value is None or value in ("", "-"):
        return None
    if isinstance(value, bool):
        raise ValueError(f"not an integer: {value!r}")
    if isinstance(value, int):
        return value
    return int(str(value).replace("_", ""), 0)


def fmt_hex(value: int, size: int) -> str:
    return f"0x{value:0{size * 2}X}"


def module_of(instance: str) -> str:
    """IP name of an ATDF instance: SERCOM4 -> SERCOM, DSU_EXT -> DSU."""
    name = _MODULE_ALIASES.get(instance, instance)
    return re.sub(r"\d+$", "", name).upper()


def is_forbidden(reg: Reg) -> str:
    for start, end, why in FORBIDDEN:
        if reg.address < end and start < reg.address + reg.size:
            return why
    return ""


def _builtin_reason(module: str, reg: Reg) -> str:
    leaf = re.sub(r"\[\d+\]$", "", reg.element.rsplit(".", 1)[-1])
    for key in (leaf, re.sub(r"\d+$", "", leaf)):
        reason = READ_SENSITIVE.get((module, key))
        if reason:
            return reason
    if reg.access == "W":
        return "write-only register"
    return is_forbidden(reg)


def classify(reg: Reg) -> Reg:
    """Fill in the module name and the reason a read would be unsafe."""
    module = reg.module.upper() or module_of(reg.instance)
    reason = _builtin_reason(module, reg) or reg.sensitive
    return replace(reg, module=module, sensitive=reason)


def merge_views(regs: Iterable[Reg]) -> list[Reg]:
    """Return one entry per (address, size), sorted by address.

    A hand-written list may name one address several times (register modes);
    extra names become aliases.  A byte that is read-sensitive under any name
    is treated as read-sensitive under all of them.
    """
    by_place: dict[tuple[int, int], Reg] = {}
    for reg in regs:
        key = (reg.address, reg.size)
        old = by_place.get(key)
        if old is None:
            by_place[key] = reg
            continue
        aliases = old.aliases
        if reg.name not in old.names():
            aliases += (reg.name,)
        by_place[key] = replace(old, aliases=aliases,
                                sensitive=old.sensitive or reg.sensitive)
    merged = sorted(by_place.values(), key=lambda r: (r.address, r.size))
    hot = [r for r in merged if r.sensitive]
    return [_taint(r, hot) for r in merged]


def _taint(reg: Reg, hot: list[Reg]) -> Reg:
    if reg.sensitive:
        return reg
    for other in hot:
        if reg.address < other.address + other.size and \
                other.address < reg.address + reg.size:
            return replace(reg, sensitive=f"overlaps {other.name}: {other.sensitive}")
    return reg


def name_pattern(pattern: str) -> re.Pattern[str]:
    """Compile a case-insensitive glob with * and ? only.

    Brackets are literal so "GCLK.PCHCTRL[22]" names one array element.
    """
    parts = (".*" if c == "*" else "." if c == "?" else re.escape(c) for c in pattern)
    return re.compile("".join(parts), re.IGNORECASE)


def matches(reg: Reg, patterns: Iterable[re.Pattern[str]]) -> bool:
    names = reg.names() + (reg.instance,)
    return any(p.fullmatch(n) for p in patterns for n in names)


def select(regs: Iterable[Reg], only: Iterable[str] = (),
           skip: Iterable[str] = ()) -> list[Reg]:
    """Filter by globs over register names, aliases and instance names."""
    only_p = [name_pattern(p) for p in only]
    skip_p = [name_pattern(p) for p in skip]
    return [r for r in regs
            if (not only_p or matches(r, only_p)) and not matches(r, skip_p)]


def partition(regs: Iterable[Reg],
              include_sensitive: bool = False) -> tuple[list[Reg], list[Reg]]:
    """Split into (to_read, skipped).  Forbidden ranges are never read."""
    to_read, skipped = [], []
    for reg in regs:
        unsafe = is_forbidden(reg) or (reg.sensitive and not include_sensitive)
        (skipped if unsafe else to_read).append(reg)
    return to_read, skipped


def default_source() -> Path:
    return DEVICE_JSON if DEVICE_JSON.exists() else REGSETS["boot"]


def source_path(source: str | Path | None) -> Path:
    if source is None:
        return default_source()
    return REGSETS.get(str(source), Path(source))


def load(source: str | Path | None = None, only: Iterable[str] = (),
         skip: Iterable[str] = (), views: dict[str, str] | None = None) -> list[Reg]:
    """Load, classify, merge and filter a register list.

    source is a device-description .json, a text list, a built-in set name
    ("boot") or None for the device description if present, else "boot".
    views picks register views of the device description (load_device_json).
    """
    path = source_path(source)
    if path.suffix.lower() == ".json":
        raw = load_device_json(path, views)
    else:
        if views:
            raise ValueError("--view applies to a device description (.json) only")
        raw = load_text(path)
    return select(merge_views(classify(r) for r in raw), only, skip)


# --- plain-text lists ---------------------------------------------------------

_MARKERS = ("sensitive", "dynamic")


def load_text(path: Path) -> list[Reg]:
    """Parse "NAME ADDRESS SIZE [ACCESS] [RESET] [sensitive] [dynamic]" lines."""
    regs = []
    for lineno, raw in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        fields = raw.split("#", 1)[0].replace(",", " ").split()
        if not fields:
            continue
        try:
            regs.append(_text_reg(fields))
        except (ValueError, IndexError, TypeError) as exc:
            raise ValueError(f"{path}:{lineno}: {exc}: {raw.strip()!r}") from None
    return regs


def _text_reg(fields: list[str]) -> Reg:
    name, address, size = fields[0], parse_int(fields[1]), parse_int(fields[2])
    markers = {f.lower() for f in fields[3:]} & set(_MARKERS)
    rest = [f for f in fields[3:] if f.lower() not in _MARKERS]
    access = rest[0].upper() if rest else "RW"
    if size not in (1, 2, 4):
        raise ValueError(f"size must be 1, 2 or 4 bytes, not {size}")
    if access not in ("R", "W", "RW"):
        raise ValueError(f"access must be R, W or RW, not {access!r}")
    return Reg(name, address, size, access,
               reset=parse_int(rest[1]) if len(rest) > 1 else None,
               sensitive="marked sensitive in the register list"
               if "sensitive" in markers else "",
               dynamic="dynamic" in markers)


def to_text(regs: Iterable[Reg]) -> str:
    """The text-list form of regs (inverse of load_text)."""
    lines = []
    for r in regs:
        reset = fmt_hex(r.reset, r.size) if r.reset is not None else "-"
        markers = " sensitive" * bool(r.sensitive) + " dynamic" * r.dynamic
        lines.append(f"{r.name:<34} {fmt_hex(r.address, 4)} {r.size} "
                     f"{r.access:<2} {reset}{markers}")
    return "\n".join(lines) + "\n"


# --- generated device description (schema pp-device/1) --------------------------
#
# Written by generator/mchpgen/jsongen.py.  Register tables live per IP and
# per view (register mode); nested groups are already flattened into
# qualified names such as "GROUP[1].DIR".  The fuse rows have no register
# views, so their words come from the memory map and the NVM-row image.

DEVICE_SCHEMA = "pp-device/1"
NVM_ROW_TYPES = ("fuses", "user_page")
USER_PAGE_FUSE_WORDS = 2        # the rest of the user page is application data


def load_device_json(path: Path, views: dict[str, str] | None = None) -> list[Reg]:
    """Every register element of every instance, plus the NVM row words.

    An instance contributes one register view: views[instance] or
    views[module] when given (e.g. {"SERCOM4": "USART_INT"}), else the IP's
    default view, which is the mode selected at reset.
    """
    doc = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(doc, dict) or doc.get("schema") != DEVICE_SCHEMA:
        found = doc.get("schema") if isinstance(doc, dict) else type(doc).__name__
        raise ValueError(f"{path}: schema {found!r}, expected {DEVICE_SCHEMA!r}")
    views = views or {}
    unknown = set(views) - {key for inst in doc["instances"]
                            for key in (inst["name"], inst["module"])}
    if unknown:
        raise ValueError(f"--view names no instance or module: {sorted(unknown)}")
    regs: list[Reg] = []
    for inst in doc["instances"]:
        ip = doc["ips"][inst["ip"]]
        view = pick_view(ip, views.get(inst["name"]) or views.get(inst["module"]))
        if view is not None:
            regs += instance_regs(inst, view)
    return regs + nvm_row_words(doc["memory"], doc["nvm_rows"])


def pick_view(ip: dict, wanted: str | None) -> dict | None:
    """The view named wanted, else the default one; None if the IP has none."""
    if not ip["views"]:
        return None
    name = wanted or ip["default_view"] or None
    for view in ip["views"]:
        if view["mode"] == name:
            return view
    if wanted:
        modes = [v["mode"] for v in ip["views"]]
        raise ValueError(f"{ip['module']} has no view {wanted!r} (views: {modes})")
    return ip["views"][0]


def instance_regs(inst: dict, view: dict) -> Iterator[Reg]:
    overrides = {o["key"]: o["value"] for o in view["reset_overrides"]}
    fuses: dict[int, list[str]] = {}
    for field in view["reset_from"]:
        fuses.setdefault(field["reg_offset"], []).append(field["fuse"])
    for regdef in view["registers"]:
        count = regdef["count"]
        for i in range(count):
            element = regdef["name"] + (f"[{i}]" if count > 1 else "")
            offset = regdef["offset"] + i * regdef["stride"]
            yield Reg(f"{inst['name']}.{element}", inst["base"] + offset,
                      regdef["size"], _access(regdef),
                      overrides.get(element, _documented_reset(regdef)),
                      module=inst["module"], ip_id=inst["ip_id"] or "",
                      sensitive=_declared_side_effect(regdef),
                      dynamic=bool({"DYNAMIC", "HOOK_PRE_READ"} & set(regdef["flags"])),
                      reset_from=tuple(sorted(fuses.get(offset, ()))))


def _documented_reset(regdef: dict) -> int | None:
    """The table's reset value, or None when the generator had to invent it.

    The generator resets registers without an ATDF initval to 0 and records
    that as a "heuristic" reset; an overlay value makes the reset known again.
    """
    sources = {source for aspect, source, _ in regdef["provenance"] if aspect == "reset"}
    return None if sources == {"heuristic"} else regdef["reset"]


def _access(regdef: dict) -> str:
    flags = set(regdef["flags"])
    if "WRITE_ONLY" in flags or regdef["access"] == "W":
        return "W"
    if "READ_ONLY" in flags or regdef["access"] == "R":
        return "R"
    return "RW"


def _declared_side_effect(regdef: dict) -> str:
    if regdef.get("read_side_effect"):
        return "device description declares a read side effect (pre_read hook)"
    return ""


def nvm_row_words(memory: list[dict], nvm: dict) -> list[Reg]:
    """Words of the NVM rows, named NVM.<segment>_WORD_<i>.

    The reset column carries the twin's default NVM image, so a silicon dump
    compared against it shows what the image should be seeded with.
    """
    image = {w["addr"]: w["value"] for w in nvm["words"]}
    fill = int.from_bytes(bytes([nvm["fill"]]) * 4, "little")
    regs = []
    for seg in memory:
        if seg["type"] not in NVM_ROW_TYPES:
            continue
        words = seg["size"] // 4
        if seg["type"] == "user_page":
            words = min(words, USER_PAGE_FUSE_WORDS)
        for i in range(words):
            addr = seg["base"] + 4 * i
            regs.append(_nvm_word(f"{seg['name']}_WORD_{i}", addr, image.get(addr, fill)))
    for i, (addr, value) in enumerate(zip(nvm["serial_addrs"], nvm["serial_default"])):
        regs.append(_nvm_word(f"SERIAL_WORD_{i}", addr, value))
    return regs


def _nvm_word(name: str, address: int, value: int) -> Reg:
    return Reg(f"NVM.{name}", address, 4, "R", value, module="NVM")


# --- command line ---------------------------------------------------------------

def parse_views(items: Iterable[str]) -> dict[str, str]:
    """["SERCOM4=USART_INT", ...] -> {"SERCOM4": "USART_INT", ...}."""
    views = {}
    for item in items:
        key, sep, mode = item.partition("=")
        if not sep or not key or not mode:
            raise ValueError(f"--view expects INSTANCE=MODE or MODULE=MODE, not {item!r}")
        views[key] = mode
    return views


def add_selection_args(parser: argparse.ArgumentParser) -> None:
    """--regs/--only/--skip/--view, shared by every tool that loads a list."""
    parser.add_argument("--regs", help="device JSON, text list or 'boot' "
                                       "(default: the device JSON if present, else boot)")
    parser.add_argument("--only", action="append", default=[], metavar="GLOB",
                        help="keep registers or instances matching GLOB (repeatable)")
    parser.add_argument("--skip", action="append", default=[], metavar="GLOB",
                        help="drop registers or instances matching GLOB (repeatable)")
    parser.add_argument("--view", action="append", default=[], metavar="NAME=MODE",
                        help="register view for an instance or module, e.g. "
                             "SERCOM4=USART_INT (default: the mode at reset)")


def load_selected(args: argparse.Namespace) -> list[Reg]:
    return load(args.regs, args.only, args.skip, parse_views(args.view))


def _export_header(args: argparse.Namespace, regs: list[Reg]) -> str:
    selection = " ".join([f"--only {p}" for p in args.only] + [f"--skip {p}" for p in args.skip]
                         + [f"--view {v}" for v in args.view])
    return "\n".join([
        "# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).",
        "#",
        f"# Exported by `py -m harness.regs --export` from {source_path(args.regs).name}",
        f"# selection: {selection or '(all)'}; {len(regs)} registers.",
        "# Format: NAME ADDRESS SIZE ACCESS RESET [sensitive] [dynamic]",
        "#   SIZE in bytes; RESET is the twin's reset value ('-' when unknown); 'sensitive'",
        "#   = reading changes hardware state (never read); 'dynamic' = computed by the model.",
    ]) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="py -m harness.regs", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    add_selection_args(parser)
    parser.add_argument("--export", metavar="FILE", help="write the list as a text list")
    parser.add_argument("--list", action="store_true", help="print every register")
    args = parser.parse_args(argv)
    try:
        regs = load_selected(args)
    except (OSError, ValueError, KeyError) as exc:
        print(f"regs: {exc}", file=sys.stderr)
        return 2
    if args.export:
        Path(args.export).write_text(_export_header(args, regs) + to_text(regs),
                                     encoding="utf-8", newline="\n")
    to_read, skipped = partition(regs)
    if args.list:
        sys.stdout.write(to_text(regs))
    per_instance = Counter(r.instance for r in to_read)
    print(f"{source_path(args.regs)}: {len(regs)} registers in {len(per_instance)} "
          f"instances; {len(to_read)} readable, {len(skipped)} skipped")
    for reg in skipped:
        print(f"  skip {reg.name:<30} {reg.sensitive or is_forbidden(reg)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
