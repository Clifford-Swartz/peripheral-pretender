/*
 * DMAC check (PIC32CM MC00, DMAC U2223): the DMA controller against what
 * silicon did in firmware/hello APP=dmacprobe
 * (docs/research/peripherals/dmac-u2223.md). Memory-to-memory transfers,
 * descriptors and their write-back copies, trigger actions, block actions,
 * suspend and resume, errors, the software trigger, arbitration, the CRC
 * engine, events, the interrupt and the register rules are checked
 * exactly; TC-paced transfers within a range. A failed check names what
 * it expected and ends its line in FAIL; PP:EXIT <failures>.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define DM              DMAC_REGS
#define TCN             (TC0_REGS->COUNT16)
#define TC1N            (TC1_REGS->COUNT16)

#define SWRST           0x0001U
#define DMAENABLE       0x0002U
#define CRCENABLE       0x0004U
#define LVLEN_ALL       0x0F00U
#define CH_SWRST        0x01U
#define CH_ENABLE       0x02U
#define EVACT(x)        ((uint32_t)(x) << 0)
#define EVIE            (1U << 3)
#define EVOE            (1U << 4)
#define LVL(x)          ((uint32_t)(x) << 5)
#define TRIGSRC(x)      ((uint32_t)(x) << 8)
#define TRIGACT(x)      ((uint32_t)(x) << 22)
#define CMD(x)          ((uint32_t)(x) << 24)
#define TA_BLOCK        0U
#define TA_BEAT         2U
#define TA_TRANSACTION  3U
#define VALID           0x0001U
#define EVOSEL(x)       ((uint16_t)((x) << 1))
#define BLOCKACT(x)     ((uint16_t)((x) << 3))
#define BEATSIZE(x)     ((uint16_t)((x) << 8))
#define SRCINC          0x0400U
#define DSTINC          0x0800U
#define STEPSEL_SRC     0x1000U
#define STEPSIZE(x)     ((uint16_t)((x) << 13))
#define BA_NOACT        0U
#define BA_INT          1U
#define BA_SUSPEND      2U
#define TERR            0x01U
#define TCMPL           0x02U
#define SUSP            0x04U
#define ST_PEND         0x01U
#define ST_BUSY         0x02U
#define ST_FERR         0x04U
#define TRIG_TC0_OVF    0x15U
#define BYTE            (VALID | BEATSIZE(0) | SRCINC | DSTINC)

typedef struct {
    uint16_t btctrl;
    uint16_t btcnt;
    uint32_t src;
    uint32_t dst;
    uint32_t next;
} desc_t;

static desc_t desc[12] __attribute__((aligned(16)));
static volatile desc_t wb[12] __attribute__((aligned(16)));
static desc_t chain[4] __attribute__((aligned(16)));
static uint8_t src[256] __attribute__((aligned(4)));
static uint8_t dst[256 + 8] __attribute__((aligned(4)));
static volatile uint32_t word;
static volatile uint32_t log32[32];
static volatile uint16_t isr_log[4];
static volatile unsigned int isr_n;

static void put_range(const char *name, uint32_t value, uint32_t lo,
                      uint32_t hi, unsigned int digits)
{
    put_field(name, value, digits);
    if (value < lo || value > hi) {
        console_puts("(expected 0x");
        console_put_hex(lo, digits);
        console_puts("..0x");
        console_put_hex(hi, digits);
        console_putc(')');
        failures++;
    }
}

static void end(void)
{
    static unsigned int seen;

    console_puts(failures == seen ? " ok\r\n" : " FAIL\r\n");
    seen = failures;
}

static void wait_ctrl_off(void)
{
    for (uint32_t n = 0; (DM->DMAC_CTRL & (DMAENABLE | CRCENABLE)) &&
         n < 1000000U; n++) {
    }
}

static void dst_fill(void)
{
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
}

static void restart(void)
{
    volatile uint32_t *p = (volatile uint32_t *)wb;

    DM->DMAC_CTRL = 0U;
    wait_ctrl_off();
    DM->DMAC_CTRL = SWRST;
    for (unsigned int i = 0; i < sizeof(wb) / 4U; i++) {
        p[i] = 0xCCCCCCCCU;
    }
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
}

static void ch_cfg(unsigned int ch, uint32_t chctrlb)
{
    DM->DMAC_CHID = (uint8_t)ch;
    DM->DMAC_CHCTRLA = 0U;
    for (uint32_t n = 0; (DM->DMAC_CHCTRLA & CH_ENABLE) && n < 100000U; n++) {
    }
    DM->DMAC_CHCTRLA = CH_SWRST;
    DM->DMAC_CHCTRLB = chctrlb;
    DM->DMAC_CHINTFLAG = 0x07U;
}

static void ch_on(unsigned int ch)
{
    DM->DMAC_CHID = (uint8_t)ch;
    DM->DMAC_CHCTRLA = CH_ENABLE;
}

static void swtrig(unsigned int ch)
{
    DM->DMAC_SWTRIGCTRL |= 1U << ch;
}

static void cmd(unsigned int ch, uint32_t c)
{
    DM->DMAC_CHID = (uint8_t)ch;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(c);
}

static void set_desc(desc_t *d, uint16_t btctrl, uint16_t n, const void *s,
                     void *t, desc_t *next)
{
    uint32_t bytes = (uint32_t)n << ((btctrl >> 8) & 3U);

    d->btctrl = btctrl;
    d->btcnt = n;
    d->src = (uint32_t)s + ((btctrl & SRCINC) ? bytes : 0U);
    d->dst = (uint32_t)t + ((btctrl & DSTINC) ? bytes : 0U);
    d->next = (uint32_t)next;
}

static void settle(void)
{
    for (volatile uint32_t i = 0; i < 2000U; i++) {
    }
}

/* CHINTFLAG / CHSTATUS / CHCTRLA of @ch, as one hex number */
static uint32_t chan(unsigned int ch)
{
    DM->DMAC_CHID = (uint8_t)ch;
    return (uint32_t)DM->DMAC_CHINTFLAG << 8 |
           (uint32_t)DM->DMAC_CHSTATUS << 4 | DM->DMAC_CHCTRLA;
}

