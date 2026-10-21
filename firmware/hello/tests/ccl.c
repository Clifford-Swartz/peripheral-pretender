/*
 * CCL check (PIC32CM MC00, CCL U2225): registers and their protection,
 * truth tables and the input order, IO pads, LINK, FEEDBACK, sequencers,
 * the output event, the TCC and TC taps, filter and edge timing, the
 * event input, JK toggling and the SERCOM input, against what silicon
 * did in firmware/hello APP=cclprobe and cclprobe2
 * (docs/research/peripherals/ccl-u2225.md).
 * Run: py -m tools.periph check mc00 ccl
 *
 * Bare EV15N46A. Inputs are pads muxed to the CCL with their pulls
 * turned by PORT.OUT (LUT0 IN0 = PA04, LUT1 IN3/4/5 = PA08/PA09/PB10);
 * outputs read on OUT0 PB23, OUT1 PB11, OUT2 PB09 and LUT3 through its
 * event (asynchronous EVSYS channel 0, PORT event 0 OUT on PB02). Timing
 * at GCLK_CCL = OSCULP32K / 32 (1024 Hz, 977 us a cycle) in ranges of
 * cycles; counting through LUT0's event into TC4.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define CC              CCL_REGS
#define GA              0U
#define GB              1U
#define FUNC_I          8U
#define C_SWRST         0x01U
#define C_ENABLE        0x02U
#define L_ENABLE        (1U << 1)
#define L_SYNCH         (1U << 4)
#define L_FILTER        (2U << 4)
#define L_EDGESEL       (1U << 7)
#define L_LUTEI         (1U << 21)
#define L_LUTEO         (1U << 22)
#define MASK            0U
#define FEEDBACK        1U
#define LINK            2U
#define EVENT           3U
#define IO              4U
#define TC              6U
#define ALTTC           7U
#define TCCIN           8U
#define SERCOM          9U
#define DFF             1U
#define JK              2U
#define LATCH           3U
#define RS              4U
#define T4              (TC4_REGS->COUNT16)
#define CYCLE_US        977U
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

static tcc_registers_t *const tccs[3] = { TCC0_REGS, TCC1_REGS, TCC2_REGS };
static tc_registers_t *const tcs[5] = {
    TC0_REGS, TC1_REGS, TC2_REGS, TC3_REGS, TC4_REGS
};

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 2);
}

static uint32_t lut(uint32_t in0, uint32_t in1, uint32_t in2, uint32_t truth,
                    uint32_t extra)
{
    return (in0 << 8) | (in1 << 12) | (in2 << 16) | (truth << 24) | extra;
}

static void pins_settle(void)
{
    delay_us(60);
}

static void ccl_setup(uint8_t seq0, uint32_t l0, uint32_t l1, uint32_t l2,
                      uint32_t l3)
{
    CC->CCL_CTRL = C_SWRST;
    CC->CCL_SEQCTRL[0] = seq0;
    CC->CCL_LUTCTRL[0] = l0;
    CC->CCL_LUTCTRL[1] = l1;
    CC->CCL_LUTCTRL[2] = l2;
    CC->CCL_LUTCTRL[3] = l3;
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
}

static unsigned int out(unsigned int n)
{
    static const uint8_t pins[4] = { 23, 11, 9, 2 };

    return (PORT_REGS->GROUP[GB].PORT_IN >> pins[n]) & 1U;
}

/* LUT1's inputs PA08, PA09, PB10 pulled to bits 0-2 of @v */
static void in_lut1(unsigned int v)
{
    scope_pull(GA, 8, v & 1U);
    scope_pull(GA, 9, (v >> 1) & 1U);
    scope_pull(GB, 10, (v >> 2) & 1U);
    pins_settle();
}

