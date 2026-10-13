# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Default image of the NVM rows region 0x00800000-0x0080FFFF (spec 4.3).

Words that the ATDF can describe are computed from its FUSES fields and
NVMCTRL parameters; the rest (production user row, serial number, calibration
row) come from the device profile (profiles/<device>.yaml). Values that only
silicon can provide (calibration, serial number) are plausible placeholders
and must be dumped from a real part before being trusted.
"""

from __future__ import annotations

from dataclasses import dataclass

from . import atdf
from .profile import NvmRows

BASE = 0x00800000
SIZE = 0x10000
FILL = 0xFF

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


def _has_fuse_group(device: atdf.Device, group: str) -> bool:
    return any(g.name_in_module == group for i in device.instances
               for g in i.groups)


def _otp1(device: atdf.Device) -> list[NvmWord]:
    if not _has_fuse_group(device, "OTP1_FUSES"):
        return []                   # not mapped on this device (MC00)
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


def default_words(device: atdf.Device, rows: NvmRows) -> list[NvmWord]:
    """Words that differ from the 0xFF fill, excluding the serial number."""
    user0, _ = _fuse(device, "USER_FUSES", "USER_WORD_0")
    user1, _ = _fuse(device, "USER_FUSES", "USER_WORD_1")
    words = [NvmWord(user0, rows.user_row[0], "USER row word 0 (production)"),
             NvmWord(user1, rows.user_row[1], "USER row word 1 (production)")]
    words += _otp1(device)
    words += [NvmWord(addr, value, what) for addr, value, what in rows.words]
    return sorted((w for w in words if w.value != 0xFFFFFFFF),
                  key=lambda w: w.addr)


def image(device: atdf.Device, rows: NvmRows, serial=None) -> bytes:
    """The full 64 KiB default image (used by tests and tools)."""
    buf = bytearray([FILL]) * SIZE
    serial = rows.serial_default if serial is None else serial
    words = default_words(device, rows) + [
        NvmWord(a, s, "serial") for a, s in zip(rows.serial_addrs, serial)]
    for w in words:
        buf[w.offset:w.offset + 4] = w.value.to_bytes(4, "little")
    return bytes(buf)