static unsigned int copied(uint8_t first, unsigned int n)
{
    unsigned int ok = 0;

    for (unsigned int i = 0; i < n; i++) {
        ok += dst[4 + i] == (uint8_t)(first + i);
    }
    return ok;
}

static void tc0_start(void)
{
    TCN.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (TCN.TC_SYNCBUSY) {
    }
    TCN.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64;
    TCN.TC_WAVE = TC_WAVE_WAVEGEN_MFRQ;
    TCN.TC_CC[0] = 749U;                    /* 1 kHz */
    while (TCN.TC_SYNCBUSY) {
    }
    TCN.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while (TCN.TC_SYNCBUSY) {
    }
}

static void tc0_stop(void)
{
    TCN.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (TCN.TC_SYNCBUSY) {
    }
}

static void check_registers(void)
{
    console_puts("reset");
    put_checked("ctrl", DM->DMAC_CTRL, 0, 4);
    put_checked("qos", DM->DMAC_QOSCTRL, 0x2A, 2);
    put_checked("chstatus", DM->DMAC_CHSTATUS, 0, 2);
    end();
    console_puts("masks");
    DM->DMAC_CRCCTRL = 0xFFFFU;
    put_checked("crcctrl", DM->DMAC_CRCCTRL, 0x3F0F, 4);
    DM->DMAC_CRCCTRL = 0U;
    DM->DMAC_PRICTRL = 0xFFFFFFFFU;
    put_checked("prictrl", DM->DMAC_PRICTRL, 0x8F8F8F8F, 8);
    DM->DMAC_PRICTRL = 0U;
    DM->DMAC_CHID = 0xFFU;
    put_checked("chid", DM->DMAC_CHID, 0x0F, 2);
    DM->DMAC_CHID = 0U;
    DM->DMAC_CHCTRLB = 0x00FFFFFFU;
    put_checked("chctrlb", DM->DMAC_CHCTRLB, 0x00C03F7F, 8);
    DM->DMAC_CHCTRLB = 0U;
    DM->DMAC_CHCTRLA = 0x40U;
    put_checked("chctrla", DM->DMAC_CHCTRLA, 0x40, 2);
    DM->DMAC_CHCTRLA = 0U;
    end();
    /* CHID 12-15 read channels 8-11 and ignore writes */
    console_puts("chid12_15");
    DM->DMAC_CHID = 9U;
    DM->DMAC_CHCTRLB = TRIGSRC(9);
    DM->DMAC_CHID = 13U;
    put_checked("reads_ch9", DM->DMAC_CHCTRLB, TRIGSRC(9), 8);
    DM->DMAC_CHCTRLB = TRIGSRC(13);
    DM->DMAC_CHID = 9U;
    put_checked("write_ignored", DM->DMAC_CHCTRLB, TRIGSRC(9), 8);
    DM->DMAC_CHCTRLB = 0U;
    end();
}

