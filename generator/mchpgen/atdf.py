# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Parse a Microchip ATDF (AVR tools device file, schema 4.x) into plain data.

Only reading happens here; no interpretation beyond number parsing. Lists keep
document order so that callers decide how to sort.
"""

from __future__ import annotations

import os
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field


def parse_int(text: str | None) -> int | None:
    """ATDF numbers are '0x..' hex or decimal; returns None for missing."""
    if text is None:
        return None
    return int(text.strip(), 0)


def _modes(attr: str | None) -> tuple[str, ...]:
    return tuple(attr.split()) if attr else ()


@dataclass
class Mode:
    """A <mode> selector: register-group modes or register-level modes."""
    name: str
    qualifier: str
    value: int
    mask: str | None
    caption: str


@dataclass
class Bitfield:
    name: str
    mask: int
    caption: str
    modes: tuple[str, ...]          # register-level modes; () = all
    values: str | None              # value-group name


@dataclass
class Register:
    name: str
    offset: int
    size: int
    count: int
    rw: str
    initval: int | None
    access: str | None              # WSYNC / RWSYNC
    atomic_op: str | None           # e.g. "clear:INTFLAG", "set:DIR"
    access_size: int | None
    modes: tuple[str, ...]          # group modes; () = all modes
    caption: str
    bitfields: list[Bitfield]
    regmodes: list[Mode]

    @property
    def size_mask(self) -> int:
        return (1 << (8 * self.size)) - 1


@dataclass
class SubgroupRef:
    """A nested <register-group> reference, e.g. PORT GROUP x3 @ 0x80."""
    name: str
    name_in_module: str
    offset: int
    size: int
    count: int
    modes: tuple[str, ...]


@dataclass
class RegisterGroup:
    name: str
    caption: str
    size: int | None
    aligned: int | None
    modes: list[Mode]
    registers: list[Register]
    subgroups: list[SubgroupRef]


@dataclass
class Value:
    name: str
    value: int
    caption: str


@dataclass
class Module:
    name: str
    id: str | None
    version: str
    name2: str | None
    caption: str
    groups: dict[str, RegisterGroup]
    value_groups: dict[str, list[Value]]


@dataclass
class InstanceGroup:
    """An instance's <register-group>: where a module group is mapped."""
    name: str
    name_in_module: str
    address_space: str
    base: int
    size: int | None


@dataclass
class Signal:
    group: str
    index: int | None
    function: str
    pad: str


@dataclass
class Instance:
    name: str
    module: str
    module_id: str | None
    module_version: str
    groups: list[InstanceGroup]
    signals: list[Signal]
    params: dict[str, str]

    def param_int(self, name: str) -> int | None:
        text = self.params.get(name)
        return parse_int(text) if text is not None else None


@dataclass
class MemorySegment:
    name: str
    start: int
    size: int
    type: str
    rw: str
    exec: bool
    pagesize: int | None
    address_space: str


@dataclass
class Interrupt:
    index: int
    name: str
    caption: str
    instances: list[str]


@dataclass
class EventPoint:
    """An EVSYS generator or user."""
    name: str
    index: int
    instance: str


@dataclass
class Device:
    name: str
    architecture: str
    family: str
    series: str
    atdf_path: str
    atdf_file: str
    timestamp: str
    schema_version: str
    variants: list[dict[str, str]]
    memory_segments: list[MemorySegment]
    params: dict[str, str]
    instances: list[Instance]
    interrupts: list[Interrupt]
    event_generators: list[EventPoint]
    event_users: list[EventPoint]
    property_groups: dict[str, dict[str, str]]
    modules: dict[str, Module]
    dfp_name: str = ""
    dfp_version: str = ""
    extra: dict[str, str] = field(default_factory=dict)

    def module_of(self, inst: Instance) -> Module:
        return self.modules[inst.module]

    def instance(self, name: str) -> Instance:
        for inst in self.instances:
            if inst.name == name:
                return inst
        raise KeyError(name)


def _parse_mode(el: ET.Element) -> Mode:
    return Mode(name=el.get("name"), qualifier=el.get("qualifier", ""),
                value=parse_int(el.get("value")) or 0, mask=el.get("mask"),
                caption=el.get("caption", ""))


def _parse_register(el: ET.Element) -> Register:
    fields = [Bitfield(name=b.get("name"), mask=parse_int(b.get("mask")),
                       caption=b.get("caption", ""), modes=_modes(b.get("modes")),
                       values=b.get("values"))
              for b in el.findall("bitfield")]
    return Register(name=el.get("name"), offset=parse_int(el.get("offset")),
                    size=parse_int(el.get("size")),
                    count=parse_int(el.get("count")) or 1,
                    rw=el.get("rw", "RW"),
                    initval=parse_int(el.get("initval")),
                    access=el.get("access"), atomic_op=el.get("atomic-op"),
                    access_size=parse_int(el.get("access-size")),
                    modes=_modes(el.get("modes")),
                    caption=el.get("caption", ""), bitfields=fields,
                    regmodes=[_parse_mode(m) for m in el.findall("mode")])


