/*
 * RN4871 Click demo for the web board view: the Click on EXT1 (through a
 * mikroBUS Xplained Pro adapter), its RN4871 Bluetooth LE module on the
 * headers' UART, SERCOM3 (PA22 TX, PA23 RX), 115200 8N1; RST_N on RST
 * (PA20). Brings the module up the way MikroElektronika's rn4871 driver
 * does (reset, "$" then "$$$", factory reset SF,1, Transparent UART
 * SS,C0, reboot R,1, command mode again, ends with ---), printing what
 * the module answers. Then it waits for a phone: once the stream is open,
 * every line the phone sends is printed and answered with "echo: <line>".
 * LED0 lights while connected. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=rn4871
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <string.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "rn4871.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define BLE             (&SERCOM3_REGS->USART_INT)
#define RST_PIN         20U     /* PA20 */
#define LED0_PIN        24U     /* PA24, active low */
#define BAUD_115200     63019U  /* 65536 x (1 - 16 x 115200 / 48 MHz) */

static void ble_uart_init(void)
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
    BLE->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                        SERCOM_USART_INT_CTRLA_RXPO(1U) |
                        SERCOM_USART_INT_CTRLA_TXPO(0U) |
                        SERCOM_USART_INT_CTRLA_DORD_Msk;
    BLE->SERCOM_BAUD = (uint16_t)BAUD_115200;
    BLE->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT |
                        SERCOM_USART_INT_CTRLB_RXEN_Msk |
                        SERCOM_USART_INT_CTRLB_TXEN_Msk;
    while (BLE->SERCOM_SYNCBUSY != 0U) {
    }
    BLE->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    while (BLE->SERCOM_SYNCBUSY != 0U) {
    }
}

static void ble_send(const char *s)
{
    while (*s) {
        while (!(BLE->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        }
        BLE->SERCOM_DATA = (uint8_t)*s++;
    }
}

static int ble_byte(void)
{
    if (!(BLE->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk)) {
        return -1;
    }
    return BLE->SERCOM_DATA;
}

/*
 * The module's output for @ms milliseconds onto VCOM, one line per reply
 * and per %status% message ("module: ...")
 */
static void show_for(uint32_t ms)
{
    static int at_start = 1, in_status;

    for (uint32_t t = 0; t < ms * 10U; t++) {
        int c = ble_byte();

        if (c < 0) {
            delay_us(100);
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '%' && in_status) {
            console_puts("%\r\n");
            in_status = 0;
            at_start = 1;
            continue;
        }
        if (at_start && c != '\n') {
            console_puts("module: ");
            at_start = 0;
        }
        if (c == '%' && !in_status) {
            in_status = 1;
        }
        if (c == '\n') {
            if (!at_start) {
                console_puts("\r\n");
            }
            at_start = 1;
        } else {
            console_putc((char)c);
        }
    }
    if (!at_start) {
        console_puts("\r\n");
        at_start = 1;
    }
}

static void command_mode(void)
{
    ble_send("$");
    delay_ms(100);
    ble_send("$$$");
}

int main(void)
{
    char line[64];
    unsigned int n = 0, connected = 0;
    char status[24];
    unsigned int ns = 0, in_status = 0;

    sys_initialize();
    systick_start();
    console_puts("PP rn4871: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = 1U << LED0_PIN;
    PA->PORT_DIRSET = (1U << LED0_PIN) | (1U << RST_PIN);
    ble_uart_init();

    PA->PORT_OUTCLR = 1U << RST_PIN;            /* reset pulse */
    delay_ms(100);
    PA->PORT_OUTSET = 1U << RST_PIN;
    show_for(200);
    command_mode();
    show_for(200);
    console_puts("> V\r\n");
    ble_send("V\r");
    show_for(100);
    console_puts("> SF,1\r\n");
    ble_send("SF,1\r");
    show_for(500);
    command_mode();
    show_for(100);
    console_puts("> SS,C0\r\n");
    ble_send("SS,C0\r");
    show_for(300);
    console_puts("> R,1\r\n");
    ble_send("R,1\r");
    show_for(500);
    command_mode();
    show_for(200);
    console_puts("> D\r\n");
    ble_send("D\r");
    show_for(200);
    ble_send("---\r");
    show_for(100);
    console_puts("waiting for a phone\r\n");

    /* Data mode: %status% messages, and lines from the phone */
    for (;;) {
        int c = ble_byte();

        if (c < 0) {
            continue;
        }
        if (c == '%') {
            if (in_status) {
                status[ns] = 0;
                console_puts("status: ");
                console_puts(status);
                console_puts("\r\n");
                if (!strncmp(status, "STREAM_OPEN", 11)) {
                    connected = 1;
                    PA->PORT_OUTCLR = 1U << LED0_PIN;
                } else if (!strncmp(status, "DISCONNECT", 10)) {
                    connected = 0;
                    PA->PORT_OUTSET = 1U << LED0_PIN;
                }
            }
            in_status = !in_status;
            ns = 0;
            continue;
        }
        if (in_status) {
            if (ns < sizeof(status) - 1U) {
                status[ns++] = (char)c;
            }
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (n && connected) {
                line[n] = 0;
                console_puts("phone: ");
                console_puts(line);
                console_puts("\r\n");
                ble_send("echo: ");
                ble_send(line);
                ble_send("\r\n");
            }
            n = 0;
        } else if (n < sizeof(line) - 1U) {
            line[n++] = (char)c;
        }
    }
}
