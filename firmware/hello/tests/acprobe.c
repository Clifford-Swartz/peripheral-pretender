/*
 * AC probe (PIC32CM MC00, AC U2245): what the analog comparators do on
 * silicon, for the model. Bare EV15N46A, no wiring. Prints raw results,
 * one experiment per block (docs/research/peripherals/ac-u2245.md):
 *
 *   E0  reset values, writable masks       E7  timing (SysTick ticks)
 *   E2  VSCALE vs INTREF, VREFOE 0/1        E8  INTFLAG per INTSEL
 *   E1  VDDANA                              E9  window mode
 *   E3  write rules while enabled           E10 CMP output on PA18
 *   E4  pads: PA04 as GPIO, pulls, SWAP     E11 COMP event to PORT (PA24)
 *   E5  VSCALE vs DAC trip codes            E12 start event (single-shot)
 *   E6  hysteresis                          E13 SWRST
 *
 * PA04/PA05 (Arduino A2/A3) are the inputs; PA06/PA07 stay on VCOM; PA17
 * (EDBG data gateway SS) and PA12/PA13 (I2C) are left alone.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define AC              AC_REGS
#define PA              (&PORT_REGS->GROUP[0])
#define TICKS_PER_MS    (CPU_CLOCK_HZ / 1000U)

/* COMPCTRL fields */
#define NEG(x)          AC_COMPCTRL_MUXNEG(x)
#define POS(x)          AC_COMPCTRL_MUXPOS(x)
#define N_AIN0          0U
#define N_AIN1          1U
#define N_GND           4U
#define N_VSCALE        5U
#define N_INTREF        6U
#define N_DAC           7U
#define P_AIN0          0U
#define P_VSCALE        4U
#define HIGH            AC_COMPCTRL_SPEED(3)
#define HYST            AC_COMPCTRL_HYSTEN_Msk
#define SINGLE          AC_COMPCTRL_SINGLE_Msk
#define EN              AC_COMPCTRL_ENABLE_Msk

static void nl(void)
{
    console_puts("\r\n");
}

static void title(const char *s)
{
    console_puts(s);
}

/* SysTick down-counter ticks since @t0 (under a millisecond) */
static uint32_t ticks_since(uint32_t t0)
{
    uint32_t now = SysTick->VAL;

    return t0 >= now ? t0 - now : t0 + TICKS_PER_MS - now;
}

static int ac_sync(void)
{
    for (uint32_t n = 0; AC->AC_SYNCBUSY != 0U; n++) {
        if (n == WAIT_LOOPS) {
            return -1;
        }
    }
    return 0;
}

static int ac_ready(unsigned int n)
{
    for (uint32_t i = 0; (AC->AC_STATUSB & (1U << n)) == 0U; i++) {
        if (i == WAIT_LOOPS) {
            return -1;
        }
    }
    return 0;
}

/* Comparator @n off, @val written, then on; waits for READY */
static int ac_cfg(unsigned int n, uint32_t val)
{
    AC->AC_COMPCTRL[n] = 0U;
    ac_sync();
    AC->AC_COMPCTRL[n] = val;
    AC->AC_COMPCTRL[n] = val | EN;
    ac_sync();
    return ac_ready(n);
}

static unsigned int ac_state(unsigned int n)
{
    delay_us(20);
    return (AC->AC_STATUSA >> n) & 1U;
}

static void dac_sync(void)
{
    for (uint32_t n = 0; DAC_REGS->DAC_SYNCBUSY != 0U && n < WAIT_LOOPS;
         n++) {
    }
}

static void dac_ctrlb(uint8_t ctrlb)
{
    DAC_REGS->DAC_CTRLA = 0U;
    dac_sync();
    DAC_REGS->DAC_CTRLB = ctrlb;
    DAC_REGS->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    for (uint32_t n = 0; !(DAC_REGS->DAC_STATUS & DAC_STATUS_READY_Msk) &&
         n < WAIT_LOOPS; n++) {
    }
}

