/*
 * OLED C Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter), an SSD1351 with a 96x96 RGB OLED on
 * SERCOM1 SPI. Draws eight colour bars, then bounces an orange square
 * over them; SW0 inverts the display while held. Runs
 * forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=oledc
 *
 * EXT1 wiring (mikroBUS Xplained Pro, OLED C Click schematic v102): CS on
 * PA17, D/C on PWM PB10, RES# on RST PA20, the 15 V supply's EN on INT
 * PB02; SPI MOSI PA18, SCK PA19. The set-up is MikroElektronika's
 * oledc_default_cfg(): unlock, re-map 32h, MUX 95, offset 20h, the panel
 * at columns 16-111 and rows 0-95.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "oledc.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

#define CS_PIN          17U     /* PA17 */
#define RST_PIN         20U     /* PA20 */
#define DC_PIN          10U     /* PB10 */
#define EN_PIN          2U      /* PB02 */

#define W               96U
#define COL0            16U
#define BOX             12U

static const uint16_t bars[8] = {
    0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000,
};

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
    pmux_c(18);                 /* PAD2 MOSI */
    pmux_c(19);                 /* PAD3 SCK */
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_MODE_SPI_MASTER |
                        SERCOM_SPIM_CTRLA_DOPO(1U) |     /* DO PAD2, SCK PAD3 */
                        SERCOM_SPIM_CTRLA_DIPO(0U);
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
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_TXC_Msk)) {
    }
}

static void select(int on)
{
    if (on) {
        PA->PORT_OUTCLR = 1U << CS_PIN;
    } else {
        PA->PORT_OUTSET = 1U << CS_PIN;
    }
}

static void cmd(uint8_t c, const uint8_t *args, unsigned int n)
{
    select(1);
    PB->PORT_OUTCLR = 1U << DC_PIN;
    spi_byte(c);
    PB->PORT_OUTSET = 1U << DC_PIN;
    for (unsigned int i = 0; i < n; i++) {
        spi_byte(args[i]);
    }
    select(0);
}

static void cmd1(uint8_t c, uint8_t a)
{
    cmd(c, &a, 1);
}

/* Fill the panel rectangle x0..x1, y0..y1 (inclusive) with @color */
static void fill(unsigned int x0, unsigned int y0, unsigned int x1,
                 unsigned int y1, uint16_t color)
{
    uint8_t cols[2] = { (uint8_t)(COL0 + x0), (uint8_t)(COL0 + x1) };
    uint8_t rows[2] = { (uint8_t)y0, (uint8_t)y1 };

    cmd(0x15, cols, 2);
    cmd(0x75, rows, 2);
    cmd(0x5C, 0, 0);
    select(1);
    for (unsigned int n = (x1 - x0 + 1U) * (y1 - y0 + 1U); n > 0U; n--) {
        spi_byte((uint8_t)(color >> 8));
        spi_byte((uint8_t)color);
    }
    select(0);
}

/* Redraw the bars under the square at (x0, y0) */
static void restore(unsigned int x0, unsigned int y0)
{
    for (unsigned int b = 0; b < 8U; b++) {
        unsigned int bx0 = b * 12U, bx1 = bx0 + 11U;
        unsigned int lo = x0 > bx0 ? x0 : bx0;
        unsigned int hi = x0 + BOX - 1U < bx1 ? x0 + BOX - 1U : bx1;

        if (lo <= hi) {
            fill(lo, y0, hi, y0 + BOX - 1U, bars[b]);
        }
    }
}

static void oled_init(void)
{
    PB->PORT_OUTSET = 1U << EN_PIN;                       /* 15 V on */
    PB->PORT_DIRSET = (1U << EN_PIN) | (1U << DC_PIN);
    PA->PORT_OUTSET = (1U << CS_PIN) | (1U << RST_PIN);
    PA->PORT_DIRSET = (1U << CS_PIN) | (1U << RST_PIN);
    delay_ms(1);
    PA->PORT_OUTCLR = 1U << RST_PIN;
    delay_ms(1);
    PA->PORT_OUTSET = 1U << RST_PIN;
    delay_ms(100);

    cmd1(0xFD, 0x12);           /* unlock */
    cmd1(0xFD, 0xB1);           /* A2h B1h B3h BBh BEh C1h accessible */
    cmd(0xAE, 0, 0);            /* sleep */
    cmd1(0xA0, 0x32);           /* 65k, COM split, reversed, cols re-mapped */
    cmd1(0xCA, 95);             /* MUX 96 */
    cmd1(0xA1, 0x00);
    cmd1(0xA2, 0x20);
    cmd1(0xBE, 0x05);
    cmd1(0xB3, 0xF1);
    cmd1(0xB1, 0x32);
    cmd1(0xB6, 0x01);
    cmd1(0xC7, 0x0F);
    cmd(0xC1, (const uint8_t[]){ 0x8A, 0x51, 0x8A }, 3);
    cmd(0xB4, (const uint8_t[]){ 0xA0, 0xB5, 0x55 }, 3);
    cmd(0xA6, 0, 0);            /* normal */
    cmd(0xAF, 0, 0);            /* display on */
}

static int sw0_pressed(void)
{
    return (SW0_GROUP->PORT_IN & (1U << SW0_PIN)) == 0U;
}

int main(void)
{
    unsigned int x = 4, y = 20, frames = 0;
    int dx = 2, dy = 1, inverse = 0;

    sys_initialize();
    systick_start();
    console_puts("PP oledc: " BOARD_BANNER "\r\n");
    console_puts("OLED C Click on EXT1: colour bars, a bouncing square; "
                 "hold SW0 to invert\r\n");
    SW0_GROUP->PORT_DIRCLR = 1U << SW0_PIN;
    SW0_GROUP->PORT_OUTSET = 1U << SW0_PIN;
    SW0_GROUP->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;
    spi_init();
    oled_init();
    for (unsigned int b = 0; b < 8U; b++) {
        fill(b * 12U, 0, b * 12U + 11U, W - 1U, bars[b]);
    }
    console_puts("bars drawn\r\n");

    for (;;) {
        int pressed = sw0_pressed();

        if (pressed != inverse) {
            inverse = pressed;
            cmd(inverse ? 0xA7 : 0xA6, 0, 0);
            console_puts(inverse ? "inverse\r\n" : "normal\r\n");
        }
        restore(x, y);
        if ((int)x + dx < 0 || x + dx + BOX > W) {
            dx = -dx;
        }
        if ((int)y + dy < 0 || y + dy + BOX > W) {
            dy = -dy;
        }
        x += dx;
        y += dy;
        fill(x, y, x + BOX - 1U, y + BOX - 1U, 0xFD20);
        if (++frames % 32U == 0U) {
            console_puts("frame");
            put_field("n", frames, 4);
            console_puts("\r\n");
        }
        delay_ms(30);
    }
}
