/*
 * FREQM probe (PIC32CM MC00, FREQM U2257): what the frequency meter does,
 * and what the board's clock sources really run at. Bare EV15N46A, no
 * wiring. Prints raw results, one row per line (plan in
 * docs/research/peripherals/freqm-u2257.md, 5.2):
 *
 *   fm <tag> v=VALUE st=STATUS if=INTFLAG us=START-to-not-BUSY f=Hz
 *
 * f is VALUE * f_REF / REFNUM with the nominal f_REF. The reference is
 * OSC48M-derived (GCLK1, the 16-bit divider). A generator whose source
 * does not run never finishes synchronizing and stays stuck, so every
 * questionable source gets a generator of its own: GCLK2 OSC48M-derived
 * MSR, GCLK3 XOSC32K, GCLK4 XOSC, GCLK5 OSCULP32K, GCLK6 OSC32K, GCLK7 the
 * FDPLL96M (from GCLK8 = 1 MHz); they run last.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define FM              FREQM_REGS
#define OTP5            ((volatile const uint32_t *)0x00806020U)
#define ENABLE          0x02U
#define SWRST           0x01U
#define BUSY            0x01U
#define OVF             0x02U
#define DONE            0x01U
#define TIMEOUT         0xFFFFFFFFU

#define S_XOSC          0U
#define S_OSCULP32K     3U
#define S_OSC32K        4U
#define S_XOSC32K       5U
#define S_OSC48M        6U
#define S_DPLL          7U

static uint32_t wraps;

static void nl(void)
{
    console_puts("\r\n");
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

/* 1 when the generator synchronized (its source runs) */
static int gen(unsigned int n, uint32_t src, uint32_t div, int divsel)
{
    GCLK_REGS->GCLK_GENCTRL[n] = GCLK_GENCTRL_SRC(src) | GCLK_GENCTRL_DIV(div) |
                                 (divsel ? GCLK_GENCTRL_DIVSEL_Msk : 0U) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY &
         (GCLK_SYNCBUSY_GENCTRL0_Msk << n); i++) {
        if (i == 2000000U) {
            console_puts("gen_stuck");
            put_field("n", n, 1);
            put_field("src", src, 1);
            nl();
            return 0;
        }
    }
    return 1;
}

