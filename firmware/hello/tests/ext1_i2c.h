/*
 * I2C host on the EV15N46A extension headers (SERCOM2, PA12 SDA, PA13 SCL,
 * smart mode), shared by the I2C Click demos (accel.c, rtc6.c).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef EXT1_I2C_H
#define EXT1_I2C_H

#include "console.h"
#include "sys_init.h"
#include "util.h"

#ifndef _PIC32CM1216MC00048_H_
#error "ext1_i2c.h is written for BOARD=pic32cm_mc00_cpro"
#endif

#define I2C             (&SERCOM2_REGS->I2CM)
#define I2C_MB          SERCOM_I2CM_INTFLAG_MB_Msk
#define I2C_SB          SERCOM_I2CM_INTFLAG_SB_Msk
#define I2C_ERR         SERCOM_I2CM_INTFLAG_ERROR_Msk
#define I2C_RXNACK      SERCOM_I2CM_STATUS_RXNACK_Msk

static void i2c_pmux(unsigned int pin, uint8_t func)
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

        if (f & (I2C_MB | I2C_SB | I2C_ERR)) {
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
    i2c_pmux(12, 2);
    i2c_pmux(13, 2);
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

/* Write @reg, then @n bytes; 0 if the target acknowledged its address */
static int i2c_write(uint8_t addr, uint8_t reg, const uint8_t *d,
                     unsigned int n)
{
    I2C->SERCOM_ADDR = (uint32_t)addr << 1;
    i2c_sync();
    if (!(i2c_wait() & I2C_MB) || (I2C->SERCOM_STATUS & I2C_RXNACK)) {
        i2c_stop();
        return -1;
    }
    I2C->SERCOM_DATA = reg;
    i2c_wait();
    for (unsigned int i = 0; i < n; i++) {
        I2C->SERCOM_DATA = d[i];
        i2c_wait();
    }
    i2c_stop();
    return 0;
}

/* Read @n bytes from @reg on; 0 if the target answered */
static int __attribute__((unused))
i2c_read(uint8_t addr, uint8_t reg, uint8_t *d, unsigned int n)
{
    if (i2c_write(addr, reg, 0, 0) != 0) {
        return -1;
    }
    I2C->SERCOM_ADDR = (uint32_t)addr << 1 | 1U;
    i2c_sync();
    if (!(i2c_wait() & I2C_SB)) {
        i2c_stop();
        return -1;
    }
    for (unsigned int i = 0; i < n; i++) {
        if (i == n - 1U) {
            I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                                SERCOM_I2CM_CTRLB_ACKACT_Msk;
            i2c_sync();
        }
        d[i] = (uint8_t)I2C->SERCOM_DATA;       /* smart mode: next byte */
        if (i != n - 1U) {
            i2c_wait();
        }
    }
    i2c_stop();
    return 0;
}

static int __attribute__((unused))
i2c_write1(uint8_t addr, uint8_t reg, uint8_t v)
{
    return i2c_write(addr, reg, &v, 1);
}

#endif /* EXT1_I2C_H */