static void check_copies(void)
{
    console_puts("copy");
    for (unsigned int bs = 0; bs < 3U; bs++) {
        restart();
        dst_fill();
        ch_cfg(0, TRIGACT(TA_BLOCK));
        set_desc(&desc[0], VALID | BEATSIZE(bs) | SRCINC | DSTINC |
                 BLOCKACT(BA_NOACT), (uint16_t)(64U >> bs), src, dst + 4, 0);
        ch_on(0);
        (void)DM->DMAC_CHSTATUS;            /* the fetch has happened */
        put_checked("enable_wb", wb[0].btctrl, desc[0].btctrl, 4);
        swtrig(0);
        put_checked("swtrig", DM->DMAC_SWTRIGCTRL, 0, 3);
        put_checked("ok", copied(0, 64), 64, 2);
        put_checked("guard", dst[3] == 0xA5 && dst[68] == 0xA5, 1, 1);
        /* NOACT, single block: TCMPL anyway, disabled, VALID cleared */
        put_checked("chan", chan(0), 0x200, 3);
        put_checked("wb", wb[0].btctrl, (uint16_t)(desc[0].btctrl & ~VALID),
                    4);
        put_checked("wbcnt", wb[0].btcnt, 0, 4);
    }
    end();

    console_puts("steps");
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    desc[0].btctrl = VALID | BEATSIZE(0) | SRCINC | STEPSEL_SRC | STEPSIZE(2);
    desc[0].btcnt = 4;
    desc[0].src = (uint32_t)src + 16U;      /* 4 beats x 4 steps */
    desc[0].dst = (uint32_t)(dst + 4);
    desc[0].next = 0U;
    ch_on(0);
    swtrig(0);
    settle();
    put_checked("src_x4_last", dst[4], 12, 2);
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    desc[0].btctrl = VALID | BEATSIZE(0) | SRCINC | DSTINC | STEPSIZE(2);
    desc[0].btcnt = 4;
    desc[0].src = (uint32_t)src + 4U;
    desc[0].dst = (uint32_t)(dst + 4) + 16U;   /* destination steps x4 */
    desc[0].next = 0U;
    ch_on(0);
    swtrig(0);
    settle();
    put_checked("dst_x4", (uint32_t)dst[4] << 24 | (uint32_t)dst[8] << 16 |
                (uint32_t)dst[12] << 8 | dst[16], 0x00010203, 8);
    put_checked("gaps", dst[5], 0xA5, 2);
    end();

    console_puts("btcnt0");                   /* 65536 beats */
    restart();
    word = 0;
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | BLOCKACT(BA_INT), 0,
             (void *)&word, (void *)&word, 0);
    ch_on(0);
    swtrig(0);
    for (uint32_t n = 0; !(DM->DMAC_CHINTFLAG & TCMPL) && n < 1000000U; n++) {
    }
    put_checked("chan", chan(0), 0x200, 3);
    end();
}

