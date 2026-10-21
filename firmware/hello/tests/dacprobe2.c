/*
 * DAC probe 2 (PIC32CM MC00, DAC U2214): what dacprobe left open. EMPTY
 * over time and after a DATA write, the DATABUF write's synchronization
 * time, a DATABUF write while full, DATABUF before the enable, the START
 * to EMPTY delay, and dithering's EMPTY/UNDERRUN rhythm.
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 dacprobe2
 *
 * Setup as dacprobe: DAC on GCLK0 (48 MHz) against VDDANA with EOEN,
 * VOUT (PA02) read back by ADC0 (about 4 codes per DAC code); START by
 * software events on asynchronous EVSYS channel 1. Times in CPU cycles
 * (48 MHz) from SysTick.
 *
 *   E1  EMPTY 2 ms after the enable; after a DATA write, at once and 1 ms
 *   E2  SYNCBUSY.DATABUF: cycles for DATABUF writes (empty, full, no EI)
 *       and for DATA
 *   E3  DATABUF 800, 1 ms, 900, START, START: VOUT and flags each
 *   E4  DATABUF written before the enable, then START
 *   E5  cycles from a START to EMPTY
 *   E6  DITHER, DATABUF A only: EMPTY / UNDERRUN masks over 40 STARTs
 *   E7  DITHER, A then B after the first START: masks over 40 STARTs, VOUT
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"
#include "adc0.h"

#define DA              DAC_REGS
#define EV              EVSYS_REGS
#define VOUT_AIN        0U
#define EMPTY           DAC_INTFLAG_EMPTY_Msk
#define UNDERRUN        DAC_INTFLAG_UNDERRUN_Msk
#define STARTEI         DAC_EVCTRL_STARTEI_Msk
#define OUT_VDD         (DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA)

static void nl(void)
{
    console_puts("\r\n");
}

/* Cycles until SYNCBUSY reads 0 */
static uint32_t dac_sync(void)
{
    uint32_t t0 = scope_now();

    for (uint32_t n = 0; DA->DAC_SYNCBUSY != 0U && n < WAIT_LOOPS; n++) {
    }
    return scope_since(t0);
}

static void dac_setup(uint8_t ctrlb, uint8_t evctrl, int en)
{
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    dac_sync();
    DA->DAC_CTRLB = ctrlb;
    DA->DAC_EVCTRL = evctrl;
    if (en) {
        DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
        dac_sync();
        for (uint32_t n = 0; !(DA->DAC_STATUS & DAC_STATUS_READY_Msk) &&
                             n < WAIT_LOOPS; n++) {
        }
    }
}

static uint16_t vout(void)
{
    delay_us(200);
    return adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2);
}

static void start(void)
{
    EV->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
}

static void put_flags(const char *name)
{
    put_field(name, DA->DAC_INTFLAG, 2);
}

static void case_empty(void)
{
    console_puts("E1");
    dac_setup(OUT_VDD, 0, 1);
    delay_ms(3);
    put_flags("after_2ms");
    DA->DAC_DATA = 100;
    put_flags("data_write");
    dac_sync();
    put_flags("data_synced");
    delay_ms(2);
    put_flags("after_1ms");
    dac_setup(OUT_VDD, STARTEI, 1);
    delay_ms(3);
    put_flags("ei_after_2ms");
    DA->DAC_DATA = 100;
    dac_sync();
    put_flags("ei_data_synced");
    nl();
}

static void case_sync(void)
{
    uint32_t a, b, c, d, e;

    dac_setup(OUT_VDD, STARTEI, 1);
    delay_ms(1);
    DA->DAC_DATABUF = 10;
    a = dac_sync();                     /* written while empty */
    DA->DAC_DATABUF = 20;
    b = dac_sync();                     /* while full */
    start();
    DA->DAC_DATABUF = 30;
    c = dac_sync();                     /* empty again */
    dac_setup(OUT_VDD, 0, 1);
    delay_ms(1);
    DA->DAC_DATABUF = 10;
    d = dac_sync();                     /* STARTEI 0 */
    DA->DAC_DATA = 10;
    e = dac_sync();
    console_puts("E2");
    put_field("empty", a, 8);
    put_field("full", b, 8);
    put_field("after_start", c, 8);
    put_field("no_ei", d, 8);
    put_field("data", e, 8);
    nl();
}

static void case_full(void)
{
    dac_setup(OUT_VDD, STARTEI, 1);
    DA->DAC_DATABUF = 800;
    dac_sync();
    delay_ms(1);
    DA->DAC_DATABUF = 900;
    dac_sync();
    console_puts("E3");
    put_flags("written");
    start();
    put_flags("start1");
    put_field("vout", vout(), 4);
    start();
    put_flags("start2");
    put_field("vout", vout(), 4);
    nl();
}

static void case_before_enable(void)
{
    dac_setup(OUT_VDD, STARTEI, 0);
    DA->DAC_DATABUF = 600;
    dac_sync();
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    delay_us(100);
    console_puts("E4");
    put_flags("enabled");
    start();
    put_flags("start");
    put_field("vout", vout(), 4);
    nl();
}

static void case_start_delay(void)
{
    uint32_t t0, t = 0;

    dac_setup(OUT_VDD, STARTEI, 1);
    DA->DAC_DATABUF = 500;
    dac_sync();
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    t0 = scope_now();
    EV->EVSYS_SWEVT = 1U << 1;
    for (uint32_t n = 0; !(DA->DAC_INTFLAG & EMPTY) && n < WAIT_LOOPS; n++) {
    }
    t = scope_since(t0);
    console_puts("E5");
    put_field("start_to_empty", t, 8);
    nl();
}

static void dither_run(const char *name, int write_b)
{
    uint32_t e0 = 0, e1 = 0, u0 = 0, u1 = 0;

    dac_setup(OUT_VDD | DAC_CTRLB_DITHER_Msk, STARTEI, 1);
    DA->DAC_DATABUF = (300U << 4) | 8U;
    dac_sync();
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    for (unsigned int k = 0; k < 40; k++) {
        uint8_t f;

        start();
        f = DA->DAC_INTFLAG;
        DA->DAC_INTFLAG = EMPTY | UNDERRUN;
        if (k < 32) {
            e0 |= (uint32_t)!!(f & EMPTY) << k;
            u0 |= (uint32_t)!!(f & UNDERRUN) << k;
        } else {
            e1 |= (uint32_t)!!(f & EMPTY) << (k - 32);
            u1 |= (uint32_t)!!(f & UNDERRUN) << (k - 32);
        }
        if (write_b && k == 0) {
            DA->DAC_DATABUF = (600U << 4) | 4U;
            dac_sync();
        }
    }
    console_puts(name);
    put_field("empty", e0, 8);
    put_field("empty_hi", e1, 2);
    put_field("underrun", u0, 8);
    put_field("underrun_hi", u1, 2);
    put_field("vout", vout(), 4);
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dacprobe2: " BOARD_BANNER "\r\n");
    adc0_init(0U, 2U);
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_DAC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    EV->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                           EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 2U;
    scope_start();

    case_empty();
    case_sync();
    case_full();
    case_before_enable();
    case_start_delay();
    dither_run("E6", 0);
    dither_run("E7", 1);

    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 0U;
    EV->EVSYS_CHANNEL[1] = 0;
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    dac_sync();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
