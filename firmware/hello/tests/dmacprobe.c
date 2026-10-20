/*
 * DMAC probe (PIC32CM MC00, DMAC U2223): what the DMA controller does on
 * silicon, for the model. Bare EV15N46A, no wiring. Prints raw results,
 * one experiment per line or block (plan in
 * docs/research/peripherals/dmac-u2223.md, section 6):
 *
 *   E0  reset values, masks              E12 aborts
 *   E1  memory copies, beat sizes        E13 suspend command mid-transfer
 *   E2  STEPSEL/STEPSIZE                 E14 status while transferring
 *   E3  fixed addresses                  E15 TC0 overflow trigger
 *   E4  BTCNT = 0                        E16 SERCOM0 TX by DMA
 *   E5  write-back timing                E17 priority and arbitration
 *   E6  software trigger                 E18 CRC, I/O mode
 *   E7  trigger actions, linked list     E19 CRC on a channel
 *   E8  when TCMPL sets                  E20 enable protection
 *   E9  BLOCKACT SUSPEND, RESUME         E21 software resets
 *   E10 invalid descriptor               E22 CHID window, INTPEND
 *   E11 bus error                        E23 descriptor alignment
 *                                        E24 events in and out
 *                                        E25 the interrupt line
 *
 * The DMAC interrupt stays off (except E25) so flags stay visible.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define DM              DMAC_REGS
#define TCN             (TC0_REGS->COUNT16)
#define TC1N            (TC1_REGS->COUNT16)

/* CTRL */
#define SWRST           0x0001U
#define DMAENABLE       0x0002U
#define CRCENABLE       0x0004U
#define LVLEN_ALL       0x0F00U
/* CHCTRLA */
#define CH_SWRST        0x01U
#define CH_ENABLE       0x02U
/* CHCTRLB */
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
#define CMD_SUSPEND     1U
#define CMD_RESUME      2U
/* BTCTRL */
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
#define BA_BOTH         3U
/* CHINTFLAG */
#define TERR            0x01U
#define TCMPL           0x02U
#define SUSP            0x04U
/* triggers (Table 24-2) */
#define TRIG_SERCOM0_TX 0x03U
#define TRIG_TC0_OVF    0x15U

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
static desc_t odd[3] __attribute__((aligned(16)));
static uint8_t src[256] __attribute__((aligned(4)));
static uint8_t dst[256 + 8] __attribute__((aligned(4)));
static volatile uint32_t word;
static volatile uint32_t log32[32];
static volatile uint32_t isr_log[8];
static volatile unsigned int isr_n;

static void nl(void)
{
    console_puts("\r\n");
}

static void title(const char *s)
{
    console_puts(s);
}

static void wait_ctrl_off(void)
{
    for (uint32_t n = 0; (DM->DMAC_CTRL & (DMAENABLE | CRCENABLE)) &&
         n < 1000000U; n++) {
    }
}

static void wb_poison(void)
{
    volatile uint32_t *p = (volatile uint32_t *)wb;

    for (unsigned int i = 0; i < sizeof(wb) / 4U; i++) {
        p[i] = 0xCCCCCCCCU;
    }
}

static void dst_fill(void)
{
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
}

/* Disabled, reset, sections set, enabled with every level */
static void restart(void)
{
    DM->DMAC_CTRL = 0U;
    wait_ctrl_off();
    DM->DMAC_CTRL = SWRST;
    for (uint32_t n = 0; (DM->DMAC_CTRL & SWRST) && n < 100000U; n++) {
    }
    wb_poison();
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
}

static void ch_off(unsigned int ch)
{
    DM->DMAC_CHID = (uint8_t)ch;
    DM->DMAC_CHCTRLA = 0U;
    for (uint32_t n = 0; (DM->DMAC_CHCTRLA & CH_ENABLE) && n < 100000U; n++) {
    }
}

