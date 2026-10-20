/*
 * TSENS probe (PIC32CM MC00, TSENS U2261): what the temperature sensor does
 * on silicon, for the model. Bare EV15N46A, no wiring. Prints raw results,
 * one experiment per line (plan in docs/research/peripherals/tsens-u2261.md):
 *
 *   T0  calibration area          T8  START while disabled
 *   T1  reset values              T9  no GCLK_TSENS
 *   T2  Harmony's init            T10 window modes
 *   T3  single conversions        T11 WINMON event (PORT toggle on PA24)
 *   T4  flag clearing             T12 START event, STARTINV
 *   T5  OVERRUN                   T14 transfer function (GAIN, OFFSET, GCLK)
 *   T6  START while converting    T15 uncalibrated, T16 overflow
 *   T7  FREERUN                   T17 SWRST retention, T18 enable protection
 *
 * Times are microseconds from SysTick at 48 MHz, counting wraps.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define TS              TSENS_REGS
#define PA              (&PORT_REGS->GROUP[0])
#define TEMP_LOG        ((volatile const uint32_t *)0x00806030U)

#define ENABLE          0x02U
#define SWRST           0x01U
#define FREERUN         0x10U
#define STARTEI         0x01U
#define STARTINV        0x02U
#define WINEO           0x04U
#define RESRDY          0x01U
#define OVERRUN         0x02U
#define WINMON          0x04U
#define OVF             0x08U

static uint32_t cal, gain, offset;

static void nl(void)
{
    console_puts("\r\n");
}

static void sync(void)
{
    for (uint32_t n = 0; TS->TSENS_SYNCBUSY != 0U && n < 1000000U; n++) {
    }
}

static void ctrla(uint8_t v)
{
    TS->TSENS_CTRLA = v;
    sync();
}

/* Wait for @mask in INTFLAG, at most @ms; returns microseconds or ~0 */
static uint32_t wait_flag(uint8_t mask, uint32_t ms)
{
    uint32_t wraps = 0, load = SysTick->LOAD + 1U, t;

    SysTick->VAL = 0U;
    (void)SysTick->CTRL;
    while (!(TS->TSENS_INTFLAG & mask)) {
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
            if (++wraps > ms) {
                return 0xFFFFFFFFU;
            }
        }
    }
    t = load - SysTick->VAL;
    return wraps * 1000U + t / (CPU_CLOCK_HZ / 1000000U);
}

static int32_t sext(uint32_t v)
{
    return (int32_t)(v << 8) >> 8;
}

static void put_value(const char *name, uint32_t v)
{
    int32_t s = sext(v);

    put_field(name, v, 6);
    console_puts(s < 0 ? "(-" : "(");
    console_put_dec((uint32_t)(s < 0 ? -s : s));
    console_putc(')');
}

/* Disabled, the calibration written as Harmony does, then @ctrlc/@evctrl */
static void setup(uint32_t g, uint32_t o, uint32_t c, uint8_t ctrlc,
                  uint8_t evctrl)
{
    ctrla(0U);
    TS->TSENS_GAIN = g;
    TS->TSENS_OFFSET = o;
    TS->TSENS_CAL = c;
    TS->TSENS_CTRLC = ctrlc;
    TS->TSENS_EVCTRL = evctrl;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
}

/* One START, RESRDY awaited; VALUE read (clears RESRDY) */
static uint32_t convert(uint32_t *us)
{
    uint32_t t;

    TS->TSENS_CTRLB = 1U;
    t = wait_flag(RESRDY | OVF, 3000U);
    if (us) {
        *us = t;
    }
    return TS->TSENS_VALUE;
}

