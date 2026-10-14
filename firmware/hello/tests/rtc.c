/*
 * RTC test (PIC32CM MC00 RTC U2250): counting rate, read synchronization,
 * periodic flags, compare with clear-on-match, the 16-bit mode, the
 * calendar across a leap day, the alarm, the interrupt and synchronization
 * without the RTC clock.
 *
 * CLK_RTC_OSC is ULP1K (1.024 kHz), the reset selection of RTCCTRL; with
 * PRESCALER DIV1 the counter also runs at 1024 Hz, so the calendar of
 * MODE2 runs 1024 times fast. Timing results are checks with tolerances.
 *
 * Seen on silicon: the RTC keeps running across system resets (only
 * power-on resets it), so the test starts with a software reset; reads
 * with COUNTSYNC lag a few CLK_RTC_OSC cycles (milliseconds at 1 kHz), and
 * reads without it are not valid, so neither is tested exactly.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define M0              (RTC_REGS->MODE0)
#define M1              (RTC_REGS->MODE1)
#define M2              (RTC_REGS->MODE2)
#define DIV1            RTC_MODE0_CTRLA_PRESCALER(1)

static volatile unsigned int cmp_irqs;

void RTC_Handler(void)
{
    M0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
    cmp_irqs++;
}

static void sync(void)
{
    while (M0.RTC_SYNCBUSY != 0U) {
    }
}

static void rtc_reset(uint16_t ctrla)
{
    M0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    sync();
    M0.RTC_CTRLA = ctrla;
    sync();
}

static void rtc_enable(void)
{
    M0.RTC_CTRLA |= RTC_MODE0_CTRLA_ENABLE_Msk;
    sync();
}

static uint32_t wait_flag(uint16_t mask, uint32_t ms)
{
    while (ms > 0U) {
        if (M0.RTC_INTFLAG & mask) {
            return 1;
        }
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    return (M0.RTC_INTFLAG & mask) ? 1U : 0U;
}

static void eol(void)
{
    console_puts("\r\n");
}

static uint32_t clock_value(unsigned y, unsigned mo, unsigned d, unsigned h,
                            unsigned mi, unsigned s)
{
    return RTC_MODE2_CLOCK_YEAR(y) | RTC_MODE2_CLOCK_MONTH(mo) |
           RTC_MODE2_CLOCK_DAY(d) | RTC_MODE2_CLOCK_HOUR(h) |
           RTC_MODE2_CLOCK_MINUTE(mi) | RTC_MODE2_CLOCK_SECOND(s);
}

static void test_reset(void)
{
    rtc_reset(0);
    console_puts("after swrst:");
    put_checked("CTRLA", M0.RTC_CTRLA, 0, 4);
    put_checked("SYNCBUSY", M0.RTC_SYNCBUSY, 0, 2);
    put_checked("RTCSEL", OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL, 0, 2);
    eol();
}

/* MODE0 at 1024 Hz: about 102 counts in 100 ms (ULP1K is not exact) */
static void test_mode0(void)
{
    uint32_t a, b;

    rtc_reset(DIV1 | RTC_MODE0_CTRLA_COUNTSYNC_Msk);
    rtc_enable();
    delay_ms(10);
    a = M0.RTC_COUNT;
    delay_ms(100);
    b = M0.RTC_COUNT;
    console_puts("mode0:");
    put_checked("rate-100ms-ok", b - a >= 92U && b - a <= 112U, 1, 1);
    eol();
}

/* PERn every 2^(n+3) cycles: PER0-2 within 40 ms, PER7 (1 Hz) not */
static void test_periodic(void)
{
    uint16_t f;

    rtc_reset(DIV1);
    rtc_enable();
    M0.RTC_INTFLAG = 0xFFFF;
    delay_ms(40);
    f = M0.RTC_INTFLAG;
    console_puts("periodic:");
    put_checked("per0-2", f & 0x07U, 0x07, 2);
    put_checked("per7", (f >> 7) & 1U, 0, 1);
    eol();
}

/* COMP0 = 20 with MATCHCLR: CMP0 and OVF together, counter restarts */
static void test_matchclr(void)
{
    uint32_t cmp, f, cnt;

    rtc_reset(DIV1 | RTC_MODE0_CTRLA_MATCHCLR_Msk |
              RTC_MODE0_CTRLA_COUNTSYNC_Msk);
    M0.RTC_COMP = 20;
    sync();
    rtc_enable();
    M0.RTC_INTFLAG = 0xFFFF;
    cmp = wait_flag(RTC_MODE0_INTFLAG_CMP0_Msk, 100);
    f = M0.RTC_INTFLAG & (RTC_MODE0_INTFLAG_CMP0_Msk |
                          RTC_MODE0_INTFLAG_OVF_Msk);
    delay_ms(8);
    cnt = M0.RTC_COUNT;             /* about 8, less the read lag */
    console_puts("matchclr:");
    put_checked("cmp0", cmp, 1, 1);
    put_checked("cmp0+ovf", f, RTC_MODE0_INTFLAG_CMP0_Msk |
                RTC_MODE0_INTFLAG_OVF_Msk, 4);
    put_checked("restarted", cnt <= 12U, 1, 1);
    eol();
}

