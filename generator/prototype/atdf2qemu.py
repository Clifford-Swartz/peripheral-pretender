#!/usr/bin/env python3
"""Prototype: ATDF register-group (+ behavior overlay) -> C descriptor table for a QEMU device.

usage: py -I atdf2qemu.py <device.atdf> <MODULE> <MODE|-> <overlay.yaml|-> [out.h]
"""
import hashlib, os, sys
import xml.etree.ElementTree as ET
import yaml

def ival(s):
    return int(s, 0) if s else 0

# Shared runtime type + generic mask logic (would live in one common header).
COMMON = r"""#ifndef MCHP_REGINFO_DEFINED
#define MCHP_REGINFO_DEFINED
enum { MCHP_F_PAC = 1, MCHP_F_DYNAMIC = 2, MCHP_F_WO = 4, MCHP_F_ALIAS_SET = 8,
       MCHP_F_ALIAS_CLEAR = 16, MCHP_F_HOOK_PRE_READ = 32, MCHP_F_HOOK_POST_WRITE = 64,
       MCHP_F_BUSY_BUSERR = 128 };
typedef struct MchpRegInfo {
    const char *name;
    uint16_t offset;
    uint8_t  size;      /* bytes: 1, 2 or 4 */
    uint32_t reset;     /* ATDF initval */
    uint32_t impl;      /* union of ATDF bitfield masks; other bits read 0 */
    uint32_t wmask;     /* plain R/W bits (ATDF rw minus overlay ro/w1c) */
    uint32_t w1c;       /* write-1-to-clear (overlay; ATDF atomic-op as hint) */
    uint32_t rdz;       /* strobe/self-clearing bits: always read as 0 */
    uint32_t enprot;    /* writes ignored while CTRLA.ENABLE=1 (overlay) */
    uint32_t syncbusy;  /* SYNCBUSY bits raised by a write (overlay) */
    uint32_t flags;
} MchpRegInfo;

/* Store a write of `val` (already lane-merged) to `ri`. Returns false for a
 * "Peripheral Access Error" (DS60001632H 12.5.2.4): changing enable-protected
 * bits while enabled; caller then sets PAC.INTFLAGn and returns MEMTX_ERROR. */
static inline bool mchp_reg_apply_write(const MchpRegInfo *ri, uint32_t *reg,
                                        uint32_t val, bool enabled)
{
    uint32_t old = *reg;
    uint32_t w = ri->wmask & ~(enabled ? ri->enprot : 0);
    *reg = ((old & ~w) | (val & w)) & ~(val & ri->w1c);
    return !(enabled && ((val ^ old) & ri->enprot));
}
#endif"""

def load_module(atdf_path, modname):
    root = ET.parse(atdf_path).getroot()
    mod = next(m for m in root.find("modules") if m.get("name") == modname)
    dev = root.find("devices/device")
    insts = [(i.get("name"), rg.get("offset"))
             for pm in dev.find("peripherals") if pm.get("name") == modname
             for i in pm.findall("instance") for rg in i.findall("register-group")]
    return root, dev, mod, insts

def collect_regs(mod, modname, mode):
    """Registers of the module's main group that apply to `mode` (ATDF 'modes' attr)."""
    rg = next(g for g in mod.findall("register-group") if g.get("name") == modname)
    regs = []
    for r in rg.findall("register"):
        ms = r.get("modes")
        if mode and ms and mode not in ms.split():
            continue
        fields = {}
        for b in r.findall("bitfield"):           # BAUD has per-FORM variants: union
            fields[b.get("name")] = fields.get(b.get("name"), 0) | ival(b.get("mask"))
        cnt = ival(r.get("count")) or 1
        for i in range(cnt):
            regs.append(dict(
                name=r.get("name") + (str(i) if cnt > 1 else ""),
                offset=ival(r.get("offset")) + i * ival(r.get("size")),
                size=ival(r.get("size")), rw=r.get("rw"),
                reset=ival(r.get("initval")), has_reset=r.get("initval") is not None,
                access=r.get("access"), atomic=r.get("atomic-op"), fields=fields))
    return regs