static void check_writeback(void)
{
    console_puts("writeback");
    restart();
    ch_cfg(2, TRIGACT(TA_BEAT));
    set_desc(&desc[2], BYTE | BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(2);
    for (unsigned int i = 0; i < 3U; i++) {
        swtrig(2);
        settle();
    }
    put_checked("wb_stale", wb[2].btcnt, 8, 4);
    put_checked("active", DM->DMAC_ACTIVE, 0x00058200, 8);
    ch_cfg(3, TRIGACT(TA_BLOCK));
    set_desc(&desc[3], BYTE | BLOCKACT(BA_INT), 8, src, dst + 20, 0);
    ch_on(3);
    swtrig(3);
    settle();
    put_checked("wb_after_loss", wb[2].btcnt, 5, 4);
    end();

    console_puts("swtrig");
    restart();
    ch_cfg(0, TRIGACT(TA_BEAT));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    DM->DMAC_SWTRIGCTRL = 1U;               /* disabled: stays set */
    put_checked("disabled", DM->DMAC_SWTRIGCTRL, 1, 3);
    put_checked("pend", DM->DMAC_PENDCH, 0, 3);
    ch_on(0);
    settle();
    put_checked("fired_at_enable", DM->DMAC_ACTIVE >> 16, 7, 4);
    restart();
    dst_fill();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE | BLOCKACT(BA_INT), 16, src, dst + 4, 0);
    ch_on(1);
    DM->DMAC_SWTRIGCTRL = 2U;
    DM->DMAC_SWTRIGCTRL = 2U;
    DM->DMAC_SWTRIGCTRL = 2U;               /* one pending, one queued */
    settle();
    put_checked("three_writes", copied(0, 16), 2, 2);
    end();
}

static void check_actions(void)
{
    static const uint8_t ta[] = { TA_BEAT, TA_BLOCK, TA_TRANSACTION };
    static const uint8_t need[] = { 12, 3, 1 };

    console_puts("actions");
    for (unsigned int a = 0; a < 3U; a++) {
        unsigned int trig = 0;

        restart();
        dst_fill();
        ch_cfg(0, TRIGACT(ta[a]));
        set_desc(&desc[0], BYTE | BLOCKACT(BA_INT), 4, src, dst + 4,
                 &chain[0]);
        set_desc(&chain[0], BYTE | BLOCKACT(BA_INT), 4, src + 4, dst + 8,
                 &chain[1]);
        set_desc(&chain[1], BYTE | BLOCKACT(BA_INT), 4, src + 8, dst + 12, 0);
        ch_on(0);
        while ((DM->DMAC_CHCTRLA & CH_ENABLE) && trig < 20U) {
            swtrig(0);
            trig++;
            settle();
            if (DM->DMAC_CHINTFLAG & TCMPL) {
                if (trig == need[a] / 3U && a < 2U) {
                    /* the next descriptor is in the write-back copy */
                    put_checked("wbnext", wb[0].next, (uint32_t)&chain[1], 8);
                }
                DM->DMAC_CHINTFLAG = TCMPL;
            }
        }
        put_checked("triggers", trig, need[a], 2);
        put_checked("ok", copied(0, 12), 12, 2);
    }
    end();

    /* TCMPL: always at the end; a middle block only with BLOCKACT INT */
    console_puts("tcmpl");
    for (unsigned int ba = 0; ba < 2U; ba++) {
        restart();
        ch_cfg(0, TRIGACT(TA_BLOCK));
        set_desc(&desc[0], BYTE | BLOCKACT(ba), 4, src, dst + 4, &chain[0]);
        set_desc(&chain[0], BYTE | BLOCKACT(ba), 4, src, dst + 8, 0);
        ch_on(0);
        swtrig(0);
        settle();
        put_checked(ba ? "int_middle" : "noact_middle", chan(0),
                    ba ? 0x202U : 0x002U, 3);
        DM->DMAC_CHINTFLAG = 0x07U;
        swtrig(0);
        settle();
        put_checked(ba ? "int_last" : "noact_last", chan(0), 0x200, 3);
    }
    end();
}

