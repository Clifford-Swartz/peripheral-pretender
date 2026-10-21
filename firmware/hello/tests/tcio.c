/*
 * TC outputs and capture check (PIC32CM MC00, TC U2249): the waveform
 * outputs on pins and the capture modes against what silicon did in
 * firmware/hello APP=tcprobe2..4 (docs/research/peripherals/tc-u2249.md).
 * Run: py -m tools.periph check mc00 tcio
 *
 * Bare EV15N46A: TC3 WO0/WO1 on PB22/PB23 watched through PORT.IN
 * (scope.h); capture events from PB09 (EXTINT9, pull resistor as the
 * stimulus) through an asynchronous EVSYS channel. A tick is 4/3 us
 * (COUNT8/16, DIV64 from 48 MHz). Waveforms are measured from the first
 * falling edge on, since silicon starts a tick or so after the enable;
 * times in microseconds, within 2 us. Captures are checked as differences
 * and durations in ticks, within 2. Nothing is printed while timing.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T3              (TC3_REGS->COUNT8)
#define T3_16           (TC3_REGS->COUNT16)
#define GB              1U
#define WO0             22U
#define WO1             23U
#define STIM            9U
#define FUNC_A          0U
#define FUNC_E          4U
#define MASK_WO         ((1U << WO0) | (1U << WO1))
#define IN_B            (&PORT_REGS->GROUP[GB].PORT_IN)
#define GEN_EIC9        23U
#define USER_TC3        26U
#define NE              48U
#define MC0             TC_INTFLAG_MC0_Msk
#define MC1             TC_INTFLAG_MC1_Msk
#define ERR             TC_INTFLAG_ERR_Msk
#define C8_DIV64        (TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV64)
#define C16_DIV64       (TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64)
/* n ticks in us, rounded */
#define TICKS_US(n)     (((n) * 4U + 1U) / 3U)

static ScopeEdge e[NE];
static unsigned int ne;

static void wait_us(uint32_t us)
{
    scope_wait_until(scope_now(), SCOPE_CYC(us));
}

static void sync3(void)
{
    for (uint32_t i = 0; T3.TC_SYNCBUSY != 0U && i < 100000U; i++) {
    }
}

/* The times (us) at which @bit becomes @level, at most @max */
static unsigned int edges(unsigned int bit, int level, uint32_t *t,
                          unsigned int max)
{
    unsigned int k = 0;

    for (unsigned int i = 1; i < ne && k < max; i++) {
        if (((e[i].v ^ e[i - 1].v) >> bit) & 1U &&
            (int)((e[i].v >> bit) & 1U) == level) {
            t[k++] = SCOPE_US(e[i].t);
        }
    }
    return k;
}

/* The first rise after @after (us), or 0 */
static uint32_t rise_after(unsigned int bit, uint32_t after)
{
    uint32_t r[8];
    unsigned int n = edges(bit, 1, r, 8);

    for (unsigned int i = 0; i < n; i++) {
        if (r[i] > after) {
            return r[i];
        }
    }
    return 0;
}

/*
 * " name: first_fall=.. period=.. high=..": PWM-like output @bit, from its
 * falling edges (period) and the rise between the first two (low time)
 */
static void check_pwm(const char *name, unsigned int bit, uint32_t fall0,
                      uint32_t period, uint32_t high)
{
    uint32_t f[4];
    unsigned int n = edges(bit, 0, f, 4);

    console_putc(' ');
    console_puts(name);
    console_putc(':');
    if (n < 2) {
        put_checked("falls", n, 2, 1);
        return;
    }
    put_near("first_fall", (int32_t)f[0], (int32_t)fall0 + 1, 2);
    put_near("period", (int32_t)(f[1] - f[0]), (int32_t)period, 2);
    put_near("high", (int32_t)(f[1] - rise_after(bit, f[0])), (int32_t)high,
             2);
}

/* NFRQ/MFRQ: toggles every @half us, the first at @first */
static void check_toggle(const char *name, unsigned int bit, uint32_t first,
                         uint32_t half)
{
    uint32_t r[2], f[2];

    console_putc(' ');
    console_puts(name);
    console_putc(':');
    if (edges(bit, 1, r, 2) < 2 || edges(bit, 0, f, 2) < 1) {
        put_checked("toggles", 0, 1, 1);
        return;
    }
    put_near("first", (int32_t)r[0], (int32_t)first + 1, 2);
    put_near("half", (int32_t)(f[0] - r[0]), (int32_t)half, 2);
    put_near("period", (int32_t)(r[1] - r[0]), (int32_t)(2 * half), 2);
}

