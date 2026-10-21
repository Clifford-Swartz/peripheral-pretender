/*
 * TCC probe 2 (PIC32CM MC00, TCC U2213): the waveform extension and the
 * modes the first round left out, for the model: output matrix, dead
 * time, swap, polarity, pattern generation, inversion, DSCRITICAL, the
 * RAMP2 modes and dithering. Prints raw results; checks nothing. The plan
 * and the answers: docs/research/peripherals/tcc-u2213.md.
 * Run: py -m tools.periph probe mc00 tccprobe2
 *
 * Bare EV15N46A. TCC0's eight outputs on port A pins of the headers:
 * WO0 PA04, WO1 PA05 (function E), WO2 PA18, WO3 PA19, WO4 PA22, WO5 PA23,
 * WO6 PA16, WO7 PA17 (function F), watched through PORT.IN (scope.h).
 * GCLK_TCC0 is 1 MHz (OSC48M / 48, generator 1), prescaler DIV1: a tick
 * and a dead-time step are 1 us. Times in us from the enable write; each
 * output prints its level at the start and its first changes.
 *
 *   B0   writable bits of WEXCTRL, DRVCTRL, PATT, WAVE on TCC0..2
 *   B1   NPWM PER 99, CC 20/40/60/80, OTMX 0..3
 *   B2   dead time: DTIEN0, DTLS 5, DTHS 10; all four slices
 *   B3   DTI0 with SWAP0
 *   B4   POL0, POL1
 *   B5   pattern: PGE0 PGV0=1, PGE4 PGV4=0, PGE1 PGV1=1; PATTBUF at update
 *   B6   INVEN0/INVEN4 with DTI0
 *   B7   DSCRITICAL: PER 49, CC0 10, CC2 30
 *   B8   RAMP2, RAMP2A, RAMP2C
 *   B9   DITH4: on PER, on CC0
 *   B10  outputs with DTI0 after STOP, after a one-shot, when disabled
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_E          4U
#define FUNC_F          5U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define NE              128U

static const uint8_t wo_pin[8] = { 4, 5, 18, 19, 22, 23, 16, 17 };
static uint32_t wo_mask;
static ScopeEdge e[NE];
static unsigned int ne;

static void nl(void)
{
    console_puts("\r\n");
}

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void tcc_reset(void)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
}

/* Enable TCC0 (already configured) and record @us of all outputs */
static void run(uint32_t us)
{
    uint32_t t0;

    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, wo_mask, t0, SCOPE_CYC(us), e, NE);
}

/* Each output: level at the start and its first @k changes */
static void put_outputs(const char *label, unsigned int k, uint8_t which)
{
    console_puts(label);
    for (unsigned int x = 0; x < 8; x++) {
        char name[3] = { 'w', (char)('0' + x), 0 };

        if (which & (1U << x)) {
            scope_put(name, e, ne, wo_pin[x], k);
        }
    }
    nl();
}

static void npwm(uint32_t per, uint32_t cc0, uint32_t cc1, uint32_t cc2,
                 uint32_t cc3)
{
    tcc_reset();
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_PER = per;
    T0->TCC_CC[0] = cc0;
    T0->TCC_CC[1] = cc1;
    T0->TCC_CC[2] = cc2;
    T0->TCC_CC[3] = cc3;
}

static void b0_regs(void)
{
    tcc_registers_t *t[3] = { TCC0_REGS, TCC1_REGS, TCC2_REGS };

    console_puts("B0 regs");
    for (unsigned int i = 0; i < 3; i++) {
        t[i]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
        t[i]->TCC_WEXCTRL = 0xFFFFFFFFU;
        t[i]->TCC_DRVCTRL = 0xFFFFFFFFU;
        t[i]->TCC_PATT = 0xFFFFU;
        t[i]->TCC_WAVE = 0xFFFFFFFFU;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
        console_puts(" |tcc");
        console_put_dec(i);
        put_field("wexctrl", t[i]->TCC_WEXCTRL, 8);
        put_field("drvctrl", t[i]->TCC_DRVCTRL, 8);
        put_field("patt", t[i]->TCC_PATT, 4);
        put_field("wave", t[i]->TCC_WAVE, 8);
        t[i]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
    }
    nl();
}