static void t0_t2(void)
{
    uint32_t lo = TEMP_LOG[0], hi = TEMP_LOG[1];

    console_puts("T0 cal");
    put_field("w0", lo, 8);
    put_field("w1", hi, 8);
    put_field("tcal", lo & 0x3FU, 2);
    put_field("fcal", (lo >> 6) & 0x3FU, 2);
    gain = ((hi & 0xFU) << 20) | (lo >> 12);
    offset = hi >> 4;
    cal = (lo & 0x3FU) << 8 | ((lo >> 6) & 0x3FU);
    put_field("gain", gain, 6);
    put_field("offset", offset, 8);
    nl();
    console_puts("T1 reset");
    put_field("ctrla", TS->TSENS_CTRLA, 2);
    put_field("ctrlc", TS->TSENS_CTRLC, 2);
    put_field("evctrl", TS->TSENS_EVCTRL, 2);
    put_field("inten", TS->TSENS_INTENSET, 2);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    put_field("status", TS->TSENS_STATUS, 2);
    put_field("value", TS->TSENS_VALUE, 8);
    put_field("winlt", TS->TSENS_WINLT, 8);
    put_field("gain", TS->TSENS_GAIN, 8);
    put_field("offset", TS->TSENS_OFFSET, 8);
    put_field("cal", TS->TSENS_CAL, 8);
    put_field("dbgctrl", TS->TSENS_DBGCTRL, 2);
    nl();
    console_puts("T2 init");
    ctrla(SWRST);
    TS->TSENS_CAL = cal;
    TS->TSENS_GAIN = gain;
    TS->TSENS_OFFSET = offset;              /* bits 27:24 = 0xF, as Harmony */
    put_field("cal", TS->TSENS_CAL, 8);
    put_field("gain", TS->TSENS_GAIN, 8);
    put_field("offset", TS->TSENS_OFFSET, 8);
    put_field("syncbusy", TS->TSENS_SYNCBUSY, 8);
    TS->TSENS_GAIN = 0xFFFFFFFFU;
    put_field("gain_mask", TS->TSENS_GAIN, 8);
    TS->TSENS_GAIN = gain;
    TS->TSENS_WINLT = 0xFFFFFFFFU;
    put_field("winlt_mask", TS->TSENS_WINLT, 8);
    TS->TSENS_WINLT = 0U;
    TS->TSENS_CTRLC = 0xFFU;
    put_field("ctrlc_mask", TS->TSENS_CTRLC, 2);
    TS->TSENS_EVCTRL = 0xFFU;
    put_field("evctrl_mask", TS->TSENS_EVCTRL, 2);
    TS->TSENS_INTENSET = 0xFFU;
    put_field("inten_mask", TS->TSENS_INTENSET, 2);
    TS->TSENS_INTENCLR = 0xFFU;
    TS->TSENS_CAL = 0xFFFFFFFFU;
    put_field("cal_mask", TS->TSENS_CAL, 8);
    TS->TSENS_CAL = cal;
    nl();
}

static int32_t t3_single(void)
{
    int32_t sum = 0, mn = 0x7FFFFFFF, mx = -0x7FFFFFFF;
    uint32_t us, tmin = ~0U, tmax = 0;

    setup(gain, offset, cal, 0U, 0U);
    for (unsigned int i = 0; i < 16U; i++) {
        int32_t v = sext(convert(&us));

        sum += v;
        mn = v < mn ? v : mn;
        mx = v > mx ? v : mx;
        tmin = us < tmin ? us : tmin;
        tmax = us > tmax ? us : tmax;
        if (i < 2U) {
            console_puts("T3 first");
            put_field("us", us, 8);
            put_value("value", (uint32_t)v & 0xFFFFFFU);
            put_field("status", TS->TSENS_STATUS, 2);
            nl();
        }
    }
    console_puts("T3 16x");
    put_value("min", (uint32_t)mn & 0xFFFFFFU);
    put_value("max", (uint32_t)mx & 0xFFFFFFU);
    put_value("mean", (uint32_t)(sum / 16) & 0xFFFFFFU);
    put_field("tmin", tmin, 8);
    put_field("tmax", tmax, 8);
    nl();
    return sum / 16;
}

