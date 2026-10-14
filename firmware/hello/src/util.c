/*
 * Helpers shared by the hello image and the peripheral tests.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "util.h"

#include "console.h"
#include "sys_init.h"

unsigned int failures;

void systick_start(void)
{
    SysTick->LOAD = CPU_CLOCK_HZ / 1000U - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

bool systick_ms_elapsed(void)
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U;
}

void delay_ms(uint32_t ms)
{
    while (ms > 0U) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
}

void delay_us(uint32_t us)
{
    uint32_t ticks = us * (CPU_CLOCK_HZ / 1000000U);
    uint32_t load = SysTick->LOAD + 1U;
    uint32_t last = SysTick->VAL;
    uint32_t elapsed = 0;

    while (elapsed < ticks) {
        uint32_t now = SysTick->VAL;

        elapsed += (last - now + load) % load;     /* VAL counts down */
        last = now;
    }
}

void put_field(const char *name, uint32_t value, unsigned int digits)
{
    console_putc(' ');
    console_puts(name);
    console_puts("=0x");
    console_put_hex(value, digits);
}

void put_checked(const char *name, uint32_t value, uint32_t expected,
                 unsigned int digits)
{
    put_field(name, value, digits);
    if (value != expected) {
        console_puts("(expected 0x");
        console_put_hex(expected, digits);
        console_putc(')');
        failures++;
    }
}

void test_exit(void)
{
    console_puts("PP:EXIT ");
    console_put_dec(failures);
    console_puts("\r\n");
    console_flush();
    for (;;) {
        __WFI();
    }
}
