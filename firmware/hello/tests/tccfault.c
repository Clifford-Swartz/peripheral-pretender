/*
 * TCC fault check (PIC32CM MC00, TCC U2213): the recoverable faults A and
 * B (source, keep, qualification, restart, hardware and software halt,
 * blanking, capture, filter, inverted source) and the non-recoverable
 * ones (EV0 FAULT, HALT = NR) against what silicon did in firmware/hello
 * APP=tccprobe8 and tccprobe9 (docs/research/peripherals/tcc-u2213.md).
 * Run: py -m tools.periph check mc00 tccfault
 *
 * Bare EV15N46A. TCC0 WO0/WO1 on PA04/PA05 (function E), WO4 on PA22
 * (function F), with PULLEN so that released they read 0; the fault pin
 * PA02 (EXTINT2, function A), its pull resistor turned by PORT.OUT, HIGH
 * sense, ASYNCH, through asynchronous EVSYS channel 0 to TCC0 MC_0, MC_1
 * and EV_0. GCLK_TCC0 1 MHz, DIV1, NPWM, PER 99, CC0 50, CC1 30. Times
 * are measured from the fault pin's own recorded edges (its pull takes
 * about 2 us) or from other edges of the same run: silicon starts the
 * outputs about 4 us after the enable, the twin at once. STATUS is not
 * read while a fault changes (silicon shows it 5-8 GCLK_TCC cycles late).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_A          0U
#define FUNC_E          4U
#define FUNC_F          5U
#define FLT             2U                      /* PA02, EXTINT2 */
#define W0              4U
#define W1              5U
#define W4              22U
#define GEN_EIC2        16U
#define USER_EV0        9U
#define USER_MC0        11U
#define USER_MC1        12U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define SET_A           (&PORT_REGS->GROUP[GA].PORT_OUTSET)
#define CLR_A           (&PORT_REGS->GROUP[GA].PORT_OUTCLR)
#define NE              160U
#define MCEI0           TCC_EVCTRL_MCEI0_Msk
#define NRDRV           (TCC_DRVCTRL_NRE0_Msk | TCC_DRVCTRL_NRV0_Msk | \
                         TCC_DRVCTRL_NRE1_Msk)

static uint32_t mask;
static ScopeEdge e[NE];
static unsigned int ne;
static ScopeStim stim[4];
static uint32_t status, intflag;

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static uint32_t count(void)
{
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_READSYNC;
    sync0();
    for (uint32_t i = 0; (T0->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) &&
         i < 200000U; i++) {
    }
    return T0->TCC_COUNT;
}

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 1);
}

/* The fault high from @on to @off us (and @on2 to @off2 unless 0) */
static unsigned int pulses(unsigned int on, unsigned int off,
                           unsigned int on2, unsigned int off2, int high)
{
    unsigned int n = 0;
    unsigned int t[4] = { on, off, on2, off2 };

    for (unsigned int i = 0; i < 4 && t[i]; i++) {
        stim[n].t = SCOPE_CYC(t[i]);
        stim[n].reg = ((i & 1U) == 0U) == (high != 0) ? SET_A : CLR_A;
        stim[n].val = 1U << FLT;
        n++;
    }
    return n;
}

/* SWRST, then NPWM PER 99, CC0 50, CC1 30, the faults; enable, record */
static void run(uint32_t ctrla, uint32_t fctrla, uint32_t fctrlb,
                uint32_t evctrl, uint32_t drvctrl, int start_high,
                unsigned int nstim, uint32_t us)
{
    uint32_t t0;

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, FLT, start_high);
    T0->TCC_CTRLA = ctrla;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 50;
    T0->TCC_CC[1] = 30;
    T0->TCC_FCTRLA = fctrla;
    T0->TCC_FCTRLB = fctrlb;
    T0->TCC_EVCTRL = evctrl;
    T0->TCC_DRVCTRL = drvctrl;
    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record_stim(IN_A, mask, t0, SCOPE_CYC(us), e, NE, stim,
                           nstim);
    status = T0->TCC_STATUS;
    intflag = T0->TCC_INTFLAG;
}

