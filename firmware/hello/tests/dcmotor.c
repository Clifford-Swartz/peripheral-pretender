/*
 * DC Motor Click demo for the twin and the web board view: the Click on
 * EXT1 (through a mikroBUS Xplained Pro adapter). TCC0 makes a 5 kHz PWM
 * on PB10 (function F, TCC0 WO[4] = channel 0, MikroElektronika's
 * frequency); SELECT1 on RST (PA20) and SELECT2 on CS (PA17) pick the
 * bridge and the decay, nSLEEP on AN (PB08) wakes it, nFAULT on INT
 * (PB02) is read. Steps of 2 s, each printed:
 *
 *   forward 50 %  (SEL1 0, SEL2 0: fast decay, drive = duty)
 *   forward 100 %
 *   brake         (SEL1 1, SEL2 1 with the PWM at 100 %)
 *   reverse 50 %  (SEL1 0, SEL2 1)
 *   coast         (fast decay at 0 %)
 *   forward 30 %, slow decay (SEL1 1, SEL2 1, duty 70 %: drive = 1 - duty)
 *   sleep         (nSLEEP low)
 *
 * After the first round "PP:EXIT n", n the steps nFAULT was low in (the
 * check "nfault"); then the same over and over.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "dcmotor.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define T               TCC0_REGS
#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define PWM_PIN         10U         /* PB10, EXT1 pin 7 */
#define SLP_PIN         8U          /* PB08, AN */
#define FLT_PIN         2U          /* PB02, INT */
#define SEL1_PIN        20U         /* PA20, RST */
#define SEL2_PIN        17U         /* PA17, CS */
#define PERIOD          (CPU_CLOCK_HZ / 5000U)

static unsigned int faults;

static void pin(volatile port_group_registers_t *g, unsigned int p, int on)
{
    if (on) {
        g->PORT_OUTSET = 1U << p;
    } else {
        g->PORT_OUTCLR = 1U << p;
    }
}

/* One step: selects, duty in percent, nSLEEP; 2 s, nFAULT watched */
static void step(const char *name, int sel1, int sel2, uint32_t duty,
                 int awake)
{
    int fault = 0;

    console_puts(name);
    console_puts("\r\n");
    pin(PA, SEL1_PIN, sel1);
    pin(PA, SEL2_PIN, sel2);
    pin(PB, SLP_PIN, awake);
    T->TCC_CCBUF[0] = PERIOD * duty / 100U;
    for (unsigned int ms = 0; ms < 2000U; ms += 10U) {
        delay_ms(10);
        fault |= (PB->PORT_IN & (1U << FLT_PIN)) == 0U;
    }
    if (fault) {
        console_puts("nFAULT low\r\n");
        faults++;
    }
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dcmotor: " BOARD_BANNER "\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T->TCC_PER = PERIOD - 1U;
    T->TCC_CC[0] = 0;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }

    /* PB10 to TCC0 WO[4] (function F); selects and nSLEEP as outputs */
    PB->PORT_PMUX[PWM_PIN / 2U] = (PB->PORT_PMUX[PWM_PIN / 2U] & 0xF0U) |
                                  0x5U;
    PB->PORT_PINCFG[PWM_PIN] = PORT_PINCFG_PMUXEN_Msk;
    PA->PORT_OUTCLR = (1U << SEL1_PIN) | (1U << SEL2_PIN);
    PA->PORT_DIRSET = (1U << SEL1_PIN) | (1U << SEL2_PIN);
    PB->PORT_OUTSET = 1U << SLP_PIN;
    PB->PORT_DIRSET = 1U << SLP_PIN;
    PB->PORT_PINCFG[FLT_PIN] = PORT_PINCFG_INEN_Msk;

    for (int round = 0;; round++) {
        step("forward 50 %", 0, 0, 50, 1);
        step("forward 100 %", 0, 0, 100, 1);
        step("brake", 1, 1, 100, 1);
        step("reverse 50 %", 0, 1, 50, 1);
        step("coast", 0, 0, 0, 1);
        step("forward 30 %, slow decay", 1, 1, 70, 1);
        step("sleep", 0, 0, 0, 0);
        if (round == 0) {
            console_puts("nfault ");
            console_puts(faults ? "FAIL\r\n" : "ok\r\n");
            console_puts(faults ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
        }
    }
}