static void t4_t9(void)
{
    uint32_t us;

    console_puts("T4 flags");
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    put_field("before", TS->TSENS_INTFLAG, 2);
    (void)TS->TSENS_VALUE;
    put_field("after_value", TS->TSENS_INTFLAG, 2);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    TS->TSENS_INTFLAG = RESRDY;
    put_field("after_w1c", TS->TSENS_INTFLAG, 2);
    nl();

    console_puts("T5 overrun");
    (void)TS->TSENS_VALUE;
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    TS->TSENS_INTFLAG = RESRDY;             /* cleared, VALUE unread */
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    (void)TS->TSENS_VALUE;
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    put_field("unread_kept", TS->TSENS_INTFLAG, 2);
    TS->TSENS_CTRLB = 1U;
    wait_flag(OVERRUN, 100U);
    put_field("second", TS->TSENS_INTFLAG, 2);
    (void)TS->TSENS_VALUE;
    put_field("after_value", TS->TSENS_INTFLAG, 2);
    nl();

    console_puts("T6 restart");
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    for (volatile unsigned int i = 0; i < 2000U; i++) {   /* ~0.2 ms */
    }
    TS->TSENS_CTRLB = 1U;
    us = wait_flag(RESRDY, 100U);
    put_field("us", us, 8);
    (void)TS->TSENS_VALUE;
    us = wait_flag(RESRDY, 20U);
    put_field("second_us", us, 8);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    (void)TS->TSENS_VALUE;
    nl();

    console_puts("T7 freerun");
    setup(gain, offset, cal, FREERUN, 0U);
    for (unsigned int i = 0; i < 4U; i++) {
        us = wait_flag(RESRDY, 100U);
        (void)TS->TSENS_VALUE;
        put_field("us", us, 8);
    }
    TS->TSENS_INTFLAG = 0x0FU;
    us = wait_flag(OVERRUN, 100U);
    put_field("unread_us", us, 8);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    nl();

    console_puts("T8 disabled");
    ctrla(0U);
    TS->TSENS_CTRLC = 0U;
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    put_field("us", wait_flag(RESRDY, 20U), 8);
    ctrla(ENABLE);
    put_field("enable_us", wait_flag(RESRDY, 20U), 8);
    nl();

    console_puts("T9 nogclk");
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = 0U;
    while (GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk) {
    }
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    put_field("us", wait_flag(RESRDY, 30U), 8);
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                             GCLK_PCHCTRL_CHEN_Msk;
    put_field("chen_us", wait_flag(RESRDY, 30U), 8);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    (void)TS->TSENS_VALUE;
    nl();
}

/* WINMON after one conversion with @mode and thresholds */
static uint32_t win_once(uint8_t mode, int32_t lt, int32_t ut, uint32_t *v)
{
    ctrla(0U);
    TS->TSENS_CTRLC = mode;
    TS->TSENS_WINLT = (uint32_t)lt & 0xFFFFFFU;
    TS->TSENS_WINUT = (uint32_t)ut & 0xFFFFFFU;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    {
        uint32_t f = TS->TSENS_INTFLAG;

        *v = TS->TSENS_VALUE;
        return f;
    }
}

