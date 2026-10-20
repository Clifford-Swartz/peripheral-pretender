/*
 * FREQM check (PIC32CM MC00, FREQM U2257): the frequency meter against
 * what silicon did in firmware/hello APP=freqmprobe
 * (docs/research/peripherals/freqm-u2257.md). Both clocks come from
 * OSC48M through the GCLK dividers, so the counts are exact on silicon as
 * on the twin; the board's 12 MHz XOSC and the FDPLL96M are checked within
 * a range. A failed check names what it expected and ends its line in
 * FAIL; PP:EXIT <failures>.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define FM              FREQM_REGS
#define ENABLE          0x02U
#define SWRST           0x01U
#define BUSY            0x01U
#define OVF             0x02U
#define DONE            0x01U
#define TIMEOUT         0xFFFFFFFFU

#define S_XOSC          0U
#define S_OSC48M        6U
#define S_DPLL          7U
#define REF_HZ          46875U          /* OSC48M / 1024 on GCLK1 */

static uint32_t wraps;

static void put_range(const char *name, uint32_t value, uint32_t lo,
                      uint32_t hi, unsigned int digits)
{
    put_field(name, value, digits);
    if (value < lo || value > hi) {
        console_puts("(expected 0x");
        console_put_hex(lo, digits);
        console_puts("..0x");
        console_put_hex(hi, digits);
        console_putc(')');
        failures++;
    }
}

static void end(void)
{
    static unsigned int seen;

    console_puts(failures == seen ? " ok\r\n" : " FAIL\r\n");
    seen = failures;
}

static void clock_zero(void)
{
    SysTick->VAL = 0U;
    (void)SysTick->CTRL;
    wraps = 0;
}

static uint32_t now_us(void)
{
    uint32_t v = SysTick->VAL;

    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        wraps++;
        v = SysTick->VAL;
    }
    return wraps * 1000U + (SysTick->LOAD + 1U - v) / 48U;
}

static void fm_sync(void)
{
    for (uint32_t n = 0; FM->FREQM_SYNCBUSY != 0U && n < 10000000U; n++) {
    }
}

static int gen(unsigned int n, uint32_t src, uint32_t div, int divsel)
{
    GCLK_REGS->GCLK_GENCTRL[n] = GCLK_GENCTRL_SRC(src) | GCLK_GENCTRL_DIV(div) |
                                 (divsel ? GCLK_GENCTRL_DIVSEL_Msk : 0U) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY &
         (GCLK_SYNCBUSY_GENCTRL0_Msk << n); i++) {
        if (i == 2000000U) {
            return 0;
        }
    }
    return 1;
}

static void pch(unsigned int id, unsigned int g, int on)
{
    uint32_t i;

    GCLK_REGS->GCLK_PCHCTRL[id] = 0U;
    for (i = 0; (GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk) &&
         i < 2000000U; i++) {
    }
    if (on) {
        GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN(g) |
                                      GCLK_PCHCTRL_CHEN_Msk;
        for (i = 0; !(GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk) &&
             i < 2000000U; i++) {
        }
    }
}

static void fm_setup(uint32_t refnum)
{
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    FM->FREQM_CFGA = (uint16_t)refnum;
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    FM->FREQM_INTFLAG = DONE;
    FM->FREQM_STATUS = OVF;
}

static uint32_t fm_run(uint32_t ms)
{
    clock_zero();
    FM->FREQM_CTRLB = 1U;
    while (FM->FREQM_STATUS & BUSY) {
        if (now_us() > ms * 1000U) {
            return TIMEOUT;
        }
    }
    return now_us();
}

/* One measurement; VALUE checked against @expect within @tol */
static void measure(const char *name, uint32_t refnum, uint32_t expect,
                    uint32_t tol)
{
    fm_setup(refnum);
    (void)fm_run(1000);
    put_range(name, FM->FREQM_VALUE, expect - tol, expect + tol, 6);
}

static void check_registers(void)
{
    console_puts("reset");
    put_checked("ctrla", FM->FREQM_CTRLA, 0, 2);
    put_checked("cfga", FM->FREQM_CFGA, 0, 4);
    put_checked("status", FM->FREQM_STATUS, 0, 2);
    put_checked("value", FM->FREQM_VALUE, 0, 8);
    end();
    console_puts("cfga");
    FM->FREQM_CFGA = 0xFFFFU;
    put_checked("mask", FM->FREQM_CFGA, 0x00FF, 4);     /* no DIVREF */
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    FM->FREQM_CFGA = 0x0010U;
    put_checked("enabled", FM->FREQM_CFGA, 0x00FF, 4);  /* enable-protected */
    FM->FREQM_CTRLA = SWRST;
    fm_sync();
    put_checked("swrst", FM->FREQM_CFGA, 0, 4);
    end();
}

