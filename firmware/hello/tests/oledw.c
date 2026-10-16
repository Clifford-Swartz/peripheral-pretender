/*
 * OLED W Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter), an SSD1306 with a 96x39 white OLED on
 * SERCOM1 SPI (the Click's jumpers on SPI). Draws a frame round the glass
 * and bounces a ball inside it; SW0 inverts the display while held. Runs
 * forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=oledw
 *
 * EXT1 wiring (mikroBUS Xplained Pro, OLED W Click schematic v100): CS on
 * PA17, D/C on PWM PB10, RES# on RST PA20; SPI MOSI PA18, SCK PA19. The
 * set-up is MikroElektronika's oledw default configuration (MUX 40,
 * offset 0, start line 0, charge pump on, COM scan reversed, alternative
 * COM layout, no segment re-map); with it, screen row y shows RAM line
 * y + 1 and screen column x RAM column 95 - x, so the frame buffer below
 * is kept that way and sent page by page, 96 bytes each, as the driver
 * does.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <string.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "oledw.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

#define CS_PIN          17U     /* PA17 */
#define RST_PIN         20U     /* PA20 */
#define DC_PIN          10U     /* PB10 */
#define SW0_PIN         25U     /* PA25 */
#define W               96U
#define H               39U
#define PAGES           5U      /* RAM lines 0-39 */

static uint8_t fb[PAGES][W];

static void pmux_c(unsigned int pin)
{
    PA->PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    PA->PORT_PMUX[pin / 2U] = (PA->PORT_PMUX[pin / 2U] &
                               ((pin & 1U) ? 0x0FU : 0xF0U)) |
                              (uint8_t)(2U << ((pin & 1U) * 4U));
}

static void spi_sync(void)
{
    while (SPI->SERCOM_SYNCBUSY != 0U) {
    }
}

static void spi_init(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] = GCLK_PCHCTRL_GEN_GCLK0 |
                                                   GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] &
             GCLK_PCHCTRL_CHEN_Msk)) {
    }
    pmux_c(16);                 /* PAD0 MISO */
    pmux_c(18);                 /* PAD2 MOSI */
    pmux_c(19);                 /* PAD3 SCK */
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_MODE_SPI_MASTER |
                        SERCOM_SPIM_CTRLA_DOPO(1U) |     /* DO PAD2, SCK PAD3 */
                        SERCOM_SPIM_CTRLA_DIPO(0U);
    SPI->SERCOM_CTRLB = SERCOM_SPIM_CTRLB_RXEN_Msk;
    SPI->SERCOM_BAUD = 2;                                 /* 8 MHz */
    spi_sync();
    SPI->SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    spi_sync();
}

static void spi_byte(uint8_t v)
{
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
    }
    SPI->SERCOM_DATA = v;
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk)) {
    }
    (void)SPI->SERCOM_DATA;
}

/* D/C low: commands, high: data; CS low around the bytes */
static void send(int data, const uint8_t *b, unsigned int n)
{
    if (data) {
        PB->PORT_OUTSET = 1U << DC_PIN;
    } else {
        PB->PORT_OUTCLR = 1U << DC_PIN;
    }
    PA->PORT_OUTCLR = 1U << CS_PIN;
    for (unsigned int i = 0; i < n; i++) {
        spi_byte(b[i]);
    }
    PA->PORT_OUTSET = 1U << CS_PIN;
}

static void oled_init(void)
{
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x27, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0xC8, 0xDA, 0x12, 0x81, 0xAF, 0xD9, 0x25, 0xDB, 0x20, 0xA4,
        0xA6, 0xAF,
    };

    PA->PORT_OUTSET = (1U << CS_PIN) | (1U << RST_PIN);
    PA->PORT_DIRSET = (1U << CS_PIN) | (1U << RST_PIN);
    PB->PORT_DIRSET = 1U << DC_PIN;
    PA->PORT_OUTCLR = 1U << RST_PIN;            /* reset pulse */
    delay_ms(1);
    PA->PORT_OUTSET = 1U << RST_PIN;
    delay_ms(1);
    send(0, init, sizeof(init));
}

/* Screen pixel (x, y): RAM line y + 1, RAM column 95 - x */
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

        send(0, at, sizeof(at));
        send(1, fb[p], W);
    }
}

int main(void)
{
    int bx = 10, by = 10, dx = 2, dy = 1;
    unsigned int inverted = 0;

    sys_initialize();
    systick_start();
    console_puts("PP oledw: " BOARD_BANNER "\r\n");
    /* SW0 switches to ground and has no pull-up on the board: the PORT's */
    PA->PORT_DIRCLR = 1U << SW0_PIN;
    PA->PORT_OUTSET = 1U << SW0_PIN;
    PA->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    spi_init();
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

            send(0, &c, 1);
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
