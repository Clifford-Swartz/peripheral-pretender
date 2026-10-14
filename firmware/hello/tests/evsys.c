/*
 * EVSYS test (PIC32CM MC00 EVSYS U2256): the three paths with software
 * events and generators (TC0 overflow, EIC EXTINT, RTC PER0), edge
 * selection, the channel clock rule, CHSTATUS and the OVR/EVD flags with
 * and without ready users, the EVD interrupt, the register masks, and the
 * event users TC1 (COUNT, RETRIGGER, START, TCINV) and PORT (OUT, SET,
 * CLR, TGL).
 *
 * Expected values are PIC32CM MC00 silicon (tests/evprobe.c explores the
 * cases this test leaves out: FALLING_EDGE with pulse generators loses
 * events on silicon, and CHBUSY after PORT OUT actions is not repeatable).
 * CHSTATUS shows a CHANNEL write a few cycles late, and an INTFLAG clear
 * reaches the NVIC late, so the test waits before reading them.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define EV              EVSYS_REGS
#define T0              (TC0_REGS->COUNT8)
#define T1              (TC1_REGS->COUNT16)
#define T1_8            (TC1_REGS->COUNT8)
#define RTC0            (RTC_REGS->MODE0)

#define SYNC            EVSYS_CHANNEL_PATH_SYNCHRONOUS
#define RESYNC          EVSYS_CHANNEL_PATH_RESYNCHRONIZED
#define ASYNC           EVSYS_CHANNEL_PATH_ASYNCHRONOUS
#define RISING          EVSYS_CHANNEL_EDGSEL_RISING_EDGE
#define BOTH            EVSYS_CHANNEL_EDGSEL_BOTH_EDGES
#define NOEVT           EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT

#define OVR0            (1U << 0)
#define EVD0            (1U << 16)
#define USRRDY0         (1U << 0)
#define CHBUSY0         (1U << 16)
#define TC1_USER        EVENT_ID_USER_TC1_EVU

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

static void t1_sync(void)
{
    while (T1.TC_SYNCBUSY != 0U) {
    }
}

static void t1_reset(void)
{
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    t1_sync();
}

/* TC1 COUNT16 with event action @evact (TCEI set unless @evact is 0) */
static void t1_setup(uint32_t evact, uint32_t extra)
{
    t1_reset();
    T1.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV1024;
    T1.TC_EVCTRL = evact | TC_EVCTRL_TCEI_Msk | extra;
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

/* CHSTATUS once a configuration change has settled */
static uint32_t chstatus(void)
{
    delay_us(20);
    return EV->EVSYS_CHSTATUS;
}

static uint32_t flags(void)
{
    uint32_t f = EV->EVSYS_INTFLAG;

    EV->EVSYS_INTFLAG = f;
    return f;
}

/* Channel 0, generator @gen, into TC1 counting; flags cleared */
static void route(unsigned int gen, uint32_t path, uint32_t edge)
{
    evsys_reset();
    t1_setup(TC_EVCTRL_EVACT_COUNT, 0);
    channel(0, gen, path, edge);
    user(TC1_USER, 1);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
}

/* TC0 overflowing every ~2 ms with its event output; off after @n */
static void tc0_overflows(unsigned int n)
{
    T0.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (T0.TC_SYNCBUSY != 0U) {
    }
    T0.TC_CTRLA = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024;
    T0.TC_PER = 99;
    T0.TC_EVCTRL = TC_EVCTRL_OVFEO_Msk;
    T0.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    T0.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while (T0.TC_SYNCBUSY != 0U) {
    }
    while (n > 0U) {
        if (T0.TC_INTFLAG & TC_INTFLAG_OVF_Msk) {
            T0.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
            n--;
        }
    }
    T0.TC_CTRLA &= ~TC_CTRLA_ENABLE_Msk;
    while (T0.TC_SYNCBUSY != 0U) {
    }
    delay_us(50);
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

static void pulses(unsigned int n)
{
    for (unsigned int i = 0; i < n; i++) {
        pin(1);
        delay_ms(1);
        pin(0);
    }
}

static void eic_sense(uint32_t sense)
{
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG0 = sense << (4U * TEST_EXTINT);
    EIC_REGS->EIC_EVCTRL = 1U << TEST_EXTINT;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
}

static uint32_t led(void)
{
    return (LED0_GROUP->PORT_OUT & LED0_MASK) ? 1U : 0U;
}

static void port_action(uint32_t act)
{
    LED0_GROUP->PORT_EVCTRL = PORT_EVCTRL_PID0(24) | PORT_EVCTRL_EVACT0(act) |
                              PORT_EVCTRL_PORTEI0_Msk;
}

static void test_reset(void)
{
    evsys_reset();
    console_puts("reset:");
    put_checked("CHANNEL0", EV->EVSYS_CHANNEL[0], 0x8000, 4);
    put_checked("USER0", EV->EVSYS_USER[0], 0, 2);
    put_checked("CHSTATUS", chstatus(), 0xFFF, 8);
    put_checked("INTFLAG", EV->EVSYS_INTFLAG, 0, 8);
    eol();
}

/* Software events through each path: five reach TC1, EVD synchronously */
static void test_software(void)
{
    static const char *const names[] = { "sync", "resync", "async" };

    for (unsigned int p = 0; p < 3U; p++) {
        route(0, p << EVSYS_CHANNEL_PATH_Pos, RISING);
        console_puts("sw ");
        console_puts(names[p]);
        console_puts(":");
        put_checked("USRRDY", chstatus() & USRRDY0,
                    p == 2U ? 0 : 1, 1);
        swevt(0, 5);
        put_checked("TC1", t1_count(), 5, 4);
        put_checked("INTFLAG", flags(), p == 2U ? 0 : EVD0, 8);
        eol();
    }
    route(0, SYNC, NOEVT);
    swevt(0, 3);
    console_puts("sw noevt:");
    put_checked("TC1", t1_count(), 0, 4);
    put_checked("INTFLAG", flags(), 0, 8);
    eol();
}

/* TC0 overflow x10: one event per pulse; BOTH also sets OVR */
static void test_generator(void)
{
    route(EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    tc0_overflows(10);
    console_puts("tc0 ovf:");
    put_checked("rising", t1_count(), 10, 4);
    put_checked("f", flags(), EVD0, 8);
    route(EVENT_ID_GEN_TC0_OVF, RESYNC, BOTH);
    tc0_overflows(10);
    put_checked("both", t1_count(), 10, 4);
    put_checked("f", flags(), EVD0 | OVR0, 8);
    route(EVENT_ID_GEN_TC0_OVF, ASYNC, NOEVT);
    tc0_overflows(10);
    put_checked("async", t1_count(), 10, 4);
    put_checked("f", flags(), 0, 8);
    eol();
}

/* Without GCLK_EVSYS_CHANNEL_0, and on channel 10 (no clock at all) */
static void test_noclock(void)
{
    clock_on(EVSYS_GCLK_ID_0, 0);
    route(EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    console_puts("no clock:");
    put_checked("USRRDY", chstatus() & USRRDY0, 1, 1);
    swevt(0, 3);
    put_checked("sw", t1_count(), 0, 4);
    tc0_overflows(3);
    put_checked("tc0", t1_count(), 0, 4);
    put_checked("f", flags(), 0, 8);
    EV->EVSYS_CHANNEL[0] |= EVSYS_CHANNEL_ONDEMAND_Msk;
    swevt(0, 3);
    put_checked("ondemand", t1_count(), 0, 4);
    channel(0, EVENT_ID_GEN_TC0_OVF, ASYNC, RISING);
    tc0_overflows(3);
    put_checked("async", t1_count(), 3, 4);
    clock_on(EVSYS_GCLK_ID_0, 1);

    evsys_reset();
    t1_setup(TC_EVCTRL_EVACT_COUNT, 0);
    channel(10, 0, SYNC, RISING);
    user(TC1_USER, 11);
    swevt(10, 3);
    put_checked("ch10-sync", t1_count(), 0, 4);
    channel(10, 0, ASYNC, RISING);
    swevt(10, 3);
    put_checked("ch10-async", t1_count(), 3, 4);
    eol();
}

/* No user, then a user that is not ready, then a ready one */
static void test_users(void)
{
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    console_puts("users:");
    put_checked("idle", chstatus() & (CHBUSY0 | USRRDY0),
                CHBUSY0 | USRRDY0, 8);
    swevt(0, 1);
    put_checked("none-f", flags(), EVD0 | OVR0, 8);
    put_checked("none-st", chstatus() & (CHBUSY0 | USRRDY0),
                CHBUSY0 | USRRDY0, 8);
    channel(0, EVENT_ID_GEN_TC0_OVF, SYNC, RISING);
    put_checked("gen-st", chstatus() & CHBUSY0, CHBUSY0, 8);

    t1_reset();                     /* disabled: not ready */
    T1.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    user(TC1_USER, 1);
    put_checked("off-st", chstatus() & (CHBUSY0 | USRRDY0), 0, 8);
    swevt(0, 1);
    put_checked("off-f", flags(), EVD0 | OVR0, 8);

    t1_reset();                     /* enabled without TCEI: ready */
    T1.TC_CTRLA = TC_CTRLA_ENABLE_Msk;
    t1_sync();
    put_checked("notcei-st", chstatus() & USRRDY0, USRRDY0, 8);
    swevt(0, 1);
    put_checked("notcei-f", flags(), EVD0, 8);
    eol();
}

/* EIC EXTINT0 into TC1: edges pulse, a level sense makes two edges */
static void test_eic(void)
{
    TEST_PIN_GROUP->PORT_DIRCLR = 1U << TEST_PIN;
    TEST_PIN_GROUP->PORT_PINCFG[TEST_PIN] =
        PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    TEST_PIN_GROUP->PORT_PMUX[TEST_PIN / 2U] &=
        (TEST_PIN & 1U) ? 0x0FU : 0xF0U;
    pin(0);
    console_puts("eic:");

    eic_sense(EIC_CONFIG0_SENSE0_RISE_Val);
    route(EVENT_ID_GEN_EIC_EXTINT_0 + TEST_EXTINT, SYNC, RISING);
    pulses(3);
    put_checked("rise", t1_count(), 3, 4);
    eic_sense(EIC_CONFIG0_SENSE0_BOTH_Val);
    route(EVENT_ID_GEN_EIC_EXTINT_0 + TEST_EXTINT, SYNC, RISING);
    pulses(3);
    put_checked("both-sense", t1_count(), 6, 4);
    eic_sense(EIC_CONFIG0_SENSE0_HIGH_Val);
    route(EVENT_ID_GEN_EIC_EXTINT_0 + TEST_EXTINT, SYNC, BOTH);
    pulses(3);
    put_checked("high-both", t1_count(), 6, 4);
    put_checked("f", flags(), EVD0, 8);

    /* Async level into TC1: TCINV moves the count to the falling edge */
    evsys_reset();
    t1_setup(TC_EVCTRL_EVACT_COUNT, TC_EVCTRL_TCINV_Msk);
    channel(0, EVENT_ID_GEN_EIC_EXTINT_0 + TEST_EXTINT, ASYNC, RISING);
    user(TC1_USER, 1);
    pin(1);
    put_checked("inv-high", t1_count(), 0, 4);
    pin(0);
    put_checked("inv-low", t1_count(), 1, 4);
    EIC_REGS->EIC_CTRLA = 0;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    eol();
}

/* RTC PER0 (128 Hz) into TC1 for 100 ms */
static void test_rtc(void)
{
    uint32_t a, b;

    RTC0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    RTC0.RTC_EVCTRL = RTC_MODE0_EVCTRL_PEREO0_Msk;
    route(EVENT_ID_GEN_RTC_PER_0, SYNC, RISING);
    RTC0.RTC_CTRLA = RTC_MODE0_CTRLA_PRESCALER(1) | RTC_MODE0_CTRLA_ENABLE_Msk;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    a = t1_count();
    delay_ms(100);
    b = t1_count();
    RTC0.RTC_CTRLA = 0;
    while (RTC0.RTC_SYNCBUSY != 0U) {
    }
    console_puts("rtc per0:");
    put_checked("100ms-ok", b - a >= 11U && b - a <= 14U, 1, 1);
    eol();
}

/* TC1 event actions: COUNT to overflow and match, RETRIGGER, START */
static void test_tc_actions(void)
{
    uint32_t a, b, f;

    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(TC1_USER, 1);
    console_puts("tc actions:");

    t1_reset();
    T1_8.TC_CTRLA = TC_CTRLA_MODE_COUNT8;
    T1_8.TC_PER = 3;
    T1_8.TC_CC[0] = 2;
    T1_8.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    T1_8.TC_INTFLAG = 0xFF;
    T1_8.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    t1_sync();
    swevt(0, 2);
    f = T1_8.TC_INTFLAG;
    put_checked("mc0@2", f & (TC_INTFLAG_MC0_Msk | TC_INTFLAG_OVF_Msk),
                TC_INTFLAG_MC0_Msk, 2);
    swevt(0, 2);
    f = T1_8.TC_INTFLAG;
    put_checked("ovf@4", f & TC_INTFLAG_OVF_Msk, TC_INTFLAG_OVF_Msk, 2);
    put_checked("count@4", t1_count() & 0xFFU, 0, 2);

    /* RETRIGGER: a running counter restarts from 0 */
    t1_setup(TC_EVCTRL_EVACT_RETRIGGER, 0);
    T1.TC_CTRLBSET = TC_CTRLBSET_CMD_RETRIGGER;   /* start it */
    t1_sync();
    delay_ms(5);
    a = t1_count();
    swevt(0, 1);
    b = t1_count();
    put_checked("retrig-ran", a >= 100U, 1, 1);
    put_checked("retrig-restart", b < 20U, 1, 1);

    /* START: enabled but waiting until the event */
    t1_setup(TC_EVCTRL_EVACT_START, 0);
    delay_ms(2);
    a = t1_count();
    swevt(0, 1);
    delay_ms(2);
    b = t1_count();
    put_checked("start-wait", a, 0, 4);
    put_checked("start-run", b >= 30U, 1, 1);
    t1_reset();
    eol();
}

/* PORT EV_0 on LED0 (PA24): TGL, CLR, SET; OUT with a pulse keeps OUT */
static void test_port(void)
{
    uint32_t a, b;

    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(EVENT_ID_USER_PORT_EV_0, 1);
    console_puts("port:");
    port_action(PORT_EVCTRL_EVACT0_TGL_Val);
    put_checked("USRRDY", chstatus() & USRRDY0, USRRDY0, 1);
    swevt(0, 1);
    a = led();
    swevt(0, 1);
    b = led();
    put_checked("tgl", a << 1 | b, 1, 1);
    port_action(PORT_EVCTRL_EVACT0_CLR_Val);
    swevt(0, 1);
    put_checked("clr", led(), 0, 1);
    port_action(PORT_EVCTRL_EVACT0_SET_Val);
    swevt(0, 1);
    put_checked("set", led(), 1, 1);
    port_action(PORT_EVCTRL_EVACT0_OUT_Val);
    LED0_GROUP->PORT_OUTCLR = LED0_MASK;
    swevt(0, 1);
    a = led();
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    swevt(0, 1);
    b = led();
    put_checked("out", a << 1 | b, 1, 1);
    LED0_GROUP->PORT_EVCTRL = 0;
    LED0_GROUP->PORT_OUTSET = LED0_MASK;
    eol();
}

static void test_irq_masks(void)
{
    uint32_t pend0, pend1;

    evsys_reset();
    channel(0, 0, SYNC, RISING);
    user(TC1_USER, 1);
    t1_setup(TC_EVCTRL_EVACT_COUNT, 0);
    EV->EVSYS_INTENSET = EVD0;
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    pend0 = NVIC_GetPendingIRQ(EVSYS_IRQn);
    swevt(0, 1);
    pend1 = NVIC_GetPendingIRQ(EVSYS_IRQn);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    delay_us(10);
    NVIC_ClearPendingIRQ(EVSYS_IRQn);
    console_puts("irq:");
    put_checked("before", pend0, 0, 1);
    put_checked("after", pend1, 1, 1);
    put_checked("cleared", NVIC_GetPendingIRQ(EVSYS_IRQn), 0, 1);
    EV->EVSYS_INTENCLR = 0xFFFFFFFFU;
    t1_reset();

    evsys_reset();
    EV->EVSYS_USER[0] = 0xFFFFFFFFU;
    EV->EVSYS_CHANNEL[1] = 0xFFFFFFFFU;
    put_checked("USER0", EV->EVSYS_USER[0], 0xF, 2);
    put_checked("CHANNEL1", EV->EVSYS_CHANNEL[1], 0xCF7F, 4);
    put_checked("USRRDY1", (chstatus() >> 1) & 1U, 0, 1);
    evsys_reset();
    eol();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP evsys: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk |
                                MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk | MCLK_APBAMASK_RTC_Msk;
    clock_on(TC0_GCLK_ID, 1);
    clock_on(TC1_GCLK_ID, 1);
    clock_on(EIC_GCLK_ID, 1);
    clock_on(EVSYS_GCLK_ID_0, 1);

    test_reset();
    test_software();
    test_generator();
    test_noclock();
    test_users();
    test_eic();
    test_rtc();
    test_tc_actions();
    test_port();
    test_irq_masks();
    test_exit();
}
