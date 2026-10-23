/*
 * ADC Click demo for the web board view: an MCP3204 on EXT1 (through a
 * mikroBUS Xplained Pro adapter) on SERCOM1 SPI, CS# on PA17. Converts
 * the four channels single-ended (VREF = VCC, the REFERENCE jumper's
 * schematic position: 3.3 V) every 100 ms and prints them as volts when
 * one moves by more than two codes. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=adc
 *
 * The data sheet's frame for hosts that send 8-bit segments: start,
 * SGL/DIFF and D2 in the first byte, D1 D0 in the second; B11..B8 come back
 * in the second byte's low nibble, B7..B0 in the third.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_spi.h"

#define VREF_MV         3300U

static uint32_t convert(unsigned int ch)
{
    uint32_t hi, lo;

    cs_low();
    spi_xfer((uint8_t)(0x06U | ch >> 2));                 /* start, single */
    hi = spi_xfer((uint8_t)(ch << 6));
    lo = spi_xfer(0x00);
    cs_high();
    return (hi & 0x0FU) << 8 | lo;
}

int main(void)
{
    uint32_t shown[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

    sys_initialize();
    systick_start();
    console_puts("PP adcclick: " BOARD_BANNER "\r\n");
    spi_init();
    console_puts("set the CH0-CH3 inputs\r\n");

    for (;;) {
        uint32_t code[4];
        bool moved = false;

        for (unsigned int ch = 0; ch < 4U; ch++) {
            code[ch] = convert(ch);
            if (code[ch] > shown[ch] + 2U || code[ch] + 2U < shown[ch]) {
                moved = true;
            }
        }
        if (moved) {
            for (unsigned int ch = 0; ch < 4U; ch++) {
                console_puts(ch ? "  CH" : "CH");
                console_putc((char)('0' + ch));
                console_putc(' ');
                put_fixed((int32_t)((code[ch] * VREF_MV + 2048U) / 4096U), 3);
                console_puts(" V");
                shown[ch] = code[ch];
            }
            console_puts("\r\n");
        }
        delay_ms(100);
    }
}
