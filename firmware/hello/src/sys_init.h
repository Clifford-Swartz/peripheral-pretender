/*
 * Board bring-up interface of the hello image. Each board directory
 * (boards/<BOARD>/) provides board.h and board.c.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef SYS_INIT_H
#define SYS_INIT_H

#include <stdint.h>

#include "board.h"

/* board.c: Harmony's SYS_Initialize for the board, access for access */
void sys_initialize(void);
/* board.c: one line per SYS_Initialize step, read back with checks */
void report_steps(void);

/* main.c: " NAME=0x<value>", and the same counting a failed check */
void put_field(const char *name, uint32_t value, unsigned int digits);
void put_checked(const char *name, uint32_t value, uint32_t expected,
                 unsigned int digits);

#endif /* SYS_INIT_H */
