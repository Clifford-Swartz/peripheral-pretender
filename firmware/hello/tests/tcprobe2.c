/*
 * TC probe 2 (PIC32CM MC00, TC U2249): waveform outputs on pins, and
 * capture, for the model. Prints raw results; checks nothing. The plan and
 * the answers: docs/research/peripherals/tc-u2249.md.
 * Run: py -m tools.periph probe mc00 tcprobe2
 *
 * Bare EV15N46A, nothing on the headers. TC3 WO0/WO1 drive PB22/PB23,
 * watched through PORT.IN (scope.h). Stimuli come from pull resistors:
 * PB09 (EXTINT9, ASYNCH, HIGH sense) feeds TC3's event input through an
 * asynchronous EVSYS channel; PB02 is TC2 WO0 as a capture pin (COPEN0).
 * GCLK_TC is 48 MHz; COUNT8 with DIV64 ticks every 1.333 us. Waveform
 * times are microseconds from the enable write.
 *
 *   E0   CTRLA writable bits (CAPTMODE?), DRVCTRL, levels of a disabled TC
 *   W1   NPWM up: PER 99, CC0 25, CC1 75
 *   W2   NPWM down
 *   W3   NFRQ: CC0 25, CC1 75
 *   W4   MFRQ: CC0 49 (TOP), CC1 20
 *   W5   MPWM: CC0 99 (TOP), CC1 25
 *   W6   INVEN0 on NPWM
 *   W7   NPWM CC0 0, CC1 = PER; CC1 120 > PER
 *   W8   levels after STOP, after a one-shot, after disable; RETRIGGER
 *   W9   COUNT16 NPWM, DIV1: TOP = MAX
 *   C1   STAMP on CC0: three pulses; read between them
 *   C2   STAMP, two pulses without a read (ERR); STAMP on both channels
 *   C3   PPW: high 200 us, low 300 us
 *   C4   PWP
 *   C5   PW
 *   C6   plain event capture (EVACT OFF), and with TCINV
 *   C7   capture on pin (COPEN0, STAMP) on TC2
 *   C8   STAMP across an overflow (MAX captured?)
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T3              (TC3_REGS->COUNT8)
#define T3_16           (TC3_REGS->COUNT16)
#define T2              (TC2_REGS->COUNT16)
#define GB              1U                      /* PORT group B */
#define WO0             22U                     /* PB22, TC3 WO0 */
#define WO1             23U                     /* PB23, TC3 WO1 */
#define STIM            9U                      /* PB09, EXTINT9 */
#define COP             2U                      /* PB02, TC2 WO0 */
#define FUNC_A          0U
#define FUNC_E          4U
#define MASK_WO         ((1U << WO0) | (1U << WO1))
#define IN_B            (&PORT_REGS->GROUP[GB].PORT_IN)
#define GEN_EIC9        23U
#define USER_TC3        26U
#define NE              48U

static ScopeEdge e[NE];

static void nl(void)
{
    console_puts("\r\n");
}

static void put_dec(const char *name, uint32_t v)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    console_put_dec(v);
}

static void wait_us(uint32_t us)
{
    scope_wait_until(scope_now(), SCOPE_CYC(us));
}

static void sync3(void)
{
    for (uint32_t i = 0; T3.TC_SYNCBUSY != 0U && i < 100000U; i++) {
    }
}

static void sync2(void)
{
    for (uint32_t i = 0; T2.TC_SYNCBUSY != 0U && i < 100000U; i++) {
    }
}

static void t3_reset(uint32_t ctrla)
{
    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3.TC_CTRLA = ctrla;
}

static void t3_enable(void)
{
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
}

static void t3_cmd(uint8_t cmd)
{
    T3.TC_CTRLBSET = cmd;
    sync3();
}

#define C8_DIV64        (TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV64)

/* COUNT8 waveform: configure, enable, record @us of WO0/WO1 */
static unsigned int wave(uint8_t wavegen, uint8_t per, uint8_t cc0,
                         uint8_t cc1, uint8_t drv, int down, uint32_t us)
{
    uint32_t t0;

    t3_reset(C8_DIV64);
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
    return scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(us), e, NE);
}

