/*
 * TCC probe 6 (PIC32CM MC00, TCC U2213): does a pin's pull resistor act
 * once a STOP (or the end of a one-shot) tri-states its output, as it does
 * not when the TCC is disabled (tcc.c: PA08 reads 0 with the pull-up)?
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe6
 *
 * TCC0 WO0 on PA04 (function E), PINCFG PMUXEN | INEN | PULLEN, the pull
 * set against the output's level; GCLK_TCC0 1 MHz, DIV1, PER 99. Each line:
 * PA04 before, then at 0, 10, 50, 200 and 1000 us after the event.
 *
 *   U1  STOP, WO0 high (CC0 above PER), pull-down
 *   U2  STOP, WO0 low (CC0 = 0), pull-up
 *   U3  disable, WO0 high, pull-down
 *   U4  disable, WO0 low, pull-up
 *   U5  the end of a one-shot, WO0 high, pull-down
 *   U6  STOP with NRE0 and NRV0 = 0, WO0 high, pull-up (driven: stays 0)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define PIN             4U
#define FUNC_E          4U

enum { EV_STOP, EV_DISABLE, EV_ONESHOT };

static const uint32_t at_us[5] = { 0, 10, 50, 200, 1000 };

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 2000000U; i++) {
    }
}

static uint32_t pa04(void)
{
    return (PORT_REGS->GROUP[GA].PORT_IN >> PIN) & 1U;
}

static void pull_case(const char *label, int ev, uint32_t cc0, int up,
                      uint32_t drvctrl)
{
    uint32_t v[6], t;

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, PIN, up);
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = cc0;
    T0->TCC_DRVCTRL = drvctrl;
    if (ev == EV_ONESHOT) {
        T0->TCC_CTRLBSET = TCC_CTRLBSET_ONESHOT_Msk;
    }
    sync0();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    sync0();
    t = scope_now();
    scope_wait_until(t, SCOPE_CYC(50));
    v[0] = pa04();
    if (ev == EV_ONESHOT) {
        /* The one-shot ends at the wrap, about 100 us after the start */
        scope_wait_until(t, SCOPE_CYC(110));
    } else if (ev == EV_STOP) {
        T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    } else {
        T0->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    }
    t = scope_now();
    for (unsigned int i = 0; i < 5; i++) {
        scope_wait_until(t, SCOPE_CYC(at_us[i]));
        v[i + 1] = pa04();
    }
    console_puts(label);
    console_puts(" before=");
    console_put_dec(v[0]);
    for (unsigned int i = 0; i < 5; i++) {
        console_puts(" @");
        console_put_dec(at_us[i]);
        console_putc('=');
        console_put_dec(v[i + 1]);
    }
    put_field("status", T0->TCC_STATUS & 0xFFU, 2);
    console_puts("\r\n");
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe6: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    scope_start();
    pull_case("U1 stop_high_pulldown", EV_STOP, 120, 0, 0);
    pull_case("U2 stop_low_pullup", EV_STOP, 0, 1, 0);
    pull_case("U3 disable_high_pulldown", EV_DISABLE, 120, 0, 0);
    pull_case("U4 disable_low_pullup", EV_DISABLE, 0, 1, 0);
    pull_case("U5 oneshot_high_pulldown", EV_ONESHOT, 120, 0, 0);
    pull_case("U6 stop_nrv0_pullup", EV_STOP, 120, 1,
              TCC_DRVCTRL_NRE0_Msk);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_release(GA, PIN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
