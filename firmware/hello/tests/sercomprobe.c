/*
 * SERCOM I2C host and SPI host probe: raw observations for the twin.
 *
 * I2C: SERCOM2 on PA12 (SDA) / PA13 (SCL), the board's I2C bus with the
 * ATECC608A (U700). Bus state after enable, an address scan, the flags of
 * an ACKed and a NACKed address, the ATECC608A wake response and an Info
 * command.
 * SPI: SERCOM1 on PA16 (PAD0) / PA18 (PAD2) / PA19 (PAD3), MOSI on PAD2;
 * MISO from PAD2 too (loopback through the pad) and from PAD0 (open).
 * Prints values, checks nothing; used to decide what the twin models.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define I2C             (&SERCOM2_REGS->I2CM)
#define SPI             (&SERCOM1_REGS->SPIM)
#define MB              SERCOM_I2CM_INTFLAG_MB_Msk
#define SB              SERCOM_I2CM_INTFLAG_SB_Msk
#define ERR             SERCOM_I2CM_INTFLAG_ERROR_Msk
#define ATECC           0x60U

static void eol(void)
{
    console_puts("\r\n");
}

static void hex2(uint32_t v)
{
    static const char x[] = "0123456789ABCDEF";
    char b[3] = { x[(v >> 4) & 0xFU], x[v & 0xFU], 0 };

    console_puts(b);
}

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

/* Wait for MB, SB or ERROR (or 5 ms); returns INTFLAG */
static uint32_t i2c_wait(void)
{
    for (uint32_t us = 5000; us > 0U; us--) {
        uint32_t f = I2C->SERCOM_INTFLAG;

        if (f & (MB | SB | ERR)) {
            return f;
        }
        delay_us(1);
    }
    return 0xEE;
}

static void i2c_stop(void)
{
    I2C->SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(3U);
    i2c_sync();
}

static uint32_t i2c_start(uint8_t addr_rw)
{
    I2C->SERCOM_ADDR = addr_rw;
    i2c_sync();
    return i2c_wait();
}

static void i2c_status(const char *name, uint32_t flags)
{
    put_field(name, flags, 2);
    put_field("st", I2C->SERCOM_STATUS, 4);
}

/* Write @n bytes to @addr; returns the number ACKed, 0xFF: address NACK */
static uint32_t i2c_write(uint8_t addr, const uint8_t *buf, uint32_t n)
{
    uint32_t i;

    i2c_start((uint8_t)(addr << 1));
    if (I2C->SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) {
        i2c_stop();
        return 0xFF;
    }
    for (i = 0; i < n; i++) {
        I2C->SERCOM_DATA = buf[i];
        i2c_wait();
        if (I2C->SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) {
            break;
        }
    }
    i2c_stop();
    return i;
}

/* Read @n bytes from @addr (smart mode): 0, or 0xFF on address NACK */
static uint32_t i2c_read(uint8_t addr, uint8_t *buf, uint32_t n)
{
    uint32_t f = i2c_start((uint8_t)(addr << 1 | 1U));

    if (!(f & SB)) {
        i2c_stop();
        return 0xFF;
    }
    I2C->SERCOM_CTRLB &= ~SERCOM_I2CM_CTRLB_ACKACT_Msk;
    i2c_sync();
    for (uint32_t i = 0; i < n; i++) {
        if (i == n - 1U) {
            /* NACK the last byte and stop when DATA is read */
            I2C->SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_ACKACT_Msk |
                                 SERCOM_I2CM_CTRLB_CMD(3U);
            i2c_sync();
        }
        buf[i] = (uint8_t)I2C->SERCOM_DATA;
        if (i != n - 1U) {
            i2c_wait();
        }
    }
    i2c_sync();
    return 0;
}

/* ATECC CRC-16 (polynomial 0x8005, data bits LSB first) */
static uint16_t atca_crc(const uint8_t *d, uint32_t n)
{
    uint16_t crc = 0;

    for (uint32_t i = 0; i < n; i++) {
        for (unsigned int b = 0; b < 8U; b++) {
            unsigned int dbit = (d[i] >> b) & 1U, cbit = crc >> 15;

            crc = (uint16_t)(crc << 1);
            if (dbit != cbit) {
                crc ^= 0x8005U;
            }
        }
    }
    return crc;
}

