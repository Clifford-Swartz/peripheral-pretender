/*
 * OLED B Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter), an SSD1306 with a 96x39 blue OLED on the
 * headers' I2C (SERCOM2). Draws a frame round the glass and bounces a ball
 * inside it; SW0 inverts the display while held. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=oledb
 *
 * EXT1 wiring (mikroBUS Xplained Pro, OLED B Click schematic v101): RES#
 * on RST PA20, D/C# (the SSD1306's SA0 on I2C) on PWM PB10. As
 * MikroElektronika's oledb driver does, commands go to 0x3C with D/C low
 * and display data to 0x3D with D/C high, each transfer starting with its
 * control byte (00h: commands, 40h: data). The set-up and the frame buffer
 * are oledw.c's: screen row y is RAM line y + 1, column x RAM column
 * 95 - x.
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
#error "oledb.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])

#define RST_PIN         20U     /* PA20 */
#define DC_PIN          10U     /* PB10: SA0 */
#define SW0_PIN         25U     /* PA25 */
#define OLED_CMD        0x3CU   /* SA0 low */
#define OLED_DATA       0x3DU   /* SA0 high */
#define W               96U
#define H               39U
#define PAGES           5U

static uint8_t fb[PAGES][W];

static void commands(const uint8_t *c, unsigned int n)
{
    PB->PORT_OUTCLR = 1U << DC_PIN;
    i2c_write(OLED_CMD, 0x00, c, n);
}

static void data(const uint8_t *d, unsigned int n)
{
    PB->PORT_OUTSET = 1U << DC_PIN;
    i2c_write(OLED_DATA, 0x40, d, n);
}

static void oled_init(void)
{
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x27, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0xC8, 0xDA, 0x12, 0x81, 0xAF, 0xD9, 0x25, 0xDB, 0x20, 0xA4,
        0xA6, 0xAF,
    };

    PA->PORT_OUTSET = 1U << RST_PIN;
    PA->PORT_DIRSET = 1U << RST_PIN;
    PB->PORT_DIRSET = 1U << DC_PIN;
    PA->PORT_OUTCLR = 1U << RST_PIN;            /* reset pulse */
    delay_ms(1);
    PA->PORT_OUTSET = 1U << RST_PIN;
    delay_ms(1);
    commands(init, sizeof(init));
}

static void plot(unsigned int x, unsigned int y)
{
    unsigned int line = y + 1U;

    if (x < W && y < H) {
        fb[line / 8U][W - 1U - x] |= (uint8_t)(1U << (line % 8U));
    }
}

static void flush(void)
{
    for (unsigned int p = 0; p < PAGES; p++) {
        uint8_t at[3] = { (uint8_t)(0xB0 | p), 0x10, 0x00 };

        commands(at, sizeof(at));
        data(fb[p], W);
    }
}

int main(void)
{
    int bx = 10, by = 10, dx = 2, dy = 1;
    unsigned int inverted = 0;

    sys_initialize();
    systick_start();
    console_puts("PP oledb: " BOARD_BANNER "\r\n");
    /* SW0 switches to ground and has no pull-up on the board: the PORT's */
    PA->PORT_DIRCLR = 1U << SW0_PIN;
    PA->PORT_OUTSET = 1U << SW0_PIN;
    PA->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    i2c_init();
    oled_init();
    console_puts("frame and a bouncing ball; hold SW0 to invert\r\n");

    for (;;) {
        unsigned int sw0 = !(PA->PORT_IN & (1U << SW0_PIN));

        memset(fb, 0, sizeof(fb));
        for (unsigned int x = 0; x < W; x++) {
            plot(x, 0);
            plot(x, H - 1U);
        }
        for (unsigned int y = 0; y < H; y++) {
            plot(0, y);
            plot(W - 1U, y);
        }
        for (int y = by; y < by + 5; y++) {
            for (int x = bx; x < bx + 5; x++) {
                plot((unsigned int)x, (unsigned int)y);
            }
        }
        flush();
        if (sw0 != inverted) {
            uint8_t c = sw0 ? 0xA7 : 0xA6;

            commands(&c, 1);
            inverted = sw0;
        }
        bx += dx;
        by += dy;
        if (bx <= 2 || bx >= (int)W - 7) {
            dx = -dx;
        }
        if (by <= 2 || by >= (int)H - 7) {
            dy = -dy;
        }
        delay_ms(40);
    }
}