static void ch_cfg(unsigned int ch, uint32_t chctrlb)
{
    ch_off(ch);
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

/* @d covers @n beats of @bs (log2 bytes) from @s to @t (start addresses) */
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

static void wait_tcmpl(unsigned int ch)
{
    DM->DMAC_CHID = (uint8_t)ch;
    for (uint32_t n = 0; !(DM->DMAC_CHINTFLAG & (TCMPL | TERR)) &&
         (DM->DMAC_CHCTRLA & CH_ENABLE) && n < 200000U; n++) {
    }
}

static void delay_quiet(void)
{
    for (volatile uint32_t i = 0; i < 2000U; i++) {
    }
}

static void put_wb(unsigned int ch)
{
    put_field("wb.btctrl", wb[ch].btctrl, 4);
    put_field("btcnt", wb[ch].btcnt, 4);
    put_field("src", wb[ch].src - (uint32_t)src, 8);
    put_field("dst", wb[ch].dst - (uint32_t)dst, 8);
    put_field("next", wb[ch].next, 8);
}

static void dump(unsigned int ch)
{
    DM->DMAC_CHID = (uint8_t)ch;
    put_field("a", DM->DMAC_CHCTRLA, 2);
    put_field("f", DM->DMAC_CHINTFLAG, 1);
    put_field("st", DM->DMAC_CHSTATUS, 1);
    put_field("busy", DM->DMAC_BUSYCH, 3);
    put_field("pend", DM->DMAC_PENDCH, 3);
    put_field("ip", DM->DMAC_INTPEND, 4);
    put_field("is", DM->DMAC_INTSTATUS, 3);
    put_field("act", DM->DMAC_ACTIVE, 8);
}

static unsigned int dst_count(uint8_t first, unsigned int n)
{
    unsigned int ok = 0;

    for (unsigned int i = 0; i < n; i++) {
        ok += dst[4 + i] == (uint8_t)(first + i);
    }
    return ok;
}

/* TC0 overflow at 1 kHz (48 MHz / 64 / 750) */
static void tc0_start(void)
{
    TCN.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (TCN.TC_SYNCBUSY) {
    }
    TCN.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64;
    TCN.TC_WAVE = TC_WAVE_WAVEGEN_MFRQ;
    TCN.TC_CC[0] = 749U;
    while (TCN.TC_SYNCBUSY) {
    }
    TCN.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while (TCN.TC_SYNCBUSY) {
    }
}

static void tc0_stop(void)
{
    TCN.TC_CTRLA = 0U;
    while (TCN.TC_SYNCBUSY) {
    }
}

static void e0_reset(void)
{
    title("E0 reset");
    put_field("ctrl", DM->DMAC_CTRL, 4);
    put_field("crcctrl", DM->DMAC_CRCCTRL, 4);
    put_field("qos", DM->DMAC_QOSCTRL, 2);
    put_field("prictrl", DM->DMAC_PRICTRL, 8);
    put_field("base", DM->DMAC_BASEADDR, 8);
    put_field("chid", DM->DMAC_CHID, 2);
    put_field("chctrlb", DM->DMAC_CHCTRLB, 8);
    put_field("chstatus", DM->DMAC_CHSTATUS, 2);
    nl();
    title("E0 masks");
    DM->DMAC_CRCCTRL = 0xFFFFU;
    put_field("crcctrl", DM->DMAC_CRCCTRL, 4);
    DM->DMAC_CRCCTRL = 0U;
    DM->DMAC_PRICTRL = 0xFFFFFFFFU;
    put_field("prictrl", DM->DMAC_PRICTRL, 8);
    DM->DMAC_PRICTRL = 0U;
    DM->DMAC_QOSCTRL = 0xFFU;
    put_field("qos", DM->DMAC_QOSCTRL, 2);
    DM->DMAC_QOSCTRL = 0x2AU;
    DM->DMAC_DBGCTRL = 0xFFU;
    put_field("dbg", DM->DMAC_DBGCTRL, 2);
    DM->DMAC_DBGCTRL = 0U;
    DM->DMAC_BASEADDR = 0xFFFFFFFFU;
    put_field("base", DM->DMAC_BASEADDR, 8);
    DM->DMAC_WRBADDR = 0xFFFFFFFFU;
    put_field("wrb", DM->DMAC_WRBADDR, 8);
    DM->DMAC_CHID = 0xFFU;
    put_field("chid", DM->DMAC_CHID, 2);
    DM->DMAC_CHID = 0U;
    DM->DMAC_CHCTRLB = 0x00FFFFFFU;          /* CMD 0 */
    put_field("chctrlb", DM->DMAC_CHCTRLB, 8);
    DM->DMAC_CHCTRLB = 0U;
    DM->DMAC_CHCTRLA = 0x40U;
    put_field("chctrla", DM->DMAC_CHCTRLA, 2);
    DM->DMAC_CHCTRLA = 0U;
    DM->DMAC_SWTRIGCTRL = 0xFFFFFFFFU;
    put_field("swtrig_dis", DM->DMAC_SWTRIGCTRL, 8);
    put_field("pend_dis", DM->DMAC_PENDCH, 3);
    DM->DMAC_SWTRIGCTRL = 0U;
    nl();
    title("E0 chid");
    for (unsigned int c = 0; c < 16U; c++) {
        DM->DMAC_CHID = (uint8_t)c;
        DM->DMAC_CHCTRLB = TRIGSRC(c);
    }
    for (unsigned int c = 0; c < 16U; c++) {
        DM->DMAC_CHID = (uint8_t)c;
        put_field("b", DM->DMAC_CHCTRLB >> 8, 2);
    }
    for (unsigned int c = 0; c < 16U; c++) {
        DM->DMAC_CHID = (uint8_t)c;
        DM->DMAC_CHCTRLB = 0U;
    }
    nl();
}

static void e1_copy(void)
{
    uint32_t t0, t;

    for (unsigned int bs = 0; bs < 3U; bs++) {
        restart();
        dst_fill();
        ch_cfg(0, TRIGACT(TA_BLOCK));
        set_desc(&desc[0], VALID | BEATSIZE(bs) | SRCINC | DSTINC |
                 BLOCKACT(BA_NOACT), (uint16_t)(64U >> bs), src, dst + 4,
                 0);
        ch_on(0);
        swtrig(0);
        put_field("E1 bs", bs, 1);
        put_field("swtrig", DM->DMAC_SWTRIGCTRL, 3);
        wait_tcmpl(0);
        delay_quiet();
        put_field("ok", dst_count(0, 64), 2);
        put_field("guard", dst[3] == 0xA5 && dst[68] == 0xA5, 1);
        dump(0);
        put_wb(0);
        nl();
    }
    /* 1 KB word copy: cycles */
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(2) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 64, src, dst, 0);
    ch_on(0);
    SysTick->VAL = 0U;
    t0 = SysTick->VAL;
    swtrig(0);
    while (!(DM->DMAC_CHINTFLAG & TCMPL)) {
    }
    t = SysTick->VAL;
    put_field("E1 256B_word_cycles", t0 >= t ? t0 - t : t0 + 48000U - t, 6);
    nl();
}