static void wave(uint8_t wavegen, uint8_t per, uint8_t cc0, uint8_t cc1,
                 uint8_t drv, int down, uint32_t us)
{
    uint32_t t0;

    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = C8_DIV64;
    T3.TC_WAVE = wavegen;
    T3.TC_DRVCTRL = drv;
    T3.TC_PER = per;
    T3.TC_CC[0] = cc0;
    T3.TC_CC[1] = cc1;
    if (down) {
        T3.TC_CTRLBSET = TC_CTRLBSET_DIR_Msk;
    }
    sync3();
    t0 = scope_now();
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(us), e, NE);
}

static void check_npwm(void)
{
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 450);
    console_puts("npwm_up");
    check_pwm("wo0", WO0, TICKS_US(25), TICKS_US(100), TICKS_US(25));
    check_pwm("wo1", WO1, TICKS_US(75), TICKS_US(100), TICKS_US(75));
    end_line();
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 1, 450);
    console_puts("npwm_down");
    /* Down: set at CCx, cleared at the reload: high CCx + 1 ticks */
    check_pwm("wo0", WO0, TICKS_US(100), TICKS_US(100), TICKS_US(26));
    check_pwm("wo1", WO1, TICKS_US(100), TICKS_US(100), TICKS_US(76));
    end_line();
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, TC_DRVCTRL_INVEN0_Msk, 0, 450);
    console_puts("inven0");
    check_pwm("wo0", WO0, TICKS_US(100), TICKS_US(100), TICKS_US(75));
    end_line();
}

static void check_freq(void)
{
    wave(TC_WAVE_WAVEGEN_NFRQ, 99, 25, 75, 0, 0, 450);
    console_puts("nfrq");
    check_toggle("wo0", WO0, TICKS_US(25), TICKS_US(100));
    end_line();
    wave(TC_WAVE_WAVEGEN_MFRQ, 99, 49, 20, 0, 0, 450);
    console_puts("mfrq");
    check_toggle("wo0", WO0, TICKS_US(49), TICKS_US(50));
    check_toggle("wo1", WO1, TICKS_US(20), TICKS_US(50));
    end_line();
}

static void check_mpwm_edges(void)
{
    uint32_t f[3], r[3];
    unsigned int nf, nr;

    wave(TC_WAVE_WAVEGEN_MPWM, 99, 99, 25, 0, 0, 450);
    console_puts("mpwm");
    nf = edges(WO0, 0, f, 3);
    nr = edges(WO0, 1, r, 3);
    put_checked("wo0_falls", nf, 3, 1);
    if (nf == 3) {
        /* A one-tick low pulse on WO0 at TOP, every period */
        put_near("wo0_period", (int32_t)(f[1] - f[0]),
                 (int32_t)TICKS_US(100), 2);
        put_near("wo0_pulse", (int32_t)(rise_after(WO0, f[0]) - f[0]),
                 (int32_t)TICKS_US(1), 1);
    }
    (void)nr;
    check_pwm("wo1", WO1, TICKS_US(25), TICKS_US(100), TICKS_US(25));
    end_line();
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 0, 99, 0, 0, 450);
    console_puts("edges");
    nf = edges(WO0, 0, f, 3) + edges(WO0, 1, r, 3);
    put_checked("cc0_0_changes", nf, 0, 1);
    put_checked("cc0_0_level", (e[ne - 1].v >> WO0) & 1U, 0, 1);
    nf = edges(WO1, 0, f, 3);
    put_checked("cc1_top_falls", nf, 3, 1);
    if (nf >= 1) {
        put_near("cc1_top_pulse", (int32_t)(rise_after(WO1, f[0]) - f[0]),
                 (int32_t)TICKS_US(1), 1);
    }
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 50, 120, 0, 0, 450);
    put_checked("cc1_above_falls", edges(WO1, 0, f, 3), 0, 1);
    put_checked("cc1_above_level", (e[ne - 1].v >> WO1) & 1U, 1, 1);
    end_line();
}

