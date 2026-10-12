/*
 * Runner test image: print a few lines, poke unmodelled hardware, exit.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * The accesses below give the runner's unimplemented-access summary known
 * input on mps2-an385: the AHB GPIO block and the VGA region are QEMU
 * "unimplemented devices" (LOG_UNIMP), and offset 0x20 of the CMSDK UART is
 * not a register (LOG_GUEST_ERROR). Build with -DEXIT_CODE=<n> for a
 * non-zero exit.
 */
#include "cmsdk_uart.h"

#ifndef EXIT_CODE
#define EXIT_CODE 0
#endif

#define GPIO0_DATA      (*(volatile uint32_t *)0x40010000u)
#define GPIO0_DATAOUT   (*(volatile uint32_t *)0x40010004u)
#define VGA_CONSOLE     (*(volatile uint32_t *)0x41000000u)
#define UART0_RESERVED  (*(volatile uint32_t *)(UART0_BASE + 0x20u))

int main(void)
{
    uint32_t gpio = 0;

    uart_init();
    uart_puts("pp runner test: exit\r\n");

    for (int i = 0; i < 3; i++) {
        gpio |= GPIO0_DATA;
    }
    GPIO0_DATAOUT = 1;
    VGA_CONSOLE = 0x41;
    UART0_RESERVED = 0;

    uart_puts("gpio read ");
    uart_put_hex(gpio);
    uart_puts("\r\n");
    pp_exit(EXIT_CODE);
    idle_forever();
    return 0;
}