static void e2_steps(void)
{
    static const uint16_t cfg[] = {
        SRCINC | STEPSEL_SRC | STEPSIZE(1), SRCINC | STEPSEL_SRC | STEPSIZE(2),
        DSTINC | STEPSIZE(1), SRCINC | DSTINC | STEPSIZE(2),
    };

    for (unsigned int i = 0; i < 4U; i++) {
        for (unsigned int bs = 0; bs < 3U; bs += 2U) {
            restart();
            dst_fill();
            ch_cfg(0, TRIGACT(TA_BLOCK));
            desc[0].btctrl = VALID | BEATSIZE(bs) | cfg[i];
            desc[0].btcnt = 4;
            /* end addresses with the step on the selected side */
            desc[0].src = (uint32_t)src + ((cfg[i] & SRCINC) ?
                          (4U << bs) << ((cfg[i] & STEPSEL_SRC) ?
                                         ((cfg[i] >> 13) & 7U) : 0U) : 0U);
            desc[0].dst = (uint32_t)(dst + 4) + ((cfg[i] & DSTINC) ?
                          (4U << bs) << ((cfg[i] & STEPSEL_SRC) ? 0U :
                                         ((cfg[i] >> 13) & 7U)) : 0U);
            desc[0].next = 0U;
            ch_on(0);
            swtrig(0);
            wait_tcmpl(0);
            delay_quiet();
            title("E2 cfg");
            put_field("i", i, 1);
            put_field("bs", bs, 1);
            put_field("f", DM->DMAC_CHINTFLAG, 1);
            console_puts(" d=");
            for (unsigned int k = 0; k < 40U; k++) {
                console_put_hex(dst[4 + k], 2);
            }
            nl();
        }
    }
}

static void e3_e4(void)
{
    uint32_t t0, t;

    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | DSTINC, 8, src + 7, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    wait_tcmpl(0);
    delay_quiet();
    title("E3 fixed_src");
    console_puts(" d=");
    for (unsigned int k = 0; k < 10U; k++) {
        console_put_hex(dst[3 + k], 2);
    }
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    word = 0;
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC, 8, src + 10, (void *)&word,
             0);
    ch_on(0);
    swtrig(0);
    wait_tcmpl(0);
    delay_quiet();
    put_field("fixed_dst", word, 8);
    nl();

    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | BLOCKACT(BA_INT), 0,
             (void *)&word, (void *)&word, 0);
    ch_on(0);
    SysTick->VAL = 0U;
    t0 = SysTick->VAL;
    {
        uint32_t ms = 0;

        (void)SysTick->CTRL;
        swtrig(0);
        while (!(DM->DMAC_CHINTFLAG & (TCMPL | TERR)) && ms < 200U) {
            if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
                ms++;
            }
        }
        t = SysTick->VAL;
        title("E4 btcnt0");
        put_field("ms", ms, 4);
        put_field("ticks", t0 >= t ? t0 - t : t0 + 48000U - t, 6);
    }
    dump(0);
    put_wb(0);
    nl();
}

static void e5_wb(void)
{
    restart();
    tc0_stop();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(1);
    delay_quiet();
    title("E5a enabled");
    put_wb(1);
    dump(1);
    nl();
    ch_cfg(2, TRIGACT(TA_BEAT));
    set_desc(&desc[2], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(2);
    for (unsigned int i = 0; i < 3U; i++) {
        swtrig(2);
        delay_quiet();
    }
    title("E5b 3beats");
    put_wb(2);
    dump(2);
    nl();
    ch_cfg(3, TRIGACT(TA_BLOCK));
    set_desc(&desc[3], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 20, 0);
    ch_on(3);
    swtrig(3);
    wait_tcmpl(3);
    delay_quiet();
    title("E5c other_ran");
    put_wb(2);
    nl();
    for (unsigned int i = 0; i < 5U; i++) {
        swtrig(2);
        delay_quiet();
    }
    title("E5d done");
    put_wb(2);
    dump(2);
    nl();
}

static void e6_swtrig(void)
{
    restart();
    tc0_stop();
    ch_cfg(0, TRIGACT(TA_BEAT));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    DM->DMAC_SWTRIGCTRL = 1U;               /* disabled channel */
    title("E6a disabled");
    put_field("swtrig", DM->DMAC_SWTRIGCTRL, 3);
    put_field("pend", DM->DMAC_PENDCH, 3);
    ch_on(0);
    delay_quiet();
    put_field("after_enable_btcnt", wb[0].btcnt, 4);
    dump(0);
    nl();
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BEAT));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(0);
    DM->DMAC_SWTRIGCTRL = 1U;
    title("E6b beat");
    put_field("swtrig", DM->DMAC_SWTRIGCTRL, 3);
    delay_quiet();
    put_field("moved", dst_count(0, 8), 1);
    nl();
    /* (c) back-to-back on a TC-paced beat channel 1 (one beat per ms) */
    restart();
    dst_fill();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 16, src, dst + 4, 0);
    ch_on(1);
    DM->DMAC_SWTRIGCTRL = 2U;
    DM->DMAC_SWTRIGCTRL = 2U;
    DM->DMAC_SWTRIGCTRL = 2U;
    title("E6c 3x");
    put_field("swtrig", DM->DMAC_SWTRIGCTRL, 3);
    delay_quiet();
    put_field("moved", dst_count(0, 16), 2);
    dump(1);
    nl();
}