static void dac_set(uint16_t code)
{
    DAC_REGS->DAC_DATA = code;
    dac_sync();
    delay_us(30);
}

static void clock_on(unsigned int id)
{
    GCLK_REGS->GCLK_PCHCTRL[id] = GCLK_PCHCTRL_GEN_GCLK0 |
                                  GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[id] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
}

static void ac_enable(void)
{
    AC->AC_CTRLA = AC_CTRLA_ENABLE_Msk;
    ac_sync();
}

static void ac_reset(void)
{
    AC->AC_CTRLA = AC_CTRLA_SWRST_Msk;
    ac_sync();
}

/* PA@pin: analog (PMUX B) or GPIO */
static void pad_analog(unsigned int pin)
{
    PA->PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    PA->PORT_PMUX[pin / 2U] = (uint8_t)((PA->PORT_PMUX[pin / 2U] &
                                         ((pin & 1U) ? 0x0FU : 0xF0U)) |
                                        (PORT_PMUX_PMUXE_B <<
                                         ((pin & 1U) * 4U)));
}

/* Trip point: the first DAC code where VSCALE(@v) > DAC no longer holds */
static unsigned int trip_code(unsigned int v)
{
    unsigned int lo = 0, hi = 1023;

    AC->AC_SCALER[0] = (uint8_t)v;
    while (lo < hi) {
        unsigned int mid = (lo + hi) / 2U;

        dac_set((uint16_t)mid);
        if (ac_state(0)) {
            lo = mid + 1U;          /* VSCALE above the DAC */
        } else {
            hi = mid;
        }
    }
    return lo;
}

static void e0_reset(void)
{
    title("E0 reset");
    put_field("ctrla", AC->AC_CTRLA, 2);
    put_field("evctrl", AC->AC_EVCTRL, 4);
    put_field("inten", AC->AC_INTENSET, 2);
    put_field("intflag", AC->AC_INTFLAG, 2);
    put_field("statusa", AC->AC_STATUSA, 2);
    put_field("statusb", AC->AC_STATUSB, 2);
    put_field("winctrl", AC->AC_WINCTRL, 2);
    put_field("scaler0", AC->AC_SCALER[0], 2);
    put_field("compctrl0", AC->AC_COMPCTRL[0], 8);
    put_field("syncbusy", AC->AC_SYNCBUSY, 8);
    nl();
    title("E0 masks");
    AC->AC_EVCTRL = 0xFFFFU;
    put_field("evctrl", AC->AC_EVCTRL, 4);
    AC->AC_INTENSET = 0xFFU;
    put_field("inten", AC->AC_INTENSET, 2);
    AC->AC_INTENCLR = 0xFFU;
    AC->AC_DBGCTRL = 0xFFU;
    put_field("dbgctrl", AC->AC_DBGCTRL, 2);
    AC->AC_WINCTRL = 0xFFU;
    ac_sync();
    put_field("winctrl", AC->AC_WINCTRL, 2);
    AC->AC_SCALER[0] = 0xFFU;
    put_field("scaler0", AC->AC_SCALER[0], 2);
    AC->AC_COMPCTRL[0] = 0xFFFFFFFDU;       /* everything but ENABLE */
    put_field("compctrl0", AC->AC_COMPCTRL[0], 8);
    AC->AC_CTRLB = 0xFFU;
    put_field("ctrlb", AC->AC_CTRLB, 2);
    nl();
    ac_reset();
}

