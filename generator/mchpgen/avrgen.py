# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""C headers for 8-bit AVR devices (ATDF architecture AVR8X).

The per-IP constant headers and register tables are the same as for Arm
parts (cgen.ip_header, cgen.ip_tables). The device level differs: no NVIC
or NVM rows, a data and a program address space, several interrupt vectors
per instance, signature bytes. Outputs:

  include/hw/avr/<device>_gen.h   memory segments, instances, vectors
  hw/avr/<device>_device.h        the MchpAvrDeviceInfo the avr8x-soc model
                                  is built from (profile: revisions, rows)
"""

from __future__ import annotations

from . import cgen
from .cgen import c_str, define, file_header, guard, struct_entry, wrap_items
from .device import DeviceSem


def soc_header_path(ds: DeviceSem) -> str:
    return f"include/hw/avr/{ds.symbol}_gen.h"


def device_header_path(ds: DeviceSem) -> str:
    return f"hw/avr/{ds.symbol}_device.h"


def _segments(ds: DeviceSem, space: str):
    return sorted((s for s in ds.device.memory_segments
                   if s.address_space == space),
                  key=lambda s: (s.start, s.name))


def _segment(ds: DeviceSem, space: str, name: str):
    for s in _segments(ds, space):
        if s.name == name:
            return s
    raise ValueError(f"{ds.device.name}: no {space} segment {name}")


def soc_header(ds: DeviceSem) -> str:
    dev = ds.device
    sym, SYM = ds.symbol, ds.symbol.upper()
    path = soc_header_path(ds)
    g = guard(path[len("include/"):])
    out = file_header(f"{dev.name} SoC description: memory map, instances, "
                      f"interrupt vectors", ds)
    out += [f"#ifndef {g}", f"#define {g}", "",
            '#include "hw/mchp/mchp_device.h"']
    for ip in sorted(ds.ips.values(), key=lambda x: x.key):
        if cgen.wants_ip_header(ip) and ip.views:
            out.append(f'#include "{cgen.ip_tables_path(ds, ip)[len("include/"):]}"')
    sig = dev.property_groups.get("SIGNATURES", {})
    out += ["", "/* Device signature (ATDF property group SIGNATURES) */"]
    for n in range(3):
        out.append(define(f"{SYM}_SIGNATURE{n}", sig[f"SIGNATURE{n}"]))
    for space in ("data", "prog"):
        segs = _segments(ds, space)
        out += ["", f"/* Memory segments, {space} address space */"]
        for s in segs:
            out += [define(f"{SYM}_{s.name}_BASE", f"0x{s.start:04X}"),
                    define(f"{SYM}_{s.name}_SIZE", f"0x{s.size:X}")]
            if s.pagesize:
                out.append(define(f"{SYM}_{s.name}_PAGE_SIZE",
                                  f"0x{s.pagesize:X}"))
        out += ["", f"static const MchpMemSegment {sym}_{space}_segments[] = {{"]
        for s in segs:
            out += struct_entry([f".name = {c_str(s.name)}",
                                 f".base = 0x{s.start:04X}",
                                 f".size = 0x{s.size:X}",
                                 f".type = {c_str(s.type)}",
                                 f".writable = {'true' if 'W' in s.rw else 'false'}",
                                 f".exec = {'true' if s.exec else 'false'}"])
        out += ["};", define(f"{SYM}_NUM_{space.upper()}_SEGMENTS",
                             str(len(segs)))]
    out += ["", "/*",
            " * Peripheral instances, by data address. CORE: the CPU's own",
            " * registers (SP, SREG; CCP is the SoC's). .irq is the lowest",
            " * vector of the instance; every vector is in the table below.",
            " */"]
    for n, i in enumerate(ds.instances):
        out.append(define(f"{SYM}_INST_{i.name.upper()}", str(n)))
    out += ["", f"static const MchpInstanceInfo {sym}_instances[] = {{"]
    for i in ds.instances:
        out += cgen._inst_entry(ds, i)
    out += ["};", define(f"{SYM}_NUM_INSTANCES", str(len(ds.instances))), ""]
    out += ["/*", " * Interrupt vectors (0 is reset). A vector with several "
            "ATDF names (TCA0", " * LUNF/OVF) is named after the first.", " */"]
    for line in ds.irqs:
        out.append(define(f"{SYM}_VECT_{line.name.upper()}", str(line.index)))
    out += ["", f"static const MchpAvrVector {sym}_vectors[] = {{"]
    for line in ds.irqs:
        inst = c_str(line.instances[0]) if line.instances else "NULL"
        out += struct_entry([f".name = {c_str(line.name)}",
                             f".instance = {inst}", f".vector = {line.index}"])
    last = max((l.index for l in ds.irqs), default=0)
    out += ["};", define(f"{SYM}_NUM_VECTORS", str(len(ds.irqs))),
            define(f"{SYM}_LAST_VECTOR", str(last)), "",
            f"#endif /* {g} */"]
    return "\n".join(out) + "\n"


def device_header(ds: DeviceSem) -> str:
    """The MchpAvrDeviceInfo the generic avr8x-soc model is built from."""
    dev = ds.device
    sym, SYM = ds.symbol, ds.symbol.upper()
    g = guard(device_header_path(ds))
    prof = ds.profile
    sig = dev.property_groups.get("SIGNATURES", {})
    flash = _segment(ds, "prog", "PROGMEM")
    out = file_header(f"{dev.name} device descriptor", ds,
                      [f"Profile: profiles/{prof.file} "
                       f"(sha1 {prof.sha1[:12]})"])
    out += [f"#ifndef {g}", f"#define {g}", "",
            f'#include "{soc_header_path(ds)[len("include/"):]}"', "",
            "/* Silicon revisions (SYSCFG.REVID); the first is the default */",
            f"static const MchpSiliconRev {sym}_revisions[] = {{"]
    for name, value in prof.silicon_revisions:
        out += struct_entry([f".name = {c_str(name)}",
                             f".revision = 0x{value:02X}"])
    out += ["};", ""]
    out += [f"/* Data-space bytes that are not erased (profiles/{prof.file}) */"]
    for n, (addr, data, what) in enumerate(prof.data_rows):
        out += [f"static const uint8_t {sym}_data_row{n}[] = {{"]
        out += wrap_items([f"0x{b:02X}" for b in data], "    ")
        out += ["};"]
    out += [f"static const MchpAvrDataRow {sym}_data_rows[] = {{"]
    for n, (addr, data, what) in enumerate(prof.data_rows):
        out += [f"    /* {what} */"] if what else []
        out += struct_entry([f".addr = 0x{addr:04X}",
                             f".bytes = {sym}_data_row{n}",
                             f".len = {len(data)}"])
    if not prof.data_rows:
        out += ["    { 0 },"]
    out += ["};", "", f"static const MchpAvrDeviceInfo {sym}_device = {{"]
    items = [
        f".name = {c_str(dev.name)}",
        ".signature = { " + ", ".join(
            f"0x{int(sig[f'SIGNATURE{n}'], 0):02X}" for n in range(3)) + " }",
        f".flash_size = {SYM}_{flash.name}_SIZE",
        f".flash_page_size = {SYM}_{flash.name}_PAGE_SIZE",
        f".io_size = {SYM}_IO_SIZE",
        f".sram_base = {SYM}_INTERNAL_SRAM_BASE",
        f".sram_size = {SYM}_INTERNAL_SRAM_SIZE",
        f".eeprom_base = {SYM}_EEPROM_BASE",
        f".eeprom_size = {SYM}_EEPROM_SIZE",
        f".progmem_map_base = {SYM}_MAPPED_PROGMEM_BASE",
        f".progmem_map_size = {SYM}_MAPPED_PROGMEM_SIZE",
        f".data_segments = {sym}_data_segments",
        f".ndata_segments = {SYM}_NUM_DATA_SEGMENTS",
        f".instances = {sym}_instances",
        f".ninstances = {SYM}_NUM_INSTANCES",
        f".vectors = {sym}_vectors", f".nvectors = {SYM}_NUM_VECTORS",
        f".last_vector = {SYM}_LAST_VECTOR",
        f".revisions = {sym}_revisions",
        f".nrevisions = ARRAY_SIZE({sym}_revisions)",
        f".data_rows = {sym}_data_rows",
        f".ndata_rows = {len(prof.data_rows)}",
    ]
    out += [f"    {item}," for item in items]
    out += ["};", "", f"#endif /* {g} */"]
    return "\n".join(out) + "\n"
