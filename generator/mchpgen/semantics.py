# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Merge ATDF facts, behavior overlays and heuristics into register tables.

One IpSem per ATDF module; one ViewSem per register view (an ATDF
register-group mode, or the single view of an IP without modes). Each RegSem
is one MchpRegInfo row. Every semantic is recorded with its source ("atdf",
"overlay" or "heuristic") so that the report can show provenance.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from . import atdf
from .overlay import Overlay, OverlayError, RegisterRules, View

FLAG_ORDER = ("PAC", "ALIAS_SET", "ALIAS_CLEAR", "ALIAS_TOGGLE", "DYNAMIC",
              "HOOK_PRE_READ", "HOOK_POST_WRITE", "BUSY_BUSERR", "WRITE_ONLY",
              "READ_ONLY")
FLAG_BITS = {name: 1 << i for i, name in enumerate(FLAG_ORDER)}

# sync: "GENCTRL{n}" or "REG.GENCTRL{n}": element i raises bit GENCTRL<i>
SYNC_ELEMENT = re.compile(r"^(?:([A-Z][A-Z0-9_]*)\.)?([A-Z][A-Z0-9_]*)\{n\}$")


@dataclass
class Diag:
    level: str          # "warning" or "info" (errors raise OverlayError)
    where: str
    msg: str


@dataclass
class FieldDef:
    name: str
    mask: int
    caption: str
    values: list[atdf.Value]

    @property
    def shift(self) -> int:
        return (self.mask & -self.mask).bit_length() - 1

    @property
    def width(self) -> int:
        return bin(self.mask).count("1")


@dataclass
class RegConst:
    """One REGn() constant plus its FIELD()s, relative to a constant scope."""
    name: str           # e.g. "CTRLA", "BAUD_FRAC"
    offset: int
    size: int
    count: int
    fields: list[FieldDef]


@dataclass
class ConstScope:
    """Constants sharing one prefix; subgroup scopes are element-relative."""
    prefix: str                     # "MCHP_SERCOM_USART_INT", "MCHP_PORT_GROUP"
    regs: list[RegConst]
    offset: int = 0                 # subgroup: offset of element 0
    stride: int = 0                 # subgroup: bytes between elements
    count: int = 0                  # subgroup: number of elements


@dataclass
class Provenance:
    aspect: str
    source: str         # atdf | overlay | heuristic
    detail: str


@dataclass
class RegSem:
    name: str                       # qualified name: "CTRLA", "GROUP[1].DIR"
    atdf_name: str
    offset: int
    size: int
    count: int
    stride: int
    reset: int
    impl: int
    wmask: int
    w1c: int = 0
    rdz: int = 0
    enprot: int = 0
    syncbusy: int = 0
    sync_reg: str | None = None
    sync_per_element: bool = False
    flags: list[str] = field(default_factory=list)
    alias_of: str | None = None     # qualified name of the storage register
    alias_index: int = -1
    rw: str = "RW"
    atdf_access: str | None = None
    access_size: int | None = None
    caption: str = ""
    note: str = ""
    fields: list[FieldDef] = field(default_factory=list)
    field_union: dict[str, int] = field(default_factory=dict)
    provenance: list[Provenance] = field(default_factory=list)

    @property
    def size_mask(self) -> int:
        return (1 << (8 * self.size)) - 1

    @property
    def flag_mask(self) -> int:
        return sum(FLAG_BITS[f] for f in self.flags)

    @property
    def ro(self) -> int:
        """Implemented bits that software cannot change by writing."""
        return self.impl & ~self.wmask & ~self.w1c

    @property
    def scope(self) -> str:
        return self.name[:len(self.name) - len(self.atdf_name)]

    def hooks(self) -> list[str]:
        return [f for f in self.flags if f.startswith("HOOK_") or f == "DYNAMIC"]


@dataclass
class ResetOverride:
    key: str
    offset: int
    size: int
    value: int


@dataclass
class FuseReset:
    """A register field whose reset value is loaded from an NVM fuse field."""
    key: str                        # "CTRLA.ENABLE"
    fuse: str                       # "USER_FUSES.USER_WORD_0.WDT_ENABLE"
    fuse_addr: int
    fuse_mask: int
    reg_offset: int
    reg_size: int
    reg_mask: int
    invert: bool
    note: str = ""


@dataclass
class IrqSource:
    flag: str
    enable: str
    source: str


