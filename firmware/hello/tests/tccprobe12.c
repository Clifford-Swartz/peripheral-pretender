/*
 * TCC probe 12 (PIC32CM MC00, TCC U2213): the circular buffers (DS
 * 34.6.3.2, WAVE.CIPEREN and CICCENx), where tccprobe10 saw CC0 alternate
 * per ramp and PER per cycle in DSBOTH but registers that read as if
 * nothing was exchanged. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe12
 *
 * TCC0 WO0 on PA04 (function E, PULLEN); GCLK_TCC0 1 MHz, DIV1. Each case
 * runs twice: once recording WO0's edges, once reading CC0, CCBUF0, PER,
 * PERBUF and STATUS at the listed times (us after the enable). The buffer
 * is written after PER and CC0 (with a SYNCBUSY wait between).
 *
 *   H1  DSBOTH PER 49, CC0 10, CCBUF0 30, CICCEN0; reads at 30, 80, 130, 180
 *   H2  DSBOTH PER 49, PERBUF 29, CC0 10, CIPEREN; reads likewise
 *   H3  RAMP2 PER 49, CC0 29, CCBUF0 10, CICCEN0; reads at 30, 80, 130, 180
 *   H4  RAMP2 PER 49, PERBUF 29, CC0 20, CIPEREN
 *   H5  NPWM (RAMP1) PER 49, CC0 29, CCBUF0 10, CICCEN0
 *   H6  DSBOTH PER 49, CC0 10, CCBUF0 30, no circular buffer
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_E          4U
#define W0              4U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define NE              64U
#define CICCEN0         (1U << 8)
#define CIPEREN         (1U << 7)

typedef struct {
    const char *label;
    uint32_t wave, per, perbuf, cc0, ccbuf0;
} Case;

static ScopeEdge e[NE];

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void setup(const Case *c)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_WAVE = c->wave;
    sync0();
    T0->TCC_PER = c->per;
    T0->TCC_CC[0] = c->cc0;
    sync0();
    if (c->perbuf) {
        T0->TCC_PERBUF = c->perbuf;
    }
    if (c->ccbuf0) {
        T0->TCC_CCBUF[0] = c->ccbuf0;
    }
}

static void run_case(const Case *c)
{
    static const uint16_t at[4] = { 30, 80, 130, 180 };
    uint32_t t0, r[4][5];
    unsigned int ne;

    setup(c);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, 1U << W0, t0, SCOPE_CYC(420), e, NE);
    console_puts(c->label);
    scope_put("w0", e, ne, W0, 14);
    console_puts("\r\n");

    setup(c);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int i = 0; i < 4; i++) {
        scope_wait_until(t0, SCOPE_CYC(at[i]));
        r[i][0] = T0->TCC_CC[0];
        r[i][1] = T0->TCC_CCBUF[0];
        r[i][2] = T0->TCC_PER;
        r[i][3] = T0->TCC_PERBUF;
        r[i][4] = T0->TCC_STATUS;
    }
    for (unsigned int i = 0; i < 4; i++) {
        console_puts("   @");
        console_put_dec(at[i]);
        put_field("cc0", r[i][0], 2);
        put_field("ccbuf0", r[i][1], 2);
        put_field("per", r[i][2], 2);
        put_field("perbuf", r[i][3], 2);
        put_field("status", r[i][4], 8);
        console_puts("\r\n");
    }
}

static const Case cases[] = {
    { "H1 dsboth_circ_cc0", TCC_WAVE_WAVEGEN_DSBOTH | CICCEN0, 49, 0, 10,
      30 },
    { "H2 dsboth_circ_per", TCC_WAVE_WAVEGEN_DSBOTH | CIPEREN, 49, 29, 10,
      0 },
    { "H3 ramp2_circ_cc0", TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_RAMP_RAMP2 |
      CICCEN0, 49, 0, 29, 10 },
    { "H4 ramp2_circ_per", TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_RAMP_RAMP2 |
      CIPEREN, 49, 29, 20, 0 },
    { "H5 npwm_circ_cc0", TCC_WAVE_WAVEGEN_NPWM | CICCEN0, 49, 0, 29, 10 },
    { "H6 dsboth_plain", TCC_WAVE_WAVEGEN_DSBOTH, 49, 0, 10, 30 },
};

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe12: " BOARD_BANNER "\r\n");
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
    scope_pmux(GA, W0, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << W0;
    scope_start();
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        run_case(&cases[i]);
    }
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_release(GA, W0);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