static void check_suspend(void)
{
    console_puts("blockact_suspend");
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_TRANSACTION));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_SUSPEND), 4, src, dst + 4,
             &chain[0]);
    set_desc(&chain[0], BYTE | BLOCKACT(BA_SUSPEND), 4, src + 4, dst + 8, 0);
    ch_on(0);
    swtrig(0);
    settle();
    put_checked("middle", chan(0), 0x422, 3);       /* SUSP, BUSY, on */
    put_checked("ok", copied(0, 8), 4, 1);
    cmd(0, 2);
    settle();
    put_checked("cmd_reads", (DM->DMAC_CHCTRLB >> 24) & 3U, 0, 1);
    put_checked("last", chan(0), 0x422, 3);         /* suspended again */
    put_checked("ok", copied(0, 8), 8, 1);
    DM->DMAC_CHINTFLAG = SUSP;
    cmd(0, 2);                                      /* DESCADDR 0 */
    settle();
    put_checked("resume_last", chan(0), 0x562, 3);  /* TERR SUSP, FERR */
    end();

    console_puts("resume_first");                   /* skips a suspend */
    restart();
    ch_cfg(0, TRIGACT(TA_TRANSACTION));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_SUSPEND), 4, src, dst + 4,
             &chain[0]);
    set_desc(&chain[0], BYTE | BLOCKACT(BA_INT), 4, src + 4, dst + 8, 0);
    ch_on(0);
    cmd(0, 2);
    swtrig(0);
    settle();
    put_checked("chan", chan(0), 0x200, 3);
    end();

    console_puts("invalid");
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], (uint16_t)(BYTE & ~VALID) | BLOCKACT(BA_INT), 4, src,
             dst + 4, 0);
    ch_on(0);
    put_checked("at_enable", chan(0), 0x542, 3);    /* TERR SUSP, FERR, on */
    put_checked("wb_untouched", wb[0].btctrl, 0xCCCC, 4);
    swtrig(0);
    put_checked("pending", chan(0), 0x552, 3);
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_INT), 4, src, dst + 4, &chain[0]);
    set_desc(&chain[0], (uint16_t)(BYTE & ~VALID) | BLOCKACT(BA_INT), 4, src,
             dst + 8, 0);
    ch_on(0);
    swtrig(0);
    settle();
    DM->DMAC_CHINTFLAG = TCMPL;
    put_checked("second", chan(0), 0x542, 3);
    chain[0].btctrl |= VALID;
    wb[0].btctrl |= VALID;
    cmd(0, 2);                                      /* fetches it again */
    swtrig(0);
    settle();
    put_checked("fixed", chan(0), 0x700, 3);
    end();

    console_puts("bus_error");
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(2) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, (void *)0x30000000U, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    settle();
    put_checked("chan", chan(0), 0x100, 3);         /* TERR, disabled */
    put_checked("wbcnt", wb[0].btcnt, 4, 4);
    end();
}

static void check_paced(void)
{
    unsigned int moved = 0;

    console_puts("disable_mid");
    restart();
    dst_fill();
    tc0_start();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE | BLOCKACT(BA_INT), 32, src, dst + 4, 0);
    ch_on(1);
    delay_ms(9);
    DM->DMAC_CHCTRLA = 0U;
    for (uint32_t n = 0; (DM->DMAC_CHCTRLA & CH_ENABLE) && n < 100000U; n++) {
    }
    for (unsigned int i = 0; i < 32U; i++) {
        moved += dst[4 + i] != 0xA5;
    }
    put_range("moved", moved, 5, 10, 2);
    put_checked("wbcnt", wb[1].btcnt, 32U - moved, 4);
    put_checked("flags", DM->DMAC_CHINTFLAG, 0, 1);
    end();

    console_puts("dmaenable_off");          /* channels keep ENABLE */
    restart();
    tc0_stop();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE, 8, src, dst + 4, 0);
    ch_on(1);
    DM->DMAC_CTRL = LVLEN_ALL;
    put_checked("ctrl", DM->DMAC_CTRL, LVLEN_ALL, 4);
    DM->DMAC_CHID = 1U;
    put_checked("enable", DM->DMAC_CHCTRLA, CH_ENABLE, 2);
    end();

    console_puts("suspend_cmd");
    restart();
    dst_fill();
    tc0_start();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE | BLOCKACT(BA_INT), 32, src, dst + 4, 0);
    ch_on(1);
    delay_ms(9);
    cmd(1, 1);
    delay_ms(5);
    moved = 0;
    for (unsigned int i = 0; i < 32U; i++) {
        moved += dst[4 + i] != 0xA5;
    }
    put_checked("chan", chan(1), 0x412, 3);         /* SUSP, PEND, on */
    put_checked("wbcnt", wb[1].btcnt, 32U - moved, 4);
    put_checked("pendch", DM->DMAC_PENDCH, 2, 3);
    DM->DMAC_CHINTFLAG = SUSP;
    cmd(1, 2);
    delay_ms(40);
    put_checked("ok_after", copied(0, 32), 32, 2);
    put_checked("done", chan(1), 0x200, 3);
    end();

    console_puts("tc_trigger");
    restart();
    dst_fill();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE | BLOCKACT(BA_INT), 20, src, dst + 4, 0);
    ch_on(1);
    delay_ms(11);
    moved = 0;
    for (unsigned int i = 0; i < 20U; i++) {
        moved += dst[4 + i] != 0xA5;
    }
    put_range("10ms", moved, 7, 12, 2);
    delay_ms(20);
    put_checked("ok_30ms", copied(0, 20), 20, 2);
    tc0_stop();
    end();
}