static void put_wave(const char *label, unsigned int n)
{
    console_puts(label);
    scope_put("wo0", e, n, WO0, 6);
    scope_put("wo1", e, n, WO1, 6);
    nl();
}

static void e0_regs(void)
{
    uint32_t ctrla, drv, in;

    t3_reset(0);
    T3.TC_CTRLA = 0xFFFFFFFEU & ~TC_CTRLA_ENABLE_Msk;
    ctrla = T3.TC_CTRLA;
    T3.TC_DRVCTRL = 0xFFU;
    drv = T3.TC_DRVCTRL;
    t3_reset(C8_DIV64);
    in = PORT_REGS->GROUP[GB].PORT_IN;
    console_puts("E0 regs");
    put_field("ctrla_ff", ctrla, 8);
    put_field("drvctrl_ff", drv, 2);
    put_field("disabled_wo0", (in >> WO0) & 1U, 1);
    put_field("disabled_wo1", (in >> WO1) & 1U, 1);
    nl();
}

static void w_modes(void)
{
    put_wave("W1 npwm_up", wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 450));
    put_wave("W2 npwm_down", wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 1,
                                  450));
    put_wave("W3 nfrq", wave(TC_WAVE_WAVEGEN_NFRQ, 99, 25, 75, 0, 0, 450));
    put_wave("W4 mfrq", wave(TC_WAVE_WAVEGEN_MFRQ, 99, 49, 20, 0, 0, 450));
    put_wave("W5 mpwm", wave(TC_WAVE_WAVEGEN_MPWM, 99, 99, 25, 0, 0, 450));
    put_wave("W6 inven0", wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75,
                               TC_DRVCTRL_INVEN0_Msk, 0, 450));
    put_wave("W7 cc0_0_cc1_per", wave(TC_WAVE_WAVEGEN_NPWM, 99, 0, 99, 0, 0,
                                      450));
    put_wave("W7 cc1_above_per", wave(TC_WAVE_WAVEGEN_NPWM, 99, 50, 120, 0,
                                      0, 450));
}

static void w8_stops(void)
{
    uint32_t in_stop, in_os, in_dis, st, t0;
    unsigned int n;

    (void)wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 20);  /* both high */
    t3_cmd(TC_CTRLBSET_CMD_STOP);
    in_stop = PORT_REGS->GROUP[GB].PORT_IN;
    st = T3.TC_STATUS;
    /* RETRIGGER from STOP: the waveform from the start again */
    t0 = scope_now();
    T3.TC_CTRLBSET = TC_CTRLBSET_CMD_RETRIGGER;
    n = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(300), e, NE);
    console_puts("W8 stop");
    put_field("wo0", (in_stop >> WO0) & 1U, 1);
    put_field("wo1", (in_stop >> WO1) & 1U, 1);
    put_field("status", st, 2);
    console_puts(" | retrigger:");
    scope_put("wo0", e, n, WO0, 4);
    scope_put("wo1", e, n, WO1, 4);
    nl();
    /* One-shot: one period, then */
    t3_reset(C8_DIV64);
    T3.TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    T3.TC_PER = 99;
    T3.TC_CC[0] = 25;
    T3.TC_CC[1] = 75;
    T3.TC_CTRLBSET = TC_CTRLBSET_ONESHOT_Msk;
    sync3();
    t0 = scope_now();
    T3.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    n = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(400), e, NE);
    in_os = PORT_REGS->GROUP[GB].PORT_IN;
    console_puts("W8 oneshot");
    scope_put("wo0", e, n, WO0, 4);
    scope_put("wo1", e, n, WO1, 4);
    put_field("after_wo0", (in_os >> WO0) & 1U, 1);
    put_field("status", T3.TC_STATUS, 2);
    nl();
    /* Disable while both are high */
    (void)wave(TC_WAVE_WAVEGEN_NPWM, 99, 25, 75, 0, 0, 20);
    T3.TC_CTRLA &= ~TC_CTRLA_ENABLE_Msk;
    sync3();
    in_dis = PORT_REGS->GROUP[GB].PORT_IN;
    console_puts("W8 disable");
    put_field("wo0", (in_dis >> WO0) & 1U, 1);
    put_field("wo1", (in_dis >> WO1) & 1U, 1);
    nl();
}