@dataclass
class ViewSem:
    mode: str                       # "" when the IP has no modes
    value: int | None               # selector value of this mode
    caption: str
    regs: list[RegSem]
    overrides: list[ResetOverride]
    reset_from: list[FuseReset]
    irq: IrqSource | None
    consts: list[ConstScope]

    def reg(self, name: str) -> RegSem:
        for r in self.regs:
            if r.name == name:
                return r
        raise KeyError(name)


@dataclass
class Selector:
    """Field selecting the register view, e.g. SERCOM CTRLA.MODE."""
    qualifier: str
    register: str
    field: str
    offset: int
    size: int
    shift: int
    width: int
    reset_value: int


@dataclass
class IpSem:
    module: atdf.Module
    key: str                        # file stem, e.g. "sercom_u2201"
    cname: str                      # C name part, e.g. "SERCOM"
    group: str | None               # MMIO register group name
    size: int
    views: list[ViewSem]
    default_view: str | None
    selector: Selector | None
    extra_consts: list[ConstScope]  # RAM structures, fuse rows: constants only
    overlay: Overlay | None
    overlay_status: str             # applied | version-mismatch | none
    core: bool
    diags: list[Diag]

    def view(self, mode: str) -> ViewSem:
        for v in self.views:
            if v.mode == mode:
                return v
        raise KeyError(mode)

    def table_prefix(self, view: ViewSem) -> str:
        return self.cname + (f"_{view.mode}" if view.mode else "")


class Context:
    """Diagnostics for one IP; errors abort generation."""

    def __init__(self, where: str):
        self.where = where
        self.diags: list[Diag] = []

    def warn(self, msg: str):
        self.diags.append(Diag("warning", self.where, msg))

    def info(self, msg: str):
        self.diags.append(Diag("info", self.where, msg))

    def error(self, msg: str):
        raise OverlayError(f"{self.where}: {msg}")


# --------------------------------------------------------------------------
# Flattening register groups


@dataclass
class _Slot:
    reg: atdf.Register
    name: str
    scope: str                      # "" or "GROUP[1]."
    offset: int


def _in_mode(modes: tuple[str, ...], mode: str) -> bool:
    return not modes or mode in modes


def _flatten(module: atdf.Module, group: atdf.RegisterGroup, mode: str,
             base: int = 0, scope: str = "") -> list[_Slot]:
    slots = [_Slot(r, scope + r.name, scope, base + r.offset)
             for r in group.registers if _in_mode(r.modes, mode)]
    for sub in group.subgroups:
        if not _in_mode(sub.modes, mode):
            continue
        inner = module.groups[sub.name_in_module]
        for i in range(sub.count):
            sub_scope = scope + (f"{sub.name}[{i}]." if sub.count > 1
                                 else f"{sub.name}.")
            slots += _flatten(module, inner, mode,
                              base + sub.offset + i * sub.size, sub_scope)
    return slots


def _group_extent(module: atdf.Module, group: atdf.RegisterGroup,
                  mode: str) -> int:
    ends = [r.offset + r.size * r.count
            for r in group.registers if _in_mode(r.modes, mode)]
    for sub in group.subgroups:
        if _in_mode(sub.modes, mode):
            inner = _group_extent(module, module.groups[sub.name_in_module], mode)
            ends.append(sub.offset + (sub.count - 1) * sub.size +
                        max(sub.size or 0, inner))
    return max(ends, default=0)


# --------------------------------------------------------------------------
# Fields and constants


def _field_union(reg: atdf.Register) -> dict[str, int]:
    """Field name -> union of its masks over register-level mode variants."""
    out: dict[str, int] = {}
    for b in reg.bitfields:
        out[b.name] = out.get(b.name, 0) | b.mask
    return out


def _field_defs(module: atdf.Module, reg: atdf.Register,
                regmode: str | None) -> list[FieldDef]:
    """Fields of one layout: regmode None = default (no modes or DEFAULT)."""
    out = []
    for b in reg.bitfields:
        if regmode is None:
            keep = not b.modes or "DEFAULT" in b.modes
        else:
            keep = regmode in b.modes
        if keep:
            out.append(FieldDef(b.name, b.mask, b.caption,
                                module.value_groups.get(b.values or "", [])))
    return out


