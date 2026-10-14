/*
 * RTC 6 Click demo for the web board view: an MCP79411 RTCC on EXT1
 * (through a mikroBUS Xplained Pro adapter) at I2C 0x6F/0x57 on SERCOM2,
 * its MFP (open drain, the Click's 10k pull-up) on INT (PB02). Prints the
 * EUI-48 from the protected EEPROM and checks the battery-backed SRAM,
 * starts the clock at 2026-10-14 12:00:00 if it is stopped, prints the
 * time every second, and sets alarm 0 five seconds ahead each time it
 * fires: "ALARM" on the VCOM port and LED0 for a second. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=rtc6
 *
 * The alarm uses the seconds mask and ALMPOL = 0 (MFP low while ALM0IF is
 * set). ALM0IF is set again while the match persists, so the demo clears
 * it once the second has passed (data sheet DS20002266H, Register 5-11).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_i2c.h"

#define RTCC            0x6FU
#define RTCEE           0x57U
#define MFP_GROUP       (&PORT_REGS->GROUP[1])
#define MFP_PIN         2U              /* PB02, EXT1 pin 9 */

static unsigned int bcd(uint8_t v)
{
    return (v >> 4) * 10U + (v & 0x0FU);
}

static uint8_t to_bcd(unsigned int v)
{
    return (uint8_t)((v / 10U) << 4 | (v % 10U));
}

static void put2(unsigned int v)
{
    console_putc((char)('0' + v / 10U));
    console_putc((char)('0' + v % 10U));
}

static int mfp_low(void)
{
    return (MFP_GROUP->PORT_IN & (1U << MFP_PIN)) == 0U;
}

static void arm_alarm(unsigned int sec)
{
    i2c_write1(RTCC, 0x0A, to_bcd(sec % 60U));     /* ALM0SEC */
    i2c_write1(RTCC, 0x0D, 0x00);                  /* ALMPOL 0, seconds */
}

int main(void)
{
    uint8_t t[7], eui[6], sram[4];
    static const uint8_t pattern[4] = { 0x50, 0x50, 0x52, 0x36 };
    static const uint8_t start[7] = { 0x80, 0x00, 0x12, 0x0B, 0x14, 0x10,
                                      0x26 };
    unsigned int last = 99, alarm_at = 99;

    sys_initialize();
    systick_start();
    console_puts("PP rtc6: " BOARD_BANNER "\r\n");

    LED0_GROUP->PORT_OUTSET = LED0_MASK;            /* off (active low) */
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    MFP_GROUP->PORT_DIRCLR = 1U << MFP_PIN;         /* R2 pulls up */
    MFP_GROUP->PORT_PINCFG[MFP_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();

    while (i2c_read(RTCEE, 0xF2, eui, 6) != 0) {
        console_puts("no RTC 6 Click on EXT1 (MCP79411 at 0x6F/0x57)\r\n");
        delay_ms(2000);
    }
    console_puts("RTC 6 Click found, EUI-48");
    for (unsigned int i = 0; i < 6U; i++) {
        console_putc(i ? ':' : ' ');
        console_put_hex(eui[i], 2);
    }
    console_puts("\r\n");
    i2c_write(RTCC, 0x20, pattern, 4);
    i2c_read(RTCC, 0x20, sram, 4);
    console_puts(sram[3] == 0x36U ? "SRAM ok\r\n" : "SRAM FAILED\r\n");

    i2c_read(RTCC, 0x03, t, 1);
    if (!(t[0] & 0x20U)) {                          /* OSCRUN */
        console_puts("clock stopped: starting at 2026-10-14 12:00:00\r\n");
        i2c_write(RTCC, 0x00, start, 7);
    }
    i2c_write1(RTCC, 0x07, 0x10);                   /* ALM0EN, no SQW */

    for (;;) {
        unsigned int sec;

        if (i2c_read(RTCC, 0x00, t, 7) != 0) {
            continue;
        }
        sec = bcd(t[0] & 0x7FU);
        if (sec != last) {
            last = sec;
            console_puts("20");
            put2(bcd(t[6]));
            console_putc('-');
            put2(bcd(t[5] & 0x1FU));
            console_putc('-');
            put2(bcd(t[4] & 0x3FU));
            console_putc(' ');
            put2(bcd(t[2] & 0x3FU));
            console_putc(':');
            put2(bcd(t[1] & 0x7FU));
            console_putc(':');
            put2(sec);
            if (alarm_at == 99U) {
                alarm_at = (sec + 5U) % 60U;
                arm_alarm(alarm_at);
            } else if (alarm_at == 100U) {
                /* the alarm second has passed: clear ALM0IF, re-arm */
                LED0_GROUP->PORT_OUTSET = LED0_MASK;
                alarm_at = (sec + 4U) % 60U;
                arm_alarm(alarm_at);
            }
            if (mfp_low() && alarm_at != 100U) {
                console_puts(" ALARM");
                LED0_GROUP->PORT_OUTCLR = LED0_MASK;
                alarm_at = 100U;
            }
            console_puts("\r\n");
        }
        delay_ms(20);
    }
}
