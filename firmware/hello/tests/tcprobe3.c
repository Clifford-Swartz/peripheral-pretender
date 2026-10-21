/*
 * TC probe 3 (PIC32CM MC00, TC U2249): what tcprobe2 left open
 * (docs/research/peripherals/tc-u2249.md, 5): MCx after a CCx read that
 * moves a buffered capture up, capture on pin (COPEN), and the overflow
 * capture of STAMP on both channels. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tcprobe3
 *
 *   D1  two captures, read CC0, INTFLAG at once, 2 us and 20 us later
 *   D2  COPEN0 on TC2 WO0 (PB02): the pin under its pull, up and down,
 *       with the TC enabled, CAPTEN0 alone, COPEN0, disabled
 *   D3  COPEN0 captures: STAMP and OFF, rising edges at known times
 *   D4  STAMP overflow capture with CAPTEN0 and CAPTEN1
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T3              (TC3_REGS->COUNT8)
#define T2              (TC2_REGS->COUNT16)
#define GB              1U
#define STIM            9U                      /* PB09, EXTINT9 */
#define COP             2U                      /* PB02, TC2 WO0 */
#define FUNC_A          0U
#define FUNC_E          4U
#define GEN_EIC9        23U
#define USER_TC3        26U
#define C8_DIV64        (TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV64)
#define C16_DIV64       (TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64)

static void nl(void)
{
    console_puts("\r\n");
}

static void put_dec(const char *name, uint32_t v)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    console_put_dec(v);
}

static void wait_us(uint32_t us)
{
    scope_wait_until(scope_now(), SCOPE_CYC(us));
}

static void sync3(void)
{
    for (uint32_t i = 0; T3.TC_SYNCBUSY != 0U && i < 100000U; i++) {
    }
}

static void sync2(void)
{
    for (uint32_t i = 0; T2.TC_SYNCBUSY != 0U && i < 100000U; i++) {
    }
}

static void pulse(uint32_t hi)
{
    scope_pull(GB, STIM, 1);
    wait_us(hi);
    scope_pull(GB, STIM, 0);
}

static void stim_setup(void)
{
    scope_pmux(GB, STIM, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GB, STIM, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG1 = 0x4U << (4U * (STIM - 8U));     /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << STIM;
    EIC_REGS->EIC_EVCTRL = 1U << STIM;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC9) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_TC3] = 1U;
}

static void t3_stamp(uint32_t capten, uint8_t per)
{
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = C8_DIV64 | capten;
    T3.TC_PER = per;
    T3.TC_EVCTRL = TC_EVCTRL_EVACT_STAMP | TC_EVCTRL_TCEI_Msk;
    sync3();
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
}

static void d1_reflag(void)
{
    uint32_t a, f0, f2, f20, b;

    t3_stamp(TC_CTRLA_CAPTEN0_Msk, 250);
    wait_us(20);
    pulse(10);
    wait_us(30);
    pulse(10);
    wait_us(10);
    a = T3.TC_CC[0];
    f0 = T3.TC_INTFLAG;
    wait_us(2);
    f2 = T3.TC_INTFLAG;
    wait_us(18);
    f20 = T3.TC_INTFLAG;
    b = T3.TC_CC[0];
    console_puts("D1 reflag");
    put_dec("cc0_a", a);
    put_field("if_now", f0, 2);
    put_field("if_2us", f2, 2);
    put_field("if_20us", f20, 2);
    put_dec("cc0_b", b);
    put_field("if_after_b", T3.TC_INTFLAG, 2);
    nl();
}

/* Pin level with the pull up and down, TC2 configured with @ctrla */
static void d2_case(const char *name, uint32_t ctrla, int enable)
{
    uint32_t up, down;

    T2.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync2();
    T2.TC_CTRLA = C16_DIV64 | ctrla;
    T2.TC_EVCTRL = TC_EVCTRL_EVACT_STAMP;
    sync2();
    if (enable) {
        T2.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
        sync2();
    }
    scope_pull(GB, COP, 1);
    wait_us(20);
    up = (PORT_REGS->GROUP[GB].PORT_IN >> COP) & 1U;
    scope_pull(GB, COP, 0);
    wait_us(20);
    down = (PORT_REGS->GROUP[GB].PORT_IN >> COP) & 1U;
    console_puts(" ");
    console_puts(name);
    console_putc('=');
    console_put_dec(up);
    console_put_dec(down);
}

static void d2_pin(void)
{
    scope_pmux(GB, COP, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    console_puts("D2 pin_up_down");
    d2_case("disabled", TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_COPEN0_Msk, 0);
    d2_case("copen", TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_COPEN0_Msk, 1);
    d2_case("capten", TC_CTRLA_CAPTEN0_Msk, 1);
    d2_case("compare", 0, 1);
    nl();
}

/* COPEN0 captures with EVACT @evact: rising edges at 100 and 200 us */
static void d3_case(const char *label, uint16_t evact)
{
    uint32_t f[4], cc[3], t0;

    T2.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync2();
    T2.TC_CTRLA = C16_DIV64 | TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_COPEN0_Msk;
    T2.TC_EVCTRL = evact;
    scope_pull(GB, COP, 0);
    sync2();
    t0 = scope_now();
    T2.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync2();
    scope_wait_until(t0, SCOPE_CYC(50));
    f[0] = T2.TC_INTFLAG;
    cc[0] = f[0] & TC_INTFLAG_MC0_Msk ? T2.TC_CC[0] : 0xFFFFU;
    scope_wait_until(t0, SCOPE_CYC(100));
    scope_pull(GB, COP, 1);
    scope_wait_until(t0, SCOPE_CYC(150));
    f[1] = T2.TC_INTFLAG;
    cc[1] = f[1] & TC_INTFLAG_MC0_Msk ? T2.TC_CC[0] : 0xFFFFU;
    scope_pull(GB, COP, 0);
    scope_wait_until(t0, SCOPE_CYC(200));
    scope_pull(GB, COP, 1);
    scope_wait_until(t0, SCOPE_CYC(250));
    f[2] = T2.TC_INTFLAG;
    cc[2] = f[2] & TC_INTFLAG_MC0_Msk ? T2.TC_CC[0] : 0xFFFFU;
    scope_pull(GB, COP, 0);
    scope_wait_until(t0, SCOPE_CYC(300));
    f[3] = T2.TC_INTFLAG;
    console_puts(label);
    for (unsigned int i = 0; i < 3; i++) {
        put_field("if", f[i], 2);
        put_dec("cc0", cc[i]);
    }
    put_field("if_end", f[3], 2);
    nl();
    T2.TC_CTRLA = 0;
    sync2();
}

static void d4_overflow(void)
{
    uint32_t fl, cc0, cc1;

    t3_stamp(TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_CAPTEN1_Msk, 20);
    wait_us(40);                        /* one overflow (28 us) */
    fl = T3.TC_INTFLAG;
    cc0 = T3.TC_CC[0];
    cc1 = T3.TC_CC[1];
    console_puts("D4 overflow_both");
    put_field("if", fl, 2);
    put_field("cc0", cc0, 2);
    put_field("cc1", cc1, 2);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tcprobe3: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC2_Msk |
                                MCLK_APBCMASK_TC3_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC3_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TC3_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 100000U; i++) {
    }
    scope_start();
    stim_setup();
    d1_reflag();
    d2_pin();
    d3_case("D3 copen_stamp", TC_EVCTRL_EVACT_STAMP);
    d3_case("D3 copen_off", TC_EVCTRL_EVACT_OFF);
    d4_overflow();
    T3.TC_CTRLA = 0;
    scope_release(GB, STIM);
    scope_release(GB, COP);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
