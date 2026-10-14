/*
 * Click demo for the web board view: Button G Click on EXT1, 7seg Click on
 * EXT2 (each through a mikroBUS Xplained Pro adapter). Every press of the
 * Click's button counts up on the two digits (00-99); the button's LED
 * lights while it is held; SW0 resets the count. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=buttong,ext2=7seg
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "clicks.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

/* EXT1: Button G Click. Button on INT (high while pressed), LED on PWM */
#define BTN_PIN         2U      /* PB02 */
#define BTN_LED_PIN     10U     /* PB10 */
/* EXT2: 7seg Click. LATCH on CS, MR# on RST, digits enabled by PWM */
#define SEG_LATCH_PIN   15U     /* PA15 */
#define SEG_MR_PIN      10U     /* PA10 */
#define SEG_EN_PIN      22U     /* PB22 */

/*
 * Segment patterns: the 74HC595 outputs Q0..Q7 drive dp, b, a, c, d, e, f, g
 * (7seg Click schematic v100)
 */
static const uint8_t digits[10] = {
    0x7E, 0x0A, 0xB6, 0x9E, 0xCA, 0xDC, 0xFC, 0x0E, 0xFE, 0xDE,
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
    pmux_c(16);                 /* PAD0 MISO */
    pmux_c(18);                 /* PAD2 MOSI */
    pmux_c(19);                 /* PAD3 SCK */
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_MODE_SPI_MASTER |
                        SERCOM_SPIM_CTRLA_DOPO(1U) |     /* DO PAD2, SCK PAD3 */
                        SERCOM_SPIM_CTRLA_DIPO(0U);
    SPI->SERCOM_CTRLB = SERCOM_SPIM_CTRLB_RXEN_Msk;
    SPI->SERCOM_BAUD = 23;                                /* 1 MHz */
    spi_sync();
    SPI->SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    spi_sync();
}

static void spi_xfer(uint8_t v)
{
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
    }
    SPI->SERCOM_DATA = v;
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk)) {
    }
    (void)SPI->SERCOM_DATA;
}

/* The right digit's byte goes first: it shifts through to the second 595 */
static void show(unsigned int n)
{
    PA->PORT_OUTCLR = 1U << SEG_LATCH_PIN;
    spi_xfer(digits[n % 10U]);
    spi_xfer(digits[(n / 10U) % 10U]);
    PA->PORT_OUTSET = 1U << SEG_LATCH_PIN;                /* STCP rising */
}

static int pressed(void)
{
    return (PB->PORT_IN & (1U << BTN_PIN)) != 0U;
}

static int sw0_pressed(void)
{
    return (SW0_GROUP->PORT_IN & (1U << SW0_PIN)) == 0U;
}

int main(void)
{
    unsigned int count = 0;
    int was = 0;

    sys_initialize();
    systick_start();
    console_puts("PP clicks: " BOARD_BANNER "\r\n");
    console_puts("Button G Click on EXT1 counts on the 7seg Click on EXT2; "
                 "SW0 resets\r\n");

    PB->PORT_DIRCLR = 1U << BTN_PIN;                      /* the Click pulls */
    PB->PORT_PINCFG[BTN_PIN] = PORT_PINCFG_INEN_Msk;
    PB->PORT_OUTCLR = 1U << BTN_LED_PIN;
    PB->PORT_DIRSET = (1U << BTN_LED_PIN) | (1U << SEG_EN_PIN);
    PB->PORT_OUTSET = 1U << SEG_EN_PIN;                   /* digits on */
    PA->PORT_OUTSET = (1U << SEG_MR_PIN) | (1U << SEG_LATCH_PIN);
    PA->PORT_DIRSET = (1U << SEG_MR_PIN) | (1U << SEG_LATCH_PIN);
    SW0_GROUP->PORT_DIRCLR = 1U << SW0_PIN;
    SW0_GROUP->PORT_OUTSET = 1U << SW0_PIN;               /* pull-up */
    SW0_GROUP->PORT_PINCFG[SW0_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;
    spi_init();
    show(count);

    for (;;) {
        int now = pressed();

        if (now) {
            PB->PORT_OUTSET = 1U << BTN_LED_PIN;
        } else {
            PB->PORT_OUTCLR = 1U << BTN_LED_PIN;
        }
        if (now && !was) {
            count = (count + 1U) % 100U;
            show(count);
            console_puts("press");
            put_field("count", count, 2);
            console_puts("\r\n");
        }
        if (sw0_pressed() && count) {
            count = 0;
            show(count);
            console_puts("reset\r\n");
        }
        was = now;
        delay_ms(5);
    }
}
