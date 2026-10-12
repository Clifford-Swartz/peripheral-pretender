/*
 * Peripheral Pretender hello image for PIC32CM5164JH01100 (EV81X90A).
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

#define IRQ_PRINT_TIMEOUT_MS    100U
#define BLINK_COUNT             3U
#define BLINK_HALF_PERIOD_MS    200U

/* The 128-bit serial number (DS60001632H 10.3); the DFP does not name it. */
static const uintptr_t serial_number_addr[] = {
    0x0080A00CU, 0x0080A040U, 0x0080A044U, 0x0080A048U,
};

static unsigned int failures;

/* SysTick from the CPU clock, wrapping every millisecond; no interrupt. */
static void systick_start(void)
{
    SysTick->LOAD = CPU_CLOCK_HZ / 1000U - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* True once per wrap: reading CTRL clears COUNTFLAG. */
static bool systick_ms_elapsed(void)
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U;
}

static void delay_ms(uint32_t ms)
{
    while (ms > 0U) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
}

static void put_field(const char *name, uint32_t value, unsigned int digits)
{
    console_putc(' ');
    console_puts(name);
    console_puts("=0x");
    console_put_hex(value, digits);
}

/* put_field(), counting a failure when @value is not @expected. */
static void put_checked(const char *name, uint32_t value, uint32_t expected,
                        unsigned int digits)
{
    put_field(name, value, digits);
    if (value != expected) {
        console_puts("(expected 0x");
        console_put_hex(expected, digits);
        console_putc(')');
        failures++;
    }
}

/*
 * One line per SYS_Initialize step, read back afterwards so that the
 * sequence itself stays identical to Harmony's.  The expected values are
 * the ones docs/research/boot-path.md records, not copies of sys_init.c.
 */
static void report_steps(void)
{
    port_group_registers_t *pb = &PORT_REGS->GROUP[1];
    sercom_usart_int_registers_t *usart = VCOM_USART;

    console_puts("step 3 nvmctrl: CTRLB <- RWS(3)\r\n");

    console_puts("step 4 port:");
    put_checked("PB.PINCFG10", pb->PORT_PINCFG[10], 0x01U, 2);
    put_checked("PB.PINCFG11", pb->PORT_PINCFG[11], 0x01U, 2);
    put_checked("PB.PMUX5", pb->PORT_PMUX[5], 0x33U, 2);
    put_checked("PC.DIR", LED0_GROUP->PORT_DIR, 0x00000020U, 8);
    console_puts("\r\n");

    console_puts("step 5 clock:");
    put_field("CAL48M", OSCCTRL_REGS->OSCCTRL_CAL48M, 8);
    put_checked("OSC48MDIV", OSCCTRL_REGS->OSCCTRL_OSC48MDIV, 0x00U, 2);
    put_checked("GENCTRL0", GCLK_REGS->GCLK_GENCTRL[0], 0x00010106U, 8);
    put_checked("PCHCTRL22", GCLK_REGS->GCLK_PCHCTRL[SERCOM4_GCLK_ID_CORE],
                0x00000040U, 8);
    put_checked("APBCMASK", MCLK_REGS->MCLK_APBCMASK, 0x08000020U, 8);
    console_puts("\r\n");

    console_puts("step 6 nvmctrl:");
    put_checked("CTRLB", NVMCTRL_REGS->NVMCTRL_CTRLB, 0x00000084U, 8);
    console_puts("\r\n");

    console_puts("step 7 sercom4:");
    put_checked("CTRLA", usart->SERCOM_CTRLA, 0x40310106U, 8);
    put_checked("BAUD", usart->SERCOM_BAUD, 63019U, 4);
    put_checked("CTRLB", usart->SERCOM_CTRLB, 0x00030000U, 8);
    console_puts("\r\n");

    console_puts("step 8 nvic:");
    put_checked("ISER", NVIC->ISER[0], 0x00002000U, 8);
    put_checked("IPR3", NVIC->IPR[3], 0x0000C000U, 8);
    console_puts("\r\n");
}

/* Identity and factory data; only the DID's device bits are checked. */
static void report_identity(void)
{
    const volatile uint32_t *cal_row = (const volatile uint32_t *)OTP5_ADDR;
    uint32_t did = DSU_REGS->DSU_DID;

    console_puts("dsu: DID=0x");
    console_put_hex(did, 8);
    if ((did & ~DSU_DID_REVISION_Msk) != CHIP_DSU_DID) {
        console_puts("(expected 0x11060x00)");
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

/* The one interrupt-driven print: NVIC, IRQ13 and INTENSET/INTENCLR.DRE. */
static void irq_print(void)
{
    static const char line[] =
        "irq13: sent by SERCOM4_Handler, one byte per DRE interrupt\r\n";
    uint32_t waited_ms = 0;

    console_irq_write(line);
    while (console_irq_busy()) {
        if (systick_ms_elapsed() && ++waited_ms > IRQ_PRINT_TIMEOUT_MS) {
            if (console_irq_cancel() != line) {
                console_puts("\r\n");   /* end the partial line */
            }
            console_puts("irq13: FAIL, interrupt-driven print stalled\r\n");
            failures++;
            return;
        }
    }
}

/* LED0 is active low: clearing PC05 turns it on. */
static void blink_led0(void)
{
    for (unsigned int i = 1; i <= BLINK_COUNT; i++) {
        console_puts("led0: blink ");
        console_put_dec(i);
        console_putc('/');
        console_put_dec(BLINK_COUNT);
        console_puts("\r\n");

        LED0_GROUP->PORT_OUTCLR = LED0_MASK;
        delay_ms(BLINK_HALF_PERIOD_MS);
        LED0_GROUP->PORT_OUTSET = LED0_MASK;
        delay_ms(BLINK_HALF_PERIOD_MS);
    }
}

int main(void)
{
    sys_initialize();
    systick_start();

    console_puts("PP hello: PIC32CM5164JH01100 on PIC32CM JH01 Curiosity Pro "
                 "(EV81X90A)\r\n");
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
