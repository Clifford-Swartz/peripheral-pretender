/*
 * EIC test: edge and level detection, flag clearing, the EIC interrupt and
 * the clock requirement of synchronous edges.
 *
 * Stimulus without external wiring: the test pin (board.h TEST_PIN_*, a
 * free pin whose function A is EXTINT TEST_EXTINT) is muxed to the EIC with
 * its pull resistor on; writing OUT moves the pull between up and down, so
 * the pad, and the EIC input, follow. Every line printed is deterministic,
 * so the transcripts of silicon and twin can be compared byte for byte.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define LINE            TEST_EXTINT
#define MASK            (1U << LINE)
#define SETTLE_US       200U    /* pull resistor against the pad */

#define SENSE_NONE      0U
#define SENSE_RISE      1U
#define SENSE_FALL      2U
#define SENSE_BOTH      3U
#define SENSE_HIGH      4U
#define SENSE_LOW       5U

static volatile unsigned int irq_count;

void EIC_Handler(void)
{
    uint32_t flags = EIC_REGS->EIC_INTFLAG;

    EIC_REGS->EIC_INTFLAG = flags;
    if (flags & MASK) {
        irq_count++;
    }
}

static void pad(bool high)
{
    if (high) {
        TEST_PIN_GROUP->PORT_OUTSET = 1U << TEST_PIN;
    } else {
        TEST_PIN_GROUP->PORT_OUTCLR = 1U << TEST_PIN;
    }
    delay_us(SETTLE_US);
}

static void eic_sync(void)
{
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
}

/* Disable, set the sense of the test line (and ASYNCH), enable. */
static void eic_config(uint32_t sense, bool asynch)
{
    EIC_REGS->EIC_CTRLA = 0U;
    eic_sync();
    if (LINE < 8U) {
        EIC_REGS->EIC_CONFIG0 = sense << (LINE * 4U);
    } else {
        EIC_REGS->EIC_CONFIG1 = sense << ((LINE % 8U) * 4U);
    }
    EIC_REGS->EIC_ASYNCH = asynch ? MASK : 0U;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    eic_sync();
    EIC_REGS->EIC_INTFLAG = 0xFFFFFFFFU;
}

static uint32_t flag(void)
{
    return (EIC_REGS->EIC_INTFLAG & MASK) ? 1U : 0U;
}

static void clear(void)
{
    EIC_REGS->EIC_INTFLAG = MASK;
}

static void gclk_eic(bool on)
{
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] =
        on ? (GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk) : 0U;
    while (((GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] &
             GCLK_PCHCTRL_CHEN_Msk) != 0U) != on) {
    }
}

/* Edge senses: flag after a falling and after a rising edge */
static void test_edges(void)
{
    static const struct {
        const char *name;
        uint32_t sense;
        uint32_t after_fall, after_rise;
    } cases[] = {
        { "rise", SENSE_RISE, 0, 1 },
        { "fall", SENSE_FALL, 1, 0 },
        { "both", SENSE_BOTH, 1, 1 },
        { "none", SENSE_NONE, 0, 0 },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint32_t f, r;

        pad(true);
        eic_config(cases[i].sense, false);
        pad(false);
        f = flag();
        clear();
        pad(true);
        r = flag();
        clear();
        console_puts("edge ");
        console_puts(cases[i].name);
        console_puts(":");
        put_checked("fall", f, cases[i].after_fall, 1);
        put_checked("rise", r, cases[i].after_rise, 1);
        console_puts("\r\n");
    }
}

/* Level senses: the flag comes back while the level persists. */
static void test_levels(void)
{
    uint32_t a, b, c;

    pad(true);
    eic_config(SENSE_HIGH, false);
    a = flag();
    clear();
    b = flag();
    pad(false);
    clear();
    c = flag();
    console_puts("level high:");
    put_checked("set", a, 1, 1);
    put_checked("after-clear", b, 1, 1);
    put_checked("low", c, 0, 1);
    console_puts("\r\n");

    eic_config(SENSE_LOW, false);
    a = flag();
    pad(true);
    clear();
    b = flag();
    console_puts("level low:");
    put_checked("set", a, 1, 1);
    put_checked("high", b, 0, 1);
    console_puts("\r\n");
}

/* Both edges with the interrupt: three low/high pulses, six interrupts */
static void test_interrupt(void)
{
    pad(true);
    eic_config(SENSE_BOTH, false);
    irq_count = 0;
    EIC_REGS->EIC_INTENSET = MASK;
    NVIC_ClearPendingIRQ(EIC_IRQn);
    NVIC_EnableIRQ(EIC_IRQn);
    for (unsigned i = 0; i < 3U; i++) {
        pad(false);
        pad(true);
    }
    NVIC_DisableIRQ(EIC_IRQn);
    EIC_REGS->EIC_INTENCLR = MASK;
    console_puts("irq both:");
    put_checked("count", irq_count, 6, 2);
    console_puts("\r\n");
}

/* CTRLA.ENABLE changed without GCLK_EIC: 1 if SYNCBUSY.ENABLE stays set */
static uint32_t sync_stalls(uint32_t ctrla)
{
    uint32_t busy;

    EIC_REGS->EIC_CTRLA = ctrla;
    delay_us(100U);
    busy = EIC_REGS->EIC_SYNCBUSY & EIC_SYNCBUSY_ENABLE_Msk ? 1U : 0U;
    gclk_eic(true);
    eic_sync();
    return busy;
}

/*
 * Synchronous edges need GCLK_EIC; asynchronous ones do not (DS 25.6.4.2).
 * Changing CTRLA.ENABLE without GCLK_EIC stalls SYNCBUSY until the clock
 * runs, but only while the configuration needs the clock (synchronous
 * edge detection), as seen on silicon.
 */
static void test_clock(void)
{
    uint32_t sync, async, s_off, s_on, a_off, a_on;

    pad(false);
    eic_config(SENSE_RISE, false);
    gclk_eic(false);
    pad(true);
    sync = flag();
    s_off = sync_stalls(0U);                    /* sync edge configured */
    gclk_eic(false);
    s_on = sync_stalls(EIC_CTRLA_ENABLE_Msk);

    pad(false);
    eic_config(SENSE_RISE, true);
    gclk_eic(false);
    pad(true);
    async = flag();
    a_off = sync_stalls(0U);                    /* asynchronous edge */
    gclk_eic(false);
    a_on = sync_stalls(EIC_CTRLA_ENABLE_Msk);

    console_puts("no gclk:");
    put_checked("sync", sync, 0, 1);
    put_checked("async", async, 1, 1);
    console_puts("\r\nno gclk syncbusy:");
    put_checked("sync-disable", s_off, 1, 1);
    put_checked("sync-enable", s_on, 1, 1);
    put_checked("async-disable", a_off, 0, 1);
    put_checked("async-enable", a_on, 0, 1);
    console_puts("\r\n");
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP eic: " BOARD_BANNER "\r\n");

    /* GCLK_EIC from GCLK0; the EIC's APB clock is on at reset */
    gclk_eic(true);
    /* Test pin: input, pull resistor on, muxed to function A (EIC) */
    TEST_PIN_GROUP->PORT_DIRCLR = 1U << TEST_PIN;
    TEST_PIN_GROUP->PORT_PINCFG[TEST_PIN] =
        PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    TEST_PIN_GROUP->PORT_PMUX[TEST_PIN / 2U] &=
        (TEST_PIN & 1U) ? 0x0FU : 0xF0U;

    test_edges();
    test_levels();
    test_interrupt();
    test_clock();
    test_exit();
}