static void check_arbitration(void)
{
    static const char *const nm[] = { "static", "round_robin", "a_higher",
                                      "b_level_off" };
    static const uint32_t first[] = { 0, 0, 0, 0 };

    console_puts("arbitration");
    for (unsigned int c = 0; c < 4U; c++) {
        uint32_t la = c == 2U ? 1U : 0U, lb = c == 3U ? 1U : 0U;
        unsigned int ok = 0;

        restart();
        if (c == 1U) {
            DM->DMAC_PRICTRL = 0x80U;
        }
        if (c == 3U) {
            DM->DMAC_CTRL = DMAENABLE | 0x0100U;
        }
        word = 0;
        for (unsigned int i = 0; i < 32U; i++) {
            src[128 + i] = (uint8_t)(i + 1U);
        }
        ch_cfg(0, TRIGACT(TA_TRANSACTION) | LVL(la));
        set_desc(&desc[0], VALID | BEATSIZE(2) | DSTINC, 32, (void *)&word,
                 (void *)log32, 0);
        ch_cfg(1, TRIGACT(TA_TRANSACTION) | LVL(lb));
        set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC, 32, src + 128,
                 (void *)&word, 0);
        ch_on(0);
        ch_on(1);
        settle();                           /* both fetched */
        DM->DMAC_SWTRIGCTRL = 3U;
        settle();
        /* round robin starts after LVLPRI (0): B, A, B, ... */
        for (unsigned int i = 0; i < 32U; i++) {
            ok += log32[i] == (c == 1U ? i + 1U : first[c]);
        }
        console_putc(' ');
        console_puts(nm[c]);
        put_checked("", ok, 32, 2);
        if (c == 1U) {
            put_checked("lvlpri", DM->DMAC_PRICTRL, 0x80, 8);
        }
        if (c == 3U) {
            put_checked("b_pending", DM->DMAC_PENDCH, 2, 3);
        }
    }
    end();
    for (unsigned int i = 0; i < 32U; i++) {
        src[128 + i] = (uint8_t)(128U + i);
    }
}

static uint32_t crc_io(uint32_t poly, uint32_t seed, uint32_t bsz,
                       const uint8_t *buf, unsigned int len, uint32_t *raw1)
{
    unsigned int step = 1U << bsz;

    DM->DMAC_CTRL = 0U;
    wait_ctrl_off();
    DM->DMAC_CRCCHKSUM = seed;
    DM->DMAC_CRCCTRL = (uint16_t)(bsz | (poly << 2) | (1U << 8));
    DM->DMAC_CTRL = CRCENABLE;
    for (unsigned int i = 0; i < len; i += step) {
        uint32_t v = 0;

        for (unsigned int k = 0; k < step; k++) {
            v |= (uint32_t)buf[i + k] << (8U * k);
        }
        DM->DMAC_CRCDATAIN = v;
        for (uint32_t n = 0; !(DM->DMAC_CRCSTATUS & 1U) && n < 1000U; n++) {
        }
        if (i == 0U && raw1) {
            *raw1 = DM->DMAC_CRCCHKSUM;     /* while busy: raw */
        }
        DM->DMAC_CRCSTATUS = 1U;
    }
    return DM->DMAC_CRCCHKSUM;
}

