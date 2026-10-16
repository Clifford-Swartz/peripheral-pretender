/*
 * GNSS Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter), its Quectel L86 on the headers' UART,
 * SERCOM3 (PA22 TX to the module's RX, PA23 RX from its TX), 9600 8N1;
 * RESET on RST (PA20, driven high), FORCE_ON on CS (left to the Click's
 * pull-up). Echoes the module's boot lines, asks for RMC and GGA only
 * (PMTK314) and prints the acknowledgement and the first RMC sentence,
 * then reports each GGA: the
 * position, altitude and satellites, or that it waits for a fix. LED0
 * toggles with every fix. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=gnss
 *
 * The parsing follows MikroElektronika's GNSS Click example: it looks for
 * "$GPGGA" (the L86 keeps the GP talker on GGA with a fix) and takes the
 * latitude, longitude and altitude fields.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <string.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "gnss.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define GNSS            (&SERCOM3_REGS->USART_INT)
#define RST_PIN         20U     /* PA20 */
#define LED0_PIN        24U     /* PA24, active low */
#define BAUD_9600       65326U  /* 65536 x (1 - 16 x 9600 / 48 MHz) */

static void gnss_uart_init(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM3_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SERCOM3_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SERCOM3_GCLK_ID_CORE] &
             GCLK_PCHCTRL_CHEN_Msk)) {
    }
    /* PA22 = PAD0 (TX), PA23 = PAD1 (RX): function C */
    PA->PORT_PINCFG[22] = PORT_PINCFG_PMUXEN_Msk;
    PA->PORT_PINCFG[23] = PORT_PINCFG_PMUXEN_Msk;
    PA->PORT_PMUX[11] = (uint8_t)(PORT_PMUX_PMUXE_C | PORT_PMUX_PMUXO_C);
    GNSS->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                         SERCOM_USART_INT_CTRLA_RXPO(1U) |
                         SERCOM_USART_INT_CTRLA_TXPO(0U) |
                         SERCOM_USART_INT_CTRLA_DORD_Msk;
    GNSS->SERCOM_BAUD = (uint16_t)BAUD_9600;
    GNSS->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT |
                         SERCOM_USART_INT_CTRLB_RXEN_Msk |
                         SERCOM_USART_INT_CTRLB_TXEN_Msk;
    while (GNSS->SERCOM_SYNCBUSY != 0U) {
    }
    GNSS->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    while (GNSS->SERCOM_SYNCBUSY != 0U) {
    }
}

static void gnss_send(const char *s)
{
    while (*s) {
        while (!(GNSS->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        }
        GNSS->SERCOM_DATA = (uint8_t)*s++;
    }
}

/* One NMEA line (without CR LF) into @buf; returns its length */
static unsigned gnss_line(char *buf, unsigned size)
{
    unsigned n = 0;

    for (;;) {
        char c;

        while (!(GNSS->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk)) {
        }
        c = (char)GNSS->SERCOM_DATA;
        if (c == '\n') {
            buf[n] = 0;
            return n;
        }
        if (c != '\r' && n < size - 1U) {
            buf[n++] = c;
        }
    }
}

/* Field @idx (0: the talker) of a comma-separated sentence, in place */
static const char *field(char *line, unsigned idx, char *out, unsigned size)
{
    const char *p = line;
    unsigned n = 0;

    while (idx && *p) {
        if (*p++ == ',') {
            idx--;
        }
    }
    while (*p && *p != ',' && *p != '*' && n < size - 1U) {
        out[n++] = *p++;
    }
    out[n] = 0;
    return out;
}

int main(void)
{
    char line[96], lat[16], ns[4], lon[16], ew[4], alt[12], sats[4];
    unsigned boot = 0, rmc_shown = 0;

    sys_initialize();
    systick_start();
    console_puts("PP gnss: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = (1U << LED0_PIN) | (1U << RST_PIN);
    PA->PORT_DIRSET = (1U << LED0_PIN) | (1U << RST_PIN);
    gnss_uart_init();

    for (;;) {
        gnss_line(line, sizeof(line));
        if (!strncmp(line, "$PMTK010,", 9)) {
            console_puts("module: ");
            console_puts(line);
            console_puts("\r\n");
            if (++boot == 2) {          /* booted: RMC and GGA only */
                gnss_send("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"
                          "*28\r\n");
            }
        } else if (!strncmp(line, "$PMTK001,", 9)) {
            console_puts("ack: ");
            console_puts(line);
            console_puts("\r\n");
        } else if ((!strncmp(line, "$GNRMC,", 7) ||
                    !strncmp(line, "$GPRMC,", 7)) && !rmc_shown) {
            console_puts("rmc: ");          /* one whole sentence */
            console_puts(line);
            console_puts("\r\n");
            rmc_shown = 1;
        } else if (!strncmp(line, "$GPGGA,", 7)) {
            field(line, 2, lat, sizeof(lat));
            if (!lat[0]) {
                console_puts("waiting for a fix\r\n");
                continue;
            }
            field(line, 3, ns, sizeof(ns));
            field(line, 4, lon, sizeof(lon));
            field(line, 5, ew, sizeof(ew));
            field(line, 7, sats, sizeof(sats));
            field(line, 9, alt, sizeof(alt));
            console_puts("fix lat ");
            console_puts(lat);
            console_puts(" ");
            console_puts(ns);
            console_puts(" lon ");
            console_puts(lon);
            console_puts(" ");
            console_puts(ew);
            console_puts(" alt ");
            console_puts(alt);
            console_puts(" m sats ");
            console_puts(sats);
            console_puts("\r\n");
            PA->PORT_OUTTGL = 1U << LED0_PIN;
        }
    }
}
