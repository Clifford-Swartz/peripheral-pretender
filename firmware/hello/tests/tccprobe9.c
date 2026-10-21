/*
 * TCC probe 9 (PIC32CM MC00, TCC U2213): what tccprobe8 left open about
 * the faults: STATUS during and after one, a fault the qualification
 * ignores, leaving a non-recoverable fault, fault capture into a capture
 * channel (CTRLA.CPTEN2), BLANK = START, KEEP with QUAL, and how soon an
 * output comes back after a restart. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe9
 *
 * Wiring and clocks as tccprobe8 (TCC0 WO0/WO1 on PA04/PA05, fault pin
 * PA02 through EIC EXTINT2 and asynchronous EVSYS channel 0 to TCC0 MC_0
 * and EV_0; GCLK_TCC0 1 MHz, DIV1, NPWM, PER 99, CC0 50, CC1 30). Each
 * case raises the fault at @on and drops it at @off (us after the enable)
 * and reads STATUS, the pins (bit 0 WO0, bit 1 WO1) and COUNT at the
 * listed times, then INTFLAG.
 *
 *   G1  SRC (MCEI0), fault 20-35: reads at 10, 28, 45
 *   G2  the same with QUAL, fault 60-70 (WO0 low): reads at 55, 65, 75
 *   G3  G2 without QUAL
 *   G4  EV0 FAULT, NRE0/NRV0, NRE1, fault 20-35: reads at 28, 45; STATUS.FAULT0
 *       cleared at 50; reads at 52, 80
 *   G5  FCTRLA.HALT = NR, the same, STATUS.FAULTA cleared at 50
 *   G6  CPTEN2 and CAPTURE = CAPT, CHSEL = CC2, fault 20-35: reads at 28, 45; CC2
 *   G7  BLANK = START, BLANKVAL 30, faults 20-25 and 40-45: reads at 22, 42
 *   G8  KEEP and QUAL, fault 20-25: reads at 30, 60, 110
 *   G9  RESTART, fault 20-35: reads at 34, 36, 37, 38, 40, 45
 * G4 and G5 also read COUNT at 85 us (READSYNC; the time it took shown).
 * The actions run in time order at absolute times from the enable.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_A          0U
#define FUNC_E          4U
#define FLT             2U                      /* PA02, EXTINT2 */
#define GEN_EIC2        16U
#define USER_EV0        9U
#define USER_MC0        11U
#define NRDRV           (TCC_DRVCTRL_NRE0_Msk | TCC_DRVCTRL_NRV0_Msk | \
                         TCC_DRVCTRL_NRE1_Msk)
#define MAXR            8U

typedef struct {
    const char *label;
    uint32_t ctrla, fctrla, evctrl, drvctrl;
    uint16_t on, off, on2, off2;        /* us; 0: none */
    uint16_t clear_at;                  /* us: STATUS write; 0: none */
    uint32_t clear;
    uint8_t nread;
    uint16_t at[MAXR];
    uint16_t count_at;                  /* us: READSYNC; 0: none */
} Case;

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static uint32_t pins(void)
{
    uint32_t in = PORT_REGS->GROUP[GA].PORT_IN;

    return ((in >> 4) & 1U) | (((in >> 5) & 1U) << 1);
}

static void fault(int on)
{
    scope_pull(GA, FLT, on);
}

enum { A_ON, A_OFF, A_CLEAR, A_READ, A_COUNT };

typedef struct {
    uint16_t t;
    uint8_t what;
} Action;

static uint32_t count(void)
{
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_READSYNC;
    sync0();
    for (uint32_t i = 0; (T0->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) &&
         i < 200000U; i++) {
    }
    return T0->TCC_COUNT;
}

static unsigned int add(Action *a, unsigned int n, uint16_t t, uint8_t what)
{
    unsigned int i = n;

    /* Insert in time order; at the same time, the earlier action first */
    while (i > 0 && a[i - 1].t > t) {
        a[i] = a[i - 1];
        i--;
    }
    a[i].t = t;
    a[i].what = what;
    return n + 1;
}

static void run_case(const Case *c)
{
    uint32_t st[MAXR], pin[MAXR], cnt = 0, cnt_at = 0, t0, intflag;
    Action a[MAXR + 6];
    unsigned int n = 0, r = 0;

    n = add(a, n, c->on, A_ON);
    n = add(a, n, c->off, A_OFF);
    if (c->on2) {
        n = add(a, n, c->on2, A_ON);
        n = add(a, n, c->off2, A_OFF);
    }
    if (c->clear_at) {
        n = add(a, n, c->clear_at, A_CLEAR);
    }
    for (unsigned int i = 0; i < c->nread; i++) {
        n = add(a, n, c->at[i], A_READ);
    }
    if (c->count_at) {
        n = add(a, n, c->count_at, A_COUNT);
    }
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    fault(0);
    T0->TCC_CTRLA = c->ctrla;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 50;
    T0->TCC_CC[1] = 30;
    T0->TCC_FCTRLA = c->fctrla;
    T0->TCC_EVCTRL = c->evctrl;
    T0->TCC_DRVCTRL = c->drvctrl;
    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int i = 0; i < n; i++) {
        scope_wait_until(t0, SCOPE_CYC(a[i].t));
        switch (a[i].what) {
        case A_ON:
            fault(1);
            break;
        case A_OFF:
            fault(0);
            break;
        case A_CLEAR:
            T0->TCC_STATUS = c->clear;
            break;
        case A_READ:
            st[r] = T0->TCC_STATUS;
            pin[r] = pins();
            r++;
            break;
        default:
            cnt = count();
            cnt_at = SCOPE_US(scope_since(t0));
            break;
        }
    }
    fault(0);
    intflag = T0->TCC_INTFLAG;
    console_puts(c->label);
    for (unsigned int i = 0; i < r; i++) {
        console_puts(" @");
        console_put_dec(c->at[i]);
        console_putc('=');
        console_put_hex(st[i], 8);
        console_putc('/');
        console_put_hex(pin[i], 1);
    }
    put_field("intflag", intflag, 8);
    put_field("cc2", T0->TCC_CC[2], 2);
    if (c->count_at) {
        console_puts(" count@");
        console_put_dec(cnt_at);
        console_putc('=');
        console_put_dec(cnt);
    }
    console_puts("\r\n");
}