static void w9_count16(void)
{
    uint32_t t0;
    unsigned int n;

    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3_16.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV1;
    T3_16.TC_WAVE = TC_WAVE_WAVEGEN_NPWM;
    T3_16.TC_CC[0] = 16384;
    T3_16.TC_CC[1] = 49152;
    sync3();
    t0 = scope_now();
    T3_16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    n = scope_record(IN_B, MASK_WO, t0, SCOPE_CYC(3000), e, NE);
    console_puts("W9 count16");
    scope_put("wo0", e, n, WO0, 5);
    scope_put("wo1", e, n, WO1, 5);
    nl();
}

/* ---- capture ---- */

static void stim_setup(void)
{
    scope_pmux(GB, STIM, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GB, STIM, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EIC_REGS->EIC_CONFIG1 = 0x4U << (4U * (STIM - 8U));   /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << STIM;
    EIC_REGS->EIC_EVCTRL = 1U << STIM;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC9) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_TC3] = 1U;      /* channel 0 */
}

/* A high pulse of @hi us then low for @lo us on the stimulus, @n times */
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

static void c_config(uint32_t ctrla, uint16_t evctrl, uint8_t per)
{
    t3_reset(ctrla);
    T3.TC_PER = per;
    T3.TC_EVCTRL = evctrl | TC_EVCTRL_TCEI_Msk;
    sync3();
    t3_enable();
}

static void c1_stamp(void)
{
    uint32_t cc[3], fl[3], t0, ts[3];

    c_config(C8_DIV64 | TC_CTRLA_CAPTEN0_Msk, TC_EVCTRL_EVACT_STAMP, 200);
    t0 = scope_now();
    for (unsigned int i = 0; i < 3; i++) {
        scope_wait_until(t0, SCOPE_CYC(50U + 60U * i));
        ts[i] = scope_since(t0);
        pulses(1, 10, 10);
        fl[i] = T3.TC_INTFLAG;
        cc[i] = T3.TC_CC[0];
    }
    console_puts("C1 stamp");
    for (unsigned int i = 0; i < 3; i++) {
        put_dec("at_us", SCOPE_US(ts[i]));
        put_field("cc0", cc[i], 2);
        put_field("if", fl[i], 2);
    }
    put_field("if_after_reads", T3.TC_INTFLAG, 2);
    nl();
}

static void c2_err(void)
{
    uint32_t if1, cc0, cc1, if2;

    c_config(C8_DIV64 | TC_CTRLA_CAPTEN0_Msk, TC_EVCTRL_EVACT_STAMP, 200);
    pulses(1, 10, 40);
    pulses(1, 10, 40);
    if1 = T3.TC_INTFLAG;
    cc0 = T3.TC_CC[0];
    if2 = T3.TC_INTFLAG;
    console_puts("C2 err");
    put_field("if", if1, 2);
    put_field("cc0", cc0, 2);
    put_field("if_after_read", if2, 2);
    put_field("cc0_again", T3.TC_CC[0], 2);
    nl();
    /* Both channels in STAMP */
    c_config(C8_DIV64 | TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_CAPTEN1_Msk,
             TC_EVCTRL_EVACT_STAMP, 200);
    pulses(1, 10, 40);
    if1 = T3.TC_INTFLAG;
    pulses(1, 10, 40);
    if2 = T3.TC_INTFLAG;
    cc0 = T3.TC_CC[0];
    cc1 = T3.TC_CC[1];
    console_puts("C2 both");
    put_field("if1", if1, 2);
    put_field("if2", if2, 2);
    put_field("cc0", cc0, 2);
    put_field("cc1", cc1, 2);
    put_field("if_after", T3.TC_INTFLAG, 2);
    nl();
}

#define C16_DIV64       (TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64)

