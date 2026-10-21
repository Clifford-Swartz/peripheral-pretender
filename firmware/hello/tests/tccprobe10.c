/*
 * TCC probe 10 (PIC32CM MC00, TCC U2213): capture and circular buffers
 * (DS 34.6.2.7, 34.6.3.2): event capture into a CPTENx channel with the
 * CCx/CCBUFx FIFO and ERR, PPW and PWP through EV1, the capture's MSB in
 * dual slope, and the circular PER and CC buffers in DSBOTH. Prints raw
 * results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe10
 *
 * Wiring and clocks as tccprobe8: the input pin PA02 (EXTINT2, its pull
 * turned by PORT.OUT), HIGH sense, ASYNCH, asynchronous EVSYS channel 0
 * to TCC0 MC_2 (user 13) and EV_1 (user 10); TCC0 WO0 on PA04; GCLK_TCC0
 * 1 MHz, DIV1. The input pulses are made at set times (us after the
 * enable) while the edges are recorded; registers are read afterwards,
 * or at set times where a line says so.
 *
 *   D1  CPTEN2, MCEI2, NPWM PER 99: pulses at 20, 40, 60 (5 us); then CC2,
 *       STATUS, INTFLAG, three times over
 *   D2  the same with CC2 read at 30 and 50 us, between the pulses
 *   D3  CPTEN2 without MCEI2
 *   D4  PPW (EVACT1, TCEI1, CPTEN0/1): high 20-40, 70-90, 120-140 us
 *   D5  PWP, the same          D6  PPW with TCINV1
 *   D7  DSBOTTOM PER 99, CPTEN2/MCEI2: pulses at 30 (up) and 130 (down)
 *   D8  DSBOTH PER 49, CC0 10, CCBUF0 30, WAVE.CICCEN0: WO0's edges
 *   D9  DSBOTH PER 49, PERBUF 29, WAVE.CIPEREN, CC0 10: WO0's edges
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_A          0U
#define FUNC_E          4U
#define PIN_IN          2U                      /* PA02, EXTINT2 */
#define W0              4U
#define GEN_EIC2        16U
#define USER_EV1        10U
#define USER_MC2        13U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define SET_A           (&PORT_REGS->GROUP[GA].PORT_OUTSET)
#define CLR_A           (&PORT_REGS->GROUP[GA].PORT_OUTCLR)
#define NE              128U
#define CICCEN0         (1U << 8)
#define CIPEREN         (1U << 7)

static ScopeEdge e[NE];
static unsigned int ne;
static ScopeStim stim[8];

static void nl(void)
{
    console_puts("\r\n");
}

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

/* Input high from @t[2i] to @t[2i + 1] us, @n times */
static unsigned int pulses(const uint16_t *t, unsigned int n)
{
    for (unsigned int i = 0; i < 2 * n; i++) {
        stim[i].t = SCOPE_CYC(t[i]);
        stim[i].reg = (i & 1U) ? CLR_A : SET_A;
        stim[i].val = 1U << PIN_IN;
    }
    return 2 * n;
}

static void setup(uint32_t ctrla, uint32_t wave, uint32_t per,
                  uint32_t evctrl)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, PIN_IN, 0);
    T0->TCC_CTRLA = ctrla;
    T0->TCC_WAVE = wave;
    sync0();
    T0->TCC_PER = per;
    T0->TCC_CC[0] = 50;
    T0->TCC_CC[1] = 30;
    T0->TCC_EVCTRL = evctrl;
    sync0();
}

static void run(unsigned int nstim, uint32_t us)
{
    uint32_t t0 = scope_now();

    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record_stim(IN_A, (1U << PIN_IN) | (1U << W0), t0,
                           SCOPE_CYC(us), e, NE, stim, nstim);
}

static void put_regs(void)
{
    put_field("status", T0->TCC_STATUS, 8);
    put_field("intflag", T0->TCC_INTFLAG, 8);
}