static void e7_actions(void)
{
    static const uint8_t ta[] = { TA_BEAT, TA_BLOCK, TA_TRANSACTION };
    static const char *const nm[] = { "beat", "block", "transaction" };

    for (unsigned int a = 0; a < 3U; a++) {
        unsigned int trig = 0;

        restart();
        dst_fill();
        ch_cfg(0, TRIGACT(ta[a]));
        set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src, dst + 4, &chain[0]);
        set_desc(&chain[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src + 4, dst + 8, &chain[1]);
        set_desc(&chain[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src + 8, dst + 12, 0);
        ch_on(0);
        title("E7 ");
        title(nm[a]);
        while ((DM->DMAC_CHCTRLA & CH_ENABLE) && trig < 20U) {
            swtrig(0);
            trig++;
            delay_quiet();
            if (DM->DMAC_CHINTFLAG & TCMPL) {
                put_field("tcmpl_at", trig, 2);
                put_field("wbnext", wb[0].next ? (wb[0].next ==
                          (uint32_t)&chain[0] ? 1U : 2U) : 0U, 1);
                DM->DMAC_CHINTFLAG = TCMPL;
            }
        }
        put_field("triggers", trig, 2);
        put_field("ok", dst_count(0, 12), 2);
        nl();
    }
}

static void e8_tcmpl(void)
{
    title("E8 tcmpl");
    for (unsigned int ba = 0; ba < 2U; ba++) {
        for (unsigned int ie = 0; ie < 2U; ie++) {
            for (unsigned int two = 0; two < 2U; two++) {
                restart();
                ch_cfg(0, TRIGACT(TA_BLOCK));
                if (ie) {
                    DM->DMAC_CHINTENSET = TCMPL;
                }
                set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                         BLOCKACT(ba), 4, src, dst + 4,
                         two ? &chain[0] : 0);
                set_desc(&chain[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                         BLOCKACT(ba), 4, src, dst + 8, 0);
                ch_on(0);
                swtrig(0);
                delay_quiet();
                /* after the first block (of two) or the only one */
                console_puts(" ");
                console_put_hex(ba, 1);
                console_put_hex(ie, 1);
                console_put_hex(two, 1);
                console_puts(":");
                console_put_hex(DM->DMAC_CHINTFLAG, 1);
                console_put_hex(DM->DMAC_CHCTRLA, 1);
                console_put_hex(DM->DMAC_INTSTATUS & 1U, 1);
                if (two) {
                    swtrig(0);
                    delay_quiet();
                    console_puts("/");
                    console_put_hex(DM->DMAC_CHINTFLAG, 1);
                    console_put_hex(DM->DMAC_CHCTRLA, 1);
                }
                DM->DMAC_CHINTENCLR = 0x07U;
            }
        }
    }
    nl();
}

static void e9_suspend(void)
{
    /* middle block suspends */
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_TRANSACTION));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_SUSPEND), 4, src, dst + 4, &chain[0]);
    set_desc(&chain[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_SUSPEND), 4, src + 4, dst + 8, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    title("E9 middle");
    dump(0);
    put_field("ok", dst_count(0, 8), 1);
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_RESUME);
    delay_quiet();
    put_field("cmd_after", (DM->DMAC_CHCTRLB >> 24) & 3U, 1);
    title(" | resumed");
    dump(0);
    put_field("ok", dst_count(0, 8), 1);
    DM->DMAC_CHINTFLAG = SUSP;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_RESUME);
    delay_quiet();
    title(" | resume_last");
    dump(0);
    nl();
    /* RESUME before a suspending block: skip */
    restart();
    ch_cfg(0, TRIGACT(TA_TRANSACTION));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_SUSPEND), 4, src, dst + 4, &chain[0]);
    set_desc(&chain[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src + 4, dst + 8, 0);
    ch_on(0);
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_RESUME);
    swtrig(0);
    delay_quiet();
    title("E9 resume_first");
    dump(0);
    nl();
}

static void e10_invalid(void)
{
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], BEATSIZE(0) | SRCINC | DSTINC | BLOCKACT(BA_INT), 4,
             src, dst + 4, 0);                /* VALID = 0 */
    ch_on(0);
    delay_quiet();
    title("E10a enabled");
    dump(0);
    swtrig(0);
    delay_quiet();
    title(" | triggered");
    dump(0);
    put_wb(0);
    nl();
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src, dst + 4, &chain[0]);
    set_desc(&chain[0], BEATSIZE(0) | SRCINC | DSTINC | BLOCKACT(BA_INT), 4,
             src, dst + 8, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    DM->DMAC_CHINTFLAG = TCMPL;
    title("E10b second_invalid");
    dump(0);
    put_wb(0);
    chain[0].btctrl |= VALID;
    wb[0].btctrl |= VALID;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_RESUME);
    swtrig(0);
    delay_quiet();
    title(" | fixed_resumed");
    dump(0);
    nl();
}

static void e11_buserr(void)
{
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(2) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, (void *)0x30000000U, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    title("E11 src_unmapped");
    dump(0);
    put_field("wb.btcnt", wb[0].btcnt, 4);
    put_field("wb.src", wb[0].src, 8);
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(2) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src, (void *)0x30000000U, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    title(" | dst_unmapped");
    dump(0);
    put_field("wb.btcnt", wb[0].btcnt, 4);
    nl();
}

static void e12_abort(void)
{
    uint32_t a1, a2;

    restart();
    tc0_start();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 32, src, dst + 4, 0);
    ch_on(1);
    delay_ms(8);
    DM->DMAC_CHCTRLA = 0U;
    a1 = DM->DMAC_CHCTRLA;
    delay_quiet();
    a2 = DM->DMAC_CHCTRLA;
    title("E12a disable_mid");
    put_field("enable_now", a1, 2);
    put_field("enable_later", a2, 2);
    dump(1);
    put_wb(1);
    nl();
    restart();
    tc0_stop();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC, 8, src, dst + 4,
             0);
    ch_on(1);
    ch_cfg(2, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[2], VALID | BEATSIZE(0) | SRCINC | DSTINC, 8, src, dst + 4,
             0);
    ch_on(2);
    DM->DMAC_CTRL = LVLEN_ALL;              /* DMAENABLE off */
    delay_quiet();
    title("E12b dmaenable_off");
    put_field("ctrl", DM->DMAC_CTRL, 4);
    DM->DMAC_CHID = 1;
    put_field("ch1", DM->DMAC_CHCTRLA, 2);
    DM->DMAC_CHID = 2;
    put_field("ch2", DM->DMAC_CHCTRLA, 2);
    nl();
}

