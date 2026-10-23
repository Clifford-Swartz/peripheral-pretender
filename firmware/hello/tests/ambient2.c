/*
 * Ambient 2 Click demo for the web board view: an OPT3001 on EXT1 (through
 * a mikroBUS Xplained Pro adapter) at I2C 0x44 on SERCOM2, its INT (open
 * drain, the Click's 4.7k pull-up) on INT (PB02). Checks the IDs, starts
 * continuous 100 ms conversions with the automatic full-scale range and
 * prints the illuminance when it moves by more than 1%. INT, in the
 * transparent hysteresis mode, goes low above 1000 lx and back high below
 * 800 lx; the demo prints each change. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=ambient2
 *
 * Registers are 16 bits, high byte first; a result is E[15:12] R[11:0],
 * 0.01 lx * 2^E * R (OPT3001 data sheet SBOS681C section 7.6), and so are
 * the limits.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_i2c.h"

#define OPT3001         0x44U
#define RESULT          0x00U
#define CONFIG          0x01U
#define LOW_LIMIT       0x02U
#define HIGH_LIMIT      0x03U
#define MANUFACTURER    0x7EU
#define DEVICE_ID       0x7FU

/* Automatic range, 100 ms, continuous, transparent hysteresis, active low */
#define CONFIG_RUN      0xC600U
#define LUX_800         0x59C4U         /* E 5 (0.32 lx), R 2500 */
#define LUX_1000        0x5C35U         /* E 5, R 3125 */

#define INT_GROUP       (&PORT_REGS->GROUP[1])
#define INT_PIN         2U              /* PB02, EXT1 pin 9 */

static uint16_t reg16(uint8_t reg)
{
    uint8_t d[2] = { 0, 0 };

    i2c_read(OPT3001, reg, d, 2);
    return (uint16_t)(d[0] << 8 | d[1]);
}

static void set16(uint8_t reg, uint16_t v)
{
    uint8_t d[2] = { (uint8_t)(v >> 8), (uint8_t)v };

    i2c_write(OPT3001, reg, d, 2);
}

int main(void)
{
    uint32_t shown = 0xFFFFFFFFU;
    unsigned int was = 2;

    sys_initialize();
    systick_start();
    console_puts("PP ambient2: " BOARD_BANNER "\r\n");
    INT_GROUP->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();
    console_puts("manufacturer 0x");
    console_put_hex(reg16(MANUFACTURER), 4);
    console_puts(" device 0x");
    console_put_hex(reg16(DEVICE_ID), 4);
    console_puts("\r\n");
    set16(LOW_LIMIT, LUX_800);
    set16(HIGH_LIMIT, LUX_1000);
    set16(CONFIG, CONFIG_RUN);

    for (;;) {
        uint16_t r;
        uint32_t centilux;
        unsigned int irq;

        delay_ms(200);
        r = reg16(RESULT);
        centilux = (uint32_t)(r & 0x0FFFU) << (r >> 12);
        if (shown == 0xFFFFFFFFU || centilux * 100U > shown * 101U ||
            centilux * 101U < shown * 100U) {
            console_puts("lux ");
            put_fixed((int32_t)centilux, 2);
            console_puts("\r\n");
            shown = centilux;
        }
        irq = !((INT_GROUP->PORT_IN >> INT_PIN) & 1U);
        if (irq != was) {
            console_puts(irq ? "INT low: above 1000 lx\r\n"
                             : "INT high\r\n");
            was = irq;
        }
    }
}
