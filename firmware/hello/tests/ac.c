/*
 * AC check (PIC32CM MC00 Curiosity Pro, AC U2245): the analog comparators
 * against what silicon did in firmware/hello APP=acprobe and acprobe2
 * (docs/research/peripherals/ac-u2245.md). No wiring: the inputs are the
 * DAC (CTRLB.IOEN), VSCALE, INTREF and PA04/PA05 driven as GPIO outputs.
 * Register behaviour, flags, window states, events and pads are checked
 * exactly; trip codes, hysteresis and timing within a range, because they
 * follow the part and its supply (VCC_SEL). A failed check names what it
 * expected and ends its line in FAIL; PP:EXIT <failures>.
 *
 * Uses PA04/PA05 (Arduino A2/A3), PA18 (CMP0) and PA24 (LED0, which blinks
 * once); PA06/PA07 stay on VCOM.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define AC              AC_REGS
#define PA              (&PORT_REGS->GROUP[0])
#define TICKS_PER_MS    (CPU_CLOCK_HZ / 1000U)

#define NEG(x)          AC_COMPCTRL_MUXNEG(x)
#define POS(x)          AC_COMPCTRL_MUXPOS(x)
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
#define VS_DAC          (POS(P_VSCALE) | NEG(N_DAC))

static volatile unsigned int irq_count;

void AC_Handler(void)
{
    uint8_t flags = AC->AC_INTFLAG;

    AC->AC_INTFLAG = flags;
    if (flags & AC_INTFLAG_COMP0_Msk) {
        irq_count++;
    }
}

/* put_field(), counting a failure outside @lo..@hi */
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

