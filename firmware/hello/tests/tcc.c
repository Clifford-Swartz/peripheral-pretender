/*
 * TCC test (PIC32CM MC00 TCC U2213): reset values and counter widths, the
 * waveform output state (STATUS.CMPx) and flags in NPWM (up, down,
 * inverted), NFRQ and the dual-slope modes, one-shot, the STOP and
 * RETRIGGER commands, double buffering, the counter event selection, the
 * events in and out through EVSYS, and the interrupt.
 *
 * TCC0 counts GCLK1 (OSC48M / 250) through the /1024 prescaler: about
 * 5.3 ms per tick, so each check samples in the middle of a tick (READSYNC
 * values and flags lag the count by a few clock cycles on silicon). PER is
 * 9 and CC0-CC3 are 3, 0, 9 and 10 unless a test says otherwise.
 *
 * Seen on silicon (tests/tccprobe.c): a CCBUFx/PERBUF write keeps its
 * SYNCBUSY bit set until the buffer is copied, so waiting for SYNCBUSY
 * waits for the end of the period (forever with CTRLB.LUPD set); STOP
 * clears COUNT.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define T               TCC0_REGS
#define T1              (TC1_REGS->COUNT16)
#define EV              EVSYS_REGS
#define TICK_MS         6U

#define CMP(st)         (((st) >> TCC_STATUS_CMP0_Pos) & 0xFU)
#define OVF             TCC_INTFLAG_OVF_Msk
#define TRG             TCC_INTFLAG_TRG_Msk
#define CNT             TCC_INTFLAG_CNT_Msk
#define MC(n)           (TCC_INTFLAG_MC0_Msk << (n))
#define FLAGS           (OVF | TRG | CNT | MC(0) | MC(1) | MC(2) | MC(3))

static const uint32_t cc_default[4] = { 3, 0, 9, 10 };

static void eol(void)
{
    console_puts("\r\n");
}

static void sync(void)
{
    while (T->TCC_SYNCBUSY != 0U) {
    }
}

static void command(uint8_t cmd)
{
    T->TCC_CTRLBSET = cmd;
    sync();
    while ((T->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) != 0U) {
    }
}

static uint32_t count(void)
{
    command(TCC_CTRLBSET_CMD_READSYNC);
    return T->TCC_COUNT;
}

static uint32_t flags(void)
{
    uint32_t f = T->TCC_INTFLAG & FLAGS;

    T->TCC_INTFLAG = f;
    return f;
}

static void setup(uint32_t wave, uint8_t ctrlb, uint32_t evctrl)
{
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync();
    T->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV1024;
    T->TCC_WAVE = wave;
    T->TCC_PER = 9;
    for (unsigned int i = 0; i < 4U; i++) {
        T->TCC_CC[i] = cc_default[i];
    }
    T->TCC_EVCTRL = evctrl;
    sync();
    if (ctrlb) {
        T->TCC_CTRLBSET = ctrlb;
        sync();
    }
    T->TCC_INTFLAG = 0xFFFFFFFFU;
}

static void enable(void)
{
    T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    sync();
}

static void disable(void)
{
    T->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    sync();
}

/* The next time COUNT is @c, after it has been something else */
static uint32_t at(uint32_t c, int down);

