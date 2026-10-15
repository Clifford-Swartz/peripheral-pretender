/*
 * Peripheral Pretender hello image (board: boards/<BOARD>/).
 *
 * Brings the chip up exactly as MPLAB Harmony does, reports the result
 * over the EDBG VCOM, blinks LED0 and ends with "PP:EXIT <n>", where n is
 * the number of failed checks.  Runs unchanged on silicon and on the twin.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include <stdbool.h>
#include <stdint.h>

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define IRQ_PRINT_TIMEOUT_MS    100U
#define BLINK_COUNT             3U
#define BLINK_HALF_PERIOD_MS    200U

/* The 128-bit serial number (DS 10.3 / 8.3); the DFP does not name it. */
/* The four serial-number words; boards on other families say where */
#ifndef BOARD_SERIAL_ADDRS
#define BOARD_SERIAL_ADDRS  { 0x0080A00CU, 0x0080A040U, 0x0080A044U, \
                              0x0080A048U }
#endif
/* The calibration row printed as calrow (OTP5 on the PIC32CM) */
#ifndef BOARD_CALROW_ADDR
#define BOARD_CALROW_ADDR   OTP5_ADDR
#endif

static const uintptr_t serial_number_addr[] = BOARD_SERIAL_ADDRS;

/* Identity and factory data; only the DID's device bits are checked. */
static void report_identity(void)
{
    const volatile uint32_t *cal_row =
        (const volatile uint32_t *)BOARD_CALROW_ADDR;
    uint32_t did = DSU_REGS->DSU_DID;

    console_puts("dsu: DID=0x");
    console_put_hex(did, 8);
    if ((did & ~DSU_DID_REVISION_Msk) !=
        (CHIP_DSU_DID & ~DSU_DID_REVISION_Msk)) {
        console_puts(BOARD_DID_EXPECTED);
        failures++;
    }
    console_puts("\r\n");

    console_puts("scb: CPUID=0x");
    console_put_hex(SCB->CPUID, 8);
    console_puts("\r\n");

    console_puts("serial:");
    for (unsigned int i = 0; i < 4U; i++) {
        console_puts(" 0x");
        console_put_hex(*(const volatile uint32_t *)serial_number_addr[i], 8);
    }
    console_puts("\r\n");

    console_puts("calrow: 0x");
    console_put_hex(cal_row[1], 8);
    console_put_hex(cal_row[0], 8);
    console_puts("\r\n");
}

/* The one interrupt-driven print: NVIC, the VCOM IRQ and INTENSET/CLR.DRE */
static void irq_print(void)
{
    static const char line[] =
        VCOM_IRQ_LABEL ": sent by " VCOM_HANDLER_NAME
        ", one byte per DRE interrupt\r\n";
    uint32_t waited_ms = 0;

    console_irq_write(line);
    while (console_irq_busy()) {
        if (systick_ms_elapsed() && ++waited_ms > IRQ_PRINT_TIMEOUT_MS) {
            if (console_irq_cancel() != line) {
                console_puts("\r\n");   /* end the partial line */
            }
            console_puts(VCOM_IRQ_LABEL
                         ": FAIL, interrupt-driven print stalled\r\n");
            failures++;
            return;
        }
    }
}

/* LED0 is active low: clearing its pin turns it on. */
static void blink_led0(void)
{
    for (unsigned int i = 1; i <= BLINK_COUNT; i++) {
        console_puts("led0: blink ");
        console_put_dec(i);
        console_putc('/');
        console_put_dec(BLINK_COUNT);
        console_puts("\r\n");

        LED0_ON();
        delay_ms(BLINK_HALF_PERIOD_MS);
        LED0_OFF();
        delay_ms(BLINK_HALF_PERIOD_MS);
    }
}

int main(void)
{
    sys_initialize();
    systick_start();

    console_puts("PP hello: " BOARD_BANNER "\r\n");
    report_steps();
    report_identity();
    irq_print();
    blink_led0();

    console_puts("PP:EXIT ");
    console_put_dec(failures);
    console_puts("\r\n");
    console_flush();

    for (;;) {
        __WFI();
    }
}
