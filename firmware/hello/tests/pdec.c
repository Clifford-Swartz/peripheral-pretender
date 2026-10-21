/*
 * PDEC check (PIC32CM MC00, PDEC U2263): registers, START/STOP and the
 * commands, X4 counting and direction, SWAP and PINVEN, READSYNC, the
 * angular and revolution counters, MC0/MC1, QERR, X2, COUNTER timing,
 * HALL codes and the CC buffer, against what silicon did in
 * firmware/hello APP=pdecprobe and pdecprobe2
 * (docs/research/peripherals/pdec-u2263.md).
 * Run: py -m tools.periph check mc00 pdec
 *
 * Bare EV15N46A, GCLK_PDEC on GCLK0 (48 MHz), FILTER 63. Phase A = PA08,
 * B = PA09 (function J) moved by their pulls; a third input through PA02
 * (EIC EXTINT2, HIGH, ASYNCH) on an asynchronous channel to EVU2, or to
 * EVU0 and EVU1 at once for simultaneous changes. Each snapshot compares
 * COUNT (after READSYNC), STATUS and INTFLAG, then clears them.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define PD              PDEC_REGS
#define GA              0U
#define FUNC_J          9U
#define FUNC_A          0U
#define PIN_A           8U
#define PIN_B           9U
#define PIN_I           2U
#define GEN_EIC2        16U
#define USER_EVU(n)     (44U + (n))
#define ENABLE          (1U << 1)
#define MODE_HALL       (1U << 2)
#define MODE_COUNTER    (2U << 2)
#define CONF(n)         ((uint32_t)(n) << 8)
#define SWAP            (1U << 14)
#define PEREN           (1U << 15)
#define PINEN(n)        (1U << (16 + (n)))
#define PINVEN(n)       (1U << (20 + (n)))
#define ANGULAR(n)      ((uint32_t)(n) << 24)
#define CMD_RETRIGGER   (1U << 5)
#define CMD_UPDATE      (2U << 5)
#define CMD_READSYNC    (3U << 5)
#define CMD_START       (4U << 5)
#define CMD_STOP        (5U << 5)
#define LUPD            (1U << 1)
#define EVEI(n)         (1U << (5 + (n)))
#define AB              (PINEN(0) | PINEN(1))

typedef struct {
    uint16_t count, status;
    uint8_t flags;
} snap_t;

static uint32_t bad;                    /* snapshots that differed */

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 2);
}

static void sync(void)
{
    for (uint32_t i = 0; PD->PDEC_SYNCBUSY && i < 200000U; i++) {
    }
}

static void cmd(uint8_t c)
{
    PD->PDEC_CTRLBSET = c;
    sync();
    for (uint32_t i = 0; (PD->PDEC_CTRLBSET & 0xE0U) && i < 200000U; i++) {
    }
}

static void clear_flags(void)
{
    PD->PDEC_INTFLAG = 0xFF;
    PD->PDEC_STATUS = 0x003F;
    sync();
}

/* SWRST, CC0/CC1, CTRLA, EVCTRL, PRESC, enabled; START, flags cleared */
static void pdec_setup(uint32_t ctrla, uint16_t evctrl, uint32_t cc0,
                       uint32_t cc1, uint8_t presc, int start)
{
    PD->PDEC_CTRLA = 1U;
    sync();
    PD->PDEC_CC[0] = cc0;
    PD->PDEC_CC[1] = cc1;
    PD->PDEC_CTRLA = ctrla;
    PD->PDEC_EVCTRL = evctrl;
    PD->PDEC_FILTER = 63;
    PD->PDEC_PRESC = presc;
    sync();
    PD->PDEC_CTRLA = ctrla | ENABLE;
    sync();
    if (start) {
        cmd(CMD_START);
    }
    clear_flags();
}

static void step(unsigned int ab)
{
    scope_pull(GA, PIN_A, ab & 1U);
    scope_pull(GA, PIN_B, (ab >> 1) & 1U);
    delay_us(200);
}

static void third(unsigned int v)
{
    scope_pull(GA, PIN_I, v);
    delay_us(200);
}