def _reg_consts(module: atdf.Module, reg: atdf.Register) -> list[RegConst]:
    """REGn() for the default layout plus one per non-DEFAULT register mode."""
    consts = [RegConst(reg.name, reg.offset, reg.size, reg.count,
                       _field_defs(module, reg, None))]
    for m in reg.regmodes:
        if m.name != "DEFAULT":
            consts.append(RegConst(f"{reg.name}_{m.name}", reg.offset, reg.size,
                                   reg.count, _field_defs(module, reg, m.name)))
    return consts


def _scope_consts(module: atdf.Module, group: atdf.RegisterGroup, mode: str,
                  prefix: str) -> list[ConstScope]:
    regs = [c for r in group.registers if _in_mode(r.modes, mode)
            for c in _reg_consts(module, r)]
    scopes = [ConstScope(prefix, regs)] if regs else []
    for sub in group.subgroups:
        if not _in_mode(sub.modes, mode):
            continue
        inner = _scope_consts(module, module.groups[sub.name_in_module], mode,
                              f"{prefix}_{sub.name}")
        if inner:
            inner[0].offset = sub.offset
            inner[0].stride = sub.size
            inner[0].count = sub.count
        scopes += inner
    return scopes


def _describe(spec) -> str:
    """Overlay rule as report text: 'all except SWRST, ENABLE', 'A->B, C->D'."""
    if isinstance(spec, dict) and set(spec) == {"all_except"}:
        return "all except " + ", ".join(spec["all_except"])
    if isinstance(spec, dict):
        return ", ".join(f"{k}->{v}" for k, v in spec.items())
    if isinstance(spec, list):
        return ", ".join(spec)
    return str(spec)


def _pair_storage(name: str, names: set[str]) -> str | None:
    """INTENSET/INTENCLR-style pairs: the storage is the <X>SET register."""
    for suffix in ("SET", "CLR"):
        stem = name[:-3]
        if name.endswith(suffix) and stem and {stem + "SET", stem + "CLR"} <= names:
            return stem + "SET"
    return None


# --------------------------------------------------------------------------
# One register


