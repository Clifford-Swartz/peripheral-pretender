/*
 * TCC capture and circular buffer check (PIC32CM MC00, TCC U2213): event
 * capture through the CCx/CCBUFx FIFO, PPW and PWP, the dual-slope MSB,
 * buffers at the enable, and circular PER and CC buffers, against what
 * silicon did in firmware/hello APP=tccprobe10..12
 * (docs/research/peripherals/tcc-u2213.md). Run:
 * py -m tools.periph check mc00 tcccapt
 *
 * Bare EV15N46A. The input pin PA02 (EXTINT2, function A), its pull
 * resistor turned by PORT.OUT, HIGH sense, ASYNCH, through asynchronous
 * EVSYS channel 0 to TCC0 MC_2 and EV_1; TCC0 WO0 on PA04 (function E,
 * PULLEN). GCLK_TCC0 1 MHz, DIV1. Captured values are compared with the
 * input's own timing (the pull takes about 2 us; silicon's counter starts
 * about 4 us after the enable, the twin's at once).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_A          0U
#define FUNC_E          4U
#define PIN_IN          2U                      /* PA02, EXTINT2 */
#define W0              4U
#define GEN_EIC2        16U
#define USER_EV1        10U
#define USER_MC2        13U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define NE              64U
#define CICCEN0         (1U << 8)
#define CIPEREN         (1U << 7)

static ScopeEdge e[NE];
static unsigned int ne;

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static uint32_t count(void)
{
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_READSYNC;
    sync0();
    for (uint32_t i = 0; (T0->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) &&
         i < 200000U; i++) {
    }
    return T0->TCC_COUNT;
}

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 8);
}

static void setup(uint32_t ctrla, uint32_t wave, uint32_t per,
                  uint32_t evctrl)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    scope_pull(GA, PIN_IN, 0);
    T0->TCC_CTRLA = ctrla;
    T0->TCC_WAVE = wave;
    sync0();
    T0->TCC_PER = per;
    T0->TCC_CC[0] = 50;
    T0->TCC_CC[1] = 30;
    T0->TCC_EVCTRL = evctrl;
    sync0();
}

/* Enable, then the input high from @t[2i] to @t[2i + 1] us, @n pulses */
static uint32_t pulses(const uint16_t *t, unsigned int n)
{
    uint32_t t0 = scope_now();

    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int i = 0; i < 2 * n; i++) {
        scope_wait_until(t0, SCOPE_CYC(t[i]));
        scope_pull(GA, PIN_IN, !(i & 1U));
    }
    return t0;
}

/* Settle well past the input's pull and a FIFO move */
static void settle_us(uint32_t us)
{
    scope_wait_until(scope_now(), SCOPE_CYC(us));
}

static void check_fifo(void)
{
    static const uint16_t t[] = { 20, 25, 40, 45, 60, 65 };
    uint32_t a, b, f1, f2, st1;

    /* Three captures: CC2, CCBUF2, then ERR (and the third is lost) */
    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_NPWM, 99,
          TCC_EVCTRL_MCEI2_Msk);
    pulses(t, 3);
    settle_us(10);
    f1 = T0->TCC_INTFLAG;
    st1 = T0->TCC_STATUS;
    a = T0->TCC_CC[2];
    settle_us(10);                  /* CCBUF2 moves up */
    f2 = T0->TCC_INTFLAG;
    b = T0->TCC_CC[2];
    settle_us(10);
    console_puts("fifo");
    put_exact("mc2_err", f1 & (TCC_INTFLAG_MC2_Msk | TCC_INTFLAG_ERR_Msk),
              TCC_INTFLAG_MC2_Msk | TCC_INTFLAG_ERR_Msk);
    put_exact("ccbufv2", st1 & TCC_STATUS_CCBUFV2_Msk,
              TCC_STATUS_CCBUFV2_Msk);
    /* 20 us apart, as the pulses */
    put_dec_range("first", a, 15, 23);
    put_dec_range("apart", b - a, 19, 21);
    put_exact("refilled_mc2", f2 & TCC_INTFLAG_MC2_Msk, TCC_INTFLAG_MC2_Msk);
    put_exact("empty_mc2", T0->TCC_INTFLAG & TCC_INTFLAG_MC2_Msk, 0);
    put_exact("empty_ccbufv2", T0->TCC_STATUS & TCC_STATUS_CCBUFV2_Msk, 0);
    end_line();

    /* No MCEI2: no capture; CC2 keeps what was written */
    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_NPWM, 99, 0);
    T0->TCC_CC[2] = 7;
    sync0();
    pulses(t, 1);
    settle_us(10);
    console_puts("no_mcei");
    put_exact("mc2", T0->TCC_INTFLAG & TCC_INTFLAG_MC2_Msk, 0);
    put_exact("cc2", T0->TCC_CC[2], 7);
    end_line();
}

