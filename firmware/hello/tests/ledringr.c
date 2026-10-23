/*
 * LED Ring R Click demo for the web board view: four 74HC595 on EXT1
 * (through a mikroBUS Xplained Pro adapter) on SERCOM1 SPI, LATCH on CS
 * (PA17), MR# on RST (PA20, held high). Runs a three-LED comet around the
 * 32 red LEDs, a step every 40 ms, and prints each turn. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=ledringr
 *
 * A frame is 32 bits, bit n lighting LED n+1 (LED1 at three o'clock,
 * counter-clockwise), sent low byte first as MikroElektronika's driver
 * does: the first byte ends in the last register of the chain.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_spi.h"

static void ring(uint32_t leds)
{
    cs_low();
    for (unsigned int i = 0; i < 4U; i++) {
        spi_xfer((uint8_t)(leds >> (8U * i)));
    }
    cs_high();                                            /* STCP rising */
}

int main(void)
{
    uint32_t turn = 0;

    sys_initialize();
    systick_start();
    console_puts("PP ledringr: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = 1U << RST_PIN;
    PA->PORT_DIRSET = 1U << RST_PIN;
    spi_init();

    for (unsigned int head = 0;; head = (head + 1U) % 32U) {
        uint32_t leds = 0;

        for (unsigned int k = 0; k < 3U; k++) {   /* the head, two behind */
            leds |= 1U << ((head + 32U - k) % 32U);
        }
        ring(leds);
        if (head == 31U) {
            console_puts("turn ");
            console_put_dec(++turn);
            console_puts("\r\n");
        }
        delay_ms(40);
    }
}