def fmask(reg, names, what):
    m = 0
    for n in names:
        if n not in reg["fields"]:
            sys.exit(f"overlay error: {reg['name']}.{n} ({what}) not in ATDF")
        m |= reg["fields"][n]
    return m

def atdf_atomic(r, names):
    """Interpret ATDF atomic-op: 'clear:INTFLAG' on INTFLAG = W1C; 'set:DIR' on DIRSET =
    alias onto DIR; 'clear:INTENCLR' on INTENCLR = alias pair (same syntax as W1C!)."""
    if not r["atomic"]:
        return None, None
    op, tgt = r["atomic"].split(":")
    if tgt != r["name"]:
        return "alias", (op, tgt)
    base = r["name"][:-3]
    if r["name"][-3:] in ("SET", "CLR") and {base + "SET", base + "CLR"} <= names:
        return "alias", (op, base)
    return ("w1c", None) if op == "clear" else (None, None)

def merge(regs, ov, warn):
    names = {r["name"] for r in regs}
    for r in regs:
        o = (ov or {}).get(r["name"], {})
        impl = 0
        for m in r["fields"].values():
            impl |= m
        r["impl"] = impl
        r["wmask"] = impl if "W" in (r["rw"] or "") else 0
        # ATDF-derived hints (atomic-op / access) -- used only as defaults
        kind, arg = atdf_atomic(r, names)
        r["w1c"] = impl if kind == "w1c" else 0
        if kind == "alias" and "alias" not in o:
            o = dict(o, alias={"storage": arg[1], "op": arg[0]})
            warn.append(f"{r['name']}: alias {arg[0]}->{arg[1]} inferred from ATDF atomic-op={r['atomic']}")
        if r["access"] in ("WSYNC", "RWSYNC") and "sync" not in o:
            warn.append(f"{r['name']}: ATDF access={r['access']} but no SYNCBUSY mapping in overlay")
        if "w1c" in o:
            new = fmask(r, o["w1c"], "w1c")
            if r["w1c"] and r["w1c"] != new:
                warn.append(f"{r['name']}: ATDF atomic-op={r['atomic']} implies w1c=0x{r['w1c']:X}, overlay says 0x{new:X}")
            if not r["w1c"]:
                warn.append(f"{r['name']}: W1C bits 0x{new:X} come only from overlay (ATDF has no atomic-op)")
            r["w1c"] = new
        ro = fmask(r, o.get("ro", []), "ro")
        r["wmask"] &= ~(ro | r["w1c"])
        ep = o.get("enable_protected")
        if ep == "all":
            r["enprot"] = r["wmask"]
        elif isinstance(ep, dict):
            r["enprot"] = r["wmask"] & ~fmask(r, ep["all_except"], "enable_protected")
        else:
            r["enprot"] = 0
        r["sync"] = {f: b for f, b in (o.get("sync") or {}).items()}
        fmask(r, r["sync"].keys(), "sync")
        # read-as-zero: strobes, self-clearing bits (zero-latency model), write-only regs
        r["rdz"] = fmask(r, o.get("strobe", []) + o.get("self_clearing", []), "strobe")
        if r["rw"] == "W":
            r["rdz"] = impl
        flags = []
        if o.get("pac_protected"): flags.append("MCHP_F_PAC")
        if o.get("dynamic"): flags.append("MCHP_F_DYNAMIC")
        if r["rw"] == "W": flags.append("MCHP_F_WO")
        al = o.get("alias")
        if al: flags.append("MCHP_F_ALIAS_" + al["op"].upper())
        for h in o.get("hooks", []): flags.append("MCHP_F_HOOK_" + h.upper())
        if o.get("write_while_syncbusy") == "bus_error": flags.append("MCHP_F_BUSY_BUSERR")
        r["flags"] = " | ".join(flags) or "0"
        r["alias"] = al["storage"] if al else None
    return regs

