/*
 * WDT check (PIC32CM MC00, WDT U2251): the watchdog against what silicon
 * did in firmware/hello APP=wdtprobe (docs/research/peripherals/
 * wdt-u2251.md, section 5). Register behaviour is checked exactly; times
 * in WDT clocks T (1/1024 s) within the phase spread that silicon showed,
 * and -5 %..+5 % for the part's OSCULP32K. A failed check names what it
 * expected and ends its line in FAIL; PP:EXIT <failures>.
 * Run: py -m tools.periph check mc00 wdt
 *
 * Checks that end in a reset keep their state and the failure count in
 * keep_area(); the next boot finishes the line ("... reset ... ok").
 * ALWAYSON is never written.
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
#define MAGIC           0x57445443U
#define TIMEOUT         0xFFFFFFFFU
#define RCAUSE_WDT      0x20U

#define CFG(window, per) ((uint8_t)(((window) << 4) | (per)))

/* n WDT clocks in microseconds, -5 % and +5 % */
#define LO(n)           ((uint32_t)(n) * 9277U / 10U)
#define HI(n)           ((uint32_t)(n) * 10254U / 10U)

struct keep {
    uint32_t magic;
    uint32_t step;
    uint32_t armed;         /* step waiting for a reset, + 1 */
    uint32_t mark;
    uint32_t last;
    uint32_t seen;
    uint32_t failures;
};

static volatile struct keep *k;
static volatile uint32_t irqs;
static uint8_t rcause;

void WDT_Handler(void)
{
    W->WDT_INTFLAG = EW;
    irqs++;
}

static void ctrla(uint8_t v)
{
    W->WDT_CTRLA = v & (ENABLE | WEN);
}

static uint32_t sync_us(uint32_t t0)
{
    while (W->WDT_SYNCBUSY != 0U) {
        if (now_us() - t0 > 200000U) {
            return TIMEOUT;
        }
    }
    return now_us() - t0;
}

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

static void setup(uint8_t config, uint8_t ewoffset)
{
    off();
    W->WDT_CONFIG = config;
    W->WDT_EWCTRL = ewoffset;
}

/* Sync times: 2 to 4 T after the write, by its phase */
static void put_sync(const char *name, uint32_t us)
{
    put_dec_range(name, us, LO(2), HI(4));
}

static void check_masks(void)
{
    uint8_t cff, c00, eff, ctrl, ien, iclr, iflag, clr;
    uint32_t sb, us, t0;

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
    us = sync_us(t0);
    ctrl = W->WDT_CTRLA;
    ctrla(0);
    (void)sync_us(now_us());
    W->WDT_INTENSET = 0xFFU;
    ien = W->WDT_INTENSET;
    W->WDT_INTENCLR = 0xFFU;
    iclr = W->WDT_INTENSET;
    W->WDT_INTFLAG = 0xFFU;
    iflag = W->WDT_INTFLAG;
    clr = W->WDT_CLEAR;
    console_puts("masks");
    put_checked("config_ff", cff, 0xFF, 2);
    put_checked("config_00", c00, 0x00, 2);
    put_checked("ewctrl_ff", eff, 0x0F, 2);
    put_checked("sb_wen", sb, WDT_SYNCBUSY_WEN_Msk, 2);
    put_sync("wen_us", us);
    put_checked("ctrla_wen", ctrl, WEN, 2);
    put_checked("intenset_ff", ien, 0x01, 2);
    put_checked("intenclr_ff", iclr, 0x00, 2);
    put_checked("intflag_ff", iflag, 0x00, 2);
    put_checked("clear_rd", clr, 0x00, 2);
    end_line();
}

static void check_sync(void)
{
    uint32_t sb_en, now, en, sb_clr, clr, clr2, w2, dis, sb_d, dclr, t0;

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
    dis = sync_us(t0);
    t0 = now_us();
    W->WDT_CLEAR = KEY;
    sb_d = W->WDT_SYNCBUSY;
    dclr = sync_us(t0);
    console_puts("sync");
    put_checked("sb_en", sb_en, WDT_SYNCBUSY_ENABLE_Msk, 2);
    put_checked("ctrla_now", now, ENABLE, 2);
    put_sync("en_us", en);
    put_checked("sb_clr", sb_clr, WDT_SYNCBUSY_CLEAR_Msk, 2);
    put_sync("clr_us", clr);
    put_dec_range("clr2_write_us", w2, 0, 50);
    put_sync("clr2_us", clr2);
    put_sync("off_us", dis);
    put_checked("sb_dclr", sb_d, WDT_SYNCBUSY_CLEAR_Msk, 2);
    put_sync("dclr_us", dclr);
    end_line();
}