static void e13_suspend_cmd(void)
{
    restart();
    dst_fill();
    tc0_start();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 32, src, dst + 4, 0);
    ch_on(1);
    delay_ms(8);
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_SUSPEND);
    delay_quiet();
    title("E13 suspended");
    put_field("cmd", (DM->DMAC_CHCTRLB >> 24) & 3U, 1);
    dump(1);
    put_field("wb.btcnt", wb[1].btcnt, 4);
    delay_ms(10);
    put_field("pend_later", DM->DMAC_PENDCH, 3);
    {
        unsigned int before = 0;

        for (unsigned int i = 0; i < 32U; i++) {
            before += dst[4 + i] != 0xA5;
        }
        put_field("moved", before, 2);
    }
    DM->DMAC_CHINTFLAG = SUSP;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_RESUME);
    delay_ms(40);
    put_field("ok_after", dst_count(0, 32), 2);
    dump(1);
    tc0_stop();
    nl();
}

/* E14: snapshots during a SERCOM0-TX-paced transfer */
static void e14_snap(uint32_t trigact, const char *nm)
{
    static const char msg[] = "0123456789abcdefghijklmnopqrstuv";
    static uint32_t snap[64];
    unsigned int n = 0;

    console_flush();
    restart();
    ch_cfg(0, TRIGSRC(TRIG_SERCOM0_TX) | TRIGACT(trigact));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | BLOCKACT(BA_INT), 32,
             msg, (void *)&SERCOM0_REGS->USART_INT.SERCOM_DATA, 0);
    DM->DMAC_CHID = 0;
    ch_on(0);
    while (n < 64U) {
        uint32_t v = DM->DMAC_CHSTATUS | (DM->DMAC_BUSYCH & 1U) << 4 |
                     (DM->DMAC_PENDCH & 1U) << 5 |
                     (DM->DMAC_ACTIVE & 0x8000U ? 1U : 0U) << 6;

        if (n == 0U || v != (snap[n - 1] & 0xFFU)) {
            snap[n++] = v;
        }
        if (!(DM->DMAC_CHCTRLA & CH_ENABLE)) {
            snap[n++] = 0x100U | v;
            break;
        }
    }
    console_flush();
    nl();
    title("E14 ");
    title(nm);
    put_field("transitions", n, 2);
    console_puts(" s=");
    for (unsigned int i = 0; i < n && i < 40U; i++) {
        console_put_hex(snap[i], 3);
        console_putc(' ');
    }
    nl();
}

static void e15_tc(void)
{
    restart();
    dst_fill();
    tc0_start();
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 20, src, dst + 4, 0);
    ch_on(1);
    delay_ms(10);
    title("E15 beat_10ms");
    {
        unsigned int moved = 0;

        for (unsigned int i = 0; i < 20U; i++) {
            moved += dst[4 + i] != 0xA5;
        }
        put_field("moved", moved, 2);
    }
    put_field("tc_intflag", TCN.TC_INTFLAG, 2);
    delay_ms(20);
    put_field("ok_30ms", dst_count(0, 20), 2);
    restart();
    dst_fill();
    ch_cfg(1, TRIGSRC(0x1BU) | TRIGACT(TA_BEAT));   /* dmac.h's TC0_OVF */
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src, dst + 4, 0);
    ch_on(1);
    delay_ms(10);
    put_field("trigsrc_1b_moved", dst_count(0, 4), 1);
    tc0_stop();
    nl();
}

static void e16_sercom(void)
{
    static const char a[] = "\r\nE16 beat: the quick brown fox jumps\r\n";
    static const char b[] = "\r\nE16 block: the quick brown fox jumps\r\n";

    for (unsigned int i = 0; i < 2U; i++) {
        const char *m = i ? b : a;

        console_flush();
        restart();
        ch_cfg(0, TRIGSRC(TRIG_SERCOM0_TX) | TRIGACT(i ? TA_BLOCK : TA_BEAT));
        set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | BLOCKACT(BA_INT),
                 sizeof(a) - 1U, m,
                 (void *)&SERCOM0_REGS->USART_INT.SERCOM_DATA, 0);
        ch_on(0);
        wait_tcmpl(0);
        delay_ms(10);
    }
    title("E16 done");
    nl();
}

static void e17_arb(void)
{
    static const char *const nm[] = { "static", "rr", "b_higher", "a_higher",
                                      "b_lvl_off" };

    for (unsigned int c = 0; c < 5U; c++) {
        uint32_t la = 0, lb = 0;

        restart();
        if (c == 1U) {
            DM->DMAC_PRICTRL = 0x80U;               /* RRLVLEN0 */
        }
        if (c == 2U) {
            lb = 1;
        }
        if (c == 3U) {
            la = 1;
        }
        if (c == 4U) {
            lb = 1;
            DM->DMAC_CTRL = DMAENABLE | 0x0100U;    /* LVLEN0 only */
        }
        word = 0;
        for (unsigned int i = 0; i < 32U; i++) {
            log32[i] = 0xEEU;
        }
        for (unsigned int i = 0; i < 32U; i++) {
            src[128 + i] = (uint8_t)(i + 1U);
        }
        /* A = channel 0 samples word; B = channel 1 writes 1..32 */
        ch_cfg(0, TRIGACT(TA_TRANSACTION) | LVL(la));
        set_desc(&desc[0], VALID | BEATSIZE(2) | DSTINC, 32, (void *)&word,
                 (void *)log32, 0);
        ch_cfg(1, TRIGACT(TA_TRANSACTION) | LVL(lb));
        set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC, 32, src + 128,
                 (void *)&word, 0);
        ch_on(0);
        ch_on(1);
        DM->DMAC_SWTRIGCTRL = 3U;
        delay_quiet();
        title("E17 ");
        title(nm[c]);
        console_puts(" log=");
        for (unsigned int i = 0; i < 32U; i++) {
            console_put_hex(log32[i], 2);
        }
        put_field("pri", DM->DMAC_PRICTRL, 8);
        put_field("pend", DM->DMAC_PENDCH, 3);
        put_field("busy", DM->DMAC_BUSYCH, 3);
        nl();
    }
}