/* E2: SCALER0 sweep against INTREF; the 64 states as two words */
static void e2_intref(int vrefoe)
{
    static const uint8_t sel[] = { 0U, 2U, 3U };

    if (vrefoe) {
        SUPC_REGS->SUPC_VREF |= SUPC_VREF_VREFOE_Msk;
    } else {
        SUPC_REGS->SUPC_VREF &= ~SUPC_VREF_VREFOE_Msk;
    }
    for (unsigned int s = 0; s < sizeof(sel); s++) {
        uint32_t lo = 0, hi = 0;
        unsigned int first = 64;

        SUPC_REGS->SUPC_VREF = (SUPC_REGS->SUPC_VREF &
                                ~SUPC_VREF_SEL_Msk) | SUPC_VREF_SEL(sel[s]);
        delay_ms(1);
        ac_cfg(0, POS(P_VSCALE) | NEG(N_INTREF) | HIGH);
        for (unsigned int v = 0; v < 64U; v++) {
            unsigned int st;

            AC->AC_SCALER[0] = (uint8_t)v;
            st = ac_state(0);
            if (v < 32U) {
                lo |= st << v;
            } else {
                hi |= st << (v - 32U);
            }
            if (st && first == 64U) {
                first = v;
            }
        }
        title("E2 intref");
        put_field("vrefoe", (uint32_t)vrefoe, 1);
        put_field("sel", sel[s], 1);
        put_field("first", first, 2);
        put_field("lo", lo, 8);
        put_field("hi", hi, 8);
        nl();
    }
}

static void e3_writes(void)
{
    title("E3 writes");
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH);
    dac_set(512);
    AC->AC_SCALER[0] = 10U;
    put_field("st10", ac_state(0), 1);
    AC->AC_SCALER[0] = 50U;
    put_field("st50", ac_state(0), 1);
    put_field("scaler", AC->AC_SCALER[0], 2);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | HIGH | EN;
    put_field("negwhileon", AC->AC_COMPCTRL[0], 8);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_DAC) | HIGH | HYST | EN;
    put_field("hystwhileon", AC->AC_COMPCTRL[0], 8);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | HIGH;   /* off+change */
    ac_sync();
    put_field("offchange", AC->AC_COMPCTRL[0], 8);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_AIN1) | HIGH | EN; /* on+chg */
    ac_sync();
    put_field("onchange", AC->AC_COMPCTRL[0], 8);
    AC->AC_EVCTRL = AC_EVCTRL_COMPEO0_Msk;
    put_field("evctrl_on", AC->AC_EVCTRL, 4);
    AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk;
    ac_sync();
    put_field("winctrl_on", AC->AC_WINCTRL, 2);
    AC->AC_WINCTRL = 0U;
    ac_sync();
    nl();
}

static void e4_pads(void)
{
    /* a) PA04 a GPIO output, input enabled, vs VDDANA/2 */
    title("E4 gpio");
    PA->PORT_PINCFG[4] = PORT_PINCFG_INEN_Msk;
    PA->PORT_DIRSET = 1U << 4;
    ac_cfg(0, POS(P_AIN0) | NEG(N_VSCALE) | HIGH);
    AC->AC_SCALER[0] = 31U;
    PA->PORT_OUTSET = 1U << 4;
    put_field("out1", ac_state(0), 1);
    put_field("adc1", adc_read(4, ADC_REFCTRL_REFSEL_INTVCC2), 4);
    PA->PORT_OUTCLR = 1U << 4;
    put_field("out0", ac_state(0), 1);
    put_field("adc0", adc_read(4, ADC_REFCTRL_REFSEL_INTVCC2), 4);
    nl();
    /* b) PMUX B with the pull-up, then pull-down */
    title("E4 pull");
    PA->PORT_DIRCLR = 1U << 4;
    pad_analog(4);
    PA->PORT_PINCFG[4] |= PORT_PINCFG_PULLEN_Msk;
    PA->PORT_OUTSET = 1U << 4;
    delay_ms(1);
    put_field("up", ac_state(0), 1);
    put_field("adcup", adc_read(4, ADC_REFCTRL_REFSEL_INTVCC2), 4);
    PA->PORT_OUTCLR = 1U << 4;
    delay_ms(1);
    put_field("down", ac_state(0), 1);
    put_field("adcdown", adc_read(4, ADC_REFCTRL_REFSEL_INTVCC2), 4);
    nl();
    /* d) PA04 high, PA05 low as GPIO; AIN0 vs AIN1, then swapped */
    title("E4 ain");
    PA->PORT_PINCFG[4] = PORT_PINCFG_INEN_Msk;
    PA->PORT_PINCFG[5] = PORT_PINCFG_INEN_Msk;
    PA->PORT_DIRSET = (1U << 4) | (1U << 5);
    PA->PORT_OUTSET = 1U << 4;
    PA->PORT_OUTCLR = 1U << 5;
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH);
    put_field("hi_lo", ac_state(0), 1);
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH | AC_COMPCTRL_SWAP_Msk);
    put_field("swap", ac_state(0), 1);
    PA->PORT_OUTCLR = 1U << 4;
    PA->PORT_OUTSET = 1U << 5;
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH);
    put_field("lo_hi", ac_state(0), 1);
    PA->PORT_DIRCLR = (1U << 4) | (1U << 5);
    PA->PORT_PINCFG[4] = 0U;
    PA->PORT_PINCFG[5] = 0U;
    nl();
}