/* The first time (us) after @after that @pin becomes @level, or 0 */
static uint32_t edge(unsigned int pin, int level, uint32_t after)
{
    for (unsigned int i = 1; i < ne; i++) {
        uint32_t t = SCOPE_US(e[i].t);

        if (((e[i].v ^ e[i - 1].v) >> pin) & 1U &&
            (int)((e[i].v >> pin) & 1U) == level && t > after) {
            return t;
        }
    }
    return 0;
}

/* Changes of @pin from @from to @to us */
static unsigned int changes(unsigned int pin, uint32_t from, uint32_t to)
{
    unsigned int n = 0;

    for (unsigned int i = 1; i < ne; i++) {
        uint32_t t = SCOPE_US(e[i].t);

        if (((e[i].v ^ e[i - 1].v) >> pin) & 1U && t >= from && t <= to) {
            n++;
        }
    }
    return n;
}

/* The level of @pin at the end of the recording */
static uint32_t last(unsigned int pin)
{
    return (e[ne - 1].v >> pin) & 1U;
}

/* How far COUNT moves in 30 us (both READSYNCs from the same start) */
static uint32_t moves(void)
{
    uint32_t t = scope_now(), c = count();

    scope_wait_until(t, SCOPE_CYC(30));
    return count() - c;
}

static void check_recoverable(void)
{
    uint32_t fr, ff, r;

    /* FCTRLA.SRC alone does nothing: EVCTRL.MCEI0 is needed */
    run(0, TCC_FCTRLA_SRC_ENABLE, 0, 0, 0, 0, pulses(20, 35, 0, 0, 1), 150);
    fr = edge(FLT, 1, 0);
    ff = edge(FLT, 0, fr);
    console_puts("source");
    put_dec_range("no_mcei_w0_changes", changes(W0, fr, ff + 3), 0, 0);
    put_exact("no_mcei_flag", (intflag & TCC_INTFLAG_FAULTA_Msk) != 0U, 0);
    run(0, TCC_FCTRLA_SRC_ENABLE, 0, MCEI0, 0, 0, pulses(20, 35, 0, 0, 1),
        150);
    fr = edge(FLT, 1, 0);
    ff = edge(FLT, 0, fr);
    /* Channel 0 clamped ahead of the output matrix: WO0 and WO4 */
    put_dec_range("w0_down", edge(W0, 0, fr - 1) - fr, 0, 2);
    put_dec_range("w0_up", edge(W0, 1, ff - 1) - ff, 0, 3);
    put_dec_range("w4_down", edge(W4, 0, fr - 1) - fr, 0, 2);
    r = edge(W1, 1, 0);
    put_dec_range("w1_high", edge(W1, 0, r) - r, 29, 32);
    put_exact("flag", (intflag & TCC_INTFLAG_FAULTA_Msk) != 0U, 1);
    put_exact("status_after", (status & TCC_STATUS_FAULTA_Msk) != 0U, 0);
    end_line();

    /* KEEP: clamped to the start of the next cycle (WO1 rises then) */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_KEEP_Msk, 0, MCEI0, 0, 0,
        pulses(20, 25, 0, 0, 1), 150);
    fr = edge(FLT, 1, 0);
    ff = edge(FLT, 0, fr);
    r = edge(W0, 1, ff);
    console_puts("keep");
    put_dec_range("w0_low_to_cycle", r - edge(W1, 1, ff), 0, 1);
    end_line();

    /* QUAL: a fault while WO0 is low is ignored, without QUAL it is not */
    console_puts("qual");
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_QUAL_Msk, 0, MCEI0, 0, 0,
        pulses(65, 75, 0, 0, 1), 110);
    put_exact("qual_flag", (intflag & TCC_INTFLAG_FAULTA_Msk) != 0U, 0);
    run(0, TCC_FCTRLA_SRC_ENABLE, 0, MCEI0, 0, 0, pulses(65, 75, 0, 0, 1),
        110);
    put_exact("noqual_flag", (intflag & TCC_INTFLAG_FAULTA_Msk) != 0U, 1);
    end_line();

    /* RESTART: a new cycle at the fault; WO1 (CC1 30) falls 30 later */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_RESTART_Msk, 0, MCEI0, 0, 0,
        pulses(20, 35, 0, 0, 1), 150);
    fr = edge(FLT, 1, 0);
    console_puts("restart");
    /* Silicon restarts the outputs a few cycles late, like at the enable */
    put_dec_range("w1_fall", edge(W1, 0, fr) - fr, 30, 36);
    put_exact("trg", (intflag & TCC_INTFLAG_TRG_Msk) != 0U, 1);
    end_line();

    /* HALT = HW: WO1's high time grows by the fault's 15 us */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_HW, 0, MCEI0, 0, 0,
        pulses(20, 35, 0, 0, 1), 150);
    r = edge(W1, 1, 0);
    console_puts("halt_hw");
    put_dec_range("w1_high", edge(W1, 0, r) - r, 43, 49);
    end_line();

    /* HALT = SW: held until software clears STATUS.FAULTA */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_SW, 0, MCEI0, 0, 0,
        pulses(20, 35, 0, 0, 1), 150);
    r = edge(W1, 1, 0);
    console_puts("halt_sw");
    put_dec_range("w1_changes", changes(W1, r + 1, 150), 0, 0);
    put_exact("w0_back", last(W0), 1);
    put_exact("status", (status & TCC_STATUS_FAULTA_Msk) != 0U, 1);
    put_dec_range("held", moves(), 0, 0);
    T0->TCC_STATUS = TCC_STATUS_FAULTA_Msk;
    scope_wait_until(scope_now(), SCOPE_CYC(10));
    put_dec_range("cleared_moves", moves(), 26, 34);
    end_line();
}