static const Case cases[] = {
    { "G1 src", 0, TCC_FCTRLA_SRC_ENABLE, TCC_EVCTRL_MCEI0_Msk, 0,
      20, 35, 0, 0, 0, 0, 3, { 10, 28, 45 }, 0 },
    { "G2 qual_low", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_QUAL_Msk,
      TCC_EVCTRL_MCEI0_Msk, 0, 60, 70, 0, 0, 0, 0, 3, { 55, 65, 75 }, 0 },
    { "G3 noqual_low", 0, TCC_FCTRLA_SRC_ENABLE, TCC_EVCTRL_MCEI0_Msk, 0,
      60, 70, 0, 0, 0, 0, 3, { 55, 65, 75 }, 0 },
    { "G4 ev0_nonrec", 0, 0, TCC_EVCTRL_EVACT0_FAULT | TCC_EVCTRL_TCEI0_Msk,
      NRDRV, 20, 35, 0, 0, 50, TCC_STATUS_FAULT0_Msk, 4,
      { 28, 45, 52, 80 }, 85 },
    { "G5 halt_nr", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_NR,
      TCC_EVCTRL_MCEI0_Msk, NRDRV, 20, 35, 0, 0, 50, TCC_STATUS_FAULTA_Msk,
      4, { 28, 45, 52, 80 }, 85 },
    { "G6 capt_cpten2", TCC_CTRLA_CPTEN2_Msk, TCC_FCTRLA_SRC_ENABLE |
      TCC_FCTRLA_CAPTURE_CAPT | TCC_FCTRLA_CHSEL_CC2, TCC_EVCTRL_MCEI0_Msk,
      0, 20, 35, 0, 0, 0, 0, 2, { 28, 45 }, 0 },
    { "G7 blank_start30", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_BLANK_START |
      TCC_FCTRLA_BLANKVAL(30), TCC_EVCTRL_MCEI0_Msk, 0, 20, 25, 40, 45, 0, 0,
      2, { 22, 42 }, 0 },
    { "G8 keep_qual", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_KEEP_Msk |
      TCC_FCTRLA_QUAL_Msk, TCC_EVCTRL_MCEI0_Msk, 0, 20, 25, 0, 0, 0, 0, 3,
      { 30, 60, 110 }, 0 },
    { "G9 restart", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_RESTART_Msk,
      TCC_EVCTRL_MCEI0_Msk, 0, 20, 35, 0, 0, 0, 0, 6,
      { 34, 36, 37, 38, 40, 45 }, 0 },
    /* Blanking from the start of the second cycle (it starts at 104) */
    { "G10 blank_start30_2nd", 0, TCC_FCTRLA_SRC_ENABLE |
      TCC_FCTRLA_BLANK_START | TCC_FCTRLA_BLANKVAL(30), TCC_EVCTRL_MCEI0_Msk,
      0, 110, 115, 0, 0, 0, 0, 1, { 112 }, 0 },
    { "G11 blank_start30_late", 0, TCC_FCTRLA_SRC_ENABLE |
      TCC_FCTRLA_BLANK_START | TCC_FCTRLA_BLANKVAL(30), TCC_EVCTRL_MCEI0_Msk,
      0, 140, 145, 0, 0, 0, 0, 1, { 142 }, 0 },
    /* A fault 2 us after WO0 falls, blanked or not: the flag says */
    { "G12 blank_fall10", 0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_BLANK_FALL |
      TCC_FCTRLA_BLANKVAL(10), TCC_EVCTRL_MCEI0_Msk, 0, 56, 59, 0, 0, 0, 0,
      1, { 70 }, 0 },
    { "G13 noblank_fall", 0, TCC_FCTRLA_SRC_ENABLE, TCC_EVCTRL_MCEI0_Msk, 0,
      56, 59, 0, 0, 0, 0, 1, { 70 }, 0 },
};

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe9: " BOARD_BANNER "\r\n");
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
    scope_pmux(GA, FLT, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    fault(0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * FLT);             /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << FLT;
    EIC_REGS->EIC_EVCTRL = 1U << FLT;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_MC0] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EV0] = 1U;
    scope_pmux(GA, 4, FUNC_E, 0);
    scope_pmux(GA, 5, FUNC_E, 0);
    scope_start();
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        run_case(&cases[i]);
    }
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_MC0] = 0;
    EVSYS_REGS->EVSYS_USER[USER_EV0] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, FLT);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << FLT;
    scope_release(GA, 4);
    scope_release(GA, 5);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
