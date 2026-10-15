/*
 * Data Gateway demo: what a Data Visualizer example does on this kit.
 * Every 10 ms it sends a Data Stream packet (start 0x03, a 16-bit
 * counter, little-endian, end 0xFC) to the EDBG's DGI over I2C (address
 * 0x28) and toggles DGI GPIO0 (PA10), so Data Visualizer can plot the
 * counter and show the GPIO's edges. Reports on VCOM once a second.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"
#include "ext1_i2c.h"

#define DGI_I2C_ADDR    0x28U
#define DGI_GPIO0_PIN   10U                         /* PA10 */

int main(void)
{
    uint16_t count = 0;

    (void)i2c_read;                                 /* shared helpers */
    (void)i2c_write1;
    sys_initialize();
    systick_start();
    console_puts("PP dgi: " BOARD_BANNER "\r\n");
    console_puts("Data Stream packets to the DGI over I2C; GPIO0 toggles\r\n");
    i2c_init();
    PORT_REGS->GROUP[0].PORT_DIRSET = 1U << DGI_GPIO0_PIN;

    for (;;) {
        uint8_t frame[3] = { count & 0xFFU, count >> 8, 0xFCU };
        int err = i2c_write(DGI_I2C_ADDR, 0x03U, frame, sizeof(frame));

        PORT_REGS->GROUP[0].PORT_OUTTGL = 1U << DGI_GPIO0_PIN;
        if (count % 100U == 0U) {
            console_puts("sent");
            put_field("count", count, 4);
            console_puts(err ? " NACK\r\n" : "\r\n");
        }
        count++;
        delay_ms(10);
    }
}