class _RegBuilder:
    """Computes one RegSem from ATDF facts, overlay rules and heuristics."""

    def __init__(self, ctx: Context, module: atdf.Module, slot: _Slot,
                 rules: RegisterRules | None, scope_names: set[str]):
        self.ctx = ctx
        self.module = module
        self.slot = slot
        self.reg = slot.reg
        self.rules = rules
        self.names = scope_names
        self.fields = _field_union(slot.reg)

    def fmask(self, names, what: str) -> int:
        mask = 0
        for n in names:
            if n not in self.fields:
                self.ctx.error(f"{self.slot.name}.{n} ({what}) is not an ATDF "
                               f"field; fields are {sorted(self.fields)}")
            mask |= self.fields[n]
        return mask

    def build(self) -> RegSem:
        r, rules = self.reg, self.rules
        prov: list[Provenance] = []
        impl = 0
        for m in self.fields.values():
            impl |= m
        if not self.fields:
            impl = r.size_mask
            prov.append(Provenance("impl", "heuristic",
                                   "no ATDF bitfields: all bits implemented"))
        reset = r.initval
        if reset is None:
            reset = 0
            prov.append(Provenance("reset", "heuristic", "no ATDF initval: 0"))
        if reset & ~impl:
            prov.append(Provenance("impl", "atdf",
                                   f"initval bits 0x{reset & ~impl:X} outside "
                                   f"bitfields kept implemented"))
            impl |= reset
        sem = RegSem(name=self.slot.name, atdf_name=r.name,
                     offset=self.slot.offset, size=r.size, count=r.count,
                     stride=r.size, reset=reset, impl=impl,
                     wmask=impl if "W" in r.rw else 0, rw=r.rw,
                     atdf_access=r.access, access_size=r.access_size,
                     caption=r.caption, note=rules.note if rules else "",
                     fields=_field_defs(self.module, r, None),
                     field_union=dict(self.fields), provenance=prov)
        flags = self._access(sem, prov)
        self._w1c_and_alias(sem, flags, prov)
        self._read_as_zero(sem, prov)
        if rules:
            ro = self.fmask(rules.ro, "ro")
            if ro:
                prov.append(Provenance("ro", "overlay",
                                       f"read-only {_describe(rules.ro)} = 0x{ro:X}"))
            sem.wmask &= ~ro
            sem.w1c &= ~ro
            if rules.pac_protected:
                flags.add("PAC")
            if rules.dynamic:
                flags.add("DYNAMIC")
                prov.append(Provenance("dynamic", "overlay", "dynamic"))
            for h in rules.hooks:
                flags.add("HOOK_" + h.upper())
                prov.append(Provenance("hooks", "overlay", h))
            if rules.bus_error_when_busy:
                flags.add("BUSY_BUSERR")
        if r.name.endswith("SYNCBUSY") and "DYNAMIC" not in flags:
            flags.add("DYNAMIC")
            prov.append(Provenance("dynamic", "heuristic",
                                   "register named *SYNCBUSY: dynamic, reads 0"))
        sem.wmask &= ~sem.w1c
        self._enable_protection(sem, prov)
        sem.flags = [f for f in FLAG_ORDER if f in flags]
        return sem

    def _access(self, sem: RegSem, prov: list) -> set[str]:
        r, rules = self.reg, self.rules
        flags = set()
        if r.rw == "R" or (rules and rules.read_only):
            flags.add("READ_ONLY")
            sem.wmask = 0
            src = "atdf" if r.rw == "R" else "overlay"
            prov.append(Provenance("access", src,
                                   f"READ_ONLY (ATDF rw={r.rw})"))
        if r.rw == "W" or (rules and rules.write_only):
            flags.add("WRITE_ONLY")
            sem.rdz |= sem.impl
            src = "atdf" if r.rw == "W" else "overlay"
            prov.append(Provenance("access", src,
                                   f"WRITE_ONLY, reads 0 (ATDF rw={r.rw})"))
        return flags

    def _alias(self, prov: list) -> dict | None:
        r, rules = self.reg, self.rules
        op, target = (r.atomic_op.split(":", 1) if r.atomic_op
                      else (None, None))
        if rules and rules.alias:
            alias = rules.alias
            if alias["storage"] not in self.names:
                self.ctx.error(f"{self.slot.name}: alias storage "
                               f"{alias['storage']} is not in this view")
            if op and target not in (r.name, alias["storage"]):
                self.ctx.warn(f"{self.slot.name}: overlay alias storage "
                              f"{alias['storage']} differs from ATDF "
                              f"atomic-op={r.atomic_op}")
            prov.append(Provenance("alias", "overlay",
                                   f"{alias['op']} -> {alias['storage']}"))
            return alias
        if op and target != r.name:
            if target not in self.names:
                self.ctx.error(f"{self.slot.name}: atomic-op target {target} "
                               f"is not in this view")
            prov.append(Provenance("alias", "heuristic",
                                   f"ATDF atomic-op={r.atomic_op}"))
            return {"storage": target, "op": op}
        pair = _pair_storage(r.name, self.names)
        if pair:
            prov.append(Provenance("alias", "heuristic",
                                   f"{pair[:-3]}SET/{pair[:-3]}CLR pair: shared "
                                   f"storage {pair}"))
            return {"storage": pair, "op": "set" if r.name == pair else "clear"}
        if op == "set":
            prov.append(Provenance("alias", "heuristic",
                                   f"ATDF atomic-op={r.atomic_op}: write 1 sets"))
            return {"storage": r.name, "op": "set"}
        return None

    def _w1c_and_alias(self, sem: RegSem, flags: set, prov: list):
        r, rules = self.reg, self.rules
        alias = self._alias(prov)
        if alias:
            if rules and rules.w1c is not None:
                self.ctx.error(f"{self.slot.name}: w1c given for an alias register")
            flags.add("ALIAS_" + alias["op"].upper())
            sem.alias_of = self.slot.scope + alias["storage"]
            return
        atdf_w1c = sem.impl if r.atomic_op == f"clear:{r.name}" else 0
        if rules and rules.w1c is not None:
            w1c = (sem.impl if rules.w1c == "all"
                   else self.fmask(rules.w1c, "w1c"))
            if atdf_w1c and atdf_w1c != w1c:
                self.ctx.info(f"{self.slot.name}: ATDF atomic-op={r.atomic_op} "
                              f"implies w1c=0x{atdf_w1c:X}, overlay says "
                              f"0x{w1c:X}")
            prov.append(Provenance("w1c", "overlay",
                                   f"{_describe(rules.w1c)} = 0x{w1c:X}" +
                                   ("" if atdf_w1c else " (no ATDF atomic-op)")))
            sem.w1c = w1c
        elif atdf_w1c:
            sem.w1c = atdf_w1c
            prov.append(Provenance("w1c", "heuristic",
                                   f"ATDF atomic-op={r.atomic_op}: all fields "
                                   f"W1C (0x{atdf_w1c:X})"))

    def _read_as_zero(self, sem: RegSem, prov: list):
        r, rules = self.reg, self.rules
        if rules and rules.strobe is not None:
            sem.rdz |= self.fmask(rules.strobe, "strobe")
            prov.append(Provenance("rdz", "overlay",
                                   f"strobe {_describe(rules.strobe)}"))
        if rules and rules.self_clearing is not None:
            sem.rdz |= self.fmask(rules.self_clearing, "self_clearing")
            prov.append(Provenance("rdz", "overlay",
                                   f"self_clearing {_describe(rules.self_clearing)}"))
            return
        if self.fields.get("SWRST", 0) & sem.wmask:
            sem.rdz |= self.fields["SWRST"]
            prov.append(Provenance("rdz", "heuristic", "SWRST self-clearing"))
        if r.name in ("CTRLBSET", "CTRLBCLR") and self.fields.get("CMD", 0):
            sem.rdz |= self.fields["CMD"]
            prov.append(Provenance("rdz", "heuristic",
                                   f"CMD in {r.name} self-clearing"))

    def _enable_protection(self, sem: RegSem, prov: list):
        ep = self.rules.enable_protected if self.rules else None
        if ep is None:
            return
        if ep == "all":
            sem.enprot = sem.wmask
        elif isinstance(ep, dict):
            sem.enprot = sem.wmask & ~self.fmask(ep["all_except"],
                                                 "enable_protected")
        else:
            sem.enprot = self.fmask(ep, "enable_protected")
            if sem.enprot & ~sem.wmask:
                self.ctx.error(f"{self.slot.name}: enable-protected bits "
                               f"0x{sem.enprot & ~sem.wmask:X} are not writable")
        prov.append(Provenance("enprot", "overlay",
                               f"{_describe(ep)} = 0x{sem.enprot:X}"))


