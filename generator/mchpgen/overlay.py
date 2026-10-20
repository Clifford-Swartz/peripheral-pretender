# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Load and structurally validate behavior overlays (overlays/<ip>_<id>.yaml).

Names inside an overlay (registers, fields, SYNCBUSY bits) are checked later
against the ATDF by semantics.py; this module only checks the YAML shape so
that typos in keys fail loudly instead of being ignored.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass, field

import yaml


class OverlayError(Exception):
    pass


REGISTER_KEYS = {
    "pac_protected", "enable_protected", "sync", "write_while_syncbusy",
    "strobe", "self_clearing", "w1c", "ro", "alias", "hooks", "dynamic",
    "write_only", "read_only", "irq_source", "note", "optional",
    "optional_fields", "extra_fields",
}
TOP_KEYS = {"ip", "id", "versions", "references", "notes", "registers",
            "modes", "reset_overrides", "reset_from"}
MODE_KEYS = {"registers", "reset_overrides", "reset_from", "note"}
ALIAS_OPS = ("set", "clear", "toggle")
HOOKS = ("pre_read", "post_write")

# "REG", "REG[3]", "GROUP[0].PMUX[15]"
OVERRIDE_KEY = re.compile(r"^(?:([A-Z][A-Z0-9_]*)\[(\d+)\]\.)?"
                          r"([A-Z][A-Z0-9_]*)(?:\[(\d+)\])?$")


@dataclass
class RegisterRules:
    """Overlay rules for one register (all optional)."""
    pac_protected: bool = False
    enable_protected: object = None     # "all" | {"all_except": [..]} | [..]
    sync: object = None                 # {FIELD: BIT} | "BIT" | "BIT{n}"
    bus_error_when_busy: bool = False
    strobe: list[str] | None = None     # None: not specified (heuristics apply)
    self_clearing: list[str] | None = None
    w1c: list[str] | str | None = None  # names, "all", or None
    ro: list[str] = field(default_factory=list)
    alias: dict | None = None           # {"storage": REG, "op": set|clear|toggle}
    hooks: list[str] = field(default_factory=list)
    dynamic: bool = False
    write_only: bool = False
    read_only: bool = False
    irq_enable: str | None = None
    note: str = ""
    optional: bool = False              # absent on some device variants
    optional_fields: list[str] = field(default_factory=list)
    # Fields silicon has and the ATDF lacks: {NAME: (lsb, width)}
    extra_fields: dict = field(default_factory=dict)


@dataclass
class ResetOverrideKey:
    group: str | None
    group_index: int | None
    register: str
    index: int | None
    text: str


@dataclass
class View:
    """Rules for one register view (a mode, or the whole IP)."""
    registers: dict[str, RegisterRules]
    reset_overrides: dict[str, tuple[ResetOverrideKey, int]]
    reset_from: dict[str, dict]


@dataclass
class Overlay:
    path: str
    ip: str
    id: str
    versions: list[str]
    references: list[str]
    notes: str
    views: dict[str, View]          # mode name -> rules; "" when no modes
    sha1: str = ""

    @property
    def file(self) -> str:
        return os.path.basename(self.path)


def _fail(where: str, msg: str):
    raise OverlayError(f"{where}: {msg}")


def _name_list(where: str, value) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
        _fail(where, f"expected a list of names, got {value!r}")
    return list(value)


def _parse_rules(where: str, raw) -> RegisterRules:
    if raw is None:
        return RegisterRules()
    if not isinstance(raw, dict):
        _fail(where, f"expected a mapping, got {raw!r}")
    unknown = set(raw) - REGISTER_KEYS
    if unknown:
        _fail(where, f"unknown keys {sorted(unknown)}")
    r = RegisterRules()
    r.pac_protected = bool(raw.get("pac_protected", False))
    r.optional = bool(raw.get("optional", False))
    if "extra_fields" in raw:
        ef = raw["extra_fields"]
        if not isinstance(ef, dict) or not all(
                isinstance(k, str) and isinstance(v, list) and len(v) == 2 and
                all(isinstance(x, int) and x >= 0 for x in v) and v[1] > 0
                for k, v in ef.items()):
            _fail(where, f"extra_fields must map NAME: [lsb, width], got {ef!r}")
        r.extra_fields = {k: (v[0], v[1]) for k, v in ef.items()}
    if "optional_fields" in raw:
        r.optional_fields = _name_list(where + ".optional_fields",
                                       raw["optional_fields"])
    ep = raw.get("enable_protected")
    if ep is not None:
        if ep == "all":
            pass
        elif isinstance(ep, dict) and set(ep) == {"all_except"}:
            _name_list(where + ".enable_protected.all_except", ep["all_except"])
        elif isinstance(ep, list):
            _name_list(where + ".enable_protected", ep)
        else:
            _fail(where, f"enable_protected must be 'all', {{all_except: [..]}} "
                         f"or a list, got {ep!r}")
        r.enable_protected = ep
    sync = raw.get("sync")
    if sync is not None:
        if isinstance(sync, dict):
            for k, v in sync.items():
                if not isinstance(k, str) or not isinstance(v, str):
                    _fail(where, f"sync entries must be FIELD: BIT, got {k!r}: {v!r}")
        elif not isinstance(sync, str):
            _fail(where, f"sync must be a mapping or a bit name, got {sync!r}")
        r.sync = sync
    wws = raw.get("write_while_syncbusy")
    if wws is not None:
        if wws != "bus_error":
            _fail(where, f"write_while_syncbusy must be 'bus_error', got {wws!r}")
        r.bus_error_when_busy = True
    for key in ("strobe", "self_clearing", "ro"):
        if key in raw:
            setattr(r, key, _name_list(f"{where}.{key}", raw[key]))
    if raw.get("w1c") == "all":
        r.w1c = "all"
    elif "w1c" in raw:
        r.w1c = _name_list(where + ".w1c", raw["w1c"])
    alias = raw.get("alias")
    if alias is not None:
        if (not isinstance(alias, dict) or set(alias) != {"storage", "op"}
                or alias["op"] not in ALIAS_OPS):
            _fail(where, f"alias must be {{storage: REG, op: set|clear|toggle}}, "
                         f"got {alias!r}")
        r.alias = dict(alias)
    hooks = raw.get("hooks", [])
    for h in _name_list(where + ".hooks", hooks):
        if h not in HOOKS:
            _fail(where, f"unknown hook {h!r} (expected {HOOKS})")
    r.hooks = list(hooks)
    for key in ("dynamic", "write_only", "read_only"):
        setattr(r, key, bool(raw.get(key, False)))
    irq = raw.get("irq_source")
    if irq is not None:
        if not isinstance(irq, dict) or set(irq) != {"enable"}:
            _fail(where, f"irq_source must be {{enable: REG}}, got {irq!r}")
        r.irq_enable = irq["enable"]
    r.note = str(raw.get("note", ""))
    return r


