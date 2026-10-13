# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Markdown report: provenance of every semantic, conflicts, missing initvals."""

from __future__ import annotations

from collections import Counter

from . import GENERATOR, nvmrows
from .device import DeviceSem
from .semantics import IpSem, RegSem


def _h(v: int, size: int = 4) -> str:
    return f"0x{v:0{2 * size}X}"


def _row(cells) -> str:
    return "| " + " | ".join(str(c).replace("|", "\\|") for c in cells) + " |"


def _table(head: list[str], rows: list[list]) -> list[str]:
    return [_row(head), _row(["---"] * len(head))] + [_row(r) for r in rows]


def _summary(ds: DeviceSem) -> list[str]:
    dev = ds.device
    flags = Counter(f for i in ds.instances for f in i.flags)
    shared = [f"{l.index} {l.name} ({', '.join(l.instances)})"
              for l in ds.irqs if len(l.instances) > 1]
    applied = sorted(ip.key for ip in ds.ips.values()
                     if ip.overlay_status == "applied")
    heuristic = sorted(ip.key for ip in ds.ips.values()
                       if ip.views and not ip.core and ip.overlay_status != "applied")
    levels = Counter(d.level for d in ds.diags)
    return _table(["item", "value"], [
        ["device", f"{dev.name} ({dev.architecture}, {dev.series})"],
        ["source", f"{dev.atdf_file} (timestamp {dev.timestamp}), "
                   f"{dev.dfp_name} {dev.dfp_version}"],
        ["instances", f"{len(ds.instances)} ({flags['CORE']} Arm core, "
                      f"{flags['FUSES']} fuse rows, {flags['NO_REGS']} without "
                      f"registers)"],
        ["module types", len(dev.modules)],
        ["NVIC lines", f"{len(ds.irqs)}; shared: {'; '.join(shared)}"],
        ["signals", len(ds.signals)],
        ["overlays applied", f"{len(applied)}: {', '.join(applied)}"],
        ["heuristics only", f"{len(heuristic)}: {', '.join(heuristic)}"],
        ["diagnostics", f"{levels['warning']} warnings, {levels['info']} notes"],
    ])


def _overlays(ds: DeviceSem) -> list[str]:
    rows = []
    for ip in sorted(ds.ips.values(), key=lambda x: x.key):
        if ip.core:
            continue
        mod = ip.module
        ov = ip.overlay
        rows.append([ip.key, mod.version,
                     f"{ov.file} {ov.versions}" if ov else "-",
                     ip.overlay_status,
                     ", ".join(v.mode or "(single)" for v in ip.views) or "-"])
    return _table(["IP", "ATDF version", "overlay", "status", "views"], rows)


def _heuristics(ds: DeviceSem) -> list[str]:
    by_kind: dict[str, list[str]] = {}
    for ip in sorted(ds.ips.values(), key=lambda x: x.key):
        if ip.core:
            continue
        for v in ip.views:
            for r in v.regs:
                for p in r.provenance:
                    if p.source == "heuristic":
                        key = f"{p.aspect}: {_kind(p.detail)}"
                        where = f"{ip.key}{'/' + v.mode if v.mode else ''} {r.name}"
                        by_kind.setdefault(key, []).append(where)
    out = []
    for kind, wheres in sorted(by_kind.items()):
        out.append(f"- **{kind}** ({len(wheres)}): " + ", ".join(wheres))
    return out


def _kind(detail: str) -> str:
    """Strip register-specific numbers so details group by rule."""
    for cut in (" (0x", ": all fields", " = 0x", ": 0x"):
        if cut in detail:
            detail = detail.split(cut)[0]
    if detail.startswith("ATDF atomic-op="):
        op = detail.split("=", 1)[1].split(":")[0]
        detail = f"ATDF atomic-op {op}:<register>"
    if "SET/" in detail and "pair" in detail:
        detail = "<X>SET/<X>CLR pair, storage <X>SET"
    if detail.startswith("CMD in "):
        detail = "CMD in CTRLBSET/CTRLBCLR self-clearing"
    return detail


def _resets(ds: DeviceSem) -> tuple[list[str], list[str]]:
    missing, changed = [], []
    for ip in sorted(ds.ips.values(), key=lambda x: x.key):
        if ip.core:
            continue
        for v in ip.views:
            where = f"{ip.key}{'/' + v.mode if v.mode else ''}"
            for r in v.regs:
                for p in r.provenance:
                    if p.aspect == "reset" and p.source == "heuristic":
                        missing.append(f"{where} {r.name}")
                    elif p.aspect == "reset":
                        changed.append(f"- {where} {r.name}: {p.detail}")
    return missing, changed


