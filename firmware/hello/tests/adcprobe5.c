/*
 * ADC probe 5 (PIC32CM MC00, ADC U2247): what a free-running sequence does
 * at the enable and at a START, where adcseq's DMA saw an extra first
 * conversion of 0x1A (adcprobe4 had none). Prints raw results; checks
 * nothing. Run: py -m tools.periph probe mc00 adcprobe5
 *
 * ADC0 on GCLK0 (48 MHz), DIV32, 12 bits, REFCTRL INTVCC2, INPUTCTRL 0x1A
 * (SCALEDVDDCORE), SEQCTRL 0x1A and 0x1B, FREERUN. Each line: the first 5
 * results as SEQSTATUS/RESULT with their times (CPU cycles from the
 * enable), read as they come.
 *
 *   Q1  enabled, no START
 *   Q2  START right after the enable
 *   Q3  START 50 us after the enable
 *   Q4  as Q2, the ADC enabled before (SWRST between)
 *   Q5  as Q2 without SEQCTRL: plain free running
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define A0              ADC0_REGS
#define CORE            0x1AU
#define ANA             0x1BU
#define TMO             SCOPE_CYC(20000)

static void sync_adc(void)
{
    for (uint32_t i = 0; A0->ADC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void run(const char *label, int start_us, uint32_t seq)
{
    uint32_t t[5], st[5], r[5], t0;
    unsigned int n = 0;

    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc();
    A0->ADC_CTRLB = ADC_CTRLB_PRESCALER_DIV32;
    A0->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
    A0->ADC_INPUTCTRL = CORE;
    A0->ADC_SEQCTRL = seq;
    A0->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk;
    sync_adc();
    t0 = scope_now();
    A0->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
    if (start_us >= 0) {
        scope_wait_until(t0, SCOPE_CYC((uint32_t)start_us));
        A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    }
    while (n < 5 && scope_since(t0) < TMO) {
        if (A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) {
            t[n] = scope_since(t0);
            st[n] = A0->ADC_SEQSTATUS;
            r[n] = A0->ADC_RESULT;
            n++;
        }
    }
    console_puts(label);
    for (unsigned int k = 0; k < n; k++) {
        console_puts(" |");
        console_put_dec(t[k]);
        console_putc(' ');
        console_put_hex(st[k], 2);
        console_putc('/');
        console_put_hex(r[k], 4);
    }
    console_puts("\r\n");
}

int main(void)
{
    sys_initialize();
    console_puts("PP adcprobe5: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_start();
    run("Q1 no_start", -1, (1U << CORE) | (1U << ANA));
    run("Q2 start_at_once", 0, (1U << CORE) | (1U << ANA));
    run("Q3 start_50us", 50, (1U << CORE) | (1U << ANA));
    run("Q4 start_at_once_again", 0, (1U << CORE) | (1U << ANA));
    run("Q5 plain_start_at_once", 0, 0);
    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