static void check_ppw(void)
{
    static const uint16_t t[] = { 30, 45, 90, 105 };
    static const uint32_t evact[4] = {
        TCC_EVCTRL_EVACT1_PPW, TCC_EVCTRL_EVACT1_PWP,
        TCC_EVCTRL_EVACT1_PPW | TCC_EVCTRL_TCINV1_Msk,
        TCC_EVCTRL_EVACT1_PWP | TCC_EVCTRL_TCINV1_Msk,
    };
    static const char *const label[4] = { "ppw", "pwp", "ppw_inv",
                                          "pwp_inv" };

    for (unsigned int k = 0; k < 4; k++) {
        unsigned int per_ch = k & 1U;
        uint32_t t0, c100, per, width;

        setup(TCC_CTRLA_CPTEN0_Msk | TCC_CTRLA_CPTEN1_Msk,
              TCC_WAVE_WAVEGEN_NPWM, 0xFFFFFF,
              evact[k] | TCC_EVCTRL_TCEI1_Msk);
        t0 = scope_now();
        T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        for (unsigned int i = 0; i < 4; i++) {
            scope_wait_until(t0, SCOPE_CYC(t[i]));
            scope_pull(GA, PIN_IN, !(i & 1U));
            if (i == 2) {
                scope_wait_until(t0, SCOPE_CYC(100));
                c100 = count();
            }
        }
        settle_us(10);
        per = T0->TCC_CC[per_ch];
        width = T0->TCC_CC[!per_ch];
        console_puts(label[k]);
        if (k < 2) {
            /* Restarted at the rise at 30 + 2 (and 90 + 2): 8 at 100 */
            put_dec_range("count100", c100, 7, 11);
            /* First captures: the rise from the start, then the width */
            put_dec_range("period", per, 27, 32);
            put_dec_range("width", width, 13, 16);
        } else {
            /* TCINV1: restarted at the falls (45 + 2): 53 at 100 */
            put_dec_range("count100", c100, 52, 56);
            put_dec_range("period", per, 42, 47);
            put_dec_range("width", width, 27, 32);
        }
        put_exact("trg", T0->TCC_INTFLAG & TCC_INTFLAG_TRG_Msk,
                  TCC_INTFLAG_TRG_Msk);
        end_line();
    }
}

static void check_ds_msb(void)
{
    static const uint16_t t[] = { 30, 35, 130, 135 };
    uint32_t a, b;

    setup(TCC_CTRLA_CPTEN2_Msk, TCC_WAVE_WAVEGEN_DSBOTTOM, 99,
          TCC_EVCTRL_MCEI2_Msk);
    pulses(t, 2);
    settle_us(10);
    a = T0->TCC_CC[2];
    settle_us(10);
    b = T0->TCC_CC[2];
    console_puts("ds_msb");
    /* Up at about 28: MSB clear; down at about 70: MSB set */
    put_exact("up_msb", a >> 23, 0);
    put_dec_range("up", a & 0x7FFFFFU, 24, 32);
    put_exact("down_msb", b >> 23, 1);
    put_dec_range("down", b & 0x7FFFFFU, 64, 74);
    end_line();
}

