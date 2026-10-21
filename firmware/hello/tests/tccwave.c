/*
 * TCC waveform extension check (PIC32CM MC00, TCC U2213): output matrix,
 * dead time, swap, polarity, pattern, inversion, DSCRITICAL, the RAMP2
 * modes, dithering, the per-instance units and the outputs after STOP
 * against what silicon did in
 * firmware/hello APP=tccprobe2..5 (docs/research/peripherals/
 * tcc-u2213.md). Run: py -m tools.periph check mc00 tccwave
 *
 * Bare EV15N46A. TCC0 WO0..7 on PA04, PA05 (function E), PA18, PA19, PA22,
 * PA23, PA16, PA17 (function F), watched through PORT.IN (scope.h).
 * GCLK_TCC0 1 MHz (OSC48M / 48), DIV1: a tick and a dead-time step are
 * 1 us. Silicon starts about 4 us after the enable, the twin at once, and
 * undriven pins keep whatever charge they had, so every time is measured
 * from an edge of the same run; within 1-2 us. Each register write waits
 * for SYNCBUSY, but for the one that checks a dropped write.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define T0              TCC0_REGS
#define GA              0U
#define FUNC_E          4U
#define FUNC_F          5U
#define IN_A            (&PORT_REGS->GROUP[GA].PORT_IN)
#define NE              128U

static const uint8_t wo_pin[8] = { 4, 5, 18, 19, 22, 23, 16, 17 };
static uint32_t wo_mask;
static ScopeEdge e[NE];
static unsigned int ne;

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void put_exact(const char *name, uint32_t value, uint32_t expect,
                      unsigned int digits)
{
    put_range(name, value, expect, expect, digits);
}

/* SWRST, then the waveform and PER 99, CC 20/40/60/80 */
static void setup(uint32_t wave, uint32_t wexctrl)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_WEXCTRL = wexctrl;
    T0->TCC_WAVE = wave;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 20;
    T0->TCC_CC[1] = 40;
    T0->TCC_CC[2] = 60;
    T0->TCC_CC[3] = 80;
    sync0();
}

/* Enable TCC0 and record @us of the outputs in @mask */
static void run_mask(uint32_t us, uint32_t mask)
{
    uint32_t t0;

    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, mask, t0, SCOPE_CYC(us), e, NE);
}

static void run(uint32_t us)
{
    run_mask(us, wo_mask);
}

/* The times (us) output @x becomes @level, at most @max */
static unsigned int edges(unsigned int x, int level, uint32_t *t,
                          unsigned int max)
{
    unsigned int k = 0, bit = wo_pin[x];

    for (unsigned int i = 1; i < ne && k < max; i++) {
        if (((e[i].v ^ e[i - 1].v) >> bit) & 1U &&
            (int)((e[i].v >> bit) & 1U) == level) {
            t[k++] = SCOPE_US(e[i].t);
        }
    }
    return k;
}

/* The first time after @after that output @x becomes @level, or 0 */
static uint32_t edge_after(unsigned int x, int level, uint32_t after)
{
    uint32_t t[8];
    unsigned int n = edges(x, level, t, 8);

    for (unsigned int i = 0; i < n; i++) {
        if (t[i] > after) {
            return t[i];
        }
    }
    return 0;
}

/* High time of output @x from its first rise after @after */
static int32_t high_after(unsigned int x, uint32_t after)
{
    uint32_t r = edge_after(x, 1, after);

    return r ? (int32_t)(edge_after(x, 0, r) - r) : -1;
}

