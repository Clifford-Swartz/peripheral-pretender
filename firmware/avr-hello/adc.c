/*
 * ADC0 check on the AVR64EA48: conversions of the internal inputs, which
 * read the same on the twin and on silicon. Each line names the input,
 * the reference and the mode, and gives RESULT (and what it should be,
 * where that follows from VDD, measured first from VDD/10 against the
 * 1.024 V reference). Ends with "PP:EXIT n", n the failed checks.
 *
 *   vdd10/1v024     VDD in mV (the Curiosity Nano runs at 3.3 V)
 *   vdd10/vdd       409 (a tenth of full scale), any VDD
 *   vdd10/2v048, /2v500, /4v096   VDD/10 against each reference
 *   gnd/1v024       0
 *   8bit, leftadj   the 12-bit code >> 4, << 4
 *   burst16         16 conversions accumulated: 16 x the 12-bit code
 *   diff vdd10-gnd  half the single-ended code: signed, +-VREF over
 *                   +-2048
 *   resrdy          INTFLAGS.RESRDY before and after reading RESULT
 *   temp            the temperature sensor and SIGROW's calibration
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <util/delay.h>

static uint8_t fails;
static volatile uint32_t sink;

static void put_hex(uint32_t v, uint8_t bytes)
{
    console_puts("0x");
    while (bytes--) {
        console_hex2((uint8_t)(v >> (8U * bytes)));
    }
}

static void field(const char *name, uint32_t v, uint8_t bytes)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    put_hex(v, bytes);
}

/* One conversion: @mode (COMMAND.MODE), @muxpos against @refsel */
static uint32_t adc_convert(uint8_t refsel, uint8_t muxpos, uint8_t mode)
{
    ADC0.CTRLC = refsel;
    ADC0.MUXPOS = muxpos;
    _delay_us(100);                         /* reference settling */
    ADC0.COMMAND = mode | ADC_START_IMMEDIATE_gc;
    for (uint16_t n = 0; !(ADC0.INTFLAGS & ADC_RESRDY_bm); n++) {
        if (n == 60000U) {
            console_puts("timeout\r\n");
            return 0xFFFFFFFFUL;
        }
    }
    return ADC0.RESULT;
}

static void check(const char *name, uint32_t got, uint32_t expect,
                  uint32_t tol)
{
    uint8_t ok = got + tol >= expect && got <= expect + tol;

    console_puts(name);
    field("result", got, 4);
    field("expect", expect, 4);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

int main(void)
{
    uint32_t c12, vdd_mv, r;
    uint8_t before, after;

    console_init();
    console_puts("PP adc: " BOARD_BANNER "\r\n");

    ADC0.CTRLB = ADC_PRESC_DIV4_gc;
    ADC0.CTRLE = 16;                        /* sample duration */
    ADC0.CTRLA = ADC_ENABLE_bm;

    c12 = adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_VDD10_gc,
                      ADC_MODE_SINGLE_12BIT_gc);
    vdd_mv = c12 * 10240UL / 4095UL;
    console_puts("vdd10/1v024");
    field("result", c12, 4);
    field("vdd_mv", vdd_mv, 2);
    console_puts("\r\n");
    fails += vdd_mv < 1800U || vdd_mv > 5500U;

    check("vdd10/vdd", adc_convert(ADC_REFSEL_VDD_gc, ADC_MUXPOS_VDD10_gc,
                                   ADC_MODE_SINGLE_12BIT_gc), 409, 12);
    check("vdd10/2v048", adc_convert(ADC_REFSEL_2V048_gc, ADC_MUXPOS_VDD10_gc,
                                     ADC_MODE_SINGLE_12BIT_gc),
          vdd_mv * 4095UL / 20480UL, 12);
    check("vdd10/2v500", adc_convert(ADC_REFSEL_2V500_gc, ADC_MUXPOS_VDD10_gc,
                                     ADC_MODE_SINGLE_12BIT_gc),
          vdd_mv * 4095UL / 25000UL, 12);
    /* A reference above VDD acts as VDD (silicon: 4.096 V at VDD 3.3 V) */
    check("vdd10/4v096", adc_convert(ADC_REFSEL_4V096_gc, ADC_MUXPOS_VDD10_gc,
                                     ADC_MODE_SINGLE_12BIT_gc),
          vdd_mv < 4096U ? 409UL : vdd_mv * 4095UL / 40960UL, 12);
    check("gnd/1v024", adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_GND_gc,
                                   ADC_MODE_SINGLE_12BIT_gc), 0, 8);
    check("8bit", adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_VDD10_gc,
                              ADC_MODE_SINGLE_8BIT_gc), c12 >> 4, 2);
    ADC0.CTRLF = ADC_LEFTADJ_bm;
    check("leftadj", adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_VDD10_gc,
                                 ADC_MODE_SINGLE_12BIT_gc), c12 << 4, 12 << 4);
    ADC0.CTRLF = ADC_SAMPNUM_ACC16_gc;
    check("burst16", adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_VDD10_gc,
                                 ADC_MODE_BURST_gc), c12 * 16UL, 12UL * 16UL);
    ADC0.CTRLF = 0;
    ADC0.MUXNEG = ADC_MUXNEG_GND_gc;
    r = adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_VDD10_gc,
                    ADC_MODE_SINGLE_12BIT_gc | ADC_DIFF_bm);
    check("diff vdd10-gnd", r, c12 / 2, 8);

    /* RESRDY: set by a conversion, cleared by reading RESULT */
    ADC0.COMMAND = ADC_MODE_SINGLE_12BIT_gc | ADC_START_IMMEDIATE_gc;
    for (uint16_t n = 0; !(ADC0.INTFLAGS & ADC_RESRDY_bm) && n < 60000U;
         n++) {
    }
    before = ADC0.INTFLAGS & ADC_RESRDY_bm;
    /* Into a volatile: avr-gcc (XC8 4.00) drops "(void)ADC0.RESULT;" */
    sink = ADC0.RESULT;
    after = ADC0.INTFLAGS & ADC_RESRDY_bm;
    console_puts("resrdy");
    field("before", before, 1);
    field("after_read", after, 1);
    console_puts(before && !after ? " ok\r\n" : " FAIL\r\n");
    fails += !(before && !after);

    r = adc_convert(ADC_REFSEL_1V024_gc, ADC_MUXPOS_TEMPSENSE_gc,
                    ADC_MODE_SINGLE_12BIT_gc);
    console_puts("temp");
    field("result", r, 4);
    field("sigrow_tempsense0", SIGROW.TEMPSENSE0, 2);
    field("sigrow_tempsense1", SIGROW.TEMPSENSE1, 2);
    console_puts("\r\n");

    console_puts("PP:EXIT ");
    console_hex2(fails);
    console_puts("\r\n");
    for (;;) {
    }
}
