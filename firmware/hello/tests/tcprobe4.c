/*
 * TC probe 4 (PIC32CM MC00, TC U2249): the flags around a capture FIFO
 * that a CCx read empties (tcprobe3 D1 saw OVF, ERR and MC0 right after).
 * Prints raw results; checks nothing. Run: py -m tools.periph probe mc00
 * tcprobe4
 *
 * TC3 COUNT8 DIV64, PER 250 (335 us a period), STAMP on channel 0 only,
 * events from PB09 (EXTINT9) as in tcprobe2. Every line: INTFLAG at once,
 * after 2 us and after 20 us, and the time since the enable.
 *
 *   F1  no capture, read CC0
 *   F2  one capture, read CC0 (the FIFO empties)
 *   F3  two captures, read CC0 twice
 *   F4  as F2, flags cleared first, then read CC0
 *   F5  as F2 with STAMP off (EVACT OFF, plain capture)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T3              (TC3_REGS->COUNT8)
#define GB              1U
#define STIM            9U
#define FUNC_A          0U
#define GEN_EIC9        23U
#define USER_TC3        26U
#define C8_DIV64        (TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV64)

static uint32_t t_en;

static void nl(void)
{
    console_puts("\r\n");
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

static void pulse(void)
{
    scope_pull(GB, STIM, 1);
    wait_us(10);
    scope_pull(GB, STIM, 0);
    wait_us(20);
}

static void start(uint16_t evact)
{
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = C8_DIV64 | TC_CTRLA_CAPTEN0_Msk;
    T3.TC_PER = 250;
    T3.TC_CC[1] = 200;                  /* no compare flag early on */
    T3.TC_EVCTRL = evact | TC_EVCTRL_TCEI_Msk;
    sync3();
    t_en = scope_now();
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
    wait_us(10);
}

/* Read CC0, then INTFLAG at once, 2 us and 20 us later */
static void read_cc(const char *name)
{
    uint32_t cc, f0, f2, f20, t;

    cc = T3.TC_CC[0];
    f0 = T3.TC_INTFLAG;
    wait_us(2);
    f2 = T3.TC_INTFLAG;
    wait_us(18);
    f20 = T3.TC_INTFLAG;
    t = SCOPE_US(scope_since(t_en));
    console_puts(" ");
    console_puts(name);
    console_puts(":cc=");
    console_put_dec(cc);
    put_field("if", f0, 2);
    put_field("if2", f2, 2);
    put_field("if20", f20, 2);
    console_puts(" t=");
    console_put_dec(t);
}

int main(void)
{
    sys_initialize();
    console_puts("PP tcprobe4: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC3_Msk |
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
    scope_pmux(GB, STIM, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GB, STIM, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG1 = 0x4U << (4U * (STIM - 8U));
    EIC_REGS->EIC_ASYNCH = 1U << STIM;
    EIC_REGS->EIC_EVCTRL = 1U << STIM;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC9) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_TC3] = 1U;

    console_puts("F1 none");
    start(TC_EVCTRL_EVACT_STAMP);
    read_cc("read");
    nl();
    console_puts("F2 one");
    start(TC_EVCTRL_EVACT_STAMP);
    pulse();
    read_cc("read");
    read_cc("again");
    nl();
    console_puts("F3 two");
    start(TC_EVCTRL_EVACT_STAMP);
    pulse();
    pulse();
    read_cc("first");
    read_cc("second");
    read_cc("third");
    nl();
    console_puts("F4 cleared");
    start(TC_EVCTRL_EVACT_STAMP);
    pulse();
    T3.TC_INTFLAG = 0xFFU;
    read_cc("read");
    nl();
    console_puts("F5 plain");
    start(TC_EVCTRL_EVACT_OFF);
    pulse();
    read_cc("read");
    read_cc("again");
    nl();
    T3.TC_CTRLA = 0;
    scope_release(GB, STIM);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