static void check_filters(void)
{
    uint32_t fr, fr2, r;

    /* BLANK = RISE, 10 cycles: a fault 2 us after WO0 rises is ignored */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_BLANK_RISE |
        TCC_FCTRLA_BLANKVAL(10), 0, MCEI0, 0, 0, pulses(6, 9, 20, 25, 1),
        150);
    fr = edge(FLT, 1, 0);
    fr2 = edge(FLT, 1, fr + 1);
    console_puts("blank");
    put_dec_range("blanked_w0_changes", changes(W0, fr, fr + 4), 0, 0);
    put_dec_range("later_w0_down", edge(W0, 0, fr2 - 1) - fr2, 0, 2);
    /* BLANK = START, 30 cycles: from the start of each cycle but the first */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_BLANK_START |
        TCC_FCTRLA_BLANKVAL(30), 0, MCEI0, 0, 0, pulses(20, 25, 115, 120, 1),
        150);
    fr = edge(FLT, 1, 0);
    fr2 = edge(FLT, 1, fr + 10);
    put_dec_range("first_cycle_w0_down", edge(W0, 0, fr - 1) - fr, 0, 2);
    put_dec_range("second_cycle_w0_changes", changes(W0, fr2, fr2 + 6), 0,
                  0);
    end_line();

    /* FILTERVAL 8: a 3 us pulse is lost, a 15 us one acts 8 cycles late */
    run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_FILTERVAL(8), 0, MCEI0, 0, 0,
        pulses(20, 23, 30, 45, 1), 150);
    fr = edge(FLT, 1, 0);
    fr2 = edge(FLT, 1, fr + 5);
    console_puts("filter");
    put_dec_range("short_w0_changes", changes(W0, fr, fr + 9), 0, 0);
    put_dec_range("long_w0_down", edge(W0, 0, fr2) - fr2, 7, 11);
    end_line();

    /* SRC = INVERT: the fault is the pin low */
    run(0, TCC_FCTRLA_SRC_INVERT, 0, MCEI0, 0, 1, pulses(20, 35, 0, 0, 0),
        150);
    fr = edge(FLT, 0, 0);
    console_puts("invert");
    put_dec_range("w0_down", edge(W0, 0, fr - 1) - fr, 0, 2);
    /* Fault B clamps channel 1 (WO1), not channel 0 */
    run(0, 0, TCC_FCTRLB_SRC_ENABLE, TCC_EVCTRL_MCEI1_Msk, 0, 0,
        pulses(20, 25, 0, 0, 1), 150);
    fr = edge(FLT, 1, 0);
    r = edge(FLT, 0, fr);
    put_dec_range("b_w1_down", edge(W1, 0, fr - 1) - fr, 0, 2);
    put_dec_range("b_w0_changes", changes(W0, fr, r), 0, 0);
    put_exact("b_flag", (intflag & TCC_INTFLAG_FAULTB_Msk) != 0U, 1);
    end_line();

    /* CAPTURE = CAPT into CC2 (CTRLA.CPTEN2): COUNT at the fault */
    run(TCC_CTRLA_CPTEN2_Msk, TCC_FCTRLA_SRC_ENABLE |
        TCC_FCTRLA_CAPTURE_CAPT | TCC_FCTRLA_CHSEL_CC2, 0, MCEI0, 0, 0,
        pulses(20, 35, 0, 0, 1), 60);
    console_puts("capture");
    put_dec_range("cc2", T0->TCC_CC[2], 16, 24);
    put_exact("mc2", (intflag & TCC_INTFLAG_MC2_Msk) != 0U, 1);
    end_line();
}

