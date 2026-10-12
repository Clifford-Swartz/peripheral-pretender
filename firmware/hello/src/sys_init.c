/*
 * Harmony SYS_Initialize replica for the PIC32CM JH01 Curiosity Pro.
 *
 * Performs, access for access, what MPLAB Harmony generates for the board's
 * csp_apps usart_echo_interrupt example (docs/research/boot-path.md, table
 * "Pre-main register touches", steps 3-8): same registers, values, access
 * widths and order, including the unbounded busy-waits.  The DFP macros
 * only give the generated literals a name.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "sys_init.h"

/* 65536 * (1 - 16 * 115200 / 48 MHz), truncated the way MCC does. */
#define SERCOM4_USART_INT_BAUD_VALUE 63019U

/* Step 4, PORT_Initialize: PB10/PB11 -> SERCOM4 PAD2/PAD3, PC05 output. */
static void port_initialize(void)
{
    PORT_REGS->GROUP[1].PORT_PINCFG[10] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PINCFG[11] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PMUX[5] =
        PORT_PMUX_PMUXE(MUX_PB10D_SERCOM4_PAD2) |
        PORT_PMUX_PMUXO(MUX_PB11D_SERCOM4_PAD3);

    PORT_REGS->GROUP[2].PORT_DIR = PORT_PC05;
    PORT_REGS->GROUP[2].PORT_PINCFG[5] = 0U;
    PORT_REGS->GROUP[2].PORT_PMUX[2] = 0U;
}

/*
 * Step 5, OSCCTRL_Initialize as generated before Harmony stopped loading
 * CAL48M on this family: CAL48M comes from bits 40:19 of a 64-bit read of
 * the software calibration row, then OSC48M is switched from /12 to /1.
 */
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

/* Step 5, CLOCK_Initialize: 48 MHz CPU, GCLK0 to SERCOM4_CORE. */
static void clock_initialize(void)
{
    oscctrl_initialize();
    osc32kctrl_initialize();
    gclk0_initialize();

    GCLK_REGS->GCLK_PCHCTRL[SERCOM4_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[SERCOM4_GCLK_ID_CORE] &
            GCLK_PCHCTRL_CHEN_Msk) == 0U) {
    }

    MCLK_REGS->MCLK_APBCMASK = MCLK_APBCMASK_SMBIST_Msk |
                               MCLK_APBCMASK_SERCOM4_Msk;
}

/* Step 6, NVMCTRL_Initialize: two wait states for 48 MHz. */
static void nvmctrl_initialize(void)
{
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_CACHEDIS_CACHE_DF_DIS_MAIN_EN |
                                  NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY |
                                  NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS |
                                  NVMCTRL_CTRLB_RWS(2U) |
                                  NVMCTRL_CTRLB_MANW_Msk;
}

/* Step 7, SERCOM4_USART_Initialize: 115200 8N1, TX on PAD2, RX on PAD3. */
static void sercom4_usart_initialize(void)
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
        (uint16_t)SERCOM_USART_INT_BAUD_BAUD(SERCOM4_USART_INT_BAUD_VALUE);
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

/* Step 8, NVIC_Initialize: interrupts on, SERCOM4 (IRQ13) at priority 3. */
static void nvic_initialize(void)
{
    __DMB();
    __enable_irq();
    NVIC_SetPriority(SERCOM4_IRQn, 3);
    NVIC_EnableIRQ(SERCOM4_IRQn);
}

void sys_initialize(void)
{
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3U);    /* step 3 */
    port_initialize();
    clock_initialize();
    nvmctrl_initialize();
    /* EVSYS_Initialize() has nothing to do in this configuration. */
    sercom4_usart_initialize();
    nvic_initialize();
}
