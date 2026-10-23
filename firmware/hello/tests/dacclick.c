/*
 * DAC Click demo for the web board view: an MCP4921 on EXT1 (through a
 * mikroBUS Xplained Pro adapter) on SERCOM1 SPI, CS# on PA17 (the Click's
 * 100k pull-up), VREF from VCC (the REFERENCE jumper's schematic position:
 * 3.3 V). Steps VOUT through 0, 1/4, 1/2, 3/4 and full scale, a second
 * each, and prints the code and the voltage it should give. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=dac
 *
 * Each write is 16 bits, as MikroElektronika's driver sends them: 0 (DAC
 * register), BUF 0, GA 1 (1x), SHDN 1 (active), then the 12-bit code;
 * the output follows when CS# rises (LDAC# is tied low on the Click).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_spi.h"

#define VREF_MV         3300U

static void dac_write(uint16_t code)
{
    cs_low();
    spi_xfer((uint8_t)(0x30U | code >> 8));
    spi_xfer((uint8_t)code);
    cs_high();
}

int main(void)
{
    static const uint16_t steps[] = { 0, 1024, 2048, 3072, 4095 };

    sys_initialize();
    systick_start();
    console_puts("PP dacclick: " BOARD_BANNER "\r\n");
    spi_init();

    for (unsigned int i = 0;; i = (i + 1U) % 5U) {
        dac_write(steps[i]);
        console_puts("code ");
        console_put_dec(steps[i]);
        console_puts(" VOUT ");
        put_fixed((int32_t)(steps[i] * VREF_MV / 4096U), 3);
        console_puts(" V\r\n");
        delay_ms(1000);
    }
}