static void check_crc(void)
{
    static const uint8_t s9[] = "123456789";
    static uint8_t b16[16];
    uint32_t raw = 0;

    for (unsigned int i = 0; i < 16U; i++) {
        b16[i] = (uint8_t)i;
    }
    restart();
    console_puts("crc_io");
    put_checked("crc16", crc_io(0, 0, 0, s9, 9, 0), 0x31C3, 8);
    put_checked("crc16_ffff", crc_io(0, 0xFFFF, 0, s9, 9, 0), 0x29B1, 8);
    put_checked("crc32", crc_io(1, 0xFFFFFFFFU, 0, s9, 9, &raw), 0xCBF43926,
                8);
    put_checked("raw1", raw, 0x1208C43E, 8);
    put_checked("half", crc_io(1, 0xFFFFFFFFU, 1, b16, 16, 0), 0xCECEE288, 8);
    put_checked("word", crc_io(1, 0xFFFFFFFFU, 2, b16, 16, 0), 0xCECEE288, 8);
    DM->DMAC_CRCDATAIN = 0x55U;              /* busy: CRCENABLE stays */
    DM->DMAC_CTRL = 0U;
    put_checked("enable_kept", DM->DMAC_CTRL, CRCENABLE, 4);
    DM->DMAC_CRCSTATUS = 1U;
    end();

    console_puts("crc_channel");
    restart();
    DM->DMAC_CTRL = LVLEN_ALL;
    wait_ctrl_off();
    DM->DMAC_CRCCHKSUM = 0xFFFFFFFFU;
    DM->DMAC_CRCCTRL = (uint16_t)((1U << 2) | (0x22U << 8));
    DM->DMAC_CTRL = DMAENABLE | CRCENABLE | LVLEN_ALL;
    ch_cfg(2, TRIGACT(TA_BLOCK));
    set_desc(&desc[2], VALID | BEATSIZE(2) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, b16, dst + 4, 0);
    put_checked("idle", DM->DMAC_CRCSTATUS, 0, 1);
    ch_on(2);
    put_checked("busy", DM->DMAC_CRCSTATUS, 1, 1);
    swtrig(2);
    settle();
    put_checked("done", DM->DMAC_CRCSTATUS, 0, 1);
    put_checked("sum", DM->DMAC_CRCCHKSUM, 0xCECEE288, 8);
    end();
}

static void check_rules(void)
{
    console_puts("protect");
    restart();
    DM->DMAC_BASEADDR = 0x20001230U;
    put_checked("base", DM->DMAC_BASEADDR == (uint32_t)desc, 1, 1);
    DM->DMAC_CTRL = DMAENABLE | CRCENABLE | LVLEN_ALL;
    DM->DMAC_CRCCTRL = 0x0104U;
    put_checked("crcctrl", DM->DMAC_CRCCTRL, 0, 4);
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
    tc0_stop();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], BYTE, 8, src, dst + 4, 0);
    ch_on(1);
    DM->DMAC_CHCTRLB = TRIGSRC(5) | TRIGACT(TA_BLOCK) | EVACT(1) | LVL(2);
    put_checked("chctrlb", DM->DMAC_CHCTRLB,
                TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT) | LVL(2), 8);
    DM->DMAC_CHCTRLA = CH_ENABLE | CH_SWRST;
    put_checked("swrst_ignored", DM->DMAC_CHCTRLA, CH_ENABLE, 2);
    end();

    console_puts("swrst");
    DM->DMAC_CTRL = 0U;
    wait_ctrl_off();
    DM->DMAC_PRICTRL = 0x0101U;
    DM->DMAC_DBGCTRL = 1U;
    DM->DMAC_CHID = 3U;
    DM->DMAC_CHCTRLB = TRIGSRC(7);
    DM->DMAC_CTRL = SWRST;
    put_checked("pri", DM->DMAC_PRICTRL, 0, 8);
    put_checked("dbg_kept", DM->DMAC_DBGCTRL, 1, 2);
    put_checked("qos", DM->DMAC_QOSCTRL, 0x2A, 2);
    DM->DMAC_CHID = 3U;
    put_checked("ch3b", DM->DMAC_CHCTRLB, 0, 8);
    DM->DMAC_DBGCTRL = 0U;
    end();

    console_puts("intpend");
    restart();
    for (unsigned int c = 0; c < 2U; c++) {
        unsigned int ch = c ? 5U : 2U;

        ch_cfg(ch, TRIGACT(TA_BLOCK));
        set_desc(&desc[ch], BYTE | BLOCKACT(BA_INT), 4, src, dst + 4, 0);
        if (c) {
            DM->DMAC_CHINTENSET = TCMPL;
        }
        ch_on(ch);
        swtrig(ch);
        settle();
    }
    put_checked("ip", DM->DMAC_INTPEND, 0x0205, 4);  /* enabled flags only */
    put_checked("is", DM->DMAC_INTSTATUS, 0x020, 3);
    DM->DMAC_INTPEND = 5U | 0x0200U;
    put_checked("cleared", DM->DMAC_INTPEND, 0, 4);
    DM->DMAC_CHID = 5U;
    put_checked("ch5", DM->DMAC_CHINTFLAG, 0, 1);
    end();
}