static void b1_otmx(void)
{
    static const char *const label[4] = { "B1 otmx0", "B1 otmx1",
                                          "B1 otmx2", "B1 otmx3" };

    for (unsigned int m = 0; m < 4; m++) {
        npwm(99, 20, 40, 60, 80);
        T0->TCC_WEXCTRL = TCC_WEXCTRL_OTMX(m);
        run(150);
        put_outputs(label[m], 3, 0xFF);
    }
}

static void b2_dti(void)
{
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    run(250);
    put_outputs("B2 dti0", 4, 0x11);
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN_Msk | TCC_WEXCTRL_DTLS(3) |
                      TCC_WEXCTRL_DTHS(7);
    run(150);
    put_outputs("B2 dti_all", 3, 0xFF);
}

static void b3_swap(void)
{
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_SWAP0_Msk;
    run(250);
    put_outputs("B3 swap0", 4, 0x11);
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_SWAP0_Msk;
    run(150);
    put_outputs("B3 swap0_nodti", 3, 0x11);
}

static void b4_pol(void)
{
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk |
                   (TCC_WAVE_POL0_Msk << 1);
    run(250);
    put_outputs("B4 pol01", 4, 0x0F);
}

static void b5_patt(void)
{
    uint32_t in1, in2;

    npwm(99, 20, 40, 60, 80);
    T0->TCC_PATT = TCC_PATT_PGE0_Msk | (TCC_PATT_PGE0_Msk << 4) |
                   (TCC_PATT_PGE0_Msk << 1) | TCC_PATT_PGV0_Msk |
                   (TCC_PATT_PGV0_Msk << 1);
    run(150);
    put_outputs("B5 patt", 3, 0xFF);
    /* PATTBUF: copied at the next update (the wrap) */
    T0->TCC_PATTBUF = 0;
    in1 = PORT_REGS->GROUP[GA].PORT_IN;
    scope_wait_until(scope_now(), SCOPE_CYC(110));
    in2 = PORT_REGS->GROUP[GA].PORT_IN;
    console_puts("B5 pattbuf");
    put_field("patt_now", T0->TCC_PATT, 4);
    put_field("w0_before", (in1 >> wo_pin[0]) & 1U, 1);
    put_field("w4_before", (in1 >> wo_pin[4]) & 1U, 1);
    put_field("w4_after", (in2 >> wo_pin[4]) & 1U, 1);
    put_field("status", T0->TCC_STATUS, 8);
    nl();
}

static void b6_inven(void)
{
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    T0->TCC_DRVCTRL = TCC_DRVCTRL_INVEN0_Msk | TCC_DRVCTRL_INVEN4_Msk;
    run(250);
    put_outputs("B6 inven_dti", 4, 0x11);
}

static void b7_dscritical(void)
{
    tcc_reset();
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_DSCRITICAL;
    T0->TCC_PER = 49;
    T0->TCC_CC[0] = 10;
    T0->TCC_CC[1] = 20;
    T0->TCC_CC[2] = 30;
    T0->TCC_CC[3] = 40;
    run(250);
    put_outputs("B7 dscritical", 5, 0x0F);
}

static void b8_ramp(void)
{
    static const uint32_t ramp[3] = { TCC_WAVE_RAMP_RAMP2,
                                      TCC_WAVE_RAMP_RAMP2A,
                                      TCC_WAVE_RAMP_RAMP2C };
    static const char *const label[3] = { "B8 ramp2", "B8 ramp2a",
                                          "B8 ramp2c" };

    for (unsigned int r = 0; r < 3; r++) {
        tcc_reset();
        T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | ramp[r];
        T0->TCC_PER = 49;
        T0->TCC_CC[0] = 29;
        T0->TCC_CC[1] = 20;
        T0->TCC_CC[2] = 10;
        T0->TCC_CC[3] = 40;
        run(250);
        put_outputs(label[r], 5, 0x0F);
    }
}

