# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Device-level view: instances with slots, aliases, IRQ sharing and ids.

Everything the SoC needs beyond register tables: where each instance lives,
how big its MMIO slot is, which NVIC line (and which OR-gate input) it drives,
its PAC/GCLK/DMAC ids, and the pad/signal table.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from . import atdf, semantics
from . import profile as profile_mod
from .profile import Profile
from .semantics import Diag, IpSem

APB_SLOT = 0x400
PAD = re.compile(r"^P([A-Z])(\d+)$")

INST_FLAGS = ("CORE", "FUSES", "NO_REGS", "SHARED_IRQ")


@dataclass
class InstanceSem:
    name: str
    module: str
    ip_id: str
    ip_version: str
    base: int
    slot_size: int
    bus: str
    alias_name: str | None
    alias_base: int
    alias_size: int
    irq: int
    irq_name: str | None
    irq_input: int
    vectors: list[int]              # AVR: every vector of the instance
    pac_id: int
    gclk_core: int
    gclk_slow: int
    dmac_rx: int
    dmac_tx: int
    flags: list[str]
    ip: IpSem
    params: dict[str, str]
    notes: list[str] = field(default_factory=list)
    # NVIC lines of the instance, consecutive from irq (SAM D5x/E5x: SERCOM
    # has four, DMAC five, EIC sixteen); 1 on parts with one line each
    nirqs: int = 1


@dataclass
class IrqLine:
    index: int
    name: str
    caption: str
    instances: list[str]


@dataclass
class SignalSem:
    instance: str
    group: str
    index: int
    function: str
    pmux: int
    pad: str
    pin: int


@dataclass
class PeriphRef:
    id: int
    instance: str
    param: str


@dataclass
class DeviceSem:
    device: atdf.Device
    ips: dict[str, IpSem]               # by module name
    instances: list[InstanceSem]
    irqs: list[IrqLine]                 # NVIC lines (index >= 0)
    exceptions: list[IrqLine]           # core exceptions (index < 0)
    signals: list[SignalSem]
    gclk_users: list[PeriphRef]
    dmac_triggers: list[PeriphRef]
    diags: list[Diag]
    profile: Profile | None = None

    @property
    def symbol(self) -> str:
        return self.device.name.lower()

    def instance(self, name: str) -> InstanceSem:
        return next(i for i in self.instances if i.name == name)

    def ip(self, module: str) -> IpSem:
        return self.ips[module]


def _pow2_at_least(n: int) -> int:
    p = 1
    while p < n:
        p <<= 1
    return p


def _segment_of(device: atdf.Device, addr: int) -> atdf.MemorySegment | None:
    for s in device.memory_segments:
        if (s.address_space in atdf.MMIO_SPACES and
                s.start <= addr < s.start + s.size):
            return s
    return None


def _pin(pad: str) -> int:
    m = PAD.match(pad)
    return (ord(m.group(1)) - ord("A")) * 32 + int(m.group(2)) if m else -1


def _pmux(function: str) -> int:
    if len(function) == 1 and "A" <= function <= "Z":
        return ord(function) - ord("A")
    return -1


def _slot(device: atdf.Device, ip: IpSem, base: int, notes: list[str]) -> int:
    if device.is_avr:
        # AVR I/O modules sit back to back; map just their registers
        return ip.size
    size = APB_SLOT
    if ip.size > APB_SLOT:
        size = _pow2_at_least(ip.size)
        notes.append(f"slot 0x{size:X}: registers extend to 0x{ip.size:X}")
    seg = _segment_of(device, base)
    if seg and seg.type == "io" and seg.start + seg.size - base < size:
        size = seg.start + seg.size - base
        notes.append(f"slot 0x{size:X}: limited by memory segment {seg.name}")
    return size


def _alias(inst: atdf.Instance, main: atdf.InstanceGroup, ip: IpSem,
           slot: int) -> tuple[str | None, int, int]:
    """Second register-group of the same layout (IOBUS, DSU_EXT)."""
    others = [g for g in inst.groups if g is not main
              and g.name_in_module == main.name_in_module]
    if not others:
        return None, 0, 0
    g = others[0]
    size = g.size if g.size else min(_pow2_at_least(max(ip.size, 4)), slot)
    return g.name, g.base, size


