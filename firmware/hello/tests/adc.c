/*
 * ADC check: conversions of ADC0's internal inputs, which are the same on
 * the twin and on silicon, and of Arduino A0 (PB08, AIN2), whatever drives
 * it (a pot on the web bench, a wire on the board). Each line gives the
 * input, the reference and the 12-bit code; the internal ones are compared
 * with the data sheet's values within a few percent. After PP:EXIT it
 * keeps reporting A0 every 250 ms (turn a pot on the web page's bench).
 *
 *   INTREF (SUPC VREF, 1.024 V) against VDDANA  gives VDDANA (the
 *                                               board's 3.3 or 5 V)
 *   VDDANA / 4 against VDDANA                   1024
 *   VDDCORE / 4 (1.2 V) against VDDANA          300 mV in VDDANA codes
 *   VDDANA / 4 against INTREF                   VDDANA / 4 / 1.024 V
 *                                               (saturates at 5 V)
 *   INTREF against INTVCC0 (VDDANA / 1.6)       1.024 V * 1.6 / VDDANA
 *
 * INTREF reaches the ADC's input only with SUPC VREF.VREFOE set: without
 * it a conversion of INTREF never completes (MC00 silicon).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define A0_GROUP        1U                          /* PB08 */
#define A0_PIN          8U
#define A0_AIN          2U

#define WAIT_LOOPS      2000000U        /* well over a conversion */

static const char *stuck;               /* the wait that timed out */

static void adc_sync(void)
{
    for (uint32_t n = 0; ADC0_REGS->ADC_SYNCBUSY != 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "syncbusy";
            return;
        }
    }
}

static int adc_wait_result(void)
{
    for (uint32_t n = 0;
         (ADC0_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) == 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "resrdy";
            return -1;
        }
    }
    return 0;
}

/*
 * REFCTRL can only be written while the ADC is disabled (silicon ignores
 * the write otherwise): switch it off around a reference change
 */
static uint16_t adc_read(uint8_t muxpos, uint8_t refsel)
{
    if (ADC0_REGS->ADC_REFCTRL != refsel) {
        ADC0_REGS->ADC_CTRLA = 0U;
        adc_sync();
        ADC0_REGS->ADC_REFCTRL = refsel;
        ADC0_REGS->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
        adc_sync();
    }
    ADC0_REGS->ADC_INPUTCTRL = muxpos;
    adc_sync();
    ADC0_REGS->ADC_INTFLAG = ADC_INTFLAG_RESRDY_Msk;
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    if (adc_wait_result() != 0) {
        return 0xFFFFU;
    }
    (void)ADC0_REGS->ADC_RESULT;            /* first after a change: drop */
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    if (adc_wait_result() != 0) {
        return 0xFFFFU;
    }
    return ADC0_REGS->ADC_RESULT;
}

static int check(const char *name, uint16_t code, uint16_t expect)
{
    int ok = code + expect / 20U >= expect && code <= expect + expect / 20U;

    console_puts(name);
    put_field("code", code, 4);
    put_field("expect", expect, 4);
    console_puts(ok ? " ok" : " FAIL");
    if (stuck) {
        console_puts(" stuck at ");
        console_puts(stuck);
        put_field("intflag", ADC0_REGS->ADC_INTFLAG, 2);
        put_field("swtrig", ADC0_REGS->ADC_SWTRIG, 2);
        put_field("refctrl", ADC0_REGS->ADC_REFCTRL, 2);
        stuck = 0;
    }
    console_puts("\r\n");
    return ok ? 0 : 1;
}

int main(void)
{
    int fails = 0;
    uint16_t a0, intref;
    uint32_t vdd_mv, expect;

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

    /* Without VREFOE, INTREF never converts: START stays set */
    (void)adc_read(ADC_INPUTCTRL_MUXPOS_INTREF, ADC_REFCTRL_REFSEL_INTVCC2);
    console_puts(stuck ? "intref without vrefoe: stalls ok\r\n"
                       : "intref without vrefoe: converted FAIL\r\n");
    fails += stuck ? 0 : 1;
    stuck = 0;
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_FLUSH_Msk;   /* drop the stalled one */
    adc_sync();

    SUPC_REGS->SUPC_VREF |= SUPC_VREF_VREFOE_Msk;   /* INTREF to the mux */
    intref = adc_read(ADC_INPUTCTRL_MUXPOS_INTREF, ADC_REFCTRL_REFSEL_INTVCC2);
    /* VDDANA from INTREF (1.024 V): the board's supply (3.3 or 5 V) */
    vdd_mv = intref ? 1024U * 4095U / intref : 0U;
    console_puts("vddana");
    put_field("mv", vdd_mv, 4);
    console_puts("\r\n");
    fails += vdd_mv < 1800U || vdd_mv > 5500U;
    fails += check("vddana4/vddana",
                   adc_read(ADC_INPUTCTRL_MUXPOS_SCALEDVDDANA,
                            ADC_REFCTRL_REFSEL_INTVCC2), 1024);
    fails += check("vddcore4/vddana",
                   adc_read(ADC_INPUTCTRL_MUXPOS_SCALEDVDDCORE,
                            ADC_REFCTRL_REFSEL_INTVCC2),
                   (uint16_t)(300U * 4095U / vdd_mv));
    expect = vdd_mv / 4U * 4095U / 1024U;
    fails += check("vddana4/intref",
                   adc_read(ADC_INPUTCTRL_MUXPOS_SCALEDVDDANA,
                            ADC_REFCTRL_REFSEL_INTREF),
                   (uint16_t)(expect > 4095U ? 4095U : expect));
    fails += check("intref/intvcc0",
                   adc_read(ADC_INPUTCTRL_MUXPOS_INTREF,
                            ADC_REFCTRL_REFSEL_INTVCC0),
                   (uint16_t)(1024U * 16U * 4095U / 10U / vdd_mv));
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
