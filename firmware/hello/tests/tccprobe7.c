/*
 * TCC probe 7 (PIC32CM MC00, TCC U2213): after tccprobe6 found PA04 at 0
 * after every STOP and disable, pull-up or pull-down: does the pull of a
 * released output follow the waveform's state (STATUS.CMP0, kept after a
 * STOP except with DIV1) rather than PORT.OUT? Prints raw results; checks
 * nothing. Run: py -m tools.periph probe mc00 tccprobe7
 *
 * TCC0 WO0 on PA04 (function E), PINCFG PMUXEN | INEN | PULLEN; WO0 high
 * (CC0 above PER). Each line: PA04 before, then at 0, 10, 50, 200 and 1000
 * us after the STOP, and STATUS.CMP0 at the end.
 *
 *   V1  48 MHz, DIV64 (CMP kept), pull-down
 *   V2  48 MHz, DIV64, pull-up
 *   V3  1 MHz, DIV1 (CMP cleared), pull-up
 *   V4  192 kHz (OSC48M / 250), DIV1, PER 3, CC0 4, pull-up (tcc.c's case)
 *   V5  48 MHz, DIV64, no pull
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define PIN             4U
#define FUNC_E          4U

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

/* @pull: 0 down, 1 up, -1 none */
static void pull_case(const char *label, unsigned int gen, uint32_t div,
                      uint32_t per, uint32_t cc0, int pull)
{
    uint32_t v[6], t;

    gclk(gen);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pmux(GA, PIN, FUNC_E, pull < 0 ? 0 : PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN, pull > 0);
    T0->TCC_CTRLA = div;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = per;
    T0->TCC_CC[0] = cc0;
    sync0();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    sync0();
    t = scope_now();
    scope_wait_until(t, SCOPE_CYC(200));
    v[0] = pa04();
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
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
    put_field("cmp0", T0->TCC_STATUS >> 24 & 1U, 1);
    put_field("status", T0->TCC_STATUS & 0xFFU, 2);
    console_puts("\r\n");
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe7: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(250) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    scope_start();
    pull_case("V1 div64_pulldown", 0, TCC_CTRLA_PRESCALER_DIV64, 99, 120, 0);
    pull_case("V2 div64_pullup", 0, TCC_CTRLA_PRESCALER_DIV64, 99, 120, 1);
    pull_case("V3 1mhz_div1_pullup", 1, TCC_CTRLA_PRESCALER_DIV1, 99, 120, 1);
    pull_case("V4 192khz_div1_pullup", 2, TCC_CTRLA_PRESCALER_DIV1, 3, 4, 1);
    pull_case("V5 div64_nopull", 0, TCC_CTRLA_PRESCALER_DIV64, 99, 120, -1);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_release(GA, PIN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