def _param(inst: atdf.Instance, *names: str) -> int:
    for n in names:
        v = inst.param_int(n)
        if v is not None:
            return v
    return -1


def _build_instance(device: atdf.Device, inst: atdf.Instance, ip: IpSem,
                    irq_of: dict[str, tuple[IrqLine, int]]) -> InstanceSem:
    notes: list[str] = []
    flags = []
    mmio = [g for g in inst.groups if g.address_space in atdf.MMIO_SPACES]
    if ip.core or (device.is_avr and inst.module == "CPU"):
        # AVR: SP and SREG belong to the CPU model, CCP to the SoC
        flags.append("CORE")
    if not mmio:
        flags.append("FUSES")
    if not ip.views and mmio:
        flags.append("NO_REGS")
    main = mmio[0] if mmio else inst.groups[0]
    slot = 0 if ("CORE" in flags or "FUSES" in flags) else _slot(device, ip,
                                                               main.base, notes)
    alias_name, alias_base, alias_size = _alias(inst, main, ip, slot)
    seg = _segment_of(device, main.base)
    irq, irq_input, irq_name = -1, 0, None
    vectors = []
    if inst.name in irq_of:
        line, irq_input = irq_of[inst.name]
        irq, irq_name = line.index, line.name
        if len(line.instances) > 1:
            flags.append("SHARED_IRQ")
    if device.is_avr:
        vectors = sorted({it.index for it in device.interrupts
                          if inst.name in it.instances})
    if inst.module_version != ip.module.version:
        notes.append(f"instance version {inst.module_version} differs from "
                     f"module version {ip.module.version}")
    return InstanceSem(
        name=inst.name, module=inst.module, ip_id=inst.module_id or "",
        ip_version=inst.module_version, base=main.base, slot_size=slot,
        bus=seg.name if seg else main.address_space, alias_name=alias_name,
        alias_base=alias_base, alias_size=alias_size, irq=irq,
        irq_name=irq_name, irq_input=irq_input, vectors=vectors,
        pac_id=_param(inst, "INSTANCE_ID"),
        gclk_core=_param(inst, "GCLK_ID_CORE", "GCLK_ID"),
        gclk_slow=_param(inst, "GCLK_ID_SLOW"),
        dmac_rx=_param(inst, "DMAC_ID_RX"), dmac_tx=_param(inst, "DMAC_ID_TX"),
        flags=[f for f in INST_FLAGS if f in flags], ip=ip,
        params=dict(inst.params), notes=notes)


def _check_overlaps(instances: list[InstanceSem]):
    mapped = sorted((i for i in instances if i.slot_size),
                    key=lambda i: i.base)
    for a, b in zip(mapped, mapped[1:]):
        if a.base + a.slot_size > b.base:
            raise ValueError(f"slot of {a.name} (0x{a.base:X}+0x{a.slot_size:X}) "
                             f"overlaps {b.name} at 0x{b.base:X}")


def _irq_map(device: atdf.Device) -> tuple[list[IrqLine], list[IrqLine],
                                           dict[str, tuple[IrqLine, int]],
                                           dict[str, int]]:
    lines, exceptions, irq_of = [], [], {}
    if device.is_avr:
        return _avr_vectors(device) + ({},)
    nirqs: dict[str, int] = {}
    # In line order, so that an instance's first line is its irq
    for it in sorted(device.interrupts, key=lambda i: i.index):
        line = IrqLine(it.index, it.name, it.caption, list(it.instances))
        (lines if it.index >= 0 else exceptions).append(line)
        if it.index < 0:
            continue
        for pos, name in enumerate(it.instances):
            if name in irq_of:
                first, _ = irq_of[name]
                more = nirqs.get(name, 1)
                # SAM D5x/E5x: an instance with several lines of its own,
                # consecutive (SERCOMn_0..3); a shared line among them is
                # not supported
                if (len(it.instances) > 1 or len(first.instances) > 1 or
                        it.index != first.index + more):
                    raise ValueError(f"{name} drives IRQ {first.index} and "
                                     f"{it.index}: several lines per instance "
                                     "must be its own and consecutive")
                nirqs[name] = more + 1
                continue
            irq_of[name] = (line, pos)
    lines.sort(key=lambda l: l.index)
    exceptions.sort(key=lambda l: l.index)
    return lines, exceptions, irq_of, nirqs