static void e5_trips(void)
{
    static const uint8_t vs[] = { 0, 3, 7, 15, 23, 31, 39, 47, 55, 63 };

    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH);
    for (unsigned int i = 0; i < sizeof(vs); i++) {
        title("E5 scaler");
        put_field("v", vs[i], 2);
        put_field("dac", trip_code(vs[i]), 4);
        put_field("expect", (1023U * (vs[i] + 1U) + 32U) / 64U, 4);
        nl();
    }
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | AC_COMPCTRL_SWAP_Msk);
    title("E5 swap31");
    put_field("dac", trip_code(31), 4);         /* state inverted: see lo */
    nl();
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH);
    title("E5 ioen0");
    dac_ctrlb(DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    put_field("dac", trip_code(31), 4);
    dac_ctrlb(DAC_CTRLB_IOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    title(" eoen0");
    put_field("dac", trip_code(31), 4);
    dac_ctrlb(DAC_CTRLB_IOEN_Msk | DAC_CTRLB_EOEN_Msk |
              DAC_CTRLB_REFSEL_VDDANA);
    nl();
}

/* E6: ramp the DAC across the trip at v=31 both ways */
static void e6_hyst(uint32_t extra, const char *name)
{
    unsigned int c = 512, up = 0, down = 0;

    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | extra);
    AC->AC_SCALER[0] = 31U;
    for (unsigned int k = c - 100U; k <= c + 100U; k++) {
        dac_set((uint16_t)k);
        if (!ac_state(0)) {
            up = k;
            break;
        }
    }
    for (unsigned int k = c + 100U; k >= c - 100U; k--) {
        dac_set((uint16_t)k);
        if (ac_state(0)) {
            down = k;
            break;
        }
    }
    title("E6 hyst ");
    title(name);
    put_field("rise_to_0", up, 4);
    put_field("fall_to_1", down, 4);
    nl();
}

