/*
 * Nano Base AN and socket-2 UART check: a POT Click on mikroBUS socket 1
 * of the Curiosity Nano Base (AN = PD4, ADC0 AIN4) and a GNSS Click on
 * socket 2 (its UART is USART1 on PC0/PC1, the same lines as the virtual
 * COM port; DS50003494A Figure 7-5). The GNSS speaks 9600 baud, so USART1
 * runs at 9600 for both: open the VCOM at 9600. Each line names a check:
 *
 *   an1        AIN4 against the 2.500 V reference: the POT's 0-2.048 V
 *              (half way, 1.024 V, on a fresh twin)
 *   uart2      an NMEA RMC sentence from the GNSS within 3 s
 *
 * Ends with "PP:EXIT n", n the failed checks, then prints the POT's
 * millivolts once a second. What the firmware prints also reaches the
 * GNSS module (it ignores lines that are not $PMTK), as on the board.
 * Time comes from TCB0 (1 ms at CLK_PER), not from busy loops: the twin
 * runs instructions, not cycles, so _delay_ms() is not to scale there.
 *
 *   qemu-system-avr -M avr64ea48-cnano,mb1=pot,mb2=gnss
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "board.h"
#include <string.h>

#define BAUD_VALUE(b)   ((uint16_t)((4UL * F_CPU + (b) / 2U) / (b)))

static uint8_t fails;

/* TCB0 periodic, 1 ms at CLK_PER: a millisecond has passed when CAPT */
static void ms_init(void)
{
    TCB0.CCMP = F_CPU / 1000U - 1U;
    TCB0.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
}

static uint8_t ms_tick(void)
{
    if (!(TCB0.INTFLAGS & TCB_CAPT_bm)) {
        return 0;
    }
    TCB0.INTFLAGS = TCB_CAPT_bm;
    return 1;
}

static void ms_wait(uint16_t ms)
{
    while (ms) {
        ms -= ms_tick();
    }
}

static void check(const char *name, uint8_t ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static void put_dec(uint16_t v)
{
    char buf[6];
    uint8_t n = 0;

    do {
        buf[n++] = (char)('0' + v % 10U);
        v /= 10U;
    } while (v);
    while (n) {
        console_putc(buf[--n]);
    }
}

/* AN of socket 1 in millivolts */
static uint16_t an1_mv(void)
{
    ADC0.MUXPOS = ADC_MUXPOS_AIN4_gc;
    ADC0.COMMAND = ADC_MODE_SINGLE_12BIT_gc | ADC_START_IMMEDIATE_gc;
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm)) {
    }
    return (uint16_t)((uint32_t)ADC0.RESULT * 2500UL / 4095UL);
}

/*
 * The next NMEA line from USART1 RX within the @ms left (counted down);
 * 0 once they run out. A line cut off by the time out is lost.
 */
static uint8_t gnss_line(char *buf, uint8_t size, uint16_t *ms)
{
    uint8_t n = 0;

    while (*ms) {
        char c;

        *ms -= ms_tick();
        if (!(USART1.STATUS & USART_RXCIF_bm)) {
            continue;
        }
        c = (char)USART1.RXDATAL;
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            buf[n] = 0;
            return 1;
        }
        if (n < size - 1U) {
            buf[n++] = c;
        }
    }
    return 0;
}

int main(void)
{
    char line[96];
    uint16_t mv, left;
    uint8_t rmc = 0;

    console_init();
    USART1.BAUD = BAUD_VALUE(9600UL);       /* the GNSS's rate, for both */
    USART1.CTRLB = USART_TXEN_bm | USART_RXEN_bm;
    console_puts("PP nanobase: " BOARD_BANNER "\r\n");
    ms_init();

    PORTD.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;     /* analog */
    ADC0.CTRLB = ADC_PRESC_DIV4_gc;
    ADC0.CTRLC = ADC_REFSEL_2V500_gc;
    ADC0.CTRLE = 16;
    ADC0.CTRLA = ADC_ENABLE_bm;
    ms_wait(1);                             /* reference settling */

    mv = an1_mv();
    console_puts("an1 ");
    put_dec(mv);
    console_puts(" mV\r\n");
    check("an1", mv >= 1000U && mv <= 1050U);

    left = 3000;
    while (!rmc && gnss_line(line, sizeof(line), &left)) {
        rmc = line[0] == '$' && !strncmp(line + 3, "RMC", 3);
    }
    if (rmc) {
        console_puts("gnss ");
        console_puts(line);
        console_puts("\r\n");
    }
    check("uart2", rmc);

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
        ms_wait(1000);
        console_puts("an1 ");
        put_dec(an1_mv());
        console_puts(" mV\r\n");
    }
}
