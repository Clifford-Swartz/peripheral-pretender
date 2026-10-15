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

/* LED0 on and off; boards with an active-high LED0 say LED0_ACTIVE_HIGH */
#ifdef LED0_ACTIVE_HIGH
#define LED0_ON()           (LED0_GROUP->PORT_OUTSET = LED0_MASK)
#define LED0_OFF()          (LED0_GROUP->PORT_OUTCLR = LED0_MASK)
#else
#define LED0_ON()           (LED0_GROUP->PORT_OUTCLR = LED0_MASK)
#define LED0_OFF()          (LED0_GROUP->PORT_OUTSET = LED0_MASK)
#endif

/* board.c: Harmony's SYS_Initialize for the board, access for access */
void sys_initialize(void);
/* board.c: one line per SYS_Initialize step, read back with checks */
void report_steps(void);

/* util.h: put_field(), put_checked() for report_steps() */
#include "util.h"

#endif /* SYS_INIT_H */
