/*
 * Test shim for include/hw/misc/mchp_regfile.h: the spec 5.1 contract
 * (docs/design/v0-vertical-slice.md) copied verbatim between the BEGIN/END
 * markers, plus the few QEMU types it uses. Only for syntax-checking
 * generated headers outside a QEMU build.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef HW_MISC_MCHP_REGFILE_H
#define HW_MISC_MCHP_REGFILE_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t hwaddr;
typedef struct MemTxAttrs {
    unsigned int secure:1;
    unsigned int debug:1;
} MemTxAttrs;

/* BEGIN spec 5.1 */
typedef struct MchpRegInfo {
    const char *name;   /* ATDF register name, e.g. "CTRLA", "PCHCTRL" */
    uint16_t offset;    /* byte offset of element 0 inside the register group */
    uint8_t size;       /* 1, 2 or 4 bytes */
    uint8_t count;      /* array elements (1 for scalars) */
    uint16_t stride;    /* bytes between array elements (== size unless ATDF says otherwise) */
    int16_t alias_of;   /* index in the same table of the storage register this one aliases,
                           or -1; may equal its own index (e.g. INTENSET is the storage) */
    uint32_t reset;     /* reset value of every element (exceptions: MchpResetOverride) */
    uint32_t impl;      /* implemented bits; reads return value & impl (0xFFFFFFFF if ATDF has no bitfields) */
    uint32_t wmask;     /* software-writable bits (RW bits minus ro/w1c) */
    uint32_t w1c;       /* write-1-to-clear bits */
    uint32_t rdz;       /* bits that always read 0 (strobes, self-clearing, write-only) */
    uint32_t enprot;    /* enable-protected bits: writable only while the IP is disabled */
    uint32_t syncbusy;  /* SYNCBUSY bits raised by writing this register (0 = none) */
    uint32_t flags;     /* MCHP_F_* */
} MchpRegInfo;

#define MCHP_F_PAC             (1u << 0)  /* PAC write-protectable */
#define MCHP_F_ALIAS_SET       (1u << 1)  /* write 1 sets bits of alias_of; read returns alias_of */
#define MCHP_F_ALIAS_CLEAR     (1u << 2)  /* write 1 clears bits of alias_of; read returns alias_of */
#define MCHP_F_ALIAS_TOGGLE    (1u << 3)  /* write 1 toggles bits of alias_of; read returns alias_of */
#define MCHP_F_DYNAMIC         (1u << 4)  /* value produced by the model's pre_read hook */
#define MCHP_F_HOOK_PRE_READ   (1u << 5)  /* call pre_read (side effects only when !attrs.debug) */
#define MCHP_F_HOOK_POST_WRITE (1u << 6)  /* call post_write after storage is updated */
#define MCHP_F_BUSY_BUSERR     (1u << 7)  /* write while own SYNCBUSY bit set = bus error (pedantic mode) */
#define MCHP_F_WRITE_ONLY      (1u << 8)  /* reads return 0 */
#define MCHP_F_READ_ONLY       (1u << 9)  /* writes ignored (+LOG_GUEST_ERROR) */

typedef struct MchpResetOverride {
    uint16_t offset;    /* byte offset of the element */
    uint8_t size;
    uint32_t value;
} MchpResetOverride;

typedef struct MchpRegFile MchpRegFile;
typedef uint32_t (*MchpRegPreReadFn)(MchpRegFile *rf, const MchpRegInfo *ri,
                                     unsigned idx, uint32_t stored,
                                     bool side_effects);
typedef void (*MchpRegPostWriteFn)(MchpRegFile *rf, const MchpRegInfo *ri,
                                   unsigned idx, uint32_t old_value,
                                   uint32_t written, uint32_t we);
typedef bool (*MchpRegEnabledFn)(MchpRegFile *rf);

struct MchpRegFile {
    const char *name;                 /* instance name for logs, e.g. "sercom4" */
    uint8_t *storage;                 /* little-endian backing store */
    uint32_t size;                    /* bytes of storage (register-group size) */
    const MchpRegInfo *regs;          /* active table (SERCOM switches it on CTRLA.MODE) */
    unsigned nregs;
    const MchpResetOverride *ovr;
    unsigned novr;
    void *opaque;                     /* owning device */
    MchpRegPreReadFn pre_read;        /* may be NULL */
    MchpRegPostWriteFn post_write;    /* may be NULL */
    MchpRegEnabledFn is_enabled;      /* NULL = never enabled (no enable protection) */
};

void mchp_regfile_init(MchpRegFile *rf, const char *name, uint32_t size,
                       const MchpRegInfo *regs, unsigned nregs);
void mchp_regfile_set_table(MchpRegFile *rf, const MchpRegInfo *regs, unsigned nregs);
void mchp_regfile_set_overrides(MchpRegFile *rf, const MchpResetOverride *ovr, unsigned n);
void mchp_regfile_reset(MchpRegFile *rf);       /* reset values, then overrides */
uint64_t mchp_regfile_read(MchpRegFile *rf, hwaddr addr, unsigned size, MemTxAttrs attrs);
void mchp_regfile_write(MchpRegFile *rf, hwaddr addr, uint64_t val, unsigned size, MemTxAttrs attrs);
uint32_t mchp_regfile_peek(MchpRegFile *rf, uint32_t offset, unsigned size);   /* raw storage */
void mchp_regfile_poke(MchpRegFile *rf, uint32_t offset, unsigned size, uint32_t val);
const MchpRegInfo *mchp_regfile_find(MchpRegFile *rf, const char *name);       /* active table */
/* VMState helper: VMSTATE_VBUFFER_UINT32 or equivalent over storage/size */
/* END spec 5.1 */

#endif /* HW_MISC_MCHP_REGFILE_H */
