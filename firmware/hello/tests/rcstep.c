/*
 * RC step response on the PIC32CM MC00 Curiosity Pro: the first analog
 * co-simulation check (docs/design/analog.md). Wire 10 kOhm from EXT1 pin 4
 * (PB09, driven here as a GPIO) to EXT1 pin 3 (PB08, ADC0 AIN2), and
 * 1 uF from pin 3 to pin 2 (GND). On the twin the same circuit is a
 * netlist (firmware/hello/circuits/rcstep.cir), simulated alongside:
 *
 *   python -m tools.pp run build/pic32cm_mc00_cpro/rcstep.elf \
 *       --machine pic32cm-mc00-cpro --circuit circuits/rcstep.cir
 *
 * Discharges the capacitor (PB09 low for 200 ms), steps PB09 high and
 * converts AIN2 against VDDANA every 500 us for 40 ms, then prints each
 * sample's time after the step (its conversion's start; the ADC samples
 * about 12 us later, at the end of SAMPLEN) and voltage, and the time
 * constant (the 63.2 % crossing, interpolated). A real 10 k / 1 u gives
 * 10.0 ms within the parts' tolerance; the check accepts 6-15 ms. PB09
 * is left low.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#ifndef _PIC32CM1216MC00048_H_
#error "rcstep.c is written for BOARD=pic32cm_mc00_cpro"
#endif

#define DRIVE_PIN       9U      /* PB09: EXT1 pin 4 */
#define SENSE_MUX       2U      /* PB08: EXT1 pin 3, ADC0 AIN2 */
#define PERIOD_US       500U
#define SAMPLES         80U     /* 40 ms */
#define CYCLES_US       (CPU_CLOCK_HZ / 1000000U)
#define TAU_MIN_US      6000U
#define TAU_MAX_US      15000U

static uint16_t code[SAMPLES];
static uint32_t at[SAMPLES];            /* CPU cycles after the step */
static uint32_t clk_last, clk_total;

/*
 * CPU cycles since clk_start(); call at least once per SysTick wrap. No
 * division (the M0+ does it in software, microseconds of skew between a
 * sample's timestamp and its start)
 */
static void clk_start(void)
{
    clk_last = SysTick->VAL;
    clk_total = 0U;
}

static uint32_t clk_cycles(void)
{
    uint32_t now = SysTick->VAL;

    /* VAL counts down from LOAD */
    clk_total += now <= clk_last ? clk_last - now :
                 clk_last + SysTick->LOAD + 1U - now;
    clk_last = now;
    return clk_total;
}

/* One conversion of the input set up by adc_read() */
static uint16_t convert(void)
{
    ADC0_REGS->ADC_INTFLAG = ADC_INTFLAG_RESRDY_Msk;
    ADC0_REGS->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    if (adc_wait_result() != 0) {
        return 0xFFFFU;
    }
    return ADC0_REGS->ADC_RESULT;
}

static uint32_t to_mv(uint16_t c, uint32_t vdd_mv)
{
    return (uint32_t)c * vdd_mv / 4095U;
}

int main(void)
{
    uint32_t vdd_mv, base, target, tau_us = 0;

    sys_initialize();
    systick_start();
    console_puts("PP rcstep: " BOARD_BANNER "\r\n");
    adc0_init(1U, 8U);                      /* PB08, function B */
    vdd_mv = adc_vddana_mv();
    (void)adc_read(SENSE_MUX, ADC_REFCTRL_REFSEL_INTVCC2);
    PORT_REGS->GROUP[1].PORT_OUTCLR = 1U << DRIVE_PIN;
    PORT_REGS->GROUP[1].PORT_DIRSET = 1U << DRIVE_PIN;
    delay_ms(200);                          /* 20 time constants */
    base = to_mv(convert(), vdd_mv);

    clk_start();
    PORT_REGS->GROUP[1].PORT_OUTSET = 1U << DRIVE_PIN;
    for (unsigned int i = 0; i < SAMPLES; i++) {
        uint32_t due = (i + 1U) * PERIOD_US * CYCLES_US;

        while ((at[i] = clk_cycles()) < due) {
        }
        code[i] = convert();
    }
    PORT_REGS->GROUP[1].PORT_OUTCLR = 1U << DRIVE_PIN;

    console_puts("vddana");
    put_field("mv", vdd_mv, 4);
    console_puts("\r\nbase mv=");
    console_put_dec(base);
    console_puts("\r\n");
    target = vdd_mv * 632U / 1000U;
    for (unsigned int i = 0; i < SAMPLES; i++) {
        uint32_t mv = to_mv(code[i], vdd_mv);
        uint32_t t_us = at[i] / CYCLES_US;

        console_puts("rc t_us=");
        console_put_dec(t_us);
        console_puts(" mv=");
        console_put_dec(mv);
        console_puts("\r\n");
        if (!tau_us && mv >= target && i > 0) {
            uint32_t prev = to_mv(code[i - 1], vdd_mv);
            uint32_t t0 = at[i - 1] / CYCLES_US;

            tau_us = t0 + (t_us - t0) * (target - prev) /
                     (mv - prev > 0U ? mv - prev : 1U);
        }
    }
    console_puts("tau_us=");
    console_put_dec(tau_us);
    if (base > 20U || tau_us < TAU_MIN_US || tau_us > TAU_MAX_US) {
        console_puts(" FAIL\r\n");
        failures++;
    } else {
        console_puts(" ok\r\n");
    }
    test_exit();
}