def emit(path_atdf, dev, mod, mode, insts, regs, ovpath, warn, syncbusy):
    pfx = f"{mod.get('name').lower()}_{(mode or 'regs').lower()}"
    P = pfx.upper()
    sha = hashlib.sha1(open(ovpath, "rb").read()).hexdigest()[:12] if ovpath != "-" else "none"
    out = [f"/* GENERATED by atdf2qemu.py -- do not edit.",
           f" * ATDF: {os.path.basename(path_atdf)} (device {dev.get('name')})",
           f" * IP:   {mod.get('name')} id={mod.get('id')} version={mod.get('version')} mode={mode}",
           f" * Overlay: {os.path.basename(ovpath)} sha1={sha}",
           f" * Instances: " + ", ".join(f"{n}@{o}" for n, o in insts) + " */", "",
           COMMON, ""]
    for r in regs:
        out.append(f"REG{r['size']*8}({P}_{r['name']}, 0x{r['offset']:02X})")
        for f, m in r["fields"].items():
            sh = (m & -m).bit_length() - 1
            out.append(f"    FIELD({P}_{r['name']}, {f}, {sh}, {bin(m >> sh).count('1')})")
    out += ["", f"static const MchpRegInfo {pfx}_regs[] = {{",
            "    /* name       off sz reset       impl        wmask       w1c         rdz         enprot      syncbusy    flags */"]
    for r in regs:
        sb = 0
        for f, bit in r["sync"].items():
            sb |= syncbusy[bit]
        out.append(f"    {{ \"{r['name']}\",{' '*(9-len(r['name']))}0x{r['offset']:02X}, {r['size']}, "
                   f"0x{r['reset']:08X}, 0x{r['impl']:08X}, 0x{r['wmask']:08X}, 0x{r['w1c']:08X}, "
                   f"0x{r['rdz']:08X}, 0x{r['enprot']:08X}, 0x{sb:08X}, {r['flags']} }},")
    out += ["};", "",
            f"static void {pfx}_reset(uint8_t *regs) /* little-endian register file */",
            "{",
            f"    for (size_t i = 0; i < ARRAY_SIZE({pfx}_regs); i++) {{",
            f"        const MchpRegInfo *ri = &{pfx}_regs[i];",
            "        stn_le_p(regs + ri->offset, ri->size, ri->reset);",
            "    }", "}"]
    return out

def main():
    atdf, modname, mode, ovpath = sys.argv[1:5]
    mode = None if mode == "-" else mode
    root, dev, mod, insts = load_module(atdf, modname)
    ov_all = yaml.safe_load(open(ovpath)) if ovpath != "-" else None
    if ov_all and (ov_all["id"] != mod.get("id") or mod.get("version") not in ov_all["versions"]):
        sys.exit(f"overlay {ov_all['id']}/{ov_all['versions']} does not match ATDF {mod.get('id')}/{mod.get('version')}")
    ov = ov_all["modes"][mode]["registers"] if ov_all else {}
    warn = []
    regs = merge(collect_regs(mod, modname, mode), ov, warn)
    unknown = set(ov) - {r["name"] for r in regs}
    if unknown:
        sys.exit(f"overlay names registers absent from ATDF: {sorted(unknown)}")
    syncbusy = next(r for r in regs if r["name"] == "SYNCBUSY")["fields"]
    lines = emit(atdf, dev, mod, mode, insts, regs, ovpath, warn, syncbusy)
    text = "\n".join(lines) + "\n"
    if len(sys.argv) > 5:
        open(sys.argv[5], "w").write(text)
    print(text)
    print("/* --- generator report (ATDF vs overlay) ---")
    for w in warn:
        print(" * " + w)
    print(" */")

if __name__ == "__main__":
    main()