/* VALUE = REFNUM * f_MSR / f_REF, exactly, for clocks from one source */
static void check_ratios(void)
{
    static const struct {
        uint8_t div;
        uint8_t divsel;
        uint32_t value;
    } msr[] = {
        { 1, 0, 261120 },   /* 48 MHz */
        { 3, 0, 87040 },    /* 16 MHz */
        { 0, 1, 130560 },   /* DIVSEL: / 2 */
        { 4, 1, 8160 },     /* DIVSEL: / 32 */
        { 255, 0, 1024 },   /* the 8-bit DIV of GCLK2 */
    };
    uint32_t us;

    gen(1, S_OSC48M, 1024, 0);
    console_puts("ratio");
    for (unsigned int i = 0; i < sizeof(msr) / sizeof(msr[0]); i++) {
        gen(2, S_OSC48M, msr[i].div, msr[i].divsel);
        pch(FREQM_GCLK_ID_MSR, 2, 1);
        pch(FREQM_GCLK_ID_REF, 1, 1);
        measure("v", 255, msr[i].value, 0);
    }
    gen(2, S_OSC48M, 1, 0);
    measure("refnum1", 1, 1024, 0);
    measure("refnum128", 128, 131072, 0);
    gen(2, S_OSC48M, 1024, 0);                  /* 8 bits: undivided */
    measure("div1024_is_div0", 255, 261120, 0);
    gen(1, S_OSC48M, 300, 0);                   /* 16-bit DIV on GCLK1 */
    gen(2, S_OSC48M, 1, 0);
    measure("ref_div300", 255, 76500, 0);
    end();

    /* REFNUM + 2 to 3 reference periods (START's phase against REF) */
    gen(1, S_OSC48M, 1024, 0);
    console_puts("duration");
    for (unsigned int i = 0; i < 3U; i++) {
        static const uint8_t rn[] = { 1, 16, 255 };

        fm_setup(rn[i]);
        us = fm_run(100);
        put_range("us", us, (2U * rn[i] + 3U) * 1000000U / (2U * REF_HZ),
                  (2U * rn[i] + 7U) * 1000000U / (2U * REF_HZ) + 5U, 6);
    }
    end();
}

static void check_behaviour(void)
{
    uint32_t v, us;

    /* 255 ms windows: REF = OSC48M / 48000 = 1 kHz */
    gen(1, S_OSC48M, 48000, 0);
    gen(2, S_OSC48M, 1, 0);
    pch(FREQM_GCLK_ID_MSR, 2, 1);
    pch(FREQM_GCLK_ID_REF, 1, 1);

    console_puts("live");
    fm_setup(255);
    clock_zero();
    FM->FREQM_CTRLB = 1U;
    while (now_us() < 100000U) {
    }
    v = FM->FREQM_VALUE;                        /* the count so far */
    put_range("v_100ms", v, 4500000, 4800000, 8);
    FM->FREQM_CTRLB = 1U;                       /* ignored while busy */
    while ((FM->FREQM_STATUS & BUSY) && now_us() < 2000000U) {
    }
    us = now_us();
    put_range("us", us, 255000, 261000, 8);
    put_checked("v", FM->FREQM_VALUE, 12240000, 8);
    put_checked("if", FM->FREQM_INTFLAG, DONE, 1);
    end();

    console_puts("refnum0");                     /* counts as 512 */
    fm_setup(0);
    us = fm_run(1000);
    put_range("us", us, 512000, 518000, 8);
    put_checked("v", FM->FREQM_VALUE, (512U * 48000U) & 0xFFFFFFU, 8);
    put_checked("st", FM->FREQM_STATUS, OVF, 1);
    end();

    console_puts("disable");
    fm_setup(255);
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    FM->FREQM_INTFLAG = DONE;
    FM->FREQM_CTRLB = 1U;                       /* ignored while disabled */
    delay_ms(2);
    put_checked("st_off", FM->FREQM_STATUS, 0, 1);
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    FM->FREQM_CTRLB = 1U;
    delay_ms(100);
    FM->FREQM_CTRLA = 0U;                       /* aborts */
    fm_sync();
    put_checked("st_aborted", FM->FREQM_STATUS, 0, 1);
    put_checked("if", FM->FREQM_INTFLAG, 0, 1);
    put_checked("v", FM->FREQM_VALUE, 0, 8);
    end();

    console_puts("no_ref");
    fm_setup(255);
    pch(FREQM_GCLK_ID_REF, 1, 0);
    put_checked("waits", fm_run(300), TIMEOUT, 8);
    put_checked("busy", FM->FREQM_STATUS, BUSY, 1);
    clock_zero();
    pch(FREQM_GCLK_ID_REF, 1, 1);
    while ((FM->FREQM_STATUS & BUSY) && now_us() < 2000000U) {
    }
    put_range("then_us", now_us(), 255000, 261000, 8);
    put_checked("v", FM->FREQM_VALUE, 12240000, 8);
    end();

    console_puts("no_msr");
    fm_setup(255);
    pch(FREQM_GCLK_ID_MSR, 2, 0);
    us = fm_run(1000);
    put_range("us", us, 255000, 261000, 8);
    put_checked("v", FM->FREQM_VALUE, 0, 8);
    put_checked("if", FM->FREQM_INTFLAG, DONE, 1);
    pch(FREQM_GCLK_ID_MSR, 2, 1);
    end();

    /* 48 MHz against OSC48M / 2^17: 33,423,360 keeps its low 24 bits */
    console_puts("overflow");
    gen(1, S_OSC48M, 16, 1);
    fm_setup(255);
    (void)fm_run(1000);
    put_checked("v", FM->FREQM_VALUE, 0xFE0000, 8);
    put_checked("st", FM->FREQM_STATUS, OVF, 1);
    FM->FREQM_STATUS = OVF;
    put_checked("w1c", FM->FREQM_STATUS, 0, 1);
    (void)fm_run(1000);
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    put_checked("cleared_by_disable", FM->FREQM_STATUS, 0, 1);
    FM->FREQM_CFGA = 100U;                      /* 13,107,200: fits */
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    (void)fm_run(1000);
    put_checked("v100", FM->FREQM_VALUE, 13107200, 8);
    put_checked("st100", FM->FREQM_STATUS, 0, 1);
    end();

    console_puts("done_irq");
    gen(1, S_OSC48M, 1024, 0);
    fm_setup(255);
    FM->FREQM_INTENCLR = DONE;
    NVIC_ClearPendingIRQ(FREQM_IRQn);
    (void)fm_run(100);
    put_checked("done", FM->FREQM_INTFLAG, DONE, 1);
    put_checked("nvic", NVIC_GetPendingIRQ(FREQM_IRQn), 0, 1);
    FM->FREQM_INTFLAG = DONE;
    FM->FREQM_INTENSET = DONE;
    (void)fm_run(100);
    put_checked("nvic_inten", NVIC_GetPendingIRQ(FREQM_IRQn), 1, 1);
    FM->FREQM_INTENCLR = DONE;
    FM->FREQM_INTFLAG = DONE;
    NVIC_ClearPendingIRQ(FREQM_IRQn);
    end();

    console_puts("swrst");
    gen(1, S_OSC48M, 48000, 0);
    fm_setup(255);
    FM->FREQM_CTRLB = 1U;
    delay_ms(50);
    FM->FREQM_CTRLA = SWRST;
    fm_sync();
    put_checked("ctrla", FM->FREQM_CTRLA, 0, 2);
    put_checked("cfga", FM->FREQM_CFGA, 0, 4);
    put_checked("st", FM->FREQM_STATUS, 0, 1);
    put_checked("v", FM->FREQM_VALUE, 0, 8);
    end();
}