static void check_units(void)
{
    tcc_registers_t *t[3] = { TCC0_REGS, TCC1_REGS, TCC2_REGS };
    static const uint32_t wex[3] = { 0xFFFF0F03, 0, 0 };
    static const uint32_t drv[3] = { 0xFFFFFFFF, 0xFF0F0F0F, 0xFF030303 };
    static const uint32_t patt[3] = { 0xFFFF, 0x0F0F, 0x0000 };
    static const uint32_t wave[3] = { 0x0F0F0FB7, 0x000303B7, 0x000303B7 };

    console_puts("units");
    for (unsigned int i = 0; i < 3; i++) {
        t[i]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
        t[i]->TCC_WEXCTRL = 0xFFFFFFFFU;
        t[i]->TCC_DRVCTRL = 0xFFFFFFFFU;
        t[i]->TCC_PATT = 0xFFFFU;
        t[i]->TCC_WAVE = 0xFFFFFFFFU;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
        put_exact("wexctrl", t[i]->TCC_WEXCTRL, wex[i], 8);
        put_exact("drvctrl", t[i]->TCC_DRVCTRL, drv[i], 8);
        put_exact("patt", t[i]->TCC_PATT, patt[i], 4);
        /* WAVE bit 6 sticks on silicon, not in the ATDF: not checked */
        put_exact("wave", t[i]->TCC_WAVE & ~0x40U, wave[i], 8);
        t[i]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        for (uint32_t n = 0; t[i]->TCC_SYNCBUSY && n < 200000U; n++) {
        }
    }
    end_line();
}

/* OTMX: each output's first fall, from WO0's (CC0 = 20) */
static void check_otmx(void)
{
    static const uint8_t ccx[4][8] = {
        { 0, 1, 2, 3, 0, 1, 2, 3 }, { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 1, 1, 1, 1, 1, 1, 1 },
    };
    static const char *const name[8] = { "w0", "w1", "w2", "w3", "w4",
                                         "w5", "w6", "w7" };

    for (unsigned int m = 0; m < 4; m++) {
        uint32_t f0;

        setup(TCC_WAVE_WAVEGEN_NPWM, TCC_WEXCTRL_OTMX(m));
        run(150);
        f0 = edge_after(0, 0, 0);
        console_puts("otmx");
        console_put_dec(m);
        for (unsigned int x = 1; x < 8; x++) {
            /* The first fall at CC: 20 + 20 * channel after WO0's */
            put_near(name[x], (int32_t)(edge_after(x, 0, 0) - f0),
                     (int32_t)(20U * ccx[m][x]), 1);
        }
        end_line();
    }
}

static void check_dti(void)
{
    uint32_t f0, r4, f4;

    setup(TCC_WAVE_WAVEGEN_NPWM,
          TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) | TCC_WEXCTRL_DTHS(10));
    run(250);
    /* WO0's fall at CC0: a pin left high falls at the start too */
    f0 = edge_after(0, 0, edge_after(0, 1, 0));
    r4 = edge_after(4, 1, f0);
    f4 = edge_after(4, 0, r4);
    console_puts("dti");
    /* Low side: high CC0 - DTLS; high side up DTHS after it falls */
    put_near("ls_high", high_after(0, f0), 15, 1);
    put_near("hs_rise", (int32_t)(r4 - f0), 10, 1);
    put_near("hs_high", (int32_t)(f4 - r4), 70, 1);
    put_near("ls_rise", (int32_t)(edge_after(0, 1, f4) - f4), 5, 1);
    end_line();
}

static void check_swap_pol(void)
{
    int32_t h0, h4;

    /* OTMX 3: WO0 = CC0, WO4 = CC1; SWAP0 exchanges them */
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_SWAP0_Msk, TCC_WEXCTRL_OTMX(3));
    run(250);
    h0 = high_after(0, edge_after(0, 0, 0));
    h4 = high_after(4, edge_after(4, 0, 0));
    console_puts("swap");
    put_near("w0_high", h0, 40, 1);
    put_near("w4_high", h4, 20, 1);
    end_line();
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk, 0);
    run(250);
    console_puts("pol");
    put_near("w0_high", high_after(0, 0), 80, 1);
    put_near("w1_high", high_after(1, edge_after(1, 0, 0)), 40, 1);
    end_line();
    /* A WAVE write during the previous one's synchronization is dropped */
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 20;
    sync0();
    console_puts("dropped");
    put_exact("wave", T0->TCC_WAVE, TCC_WAVE_WAVEGEN_NPWM, 8);
    end_line();
}

