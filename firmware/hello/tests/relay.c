/*
 * Relay Click demo for the web board view: Relay Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter). SW0 toggles relay 1 (PWM pin); relay 2
 * (CS pin) toggles every 2 s. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=relay
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "relay.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define REL1_PIN        10U     /* PB10: EXT1 PWM */
#define REL2_PIN        17U     /* PA17: EXT1 CS */

static int sw0_pressed(void)
{
    return (SW0_GROUP->PORT_IN & (1U << SW0_PIN)) == 0U;
}

int main(void)
{
    uint32_t ms = 0;
    int was = 0, rel1 = 0, rel2 = 0;

    sys_initialize();
    systick_start();
    console_puts("PP relay: " BOARD_BANNER "\r\n");
    console_puts("SW0 toggles relay 1; relay 2 toggles every 2 s\r\n");

    PB->PORT_OUTCLR = 1U << REL1_PIN;
    PB->PORT_DIRSET = 1U << REL1_PIN;
    PA->PORT_OUTCLR = 1U << REL2_PIN;
    PA->PORT_DIRSET = 1U << REL2_PIN;
    SW0_GROUP->PORT_DIRCLR = 1U << SW0_PIN;
    SW0_GROUP->PORT_OUTSET = 1U << SW0_PIN;               /* pull-up */
    SW0_GROUP->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;

    for (;;) {
        int now = sw0_pressed();

        if (now && !was) {
            rel1 = !rel1;
            PB->PORT_OUTTGL = 1U << REL1_PIN;
            console_puts(rel1 ? "relay 1 on\r\n" : "relay 1 off\r\n");
        }
        was = now;
        if (++ms >= 2000U) {
            ms = 0;
            rel2 = !rel2;
            PA->PORT_OUTTGL = 1U << REL2_PIN;
            console_puts(rel2 ? "relay 2 on\r\n" : "relay 2 off\r\n");
        }
        delay_ms(1);
    }
}