/* Compare COUNT, STATUS, INTFLAG with @e; count mismatches in bad */
static void snap(const snap_t *e)
{
    uint16_t c, st;
    uint8_t f;

    cmd(CMD_READSYNC);
    c = PD->PDEC_COUNT;
    st = PD->PDEC_STATUS;
    f = PD->PDEC_INTFLAG;
    clear_flags();
    bad += c != e->count || st != e->status || f != e->flags;
}

/* Steps through @seq (A bit 0, B bit 1), a snapshot after each */
static void run(const uint8_t *seq, const snap_t *e, unsigned int n)
{
    for (unsigned int k = 0; k < n; k++) {
        step(seq[k]);
        snap(&e[k]);
    }
}

/* S0-leading order 10 11 01 00, and the reverse 01 11 10 00 */
static const uint8_t fwd4[4] = { 1, 3, 2, 0 };
static const uint8_t rev4[4] = { 2, 3, 1, 0 };

static void check_registers(void)
{
    console_puts("registers");
    PD->PDEC_CTRLA = 1U;
    sync();
    put_range("ctrla", PD->PDEC_CTRLA, 0, 0, 8);
    put_range("status", PD->PDEC_STATUS, 0x40, 0x40, 4);
    PD->PDEC_CTRLA = 0xFFFFFFFCU;
    sync();
    put_range("ctrla_mask", PD->PDEC_CTRLA, 0xF777CF4CU, 0xF777CF4CU, 8);
    PD->PDEC_EVCTRL = 0xFFFF;
    put_range("evctrl_mask", PD->PDEC_EVCTRL, 0x3FFF, 0x3FFF, 4);
    PD->PDEC_CC[0] = 0xFFFFFFFFU;
    PD->PDEC_PRESC = 0xFF;
    PD->PDEC_FILTER = 0xFF;
    sync();
    put_range("cc0_16_bits", PD->PDEC_CC[0], 0xFFFF, 0xFFFF, 8);
    put_exact("presc_mask", PD->PDEC_PRESC, 0x0F);
    put_exact("filter_mask", PD->PDEC_FILTER, 0xFF);
    PD->PDEC_CTRLA = 1U;
    sync();
    end_line();
}

/* Stopped at the enable; START compares (CC0 = CC1 = 0: MC0, MC1) */
static void check_start(void)
{
    static const snap_t stopped = { 0, 0x40, 0 };
    static const snap_t counted = { 4, 0, 0x08 };

    console_puts("start");
    step(0);
    pdec_setup(AB, 0, 0, 0, 0, 0);
    for (unsigned int k = 0; k < 4; k++) {
        step(fwd4[k]);
    }
    snap(&stopped);
    cmd(CMD_START);
    {
        static const snap_t at_start = { 0, 0, 0x30 };

        snap(&at_start);
    }
    for (unsigned int k = 0; k < 4; k++) {
        step(fwd4[k]);
    }
    snap(&counted);
    put_exact("bad", bad, 0);
    bad = 0;
    end_line();
}

/* Up four per cycle; a reversal: DIR at once, the DIR flag a step later */
static void check_direction(void)
{
    static const snap_t up[4] = {
        { 1, 0, 0x08 }, { 2, 0, 0x08 }, { 3, 0, 0x08 }, { 4, 0, 0x08 },
    };
    static const snap_t down[4] = {
        { 3, 0x80, 0x00 }, { 2, 0x80, 0x0C }, { 1, 0x80, 0x08 },
        { 0, 0x80, 0x38 },
    };

    console_puts("direction");
    step(0);
    pdec_setup(AB, 0, 0, 0, 0, 1);
    run(fwd4, up, 4);
    run(rev4, down, 4);
    put_exact("bad", bad, 0);
    bad = 0;
    /* SWAP and PINVEN0 count the same order down */
    for (unsigned int i = 0; i < 2; i++) {
        static const snap_t swapped = { 0xFFFC, 0x80, 0x0D };

        step(0);
        pdec_setup(AB | (i ? PINVEN(0) : SWAP), 0, 0, 0, 0, 1);
        for (unsigned int k = 0; k < 4; k++) {
            step(fwd4[k]);
        }
        snap(&swapped);
    }
    put_exact("swap_pinven_bad", bad, 0);
    bad = 0;
    end_line();
}

