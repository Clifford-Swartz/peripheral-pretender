/*
 * Sleep check (PIC32CM MC00, PM): what IDLE and STANDBY stop and what
 * wakes the core, against what silicon did in firmware/hello
 * APP=sleepprobe and sleepprobe2 (docs/research/peripherals/pm-u2240.md).
 * Run: py -m tools.periph check mc00 sleep
 *
 * Bare EV15N46A. The RTC (ULP1K, 1024 Hz) wakes the core with CMP0 51
 * counts (about 50 ms) after the sleep starts. Counters at 62.5 kHz
 * (OSC48M / 48 / DIV16, about 3100 counts in 50 ms): TC0 on GCLK1 without
 * RUNSTDBY, TC2 on GCLK2 with RUNSTDBY; TC4 on GCLK3 raises MC0 312 counts
 * (5 ms) after it starts. SysTick (24 bits, CPU clock) shows whether the
 * CPU clock ran. Checks are ranges: silicon reads the RTC several ms late
 * after a standby, and its first standby after the debugger's reset ran
 * SysTick for a while (a warm-up sleep comes first).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define M0              (RTC_REGS->MODE0)
#define T0              (TC0_REGS->COUNT16)
#define T1              (TC1_REGS->COUNT16)
#define T2              (TC2_REGS->COUNT16)
#define T4              (TC4_REGS->COUNT16)
#define IDLE            2U
#define STANDBY         4U
#define TC_DIV16        TC_CTRLA_PRESCALER_DIV16
#define RUNSTDBY        TC_CTRLA_RUNSTDBY_Msk
#define WAKE_COUNTS     51U
#define NO_TC4          0xFFFFFFFFU
#define WOKE_RTC        1U
#define WOKE_TC4        2U
#define SLEEP_TICKS     3100U           /* 50 ms at 62.5 kHz */

typedef struct {
    uint32_t rtc, systick, tc0, tc2, tc4, woke;
} sleep_t;

static volatile uint32_t woke;

void RTC_Handler(void)
{
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    woke |= WOKE_RTC;
}

void TC4_Handler(void)
{
    T4.TC_INTFLAG = TC_INTFLAG_MC0_Msk;
    T4.TC_INTENCLR = TC_INTENCLR_MC0_Msk;
    woke |= WOKE_TC4;
}

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 2);
}