static void e7_timing(void)
{
    uint32_t t0, a, b, c;

    title("E7 enable");
    for (unsigned int sp = 0; sp < 2U; sp++) {
        uint32_t speed = sp ? HIGH : 0U;

        AC->AC_COMPCTRL[0] = 0U;
        ac_sync();
        AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | speed;
        t0 = SysTick->VAL;
        AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | speed | EN;
        while (AC->AC_SYNCBUSY != 0U) {
        }
        a = ticks_since(t0);
        while (!(AC->AC_STATUSB & 1U)) {
        }
        b = ticks_since(t0);
        put_field(sp ? "high_sync" : "low_sync", a, 4);
        put_field(sp ? "high_ready" : "low_ready", b, 4);
    }
    nl();
    title("E7 single");
    for (unsigned int f = 0; f < 3U; f++) {
        ac_cfg(0, POS(P_VSCALE) | NEG(N_GND) | HIGH | SINGLE |
                  AC_COMPCTRL_FLEN(f));
        t0 = SysTick->VAL;
        AC->AC_CTRLB = AC_CTRLB_START0_Msk;
        c = AC->AC_STATUSB & 1U;
        while (!(AC->AC_STATUSB & 1U)) {
        }
        a = ticks_since(t0);
        put_field("flen", f, 1);
        put_field("ready_after_start", c, 1);
        put_field("ticks", a, 4);
    }
    nl();
    title("E7 ctrla");
    t0 = SysTick->VAL;
    AC->AC_CTRLA = 0U;
    while (AC->AC_SYNCBUSY != 0U) {
    }
    put_field("disable", ticks_since(t0), 4);
    put_field("ready_ctrla_off", AC->AC_STATUSB, 2);
    t0 = SysTick->VAL;
    AC->AC_CTRLA = AC_CTRLA_ENABLE_Msk;
    while (AC->AC_SYNCBUSY != 0U) {
    }
    put_field("enable", ticks_since(t0), 4);
    AC->AC_COMPCTRL[0] &= ~EN;
    ac_sync();
    put_field("ready_comp_off", AC->AC_STATUSB, 2);
    put_field("state_comp_off", AC->AC_STATUSA, 2);
    nl();
}

/* E8: flags per INTSEL as STATE goes 1 -> 0 -> 1 */
static void e8_intsel(void)
{
    static const char *const names[] = { "toggle", "rising", "falling",
                                         "eoc" };

    for (unsigned int s = 0; s < 4U; s++) {
        uint32_t first, f10, f01;

        dac_set(400);                               /* v=31 above: 1 */
        AC->AC_INTFLAG = 0xFFU;
        ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | AC_COMPCTRL_INTSEL(s));
        AC->AC_SCALER[0] = 31U;
        delay_us(50);
        first = AC->AC_INTFLAG;
        AC->AC_INTFLAG = 0xFFU;
        dac_set(620);                               /* now 0 */
        delay_us(50);
        f10 = AC->AC_INTFLAG;
        AC->AC_INTFLAG = 0xFFU;
        dac_set(400);
        delay_us(50);
        f01 = AC->AC_INTFLAG;
        AC->AC_INTFLAG = 0xFFU;
        title("E8 ");
        title(names[s]);
        put_field("first", first, 2);
        put_field("fall", f10, 2);
        put_field("rise", f01, 2);
        nl();
    }
    /* single-shot: EOC per START; TOGGLE across equal and different shots */
    title("E8 single");
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | SINGLE |
              AC_COMPCTRL_INTSEL(3));
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    delay_us(10);
    put_field("eoc", AC->AC_INTFLAG, 2);
    AC->AC_INTFLAG = 0xFFU;
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | SINGLE |
              AC_COMPCTRL_INTSEL(0));
    AC->AC_SCALER[0] = 31U;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;             /* same result */
    ac_ready(0);
    delay_us(10);
    put_field("tog_same", AC->AC_INTFLAG, 2);
    AC->AC_INTFLAG = 0xFFU;
    dac_set(620);
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;             /* different */
    ac_ready(0);
    delay_us(10);
    put_field("tog_diff", AC->AC_INTFLAG, 2);
    put_field("state", AC->AC_STATUSA, 2);
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLB = 0x02U;                           /* START1: COMP1 off */
    put_field("start1_off", AC->AC_STATUSB, 2);
    nl();
}