static uint32_t next_at(uint32_t c, int down)
{
    for (uint32_t ms = 40U * TICK_MS; ms > 0U && count() == c;) {
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    return at(c, down);
}

/*
 * Wait until COUNT is @c (with direction @down in dual slope: -1 for
 * either), then until the middle of that tick; 1 when it happened
 */
static uint32_t at(uint32_t c, int down)
{
    for (uint32_t ms = 40U * TICK_MS; ms > 0U;) {
        uint32_t n = count();
        int d = (T->TCC_CTRLBSET & TCC_CTRLBSET_DIR_Msk) ? 1 : 0;

        if (n == c && (down < 0 || d == down)) {
            delay_ms(TICK_MS / 2U);
            return 1;
        }
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
    return 0;
}

static void test_reset(void)
{
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync();
    TCC2_REGS->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (TCC2_REGS->TCC_SYNCBUSY != 0U) {
    }
    console_puts("reset:");
    put_checked("STATUS", T->TCC_STATUS, 0x1, 8);
    put_checked("PER", T->TCC_PER, 0xFFFFFF, 8);
    put_checked("TCC2-PER", TCC2_REGS->TCC_PER, 0xFFFF, 8);
    put_checked("WAVE", T->TCC_WAVE, 0, 8);
    put_checked("COUNT", count(), 0, 8);
    eol();
}

/* NPWM up: high for COUNT < CCx, one tick late; flags at COUNT == CCx */
static void test_npwm(void)
{
    uint32_t c2, c5, f;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0, 0);
    enable();
    at(2, -1);
    c2 = CMP(T->TCC_STATUS);
    at(5, -1);
    c5 = CMP(T->TCC_STATUS);
    flags();
    next_at(5, -1);             /* one whole period later */
    f = flags();
    console_puts("npwm:");
    put_checked("cmp@2", c2, 0xD, 1);
    put_checked("cmp@5", c5, 0xC, 1);
    /* CNTSEL START: CNT at each new cycle */
    put_checked("flags", f, OVF | CNT | MC(0) | MC(2), 8);

    /* Down: high for 0 < COUNT <= CCx */
    setup(TCC_WAVE_WAVEGEN_NPWM, TCC_CTRLBSET_DIR_Msk, 0);
    enable();
    at(5, -1);
    c5 = CMP(T->TCC_STATUS);
    at(2, -1);
    c2 = CMP(T->TCC_STATUS);
    /* Counting down a CCx above TOP never sets its output */
    put_checked("down-cmp@5", c5, 0x4, 1);
    put_checked("down-cmp@2", c2, 0x5, 1);

    /* POL0 and POL2 invert channels 0 and 2 */
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk | TCC_WAVE_POL2_Msk, 0,
          0);
    enable();
    at(2, -1);
    put_checked("pol-cmp@2", CMP(T->TCC_STATUS), 0x8, 1);
    disable();
    eol();
}

/* NFRQ: each output toggles one tick after its match */
static void test_nfrq(void)
{
    uint32_t a, b;

    setup(TCC_WAVE_WAVEGEN_NFRQ, 0, 0);
    enable();
    at(2, -1);
    a = CMP(T->TCC_STATUS);
    at(5, -1);
    b = CMP(T->TCC_STATUS);
    console_puts("nfrq:");
    put_checked("cmp@2", a, 0x2, 1);
    put_checked("cmp@5", b, 0x3, 1);
    next_at(2, -1);             /* after CC2 (9) and CC1 (0) again */
    put_checked("cmp@2-next", CMP(T->TCC_STATUS), 0x5, 1);
    disable();
    eol();
}

/* Dual slope: set leaving CCx up, cleared leaving it down */
static void test_dual(void)
{
    uint32_t up5, dn2, f1, f2;

    setup(TCC_WAVE_WAVEGEN_DSBOTTOM, 0, 0);
    enable();
    at(5, 0);
    up5 = CMP(T->TCC_STATUS);
    f1 = flags();
    at(5, 1);
    console_puts("dsbottom:");
    put_checked("dir-down", (T->TCC_CTRLBSET & TCC_CTRLBSET_DIR_Msk) ? 1 : 0,
                1, 1);
    at(2, 1);
    dn2 = CMP(T->TCC_STATUS);
    f2 = flags();
    at(5, 0);                   /* past ZERO */
    /* CC1 = 0 stays set, CC2 = TOP never sets */
    put_checked("cmp-up@5", up5, 0x3, 1);
    put_checked("cmp-down@2", dn2, 0x2, 1);
    put_checked("flags-first", f1, MC(0), 8);
    put_checked("flags-down", f2, 0, 8);
    put_checked("flags-zero", flags(), OVF | CNT | MC(0), 8);

    setup(TCC_WAVE_WAVEGEN_DSTOP, 0, 0);
    enable();
    at(5, 1);
    put_checked("dstop-top", flags() & OVF, OVF, 8);
    at(5, 0);
    put_checked("dstop-zero", flags() & OVF, 0, 8);

    setup(TCC_WAVE_WAVEGEN_DSBOTH, 0, 0);
    enable();
    at(5, 1);
    put_checked("dsboth-top", flags() & OVF, OVF, 8);
    at(5, 0);
    put_checked("dsboth-zero", flags() & OVF, OVF, 8);
    disable();
    eol();
}

