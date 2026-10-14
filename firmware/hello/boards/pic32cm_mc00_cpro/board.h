/*
 * PIC32CM MC00 Curiosity Pro (EV15N46A) wiring for the hello image.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef BOARD_H
#define BOARD_H

#include "pic32cm1216mc00048.h"

#define BOARD_BANNER \
    "PIC32CM1216MC00048 on PIC32CM MC00 Curiosity Pro (EV15N46A)"
#define BOARD_DID_EXPECTED  "(expected 0x11070x06)"

/* EV15N46A wiring (DS70005440A): EDBG VCOM on SERCOM0, LED0 on PA24. */
#define VCOM_USART          (&SERCOM0_REGS->USART_INT)
#define VCOM_IRQn           SERCOM0_IRQn
#define VCOM_Handler        SERCOM0_Handler
#define VCOM_HANDLER_NAME   "SERCOM0_Handler"
#define VCOM_IRQ_LABEL      "irq9"
#define LED0_GROUP          (&PORT_REGS->GROUP[0])  /* group 0 = port A */
#define LED0_MASK           PORT_PA24               /* active low */

/* GCLK0 runs from OSC48M divided by 1 once sys_initialize() returns. */
#define CPU_CLOCK_HZ        48000000U

/* Peripheral tests: a free pin whose function A is EXTINT TEST_EXTINT
 * (PA16, EXT1/EXT2 SPI MISO, unused without an extension board) */
#define TEST_PIN_GROUP      (&PORT_REGS->GROUP[0])
#define TEST_PIN            16U
#define TEST_EXTINT         0U

#endif /* BOARD_H */
