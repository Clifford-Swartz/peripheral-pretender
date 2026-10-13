/*
 * Harmony SYS_Initialize replica for the PIC32CM MC00 Curiosity Pro.
 *
 * Performs, access for access, what MPLAB Harmony generates for the board's
 * csp_apps_pic32cm_mc00 usart_echo_interrupt example (configuration
 * pic32cm_mc00_curiosity_pro): same registers, values, access widths and
 * order, including the unbounded busy-waits.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"

/* 65536 * (1 - 16 * 115200 / 48 MHz), truncated the way MCC does. */
#define SERCOM0_USART_INT_BAUD_VALUE 63019U

/* PORT_Initialize: PA24 output high (LED0 off), PA06/PA07 -> SERCOM0. */
static void port_initialize(void)
{
    PORT_REGS->GROUP[0].PORT_DIR = 0x1000000U;
    PORT_REGS->GROUP[0].PORT_OUT = 0x1000000U;
    PORT_REGS->GROUP[0].PORT_PINCFG[6] = 0x1U;
    PORT_REGS->GROUP[0].PORT_PINCFG[7] = 0x1U;
    PORT_REGS->GROUP[0].PORT_PINCFG[24] = 0x0U;

    PORT_REGS->GROUP[0].PORT_PMUX[3] =
        PORT_PMUX_PMUXE(MUX_PA06D_SERCOM0_PAD2) |
        PORT_PMUX_PMUXO(MUX_PA07D_SERCOM0_PAD3);
    PORT_REGS->GROUP[0].PORT_PMUX[12] = 0x0U;
}

/* CAL48M from bits 40:19 of the calibration row, then OSC48M at /1. */
static void oscctrl_initialize(void)
{
    uint32_t calib =
        (uint32_t)((*(const uint64_t *)OTP5_ADDR >> 19) & 0x3FFFFFU);

    OSCCTRL_REGS->OSCCTRL_CAL48M = calib;
    OSCCTRL_REGS->OSCCTRL_OSC48MDIV = OSCCTRL_OSC48MDIV_DIV(0U);
    while ((OSCCTRL_REGS->OSCCTRL_OSC48MSYNCBUSY &
            OSCCTRL_OSC48MSYNCBUSY_OSC48MDIV_Msk) != 0U) {
    }
    while ((OSCCTRL_REGS->OSCCTRL_STATUS &
            OSCCTRL_STATUS_OSC48MRDY_Msk) == 0U) {
    }
    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL |= OSCCTRL_OSC48MCTRL_ONDEMAND_Msk;
}

static void osc32kctrl_initialize(void)
{
    OSC32KCTRL_REGS->OSC32KCTRL_OSC32K = 0U;
    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL(0U);
}

static void gclk0_initialize(void)
{
    GCLK_REGS->GCLK_GENCTRL[0] = GCLK_GENCTRL_DIV(1U) |
                                 GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_GENEN_Msk;
    while ((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL0_Msk) != 0U) {
    }
}

/* CLOCK_Initialize: 48 MHz CPU, GCLK0 to SERCOM0_CORE. */
static void clock_initialize(void)
{
    oscctrl_initialize();
    osc32kctrl_initialize();
    gclk0_initialize();

    GCLK_REGS->GCLK_PCHCTRL[SERCOM0_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[SERCOM0_GCLK_ID_CORE] &
            GCLK_PCHCTRL_CHEN_Msk) == 0U) {
    }

    MCLK_REGS->MCLK_APBCMASK = MCLK_APBCMASK_SERCOM0_Msk;
}

/* NVMCTRL_Initialize: two wait states for 48 MHz. */
static void nvmctrl_initialize(void)
{
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_CACHEDIS_CACHE_DF_DIS_MAIN_EN |
                                  NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY |
                                  NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS |
                                  NVMCTRL_CTRLB_RWS(2U) |
                                  NVMCTRL_CTRLB_MANW_Msk;
}

