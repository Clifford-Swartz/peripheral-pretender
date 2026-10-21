/*
 * PDEC probe (PIC32CM MC00, PDEC U2263): QDEC basics
 * (docs/research/peripherals/pdec-u2263.md experiments E0-E10, E15).
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 pdecprobe
 *
 * Bare EV15N46A, GCLK_PDEC on GCLK0 (48 MHz), FILTER 63 (1.3 us) against
 * the slow pull edges. Phase A = PA08, B = PA09 (PDEC function J, pins
 * PINEN0/1) moved by their pull resistors (PORT.OUT). The index (signal 2)
 * comes through the event path: PA02 as EIC EXTINT2 (HIGH, ASYNCH) on an
 * asynchronous EVSYS channel to EVU2 (EVEI2). For simultaneous A/B edges
 * the same event also feeds EVU0 and EVU1. Each snap: COUNT after READSYNC,
 * STATUS, INTFLAG (both then cleared).
 *
 *   Q0  reset values and write masks
 *   Q1  X4 enabled: 4 steps before START, 4 after
 *   Q2  direction: three A-leading cycles (snap per step in the first),
 *       then three reversed
 *   Q3  SWAP, PINVEN0: the A-leading cycle again
 *   Q4  COUNT without READSYNC; COUNT written while running
 *   Q5  PEREN, ANGULAR 0, CC0 = 3 | 2 << 9: 14 steps up, 14 down
 *   Q6  PEREN 0, ANGULAR 7: COUNT 0xFFFE, 3 up, 3 down
 *   Q7  index pulses at A = B = 0 and at A = 1, counting up and down
 *   Q8  STOP, START, RETRIGGER
 *   Q9  QERR: one event into EVU0 and EVU1
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define PD              PDEC_REGS
#define GA              0U
#define FUNC_J          9U
#define FUNC_A          0U
#define PIN_A           8U
#define PIN_B           9U
#define PIN_I           2U                      /* PA02, EXTINT2 */
#define GEN_EIC2        16U
#define USER_EVU(n)     (44U + (n))
#define ENABLE          (1U << 1)
#define SWAP            (1U << 14)
#define PEREN           (1U << 15)
#define PINEN(n)        (1U << (16 + (n)))
#define PINVEN(n)       (1U << (20 + (n)))
#define ANGULAR(n)      ((uint32_t)(n) << 24)
#define CMD_RETRIGGER   (1U << 5)
#define CMD_UPDATE      (2U << 5)
#define CMD_READSYNC    (3U << 5)
#define CMD_START       (4U << 5)
#define CMD_STOP        (5U << 5)
#define EVEI(n)         (1U << (5 + (n)))

static void nl(void)
{
    console_puts("\r\n");
}

static void sync(void)
{
    for (uint32_t i = 0; PD->PDEC_SYNCBUSY && i < 200000U; i++) {
    }
}

static void cmd(uint8_t c)
{
    PD->PDEC_CTRLBSET = c;
    sync();
    for (uint32_t i = 0; (PD->PDEC_CTRLBSET & 0xE0U) && i < 200000U; i++) {
    }
}

/* SWRST, then CTRLA (@ctrla), EVCTRL, FILTER 63, enabled */
static void pdec_setup(uint32_t ctrla, uint16_t evctrl, int start)
{
    PD->PDEC_CTRLA = 1U;
    sync();
    PD->PDEC_CTRLA = ctrla;
    PD->PDEC_EVCTRL = evctrl;
    PD->PDEC_FILTER = 63;
    sync();
    PD->PDEC_CTRLA = ctrla | ENABLE;
    sync();
    if (start) {
        cmd(CMD_START);
    }
}

static void step(unsigned int a, unsigned int b)
{
    scope_pull(GA, PIN_A, a);
    scope_pull(GA, PIN_B, b);
    delay_us(200);
}

static void index_pulse(void)
{
    scope_pull(GA, PIN_I, 1);
    delay_us(200);
    scope_pull(GA, PIN_I, 0);
    delay_us(200);
}