static void crc_io(uint32_t poly, uint32_t seed, uint32_t bsz,
                   const uint8_t *buf, unsigned int len, const char *nm)
{
    uint32_t st1 = 0, raw1 = 0;
    unsigned int step = 1U << bsz;

    DM->DMAC_CTRL &= (uint16_t)~CRCENABLE;
    wait_ctrl_off();
    DM->DMAC_CTRL = 0U;
    DM->DMAC_CRCCHKSUM = seed;
    DM->DMAC_CRCCTRL = (uint16_t)(bsz | (poly << 2) | (1U << 8));
    DM->DMAC_CTRL = CRCENABLE;
    title("E18 ");
    title(nm);
    put_field("st0", DM->DMAC_CRCSTATUS, 1);
    for (unsigned int i = 0; i < len; i += step) {
        uint32_t v = 0;

        for (unsigned int k = 0; k < step; k++) {
            v |= (uint32_t)buf[i + k] << (8U * k);
        }
        DM->DMAC_CRCDATAIN = v;
        for (uint32_t n = 0; !(DM->DMAC_CRCSTATUS & 1U) && n < 1000U; n++) {
        }
        if (i == 0U) {
            st1 = DM->DMAC_CRCSTATUS;
            raw1 = DM->DMAC_CRCCHKSUM;
        }
        DM->DMAC_CRCSTATUS = 1U;
    }
    put_field("st1", st1, 1);
    put_field("raw1", raw1, 8);
    put_field("sum", DM->DMAC_CRCCHKSUM, 8);
    put_field("st", DM->DMAC_CRCSTATUS, 1);
    nl();
}

static void e18_crc(void)
{
    static const uint8_t s9[] = "123456789";
    static uint8_t b16[16];

    for (unsigned int i = 0; i < 16U; i++) {
        b16[i] = (uint8_t)i;
    }
    restart();
    DM->DMAC_CTRL = 0U;
    crc_io(0, 0, 0, s9, 9, "crc16_s0_123456789");
    crc_io(0, 0xFFFF, 0, s9, 9, "crc16_sffff_123456789");
    crc_io(1, 0xFFFFFFFFU, 0, s9, 9, "crc32_123456789");
    crc_io(1, 0xFFFFFFFFU, 0, b16, 16, "crc32_b16_byte");
    crc_io(1, 0xFFFFFFFFU, 1, b16, 16, "crc32_b16_half");
    crc_io(1, 0xFFFFFFFFU, 2, b16, 16, "crc32_b16_word");
    crc_io(0, 0xFFFF, 2, b16, 16, "crc16_b16_word");
    /* CRCENABLE cleared while CRCBUSY = 1 */
    DM->DMAC_CRCDATAIN = 0x55U;
    DM->DMAC_CTRL = 0U;
    title("E18 crcenable_busy");
    put_field("ctrl", DM->DMAC_CTRL, 4);
    DM->DMAC_CRCSTATUS = 1U;
    put_field("ctrl_after_w1c", DM->DMAC_CTRL, 4);
    nl();
}

static void e19_crc_ch(void)
{
    static uint8_t b16[16];

    for (unsigned int i = 0; i < 16U; i++) {
        b16[i] = (uint8_t)i;
    }
    for (unsigned int bs = 0; bs < 3U; bs += 2U) {
        restart();
        DM->DMAC_CTRL = LVLEN_ALL;
        wait_ctrl_off();
        DM->DMAC_CRCCHKSUM = 0xFFFFFFFFU;
        DM->DMAC_CRCCTRL = (uint16_t)((1U << 2) | (0x22U << 8));
        DM->DMAC_CTRL = DMAENABLE | CRCENABLE | LVLEN_ALL;
        ch_cfg(2, TRIGACT(TA_BLOCK));
        set_desc(&desc[2], VALID | BEATSIZE(bs) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), (uint16_t)(16U >> bs), b16, dst + 4, 0);
        title("E19 ch2");
        put_field("bs", bs, 1);
        put_field("st_before", DM->DMAC_CRCSTATUS, 1);
        ch_on(2);
        put_field("st_enabled", DM->DMAC_CRCSTATUS, 1);
        swtrig(2);
        wait_tcmpl(2);
        delay_quiet();
        put_field("st_done", DM->DMAC_CRCSTATUS, 1);
        put_field("sum", DM->DMAC_CRCCHKSUM, 8);
        put_field("a", DM->DMAC_CHCTRLA, 2);
        nl();
    }
}

