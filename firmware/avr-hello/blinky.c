/*
 * Blinky demo for the web board view: LED0 blinks at 2 Hz, at 8 Hz while
 * SW0 is held, and each press is reported on the VCOM port. Runs forever.
 * Time comes from TCB0 in periodic interrupt mode (1 ms at CLK_PER), so
 * the demo also exercises CPUINT and the interrupt vectors.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <avr/interrupt.h>

static volatile uint16_t ticks;

ISR(TCB0_INT_vect)
{
    TCB0.INTFLAGS = TCB_CAPT_bm;
    ticks++;
}

static uint16_t now_ms(void)
{
    uint16_t t;

    cli();
    t = ticks;
    sei();
    return t;
}

int main(void)
{
    uint16_t last = 0, presses = 0;
    uint8_t was = 0;

    console_init();
    LED0_PORT.OUTSET = LED0_bm;                     /* off (active low) */
    LED0_PORT.DIRSET = LED0_bm;
    SW0_PORT.DIRCLR = SW0_bm;
    SW0_PINCTRL = PORT_PULLUPEN_bm;

    TCB0.CCMP = F_CPU / 1000U - 1U;                 /* 1 ms */
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
    sei();

    console_puts("PP blinky: " BOARD_BANNER "\r\n");
    console_puts("LED0 blinks at 2 Hz; hold SW0 for 8 Hz\r\n");
    for (;;) {
        uint8_t pressed = !(SW0_PORT.IN & SW0_bm);
        uint16_t t = now_ms();

        if (pressed && !was) {
            presses++;
            console_puts("SW0 pressed count=0x");
            console_hex2(presses >> 8);
            console_hex2(presses & 0xFF);
            console_puts("\r\n");
        }
        was = pressed;
        if ((uint16_t)(t - last) >= (pressed ? 62U : 250U)) {
            last = t;
            LED0_PORT.OUTTGL = LED0_bm;
        }
    }
}
