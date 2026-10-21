/*
 * DAC probe 3 (PIC32CM MC00, DAC U2214): dithering step by step, and
 * SYNCBUSY around DATABUF. Each START prints INTFLAG and SYNCBUSY before
 * they are cleared and VOUT (ADC0 on PA02, about 4 codes per DAC code;
 * dithering moves it by one DAC code).
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 dacprobe3
 *
 * Setup as dacprobe. A = 300, B = 600 (DATABUF << 4 with DITHER).
 *
 *   G1  no DITHER: SYNCBUSY after a DATABUF write, after START, after a
 *       write while disabled, at the enable
 *   G2  DITHER, A written: 20 STARTs, B never
 *   G3  DITHER, A written, B right after the first START: 20 STARTs
 *   G4  DITHER, A written, B right after the first EMPTY: 36 STARTs
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define DA              DAC_REGS
#define EV              EVSYS_REGS
#define VOUT_AIN        0U
#define EMPTY           DAC_INTFLAG_EMPTY_Msk
#define UNDERRUN        DAC_INTFLAG_UNDERRUN_Msk
#define STARTEI         DAC_EVCTRL_STARTEI_Msk
#define OUT_VDD         (DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA)
#define VAL_A           ((300U << 4) | 8U)
#define VAL_B           ((600U << 4) | 4U)
#define SHORT_WAIT      20000U

static void nl(void)
{
    console_puts("\r\n");
}

/* Wait for SYNCBUSY bits @mask to clear, but not for long */
static void sync_short(uint32_t mask)
{
    for (uint32_t n = 0; (DA->DAC_SYNCBUSY & mask) && n < SHORT_WAIT; n++) {
    }
}

static void dac_setup(uint8_t ctrlb, uint8_t evctrl, int en)
{
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    sync_short(0xFFFFFFFFU);
    DA->DAC_CTRLB = ctrlb;
    DA->DAC_EVCTRL = evctrl;
    if (en) {
        DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
        sync_short(DAC_SYNCBUSY_ENABLE_Msk);
        for (uint32_t n = 0; !(DA->DAC_STATUS & DAC_STATUS_READY_Msk) &&
                             n < WAIT_LOOPS; n++) {
        }
    }
}

static uint16_t vout(void)
{
    delay_us(100);
    return adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2);
}

static void start(void)
{
    EV->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
}

static void case_sync(void)
{
    console_puts("G1");
    dac_setup(OUT_VDD, STARTEI, 1);
    put_field("enabled", DA->DAC_SYNCBUSY, 2);
    DA->DAC_DATABUF = 500;
    delay_us(50);
    put_field("databuf", DA->DAC_SYNCBUSY, 2);
    DA->DAC_DATA = 100;
    delay_us(50);
    put_field("data_too", DA->DAC_SYNCBUSY, 2);
    put_field("vout", vout(), 4);
    start();
    put_field("start", DA->DAC_SYNCBUSY, 2);
    put_field("vout", vout(), 4);
    dac_setup(OUT_VDD, STARTEI, 0);
    DA->DAC_DATABUF = 700;
    delay_us(50);
    put_field("disabled_write", DA->DAC_SYNCBUSY, 2);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    delay_us(50);
    put_field("enable", DA->DAC_SYNCBUSY, 2);
    put_field("flags", DA->DAC_INTFLAG, 2);
    start();
    put_field("start", DA->DAC_SYNCBUSY, 2);
    put_field("vout", vout(), 4);
    nl();
}

/* B written after START number @b_after (1-based), or after the first EMPTY */
static void dither_steps(const char *name, unsigned int n, int b_after,
                         int b_on_empty)
{
    int b_done = 0;

    dac_setup(OUT_VDD | DAC_CTRLB_DITHER_Msk, STARTEI, 1);
    DA->DAC_DATABUF = VAL_A;
    delay_us(20);
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    console_puts(name);
    nl();
    for (unsigned int k = 1; k <= n; k++) {
        uint8_t f;
        uint32_t sb;

        start();
        f = DA->DAC_INTFLAG;
        sb = DA->DAC_SYNCBUSY;
        DA->DAC_INTFLAG = EMPTY | UNDERRUN;
        console_puts(" s");
        console_put_dec(k);
        put_field("f", f, 1);
        put_field("sb", sb, 1);
        put_field("v", vout(), 4);
        if (!b_done && ((b_after && k == (unsigned int)b_after) ||
                        (b_on_empty && (f & EMPTY)))) {
            DA->DAC_DATABUF = VAL_B;
            delay_us(20);
            console_puts(" B");
            b_done = 1;
        }
        nl();
    }
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dacprobe3: " BOARD_BANNER "\r\n");
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

    case_sync();
    dither_steps("G2", 20, 0, 0);
    dither_steps("G3", 20, 1, 0);
    dither_steps("G4", 36, 0, 1);

    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 0U;
    EV->EVSYS_CHANNEL[1] = 0;
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    sync_short(0xFFFFFFFFU);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