static void e20_protect(void)
{
    restart();
    title("E20 protect");
    DM->DMAC_BASEADDR = 0x20001230U;
    put_field("base_on", DM->DMAC_BASEADDR == (uint32_t)desc, 1);
    DM->DMAC_CTRL = DMAENABLE | CRCENABLE | LVLEN_ALL;
    DM->DMAC_CRCCTRL = 0x0104U;
    put_field("crcctrl_on", DM->DMAC_CRCCTRL, 4);
    DM->DMAC_CRCCHKSUM = 0x1234U;
    put_field("chksum_on", DM->DMAC_CRCCHKSUM, 8);
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
    ch_cfg(1, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT));
    set_desc(&desc[1], VALID | BEATSIZE(0) | SRCINC | DSTINC, 8, src, dst + 4,
             0);
    tc0_stop();
    ch_on(1);
    DM->DMAC_CHCTRLB = TRIGSRC(5) | TRIGACT(TA_BLOCK) | EVACT(1) | LVL(2);
    put_field("chctrlb_on", DM->DMAC_CHCTRLB, 8);
    DM->DMAC_CHCTRLA = CH_ENABLE | CH_SWRST;
    put_field("chctrla_swrst_on", DM->DMAC_CHCTRLA, 2);
    put_field("chctrlb_after", DM->DMAC_CHCTRLB, 8);
    nl();
}

static void e21_swrst(void)
{
    restart();
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    DM->DMAC_CHID = 3;
    DM->DMAC_CHCTRLB = TRIGSRC(7) | LVL(1);
    DM->DMAC_CHINTENSET = TCMPL;
    DM->DMAC_CTRL = 0U;
    wait_ctrl_off();
    DM->DMAC_PRICTRL = 0x0101U;
    DM->DMAC_QOSCTRL = 0x15U;
    DM->DMAC_DBGCTRL = 1U;
    DM->DMAC_CRCCTRL = 0x0104U;
    DM->DMAC_CTRL = SWRST;
    title("E21 swrst");
    put_field("ctrl_now", DM->DMAC_CTRL, 4);
    for (uint32_t n = 0; (DM->DMAC_CTRL & SWRST) && n < 100000U; n++) {
    }
    put_field("pri", DM->DMAC_PRICTRL, 8);
    put_field("qos", DM->DMAC_QOSCTRL, 2);
    put_field("dbg", DM->DMAC_DBGCTRL, 2);
    put_field("crcctrl", DM->DMAC_CRCCTRL, 4);
    put_field("base", DM->DMAC_BASEADDR, 8);
    DM->DMAC_CHID = 3;
    put_field("ch3b", DM->DMAC_CHCTRLB, 8);
    put_field("ch3inten", DM->DMAC_CHINTENSET, 1);
    DM->DMAC_CHID = 0;
    put_field("ch0flag", DM->DMAC_CHINTFLAG, 1);
    put_field("chid", DM->DMAC_CHID, 1);
    nl();
    /* channel SWRST */
    restart();
    DM->DMAC_CHID = 4;
    DM->DMAC_CHCTRLB = TRIGSRC(9) | LVL(3);
    DM->DMAC_CHINTENSET = SUSP;
    DM->DMAC_CHCTRLA = CH_SWRST;
    title("E21 ch_swrst");
    put_field("a_now", DM->DMAC_CHCTRLA, 2);
    delay_quiet();
    put_field("a", DM->DMAC_CHCTRLA, 2);
    put_field("b", DM->DMAC_CHCTRLB, 8);
    put_field("inten", DM->DMAC_CHINTENSET, 1);
    nl();
}

static void e22_intpend(void)
{
    restart();
    for (unsigned int c = 0; c < 2U; c++) {
        unsigned int ch = c ? 5U : 2U;

        ch_cfg(ch, TRIGACT(TA_BLOCK));
        set_desc(&desc[ch], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src, dst + 4, 0);
        if (c) {
            DM->DMAC_CHINTENSET = TCMPL;
        }
        ch_on(ch);
        swtrig(ch);
        delay_quiet();
    }
    title("E22 intpend");
    put_field("ip", DM->DMAC_INTPEND, 4);
    put_field("is", DM->DMAC_INTSTATUS, 3);
    DM->DMAC_INTPEND = 5U;
    put_field("ip_w5", DM->DMAC_INTPEND, 4);
    DM->DMAC_INTPEND = 5U | 0x0200U;          /* TCMPL into channel 5 */
    put_field("ip_clr5", DM->DMAC_INTPEND, 4);
    DM->DMAC_CHID = 5;
    put_field("ch5f", DM->DMAC_CHINTFLAG, 1);
    DM->DMAC_INTPEND = 7U;
    put_field("ip_w7", DM->DMAC_INTPEND, 4);
    nl();
    title("E22 chid12_15");
    for (unsigned int c = 12; c < 16U; c++) {
        DM->DMAC_CHID = (uint8_t)c;
        DM->DMAC_CHCTRLB = LVL(3);
        put_field("b", DM->DMAC_CHCTRLB, 8);
        put_field("chid", DM->DMAC_CHID, 2);
    }
    DM->DMAC_CHID = 0;
    put_field("ch0b", DM->DMAC_CHCTRLB, 8);
    nl();
}

static void e23_align(void)
{
    desc_t *d8 = (desc_t *)((uint8_t *)odd + 8);

    restart();
    dst_fill();
    DM->DMAC_CTRL = LVLEN_ALL;
    wait_ctrl_off();
    DM->DMAC_BASEADDR = (uint32_t)d8;
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(d8, VALID | BEATSIZE(0) | SRCINC | DSTINC | BLOCKACT(BA_INT), 4,
             src, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    title("E23 base_mod8");
    put_field("base_rd", DM->DMAC_BASEADDR & 0xFU, 1);
    put_field("ok", dst_count(0, 4), 1);
    dump(0);
    nl();
    /* BASEADDR = WRBADDR: run, then again */
    restart();
    DM->DMAC_CTRL = LVLEN_ALL;
    wait_ctrl_off();
    DM->DMAC_WRBADDR = (uint32_t)desc;
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
    ch_cfg(0, TRIGACT(TA_BLOCK));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 4, src, dst + 4, 0);
    ch_on(0);
    swtrig(0);
    delay_quiet();
    title("E23 same_sections");
    put_field("desc0", desc[0].btctrl, 4);
    DM->DMAC_CHINTFLAG = 0x07U;
    ch_on(0);
    swtrig(0);
    delay_quiet();
    dump(0);
    nl();
}