static void test_i2c(void)
{
    uint8_t buf[8], cmd[8], atecc = ATECC;
    uint32_t r, found = 0;

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM2_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM2_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
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
    console_puts("i2c-init:");
    put_field("st", I2C->SERCOM_STATUS, 4);
    I2C->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE_Msk;
    i2c_sync();
    put_field("enabled-st", I2C->SERCOM_STATUS, 4);
    I2C->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1U);   /* force idle */
    i2c_sync();
    put_field("idle-st", I2C->SERCOM_STATUS, 4);
    put_field("flags", I2C->SERCOM_INTFLAG, 2);
    eol();

    /* NACKed write address (0x10), then the flags before the stop */
    console_puts("i2c-nack:");
    r = i2c_start(0x10U << 1);
    i2c_status("w-f", r);
    i2c_stop();
    put_field("stop-st", I2C->SERCOM_STATUS, 4);
    delay_us(100);
    put_field("stop-st-100us", I2C->SERCOM_STATUS, 4);
    put_field("stop-f", I2C->SERCOM_INTFLAG, 2);
    r = i2c_start(0x10U << 1 | 1U);
    i2c_status("r-f", r);
    i2c_stop();
    eol();

    console_puts("i2c-scan:");
    for (uint32_t a = 1; a < 0x78U; a++) {
        i2c_start((uint8_t)(a << 1));
        if (!(I2C->SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk)) {
            console_puts(" ");
            hex2(a);
            found++;
        }
        i2c_stop();
        delay_us(50);
    }
    put_field("count", found, 2);
    eol();

    /* Wake (SDA low > 60 us via address 0), then scan while awake */
    console_puts("wake-scan:");
    r = i2c_start(0x00);
    i2c_status("a0-f", r);
    i2c_stop();
    delay_ms(2);
    found = 0;
    for (uint32_t a = 1; a < 0x78U; a++) {
        i2c_start((uint8_t)(a << 1));
        if (!(I2C->SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk)) {
            console_puts(" ");
            hex2(a);
            if (a != 0x28U) {
                atecc = (uint8_t)a;
            }
        }
        i2c_stop();
        delay_us(50);
    }
    eol();

    console_puts("atecc-wake:");
    put_field("addr", atecc, 2);
    r = i2c_start(0x00);
    i2c_stop();
    delay_ms(2);
    r = i2c_read(atecc, buf, 4);
    put_field("rd", r, 2);
    console_puts(" resp=");
    for (unsigned int i = 0; i < 4U; i++) {
        hex2(buf[i]);
    }
    eol();

    /* Info (opcode 0x30, mode 0): count, opcode, mode, param2, CRC */
    console_puts("atecc-info:");
    cmd[0] = 0x03;                      /* word address: command */
    cmd[1] = 7;
    cmd[2] = 0x30;
    cmd[3] = 0x00;
    cmd[4] = 0x00;
    cmd[5] = 0x00;
    r = atca_crc(&cmd[1], 5);
    cmd[6] = (uint8_t)r;
    cmd[7] = (uint8_t)(r >> 8);
    r = i2c_write(atecc, cmd, 8);
    put_field("wr", r, 2);
    delay_ms(2);
    r = i2c_read(atecc, buf, 7);
    put_field("rd", r, 2);
    console_puts(" resp=");
    for (unsigned int i = 0; i < 7U; i++) {
        hex2(buf[i]);
    }
    put_field("crc-ok", atca_crc(buf, 5) == (uint16_t)(buf[5] | buf[6] << 8),
              1);
    cmd[0] = 0x01;                      /* sleep */
    put_field("sleep", i2c_write(atecc, cmd, 1), 2);
    eol();

    /* The EDBG data gateway (DGI) at 0x28: writes and reads */
    console_puts("dgi:");
    cmd[0] = 0x00;
    cmd[1] = 0x00;
    put_field("wr2", i2c_write(0x28, cmd, 2), 2);
    r = i2c_read(0x28, buf, 4);
    put_field("rd", r, 2);
    console_puts(" resp=");
    for (unsigned int i = 0; i < 4U; i++) {
        hex2(buf[i]);
    }
    /* Read-address flags: SB after the first byte, CLKHOLD held */
    r = i2c_start(0x28U << 1 | 1U);
    i2c_status("r-f", r);
    I2C->SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_ACKACT_Msk |
                         SERCOM_I2CM_CTRLB_CMD(3U);
    i2c_sync();
    put_field("stop-st", I2C->SERCOM_STATUS, 4);
    delay_us(200);
    put_field("st-200us", I2C->SERCOM_STATUS, 4);
    /* Write address ACKed: MB, then one data byte */
    r = i2c_start(0x28U << 1);
    i2c_status("w-f", r);
    I2C->SERCOM_DATA = 0x00;
    r = i2c_wait();
    i2c_status("d-f", r);
    i2c_stop();
    eol();

    I2C->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    i2c_sync();
}

