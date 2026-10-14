/*
 * Thumbstick Click demo for the web board view: the Click on EXT1 (through
 * a mikroBUS Xplained Pro adapter), its MCP3204 ADC on SERCOM1 SPI (CS# on
 * PA17) and the stick's push switch on INT (PB02, low while pushed).
 * Reads both axes every 20 ms and prints them with the direction whenever
 * they move; LED0 lights while the stick is pushed. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=thumbstick
 *
 * The axes follow MikroElektronika's thumbstick driver: CH0 vertical (more
 * is down), CH1 horizontal (more is right), centre 2048.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "thumbstick.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

#define CS_PIN          17U     /* PA17 */
#define SW_PIN          2U      /* PB02 */
#define DEAD            400     /* counts from the centre: still centred */

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
                        SERCOM_SPIM_CTRLA_DIPO(0U);      /* DI PAD0 */
    SPI->SERCOM_CTRLB = SERCOM_SPIM_CTRLB_RXEN_Msk;
    SPI->SERCOM_BAUD = 23;                                /* 1 MHz */
    spi_sync();
    SPI->SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    spi_sync();
}

static uint8_t spi_xfer(uint8_t v)
{
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
    }
    SPI->SERCOM_DATA = v;
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk)) {
    }
    return (uint8_t)SPI->SERCOM_DATA;
}

/* Single-ended conversion of @ch (MCP3204 data sheet Figure 6-1) */
static int adc_read(unsigned int ch)
{
    unsigned int hi, lo;

    PA->PORT_OUTCLR = 1U << CS_PIN;
    (void)spi_xfer(0x06U);                      /* start, single-ended */
    hi = spi_xfer((uint8_t)(ch << 6));
    lo = spi_xfer(0x00U);
    PA->PORT_OUTSET = 1U << CS_PIN;
    return (int)(((hi & 0x0FU) << 8) | lo);
}

static void put_axis(const char *name, int v)
{
    console_puts(name);
    console_put_dec((uint32_t)v);
}

static const char *direction(int x, int y)
{
    if (y < 2048 - DEAD) {
        return x > 2048 + DEAD ? "up-right" : x < 2048 - DEAD ? "up-left" : "up";
    }
    if (y > 2048 + DEAD) {
        return x > 2048 + DEAD ? "down-right" : x < 2048 - DEAD ? "down-left"
                                                                : "down";
    }
    return x > 2048 + DEAD ? "right" : x < 2048 - DEAD ? "left" : "centre";
}

int main(void)
{
    int last_x = -1000, last_y = -1000, last_sw = -1;

    sys_initialize();
    systick_start();
    console_puts("PP thumbstick: " BOARD_BANNER "\r\n");
    console_puts("Thumbstick Click on EXT1: move or push the stick\r\n");

    PB->PORT_DIRCLR = 1U << SW_PIN;                       /* R3 pulls up */
    PB->PORT_PINCFG[SW_PIN] = PORT_PINCFG_INEN_Msk;
    PA->PORT_OUTSET = 1U << CS_PIN;
    PA->PORT_DIRSET = 1U << CS_PIN;
    LED0_GROUP->PORT_OUTSET = LED0_MASK;                  /* off */
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    spi_init();

    for (;;) {
        int y = adc_read(0), x = adc_read(1);
        int sw = (PB->PORT_IN & (1U << SW_PIN)) == 0U;

        if (sw) {
            LED0_GROUP->PORT_OUTCLR = LED0_MASK;
        } else {
            LED0_GROUP->PORT_OUTSET = LED0_MASK;
        }
        if (sw != last_sw || x - last_x > 40 || last_x - x > 40 ||
            y - last_y > 40 || last_y - y > 40) {
            put_axis("stick x=", x);
            put_axis(" y=", y);
            console_puts(" ");
            console_puts(direction(x, y));
            console_puts(sw ? " pushed\r\n" : "\r\n");
            last_x = x;
            last_y = y;
            last_sw = sw;
        }
        delay_ms(20);
    }
}
