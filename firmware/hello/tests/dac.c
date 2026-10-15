/*
 * DAC check (PIC32CM MC00/JH01, DAC U2214): the 10-bit DAC drives VOUT
 * (PA02, the DAC OUT header J700 on the MC00 Curiosity Pro), which is also
 * ADC0 AIN0, so ADC0 reads back what the DAC puts out with no wiring.
 * Each line: the DAC code, the ADC's 12-bit code of PA02 against VDDANA,
 * and what it should be.
 *
 *   ready         STATUS.READY after enable (loops counted)
 *   vddana        from INTREF, as in adc.c
 *   dac=N vddana  DAC against VDDANA: ADC ~ N * 4 (4095 / 1023)
 *   dac=1023 intref  DAC against INTREF: 1.024 V in VDDANA codes
 *
 * After PP:EXIT it ramps the DAC up and down forever (a triangle on PA02).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define VOUT_GROUP      0U                          /* PA02 */
#define VOUT_PIN        2U
#define VOUT_AIN        0U

static void dac_sync(void)
{
    for (uint32_t n = 0; DAC_REGS->DAC_SYNCBUSY != 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "dac syncbusy";
            return;
        }
    }
}

static void dac_set(uint16_t code)
{
    DAC_REGS->DAC_DATA = code;
    dac_sync();
    delay_ms(2);                                    /* settle */
}

/* DAC disabled around a CTRLB change (enable-protected, as REFCTRL is) */
static void dac_config(uint8_t ctrlb)
{
    DAC_REGS->DAC_CTRLA = 0U;
    dac_sync();
    DAC_REGS->DAC_CTRLB = ctrlb;
    DAC_REGS->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
}

int main(void)
{
    static const uint16_t codes[] = { 0U, 256U, 512U, 768U, 1023U };
    int fails = 0;
    uint32_t vdd_mv, loops;

    sys_initialize();
    systick_start();
    console_puts("PP dac: " BOARD_BANNER "\r\n");

    adc0_init(VOUT_GROUP, VOUT_PIN);                /* PA02: AIN0 / VOUT */
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_DAC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }

    vdd_mv = adc_vddana_mv();
    console_puts("vddana");
    put_field("mv", vdd_mv, 4);
    console_puts("\r\n");

    DAC_REGS->DAC_CTRLB = DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA;
    DAC_REGS->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    for (loops = 0; (DAC_REGS->DAC_STATUS & DAC_STATUS_READY_Msk) == 0U;
         loops++) {
        if (loops == WAIT_LOOPS) {
            break;
        }
    }
    console_puts("ready");
    put_field("loops", loops, 8);
    console_puts(loops < WAIT_LOOPS ? " ok\r\n" : " FAIL\r\n");
    fails += loops >= WAIT_LOOPS;

    for (unsigned int i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        dac_set(codes[i]);
        console_puts("dac");
        put_field("set", codes[i], 4);
        fails += check(" vddana",
                       adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2),
                       (uint16_t)(codes[i] * 4095U / 1023U));
    }

    dac_config(DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_INTREF);
    dac_set(1023U);
    fails += check("dac=1023 intref",
                   adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2),
                   (uint16_t)(1024U * 4095U / vdd_mv));

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    dac_config(DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    for (;;) {
        for (uint16_t c = 0; c < 1023U; c += 8U) {
            DAC_REGS->DAC_DATA = c;
            delay_ms(1);
        }
        for (uint16_t c = 1023U; c > 0U; c -= 8U) {
            DAC_REGS->DAC_DATA = c;
            delay_ms(1);
        }
    }
}