static void t10_t12(int32_t v0)
{
    static const int32_t pairs[3][2] = {
        { -300, 300 }, { 300, 600 }, { -600, -300 },
    };
    uint32_t v;

    for (uint8_t m = 1; m <= 6U; m++) {
        console_puts("T10 mode");
        put_field("m", m, 1);
        for (unsigned int p = 0; p < 3U; p++) {
            put_field("f", win_once(m, v0 + pairs[p][0], v0 + pairs[p][1], &v),
                      2);
        }
        nl();
    }
    console_puts("T10 signed");
    put_field("above_m100", win_once(1, -100, 0, &v), 2);
    put_field("below_m100", win_once(2, 0, -100, &v), 2);
    nl();
    /* Repeated matches, then hysteresis: one mode, thresholds moved */
    console_puts("T10 repeat");
    win_once(1, v0 - 300, v0 + 300, &v);
    for (unsigned int i = 0; i < 2U; i++) {
        TS->TSENS_CTRLB = 1U;
        wait_flag(RESRDY, 100U);
        put_field("f", TS->TSENS_INTFLAG, 2);
        (void)TS->TSENS_VALUE;
    }
    nl();
    console_puts("T10 hyst_above");
    {
        /* LT/UT around V0: above UT, between, below LT, between, above */
        static const int32_t lt[] = { -600, -600, 300, -600, -600 };
        static const int32_t ut[] = { -300, 300, 600, 300, -300 };

        for (unsigned int i = 0; i < 5U; i++) {
            put_field("f", win_once(5, v0 + lt[i], v0 + ut[i], &v), 2);
        }
    }
    nl();
    console_puts("T10 hyst_below");
    {
        static const int32_t lt[] = { 300, -300, -600, -300, 300 };
        static const int32_t ut[] = { 600, 300, -300, 300, 600 };

        for (unsigned int i = 0; i < 5U; i++) {
            put_field("f", win_once(6, v0 + lt[i], v0 + ut[i], &v), 2);
        }
    }
    nl();

    /* T11: WINMON events toggle PA24 (PORT EV0, TGL) */
    console_puts("T11 winmon_event");
    EVSYS_REGS->EVSYS_CHANNEL[0] =
        EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_TSENS_WINMON) |
        EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 1U;
    PA->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                      PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_TGL_Val) |
                      PORT_EVCTRL_PORTEI0_Msk;
    PA->PORT_PINCFG[24] = PORT_PINCFG_INEN_Msk;
    PA->PORT_DIRSET = 1U << 24;
    PA->PORT_OUTCLR = 1U << 24;
    ctrla(0U);
    TS->TSENS_EVCTRL = WINEO;
    TS->TSENS_CTRLC = 1U;                       /* ABOVE */
    TS->TSENS_WINLT = (uint32_t)(v0 - 300) & 0xFFFFFFU;
    ctrla(ENABLE);
    for (unsigned int i = 0; i < 3U; i++) {
        TS->TSENS_CTRLB = 1U;
        wait_flag(RESRDY, 100U);
        (void)TS->TSENS_VALUE;
        put_field("pa24", (PA->PORT_IN >> 24) & 1U, 1);
    }
    ctrla(0U);
    TS->TSENS_CTRLC = 2U;                       /* BELOW v0-300: no match */
    TS->TSENS_WINUT = (uint32_t)(v0 - 300) & 0xFFFFFFU;
    ctrla(ENABLE);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    (void)TS->TSENS_VALUE;
    put_field("nomatch", (PA->PORT_IN >> 24) & 1U, 1);
    PA->PORT_EVCTRL = 0U;
    PA->PORT_OUTSET = 1U << 24;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    nl();

    /* T12: software events on channel 1 start conversions */
    console_puts("T12 start_event");
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TSENS_START] = 2U;
    put_field("usrrdy_off", EVSYS_REGS->EVSYS_CHSTATUS & 0xFFU, 2);
    setup(gain, offset, cal, 0U, STARTEI);
    put_field("usrrdy_on", EVSYS_REGS->EVSYS_CHSTATUS & 0xFFU, 2);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_field("usrrdy_busy", EVSYS_REGS->EVSYS_CHSTATUS & 0xFFU, 2);
    put_field("us", wait_flag(RESRDY, 100U), 8);
    (void)TS->TSENS_VALUE;
    setup(gain, offset, cal, 0U, STARTEI | STARTINV);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_field("inv_us", wait_flag(RESRDY, 100U), 8);
    (void)TS->TSENS_VALUE;
    put_field("inv_again_us", wait_flag(RESRDY, 20U), 8);
    setup(gain, offset, cal, 0U, 0U);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_field("no_startei_us", wait_flag(RESRDY, 20U), 8);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TSENS_START] = 0U;
    nl();
}

static void t14_line(const char *name, uint32_t g, uint32_t o, uint32_t c)
{
    int32_t sum = 0;
    uint32_t us = 0, t;

    setup(g, o, c, 0U, 0U);
    for (unsigned int i = 0; i < 4U; i++) {
        sum += sext(convert(&t));
        us += t;
    }
    console_puts("T14 ");
    console_puts(name);
    put_value("mean", (uint32_t)(sum / 4) & 0xFFFFFFU);
    put_field("us", us / 4U, 8);
    put_field("status", TS->TSENS_STATUS, 2);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    nl();
}