/* " ok", or " FAIL" when a check on the line failed */
static void end(void)
{
    static unsigned int seen;

    console_puts(failures == seen ? " ok\r\n" : " FAIL\r\n");
    seen = failures;
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

/* Comparator @n off, @val written, then on; waits for READY */
static void ac_cfg(unsigned int n, uint32_t val)
{
    ac_off(n);
    AC->AC_COMPCTRL[n] = val;
    AC->AC_COMPCTRL[n] = val | EN;
    ac_sync();
    ac_ready(n);
}

static void ctrla(uint8_t v)
{
    AC->AC_CTRLA = v;
    ac_sync();
}

static unsigned int state(unsigned int n)
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

/* The first DAC code where VSCALE(@v) > DAC no longer holds */
static unsigned int trip_code(unsigned int v)
{
    unsigned int lo = 0, hi = 1023;

    AC->AC_SCALER[0] = (uint8_t)v;
    while (lo < hi) {
        unsigned int mid = (lo + hi) / 2U;

        dac_set((uint16_t)mid);
        if (state(0)) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static void check_reset(void)
{
    console_puts("reset");
    put_checked("ctrla", AC->AC_CTRLA, 0, 2);
    put_checked("evctrl", AC->AC_EVCTRL, 0, 4);
    put_checked("inten", AC->AC_INTENSET, 0, 2);
    put_checked("intflag", AC->AC_INTFLAG, 0, 2);
    put_checked("statusa", AC->AC_STATUSA, 0, 2);
    put_checked("statusb", AC->AC_STATUSB, 0, 2);
    put_checked("winctrl", AC->AC_WINCTRL, 0, 2);
    put_checked("compctrl0", AC->AC_COMPCTRL[0], 0, 8);
    end();
    console_puts("masks");
    AC->AC_EVCTRL = 0xFFFFU;
    put_checked("evctrl", AC->AC_EVCTRL, 0x3313, 4);
    AC->AC_INTENSET = 0xFFU;
    put_checked("inten", AC->AC_INTENSET, 0x13, 2);
    AC->AC_INTENCLR = 0xFFU;
    AC->AC_WINCTRL = 0xFFU;
    ac_sync();
    put_checked("winctrl", AC->AC_WINCTRL, 0x07, 2);
    AC->AC_SCALER[0] = 0xFFU;
    put_checked("scaler0", AC->AC_SCALER[0], 0x3F, 2);
    AC->AC_COMPCTRL[0] = 0xFFFFFFFDU;       /* everything but ENABLE */
    put_checked("compctrl0", AC->AC_COMPCTRL[0], 0x373BF75C, 8);
    AC->AC_CTRLB = 0xFFU;
    put_checked("ctrlb", AC->AC_CTRLB, 0, 2);
    end();
    ctrla(AC_CTRLA_SWRST_Msk);
}

/* COMPCTRL is locked by its own ENABLE; EVCTRL by CTRLA.ENABLE */
static void check_writes(void)
{
    const uint32_t a = VS_DAC | HIGH;

    console_puts("writes");
    ac_cfg(0, a);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | HIGH | HYST | EN;
    put_checked("while_on", AC->AC_COMPCTRL[0], a | EN, 8);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | HIGH;
    ac_sync();
    put_checked("disable_keeps", AC->AC_COMPCTRL[0], a, 8);
    AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_AIN1) | HIGH | EN;
    ac_sync();
    put_checked("enable_takes", AC->AC_COMPCTRL[0],
                POS(P_VSCALE) | NEG(N_AIN1) | HIGH | EN, 8);
    AC->AC_EVCTRL = AC_EVCTRL_COMPEO0_Msk;
    put_checked("evctrl_on", AC->AC_EVCTRL, 0, 4);
    AC->AC_SCALER[0] = 50U;
    put_checked("scaler_on", AC->AC_SCALER[0], 50, 2);
    ac_off(0);
    AC->AC_COMPCTRL[0] = 3U << 20;          /* undocumented, writable */
    put_checked("bits21_20", AC->AC_COMPCTRL[0], 3U << 20, 8);
    ac_off(0);
    end();
}

static void check_timing(void)
{
    uint32_t t0, c;

    console_puts("timing");
    for (unsigned int sp = 0; sp < 2U; sp++) {
        uint32_t speed = sp ? HIGH : 0U;

        ac_off(0);
        AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | speed;
        t0 = SysTick->VAL;
        AC->AC_COMPCTRL[0] = POS(P_VSCALE) | NEG(N_GND) | speed | EN;
        while (!(AC->AC_STATUSB & 1U)) {
        }
        /* silicon: 358 (7.5 us) and 133 (2.8 us) ticks at 48 MHz */
        if (sp) {
            put_range("high_ready", ticks_since(t0), 80, 200, 4);
        } else {
            put_range("low_ready", ticks_since(t0), 250, 500, 4);
        }
    }
    ac_cfg(0, POS(P_VSCALE) | NEG(N_GND) | HIGH | SINGLE);
    t0 = SysTick->VAL;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    c = AC->AC_STATUSB & 1U;
    while (!(AC->AC_STATUSB & 1U)) {
    }
    put_range("shot", ticks_since(t0), 70, 200, 4);    /* silicon 123 */
    put_checked("ready_at_start", c, 0, 1);
    ac_off(0);
    end();
}

static void check_trips(uint32_t vdd_mv)
{
    static const uint8_t vs[] = { 7, 31, 47 };     /* inside VICM at 3.3 V */
    unsigned int up, down, first = 64;

    ac_cfg(0, VS_DAC | HIGH);
    for (unsigned int i = 0; i < sizeof(vs); i++) {
        uint32_t expect = (1023U * (vs[i] + 1U) + 32U) / 64U;

        console_puts("trip");                       /* silicon: +0..+2 */
        put_field("v", vs[i], 2);
        put_range("dac", trip_code(vs[i]), expect - 4U, expect + 4U, 4);
        end();
    }
    /* Without IOEN the DAC input reads high: STATE 0 at every code */
    console_puts("dac_no_ioen");
    dac_ctrlb(DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    put_checked("trip", trip_code(31), 0, 4);
    dac_ctrlb(DAC_CTRLB_IOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    end();

    /* Hysteresis: about 60 mV either side, no wider without HYSTEN */
    for (unsigned int h = 0; h < 3U; h++) {
        static const uint32_t cfg[] = { HIGH, HYST, HIGH | HYST };
        static const char *const names[] = { "hyst_off", "hyst_low",
                                             "hyst_high" };

        ac_cfg(0, VS_DAC | cfg[h]);
        AC->AC_SCALER[0] = 31U;
        up = down = 0;
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
        console_puts(names[h]);
        if (h) {                                    /* silicon 62 mV */
            put_range("mv_each_side", (up - down) * vdd_mv / 2046U, 35, 90,
                      4);
        } else {
            put_range("codes", up - down, 0, 3, 4);
        }
        end();
    }

    /* VSCALE against INTREF (1.024 V): the first code above it */
    ac_cfg(0, POS(P_VSCALE) | NEG(N_INTREF) | HIGH);
    for (unsigned int v = 0; v < 64U && first == 64U; v++) {
        AC->AC_SCALER[0] = (uint8_t)v;
        if (state(0)) {
            first = v;
        }
    }
    console_puts("intref");
    put_range("first", first, 64U * 1024U / vdd_mv - 1U,
              64U * 1024U / vdd_mv + 1U, 2);
    end();
}

/* INTFLAG per INTSEL as STATE goes 1 -> 0 -> 1 */
static void check_flags(void)
{
    static const char *const names[] = { "toggle", "rising", "falling",
                                         "eoc" };
    static const uint8_t expect[4][3] = {
        { 0, 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 1, 0, 0 },
    };

    AC->AC_SCALER[0] = 31U;
    for (unsigned int s = 0; s < 4U; s++) {
        dac_set(400);                               /* v=31 above: 1 */
        AC->AC_INTFLAG = 0xFFU;
        ac_cfg(0, VS_DAC | HIGH | AC_COMPCTRL_INTSEL(s));
        AC->AC_SCALER[0] = 31U;
        delay_us(50);
        console_puts(names[s]);
        put_checked("first", AC->AC_INTFLAG, expect[s][0], 2);
        AC->AC_INTFLAG = 0xFFU;
        dac_set(620);
        delay_us(50);
        put_checked("fall", AC->AC_INTFLAG, expect[s][1], 2);
        AC->AC_INTFLAG = 0xFFU;
        dac_set(400);
        delay_us(50);
        put_checked("rise", AC->AC_INTFLAG, expect[s][2], 2);
        AC->AC_INTFLAG = 0xFFU;
        end();
    }
    /* Single-shot: EOC per START; TOGGLE only when the result changes */
    console_puts("single");
    ac_cfg(0, VS_DAC | HIGH | SINGLE | AC_COMPCTRL_INTSEL(3));
    AC->AC_SCALER[0] = 31U;
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    delay_us(10);
    put_checked("eoc", AC->AC_INTFLAG, 1, 2);
    ac_cfg(0, VS_DAC | HIGH | SINGLE | AC_COMPCTRL_INTSEL(0));
    AC->AC_SCALER[0] = 31U;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    AC->AC_INTFLAG = 0xFFU;
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    delay_us(10);
    put_checked("tog_same", AC->AC_INTFLAG, 0, 2);
    AC->AC_INTFLAG = 0xFFU;
    dac_set(620);
    AC->AC_CTRLB = AC_CTRLB_START0_Msk;
    ac_ready(0);
    delay_us(10);
    put_checked("tog_diff", AC->AC_INTFLAG, 1, 2);
    put_checked("state", AC->AC_STATUSA, 0, 2);
    AC->AC_INTFLAG = 0xFFU;
    ac_off(0);
    end();
}

static void check_window(void)
{
    static const uint8_t pairs[4][2] = { {50, 50}, {50, 10}, {10, 50},
                                         {10, 10} };
    static const uint8_t wstate[4] = { 0x03, 0x11, 0x12, 0x20 };
    static const uint16_t walk[] = { 800, 512, 200, 512, 800, 800, 512, 512 };
    /* INTFLAG then STATUSA after each step of the walk (silicon) */
    static const uint8_t flags[4][7] = {
        { 0x01, 0x12, 0x02, 0x01, 0x00, 0x01, 0x00 },
        { 0x11, 0x02, 0x12, 0x01, 0x00, 0x11, 0x00 },
        { 0x01, 0x02, 0x02, 0x11, 0x00, 0x01, 0x00 },
        { 0x01, 0x12, 0x02, 0x11, 0x00, 0x01, 0x00 },
    };
    static const uint8_t st[7] = { 0x11, 0x03, 0x11, 0x20, 0x20, 0x11, 0x11 };

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
    console_puts("window");
    for (unsigned int i = 0; i < 4U; i++) {
        AC->AC_SCALER[0] = pairs[i][0];
        AC->AC_SCALER[1] = pairs[i][1];
        delay_us(30);
        put_checked("statusa", AC->AC_STATUSA, wstate[i], 2);
    }
    end();
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
            if (k) {
                put_checked("f", AC->AC_INTFLAG, flags[w][k - 1U], 2);
                put_checked("st", AC->AC_STATUSA, st[k - 1U], 2);
            }
            AC->AC_INTFLAG = 0xFFU;
        }
        end();
    }
    ac_off(0);
    ac_off(1);
    ctrla(0);
    AC->AC_WINCTRL = 0U;
    ac_sync();
    ctrla(AC_CTRLA_ENABLE_Msk);
}

static void check_pads(void)
{
    /* A GPIO-driven PA04 reaches AIN0 whatever its PMUX */
    console_puts("ain0_gpio");
    PA->PORT_PINCFG[4] = PORT_PINCFG_INEN_Msk;
    PA->PORT_DIRSET = 1U << 4;
    ac_cfg(0, POS(P_AIN0) | NEG(N_VSCALE) | HIGH);
    AC->AC_SCALER[0] = 31U;
    PA->PORT_OUTSET = 1U << 4;
    put_checked("high", state(0), 1, 1);
    PA->PORT_OUTCLR = 1U << 4;
    put_checked("low", state(0), 0, 1);
    end();
    /* AIN0 vs AIN1, and SWAP: the logical result stays */
    console_puts("ain0_ain1");
    PA->PORT_PINCFG[5] = PORT_PINCFG_INEN_Msk;
    PA->PORT_DIRSET = (1U << 4) | (1U << 5);
    PA->PORT_OUTSET = 1U << 4;
    PA->PORT_OUTCLR = 1U << 5;
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH);
    put_checked("hi_lo", state(0), 1, 1);
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH | AC_COMPCTRL_SWAP_Msk);
    put_checked("swap", state(0), 1, 1);
    PA->PORT_OUTCLR = 1U << 4;
    PA->PORT_OUTSET = 1U << 5;
    ac_cfg(0, POS(P_AIN0) | NEG(N_AIN1) | HIGH);
    put_checked("lo_hi", state(0), 0, 1);
    PA->PORT_DIRCLR = (1U << 4) | (1U << 5);
    PA->PORT_PINCFG[4] = 0U;
    PA->PORT_PINCFG[5] = 0U;
    end();
    /* CMP0 on PA18 (function H): follows STATE, 0 with OUT off */
    console_puts("cmp0_pa18");
    PA->PORT_PINCFG[18] = PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_INEN_Msk;
    PA->PORT_PMUX[9] = (uint8_t)((PA->PORT_PMUX[9] & 0xF0U) |
                                 PORT_PMUX_PMUXE_H);
    for (unsigned int o = 1; o < 3U; o++) {
        ac_cfg(0, VS_DAC | HIGH | AC_COMPCTRL_OUT(o));
        AC->AC_SCALER[0] = 31U;
        dac_set(400);
        put_checked(o == 1U ? "async1" : "sync1", (PA->PORT_IN >> 18) & 1U,
                    1, 1);
        dac_set(620);
        put_checked(o == 1U ? "async0" : "sync0", (PA->PORT_IN >> 18) & 1U,
                    0, 1);
    }
    ac_cfg(0, VS_DAC | HIGH);
    dac_set(400);
    PA->PORT_PINCFG[18] |= PORT_PINCFG_PULLEN_Msk;
    PA->PORT_OUTSET = 1U << 18;
    put_checked("off_pullup", (PA->PORT_IN >> 18) & 1U, 0, 1);
    PA->PORT_PINCFG[18] = 0U;
    PA->PORT_OUTCLR = 1U << 18;
    end();
}

