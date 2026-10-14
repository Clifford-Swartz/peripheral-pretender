/*
 * Breathe demo for the web board view: TCC1 drives LED0 (PA24, function F,
 * TCC1 WO[2]) with a 10 kHz PWM whose duty cycle rises and falls every two
 * seconds; holding SW0 holds the LED at full brightness. Runs forever.
 *
 * WAVE.POL0 inverts the output, so CC0 is the time LED0 (active low) is on.
 * CCBUF0 takes each new duty cycle; the TCC copies it at the next cycle.
 * Its SYNCBUSY bit stays set until then (silicon), so the demo does not
 * wait for it.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define T               TCC1_REGS
#define TOP             4799U           /* 48 MHz / 4800 = 10 kHz */
#define SW0_PIN         25U

int main(void)
{
    uint32_t ms = 0;

    sys_initialize();
    systick_start();
    console_puts("PP breathe: " BOARD_BANNER "\r\n");
    console_puts("LED0 breathes (TCC1 WO[2] PWM); hold SW0 for full\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC1_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC1_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TCC1_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk;
    T->TCC_PER = TOP;
    T->TCC_CC[0] = 0;
    while (T->TCC_SYNCBUSY != 0U) {
    }
    T->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;
    while (T->TCC_SYNCBUSY != 0U) {
    }

    /* PA24 to TCC1 WO[2] (function F); SW0 input with pull-up */
    PORT_REGS->GROUP[0].PORT_PMUX[12] =
        (PORT_REGS->GROUP[0].PORT_PMUX[12] & 0xF0U) | 0x5U;
    PORT_REGS->GROUP[0].PORT_PINCFG[24] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_OUTSET = 1U << SW0_PIN;
    PORT_REGS->GROUP[0].PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                               PORT_PINCFG_PULLEN_Msk;

    for (;;) {
        uint32_t phase = ms % 2000U;
        uint32_t level = phase < 1000U ? phase : 2000U - phase;   /* 0..1000 */
        int pressed = !(PORT_REGS->GROUP[0].PORT_IN & (1U << SW0_PIN));

        /* Perceived brightness: square the linear ramp */
        T->TCC_CCBUF[0] = pressed ? TOP + 1U
                                  : level * level * (TOP + 1U) / 1000000U;
        delay_ms(10);
        ms += 10U;
    }
}