def _parse_overrides(where: str, raw) -> dict[str, tuple[ResetOverrideKey, int]]:
    out = {}
    for key, value in (raw or {}).items():
        m = OVERRIDE_KEY.match(str(key))
        if not m or not isinstance(value, int):
            _fail(where, f"bad reset override {key!r}: {value!r} (expected "
                         f"'REG', 'REG[i]' or 'GROUP[g].REG[i]' -> integer)")
        grp, gidx, reg, idx = m.groups()
        out[str(key)] = (ResetOverrideKey(grp, int(gidx) if gidx else None, reg,
                                          int(idx) if idx else None, str(key)),
                         value)
    return out


def _parse_reset_from(where: str, raw) -> dict[str, dict]:
    out = {}
    for key, value in (raw or {}).items():
        if isinstance(value, str):
            value = {"fuse": value}
        if (not isinstance(value, dict) or "fuse" not in value
                or set(value) - {"fuse", "invert", "note"}):
            _fail(where, f"reset_from {key!r} must be 'GROUP.REG.FIELD' or "
                         f"{{fuse: ..., invert: bool}}, got {value!r}")
        out[str(key)] = dict(value)
    return out


def _parse_view(where: str, raw: dict) -> View:
    regs = {}
    for name, rules in (raw.get("registers") or {}).items():
        regs[str(name)] = _parse_rules(f"{where}.{name}", rules)
    return View(registers=regs,
                reset_overrides=_parse_overrides(where + ".reset_overrides",
                                                 raw.get("reset_overrides")),
                reset_from=_parse_reset_from(where + ".reset_from",
                                             raw.get("reset_from")))


def load(path: str) -> Overlay:
    import hashlib
    with open(path, "rb") as f:
        data = f.read()
    raw = yaml.safe_load(data)
    where = os.path.basename(path)
    if not isinstance(raw, dict):
        _fail(where, "top level must be a mapping")
    unknown = set(raw) - TOP_KEYS
    if unknown:
        _fail(where, f"unknown top-level keys {sorted(unknown)}")
    for key in ("ip", "id", "versions"):
        if key not in raw:
            _fail(where, f"missing '{key}'")
    versions = [str(v) for v in raw["versions"]]
    if ("modes" in raw) == ("registers" in raw):
        _fail(where, "exactly one of 'modes' or 'registers' is required")
    views = {}
    if "modes" in raw:
        for mode, body in raw["modes"].items():
            body = body or {}
            unknown = set(body) - MODE_KEYS
            if unknown:
                _fail(f"{where}.{mode}", f"unknown keys {sorted(unknown)}")
            views[str(mode)] = _parse_view(f"{where}.{mode}", body)
    else:
        views[""] = _parse_view(where, {"registers": raw["registers"]})
    top = _parse_view(where, {"reset_overrides": raw.get("reset_overrides"),
                              "reset_from": raw.get("reset_from")})
    for view in views.values():
        for key, item in top.reset_overrides.items():
            view.reset_overrides.setdefault(key, item)
        for key, item in top.reset_from.items():
            view.reset_from.setdefault(key, item)
    ov = Overlay(path=os.path.abspath(path), ip=str(raw["ip"]), id=str(raw["id"]),
                 versions=versions, references=list(raw.get("references") or []),
                 notes=str(raw.get("notes", "")), views=views)
    ov.sha1 = hashlib.sha1(data).hexdigest()
    expected = f"{ov.ip.lower()}_{ov.id.lower()}.yaml"
    if os.path.basename(path) != expected:
        _fail(where, f"file name must be {expected}")
    return ov


def load_dir(path: str) -> dict[tuple[str, str], Overlay]:
    """All overlays in a directory, keyed by (ip, id)."""
    out = {}
    for name in sorted(os.listdir(path)):
        if name.endswith((".yaml", ".yml")):
            ov = load(os.path.join(path, name))
            out[(ov.ip, ov.id)] = ov
    return out