static void check_registers(void)
{
    console_puts("registers");
    CC->CCL_CTRL = C_SWRST;
    put_exact("ctrl", CC->CCL_CTRL, 0);
    CC->CCL_SEQCTRL[0] = 0xFF;
    CC->CCL_LUTCTRL[1] = 0xFFFFFFFFU;
    put_exact("seq_mask", CC->CCL_SEQCTRL[0], 0x0F);
    put_range("lut_mask", CC->CCL_LUTCTRL[1], 0xFF7FFFB2U, 0xFF7FFFB2U, 8);
    CC->CCL_CTRL = C_SWRST;
    put_exact("swrst", CC->CCL_LUTCTRL[1], 0);
    /* CCL on: LUTCTRL takes ENABLE only; SEQCTRL and RUNSTDBY nothing */
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x11, 0);
    CC->CCL_CTRL = C_ENABLE;
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x33, L_ENABLE);
    put_range("ccl_on", CC->CCL_LUTCTRL[1], 0x11000002U, 0x11000002U, 8);
    CC->CCL_SEQCTRL[0] = 2;
    put_exact("seq_on", CC->CCL_SEQCTRL[0], 0);
    CC->CCL_CTRL = C_ENABLE | 0x40U;
    put_exact("runstdby_on", CC->CCL_CTRL, C_ENABLE);
    /* CCL off: a LUT that was on takes ENABLE only; off to on takes all */
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x22, 0);
    put_range("lut_was_on", CC->CCL_LUTCTRL[1], 0x11000000U, 0x11000000U, 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x44, L_ENABLE);
    put_range("off_to_on", CC->CCL_LUTCTRL[1], 0x44000002U, 0x44000002U, 8);
    CC->CCL_SEQCTRL[0] = 2;
    put_exact("seq_off", CC->CCL_SEQCTRL[0], 2);
    end_line();
}

static void check_masked(void)
{
    static const uint8_t t[] = { 0x00, 0x01, 0xFE, 0xFF };
    uint32_t got = 0;

    console_puts("masked");
    for (unsigned int i = 0; i < sizeof(t); i++) {
        ccl_setup(0, 0, lut(MASK, MASK, MASK, t[i], L_ENABLE), 0, 0);
        got |= out(1) << i;
    }
    put_exact("truth0", got, 0xA);              /* OUT = TRUTH[0] */
    /* off drives the pad to 0: the pull-up does not show */
    PORT_REGS->GROUP[GB].PORT_PINCFG[11] |= PORT_PINCFG_PULLEN_Msk;
    scope_pull(GB, 11, 1);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0xFF, 0);
    pins_settle();
    put_exact("lut_off", out(1), 0);
    CC->CCL_CTRL = 0;
    pins_settle();
    put_exact("ccl_off", out(1), 0);
    scope_pull(GB, 11, 0);
    PORT_REGS->GROUP[GB].PORT_PINCFG[11] &= ~PORT_PINCFG_PULLEN_Msk;
    end_line();
}