def _device_notes(ds: DeviceSem) -> list[str]:
    out = []
    for i in ds.instances:
        for n in i.notes:
            out.append(f"- {i.name}: {n}")
        if i.alias_name:
            out.append(f"- {i.name}: alias {i.alias_name} at 0x{i.alias_base:08X} "
                       f"(0x{i.alias_size:X} bytes) mirrors the registers at "
                       f"0x{i.base:08X}")
    for l in ds.irqs:
        if len(l.instances) > 1:
            out.append(f"- IRQ {l.index} {l.name}: OR gate, inputs "
                       + ", ".join(f"{n}={k}" for k, n in enumerate(l.instances)))
    out.append(f"- Default slot size 0x{0x400:X} (APB); larger only when the "
               f"registers extend further, smaller when the memory segment is.")
    return out


def _nvm(ds: DeviceSem) -> list[str]:
    rows = [[_h(w.addr), _h(w.value), w.what]
            for w in nvmrows.default_words(ds.device, ds.profile.nvm_rows)]
    rows += [[_h(a), _h(s), "serial number (default; SoC property serial-number)"]
             for a, s in zip(ds.profile.nvm_rows.serial_addrs,
                             ds.profile.nvm_rows.serial_default)]
    return (_table(["address", "value", "content"], rows) +
            ["", f"Everything else in 0x{nvmrows.BASE:08X}-"
                 f"0x{nvmrows.BASE + nvmrows.SIZE - 1:08X} reads 0x{nvmrows.FILL:02X}. "
                 f"NVMCTRL.PARAM implied by the NVMCTRL parameters: "
                 f"0x{nvmrows.nvm_param(ds.device):08X}."])


def _sources(r: RegSem) -> str:
    parts = [f"{p.aspect}={p.source}: {p.detail}" for p in r.provenance
             if p.source != "atdf" or p.aspect in ("impl",)]
    return "; ".join(parts)


def _ip_section(ip: IpSem) -> list[str]:
    mod = ip.module
    ov = (f"overlay `overlays/{ip.overlay.file}` ({ip.overlay_status})"
          if ip.overlay else "no overlay")
    out = [f"### {mod.name} {mod.id} {mod.version}", "",
           f"{ov}; register file 0x{ip.size:X} bytes"
           + (f"; views selected by {ip.selector.register}.{ip.selector.field}, "
              f"default {ip.default_view}" if ip.selector else "") + ".", ""]
    if ip.overlay and ip.overlay.references:
        out += ["References: " + "; ".join(ip.overlay.references), ""]
    for v in ip.views:
        if v.mode:
            out += [f"#### {v.mode} (value {v.value})", ""]
        rows = []
        for r in v.regs:
            rows.append([f"0x{r.offset:02X}", r.name,
                         f"{r.size}x{r.count}" if r.count > 1 else r.size,
                         _h(r.reset, r.size), " ".join(r.flags) or "-",
                         _h(r.w1c, r.size) if r.w1c else "",
                         _h(r.ro, r.size)
                         if r.ro and "READ_ONLY" not in r.flags else "",
                         _h(r.rdz, r.size) if r.rdz else "",
                         _h(r.enprot, r.size) if r.enprot else "",
                         f"{r.sync_reg} 0x{r.syncbusy:X}" if r.syncbusy else "",
                         _sources(r)])
        out += _table(["off", "register", "size", "reset", "flags", "w1c",
                       "ro bits", "rdz", "enprot", "sync", "provenance "
                       "(non-ATDF)"], rows)
        if v.irq:
            out += ["", f"IRQ source: {v.irq.flag} & {v.irq.enable} "
                        f"({v.irq.source})."]
        out.append("")
    return out


def report(ds: DeviceSem) -> str:
    dev = ds.device
    missing, changed = _resets(ds)
    out = [f"# Generator report: {dev.name}", "",
           f"Generated by `{GENERATOR}` from `{dev.atdf_file}` "
           f"({dev.dfp_name} {dev.dfp_version}). Do not edit; rerun the "
           f"generator.", "",
           "Sources: **atdf** = taken from the ATDF; **overlay** = "
           "`overlays/<ip>_<id>.yaml` (datasheet facts); **heuristic** = "
           "generator rule (spec 5.2), listed below.", "", "## Summary", ""]
    out += _summary(ds)
    out += ["", "## Overlays", ""] + _overlays(ds)
    out += ["", "## Diagnostics", ""]
    out += [f"- {d.level}: `{d.where}` {d.msg}" for d in ds.diags] or ["- none"]
    out += ["", "## Heuristics applied", "",
            "Rules used where no overlay rule exists (grouped; IP/view register):",
            ""] + (_heuristics(ds) or ["- none"])
    out += ["", "## Missing ATDF initvals (reset 0 unless an overlay sets it)", "",
            ", ".join(missing) or "none"]
    out += ["", "## Reset values set by overlays", ""] + (changed or ["- none"])
    out += ["", "## Device-level decisions", ""] + _device_notes(ds)
    out += ["", "## NVM rows default image", ""] + _nvm(ds)
    out += ["", "## Register semantics per IP", "",
            "Masks are per element. `ro bits`: implemented bits that writes "
            "cannot change (omitted for READ_ONLY registers).", ""]
    for ip in sorted(ds.ips.values(), key=lambda x: x.key):
        if ip.views and not ip.core:
            out += _ip_section(ip)
    return "\n".join(out) + "\n"