static void t14_t18(void)
{
    uint32_t us, v;

    t14_line("cal", gain, offset, cal);
    t14_line("gain_half", gain / 2U, offset, cal);
    t14_line("gain_double", gain * 2U, offset, cal);
    t14_line("offset0", gain, 0U, cal);
    t14_line("offset_p1000", gain, offset + 1000U, cal);
    t14_line("cal0", gain, offset, 0U);
    t14_line("fcal_p4", gain, offset, cal + 4U);
    t14_line("tcal_p4", gain, offset, cal + (4U << 8));
    /* GCLK_TSENS from GCLK2 = OSC48M / 2 and / 4 */
    for (unsigned int d = 2; d <= 4U; d += 2U) {
        GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_DIV(d) |
                                     GCLK_GENCTRL_SRC_OSC48M |
                                     GCLK_GENCTRL_GENEN_Msk;
        while (GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL2_Msk) {
        }
        GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = 0U;
        while (GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] &
               GCLK_PCHCTRL_CHEN_Msk) {
        }
        GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK2 |
                                                 GCLK_PCHCTRL_CHEN_Msk;
        t14_line(d == 2U ? "gclk_div2" : "gclk_div4", gain, offset, cal);
    }
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = 0U;
    while (GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                             GCLK_PCHCTRL_CHEN_Msk;

    t14_line("T15_uncal", 0U, 0U, 0U);
    t14_line("ovf16", gain, 0x100000U, cal);

    console_puts("T16 overflow");
    setup(0xFFFFFFU, 0x800000U, cal, 0U, 0U);
    TS->TSENS_CTRLB = 1U;
    us = wait_flag(RESRDY | OVF, 3000U);
    put_field("us", us, 8);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    put_field("status", TS->TSENS_STATUS, 2);
    put_value("value", TS->TSENS_VALUE);
    setup(gain, offset, cal, 0U, 0U);
    put_field("status_after_enable", TS->TSENS_STATUS, 2);
    v = convert(&us);
    put_field("status_next", TS->TSENS_STATUS, 2);
    put_value("value", v);
    nl();

    console_puts("T17 swrst");
    ctrla(0U);
    TS->TSENS_GAIN = 0x123456U;
    TS->TSENS_OFFSET = 0x654321U;
    TS->TSENS_CAL = 0x00001234U;
    TS->TSENS_DBGCTRL = 1U;
    TS->TSENS_WINLT = 0x111111U;
    TS->TSENS_CTRLC = 3U;
    TS->TSENS_INTENSET = 1U;
    ctrla(SWRST);
    put_field("gain", TS->TSENS_GAIN, 8);
    put_field("offset", TS->TSENS_OFFSET, 8);
    put_field("cal", TS->TSENS_CAL, 8);
    put_field("dbgctrl", TS->TSENS_DBGCTRL, 2);
    put_field("winlt", TS->TSENS_WINLT, 8);
    put_field("ctrlc", TS->TSENS_CTRLC, 2);
    put_field("inten", TS->TSENS_INTENSET, 2);
    put_value("value", TS->TSENS_VALUE);
    nl();

    console_puts("T18 enprot");
    setup(gain, offset, cal, 0U, 0U);
    TS->TSENS_CTRLC = 3U;
    TS->TSENS_EVCTRL = 1U;
    TS->TSENS_WINLT = 0x222222U;
    TS->TSENS_GAIN = 0x333333U;
    TS->TSENS_OFFSET = 0x444444U;
    TS->TSENS_CAL = 0x0101U;
    TS->TSENS_CTRLA = ENABLE | 0x40U;       /* RUNSTDBY */
    sync();
    put_field("ctrlc", TS->TSENS_CTRLC, 2);
    put_field("evctrl", TS->TSENS_EVCTRL, 2);
    put_field("winlt", TS->TSENS_WINLT, 8);
    put_field("gain", TS->TSENS_GAIN, 8);
    put_field("offset", TS->TSENS_OFFSET, 8);
    put_field("cal", TS->TSENS_CAL, 8);
    put_field("ctrla", TS->TSENS_CTRLA, 2);
    put_field("pac_intflag", PAC_REGS->PAC_INTFLAGB, 8);
    nl();
}

int main(void)
{
    int32_t v0;

    sys_initialize();
    systick_start();
    console_puts("PP tsensprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_TSENS_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                             GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    t0_t2();
    v0 = t3_single();
    t4_t9();
    setup(gain, offset, cal, 0U, 0U);
    t10_t12(v0);
    t14_t18();
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
