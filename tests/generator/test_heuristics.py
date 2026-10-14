# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Spec 5.2 heuristics on IPs without an overlay (TC, MTB, DMAC, ICM).

TC has an overlay now; its heuristic semantics are rebuilt without one.
"""

import pytest

from mchpgen import semantics


@pytest.fixture(scope="module")
def tc(ds):
    ip = semantics.build_ip(ds.device, ds.device.modules["TC"], {})
    assert ip.overlay is None and ip.overlay_status == "none"
    return ip


def prov(reg, aspect):
    return [(p.source, p.detail) for p in reg.provenance if p.aspect == aspect]


def test_syncbusy_dynamic_read_only(tc):
    sb = tc.view("COUNT16").reg("SYNCBUSY")
    assert sb.flags == ["DYNAMIC", "READ_ONLY"] and sb.wmask == 0 and sb.rdz == 0
    assert prov(sb, "dynamic")[0][0] == "heuristic"


def test_swrst_self_clearing(tc):
    ctrla = tc.view("COUNT8").reg("CTRLA")
    assert ctrla.rdz == 0x1
    assert ("heuristic", "SWRST self-clearing") in prov(ctrla, "rdz")


def test_ctrlb_set_clear_pair_and_cmd(tc):
    view = tc.view("COUNT32")
    clr, setr = view.reg("CTRLBCLR"), view.reg("CTRLBSET")
    assert view.regs[clr.alias_index] is setr and view.regs[setr.alias_index] is setr
    assert (clr.flags, setr.flags) == (["ALIAS_CLEAR"], ["ALIAS_SET"])
    assert clr.rdz == setr.rdz == 0xE0                    # CMD
    assert clr.w1c == 0                                   # pair, not W1C


def test_intflag_w1c_and_inten_alias(tc):
    view = tc.view("COUNT16")
    intflag = view.reg("INTFLAG")
    assert intflag.w1c == intflag.impl == 0x33 and intflag.wmask == 0
    assert prov(intflag, "w1c")[0][0] == "heuristic"
    assert view.regs[view.reg("INTENCLR").alias_index].name == "INTENSET"
    assert view.irq.flag == "INTFLAG" and view.irq.source == "heuristic"


def test_sync_by_name(tc):
    view = tc.view("COUNT16")
    sb = {f.name: f.mask for f in view.reg("SYNCBUSY").fields}
    assert view.reg("CTRLA").syncbusy == sb["SWRST"] | sb["ENABLE"]
    assert view.reg("COUNT").syncbusy == sb["COUNT"]
    cc = view.reg("CC")
    assert (cc.syncbusy, cc.sync_per_element) == (sb["CC0"], True)
    assert view.reg("CTRLBSET").syncbusy == sb["CTRLB"]


def test_atdf_resets_kept(tc):
    assert tc.view("COUNT8").reg("PER").reset == 0xFF
    assert tc.view("COUNT16").reg("STATUS").reset == 0x01


def test_access_heuristics(ds):
    mtb = ds.ip("MTB").view("")
    base = mtb.reg("BASE")
    assert base.flags == ["READ_ONLY"] and base.impl == 0xFFFFFFFF
    assert ("heuristic", "no ATDF initval: 0") in prov(base, "reset")
    claim = mtb.reg("CLAIMCLR")
    assert mtb.regs[claim.alias_index].name == "CLAIMSET"
    icm = ds.ip("ICM").view("")
    ier = icm.reg("IER")
    assert {"WRITE_ONLY", "ALIAS_SET"} <= set(ier.flags)
    assert icm.regs[ier.alias_index].name == "IMR"
    dmac = ds.ip("DMAC").view("")
    assert dmac.regs[dmac.reg("CHINTENCLR").alias_index].name == "CHINTENSET"
    assert dmac.reg("CHINTFLAG").w1c == dmac.reg("CHINTFLAG").impl


def test_report_lists_heuristics(generated):
    text = (generated / "report.md").read_text(encoding="utf-8")
    heur = text[text.index("## Heuristics applied"):
                text.index("## Missing ATDF initvals")]
    assert "**dynamic: register named *SYNCBUSY" in heur
    assert "rtc_03608/MODE0 SYNCBUSY" in heur
    assert "**rdz: SWRST self-clearing**" in heur
    assert "**alias: <X>SET/<X>CLR pair, storage <X>SET**" in heur
    missing = text[text.index("## Missing ATDF initvals"):
                   text.index("## Reset values set by overlays")]
    assert "mtb_u2002 BASE" in missing
