/*
 * ADC probe (PIC32CM MC00, ADC U2247): conversion time, free-running
 * period, automatic sequences and double buffering (DS 36.6.2.8,
 * 36.6.2.12, 36.6.3.3), where the twin converts at once. Prints raw
 * results; checks nothing. Run: py -m tools.periph probe mc00 adcprobe
 *
 * ADC0 on GCLK0 (48 MHz), REFCTRL INTVCC2 (VDDANA); inputs SCALEDVDDCORE
 * (0x1A) and SCALEDVDDANA (0x1B), no pins. Times in CPU cycles (48 MHz,
 * SysTick) from the SWTRIG.START write.
 *
 *   T1-T8  START to RESRDY (and to START reading 0) for prescaler,
 *          resolution, SAMPLEN, OFFCOMP and averaging settings
 *   F1     free running: the times of 6 RESRDYs, RESULT read at each
 *   S1     SEQCTRL 0x1A and 0x1B: per conversion time, SEQSTATUS, RESULT
 *   S2     SEQCTRL AIN0, AIN1, 0x1A, 0x1B, no read: INTFLAG, SEQSTATUS
 *   S3     S1 with FREERUN: the first 6 SEQSTATUS values
 *   B1     free running on 0x1A, INPUTCTRL to 0x1B: SYNCBUSY time and
 *          the first RESULTs after
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define A0              ADC0_REGS
#define CORE            0x1AU
#define ANA             0x1BU
#define TMO             SCOPE_CYC(20000)

static void nl(void)
{
    console_puts("\r\n");
}

static void sync_adc(void)
{
    for (uint32_t i = 0; A0->ADC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void adc_setup(uint32_t prescaler, uint32_t ctrlc, uint32_t sampctrl,
                      uint32_t avgctrl, uint32_t mux, uint32_t seq)
{
    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc();
    A0->ADC_CTRLB = prescaler;
    A0->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
    A0->ADC_SAMPCTRL = sampctrl;
    A0->ADC_AVGCTRL = avgctrl;
    A0->ADC_INPUTCTRL = mux;
    A0->ADC_SEQCTRL = seq;
    A0->ADC_CTRLC = ctrlc;
    sync_adc();
    A0->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
    sync_adc();
    A0->ADC_INTFLAG = 0xFF;
}

/* Cycles from START to RESRDY, and to SWTRIG.START reading 0 */
static void timed(const char *label, uint32_t prescaler, uint32_t ctrlc,
                  uint32_t sampctrl, uint32_t avgctrl)
{
    uint32_t t0, tr = 0, ts = 0, sum = 0, sum_s = 0;

    adc_setup(prescaler, ctrlc, sampctrl, avgctrl, CORE, 0);
    for (unsigned int k = 0; k < 4; k++) {
        t0 = scope_now();
        A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
        ts = 0;
        for (;;) {
            uint32_t d = scope_since(t0);

            if (!ts && !(A0->ADC_SWTRIG & ADC_SWTRIG_START_Msk)) {
                ts = d;
            }
            if (A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) {
                tr = d;
                break;
            }
            if (d > TMO) {
                tr = 0xFFFFFF;
                break;
            }
        }
        (void)A0->ADC_RESULT;
        sum += tr;
        sum_s += ts;
    }
    console_puts(label);
    put_field("resrdy", sum / 4U, 6);
    put_field("start_clear", sum_s / 4U, 6);
    put_field("result", A0->ADC_RESULT, 4);
    nl();
}

static void free_run(void)
{
    uint32_t t[6], t0;

    adc_setup(ADC_CTRLB_PRESCALER_DIV32, ADC_CTRLC_RESSEL_12BIT |
              ADC_CTRLC_FREERUN_Msk, 0, 0, CORE, 0);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    for (unsigned int k = 0; k < 6; k++) {
        while (!(A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) &&
               scope_since(t0) < TMO) {
        }
        t[k] = scope_since(t0);
        (void)A0->ADC_RESULT;
    }
    console_puts("F1 freerun");
    for (unsigned int k = 0; k < 6; k++) {
        console_putc(' ');
        console_put_dec(t[k]);
    }
    nl();
}

