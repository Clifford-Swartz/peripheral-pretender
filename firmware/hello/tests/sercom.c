/*
 * SERCOM I2C host and SPI host test (PIC32CM MC00).
 *
 * I2C: SERCOM2 on PA12/PA13, the board's I2C bus; the only device on it is
 * the EDBG data gateway at 0x28 (the ATECC608A footprint, U700, is not
 * fitted). Bus state after enable and after forcing IDLE, the flags of a
 * NACKed write and read address, the stop (OWNER, then IDLE), a scan, the
 * general call, writes and reads at 0x28.
 * SPI: SERCOM1, MOSI on PAD2 (PA18), 1 MHz. MISO from PAD2 is a loopback
 * through the pad; from PAD0 (PA16, nothing connected) it reads 0xFF. The
 * receive buffer holds two characters; a third is lost and STATUS.BUFOVF
 * and INTFLAG.ERROR show when the buffer runs empty.
 *
 * Expected values are PIC32CM MC00 silicon (tests/sercomprobe.c).
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
#define RXNACK          SERCOM_I2CM_STATUS_RXNACK_Msk
#define DGI             0x28U

#define ST_IDLE         0x0010U
#define ST_OWNER        0x0020U
#define ST_CLKHOLD      0x0080U

static void eol(void)
{
    console_puts("\r\n");
}

static void pmux(unsigned int pin, uint8_t func)
{
    PORT_REGS->GROUP[0].PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_PMUX[pin / 2U] =
        (PORT_REGS->GROUP[0].PORT_PMUX[pin / 2U] &
         ((pin & 1U) ? 0x0FU : 0xF0U)) |
        (uint8_t)(func << ((pin & 1U) * 4U));
}

static void clock_on(unsigned int id)
{
    GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN_GCLK0 |
                                  GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
}

/* -- I2C --------------------------------------------------------------- */

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

static void test_i2c_bus(void)
{
    uint32_t f, st;

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM2_Msk;
    clock_on(SERCOM2_GCLK_ID_CORE);
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
    console_puts("i2c-bus:");
    put_checked("enabled", I2C->SERCOM_STATUS, 0, 4);      /* UNKNOWN */
    I2C->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1U);
    i2c_sync();
    put_checked("idle", I2C->SERCOM_STATUS, ST_IDLE, 4);
    put_checked("flags", I2C->SERCOM_INTFLAG, 0, 2);
    eol();

    console_puts("i2c-nack:");
    f = i2c_start(0x10U << 1);
    put_checked("w-flags", f, MB, 2);
    put_checked("w-st", I2C->SERCOM_STATUS,
                ST_CLKHOLD | ST_OWNER | RXNACK, 4);
    i2c_stop();
    st = I2C->SERCOM_STATUS;
    put_checked("stop-flags", I2C->SERCOM_INTFLAG, 0, 2);
    put_checked("stop-st", st, ST_OWNER | RXNACK, 4);
    delay_us(100);
    put_checked("idle-st", I2C->SERCOM_STATUS, ST_IDLE | RXNACK, 4);
    f = i2c_start(0x10U << 1 | 1U);
    put_checked("r-flags", f, MB, 2);           /* MB, not SB */
    put_checked("r-st", I2C->SERCOM_STATUS,
                ST_CLKHOLD | ST_OWNER | RXNACK, 4);
    i2c_stop();
    f = i2c_start(0x00);                        /* general call */
    put_checked("gc-st", I2C->SERCOM_STATUS,
                ST_CLKHOLD | ST_OWNER | RXNACK, 4);
    i2c_stop();
    eol();
}

static void test_i2c_scan(void)
{
    uint32_t found = 0, first = 0;

    for (uint32_t a = 1; a < 0x78U; a++) {
        i2c_start((uint8_t)(a << 1));
        if (!(I2C->SERCOM_STATUS & RXNACK)) {
            found++;
            first = first ? first : a;
        }
        i2c_stop();
        delay_us(50);
    }
    console_puts("i2c-scan:");
    put_checked("devices", found, 1, 2);
    put_checked("addr", first, DGI, 2);
    eol();
}

