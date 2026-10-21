/*
 * TCC probe 4 (PIC32CM MC00, TCC U2213): STATUS.CMPx and the outputs
 * after the STOP command, where tcc.c (CMP kept, 0xC) and tccprobe3 (CMP
 * 0) disagree. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe4
 *
 * TCC0 WO0..3 on PA04, PA05, PA18, PA19 (scope.h pins as in tccprobe2).
 *
 *   S1  1 MHz, DIV1, PER 99, CC 20/40/60/80, STOP at about 50 us
 *   S2  48 MHz, DIV1024, PER 9, CC 3/0/9/10, STOP at COUNT 5 (tcc.c)
 *   S3  1 MHz, DIV1024, PER 9, CC 3/0/9/10, STOP at COUNT 5
 *   S4  48 MHz, DIV64, PER 99, CC 20/40/60/80, STOP at COUNT 50
 * Each: CMP and pins at once and 200 us after the STOP, and COUNT.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_E          4U
#define FUNC_F          5U

static const uint8_t wo_pin[4] = { 4, 5, 18, 19 };

static void nl(void)
{
    console_puts("\r\n");
}

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 2000000U; i++) {
    }
}

static uint32_t pins(void)
{
    uint32_t in = PORT_REGS->GROUP[GA].PORT_IN, v = 0;

    for (unsigned int x = 0; x < 4; x++) {
        v |= ((in >> wo_pin[x]) & 1U) << x;
    }
    return v;
}

static uint32_t count(void)
{
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_READSYNC;
    sync0();
    for (uint32_t i = 0; (T0->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) &&
         i < 2000000U; i++) {
    }
    return T0->TCC_COUNT;
}

static void gclk(unsigned int gen)
{
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = 0;
    for (uint32_t i = 0; (GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                          GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(gen) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
}

/* Configure, enable, wait for COUNT >= @at (or @us), STOP, report */
static void stop_case(const char *label, unsigned int gen, uint32_t div,
                      uint32_t per, const uint32_t *cc, uint32_t at)
{
    uint32_t c_before, cmp_before, pin_before, cmp0, pin0, cmp1, pin1, c1;

    gclk(gen);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_CTRLA = div;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_PER = per;
    for (unsigned int i = 0; i < 4; i++) {
        T0->TCC_CC[i] = cc[i];
    }
    sync0();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    sync0();
    for (uint32_t i = 0; count() < at && i < 200000U; i++) {
    }
    c_before = count();
    cmp_before = T0->TCC_STATUS >> 24 & 0xFU;
    pin_before = pins();
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    sync0();
    cmp0 = T0->TCC_STATUS >> 24 & 0xFU;
    pin0 = pins();
    scope_wait_until(scope_now(), SCOPE_CYC(200));
    cmp1 = T0->TCC_STATUS >> 24 & 0xFU;
    pin1 = pins();
    c1 = count();
    console_puts(label);
    put_field("count", c_before, 2);
    put_field("cmp", cmp_before, 1);
    put_field("pins", pin_before, 1);
    console_puts(" | stop:");
    put_field("cmp", cmp0, 1);
    put_field("pins", pin0, 1);
    put_field("cmp_200us", cmp1, 1);
    put_field("pins_200us", pin1, 1);
    put_field("count", c1, 2);
    put_field("status", T0->TCC_STATUS & 0xFFU, 2);
    nl();
}

int main(void)
{
    static const uint32_t cc_a[4] = { 20, 40, 60, 80 };
    static const uint32_t cc_b[4] = { 3, 0, 9, 10 };

    sys_initialize();
    console_puts("PP tccprobe4: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    for (unsigned int x = 0; x < 4; x++) {
        scope_pmux(GA, wo_pin[x], x < 2 ? FUNC_E : FUNC_F, 0);
    }
    scope_start();
    stop_case("S1 1mhz_div1", 1, TCC_CTRLA_PRESCALER_DIV1, 99, cc_a, 46);
    stop_case("S2 48mhz_div1024", 0, TCC_CTRLA_PRESCALER_DIV1024, 9, cc_b,
              5);
    stop_case("S3 1mhz_div1024", 1, TCC_CTRLA_PRESCALER_DIV1024, 9, cc_b, 5);
    stop_case("S4 48mhz_div64", 0, TCC_CTRLA_PRESCALER_DIV64, 99, cc_a, 50);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    for (unsigned int x = 0; x < 4; x++) {
        scope_release(GA, wo_pin[x]);
    }
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
