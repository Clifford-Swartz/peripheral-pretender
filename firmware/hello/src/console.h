/*
 * EDBG VCOM console on SERCOM4.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

/* Blocking output: each byte waits for INTFLAG.DRE.  Bytes go out as is. */
void console_putc(char c);
void console_puts(const char *s);
void console_put_hex(uint32_t value, unsigned int digits);  /* no "0x" */
void console_put_dec(uint32_t value);
void console_flush(void);           /* wait until the last stop bit is out */

/*
 * Interrupt-driven output: SERCOM4_Handler sends @s, one byte per DRE
 * interrupt.  Do not use the blocking functions while it is busy.
 * console_irq_cancel() returns the part of @s that was not sent.
 */
void console_irq_write(const char *s);
bool console_irq_busy(void);
const char *console_irq_cancel(void);

#endif /* CONSOLE_H */
