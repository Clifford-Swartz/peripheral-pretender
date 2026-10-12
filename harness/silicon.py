# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""pyOCD access to the PIC32CM JH01 Curiosity Pro through its EDBG (CMSIS-DAP).

Sessions open in pyOCD's "attach" connect mode unless a caller passes
allow_halt=True: attach neither halts nor resets the core (pyOCD sets
DHCSR.C_DEBUGEN only and clears it again on disconnect).  Everything that
halts, resets, writes or programs the chip sits behind an allow_* argument
that the command-line tools set only from an explicit --allow-* option, given
after the user approved that step (docs/silicon-harness.md, "Safety rules").

pyOCD is imported lazily so dry runs and tests work without it.
"""
from __future__ import annotations

import contextlib
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

DFP_DIR = Path.home() / ".mchp_packs" / "Microchip" / "PIC32CM-JH_DFP" / "1.3.221"
TARGET = "pic32cm5164jh01100"
FREQUENCY = 4_000_000           # Zephyr's pyocd runner uses 4 MHz for this board

FLASH = (0x00000000, 0x00080000)
DSU_DID = 0x41002018
DSU_DID_EXT = 0x41002118        # external (debugger) mirror, readable when protected
DSU_STATUSB = 0x41002102        # external mirror: PROT bit 0, DBGPRES bit 1
DSU_STATUSA_EXT = 0x41002101
SCB_CPUID = 0xE000ED00
DHCSR, AIRCR, DEMCR = 0xE000EDF0, 0xE000ED0C, 0xE000EDFC
USER_ROW = 0x00804000
APP_START = 0x00001000          # where the factory bootloader looks for an image

# BOOTPROT[3:0] -> protected bytes from 0x0 (ATDF USER_FUSES_NVMCTRL__BOOTPROT,
# DS60001632H Table 26-2).  Production parts ship 0xB: the factory UART/I2C
# bootloader in 0x0-0xFFF (DS60001632H Appendix A, p.1211).
BOOTPROT_BYTES = {0xF: 0, 0xE: 512, 0xD: 1024, 0xC: 2048, 0xB: 4096,
                  0xA: 8192, 0x9: 16384, 0x8: 32768, 0x7: 40960, 0x6: 49152,
                  0x5: 57344, 0x4: 65536, 0x3: 73728, 0x2: 81920, 0x1: 90112,
                  0x0: 98304}
LOCK_REGION = 0x8000            # 16 regions of 32 KB (DS60001632H Table 26-1)
SILICON_REVISIONS = {1: "B0", 2: "C0"}      # DID[11:8], errata DS80001000H Table 2


class SiliconError(Exception):
    pass


@dataclass(frozen=True)
class NvmProtection:
    """What the NVM user row and the security bit allow a programmer to write."""

    user_row: tuple[int, int]
    secured: bool

    @property
    def bootprot_bytes(self) -> int:
        return BOOTPROT_BYTES[self.user_row[0] & 0xF]

    @property
    def locked_regions(self) -> list[int]:
        locks = self.user_row[1] >> 16          # 1 = unlocked
        return [i for i in range(16) if not locks & (1 << i)]

    def conflicts(self, ranges: list[tuple[int, int]]) -> list[str]:
        """Reasons why writing [start, end) ranges would fail or be unsafe."""
        problems = []
        if self.secured:
            problems.append("device is secured (DSU.STATUSB.PROT=1); only a chip "
                            "erase would unlock it and the harness never erases")
        boot = self.bootprot_bytes
        for start, end in ranges:
            if start < boot:
                problems.append(
                    f"image writes 0x{start:08X}-0x{end - 1:08X} but BOOTPROT="
                    f"0x{self.user_row[0] & 0xF:X} protects 0x0-0x{boot - 1:X} "
                    "(factory bootloader); link the image behind it, or have the "
                    "user program the board once with MPLAB, whose erase clears "
                    "BOOTPROT and keeps the rest of the user row (DFP "
                    "scripts/dap_cortex-m0plus.py restoreBOOTPROTandNVMCTRLREGIONLOCKS)")
            for region in self.locked_regions:
                lo, hi = region * LOCK_REGION, (region + 1) * LOCK_REGION
                if start < hi and lo < end:
                    problems.append(f"flash region {region} (0x{lo:05X}-0x{hi - 1:05X}) "
                                    "is locked in the user row")
        return problems


@dataclass
class Preflight:
    """Read-only facts about the attached chip."""

    probe: str
    did: int | None = None
    cpuid: int | None = None
    statusb: int | None = None
    protection: NvmProtection | None = None
    flash_words: dict[int, int] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)

    def describe(self) -> list[str]:
        lines = [f"probe        {self.probe}"]
        if self.did is not None:
            rev = (self.did >> 8) & 0xF
            lines.append(f"DSU.DID      0x{self.did:08X} (revision "
                         f"{SILICON_REVISIONS.get(rev, '?')})")
        if self.cpuid is not None:
            lines.append(f"SCB.CPUID    0x{self.cpuid:08X}")
        if self.statusb is not None:
            lines.append(f"DSU.STATUSB  0x{self.statusb:02X} (PROT={self.statusb & 1}, "
                         f"DBGPRES={self.statusb >> 1 & 1})")
        if self.protection:
            p = self.protection
            lines.append(f"user row     0x{p.user_row[0]:08X} 0x{p.user_row[1]:08X} "
                         f"(BOOTPROT {p.bootprot_bytes} bytes, locked regions "
                         f"{p.locked_regions or 'none'})")
        for addr, word in sorted(self.flash_words.items()):
            lines.append(f"flash@0x{addr:05X} 0x{word:08X}"
                         + (" (erased)" if word == 0xFFFFFFFF else ""))
        lines += [f"error        {e}" for e in self.errors]
        return lines


def list_probes() -> list[tuple[str, str]]:
    """(unique id, description) of every connected CMSIS-DAP/J-Link probe."""
    from pyocd.core.helpers import ConnectHelper

    probes = ConnectHelper.get_all_connected_probes(blocking=False,
                                                    print_wait_message=False)
    return [(p.unique_id, p.description) for p in probes]


@contextlib.contextmanager
def session(probe: str | None = None, pack: str | Path | None = None,
            target: str = TARGET, frequency: int = FREQUENCY,
            allow_halt: bool = False) -> Iterator:
    """Open a pyOCD session; yields the session.

    allow_halt=False connects in "attach" mode (no halt, no reset).  With
    allow_halt=True pyOCD halts the core on connect, as programming needs.
    """
    from pyocd.core.helpers import ConnectHelper

    probes = list_probes()
    if probe:
        probes = [p for p in probes if p[0].upper() == probe.upper()]
    if not probes:
        which = f" with unique id {probe}" if probe else ""
        raise SiliconError(f"no debug probe{which} found; is the board connected "
                           "over its DEBUG USB port?")
    if len(probes) > 1:
        found = ", ".join(f"{uid} ({desc})" for uid, desc in probes)
        raise SiliconError(f"several probes ({found}); pass --probe")
    options = {
        "connect_mode": "halt" if allow_halt else "attach",
        "target_override": target,
        "pack": str(pack or DFP_DIR),
        "frequency": frequency,
        "auto_unlock": False,           # never mass-erase a locked part
        "resume_on_disconnect": True,
        "hide_programming_progress": True,
    }
    sess = ConnectHelper.session_with_chosen_probe(
        blocking=False, unique_id=probes[0][0], options=options)
    if sess is None:
        raise SiliconError(f"probe {probes[0][0]} disappeared")
    with sess:
        yield sess


def read_reg(target, address: int, size: int) -> int:
    """One access of exactly `size` bytes; raises pyOCD TransferError on a fault."""
    return target.read_memory(address, transfer_size=size * 8)


def preflight(sess) -> Preflight:
    """Read-only identification: DID, CPUID, security, user row, flash heads."""
    from pyocd.core.exceptions import TransferError

    target = sess.target
    result = Preflight(probe=sess.probe.unique_id)

    def read(address: int) -> int | None:
        try:
            return read_reg(target, address, 4)
        except TransferError as exc:
            result.errors.append(f"read 0x{address:08X}: {exc}")
            return None

    result.did = read(DSU_DID)
    if result.did is None:
        result.did = read(DSU_DID_EXT)
    result.cpuid = read(SCB_CPUID)
    try:
        result.statusb = read_reg(target, DSU_STATUSB, 1)
    except TransferError as exc:
        result.errors.append(f"read DSU.STATUSB: {exc}")
    words = (read(USER_ROW), read(USER_ROW + 4))
    if None not in words and result.statusb is not None:
        result.protection = NvmProtection(words, bool(result.statusb & 1))
    for addr in (0x0, 0x4, APP_START, APP_START + 4):
        word = read(addr)
        if word is not None:
            result.flash_words[addr] = word
    return result


def reset_and_halt_dfp(target, allow_halt: bool = False) -> None:
    """Reset through SYSRESETREQ and halt before the first instruction.

    Mirrors reset_and_halt() in the DFP's scripts/dap_cortex-m0plus.py
    (lines 462-485), which MPLAB tools run for this part.
    """
    if not allow_halt:
        raise SiliconError("reset-and-halt needs --allow-halt (user approval)")
    from pyocd.core.exceptions import TransferError

    target.halt()
    target.write32(DEMCR, 0x01000001)               # TRCENA | VC_CORERESET
    try:
        target.write32(AIRCR, 0x05FA0004)           # VECTKEY | SYSRESETREQ
    except TransferError:
        pass                                        # the reset can eat the ack
    seen_reset = False
    for _ in range(10):
        dhcsr = target.read32(DHCSR)
        if dhcsr & 0x02000000:                      # S_RESET_ST
            seen_reset = True
            target.write32(DHCSR, 0xA05F0003)       # DBGKEY | C_HALT | C_DEBUGEN
            target.write8(DSU_STATUSA_EXT, 0x02)    # clear STATUSA.CRSTEXT
            dhcsr = target.read32(DHCSR)
        if seen_reset and dhcsr & 0x00020000:       # S_HALT
            break
        time.sleep(0.1)
    else:
        raise SiliconError("the device did not come out of reset halted")
    target.write32(DEMCR, 0x01000000)               # TRCENA
    target.get_state()                              # resync pyOCD's state cache
