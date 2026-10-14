/*
 * Accel Click demo for the web board view: an ADXL345 accelerometer on
 * EXT1 (through a mikroBUS Xplained Pro adapter) at I2C 0x53 on SERCOM2,
 * DATA_READY on INT1 to the header's INT pin (PB02). Full resolution,
 * +-2 g, 25 Hz. On each new sample, prints x, y and z in mg and which way
 * the Click leans whenever that changes; LED0 lights while it leans more
 * than about 30 degrees. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=accel
 *
 * The axes follow the Click's silkscreen: x to the right, y up, z out of
 * the board. At rest the sensor reads the opposite of gravity: flat reads
 * z = +1 g, and the side that dips reads negative.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_i2c.h"

#define ADXL345         0x53U
#define INT_GROUP       (&PORT_REGS->GROUP[1])
#define INT_PIN         2U              /* PB02, EXT1 pin 9 */
#define LEAN_MG         500             /* sin(30 degrees) */

static int abs_i(int v)
{
    return v < 0 ? -v : v;
}

static void put_mg(const char *name, int v)
{
    console_puts(name);
    console_putc(v < 0 ? '-' : '+');
    console_put_dec((uint32_t)abs_i(v));
}

static const char *lean(int x, int y)
{
    if (abs_i(x) < LEAN_MG && abs_i(y) < LEAN_MG) {
        return "flat";
    }
    if (abs_i(x) >= abs_i(y)) {
        return x < 0 ? "right side down" : "left side down";
    }
    return y < 0 ? "top down" : "bottom down";
}

int main(void)
{
    uint8_t id = 0;
    const char *last = "";

    sys_initialize();
    systick_start();
    console_puts("PP accel: " BOARD_BANNER "\r\n");

    LED0_GROUP->PORT_OUTSET = LED0_MASK;            /* off (active low) */
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    INT_GROUP->PORT_DIRCLR = 1U << INT_PIN;         /* push-pull INT1 */
    INT_GROUP->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();

    while (i2c_read(ADXL345, 0x00, &id, 1) != 0 || id != 0xE5U) {
        console_puts("no Accel Click on EXT1 (ADXL345 at 0x53)\r\n");
        delay_ms(2000);
    }
    console_puts("Accel Click found (DEVID 0xE5)\r\n");
    i2c_write1(ADXL345, 0x31, 0x08);    /* DATA_FORMAT: full res, +-2 g */
    i2c_write1(ADXL345, 0x2C, 0x08);    /* BW_RATE: 25 Hz */
    i2c_write1(ADXL345, 0x2F, 0x00);    /* INT_MAP: all on INT1 */
    i2c_write1(ADXL345, 0x2E, 0x80);    /* INT_ENABLE: DATA_READY */
    i2c_write1(ADXL345, 0x2D, 0x08);    /* POWER_CTL: measure */

    for (;;) {
        uint8_t d[6];
        int x, y, z;
        const char *now;

        while (!(INT_GROUP->PORT_IN & (1U << INT_PIN))) {
        }
        if (i2c_read(ADXL345, 0x32, d, 6) != 0) {
            continue;
        }
        /* 256 LSB/g at full resolution: mg = code * 1000 / 256 */
        x = (int16_t)(d[0] | d[1] << 8) * 1000 / 256;
        y = (int16_t)(d[2] | d[3] << 8) * 1000 / 256;
        z = (int16_t)(d[4] | d[5] << 8) * 1000 / 256;
        now = lean(x, y);
        if (now != last) {
            put_mg("x=", x);
            put_mg(" y=", y);
            put_mg(" z=", z);
            console_puts(" mg ");
            console_puts(now);
            console_puts("\r\n");
            last = now;
        }
        if (now[0] == 'f') {
            LED0_GROUP->PORT_OUTSET = LED0_MASK;
        } else {
            LED0_GROUP->PORT_OUTCLR = LED0_MASK;
        }
    }
}
