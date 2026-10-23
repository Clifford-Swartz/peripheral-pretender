/*
 * LCD mini Click demo for the web board view: on EXT1 (through a mikroBUS
 * Xplained Pro adapter), SERCOM1 SPI to an MCP23S17 (CS# on PA17, RESET#
 * on RST, PA20) whose port B drives the 16x2 LCD in 4-bit mode (RS GPB2,
 * E GPB3, DB4-DB7 GPB4-GPB7), an MCP4161 for the contrast (CS# on AN,
 * PB08) and the backlight on PWM (PB10, high: on). Writes two lines, then
 * counts the seconds on the second. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=lcdmini
 *
 * The LCD's R/W is tied low, so it is never read: the demo waits the
 * ST7066U's execution times instead of polling the busy flag. Contrast
 * as MikroElektronika's example sets it: 200, written as wiper 255 - 200.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_spi.h"

#define IODIRB          0x01U
#define OLATB           0x15U
#define LCD_RS          0x04U   /* GPB2 */
#define LCD_E           0x08U   /* GPB3 */
#define CONTRAST        200U

static void expander_write(uint8_t reg, uint8_t v)
{
    cs_low();
    spi_xfer(0x40);                                       /* opcode, write */
    spi_xfer(reg);
    spi_xfer(v);
    cs_high();
}

/* DB7-DB4 = @nibble with RS = @rs, latched by E's falling edge */
static void lcd_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t v = (uint8_t)(nibble << 4) | rs;

    expander_write(OLATB, v | LCD_E);
    expander_write(OLATB, v);
}

static void lcd_byte(uint8_t b, uint8_t rs)
{
    lcd_nibble(b >> 4, rs);
    lcd_nibble(b & 0x0FU, rs);
    delay_us(50);                                         /* 37 us */
}

static void lcd_cmd(uint8_t c)
{
    lcd_byte(c, 0);
    if (c < 0x04U) {
        delay_ms(2);                                      /* clear, home */
    }
}

static void lcd_at(unsigned int line, unsigned int col, const char *s)
{
    lcd_cmd((uint8_t)(0x80U | (line ? 0x40U : 0U) | col));
    while (*s) {
        lcd_byte((uint8_t)*s++, LCD_RS);
    }
}

static void contrast(uint8_t value)
{
    PB->PORT_OUTCLR = 1U << AN_PIN;
    spi_xfer(0x00);                                       /* wiper 0, write */
    spi_xfer((uint8_t)(255U - value));
    PB->PORT_OUTSET = 1U << AN_PIN;
}

int main(void)
{
    char clock[] = "00:00";

    sys_initialize();
    systick_start();
    console_puts("PP lcdmini: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = 1U << RST_PIN;
    PA->PORT_DIRSET = 1U << RST_PIN;
    PB->PORT_OUTSET = (1U << AN_PIN) | (1U << PWM_PIN);   /* backlight on */
    PB->PORT_DIRSET = (1U << AN_PIN) | (1U << PWM_PIN);
    spi_init();
    expander_write(OLATB, 0x00);
    expander_write(IODIRB, 0x00);
    contrast(CONTRAST);

    /* The data sheet's 4-bit initialisation: 3, 3, 3, then 2 */
    delay_ms(40);
    lcd_nibble(0x3, 0);
    delay_ms(5);
    lcd_nibble(0x3, 0);
    delay_us(150);
    lcd_nibble(0x3, 0);
    delay_us(50);
    lcd_nibble(0x2, 0);
    delay_us(50);
    lcd_cmd(0x28);                                        /* 4 bits, 2 lines */
    lcd_cmd(0x08);                                        /* display off */
    lcd_cmd(0x01);                                        /* clear */
    lcd_cmd(0x06);                                        /* increment */
    lcd_cmd(0x0C);                                        /* display on */
    lcd_at(0, 3, "Peripheral");
    lcd_at(1, 0, "Pretender");
    console_puts("LCD: Peripheral / Pretender\r\n");

    for (uint32_t s = 0;; s++) {
        clock[0] = (char)('0' + s / 600U % 10U);
        clock[1] = (char)('0' + s / 60U % 10U);
        clock[3] = (char)('0' + s % 60U / 10U);
        clock[4] = (char)('0' + s % 10U);
        lcd_at(1, 11, clock);
        delay_ms(1000);
    }
}
