/*
 * ADC0 helpers on the PIC32CM MC00/JH01 (ADC U2247), shared by the ADC and
 * DAC checks (adc.c, dac.c): set-up, single conversions with timeouts, a
 * checked line on VCOM.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef ADC0_H
#define ADC0_H

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define WAIT_LOOPS      2000000U        /* well over a conversion */

static const char *stuck;               /* the wait that timed out */

static inline void adc_sync(void)
{
    for (uint32_t n = 0; ADC0_REGS->ADC_SYNCBUSY != 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "syncbusy";
            return;
        }
    }
}

static inline int adc_wait_result(void)
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

/* Clocks, 48 MHz / 32, 12 bits, enabled; @group/@pin (function B) too */
static inline void adc0_init(unsigned int group, unsigned int pin)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    PORT_REGS->GROUP[group].PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[group].PORT_PMUX[pin / 2U] =
        (uint8_t)(PORT_PMUX_PMUXE_B << ((pin & 1U) * 4U));
    ADC0_REGS->ADC_CTRLB = 4U;
    ADC0_REGS->ADC_SAMPCTRL = ADC_SAMPCTRL_SAMPLEN(15);
    ADC0_REGS->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT;
    adc_sync();
    ADC0_REGS->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
    adc_sync();
}

/*
 * One conversion of @muxpos against @refsel. REFCTRL can only be written
 * while the ADC is disabled (silicon ignores the write otherwise): switch
 * it off around a reference change. 0xFFFF when it times out.
 */
static inline uint16_t adc_read(uint8_t muxpos, uint8_t refsel)
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

/* VDDANA in mV, from INTREF (SUPC VREF, 1.024 V; sets VREF.VREFOE) */
static inline uint32_t adc_vddana_mv(void)
{
    uint16_t intref;

    SUPC_REGS->SUPC_VREF |= SUPC_VREF_VREFOE_Msk;
    intref = adc_read(ADC_INPUTCTRL_MUXPOS_INTREF, ADC_REFCTRL_REFSEL_INTVCC2);
    return intref ? 1024U * 4095U / intref : 0U;
}

/* "name code=.. expect=.. ok|FAIL": within 5 % (or 20 codes) of @expect */
static inline int check(const char *name, uint16_t code, uint16_t expect)
{
    uint16_t tol = expect / 20U > 20U ? expect / 20U : 20U;
    int ok = code + tol >= expect && code <= expect + tol;

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

#endif /* ADC0_H */