# --------------------------------------------------------------------------
# One view


class _ViewBuilder:
    def __init__(self, ctx: Context, device: atdf.Device, module: atdf.Module,
                 group: atdf.RegisterGroup, mode: atdf.Mode | None,
                 rules: View | None):
        self.ctx = ctx
        self.device = device
        self.module = module
        self.group = group
        self.mode = mode
        self.rules = rules
        self.mode_name = mode.name if mode else ""
        self.slots = sorted(_flatten(module, group, self.mode_name),
                            key=lambda s: (s.offset, s.name))

    def build(self, prefix: str) -> ViewSem:
        rules = self.rules.registers if self.rules else {}
        unknown = sorted(set(rules) - {s.reg.name for s in self.slots})
        if unknown:
            self.ctx.error(f"overlay names registers absent from the ATDF: "
                           f"{unknown}")
        by_scope: dict[str, set[str]] = {}
        for s in self.slots:
            by_scope.setdefault(s.scope, set()).add(s.reg.name)
        regs = [_RegBuilder(self.ctx, self.module, s, rules.get(s.reg.name),
                            by_scope[s.scope]).build()
                for s in self.slots]
        index = {r.name: i for i, r in enumerate(regs)}
        for r in regs:
            if r.alias_of is not None:
                r.alias_index = index[r.alias_of]
        by_name = {r.name: r for r in regs}
        for r in regs:
            rule = rules.get(r.atdf_name)
            if rule and rule.sync is not None:
                self._sync_overlay(r, rule.sync, by_name)
            elif r.atdf_access in ("WSYNC", "RWSYNC"):
                self._sync_heuristic(r, by_name)
        return ViewSem(mode=self.mode_name,
                       value=self.mode.value if self.mode else None,
                       caption=self.mode.caption if self.mode else "",
                       regs=regs, overrides=self._overrides(regs),
                       reset_from=self._reset_from(regs),
                       irq=self._irq(regs, rules),
                       consts=_scope_consts(self.module, self.group,
                                            self.mode_name, prefix))

    # -- synchronization ----------------------------------------------------

    def _sync_bits(self, r: RegSem, regname: str | None,
                   by_name: dict[str, RegSem]) -> tuple[str, dict[str, int]]:
        name = r.scope + (regname or "SYNCBUSY")
        if name not in by_name:
            self.ctx.error(f"{r.name}: sync register {name} not found")
        return name, by_name[name].field_union

    def _sync_overlay(self, r: RegSem, spec, by_name: dict[str, RegSem]):
        if isinstance(spec, dict):
            mask, sync_reg = 0, None
            for fname, bit in spec.items():
                if fname not in r.field_union:
                    self.ctx.error(f"{r.name}.{fname} (sync) is not an ATDF field")
                regname, _, bitname = bit.rpartition(".")
                sync_reg, bits = self._sync_bits(r, regname or None, by_name)
                if bitname not in bits:
                    self.ctx.error(f"{r.name}: sync bit {bit} is not a field "
                                   f"of {sync_reg}")
                mask |= bits[bitname]
            r.syncbusy, r.sync_reg = mask, sync_reg
        elif SYNC_ELEMENT.match(spec):
            regname, stem = SYNC_ELEMENT.match(spec).groups()
            r.sync_reg, bits = self._sync_bits(r, regname, by_name)
            masks = []
            for i in range(r.count):
                if f"{stem}{i}" not in bits:
                    self.ctx.error(f"{r.name}: sync bit {stem}{i} missing in "
                                   f"{r.sync_reg}")
                masks.append(bits[f"{stem}{i}"])
            if any(m != masks[0] << i for i, m in enumerate(masks)):
                self.ctx.error(f"{r.name}: per-element sync bits are not "
                               f"consecutive")
            r.syncbusy, r.sync_per_element = masks[0], True
        else:
            regname, _, bitname = spec.rpartition(".")
            r.sync_reg, bits = self._sync_bits(r, regname or None, by_name)
            if bitname not in bits:
                self.ctx.error(f"{r.name}: sync bit {spec} is not a field of "
                               f"{r.sync_reg}")
            r.syncbusy = bits[bitname]
        r.provenance.append(Provenance(
            "sync", "overlay", f"{_describe(spec)}: {r.sync_reg} 0x{r.syncbusy:X}"
            + (" << element" if r.sync_per_element else "")))
        if r.atdf_access is None:
            self.ctx.info(f"{r.name}: overlay sync, but the ATDF has no "
                          f"access=WSYNC/RWSYNC")

    def _sync_heuristic(self, r: RegSem, by_name: dict[str, RegSem]):
        sb_name = r.scope + "SYNCBUSY"
        if sb_name not in by_name:
            self.ctx.warn(f"{r.name}: ATDF access={r.atdf_access} but no "
                          f"SYNCBUSY register in this view")
            return
        bits = by_name[sb_name].field_union
        mask = 0
        for fname, fmask in r.field_union.items():
            if fname in bits:
                mask |= bits[fname]
        stem = r.atdf_name[:-3] if r.atdf_name.endswith(("SET", "CLR")) else None
        per_element = False
        if not mask:
            for candidate in (r.atdf_name, stem):
                if candidate in bits:
                    mask = bits[candidate]
                    break
        if not mask and r.count > 1 and f"{r.atdf_name}0" in bits:
            mask, per_element = bits[f"{r.atdf_name}0"], True
        if not mask:
            self.ctx.warn(f"{r.name}: ATDF access={r.atdf_access} but no "
                          f"SYNCBUSY bit matches by name (no overlay mapping)")
            return
        r.syncbusy, r.sync_reg, r.sync_per_element = mask, sb_name, per_element
        r.provenance.append(Provenance("sync", "heuristic",
                                       f"access={r.atdf_access}; SYNCBUSY bits "
                                       f"matched by name: 0x{mask:X}"
                                       + (" << element" if per_element else "")))

    # -- reset values ---------------------------------------------------------

    def _overrides(self, regs: list[RegSem]) -> list[ResetOverride]:
        out = []
        items = sorted(self.rules.reset_overrides.items()) if self.rules else []
        for text, (key, value) in items:
            if key.group is None:
                matches = [r for r in regs if r.atdf_name == key.register]
            else:
                scope = key.group + (f"[{key.group_index}]."
                                     if key.group_index is not None else ".")
                matches = [r for r in regs if r.name == scope + key.register]
            if not matches:
                self.ctx.error(f"reset override {text}: no such register")
            for r in matches:
                self._apply_override(r, key, text, value, out)
        return sorted(out, key=lambda o: o.offset)

    def _apply_override(self, r: RegSem, key, text: str, value: int,
                        out: list[ResetOverride]):
        if value & ~r.size_mask:
            self.ctx.error(f"reset override {text}: 0x{value:X} does not fit "
                           f"{r.size} bytes")
        if value & ~r.impl:
            self.ctx.warn(f"reset override {text}: bits 0x{value & ~r.impl:X} "
                          f"outside bitfields kept implemented")
            r.impl |= value
        if key.index is None:
            old, r.reset = r.reset, value
            r.provenance.append(Provenance(
                "reset", "overlay",
                f"reset_overrides {text} = 0x{value:X} (ATDF initval 0x{old:X}"
                f"{', same' if old == value else ''})"))
            return
        if key.index >= r.count:
            self.ctx.error(f"reset override {text}: {r.name} has {r.count} "
                           f"elements")
        out.append(ResetOverride(text, r.offset + key.index * r.stride, r.size,
                                 value))
        r.provenance.append(Provenance(
            "reset", "overlay", f"reset_overrides {text}: element {key.index} "
                                f"= 0x{value:X} (others 0x{r.reset:X})"))

    def _reset_from(self, regs: list[RegSem]) -> list[FuseReset]:
        out = []
        items = sorted(self.rules.reset_from.items()) if self.rules else []
        for key, spec in items:
            regname, _, fname = key.rpartition(".")
            reg = next((r for r in regs if r.name == regname), None)
            if reg is None or fname not in reg.field_union:
                self.ctx.error(f"reset_from {key}: no such register field")
            reg_mask = reg.field_union[fname]
            addr, fmask = _fuse_field(self.ctx, self.device, spec["fuse"])
            if bin(reg_mask).count("1") != bin(fmask).count("1"):
                self.ctx.error(f"reset_from {key}: width differs from "
                               f"{spec['fuse']}")
            invert = bool(spec.get("invert"))
            out.append(FuseReset(key, spec["fuse"], addr, fmask, reg.offset,
                                 reg.size, reg_mask, invert,
                                 str(spec.get("note", ""))))
            reg.provenance.append(Provenance(
                "reset", "overlay", f"reset_from {fname} <- "
                                    f"{'NOT ' if invert else ''}{spec['fuse']}"))
        return out

    def _irq(self, regs: list[RegSem], rules) -> IrqSource | None:
        names = {r.name for r in regs}
        for r in regs:
            rule = rules.get(r.atdf_name)
            if rule and rule.irq_enable:
                if rule.irq_enable not in names:
                    self.ctx.error(f"{r.name}: irq_source enable register "
                                   f"{rule.irq_enable} missing")
                return IrqSource(r.name, rule.irq_enable, "overlay")
        if {"INTFLAG", "INTENSET"} <= names:
            return IrqSource("INTFLAG", "INTENSET", "heuristic")
        return None


