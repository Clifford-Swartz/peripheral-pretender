/*
 * WDT probe (PIC32CM MC00, WDT U2251): what the watchdog does on silicon,
 * for the model. Prints raw results, one experiment per line; checks
 * nothing. The plan and the answers: docs/research/peripherals/
 * wdt-u2251.md. Run: py -m tools.periph probe mc00 wdtprobe
 *
 * Experiments that end in a reset keep their state in keep_area(); the
 * next boot prints "E<n> reset rcause= after_us= seen= ..." and carries
 * on with the next experiment. Times are microseconds from the write
 * that starts the experiment; "none" is a time-out of the probe's own.
 * ALWAYSON is never written: only a power-on reset clears it.
 *
 *   E0   boot: reset cause, registers, user row words
 *   E1   writable masks while disabled
 *   E2   synchronization times: ENABLE, CLEAR, CLEAR while busy, disable,
 *        CLEAR while disabled
 *   E3   enable protection while enabled; WEN with ENABLE
 *   E4   early warning in normal mode, from enable and from a clear
 *   E5   does a disable and re-enable restart the count?
 *   E6   does a clear before the early warning restart it?
 *   E7   the early-warning interrupt
 *   E8   time-out in normal mode (reset), early warning seen
 *   E9   EWOFFSET beyond PER: reset without an early warning
 *   E10  wrong CLEAR key while disabled (reset)
 *   E11  wrong CLEAR key while enabled (reset)
 *   E12  window mode: early warning, clear in the open window
 *   E13  window mode: clear in the closed window (reset)
 *   E14  window mode: time-out (reset)
 *   E15  reserved PER value 0xC
 *   E16  reserved EWOFFSET value 0xB
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

#define W               WDT_REGS
#define ENABLE          WDT_CTRLA_ENABLE_Msk
#define WEN             WDT_CTRLA_WEN_Msk
#define EW              WDT_INTFLAG_EW_Msk
#define KEY             0xA5U
#define BAD_KEY         0x5AU
#define MAGIC           0x57445450U
#define TIMEOUT         0xFFFFFFFFU

/* CONFIG: WINDOW in 7:4, PER in 3:0; 0 = 8 cycles ... 0xB = 16384 */
#define CFG(window, per) ((uint8_t)(((window) << 4) | (per)))

struct keep {
    uint32_t magic;
    uint32_t step;          /* the next experiment */
    uint32_t armed;         /* experiment number waiting for a reset, + 1 */
    uint32_t mark;          /* now_us() at the write that starts it */
    uint32_t last;          /* the latest now_us() before the reset */
    uint32_t seen;          /* INTFLAG bits seen while waiting */
};

static volatile struct keep *k;
static volatile uint32_t irqs;

void WDT_Handler(void)
{
    W->WDT_INTFLAG = EW;
    irqs++;
}

static void nl(void)
{
    console_puts("\r\n");
}

static void put_us(const char *name, uint32_t us)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    if (us == TIMEOUT) {
        console_puts("none");
    } else {
        console_put_dec(us);
    }
}

/* Never ALWAYSON */
static void ctrla(uint8_t v)
{
    W->WDT_CTRLA = v & (ENABLE | WEN);
}

/* Microseconds from @t0 until SYNCBUSY reads 0 */
static uint32_t sync_us(uint32_t t0)
{
    while (W->WDT_SYNCBUSY != 0U) {
        if (now_us() - t0 > 200000U) {
            return TIMEOUT;
        }
    }
    return now_us() - t0;
}

/* Microseconds from @t0 until INTFLAG.EW, at most @limit */
static uint32_t wait_ew(uint32_t t0, uint32_t limit)
{
    while (!(W->WDT_INTFLAG & EW)) {
        if (now_us() - t0 > limit) {
            return TIMEOUT;
        }
    }
    return now_us() - t0;
}

static void off(void)
{
    ctrla(0);
    (void)sync_us(now_us());
    W->WDT_INTENCLR = EW;
    W->WDT_INTFLAG = EW;
}

/* Disabled: CONFIG, EWCTRL and WEN for the next enable */
static void setup(uint8_t config, uint8_t ewoffset)
{
    off();
    W->WDT_CONFIG = config;
    W->WDT_EWCTRL = ewoffset;
}