static void spi_sync(void)
{
    while (SPI->SERCOM_SYNCBUSY != 0U) {
    }
}

static void spi_setup(uint32_t dipo, int rxen)
{
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_MODE_SPI_MASTER |
                        SERCOM_SPIM_CTRLA_DOPO(1U) |     /* DO PAD2, SCK PAD3 */
                        SERCOM_SPIM_CTRLA_DIPO(dipo);
    SPI->SERCOM_CTRLB = rxen ? SERCOM_SPIM_CTRLB_RXEN_Msk : 0U;
    SPI->SERCOM_BAUD = 23;                                /* 1 MHz */
    spi_sync();
    SPI->SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    spi_sync();
}

static uint32_t spi_xfer(uint8_t v)
{
    while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
    }
    SPI->SERCOM_DATA = v;
    for (uint32_t us = 1000; us > 0U; us--) {
        if (SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk) {
            return SPI->SERCOM_DATA;
        }
        delay_us(1);
    }
    return 0x1EE;
}

static void test_spi(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] &
             GCLK_PCHCTRL_CHEN_Msk)) {
    }
    pmux(16, 2);
    pmux(18, 2);
    pmux(19, 2);

    spi_setup(2, 1);                /* MISO from PAD2: the MOSI pad */
    console_puts("spi-loop:");
    put_field("f0", SPI->SERCOM_INTFLAG, 2);
    put_field("5A", spi_xfer(0x5A), 3);
    put_field("A5", spi_xfer(0xA5), 3);
    put_field("3C", spi_xfer(0x3C), 3);
    put_field("f", SPI->SERCOM_INTFLAG, 2);
    eol();

    spi_setup(0, 1);                /* MISO from PAD0 (PA16, open) */
    console_puts("spi-open:");
    put_field("5A", spi_xfer(0x5A), 3);
    put_field("A5", spi_xfer(0xA5), 3);
    eol();

    /* Three bytes without reading: overflow */
    spi_setup(2, 1);
    console_puts("spi-ovf:");
    for (unsigned int i = 0; i < 3U; i++) {
        while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
        }
        SPI->SERCOM_DATA = 0x11U * (i + 1U);
        delay_us(20);
    }
    put_field("f", SPI->SERCOM_INTFLAG, 2);
    put_field("st", SPI->SERCOM_STATUS, 4);
    put_field("d1", SPI->SERCOM_DATA, 3);
    put_field("st1", SPI->SERCOM_STATUS, 4);
    put_field("d2", SPI->SERCOM_DATA, 3);
    put_field("st2", SPI->SERCOM_STATUS, 4);
    put_field("d3", SPI->SERCOM_DATA, 3);
    put_field("f2", SPI->SERCOM_INTFLAG, 2);
    eol();

    spi_setup(2, 0);                /* receiver off */
    console_puts("spi-norx:");
    SPI->SERCOM_DATA = 0x77;
    delay_us(20);
    put_field("f", SPI->SERCOM_INTFLAG, 2);
    put_field("d", SPI->SERCOM_DATA, 3);
    eol();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP sercomprobe: " BOARD_BANNER "\r\n");
    test_i2c();
    test_spi();
    test_exit();
}