/* " c<count>/s<status>/f<intflag>", then the flags cleared */
static void snap(void)
{
    cmd(CMD_READSYNC);
    console_puts(" c");
    console_put_hex(PD->PDEC_COUNT & 0xFFFFU, 4);
    console_puts("/s");
    console_put_hex(PD->PDEC_STATUS, 4);
    console_puts("/f");
    console_put_hex(PD->PDEC_INTFLAG, 2);
    PD->PDEC_INTFLAG = 0xFF;
    PD->PDEC_STATUS = 0x003F;
    sync();
}

/* A-leading quadrature: 00 -> 10 -> 11 -> 01 -> 00 */
static const uint8_t fwd[4] = { 1, 3, 2, 0 };
static const uint8_t rev[4] = { 2, 3, 1, 0 };

static void cycle(const uint8_t *seq, int snaps)
{
    for (unsigned int k = 0; k < 4; k++) {
        step(seq[k] & 1U, seq[k] >> 1);
        if (snaps) {
            snap();
        }
    }
}

static void case_reset(void)
{
    console_puts("Q0");
    PD->PDEC_CTRLA = 1U;
    sync();
    put_field("ctrla", PD->PDEC_CTRLA, 8);
    put_field("status", PD->PDEC_STATUS, 4);
    PD->PDEC_CTRLA = 0xFFFFFFFCU;
    sync();
    put_field("ctrla_ones", PD->PDEC_CTRLA, 8);
    PD->PDEC_EVCTRL = 0xFFFF;
    put_field("evctrl_ones", PD->PDEC_EVCTRL, 4);
    PD->PDEC_CC[0] = 0xFFFFFFFFU;
    PD->PDEC_PRESC = 0xFF;
    PD->PDEC_FILTER = 0xFF;
    sync();
    put_field("cc0_ones", PD->PDEC_CC[0], 8);
    put_field("presc_ones", PD->PDEC_PRESC, 2);
    put_field("filter_ones", PD->PDEC_FILTER, 2);
    PD->PDEC_CTRLA = 1U;
    sync();
    nl();
}

static void case_start(void)
{
    console_puts("Q1");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1), 0, 0);
    snap();
    cycle(fwd, 0);
    snap();
    cmd(CMD_START);
    snap();
    cycle(fwd, 0);
    snap();
    nl();
}

static void case_direction(void)
{
    console_puts("Q2");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1), 0, 1);
    snap();
    cycle(fwd, 1);
    cycle(fwd, 0);
    cycle(fwd, 0);
    console_puts(" |");
    snap();
    cycle(rev, 1);
    cycle(rev, 0);
    cycle(rev, 0);
    console_puts(" |");
    snap();
    nl();
}

static void case_swap(void)
{
    static const uint32_t cfg[2] = { SWAP, PINVEN(0) };

    console_puts("Q3");
    for (unsigned int i = 0; i < 2; i++) {
        step(0, 0);
        pdec_setup(PINEN(0) | PINEN(1) | cfg[i], 0, 1);
        PD->PDEC_INTFLAG = 0xFF;
        cycle(fwd, 0);
        console_puts(i ? " pinven0" : " swap");
        snap();
    }
    nl();
}

static void case_readsync(void)
{
    console_puts("Q4");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1), 0, 1);
    cycle(fwd, 0);
    step(1, 0);
    put_field("no_readsync", PD->PDEC_COUNT, 8);
    snap();
    PD->PDEC_COUNT = 0x0100;
    sync();
    put_field("written", PD->PDEC_COUNT, 8);
    step(1, 1);
    snap();
    nl();
}

static void case_angular(void)
{
    console_puts("Q5");
    step(0, 0);
    PD->PDEC_CTRLA = 1U;
    sync();
    PD->PDEC_CC[0] = 3U | (2U << 9);
    sync();
    pdec_setup(PINEN(0) | PINEN(1) | PEREN | ANGULAR(0), 0, 1);
    for (unsigned int k = 0; k < 14; k++) {
        step(fwd[k % 4] & 1U, fwd[k % 4] >> 1);
        snap();
    }
    console_puts(" |");
    for (unsigned int k = 0; k < 14; k++) {
        step(rev[k % 4] & 1U, rev[k % 4] >> 1);
        snap();
    }
    nl();
}

