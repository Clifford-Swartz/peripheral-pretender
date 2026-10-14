/*
 * Virtual COM port output (USART1, TX on PC0, 115200 8N1) for avr-hello.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"

/* USART1 normal mode: BAUD = 64 * f / (16 * baud) */
#define BAUD_VALUE(b)   ((uint16_t)((4UL * F_CPU + (b) / 2U) / (b)))

void console_init(void)
{
    PORTC.DIRSET = PIN0_bm;                         /* TX */
    USART1.BAUD = BAUD_VALUE(115200UL);
    USART1.CTRLB = USART_TXEN_bm;
}

void console_putc(char c)
{
    while (!(USART1.STATUS & USART_DREIF_bm)) {
    }
    USART1.TXDATAL = (uint8_t)c;
}

void console_puts(const char *s)
{
    while (*s) {
        console_putc(*s++);
    }
}

void console_hex2(uint8_t v)
{
    static const char digits[] = "0123456789ABCDEF";

    console_putc(digits[v >> 4]);
    console_putc(digits[v & 0x0F]);
}

/* Wait until the last character has left the shift register */
void console_flush(void)
{
    while (!(USART1.STATUS & USART_TXCIF_bm)) {
    }
}