static void sequence(const char *label, uint32_t seq, uint32_t ctrlc,
                     int read)
{
    uint32_t t[6], st[6], r[6], t0;
    unsigned int n = 0;

    adc_setup(ADC_CTRLB_PRESCALER_DIV32, ADC_CTRLC_RESSEL_12BIT | ctrlc, 0,
              0, CORE, seq);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    if (!read) {
        scope_wait_until(t0, SCOPE_CYC(500));
        console_puts(label);
        put_field("intflag", A0->ADC_INTFLAG, 2);
        put_field("seqstatus", A0->ADC_SEQSTATUS, 2);
        put_field("result", A0->ADC_RESULT, 4);
        put_field("swtrig", A0->ADC_SWTRIG, 2);
        nl();
        return;
    }
    while (n < 6 && scope_since(t0) < TMO) {
        if (A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) {
            t[n] = scope_since(t0);
            st[n] = A0->ADC_SEQSTATUS;
            r[n] = A0->ADC_RESULT;
            n++;
        }
        if (!(ctrlc & ADC_CTRLC_FREERUN_Msk) && n >= 2 &&
            !(A0->ADC_SEQSTATUS & ADC_SEQSTATUS_SEQBUSY_Msk)) {
            break;
        }
    }
    console_puts(label);
    for (unsigned int k = 0; k < n; k++) {
        console_puts(" |");
        console_put_dec(t[k]);
        put_field("seq", st[k], 2);
        put_field("r", r[k], 4);
    }
    put_field("seqstatus_end", A0->ADC_SEQSTATUS, 2);
    nl();
}

static void double_buffer(void)
{
    uint32_t r[8], sb_cyc, t0;

    adc_setup(ADC_CTRLB_PRESCALER_DIV32, ADC_CTRLC_RESSEL_12BIT |
              ADC_CTRLC_FREERUN_Msk, 0, 0, CORE, 0);
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    for (unsigned int k = 0; k < 3; k++) {
        while (!(A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk)) {
        }
        r[k] = A0->ADC_RESULT;
    }
    t0 = scope_now();
    A0->ADC_INPUTCTRL = ANA;
    while ((A0->ADC_SYNCBUSY & ADC_SYNCBUSY_INPUTCTRL_Msk) &&
           scope_since(t0) < TMO) {
    }
    sb_cyc = scope_since(t0);
    for (unsigned int k = 3; k < 8; k++) {
        while (!(A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) &&
               scope_since(t0) < TMO) {
        }
        r[k] = A0->ADC_RESULT;
    }
    console_puts("B1 dbuf");
    put_field("syncbusy_cyc", sb_cyc, 6);
    for (unsigned int k = 0; k < 8; k++) {
        console_putc(' ');
        console_put_dec(r[k]);
    }
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP adcprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_start();

    timed("T1 div32_12b_s15", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_12BIT, ADC_SAMPCTRL_SAMPLEN(15), 0);
    timed("T2 div32_12b_s0", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_12BIT, 0, 0);
    timed("T3 div2_12b_s0", ADC_CTRLB_PRESCALER_DIV2,
          ADC_CTRLC_RESSEL_12BIT, 0, 0);
    timed("T4 div32_8b_s0", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_8BIT, 0, 0);
    timed("T5 div32_10b_s0", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_10BIT, 0, 0);
    timed("T6 div32_12b_offcomp", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_12BIT, ADC_SAMPCTRL_OFFCOMP_Msk, 0);
    timed("T7 div256_12b_s0", ADC_CTRLB_PRESCALER_DIV256,
          ADC_CTRLC_RESSEL_12BIT, 0, 0);
    timed("T8 div32_avg16", ADC_CTRLB_PRESCALER_DIV32,
          ADC_CTRLC_RESSEL_16BIT, 0, ADC_AVGCTRL_SAMPLENUM_16 |
          ADC_AVGCTRL_ADJRES(4));
    free_run();
    sequence("S1 seq2", (1U << CORE) | (1U << ANA), 0, 1);
    sequence("S2 seq4_noread", 3U | (1U << CORE) | (1U << ANA), 0, 0);
    sequence("S3 seq2_freerun", (1U << CORE) | (1U << ANA),
             ADC_CTRLC_FREERUN_Msk, 1);
    double_buffer();

    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