static void c_period(const char *label, uint16_t evact, uint16_t inv)
{
    uint32_t if1, cc0, cc1, if2, cc0b, cc1b;

    T3.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync3();
    T3_16.TC_CTRLA = C16_DIV64 | TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_CAPTEN1_Msk;
    T3_16.TC_EVCTRL = evact | inv | TC_EVCTRL_TCEI_Msk;
    sync3();
    T3_16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync3();
    pulses(3, 200, 300);
    if1 = T3_16.TC_INTFLAG;
    cc0 = T3_16.TC_CC[0];
    cc1 = T3_16.TC_CC[1];
    if2 = T3_16.TC_INTFLAG;
    pulses(2, 100, 400);
    cc0b = T3_16.TC_CC[0];
    cc1b = T3_16.TC_CC[1];
    console_puts(label);
    put_field("if", if1, 2);
    put_dec("cc0", cc0);
    put_dec("cc1", cc1);
    put_field("if_after", if2, 2);
    put_dec("cc0b", cc0b);
    put_dec("cc1b", cc1b);
    put_field("status", T3_16.TC_STATUS, 2);
    nl();
}

static void c6_plain(void)
{
    uint32_t cc[2], fl[2];

    for (unsigned int inv = 0; inv < 2; inv++) {
        c_config(C8_DIV64 | TC_CTRLA_CAPTEN0_Msk,
                 TC_EVCTRL_EVACT_OFF | (inv ? TC_EVCTRL_TCINV_Msk : 0), 200);
        pulses(1, 100, 10);
        fl[inv] = T3.TC_INTFLAG;
        cc[inv] = T3.TC_CC[0];
    }
    console_puts("C6 plain");
    put_field("if", fl[0], 2);
    put_field("cc0", cc[0], 2);
    put_field("if_inv", fl[1], 2);
    put_field("cc0_inv", cc[1], 2);
    nl();
}

static void c7_pin(void)
{
    uint32_t fl1, cc1, fl2, cc2;

    T2.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    sync2();
    T2.TC_CTRLA = C16_DIV64 | TC_CTRLA_CAPTEN0_Msk | TC_CTRLA_COPEN0_Msk;
    T2.TC_EVCTRL = TC_EVCTRL_EVACT_STAMP;
    scope_pmux(GB, COP, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GB, COP, 0);
    sync2();
    T2.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    sync2();
    wait_us(100);
    fl1 = T2.TC_INTFLAG;
    scope_pull(GB, COP, 1);
    wait_us(100);
    fl2 = T2.TC_INTFLAG;
    cc1 = T2.TC_CC[0];
    scope_pull(GB, COP, 0);
    wait_us(100);
    cc2 = T2.TC_CC[0];
    console_puts("C7 copen");
    put_field("if_before", fl1, 2);
    put_field("if_rise", fl2, 2);
    put_dec("cc0", cc1);
    put_field("if_fall", T2.TC_INTFLAG, 2);
    put_dec("cc0_fall", cc2);
    put_field("pin", (PORT_REGS->GROUP[GB].PORT_IN >> COP) & 1U, 1);
    nl();
    T2.TC_CTRLA = 0;
    sync2();
    scope_release(GB, COP);
}

static void c8_overflow(void)
{
    uint32_t cc0, fl;

    /* PER 20 (27 us); a capture after a wrap with the flag still set */
    c_config(C8_DIV64 | TC_CTRLA_CAPTEN0_Msk, TC_EVCTRL_EVACT_STAMP, 20);
    wait_us(100);
    pulses(1, 10, 10);
    fl = T3.TC_INTFLAG;
    cc0 = T3.TC_CC[0];
    console_puts("C8 overflow");
    put_field("if", fl, 2);
    put_field("cc0", cc0, 2);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tcprobe2: " BOARD_BANNER "\r\n");
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
    e0_regs();
    w_modes();
    w8_stops();
    w9_count16();
    t3_reset(0);
    stim_setup();
    c1_stamp();
    c2_err();
    c_period("C3 ppw", TC_EVCTRL_EVACT_PPW, 0);
    c_period("C4 pwp", TC_EVCTRL_EVACT_PWP, 0);
    c_period("C5 pw", TC_EVCTRL_EVACT_PW, 0);
    c_period("C5 pw_inv", TC_EVCTRL_EVACT_PW, TC_EVCTRL_TCINV_Msk);
    c6_plain();
    c7_pin();
    c8_overflow();
    T3.TC_CTRLA = 0;
    scope_release(GB, WO0);
    scope_release(GB, WO1);
    scope_release(GB, STIM);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