static void fifo_case(const char *label, uint32_t evctrl)
{
    static const uint16_t t[] = { 20, 25, 40, 45, 60, 65 };

    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_NPWM, 99, evctrl);
    run(pulses(t, 3), 90);
    console_puts(label);
    scope_put("in", e, ne, PIN_IN, 6);
    for (unsigned int i = 0; i < 3; i++) {
        put_regs();
        put_field("cc2", T0->TCC_CC[2], 8);
    }
    put_regs();
    nl();
}

static void read_between(void)
{
    static const uint16_t t[] = { 20, 25, 40, 45 };
    uint32_t t0, c[2];

    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_NPWM, 99,
          TCC_EVCTRL_MCEI2_Msk);
    pulses(t, 2);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int i = 0; i < 4; i++) {
        scope_wait_until(t0, stim[i].t);
        *stim[i].reg = stim[i].val;
        if (i & 1U) {
            scope_wait_until(t0, stim[i].t + SCOPE_CYC(5));
            c[i / 2] = T0->TCC_CC[2];
        }
    }
    console_puts("D2 read_between");
    put_field("cc2_30us", c[0], 8);
    put_field("cc2_50us", c[1], 8);
    put_regs();
    nl();
}

static void ppw_case(const char *label, uint32_t evact, uint32_t inv)
{
    static const uint16_t t[] = { 20, 40, 70, 90, 120, 140 };

    setup(TCC_CTRLA_CPTEN0_Msk | TCC_CTRLA_CPTEN1_Msk,
          TCC_WAVE_WAVEGEN_NPWM, 0xFFFFFF, evact | TCC_EVCTRL_TCEI1_Msk |
          inv);
    run(pulses(t, 3), 160);
    console_puts(label);
    scope_put("in", e, ne, PIN_IN, 6);
    put_regs();
    put_field("cc0", T0->TCC_CC[0], 8);
    put_field("cc1", T0->TCC_CC[1], 8);
    put_regs();
    nl();
}

static void ds_capture(void)
{
    static const uint16_t t[] = { 30, 35, 130, 135 };
    uint32_t a;

    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_DSBOTTOM, 99,
          TCC_EVCTRL_MCEI2_Msk);
    run(pulses(t, 2), 150);
    a = T0->TCC_CC[2];
    console_puts("D7 ds_msb");
    scope_put("in", e, ne, PIN_IN, 4);
    put_field("cc2_first", a, 8);
    put_field("cc2_second", T0->TCC_CC[2], 8);
    put_regs();
    nl();
}

static void circular(const char *label, uint32_t wave, uint32_t per,
                     uint32_t perbuf, uint32_t ccbuf)
{
    setup(0, TCC_WAVE_WAVEGEN_DSBOTH | wave, per, 0);
    T0->TCC_CC[0] = 10;
    sync0();
    if (perbuf) {
        T0->TCC_PERBUF = perbuf;
    }
    if (ccbuf) {
        T0->TCC_CCBUF[0] = ccbuf;
    }
    run(0, 450);
    console_puts(label);
    scope_put("w0", e, ne, W0, 12);
    put_field("per", T0->TCC_PER, 8);
    put_field("perbuf", T0->TCC_PERBUF, 8);
    put_field("cc0", T0->TCC_CC[0], 8);
    put_field("ccbuf0", T0->TCC_CCBUF[0], 8);
    put_regs();
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe10: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN_IN, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN_IN, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * PIN_IN);          /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << PIN_IN;
    EIC_REGS->EIC_EVCTRL = 1U << PIN_IN;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 1U;
    scope_pmux(GA, W0, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << W0;
    scope_start();

    fifo_case("D1 fifo", TCC_EVCTRL_MCEI2_Msk);
    read_between();
    fifo_case("D3 no_mcei", 0);
    ppw_case("D4 ppw", TCC_EVCTRL_EVACT1_PPW, 0);
    ppw_case("D5 pwp", TCC_EVCTRL_EVACT1_PWP, 0);
    ppw_case("D6 ppw_inv", TCC_EVCTRL_EVACT1_PPW, TCC_EVCTRL_TCINV1_Msk);
    ds_capture();
    circular("D8 circ_cc0", CICCEN0, 49, 0, 30);
    circular("D9 circ_per", CIPEREN, 49, 29, 0);

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 0;
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    scope_release(GA, W0);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