/* One-shot stops at the wrap; STOP keeps COUNT; RETRIGGER restarts */
static void test_commands(void)
{
    uint32_t st, a, b;

    setup(TCC_WAVE_WAVEGEN_NPWM, TCC_CTRLBSET_ONESHOT_Msk, 0);
    enable();
    delay_ms(15U * TICK_MS);
    st = T->TCC_STATUS;
    console_puts("commands:");
    put_checked("oneshot-stop", st & TCC_STATUS_STOP_Msk, 1, 1);
    put_checked("oneshot-cmp", CMP(st), 0, 1);
    put_checked("oneshot-count", count(), 0, 8);
    put_checked("oneshot-flags", flags() & (OVF | MC(0)), OVF | MC(0), 8);
    command(TCC_CTRLBSET_CMD_RETRIGGER);
    at(4, -1);
    put_checked("restart", T->TCC_STATUS & TCC_STATUS_STOP_Msk, 0, 1);

    setup(TCC_WAVE_WAVEGEN_NPWM, 0, 0);
    enable();
    at(5, -1);
    command(TCC_CTRLBSET_CMD_STOP);
    a = count();
    delay_ms(3U * TICK_MS);
    b = count();
    st = T->TCC_STATUS;
    put_checked("stop-clears", a == 0U && b == 0U, 1, 1);
    put_checked("stop-status", st & TCC_STATUS_STOP_Msk, 1, 1);
    put_checked("stop-cmp", CMP(st), 0xC, 1);   /* kept from COUNT 5 */
    flags();
    command(TCC_CTRLBSET_CMD_RETRIGGER);    /* resumes from COUNT */
    delay_ms(TICK_MS + TICK_MS / 2U);
    put_checked("resume", count(), 1, 8);
    put_checked("resume-flags", flags(), CNT, 8);
    at(4, -1);
    command(TCC_CTRLBSET_CMD_RETRIGGER);    /* running: back to ZERO */
    a = count();
    put_checked("retrigger", a <= 1U, 1, 1);
    put_checked("trg", flags() & TRG, TRG, 8);
    disable();
    eol();
}

/* CC0 buffered: reads show CCBUF0 at once, CC0 changes at the wrap */
static void test_buffers(void)
{
    uint32_t sb, c, c0, d;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0, 0);
    enable();
    at(2, -1);
    T->TCC_CCBUF[0] = 6;
    sb = T->TCC_SYNCBUSY;
    sync();                     /* until the wrap copies CCBUF0 */
    c = count();
    c0 = T->TCC_CC[0];
    at(5, -1);
    d = CMP(T->TCC_STATUS) & 1U;
    console_puts("buffers:");
    put_checked("busy", sb, TCC_SYNCBUSY_CC0_Msk, 4);
    put_checked("synced-at-wrap", c <= 1U, 1, 1);
    put_checked("cc0", c0, 6, 2);
    put_checked("cmp0@5-new", d, 1, 1);
    put_checked("valid-after", (T->TCC_STATUS & TCC_STATUS_CCBUFV0_Msk) ?
                1U : 0U, 0, 1);

    /* LUPD: no copy (SYNCBUSY stays set) until the UPDATE command */
    T->TCC_CTRLBSET = TCC_CTRLBSET_LUPD_Msk;
    sync();
    T->TCC_CCBUF[0] = 2;
    delay_ms(25U * TICK_MS);
    sb = T->TCC_SYNCBUSY;
    put_checked("lupd-busy", sb, TCC_SYNCBUSY_CC0_Msk, 4);
    put_checked("lupd-valid", (T->TCC_STATUS & TCC_STATUS_CCBUFV0_Msk) ?
                1U : 0U, 1, 1);
    T->TCC_CTRLBSET = TCC_CTRLBSET_CMD_UPDATE;
    sync();
    while ((T->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) != 0U) {
    }
    at(4, -1);
    put_checked("updated", CMP(T->TCC_STATUS) & 1U, 0, 1);
    disable();
    eol();
}

