/*
 * Runner test image: a bus fault to an unmapped address, or a lockup.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * Default build: write to an address no device claims. QEMU logs it under
 * -d invalid_mem ("Invalid write at addr ...") and raises a BusFault, which
 * escalates to HardFault; the handler (installed through VTOR) reports it
 * and exits with 1. Build with -DLOCKUP to fault with FAULTMASK set: the
 * fault cannot escalate, the CPU locks up and QEMU aborts with
 * "qemu: fatal: Lockup: ..." and a register dump.
 */
#include "cmsdk_uart.h"

#define SCB_VTOR        (*(volatile uint32_t *)0xE000ED08u)
#define UNMAPPED        (*(volatile uint32_t *)0x30000000u)

#ifndef LOCKUP
static void (*ram_vectors[16])(void) __attribute__((aligned(128)));

static void fault_handler(void)
{
    uart_puts("hardfault\r\n");
    pp_exit(1);
    idle_forever();
}
#endif

int main(void)
{
    uart_init();
#ifdef LOCKUP
    uart_puts("pp runner test: lockup\r\n");
    __asm__ volatile("cpsid f\n\tudf #0" ::: "memory");
#else
    const uint32_t *flash_vectors = (const uint32_t *)0;

    for (int i = 0; i < 16; i++) {
        ram_vectors[i] = (void (*)(void))flash_vectors[i];
    }
    ram_vectors[3] = fault_handler;
    SCB_VTOR = (uint32_t)ram_vectors;
    uart_puts("pp runner test: fault\r\n");
    UNMAPPED = 1;
#endif
    uart_puts("survived\r\n");
    pp_exit(0);
    idle_forever();
    return 0;
}
