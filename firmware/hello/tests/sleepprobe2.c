/*
 * Sleep probe 2 (PIC32CM MC00, PM): standby case by case, each counter on
 * its own GCLK channel and generator (sleepprobe's TC0 shared TC1's), the
 * wake-up timer armed right before WFI. Prints raw results; checks
 * nothing.
 * Run: py -m tools.periph probe mc00 sleepprobe2
 *
 * Bare EV15N46A, the debugger attached. The RTC (ULP1K, 1024 Hz) wakes
 * the core with CMP0 51 counts (about 50 ms) after the sleep starts.
 * Counters at 62.5 kHz (OSC48M / 48 / DIV16): TC0 on GCLK1 without
 * RUNSTDBY, TC2 on GCLK2 with RUNSTDBY; TC4 on GCLK3 raises MC0 5 ms
 * (312 counts) after it starts. Each line: RTC counts asleep, SysTick
 * cycles (CPU clock), TC0 and TC2 counts, TC4's count at the wake-up,
 * which handlers ran (bit 0 RTC, bit 1 TC4).
 *
 *   A1  IDLE, RTC wake-up
 *   A2  STANDBY, RTC wake-up (OSC48M RUNSTDBY 0, GCLKs RUNSTDBY 0)
 *   A3  as A2, again
 *   A4  STANDBY, OSC48M RUNSTDBY 1
 *   A5  STANDBY, OSC48M RUNSTDBY 1, GENCTRL1.RUNSTDBY 1
 *   A6  IDLE, TC4 without RUNSTDBY armed
 *   A7  STANDBY, TC4 without RUNSTDBY armed (OSC48M RUNSTDBY 1)
 *   A8  STANDBY, TC4 with RUNSTDBY armed (OSC48M RUNSTDBY 1)
 *   A9  STANDBY, TC4 with RUNSTDBY armed, OSC48M RUNSTDBY 0
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define M0              (RTC_REGS->MODE0)
#define T0              (TC0_REGS->COUNT16)
#define T2              (TC2_REGS->COUNT16)
#define T4              (TC4_REGS->COUNT16)
#define IDLE            2U
#define STANDBY         4U
#define TC_DIV16        TC_CTRLA_PRESCALER_DIV16
#define RUNSTDBY        TC_CTRLA_RUNSTDBY_Msk
#define WAKE_COUNTS     51U
#define NO_TC4          0xFFFFFFFFU

static volatile uint32_t woke;

void RTC_Handler(void)
{
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    woke |= 1U;
}

void TC4_Handler(void)
{
    T4.TC_INTFLAG = TC_INTFLAG_MC0_Msk;
    T4.TC_INTENCLR = TC_INTENCLR_MC0_Msk;
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

static void gclk(unsigned int n, uint32_t runstdby)
{
    GCLK_REGS->GCLK_GENCTRL[n] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk | runstdby;
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

static void osc48m_runstdby(int on)
{
    uint8_t v = OSCCTRL_REGS->OSCCTRL_OSC48MCTRL;

    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL = on ?
        (v | OSCCTRL_OSC48MCTRL_RUNSTDBY_Msk) :
        (v & ~OSCCTRL_OSC48MCTRL_RUNSTDBY_Msk);
}

/*
 * One sleep in @mode; @tc4 is NO_TC4 or TC4's RUNSTDBY (0 or RUNSTDBY):
 * TC4 then starts right before WFI and raises MC0 312 counts later
 */
static void sleep_case(const char *name, uint32_t mode, uint32_t tc4)
{
    uint32_t r0, r1, a0, a1, b0, b1, s0, s, c4 = 0;

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
    s = scope_since(s0);
    if (tc4 != NO_TC4) {
        c4 = tc_count(&T4);
    }
    r1 = rtc_count();
    a1 = tc_count(&T0);
    b1 = tc_count(&T2);
    PM_REGS->PM_SLEEPCFG = IDLE;
    console_puts(name);
    put_field("rtc", r1 - r0, 4);
    put_field("systick", s, 8);
    put_field("tc0", (a1 - a0) & 0xFFFFU, 4);
    put_field("tc2", (b1 - b0) & 0xFFFFU, 4);
    put_field("tc4", c4, 4);
    put_field("woke", woke, 1);
    nl();
    T4.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    tc_sync(&T4);
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP sleepprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_RTC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC2_Msk |
                                MCLK_APBCMASK_TC4_Msk;
    gclk(1, 0);
    gclk(2, 0);
    gclk(3, 0);
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

    sleep_case("A1", IDLE, NO_TC4);
    sleep_case("A2", STANDBY, NO_TC4);
    sleep_case("A3", STANDBY, NO_TC4);
    osc48m_runstdby(1);
    sleep_case("A4", STANDBY, NO_TC4);
    gclk(1, GCLK_GENCTRL_RUNSTDBY_Msk);
    sleep_case("A5", STANDBY, NO_TC4);
    gclk(1, 0);
    sleep_case("A6", IDLE, 0);
    sleep_case("A7", STANDBY, 0);
    sleep_case("A8", STANDBY, RUNSTDBY);
    osc48m_runstdby(0);
    sleep_case("A9", STANDBY, RUNSTDBY);

    NVIC_DisableIRQ(RTC_IRQn);
    NVIC_DisableIRQ(TC4_IRQn);
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    rtc_sync();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