static void test_i2c_dgi(void)
{
    uint32_t f, b[3];

    console_puts("i2c-dgi:");
    f = i2c_start(DGI << 1);
    put_checked("w-flags", f, MB, 2);
    put_checked("w-st", I2C->SERCOM_STATUS, ST_CLKHOLD | ST_OWNER, 4);
    I2C->SERCOM_DATA = 0x00;
    f = i2c_wait();
    put_checked("d-flags", f, MB, 2);
    put_checked("d-st", I2C->SERCOM_STATUS, ST_CLKHOLD | ST_OWNER, 4);
    I2C->SERCOM_DATA = 0x5A;
    f = i2c_wait();
    put_checked("d2-ack", I2C->SERCOM_STATUS & RXNACK, 0, 1);
    i2c_stop();

    /* Read three bytes in smart mode: SB per byte, NACK + stop at the end */
    f = i2c_start(DGI << 1 | 1U);
    put_checked("r-flags", f, SB, 2);
    put_checked("r-st", I2C->SERCOM_STATUS, ST_CLKHOLD | ST_OWNER, 4);
    I2C->SERCOM_CTRLB &= ~SERCOM_I2CM_CTRLB_ACKACT_Msk;
    i2c_sync();
    b[0] = I2C->SERCOM_DATA;
    f = i2c_wait();
    b[1] = I2C->SERCOM_DATA;
    i2c_wait();
    I2C->SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_ACKACT_Msk |
                         SERCOM_I2CM_CTRLB_CMD(3U);
    i2c_sync();
    b[2] = I2C->SERCOM_DATA;
    put_checked("next-flags", f, SB, 2);
    put_checked("data", b[0] | b[1] << 8 | b[2] << 16, 0, 6);
    put_checked("stop-st", I2C->SERCOM_STATUS, ST_IDLE, 4);
    delay_us(200);
    put_checked("idle-st", I2C->SERCOM_STATUS, ST_IDLE, 4);
    eol();
    I2C->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    i2c_sync();
}

/* -- SPI ----------------------------------------------------------------- */

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
    uint32_t a, b, c, f;

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;
    clock_on(SERCOM1_GCLK_ID_CORE);
    pmux(16, 2);
    pmux(18, 2);
    pmux(19, 2);

    spi_setup(2, 1);
    f = SPI->SERCOM_INTFLAG;
    a = spi_xfer(0x5A);
    b = spi_xfer(0xA5);
    c = spi_xfer(0x3C);
    console_puts("spi-loop:");
    put_checked("enabled-flags", f, SERCOM_SPIM_INTFLAG_DRE_Msk, 2);
    put_checked("data", a | b << 8 | c << 16, 0x3CA55A, 6);
    put_checked("flags", SPI->SERCOM_INTFLAG, SERCOM_SPIM_INTFLAG_DRE_Msk |
                SERCOM_SPIM_INTFLAG_TXC_Msk, 2);
    eol();

    spi_setup(0, 1);
    a = spi_xfer(0x5A);
    b = spi_xfer(0xA5);
    console_puts("spi-open:");
    put_checked("data", a | b << 8, 0xFFFF, 4);
    eol();

    spi_setup(2, 1);
    for (unsigned int i = 0; i < 3U; i++) {
        while (!(SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk)) {
        }
        SPI->SERCOM_DATA = 0x11U * (i + 1U);
        delay_us(20);
    }
    console_puts("spi-ovf:");
    put_checked("flags", SPI->SERCOM_INTFLAG, 0x07, 2);
    put_checked("st", SPI->SERCOM_STATUS, 0, 4);
    put_checked("d1", SPI->SERCOM_DATA, 0x11, 2);
    put_checked("st1", SPI->SERCOM_STATUS, 0, 4);
    put_checked("d2", SPI->SERCOM_DATA, 0x22, 2);
    put_checked("st2", SPI->SERCOM_STATUS, SERCOM_SPIM_STATUS_BUFOVF_Msk, 4);
    /* A 0 takes the lost character's place, with RXC */
    put_checked("flags2", SPI->SERCOM_INTFLAG, 0x87, 2);
    put_checked("d3", SPI->SERCOM_DATA, 0, 2);
    put_checked("flags3", SPI->SERCOM_INTFLAG, 0x83, 2);
    eol();

    spi_setup(2, 0);
    SPI->SERCOM_DATA = 0x77;
    delay_us(20);
    console_puts("spi-norx:");
    put_checked("flags", SPI->SERCOM_INTFLAG, SERCOM_SPIM_INTFLAG_DRE_Msk |
                SERCOM_SPIM_INTFLAG_TXC_Msk, 2);
    eol();
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_SWRST_Msk;
    spi_sync();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP sercom: " BOARD_BANNER "\r\n");
    test_i2c_bus();
    test_i2c_scan();
    test_i2c_dgi();
    test_spi();
    test_exit();
}
