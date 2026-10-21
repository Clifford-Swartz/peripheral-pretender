/*
 * Sleep probe (PIC32CM MC00, PM): what IDLE and STANDBY stop, and what
 * wakes the core (DS 16.5.3). Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 sleepprobe
 *
 * Bare EV15N46A, the debugger attached (the harness keeps it). The RTC
 * (MODE0, ULP1K at 1024 Hz, runs in standby) times each sleep and wakes
 * the core with CMP0 about 50 ms later. TC0, TC1 and TC2 count GCLK1
 * (OSC48M / 48 = 1 MHz) with DIV16 (62.5 kHz): TC0 without RUNSTDBY, TC1
 * with it; TC2 raises MC0 5 ms after its start (an early wake-up if it
 * runs). SysTick (24 bits, CPU clock) shows whether the CPU clock ran.
 * Each line: SLEEPCFG read back, RTC counts asleep, SysTick cycles, TC0
 * and TC1 counts, which handlers ran (bit 0 RTC, bit 1 TC2).
 *
 *   S1  IDLE, RTC wake
 *   S2  STANDBY, RTC wake
 *   S3  STANDBY, TC2 MC0 armed without RUNSTDBY
 *   S4  STANDBY, TC2 with RUNSTDBY (OSC48M RUNSTDBY 0, GCLK1 RUNSTDBY 0)
 *   S5  STANDBY, TC2 with RUNSTDBY and OSC48M RUNSTDBY 1
 *   S6  STANDBY, S5 plus GENCTRL1.RUNSTDBY 1
 *   S7  reset values: SLEEPCFG, STDBYCFG; DSU STATUSB (DBGPRES)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define M0              (RTC_REGS->MODE0)
#define T0              (TC0_REGS->COUNT16)
#define T1              (TC1_REGS->COUNT16)
#define T2              (TC2_REGS->COUNT16)
#define IDLE            2U
#define STANDBY         4U
#define TC_DIV16        TC_CTRLA_PRESCALER_DIV16
#define WAKE_COUNTS     51U                     /* ~50 ms at 1024 Hz */

static volatile uint32_t woke;

void RTC_Handler(void)
{
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    woke |= 1U;
}

void TC2_Handler(void)
{
    T2.TC_INTFLAG = TC_INTFLAG_MC0_Msk;
    T2.TC_INTENCLR = TC_INTENCLR_MC0_Msk;
    woke |= 2U;
}

static void nl(void)
{
    console_puts("\r\n");
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

static void tc_start(tc_count16_registers_t *t, uint32_t runstdby)
{
    t->TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(t);
    t->TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_DIV16 | runstdby;
    t->TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    tc_sync(t);
}

static void gclk1(uint32_t runstdby)
{
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk | runstdby;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
}

static void osc48m_runstdby(int on)
{
    uint8_t v = OSCCTRL_REGS->OSCCTRL_OSC48MCTRL;

    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL = on ?
        (v | OSCCTRL_OSC48MCTRL_RUNSTDBY_Msk) :
        (v & ~OSCCTRL_OSC48MCTRL_RUNSTDBY_Msk);
}

/* TC2 raises MC0 after 5 ms (312 counts) */
static void tc2_arm(uint32_t runstdby)
{
    T2.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(&T2);
    T2.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_DIV16 | runstdby;
    T2.TC_CC[0] = 312;
    tc_sync(&T2);
    T2.TC_INTFLAG = 0xFFU;
    T2.TC_INTENSET = TC_INTENSET_MC0_Msk;
    T2.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    tc_sync(&T2);
}

static void sleep_case(const char *name, uint32_t mode)
{
    uint32_t r0, r1, a0, a1, b0, b1, s0, s;

    console_flush();
    woke = 0;
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    r0 = rtc_count();
    M0.RTC_COMP = r0 + WAKE_COUNTS;
    rtc_sync();
    a0 = tc_count(&T0);
    b0 = tc_count(&T1);
    PM_REGS->PM_SLEEPCFG = (uint8_t)mode;
    for (uint32_t i = 0; PM_REGS->PM_SLEEPCFG != mode && i < 1000U; i++) {
    }
    s0 = scope_now();
    __DSB();
    __WFI();
    s = scope_since(s0);
    r1 = rtc_count();
    a1 = tc_count(&T0);
    b1 = tc_count(&T1);
    PM_REGS->PM_SLEEPCFG = IDLE;
    console_puts(name);
    put_field("sleepcfg", mode, 2);
    put_field("rtc", r1 - r0, 4);
    put_field("systick", s, 8);
    put_field("tc0", (a1 - a0) & 0xFFFFU, 4);
    put_field("tc1", (b1 - b0) & 0xFFFFU, 4);
    put_field("woke", woke, 1);
    nl();
    T2.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(&T2);
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP sleepprobe: " BOARD_BANNER "\r\n");
    console_puts("S7");
    put_field("sleepcfg", PM_REGS->PM_SLEEPCFG, 2);
    put_field("stdbycfg", PM_REGS->PM_STDBYCFG, 4);
    put_field("dsu_statusb", DSU_REGS->DSU_STATUSB, 2);
    put_field("osc48mctrl", OSCCTRL_REGS->OSCCTRL_OSC48MCTRL, 2);
    nl();

    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_RTC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk |
                                MCLK_APBCMASK_TC2_Msk;
    gclk1(0);
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC2_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TC2_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    rtc_sync();
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_PRESCALER(1) |
                   RTC_MODE0_CTRLA_COUNTSYNC_Msk;
    rtc_sync();
    M0.RTC_INTENSET = RTC_MODE0_INTENSET_CMP0_Msk;
    M0.RTC_CTRLA |= RTC_MODE0_CTRLA_ENABLE_Msk;
    rtc_sync();
    NVIC_EnableIRQ(RTC_IRQn);
    NVIC_EnableIRQ(TC2_IRQn);
    tc_start(&T0, 0);
    tc_start(&T1, TC_CTRLA_RUNSTDBY_Msk);
    scope_start();

    sleep_case("S1", IDLE);
    sleep_case("S2", STANDBY);
    tc2_arm(0);
    sleep_case("S3", STANDBY);
    tc2_arm(TC_CTRLA_RUNSTDBY_Msk);
    sleep_case("S4", STANDBY);
    osc48m_runstdby(1);
    tc2_arm(TC_CTRLA_RUNSTDBY_Msk);
    sleep_case("S5", STANDBY);
    gclk1(GCLK_GENCTRL_RUNSTDBY_Msk);
    tc2_arm(TC_CTRLA_RUNSTDBY_Msk);
    sleep_case("S6", STANDBY);
    tc2_arm(0);
    sleep_case("S1b", IDLE);

    osc48m_runstdby(0);
    gclk1(0);
    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_DisableIRQ(TC2_IRQn);
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    rtc_sync();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
