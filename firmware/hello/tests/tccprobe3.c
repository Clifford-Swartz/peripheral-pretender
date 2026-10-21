/*
 * TCC probe 3 (PIC32CM MC00, TCC U2213): what tccprobe2 left open
 * (docs/research/peripherals/tcc-u2213.md): POL on STATUS.CMPx and on the
 * pin, SWAP where the pair's outputs differ, the outputs after STOP with
 * and without dead time, pattern over dead time, dithering in tenths of
 * a microsecond. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 tccprobe3
 *
 * Pins and clock as in tccprobe2 (TCC0 WO0..7 on PA04/05/18/19/22/23/16/17,
 * GCLK_TCC0 1 MHz, DIV1: 1 us ticks).
 *
 *   P1  POL0 in NPWM and DSBOTTOM: CMP0 and the WO0 pin at two instants
 *   P2  SWAP0 with OTMX 3 (WO0 = CC0, WO4 = CC1), without and with DTI0
 *   P3  STOP with the output low: without DTI, with DTI0
 *   P4  PATT on WO0 and WO4 over DTI0
 *   P5  DITH4 on PER and on CC0, tenths of us
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
#define CMP0            (1U << 24)

static const uint8_t wo_pin[8] = { 4, 5, 18, 19, 22, 23, 16, 17 };
static uint32_t wo_mask;
static ScopeEdge e[NE];
static unsigned int ne;

static void nl(void)
{
    console_puts("\r\n");
}

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void tcc_reset(void)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
}

static uint32_t pin(unsigned int x)
{
    return (PORT_REGS->GROUP[GA].PORT_IN >> wo_pin[x]) & 1U;
}

static void run(uint32_t us)
{
    uint32_t t0;

    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    ne = scope_record(IN_A, wo_mask, t0, SCOPE_CYC(us), e, NE);
}

static void put_outputs(const char *label, unsigned int k, uint8_t which)
{
    console_puts(label);
    for (unsigned int x = 0; x < 8; x++) {
        char name[3] = { 'w', (char)('0' + x), 0 };

        if (which & (1U << x)) {
            scope_put(name, e, ne, wo_pin[x], k);
        }
    }
    nl();
}

static void setup(uint32_t wave, uint32_t wexctrl)
{
    tcc_reset();
    T0->TCC_WAVE = wave;
    T0->TCC_WEXCTRL = wexctrl;
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 20;
    T0->TCC_CC[1] = 40;
    T0->TCC_CC[2] = 60;
    T0->TCC_CC[3] = 80;
}

/* CMP0 and the WO0 pin @a and @b us after the enable */
static void p1_case(const char *name, uint32_t wave, uint32_t a, uint32_t b)
{
    uint32_t t0, s1, p1, s2, p2;

    setup(wave, 0);
    sync0();
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    scope_wait_until(t0, SCOPE_CYC(a));
    s1 = T0->TCC_STATUS & CMP0;
    p1 = pin(0);
    scope_wait_until(t0, SCOPE_CYC(b));
    s2 = T0->TCC_STATUS & CMP0;
    p2 = pin(0);
    console_puts(" ");
    console_puts(name);
    console_puts(":cmp0=");
    console_put_dec(s1 ? 1 : 0);
    console_puts(",pin=");
    console_put_dec(p1);
    console_puts(";cmp0=");
    console_put_dec(s2 ? 1 : 0);
    console_puts(",pin=");
    console_put_dec(p2);
}

static void p1_pol(void)
{
    console_puts("P1 pol");
    p1_case("npwm", TCC_WAVE_WAVEGEN_NPWM, 14, 60);
    p1_case("npwm_pol0", TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk, 14, 60);
    /* DSBOTTOM, PER 99: counting up through CC0 = 20 at 24 us */
    p1_case("dsbottom", TCC_WAVE_WAVEGEN_DSBOTTOM, 14, 60);
    p1_case("dsbottom_pol0", TCC_WAVE_WAVEGEN_DSBOTTOM | TCC_WAVE_POL0_Msk,
            14, 60);
    nl();
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk, 0);
    run(250);
    put_outputs("P1 npwm_pol0_wave", 4, 0x01);
}

