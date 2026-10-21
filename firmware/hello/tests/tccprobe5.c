/*
 * TCC probe 5 (PIC32CM MC00, TCC U2213): what STOP does with prescaler
 * DIV1, where tccprobe4 S1 saw CMP kept at first and 0 later, the pins
 * then reading as undriven. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe5
 *
 * TCC0 WO0..3 on PA04, PA05, PA18, PA19; PER 99, CC 20/40/60/80 (NPWM).
 * Each line: CMP/pins before the STOP, then at 0, 2, 5, 10, 50, 200 us.
 *
 *   T1  1 MHz, DIV1      T2  1 MHz, DIV2      T3  48 MHz, DIV1
 *   T4  1 MHz, DIV1, CC2 and CC3 above PER (outputs high throughout)
 *   T5  1 MHz, DIV1, CC0 above PER and CC2 = 0: WO0 high, WO2 low, the
 *       opposite of the levels the undriven pins float to
 *   T6  the same at 48 MHz, DIV64
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_E          4U
#define FUNC_F          5U

static const uint8_t wo_pin[4] = { 4, 5, 18, 19 };
static const uint32_t at_us[6] = { 0, 2, 5, 10, 50, 200 };

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

static void stop_case(const char *label, unsigned int gen, uint32_t div,
                      uint32_t cc0, uint32_t cc2, uint32_t cc3,
                      uint32_t run_us)
{
    uint32_t cmp[7], pin[7], t;

    gclk(gen);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_CTRLA = div;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = cc0;
    T0->TCC_CC[1] = 40;
    T0->TCC_CC[2] = cc2;
    T0->TCC_CC[3] = cc3;
    sync0();
    t = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    scope_wait_until(t, SCOPE_CYC(run_us));
    cmp[0] = T0->TCC_STATUS >> 24 & 0xFU;
    pin[0] = pins();
    t = scope_now();
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    for (unsigned int i = 0; i < 6; i++) {
        scope_wait_until(t, SCOPE_CYC(at_us[i]));
        cmp[i + 1] = T0->TCC_STATUS >> 24 & 0xFU;
        pin[i + 1] = pins();
    }
    console_puts(label);
    console_puts(" before=");
    console_put_hex(cmp[0], 1);
    console_putc('/');
    console_put_hex(pin[0], 1);
    for (unsigned int i = 0; i < 6; i++) {
        console_puts(" @");
        console_put_dec(at_us[i]);
        console_putc('=');
        console_put_hex(cmp[i + 1], 1);
        console_putc('/');
        console_put_hex(pin[i + 1], 1);
    }
    put_field("status", T0->TCC_STATUS & 0xFFU, 2);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe5: " BOARD_BANNER "\r\n");
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
    stop_case("T1 1mhz_div1", 1, TCC_CTRLA_PRESCALER_DIV1, 20, 60, 80, 50);
    stop_case("T2 1mhz_div2", 1, TCC_CTRLA_PRESCALER_DIV2, 20, 60, 80, 100);
    stop_case("T3 48mhz_div1", 0, TCC_CTRLA_PRESCALER_DIV1, 20, 60, 80, 50);
    stop_case("T4 1mhz_div1_high", 1, TCC_CTRLA_PRESCALER_DIV1, 20, 120,
              150, 50);
    /* WO0 always high, WO2 always low: kept and floating levels differ */
    stop_case("T5 1mhz_div1_wo0hi_wo2lo", 1, TCC_CTRLA_PRESCALER_DIV1, 120,
              0, 80, 50);
    stop_case("T6 48mhz_div64_wo0hi_wo2lo", 0, TCC_CTRLA_PRESCALER_DIV64,
              120, 0, 80, 60);
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