static void check_readsync(void)
{
    console_puts("readsync");
    step(0);
    pdec_setup(AB, 0, 0, 0, 0, 1);
    for (unsigned int k = 0; k < 4; k++) {
        step(fwd4[k]);
    }
    step(1);
    put_range("without", PD->PDEC_COUNT, 0, 0, 8);
    cmd(CMD_READSYNC);
    put_range("with", PD->PDEC_COUNT, 5, 5, 8);
    PD->PDEC_COUNT = 0x0100;
    sync();
    put_range("written", PD->PDEC_COUNT, 0x100, 0x100, 8);
    step(3);
    cmd(CMD_READSYNC);
    put_range("counts_on", PD->PDEC_COUNT, 0x101, 0x101, 8);
    end_line();
}

/* ANGULAR 0, PEREN, CC0 = 3 | 2 << 9: angular wraps move the revolution */
static void check_angular(void)
{
    static const snap_t e[14] = {
        { 0x001, 0, 0x08 }, { 0x002, 0, 0x08 }, { 0x003, 0, 0x08 },
        { 0x200, 0, 0x08 }, { 0x201, 0, 0x08 }, { 0x202, 0, 0x08 },
        { 0x203, 0, 0x08 }, { 0x400, 0, 0x08 }, { 0x401, 0, 0x08 },
        { 0x402, 0, 0x08 }, { 0x403, 0, 0x18 }, { 0x000, 0, 0x29 },
        { 0x001, 0, 0x08 }, { 0x002, 0, 0x08 },
    };
    static const snap_t d[6] = {
        { 0x003, 0, 0x08 }, { 0x002, 0x80, 0x00 }, { 0x001, 0x80, 0x0C },
        { 0x000, 0x80, 0x28 }, { 0x403, 0x80, 0x19 }, { 0x402, 0x80, 0x08 },
    };
    uint8_t seq[14];

    console_puts("angular");
    for (unsigned int k = 0; k < 14; k++) {
        seq[k] = fwd4[k % 4];
    }
    step(0);
    pdec_setup(AB | PEREN | ANGULAR(0), 0, 3U | (2U << 9), 0, 0, 1);
    run(seq, e, 14);
    for (unsigned int k = 0; k < 6; k++) {
        seq[k] = rev4[k % 4];
    }
    run(seq, d, 6);
    put_exact("bad", bad, 0);
    bad = 0;
    /* ANGULAR 7, PEREN 0: a 16-bit counter wrapping with OVF */
    {
        static const uint8_t s16[6] = { 1, 3, 2, 3, 1, 0 };
        static const snap_t w[6] = {
            { 0xFFFF, 0, 0x08 }, { 0x0000, 0, 0x39 }, { 0x0001, 0, 0x08 },
            { 0x0000, 0x80, 0x30 }, { 0xFFFF, 0x80, 0x0D },
            { 0xFFFE, 0x80, 0x08 },
        };

        step(0);
        pdec_setup(AB | ANGULAR(7), 0, 0, 0, 0, 1);
        PD->PDEC_COUNT = 0xFFFE;
        sync();
        run(s16, w, 6);
        put_exact("wrap16_bad", bad, 0);
        bad = 0;
    }
    end_line();
}

/* MC0/MC1: COUNT reaching CC0 = 5, CC1 = 3, both ways */
static void check_compare(void)
{
    static const snap_t e[7] = {
        { 1, 0, 0x08 }, { 2, 0, 0x08 }, { 3, 0, 0x28 }, { 4, 0, 0x08 },
        { 5, 0, 0x18 }, { 6, 0, 0x08 }, { 7, 0, 0x08 },
    };
    static const uint8_t back[6] = { 3, 1, 0, 2, 3, 1 };
    static const snap_t d[6] = {
        { 6, 0x80, 0x00 }, { 5, 0x80, 0x1C }, { 4, 0x80, 0x08 },
        { 3, 0x80, 0x28 }, { 2, 0x80, 0x08 }, { 1, 0x80, 0x08 },
    };
    uint8_t seq[7];

    console_puts("compare");
    for (unsigned int k = 0; k < 7; k++) {
        seq[k] = fwd4[k % 4];
    }
    step(0);
    pdec_setup(AB | ANGULAR(7), 0, 5, 3, 0, 1);
    run(seq, e, 7);
    run(back, d, 6);
    put_exact("bad", bad, 0);
    bad = 0;
    end_line();
}

