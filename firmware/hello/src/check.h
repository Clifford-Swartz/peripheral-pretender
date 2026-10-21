/*
 * Helpers for probe and checked test applications (firmware/hello/tests):
 * checked fields that count failures, a line verdict, a microsecond clock
 * from SysTick that counts its wraps, short settling delays.
 *
 * Conventions (playbooks/model-peripheral.md):
 *   - a checked line ends with end_line(): " ok", or " FAIL" when a check
 *     on it failed; test_exit() prints PP:EXIT <failures>
 *   - read memory a DMA writes through volatile; the compiler moves plain
 *     loads across device register accesses
 *   - delay_ms(n) can last n - 1 ms and a little (a SysTick wrap may
 *     already be pending): space timed events with margin, or use
 *     clock_zero()/now_us() and never print while timing (a print blocks
 *     longer than a SysTick period, and wraps are lost)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef CHECK_H
#define CHECK_H

#include "console.h"
#include "sys_init.h"
#include "util.h"

static uint32_t check_wraps;

/* " NAME=0x<value>", counting a failure outside @lo..@hi */
static inline void put_range(const char *name, uint32_t value, uint32_t lo,
                             uint32_t hi, unsigned int digits)
{
    put_field(name, value, digits);
    if (value < lo || value > hi) {
        console_puts("(expected 0x");
        console_put_hex(lo, digits);
        console_puts("..0x");
        console_put_hex(hi, digits);
        console_putc(')');
        failures++;
    }
}

static inline void put_signed(int32_t v)
{
    console_puts(v < 0 ? "-" : "");
    console_put_dec((uint32_t)(v < 0 ? -v : v));
}

/* " NAME=<v>" in decimal, counting a failure unless within @tol of @expect */
static inline void put_near(const char *name, int32_t value, int32_t expect,
                            int32_t tol)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    put_signed(value);
    if (value < expect - tol || value > expect + tol) {
        console_puts("(expected ");
        put_signed(expect);
        console_puts("+-");
        console_put_dec((uint32_t)tol);
        console_putc(')');
        failures++;
    }
}

/* " ok", or " FAIL" when a check since the last end_line() failed */
static inline void end_line(void)
{
    static unsigned int seen;

    console_puts(failures == seen ? " ok\r\n" : " FAIL\r\n");
    seen = failures;
}

/* Microseconds from SysTick (needs systick_start()), counting wraps */
static inline void clock_zero(void)
{
    SysTick->VAL = 0U;
    (void)SysTick->CTRL;
    check_wraps = 0;
}

static inline uint32_t now_us(void)
{
    uint32_t v = SysTick->VAL;

    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        check_wraps++;
        v = SysTick->VAL;
    }
    return check_wraps * 1000U +
           (SysTick->LOAD + 1U - v) / (CPU_CLOCK_HZ / 1000000U);
}

/* Spin until @us microseconds after clock_zero() */
static inline void wait_until_us(uint32_t us)
{
    while (now_us() < us) {
    }
}

/*
 * KEEP_SIZE bytes at the top of RAM that a system reset (watchdog, reset
 * request, reset pin) leaves alone: the stack ends below them and the
 * startup code clears only .bss. A power-on reset leaves them random, so
 * mark them valid with a magic number. For probes that reset the part.
 */
#define KEEP_SIZE       256U

extern char _end[], _ram_end_[];

static inline volatile void *keep_area(void)
{
    char *keep = _ram_end_ + 1 - KEEP_SIZE;

    if (_end > keep) {
        console_puts("keep_area: the image reaches the top of RAM FAIL\r\n");
        test_exit();
    }
    return keep;
}

/* A short pause (some hundred cycles) for a transfer to land */
static inline void settle(void)
{
    for (volatile uint32_t i = 0; i < 2000U; i++) {
    }
}

#endif /* CHECK_H */
