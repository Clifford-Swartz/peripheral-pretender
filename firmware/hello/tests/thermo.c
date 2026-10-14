/*
 * Thermo 8 Click demo for the web board view: an MCP9808 temperature
 * sensor on EXT1 (I2C on SERCOM2 PA12/PA13, ALERT on the INT pin PB02).
 * Sets TUPPER 30 C, TLOWER 10 C and TCRIT 80 C with the alert output on
 * (comparator, active low), then reports the temperature on the VCOM port
 * whenever it changes. LED0 follows the ALERT line. Runs forever.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "thermo.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define I2C             (&SERCOM2_REGS->I2CM)
#define MB              SERCOM_I2CM_INTFLAG_MB_Msk
#define SB              SERCOM_I2CM_INTFLAG_SB_Msk
#define ERR             SERCOM_I2CM_INTFLAG_ERROR_Msk
#define RXNACK          SERCOM_I2CM_STATUS_RXNACK_Msk

#define MCP9808         0x18U
#define REG_CONFIG      1U
#define REG_TUPPER      2U
#define REG_TLOWER      3U
#define REG_TCRIT       4U
#define REG_TA          5U
#define REG_MANUF       6U

/* EXT1 pin 9 (INT): PB02 */
#define INT_GROUP       (&PORT_REGS->GROUP[1])
#define INT_PIN         2U

static void pmux(unsigned int pin, uint8_t func)
{
    PORT_REGS->GROUP[0].PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_PMUX[pin / 2U] =
        (PORT_REGS->GROUP[0].PORT_PMUX[pin / 2U] &
         ((pin & 1U) ? 0x0FU : 0xF0U)) |
        (uint8_t)(func << ((pin & 1U) * 4U));
}

static void i2c_sync(void)
{
    while (I2C->SERCOM_SYNCBUSY != 0U) {
    }
}

static uint32_t i2c_wait(void)
{
    for (uint32_t us = 5000; us > 0U; us--) {
        uint32_t f = I2C->SERCOM_INTFLAG;

        if (f & (MB | SB | ERR)) {
            return f;
        }
        delay_us(1);
    }
    return 0;
}

static void i2c_stop(void)
{
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                        SERCOM_I2CM_CTRLB_ACKACT_Msk |
                        SERCOM_I2CM_CTRLB_CMD(3U);
    i2c_sync();
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;
    i2c_sync();
}

static void i2c_init(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM2_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM2_GCLK_ID_CORE] = GCLK_PCHCTRL_GEN_GCLK0 |
                                                   GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SERCOM2_GCLK_ID_CORE] &
             GCLK_PCHCTRL_CHEN_Msk)) {
    }
    pmux(12, 2);
    pmux(13, 2);
    I2C->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    i2c_sync();
    I2C->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
                        SERCOM_I2CM_CTRLA_SDAHOLD(2U);
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;
    I2C->SERCOM_BAUD = SERCOM_I2CM_BAUD_BAUD(235U);     /* ~100 kHz */
    i2c_sync();
    I2C->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE_Msk;
    i2c_sync();
    I2C->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1U);  /* IDLE */
    i2c_sync();
}

/* Write the register pointer, then n bytes; 0 if the sensor answered */
static int mcp_write(uint8_t reg, uint16_t val, unsigned int n)
{
    I2C->SERCOM_ADDR = MCP9808 << 1;
    i2c_sync();
    if (!(i2c_wait() & MB) || (I2C->SERCOM_STATUS & RXNACK)) {
        i2c_stop();
        return -1;
    }
    I2C->SERCOM_DATA = reg;
    i2c_wait();
    for (unsigned int i = n; i > 0U; i--) {
        I2C->SERCOM_DATA = (uint8_t)(val >> ((i - 1U) * 8U));
        i2c_wait();
    }
    i2c_stop();
    return 0;
}

/* A 16-bit register, MSB first; -1 if the sensor does not answer */
static int32_t mcp_read(uint8_t reg)
{
    uint32_t v;

    if (mcp_write(reg, 0, 0) != 0) {
        return -1;
    }
    I2C->SERCOM_ADDR = MCP9808 << 1 | 1U;
    i2c_sync();
    if (!(i2c_wait() & SB)) {
        i2c_stop();
        return -1;
    }
    v = I2C->SERCOM_DATA << 8;                  /* ACK, next byte */
    i2c_wait();
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                        SERCOM_I2CM_CTRLB_ACKACT_Msk;
    i2c_sync();
    v |= I2C->SERCOM_DATA;                      /* NACK */
    i2c_stop();
    return (int32_t)v;
}

/* TA bits 12:0: two's complement in 1/16 C */
static void put_temp(uint32_t ta)
{
    int32_t t = (int32_t)((ta & 0x1FFFU) << 19) >> 19;
    uint32_t a = (uint32_t)(t < 0 ? -t : t);

    console_puts(" T=");
    console_putc(t < 0 ? '-' : '+');
    console_put_dec(a / 16U);
    console_putc('.');
    console_put_dec((a % 16U) * 625U / 1000U);
    console_put_dec((a % 16U) * 625U / 100U % 10U);
    console_puts(" C");
}

static int alert_on(void)
{
    return (INT_GROUP->PORT_IN & (1U << INT_PIN)) == 0U;
}

int main(void)
{
    uint32_t last = 0xFFFFFFFFU;

    sys_initialize();
    systick_start();
    console_puts("PP thermo: " BOARD_BANNER "\r\n");

    LED0_GROUP->PORT_OUTSET = LED0_MASK;            /* off (active low) */
    LED0_GROUP->PORT_DIRSET = LED0_MASK;
    INT_GROUP->PORT_DIRCLR = 1U << INT_PIN;
    INT_GROUP->PORT_OUTSET = 1U << INT_PIN;         /* pull-up */
    INT_GROUP->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk |
                                      PORT_PINCFG_PULLEN_Msk;
    i2c_init();

    while (mcp_read(REG_MANUF) != 0x0054) {
        console_puts("no Thermo 8 Click on EXT1 (MCP9808 at 0x18)\r\n");
        LED0_GROUP->PORT_OUTTGL = LED0_MASK;
        delay_ms(2000);
    }
    console_puts("Thermo 8 Click found; alert above 30 C or below 10 C\r\n");
    mcp_write(REG_TUPPER, 30U * 16U, 2);
    mcp_write(REG_TLOWER, 10U * 16U, 2);
    mcp_write(REG_TCRIT, 80U * 16U, 2);
    mcp_write(REG_CONFIG, 0x0008U, 2);              /* alert on, comparator */

    for (;;) {
        int32_t ta = mcp_read(REG_TA);
        int alert = alert_on();
        uint32_t now = (uint32_t)ta | (alert ? 0x10000U : 0U);

        if (alert) {
            LED0_GROUP->PORT_OUTCLR = LED0_MASK;
        } else {
            LED0_GROUP->PORT_OUTSET = LED0_MASK;
        }
        if (ta >= 0 && now != last) {
            last = now;
            put_temp((uint32_t)ta);
            if (ta & 0x8000) {
                console_puts(" CRITICAL");
            } else if (ta & 0x4000) {
                console_puts(" HIGH");
            } else if (ta & 0x2000) {
                console_puts(" LOW");
            }
            console_puts(alert ? " alert\r\n" : "\r\n");
        }
        delay_ms(200);
    }
}