static void put_regs(void)
{
    put_field("ctrla", W->WDT_CTRLA, 2);
    put_field("config", W->WDT_CONFIG, 2);
    put_field("ewctrl", W->WDT_EWCTRL, 2);
    put_field("intenset", W->WDT_INTENSET, 2);
    put_field("intflag", W->WDT_INTFLAG, 2);
    put_field("syncbusy", W->WDT_SYNCBUSY, 2);
}

/*
 * Start the reset clock at the write that follows (k->mark), then wait up
 * to @limit microseconds for the reset. Without one, say so and go on.
 */
static void mark(unsigned int n)
{
    console_flush();
    k->armed = n + 1U;
    k->seen = 0;
    clock_zero();
    k->mark = now_us();
}

static void await_reset(uint32_t limit)
{
    for (;;) {
        uint32_t t = now_us();

        k->last = t;
        k->seen |= W->WDT_INTFLAG;
        if (t - k->mark > limit) {
            break;
        }
    }
    k->armed = 0;
    console_puts(" no_reset");
    put_field("seen", k->seen, 2);
    put_regs();
    nl();
    off();
}

static void e0_boot(void)
{
    console_puts("E0 boot");
    put_regs();
    put_field("user0", USER_FUSES_REGS->FUSES_USER_WORD_0, 8);
    put_field("user1", USER_FUSES_REGS->FUSES_USER_WORD_1, 8);
    nl();
}

static void e1_masks(void)
{
    uint8_t cff, c00, eff, ctrl, ien, iclr, iclr2, iflag, clr;
    uint32_t sb, wen_us, t0;

    off();
    W->WDT_CONFIG = 0xFFU;
    cff = W->WDT_CONFIG;
    W->WDT_CONFIG = 0x00U;
    c00 = W->WDT_CONFIG;
    W->WDT_EWCTRL = 0xFFU;
    eff = W->WDT_EWCTRL;
    W->WDT_EWCTRL = 0x00U;
    clock_zero();
    t0 = now_us();
    ctrla(WEN);
    sb = W->WDT_SYNCBUSY;
    wen_us = sync_us(t0);
    ctrl = W->WDT_CTRLA;
    ctrla(0);
    (void)sync_us(now_us());
    W->WDT_INTENSET = 0xFFU;
    ien = W->WDT_INTENSET;
    iclr = W->WDT_INTENCLR;
    W->WDT_INTENCLR = 0xFFU;
    iclr2 = W->WDT_INTENSET;
    W->WDT_INTFLAG = 0xFFU;
    iflag = W->WDT_INTFLAG;
    clr = W->WDT_CLEAR;
    console_puts("E1 masks");
    put_field("config_ff", cff, 2);
    put_field("config_00", c00, 2);
    put_field("ewctrl_ff", eff, 2);
    put_field("sb_wen", sb, 2);
    put_us("wen_us", wen_us);
    put_field("ctrla_wen", ctrl, 2);
    put_field("intenset_ff", ien, 2);
    put_field("intenclr", iclr, 2);
    put_field("intenclr_ff", iclr2, 2);
    put_field("intflag_ff", iflag, 2);
    put_field("clear_rd", clr, 2);
    nl();
}

static void e2_sync(void)
{
    uint32_t sb_en, now, en, sb_clr, clr, clr2, w2, off_us, sb_d, dclr, t0;

    setup(CFG(0, 0xB), 0xA);
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    sb_en = W->WDT_SYNCBUSY;
    now = W->WDT_CTRLA;
    en = sync_us(t0);
    t0 = now_us();
    W->WDT_CLEAR = KEY;
    sb_clr = W->WDT_SYNCBUSY;
    clr = sync_us(t0);
    t0 = now_us();
    W->WDT_CLEAR = KEY;
    W->WDT_CLEAR = KEY;
    w2 = now_us() - t0;
    clr2 = sync_us(t0);
    t0 = now_us();
    ctrla(0);
    off_us = sync_us(t0);
    t0 = now_us();
    W->WDT_CLEAR = KEY;
    sb_d = W->WDT_SYNCBUSY;
    dclr = sync_us(t0);
    console_puts("E2 sync");
    put_field("sb_en", sb_en, 2);
    put_field("ctrla_now", now, 2);
    put_us("en_us", en);
    put_field("sb_clr", sb_clr, 2);
    put_us("clr_us", clr);
    put_us("clr2_write_us", w2);
    put_us("clr2_us", clr2);
    put_us("off_us", off_us);
    put_field("sb_dclr", sb_d, 2);
    put_us("dclr_us", dclr);
    nl();
}