/* CNTSEL: where the CNT flag comes, printed per setting */
static void test_cntsel(void)
{
    static const char *const names[] = { "start", "end", "between",
                                         "boundary" };

    console_puts("cntsel:");
    for (unsigned int sel = 0; sel < 4U; sel++) {
        uint32_t f0, f1, f2;

        setup(TCC_WAVE_WAVEGEN_NFRQ, TCC_CTRLBSET_ONESHOT_Msk,
              TCC_EVCTRL_CNTSEL(sel));
        enable();
        at(3, -1);
        f0 = flags() & CNT;
        at(8, -1);
        f1 = flags() & CNT;
        delay_ms(4U * TICK_MS);     /* past the one-shot stop */
        f2 = flags() & CNT;
        /* At the start, mid-period, at the one-shot end */
        console_puts(" ");
        console_puts(names[sel]);
        put_checked("", (f0 ? 4U : 0U) | (f1 ? 2U : 0U) | (f2 ? 1U : 0U),
                    sel == 0U ? 4U : sel == 3U ? 5U : sel == 1U ? 1U : 0U, 1);
    }
    disable();
    eol();
}

static void evsys_route(unsigned int gen, unsigned int user)
{
    EV->EVSYS_CTRLA = EVSYS_CTRLA_SWRST_Msk;
    while (EV->EVSYS_CTRLA & EVSYS_CTRLA_SWRST_Msk) {
    }
    EV->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(gen) |
                           EVSYS_CHANNEL_PATH_SYNCHRONOUS |
                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    EV->EVSYS_USER[user] = 1;
}

static void swevt(unsigned int times)
{
    for (unsigned int i = 0; i < times; i++) {
        EV->EVSYS_SWEVT = 1;
        delay_us(50);
    }
}

static uint32_t t1_count(void)
{
    T1.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
    while (T1.TC_SYNCBUSY != 0U) {
    }
    while ((T1.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U) {
    }
    return T1.TC_COUNT;
}

static void test_events(void)
{
    uint32_t a;

    console_puts("events:");
    /* Out: OVF and MC0 into TC1 counting events, three periods */
    T1.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while (T1.TC_SYNCBUSY != 0U) {
    }
    T1.TC_EVCTRL = TC_EVCTRL_EVACT_COUNT | TC_EVCTRL_TCEI_Msk;
    T1.TC_CTRLA = TC_CTRLA_ENABLE_Msk;
    while (T1.TC_SYNCBUSY != 0U) {
    }
    evsys_route(EVENT_ID_GEN_TCC0_OVF, EVENT_ID_USER_TC1_EVU);
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_OVFEO_Msk);
    enable();
    at(5, -1);
    a = t1_count();
    next_at(5, -1);
    next_at(5, -1);
    next_at(5, -1);
    put_checked("ovf-out", t1_count() - a, 3, 4);
    evsys_route(EVENT_ID_GEN_TCC0_MC_0, EVENT_ID_USER_TC1_EVU);
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_MCEO0_Msk);
    enable();
    at(5, -1);
    a = t1_count();
    next_at(5, -1);
    next_at(5, -1);
    put_checked("mc0-out", t1_count() - a, 2, 4);

    /* In: EV0 COUNTEV, three software events move COUNT by three */
    evsys_route(0, EVENT_ID_USER_TCC0_EV_0);
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT0_COUNTEV |
          TCC_EVCTRL_TCEI0_Msk);
    enable();
    delay_ms(3U * TICK_MS);
    put_checked("countev-idle", count(), 0, 8);
    swevt(3);
    put_checked("countev", count(), 3, 8);
    /* EV0 START: waits for the event */
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT0_START |
          TCC_EVCTRL_TCEI0_Msk);
    enable();
    delay_ms(3U * TICK_MS);
    put_checked("start-wait", count(), 0, 8);
    swevt(1);
    delay_ms(3U * TICK_MS);
    a = count();
    put_checked("start-run", a >= 2U && a <= 4U, 1, 1);
    /* EV0 RETRIGGER: back to ZERO, TRG */
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT0_RETRIGGER |
          TCC_EVCTRL_TCEI0_Msk);
    enable();
    swevt(1);
    at(6, -1);
    flags();
    swevt(1);
    put_checked("retrigger", count() <= 1U, 1, 1);
    put_checked("retrigger-trg", flags() & TRG, TRG, 8);
    /* EV0 INC: events, not the clock, move the counter (silicon) */
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT0_INC |
          TCC_EVCTRL_TCEI0_Msk);
    enable();
    delay_ms(2U * TICK_MS);
    put_checked("inc-idle", count(), 0, 8);
    swevt(2);
    put_checked("inc", count(), 2, 8);
    setup(TCC_WAVE_WAVEGEN_NPWM, TCC_CTRLBSET_DIR_Msk,
          TCC_EVCTRL_EVACT0_INC | TCC_EVCTRL_TCEI0_Msk);
    enable();
    swevt(1);
    put_checked("inc-down@0", count(), 9, 8);
    /* EV1 DEC and STOP on a running counter, through channel 1 */
    EV->EVSYS_USER[EVENT_ID_USER_TCC0_EV_1] = 2;
    EV->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_PATH_SYNCHRONOUS |
                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_1] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT1_DEC |
          TCC_EVCTRL_TCEI1_Msk);
    enable();
    at(6, -1);
    EV->EVSYS_SWEVT = 2;
    delay_us(50);
    put_checked("dec", count(), 5, 8);
    setup(TCC_WAVE_WAVEGEN_NPWM, 0, TCC_EVCTRL_EVACT1_STOP |
          TCC_EVCTRL_TCEI1_Msk);
    enable();
    at(5, -1);
    EV->EVSYS_SWEVT = 2;
    delay_us(50);
    a = count();
    delay_ms(2U * TICK_MS);
    put_checked("paused", a == 5U && count() == 5U, 1, 1);
    put_checked("paused-stop", T->TCC_STATUS & TCC_STATUS_STOP_Msk, 1, 1);
    disable();
    EV->EVSYS_CTRLA = EVSYS_CTRLA_SWRST_Msk;
    eol();
}