/* CHEN reads back only once the generator runs: bounded waits */
static int pch(unsigned int id, unsigned int g, int on)
{
    uint32_t i;

    GCLK_REGS->GCLK_PCHCTRL[id] = 0U;
    for (i = 0; (GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk) &&
         i < 2000000U; i++) {
    }
    if (!on) {
        return 1;
    }
    GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN(g) | GCLK_PCHCTRL_CHEN_Msk;
    for (i = 0; !(GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk) &&
         i < 2000000U; i++) {
    }
    return i < 2000000U;
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

static void row(const char *tag, uint32_t refnum, uint32_t f_ref, uint32_t ms)
{
    uint32_t us, v;

    fm_setup(refnum);
    us = fm_run(ms);
    v = FM->FREQM_VALUE;
    console_puts("fm ");
    console_puts(tag);
    put_field("v", v, 6);
    console_puts("(");
    console_put_dec(v);
    console_puts(")");
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    put_field("us", us, 8);
    if (refnum && f_ref) {
        console_puts(" f=");
        console_put_dec((uint32_t)((uint64_t)v * f_ref / refnum));
    }
    nl();
}

/* MSR from generator @g, REF from GCLK1 */
static void msr(unsigned int g)
{
    pch(FREQM_GCLK_ID_MSR, g, 1);
    pch(FREQM_GCLK_ID_REF, 1, 1);
}

static void osc_report(void)
{
    console_puts("osc");
    put_field("oscctrl_status", OSCCTRL_REGS->OSCCTRL_STATUS, 8);
    put_field("osc32k_status", OSC32KCTRL_REGS->OSC32KCTRL_STATUS, 8);
    put_field("xosc32k", OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K, 4);
    put_field("xoscctrl", OSCCTRL_REGS->OSCCTRL_XOSCCTRL, 4);
    nl();
}

static void ratios(void)
{
    static const char *const g0[] = { "G0.div2", "G0.div4", "G0.div8" };
    static const uint8_t g0d[] = { 2, 4, 8 };

    /* REF: OSC48M / 1024 = 46,875 Hz on GCLK1 */
    gen(1, S_OSC48M, 1024, 0);
    gen(2, S_OSC48M, 1, 0);
    msr(2);
    row("S1 osc48m", 255, 46875, 2000);
    for (unsigned int i = 0; i < 3U; i++) {
        gen(2, S_OSC48M, g0d[i], 0);
        row(g0[i], 255, 46875, 2000);
    }
    gen(2, S_OSC48M, 3, 0);
    row("G1 div3", 255, 46875, 2000);
    for (unsigned int n = 0; n <= 4U; n++) {
        static const char *const t[] = { "G2.0 divsel", "G2.1 divsel",
                                         "G2.2 divsel", "G2.3 divsel",
                                         "G2.4 divsel" };

        gen(2, S_OSC48M, n, 1);
        row(t[n], 255, 46875, 2000);
    }
    gen(2, S_OSC48M, 0, 0);
    row("G3 div0", 255, 46875, 2000);

    gen(2, S_OSC48M, 1024, 0);
    row("R1.255 equal", 255, 46875, 2000);
    row("R1.128 equal", 128, 46875, 2000);
    row("R1.1 equal", 1, 46875, 2000);
    pch(FREQM_GCLK_ID_MSR, 1, 1);
    row("R1b same_gen", 255, 46875, 2000);
    pch(FREQM_GCLK_ID_MSR, 2, 1);
    gen(2, S_OSC48M, 512, 0);
    row("R2.255 double", 255, 46875, 2000);
    row("R2.1 double", 1, 46875, 2000);
    gen(2, S_OSC48M, 8, 0);
    row("R3 x128", 255, 46875, 2000);
    gen(2, S_OSC48M, 1024, 0);
    gen(1, S_OSC48M, 512, 0);
    row("R4 ref_faster", 255, 93750, 2000);
    gen(2, S_OSC48M, 1, 0);
    gen(1, S_OSC48M, 300, 0);
    row("G4 ref_div300", 255, 160000, 2000);
}

static void behaviour(void)
{
    uint32_t us, v1, v2, v3;

    /* 255 ms windows: REF = OSC48M / 48000 = 1 kHz */
    gen(1, S_OSC48M, 48000, 0);
    gen(2, S_OSC48M, 1, 0);
    msr(2);

    console_puts("E1 refnum0");
    fm_setup(0);
    us = fm_run(2000);
    put_field("us", us, 8);
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    put_field("v", FM->FREQM_VALUE, 8);
    FM->FREQM_CTRLA = SWRST;
    fm_sync();
    nl();

    console_puts("E2 restart");
    fm_setup(255);
    clock_zero();
    FM->FREQM_CTRLB = 1U;
    while (now_us() < 100000U) {
    }
    v1 = FM->FREQM_VALUE;
    FM->FREQM_CTRLB = 1U;                   /* again while busy */
    while (now_us() < 150000U) {
    }
    v2 = FM->FREQM_VALUE;
    while ((FM->FREQM_STATUS & BUSY) && now_us() < 2000000U) {
    }
    us = now_us();
    v3 = FM->FREQM_VALUE;
    put_field("v_mid", v1, 8);
    put_field("v_mid2", v2, 8);
    put_field("us", us, 8);
    put_field("v", v3, 8);
    put_field("if", FM->FREQM_INTFLAG, 1);
    nl();

    console_puts("E3 start_disabled");
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    FM->FREQM_INTFLAG = DONE;
    FM->FREQM_CTRLB = 1U;
    put_field("st", FM->FREQM_STATUS, 1);
    delay_ms(300);
    put_field("st_later", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    put_field("st_enabled", FM->FREQM_STATUS, 1);
    delay_ms(300);
    put_field("if_enabled", FM->FREQM_INTFLAG, 1);
    nl();

    console_puts("E4 disable_mid");
    fm_setup(255);
    FM->FREQM_CTRLB = 1U;
    delay_ms(100);
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    put_field("v", FM->FREQM_VALUE, 8);
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    delay_ms(300);
    put_field("st_reenabled", FM->FREQM_STATUS, 1);
    put_field("if_reenabled", FM->FREQM_INTFLAG, 1);
    nl();

    console_puts("E5 no_ref");
    fm_setup(255);
    pch(FREQM_GCLK_ID_REF, 1, 0);
    us = fm_run(1000);
    put_field("us", us, 8);
    put_field("st", FM->FREQM_STATUS, 1);
    clock_zero();
    pch(FREQM_GCLK_ID_REF, 1, 1);
    while ((FM->FREQM_STATUS & BUSY) && now_us() < 2000000U) {
    }
    put_field("after_ref_us", now_us(), 8);
    put_field("v", FM->FREQM_VALUE, 8);
    put_field("if", FM->FREQM_INTFLAG, 1);
    nl();

    console_puts("E6 no_msr");
    fm_setup(255);
    pch(FREQM_GCLK_ID_MSR, 2, 0);
    us = fm_run(2000);
    put_field("us", us, 8);
    put_field("v", FM->FREQM_VALUE, 8);
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    pch(FREQM_GCLK_ID_MSR, 2, 1);
    nl();

    /* E7: 48 MHz against OSC48M / 2^17 (366 Hz, DIVSEL): 33.4 M */
    gen(1, S_OSC48M, 16, 1);
    console_puts("E7 overflow");
    fm_setup(255);
    us = fm_run(2000);
    put_field("us", us, 8);
    put_field("v", FM->FREQM_VALUE, 8);
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    FM->FREQM_CFGA = 100U;                  /* 13.1 M: fits */
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    FM->FREQM_INTFLAG = DONE;
    put_field("st_kept", FM->FREQM_STATUS, 1);
    us = fm_run(2000);
    put_field("v100", FM->FREQM_VALUE, 8);
    put_field("st100", FM->FREQM_STATUS, 1);
    FM->FREQM_STATUS = BUSY;
    put_field("st_w1busy", FM->FREQM_STATUS, 1);
    FM->FREQM_STATUS = OVF;
    put_field("st_w1ovf", FM->FREQM_STATUS, 1);
    nl();

    console_puts("E8 cfga");
    FM->FREQM_CTRLA = 0U;
    fm_sync();
    FM->FREQM_CFGA = 0x80FFU;
    put_field("cfga", FM->FREQM_CFGA, 4);
    FM->FREQM_CFGA = 0xFFFFU;
    put_field("cfga_all", FM->FREQM_CFGA, 4);
    FM->FREQM_CFGA = 0x00FFU;
    FM->FREQM_CTRLA = ENABLE;
    fm_sync();
    FM->FREQM_CFGA = 0x0010U;
    put_field("cfga_enabled", FM->FREQM_CFGA, 4);
    nl();

    gen(1, S_OSC48M, 1024, 0);
    console_puts("E10 done_flag");
    fm_setup(255);
    FM->FREQM_INTENCLR = DONE;
    NVIC_ClearPendingIRQ(FREQM_IRQn);
    us = fm_run(100);
    put_field("if_no_inten", FM->FREQM_INTFLAG, 1);
    put_field("nvic", NVIC_GetPendingIRQ(FREQM_IRQn), 1);
    FM->FREQM_INTFLAG = DONE;
    FM->FREQM_INTENSET = DONE;
    us = fm_run(100);
    put_field("nvic_inten", NVIC_GetPendingIRQ(FREQM_IRQn), 1);
    FM->FREQM_INTENCLR = DONE;
    FM->FREQM_INTFLAG = DONE;
    NVIC_ClearPendingIRQ(FREQM_IRQn);
    FM->FREQM_INTENSET = 0xFFU;
    put_field("inten_mask", FM->FREQM_INTENSET, 2);
    FM->FREQM_INTENCLR = 0xFFU;
    nl();

    console_puts("E11 sync");
    {
        uint32_t n;

        FM->FREQM_CTRLA = 0U;
        fm_sync();
        clock_zero();
        FM->FREQM_CTRLA = ENABLE;
        for (n = 0; FM->FREQM_SYNCBUSY != 0U; n++) {
        }
        put_field("enable_us", now_us(), 8);
        put_field("loops", n, 8);
        clock_zero();
        FM->FREQM_CTRLA = SWRST;
        for (n = 0; FM->FREQM_SYNCBUSY != 0U; n++) {
        }
        put_field("swrst_us", now_us(), 8);
    }
    nl();

    console_puts("E12 swrst_mid");
    gen(1, S_OSC48M, 48000, 0);
    fm_setup(255);
    FM->FREQM_CTRLB = 1U;
    delay_ms(100);
    FM->FREQM_CTRLA = SWRST;
    fm_sync();
    put_field("ctrla", FM->FREQM_CTRLA, 2);
    put_field("cfga", FM->FREQM_CFGA, 4);
    put_field("st", FM->FREQM_STATUS, 1);
    put_field("if", FM->FREQM_INTFLAG, 1);
    put_field("v", FM->FREQM_VALUE, 8);
    nl();

    /* E13: latency at REF 46,875 Hz for REFNUM 1, 16, 255 */
    gen(1, S_OSC48M, 1024, 0);
    console_puts("E13 timing");
    for (unsigned int i = 0; i < 3U; i++) {
        static const uint8_t rn[] = { 1, 16, 255 };

        fm_setup(rn[i]);
        put_field("n", rn[i], 2);
        put_field("us", fm_run(100), 8);
        put_field("v", FM->FREQM_VALUE, 6);
    }
    nl();
}

static void sources(void)
{
    /* REF: OSC48M / 1024 = 46,875 Hz on GCLK1 */
    gen(1, S_OSC48M, 1024, 0);
    osc_report();
    OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K = 0U;
    OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K = OSC32KCTRL_XOSC32K_ENABLE_Msk |
                                          OSC32KCTRL_XOSC32K_EN32K_Msk;
    delay_ms(200);
    OSCCTRL_REGS->OSCCTRL_XOSCCTRL = OSCCTRL_XOSCCTRL_ENABLE_Msk;
    delay_ms(5);
    osc_report();
    if (gen(5, S_OSCULP32K, 1, 0)) {
        msr(5);
        row("S6 osculp32k", 255, 46875, 2000);
    }
    OSC32KCTRL_REGS->OSC32KCTRL_OSC32K = OSC32KCTRL_OSC32K_ENABLE_Msk |
                                         OSC32KCTRL_OSC32K_EN32K_Msk |
                                         OSC32KCTRL_OSC32K_CALIB(0x3F);
    delay_ms(5);
    if (gen(6, S_OSC32K, 1, 0)) {
        msr(6);
        row("S4 osc32k_cal3f", 255, 46875, 2000);
        OSC32KCTRL_REGS->OSC32KCTRL_OSC32K =
            OSC32KCTRL_OSC32K_ENABLE_Msk | OSC32KCTRL_OSC32K_EN32K_Msk |
            OSC32KCTRL_OSC32K_CALIB((OTP5[0] >> 12) & 0x7FU);
        delay_ms(5);
        row("S5 osc32k_otp", 255, 46875, 2000);
    }
    console_puts("otp5");
    put_field("osc32k_cal", (OTP5[0] >> 12) & 0x7FU, 2);
    nl();

    /* FDPLL96M from the GCLK reference: GCLK8 = OSC48M / 48 = 1 MHz, x 96 */
    gen(8, S_OSC48M, 48, 0);
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
    put_field("rdy_us", now_us(), 8);
    put_field("status", OSCCTRL_REGS->OSCCTRL_DPLLSTATUS, 2);
    nl();
    if (gen(7, S_DPLL, 1, 0)) {
        msr(7);
        row("S11 dpll96m", 255, 46875, 2000);
        OSCCTRL_REGS->OSCCTRL_DPLLPRESC = 1U;
        while (OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY != 0U) {
        }
        row("S9.1 dpll48m", 255, 46875, 2000);
        OSCCTRL_REGS->OSCCTRL_DPLLPRESC = 2U;
        while (OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY != 0U) {
        }
        row("S9.2 dpll24m", 255, 46875, 2000);
    }
    /* The board's oscillators last: a dead one leaves its generator stuck */
    if (gen(4, S_XOSC, 1, 0)) {
        msr(4);
        row("S3 xosc_ext", 255, 46875, 2000);
    }
}

/*
 * XOSC32K, first thing (GCLK3 stays stuck if it does not run): crystal
 * mode with the longest start-up, then (round 2, run separately) external
 * clock mode. Its register survives every reset but power-on.
 */
static void xosc32k_first(void)
{
    gen(1, S_OSC48M, 1024, 0);
    osc_report();
    OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K = 0U;
    OSC32KCTRL_REGS->OSC32KCTRL_XOSC32K = OSC32KCTRL_XOSC32K_ENABLE_Msk |
                                          OSC32KCTRL_XOSC32K_EN32K_Msk |
                                          OSC32KCTRL_XOSC32K_XTALEN_Msk |
                                          OSC32KCTRL_XOSC32K_STARTUP(6);
    delay_ms(1500);
    osc_report();
    if (gen(3, S_XOSC32K, 1, 0)) {
        msr(3);
        row("S7b xosc32k_xtal", 255, 46875, 2000);
    }
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP freqmprobe: " BOARD_BANNER "\r\n");
    console_puts("reset");
    put_field("ctrla", FM->FREQM_CTRLA, 2);
    put_field("cfga", FM->FREQM_CFGA, 4);
    put_field("status", FM->FREQM_STATUS, 2);
    put_field("value", FM->FREQM_VALUE, 8);
    nl();
    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL &= (uint8_t)~OSCCTRL_OSC48MCTRL_ONDEMAND_Msk;
    xosc32k_first();
    ratios();
    behaviour();
    sources();
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
