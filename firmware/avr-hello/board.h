/*
 * AVR64EA48 Curiosity Nano (EV66E56A) on a Curiosity Nano Base for Click
 * boards (AC164162): what the avr-hello applications use.
 *
 * Board user guide DS50003494A Tables 4-1 to 4-3; Nano Base user guide
 * DS50002839B Figure 3-1 with the Nano's edge pins (schematic sheet 2).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef AVR_HELLO_BOARD_H
#define AVR_HELLO_BOARD_H

/* Reset clock: OSCHF 20 MHz / 6 (DS40002324, Main Clock After Reset) */
#define F_CPU 3333333UL

#include <avr/io.h>

#define BOARD_BANNER    "AVR64EA48 on AVR64EA48 Curiosity Nano"

/* LED0: PB3, yellow, active low. SW0: PB2 to ground, no pull-up. */
#define LED0_PORT       PORTB
#define LED0_bm         PIN3_bm
#define SW0_PORT        PORTB
#define SW0_bm          PIN2_bm
#define SW0_PINCTRL     PORTB.PIN2CTRL

/* mikroBUS socket 1 of the Nano Base: SPI0 (PA4 MOSI, PA5 MISO, PA6 SCK) */
#define MB1_CS_PORT     PORTA
#define MB1_CS_bm       PIN7_bm

/* Virtual COM port: USART1, TX on PC0, 115200 8N1 */
void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_hex2(uint8_t v);
void console_flush(void);

#endif /* AVR_HELLO_BOARD_H */
