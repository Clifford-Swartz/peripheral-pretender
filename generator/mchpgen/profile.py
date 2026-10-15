# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Device profiles: facts that differ between devices sharing an IP.

Overlays describe one IP version and are shared by every device that has
it. A profile (profiles/<device lowercase>.yaml) holds what belongs to one
device instead: reset values set by the device's configuration (DSU.DID,
NVMCTRL.PARAM, PAC.STATUSx) and the default NVM rows image (production user
row, serial-number words, calibration row). Profile reset overrides replace
an overlay's override of the same register.
"""

from __future__ import annotations

import copy
import hashlib
import os
from dataclasses import dataclass, field

import yaml

from .overlay import (Overlay, OverlayError, _parse_overrides,
                      _parse_reset_from)

TOP_KEYS = {"device", "references", "notes", "reset_overrides", "nvm_rows",
            "silicon_revisions", "data_rows", "reset_from"}
NVM_KEYS = {"user_row", "serial_addrs", "serial_default", "words"}


@dataclass
class NvmRows:
    user_row: tuple[int, int]
    serial_addrs: tuple[int, int, int, int]
    serial_default: tuple[int, int, int, int]
    # Other words that differ from the 0xFF fill: (address, value, comment)
    words: list[tuple[int, int, str]] = field(default_factory=list)


@dataclass
class Profile:
    path: str
    device: str
    references: list[str]
    reset_overrides: dict[str, dict]    # module -> {key: (ResetOverrideKey, value)}
    nvm_rows: NvmRows | None            # Arm parts; None on AVR
    # (name, DSU.DID.REVISION on Arm, SYSCFG.REVID on AVR); first = default
    silicon_revisions: list[tuple[str, int]] = field(default_factory=list)
    # AVR: bytes of the data space that differ from the defaults (SIGROW
    # serial number, calibration): (data address, bytes, comment)
    data_rows: list[tuple[int, bytes, str]] = field(default_factory=list)
    # module -> {REG.FIELD: fuse spec}: where this device keeps the fuses
    # an overlay's reset_from names (SAM E51 WDT fuses sit in USER_WORD_1)
    reset_from: dict[str, dict] = field(default_factory=dict)
    sha1: str = ""

    @property
    def file(self) -> str:
        return os.path.basename(self.path)


def _fail(where: str, msg: str):
    raise OverlayError(f"{where}: {msg}")


def _ints(where: str, value, n: int) -> tuple[int, ...]:
    if (not isinstance(value, list) or len(value) != n
            or not all(isinstance(v, int) for v in value)):
        _fail(where, f"expected a list of {n} integers, got {value!r}")
    return tuple(value)


def _nvm_rows(where: str, raw) -> NvmRows:
    if not isinstance(raw, dict) or set(raw) - NVM_KEYS:
        _fail(where, f"keys must be among {sorted(NVM_KEYS)}")
    for key in ("user_row", "serial_addrs", "serial_default"):
        if key not in raw:
            _fail(where, f"missing '{key}'")
    words = []
    for i, w in enumerate(raw.get("words") or []):
        if (not isinstance(w, dict) or set(w) - {"addr", "value", "what"}
                or not isinstance(w.get("addr"), int)
                or not isinstance(w.get("value"), int)):
            _fail(f"{where}.words[{i}]", "expected {addr: int, value: int, "
                                         f"what: text}}, got {w!r}")
        words.append((w["addr"], w["value"], str(w.get("what", ""))))
    return NvmRows(user_row=_ints(where + ".user_row", raw["user_row"], 2),
                   serial_addrs=_ints(where + ".serial_addrs",
                                      raw["serial_addrs"], 4),
                   serial_default=_ints(where + ".serial_default",
                                        raw["serial_default"], 4),
                   words=words)


def _data_rows(where: str, raw) -> list[tuple[int, bytes, str]]:
    if not isinstance(raw, list):
        _fail(where, f"expected a list, got {raw!r}")
    rows = []
    for i, r in enumerate(raw):
        if (not isinstance(r, dict) or set(r) - {"addr", "bytes", "what"}
                or not isinstance(r.get("addr"), int)
                or not isinstance(r.get("bytes"), list)
                or not all(isinstance(b, int) and 0 <= b <= 255
                           for b in r["bytes"])):
            _fail(f"{where}[{i}]", "expected {addr: int, bytes: [0-255...], "
                                   f"what: text}}, got {r!r}")
        rows.append((r["addr"], bytes(r["bytes"]), str(r.get("what", ""))))
    return rows


def load(path: str) -> Profile:
    with open(path, "rb") as f:
        data = f.read()
    raw = yaml.safe_load(data)
    where = os.path.basename(path)
    if not isinstance(raw, dict):
        _fail(where, "top level must be a mapping")
    unknown = set(raw) - TOP_KEYS
    if unknown:
        _fail(where, f"unknown top-level keys {sorted(unknown)}")
    if "device" not in raw:
        _fail(where, "missing 'device'")
    expected = f"{str(raw['device']).lower()}.yaml"
    if os.path.basename(path) != expected:
        _fail(where, f"file name must be {expected}")
    overrides = {}
    for module, regs in (raw.get("reset_overrides") or {}).items():
        overrides[str(module)] = _parse_overrides(
            f"{where}.reset_overrides.{module}", regs)
    fuses = {}
    for module, items in (raw.get("reset_from") or {}).items():
        fuses[str(module)] = _parse_reset_from(
            f"{where}.reset_from.{module}", items)
    revs = raw.get("silicon_revisions")
    if (not isinstance(revs, dict) or not revs
            or not all(isinstance(v, int) and 0 <= v <= 255
                       for v in revs.values())):
        _fail(where, "silicon_revisions must map names to revision ids "
                     "(DSU.DID.REVISION 0-15 on Arm, SYSCFG.REVID on AVR; "
                     f"default first), got {revs!r}")
    nvm = raw.get("nvm_rows")
    prof = Profile(path=os.path.abspath(path), device=str(raw["device"]),
                   references=list(raw.get("references") or []),
                   reset_overrides=overrides,
                   reset_from=fuses,
                   nvm_rows=(_nvm_rows(where + ".nvm_rows", nvm)
                             if nvm is not None else None),
                   silicon_revisions=[(str(k), v) for k, v in revs.items()],
                   data_rows=_data_rows(where + ".data_rows",
                                        raw.get("data_rows") or []))
    prof.sha1 = hashlib.sha1(data).hexdigest()
    return prof


def find(profile_dir: str, device_name: str) -> Profile:
    path = os.path.join(profile_dir, f"{device_name.lower()}.yaml")
    if not os.path.isfile(path):
        raise OverlayError(f"no device profile {path}")
    return load(path)


def apply(overlays: dict[tuple[str, str], Overlay], prof: Profile,
          module_ids: dict[str, str]) -> dict[tuple[str, str], Overlay]:
    """Overlays with the profile's reset overrides (and fuse sources of
    reset values) layered on top.

    @module_ids maps the device's module names to their IP ids. Only the
    overlays that change are copied.
    """
    out = dict(overlays)
    for module, items in sorted(prof.reset_overrides.items()):
        key = (module, module_ids.get(module, ""))
        if key not in overlays:
            _fail(prof.file, f"reset_overrides.{module}: the device has no "
                             f"overlaid {module} module")
        ov = copy.deepcopy(out[key])
        for view in ov.views.values():
            view.reset_overrides.update(items)
        out[key] = ov
    for module, items in sorted(prof.reset_from.items()):
        key = (module, module_ids.get(module, ""))
        if key not in overlays:
            _fail(prof.file, f"reset_from.{module}: the device has no "
                             f"overlaid {module} module")
        ov = copy.deepcopy(out[key])
        for view in ov.views.values():
            unknown = set(items) - set(view.reset_from)
            if unknown:
                _fail(prof.file, f"reset_from.{module}: {sorted(unknown)} "
                                 "are not in the overlay's reset_from")
            view.reset_from.update(items)
        out[key] = ov
    return out