static void check_protect(void)
{
    uint8_t cfg, ew, en_wen, off_wen, both, after;

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
    ctrla(0);
    (void)sync_us(now_us());
    after = W->WDT_CTRLA;
    off();
    console_puts("protect");
    put_checked("config", cfg, 0x0B, 2);
    put_checked("ewctrl", ew, 0x0A, 2);
    put_checked("ctrla_en_wen", en_wen, ENABLE, 2);
    put_checked("ctrla_off_wen", off_wen, 0x00, 2);
    put_checked("ctrla_both", both, ENABLE | WEN, 2);
    put_checked("ctrla_off", after, WEN, 2);
    end_line();
}

/*
 * Early warning N clocks into the count: from an enable, which starts the
 * count one T before its 3-4 T sync ends, N + 2..3 T; from a clear, which
 * restarts it when its 2-3 T sync ends, the same.
 */
static void put_ew(const char *name, uint32_t us, uint32_t n)
{
    put_dec_range(name, us, LO(n + 1U), HI(n + 4U));
}

static void check_ew_normal(void)
{
    uint32_t t0, t1, en, ew, flag, ew2;

    setup(CFG(0, 0x5), 0x3);            /* PER 256, EWOFFSET 64 */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    en = sync_us(t0);
    ew = wait_ew(t0, 200000U);
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    (void)sync_us(t1);
    flag = W->WDT_INTFLAG;
    W->WDT_INTFLAG = EW;
    ew2 = wait_ew(t1, 200000U);
    W->WDT_CLEAR = KEY;
    off();
    console_puts("ew_normal");
    put_sync("en_us", en);
    put_ew("ew_us", ew, 64);
    put_checked("flag_after_clear", flag, EW, 2);
    put_ew("ew2_us", ew2, 64);
    end_line();
}

static void check_restarts(void)
{
    uint32_t t0, t1, reen, clr;

    /* A disable and re-enable restarts the count */
    setup(CFG(0, 0x5), 0x2);            /* PER 256, EWOFFSET 32 */
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
    reen = wait_ew(t1, 200000U);
    off();
    /* So does a clear before the early warning */
    setup(CFG(0, 0x5), 0x2);
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE);
    (void)sync_us(t0);
    wait_until_us(t0 + 20000U);
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    clr = wait_ew(t1, 200000U);
    off();
    console_puts("restarts");
    put_ew("reenable_ew_us", reen, 32);
    put_ew("clear_ew_us", clr, 32);
    end_line();
}

static void check_irq(void)
{
    uint32_t t0, t1, at = TIMEOUT, at2 = TIMEOUT, n1;

    setup(CFG(0, 0x4), 0x2);            /* PER 128, EWOFFSET 32 */
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
    console_puts("irq");
    put_checked("irqs", n1, 1, 1);
    put_ew("irq_us", at, 32);
    put_checked("irqs2", irqs, 2, 1);
    put_ew("irq2_us", at2, 32);
    end_line();
}

static void check_window(void)
{
    uint32_t t0, t1, ew, ew2;

    setup(CFG(0x3, 0x3), 0x0);          /* closed 64, open 64; EW 8 */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE | WEN);
    ew = wait_ew(t0, 120000U);
    wait_until_us(t0 + 90000U);         /* in the open window */
    t1 = now_us();
    W->WDT_CLEAR = KEY;
    (void)sync_us(t1);
    W->WDT_INTFLAG = EW;
    ew2 = wait_ew(t1, 120000U);
    wait_until_us(t1 + 90000U);
    W->WDT_CLEAR = KEY;
    (void)sync_us(now_us());
    off();
    console_puts("window");
    put_ew("ew_us", ew, 64);
    put_ew("ew2_us", ew2, 64);
    end_line();
}