def _fuse_field(ctx: Context, device: atdf.Device, spec: str) -> tuple[int, int]:
    """'USER_FUSES.USER_WORD_0.WDT_PER' -> (absolute word address, mask)."""
    parts = spec.split(".")
    if len(parts) == 3:
        gname, rname, fname = parts
        for inst in device.instances:
            for g in inst.groups:
                if g.name_in_module != gname:
                    continue
                for r in device.modules[inst.module].groups[gname].registers:
                    for b in r.bitfields:
                        if (r.name, b.name) == (rname, fname):
                            return g.base + r.offset, b.mask
    ctx.error(f"fuse field {spec!r} not found (expected GROUP.REGISTER.FIELD)")
    raise AssertionError("unreachable")


# --------------------------------------------------------------------------
# One IP


def _selector(ctx: Context, group: atdf.RegisterGroup,
              views: list[ViewSem]) -> Selector | None:
    if not group.modes:
        return None
    quals = sorted({m.qualifier for m in group.modes})
    masks = sorted({m.mask for m in group.modes if m.mask})
    if masks:
        ctx.info(f"ATDF mode masks {masks} ignored: the mode value is compared "
                 f"with the whole selector field")
    names = {r.name for r in views[0].regs}
    picked = set()
    for q in quals:
        # "SERCOM.USART_CTRLA.MODE", "TC.CTRLA.MODE", "RTC.MODE0.CTRLA.MODE"
        parts = q.split(".")
        reg = parts[-2]
        if reg not in names and "_" in reg:
            reg = reg.rsplit("_", 1)[1]
        picked.add((reg, parts[-1]))
    if len(picked) != 1:
        ctx.error(f"mode qualifiers name different fields: {quals}")
    reg, fld = picked.pop()
    found = set()
    for v in views:
        r = next((x for x in v.regs if x.name == reg), None)
        if r is None or fld not in r.field_union:
            ctx.error(f"mode {v.mode}: selector {reg}.{fld} missing")
        found.add((r.offset, r.size, r.field_union[fld], r.reset))
    if len(found) != 1:
        ctx.error(f"mode selector {reg}.{fld} differs between views")
    offset, size, mask, reset = found.pop()
    shift = (mask & -mask).bit_length() - 1
    return Selector(quals[0], reg, fld, offset, size, shift,
                    bin(mask).count("1"), (reset & mask) >> shift)


