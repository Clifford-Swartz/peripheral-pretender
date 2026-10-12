/*
 * Test shim for QEMU include/hw/core/registerfields.h: the REGn()/FIELD()
 * macros with identical expansion (A_*, R_*, R_*_SHIFT/_LENGTH/_MASK), without
 * the qemu/bitops.h dependency that needs qemu/osdep.h. Syntax checks only.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef REGISTERFIELDS_H
#define REGISTERFIELDS_H

/* Same definition as include/qemu/bitops.h */
#define MAKE_64BIT_MASK(shift, length) \
    (((~0ULL) >> (64 - (length))) << (shift))

#define REG32(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 4 };

#define REG8(reg, addr)                                                   \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) };

#define REG16(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 2 };

#define REG64(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 8 };

#define FIELD(reg, field, shift, length)                                  \
    enum { R_ ## reg ## _ ## field ## _SHIFT = (shift)};                  \
    enum { R_ ## reg ## _ ## field ## _LENGTH = (length)};                \
    enum { R_ ## reg ## _ ## field ## _MASK =                             \
                                        MAKE_64BIT_MASK(shift, length)};

#endif