static void e9_window(void)
{
    static const uint8_t pairs[4][2] = { {50, 50}, {50, 10}, {10, 50},
                                         {10, 10} };

    dac_set(512);
    AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk;           /* allowed while on? */
    ac_sync();
    AC->AC_CTRLA = 0U;
    ac_sync();
    AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk;
    ac_sync();
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_DAC) | HIGH | EN;
    AC->AC_COMPCTRL[1] = POS(P_VSCALE) | NEG(N_DAC) | HIGH | EN;
    ac_sync();
    ac_enable();
    ac_ready(0);
    ac_ready(1);
    for (unsigned int i = 0; i < 4U; i++) {
        AC->AC_SCALER[0] = pairs[i][0];
        AC->AC_SCALER[1] = pairs[i][1];
        delay_us(30);
        title("E9 win");
        put_field("s0", pairs[i][0], 2);
        put_field("s1", pairs[i][1], 2);
        put_field("statusa", AC->AC_STATUSA, 2);
        nl();
    }
    /* WIN0 per WINTSEL along BELOW, INSIDE, ABOVE, INSIDE, BELOW, stay */
    for (unsigned int w = 0; w < 4U; w++) {
        static const uint16_t walk[] = { 800, 512, 200, 512, 800, 800 };

        AC->AC_CTRLA = 0U;
        ac_sync();
        AC->AC_WINCTRL = AC_WINCTRL_WEN0_Msk | AC_WINCTRL_WINTSEL0(w);
        ac_sync();
        ac_enable();
        AC->AC_SCALER[0] = 40U;                     /* upper limit ~0.63 */
        AC->AC_SCALER[1] = 22U;                     /* lower ~0.36 VDDANA */
        title("E9 wintsel");
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
    AC->AC_CTRLA = 0U;
    ac_sync();
    AC->AC_WINCTRL = 0U;
    ac_sync();
    ac_enable();
}

static void e10_pad(void)
{
    title("E10 cmp0");
    PA->PORT_PINCFG[18] = PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_INEN_Msk;
    PA->PORT_PMUX[9] = (uint8_t)((PA->PORT_PMUX[9] & 0xF0U) |
                                 PORT_PMUX_PMUXE_H);
    for (unsigned int o = 1; o < 3U; o++) {
        ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | AC_COMPCTRL_OUT(o));
        AC->AC_SCALER[0] = 31U;
        dac_set(400);
        put_field(o == 1U ? "async1" : "sync1",
                  (PA->PORT_IN >> 18) & 1U, 1);
        dac_set(620);
        put_field(o == 1U ? "async0" : "sync0",
                  (PA->PORT_IN >> 18) & 1U, 1);
    }
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH);
    PA->PORT_PINCFG[18] |= PORT_PINCFG_PULLEN_Msk;
    PA->PORT_OUTSET = 1U << 18;
    put_field("off_pullup", (PA->PORT_IN >> 18) & 1U, 1);
    PA->PORT_OUTCLR = 1U << 18;
    put_field("off_pulldown", (PA->PORT_IN >> 18) & 1U, 1);
    PA->PORT_PINCFG[18] = 0U;
    nl();
}

/* E11: COMP0's event drives LED0 (PA24) through PORT EV0, action OUT */
static void e11_events(void)
{
    title("E11 event");
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(73) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[1] = 1U;                 /* PORT_EV_0 */
    PA->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                      PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_OUT_Val) |
                      PORT_EVCTRL_PORTEI0_Msk;
    PA->PORT_DIRSET = 1U << 24;
    PA->PORT_PINCFG[24] = PORT_PINCFG_INEN_Msk;
    AC->AC_CTRLA = 0U;
    ac_sync();
    AC->AC_EVCTRL = AC_EVCTRL_COMPEO0_Msk;
    ac_enable();
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH);
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    put_field("st1_pa24", (PA->PORT_IN >> 24) & 1U, 1);
    dac_set(620);
    put_field("st0_pa24", (PA->PORT_IN >> 24) & 1U, 1);
    dac_set(400);
    put_field("st1_pa24", (PA->PORT_IN >> 24) & 1U, 1);
    /* Toggle action: pulses per change? */
    PA->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                      PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_TGL_Val) |
                      PORT_EVCTRL_PORTEI0_Msk;
    PA->PORT_OUTCLR = 1U << 24;
    for (unsigned int k = 0; k < 4U; k++) {
        dac_set(k & 1U ? 400 : 620);
    }
    put_field("tgl4", (PA->PORT_IN >> 24) & 1U, 1);
    PA->PORT_EVCTRL = 0U;
    PA->PORT_OUTSET = 1U << 24;
    EVSYS_REGS->EVSYS_USER[1] = 0U;
    nl();
}