def _mmio_groups(device: atdf.Device, module: atdf.Module) -> set[str]:
    return {g.name_in_module for inst in device.instances
            if inst.module == module.name for g in inst.groups
            if g.address_space == "base"}


def _const_prefix(module: atdf.Module, group: atdf.RegisterGroup) -> str:
    """Prefix for a constants-only group, avoiding repeated names like CMSIS:
    CAN_RXBE_DATA -> MCHP_CAN_RXBE_DATA, USER_FUSES.USER_WORD_0 ->
    MCHP_FUSES_USER_WORD_0, DMAC_DESCRIPTOR.BTCTRL -> MCHP_DMAC_DESCRIPTOR_BTCTRL.
    """
    names = [r.name for r in group.registers]
    stem = group.name.removesuffix("_" + module.name)
    if all(n.startswith(group.name + "_") for n in names):
        return "MCHP"
    if stem != group.name and all(n.startswith(stem + "_") for n in names):
        return f"MCHP_{module.name}"
    if group.name.startswith(module.name + "_"):
        return "MCHP_" + group.name
    return f"MCHP_{module.name}_{group.name}"


def _check_overlay_modes(ctx: Context, ov: Overlay, group: atdf.RegisterGroup):
    ov_modes = set(ov.views) - {""}
    atdf_modes = {m.name for m in group.modes}
    if bool(ov_modes) != bool(atdf_modes) or ov_modes - atdf_modes:
        ctx.error(f"overlay modes {sorted(ov_modes)} do not match ATDF modes "
                  f"{sorted(atdf_modes)}")
    for m in sorted(atdf_modes - ov_modes):
        ctx.warn(f"mode {m}: no overlay rules, heuristics only")