static void check_patt_inven(void)
{
    uint32_t f0, r4;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0);
    T0->TCC_PATT = TCC_PATT_PGE0_Msk | (TCC_PATT_PGE0_Msk << 1) |
                   (TCC_PATT_PGE0_Msk << 4) | TCC_PATT_PGV0_Msk |
                   (TCC_PATT_PGV0_Msk << 1);
    sync0();
    run(250);
    console_puts("patt");
    put_exact("w0_falls", edges(0, 0, (uint32_t[4]){ 0 }, 4), 0, 1);
    put_exact("w0_level", (e[ne - 1].v >> wo_pin[0]) & 1U, 1, 1);
    put_exact("w1_level", (e[ne - 1].v >> wo_pin[1]) & 1U, 1, 1);
    put_exact("w4_level", (e[ne - 1].v >> wo_pin[4]) & 1U, 0, 1);
    put_near("w2_period",
             (int32_t)(edge_after(2, 0, edge_after(2, 0, 0)) -
                       edge_after(2, 0, 0)), 100, 1);
    /* PATT reads PATTBUF until the update copies it */
    T0->TCC_PATTBUF = 0;
    put_exact("patt_reads_buf", T0->TCC_PATT, 0, 4);
    end_line();
    setup(TCC_WAVE_WAVEGEN_NPWM,
          TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) | TCC_WEXCTRL_DTHS(10));
    T0->TCC_DRVCTRL = TCC_DRVCTRL_INVEN0_Msk | TCC_DRVCTRL_INVEN4_Msk;
    run(250);
    /* Inverted after dead time: WO0 low CC0 - DTLS, WO4 low PER - CC0 - DTHS */
    f0 = edge_after(0, 0, edge_after(0, 1, 0));
    r4 = edge_after(4, 1, edge_after(4, 0, 0));
    console_puts("inven");
    put_near("w0_low", (int32_t)(edge_after(0, 1, f0) - f0), 15, 1);
    put_near("w4_high", (int32_t)(edge_after(4, 0, r4) - r4), 30, 1);
    end_line();
}

static void check_dscritical(void)
{
    uint32_t f;

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_DSCRITICAL;
    sync0();
    T0->TCC_PER = 49;
    T0->TCC_CC[0] = 10;
    T0->TCC_CC[1] = 20;
    T0->TCC_CC[2] = 30;
    T0->TCC_CC[3] = 40;
    run(250);
    console_puts("dscritical");
    /*
     * WO0 up at CC0 = 10, down at CC2 = 30; WO1 up at CC1 = 20, down at
     * CC3 = 40; WO2 and WO3 on CC2 and CC3 both ways (DSBOTTOM)
     */
    f = edge_after(0, 0, edge_after(0, 1, 0));
    put_near("w0_high", high_after(0, f), 58, 1);
    put_near("w0_period", (int32_t)(edge_after(0, 0, f) - f), 98, 1);
    put_near("w1_high", high_after(1, edge_after(1, 0, 0)), 38, 1);
    put_near("w2_high", high_after(2, edge_after(2, 0, 0)), 38, 1);
    put_near("w3_high", high_after(3, edge_after(3, 0, 0)), 18, 1);
    end_line();
}

static void check_ramp(void)
{
    static const uint32_t ramp[3] = { TCC_WAVE_RAMP_RAMP2,
                                      TCC_WAVE_RAMP_RAMP2A,
                                      TCC_WAVE_RAMP_RAMP2C };
    static const char *const label[3] = { "ramp2", "ramp2a", "ramp2c" };
    /* High times of WO0 and WO1 (in their cycles), and WO0's period */
    static const int32_t w0h[3] = { 29, 29, 10 }, w1h[3] = { 20, 29, 20 };
    static const int32_t w0p[3] = { 100, 100, 80 };

    for (unsigned int r = 0; r < 3; r++) {
        uint32_t f0;

        T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        sync0();
        T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM | ramp[r];
        sync0();
        T0->TCC_PER = 49;
        T0->TCC_CC[0] = 29;
        T0->TCC_CC[1] = 20;
        T0->TCC_CC[2] = 10;
        T0->TCC_CC[3] = 40;
        run(300);
        f0 = edge_after(0, 0, 0);
        console_puts(label[r]);
        put_near("w0_high", high_after(0, f0), w0h[r], 1);
        put_near("w0_period", (int32_t)(edge_after(0, 0, f0) - f0), w0p[r],
                 1);
        put_near("w1_high", high_after(1, edge_after(1, 0, 0)), w1h[r], 1);
        end_line();
    }
}

