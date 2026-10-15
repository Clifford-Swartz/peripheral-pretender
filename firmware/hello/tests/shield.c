/*
 * Arduino-header demo: D13 blinks at 2 Hz, at 8 Hz while D2 is pulled to
 * ground (a button from D2 to GND; D2 has the pad's pull-up), and LED0
 * follows D13. Plug an LED into D13 and the GND beside it, a button into
 * D2 and GND, on the board or on the web page. Runs forever.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#ifndef ARDUINO_D13_PIN
#error "this board has no Arduino header pins in board.h"
#endif

static int d2_low(void)
{
    return (ARDUINO_GROUP->PORT_IN & (1U << ARDUINO_D2_PIN)) == 0U;
}

int main(void)
{
    uint32_t ms = 0, presses = 0;
    int was = 0, on = 0;

    sys_initialize();
    systick_start();
    console_puts("PP shield: " BOARD_BANNER "\r\n");
    console_puts("D13 blinks at 2 Hz; pull D2 low for 8 Hz\r\n");

    LED0_OFF();
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    ARDUINO_GROUP->PORT_DIRSET = 1U << ARDUINO_D13_PIN;
    ARDUINO_GROUP->PORT_DIRCLR = 1U << ARDUINO_D2_PIN;
    ARDUINO_GROUP->PORT_OUTSET = 1U << ARDUINO_D2_PIN;      /* pull-up */
    ARDUINO_GROUP->PORT_PINCFG[ARDUINO_D2_PIN] = PORT_PINCFG_INEN_Msk |
                                                 PORT_PINCFG_PULLEN_Msk;

    for (;;) {
        int now = d2_low();

        if (now && !was) {
            presses++;
            console_puts("D2 low");
            put_field("count", presses, 4);
            console_puts("\r\n");
        }
        was = now;
        if (++ms >= (now ? 62U : 250U)) {
            ms = 0;
            on = !on;
            if (on) {
                ARDUINO_GROUP->PORT_OUTSET = 1U << ARDUINO_D13_PIN;
                LED0_ON();
            } else {
                ARDUINO_GROUP->PORT_OUTCLR = 1U << ARDUINO_D13_PIN;
                LED0_OFF();
            }
        }
        delay_ms(1);
    }
}