def build_ip(device: atdf.Device, module: atdf.Module,
             overlays: dict[tuple[str, str], Overlay]) -> IpSem:
    core = module.id is None
    key = (module.name.lower() if core
           else f"{module.name.lower()}_{module.id.lower()}")
    ctx = Context(key)
    ov = overlays.get((module.name, module.id or ""))
    status = "none"
    if ov and module.version in ov.versions:
        status = "applied"
    elif ov:
        status = "version-mismatch"
        ctx.warn(f"overlay {ov.file} lists versions {ov.versions} but the ATDF "
                 f"has {module.version}: overlay NOT applied, heuristics only")
    mmio = sorted(_mmio_groups(device, module))
    if len(mmio) > 1:
        ctx.error(f"several MMIO register groups {mmio}")
    group = module.groups[mmio[0]] if mmio else None
    views: list[ViewSem] = []
    extra = []
    for gname, g in module.groups.items():
        used = group is not None and (
            g is group or any(s.name_in_module == gname for s in group.subgroups))
        if not used and g.registers:
            extra += _scope_consts(module, g, "", _const_prefix(module, g))
    size = 0
    if group is not None:
        applied = ov if status == "applied" else None
        if applied:
            _check_overlay_modes(ctx, applied, group)
        for mode in group.modes or [None]:
            name = mode.name if mode else ""
            ctx.where = key + (f"/{name}" if name else "")
            vb = _ViewBuilder(ctx, device, module, group, mode,
                              applied.views.get(name) if applied else None)
            if vb.slots:
                prefix = f"MCHP_{module.name}" + (f"_{name}" if name else "")
                views.append(vb.build(prefix))
            size = max(size, _group_extent(module, group, name))
        ctx.where = key
        views.sort(key=lambda v: (v.value if v.value is not None else -1, v.mode))
    selector = _selector(ctx, group, views) if views else None
    default = None
    if views:
        default = views[0].mode
        if selector:
            default = next((v.mode for v in views
                            if v.value == selector.reset_value), default)
    return IpSem(module=module, key=key, cname=module.name,
                 group=group.name if group else None,
                 size=(size + 3) & ~3 if views else 0, views=views,
                 default_view=default, selector=selector, extra_consts=extra,
                 overlay=ov, overlay_status=status, core=core, diags=ctx.diags)
