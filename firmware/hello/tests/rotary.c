/*
 * Rotary Click demo for the web board view: a Rotary G (R, B, Y) Click on
 * EXT1 (through a mikroBUS Xplained Pro adapter). Its EC12D encoder: A on
 * PWM (PB10), B on AN (PB08), the push switch on INT (PB02, high while
 * pressed, no pull on the Click: the PORT's pull-down is enabled); its 16
 * LEDs on two 74HC595 on SERCOM1 SPI, LATCH on CS (PA17), MR# on RST
 * (PA20). Counts detents the way MikroElektronika's driver does (at a
 * detent A = B; the first sample with A != B is a step, clockwise when A
 * moved) and lights the LED of the position (LED1 at six o'clock,
 * clockwise); every LED while the switch is held. Prints the position and
 * the switch when they change. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=rotaryg
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "rotary.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

#define ENC_A_PIN       10U     /* PB10, PWM */
#define ENC_B_PIN       8U      /* PB08, AN */
#define SW_PIN          2U      /* PB02, INT */
#define LATCH_PIN       17U     /* PA17, CS */
#define MR_PIN          20U     /* PA20, RST */

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

/* Bit n lights LED n+1: the high byte goes first, to the second 595 */
static void ring(uint16_t leds)
{
    PA->PORT_OUTCLR = 1U << LATCH_PIN;
    spi_xfer((uint8_t)(leds >> 8));
    spi_xfer((uint8_t)leds);
    PA->PORT_OUTSET = 1U << LATCH_PIN;                    /* STCP rising */
}

static unsigned int pin(unsigned int n)
{
    return (PB->PORT_IN >> n) & 1U;
}

int main(void)
{
    int position = 0, shown = -1;
    unsigned int start, held = 0, was = 2;

    sys_initialize();
    systick_start();
    console_puts("PP rotary: " BOARD_BANNER "\r\n");
    PA->PORT_OUTCLR = 1U << LATCH_PIN;
    PA->PORT_OUTSET = 1U << MR_PIN;
    PA->PORT_DIRSET = (1U << LATCH_PIN) | (1U << MR_PIN);
    PB->PORT_PINCFG[ENC_A_PIN] = PORT_PINCFG_INEN_Msk;
    PB->PORT_PINCFG[ENC_B_PIN] = PORT_PINCFG_INEN_Msk;
    PB->PORT_OUTCLR = 1U << SW_PIN;                       /* pull-down */
    PB->PORT_PINCFG[SW_PIN] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
    spi_init();
    start = pin(ENC_A_PIN) & pin(ENC_B_PIN);
    console_puts("turn the knob or push it\r\n");

    for (;;) {
        unsigned int a = pin(ENC_A_PIN), b = pin(ENC_B_PIN);
        unsigned int sw = pin(SW_PIN);

        if (a == b) {
            start = a & b;
        } else if (!held) {
            position += a != start ? 1 : -1;
            held = 1;               /* one step until the next detent */
        }
        if (a == b) {
            held = 0;
        }
        if (position != shown || sw != was) {
            int slot = ((position % 16) + 16) % 16;

            ring(sw ? 0xFFFFU : (uint16_t)(1U << slot));
            if (position != shown) {
                console_puts("position ");
                if (position < 0) {
                    console_putc('-');
                }
                console_put_dec((uint32_t)(position < 0 ? -position
                                                        : position));
                console_puts("\r\n");
            }
            if (sw != was && was != 2U) {
                console_puts(sw ? "pressed\r\n" : "released\r\n");
            }
            shown = position;
            was = sw;
        }
        delay_us(100);
    }
}
