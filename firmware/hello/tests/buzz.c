/*
 * Buzz Click demo for the web board view: a piezo buzzer on EXT1 (through
 * a mikroBUS Xplained Pro adapter), driven by TCC0 from the header's PWM
 * pin PB10 (function F, TCC0 WO[4] = channel 0). Plays C5 E5 G5 C6 (each
 * 250 ms with a 50 ms rest) and a second of silence, over and over;
 * holding SW0 holds A4 (440 Hz) instead. Each note is printed on the VCOM
 * port. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=buzz
 *
 * A 50 % duty square wave: PER sets the pitch (48 MHz / (PER + 1)), CC0
 * half of it. Rests set CC0 to 0, which holds the pin low (the buzzer's
 * transistor off). PERBUF/CCBUF take effect at the next cycle.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "buzz.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define T               TCC0_REGS
#define PWM_PIN         10U         /* PB10, EXT1 pin 7 */

static const uint16_t tune[] = { 523, 659, 784, 1047 };

static void play(uint32_t hz)
{
    if (hz == 0U) {
        T->TCC_CCBUF[0] = 0;
        return;
    }
    uint32_t top = CPU_CLOCK_HZ / hz - 1U;

    T->TCC_PERBUF = top;
    T->TCC_CCBUF[0] = (top + 1U) / 2U;
    console_puts("note ");
    console_put_dec(hz);
    console_puts(" Hz\r\n");
}

static int sw0_pressed(void)
{
    return (SW0_GROUP->PORT_IN & (1U << SW0_PIN)) == 0U;
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP buzz: " BOARD_BANNER "\r\n");
    console_puts("Buzz Click on EXT1 (TCC0 WO[4] on PB10); hold SW0 for A4\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T->TCC_PER = CPU_CLOCK_HZ / 1000U - 1U;
    T->TCC_CC[0] = 0;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }

    /* PB10 to TCC0 WO[4] (function F); SW0 input with pull-up */
    PORT_REGS->GROUP[1].PORT_PMUX[PWM_PIN / 2U] =
        (PORT_REGS->GROUP[1].PORT_PMUX[PWM_PIN / 2U] & 0xF0U) | 0x5U;
    PORT_REGS->GROUP[1].PORT_PINCFG[PWM_PIN] = PORT_PINCFG_PMUXEN_Msk;
    SW0_GROUP->PORT_DIRCLR = 1U << SW0_PIN;
    SW0_GROUP->PORT_OUTSET = 1U << SW0_PIN;
    SW0_GROUP->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;

    for (;;) {
        if (sw0_pressed()) {
            play(440);
            while (sw0_pressed()) {
                delay_ms(10);
            }
            play(0);
            continue;
        }
        for (unsigned int i = 0; i < sizeof(tune) / sizeof(tune[0]); i++) {
            play(tune[i]);
            delay_ms(250);
            play(0);
            delay_ms(50);
        }
        for (unsigned int ms = 0; ms < 1000U && !sw0_pressed(); ms += 10U) {
            delay_ms(10);
        }
    }
}