/* Successive periods (rise to rise) and high times of WO0 */
static void put_dith(const char *label)
{
    uint32_t last_r = 0, last_f = 0;
    unsigned int shown = 0;
    uint32_t m = 1U << wo_pin[0];

    console_puts(label);
    console_puts(" periods/highs=");
    for (unsigned int i = 1; i < ne && shown < 18; i++) {
        if (!((e[i].v ^ e[i - 1].v) & m)) {
            continue;
        }
        if (e[i].v & m) {
            if (last_r) {
                console_put_dec(SCOPE_US(e[i].t - last_r));
                console_putc('/');
                console_put_dec(SCOPE_US(last_f - last_r));
                console_putc(' ');
                shown++;
            }
            last_r = e[i].t;
        } else {
            last_f = e[i].t;
        }
    }
    nl();
}

static void b9_dith(void)
{
    tcc_reset();
    T0->TCC_CTRLA = TCC_CTRLA_RESOLUTION_DITH4;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_PER = (49U << 4) | 8U;          /* 49 + 8/16 */
    T0->TCC_CC[0] = 20U << 4;
    run(1200);
    put_dith("B9 dith4_per");
    tcc_reset();
    T0->TCC_CTRLA = TCC_CTRLA_RESOLUTION_DITH4;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_PER = 49U << 4;
    T0->TCC_CC[0] = (20U << 4) | 4U;        /* 20 + 4/16 */
    run(1200);
    put_dith("B9 dith4_cc");
    console_puts("B9 regs");
    put_field("per", T0->TCC_PER, 8);
    put_field("cc0", T0->TCC_CC[0], 8);
    nl();
}

static void b10_stops(void)
{
    uint32_t in_stop, in_os, in_dis;

    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    run(50);                                /* WO0 low, WO4 high */
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    sync0();
    scope_wait_until(scope_now(), SCOPE_CYC(30));
    in_stop = PORT_REGS->GROUP[GA].PORT_IN;
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    T0->TCC_CTRLBSET = TCC_CTRLBSET_ONESHOT_Msk;
    run(150);
    in_os = PORT_REGS->GROUP[GA].PORT_IN;
    put_outputs("B10 oneshot", 4, 0x11);
    npwm(99, 20, 40, 60, 80);
    T0->TCC_WEXCTRL = TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
                      TCC_WEXCTRL_DTHS(10);
    run(50);
    T0->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    sync0();
    in_dis = PORT_REGS->GROUP[GA].PORT_IN;
    console_puts("B10 levels");
    put_field("stop_w0", (in_stop >> wo_pin[0]) & 1U, 1);
    put_field("stop_w4", (in_stop >> wo_pin[4]) & 1U, 1);
    put_field("oneshot_w0", (in_os >> wo_pin[0]) & 1U, 1);
    put_field("oneshot_w4", (in_os >> wo_pin[4]) & 1U, 1);
    put_field("disabled_w0", (in_dis >> wo_pin[0]) & 1U, 1);
    put_field("disabled_w4", (in_dis >> wo_pin[4]) & 1U, 1);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_TCC1_Msk |
                                MCLK_APBCMASK_TCC2_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC2_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    for (unsigned int x = 0; x < 8; x++) {
        scope_pmux(GA, wo_pin[x], x < 2 ? FUNC_E : FUNC_F, 0);
        wo_mask |= 1U << wo_pin[x];
    }
    scope_start();
    b0_regs();
    b1_otmx();
    b2_dti();
    b3_swap();
    b4_pol();
    b5_patt();
    b6_inven();
    b7_dscritical();
    b8_ramp();
    b9_dith();
    b10_stops();
    tcc_reset();
    for (unsigned int x = 0; x < 8; x++) {
        scope_release(GA, wo_pin[x]);
    }
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
