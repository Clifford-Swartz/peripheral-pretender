/*
 * SPI host on EXT1 of the EV15N46A (SERCOM1: PA16 MISO PAD0, PA18 MOSI
 * PAD2, PA19 SCK PAD3; mode 0, 1 MHz) with the mikroBUS adapter's other
 * pins as GPIO: CS PA17, RST PA20, PWM PB10, AN PB08, INT PB02. Shared by
 * the SPI Click demos (adcclick.c, dacclick.c, ledringr.c, lcdmini.c).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef EXT1_SPI_H
#define EXT1_SPI_H

#include "console.h"
#include "sys_init.h"
#include "util.h"

#ifndef _PIC32CM1216MC00048_H_
#error "ext1_spi.h is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define SPI             (&SERCOM1_REGS->SPIM)

#define CS_PIN          17U     /* PA17 */
#define RST_PIN         20U     /* PA20 */
#define PWM_PIN         10U     /* PB10 */
#define AN_PIN          8U      /* PB08 */
#define INT_PIN         2U      /* PB02 */

static void spi_pmux_c(unsigned int pin)
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

/* SERCOM1 as SPI host; CS (PA17) an output, high */
static void spi_init(void)
{
    PA->PORT_OUTSET = 1U << CS_PIN;
    PA->PORT_DIRSET = 1U << CS_PIN;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] = GCLK_PCHCTRL_GEN_GCLK0 |
                                                   GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] &
             GCLK_PCHCTRL_CHEN_Msk)) {
    }
    spi_pmux_c(16);
    spi_pmux_c(18);
    spi_pmux_c(19);
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

static uint8_t spi_xfer(uint8_t v)
{
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
    }
    SPI->SERCOM_DATA = v;
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk)) {
    }
    return (uint8_t)SPI->SERCOM_DATA;
}

static void cs_low(void)
{
    PA->PORT_OUTCLR = 1U << CS_PIN;
}

static void cs_high(void)
{
    PA->PORT_OUTSET = 1U << CS_PIN;
}

#endif /* EXT1_SPI_H */
