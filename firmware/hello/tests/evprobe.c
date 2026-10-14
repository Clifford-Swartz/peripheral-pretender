/*
 * EVSYS probe: raw observations of behaviour the data sheet leaves open
 * (edge selection with pulse generators, channels without a generic
 * clock, the flags with and without users, the software event on each
 * path, the EIC and RTC generators, the PORT event user). Prints values,
 * checks nothing; used to decide what the twin models.
 *
 * The event user throughout is TC1 in COUNT16 with EVACT COUNT: every
 * event that reaches it adds one to COUNT.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define EV              EVSYS_REGS
#define T0              (TC0_REGS->COUNT8)
#define T1              (TC1_REGS->COUNT16)
#define RTC0            (RTC_REGS->MODE0)

#define SYNC            EVSYS_CHANNEL_PATH_SYNCHRONOUS
#define RESYNC          EVSYS_CHANNEL_PATH_RESYNCHRONIZED
#define ASYNC           EVSYS_CHANNEL_PATH_ASYNCHRONOUS
#define RISING          EVSYS_CHANNEL_EDGSEL_RISING_EDGE
#define FALLING         EVSYS_CHANNEL_EDGSEL_FALLING_EDGE
#define BOTH            EVSYS_CHANNEL_EDGSEL_BOTH_EDGES
#define NOEVT           EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT

static void eol(void)
{
    console_puts("\r\n");
}

static void clock_on(unsigned int id, int on)
{
    GCLK_REGS->GCLK_PCHCTRL[id] = on ? (GCLK_PCHCTRL_GEN_GCLK0 |
                                        GCLK_PCHCTRL_CHEN_Msk) : 0U;
    while (((GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk) != 0U) !=
           (on != 0)) {
    }
}

static void t0_sync(void)
{
    while (T0.TC_SYNCBUSY != 0U) {
    }
}

static void t1_sync(void)
{
    while (T1.TC_SYNCBUSY != 0U) {
    }
}

/* TC1 counting events; COUNT read through READSYNC */
static void t1_setup(void)
{
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    T1.TC_CTRLA = TC_CTRLA_MODE_COUNT16;
    T1.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    T1.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    t1_sync();
}

