"""Run firmware/hello on a throwaway functional mock of the PIC32CM5164JH01100.

Scratch tool for the firmware agent (not the twin, not part of the project):
Unicorn executes the real ELF; peripherals are a store/read-back register
file plus the few behaviours the image needs (ready bits, SYNCBUSY = 0,
instant SERCOM TX with DRE/TXC, INTENSET/INTENCLR and PORT SET/CLR aliases,
DSU.DID, NVM rows, SysTick COUNTFLAG).  IRQ13 is level-triggered: while
INTFLAG & INTENSET & DRE and NVIC ISER bit 13 and PRIMASK == 0, the handler
from the vector table is called at an instruction boundary.

usage: py -I run_mock.py ELF [--cpuid 0x410CC200] [--trace]
"""

import argparse
import struct
import sys

from unicorn import (UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED,
                     UC_MODE_MCLASS, UC_MODE_THUMB, Uc, UcError)
from unicorn import arm_const as A

FLASH, FLASH_SIZE = 0x00000000, 0x80000
NVM, NVM_SIZE = 0x00800000, 0x10000
SRAM, SRAM_SIZE = 0x20000000, 0x10000
PERIPH, PERIPH_SIZE = 0x40000000, 0x04000000
PPB, PPB_SIZE = 0xE000E000, 0x1000
SENTINEL = FLASH + FLASH_SIZE - 0x10      # handler "return address"

SERCOM4 = 0x42001400
PORT = 0x41000000

RESET_VALUES = {                          # address: (size, value)
    0x40001014: (1, 0x82),                # OSCCTRL.OSC48MCTRL
    0x40001015: (1, 0x0B),                # OSCCTRL.OSC48MDIV
    0x40001038: (4, 0x00200220),          # OSCCTRL.CAL48M
    0x40001C20: (4, 0x00000106),          # GCLK.GENCTRL[0]
    0x4000081C: (4, 0x08000000),          # MCLK.APBCMASK
    0x41004004: (4, 0x00000080),          # NVMCTRL.CTRLB
    0x41004008: (4, 0x08032000),          # NVMCTRL.PARAM
    0x41002018: (4, 0x11060100),          # DSU.DID (B0)
}


