/*
 * Expand 2 Click demo for the web board view: an MCP23017 on EXT1 (through
 * a mikroBUS Xplained Pro adapter) at I2C 0x20 on SERCOM2, RESET# on RST
 * (PA20, held high), INTA on INT (PB02). PB0-PB6: inputs with the
 * pull-ups on and interrupt-on-change; port A: outputs that light the pins
 * of port B held low (PAn high while PBn is low). IOCON.MIRROR ties both
 * ports' interrupts to INTA, so the demo reads port B when INT falls (and
 * every half second) and prints the pins that are low. Runs forever.
 *
 *   -M pic32cm-mc00-cpro,ext1=expand2
 *
 * Registers with IOCON.BANK = 0 (MCP23017 data sheet DS20001952D table
 * 3-3); reading GPIOB clears port B's interrupt. GPA7 and GPB7 are outputs
 * only on the MCP23017 (section 3.5.1), so PB7 stays an output, low.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "ext1_i2c.h"

#define MCP23017        0x20U
#define IODIRA          0x00U
#define IODIRB          0x01U
#define GPINTENB        0x05U
#define INTCONB         0x09U
#define IOCON           0x0AU
#define GPPUB           0x0DU
#define GPIOB           0x13U
#define OLATA           0x14U
#define IOCON_MIRROR    0x40U
#define INPUTS          0x7FU   /* PB0-PB6 */

#define PA              (&PORT_REGS->GROUP[0])
#define PB              (&PORT_REGS->GROUP[1])
#define RST_PIN         20U     /* PA20 */
#define INT_PIN         2U      /* PB02 */

int main(void)
{
    uint8_t b = INPUTS, shown = 0;
    unsigned int quiet = 0;

    sys_initialize();
    systick_start();
    console_puts("PP expand2: " BOARD_BANNER "\r\n");
    PA->PORT_OUTSET = 1U << RST_PIN;
    PA->PORT_DIRSET = 1U << RST_PIN;
    PB->PORT_PINCFG[INT_PIN] = PORT_PINCFG_INEN_Msk;
    i2c_init();
    delay_ms(1);
    if (i2c_write1(MCP23017, IOCON, IOCON_MIRROR) != 0) {
        console_puts("no MCP23017 at 0x20\r\n");
        for (;;) {
        }
    }
    i2c_write1(MCP23017, IODIRA, 0x00);
    i2c_write1(MCP23017, OLATA, 0x00);
    i2c_write1(MCP23017, IODIRB, INPUTS);
    i2c_write1(MCP23017, GPPUB, INPUTS);
    i2c_write1(MCP23017, INTCONB, 0x00);                  /* any change */
    i2c_write1(MCP23017, GPINTENB, INPUTS);
    console_puts("hold one of PB0-PB6 low\r\n");

    for (;;) {
        bool irq = !((PB->PORT_IN >> INT_PIN) & 1U);

        if (irq || ++quiet >= 50U) {
            quiet = 0;
            i2c_read(MCP23017, GPIOB, &b, 1);
            b &= INPUTS;
            i2c_write1(MCP23017, OLATA, (uint8_t)(~b & INPUTS));
        }
        if (b != shown) {
            console_puts("low:");
            for (unsigned int n = 0; n < 7U; n++) {
                if (!(b >> n & 1U)) {
                    console_puts(" PB");
                    console_putc((char)('0' + n));
                }
            }
            console_puts(b == INPUTS ? " none\r\n" : "\r\n");
            shown = b;
        }
        delay_ms(10);
    }
}
