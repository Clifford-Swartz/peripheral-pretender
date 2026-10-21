/*
 * TCC probe 8 (PIC32CM MC00, TCC U2213): the recoverable faults A and B
 * (FCTRLA/FCTRLB: keep, qualification, restart, hardware and software
 * halt, blanking, capture, filter, inverted source) and the
 * non-recoverable faults (EV0 FAULT action, FCTRLA.HALT = NR), DS 34.6.3.5
 * and 34.6.3.6. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe8
 *
 * Bare EV15N46A. TCC0 WO0/WO1 on PA04/PA05 (function E), WO4 on PA22
 * (function F), watched through PORT.IN with the fault pin: PA02 (EXTINT2,
 * function A), its pull resistor turned by PORT.OUT, HIGH sense, ASYNCH,
 * through asynchronous EVSYS channel 0 to TCC0 MC_0 and MC_1 (faults A
 * and B) and EV_0. GCLK_TCC0 1 MHz, DIV1, NPWM, PER 99, CC0 50, CC1 30:
 * WO0 is high for 50 us and WO1 for 30 us of each 100 us, starting about
 * 4 us after the enable.
 *
 * The fault input is high (pull-up) from 20 to 35 us, 70 to 80 us and
 * 120 to 125 us after the enable unless a line says otherwise. Each line:
 * the level at the start and the times (us) of the changes of the fault
 * pin (f), WO0 (w0), WO1 (w1) and WO4 (w4); STATUS, INTFLAG, CC2, COUNT.
 *
 *   F1  FCTRLA.SRC = ENABLE (no EVCTRL.MCEI0)
 *   F2  the same with EVCTRL.MCEI0
 *   F3  KEEP                 F4  QUAL              F5  RESTART
 *   F6  HALT = HW            F7  HALT = SW, then STATUS.FAULTA cleared
 *   F8  BLANK = RISE, BLANKVAL 10, fault 6-9 us and 20-25 us
 *   F9  CAPTURE = CAPT, CHSEL = CC2
 *   F10 FILTERVAL 8, fault 20-23 us (3 us) and 30-45 us
 *   F11 SRC = INVERT, fault (pin low) 20-35 us
 *   F12 fault B (FCTRLB.SRC = ENABLE), fault 20-25 us
 *   F13 EV0 FAULT (non-recoverable), DRVCTRL NRE0, NRV0, NRE1
 *   F14 FCTRLA.HALT = NR, DRVCTRL as F13
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
#define GEN_EIC2        16U
#define USER_EV0        9U
#define USER_MC0        11U
#define USER_MC1        12U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define SET_A           (&PORT_REGS->GROUP[GA].PORT_OUTSET)
#define CLR_A           (&PORT_REGS->GROUP[GA].PORT_OUTCLR)
#define NE              160U
#define US(x)           SCOPE_CYC(x)

static const uint8_t wo_pin[3] = { 4, 5, 22 };
static uint32_t mask;
static ScopeEdge e[NE];
static unsigned int ne;

/* The standard fault: high 20-35, 70-80, 120-125 us */
static ScopeStim std_stim[6];
static ScopeStim stim[6];

static void nl(void)
{
    console_puts("\r\n");
}

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

static void stim_set(ScopeStim *s, unsigned int i, uint32_t us, int high)
{
    s[i].t = US(us);
    s[i].reg = high ? SET_A : CLR_A;
    s[i].val = 1U << FLT;
}