static void check_truth(void)
{
    uint32_t bad = 0;

    console_puts("truth");
    for (unsigned int b = 0; b < 8; b++) {
        unsigned int seen = 0;

        ccl_setup(0, 0, lut(IO, IO, IO, 1U << b, L_ENABLE), 0, 0);
        for (unsigned int v = 0; v < 8; v++) {
            in_lut1(v);
            seen |= out(1) << v;
        }
        bad |= (seen != (1U << b)) << b;
    }
    /* each one-hot TRUTH lights only its input combination, IN0 = IN3 */
    put_exact("one_hot_bad", bad, 0);
    /* an IO input needs the pad muxed: a GPIO-driven pad reads 0 */
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    PORT_REGS->GROUP[GA].PORT_PINCFG[8] = PORT_PINCFG_INEN_Msk;
    PORT_REGS->GROUP[GA].PORT_DIRSET = 1U << 8;
    PORT_REGS->GROUP[GA].PORT_OUTSET = 1U << 8;
    pins_settle();
    put_exact("gpio_pad", out(1), 0);
    PORT_REGS->GROUP[GA].PORT_DIRCLR = 1U << 8;
    scope_pmux(GA, 8, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    end_line();
}

static void check_link(void)
{
    console_puts("link");
    ccl_setup(0, lut(LINK, MASK, MASK, 0xAA, L_ENABLE),
              lut(IO, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    in_lut1(1);
    put_exact("lut0_1", out(0), 1);
    in_lut1(0);
    put_exact("lut0_0", out(0), 0);
    /* LUT3 reads LUT0 */
    ccl_setup(0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE), 0, 0,
              lut(LINK, MASK, MASK, 0xAA, L_ENABLE | L_LUTEO));
    put_exact("lut3_1", out(3), 1);
    ccl_setup(0, lut(MASK, MASK, MASK, 0x00, L_ENABLE), 0, 0,
              lut(LINK, MASK, MASK, 0xAA, L_ENABLE | L_LUTEO));
    put_exact("lut3_0", out(3), 0);
    /* FEEDBACK without a sequencer reads 0 */
    ccl_setup(0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE),
              lut(FEEDBACK, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    put_exact("feedback", out(1), 0);
    end_line();
}

/* Sequencer 0 from PA04 (bit 0: D, J, S) and PA08 (bit 1: G, K, R) */
static uint32_t seq_run(uint8_t mode, const uint8_t *steps, unsigned int n)
{
    uint32_t got = 0;

    scope_pull(GA, 4, 0);
    scope_pull(GA, 8, 0);
    pins_settle();
    CC->CCL_CTRL = C_SWRST;
    CC->CCL_SEQCTRL[0] = mode;
    CC->CCL_LUTCTRL[1] = lut(IO, MASK, MASK, 0xAA, L_ENABLE);
    CC->CCL_LUTCTRL[0] = lut(IO, MASK, MASK, 0xAA, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    CC->CCL_CTRL = C_ENABLE;
    for (unsigned int i = 0; i < n; i++) {
        scope_pull(GA, 4, steps[i] & 1U);
        scope_pull(GA, 8, (steps[i] >> 1) & 1U);
        pins_settle();
        got |= out(0) << i;
    }
    return got;
}

static void check_sequencer(void)
{
    static const uint8_t gated[] = { 0x1, 0x3, 0x1, 0x0, 0x2, 0x0, 0x3, 0x1 };
    static const uint8_t jk[] = { 0x0, 0x1, 0x0, 0x2, 0x0, 0x1, 0x3 };

    console_puts("sequencer");
    /* gated D: 0,1,1,1,0,0,1,1 (bit 0 first) */
    put_exact("dff", seq_run(DFF, gated, sizeof(gated)), 0xCE);
    put_exact("latch", seq_run(LATCH, gated, sizeof(gated)), 0xCE);
    /* hold, set, hold, clear, hold, set (JK; RS also with S = R = 1) */
    put_exact("jk", seq_run(JK, jk, 6), 0x26);
    put_exact("rs", seq_run(RS, jk, sizeof(jk)), 0x66);
    /* OUT1 stays the odd LUT's own output */
    put_exact("out1_raw", out(1), 1);
    CC->CCL_LUTCTRL[0] = lut(IO, MASK, MASK, 0xAA, 0);
    pins_settle();
    put_exact("even_off_clears", out(0), 0);
    end_line();
}

static void check_event_out(void)
{
    console_puts("event_out");
    ccl_setup(0, 0, 0, 0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE | L_LUTEO));
    put_exact("lutout_1", out(3), 1);
    ccl_setup(0, 0, 0, 0, lut(MASK, MASK, MASK, 0x00, L_ENABLE | L_LUTEO));
    put_exact("lutout_0", out(3), 0);
    ccl_setup(0, 0, 0, 0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE));
    put_exact("no_luteo", out(3), 0);
    end_line();
}

static void tcc_static(unsigned int n, unsigned int v)
{
    tcc_registers_t *t = tccs[n];

    t->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; t->TCC_SYNCBUSY && i < 200000U; i++) {
    }
    t->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    t->TCC_PER = 0xFF;
    for (unsigned int x = 0; x < 3; x++) {
        t->TCC_CC[x] = (v >> x) & 1U ? 0x100U : 0U;
    }
    t->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; t->TCC_SYNCBUSY && i < 200000U; i++) {
    }
    delay_us(30);
}

/* LUTn IN0-2 = TCC(n % 3) WO[0-2]: the inputs read for CC0-2 bits @v */
static unsigned int tcc_inputs(unsigned int n, unsigned int v)
{
    static const uint8_t buf[3] = { 0xAA, 0xCC, 0xF0 };
    unsigned int got = 0;

    tcc_static(n % 3, v);
    for (unsigned int x = 0; x < 3; x++) {
        uint32_t l = lut(TCCIN, TCCIN, TCCIN, buf[x], L_ENABLE | L_LUTEO);

        ccl_setup(0, n == 0 ? l : 0, n == 1 ? l : 0, n == 2 ? l : 0,
                  n == 3 ? l : 0);
        got |= out(n) << x;
    }
    return got;
}

static void check_tcc(void)
{
    static const char *const names[4] = { "lut0", "lut1", "lut2", "lut3" };

    console_puts("tcc_taps");
    for (unsigned int n = 0; n < 4; n++) {
        uint32_t bad = 0;

        for (unsigned int v = 0; v < 8; v++) {
            /* TCC1 and TCC2 have two channels: WO[2] is CC0's output */
            unsigned int want = (n == 1 || n == 2) ?
                                ((v & 1U) * 5U) | (v & 2U) : v;

            bad |= (tcc_inputs(n, v) != want) << v;
        }
        put_exact(names[n], bad, 0);
    }
    for (unsigned int n = 0; n < 3; n++) {
        tccs[n]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    }
    end_line();
}