/* SERCOM0_USART_Initialize: 115200 8N1, TX on PAD2, RX on PAD3. */
static void sercom0_usart_initialize(void)
{
    sercom_usart_int_registers_t *usart = VCOM_USART;

    usart->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                          SERCOM_USART_INT_CTRLA_RXPO(3U) |
                          SERCOM_USART_INT_CTRLA_TXPO(1U) |
                          SERCOM_USART_INT_CTRLA_DORD_Msk |
                          SERCOM_USART_INT_CTRLA_IBON_Msk |
                          SERCOM_USART_INT_CTRLA_FORM(0U) |
                          SERCOM_USART_INT_CTRLA_SAMPR(0U);
    usart->SERCOM_BAUD =
        (uint16_t)SERCOM_USART_INT_BAUD_BAUD(SERCOM0_USART_INT_BAUD_VALUE);
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

/* NVIC_Initialize: interrupts on, SERCOM0 (IRQ9) at priority 3. */
static void nvic_initialize(void)
{
    __DMB();
    __enable_irq();
    NVIC_SetPriority(SERCOM0_IRQn, 3);
    NVIC_EnableIRQ(SERCOM0_IRQn);
}

void sys_initialize(void)
{
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3U);
    port_initialize();
    clock_initialize();
    nvmctrl_initialize();
    /* EVSYS_Initialize() has nothing to do in this configuration. */
    sercom0_usart_initialize();
    nvic_initialize();
}

/*
 * One line per SYS_Initialize step, read back afterwards so that the
 * sequence itself stays identical to Harmony's. The expected values are
 * the ones the Harmony example generates.
 */
void report_steps(void)
{
    port_group_registers_t *pa = &PORT_REGS->GROUP[0];
    sercom_usart_int_registers_t *usart = VCOM_USART;

    console_puts("step 3 nvmctrl: CTRLB <- RWS(3)\r\n");

    console_puts("step 4 port:");
    put_checked("PA.PINCFG6", pa->PORT_PINCFG[6], 0x01U, 2);
    put_checked("PA.PINCFG7", pa->PORT_PINCFG[7], 0x01U, 2);
    put_checked("PA.PMUX3", pa->PORT_PMUX[3], 0x33U, 2);
    put_checked("PA.DIR", LED0_GROUP->PORT_DIR, 0x01000000U, 8);
    console_puts("\r\n");

    console_puts("step 5 clock:");
    put_field("CAL48M", OSCCTRL_REGS->OSCCTRL_CAL48M, 8);
    put_checked("OSC48MDIV", OSCCTRL_REGS->OSCCTRL_OSC48MDIV, 0x00U, 2);
    put_checked("GENCTRL0", GCLK_REGS->GCLK_GENCTRL[0], 0x00010106U, 8);
    put_checked("PCHCTRL19", GCLK_REGS->GCLK_PCHCTRL[SERCOM0_GCLK_ID_CORE],
                0x00000040U, 8);
    put_checked("APBCMASK", MCLK_REGS->MCLK_APBCMASK, 0x00000002U, 8);
    console_puts("\r\n");

    console_puts("step 6 nvmctrl:");
    put_checked("CTRLB", NVMCTRL_REGS->NVMCTRL_CTRLB, 0x00000084U, 8);
    console_puts("\r\n");

    console_puts("step 7 sercom0:");
    put_checked("CTRLA", usart->SERCOM_CTRLA, 0x40310106U, 8);
    put_checked("BAUD", usart->SERCOM_BAUD, 63019U, 4);
    put_checked("CTRLB", usart->SERCOM_CTRLB, 0x00030000U, 8);
    console_puts("\r\n");

    console_puts("step 8 nvic:");
    put_checked("ISER", NVIC->ISER[0], 0x00000200U, 8);
    put_checked("IPR2", NVIC->IPR[2], 0x0000C000U, 8);
    console_puts("\r\n");
}
