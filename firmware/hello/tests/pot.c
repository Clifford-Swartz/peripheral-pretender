/*
 * POT Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter) drives AN, header pin 3 = PB08 = ADC0
 * AIN2, with 0-2.048 V (its MCP1501 reference, jumper OUT SEL on REF).
 * Converts AIN2 against VDDANA every 50 ms and prints the voltage when it
 * moves by more than 10 mV; LED0 lights above half travel (1.024 V). Runs
 * forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=pot
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "pot.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define AN_MUXPOS       2U      /* PB08: ADC0 AIN2 */
#define LED0_PIN        24U     /* PA24, active low */
#define HALF_MV         1024U

int main(void)
{
    uint32_t vdd_mv, last = 0xFFFFFFFFU;

    sys_initialize();
    systick_start();
    console_puts("PP pot: " BOARD_BANNER "\r\n");
    PORT_REGS->GROUP[0].PORT_OUTSET = 1U << LED0_PIN;
    PORT_REGS->GROUP[0].PORT_DIRSET = 1U << LED0_PIN;

    adc0_init(1U, 8U);                      /* PB08, function B */
    vdd_mv = adc_vddana_mv();
    console_puts("vddana");
    put_field("mv", vdd_mv, 4);
    console_puts("\r\nturn the POT Click's knob\r\n");
    for (;;) {
        uint16_t code = adc_read(AN_MUXPOS, ADC_REFCTRL_REFSEL_INTVCC2);
        uint32_t mv = (uint32_t)code * vdd_mv / 4095U;

        if (code != 0xFFFFU &&
            (last == 0xFFFFFFFFU || mv + 10U < last || mv > last + 10U)) {
            console_puts("pot ");
            console_put_dec(mv);
            console_puts(" mV\r\n");
            last = mv;
        }
        if (mv > HALF_MV) {
            PORT_REGS->GROUP[0].PORT_OUTCLR = 1U << LED0_PIN;
        } else {
            PORT_REGS->GROUP[0].PORT_OUTSET = 1U << LED0_PIN;
        }
        delay_ms(50);
    }
}
