/*
 * Thermo K Click demo for the web board view: an MCP9600 on EXT1 (through
 * a mikroBUS Xplained Pro adapter) at I2C 0x60 on SERCOM2 with a type K
 * thermocouple, ALERT1 (push-pull) on INT (PB02). Prints the device ID,
 * sets ALERT1 to go low above 100 C (hysteresis 2 C, comparator mode),
 * then prints the hot and cold junction temperatures when either moves by
 * a quarter degree, and each change of ALERT1. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=thermok
 *
 * TH, TC and the limits are signed 16-bit, 0.0625 C a count (MCP9600 data
 * sheet DS20005426F registers 5-2 to 5-11); the power-on configuration is
 * a type K thermocouple, 18-bit conversions, normal mode.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_i2c.h"

#define MCP9600         0x60U
#define TH              0x00U
#define TC              0x02U
#define ALERT1_CONFIG   0x08U
#define ALERT1_HYST     0x0CU
#define ALERT1_LIMIT    0x10U
#define DEVICE_ID       0x20U

/* Enabled, rising, active low, comparator mode, on TH */
#define ALERT_ON_RISING 0x09U

#define INT_GROUP       (&PORT_REGS->GROUP[1])
#define INT_PIN         2U              /* PB02, EXT1 pin 9 */

static int16_t temp16(uint8_t reg)
{
    uint8_t d[2] = { 0, 0 };

    i2c_read(MCP9600, reg, d, 2);
    return (int16_t)(d[0] << 8 | d[1]);
}

/* Hundredths of a degree from 0.0625 C counts */
static int32_t centi(int16_t t)
{
    return (int32_t)t * 25 / 4;
}

int main(void)
{
    uint8_t id[2] = { 0, 0 };
    uint8_t limit[2] = { 100 * 16 >> 8, 100 * 16 & 0xFF };
    int16_t hot_shown = INT16_MIN, cold_shown = INT16_MIN;
    unsigned int was = 2;

    sys_initialize();
    systick_start();
    console_puts("PP thermok: " BOARD_BANNER "\r\n");
    INT_GROUP->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();
    i2c_read(MCP9600, DEVICE_ID, id, 2);
    console_puts("device 0x");
    console_put_hex(id[0], 2);
    console_puts(" revision 0x");
    console_put_hex(id[1], 2);
    console_puts("\r\n");
    i2c_write(MCP9600, ALERT1_LIMIT, limit, 2);
    i2c_write1(MCP9600, ALERT1_HYST, 2);
    i2c_write1(MCP9600, ALERT1_CONFIG, ALERT_ON_RISING);

    for (;;) {
        int16_t hot, cold;
        unsigned int alert;

        delay_ms(250);
        hot = temp16(TH);
        cold = temp16(TC);
        if (hot > hot_shown + 3 || hot < hot_shown - 3 ||
            cold > cold_shown + 3 || cold < cold_shown - 3) {
            console_puts("hot ");
            put_fixed(centi(hot), 2);
            console_puts(" C cold ");
            put_fixed(centi(cold), 2);
            console_puts(" C\r\n");
            hot_shown = hot;
            cold_shown = cold;
        }
        alert = !((INT_GROUP->PORT_IN >> INT_PIN) & 1U);
        if (alert != was) {
            console_puts(alert ? "ALERT1: above 100 C\r\n" : "ALERT1 off\r\n");
            was = alert;
        }
    }
}
