/*
 * Blinky demo for the web board view: LED0 blinks at 2 Hz, at 8 Hz while
 * SW0 is held, and each press is reported on the VCOM port. Runs forever.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* SW0: PA25 to ground, no external pull-up (board user guide) */
#define SW0_GROUP       (&PORT_REGS->GROUP[0])
#define SW0_PIN         25U

static int sw0_pressed(void)
{
    return (SW0_GROUP->PORT_IN & (1U << SW0_PIN)) == 0U;
}

int main(void)
{
    uint32_t ms = 0, presses = 0;
    int was = 0;

    sys_initialize();
    systick_start();
    console_puts("PP blinky: " BOARD_BANNER "\r\n");
    console_puts("LED0 blinks at 2 Hz; hold SW0 for 8 Hz\r\n");

    LED0_GROUP->PORT_OUTSET = LED0_MASK;            /* off (active low) */
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    SW0_GROUP->PORT_DIRCLR = 1U << SW0_PIN;
    SW0_GROUP->PORT_OUTSET = 1U << SW0_PIN;         /* pull-up */
    SW0_GROUP->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;

    for (;;) {
        int now = sw0_pressed();

        if (now && !was) {
            presses++;
            console_puts("SW0 pressed");
            put_field("count", presses, 4);
            console_puts("\r\n");
        }
        was = now;
        if (++ms >= (now ? 62U : 250U)) {
            ms = 0;
            LED0_GROUP->PORT_OUTTGL = LED0_MASK;
        }
        delay_ms(1);
    }
}
