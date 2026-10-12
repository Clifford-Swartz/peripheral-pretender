# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Default image of the NVM rows region 0x00800000-0x0080FFFF (spec 4.3).

Words that the ATDF can describe are computed from its FUSES fields and
NVMCTRL parameters; the rest come from DS60001632H tables 10-3/10-4. Values
that only silicon can provide (calibration, serial number) are plausible
placeholders and must be dumped from a real part before being trusted.
"""

from __future__ import annotations

from dataclasses import dataclass

from . import atdf

BASE = 0x00800000
SIZE = 0x10000
FILL = 0xFF

# DS60001632H 10.3: 128-bit serial number, word addresses
SERIAL_ADDRS = (0x0080A00C, 0x0080A040, 0x0080A044, 0x0080A048)
SERIAL_DEFAULT = (0x50500000, 0x00000001, 0x00000002, 0x00000003)

# DS60001632H Table 10-3, production settings (BOOTPROT 0xB, BODVDD level 8
# enabled with reset action, BODCORE calibration 0x0A8, WDT off with
# PER/WINDOW/EWOFFSET 0xB, region locks 0xFFFF). Not the DFP default
# 0xFFFFFFFF, which would boot with an always-on WDT and BODVDD off.
USER_ROW_PRODUCTION = (0xB15088FB, 0xFFFFF8BB)


@dataclass
class NvmWord:
    addr: int           # absolute address
    value: int          # 32-bit little-endian word
    what: str

    @property
    def offset(self) -> int:
        return self.addr - BASE


def _deposit(word: int, mask: int, value: int) -> int:
    shift = (mask & -mask).bit_length() - 1
    if value << shift & ~mask:
        raise ValueError(f"0x{value:X} does not fit mask 0x{mask:X}")
    return (word & ~mask) | (value << shift)


def _fuse(device: atdf.Device, group: str, reg: str) -> tuple[int, dict[str, int]]:
    fuses = device.modules["FUSES"]
    base = next(g.base for i in device.instances for g in i.groups
                if g.name_in_module == group)
    r = next(r for r in fuses.groups[group].registers if r.name == reg)
    return base + r.offset, {b.name: b.mask for b in r.bitfields}


def nvm_param(device: atdf.Device) -> int:
    """NVMCTRL.PARAM implied by the NVMCTRL instance parameters."""
    nvm = device.instance("NVMCTRL")
    psz = (nvm.param_int("PAGE_SIZE") // 8).bit_length() - 1
    return (nvm.param_int("DATAFLASH_PAGES") << 20 | psz << 16 |
            nvm.param_int("PAGES"))


def _otp1(device: atdf.Device) -> list[NvmWord]:
    nvm = device.instance("NVMCTRL")
    psz = (nvm.param_int("PAGE_SIZE") // 8).bit_length() - 1
    addr0, f0 = _fuse(device, "OTP1_FUSES", "OTP1_WORD_0")
    addr1, f1 = _fuse(device, "OTP1_FUSES", "OTP1_WORD_1")
    w0 = _deposit(0xFFFFFFFF, f0["NVMCTRL_NVMP"], nvm.param_int("PAGES"))
    w0 = _deposit(w0, f0["NVMCTRL_PSZ"], psz)
    w1 = _deposit(0xFFFFFFFF, f1["NVMCTRL_DATAFLASHP"],
                  nvm.param_int("DATAFLASH_PAGES"))
    return [NvmWord(addr0, w0, "OTP1 word 0: NVMP, PSZ (NVM_LOCK left erased)"),
            NvmWord(addr1, w1, "OTP1 word 1: DATAFLASHP")]


def _otp5(device: atdf.Device) -> list[NvmWord]:
    """Software calibration row, DS Table 10-4 (ATDF only has bits 11:0)."""
    addr, f = _fuse(device, "OTP5_FUSES", "OTP5_WORD_0")
    row = (1 << 64) - 1
    for name in ("ADC0_BIASREFBUF", "ADC0_BIASCOMP", "ADC1_BIASREFBUF",
                 "ADC1_BIASCOMP"):
        row = _deposit(row, f[name], 4)             # mid-scale of 3 bits
    row = _deposit(row, 0x7F << 12, 0x3F)           # OSC32K CAL = OSC32K.CALIB reset
    row = _deposit(row, 0x3FFFFF << 19, 0x200220)   # CAL48M = OSCCTRL.CAL48M reset
    return [NvmWord(addr, row & 0xFFFFFFFF,
                    "OTP5 word 0: ADC bias mid-scale, OSC32K CAL 0x3F, CAL48M low"),
            NvmWord(addr + 4, row >> 32, "OTP5 word 1: CAL48M high, reserved")]


def default_words(device: atdf.Device) -> list[NvmWord]:
    """Words that differ from the 0xFF fill, excluding the serial number."""
    user0, _ = _fuse(device, "USER_FUSES", "USER_WORD_0")
    user1, _ = _fuse(device, "USER_FUSES", "USER_WORD_1")
    words = [NvmWord(user0, USER_ROW_PRODUCTION[0], "USER row word 0 (production)"),
             NvmWord(user1, USER_ROW_PRODUCTION[1], "USER row word 1 (production)")]
    words += _otp1(device) + _otp5(device)
    return sorted((w for w in words if w.value != 0xFFFFFFFF),
                  key=lambda w: w.addr)


def image(device: atdf.Device, serial=SERIAL_DEFAULT) -> bytes:
    """The full 64 KiB default image (used by tests and tools)."""
    buf = bytearray([FILL]) * SIZE
    words = default_words(device) + [NvmWord(a, s, "serial")
                                     for a, s in zip(SERIAL_ADDRS, serial)]
    for w in words:
        buf[w.offset:w.offset + 4] = w.value.to_bytes(4, "little")
    return bytes(buf)
