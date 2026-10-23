/*
 * Scrolling text on the 8x8 G Click: "MICROCHIP" runs right to left across
 * the MAX7219's 8x8 green LED matrix, in mikroBUS socket 1 of the Curiosity
 * Nano Base (SPI0, CS on PA7). The same image runs on the AVR64EA48
 * Curiosity Nano and on its twin (avr64ea48-cnano, mb1=8x8g).
 *
 * MAX7219 data sheet: 16-bit frames, register in D11..D8, data in D7..D0,
 * latched by the rising edge of LOAD (CS). As in matrix.c, digit 7 is the
 * left column and bit 7 of a digit the top row (MikroElektronika's 8x8 G
 * Click schematic v100).
 *
 * VCOM (USART1, 115200 8N1): a banner and "PP:EXIT 0" once the matrix is
 * set up, then the text scrolls forever, a column every 80 ms (TCB0).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define REG_DIGIT0      0x01
#define REG_DECODE      0x09
#define REG_INTENSITY   0x0A
#define REG_SCAN_LIMIT  0x0B
#define REG_SHUTDOWN    0x0C
#define REG_TEST        0x0F
#define STEP_MS         80U
#define GAP             8U              /* blank columns after the text */

static const char message[] = "MICROCHIP";

/* 5x7 glyphs, a column a byte, LSB the top row */
static const uint8_t font[][5] PROGMEM = {
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F },   /* M */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 },   /* I */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 },   /* C */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 },   /* R */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E },   /* O */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F },   /* H */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 },   /* P */
};
static const char letters[] = "MICROHP";

static volatile uint16_t ticks;

ISR(TCB0_INT_vect)
{
    TCB0.INTFLAGS = TCB_CAPT_bm;
    ticks++;
}

static void spi_init(void)
{
    PORTA.DIRSET = PIN4_bm | PIN6_bm;               /* MOSI, SCK */
    MB1_CS_PORT.OUTSET = MB1_CS_bm;                 /* LOAD idle high */
    MB1_CS_PORT.DIRSET = MB1_CS_bm;
    SPI0.CTRLB = SPI_SSD_bm;                        /* CS is a GPIO */
    SPI0.CTRLA = SPI_MASTER_bm | SPI_PRESC_DIV4_gc | SPI_ENABLE_bm;
}

static void spi_byte(uint8_t b)
{
    SPI0.DATA = b;
    while (!(SPI0.INTFLAGS & SPI_IF_bm)) {
    }
    (void)SPI0.DATA;
}

static void max7219_write(uint8_t reg, uint8_t data)
{
    MB1_CS_PORT.OUTCLR = MB1_CS_bm;
    spi_byte(reg);
    spi_byte(data);
    MB1_CS_PORT.OUTSET = MB1_CS_bm;                 /* latch */
}

/* Bit 0 (the font's top row) to bit 7 (the matrix's top row) */
static uint8_t flip(uint8_t v)
{
    uint8_t r = 0;

    for (uint8_t i = 0; i < 8; i++) {
        r = (uint8_t)((r << 1) | (v & 1U));
        v >>= 1;
    }
    return r;
}

/* Column @n of the text strip: 6 per letter (5 and a space), then a gap */
static uint8_t strip_column(uint16_t n)
{
    uint16_t letter = n / 6U;
    uint8_t col = (uint8_t)(n % 6U);

    if (letter >= sizeof(message) - 1U || col == 5U) {
        return 0;
    }
    for (uint8_t g = 0; g < sizeof(letters) - 1U; g++) {
        if (letters[g] == message[letter]) {
            return flip(pgm_read_byte(&font[g][col]));
        }
    }
    return 0;
}

/* Show 8 columns of the strip from @start, left to right */
static void show(uint16_t start, uint16_t len)
{
    for (uint8_t i = 0; i < 8; i++) {
        max7219_write(REG_DIGIT0 + 7 - i, strip_column((start + i) % len));
    }
}

int main(void)
{
    const uint16_t len = (sizeof(message) - 1U) * 6U + GAP;
    uint16_t pos = 0, last = 0;

    console_init();
    spi_init();
    max7219_write(REG_TEST, 0);
    max7219_write(REG_SCAN_LIMIT, 7);
    max7219_write(REG_DECODE, 0);
    max7219_write(REG_INTENSITY, 4);
    max7219_write(REG_SHUTDOWN, 1);
    show(pos, len);

    console_puts("PP scroll: \"MICROCHIP\" on the 8x8 G Click, mikroBUS 1\r\n"
                 "PP:EXIT 0\r\n");
    console_flush();

    TCB0.CCMP = F_CPU / 1000U - 1U;                 /* 1 ms */
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
    sei();
    for (;;) {
        uint16_t t;

        cli();
        t = ticks;
        sei();
        if ((uint16_t)(t - last) < STEP_MS) {
            continue;
        }
        last = t;
        pos = (pos + 1U) % len;
        show(pos, len);
    }
}
