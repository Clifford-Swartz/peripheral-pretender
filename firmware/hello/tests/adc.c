/*
 * ADC check: conversions of ADC0's internal inputs, which are the same on
 * the twin and on silicon, and of Arduino A0 (PB08, AIN2), whatever drives
 * it (a pot on the web bench, a wire on the board). Each line gives the
 * input, the reference and the 12-bit code; the internal ones are compared
 * with the data sheet's values within a few percent. After PP:EXIT it
 * keeps reporting A0 every 250 ms (turn a pot on the web page's bench).
 *
 *   INTREF (1.0 V) against VDDANA      ~ 1241 at 3.3 V
 *   VDDANA / 4 against VDDANA          ~ 1024
 *   VDDCORE / 4 (1.2 V) against VDDANA ~ 372
 *   INTREF against INTVCC0             ~ 1861 if INTVCC0 = VDDANA / 1.48
 *                                        (2482 if it were VDDANA / 1.6)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define A0_GROUP        1U                          /* PB08 */
#define A0_PIN          8U
#define A0_AIN          2U

static void adc_sync(void)
{
    while (ADC0_REGS->ADC_SYNCBUSY != 0U) {
    }
}

static uint16_t adc_read(uint8_t muxpos, uint8_t refsel)
{
    ADC0_REGS->ADC_REFCTRL = refsel;
    ADC0_REGS->ADC_INPUTCTRL = muxpos;
    adc_sync();
    ADC0_REGS->ADC_INTFLAG = ADC_INTFLAG_RESRDY_Msk;
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    while ((ADC0_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) == 0U) {
    }
    (void)ADC0_REGS->ADC_RESULT;            /* first after a change: drop */
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    while ((ADC0_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) == 0U) {
    }
    return ADC0_REGS->ADC_RESULT;
}

static int check(const char *name, uint16_t code, uint16_t expect)
{
    int ok = code + expect / 20U >= expect && code <= expect + expect / 20U;

    console_puts(name);
    put_field("code", code, 4);
    put_field("expect", expect, 4);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    return ok ? 0 : 1;
}

int main(void)
{
    int fails = 0;
    uint16_t a0;

    sys_initialize();
    systick_start();
    console_puts("PP adc: " BOARD_BANNER "\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    /* A0: PB08 to the ADC (function B) */
    PORT_REGS->GROUP[A0_GROUP].PORT_PINCFG[A0_PIN] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[A0_GROUP].PORT_PMUX[A0_PIN / 2U] =
        (uint8_t)(PORT_PMUX_PMUXE_B << ((A0_PIN & 1U) * 4U));

    ADC0_REGS->ADC_CTRLB = 4U;                      /* 48 MHz / 32 */
    ADC0_REGS->ADC_SAMPCTRL = ADC_SAMPCTRL_SAMPLEN(15);
    ADC0_REGS->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT;
    adc_sync();
    ADC0_REGS->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
    adc_sync();

    fails += check("intref/vddana",
                   adc_read(ADC_INPUTCTRL_MUXPOS_INTREF,
                            ADC_REFCTRL_REFSEL_INTVCC2), 1241);
    fails += check("vddana4/vddana",
                   adc_read(ADC_INPUTCTRL_MUXPOS_SCALEDVDDANA,
                            ADC_REFCTRL_REFSEL_INTVCC2), 1024);
    fails += check("vddcore4/vddana",
                   adc_read(ADC_INPUTCTRL_MUXPOS_SCALEDVDDCORE,
                            ADC_REFCTRL_REFSEL_INTVCC2), 372);
    fails += check("intref/intvcc0",
                   adc_read(ADC_INPUTCTRL_MUXPOS_INTREF,
                            ADC_REFCTRL_REFSEL_INTVCC0), 1861);
    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
        a0 = adc_read(A0_AIN, ADC_REFCTRL_REFSEL_INTVCC2);
        console_puts("a0");
        put_field("code", a0, 4);
        put_field("mv", (uint32_t)a0 * 3300U / 4095U, 4);
        console_puts("\r\n");
        delay_ms(250);
    }
}