static void check_nonrecoverable(void)
{
    static const char *const label[2] = { "nonrec_ev0", "halt_nr" };
    static const uint32_t st_bit[2] = { TCC_STATUS_FAULT0_Msk,
                                        TCC_STATUS_FAULTA_Msk };

    for (unsigned int k = 0; k < 2; k++) {
        uint32_t fr;

        if (k == 0) {
            run(0, 0, 0, TCC_EVCTRL_EVACT0_FAULT | TCC_EVCTRL_TCEI0_Msk,
                NRDRV, 0, pulses(20, 35, 0, 0, 1), 150);
        } else {
            run(0, TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_NR, 0, MCEI0,
                NRDRV, 0, pulses(20, 35, 0, 0, 1), 150);
        }
        fr = edge(FLT, 1, 0);
        console_puts(label[k]);
        /* NRV0 = 1 on WO0, NRV1 = 0 on WO1; held after the input goes */
        put_dec_range("w0_changes", changes(W0, fr, 150), 0, 0);
        put_exact("w0", last(W0), 1);
        put_dec_range("w1_down", edge(W1, 0, fr - 1) - fr, 0, 2);
        put_exact("w1", last(W1), 0);
        put_exact("status", (status & st_bit[k]) != 0U, 1);
        put_dec_range("held", moves(), 0, 0);
        /* Cleared with the input gone: counting goes on */
        T0->TCC_STATUS = st_bit[k];
        scope_wait_until(scope_now(), SCOPE_CYC(10));
        put_dec_range("cleared_moves", moves(), 26, 34);
        put_exact("cleared_status", (T0->TCC_STATUS & st_bit[k]) != 0U, 0);
        end_line();
    }
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccfault: " BOARD_BANNER "\r\n");
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
    scope_pull(GA, FLT, 0);
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
    EVSYS_REGS->EVSYS_USER[USER_MC0] = 1U;                  /* channel 0 */
    EVSYS_REGS->EVSYS_USER[USER_MC1] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EV0] = 1U;
    mask = (1U << FLT) | (1U << W0) | (1U << W1) | (1U << W4);
    /* Released, a TCC pin with PULLEN is pulled down (tccprobe7): each run
     * starts from 0 */
    scope_pmux(GA, W0, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, W1, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, W4, FUNC_F, PORT_PINCFG_PULLEN_Msk);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = (1U << W0) | (1U << W1) | (1U << W4);
    scope_start();

    check_recoverable();
    check_filters();
    check_nonrecoverable();

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_MC0] = 0;
    EVSYS_REGS->EVSYS_USER[USER_MC1] = 0;
    EVSYS_REGS->EVSYS_USER[USER_EV0] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, FLT);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << FLT;
    scope_release(GA, W0);
    scope_release(GA, W1);
    scope_release(GA, W4);
    test_exit();
}