static void check_stops(void)
{
    uint32_t in_stop, st_stop, in_os, st_os, in_dis, f[3];
    unsigned int nos;

    wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 20);
    T3.TC_CTRLBSET = TC_CTRLBSET_CMD_STOP;
    sync3();
    in_stop = PORT_REGS->GROUP[GB].PORT_IN & MASK_WO;
    st_stop = T3.TC_STATUS;
    /* One-shot: one period, then the outputs are low */
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = C8_DIV64;
    T3.TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    T3.TC_PER = 99;
    T3.TC_CC[0] = 25;
    T3.TC_CC[1] = 75;
    T3.TC_CTRLBSET = TC_CTRLBSET_ONESHOT_Msk;
    sync3();
    {
        uint32_t t0 = scope_now();

        T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
        ne = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(400), e, NE);
    }
    nos = edges(WO0, 0, f, 3);
    in_os = PORT_REGS->GROUP[GB].PORT_IN & MASK_WO;
    st_os = T3.TC_STATUS;
    wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 20);
    T3.TC_CTRLA &= ~TC_CTRLA_ENABLE_Msk;
    sync3();
    in_dis = PORT_REGS->GROUP[GB].PORT_IN & MASK_WO;
    console_puts("stops");
    put_checked("stop_pins", in_stop, 0, 8);
    put_checked("stop_status", st_stop & TC_STATUS_STOP_Msk,
                TC_STATUS_STOP_Msk, 2);
    put_checked("oneshot_falls", nos, 1, 1);
    put_checked("oneshot_pins", in_os, 0, 8);
    put_checked("oneshot_status", st_os & TC_STATUS_STOP_Msk,
                TC_STATUS_STOP_Msk, 2);
    put_checked("disabled_pins", in_dis, 0, 8);
    end_line();
}

static void check_count16(void)
{
    uint32_t t0;

    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3_16.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV1;
    T3_16.TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    T3_16.TC_CC[0] = 16384;
    T3_16.TC_CC[1] = 49152;
    sync3();
    t0 = scope_now();
    T3_16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(3000), e, NE);
    console_puts("count16");
    check_pwm("wo0", WO0, 341, 1365, 341);
    check_pwm("wo1", WO1, 1024, 1365, 1024);
    end_line();
}

/* ---- capture ---- */

static void stim_setup(void)
{
    scope_pmux(GB, STIM, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GB, STIM, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG1 = 0x4U << (4U * (STIM - 8U));     /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << STIM;
    EIC_REGS->EIC_EVCTRL = 1U << STIM;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC9) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_TC3] = 1U;
}

static void pulses(unsigned int n, uint32_t hi, uint32_t lo)
{
    for (unsigned int i = 0; i < n; i++) {
        uint32_t t = scope_now();

        scope_pull(GB, STIM, 1);
        scope_wait_until(t, SCOPE_CYC(hi));
        scope_pull(GB, STIM, 0);
        scope_wait_until(t, SCOPE_CYC(hi + lo));
    }
}

static void capture8(uint32_t capten, uint16_t evact, uint8_t per)
{
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = C8_DIV64 | capten;
    T3.TC_PER = per;
    T3.TC_EVCTRL = evact | TC_EVCTRL_TCEI_Msk;
    sync3();
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
}

static void capture16(uint16_t evact)
{
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3_16.TC_CTRLA = C16_DIV64 | TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_CAPTEN1_Msk;
    T3_16.TC_EVCTRL = evact | TC_EVCTRL_TCEI_Msk;
    sync3();
    T3_16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
}

static void check_stamp(void)
{
    uint32_t cc[3], fl[3], t0, a, ifr, b, if3;

    capture8(TC_CTRLA_CAPTEN0_Msk, TC_EVCTRL_EVACT_STAMP, 200);
    t0 = scope_now();
    for (unsigned int i = 0; i < 3; i++) {
        scope_wait_until(t0, SCOPE_CYC(50U + 60U * i));
        pulses(1, 10, 10);
        fl[i] = T3.TC_INTFLAG;
        cc[i] = T3.TC_CC[0];
    }
    /* The FIFO: two captures, then a third with both full */
    capture8(TC_CTRLA_CAPTEN0_Msk, TC_EVCTRL_EVACT_STAMP, 200);
    pulses(1, 10, 40);
    pulses(1, 10, 40);
    a = T3.TC_CC[0];
    ifr = T3.TC_INTFLAG;
    b = T3.TC_CC[0];
    pulses(3, 10, 20);
    if3 = T3.TC_INTFLAG;
    console_puts("stamp");
    put_checked("mc0", fl[0] & MC0, MC0, 2);
    put_checked("mc1_at_start", fl[0] & MC1, MC1, 2);  /* CC1 = 0 */
    put_near("step1", (int32_t)(cc[1] - cc[0]), 45, 2);
    put_near("step2", (int32_t)(cc[2] - cc[1]), 45, 2);
    /*
     * Whether MC0 reads clear right after the read that moved the buffer
     * up depends on the instructions in between (cleared in tcprobe2,
     * set again here): only the FIFO's order is checked
     */
    (void)ifr;
    put_near("fifo_step", (int32_t)(b - a), 38, 2);
    put_checked("err", if3 & ERR, ERR, 2);
    end_line();
}