/* MODE1: PER = 50, COMP0 = 10, COMP1 = 60 (above PER: never) */
static void test_mode1(void)
{
    uint32_t ovf, f;

    rtc_reset(RTC_MODE0_CTRLA_MODE(1) | DIV1);
    M1.RTC_PER = 50;
    M1.RTC_COMP[0] = 10;
    M1.RTC_COMP[1] = 60;
    sync();
    rtc_enable();
    M1.RTC_INTFLAG = 0xFFFF;
    ovf = wait_flag(RTC_MODE1_INTFLAG_OVF_Msk, 100);
    f = M1.RTC_INTFLAG;
    console_puts("mode1:");
    put_checked("ovf", ovf, 1, 1);
    put_checked("cmp0", (f >> 8) & 1U, 1, 1);
    put_checked("cmp1", (f >> 9) & 1U, 0, 1);
    eol();
}

/* MODE2: 23:59:58 Feb 28th, year 0 (leap) and year 1; alarm HH:MM:SS */
static void test_calendar(void)
{
    uint32_t leap, plain, alarm;

    rtc_reset(RTC_MODE0_CTRLA_MODE(2) | DIV1 | RTC_MODE2_CTRLA_CLOCKSYNC_Msk);
    M2.RTC_CLOCK = clock_value(0, 2, 28, 23, 59, 58);
    sync();
    rtc_enable();
    delay_ms(20);                   /* about twenty "seconds" */
    leap = M2.RTC_CLOCK & 0xFFFE0000U;      /* the date part */

    M2.RTC_CTRLA &= ~RTC_MODE0_CTRLA_ENABLE_Msk;
    sync();
    M2.RTC_CLOCK = clock_value(1, 2, 28, 23, 59, 58);
    M2.RTC_ALARM = clock_value(0, 0, 0, 0, 0, 40);
    M2.RTC_MASK = RTC_MODE2_MASK_SEL(3);   /* HHMMSS */
    sync();
    M2.RTC_INTFLAG = 0xFFFF;
    rtc_enable();
    delay_ms(20);
    plain = M2.RTC_CLOCK & 0xFFFE0000U;
    alarm = wait_flag(RTC_MODE2_INTFLAG_ALARM0_Msk, 100);
    console_puts("calendar:");
    put_checked("leap", leap, clock_value(0, 2, 29, 0, 0, 0), 8);
    put_checked("plain", plain, clock_value(1, 3, 1, 0, 0, 0), 8);
    put_checked("alarm", alarm, 1, 1);
    eol();
}

static void test_irq(void)
{
    uint32_t ms = 100;

    rtc_reset(DIV1 | RTC_MODE0_CTRLA_MATCHCLR_Msk);
    M0.RTC_COMP = 9;
    sync();
    M0.RTC_INTENSET = RTC_MODE0_INTENSET_CMP0_Msk;
    cmp_irqs = 0;
    NVIC_ClearPendingIRQ(RTC_IRQn);
    NVIC_EnableIRQ(RTC_IRQn);
    rtc_enable();
    while (cmp_irqs < 3U && ms > 0U) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    NVIC_DisableIRQ(RTC_IRQn);
    M0.RTC_INTENCLR = RTC_MODE0_INTENCLR_CMP0_Msk;
    console_puts("irq:");
    put_checked("three-cmp0", cmp_irqs >= 3U, 1, 1);
    eol();
}

/* CLK_RTC_OSC off (OSC1K with OSC32K disabled): ENABLE does not sync */
static void test_noclock(void)
{
    uint32_t busy, done;

    rtc_reset(DIV1);
    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL_OSC1K;
    M0.RTC_CTRLA |= RTC_MODE0_CTRLA_ENABLE_Msk;
    delay_ms(1);
    busy = (M0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_ENABLE_Msk) ? 1U : 0U;
    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL_ULP1K;
    sync();
    done = M0.RTC_SYNCBUSY;
    console_puts("no clock:");
    put_checked("enable-busy", busy, 1, 1);
    put_checked("after-clock", done, 0, 2);
    eol();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP rtc: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_RTC_Msk;

    test_reset();
    test_mode0();
    test_periodic();
    test_matchclr();
    test_mode1();
    test_calendar();
    test_irq();
    test_noclock();
    test_exit();
}