static void p2_swap(void)
{
    setup(TCC_WAVE_WAVEGEN_NPWM, TCC_WEXCTRL_OTMX(3));
    run(150);
    put_outputs("P2 otmx3", 3, 0x11);
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_SWAP0_Msk, TCC_WEXCTRL_OTMX(3));
    run(150);
    put_outputs("P2 otmx3_swap0", 3, 0x11);
    setup(TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_SWAP0_Msk,
          TCC_WEXCTRL_OTMX(3) | TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) |
          TCC_WEXCTRL_DTHS(10));
    run(250);
    put_outputs("P2 otmx3_swap0_dti0", 4, 0x11);
}

static void p3_stop(void)
{
    uint32_t a0, s0, b0, b4, sb, t;

    setup(TCC_WAVE_WAVEGEN_NPWM, 0);
    run(50);                    /* WO0 low since 24 us */
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    sync0();
    t = scope_now();
    scope_wait_until(t, SCOPE_CYC(30));
    a0 = pin(0);
    s0 = T0->TCC_STATUS;
    setup(TCC_WAVE_WAVEGEN_NPWM,
          TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) | TCC_WEXCTRL_DTHS(10));
    run(50);
    T0->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
    sync0();
    t = scope_now();
    scope_wait_until(t, SCOPE_CYC(30));
    b0 = pin(0);
    b4 = pin(4);
    sb = T0->TCC_STATUS;
    console_puts("P3 stop");
    put_field("nodti_w0", a0, 1);
    put_field("nodti_status", s0, 8);
    put_field("dti_w0", b0, 1);
    put_field("dti_w4", b4, 1);
    put_field("dti_status", sb, 8);
    nl();
}

static void p4_patt_dti(void)
{
    setup(TCC_WAVE_WAVEGEN_NPWM,
          TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTLS(5) | TCC_WEXCTRL_DTHS(10));
    T0->TCC_PATT = TCC_PATT_PGE0_Msk | (TCC_PATT_PGE0_Msk << 4) |
                   TCC_PATT_PGV0_Msk | (TCC_PATT_PGV0_Msk << 4);
    run(150);
    put_outputs("P4 patt_dti", 3, 0x11);
    /* PATTBUF to the next update */
    T0->TCC_PATTBUF = 0;
    scope_wait_until(scope_now(), SCOPE_CYC(110));
    console_puts("P4 pattbuf_after");
    put_field("w0", pin(0), 1);
    put_field("w4", pin(4), 1);
    put_field("patt", T0->TCC_PATT, 4);
    nl();
}

/* WO0's periods and high times in tenths of a us */
static void put_dith(const char *label)
{
    uint32_t last_r = 0, last_f = 0;
    unsigned int shown = 0;
    uint32_t m = 1U << wo_pin[0];

    console_puts(label);
    console_puts(" p/h=");
    for (unsigned int i = 1; i < ne && shown < 17; i++) {
        if (!((e[i].v ^ e[i - 1].v) & m)) {
            continue;
        }
        if (e[i].v & m) {
            if (last_r) {
                console_put_dec((e[i].t - last_r) * 10U / 48U);
                console_putc('/');
                console_put_dec((last_f - last_r) * 10U / 48U);
                console_putc(' ');
                shown++;
            }
            last_r = e[i].t;
        } else {
            last_f = e[i].t;
        }
    }
    nl();
}

static void p5_dith(void)
{
    static const uint32_t per[2] = { (49U << 4) | 8U, 49U << 4 };
    static const uint32_t cc[2] = { 20U << 4, (20U << 4) | 4U };
    static const char *const label[2] = { "P5 dith4_per", "P5 dith4_cc" };

    for (unsigned int i = 0; i < 2; i++) {
        tcc_reset();
        T0->TCC_CTRLA = TCC_CTRLA_RESOLUTION_DITH4;
        T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
        T0->TCC_PER = per[i];
        T0->TCC_CC[0] = cc[i];
        run(1000);
        put_dith(label[i]);
    }
}

int main(void)
{
    sys_initialize();
    console_puts("PP tccprobe3: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    for (unsigned int x = 0; x < 8; x++) {
        scope_pmux(GA, wo_pin[x], x < 2 ? FUNC_E : FUNC_F, 0);
        wo_mask |= 1U << wo_pin[x];
    }
    scope_start();
    p1_pol();
    p2_swap();
    p3_stop();
    p4_patt_dti();
    p5_dith();
    tcc_reset();
    for (unsigned int x = 0; x < 8; x++) {
        scope_release(GA, wo_pin[x]);
    }
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
