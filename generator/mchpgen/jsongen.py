# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""devices/<device>.json: the same device description for tools and harness.

Numbers are plain integers. Absolute register address of element i:
instance.base + register.offset + i * register.stride.
"""

from __future__ import annotations

import json

from . import GENERATOR, nvmrows
from .device import DeviceSem, InstanceSem
from .semantics import IpSem, RegSem, ViewSem

SCHEMA = "pp-device/1"


def _field(f) -> dict:
    return {"name": f.name, "mask": f.mask, "shift": f.shift, "width": f.width,
            "caption": f.caption,
            "values": [{"name": v.name, "value": v.value, "caption": v.caption}
                       for v in f.values]}


def _reg(r: RegSem) -> dict:
    return {
        "name": r.name, "offset": r.offset, "size": r.size, "count": r.count,
        "stride": r.stride, "reset": r.reset, "access": r.rw,
        "access_size": r.access_size, "impl": r.impl, "wmask": r.wmask,
        "w1c": r.w1c, "ro": r.ro, "rdz": r.rdz, "enprot": r.enprot,
        "syncbusy": r.syncbusy, "sync_register": r.sync_reg,
        "sync_per_element": r.sync_per_element, "flags": list(r.flags),
        "alias_of": r.alias_of,
        "read_side_effect": "HOOK_PRE_READ" in r.flags,
        "caption": r.caption, "note": r.note,
        "fields": [_field(f) for f in r.fields],
        "provenance": [[p.aspect, p.source, p.detail] for p in r.provenance],
    }


def _view(v: ViewSem) -> dict:
    return {
        "mode": v.mode or None, "value": v.value, "caption": v.caption,
        "irq": ({"flag": v.irq.flag, "enable": v.irq.enable, "source": v.irq.source}
                if v.irq else None),
        "registers": [_reg(r) for r in v.regs],
        "reset_overrides": [{"key": o.key, "offset": o.offset, "size": o.size,
                             "value": o.value} for o in v.overrides],
        "reset_from": [{"register_field": f.key, "fuse": f.fuse,
                        "fuse_addr": f.fuse_addr, "fuse_mask": f.fuse_mask,
                        "reg_offset": f.reg_offset, "reg_size": f.reg_size,
                        "reg_mask": f.reg_mask, "invert": f.invert}
                       for f in v.reset_from],
    }


def _ip(ip: IpSem) -> dict:
    mod = ip.module
    ov = None
    if ip.overlay:
        ov = {"file": f"overlays/{ip.overlay.file}", "sha1": ip.overlay.sha1,
              "status": ip.overlay_status, "versions": ip.overlay.versions}
    sel = None
    if ip.selector:
        s = ip.selector
        sel = {"register": s.register, "field": s.field, "offset": s.offset,
               "shift": s.shift, "width": s.width, "qualifier": s.qualifier}
    return {"module": mod.name, "id": mod.id, "version": mod.version,
            "caption": mod.caption, "core": ip.core, "overlay": ov,
            "size": ip.size, "selector": sel, "default_view": ip.default_view,
            "views": [_view(v) for v in ip.views]}


def _instance(i: InstanceSem) -> dict:
    avr = {"vectors": list(i.vectors)} if i.vectors else {}
    return avr | {
        "name": i.name, "module": i.module, "ip": i.ip.key, "ip_id": i.ip_id,
        "ip_version": i.ip_version, "base": i.base, "slot_size": i.slot_size,
        "bus": i.bus,
        "alias": ({"name": i.alias_name, "base": i.alias_base,
                   "size": i.alias_size} if i.alias_name else None),
        "irq": ({"line": i.irq, "name": i.irq_name, "input": i.irq_input,
                 "shared": "SHARED_IRQ" in i.flags} if i.irq >= 0 else None),
        "pac_id": i.pac_id, "gclk_core": i.gclk_core, "gclk_slow": i.gclk_slow,
        "dmac_rx": i.dmac_rx, "dmac_tx": i.dmac_tx, "flags": list(i.flags),
        "params": dict(i.params), "notes": list(i.notes),
    }


def _avr_core(ds: DeviceSem) -> dict:
    """AVR: the core type and the signature instead of NVIC facts."""
    sig = ds.device.property_groups.get("SIGNATURES", {})
    cpu = next((i for i in ds.device.instances if i.module == "CPU"), None)
    return {"type": "avrxt",
            "core_version": cpu.params.get("CORE_VERSION") if cpu else None,
            "num_vectors": max((l.index for l in ds.irqs), default=0) + 1,
            "signature": [int(sig[f"SIGNATURE{n}"], 0) for n in range(3)],
            "revisions": {n: v for n, v in ds.profile.silicon_revisions}}


def device_json(ds: DeviceSem) -> str:
    dev = ds.device
    p = dev.params
    doc = {
        "schema": SCHEMA,
        "generated_by": GENERATOR,
        "source": {"atdf": dev.atdf_file, "atdf_timestamp": dev.timestamp,
                   "dfp": dev.dfp_name, "dfp_version": dev.dfp_version},
        "device": {"name": dev.name, "architecture": dev.architecture,
                   "family": dev.family, "series": dev.series,
                   "variants": dev.variants},
        "core": _avr_core(ds) if dev.is_avr else {"num_irq": int(p["NUM_IRQ"]),
                 "nvic_prio_bits": int(p["__NVIC_PRIO_BITS"]),
                 "cm0p_rev": int(p["__CM0P_REV"], 0),
                 "mpu_present": p["__MPU_PRESENT"] == "1",
                 "vtor_present": p["__VTOR_PRESENT"] == "1",
                 "dsu_did_signature": int(dev.property_groups.get(
                     "SIGNATURES", {}).get("DSU_DID", "0"), 0),
                 "dsu_did_mask": 0xFFFFF0FF},
        "memory": [{"name": s.name, "base": s.start, "size": s.size,
                    "type": s.type, "rw": s.rw, "exec": s.exec,
                    "address_space": s.address_space}
                   for s in sorted(dev.memory_segments,
                                   key=lambda s: (s.address_space, s.start))],
        "nvm_rows": None if dev.is_avr else {
            "base": nvmrows.BASE, "size": nvmrows.SIZE, "fill": nvmrows.FILL,
            "words": [{"addr": w.addr, "value": w.value, "what": w.what}
                      for w in nvmrows.default_words(dev, ds.profile.nvm_rows)],
            "serial_addrs": list(ds.profile.nvm_rows.serial_addrs),
            "serial_default": list(ds.profile.nvm_rows.serial_default),
        },
        "interrupts": [{"line": l.index, "name": l.name, "caption": l.caption,
                        "instances": l.instances} for l in ds.irqs],
        "exceptions": [{"index": l.index, "name": l.name, "caption": l.caption}
                       for l in ds.exceptions],
        "instances": [_instance(i) for i in ds.instances],
        "ips": {ip.key: _ip(ip) for ip in sorted(ds.ips.values(),
                                                 key=lambda x: x.key)},
        "signals": [{"pad": s.pad, "pin": s.pin, "function": s.function,
                     "pmux": s.pmux, "instance": s.instance, "group": s.group,
                     "index": s.index} for s in ds.signals],
        "gclk_channels": [{"id": r.id, "instance": r.instance, "param": r.param}
                          for r in ds.gclk_users],
        "dmac_triggers": [{"id": r.id, "instance": r.instance, "param": r.param}
                          for r in ds.dmac_triggers],
        "events": {
            "generators": [{"index": e.index, "name": e.name,
                            "instance": e.instance}
                           for e in sorted(dev.event_generators,
                                           key=lambda e: e.index)],
            "users": [{"index": e.index, "name": e.name, "instance": e.instance}
                      for e in sorted(dev.event_users, key=lambda e: e.index)],
        },
    }
    if dev.is_avr:
        del doc["nvm_rows"]
        doc["data_rows"] = [{"addr": a, "bytes": list(b), "what": w}
                            for a, b, w in ds.profile.data_rows]
    return json.dumps(doc, indent=1) + "\n"
