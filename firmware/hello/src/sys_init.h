/*
 * Harmony SYS_Initialize replica for the PIC32CM JH01 Curiosity Pro.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef SYS_INIT_H
#define SYS_INIT_H

#include "pic32cm5164jh01100.h"

/* EV81X90A wiring (DS70005482B): EDBG VCOM on SERCOM4, LED0 on PC05. */
#define VCOM_USART      (&SERCOM4_REGS->USART_INT)
#define LED0_GROUP      (&PORT_REGS->GROUP[2])     /* group 2 = port C */
#define LED0_MASK       PORT_PC05                  /* active low */

/* GCLK0 runs from OSC48M divided by 1 once sys_initialize() returns. */
#define CPU_CLOCK_HZ    48000000U

void sys_initialize(void);

#endif /* SYS_INIT_H */