static void tc_static(unsigned int m, int high)
{
    tc_count8_registers_t *t = &tcs[m]->COUNT8;

    t->TC_CTRLA = TC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; t->TC_SYNCBUSY && i < 200000U; i++) {
    }
    t->TC_CTRLA = TC_CTRLA_MODE_COUNT8;
    t->TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    t->TC_PER = 0x7F;
    t->TC_CC[0] = high ? 0xFFU : 0U;
    t->TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; t->TC_SYNCBUSY && i < 200000U; i++) {
    }
}

static void check_tc(void)
{
    uint32_t bad = 0;

    console_puts("tc_taps");
    for (unsigned int m = 0; m < 5; m++) {
        for (unsigned int k = 0; k < 5; k++) {
            tc_static(k, k == m);
        }
        delay_us(30);
        for (unsigned int n = 0; n < 4; n++) {
            for (unsigned int alt = 0; alt < 2; alt++) {
                uint32_t l = lut(alt ? ALTTC : TC, MASK, MASK, 0xAA,
                                 L_ENABLE | L_LUTEO);

                ccl_setup(0, n == 0 ? l : 0, n == 1 ? l : 0, n == 2 ? l : 0,
                          n == 3 ? l : 0);
                /* TC: TCn, ALTTC: TC(n + 1) */
                bad |= (out(n) != (n + alt == m)) << m;
            }
        }
    }
    put_exact("bad", bad, 0);
    for (unsigned int k = 0; k < 5; k++) {
        tcs[k]->COUNT8.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    }
    end_line();
}

static void gclk_ccl(unsigned int gen)
{
    GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] = 0;
    for (uint32_t i = 0; (GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] &
                          GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] = GCLK_PCHCTRL_GEN(gen) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
}

/* Microseconds until OUT1 reads @level after PA08 is pulled to it */
static uint32_t latency(unsigned int level)
{
    uint32_t t0;

    scope_pull(GA, 8, level);
    t0 = scope_now();
    while (out(1) != level && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    return scope_since(t0) / (CPU_CLOCK_HZ / 1000000U);
}

/* The longest and shortest latency over 12 changes at varied phases */
static void latency_range(uint32_t filt, uint32_t *lo, uint32_t *hi)
{
    *lo = 0xFFFFFFFFU;
    *hi = 0;
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA, L_ENABLE | filt), 0, 0);
    scope_pull(GA, 8, 0);
    delay_us(10000);
    for (unsigned int k = 0; k < 6; k++) {
        uint32_t a, b;

        delay_us(137 * k);
        a = latency(1);
        delay_us(8000);
        b = latency(0);
        delay_us(8000);
        *lo = MIN(*lo, MIN(a, b));
        *hi = MAX(*hi, MAX(a, b));
    }
}

static void check_timing(void)
{
    uint32_t lo, hi, t0, w;

    console_puts("timing");
    gclk_ccl(1);                                /* 1024 Hz */
    latency_range(0, &lo, &hi);
    put_dec_range("none_us", hi, 0, 50);
    /* SYNCH: the second edge after a change; FILTER: the fourth */
    latency_range(L_SYNCH, &lo, &hi);
    put_dec_range("synch_min", lo, CYCLE_US, 2 * CYCLE_US + 150);
    put_dec_range("synch_max", hi, CYCLE_US, 2 * CYCLE_US + 150);
    latency_range(L_FILTER, &lo, &hi);
    put_dec_range("filter_min", lo, 3 * CYCLE_US, 4 * CYCLE_US + 150);
    put_dec_range("filter_max", hi, 3 * CYCLE_US, 4 * CYCLE_US + 150);
    /* EDGESEL: a one-cycle pulse on a rising edge, none on a falling one */
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA,
                        L_ENABLE | L_SYNCH | L_EDGESEL), 0, 0);
    scope_pull(GA, 8, 0);
    delay_us(10000);
    scope_pull(GA, 8, 1);
    t0 = scope_now();
    while (!out(1) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    t0 = scope_now();
    while (out(1) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    w = scope_since(t0) / (CPU_CLOCK_HZ / 1000000U);
    put_dec_range("pulse_us", w, CYCLE_US - 100, CYCLE_US + 100);
    scope_pull(GA, 8, 0);
    t0 = scope_now();
    w = 0;
    while (scope_since(t0) < SCOPE_CYC(10000)) {
        w |= out(1);
    }
    put_exact("on_fall", w, 0);
    end_line();
}

static void counter_start(void)
{
    T4.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; T4.TC_SYNCBUSY && i < 200000U; i++) {
    }
    T4.TC_CTRLA = TC_CTRLA_MODE_COUNT16;
    T4.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    T4.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; T4.TC_SYNCBUSY && i < 200000U; i++) {
    }
}

