/*
 * DGI GPIO probe: drives SW0's pad (PB2, the debugger's DBG2 / DGI GPIO0
 * line on a Curiosity Nano) in a fixed pattern, so what the debugger's DGI
 * reports can be compared with the twin's. One 10 ms frame, repeated:
 * high 1 ms, low 2 ms, high 3 ms, low 4 ms. Prints the pattern once.
 *
 * SW0 shorts PB2 to ground: do not press it while this runs.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <util/delay.h>

int main(void)
{
    console_init();
    console_puts("PP dgigpio: " BOARD_BANNER "\r\n");
    console_puts("PB2: 1 ms high, 2 low, 3 high, 4 low\r\n");
    SW0_PORT.OUTCLR = SW0_bm;
    SW0_PORT.DIRSET = SW0_bm;
    for (;;) {
        SW0_PORT.OUTSET = SW0_bm;
        _delay_ms(1);
        SW0_PORT.OUTCLR = SW0_bm;
        _delay_ms(2);
        SW0_PORT.OUTSET = SW0_bm;
        _delay_ms(3);
        SW0_PORT.OUTCLR = SW0_bm;
        _delay_ms(4);
    }
}