static void check_commands(void)
{
    static const snap_t five = { 5, 0, 0x08 };
    static const snap_t stopped = { 5, 0x40, 0 };
    static const snap_t started = { 7, 0, 0x08 };
    static const snap_t retrig = { 0, 0, 0x30 };
    static const snap_t under = { 0xFFFF, 0x80, 0x01 };
    static const snap_t retrig_down = { 0, 0x80, 0x30 };
    static const snap_t reenabled = { 0, 0xC0, 0 };

    console_puts("commands");
    step(0);
    pdec_setup(AB, 0, 0, 0, 0, 1);
    for (unsigned int k = 0; k < 4; k++) {
        step(fwd4[k]);
    }
    step(1);
    snap(&five);
    cmd(CMD_STOP);                      /* keeps COUNT, ignores steps */
    step(3);
    step(2);
    snap(&stopped);
    cmd(CMD_START);
    step(0);
    step(1);
    snap(&started);
    cmd(CMD_RETRIGGER);                 /* 0, also counting down */
    snap(&retrig);
    step(0);
    snap(&under);
    cmd(CMD_RETRIGGER);
    snap(&retrig_down);
    PD->PDEC_CTRLA &= ~ENABLE;
    sync();
    PD->PDEC_CTRLA |= ENABLE;           /* stopped again */
    sync();
    snap(&reenabled);
    put_exact("bad", bad, 0);
    bad = 0;
    end_line();
}

/* One event into EVU0 and EVU1: QERR, ERR, no count (not the first) */
static void check_qerr(void)
{
    static const snap_t first = { 0, 0, 0 };
    static const snap_t second = { 0, 0x01, 0x02 };

    console_puts("qerr");
    third(0);
    EVSYS_REGS->EVSYS_USER[USER_EVU(0)] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(1)] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 0U;
    pdec_setup(0, EVEI(0) | EVEI(1), 0, 0, 0, 1);
    third(1);
    snap(&first);
    third(0);
    snap(&second);
    EVSYS_REGS->EVSYS_USER[USER_EVU(0)] = 0U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(1)] = 0U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 1U;
    put_exact("bad", bad, 0);
    bad = 0;
    end_line();
}

/* X2: each S0 edge counts, down while S1 is 1; DIR follows S1 */
static void check_x2(void)
{
    static const snap_t f[4] = {
        { 1, 0, 0x08 }, { 1, 0x80, 0x00 }, { 0, 0x80, 0x3C }, { 0, 0, 0x00 },
    };
    static const snap_t r[4] = {
        { 0, 0x80, 0x00 }, { 0xFFFF, 0x80, 0x09 }, { 0xFFFF, 0, 0x00 },
        { 0, 0, 0x3D },
    };

    console_puts("x2");
    step(0);
    pdec_setup(AB | CONF(2), 0, 0, 0, 0, 1);
    run(fwd4, f, 4);
    run(rev4, r, 4);
    put_exact("bad", bad, 0);
    bad = 0;
    end_line();
}

static uint32_t count_now(void)
{
    cmd(CMD_READSYNC);
    return PD->PDEC_COUNT;
}