void DMAC_Handler(void)
{
    uint16_t ip = DM->DMAC_INTPEND;
    uint8_t keep = DM->DMAC_CHID;

    if (isr_n < 4U) {
        isr_log[isr_n++] = ip;
    }
    DM->DMAC_CHID = (uint8_t)(ip & 0xFU);
    DM->DMAC_CHINTFLAG = DM->DMAC_CHINTFLAG;
    DM->DMAC_CHID = keep;
}

static void check_events(void)
{
    console_puts("event_out");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC1_Msk;
    TC1N.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (TC1N.TC_SYNCBUSY) {
    }
    TC1N.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    TC1N.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_ENABLE_Msk;
    while (TC1N.TC_SYNCBUSY) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_DMAC_CH_0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC1_EVU] = 1U;
    for (unsigned int sel = 1; sel <= 3U; sel += 2U) {
        restart();
        ch_cfg(0, TRIGACT(TA_TRANSACTION) | EVOE);
        set_desc(&desc[0], BYTE | EVOSEL(sel), 4, src, dst + 4, &chain[0]);
        set_desc(&chain[0], BYTE | EVOSEL(sel), 4, src, dst + 8, &chain[1]);
        set_desc(&chain[1], BYTE | EVOSEL(sel), 4, src, dst + 12, 0);
        TC1N.TC_COUNT = 0U;
        while (TC1N.TC_SYNCBUSY) {
        }
        ch_on(0);
        swtrig(0);
        settle();
        TC1N.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
        while (TC1N.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) {
        }
        put_checked(sel == 1U ? "block" : "beat", TC1N.TC_COUNT,
                    sel == 1U ? 3U : 12U, 4);
    }
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC1_EVU] = 0U;
    end();

    console_puts("event_in");
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 2U;
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BEAT) | EVIE | EVACT(1));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(0);
    for (unsigned int i = 0; i < 5U; i++) {
        EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
        settle();
    }
    put_checked("trig", copied(0, 8), 5, 1);
    /* silicon: a RESUME event left a software request suspended */
    restart();
    ch_cfg(0, TRIGACT(TA_BEAT) | EVIE | EVACT(5));
    set_desc(&desc[0], BYTE | BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(0);
    cmd(0, 1);
    swtrig(0);
    settle();
    put_checked("suspended", chan(0), 0x412, 3);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    settle();
    put_checked("not_resumed", chan(0), 0x412, 3);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 0U;
    end();

    console_puts("irq");
    restart();
    isr_n = 0;
    for (unsigned int c = 0; c < 2U; c++) {
        unsigned int ch = c ? 11U : 0U;

        ch_cfg(ch, TRIGACT(TA_BLOCK));
        set_desc(&desc[ch], BYTE | BLOCKACT(BA_INT), 4, src, dst + 4, 0);
        DM->DMAC_CHINTENSET = TCMPL;
        ch_on(ch);
    }
    __disable_irq();
    NVIC_ClearPendingIRQ(DMAC_IRQn);
    NVIC_EnableIRQ(DMAC_IRQn);
    DM->DMAC_SWTRIGCTRL = (1U << 11) | 1U;
    settle();
    __enable_irq();
    settle();
    NVIC_DisableIRQ(DMAC_IRQn);
    put_checked("n", isr_n, 2, 1);
    put_checked("first", isr_log[0], 0x0200, 4);
    put_checked("second", isr_log[1], 0x020B, 4);
    end();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dmac: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                           GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    for (unsigned int i = 0; i < sizeof(src); i++) {
        src[i] = (uint8_t)i;
    }
    check_registers();
    check_copies();
    check_writeback();
    check_actions();
    check_suspend();
    check_paced();
    check_arbitration();
    check_crc();
    check_rules();
    check_events();
    restart();
    DM->DMAC_CTRL = 0U;
    test_exit();
}