static void case_wrap16(void)
{
    console_puts("Q6");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1) | ANGULAR(7), 0, 1);
    PD->PDEC_COUNT = 0xFFFE;
    sync();
    for (unsigned int k = 0; k < 3; k++) {
        step(fwd[k] & 1U, fwd[k] >> 1);
        snap();
    }
    console_puts(" |");
    step(1, 1);                     /* back down: 11, 10, 00 */
    snap();
    step(1, 0);
    snap();
    step(0, 0);
    snap();
    nl();
}

static void case_index(void)
{
    console_puts("Q7");
    step(0, 0);
    PD->PDEC_CTRLA = 1U;
    sync();
    PD->PDEC_CC[0] = 7U | (3U << 9);
    sync();
    pdec_setup(PINEN(0) | PINEN(1) | PEREN, EVEI(2), 1);
    cycle(fwd, 0);
    step(1, 0);
    step(1, 1);
    snap();                         /* angular 6 */
    step(0, 1);
    step(0, 0);
    snap();
    index_pulse();                  /* at A = B = 0 */
    console_puts(" idx00");
    snap();
    step(1, 0);
    index_pulse();                  /* at A = 1 */
    console_puts(" idx10");
    snap();
    step(0, 0);
    step(0, 1);                     /* down */
    step(1, 1);
    step(1, 0);
    step(0, 0);
    snap();
    index_pulse();
    console_puts(" idx_down");
    snap();
    nl();
}

static void case_commands(void)
{
    console_puts("Q8");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1), 0, 1);
    cycle(fwd, 0);
    step(1, 0);
    snap();
    cmd(CMD_STOP);
    console_puts(" stop");
    snap();
    step(1, 1);
    step(0, 1);
    snap();
    cmd(CMD_START);
    console_puts(" start");
    step(0, 0);
    step(1, 0);
    snap();
    cmd(CMD_RETRIGGER);
    console_puts(" retrig");
    snap();
    step(0, 0);
    snap();
    cmd(CMD_RETRIGGER);
    console_puts(" retrig_down");
    snap();
    PD->PDEC_CTRLA &= ~ENABLE;
    sync();
    PD->PDEC_CTRLA |= ENABLE;
    sync();
    console_puts(" reenable");
    snap();
    nl();
}

static void case_qerr(void)
{
    console_puts("Q9");
    scope_pull(GA, PIN_I, 0);
    delay_us(200);
    EVSYS_REGS->EVSYS_USER[USER_EVU(0)] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(1)] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 0U;
    pdec_setup(0, EVEI(0) | EVEI(1), 1);
    snap();
    scope_pull(GA, PIN_I, 1);
    delay_us(200);
    snap();
    scope_pull(GA, PIN_I, 0);
    delay_us(200);
    snap();
    EVSYS_REGS->EVSYS_USER[USER_EVU(0)] = 0U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(1)] = 0U;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 1U;
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP pdecprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_PDEC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN_A, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_B, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_I, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN_I, 0);
    console_puts("Q pads");
    step(1, 1);
    put_field("ab11", (PORT_REGS->GROUP[GA].PORT_IN >> PIN_A) & 3U, 1);
    step(0, 0);
    put_field("ab00", (PORT_REGS->GROUP[GA].PORT_IN >> PIN_A) & 3U, 1);
    nl();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * PIN_I);          /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << PIN_I;
    EIC_REGS->EIC_EVCTRL = 1U << PIN_I;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 1U;

    case_reset();
    case_start();
    case_direction();
    case_swap();
    case_readsync();
    case_angular();
    case_wrap16();
    case_index();
    case_commands();
    case_qerr();

    PD->PDEC_CTRLA = 1U;
    sync();
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    scope_release(GA, PIN_A);
    scope_release(GA, PIN_B);
    scope_release(GA, PIN_I);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = (1U << PIN_A) | (1U << PIN_B) |
                                       (1U << PIN_I);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
