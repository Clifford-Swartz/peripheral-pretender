/*
 * TC test: counting, READSYNC, commands, one-shot, direction, double
 * buffering, compare match, MFRQ, the counting rate, COUNT32 pairing, the
 * overflow interrupt and synchronization without the clock.
 *
 * TC0 (and TC1 for COUNT32) run from GCLK0 (48 MHz) / 1024 = 46875 Hz.
 * Timing-dependent results are printed as checks with a tolerance, so the
 * transcripts of silicon and twin compare byte for byte.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define TC              TC0_REGS
#define C8              (TC->COUNT8)
#define C16             (TC->COUNT16)
#define TC_HZ           (CPU_CLOCK_HZ / 1024U)

static volatile unsigned int ovf_irqs;

void TC0_Handler(void)
{
    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    ovf_irqs++;
}

static void sync(void)
{
    while (C8.TC_SYNCBUSY != 0U) {
    }
}

/* Software reset, then CTRLA (without ENABLE) */
static void tc_reset(uint32_t ctrla)
{
    C8.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync();
    C8.TC_CTRLA = ctrla;
    sync();
}

static void tc_enable(void)
{
    C8.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync();
}

static void command(uint8_t cmd)
{
    C8.TC_CTRLBSET = cmd;
    sync();
    while ((C8.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U) {
    }
}

static uint32_t count8(void)
{
    command(TC_CTRLBSET_CMD_READSYNC);
    return C8.TC_COUNT;
}

static uint32_t count16(void)
{
    command(TC_CTRLBSET_CMD_READSYNC);
    return C16.TC_COUNT;
}

/* Wait up to @ms for @mask in INTFLAG; 1 if it came */
static uint32_t wait_flag(uint8_t mask, uint32_t ms)
{
    while (ms > 0U) {
        if (C8.TC_INTFLAG & mask) {
            return 1;
        }
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    return (C8.TC_INTFLAG & mask) ? 1U : 0U;
}

static void eol(void)
{
    console_puts("\r\n");
}

static void test_reset(void)
{
    console_puts("reset:");
    put_checked("CTRLA", C8.TC_CTRLA, 0, 8);
    put_checked("STATUS", C8.TC_STATUS, 0x01, 2);
    put_checked("SYNCBUSY", C8.TC_SYNCBUSY, 0, 2);
    eol();
}

/* COUNT8, PER = 99: overflow; COUNT reads need READSYNC */
static void test_count8(void)
{
    uint32_t a, b, c, d;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    sync();
    tc_enable();
    console_puts("count8:");
    put_checked("ovf", wait_flag(TC_INTFLAG_OVF_Msk, 20), 1, 1);
    put_checked("stop", C8.TC_STATUS & TC_STATUS_STOP_Msk, 0, 1);
    eol();

    a = C8.TC_COUNT;
    delay_ms(1);
    b = C8.TC_COUNT;
    c = count8();
    delay_ms(1);
    d = count8();
    console_puts("readsync:");
    put_checked("unsynced-stale", a == b, 1, 1);
    put_checked("synced-moves", c != d, 1, 1);
    eol();
}

static void test_commands(void)
{
    uint32_t stop, cnt, run;

    command(TC_CTRLBSET_CMD_STOP);
    stop = C8.TC_STATUS & TC_STATUS_STOP_Msk;
    cnt = count8();
    command(TC_CTRLBSET_CMD_RETRIGGER);
    run = C8.TC_STATUS & TC_STATUS_STOP_Msk;
    console_puts("stop/retrigger:");
    put_checked("stopped", stop, 1, 1);
    put_checked("count", cnt, 0, 2);
    put_checked("running", run, 0, 1);
    eol();
}

static void test_oneshot(void)
{
    uint32_t ovf, stop, cnt, again;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_CTRLBSET = TC_CTRLBSET_ONESHOT_Msk;
    sync();
    tc_enable();
    ovf = wait_flag(TC_INTFLAG_OVF_Msk, 20);
    delay_ms(1);
    stop = C8.TC_STATUS & TC_STATUS_STOP_Msk;
    cnt = count8();
    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    again = wait_flag(TC_INTFLAG_OVF_Msk, 10);
    console_puts("oneshot:");
    put_checked("ovf", ovf, 1, 1);
    put_checked("stopped", stop, 1, 1);
    put_checked("count", cnt, 0, 2);
    put_checked("ovf-again", again, 0, 1);
    eol();
}

/* Counting down starts from COUNT (0): it underflows to TOP at once */
static void test_down(void)
{
    uint32_t start, ovf, top;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_CTRLBSET = TC_CTRLBSET_DIR_Msk;
    sync();
    tc_enable();
    start = count8();
    delay_us(100);
    ovf = (C8.TC_INTFLAG & TC_INTFLAG_OVF_Msk) ? 1U : 0U;
    top = count8();
    console_puts("down:");
    put_checked("start", start, 0, 2);
    put_checked("underflow@100us", ovf, 1, 1);
    put_checked("near-top", top >= 80U && top <= 99U, 1, 1);
    eol();
}

/* Overflows in @ms */
static uint32_t ovf_count(uint32_t ms)
{
    uint32_t n = 0;

    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    while (ms > 0U) {
        if (C8.TC_INTFLAG & TC_INTFLAG_OVF_Msk) {
            C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
            n++;
        }
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    return n;
}

/*
 * PERBUF written mid-period: PER reads show the buffer while PERBUFV is
 * set, TOP changes at the overflow (or UPDATE when LUPD locks it); CCx
 * reads the same way. Behaviour seen on silicon (tests/tcprobe.c).
 */
static void test_buffers(void)
{
    uint32_t v1, p1, v2, p2, n, v3, p3, v4, p4, cv, c0;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    sync();
    tc_enable();
    delay_ms(1);
    C8.TC_PERBUF = 49;
    delay_us(10);                   /* PERBUFV shows after synchronization */
    v1 = (C8.TC_STATUS & TC_STATUS_PERBUFV_Msk) ? 1U : 0U;
    p1 = C8.TC_PER;
    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    wait_flag(TC_INTFLAG_OVF_Msk, 20);
    delay_us(100);
    v2 = (C8.TC_STATUS & TC_STATUS_PERBUFV_Msk) ? 1U : 0U;
    p2 = C8.TC_PER;
    console_puts("perbuf:");
    put_checked("valid", v1, 1, 1);
    put_checked("PER-read", p1, 49, 2);
    put_checked("valid@ovf", v2, 0, 1);
    put_checked("PER@ovf", p2, 49, 2);
    eol();

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_CTRLBSET = TC_CTRLBSET_LUPD_Msk;
    sync();
    tc_enable();
    delay_ms(1);
    C8.TC_PERBUF = 19;
    n = ovf_count(21);              /* TOP stays 99: about 10 */
    v3 = (C8.TC_STATUS & TC_STATUS_PERBUFV_Msk) ? 1U : 0U;
    p3 = C8.TC_PER;
    command(TC_CTRLBSET_CMD_UPDATE);
    v4 = (C8.TC_STATUS & TC_STATUS_PERBUFV_Msk) ? 1U : 0U;
    p4 = C8.TC_PER;
    console_puts("perbuf locked:");
    put_checked("top-kept", n >= 7U && n <= 12U, 1, 1);
    put_checked("valid", v3, 1, 1);
    put_checked("PER-read", p3, 19, 2);
    put_checked("valid@update", v4, 0, 1);
    put_checked("PER@update", p4, 19, 2);
    eol();

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_CC[0] = 10;
    sync();
    tc_enable();
    delay_ms(1);
    C8.TC_CCBUF[0] = 20;
    delay_us(10);
    cv = (C8.TC_STATUS & TC_STATUS_CCBUFV0_Msk) ? 1U : 0U;
    c0 = C8.TC_CC[0];
    console_puts("ccbuf:");
    put_checked("valid", cv, 1, 1);
    put_checked("CC0-read", c0, 20, 2);
    eol();
}

static void test_match(void)
{
    uint32_t mc0, mc1;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_CC[0] = 10;
    C8.TC_CC[1] = 150;                          /* above TOP: never */
    sync();
    tc_enable();
    mc0 = wait_flag(TC_INTFLAG_MC0_Msk, 20);
    wait_flag(TC_INTFLAG_OVF_Msk, 20);
    C8.TC_INTFLAG = 0xFF;
    wait_flag(TC_INTFLAG_OVF_Msk, 20);
    mc1 = (C8.TC_INTFLAG & TC_INTFLAG_MC1_Msk) ? 1U : 0U;
    console_puts("match:");
    put_checked("mc0", mc0, 1, 1);
    put_checked("mc1-above-top", mc1, 0, 1);
    eol();
}

/* NFRQ (TOP = PER = 99) against MFRQ (TOP = CC0 = 19): 5 times faster */
static void test_mfrq(void)
{
    uint32_t nfrq, mfrq;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    sync();
    tc_enable();
    nfrq = ovf_count(50);

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_WAVE = TC_WAVE_WAVEGEN_MFRQ;
    C8.TC_PER = 99;
    C8.TC_CC[0] = 19;
    sync();
    tc_enable();
    mfrq = ovf_count(50);
    console_puts("mfrq:");
    /* 46875 Hz: 23.4 NFRQ and 117 MFRQ overflows in 50 ms */
    put_checked("nfrq-ok", nfrq >= 18U && nfrq <= 28U, 1, 1);
    put_checked("ratio-5", mfrq >= nfrq * 4U && mfrq <= nfrq * 6U, 1, 1);
    eol();
}

/* COUNT16 for 20 ms: 937.5 ticks at 46875 Hz */
static void test_rate(void)
{
    uint32_t a, b, delta;

    tc_reset(TC_CTRLA_PRESCALER_DIV1024);
    tc_enable();
    a = count16();
    delay_ms(20);
    b = count16();
    delta = (b - a) & 0xFFFFU;
    console_puts("rate:");
    put_checked("ticks-20ms-ok", delta >= 800U && delta <= 1000U, 1, 1);
    eol();
}

static void test_pair(void)
{
    uint32_t slave, off;

    tc_reset(TC_CTRLA_MODE_COUNT32 | TC_CTRLA_PRESCALER_DIV1024);
    tc_enable();
    slave = (TC1_REGS->COUNT8.TC_STATUS & TC_STATUS_SLAVE_Msk) ? 1U : 0U;
    tc_reset(0);
    off = (TC1_REGS->COUNT8.TC_STATUS & TC_STATUS_SLAVE_Msk) ? 1U : 0U;
    console_puts("count32:");
    put_checked("tc1-slave", slave, 1, 1);
    put_checked("after-reset", off, 0, 1);
    eol();
}

static void test_irq(void)
{
    uint32_t ms = 50;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    C8.TC_PER = 99;
    C8.TC_INTENSET = TC_INTENSET_OVF_Msk;
    sync();
    ovf_irqs = 0;
    NVIC_ClearPendingIRQ(TC0_IRQn);
    NVIC_EnableIRQ(TC0_IRQn);
    tc_enable();
    while (ovf_irqs < 5U && ms > 0U) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    NVIC_DisableIRQ(TC0_IRQn);
    C8.TC_INTENCLR = TC_INTENCLR_OVF_Msk;
    console_puts("irq:");
    put_checked("five-ovf", ovf_irqs >= 5U, 1, 1);
    eol();
}

/* Synchronization without GCLK_TC */
static void test_noclock(void)
{
    uint32_t busy, done;

    tc_reset(TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024);
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] = 0U;
    while (GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk) {
    }
    C8.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    delay_us(100U);
    busy = (C8.TC_SYNCBUSY & TC_SYNCBUSY_ENABLE_Msk) ? 1U : 0U;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    sync();
    done = C8.TC_SYNCBUSY;
    console_puts("no gclk:");
    put_checked("enable-busy", busy, 1, 1);
    put_checked("after-gclk", done, 0, 2);
    eol();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP tc: " BOARD_BANNER "\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }

    test_reset();
    test_count8();
    test_commands();
    test_oneshot();
    test_down();
    test_buffers();
    test_match();
    test_mfrq();
    test_rate();
    test_pair();
    test_irq();
    test_noclock();
    test_exit();
}