static void rtc_sync(void)
{
    for (uint32_t i = 0; M0.RTC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static uint32_t rtc_count(void)
{
    rtc_sync();
    return M0.RTC_COUNT;
}

static void tc_sync(tc_count16_registers_t *t)
{
    for (uint32_t i = 0; t->TC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static uint32_t tc_count(tc_count16_registers_t *t)
{
    t->TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
    tc_sync(t);
    for (uint32_t i = 0; (t->TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) && i < 200000U;
         i++) {
    }
    return t->TC_COUNT;
}

static void tc_setup(tc_count16_registers_t *t, uint32_t extra)
{
    t->TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(t);
    t->TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_DIV16 | extra;
}

static void tc_enable(tc_count16_registers_t *t)
{
    t->TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    tc_sync(t);
}

static void gclk(unsigned int n)
{
    GCLK_REGS->GCLK_GENCTRL[n] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) | GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
}

static void channel(unsigned int id, unsigned int gen)
{
    GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN(gen) |
                                  GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[id] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
}

/*
 * One sleep in @mode; @tc4 is NO_TC4 or TC4's RUNSTDBY (0 or RUNSTDBY):
 * TC4 then starts right before WFI and raises MC0 312 counts later
 */
static void sleep_once(sleep_t *r, uint32_t mode, uint32_t tc4)
{
    uint32_t r0, a0, b0, s0;

    console_flush();
    if (tc4 != NO_TC4) {
        tc_setup(&T4, tc4);
        T4.TC_CC[0] = 312;
        tc_sync(&T4);
        T4.TC_INTFLAG = 0xFFU;
        T4.TC_INTENSET = TC_INTENSET_MC0_Msk;
    }
    woke = 0;
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    r0 = rtc_count();
    M0.RTC_COMP = r0 + WAKE_COUNTS;
    rtc_sync();
    a0 = tc_count(&T0);
    b0 = tc_count(&T2);
    PM_REGS->PM_SLEEPCFG = (uint8_t)mode;
    for (uint32_t i = 0; PM_REGS->PM_SLEEPCFG != mode && i < 1000U; i++) {
    }
    if (tc4 != NO_TC4) {
        tc_enable(&T4);
    }
    s0 = scope_now();
    __DSB();
    __WFI();
    r->systick = scope_since(s0);
    r->tc4 = tc4 != NO_TC4 ? tc_count(&T4) : 0;
    r->woke = woke;
    r->rtc = rtc_count() - r0;
    r->tc0 = (tc_count(&T0) - a0) & 0xFFFFU;
    r->tc2 = (tc_count(&T2) - b0) & 0xFFFFU;
    PM_REGS->PM_SLEEPCFG = IDLE;
    T4.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(&T4);
}

static void check_reset(void)
{
    console_puts("reset");
    put_exact("sleepcfg", PM_REGS->PM_SLEEPCFG, 0);
    put_range("stdbycfg", PM_REGS->PM_STDBYCFG, 0x0400, 0x0400, 4);
    end_line();
}

static void check_idle(void)
{
    sleep_t r;

    sleep_once(&r, IDLE, NO_TC4);
    console_puts("idle");
    put_exact("woke", r.woke, WOKE_RTC);
    put_dec_range("systick_ms", r.systick / 48000U, 45, 55);
    put_dec_range("tc0", r.tc0, SLEEP_TICKS * 9 / 10, SLEEP_TICKS * 11 / 10);
    put_dec_range("tc2", r.tc2, SLEEP_TICKS * 9 / 10, SLEEP_TICKS * 11 / 10);
    end_line();
}

static void check_standby(const char *name, int check_systick)
{
    sleep_t r;

    sleep_once(&r, STANDBY, NO_TC4);
    console_puts(name);
    put_exact("woke", r.woke, WOKE_RTC);
    if (check_systick) {
        put_dec_range("systick_us", r.systick / 48U, 0, 1000);
    }
    /* TC0 counts only around the sleep (silicon: up to ~7 ms after it) */
    put_dec_range("tc0", r.tc0, 0, SLEEP_TICKS / 5);
    put_dec_range("tc2", r.tc2, SLEEP_TICKS * 9 / 10, SLEEP_TICKS * 6 / 5);
    end_line();
}

static void check_tc4(const char *name, uint32_t mode, uint32_t runstdby)
{
    sleep_t r;

    sleep_once(&r, mode, runstdby);
    console_puts(name);
    if (mode == STANDBY && !runstdby) {
        /* TC4 stopped: the RTC wakes the core, TC4 has not counted */
        put_exact("woke_rtc", r.woke & WOKE_RTC, WOKE_RTC);
        put_dec_range("tc4", r.tc4, 0, 10);
    } else {
        /* TC4 runs and wakes the core after 312 counts */
        put_exact("woke", r.woke, WOKE_TC4);
        put_dec_range("tc4", r.tc4, 312, 400);
        put_dec_range("rtc", r.rtc, 4, 20);
    }
    end_line();
}

/* TC1 with RUNSTDBY keeps TC0's (shared) channel running: TC0 counts */
static void check_shared(void)
{
    sleep_t r;

    tc_setup(&T1, RUNSTDBY);
    tc_enable(&T1);
    sleep_once(&r, STANDBY, NO_TC4);
    console_puts("shared_channel");
    put_exact("woke", r.woke, WOKE_RTC);
    put_dec_range("tc0", r.tc0, SLEEP_TICKS * 9 / 10, SLEEP_TICKS * 6 / 5);
    end_line();
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(&T1);
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP sleep: " BOARD_BANNER "\r\n");
    check_reset();
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_RTC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk |
                                MCLK_APBCMASK_TC2_Msk | MCLK_APBCMASK_TC4_Msk;
    gclk(1);
    gclk(2);
    gclk(3);
    channel(TC0_GCLK_ID, 1);
    channel(TC2_GCLK_ID, 2);
    channel(TC4_GCLK_ID, 3);
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    rtc_sync();
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_PRESCALER(1) |
                   RTC_MODE0_CTRLA_COUNTSYNC_Msk;
    rtc_sync();
    M0.RTC_INTENSET = RTC_MODE0_INTENSET_CMP0_Msk;
    M0.RTC_CTRLA |= RTC_MODE0_CTRLA_ENABLE_Msk;
    rtc_sync();
    NVIC_EnableIRQ(RTC_IRQn);
    NVIC_EnableIRQ(TC4_IRQn);
    tc_setup(&T0, 0);
    tc_enable(&T0);
    tc_setup(&T2, RUNSTDBY);
    tc_enable(&T2);
    scope_start();

    check_idle();
    check_standby("standby_first", 0);
    check_standby("standby", 1);
    check_tc4("idle_tc4", IDLE, 0);
    check_tc4("standby_tc4_stopped", STANDBY, 0);
    check_tc4("standby_tc4_runstdby", STANDBY, RUNSTDBY);
    check_shared();

    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_DisableIRQ(TC4_IRQn);
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    rtc_sync();
    test_exit();
}
