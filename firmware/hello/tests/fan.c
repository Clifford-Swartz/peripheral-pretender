/*
 * Fan Click check for the twin and the web board view: the Click on EXT1
 * (through a mikroBUS Xplained Pro adapter), its EMC2301 on the headers'
 * I2C (SERCOM2) at 0x2F, a four-wire fan on its terminals. RPM is
 * 7864320 / TACH count, as MikroElektronika's driver computes it (2-pole
 * fan, 5 edges, range x2: the reset settings). Each line names a check:
 *
 *   id         Product ID 0x37, Manufacturer ID 0x5D
 *   stopped    a fan at rest reads the full count (960 rpm, the floor)
 *   direct     Fan Setting 50 % spins it to about 2000 rpm in 3 s
 *   rpm mode   the RPM-based algorithm holds a 3000 rpm target
 *
 * Then "PP:EXIT n", n the failed checks, and MikroElektronika's example:
 * the duty from 0 to 100 % in steps of 10, a reading 2 s after each.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "fan.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define EMC_ADDR        0x2FU

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static uint8_t emc_reg(uint8_t reg)
{
    uint8_t v = 0;

    i2c_read(EMC_ADDR, reg, &v, 1);
    return v;
}

/* The driver's way: 0x3E, then 0x3F, each its own transfer */
static uint32_t fan_rpm(void)
{
    uint32_t count = ((uint32_t)emc_reg(0x3E) << 8 | emc_reg(0x3F)) >> 3;

    return count ? 7864320U / count : 0;
}

static void put_rpm(uint32_t rpm)
{
    console_puts(" Rotation per minute : ");
    console_put_dec(rpm);
    console_puts("rpm\r\n");
}

int main(void)
{
    uint32_t rpm;

    sys_initialize();
    systick_start();
    console_puts("PP fan: " BOARD_BANNER "\r\n");
    i2c_init();

    check("id", emc_reg(0xFD) == 0x37U && emc_reg(0xFE) == 0x5DU);
    rpm = fan_rpm();
    put_rpm(rpm);
    check("stopped", rpm == 960U);

    i2c_write1(EMC_ADDR, 0x30, 0x80);   /* MikroElektronika's start */
    i2c_write1(EMC_ADDR, 0x2D, 0x80);
    delay_ms(3000);
    rpm = fan_rpm();
    put_rpm(rpm);
    check("direct", rpm > 1900U && rpm < 2100U);

    /* 3000 rpm: TACH Target 7864320 / 3000 = 2621, low byte first */
    i2c_write1(EMC_ADDR, 0x3C, (uint8_t)((2621U & 0x1FU) << 3));
    i2c_write1(EMC_ADDR, 0x3D, (uint8_t)(2621U >> 5));
    i2c_write1(EMC_ADDR, 0x32, 0x2BU | 0x80U);
    delay_ms(10000);
    rpm = fan_rpm();
    put_rpm(rpm);
    check("rpm mode", rpm > 2850U && rpm < 3150U);
    i2c_write1(EMC_ADDR, 0x32, 0x2B);   /* back to Direct Setting */

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
        for (uint32_t duty = 0; duty <= 100U; duty += 10U) {
            i2c_write1(EMC_ADDR, 0x30, (uint8_t)(duty * 255U / 100U));
            console_puts(" Duty Ratio : ");
            console_put_dec(duty);
            console_puts("%\r\n");
            delay_ms(2000);
            put_rpm(fan_rpm());
        }
    }
}