static void check_overflow(void)
{
    uint32_t fl, cc0, cc1;

    /* STAMP captures MAX at an overflow, on every capture channel */
    capture8(TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_CAPTEN1_Msk,
             TC_EVCTRL_EVACT_STAMP, 20);
    wait_us(40);
    fl = T3.TC_INTFLAG;
    cc0 = T3.TC_CC[0];
    cc1 = T3.TC_CC[1];
    console_puts("overflow");
    put_checked("mc", fl & (MC0 | MC1), MC0 | MC1, 2);
    put_checked("cc0", cc0, 0xFF, 2);
    put_checked("cc1", cc1, 0xFF, 2);
    end_line();
}

/* PPW (@pwp: PWP): two pulses 200 us high, 300 us low */
static void check_period(const char *label, uint16_t evact, int pwp)
{
    uint32_t c[4], fl;
    unsigned int per = pwp ? 1U : 0U;

    capture16(evact);
    pulses(2, 200, 300);
    fl = T3_16.TC_INTFLAG;
    c[0] = T3_16.TC_CC[per];
    c[1] = T3_16.TC_CC[!per];
    wait_us(5);
    c[2] = T3_16.TC_CC[per];
    c[3] = T3_16.TC_CC[!per];
    console_puts(label);
    put_checked("mc", fl & (MC0 | MC1), MC0 | MC1, 2);
    put_checked("err", fl & ERR, 0, 2);
    put_range("first_period", c[0], 0, 3, 4);
    put_near("width", (int32_t)c[1], 150, 2);
    put_near("period", (int32_t)c[2], 375, 2);
    put_near("width2", (int32_t)c[3], 150, 2);
    end_line();
}

static void check_pw(void)
{
    uint32_t a, b, st, ia, fl;

    capture16(TC_EVCTRL_EVACT_PW);
    pulses(2, 200, 300);
    fl = T3_16.TC_INTFLAG;
    st = T3_16.TC_STATUS;
    a = T3_16.TC_CC[0];
    wait_us(5);
    b = T3_16.TC_CC[0];
    capture16(TC_EVCTRL_EVACT_PW | TC_EVCTRL_TCINV_Msk);
    pulses(3, 200, 300);
    ia = T3_16.TC_CC[0];
    console_puts("pw");
    put_checked("mc0", fl & MC0, MC0, 2);
    put_checked("stopped", st & TC_STATUS_STOP_Msk, TC_STATUS_STOP_Msk, 2);
    put_near("width", (int32_t)a, 150, 2);
    put_near("width2", (int32_t)b, 150, 2);
    /* Inverted: the low phase, and nothing before the first pulse */
    put_near("inv_low", (int32_t)ia, 225, 2);
    end_line();
}

static void check_plain(void)
{
    uint32_t cc[2];

    for (unsigned int inv = 0; inv < 2; inv++) {
        capture8(TC_CTRLA_CAPTEN0_Msk,
                 TC_EVCTRL_EVACT_OFF | (inv ? TC_EVCTRL_TCINV_Msk : 0), 200);
        pulses(1, 100, 10);
        cc[inv] = T3.TC_CC[0];
    }
    console_puts("plain");
    put_range("rise", cc[0], 0, 4, 2);
    put_near("fall", (int32_t)cc[1], 76, 3);
    end_line();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tcio: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC2_Msk |
                                MCLK_APBCMASK_TC3_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC3_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TC3_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 100000U; i++) {
    }
    scope_pmux(GB, WO0, FUNC_E, 0);
    scope_pmux(GB, WO1, FUNC_E, 0);
    scope_start();
    check_npwm();
    check_freq();
    check_mpwm_edges();
    check_stops();
    check_count16();
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    stim_setup();
    check_stamp();
    check_overflow();
    check_period("ppw", TC_EVCTRL_EVACT_PPW, 0);
    check_period("pwp", TC_EVCTRL_EVACT_PWP, 1);
    check_pw();
    check_plain();
    T3.TC_CTRLA = 0;
    scope_release(GB, WO0);
    scope_release(GB, WO1);
    scope_release(GB, STIM);
    test_exit();
}
