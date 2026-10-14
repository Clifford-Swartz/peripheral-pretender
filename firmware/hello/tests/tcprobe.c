/*
 * TC probe: raw observations of behaviour the data sheet leaves open
 * (start value when counting down, buffer updates around the start, PER
 * reads while PERBUFV is set). Prints values, checks nothing; used to
 * decide what the twin models.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define C8              (TC0_REGS->COUNT8)

static void sync(void)
{
    while (C8.TC_SYNCBUSY != 0U) {
    }
}

static void command(uint8_t cmd)
{
    C8.TC_CTRLBSET = cmd;
    sync();
    while ((C8.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U) {
    }
}

static uint32_t count(void)
{
    command(TC_CTRLBSET_CMD_READSYNC);
    return C8.TC_COUNT;
}

static void setup(uint8_t ctrlb)
{
    C8.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync();
    C8.TC_CTRLA = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024;
    C8.TC_PER = 99;
    sync();
    if (ctrlb) {
        C8.TC_CTRLBSET = ctrlb;
        sync();
    }
}

static void enable(void)
{
    C8.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync();
}

static uint32_t ovf(void)
{
    return (C8.TC_INTFLAG & TC_INTFLAG_OVF_Msk) ? 1U : 0U;
}

static uint32_t bufv(void)
{
    return (C8.TC_STATUS & TC_STATUS_PERBUFV_Msk) ? 1U : 0U;
}

static void wait_ovf(void)
{
    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    for (uint32_t ms = 20; ms > 0U && !ovf();) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
}

int main(void)
{
    uint32_t a, b, c, d;

    sys_initialize();
    systick_start();
    console_puts("PP tcprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }

    /* P1: down counting right after enable */
    setup(TC_CTRLBSET_DIR_Msk);
    enable();
    a = count();
    delay_us(100);
    b = ovf();
    c = count();
    delay_ms(1);
    d = count();
    console_puts("p1 down:");
    put_field("count0", a, 2);
    put_field("ovf@100us", b, 1);
    put_field("count@100us", c, 2);
    put_field("count@1ms", d, 2);
    console_puts("\r\n");

    /* P2: PERBUF written mid-period */
    setup(0);
    enable();
    delay_ms(1);
    C8.TC_PERBUF = 49;
    a = bufv();
    b = C8.TC_PER;
    wait_ovf();
    delay_us(100);
    c = bufv();
    d = C8.TC_PER;
    console_puts("p2 mid:");
    put_field("valid", a, 1);
    put_field("PER", b, 2);
    put_field("valid@ovf", c, 1);
    put_field("PER@ovf", d, 2);
    console_puts("\r\n");

    /* P3: PERBUF written right after enable */
    setup(0);
    enable();
    C8.TC_PERBUF = 49;
    a = bufv();
    b = C8.TC_PER;
    delay_us(100);
    c = bufv();
    d = C8.TC_PER;
    console_puts("p3 start:");
    put_field("valid", a, 1);
    put_field("PER", b, 2);
    put_field("valid@100us", c, 1);
    put_field("PER@100us", d, 2);
    console_puts("\r\n");

    /* P4: PERBUF written before enable */
    setup(0);
    C8.TC_PERBUF = 49;
    a = bufv();
    b = C8.TC_PER;
    enable();
    delay_us(100);
    c = bufv();
    d = C8.TC_PER;
    console_puts("p4 before:");
    put_field("valid", a, 1);
    put_field("PER", b, 2);
    put_field("valid@run", c, 1);
    put_field("PER@run", d, 2);
    console_puts("\r\n");

    /* P5: LUPD, PERBUF mid-period: PER read while PERBUFV */
    setup(TC_CTRLBSET_LUPD_Msk);
    enable();
    delay_ms(1);
    C8.TC_PERBUF = 77;
    a = bufv();
    b = C8.TC_PER;
    wait_ovf();
    delay_us(100);
    c = bufv();
    d = C8.TC_PER;
    console_puts("p5 lupd:");
    put_field("valid", a, 1);
    put_field("PER", b, 2);
    put_field("valid@ovf", c, 1);
    put_field("PER@ovf", d, 2);
    command(TC_CTRLBSET_CMD_UPDATE);
    put_field("valid@upd", bufv(), 1);
    put_field("PER@upd", C8.TC_PER, 2);
    C8.TC_STATUS = TC_STATUS_PERBUFV_Msk;
    console_puts("\r\n");

    /* P6: CC0 buffer the same way, COUNT8 */
    setup(0);
    C8.TC_CC[0] = 10;
    sync();
    enable();
    delay_ms(1);
    C8.TC_CCBUF[0] = 20;
    a = (C8.TC_STATUS & TC_STATUS_CCBUFV0_Msk) ? 1U : 0U;
    b = C8.TC_CC[0];
    wait_ovf();
    delay_us(100);
    c = (C8.TC_STATUS & TC_STATUS_CCBUFV0_Msk) ? 1U : 0U;
    d = C8.TC_CC[0];
    console_puts("p6 ccbuf:");
    put_field("valid", a, 1);
    put_field("CC0", b, 2);
    put_field("valid@ovf", c, 1);
    put_field("CC0@ovf", d, 2);
    console_puts("\r\n");

    /* P7: LUPD, PERBUF = 19 mid-period: which TOP does the counter use? */
    setup(TC_CTRLBSET_LUPD_Msk);
    enable();
    delay_ms(1);
    C8.TC_PERBUF = 19;
    wait_ovf();
    a = 0;
    C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
    for (uint32_t ms = 21; ms > 0U;) {
        if (ovf()) {
            C8.TC_INTFLAG = TC_INTFLAG_OVF_Msk;
            a++;
        }
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    console_puts("p7 lupd top:");
    put_field("ovf-in-20ms", a, 2);
    console_puts("\r\n");
    C8.TC_STATUS = TC_STATUS_PERBUFV_Msk;

    /* P8: CCBUF in NPWM mode, mid-period */
    C8.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync();
    C8.TC_CTRLA = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1024;
    C8.TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    C8.TC_PER = 99;
    C8.TC_CC[0] = 10;
    sync();
    enable();
    delay_ms(1);
    C8.TC_CCBUF[0] = 20;
    a = (C8.TC_STATUS & TC_STATUS_CCBUFV0_Msk) ? 1U : 0U;
    b = C8.TC_CC[0];
    wait_ovf();
    delay_us(100);
    c = (C8.TC_STATUS & TC_STATUS_CCBUFV0_Msk) ? 1U : 0U;
    d = C8.TC_CC[0];
    console_puts("p8 ccbuf npwm:");
    put_field("valid", a, 1);
    put_field("CC0", b, 2);
    put_field("valid@ovf", c, 1);
    put_field("CC0@ovf", d, 2);
    console_puts("\r\n");

    /* P9: COUNT read-back after a COUNT write, without READSYNC */
    setup(0);
    C8.TC_COUNT = 42;
    sync();
    a = C8.TC_COUNT;
    console_puts("p9 count write:");
    put_field("read", a, 2);
    console_puts("\r\n");
    test_exit();
}