def load_elf(path):
    data = open(path, "rb").read()
    assert data[:4] == b"\x7fELF" and data[4] == 1 and data[5] == 1
    phoff, = struct.unpack_from("<I", data, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
    segs = []
    for i in range(phnum):
        p_type, p_off, _vaddr, p_paddr, p_filesz = struct.unpack_from(
            "<IIIII", data, phoff + i * phentsize)
        if p_type == 1 and p_filesz:
            segs.append((p_paddr, data[p_off:p_off + p_filesz]))
    return segs


class Mock:
    def __init__(self, cpuid, trace):
        self.regs = {}                    # byte address -> byte value
        self.uart = bytearray()
        self.events = []                  # (uart offset, text)
        self.trace = trace
        self.mmio_log = []
        self.cpuid = cpuid
        self.done = False
        self.irq_check = False
        self.in_handler = False
        self.stuck = None
        self.systick_reads = 0
        for addr, (size, value) in RESET_VALUES.items():
            self.store(addr, size, value)

    # -- byte-lane storage -------------------------------------------------
    def load(self, addr, size):
        return sum(self.regs.get(addr + i, 0) << (8 * i) for i in range(size))

    def store(self, addr, size, value):
        for i in range(size):
            self.regs[addr + i] = (value >> (8 * i)) & 0xFF

    def bits(self, addr, mask, set_):
        old = self.load(addr, 4)
        self.store(addr, 4, (old | mask) if set_ else (old & ~mask))

    # -- peripheral space ----------------------------------------------------
    def periph_read(self, uc, off, size, _ud):
        addr = PERIPH + off
        value = self.load(addr, size)
        if addr in (0x40001018, 0x40001C04, SERCOM4 + 0x1C):  # SYNCBUSYs
            value = 0
        elif addr == 0x4000100C:                               # OSCCTRL.STATUS
            value = 0x10 if self.load(0x40001014, 1) & 0x2 else 0
        elif addr == SERCOM4 + 0x18:                           # INTFLAG
            value = self.load(addr, 1)
            if self.load(SERCOM4, 4) & 0x2:
                value |= 0x01                                  # DRE
        self.log("R", addr, size, value)
        return value

    def periph_write(self, uc, off, size, value, _ud):
        addr = PERIPH + off
        self.log("W", addr, size, value)
        if addr == self.stuck:
            return
        port_alias = {0x04: (0x00, False), 0x08: (0x00, True),
                      0x14: (0x10, False), 0x18: (0x10, True)}
        if PORT <= addr < PORT + 0x180 and (addr - PORT) % 0x80 in port_alias:
            group = PORT + (addr - PORT) // 0x80 * 0x80
            reg, set_ = port_alias[(addr - PORT) % 0x80]
            self.bits(group + reg, value, set_)
            if group == PORT + 0x100 and reg == 0x10 and value & 0x20:
                self.event("LED0 %s" % ("off" if set_ else "on"))
            return
        if addr == SERCOM4 + 0x16:                             # INTENSET
            self.store(addr, 1, self.load(addr, 1) | value)
            self.irq_check = True       # taken at the next instruction edge
            return
        if addr == SERCOM4 + 0x14:                             # INTENCLR
            self.store(SERCOM4 + 0x16, 1, self.load(SERCOM4 + 0x16, 1) & ~value)
            return
        if addr == SERCOM4 + 0x18:                             # INTFLAG W1C
            self.store(addr, 1, self.load(addr, 1) & ~value)
            return
        if addr == SERCOM4 + 0x28:                             # DATA
            self.uart.append(value & 0xFF)
            self.store(SERCOM4 + 0x18, 1, self.load(SERCOM4 + 0x18, 1) | 0x02)
            if self.uart.endswith(b"\n") and b"PP:EXIT" in self.last_line():
                self.done = True
            return
        if addr == 0x40001038:                                 # CAL48M impl
            value &= 0x003F033F
        self.store(addr, size, value)

    def last_line(self):
        return bytes(self.uart).rstrip(b"\r\n").split(b"\n")[-1]

    # -- private peripheral bus ----------------------------------------------
    def ppb_read(self, uc, off, size, _ud):
        addr = PPB + off
        if addr == 0xE000ED00:
            return self.cpuid
        value = self.load(addr, size)
        if addr == 0xE000E010 and value & 1:                   # SysTick CSR
            self.systick_reads += 1
            value |= 1 << 16                                   # 1 ms per poll
        return value

    def ppb_write(self, uc, off, size, value, _ud):
        addr = PPB + off
        if 0xE000E400 <= addr < 0xE000E420:                    # 2 prio bits
            value &= int.from_bytes(b"\xC0" * size, "little")
        if addr == 0xE000E100:                                 # ISER
            value |= self.load(addr, 4)
        self.store(addr, size, value)

    # -- helpers ---------------------------------------------------------------
    def log(self, kind, addr, size, value):
        if self.trace and len(self.mmio_log) < 400:
            self.mmio_log.append("%s%d 0x%08X = 0x%0*X" %
                                 (kind, size * 8, addr, size * 2, value))

    def event(self, text):
        self.events.append((len(self.uart), text))

    def irq13_level(self, uc):
        flag = self.load(SERCOM4 + 0x18, 1)
        if self.load(SERCOM4, 4) & 0x2:
            flag |= 0x01
        enabled = self.load(0xE000E100, 4) & (1 << 13)
        primask = uc.reg_read(A.UC_ARM_REG_PRIMASK)
        return bool(flag & self.load(SERCOM4 + 0x16, 1)) and enabled and \
            not primask


def call_handler(uc, handler):
    """Run an exception handler as a function call at an instruction edge."""
    names = [getattr(A, "UC_ARM_REG_R%d" % i) for i in range(13)] + \
        [A.UC_ARM_REG_SP, A.UC_ARM_REG_LR, A.UC_ARM_REG_PC, A.UC_ARM_REG_XPSR]
    saved = [uc.reg_read(r) for r in names]
    uc.reg_write(A.UC_ARM_REG_LR, SENTINEL | 1)
    uc.emu_start(handler | 1, SENTINEL, count=100000)
    for r, v in zip(names, saved):
        uc.reg_write(r, v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--cpuid", type=lambda s: int(s, 0), default=0x410CC200)
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("--no-irq", action="store_true",
                    help="never deliver IRQ13 (broken NVIC wiring)")
    ap.add_argument("--irq-limit", type=int, default=10000,
                    help="stop delivering IRQ13 after this many entries")
    ap.add_argument("--stuck", type=lambda s: int(s, 0), default=None,
                    help="drop writes to this peripheral address")
    args = ap.parse_args()

    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.ctl_set_cpu_model(A.UC_CPU_ARM_CORTEX_M0)
    m = Mock(args.cpuid, args.trace)
    m.stuck = args.stuck

    uc.mem_map(FLASH, FLASH_SIZE)
    uc.mem_map(NVM, NVM_SIZE)
    uc.mem_map(SRAM, SRAM_SIZE)
    uc.mmio_map(PERIPH, PERIPH_SIZE, m.periph_read, None, m.periph_write, None)
    uc.mmio_map(PPB, PPB_SIZE, m.ppb_read, None, m.ppb_write, None)

    nvm = bytearray(b"\xFF" * NVM_SIZE)
    struct.pack_into("<II", nvm, 0x4000, 0xB15088FB, 0xFFFFF8BB)   # USER
    cal_row = (0x00200220 << 19) | (0x40 << 12) | 0x249            # OTP5
    struct.pack_into("<Q", nvm, 0x6020, cal_row)
    for off, word in ((0xA00C, 0x50500000), (0xA040, 1), (0xA044, 2),
                      (0xA048, 3)):
        struct.pack_into("<I", nvm, off, word)
    uc.mem_write(NVM, bytes(nvm))
    for addr, blob in load_elf(args.elf):
        uc.mem_write(addr, blob)

    def unmapped(uc, access, addr, size, value, _ud):
        print("UNMAPPED access type %d at 0x%08X size %d (pc 0x%08X)" %
              (access, addr, size, uc.reg_read(A.UC_ARM_REG_PC)))
        return False
    uc.hook_add(UC_HOOK_MEM_UNMAPPED, unmapped)

    # Stopping inside an MMIO callback leaves PC on the store, so stop at
    # the next instruction edge instead (the instruction has not run yet).
    def edge(uc, address, size, _ud):
        if m.done or (m.irq_check and not m.in_handler):
            uc.emu_stop()
    uc.hook_add(UC_HOOK_CODE, edge)

    sp, reset = struct.unpack("<II", uc.mem_read(0, 8))
    sercom4_handler, = struct.unpack("<I", uc.mem_read((16 + 13) * 4, 4))
    uc.reg_write(A.UC_ARM_REG_SP, sp)
    pc, budget, handler_calls = reset & ~1, 50, 0
    while not m.done and budget > 0:
        budget -= 1
        try:
            uc.emu_start(pc | 1, 0xFFFFFFFE, count=20_000_000)
        except UcError as e:
            print("CPU error: %s at pc 0x%08X" %
                  (e, uc.reg_read(A.UC_ARM_REG_PC)))
            break
        pc = uc.reg_read(A.UC_ARM_REG_PC)
        m.in_handler = True
        while (m.irq_check and not args.no_irq and m.irq13_level(uc)
               and handler_calls < args.irq_limit):
            call_handler(uc, sercom4_handler & ~1)
            handler_calls += 1
        m.in_handler = False
        m.irq_check = False

    sys.stdout.write(m.uart.decode("ascii", "replace").replace("\r\n", "\n"))
    print("---- mock: %s, IRQ13 handler calls %d, SysTick CSR polls %d, "
          "stopped at pc 0x%08X" % ("PP:EXIT seen" if m.done else "NO EXIT",
                                   handler_calls, m.systick_reads, pc))
    for pos, text in m.events:
        print("---- event at uart byte %d: %s" % (pos, text))
    if args.trace:
        print("\n".join(m.mmio_log))
    return 0 if m.done else 1


if __name__ == "__main__":
    sys.exit(main())