/* Average period and high time of WO0 over 16 cycles, in tenths of us */
static void dith_avg(uint32_t *period, uint32_t *high)
{
    uint32_t r[18], f[18], m = 1U << wo_pin[0], nr = 0, nf = 0;

    for (unsigned int i = 1; i < ne; i++) {
        if (!((e[i].v ^ e[i - 1].v) & m)) {
            continue;
        }
        if ((e[i].v & m) && nr < 18) {
            r[nr++] = e[i].t;
        } else if (!(e[i].v & m) && nr && nf < 18) {
            f[nf++] = e[i].t;
        }
    }
    *period = nr >= 17 ? (r[16] - r[0]) * 10U / SCOPE_CYC(1U) / 16U : 0;
    *high = 0;
    for (unsigned int i = 0; i < 16 && i < nf; i++) {
        *high += f[i] - r[i];
    }
    *high = *high * 10U / SCOPE_CYC(1U) / 16U;
}

static void check_dith(void)
{
    uint32_t p, h, p2, h2;

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_CTRLA = TCC_CTRLA_RESOLUTION_DITH4;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = (49U << 4) | 8U;          /* 49 + 8/16 */
    T0->TCC_CC[0] = 20U << 4;
    /* WO0 alone: 16 frames of all eight outputs overflow e[] */
    run_mask(1000, 1U << wo_pin[0]);
    dith_avg(&p, &h);
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_CTRLA = TCC_CTRLA_RESOLUTION_DITH4;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 49U << 4;
    T0->TCC_CC[0] = (20U << 4) | 4U;        /* 20 + 4/16 */
    run_mask(1000, 1U << wo_pin[0]);
    dith_avg(&p2, &h2);
    console_puts("dith4");
    put_near("per_period", (int32_t)p, 505, 3);
    put_near("per_high", (int32_t)h, 200, 4);
    put_near("cc_period", (int32_t)p2, 500, 3);
    put_near("cc_high", (int32_t)h2, 202, 4);
    end_line();
}

/*
 * STOP puts an output whose DRVCTRL.NREx is set at NRVx (DS 31.6.2.4):
 * WO0 low goes high, WO1 high goes low. With DIV1, STATUS.CMPx then reads 0
 */
static void check_stop(void)
{
    uint32_t before, after, t;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0);
    T0->TCC_DRVCTRL = TCC_DRVCTRL_NRE0_Msk | TCC_DRVCTRL_NRV0_Msk |
                      TCC_DRVCTRL_NRE1_Msk;
    sync0();
    t = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    scope_wait_until(t, SCOPE_CYC(32));     /* COUNT about 30 */
    before = *IN_A;
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    sync0();
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    after = *IN_A;
    console_puts("stop");
    put_exact("w0_before", (before >> wo_pin[0]) & 1U, 0, 1);
    put_exact("w1_before", (before >> wo_pin[1]) & 1U, 1, 1);
    put_exact("w0_after", (after >> wo_pin[0]) & 1U, 1, 1);
    put_exact("w1_after", (after >> wo_pin[1]) & 1U, 0, 1);
    put_exact("cmp", T0->TCC_STATUS >> 24 & 0xFU, 0, 1);
    put_exact("stopped", (T0->TCC_STATUS & TCC_STATUS_STOP_Msk) != 0U, 1, 1);
    end_line();
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccwave: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_TCC1_Msk |
                                MCLK_APBCMASK_TCC2_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TCC2_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    for (unsigned int x = 0; x < 8; x++) {
        scope_pmux(GA, wo_pin[x], x < 2 ? FUNC_E : FUNC_F, 0);
        wo_mask |= 1U << wo_pin[x];
    }
    scope_start();
    check_units();
    check_otmx();
    check_dti();
    check_swap_pol();
    check_patt_inven();
    check_dscritical();
    check_ramp();
    check_dith();
    check_stop();
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    for (unsigned int x = 0; x < 8; x++) {
        scope_release(GA, wo_pin[x]);
    }
    test_exit();
}