def _parse_group(el: ET.Element) -> RegisterGroup:
    subs = [SubgroupRef(name=s.get("name"),
                        name_in_module=s.get("name-in-module", s.get("name")),
                        offset=parse_int(s.get("offset")) or 0,
                        size=parse_int(s.get("size")),
                        count=parse_int(s.get("count")) or 1,
                        modes=_modes(s.get("modes")))
            for s in el.findall("register-group")]
    return RegisterGroup(name=el.get("name"), caption=el.get("caption", ""),
                         size=parse_int(el.get("size")),
                         aligned=parse_int(el.get("aligned")),
                         modes=[_parse_mode(m) for m in el.findall("mode")],
                         registers=[_parse_register(r)
                                    for r in el.findall("register")],
                         subgroups=subs)


def _parse_module(el: ET.Element) -> Module:
    groups = {}
    for g in el.findall("register-group"):
        groups[g.get("name")] = _parse_group(g)
    values = {}
    for vg in el.findall("value-group"):
        values[vg.get("name")] = [Value(v.get("name"), parse_int(v.get("value")),
                                        v.get("caption", ""))
                                  for v in vg.findall("value")]
    return Module(name=el.get("name"), id=el.get("id"),
                  version=el.get("version", ""), name2=el.get("name2"),
                  caption=el.get("caption", ""), groups=groups,
                  value_groups=values)


def _parse_instance(mod_el: ET.Element, el: ET.Element) -> Instance:
    groups = [InstanceGroup(name=g.get("name"),
                            name_in_module=g.get("name-in-module"),
                            address_space=g.get("address-space", "base"),
                            base=parse_int(g.get("offset")),
                            size=parse_int(g.get("size")))
              for g in el.findall("register-group")]
    signals = [Signal(group=s.get("group"), index=parse_int(s.get("index")),
                      function=s.get("function"), pad=s.get("pad"))
               for s in el.findall("signals/signal")]
    params = {p.get("name"): p.get("value")
              for p in el.findall("parameters/param")}
    return Instance(name=el.get("name"), module=mod_el.get("name"),
                    module_id=mod_el.get("id"),
                    module_version=mod_el.get("version", ""),
                    groups=groups, signals=signals, params=params)


def _parse_events(dev: ET.Element, tag: str, item: str) -> list[EventPoint]:
    return [EventPoint(name=e.get("name"), index=parse_int(e.get("index")),
                       instance=e.get("module-instance", ""))
            for e in dev.findall(f"events/{tag}/{item}")]


def find_dfp(atdf_path: str) -> tuple[str, str]:
    """Return (pack name, version) from the nearest *.pdsc above the ATDF."""
    d = os.path.dirname(os.path.abspath(atdf_path))
    for _ in range(4):
        d = os.path.dirname(d)
        for f in sorted(os.listdir(d)):
            if f.endswith(".pdsc"):
                root = ET.parse(os.path.join(d, f)).getroot()
                name = root.findtext("name", "").strip()
                rel = root.find("releases/release")
                return name, rel.get("version", "") if rel is not None else ""
    return "", ""


def parse(path: str) -> Device:
    root = ET.parse(path).getroot()
    dev = root.find("devices/device")
    segments = []
    for space in dev.findall("address-spaces/address-space"):
        for s in space.findall("memory-segment"):
            segments.append(MemorySegment(
                name=s.get("name"), start=parse_int(s.get("start")),
                size=parse_int(s.get("size")), type=s.get("type", ""),
                rw=s.get("rw", ""), exec=s.get("exec") == "true",
                pagesize=parse_int(s.get("pagesize")),
                address_space=space.get("id")))
    instances = []
    for mod_el in dev.findall("peripherals/module"):
        for inst_el in mod_el.findall("instance"):
            instances.append(_parse_instance(mod_el, inst_el))
    interrupts = [Interrupt(index=parse_int(i.get("index")), name=i.get("name"),
                            caption=i.get("caption", ""),
                            instances=i.get("module-instance", "").split())
                  for i in dev.findall("interrupts/interrupt")]
    props = {}
    for pg in dev.findall("property-groups/property-group"):
        props[pg.get("name")] = {p.get("name"): p.get("value")
                                 for p in pg.findall("property")}
    modules = {}
    for mod_el in root.findall("modules/module"):
        modules[mod_el.get("name")] = _parse_module(mod_el)
    file_el = root.find("file")
    device = Device(
        name=dev.get("name"), architecture=dev.get("architecture", ""),
        family=dev.get("family", ""), series=dev.get("series", ""),
        atdf_path=os.path.abspath(path), atdf_file=os.path.basename(path),
        timestamp=file_el.get("timestamp", "") if file_el is not None else "",
        schema_version=root.get("schema-version", ""),
        variants=[{k: v for k, v in var.attrib.items() if not k.startswith("{")}
                  for var in root.findall("variants/variant")],
        memory_segments=segments,
        params={p.get("name"): p.get("value")
                for p in dev.findall("parameters/param")},
        instances=instances, interrupts=interrupts,
        event_generators=_parse_events(dev, "generators", "generator"),
        event_users=_parse_events(dev, "users", "user"),
        property_groups=props, modules=modules)
    device.dfp_name, device.dfp_version = find_dfp(path)
    return device