static void check_events(void)
{
    /* COMP0's event, a level copy of STATE, drives PA24 (PORT EV0 OUT) */
    console_puts("comp_event");
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_AC_COMP_0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 1U;
    PA->PORT_EVCTRL = PORT_EVCTRL_PID0(24) |
                      PORT_EVCTRL_EVACT0(PORT_EVCTRL_EVACT0_OUT_Val) |
                      PORT_EVCTRL_PORTEI0_Msk;
    PA->PORT_DIRSET = 1U << 24;
    PA->PORT_PINCFG[24] = PORT_PINCFG_INEN_Msk;
    ctrla(0);
    AC->AC_EVCTRL = AC_EVCTRL_COMPEO0_Msk;
    ctrla(AC_CTRLA_ENABLE_Msk);
    ac_cfg(0, VS_DAC | HIGH);
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    put_checked("st1_pa24", (PA->PORT_IN >> 24) & 1U, 1, 1);
    dac_set(620);
    put_checked("st0_pa24", (PA->PORT_IN >> 24) & 1U, 0, 1);
    PA->PORT_EVCTRL = 0U;
    PA->PORT_OUTSET = 1U << 24;                     /* LED0 off */
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    end();

    /* A software event on SOC0 starts a single shot, inverted or not */
    console_puts("soc_event");
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_AC_SOC_0] = 2U;    /* channel 1 */
    ctrla(0);
    AC->AC_EVCTRL = AC_EVCTRL_COMPEI0_Msk;
    ctrla(AC_CTRLA_ENABLE_Msk);
    ac_cfg(0, VS_DAC | HIGH | SINGLE | AC_COMPCTRL_INTSEL(3));
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    AC->AC_INTFLAG = 0xFFU;
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_checked("eoc", AC->AC_INTFLAG, 1, 2);
    put_checked("state", AC->AC_STATUSA, 1, 2);
    AC->AC_INTFLAG = 0xFFU;
    ctrla(0);
    AC->AC_EVCTRL = AC_EVCTRL_COMPEI0_Msk | AC_EVCTRL_INVEI0_Msk;
    ctrla(AC_CTRLA_ENABLE_Msk);
    ac_ready(0);
    AC->AC_INTFLAG = 0xFFU;
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_checked("inv_eoc", AC->AC_INTFLAG, 1, 2);
    ac_cfg(0, VS_DAC | HIGH | AC_COMPCTRL_INTSEL(3));
    delay_us(20);
    AC->AC_INTFLAG = 0xFFU;
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
    put_checked("continuous", AC->AC_INTFLAG, 0, 2);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_AC_SOC_0] = 0U;
    ctrla(0);
    AC->AC_EVCTRL = 0U;
    ctrla(AC_CTRLA_ENABLE_Msk);
    end();
}