/* Checks that end in a reset */

static void arm(unsigned int step)
{
    console_flush();
    k->failures = failures;
    k->armed = step + 1U;
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
    console_puts(" no_reset FAIL\r\n");
    failures++;
    check_restore(failures);
    off();
}

static void reset_timeout(void)
{
    setup(CFG(0, 0x2), 0x1);            /* PER 32, EWOFFSET 16 */
    console_puts("timeout armed\r\n");
    arm(k->step - 1U);
    ctrla(ENABLE);
    await_reset(300000U);
}

static void reset_ew_late(void)
{
    setup(CFG(0, 0x1), 0x2);            /* PER 16, EWOFFSET 32 */
    console_puts("ew_late armed\r\n");
    arm(k->step - 1U);
    ctrla(ENABLE);
    await_reset(300000U);
}

static void reset_badkey_off(void)
{
    off();
    console_puts("badkey_off armed\r\n");
    arm(k->step - 1U);
    W->WDT_CLEAR = BAD_KEY;
    await_reset(300000U);
}

static void reset_badkey_on(void)
{
    setup(CFG(0, 0xB), 0xA);
    ctrla(ENABLE);
    (void)sync_us(now_us());
    console_puts("badkey_on armed\r\n");
    arm(k->step - 1U);
    W->WDT_CLEAR = BAD_KEY;
    await_reset(300000U);
}

static void reset_window_early(void)
{
    uint32_t t0;

    setup(CFG(0x3, 0x3), 0x0);          /* closed 64, open 64 */
    clock_zero();
    t0 = now_us();
    ctrla(ENABLE | WEN);
    (void)sync_us(t0);
    wait_until_us(t0 + 20000U);
    console_puts("window_early armed\r\n");
    arm(k->step - 1U);
    W->WDT_CLEAR = KEY;
    await_reset(300000U);
}

static void reset_window_timeout(void)
{
    setup(CFG(0x2, 0x2), 0x0);          /* closed 32, open 32 */
    console_puts("window_timeout armed\r\n");
    arm(k->step - 1U);
    ctrla(ENABLE | WEN);
    await_reset(300000U);
}

static void (*const steps[])(void) = {
    check_masks, check_sync, check_protect, check_ew_normal,
    check_restarts, check_irq, check_window, reset_timeout, reset_ew_late,
    reset_badkey_off, reset_badkey_on, reset_window_early,
    reset_window_timeout,
};

/* What each reset check expects: time after the write, EW seen */
static const struct {
    const char *name;
    uint32_t lo, hi, seen;
} resets[] = {
    [7] = { "timeout", LO(33), HI(36), EW },            /* PER + 1..4 */
    [8] = { "ew_late", LO(17), HI(20), 0 },
    [9] = { "badkey_off", LO(2), HI(5), 0 },            /* sync, + 1 T */
    [10] = { "badkey_on", LO(2), HI(4), 0 },            /* sync */
    [11] = { "window_early", LO(2), HI(4), 0 },
    [12] = { "window_timeout", LO(65), HI(68), EW },    /* WINDOW + PER */
};

static void finish_reset(unsigned int step)
{
    console_puts(resets[step].name);
    console_puts(" reset");
    put_checked("rcause", rcause, RCAUSE_WDT, 2);
    put_dec_range("after_us", k->last - k->mark, resets[step].lo,
                  resets[step].hi);
    put_checked("seen", k->seen, resets[step].seen, 2);
    put_checked("ctrla", W->WDT_CTRLA, 0x00, 2);
    end_line();
}

int main(void)
{
    rcause = RSTC_REGS->RSTC_RCAUSE;
    sys_initialize();
    systick_start();
    console_puts("PP wdt: " BOARD_BANNER "\r\n");
    k = keep_area();
    if (k->magic == MAGIC && k->armed != 0U) {
        check_restore(k->failures);
        finish_reset(k->armed - 1U);
        k->armed = 0;
    } else {
        k->magic = MAGIC;
        k->step = 0;
        k->armed = 0;
    }
    while (k->step < sizeof(steps) / sizeof(steps[0])) {
        steps[k->step++]();
    }
    k->magic = 0;
    test_exit();
}
