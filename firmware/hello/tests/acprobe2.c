/*
 * AC probe, second round (PIC32CM MC00, AC U2245): window mode with both
 * comparators reconfigured, the undocumented COMPCTRL bits 21:20 as
 * hysteresis levels, VDDANA, STATE before READY, low-speed single shots.
 * Prints raw results; see acprobe.c and docs/research/peripherals/ac-u2245.md.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define AC              AC_REGS
#define TICKS_PER_MS    (CPU_CLOCK_HZ / 1000U)
#define HIGH            AC_COMPCTRL_SPEED(3)
#define EN              AC_COMPCTRL_ENABLE_Msk
#define VS_DAC          (AC_COMPCTRL_MUXPOS(4) | AC_COMPCTRL_MUXNEG(7))

static void nl(void)
{
    console_puts("\r\n");
}

static uint32_t ticks_since(uint32_t t0)
{
    uint32_t now = SysTick->VAL;

    return t0 >= now ? t0 - now : t0 + TICKS_PER_MS - now;
}

static void ac_sync(void)
{
    for (uint32_t n = 0; AC->AC_SYNCBUSY != 0U && n < WAIT_LOOPS; n++) {
    }
}

static void ac_ready(unsigned int n)
{
    for (uint32_t i = 0; !(AC->AC_STATUSB & (1U << n)) && i < WAIT_LOOPS;
         i++) {
    }
}

static void ac_off(unsigned int n)
{
    AC->AC_COMPCTRL[n] = 0U;
    ac_sync();
}

static void ac_cfg(unsigned int n, uint32_t val)
{
    ac_off(n);
    AC->AC_COMPCTRL[n] = val;
    AC->AC_COMPCTRL[n] = val | EN;
    ac_sync();
    ac_ready(n);
}

static void dac_sync(void)
{
    for (uint32_t n = 0; DAC_REGS->DAC_SYNCBUSY != 0U && n < WAIT_LOOPS;
         n++) {
    }
}

static void dac_set(uint16_t code)
{
    DAC_REGS->DAC_DATA = code;
    dac_sync();
    delay_us(30);
}

static unsigned int state(unsigned int n)
{
    delay_us(20);
    return (AC->AC_STATUSA >> n) & 1U;
}

static void clock_on(unsigned int id)
{
    GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN_GCLK0 |
                                  GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
}

static void ctrla(uint8_t v)
{
    AC->AC_CTRLA = v;
    ac_sync();
}

int main(void)
{
    static const uint16_t walk[] = { 800, 512, 200, 512, 800, 800, 512, 512 };
    uint32_t t0;

    sys_initialize();
    systick_start();
    console_puts("PP acprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_AC_Msk |
                                MCLK_APBCMASK_DAC_Msk;
    clock_on(AC_GCLK_ID);
    clock_on(DAC_GCLK_ID);
    adc0_init(0, 4);
    console_puts("vddana");
    put_field("mv", adc_vddana_mv(), 4);
    nl();

    DAC_REGS->DAC_CTRLB = DAC_CTRLB_IOEN_Msk | DAC_CTRLB_REFSEL_VDDANA;
    DAC_REGS->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    for (uint32_t n = 0; !(DAC_REGS->DAC_STATUS & DAC_STATUS_READY_Msk) &&
         n < WAIT_LOOPS; n++) {
    }
    ctrla(AC_CTRLA_ENABLE_Msk);

    /* Hysteresis levels: COMPCTRL bits 21:20 with HYSTEN, at v=31 */
    for (unsigned int sp = 0; sp < 2U; sp++) {
        for (unsigned int h = 0; h < 4U; h++) {
            unsigned int up = 0, down = 0;

            ac_cfg(0, VS_DAC | (sp ? HIGH : 0U) | AC_COMPCTRL_HYSTEN_Msk |
                      ((uint32_t)h << 20));
            AC->AC_SCALER[0] = 31U;
            for (unsigned int k = 412; k <= 612U; k++) {
                dac_set((uint16_t)k);
                if (!state(0)) {
                    up = k;
                    break;
                }
            }
            for (unsigned int k = 612; k >= 412U; k--) {
                dac_set((uint16_t)k);
                if (state(0)) {
                    down = k;
                    break;
                }
            }
            console_puts("hyst");
            put_field("speed", sp ? 3U : 0U, 1);
            put_field("lvl", h, 1);
            put_field("compctrl", AC->AC_COMPCTRL[0], 8);
            put_field("rise_to_0", up, 4);
            put_field("fall_to_1", down, 4);
            nl();
        }
    }

    /* STATE before READY, and low-speed single shots */
    ac_cfg(0, VS_DAC | HIGH);
    AC->AC_SCALER[0] = 31U;
    dac_set(200);                                   /* STATE 1 */
    ac_cfg(0, VS_DAC);
    ac_off(0);
    dac_set(800);                                   /* would be 0 */
    AC->AC_COMPCTRL[0] = VS_DAC;
    AC->AC_COMPCTRL[0] = VS_DAC | EN;
    console_puts("prerady");
    put_field("statusa", AC->AC_STATUSA, 2);
    put_field("statusb", AC->AC_STATUSB, 2);
    ac_sync();
    ac_ready(0);
    put_field("after", AC->AC_STATUSA, 2);
    nl();
    console_puts("single_low");
    ac_cfg(0, VS_DAC | AC_COMPCTRL_SINGLE_Msk);
    for (unsigned int i = 0; i < 2U; i++) {
        t0 = SysTick->VAL;
        AC->AC_CTRLB = AC_CTRLB_START0_Msk;
        while (!(AC->AC_STATUSB & 1U)) {
        }
        put_field("ticks", ticks_since(t0), 4);
    }
    nl();

    /* Window: both comparators reconfigured while disabled */
    ac_off(0);
    ac_off(1);
    ctrla(0);
    AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk;
    ac_sync();
    AC->AC_COMPCTRL[0] = VS_DAC | HIGH;
    AC->AC_COMPCTRL[0] = VS_DAC | HIGH | EN;
    AC->AC_COMPCTRL[1] = VS_DAC | HIGH;
    AC->AC_COMPCTRL[1] = VS_DAC | HIGH | EN;
    ac_sync();
    ctrla(AC_CTRLA_ENABLE_Msk);
    ac_ready(0);
    ac_ready(1);
    dac_set(512);
    {
        static const uint8_t pairs[4][2] = { {50, 50}, {50, 10}, {10, 50},
                                             {10, 10} };

        for (unsigned int i = 0; i < 4U; i++) {
            AC->AC_SCALER[0] = pairs[i][0];
            AC->AC_SCALER[1] = pairs[i][1];
            delay_us(30);
            console_puts("win");
            put_field("s0", pairs[i][0], 2);
            put_field("s1", pairs[i][1], 2);
            put_field("statusa", AC->AC_STATUSA, 2);
            nl();
        }
    }
    /* WIN0 per WINTSEL: limits COMP0 40 (upper), COMP1 22 (lower) */
    for (unsigned int w = 0; w < 4U; w++) {
        ctrla(0);
        AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk | AC_WINCTRL_WINTSEL0(w);
        ac_sync();
        ctrla(AC_CTRLA_ENABLE_Msk);
        ac_ready(0);
        ac_ready(1);
        AC->AC_SCALER[0] = 40U;
        AC->AC_SCALER[1] = 22U;
        console_puts("wintsel");
        put_field("w", w, 1);
        for (unsigned int k = 0; k < sizeof(walk) / sizeof(walk[0]); k++) {
            dac_set(walk[k]);
            delay_us(30);
            if (k == 0U) {
                AC->AC_INTFLAG = 0xFFU;
                continue;
            }
            put_field("f", AC->AC_INTFLAG, 2);
            put_field("st", AC->AC_STATUSA, 2);
            AC->AC_INTFLAG = 0xFFU;
        }
        nl();
    }
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
