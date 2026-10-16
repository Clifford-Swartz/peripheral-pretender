/*
 * LR 2 Click check for the twin and the web board view: the Click on EXT1
 * (through a mikroBUS Xplained Pro adapter), its RN2903 LoRa module on
 * the headers' UART, SERCOM3 (PA22 TX, PA23 RX), 57600 8N1; RESET on RST
 * (PA20), UART_RTS on CS (PA17, driven high as MikroElektronika's driver
 * does). Each line names a check:
 *
 *   boot       a reset pulse brings the version line
 *   version    sys get ver answers the same
 *   keys       an OTAA join without keys answers keys_not_init
 *   join       with DevEUI, AppEUI and AppKey set: ok, then accepted
 *   uplink     mac tx uncnf 1 "Hello": ok, then mac_tx_ok (or mac_rx)
 *   p2p        MikroElektronika's set-up (mac pause, radio set wdt 0) and
 *              its transmitter's packet, radio tx "MikroE\r\n": ok, then
 *              radio_tx_ok
 *
 * Then "PP:EXIT n", n the failed checks, and a loop: the P2P packet,
 * three seconds listening for the other radio ("received: <text>"), and
 * every fourth time an uplink ("downlink: <text>" if the network had one).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <string.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"

/* EV15N46A only: the pins and peripherals below are that board's */
#ifndef _PIC32CM1216MC00048_H_
#error "lr2.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define PA              (&PORT_REGS->GROUP[0])
#define LORA            (&SERCOM3_REGS->USART_INT)
#define RST_PIN         20U     /* PA20 */
#define CS_PIN          17U     /* PA17 */
#define BAUD_57600      64278U  /* 65536 x (1 - 16 x 57600 / 48 MHz) */

static unsigned int fails;

static void check(const char *name, int ok)
{
    console_puts(name);
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static void lora_uart_init(void)
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
    LORA->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                         SERCOM_USART_INT_CTRLA_RXPO(1U) |
                         SERCOM_USART_INT_CTRLA_TXPO(0U) |
                         SERCOM_USART_INT_CTRLA_DORD_Msk;
    LORA->SERCOM_BAUD = (uint16_t)BAUD_57600;
    LORA->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT |
                         SERCOM_USART_INT_CTRLB_RXEN_Msk |
                         SERCOM_USART_INT_CTRLB_TXEN_Msk;
    while (LORA->SERCOM_SYNCBUSY != 0U) {
    }
    LORA->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    while (LORA->SERCOM_SYNCBUSY != 0U) {
    }
}

static void lora_send(const char *s)
{
    while (*s) {
        while (!(LORA->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {
        }
        LORA->SERCOM_DATA = (uint8_t)*s++;
    }
}

/* One reply line (CR LF dropped) within @ms; 0 on a timeout */
static int lora_line(char *buf, unsigned int size, uint32_t ms)
{
    unsigned int n = 0;

    for (uint32_t t = 0; t < ms * 10U; t++) {
        int c;

        if (!(LORA->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk)) {
            delay_us(100);
            continue;
        }
        c = LORA->SERCOM_DATA;
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            buf[n] = 0;
            return 1;
        }
        if (n < size - 1U) {
            buf[n++] = (char)c;
        }
    }
    buf[n] = 0;
    return 0;
}

/* A command and its first reply, both shown on VCOM */
static const char *lora_cmd(const char *cmd, uint32_t ms)
{
    static char reply[160];

    console_puts("> ");
    console_puts(cmd);
    console_puts("\r\n");
    lora_send(cmd);
    lora_send("\r\n");
    if (!lora_line(reply, sizeof(reply), ms)) {
        strcpy(reply, "(no answer)");
    }
    console_puts("< ");
    console_puts(reply);
    console_puts("\r\n");
    return reply;
}

/* The second reply of a command that answered "ok" */
static const char *lora_wait(uint32_t ms)
{
    static char reply[160];

    if (!lora_line(reply, sizeof(reply), ms)) {
        strcpy(reply, "(no answer)");
    }
    console_puts("< ");
    console_puts(reply);
    console_puts("\r\n");
    return reply;
}

static void put_hex_text(const char *hex)
{
    while (hex[0] && hex[1]) {
        unsigned int v = 0;

        for (int i = 0; i < 2; i++) {
            char c = hex[i];

            v = v << 4 | (unsigned int)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
        }
        if (v >= 0x20U && v < 0x7FU) {
            console_putc((char)v);
        }
        hex += 2;
    }
}

int main(void)
{
    char line[160];
    unsigned int round = 0;

    sys_initialize();
    systick_start();
    console_puts("PP lr2: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = 1U << RST_PIN;    /* high before it drives */
    PA->PORT_DIRSET = (1U << RST_PIN) | (1U << CS_PIN);
    lora_uart_init();

    /* MikroElektronika's reset: high 100 ms, low 300 ms, high, CS high */
    delay_ms(100);
    PA->PORT_OUTCLR = 1U << RST_PIN;
    delay_ms(300);
    PA->PORT_OUTSET = 1U << RST_PIN;
    PA->PORT_OUTSET = 1U << CS_PIN;
    check("boot", lora_line(line, sizeof(line), 1000) &&
                  !strncmp(line, "RN2903 ", 7));
    console_puts("< ");
    console_puts(line);
    console_puts("\r\n");
    check("version", !strncmp(lora_cmd("sys get ver", 500), "RN2903 ", 7));

    check("keys", !strcmp(lora_cmd("mac join otaa", 500), "keys_not_init"));
    lora_cmd("mac set deveui 0004A30B001A2B3C", 500);
    lora_cmd("mac set appeui 70B3D57ED0000001", 500);
    lora_cmd("mac set appkey 2B7E151628AED2A6ABF7158809CF4F3C", 500);
    check("join", !strcmp(lora_cmd("mac join otaa", 500), "ok") &&
                  !strcmp(lora_wait(10000), "accepted"));
    if (!strcmp(lora_cmd("mac tx uncnf 1 48656C6C6F", 500), "ok")) {
        const char *r = lora_wait(10000);

        check("uplink", !strcmp(r, "mac_tx_ok") || !strncmp(r, "mac_rx ", 7));
    } else {
        check("uplink", 0);
    }

    lora_cmd("mac pause", 500);
    lora_cmd("radio set wdt 0", 500);
    check("p2p", !strcmp(lora_cmd("radio tx 4D696B726F450D0A", 500), "ok") &&
                 !strcmp(lora_wait(5000), "radio_tx_ok"));

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    lora_cmd("radio set wdt 3000", 500);
    for (;; round++) {
        const char *r;

        if (round % 4U == 3U) {
            lora_cmd("mac resume", 500);
            if (!strcmp(lora_cmd("mac tx uncnf 1 70696E67", 500), "ok")) {
                r = lora_wait(10000);
                if (!strncmp(r, "mac_rx 1 ", 9)) {
                    console_puts("downlink: ");
                    put_hex_text(r + 9);
                    console_puts("\r\n");
                }
            }
            lora_cmd("mac pause", 500);
            continue;
        }
        if (!strcmp(lora_cmd("radio tx 4D696B726F450D0A", 500), "ok")) {
            lora_wait(5000);
        }
        if (!strcmp(lora_cmd("radio rx 0", 500), "ok")) {
            r = lora_wait(5000);
            if (!strncmp(r, "radio_rx  ", 10)) {
                console_puts("received: ");
                put_hex_text(r + 10);
                console_puts("\r\n");
            }
        }
    }
}