/* E12: a software event starts a single-shot comparison */
static void e12_start(void)
{
    title("E12 soc");
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[34] = 2U;                /* AC_SOC_0 <- ch 1 */
    AC->AC_CTRLA = 0U;
    ac_sync();
    AC->AC_EVCTRL = AC_EVCTRL_COMPEI0_Msk;
    ac_enable();
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | SINGLE |
              AC_COMPCTRL_INTSEL(3));
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    AC->AC_INTFLAG = 0xFFU;
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_field("intflag", AC->AC_INTFLAG, 2);
    put_field("state", AC->AC_STATUSA, 2);
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLA = 0U;
    ac_sync();
    AC->AC_EVCTRL = AC_EVCTRL_COMPEI0_Msk | AC_EVCTRL_INVEI0_Msk;
    ac_enable();
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_field("inv_intflag", AC->AC_INTFLAG, 2);
    AC->AC_INTFLAG = 0xFFU;
    ac_cfg(0, POS(P_VSCALE) | NEG(N_DAC) | HIGH | AC_COMPCTRL_INTSEL(3));
    AC->AC_INTFLAG = 0xFFU;
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_field("cont_intflag", AC->AC_INTFLAG, 2);
    EVSYS_REGS->EVSYS_USER[34] = 0U;
    nl();
}

static void e13_swrst(void)
{
    title("E13 swrst");
    AC->AC_CTRLA = AC_CTRLA_SWRST_Msk;
    put_field("ctrla_now", AC->AC_CTRLA, 2);
    put_field("sync_now", AC->AC_SYNCBUSY, 2);
    ac_sync();
    put_field("ctrla", AC->AC_CTRLA, 2);
    put_field("compctrl0", AC->AC_COMPCTRL[0], 8);
    put_field("scaler0", AC->AC_SCALER[0], 2);
    put_field("evctrl", AC->AC_EVCTRL, 4);
    put_field("statusb", AC->AC_STATUSB, 2);
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP acprobe: " BOARD_BANNER "\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_AC_Msk |
                                MCLK_APBCMASK_DAC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    clock_on(AC_GCLK_ID);
    clock_on(DAC_GCLK_ID);
    e0_reset();

    /* DAC on VDDANA, internal and external outputs */
    dac_ctrlb(DAC_CTRLB_IOEN_Msk | DAC_CTRLB_EOEN_Msk |
              DAC_CTRLB_REFSEL_VDDANA);
    ac_enable();

    e2_intref(0);                   /* before anything sets VREFOE */
    adc0_init(0, 4);                /* PA04 = ADC0 AIN4 */
    title("E1 vddana");
    put_field("mv", adc_vddana_mv(), 4);
    nl();
    e2_intref(1);
    SUPC_REGS->SUPC_VREF = (SUPC_REGS->SUPC_VREF & ~SUPC_VREF_SEL_Msk) |
                           SUPC_VREF_SEL(0);

    e3_writes();
    e4_pads();
    e5_trips();
    e6_hyst(0U, "low");
    e6_hyst(HYST, "low_hyst");
    e6_hyst(HIGH, "high");
    e6_hyst(HIGH | HYST, "high_hyst");
    e6_hyst(HIGH | HYST | SINGLE, "single_hyst");
    e7_timing();
    e8_intsel();
    e9_window();
    e10_pad();
    e11_events();
    e12_start();
    e13_swrst();
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
