/*
 * Servo Click demo for the twin and the web board view: the Click on EXT1
 * (through a mikroBUS Xplained Pro adapter), its PCA9685 at 0x40 and
 * LTC2497 at 0x14 on the headers' I2C (SERCOM2), OE# on CS (PA17, driven
 * low). Set up as MikroElektronika's servo driver does (MODE1 0x81, 0xF1,
 * PRE_SCALE 202 for 30 Hz, MODE1 0xA1) with its angle law: OFF count =
 * degrees x 330 / 180 + 10, at least 70. Each line names a check:
 *
 *   mode1      MODE1 reads 0x21 (RESTART cleared, EXTCLK left clear)
 *   prescale   PRE_SCALE reads 202
 *   readback   servo 1's OFF count at 90 degrees reads back (175)
 *   current    the LTC2497 reads servo 1's current through its 0.1 Ohm
 *              shunt (150 mA on the twin), CH0 single-ended
 *
 * Then "PP:EXIT n", n the failed checks, and MikroElektronika's example
 * on servo 1 (0 to 180 and back, 10 ms a degree; a line every 30), with
 * servo 2 mirroring it and servos 3 and 4 held at 45 and 135 degrees.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "servo.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PCA_ADDR        0x40U
#define LTC_ADDR        0x14U
#define PA              (&PORT_REGS->GROUP[0])
#define OE_PIN          17U         /* PA17, CS */

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static uint8_t pca_reg(uint8_t reg)
{
    uint8_t v = 0;

    i2c_read(PCA_ADDR, reg, &v, 1);
    return v;
}

/* Servo @n (1-16) to @deg degrees, MikroElektronika's law */
static void set_angle(unsigned int n, uint32_t deg)
{
    uint32_t off = deg * 330U / 180U + 10U;
    uint8_t d[4];

    if (off < 70U) {
        off = 70U;
    }
    d[0] = 0;
    d[1] = 0;
    d[2] = (uint8_t)(off & 0xFFU);
    d[3] = (uint8_t)(off >> 8);
    i2c_write(PCA_ADDR, (uint8_t)(0x06U + 4U * (n - 1U)), d, 4);
}

/* LTC2497 CH0: select, wait out the conversion, read (mA) */
static uint32_t servo1_ma(void)
{
    uint8_t d[3] = { 0 };
    uint32_t raw, code;

    i2c_write(LTC_ADDR, 0xB0, 0, 0);
    delay_ms(200);
    I2C->SERCOM_ADDR = (uint32_t)LTC_ADDR << 1 | 1U;
    i2c_sync();
    if (!(i2c_wait() & I2C_SB)) {
        i2c_stop();
        return 0;
    }
    for (unsigned int i = 0; i < 3U; i++) {
        if (i == 2U) {
            I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                                SERCOM_I2CM_CTRLB_ACKACT_Msk;
            i2c_sync();
        }
        d[i] = (uint8_t)I2C->SERCOM_DATA;
        if (i != 2U) {
            i2c_wait();
        }
    }
    i2c_stop();
    raw = (uint32_t)d[0] << 16 | (uint32_t)d[1] << 8 | d[2];
    code = (raw >> 6) & 0xFFFFU;
    /* 1.024 V / 65536 = 15.625 uV a count, over 0.1 Ohm */
    return code * 15625U / 100000U;
}

int main(void)
{
    uint32_t ma;

    sys_initialize();
    systick_start();
    console_puts("PP servo: " BOARD_BANNER "\r\n");
    i2c_init();
    PA->PORT_OUTCLR = 1U << OE_PIN;     /* OE# low: outputs on */
    PA->PORT_DIRSET = 1U << OE_PIN;

    i2c_write1(PCA_ADDR, 0x00, 0x81);
    i2c_write1(PCA_ADDR, 0x00, 0xF1);   /* "sleep", as the driver writes */
    i2c_write1(PCA_ADDR, 0xFE, 202);    /* 25 MHz / 4096 / 30 - 1 */
    i2c_write1(PCA_ADDR, 0x00, 0xA1);
    check("mode1", pca_reg(0x00) == 0x21U);
    check("prescale", pca_reg(0xFE) == 202U);
    set_angle(1, 90);
    check("readback", pca_reg(0x08) == 175U && pca_reg(0x09) == 0U);
    ma = servo1_ma();
    console_puts("servo 1 current ");
    console_put_dec(ma);
    console_puts(" mA\r\n");
    check("current", ma >= 140U && ma <= 160U);

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    set_angle(3, 45);
    set_angle(4, 135);
    for (;;) {
        for (int dir = 0; dir < 2; dir++) {
            for (uint32_t i = 0; i <= 180U; i++) {
                uint32_t deg = dir ? 180U - i : i;

                set_angle(1, deg);
                set_angle(2, 180U - deg);
                if (deg % 30U == 0U) {
                    console_puts("angle ");
                    console_put_dec(deg);
                    console_puts("\r\n");
                }
                delay_ms(10);
            }
        }
    }
}
