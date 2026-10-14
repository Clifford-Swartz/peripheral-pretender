/*
 * PIC32CM JH01 Curiosity Pro (EV81X90A) wiring for the hello image.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef BOARD_H
#define BOARD_H

#include "pic32cm5164jh01100.h"

#define BOARD_BANNER \
    "PIC32CM5164JH01100 on PIC32CM JH01 Curiosity Pro (EV81X90A)"
#define BOARD_DID_EXPECTED  "(expected 0x11060x00)"

/* EV81X90A wiring (DS70005482B): EDBG VCOM on SERCOM4, LED0 on PC05. */
#define VCOM_USART          (&SERCOM4_REGS->USART_INT)
#define VCOM_IRQn           SERCOM4_IRQn
#define VCOM_Handler        SERCOM4_Handler
#define VCOM_HANDLER_NAME   "SERCOM4_Handler"
#define VCOM_IRQ_LABEL      "irq13"
#define LED0_GROUP          (&PORT_REGS->GROUP[2])  /* group 2 = port C */
#define LED0_MASK           PORT_PC05               /* active low */

/* GCLK0 runs from OSC48M divided by 1 once sys_initialize() returns. */
#define CPU_CLOCK_HZ        48000000U

/* Peripheral tests: a free pin whose function A is EXTINT TEST_EXTINT
 * (PA16; not checked against the board schematic yet) */
#define TEST_PIN_GROUP      (&PORT_REGS->GROUP[0])
#define TEST_PIN            16U
#define TEST_EXTINT         0U

#endif /* BOARD_H */