static void e24_events(void)
{
    /* (a) CH0 EVOE -> EVSYS ch 0 -> TC1 COUNT */
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
    title("E24 evout");
    for (unsigned int sel = 1; sel <= 3U; sel += 2U) {
        restart();
        ch_cfg(0, TRIGACT(TA_TRANSACTION) | EVOE);
        set_desc(&desc[0], VALID | EVOSEL(sel) | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src, dst + 4, &chain[0]);
        set_desc(&chain[0], VALID | EVOSEL(sel) | BEATSIZE(0) | SRCINC |
                 DSTINC | BLOCKACT(BA_INT), 4, src, dst + 8, &chain[1]);
        set_desc(&chain[1], VALID | EVOSEL(sel) | BEATSIZE(0) | SRCINC |
                 DSTINC | BLOCKACT(BA_INT), 4, src, dst + 12, 0);
        TC1N.TC_COUNT = 0U;
        while (TC1N.TC_SYNCBUSY) {
        }
        ch_on(0);
        swtrig(0);
        delay_quiet();
        TC1N.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
        while (TC1N.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) {
        }
        put_field(sel == 1U ? "block_events" : "beat_events", TC1N.TC_COUNT, 4);
    }
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC1_EVU] = 0U;
    nl();
    /* (b) EVSYS SWEVT on channel 1 -> DMAC CH0 TRIG, BEAT */
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 2U;
    restart();
    dst_fill();
    ch_cfg(0, TRIGACT(TA_BEAT) | EVIE | EVACT(1));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 8, src, dst + 4, 0);
    ch_on(0);
    for (unsigned int i = 0; i < 5U; i++) {
        EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
        delay_quiet();
    }
    title("E24 evin_trig");
    put_field("moved", dst_count(0, 8), 1);
    dump(0);
    nl();
    /* EVACT SUSPEND then RESUME */
    restart();
    tc0_start();
    ch_cfg(0, TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT) | EVIE | EVACT(4));
    set_desc(&desc[0], VALID | BEATSIZE(0) | SRCINC | DSTINC |
             BLOCKACT(BA_INT), 32, src, dst + 4, 0);
    ch_on(0);
    delay_ms(3);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_quiet();
    title("E24 evin_suspend");
    dump(0);
    ch_off(0);
    DM->DMAC_CHCTRLB = TRIGSRC(TRIG_TC0_OVF) | TRIGACT(TA_BEAT) | EVIE |
                       EVACT(5);
    DM->DMAC_CHCTRLA = CH_ENABLE;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(CMD_SUSPEND);
    delay_quiet();
    title(" | evin_resume");
    dump(0);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_quiet();
    title(" ->");
    dump(0);
    tc0_stop();
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 0U;
    nl();
}

void DMAC_Handler(void)
{
    uint16_t ip = DM->DMAC_INTPEND;
    uint8_t keep = DM->DMAC_CHID;

    if (isr_n < 8U) {
        isr_log[isr_n++] = ip;
    }
    DM->DMAC_CHID = (uint8_t)(ip & 0xFU);
    DM->DMAC_CHINTFLAG = DM->DMAC_CHINTFLAG;
    DM->DMAC_CHID = keep;
}

static void e25_irq(void)
{
    restart();
    isr_n = 0;
    for (unsigned int c = 0; c < 2U; c++) {
        unsigned int ch = c ? 11U : 0U;

        ch_cfg(ch, TRIGACT(TA_BLOCK));
        set_desc(&desc[ch], VALID | BEATSIZE(0) | SRCINC | DSTINC |
                 BLOCKACT(BA_INT), 4, src, dst + 4, 0);
        DM->DMAC_CHINTENSET = TCMPL;
        ch_on(ch);
    }
    __disable_irq();
    NVIC_ClearPendingIRQ(DMAC_IRQn);
    NVIC_EnableIRQ(DMAC_IRQn);
    DM->DMAC_SWTRIGCTRL = (1U << 11) | 1U;
    delay_quiet();
    __enable_irq();
    delay_quiet();
    NVIC_DisableIRQ(DMAC_IRQn);
    title("E25 irq");
    put_field("n", isr_n, 1);
    for (unsigned int i = 0; i < isr_n; i++) {
        put_field("ip", isr_log[i], 4);
    }
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dmacprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                           GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    for (unsigned int i = 0; i < sizeof(src); i++) {
        src[i] = (uint8_t)i;
    }
    e0_reset();
    e1_copy();
    e2_steps();
    e3_e4();
    e5_wb();
    e6_swtrig();
    e7_actions();
    e8_tcmpl();
    e9_suspend();
    e10_invalid();
    e11_buserr();
    e12_abort();
    e13_suspend_cmd();
    e14_snap(TA_BEAT, "beat");
    e14_snap(TA_BLOCK, "block");
    e15_tc();
    e16_sercom();
    e17_arb();
    e18_crc();
    e19_crc_ch();
    e20_protect();
    e21_swrst();
    e22_intpend();
    e23_align();
    e24_events();
    e25_irq();
    restart();
    DM->DMAC_CTRL = 0U;
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
