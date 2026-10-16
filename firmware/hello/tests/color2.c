/*
 * Color 2 Click check for the twin and the web board view: the Click on
 * EXT1 (through a mikroBUS Xplained Pro adapter), its ISL29125 on the
 * headers' I2C (SERCOM2) at 0x44, INT (open drain, the Click's 4.7k
 * pull-up) on PB02. Set up as MikroElektronika's example: G/R/B, 10000
 * lux range, 16 bits, IR compensation 0x3F, INT on conversion done. Each
 * line names a check:
 *
 *   id         the device ID reads 0x7D, STATUS has the brownout flag
 *   config     CONFIG1..3 read back what was written
 *   int        INT goes low when a G/R/B cycle is done (within 1 s)
 *   clear      reading STATUS releases INT, CONVENF was set
 *   colour     the twin's default light (pure red) classifies as RED
 *
 * Then "PP:EXIT n", n the failed checks, and from there on a line
 * "R=.. G=.. B=.. hue=.. <COLOUR>" whenever the colour name changes,
 * classified as MikroElektronika's driver does (hue from the counts'
 * ratios; WHITE at 97 % of full scale or more, BLACK at 3 % or less; its
 * check for darkness looks only at red).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "color2.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define ISL_ADDR        0x44U
#define INT_GROUP       (&PORT_REGS->GROUP[1])
#define INT_PIN         2U              /* PB02, EXT1 pin 9 */

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static int int_low(void)
{
    return (INT_GROUP->PORT_IN & (1U << INT_PIN)) == 0U;
}

static uint8_t isl_reg(uint8_t reg)
{
    uint8_t v = 0;

    i2c_read(ISL_ADDR, reg, &v, 1);
    return v;
}

/* Wait (up to @ms) for INT low */
static int wait_int(unsigned int ms)
{
    while (!int_low() && ms > 0U) {
        delay_us(1000);
        ms--;
    }
    return int_low();
}

static void read_rgb(uint32_t *r, uint32_t *g, uint32_t *b)
{
    uint8_t d[6] = { 0 };

    i2c_read(ISL_ADDR, 0x09, d, 6);
    *g = d[0] | (uint32_t)d[1] << 8;
    *r = d[2] | (uint32_t)d[3] << 8;
    *b = d[4] | (uint32_t)d[5] << 8;
}

/* MikroElektronika's color2_rgb_to_hsl() and color2_get_color() */
static const char *classify(uint32_t r, uint32_t g, uint32_t b, int *hue)
{
    uint32_t max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint32_t min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int32_t chroma = (int32_t)(max - min), h = 0;

    *hue = 0;
    if (r == 0U) {
        return "BLACK";                 /* the driver tests only red */
    }
    if (chroma != 0) {
        if (max == r) {
            h = 60 * ((int32_t)g - (int32_t)b) / chroma;
        } else if (max == g) {
            h = 60 * ((int32_t)b - (int32_t)r) / chroma + 120;
        } else {
            h = 60 * ((int32_t)r - (int32_t)g) / chroma + 240;
        }
        if (h < 0) {
            h += 360;
        }
    }
    *hue = h;
    if (max * 100U >= 97U * 65535U) {
        return "WHITE";
    }
    if (max * 100U <= 3U * 65535U) {
        return "BLACK";
    }
    if (h >= 330 || h < 30) {
        return "RED";
    }
    if (h >= 270) {
        return "MAGENTA";
    }
    if (h >= 210) {
        return "BLUE";
    }
    if (h >= 150) {
        return "CYAN";
    }
    if (h >= 90) {
        return "GREEN";
    }
    return "YELLOW";
}

static const char *report(void)
{
    uint32_t r, g, b;
    int hue;
    const char *name;

    isl_reg(0x08);                      /* releases INT */
    read_rgb(&r, &g, &b);
    name = classify(r, g, b, &hue);
    console_puts("R=");
    console_put_dec(r);
    console_puts(" G=");
    console_put_dec(g);
    console_puts(" B=");
    console_put_dec(b);
    console_puts(" hue=");
    console_put_dec((uint32_t)hue);
    console_puts(" ");
    console_puts(name);
    console_puts("\r\n");
    return name;
}

int main(void)
{
    static const uint8_t cfg[3] = { 0x0D, 0x3F, 0x10 };
    uint8_t back[3] = { 0 };
    const char *name, *last;
    uint8_t st;

    sys_initialize();
    systick_start();
    console_puts("PP color2: " BOARD_BANNER "\r\n");
    INT_GROUP->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();

    check("id", isl_reg(0x00) == 0x7DU && (isl_reg(0x08) & 0x04U));
    for (unsigned int i = 0; i < 3U; i++) {
        i2c_write1(ISL_ADDR, (uint8_t)(1U + i), cfg[i]);
    }
    i2c_read(ISL_ADDR, 0x01, back, 3);
    check("config", back[0] == cfg[0] && back[1] == cfg[1] &&
                    back[2] == cfg[2]);
    check("int", wait_int(1000));
    st = isl_reg(0x08);
    check("clear", (st & 0x02U) && !int_low());
    wait_int(1000);
    name = report();
    check("colour", name[0] == 'R');

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    last = name;
    for (;;) {
        uint32_t r, g, b;
        int hue;

        if (!wait_int(1000)) {
            continue;
        }
        isl_reg(0x08);
        read_rgb(&r, &g, &b);
        name = classify(r, g, b, &hue);
        if (name != last) {
            /* Read again to print the counts with the name */
            wait_int(1000);
            last = report();
        }
    }
}
