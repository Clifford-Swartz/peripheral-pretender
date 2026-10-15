/*
 * Clock and console bring-up for the Curiosity CircuitPython board, in the
 * order the CircuitPython SAM D5x port uses (samd-peripherals clocks.c):
 * the 32.768 kHz crystal, GCLK5 = DFLL48M / 24 as the 2 MHz reference of
 * DPLL0, DPLL0 x 60 = 120 MHz on GCLK0, GCLK1 = DFLL48M for SERCOM3, then
 * the debug UART at 115200 8N1.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"

/* 65536 * (1 - 16 * 115200 / 48 MHz), as MCC computes it */
#define SERCOM3_USART_INT_BAUD_VALUE 63019U

static void gclk_generator(unsigned int gen, uint32_t src, uint32_t div)
{
    GCLK_REGS->GCLK_GENCTRL[gen] = GCLK_GENCTRL_DIV(div) | src |
                                   GCLK_GENCTRL_GENEN_Msk;
    while ((GCLK_REGS->GCLK_SYNCBUSY & (1U << (2U + gen))) != 0U) {
    }
}

/* PB23 (LED) output low, PA22/PA23 to SERCOM3 (function C) */
static void port_initialize(void)
{
    PORT_REGS->GROUP[1].PORT_DIRSET = PORT_PB23;
    PORT_REGS->GROUP[1].PORT_OUTCLR = PORT_PB23;
    PORT_REGS->GROUP[0].PORT_PINCFG[22] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_PINCFG[23] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_PMUX[11] =
        PORT_PMUX_PMUXE(MUX_PA22C_SERCOM3_PAD0) |
        PORT_PMUX_PMUXO(MUX_PA23C_SERCOM3_PAD1);
    /* USER button: input, the board has the pull-up (R20) */
    PORT_REGS->GROUP[0].PORT_PINCFG[27] = PORT_PINCFG_INEN_Msk;
}

static void clock_initialize(void)
{
    OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K = OSC32KCTRL_XOSC32K_ONDEMAND_Msk |
                                          OSC32KCTRL_XOSC32K_EN32K_Msk |
                                          OSC32KCTRL_XOSC32K_XTALEN_Msk |
                                          OSC32KCTRL_XOSC32K_ENABLE_Msk |
                                          OSC32KCTRL_XOSC32K_CGM(1U);
    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL_XOSC32K;
    MCLK_REGS->MCLK_CPUDIV = MCLK_CPUDIV_DIV(1U);

    gclk_generator(1U, GCLK_GENCTRL_SRC_DFLL, 1U);
    gclk_generator(5U, GCLK_GENCTRL_SRC_DFLL, 24U);

    GCLK_REGS->GCLK_PCHCTRL[OSCCTRL_GCLK_ID_FDPLL0] =
        GCLK_PCHCTRL_GEN_GCLK5 | GCLK_PCHCTRL_CHEN_Msk;
    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(0U) |
                                              OSCCTRL_DPLLRATIO_LDR(59U);
    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_REFCLK_GCLK;
    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLA = OSCCTRL_DPLLCTRLA_ENABLE_Msk;
    while ((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSTATUS &
            (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) !=
           (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) {
    }
    gclk_generator(0U, GCLK_GENCTRL_SRC_DPLL0, 1U);

    GCLK_REGS->GCLK_PCHCTRL[SERCOM3_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[SERCOM3_GCLK_ID_CORE] &
            GCLK_PCHCTRL_CHEN_Msk) == 0U) {
    }
    MCLK_REGS->MCLK_APBBMASK |= MCLK_APBBMASK_SERCOM3_Msk;
}

/* 115200 8N1, TX on PAD0, RX on PAD1 */
static void sercom3_usart_initialize(void)
{
    sercom_usart_int_registers_t *usart = VCOM_USART;

    usart->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                          SERCOM_USART_INT_CTRLA_RXPO(1U) |
                          SERCOM_USART_INT_CTRLA_TXPO(0U) |
                          SERCOM_USART_INT_CTRLA_DORD_Msk |
                          SERCOM_USART_INT_CTRLA_FORM(0U) |
                          SERCOM_USART_INT_CTRLA_SAMPR(0U);
    usart->SERCOM_BAUD =
        (uint16_t)SERCOM_USART_INT_BAUD_BAUD(SERCOM3_USART_INT_BAUD_VALUE);
    usart->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT |
                          SERCOM_USART_INT_CTRLB_SBMODE_1_BIT |
                          SERCOM_USART_INT_CTRLB_RXEN_Msk |
                          SERCOM_USART_INT_CTRLB_TXEN_Msk;
    while (usart->SERCOM_SYNCBUSY != 0U) {
    }
    usart->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    while (usart->SERCOM_SYNCBUSY != 0U) {
    }
}

static void nvic_initialize(void)
{
    __DMB();
    __enable_irq();
    NVIC_SetPriority(SERCOM3_0_IRQn, 3);
    NVIC_EnableIRQ(SERCOM3_0_IRQn);
}

void sys_initialize(void)
{
    /* Five wait states before 120 MHz, with automatic wait states */
    NVMCTRL_REGS->NVMCTRL_CTRLA = NVMCTRL_CTRLA_RWS(5U) |
                                  NVMCTRL_CTRLA_AUTOWS_Msk;
    port_initialize();
    clock_initialize();
    sercom3_usart_initialize();
    nvic_initialize();
}

/* The values sys_initialize() programmed, read back */
void report_steps(void)
{
    port_group_registers_t *pa = &PORT_REGS->GROUP[0];
    sercom_usart_int_registers_t *usart = VCOM_USART;

    console_puts("step 1 port:");
    put_checked("PA.PINCFG22", pa->PORT_PINCFG[22], 0x01U, 2);
    put_checked("PA.PMUX11", pa->PORT_PMUX[11], 0x22U, 2);
    put_checked("PB.DIR", LED0_GROUP->PORT_DIR, PORT_PB23, 8);
    console_puts("\r\n");

    console_puts("step 2 clock:");
    put_checked("GENCTRL0", GCLK_REGS->GCLK_GENCTRL[0], 0x00010107U, 8);
    put_checked("GENCTRL5", GCLK_REGS->GCLK_GENCTRL[5], 0x00180106U, 8);
    put_checked("DPLLSTATUS",
                OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSTATUS, 0x03U, 8);
    put_checked("XOSC32KRDY", OSC32KCTRL_REGS->OSC32KCTRL_STATUS &
                OSC32KCTRL_STATUS_XOSC32KRDY_Msk, 0x01U, 1);
    console_puts("\r\n");

    console_puts("step 3 sercom3:");
    put_checked("CTRLA", usart->SERCOM_CTRLA, 0x40100006U, 8);
    put_checked("BAUD", usart->SERCOM_BAUD, 63019U, 4);
    put_checked("CTRLB", usart->SERCOM_CTRLB, 0x00030000U, 8);
    console_puts("\r\n");

    console_puts("step 4 nvic:");
    put_checked("ISER1", NVIC->ISER[1], 1U << (58U - 32U), 8);
    console_puts("\r\n");
}