static uint32_t t1_count(void)
{
    T1.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
    t1_sync();
    while ((T1.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U) {
    }
    return T1.TC_COUNT;
}

static void evsys_reset(void)
{
    EV->EVSYS_CTRLA = EVSYS_CTRLA_SWRST_Msk;
    while (EV->EVSYS_CTRLA & EVSYS_CTRLA_SWRST_Msk) {
    }
}

static void channel(unsigned int ch, unsigned int gen, uint32_t path,
                    uint32_t edge)
{
    EV->EVSYS_CHANNEL[ch] = EVSYS_CHANNEL_EVGEN(gen) | path | edge;
}

static void user(unsigned int u, unsigned int ch1)
{
    EV->EVSYS_USER[u] = ch1;        /* channel + 1, 0: none */
}

static void swevt(unsigned int ch, unsigned int times)
{
    for (unsigned int i = 0; i < times; i++) {
        EV->EVSYS_SWEVT = 1U << ch;
        delay_us(20);
    }
}

static void status(void)
{
    put_field("INTFLAG", EV->EVSYS_INTFLAG, 8);
    put_field("CHSTATUS", EV->EVSYS_CHSTATUS, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
}

/* TC0 overflowing every ~2 ms; disabled after @n overflows */
static void tc0_overflows(unsigned int n)
{
    T0.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t0_sync();
    T0.TC_CTRLA = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024;
    T0.TC_PER = 99;
    T0.TC_EVCTRL = TC_EVCTRL_OVFEO_Msk;
    t0_sync();
    T0.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    T0.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    t0_sync();
    while (n > 0U) {
        if (T0.TC_INTFLAG & TC_INTFLAG_OVF_Msk) {
            T0.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
            n--;
        }
    }
    T0.TC_CTRLA &= ~TC_CTRLA_ENABLE_Msk;
    t0_sync();
}

static void route_tc0(const char *label, uint32_t path, uint32_t edge)
{
    evsys_reset();
    t1_setup();
    channel(0, EVENT_ID_GEN_TC0_OVF, path, edge);
    user(EVENT_ID_USER_TC1_EVU, 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    tc0_overflows(10);
    delay_us(50);
    console_puts(label);
    put_field("TC1", t1_count(), 4);
    status();
    eol();
}

static void pin(int high)
{
    if (high) {
        TEST_PIN_GROUP->PORT_OUTSET = 1U << TEST_PIN;
    } else {
        TEST_PIN_GROUP->PORT_OUTCLR = 1U << TEST_PIN;
    }
    delay_us(100);
}

static void eic_setup(uint32_t sense)
{
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG0 = sense << (4U * TEST_EXTINT);
    EIC_REGS->EIC_EVCTRL = 1U << TEST_EXTINT;
    EIC_REGS->EIC_ASYNCH = 0;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
}

static void eic_probe(const char *label, uint32_t sense, uint32_t edge)
{
    evsys_reset();
    t1_setup();
    pin(0);
    eic_setup(sense);
    channel(0, EVENT_ID_GEN_EIC_EXTINT_0 + TEST_EXTINT, SYNC, edge);
    user(EVENT_ID_USER_TC1_EVU, 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    for (unsigned int i = 0; i < 3U; i++) {
        pin(1);
        delay_ms(1);
        pin(0);
    }
    console_puts(label);
    put_field("TC1", t1_count(), 4);
    status();
    eol();
}

int main(void)
{
    uint32_t a, b;

    sys_initialize();
    systick_start();
    console_puts("PP evprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk |
                                MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk | MCLK_APBAMASK_RTC_Msk;
    clock_on(TC0_GCLK_ID, 1);
    clock_on(TC1_GCLK_ID, 1);
    clock_on(EIC_GCLK_ID, 1);

    /* E0: reset values */
    evsys_reset();
    console_puts("e0 reset:");
    put_field("CHANNEL0", EV->EVSYS_CHANNEL[0], 8);
    put_field("CHANNEL11", EV->EVSYS_CHANNEL[11], 8);
    status();
    eol();

    /* E1: SYNC, no channel clock, no user */
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    console_puts("e1 noclk-nouser:");
    put_field("CHANNEL0", EV->EVSYS_CHANNEL[0], 8);
    status();
    swevt(0, 1);
    status();
    eol();

    /* E2: SYNC with the channel clock, no user */
    clock_on(EVSYS_GCLK_ID_0, 1);
    evsys_reset();
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    console_puts("e2 clk-nouser:");
    status();
    swevt(0, 1);
    status();
    eol();

    /* E3: SWEVT x5 into TC1, each path, clock on */
    for (unsigned int p = 0; p < 3U; p++) {
        evsys_reset();
        t1_setup();
        channel(0, EVENT_ID_GEN_TC0_OVF, p, RISING);
        user(EVENT_ID_USER_TC1_EVU, 1);
        console_puts(p == 0 ? "e3 sw sync:" : p == 1 ? "e3 sw resync:"
                                                     : "e3 sw async:");
        put_field("USER", EV->EVSYS_USER[EVENT_ID_USER_TC1_EVU], 2);
        status();
        swevt(0, 5);
        put_field("TC1", t1_count(), 4);
        status();
        eol();
    }

    /* E4: SWEVT with EVGEN 0 (no generator) */
    evsys_reset();
    t1_setup();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    swevt(0, 5);
    console_puts("e4 sw nogen:");
    put_field("TC1", t1_count(), 4);
    status();
    eol();

    /* E5: SWEVT with EDGSEL NO_EVT_OUTPUT and FALLING */
    evsys_reset();
    t1_setup();
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, NOEVT);
    user(EVENT_ID_USER_TC1_EVU, 1);
    swevt(0, 5);
    console_puts("e5 sw noevt:");
    put_field("TC1", t1_count(), 4);
    status();
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, FALLING);
    swevt(0, 5);
    put_field("TC1-falling", t1_count(), 4);
    status();
    eol();

    /* E6: TC0 OVF x10 through each path and edge */
    route_tc0("e6 sync rising:", SYNC, RISING);
    route_tc0("e6 sync falling:", SYNC, FALLING);
    route_tc0("e6 sync both:", SYNC, BOTH);
    route_tc0("e6 sync noevt:", SYNC, NOEVT);
    route_tc0("e6 resync rising:", RESYNC, RISING);
    route_tc0("e6 resync both:", RESYNC, BOTH);
    route_tc0("e6 async rising:", ASYNC, RISING);
    route_tc0("e6 async noevt:", ASYNC, NOEVT);

    /* E7: no channel clock: SWEVT and TC0 through SYNC and ASYNC */
    clock_on(EVSYS_GCLK_ID_0, 0);
    evsys_reset();
    t1_setup();
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    console_puts("e7 noclk sync:");
    status();
    swevt(0, 5);
    put_field("TC1-sw", t1_count(), 4);
    status();
    eol();
    route_tc0("e7 noclk sync tc0:", SYNC, RISING);
    route_tc0("e7 noclk resync tc0:", RESYNC, RISING);
    route_tc0("e7 noclk async tc0:", ASYNC, RISING);
    evsys_reset();
    t1_setup();
    EV->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_TC0_OVF) |
                           SYNC | RISING | EVSYS_CHANNEL_ONDEMAND_Msk;
    user(EVENT_ID_USER_TC1_EVU, 1);
    swevt(0, 5);
    console_puts("e7 noclk ondemand:");
    put_field("TC1-sw", t1_count(), 4);
    status();
    eol();
    clock_on(EVSYS_GCLK_ID_0, 1);

    /* E8: channel 10 (no generic clock of its own) */
    evsys_reset();
    t1_setup();
    channel(10, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 11);
    swevt(10, 5);
    console_puts("e8 ch10 sync:");
    put_field("TC1-sw", t1_count(), 4);
    status();
    eol();
    route_tc0("e8 ch10 async tc0:", ASYNC, RISING);

    /* E9: EIC EXTINT events: three 1 ms high pulses */
    TEST_PIN_GROUP->PORT_DIRCLR = 1U << TEST_PIN;
    TEST_PIN_GROUP->PORT_PINCFG[TEST_PIN] =
        PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    TEST_PIN_GROUP->PORT_PMUX[TEST_PIN / 2U] &=
        (TEST_PIN & 1U) ? 0x0FU : 0xF0U;
    eic_probe("e9 eic rise:", EIC_CONFIG0_SENSE0_RISE_Val, RISING);
    eic_probe("e9 eic both-sense:", EIC_CONFIG0_SENSE0_BOTH_Val, RISING);
    eic_probe("e9 eic high r:", EIC_CONFIG0_SENSE0_HIGH_Val, RISING);
    eic_probe("e9 eic high f:", EIC_CONFIG0_SENSE0_HIGH_Val, FALLING);
    eic_probe("e9 eic high b:", EIC_CONFIG0_SENSE0_HIGH_Val, BOTH);
    eic_probe("e9 eic rise b:", EIC_CONFIG0_SENSE0_RISE_Val, BOTH);
    EIC_REGS->EIC_CTRLA = 0;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }

    /* E10: RTC PER0 (128 Hz at 1024 Hz) for 100 ms */
    RTC0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    RTC0.RTC_EVCTRL = RTC_MODE0_EVCTRL_PEREO0_Msk;
    evsys_reset();
    t1_setup();
    channel(0, EVENT_ID_GEN_RTC_PER_0, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    RTC0.RTC_CTRLA = RTC_MODE0_CTRLA_PRESCALER(1) | RTC_MODE0_CTRLA_ENABLE_Msk;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    a = t1_count();
    delay_ms(100);
    b = t1_count();
    RTC0.RTC_CTRLA = 0;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    console_puts("e10 rtc per0:");
    put_field("in100ms", b - a, 4);
    status();
    eol();

    /* E11: PORT event user: TGL / SET / CLR on LED0 (PA24) */
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_PORT_EV_0, 1);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_TGL_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    console_puts("e11 port:");
    put_field("EVCTRL", LED0_GROUP->PORT_EVCTRL, 8);
    a = LED0_GROUP->PORT_OUT & LED0_MASK;
    swevt(0, 1);
    b = LED0_GROUP->PORT_OUT & LED0_MASK;
    put_field("tgl", (a ? 2U : 0U) | (b ? 1U : 0U), 1);
    swevt(0, 1);
    put_field("tgl2", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_CLR_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    swevt(0, 1);
    put_field("clr", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_SET_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    swevt(0, 1);
    put_field("set", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_OUT_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    swevt(0, 1);
    put_field("out", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    status();
    LED0_GROUP->PORT_EVCTRL = 0;
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    eol();

    /* E12: user readback width, CHANNEL write mask */
    evsys_reset();
    EV->EVSYS_USER[0] = 0xFFFFFFFFU;
    EV->EVSYS_CHANNEL[1] = 0xFFFFFFFFU;
    console_puts("e12 masks:");
    put_field("USER0", EV->EVSYS_USER[0], 8);
    put_field("CHANNEL1", EV->EVSYS_CHANNEL[1], 8);
    status();
    eol();

    /* E13: CHBUSY/USRRDY of a fresh channel without users, per setting */
    evsys_reset();
    console_puts("e13 nouser:");
    channel(0, 0, SYNC, RISING);
    put_field("nogen", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    put_field("sync", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, NOEVT);
    put_field("noevt", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, RESYNC, RISING);
    put_field("resync", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, ASYNC, RISING);
    put_field("async", EV->EVSYS_CHSTATUS, 8);
    channel(1, 0, SYNC, RISING);
    put_field("ch1-noclk", EV->EVSYS_CHSTATUS, 8);
    eol();

    /* E14: no user: SWEVT x1, x3 */
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    console_puts("e14 nouser sw:");
    swevt(0, 1);
    put_field("x1", EV->EVSYS_INTFLAG, 8);
    put_field("st", EV->EVSYS_CHSTATUS, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    swevt(0, 3);
    put_field("x3", EV->EVSYS_INTFLAG, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    user(EVENT_ID_USER_TC1_EVU, 1);
    t1_setup();
    put_field("user-st", EV->EVSYS_CHSTATUS, 8);
    swevt(0, 1);
    put_field("user-x1", EV->EVSYS_INTFLAG, 8);
    put_field("TC1", t1_count(), 4);
    put_field("st2", EV->EVSYS_CHSTATUS, 8);
    eol();

    /* E15: user disabled (TC1 off, EVACT COUNT): busy? */
    evsys_reset();
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    console_puts("e15 user-off:");
    put_field("st", EV->EVSYS_CHSTATUS, 8);
    swevt(0, 1);
    put_field("x1", EV->EVSYS_INTFLAG, 8);
    put_field("st1", EV->EVSYS_CHSTATUS, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    swevt(0, 1);
    put_field("x2", EV->EVSYS_INTFLAG, 8);
    eol();

    /* E16: PORT user alone: CHSTATUS before and after, flags per event */
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_TGL_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    user(EVENT_ID_USER_PORT_EV_0, 1);
    console_puts("e16 port:");
    put_field("st", EV->EVSYS_CHSTATUS, 8);
    swevt(0, 1);
    put_field("x1", EV->EVSYS_INTFLAG, 8);
    put_field("st1", EV->EVSYS_CHSTATUS, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    swevt(0, 1);
    put_field("x2", EV->EVSYS_INTFLAG, 8);
    put_field("led", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_EVCTRL = 0;
    put_field("st-ei-off", EV->EVSYS_CHSTATUS, 8);
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    eol();

    /* E17: FALLING SWEVT from a fresh channel, x1 and x5 */
    evsys_reset();
    t1_setup();
    channel(0, 0, SYNC, FALLING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    console_puts("e17 falling:");
    swevt(0, 1);
    put_field("TC1", t1_count(), 4);
    put_field("f", EV->EVSYS_INTFLAG, 8);
    swevt(0, 4);
    put_field("TC1-5", t1_count(), 4);
    status();
    eol();

    /* E18: EVD interrupt: NVIC pending after one event */
    evsys_reset();
    t1_setup();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    EV->EVSYS_INTENSET = 1U << 16;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    console_puts("e18 irq:");
    put_field("INTENSET", EV->EVSYS_INTENSET, 8);
    put_field("pend0", NVIC_GetPendingIRQ(EVSYS_IRQn), 1);
    swevt(0, 1);
    put_field("pend1", NVIC_GetPendingIRQ(EVSYS_IRQn), 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    put_field("pend2", NVIC_GetPendingIRQ(EVSYS_IRQn), 1);
    eol();

    /* E19: TC user readiness: enabled without TCEI, TCEI while disabled */
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_TC1_EVU, 1);
    console_puts("e19 tc ready:");
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    T1.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    put_field("tcei-off", EV->EVSYS_CHSTATUS & 1U, 1);
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    T1.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT;
    T1.TC_CTRLA = TC_CTRLA_ENABLE_Msk;
    t1_sync();
    put_field("on-notcei", EV->EVSYS_CHSTATUS & 1U, 1);
    swevt(0, 1);
    put_field("f", EV->EVSYS_INTFLAG, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    T1.TC_EVCTRL = TC_EVCTRL_TCEI_Msk;          /* EVACT OFF */
    T1.TC_CTRLA = TC_CTRLA_ENABLE_Msk;
    t1_sync();
    put_field("on-off", EV->EVSYS_CHSTATUS & 1U, 1);
    eol();

    /* E20: PORT OUT action from OUT = 0 and from OUT = 1 */
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_PORT_EV_0, 1);
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_OUT_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    console_puts("e20 port out:");
    LED0_GROUP->PORT_OUTCLR = LED0_MASK;
    swevt(0, 1);
    put_field("from0", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    swevt(0, 1);
    put_field("from1", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    channel(0, 0, ASYNC, RISING);
    LED0_GROUP->PORT_OUTCLR = LED0_MASK;
    swevt(0, 1);
    put_field("async0", (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U, 1);
    LED0_GROUP->PORT_EVCTRL = 0;
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    /* Other group's EVCTRL: does group 1 (port B) see EV_0 too? */
    PORT_REGS->GROUP[1].PORT_EVCTRL = PORT_EVCTRL_PID0(31) |
                              PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_TGL_Val) |
                              PORT_EVCTRL_PORTEI0_Msk;
    put_field("pb-evctrl", PORT_REGS->GROUP[1].PORT_EVCTRL, 8);
    PORT_REGS->GROUP[1].PORT_EVCTRL = 0;
    eol();

    /* E21: CHSTATUS right after a write and 1 ms later */
    console_puts("e21 settle:");
    evsys_reset();
    a = EV->EVSYS_CHSTATUS;
    delay_ms(1);
    put_field("rst", a, 8);
    put_field("rst-1ms", EV->EVSYS_CHSTATUS, 8);
    channel(0, 0, SYNC, RISING);
    a = EV->EVSYS_CHSTATUS;
    delay_ms(1);
    put_field("nogen", a, 8);
    put_field("nogen-1ms", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    a = EV->EVSYS_CHSTATUS;
    delay_ms(1);
    put_field("gen", a, 8);
    put_field("gen-1ms", EV->EVSYS_CHSTATUS, 8);
    eol();
    console_puts("e21 users:");
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    user(EVENT_ID_USER_TC1_EVU, 1);
    a = EV->EVSYS_CHSTATUS;
    delay_ms(1);
    put_field("off", a, 8);
    put_field("off-1ms", EV->EVSYS_CHSTATUS, 8);
    channel(0, 0, SYNC, RISING);
    delay_ms(1);
    put_field("off-nogen", EV->EVSYS_CHSTATUS, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, RESYNC, RISING);
    delay_ms(1);
    put_field("off-resync", EV->EVSYS_CHSTATUS, 8);
    T1.TC_CTRLA = TC_CTRLA_ENABLE_Msk;
    t1_sync();
    delay_ms(1);
    put_field("on", EV->EVSYS_CHSTATUS, 8);
    eol();
    console_puts("e21 irq:");
    EV->EVSYS_INTENSET = 1U << 16;
    swevt(0, 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    put_field("now", NVIC_GetPendingIRQ(EVSYS_IRQn), 1);
    swevt(0, 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    delay_us(10);
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    put_field("10us", NVIC_GetPendingIRQ(EVSYS_IRQn), 1);
    EV->EVSYS_INTENCLR = 0xFFFFFFFFU;
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
    eol();

    evsys_reset();
    test_exit();
}
