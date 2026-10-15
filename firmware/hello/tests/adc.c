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

#include "adc0.h"

#define A0_GROUP        1U                          /* PB08 */
#define A0_PIN          8U
#define A0_AIN          2U

int main(void)
{
    int fails = 0;
    uint16_t a0, intref;
    uint32_t vdd_mv, expect;

    sys_initialize();
    systick_start();
    console_puts("PP adc: " BOARD_BANNER "\r\n");

    adc0_init(A0_GROUP, A0_PIN);

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