/* TCC2 counts 16 bits: from 0xFFFE it overflows after two ticks */
static void test_width(void)
{
    tcc_registers_t *t2 = TCC2_REGS;

    t2->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (t2->TCC_SYNCBUSY != 0U) {
    }
    t2->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV1024;
    t2->TCC_COUNT = 0xFFFE;
    while (t2->TCC_SYNCBUSY != 0U) {
    }
    t2->TCC_INTFLAG = 0xFFFFFFFFU;
    t2->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    while (t2->TCC_SYNCBUSY != 0U) {
    }
    delay_ms(TICK_MS / 2U);
    console_puts("width:");
    put_checked("ovf@1", t2->TCC_INTFLAG & OVF, 0, 1);
    delay_ms(2U * TICK_MS);
    put_checked("ovf@2", t2->TCC_INTFLAG & OVF, OVF, 1);
    t2->TCC_CTRLA = 0;
    while (t2->TCC_SYNCBUSY != 0U) {
    }
    eol();
}

static void test_irq(void)
{
    uint32_t p0, p1;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0, 0);
    T->TCC_INTENSET = OVF;
    NVIC_ClearPendingIRQ(TCC0_IRQn);
    enable();
    at(5, -1);
    p0 = NVIC_GetPendingIRQ(TCC0_IRQn);
    at(2, -1);
    p1 = NVIC_GetPendingIRQ(TCC0_IRQn);
    disable();
    T->TCC_INTENCLR = 0xFFFFFFFFU;
    T->TCC_INTFLAG = 0xFFFFFFFFU;
    NVIC_ClearPendingIRQ(TCC0_IRQn);
    console_puts("irq:");
    put_checked("before-ovf", p0, 0, 1);
    put_checked("after-ovf", p1, 1, 1);
    eol();
}

/* Without GCLK_TCC0 the ENABLE synchronization does not complete */
static void test_noclock(void)
{
    uint32_t busy;

    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync();
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = 0;
    while (GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk) {
    }
    T->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;
    delay_ms(1);
    busy = T->TCC_SYNCBUSY;
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN_Msk;
    sync();
    console_puts("no clock:");
    put_checked("enable-busy", busy & TCC_SYNCBUSY_ENABLE_Msk,
                TCC_SYNCBUSY_ENABLE_Msk, 1);
    put_checked("after-clock", T->TCC_SYNCBUSY, 0, 2);
    disable();
    eol();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP tcc: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_TCC2_Msk |
                                MCLK_APBCMASK_TC1_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(250U) |
                                 GCLK_GENCTRL_GENEN_Msk;
    while (GCLK_REGS->GCLK_SYNCBUSY != 0U) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC2_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC1_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_0] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;

    test_reset();
    test_npwm();
    test_nfrq();
    test_dual();
    test_commands();
    test_buffers();
    test_cntsel();
    test_events();
    test_width();
    test_irq();
    test_noclock();
    test_exit();
}
