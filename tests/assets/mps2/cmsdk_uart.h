/*
 * Polled CMSDK APB UART0 driver for the mps2-an385 runner test images.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * QEMU connects UART0 (0x40004000) to the first -serial backend. Exit
 * protocol (docs/design/v0-vertical-slice.md section 5.6): the last line a
 * test prints is "PP:EXIT <decimal>"; no semihosting.
 */
#ifndef CMSDK_UART_H
#define CMSDK_UART_H

#include <stdint.h>

#define UART0_BASE      0x40004000u
#define UART0_DATA      (*(volatile uint32_t *)(UART0_BASE + 0x00u))
#define UART0_STATE     (*(volatile uint32_t *)(UART0_BASE + 0x04u))
#define UART0_CTRL      (*(volatile uint32_t *)(UART0_BASE + 0x08u))
#define UART0_BAUDDIV   (*(volatile uint32_t *)(UART0_BASE + 0x10u))

#define UART_STATE_TXFULL   (1u << 0)
#define UART_STATE_RXFULL   (1u << 1)
#define UART_CTRL_TX_EN     (1u << 0)
#define UART_CTRL_RX_EN     (1u << 1)

static inline void uart_init(void)
{
    UART0_BAUDDIV = 16;     /* smallest divider the model accepts */
    UART0_CTRL = UART_CTRL_TX_EN | UART_CTRL_RX_EN;
}

static inline void uart_putc(char c)
{
    while (UART0_STATE & UART_STATE_TXFULL) {
    }
    UART0_DATA = (uint8_t)c;
}

static inline void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

static inline char uart_getc(void)
{
    while (!(UART0_STATE & UART_STATE_RXFULL)) {
    }
    return (char)UART0_DATA;
}

static inline void uart_put_dec(uint32_t v)
{
    char buf[11];
    int i = 0;

    do {
        buf[i++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (i) {
        uart_putc(buf[--i]);
    }
}

static inline void uart_put_hex(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(digits[(v >> shift) & 0xfu]);
    }
}

static inline void pp_exit(uint32_t code)
{
    uart_puts("PP:EXIT ");
    uart_put_dec(code);
    uart_puts("\r\n");
}

static inline void idle_forever(void)
{
    for (;;) {
        __asm__ volatile("wfi");
    }
}

#endif
