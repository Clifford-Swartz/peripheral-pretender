/*
 * EEPROM 3 Click check for the twin and the web board view: the Click on
 * EXT1 (through a mikroBUS Xplained Pro adapter), its AT24CM02 on the
 * headers' I2C (SERCOM2) at 0x50-0x53 (ADD jumper on 0; the address's low
 * two bits are A17:A16). Each line names a check:
 *
 *   blank      a fresh part reads 0xFF
 *   write      "Mikroe" at 0x00100 (as MikroElektronika's example), then
 *              polling the address until the write cycle ends
 *   read       the bytes back, with a current-address read after setting
 *              the address (the driver's way: address, STOP, read)
 *   wrap       4 bytes at 0x001FE stay in their page: 0x1FE, 0x1FF, 0x100,
 *              0x101
 *   upper      a byte at 0x30000 through device address 0x53 (A17:A16 = 3)
 *              does not land at 0x00000
 *   sequential 0x3FFFF then 0x00000: reads wrap around the memory
 *
 * Ends with "PP:EXIT n", n the failed checks, then idles.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <string.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "eeprom3.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define EE_ADDR         0x50U

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

/* START + device address: 0 if acknowledged (a busy part does not) */
static int ee_start(uint32_t a, int rd)
{
    I2C->SERCOM_ADDR = (EE_ADDR | ((a >> 16) & 3U)) << 1 | (rd ? 1U : 0U);
    i2c_sync();
    if (!(i2c_wait() & (rd ? I2C_SB : I2C_MB)) ||
        (I2C->SERCOM_STATUS & I2C_RXNACK)) {
        i2c_stop();
        return -1;
    }
    return 0;
}

/* Address bytes then @n data bytes; returns the polls until it was done */
static int ee_write(uint32_t a, const uint8_t *d, unsigned int n)
{
    int polls = 0;

    if (ee_start(a, 0)) {
        return -1;
    }
    I2C->SERCOM_DATA = (a >> 8) & 0xFFU;
    i2c_wait();
    I2C->SERCOM_DATA = a & 0xFFU;
    i2c_wait();
    for (unsigned int i = 0; i < n; i++) {
        I2C->SERCOM_DATA = d[i];
        i2c_wait();
    }
    i2c_stop();
    /* Acknowledge polling: no ACK while the write cycle runs */
    while (ee_start(a, 0) != 0 && polls < 1000) {
        polls++;
        delay_us(100);
    }
    i2c_stop();
    return polls;
}

/* The address (STOP), then a current-address read of @n bytes */
static int ee_read(uint32_t a, uint8_t *d, unsigned int n)
{
    if (ee_start(a, 0)) {
        return -1;
    }
    I2C->SERCOM_DATA = (a >> 8) & 0xFFU;
    i2c_wait();
    I2C->SERCOM_DATA = a & 0xFFU;
    i2c_wait();
    i2c_stop();
    if (ee_start(a, 1)) {
        return -1;
    }
    for (unsigned int i = 0; i < n; i++) {
        if (i == n - 1U) {
            I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                                SERCOM_I2CM_CTRLB_ACKACT_Msk;
            i2c_sync();
        }
        d[i] = (uint8_t)I2C->SERCOM_DATA;
        if (i != n - 1U) {
            i2c_wait();
        }
    }
    i2c_stop();
    return 0;
}

int main(void)
{
    static const uint8_t text[] = "Mikroe";
    static const uint8_t four[] = { 0xA1, 0xA2, 0xA3, 0xA4 };
    uint8_t buf[8], b;
    int polls;

    sys_initialize();
    systick_start();
    console_puts("PP eeprom3: " BOARD_BANNER "\r\n");
    i2c_init();

    check("blank", ee_read(0x100, buf, 4) == 0 && buf[0] == 0xFF &&
                   buf[3] == 0xFF);
    polls = ee_write(0x100, text, 6);
    console_puts("write polls=");
    console_put_dec((uint32_t)polls);
    console_puts("\r\n");
    check("write", polls > 0 && polls < 1000);
    memset(buf, 0, sizeof(buf));
    check("read", ee_read(0x100, buf, 6) == 0 && !memcmp(buf, text, 6));
    ee_write(0x1FE, four, 4);
    ee_read(0x1FE, buf, 2);
    b = buf[1];
    ee_read(0x100, buf, 2);
    check("wrap", b == 0xA2 && buf[0] == 0xA3 && buf[1] == 0xA4);
    b = 0x5A;
    ee_write(0x30000, &b, 1);
    ee_read(0x30000, buf, 1);
    b = buf[0];
    ee_read(0x00000, buf, 1);
    check("upper", b == 0x5A && buf[0] == 0xFF);
    b = 0x77;
    ee_write(0x3FFFF, &b, 1);
    ee_read(0x3FFFF, buf, 2);
    check("sequential", buf[0] == 0x77 && buf[1] == 0xFF);

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
    }
}
