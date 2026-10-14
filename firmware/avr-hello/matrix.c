/*
 * 8x8 G Click demo: a MAX7219 driving an 8x8 green LED matrix, in mikroBUS
 * socket 1 of the Curiosity Nano Base (SPI0, CS on PA7). Draws a smiley,
 * reports the frame on the VCOM port and prints "PP:EXIT 0", then scrolls
 * "AVR" across the matrix forever (TCB0 sets the pace).
 *
 * MAX7219 data sheet: 16-bit frames, register in D11..D8, data in D7..D0,
 * latched by the rising edge of LOAD (CS). The 8x8 G Click's schematic
 * (MikroElektronika v100) draws digit 7 as the left column and segments DP,
 * A..G as the rows top to bottom; the physical LED order is not confirmed.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <avr/interrupt.h>

#define REG_DIGIT0      0x01
#define REG_DECODE      0x09
#define REG_INTENSITY   0x0A
#define REG_SCAN_LIMIT  0x0B
#define REG_SHUTDOWN    0x0C
#define REG_TEST        0x0F

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

/* Show 8 columns, left to right (bit 7 = top row): digit 7 is the left */
static void show(const uint8_t *cols)
{
    for (uint8_t i = 0; i < 8; i++) {
        max7219_write(REG_DIGIT0 + 7 - i, cols[i]);
    }
}

static const uint8_t smiley[8] = {
    0x3C, 0x42, 0xA9, 0x85, 0x85, 0xA9, 0x42, 0x3C,
};

/* "AVR " as 5x7 columns, bit 7 = top row */
static const uint8_t text[] = {
    0x7E, 0x90, 0x90, 0x90, 0x7E, 0x00,
    0xF8, 0x04, 0x02, 0x04, 0xF8, 0x00,
    0xFE, 0x90, 0x98, 0x94, 0x62, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int main(void)
{
    uint8_t frame[8];
    uint8_t pos = 0;
    uint16_t last = 0;

    console_init();
    spi_init();
    max7219_write(REG_TEST, 0);
    max7219_write(REG_SCAN_LIMIT, 7);
    max7219_write(REG_DECODE, 0);
    max7219_write(REG_INTENSITY, 4);
    max7219_write(REG_SHUTDOWN, 1);
    show(smiley);

    console_puts("PP matrix: 8x8 G Click on mikroBUS 1 of the Nano Base\r\n"
                 "frame:");
    for (uint8_t i = 0; i < 8; i++) {
        console_putc(' ');
        console_hex2(smiley[i]);
    }
    console_puts("\r\nPP:EXIT 0\r\n");
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
        if ((uint16_t)(t - last) < 120U) {
            continue;
        }
        last = t;
        for (uint8_t i = 0; i < 8; i++) {
            frame[i] = text[(pos + i) % sizeof(text)];
        }
        show(frame);
        pos = (pos + 1) % sizeof(text);
    }
}