static void e3_protect(void)
{
    uint8_t cfg, ew, en_wen, off_wen, both;

    setup(CFG(0, 0xB), 0xA);
    ctrla(ENABLE);
    (void)sync_us(now_us());
    W->WDT_CONFIG = 0x00U;
    cfg = W->WDT_CONFIG;
    W->WDT_EWCTRL = 0x00U;
    ew = W->WDT_EWCTRL;
    ctrla(ENABLE | WEN);
    (void)sync_us(now_us());
    en_wen = W->WDT_CTRLA;
    ctrla(WEN);
    (void)sync_us(now_us());
    off_wen = W->WDT_CTRLA;
    ctrla(0);
    (void)sync_us(now_us());
    ctrla(ENABLE | WEN);
    (void)sync_us(now_us());
    both = W->WDT_CTRLA;
    off();
    console_puts("E3 protect");
    put_field("config", cfg, 2);
    put_field("ewctrl", ew, 2);
    put_field("ctrla_en_wen", en_wen, 2);
    put_field("ctrla_off_wen", off_wen, 2);
    put_field("ctrla_both", both, 2);
    put_field("ctrla_off", W->WDT_CTRLA, 2);
    nl();
}

static void e4_ew_normal(void)
{
    uint32_t t0, t1, en, ew, clr, flag, ew2;

    setup(CFG(0, 0x5), 0x3);            /* 256 and 64 cycles */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    en = sync_us(t0);
    ew = wait_ew(t0, 200000U);
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    clr = sync_us(t1);
    flag = W->WDT_INTFLAG;
    W->WDT_INTFLAG = EW;
    ew2 = wait_ew(t1, 200000U);
    W->WDT_CLEAR = KEY;
    off();
    console_puts("E4 ew_normal");
    put_us("en_us", en);
    put_us("ew_us", ew);
    put_us("clr_us", clr);
    put_field("flag_after_clear", flag, 2);
    put_us("ew2_us", ew2);
    nl();
}

static void e5_reenable(void)
{
    uint32_t t0, t1, en, ew;

    setup(CFG(0, 0x5), 0x2);            /* 256 and 32 cycles */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    (void)sync_us(t0);
    wait_until_us(t0 + 20000U);
    ctrla(0);
    (void)sync_us(now_us());
    W->WDT_INTFLAG = EW;
    t1 = now_us();
    ctrla(ENABLE);
    en = sync_us(t1);
    ew = wait_ew(t1, 200000U);
    off();
    console_puts("E5 reenable");
    put_us("en_us", en);
    put_us("ew_us", ew);
    nl();
}

static void e6_clear_before_ew(void)
{
    setup(CFG(0, 0x5), 0x2);            /* 256 and 32 cycles */
    console_puts("E6 clear_before_ew");
    clock_zero();
    uint32_t t0 = now_us();
    ctrla(ENABLE);
    (void)sync_us(t0);
    wait_until_us(t0 + 20000U);
    uint32_t t1 = now_us();
    W->WDT_CLEAR = KEY;
    put_us("ew_us", wait_ew(t1, 200000U));
    off();
    nl();
}

static void e7_irq(void)
{
    uint32_t t0, t1, at = TIMEOUT, at2 = TIMEOUT, n1;

    setup(CFG(0, 0x4), 0x2);            /* 128 and 32 cycles */
    irqs = 0;
    W->WDT_INTENSET = EW;
    NVIC_ClearPendingIRQ(WDT_IRQn);
    NVIC_EnableIRQ(WDT_IRQn);
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    while (now_us() - t0 < 100000U) {
        if (irqs && at == TIMEOUT) {
            at = now_us() - t0;
        }
    }
    n1 = irqs;
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    while (now_us() - t1 < 60000U) {
        if (irqs > n1 && at2 == TIMEOUT) {
            at2 = now_us() - t1;
        }
    }
    NVIC_DisableIRQ(WDT_IRQn);
    off();
    console_puts("E7 irq");
    put_field("irqs", n1, 1);
    put_us("irq_us", at);
    put_field("irqs2", irqs, 1);
    put_us("irq2_us", at2);
    nl();
}

static void e8_timeout(void)
{
    setup(CFG(0, 0x2), 0x1);            /* 32 and 16 cycles */
    console_puts("E8 timeout per=32 ew=16 armed");
    nl();
    mark(8);
    ctrla(ENABLE);
    await_reset(300000U);
}