/* WO0's high times from its first rise: @n of them */
static void highs(uint32_t *h, unsigned int n)
{
    unsigned int k = 0;
    uint32_t rise = 0, m = 1U << W0;

    for (unsigned int i = 1; i < ne && k < n; i++) {
        if (!((e[i].v ^ e[i - 1].v) & m)) {
            continue;
        }
        if (e[i].v & m) {
            rise = e[i].t;
        } else if (rise) {
            h[k++] = SCOPE_US(e[i].t - rise);
        }
    }
    while (k < n) {
        h[k++] = 0;
    }
}

static void check_buffers(void)
{
    uint32_t h[4], t0;

    /* A CC0 or PER write reaches its buffer too; the enable copies none */
    setup(0, TCC_WAVE_WAVEGEN_NPWM, 49, 0);
    T0->TCC_CC[0] = 10;
    sync0();
    console_puts("buffers");
    put_exact("ccbuf0", T0->TCC_CCBUF[0], 10);
    put_exact("perbuf", T0->TCC_PERBUF, 49);
    T0->TCC_CCBUF[0] = 30;
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, 1U << W0, t0, SCOPE_CYC(160), e, NE);
    highs(h, 2);
    put_dec_range("first_high", h[0], 9, 11);
    put_dec_range("second_high", h[1], 29, 31);
    end_line();

    /* Circular CC0, single slope: CC0 and CCBUF0 take turns each cycle */
    setup(0, TCC_WAVE_WAVEGEN_NPWM | CICCEN0, 49, 0);
    T0->TCC_CC[0] = 29;
    sync0();
    T0->TCC_CCBUF[0] = 10;
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, 1U << W0, t0, SCOPE_CYC(220), e, NE);
    highs(h, 4);
    console_puts("circular");
    put_dec_range("npwm_high1", h[0], 28, 30);
    put_dec_range("npwm_high2", h[1], 9, 11);
    put_dec_range("npwm_high3", h[2], 28, 30);
    put_dec_range("npwm_high4", h[3], 9, 11);
    /* DSBOTH: CC0 10 counting up, CCBUF0 30 counting down: high 58 */
    setup(0, TCC_WAVE_WAVEGEN_DSBOTH | CICCEN0, 49, 0);
    T0->TCC_CC[0] = 10;
    sync0();
    T0->TCC_CCBUF[0] = 30;
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, 1U << W0, t0, SCOPE_CYC(320), e, NE);
    highs(h, 3);
    put_dec_range("dsboth_high1", h[0], 57, 59);
    put_dec_range("dsboth_high2", h[1], 57, 59);
    /* DSBOTH: PER 49 for a whole cycle, then PERBUF 29 (CC0 10) */
    setup(0, TCC_WAVE_WAVEGEN_DSBOTH | CIPEREN, 49, 0);
    T0->TCC_CC[0] = 10;
    sync0();
    T0->TCC_PERBUF = 29;
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, 1U << W0, t0, SCOPE_CYC(320), e, NE);
    highs(h, 3);
    put_dec_range("per_high1", h[0], 77, 79);
    put_dec_range("per_high2", h[1], 37, 39);
    put_dec_range("per_high3", h[2], 77, 79);
    end_line();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tcccapt: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN_IN, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN_IN, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * PIN_IN);          /* HIGH */
    EIC_REGS->EIC_ASYNCH = 1U << PIN_IN;
    EIC_REGS->EIC_EVCTRL = 1U << PIN_IN;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 1U;
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 1U;
    scope_pmux(GA, W0, FUNC_E, PORT_PINCFG_PULLEN_Msk);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << W0;
    scope_start();

    check_fifo();
    check_ppw();
    check_ds_msb();
    check_buffers();

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 0;
    EVSYS_REGS->EVSYS_USER[USER_EV1] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    scope_release(GA, W0);
    test_exit();
}