def _avr_vectors(device: atdf.Device) -> tuple[list[IrqLine], list[IrqLine],
                                                dict[str, tuple[IrqLine, int]]]:
    """AVR interrupt vectors: an instance has several (USART RXC, DRE,
    TXC) and a vector may have several names (TCA0 LUNF/OVF, one per timer
    mode); each vector becomes one line named after its first entry. The
    instance's IRQ is its lowest vector."""
    by_index: dict[int, IrqLine] = {}
    for it in device.interrupts:
        name = "_".join(it.instances + [it.name]) if it.instances else it.name
        line = by_index.get(it.index)
        if line is None:
            by_index[it.index] = IrqLine(it.index, name,
                                         it.caption or name, list(it.instances))
        else:
            line.caption += f", {name}"
            line.instances += [i for i in it.instances
                               if i not in line.instances]
    lines = sorted(by_index.values(), key=lambda l: l.index)
    irq_of: dict[str, tuple[IrqLine, int]] = {}
    for line in lines:
        for name in line.instances:
            irq_of.setdefault(name, (line, 0))
    return lines, [], irq_of


def _refs(device: atdf.Device, prefix: str) -> list[PeriphRef]:
    out = []
    for inst in device.instances:
        for name, value in inst.params.items():
            if name.startswith(prefix):
                out.append(PeriphRef(int(value, 0), inst.name, name))
    return sorted(out, key=lambda r: (r.id, r.instance, r.param))


def build(device: atdf.Device, overlays, prof: Profile | None = None
          ) -> DeviceSem:
    if prof is not None:
        if (prof.nvm_rows is None) != device.is_avr:
            raise ValueError(f"profile {prof.file}: nvm_rows is "
                             + ("for Arm parts only" if device.is_avr
                                else "required"))
        if not device.is_avr and any(v > 15 for _, v in
                                     prof.silicon_revisions):
            raise ValueError(f"profile {prof.file}: DSU.DID.REVISION values "
                             "are 0-15")
        if prof.device != device.name:
            raise ValueError(f"profile {prof.file} is for {prof.device}, not "
                             f"{device.name}")
        overlays = profile_mod.apply(
            overlays, prof, {n: m.id or "" for n, m in device.modules.items()})
    ips = {name: semantics.build_ip(device, mod, overlays)
           for name, mod in sorted(device.modules.items())}
    diags = [d for ip in ips.values() for d in ip.diags]
    for (ip_name, ip_id), ov in sorted(overlays.items()):
        mod = device.modules.get(ip_name)
        if mod is None or mod.id != ip_id:
            diags.append(Diag("info", ov.file,
                              f"no {ip_name} {ip_id} module in this ATDF"))
    lines, exceptions, irq_of, nirqs = _irq_map(device)
    # An instance with no register group at all (SAM E51 PUKCC: a ROM
    # library's coprocessor) has nothing to map
    bare = {i.name for i in device.instances if not i.groups}
    for name in sorted(bare):
        diags.append(Diag("info", device.name, f"{name}: no register group "
                          "in the ATDF, not mapped"))
    instances = [_build_instance(device, inst, ips[inst.module], irq_of)
                 for inst in device.instances if inst.name not in bare]
    for i in instances:
        i.nirqs = nirqs.get(i.name, 1)
    known = {i.name for i in instances} | bare
    for line in lines:
        for name in line.instances:
            if name not in known:
                raise ValueError(f"IRQ {line.index} names unknown instance {name}")
    instances.sort(key=lambda i: ("FUSES" in i.flags, "CORE" in i.flags,
                                  i.base, i.name))
    _check_overlaps(instances)
    signals = sorted((SignalSem(inst.name, s.group,
                                s.index if s.index is not None else -1,
                                s.function, _pmux(s.function), s.pad, _pin(s.pad))
                      for inst in device.instances for s in inst.signals),
                     key=lambda s: (s.pin, s.pad, s.pmux, s.function, s.instance,
                                    s.group, s.index))
    return DeviceSem(device=device, ips=ips, instances=instances, irqs=lines,
                     exceptions=exceptions, signals=signals,
                     gclk_users=_refs(device, "GCLK_ID"),
                     dmac_triggers=_refs(device, "DMAC_ID_"), diags=diags,
                     profile=prof)
