/*
 * Curiosity CircuitPython board (EV78P89A, PyKit Explorer) wiring for the
 * hello image.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef BOARD_H
#define BOARD_H

#include "same51j20a.h"

#define BOARD_BANNER \
    "ATSAME51J20A on Curiosity CircuitPython (EV78P89A, PyKit Explorer)"
#define BOARD_DID_EXPECTED  "(expected 0x61810x04)"

/*
 * Schematic 02-01318 rev 3: DEBUG_TX PA22 / DEBUG_RX PA23 (SERCOM3 pads 0
 * and 1, function C), red LED D6 on PB23 (D13, active high), USER button
 * on PA27 to ground with a 10 kOhm pull-up (R20).
 */
#define VCOM_USART          (&SERCOM3_REGS->USART_INT)
#define VCOM_IRQn           SERCOM3_0_IRQn          /* DRE */
#define VCOM_Handler        SERCOM3_0_Handler
#define VCOM_HANDLER_NAME   "SERCOM3_0_Handler"
#define VCOM_IRQ_LABEL      "irq58"
#define LED0_GROUP          (&PORT_REGS->GROUP[1])  /* group 1 = port B */
#define LED0_MASK           PORT_PB23
#define LED0_ACTIVE_HIGH    1                       /* through 1 kOhm to GND */
#define SW0_GROUP           (&PORT_REGS->GROUP[0])
#define SW0_PIN             27U

/* Serial number words and the software calibration area (DS60001507) */
#define BOARD_SERIAL_ADDRS  { 0x008061FCU, 0x00806010U, 0x00806014U, \
                              0x00806018U }
#define BOARD_CALROW_ADDR   SW0_ADDR

/* DPLL0 at 120 MHz drives GCLK0 once sys_initialize() returns. */
#define CPU_CLOCK_HZ        120000000U

#endif /* BOARD_H */
