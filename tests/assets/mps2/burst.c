/*
 * Runner test image: a long deterministic transcript for integrity checks.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * Prints BURST_LINES lines "burst <i> <62-char pattern rotated by i>" as
 * fast as the UART accepts them, so lost, duplicated or reordered bytes in
 * the runner's transport show up as a transcript mismatch.
 */
#include "cmsdk_uart.h"

#define BURST_LINES 2000

static const char pattern[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int main(void)
{
    const unsigned n = sizeof(pattern) - 1;

    uart_init();
    for (unsigned i = 0; i < BURST_LINES; i++) {
        uart_puts("burst ");
        uart_put_dec(i);
        uart_putc(' ');
        for (unsigned j = 0; j < n; j++) {
            uart_putc(pattern[(i + j) % n]);
        }
        uart_puts("\r\n");
    }
    pp_exit(0);
    idle_forever();
    return 0;
}