static void e9_ew_late(void)
{
    setup(CFG(0, 0x1), 0x2);            /* 16 and 32 cycles */
    console_puts("E9 ew_late per=16 ew=32 armed");
    nl();
    mark(9);
    ctrla(ENABLE);
    await_reset(300000U);
}

static void e10_badkey_off(void)
{
    off();
    console_puts("E10 badkey_off armed");
    nl();
    mark(10);
    W->WDT_CLEAR = BAD_KEY;
    await_reset(300000U);
}

static void e11_badkey_on(void)
{
    setup(CFG(0, 0xB), 0xA);
    ctrla(ENABLE);
    (void)sync_us(now_us());
    console_puts("E11 badkey_on armed");
    nl();
    mark(11);
    W->WDT_CLEAR = BAD_KEY;
    await_reset(300000U);
}

static void e12_window(void)
{
    uint32_t t0, t1, en, ew, clr, ew2;

    setup(CFG(0x3, 0x3), 0x0);          /* closed 64, open 64; ew 8 */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE | WEN);
    en = sync_us(t0);
    ew = wait_ew(t0, 120000U);
    wait_until_us(t0 + 90000U);         /* the open window */
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    clr = sync_us(t1);
    W->WDT_INTFLAG = EW;
    ew2 = wait_ew(t1, 120000U);
    wait_until_us(t1 + 90000U);
    W->WDT_CLEAR = KEY;
    (void)sync_us(now_us());
    off();
    console_puts("E12 window");
    put_us("en_us", en);
    put_us("ew_us", ew);
    put_us("clr_us", clr);
    put_us("ew2_us", ew2);
    console_puts(" alive");
    nl();
}

static void e13_window_early(void)
{
    setup(CFG(0x3, 0x3), 0x0);          /* closed 64, open 64 */
    clock_zero();
    uint32_t t0 = now_us();
    ctrla(ENABLE | WEN);
    (void)sync_us(t0);
    wait_until_us(t0 + 20000U);
    console_puts("E13 window_early armed");
    nl();
    mark(13);
    W->WDT_CLEAR = KEY;
    await_reset(300000U);
}

static void e14_window_timeout(void)
{
    setup(CFG(0x2, 0x2), 0x0);          /* closed 32, open 32 */
    console_puts("E14 window_timeout armed");
    nl();
    mark(14);
    ctrla(ENABLE | WEN);
    await_reset(300000U);
}

static void e15_per_reserved(void)
{
    setup(CFG(0, 0xC), 0x0);
    console_puts("E15 per_reserved");
    put_field("config", W->WDT_CONFIG, 2);
    console_puts(" armed");
    nl();
    mark(15);
    ctrla(ENABLE);
    await_reset(45000000U);
}

static void e16_ew_reserved(void)
{
    setup(CFG(0, 0x7), 0xB);            /* 1024 cycles; ew reserved */
    console_puts("E16 ew_reserved");
    put_field("ewctrl", W->WDT_EWCTRL, 2);
    clock_zero();
    uint32_t t0 = now_us();
    ctrla(ENABLE);
    put_us("ew_us", wait_ew(t0, 900000U));
    W->WDT_CLEAR = KEY;
    off();
    nl();
}

static void (*const steps[])(void) = {
    e0_boot, e1_masks, e2_sync, e3_protect, e4_ew_normal, e5_reenable,
    e6_clear_before_ew, e7_irq, e8_timeout, e9_ew_late, e10_badkey_off,
    e11_badkey_on, e12_window, e13_window_early, e14_window_timeout,
    e15_per_reserved, e16_ew_reserved,
};

int main(void)
{
    uint8_t rcause = RSTC_REGS->RSTC_RCAUSE;

    sys_initialize();
    systick_start();
    console_puts("PP wdtprobe: " BOARD_BANNER "\r\n");
    k = keep_area();
    if (k->magic == MAGIC && k->armed != 0U) {
        console_puts("E");
        console_put_dec(k->armed - 1U);
        console_puts(" reset");
        put_field("rcause", rcause, 2);
        put_us("after_us", k->last - k->mark);
        put_field("seen", k->seen, 2);
        put_regs();
        nl();
        k->armed = 0;
    } else {
        k->magic = MAGIC;
        k->step = 0;
        k->armed = 0;
        console_puts("fresh");
        put_field("rcause", rcause, 2);
        nl();
    }
    while (k->step < sizeof(steps) / sizeof(steps[0])) {
        steps[k->step++]();
    }
    k->magic = 0;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