static void check_irq(void)
{
    console_puts("irq");
    ac_cfg(0, VS_DAC | HIGH | AC_COMPCTRL_INTSEL(0));
    AC->AC_SCALER[0] = 31U;
    dac_set(400);
    delay_us(50);
    AC->AC_INTFLAG = 0xFFU;
    irq_count = 0;
    AC->AC_INTENSET = AC_INTENSET_COMP0_Msk;
    NVIC_ClearPendingIRQ(AC_IRQn);
    NVIC_EnableIRQ(AC_IRQn);
    dac_set(620);
    delay_us(50);
    dac_set(400);
    delay_us(50);
    NVIC_DisableIRQ(AC_IRQn);
    AC->AC_INTENCLR = 0xFFU;
    put_checked("toggles", irq_count, 2, 2);
    end();
}

static void check_swrst(void)
{
    console_puts("swrst");
    ctrla(AC_CTRLA_SWRST_Msk);
    put_checked("ctrla", AC->AC_CTRLA, 0, 2);
    put_checked("compctrl0", AC->AC_COMPCTRL[0], 0, 8);
    put_checked("scaler0", AC->AC_SCALER[0], 0, 2);
    put_checked("statusb", AC->AC_STATUSB, 0, 2);
    end();
}

int main(void)
{
    uint32_t vdd_mv;

    sys_initialize();
    systick_start();
    console_puts("PP ac: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_AC_Msk |
                                MCLK_APBCMASK_DAC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    clock_on(AC_GCLK_ID);
    clock_on(DAC_GCLK_ID);
    check_reset();

    adc0_init(0, 4);
    vdd_mv = adc_vddana_mv();
    console_puts("vddana");
    put_range("mv", vdd_mv, 1620, 5500, 4);
    end();
    dac_ctrlb(DAC_CTRLB_IOEN_Msk | DAC_CTRLB_REFSEL_VDDANA);
    ctrla(AC_CTRLA_ENABLE_Msk);

    check_writes();
    check_timing();
    check_trips(vdd_mv);
    check_flags();
    check_window();
    check_pads();
    check_events();
    check_irq();
    check_swrst();
    test_exit();
}
