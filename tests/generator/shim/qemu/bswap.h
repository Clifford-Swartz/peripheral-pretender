/*
 * Test shim for QEMU include/qemu/bswap.h: only stl_le_p(), as used by the
 * generated NVM-rows header. Syntax checks and the image round-trip test.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef QEMU_BSWAP_H
#define QEMU_BSWAP_H

#include <stdint.h>

static inline void stl_le_p(void *ptr, uint32_t v)
{
    uint8_t *p = ptr;

    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

#endif