static void fault_case(const char *label, uint32_t fctrla, uint32_t fctrlb,
                       uint32_t evctrl, uint32_t drvctrl, int start_high,
                       const ScopeStim *st, unsigned int n)
{
    uint32_t t0, status, intflag;

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, FLT, start_high);
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
    ne = scope_record_stim(IN_A, mask, t0, US(260), e, NE, st, n);
    status = T0->TCC_STATUS;
    intflag = T0->TCC_INTFLAG;
    console_puts(label);
    scope_put("f", e, ne, FLT, 8);
    scope_put("w0", e, ne, wo_pin[0], 8);
    scope_put("w1", e, ne, wo_pin[1], 6);
    scope_put("w4", e, ne, wo_pin[2], 6);
    put_field("status", status, 8);
    put_field("intflag", intflag, 8);
    put_field("cc2", T0->TCC_CC[2], 2);
    put_field("count", count(), 2);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe8: " BOARD_BANNER "\r\n");
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

    mask = 1U << FLT;
    for (unsigned int x = 0; x < 3; x++) {
        scope_pmux(GA, wo_pin[x], x < 2 ? FUNC_E : FUNC_F, 0);
        mask |= 1U << wo_pin[x];
    }
    stim_set(std_stim, 0, 20, 1);
    stim_set(std_stim, 1, 35, 0);
    stim_set(std_stim, 2, 70, 1);
    stim_set(std_stim, 3, 80, 0);
    stim_set(std_stim, 4, 120, 1);
    stim_set(std_stim, 5, 125, 0);
    scope_start();

    fault_case("F1 src", TCC_FCTRLA_SRC_ENABLE, 0, 0, 0, 0, std_stim, 6);
    fault_case("F2 src_mcei0", TCC_FCTRLA_SRC_ENABLE, 0, TCC_EVCTRL_MCEI0_Msk,
               0, 0, std_stim, 6);
    fault_case("F3 keep", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_KEEP_Msk, 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    fault_case("F4 qual", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_QUAL_Msk, 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    fault_case("F5 restart", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_RESTART_Msk,
               0, TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    fault_case("F6 halt_hw", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_HW, 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    fault_case("F7 halt_sw", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_SW, 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    {
        uint32_t c1 = count(), c2, st;

        T0->TCC_STATUS = TCC_STATUS_FAULTA_Msk;
        scope_wait_until(scope_now(), US(30));
        c2 = count();
        st = T0->TCC_STATUS;
        console_puts("F7 after_clear");
        put_field("count_before", c1, 2);
        put_field("count_30us", c2, 2);
        put_field("status", st, 8);
        nl();
    }
    stim_set(stim, 0, 6, 1);
    stim_set(stim, 1, 9, 0);
    stim_set(stim, 2, 20, 1);
    stim_set(stim, 3, 25, 0);
    fault_case("F8 blank_rise10", TCC_FCTRLA_SRC_ENABLE |
               TCC_FCTRLA_BLANK_RISE | TCC_FCTRLA_BLANKVAL(10), 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, stim, 4);
    fault_case("F9 capt_cc2", TCC_FCTRLA_SRC_ENABLE |
               TCC_FCTRLA_CAPTURE_CAPT | TCC_FCTRLA_CHSEL_CC2, 0,
               TCC_EVCTRL_MCEI0_Msk, 0, 0, std_stim, 6);
    stim_set(stim, 0, 20, 1);
    stim_set(stim, 1, 23, 0);
    stim_set(stim, 2, 30, 1);
    stim_set(stim, 3, 45, 0);
    fault_case("F10 filter8", TCC_FCTRLA_SRC_ENABLE |
               TCC_FCTRLA_FILTERVAL(8), 0, TCC_EVCTRL_MCEI0_Msk, 0, 0,
               stim, 4);
    stim_set(stim, 0, 20, 0);
    stim_set(stim, 1, 35, 1);
    fault_case("F11 invert", TCC_FCTRLA_SRC_INVERT, 0, TCC_EVCTRL_MCEI0_Msk,
               0, 1, stim, 2);
    stim_set(stim, 0, 20, 1);
    stim_set(stim, 1, 25, 0);
    fault_case("F12 fault_b", 0, TCC_FCTRLB_SRC_ENABLE,
               TCC_EVCTRL_MCEI1_Msk, 0, 0, stim, 2);
    fault_case("F13 ev0_nonrec", 0, 0, TCC_EVCTRL_EVACT0_FAULT |
               TCC_EVCTRL_TCEI0_Msk, TCC_DRVCTRL_NRE0_Msk |
               TCC_DRVCTRL_NRV0_Msk | TCC_DRVCTRL_NRE1_Msk, 0, std_stim, 6);
    fault_case("F14 halt_nr", TCC_FCTRLA_SRC_ENABLE | TCC_FCTRLA_HALT_NR, 0,
               TCC_EVCTRL_MCEI0_Msk, TCC_DRVCTRL_NRE0_Msk |
               TCC_DRVCTRL_NRV0_Msk | TCC_DRVCTRL_NRE1_Msk, 0, std_stim, 6);

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_MC0] = 0;
    EVSYS_REGS->EVSYS_USER[USER_MC1] = 0;
    EVSYS_REGS->EVSYS_USER[USER_EV0] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, FLT);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << FLT;
    for (unsigned int x = 0; x < 3; x++) {
        scope_release(GA, wo_pin[x]);
    }
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
