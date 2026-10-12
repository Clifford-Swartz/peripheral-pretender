# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Reset values the ATDF gets wrong or lacks, and user-row loaded resets."""

import pytest

from mchpgen import nvmrows

ERASED = (0xFFFFFFFF, 0xFFFFFFFF)


def apply_reset_from(view, user_row: tuple[int, int]) -> dict[int, int]:
    """Register offset -> reset value after loading fuse fields (as QEMU will)."""
    words = {nvmrows.BASE + 0x4000: user_row[0], nvmrows.BASE + 0x4004: user_row[1]}
    regs = {r.offset: r.reset for r in view.regs}
    for fr in view.reset_from:
        shift = (fr.fuse_mask & -fr.fuse_mask).bit_length() - 1
        width = bin(fr.fuse_mask).count("1")
        v = (words[fr.fuse_addr] & fr.fuse_mask) >> shift
        if fr.invert:
            v ^= (1 << width) - 1
        rshift = (fr.reg_mask & -fr.reg_mask).bit_length() - 1
        regs[fr.reg_offset] = (regs[fr.reg_offset] & ~fr.reg_mask) | (v << rshift)
    return regs


def test_genctrl0_override(ds):
    gclk = ds.ip("GCLK").view("")
    genctrl = gclk.reg("GENCTRL")
    assert genctrl.reset == 0
    assert [(o.offset, o.size, o.value) for o in gclk.overrides] == [(0x20, 4, 0x106)]
    assert genctrl.impl & 0x106 == 0x106


def test_port_pmux15_override(ds):
    port = ds.ip("PORT").view("")
    assert [(o.key, o.offset, o.size, o.value) for o in port.overrides] == \
        [("GROUP[0].PMUX[15]", 0x3F, 1, 0x06)]
    for g in range(3):
        assert port.reg(f"GROUP[{g}].PMUX").reset == 0


def test_nvmctrl(ds, dev):
    nvm = ds.ip("NVMCTRL").view("")
    param = nvm.reg("PARAM")
    assert param.reset == 0x08032000 == nvmrows.nvm_param(dev)
    assert "READ_ONLY" in param.flags and param.wmask == 0
    assert nvm.reg("CTRLB").reset == 0x80
    intflag = nvm.reg("INTFLAG")
    assert intflag.reset == 0x1
    assert intflag.ro & 0x1 and not intflag.w1c & 0x1      # READY survives W1C
    assert intflag.w1c == 0x01000302                        # ERROR SERR DERR FLTCAP
    assert "READ_ONLY" in nvm.reg("LOCK").flags


def test_clock_resets(ds):
    osc = ds.ip("OSCCTRL").view("")
    assert (osc.reg("OSC48MCTRL").reset, osc.reg("OSC48MDIV").reset) == (0x82, 0x0B)
    assert osc.reg("STATUS").reset == 0x10 and "DYNAMIC" in osc.reg("STATUS").flags
    mclk = ds.ip("MCLK").view("")
    assert mclk.reg("INTFLAG").reset == 0x01 and mclk.reg("INTFLAG").w1c == 0x01
    ulp = ds.ip("OSC32KCTRL").view("").reg("OSCULP32K")
    assert ulp.reset == 0x6 and ulp.impl & 0x6 == 0x6


def test_pac_status(ds):
    pac = ds.ip("PAC").view("")
    assert pac.reg("STATUSB").reset == 0x00000002
    statusc = pac.reg("STATUSC")
    assert statusc.reset == 0x09000000
    assert statusc.reset & statusc.impl == statusc.reset   # bit 24 reads back


def test_rcause_and_did(ds):
    assert ds.ip("RSTC").view("").reg("RCAUSE").reset == 0x01
    assert ds.ip("DSU").view("").reg("DID").reset == 0x11060100


@pytest.mark.parametrize("row, ctrla, config, ewctrl", [
    (nvmrows.USER_ROW_PRODUCTION, 0x00, 0xBB, 0x0B),
    (ERASED, 0x86, 0xFF, 0x0F),     # always-on WDT: why the DFP default is wrong
])
def test_wdt_reset_from_user_row(ds, row, ctrla, config, ewctrl):
    regs = apply_reset_from(ds.ip("WDT").view(""), row)
    assert (regs[0x0], regs[0x1], regs[0x2]) == (ctrla, config, ewctrl)


@pytest.mark.parametrize("row, bodvdd", [
    # ENABLE=1, ACTION=1 (reset), LEVEL=8, HYST=0
    (nvmrows.USER_ROW_PRODUCTION, 0x2 | (1 << 3) | (8 << 16)),
    (ERASED, (3 << 3) | (0x3F << 16) | 0x4),   # BODVDD disabled
])
def test_supc_reset_from_user_row(ds, row, bodvdd):
    regs = apply_reset_from(ds.ip("SUPC").view(""), row)
    assert regs[0x10] == bodvdd


def test_nvmctrl_lock_from_user_row(ds):
    regs = apply_reset_from(ds.ip("NVMCTRL").view(""), nvmrows.USER_ROW_PRODUCTION)
    assert regs[0x20] == 0xFFFF


def test_nvm_image(dev):
    img = nvmrows.image(dev)
    assert len(img) == 0x10000
    word = lambda off: int.from_bytes(img[off:off + 4], "little")  # noqa: E731
    assert (word(0x4000), word(0x4004)) == (0xB15088FB, 0xFFFFF8BB)
    calib = int.from_bytes(img[0x6020:0x6028], "little")
    assert (calib >> 19) & 0x3FFFFF == 0x200220           # Harmony CAL48M read
    assert (calib >> 12) & 0x7F == 0x3F                   # OSC32K CALIB
    otp1 = word(0x6000)
    assert (otp1 >> 16) & 0x3FFF == 8192 and (otp1 >> 8) & 0x3 == 3
    assert word(0x6004) & 0xFF == 128
    assert [word(a - nvmrows.BASE) for a in nvmrows.SERIAL_ADDRS] == \
        list(nvmrows.SERIAL_DEFAULT)
    assert img[0x0:0x8] == b"\xff" * 8 and img[0x6030:0x6038] == b"\xff" * 8


def test_user_row_decodes_to_production_settings(dev):
    fields = {b.name: b.mask
              for g in ("USER_WORD_0", "USER_WORD_1")
              for r in dev.modules["FUSES"].groups["USER_FUSES"].registers
              if r.name == g for b in r.bitfields}

    def get(word, name):
        mask = fields[name]
        return (word & mask) >> ((mask & -mask).bit_length() - 1)
    w0, w1 = nvmrows.USER_ROW_PRODUCTION
    assert get(w0, "NVMCTRL_BOOTPROT") == 0xB
    assert get(w0, "BODVDDUSERLEVEL") == 8 and get(w0, "BODVDD_DIS") == 0
    assert get(w0, "BODVDD_ACTION") == 1
    assert get(w0, "WDT_ENABLE") == 0 and get(w0, "WDT_ALWAYSON") == 0
    assert get(w0, "WDT_PER") == 0xB
    assert get(w1, "WDT_WINDOW") == 0xB and get(w1, "WDT_EWOFFSET") == 0xB
    assert get(w1, "WDT_WEN") == 0 and get(w1, "NVMCTRL_REGION_LOCKS") == 0xFFFF
    assert (w0 >> 17) & 0x1FF == 0x0A8                    # BODCORE calibration