/* The FDPLL96M and the board's XOSC, against OSC48M / 1024 */
static void check_sources(void)
{
    gen(1, S_OSC48M, 1024, 0);
    gen(8, S_OSC48M, 48, 0);                    /* 1 MHz FDPLL reference */
    pch(OSCCTRL_GCLK_ID_FDPLL, 8, 1);
    OSCCTRL_REGS->OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDR(95);
    while (OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY != 0U) {
    }
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_REFCLK_GCLK |
                                      OSCCTRL_DPLLCTRLB_LBYPASS_Msk;
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = OSCCTRL_DPLLCTRLA_ENABLE_Msk;
    while (OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY != 0U) {
    }
    clock_zero();
    while (!(OSCCTRL_REGS->OSCCTRL_DPLLSTATUS &
             OSCCTRL_DPLLSTATUS_CLKRDY_Msk) && now_us() < 100000U) {
    }
    console_puts("dpll");
    put_checked("clkrdy", (OSCCTRL_REGS->OSCCTRL_DPLLSTATUS &
                           OSCCTRL_DPLLSTATUS_CLKRDY_Msk) != 0U, 1, 1);
    if (gen(7, S_DPLL, 1, 0)) {
        pch(FREQM_GCLK_ID_MSR, 7, 1);
        pch(FREQM_GCLK_ID_REF, 1, 1);
        measure("96m", 255, 522240, 2);
    }
    end();

    /* XOSC: the board's 12 MHz oscillator; against OSC48M (+-3 % DS) */
    OSCCTRL_REGS->OSCCTRL_XOSCCTRL = OSCCTRL_XOSCCTRL_ENABLE_Msk;
    clock_zero();
    while (!(OSCCTRL_REGS->OSCCTRL_STATUS & OSCCTRL_STATUS_XOSCRDY_Msk) &&
           now_us() < 100000U) {
    }
    console_puts("xosc");
    put_checked("rdy", (OSCCTRL_REGS->OSCCTRL_STATUS &
                        OSCCTRL_STATUS_XOSCRDY_Msk) != 0U, 1, 1);
    if (gen(4, S_XOSC, 1, 0)) {
        pch(FREQM_GCLK_ID_MSR, 4, 1);
        measure("12m", 255, 65280, 2600);
    }
    end();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP freqm: " BOARD_BANNER "\r\n");
    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL &= (uint8_t)~OSCCTRL_OSC48MCTRL_ONDEMAND_Msk;
    check_registers();
    check_ratios();
    check_behaviour();
    check_sources();
    test_exit();
}
