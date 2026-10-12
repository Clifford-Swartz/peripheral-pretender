/*
 * EDBG VCOM console on SERCOM4.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <stddef.h>

#include "console.h"
#include "sys_init.h"

/* Rest of the text SERCOM4_Handler still has to send; NULL when idle. */
static const char *volatile irq_text;

void console_putc(char c)
{
    sercom_usart_int_registers_t *usart = VCOM_USART;

    while ((usart->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk) == 0U) {
    }
    usart->SERCOM_DATA = (uint8_t)c;
}

void console_puts(const char *s)
{
    while (*s != '\0') {
        console_putc(*s++);
    }
}

void console_put_hex(uint32_t value, unsigned int digits)
{
    static const char hex[] = "0123456789ABCDEF";

    while (digits > 0U) {
        digits--;
        console_putc(hex[(value >> (4U * digits)) & 0xFU]);
    }
}

void console_put_dec(uint32_t value)
{
    char buf[10];
    unsigned int n = 0;

    do {
        buf[n++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (n > 0U) {
        console_putc(buf[--n]);
    }
}

void console_flush(void)
{
    while ((VCOM_USART->SERCOM_INTFLAG &
            SERCOM_USART_INT_INTFLAG_TXC_Msk) == 0U) {
    }
}

void console_irq_write(const char *s)
{
    irq_text = s;
    VCOM_USART->SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_DRE_Msk;
}

bool console_irq_busy(void)
{
    return irq_text != NULL;
}

const char *console_irq_cancel(void)
{
    const char *rest;

    VCOM_USART->SERCOM_INTENCLR = SERCOM_USART_INT_INTENCLR_DRE_Msk;
    rest = irq_text;
    irq_text = NULL;
    return rest;
}

/*
 * IRQ13.  The line is level-triggered (INTFLAG & INTENSET), so it stays
 * pending while DRE is set and the handler runs again for the next byte.
 * On silicon it can also be entered while DATA is still full; that entry
 * just returns.
 */
void SERCOM4_Handler(void)
{
    sercom_usart_int_registers_t *usart = VCOM_USART;
    const char *p = irq_text;

    if ((usart->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk) == 0U) {
        return;
    }
    if (p != NULL && *p != '\0') {
        usart->SERCOM_DATA = (uint8_t)*p++;
    }
    if (p == NULL || *p == '\0') {
        usart->SERCOM_INTENCLR = SERCOM_USART_INT_INTENCLR_DRE_Msk;
        p = NULL;
    }
    irq_text = p;
}
