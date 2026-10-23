/*
 * Helpers shared by the hello image and the peripheral tests: SysTick
 * timing, checked report fields and the exit line.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>

/* Failed checks so far; the exit line reports it */
extern unsigned int failures;

/* SysTick from the CPU clock, wrapping every millisecond; no interrupt */
void systick_start(void);
/* True once per wrap: reading CTRL clears COUNTFLAG */
bool systick_ms_elapsed(void);
void delay_ms(uint32_t ms);
/* Busy-wait about @us microseconds (needs systick_start(), us < 1000) */
void delay_us(uint32_t us);

/* " NAME=0x<value>" */
void put_field(const char *name, uint32_t value, unsigned int digits);
/* put_field(), counting a failure when @value is not @expected */
void put_checked(const char *name, uint32_t value, uint32_t expected,
                 unsigned int digits);

/* @v in units of 10^-@frac as a signed decimal: put_fixed(-1250, 2)
 * prints "-12.50" */
void put_fixed(int32_t v, unsigned int frac);

/* "PP:EXIT <failures>", wait for the last stop bit, then WFI forever */
void test_exit(void) __attribute__((noreturn));

#endif /* UTIL_H */