static uint32_t counter_read(void)
{
    T4.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
    for (uint32_t i = 0; T4.TC_SYNCBUSY && i < 200000U; i++) {
    }
    return T4.TC_COUNT;
}

static void check_counted(void)
{
    uint32_t t0, n;

    console_puts("counted");
    gclk_ccl(2);                                /* 1 MHz */
    counter_start();
    /* JK with J = K = 1 toggles at half the clock: 1000 rises in 2 ms */
    ccl_setup(JK, lut(MASK, MASK, MASK, 0xFF, L_ENABLE | L_LUTEO),
              lut(MASK, MASK, MASK, 0xFF, L_ENABLE), 0, 0);
    n = counter_read();
    t0 = scope_now();
    scope_wait_until(t0, SCOPE_CYC(2000));
    put_dec_range("jk_2ms", counter_read() - n, 990, 1010);
    /* EVENT: one strobe per software event on an asynchronous channel */
    EVSYS_REGS->EVSYS_CHANNEL[2] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_CCL_LUTIN_0] = 3U;
    ccl_setup(0, lut(EVENT, MASK, MASK, 0xAA,
                     L_ENABLE | L_LUTEI | L_LUTEO), 0, 0, 0);
    n = counter_read();
    for (unsigned int k = 0; k < 10; k++) {
        EVSYS_REGS->EVSYS_SWEVT = 1U << 2;
        delay_us(50);
    }
    put_dec_range("event_strobes", counter_read() - n, 10, 10);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_CCL_LUTIN_0] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[2] = 0;
    /* SERCOM: the TX input reads 1, the SERCOM off or idle */
    ccl_setup(0, 0, lut(SERCOM, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    put_exact("sercom_idle", out(1), 1);
    end_line();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP ccl: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_CCL_Msk |
                                MCLK_APBCMASK_EVSYS_Msk |
                                MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_TCC1_Msk |
                                MCLK_APBCMASK_TCC2_Msk |
                                MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk |
                                MCLK_APBCMASK_TC2_Msk | MCLK_APBCMASK_TC3_Msk |
                                MCLK_APBCMASK_TC4_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSCULP32K |
                                 GCLK_GENCTRL_DIV(32) | GCLK_GENCTRL_GENEN_Msk;
    GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) | GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    gclk_ccl(0);
    {
        static const uint8_t ids[] = {
            TCC0_GCLK_ID, TCC1_GCLK_ID, TCC2_GCLK_ID, TC0_GCLK_ID,
            TC2_GCLK_ID, TC4_GCLK_ID, EVSYS_GCLK_ID_1
        };

        for (unsigned int i = 0; i < sizeof(ids); i++) {
            GCLK_REGS->GCLK_PCHCTRL[ids[i]] = GCLK_PCHCTRL_GEN(0) |
                                              GCLK_PCHCTRL_CHEN_Msk;
        }
    }
    scope_pmux(GA, 4, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, 8, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, 9, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GB, 10, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GB, 23, FUNC_I, 0);
    scope_pmux(GB, 11, FUNC_I, 0);
    scope_pmux(GB, 9, FUNC_I, 0);
    PORT_REGS->GROUP[GB].PORT_DIRSET = 1U << 2;
    PORT_REGS->GROUP[GB].PORT_PINCFG[2] = PORT_PINCFG_INEN_Msk;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = PORT_EVCTRL_PID0(2) |
                                       PORT_EVCTRL_PORTEI0_Msk;
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_CCL_LUTOUT_3) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 1U;
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_CCL_LUTOUT_0) |
                                   EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                                   EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC4_EVU] = 2U;

    check_registers();
    check_masked();
    check_truth();
    check_link();
    check_sequencer();
    check_event_out();
    check_tcc();
    check_tc();
    scope_start();
    check_timing();
    check_counted();

    CC->CCL_CTRL = C_SWRST;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC4_EVU] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[1] = 0;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = 0;
    PORT_REGS->GROUP[GB].PORT_DIRCLR = 1U << 2;
    scope_release(GA, 4);
    scope_release(GA, 8);
    scope_release(GA, 9);
    scope_release(GB, 10);
    scope_release(GB, 23);
    scope_release(GB, 11);
    scope_release(GB, 9);
    test_exit();
}
