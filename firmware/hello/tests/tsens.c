/*
 * TSENS check (PIC32CM MC00, TSENS U2261): the temperature sensor against
 * what silicon did in firmware/hello APP=tsensprobe
 * (docs/research/peripherals/tsens-u2261.md). The reading follows the
 * chip's temperature, so the checks use relations that hold at any
 * temperature: OFFSET adds, VALUE - OFFSET scales with GAIN / f_GCLK, the
 * conversion takes (2 GAIN + 2000) clocks; flags, starts, window modes,
 * events, overflow and register rules are checked exactly. A failed check
 * names what it expected and ends its line in FAIL; PP:EXIT <failures>.
 *
 * Uses PA24 (LED0, which blinks) for the WINMON event.
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
#define TIMEOUT         0xFFFFFFFFU

static uint32_t cal, gain, offset;
static volatile unsigned int irq_count;

void TSENS_Handler(void)
{
    uint8_t f = TS->TSENS_INTFLAG & TS->TSENS_INTENSET;

    TS->TSENS_INTFLAG = f;
    irq_count++;
}

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

static void put_signed(int32_t v)
{
    console_puts(v < 0 ? "-" : "");
    console_put_dec((uint32_t)(v < 0 ? -v : v));
}

/* " NAME=<v>", counting a failure unless within @tol of @expect */
static void put_near(const char *name, int32_t value, int32_t expect,
                     int32_t tol)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    put_signed(value);
    if (value < expect - tol || value > expect + tol) {
        console_puts("(expected ");
        put_signed(expect);
        console_puts("+-");
        console_put_dec((uint32_t)tol);
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

/* Wait for @mask in INTFLAG, at most @ms; microseconds or TIMEOUT */
static uint32_t wait_flag(uint8_t mask, uint32_t ms)
{
    uint32_t wraps = 0, load = SysTick->LOAD + 1U, t;

    SysTick->VAL = 0U;
    (void)SysTick->CTRL;
    while (!(TS->TSENS_INTFLAG & mask)) {
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
            if (++wraps > ms) {
                return TIMEOUT;
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

static void setup(uint32_t g, uint32_t o, uint8_t ctrlc, uint8_t evctrl)
{
    ctrla(0U);
    TS->TSENS_GAIN = g;
    TS->TSENS_OFFSET = o;
    TS->TSENS_CAL = cal;
    TS->TSENS_CTRLC = ctrlc;
    TS->TSENS_EVCTRL = evctrl;
    (void)TS->TSENS_VALUE;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
}

/* Mean of four conversions with @g and @o; time of the last */
static int32_t mean4(uint32_t g, uint32_t o, uint32_t *us)
{
    int32_t sum = 0;

    setup(g, o, 0U, 0U);
    for (unsigned int i = 0; i < 4U; i++) {
        TS->TSENS_CTRLB = 1U;
        *us = wait_flag(RESRDY, 100U);
        sum += sext(TS->TSENS_VALUE);
    }
    return sum / 4;
}

static void gclk_tsens(unsigned int gen)
{
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = 0U;
    while (GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk) {
    }
    if (gen < 16U) {
        GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN(gen) |
                                                 GCLK_PCHCTRL_CHEN_Msk;
    }
}

static void check_registers(void)
{
    uint32_t lo = TEMP_LOG[0], hi = TEMP_LOG[1];

    console_puts("reset");
    put_checked("ctrla", TS->TSENS_CTRLA, 0, 2);
    put_checked("ctrlc", TS->TSENS_CTRLC, 0, 2);
    put_checked("intflag", TS->TSENS_INTFLAG, 0, 2);
    put_checked("value", TS->TSENS_VALUE, 0, 8);
    put_checked("gain", TS->TSENS_GAIN, 0, 8);
    put_checked("cal", TS->TSENS_CAL, 0, 8);
    end();

    /* Harmony's TSENS_Initialize(): the factory calibration */
    cal = (lo & 0x3FU) << 8 | ((lo >> 6) & 0x3FU);
    gain = ((hi & 0xFU) << 20) | (lo >> 12);
    offset = hi >> 4;
    console_puts("calibration");
    put_range("gain", gain, 0x8000, 0x40000, 6);
    ctrla(SWRST);
    TS->TSENS_CAL = cal;
    TS->TSENS_GAIN = gain;
    TS->TSENS_OFFSET = offset;
    put_checked("cal", TS->TSENS_CAL, cal, 8);
    put_checked("offset_24bit", TS->TSENS_OFFSET, offset & 0xFFFFFFU, 8);
    end();

    console_puts("masks");
    TS->TSENS_WINLT = 0xFFFFFFFFU;
    put_checked("winlt", TS->TSENS_WINLT, 0xFFFFFF, 8);
    TS->TSENS_WINLT = 0U;
    TS->TSENS_CTRLC = 0xFFU;
    put_checked("ctrlc", TS->TSENS_CTRLC, 0x17, 2);
    TS->TSENS_CTRLC = 0U;
    TS->TSENS_EVCTRL = 0xFFU;
    put_checked("evctrl", TS->TSENS_EVCTRL, 0x07, 2);
    TS->TSENS_EVCTRL = 0U;
    TS->TSENS_INTENSET = 0xFFU;
    put_checked("inten", TS->TSENS_INTENSET, 0x0F, 2);
    TS->TSENS_INTENCLR = 0xFFU;
    TS->TSENS_CAL = 0xFFFFFFFFU;
    put_checked("cal_mask", TS->TSENS_CAL, 0x3F3F, 8);
    TS->TSENS_CAL = cal;
    end();
}

static int32_t check_conversion(void)
{
    uint32_t us, exp_us = (2U * gain + 2000U) / 48U;
    int32_t v0, v, k;

    console_puts("conversion");
    v0 = mean4(gain, offset, &us);
    put_range("value", (uint32_t)v0, 500, 5000, 6);    /* 5..50 C */
    put_range("us", us, exp_us - exp_us / 50U, exp_us + exp_us / 50U, 8);
    end();

    /* VALUE = OFFSET + GAIN * D / f: silicon within a few counts */
    k = v0 - sext(offset);
    console_puts("transfer");
    put_near("offset0", mean4(gain, 0U, &us), k, 50);
    put_near("offset_p1000", mean4(gain, offset + 1000U, &us), v0 + 1000,
             50);
    v = mean4(gain / 2U, offset, &us);
    put_near("gain_half", v - sext(offset), k / 2, 50);
    put_range("gain_half_us", us, exp_us / 2U - exp_us / 50U,
              exp_us / 2U + exp_us / 50U, 8);
    GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_DIV(2) |
                                 GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_GENEN_Msk;
    while (GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL2_Msk) {
    }
    gclk_tsens(2);
    v = mean4(gain, offset, &us);
    put_near("gclk_half", v - sext(offset), 2 * k, 100);
    put_range("gclk_half_us", us, 2U * exp_us - exp_us / 25U,
              2U * exp_us + exp_us / 25U, 8);
    gclk_tsens(0);
    end();
    return v0;
}

static void check_flags(void)
{
    uint32_t us, exp_us = (2U * gain + 2000U) / 48U;

    console_puts("flags");
    setup(gain, offset, 0U, 0U);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    put_checked("resrdy", TS->TSENS_INTFLAG, RESRDY, 2);
    (void)TS->TSENS_VALUE;
    put_checked("value_read", TS->TSENS_INTFLAG, 0, 2);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    TS->TSENS_INTFLAG = RESRDY;                     /* VALUE unread */
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    put_checked("overrun", TS->TSENS_INTFLAG, RESRDY | OVERRUN, 2);
    (void)TS->TSENS_VALUE;
    put_checked("overrun_stays", TS->TSENS_INTFLAG, OVERRUN, 2);
    end();

    console_puts("starts");
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;
    delay_us(200);
    TS->TSENS_CTRLB = 1U;                           /* while converting */
    wait_flag(RESRDY, 100U);
    (void)TS->TSENS_VALUE;
    put_checked("busy_start", TS->TSENS_INTFLAG, 0, 2);     /* ignored */
    put_checked("second", wait_flag(RESRDY, 20U), TIMEOUT, 8);
    ctrla(0U);
    TS->TSENS_INTFLAG = 0x0FU;
    TS->TSENS_CTRLB = 1U;                           /* while disabled */
    put_checked("disabled", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    ctrla(ENABLE);
    put_checked("dropped", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    TS->TSENS_CTRLB = 1U;                           /* disabled mid-way */
    delay_ms(1);
    ctrla(0U);
    ctrla(ENABLE);
    put_checked("aborted", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    gclk_tsens(16);                                 /* GCLK_TSENS off */
    TS->TSENS_CTRLB = 1U;
    put_checked("no_gclk", wait_flag(RESRDY, 20U), TIMEOUT, 8);
    gclk_tsens(0);
    put_checked("gclk_back", wait_flag(RESRDY, 20U) != TIMEOUT, 1, 1);
    (void)TS->TSENS_VALUE;
    end();

    console_puts("freerun");
    setup(gain, offset, FREERUN, 0U);
    wait_flag(RESRDY, 100U);
    (void)TS->TSENS_VALUE;
    us = wait_flag(RESRDY, 100U);
    (void)TS->TSENS_VALUE;
    put_range("period_us", us, exp_us - exp_us / 25U, exp_us + exp_us / 25U,
              8);
    /* Disabled while FREERUN converts: one more conversion at enable */
    ctrla(0U);
    TS->TSENS_CTRLC = 0U;
    (void)TS->TSENS_VALUE;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
    us = wait_flag(RESRDY, 10U);
    put_range("reenable_us", us, exp_us - exp_us / 25U,
              exp_us + exp_us / 25U, 8);
    (void)TS->TSENS_VALUE;
    put_checked("then", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    end();

    console_puts("irq");
    setup(gain, offset, 0U, 0U);
    irq_count = 0;
    TS->TSENS_INTENSET = RESRDY;
    NVIC_ClearPendingIRQ(TSENS_IRQn);
    NVIC_EnableIRQ(TSENS_IRQn);
    for (unsigned int i = 0; i < 3U; i++) {
        TS->TSENS_CTRLB = 1U;
        delay_ms(6);                /* >= 5 ms: delay_ms(5) can be 3.4 */
    }
    NVIC_DisableIRQ(TSENS_IRQn);
    TS->TSENS_INTENCLR = 0x0FU;
    put_checked("count", irq_count, 3, 2);
    end();
}

static uint32_t win_once(uint8_t mode, int32_t lt, int32_t ut)
{
    uint32_t f;

    ctrla(0U);
    TS->TSENS_CTRLC = mode;
    TS->TSENS_WINLT = (uint32_t)lt & 0xFFFFFFU;
    TS->TSENS_WINUT = (uint32_t)ut & 0xFFFFFFU;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    f = TS->TSENS_INTFLAG;
    (void)TS->TSENS_VALUE;
    return f;
}

static void check_window(int32_t v0)
{
    static const int32_t pairs[3][2] = {
        { -300, 300 }, { 300, 600 }, { -600, -300 },
    };
    /* silicon: INTFLAG (RESRDY 0x01, WINMON 0x04) per mode and pair */
    static const uint8_t expect[6][3] = {
        { 5, 1, 5 }, { 5, 5, 1 }, { 5, 1, 1 },
        { 1, 5, 5 }, { 1, 1, 5 }, { 1, 5, 1 },
    };

    setup(gain, offset, 0U, 0U);
    for (uint8_t m = 1; m <= 6U; m++) {
        console_puts("window");
        put_field("mode", m, 1);
        for (unsigned int p = 0; p < 3U; p++) {
            put_checked("f", win_once(m, v0 + pairs[p][0], v0 + pairs[p][1]),
                        expect[m - 1U][p], 2);
        }
        end();
    }
    console_puts("window_signed");
    put_checked("above_m100", win_once(1, -100, 0), 5, 2);
    put_checked("below_m100", win_once(2, 0, -100), 1, 2);
    end();

    /* A WINMON pulse per matching result toggles PA24 (PORT EV0, TGL) */
    console_puts("winmon_event");
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
    TS->TSENS_CTRLC = 1U;
    TS->TSENS_WINLT = (uint32_t)(v0 - 300) & 0xFFFFFFU;
    ctrla(ENABLE);
    for (unsigned int i = 0; i < 2U; i++) {
        TS->TSENS_CTRLB = 1U;
        wait_flag(RESRDY, 100U);
        (void)TS->TSENS_VALUE;
        put_checked("pa24", (PA->PORT_IN >> 24) & 1U, i == 0U ? 1 : 0, 1);
    }
    ctrla(0U);
    TS->TSENS_CTRLC = 2U;                           /* BELOW: no match */
    TS->TSENS_WINUT = (uint32_t)(v0 - 300) & 0xFFFFFFU;
    ctrla(ENABLE);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY, 100U);
    (void)TS->TSENS_VALUE;
    put_checked("nomatch", (PA->PORT_IN >> 24) & 1U, 0, 1);
    PA->PORT_EVCTRL = 0U;
    PA->PORT_OUTSET = 1U << 24;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    end();

    /* Software events on channel 1 start conversions */
    console_puts("start_event");
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TSENS_START] = 2U;
    setup(gain, offset, 0U, STARTEI);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_checked("startei", wait_flag(RESRDY, 20U) != TIMEOUT, 1, 1);
    (void)TS->TSENS_VALUE;
    setup(gain, offset, 0U, STARTEI | STARTINV);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_checked("startinv", wait_flag(RESRDY, 20U) != TIMEOUT, 1, 1);
    (void)TS->TSENS_VALUE;
    put_checked("once", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    setup(gain, offset, 0U, 0U);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    put_checked("no_startei", wait_flag(RESRDY, 10U), TIMEOUT, 8);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TSENS_START] = 0U;
    end();
}

static void check_limits(void)
{
    uint32_t us;

    /* GAIN = 0 counts as 2^24: about 0.7 s at 48 MHz */
    console_puts("gain0");
    setup(0U, 0U, 0U, 0U);
    TS->TSENS_CTRLB = 1U;
    us = wait_flag(RESRDY | OVF, 1500U);
    put_range("ms", us / 1000U, 690, 710, 4);
    (void)TS->TSENS_VALUE;
    end();

    /* Beyond 24 bits: OVF and STATUS.OVF, no RESRDY */
    console_puts("overflow");
    setup(0xFFFFFFU, 0x800000U, 0U, 0U);
    TS->TSENS_CTRLB = 1U;
    wait_flag(RESRDY | OVF, 1500U);
    put_checked("intflag", TS->TSENS_INTFLAG, OVF, 2);
    put_checked("status", TS->TSENS_STATUS, 1, 2);
    setup(gain, offset, 0U, 0U);
    put_checked("status_reenabled", TS->TSENS_STATUS, 0, 2);
    end();

    console_puts("swrst");
    ctrla(0U);
    TS->TSENS_GAIN = 0x123456U;
    TS->TSENS_OFFSET = 0x654321U;
    TS->TSENS_CAL = 0x00001234U;
    TS->TSENS_DBGCTRL = 1U;
    TS->TSENS_WINLT = 0x111111U;
    TS->TSENS_CTRLC = 3U;
    ctrla(SWRST);
    put_checked("gain", TS->TSENS_GAIN, 0x123456, 8);
    put_checked("offset", TS->TSENS_OFFSET, 0x654321, 8);
    put_checked("cal", TS->TSENS_CAL, 0x1234, 8);
    put_checked("dbgctrl", TS->TSENS_DBGCTRL, 1, 2);
    put_checked("winlt", TS->TSENS_WINLT, 0, 8);
    put_checked("ctrlc", TS->TSENS_CTRLC, 0, 2);
    end();

    console_puts("enable_protected");
    setup(gain, offset, 0U, 0U);
    TS->TSENS_CTRLC = 3U;
    TS->TSENS_EVCTRL = 1U;
    TS->TSENS_WINLT = 0x222222U;
    TS->TSENS_GAIN = 0x333333U;
    TS->TSENS_OFFSET = 0x444444U;
    TS->TSENS_CAL = 0x0101U;
    TS->TSENS_CTRLA = ENABLE | 0x40U;
    sync();
    put_checked("ctrlc", TS->TSENS_CTRLC, 0, 2);
    put_checked("evctrl", TS->TSENS_EVCTRL, 0, 2);
    put_checked("winlt", TS->TSENS_WINLT, 0, 8);
    put_checked("gain", TS->TSENS_GAIN, gain, 8);
    put_checked("offset", TS->TSENS_OFFSET, offset & 0xFFFFFFU, 8);
    put_checked("cal", TS->TSENS_CAL, cal, 8);
    put_checked("ctrla", TS->TSENS_CTRLA, ENABLE, 2);
    end();
}

int main(void)
{
    int32_t v0;

    sys_initialize();
    systick_start();
    console_puts("PP tsens: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_TSENS_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk;
    gclk_tsens(0);
    while (!(GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    check_registers();
    v0 = check_conversion();
    console_puts("temperature");
    put_field("centi_c", (uint32_t)v0, 4);
    end();
    check_flags();
    check_window(v0);
    check_limits();
    test_exit();
}
