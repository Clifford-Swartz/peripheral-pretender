/*
 * Weather Click check for the twin and the web board view: the Click on
 * EXT1 (through a mikroBUS Xplained Pro adapter), its BME280 on the
 * headers' I2C (SERCOM2) at 0x76, set up as MikroElektronika's driver
 * does (humidity x1, temperature x2, pressure x16, normal mode, standby
 * 500 ms, IIR filter 16) and compensated with the data sheet's integer
 * code (section 4.2.3). Each line names a check:
 *
 *   id         the chip ID reads 0x60
 *   calib      the calibration image reads (dig_T1, dig_P1, dig_H1 set)
 *   config     ctrl_hum, ctrl_meas and config read back
 *   measure    the first results (25.00 C, 1013.25 hPa, 50.00 %RH on
 *              the twin by default) are in range
 *
 * Then "PP:EXIT n", n the failed checks, and every 1.5 s a line
 * "T=25.00 C P=1013.25 hPa H=50.00 %RH" (as MikroElektronika's example).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "weather.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define BME_ADDR        0x76U

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static struct {
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
    uint8_t h1, h3;
    int16_t h2, h4, h5;
    int8_t h6;
} cal;

static int32_t t_fine;

static void read_calib(void)
{
    uint8_t a[26], b[7];

    i2c_read(BME_ADDR, 0x88, a, 26);
    i2c_read(BME_ADDR, 0xE1, b, 7);
    cal.t1 = (uint16_t)(a[0] | a[1] << 8);
    cal.t2 = (int16_t)(a[2] | a[3] << 8);
    cal.t3 = (int16_t)(a[4] | a[5] << 8);
    cal.p1 = (uint16_t)(a[6] | a[7] << 8);
    cal.p2 = (int16_t)(a[8] | a[9] << 8);
    cal.p3 = (int16_t)(a[10] | a[11] << 8);
    cal.p4 = (int16_t)(a[12] | a[13] << 8);
    cal.p5 = (int16_t)(a[14] | a[15] << 8);
    cal.p6 = (int16_t)(a[16] | a[17] << 8);
    cal.p7 = (int16_t)(a[18] | a[19] << 8);
    cal.p8 = (int16_t)(a[20] | a[21] << 8);
    cal.p9 = (int16_t)(a[22] | a[23] << 8);
    cal.h1 = a[25];
    cal.h2 = (int16_t)(b[0] | b[1] << 8);
    cal.h3 = b[2];
    cal.h4 = (int16_t)((int8_t)b[3] * 16 | (b[4] & 0x0F));
    cal.h5 = (int16_t)((int8_t)b[5] * 16 | (b[4] >> 4));
    cal.h6 = (int8_t)b[6];
}

/* 0.01 degree Celsius */
static int32_t comp_t(int32_t adc)
{
    int32_t var1 = (((adc >> 3) - ((int32_t)cal.t1 << 1)) * cal.t2) >> 11;
    int32_t d = (adc >> 4) - (int32_t)cal.t1;
    int32_t var2 = (((d * d) >> 12) * cal.t3) >> 14;

    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

/* Pa in Q24.8 */
static uint32_t comp_p(int32_t adc)
{
    int64_t var1 = (int64_t)t_fine - 128000, var2, p;

    var2 = var1 * var1 * cal.p6;
    var2 = var2 + ((var1 * cal.p5) * 131072);
    var2 = var2 + ((int64_t)cal.p4 * 34359738368LL);
    var1 = ((var1 * var1 * cal.p3) >> 8) + ((var1 * cal.p2) * 4096);
    var1 = (((int64_t)1 << 47) + var1) * cal.p1 >> 33;
    if (var1 == 0) {
        return 0;
    }
    p = 1048576 - adc;
    p = ((p * 2147483648LL) - var2) * 3125 / var1;
    var1 = ((int64_t)cal.p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)cal.p8 * p) >> 19;
    return (uint32_t)(((p + var1 + var2) >> 8) + ((int64_t)cal.p7 << 4));
}

/* %RH in Q22.10 */
static uint32_t comp_h(int32_t adc)
{
    int32_t v = t_fine - 76800;

    v = ((((adc << 14) - ((int32_t)cal.h4 << 20) - (cal.h5 * v)) + 16384)
         >> 15) *
        (((((((v * cal.h6) >> 10) * (((v * cal.h3) >> 11) + 32768)) >> 10)
           + 2097152) * cal.h2 + 8192) >> 14);
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * cal.h1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return (uint32_t)v >> 12;
}

static void put_fixed(int32_t v, uint32_t scale)
{
    uint32_t frac;

    if (v < 0) {
        console_putc('-');
        v = -v;
    }
    console_put_dec((uint32_t)v / scale);
    console_putc('.');
    frac = (uint32_t)v % scale;
    if (frac < 10U) {
        console_putc('0');
    }
    console_put_dec(frac);
}

/* One burst read of 0xF7..0xFE; T in 0.01 C, P in 0.01 hPa (= Pa), H in
   0.01 %RH */
static void measure(int32_t *t, int32_t *p, int32_t *h)
{
    uint8_t d[8];

    i2c_read(BME_ADDR, 0xF7, d, 8);
    *t = comp_t((int32_t)(d[3] << 12 | d[4] << 4 | d[5] >> 4));
    *p = (int32_t)(comp_p((int32_t)(d[0] << 12 | d[1] << 4 | d[2] >> 4))
                   / 256U);
    *h = (int32_t)(comp_h((int32_t)(d[6] << 8 | d[7])) * 100U / 1024U);
}

static void report(int32_t t, int32_t p, int32_t h)
{
    console_puts("T=");
    put_fixed(t, 100);
    console_puts(" C P=");
    put_fixed(p, 100);
    console_puts(" hPa H=");
    put_fixed(h, 100);
    console_puts(" %RH\r\n");
}

int main(void)
{
    uint8_t id = 0, back[4] = { 0 };
    int32_t t, p, h;

    sys_initialize();
    systick_start();
    console_puts("PP weather: " BOARD_BANNER "\r\n");
    i2c_init();

    i2c_read(BME_ADDR, 0xD0, &id, 1);
    check("id", id == 0x60U);
    read_calib();
    check("calib", cal.t1 != 0U && cal.p1 != 0U && cal.h1 != 0U);
    i2c_write1(BME_ADDR, 0xF2, 0x01);
    i2c_write1(BME_ADDR, 0xF4, 0x57);
    i2c_write1(BME_ADDR, 0xF5, 0x90);
    i2c_read(BME_ADDR, 0xF2, back, 4);
    check("config", back[0] == 0x01U && back[2] == 0x57U &&
                    back[3] == 0x90U);
    delay_ms(1000);
    measure(&t, &p, &h);
    report(t, p, h);
    check("measure", t > -4000 && t < 8500 && p > 30000 && p < 110000 &&
                     h >= 0 && h <= 10000);

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
        delay_ms(1500);
        measure(&t, &p, &h);
        report(t, p, h);
    }
}