/* COUNTER: GCLK / 1024 = 46.875 kHz, counting from the enable */
static void check_counter(void)
{
    uint32_t t0, a, seen[3] = { 0, 0, 0 };

    console_puts("counter");
    pdec_setup(MODE_COUNTER, 0, 0, 0, 10, 0);
    a = count_now();
    t0 = scope_now();
    scope_wait_until(t0, SCOPE_CYC(10000));
    put_dec_range("no_start_10ms", count_now() - a, 460, 478);
    put_range("status", PD->PDEC_STATUS, 0, 0, 4);
    /* PEREN, CC0 = 4687 (100 ms), CC1 = 1000 (21.3 ms) */
    pdec_setup(MODE_COUNTER | PEREN, 0, 4687, 1000, 10, 1);
    t0 = scope_now();
    while (scope_since(t0) < SCOPE_CYC(110000)) {
        uint8_t f = PD->PDEC_INTFLAG;
        uint32_t ms = scope_since(t0) / (CPU_CLOCK_HZ / 1000U) + 1;

        seen[0] = (f & 0x01) && !seen[0] ? ms : seen[0];
        seen[1] = (f & 0x10) && !seen[1] ? ms : seen[1];
        seen[2] = (f & 0x20) && !seen[2] ? ms : seen[2];
    }
    put_dec_range("ovf_ms", seen[0], 99, 102);
    put_dec_range("mc0_ms", seen[1], 99, 102);
    put_dec_range("mc1_ms", seen[2], 21, 23);
    end_line();
}

/* HALL: the code in COUNT[2:0]; 1-3-2-6-4-5 clears DIR, the reverse sets it */
static void hall(unsigned int code)
{
    scope_pull(GA, PIN_A, code & 1U);
    scope_pull(GA, PIN_B, (code >> 1) & 1U);
    third((code >> 2) & 1U);
}

static void check_hall(void)
{
    static const uint8_t seq[6] = { 3, 2, 6, 4, 5, 1 };
    static const uint8_t back[6] = { 5, 4, 6, 2, 3, 1 };
    uint32_t wrong = 0;

    console_puts("hall");
    hall(1);
    pdec_setup(MODE_HALL | AB, EVEI(2), 3, 0, 0, 1);
    for (unsigned int k = 0; k < 6; k++) {
        hall(seq[k]);
        cmd(CMD_READSYNC);
        wrong |= ((PD->PDEC_COUNT & 7U) != seq[k] ||
                  (PD->PDEC_STATUS & 0x80U)) << k;
    }
    for (unsigned int k = 0; k < 6; k++) {
        hall(back[k]);
        cmd(CMD_READSYNC);
        wrong |= ((PD->PDEC_COUNT & 7U) != back[k] ||
                  !(PD->PDEC_STATUS & 0x80U)) << (k + 8);
    }
    put_range("wrong", wrong, 0, 0, 4);
    hall(0);
    put_exact("herr_code0", (PD->PDEC_STATUS >> 5) & 1U, 1);
    end_line();
}

/* CCBUF0: CC0 reads it while valid, a second write is dropped */
static void check_buffers(void)
{
    console_puts("buffers");
    step(0);
    pdec_setup(AB | PEREN | ANGULAR(7), 0, 0x100, 0, 0, 1);
    PD->PDEC_CCBUF[0] = 0x200;
    sync();
    put_range("cc0_shows_buf", PD->PDEC_CC[0], 0x200, 0x200, 8);
    put_range("ccbufv0", PD->PDEC_STATUS & 0x1000U, 0x1000, 0x1000, 4);
    step(1);
    put_range("still_valid", PD->PDEC_STATUS & 0x1000U, 0x1000, 0x1000, 4);
    PD->PDEC_CTRLBSET = LUPD;
    sync();
    PD->PDEC_CCBUF[0] = 0x300;
    sync();
    cmd(CMD_UPDATE);
    put_range("cc0_updated", PD->PDEC_CC[0], 0x200, 0x200, 8);
    put_range("valid_cleared", PD->PDEC_STATUS & 0x1000U, 0, 0, 4);
    end_line();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP pdec: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_PDEC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN_A, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_B, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_I, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN_I, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * PIN_I);
    EIC_REGS->EIC_ASYNCH = 1U << PIN_I;
    EIC_REGS->EIC_EVCTRL = 1U << PIN_I;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 1U;

    check_registers();
    check_start();
    check_direction();
    check_readsync();
    check_angular();
    check_compare();
    check_commands();
    check_qerr();
    check_x2();
    scope_start();
    check_counter();
    check_hall();
    check_buffers();

    PD->PDEC_CTRLA = 1U;
    sync();
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    scope_release(GA, PIN_A);
    scope_release(GA, PIN_B);
    scope_release(GA, PIN_I);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = (1U << PIN_A) | (1U << PIN_B) |
                                       (1U << PIN_I);
    test_exit();
}
