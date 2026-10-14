/*
 * hello for the AVR64EA48 Curiosity Nano (EV66E56A): the AVR counterpart of
 * firmware/hello. Prints a banner and a few identity registers on the
 * virtual COM port (USART1, TX on PC0, 115200 8N1), blinks LED0 (PB3,
 * active low) three times, then prints "PP:EXIT 0".
 *
 * Runs at the reset clock: OSCHF 20 MHz / 6 = 3.333 MHz (DS40002324, Main
 * Clock After Reset), so it needs no protected writes.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#define F_CPU 3333333UL

#include <avr/io.h>
#include <util/delay.h>

/* USART1 normal mode: BAUD = 64 * f / (16 * baud) */
#define BAUD_VALUE(b)   ((uint16_t)((4UL * F_CPU + (b) / 2U) / (b)))

static void uart_init(void)
{
    PORTC.DIRSET = PIN0_bm;                         /* TX */
    USART1.BAUD = BAUD_VALUE(115200UL);
    USART1.CTRLB = USART_TXEN_bm;
}

static void putc_(char c)
{
    while (!(USART1.STATUS & USART_DREIF_bm)) {
    }
    USART1.TXDATAL = (uint8_t)c;
}

static void puts_(const char *s)
{
    while (*s) {
        putc_(*s++);
    }
}

static void hex2(uint8_t v)
{
    static const char digits[] = "0123456789ABCDEF";

    putc_(digits[v >> 4]);
    putc_(digits[v & 0x0F]);
}

static void field(const char *name, uint8_t v)
{
    putc_(' ');
    puts_(name);
    puts_("=0x");
    hex2(v);
}

int main(void)
{
    uart_init();
    PORTB.OUTSET = PIN3_bm;                         /* LED0 off */
    PORTB.DIRSET = PIN3_bm;

    puts_("\r\nPP avr-hello: AVR64EA48 on AVR64EA48 Curiosity Nano\r\n");
    puts_("id:");
    field("sig0", SIGROW.DEVICEID0);
    field("sig1", SIGROW.DEVICEID1);
    field("sig2", SIGROW.DEVICEID2);
    field("rev", SYSCFG.REVID);
    puts_("\r\nclock:");
    field("mclkctrla", CLKCTRL.MCLKCTRLA);
    field("mclkctrlb", CLKCTRL.MCLKCTRLB);
    puts_("\r\n");

    for (uint8_t i = 0; i < 6; i++) {
        PORTB.OUTTGL = PIN3_bm;
        _delay_ms(150);
    }
    puts_("PP:EXIT 0\r\n");
    while (!(USART1.STATUS & USART_TXCIF_bm)) {
    }
    for (;;) {
    }
}
