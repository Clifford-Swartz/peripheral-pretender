/*
 * TCC probe 11 (PIC32CM MC00, TCC U2213): which channel PPW and PWP put
 * the period and the pulse width in, and which edge restarts the counter,
 * where tccprobe10 read PPW the other way round from DS 34.6.2.7. Prints
 * raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe11
 *
 * Wiring and clocks as tccprobe10 (input PA02 through EIC EXTINT2 and
 * asynchronous EVSYS channel 0 to TCC0 EV_1; GCLK_TCC0 1 MHz, DIV1, PER
 * 0xFFFFFF). The input is high 30-45 us and 90-105 us after the enable
 * (width 15, period 60; the pull takes about 2 us). COUNT is read at
 * 100 us (READSYNC); then CC0, CC1, CC0, CC1 (both FIFO levels), with
 * STATUS and INTFLAG first.
 *
 *   E1  PPW                 E2  PWP
 *   E3  PPW with TCINV1     E4  PWP with TCINV1
 *   E5  PPW with CPTEN0 only
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_A          0U
#define PIN_IN          2U                      /* PA02, EXTINT2 */
#define GEN_EIC2        16U
#define USER_EV1        10U

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

static void ppw_case(const char *label, uint32_t evact, uint32_t inv,
                     uint32_t cpten)
{
    static const uint16_t at[4] = { 30, 45, 90, 105 };
    uint32_t t0, c100, cc[4];

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, PIN_IN, 0);
    T0->TCC_CTRLA = cpten;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 0xFFFFFF;
    T0->TCC_EVCTRL = evact | TCC_EVCTRL_TCEI1_Msk | inv;
    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int i = 0; i < 4; i++) {
        scope_wait_until(t0, SCOPE_CYC(at[i]));
        scope_pull(GA, PIN_IN, !(i & 1U));
        if (i == 2) {
            scope_wait_until(t0, SCOPE_CYC(100));
            c100 = count();
        }
    }
    scope_wait_until(t0, SCOPE_CYC(120));
    console_puts(label);
    put_field("count100", c100, 8);
    put_field("status", T0->TCC_STATUS, 8);
    put_field("intflag", T0->TCC_INTFLAG, 8);
    for (unsigned int i = 0; i < 4; i++) {
        cc[i] = T0->TCC_CC[i & 1U];
    }
    put_field("cc0", cc[0], 8);
    put_field("cc1", cc[1], 8);
    put_field("cc0_2", cc[2], 8);
    put_field("cc1_2", cc[3], 8);
    put_field("intflag_after", T0->TCC_INTFLAG, 8);
    console_puts("\r\n");
}

int main(void)
{
    static const uint32_t both = TCC_CTRLA_CPTEN0_Msk | TCC_CTRLA_CPTEN1_Msk;

    sys_initialize();
    console_puts("PP tccprobe11: " BOARD_BANNER "\r\n");
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
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 1U;
    scope_start();

    ppw_case("E1 ppw", TCC_EVCTRL_EVACT1_PPW, 0, both);
    ppw_case("E2 pwp", TCC_EVCTRL_EVACT1_PWP, 0, both);
    ppw_case("E3 ppw_inv", TCC_EVCTRL_EVACT1_PPW, TCC_EVCTRL_TCINV1_Msk,
             both);
    ppw_case("E4 pwp_inv", TCC_EVCTRL_EVACT1_PWP, TCC_EVCTRL_TCINV1_Msk,
             both);
    ppw_case("E5 ppw_cpten0", TCC_EVCTRL_EVACT1_PPW, 0,
             TCC_CTRLA_CPTEN0_Msk);

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
