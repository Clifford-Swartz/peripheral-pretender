/*
 * CCL probe (PIC32CM MC00, CCL U2225): registers, truth tables, the input
 * order, IO inputs, LINK, FEEDBACK, the sequencers and the output event
 * (docs/research/peripherals/ccl-u2225.md experiments E0-E4, E10-E12,
 * E15). Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 cclprobe
 *
 * Bare EV15N46A, GCLK_CCL on GCLK0 (48 MHz). Inputs are pads muxed to the
 * CCL (function I, J for IN9-11) with their pull resistor turned by
 * PORT.OUT: LUT0 IN0 = PA04; LUT1 IN3/4/5 = PA08/PA09/PB10; LUT2 IN6/7/8 =
 * PA22/PA23/PB08. Outputs read on their pads through PORT.IN: OUT0 PB23,
 * OUT1 PB11, OUT2 PB09; LUT3 (no free pad) through its LUTOUT event on an
 * asynchronous EVSYS channel into PORT event 0, OUT action on PB02.
 *
 *   C0  registers: reset, write masks, SWRST, enable protection
 *   C1  LUT1 all MASK: OUT1 = TRUTH[0]; the pad with the LUT or CCL off
 *   C2  LUT1 on PA08/PA09/PB10: each one-hot TRUTH over the 8 inputs
 *   C3  PA08 as a GPIO output (PMUXEN 0): does LUT1 IN0 see it?
 *   C4  LINK: LUT0 IN0 = LINK (LUT1); LUT3 IN0 = LINK (LUT0?)
 *   C5  FEEDBACK with SEQSEL DISABLE, then with DFF
 *   C6  sequencer 0: DFF, JK, LATCH, RS from PA04 (even) and LUT1 (odd)
 *   C7  output event: LUTEO 0/1, LUT3 through the event path
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define CC              CCL_REGS
#define GA              0U
#define GB              1U
#define FUNC_I          8U
#define FUNC_J          9U
#define C_SWRST         0x01U
#define C_ENABLE        0x02U
#define L_ENABLE        (1U << 1)
#define L_EDGESEL       (1U << 7)
#define L_LUTEI         (1U << 21)
#define L_LUTEO         (1U << 22)
#define MASK            0U
#define FEEDBACK        1U
#define LINK            2U
#define IO              4U
#define DFF             1U
#define JK              2U
#define LATCH           3U
#define RS              4U

static void nl(void)
{
    console_puts("\r\n");
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

static void ccl_reset(void)
{
    CC->CCL_CTRL = C_SWRST;
    for (volatile int i = 0; i < 10; i++) {
    }
}

/* SWRST, then SEQCTRL, the four LUTCTRLs and CTRL.ENABLE */
static void ccl_setup(uint8_t seq0, uint32_t l0, uint32_t l1, uint32_t l2,
                      uint32_t l3)
{
    ccl_reset();
    CC->CCL_SEQCTRL[0] = seq0;
    CC->CCL_LUTCTRL[0] = l0;
    CC->CCL_LUTCTRL[1] = l1;
    CC->CCL_LUTCTRL[2] = l2;
    CC->CCL_LUTCTRL[3] = l3;
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
}

static unsigned int pin_in(unsigned int g, unsigned int pin)
{
    return (PORT_REGS->GROUP[g].PORT_IN >> pin) & 1U;
}

static unsigned int out0(void)
{
    return pin_in(GB, 23);
}

static unsigned int out1(void)
{
    return pin_in(GB, 11);
}

static unsigned int out3(void)
{
    return pin_in(GB, 2);                   /* the event drives the pin */
}

/* LUT1's inputs IN3/4/5 (PA08, PA09, PB10) pulled to bits 0-2 of @v */
static void in_lut1(unsigned int v)
{
    scope_pull(GA, 8, v & 1U);
    scope_pull(GA, 9, (v >> 1) & 1U);
    scope_pull(GB, 10, (v >> 2) & 1U);
    pins_settle();
}

static void put_bit(const char *name, unsigned int v)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    console_putc((char)('0' + v));
}

static void case_registers(void)
{
    console_puts("C0");
    ccl_reset();
    put_field("ctrl", CC->CCL_CTRL, 2);
    put_field("lut0", CC->CCL_LUTCTRL[0], 8);
    CC->CCL_CTRL = 0x43;
    CC->CCL_SEQCTRL[0] = 0xFF;
    CC->CCL_LUTCTRL[1] = 0xFFFFFFFFU;
    put_field("ctrl_ones", CC->CCL_CTRL, 2);
    put_field("seq_ones", CC->CCL_SEQCTRL[0], 2);
    put_field("lut1_ones", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_CTRL = C_SWRST;
    put_field("swrst_read", CC->CCL_CTRL, 2);
    put_field("after_swrst", CC->CCL_LUTCTRL[1], 8);
    /* enable protection: CCL on, LUT1 written */
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x11, 0);
    CC->CCL_CTRL = C_ENABLE;
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x22, 0);
    put_field("truth_on", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x33, L_ENABLE);
    put_field("truth_with_en", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x44, 0);
    put_field("truth_en_off", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_SEQCTRL[0] = 2;
    put_field("seq_on", CC->CCL_SEQCTRL[0], 2);
    CC->CCL_CTRL = C_ENABLE | 0x40;
    put_field("runstdby_on", CC->CCL_CTRL, 2);
    nl();
}

static void case_masked(void)
{
    static const uint8_t t[] = { 0x00, 0x01, 0xFE, 0xFF, 0x55 };

    console_puts("C1");
    for (unsigned int i = 0; i < sizeof(t); i++) {
        ccl_setup(0, 0, lut(MASK, MASK, MASK, t[i], L_ENABLE), 0, 0);
        console_puts(" t");
        console_put_hex(t[i], 2);
        put_bit("", out1());
    }
    /* TRUTH 0xFF: pull PB11 down, then LUT off, then CCL off */
    ccl_setup(0, 0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE), 0, 0);
    PORT_REGS->GROUP[GB].PORT_PINCFG[11] |= PORT_PINCFG_PULLEN_Msk;
    scope_pull(GB, 11, 0);
    pins_settle();
    put_bit("on_pulldown", out1());
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0xFF, 0);
    pins_settle();
    put_bit("lut_off_pulldown", out1());
    scope_pull(GB, 11, 1);
    pins_settle();
    put_bit("lut_off_pullup", out1());
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x00, L_ENABLE);
    pins_settle();
    put_bit("t00_pullup", out1());
    CC->CCL_CTRL = 0;
    pins_settle();
    put_bit("ccl_off_pullup", out1());
    scope_pull(GB, 11, 0);
    pins_settle();
    put_bit("ccl_off_pulldown", out1());
    PORT_REGS->GROUP[GB].PORT_PINCFG[11] &= ~PORT_PINCFG_PULLEN_Msk;
    nl();
}

static void case_truth(void)
{
    console_puts("C2");
    for (unsigned int b = 0; b < 8; b++) {
        unsigned int seen = 0;

        ccl_setup(0, 0, lut(IO, IO, IO, 1U << b, L_ENABLE), 0, 0);
        for (unsigned int v = 0; v < 8; v++) {
            in_lut1(v);
            seen |= out1() << v;
        }
        console_puts(" t");
        console_put_hex(1U << b, 2);
        console_putc(':');
        console_put_hex(seen, 2);
    }
    /* IN0 only, IN1 only, IN2 only as buffers */
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    in_lut1(1);
    put_bit("in0_only_1", out1());
    in_lut1(6);
    put_bit("in0_only_6", out1());
    nl();
}

static void case_gpio_pad(void)
{
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    console_puts("C3");
    PORT_REGS->GROUP[GA].PORT_PINCFG[8] = PORT_PINCFG_INEN_Msk;  /* GPIO */
    PORT_REGS->GROUP[GA].PORT_DIRSET = 1U << 8;
    PORT_REGS->GROUP[GA].PORT_OUTSET = 1U << 8;
    pins_settle();
    put_bit("gpio_high", out1());
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << 8;
    pins_settle();
    put_bit("gpio_low", out1());
    PORT_REGS->GROUP[GA].PORT_DIRCLR = 1U << 8;
    scope_pmux(GA, 8, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    nl();
}

static void case_link(void)
{
    console_puts("C4");
    /* LUT0 = buffer of LINK (LUT1 = buffer of PA08) */
    ccl_setup(0, lut(LINK, MASK, MASK, 0xAA, L_ENABLE),
              lut(IO, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    in_lut1(1);
    put_bit("lut0_link_1", out0());
    in_lut1(0);
    put_bit("lut0_link_0", out0());
    /* LUT3 = buffer of LINK: LUT0 (wrap?) at TRUTH 0xFF, then 0x00 */
    ccl_setup(0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE), 0, 0,
              lut(LINK, MASK, MASK, 0xAA, L_ENABLE | L_LUTEO));
    put_bit("lut3_link_lut0_1", out3());
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[0] = 0;                 /* LUT off: writable again */
    CC->CCL_LUTCTRL[0] = lut(MASK, MASK, MASK, 0x00, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
    put_bit("lut3_link_lut0_0", out3());
    nl();
}

static void case_feedback(void)
{
    console_puts("C5");
    /* SEQ0 DISABLE: LUT0 = 1 (masked), LUT1 = buffer of FEEDBACK */
    ccl_setup(0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE),
              lut(FEEDBACK, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    put_bit("seq_off_lut0_1", out1());
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[0] = 0;
    CC->CCL_LUTCTRL[0] = lut(MASK, MASK, MASK, 0x00, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
    put_bit("seq_off_lut0_0", out1());
    nl();
}

/*
 * Sequencer 0: even LUT0 = buffer of PA04 (D, J or S), odd LUT1 = buffer
 * of PA08 (G, K or R); print OUT0 after each step of @steps (bit 0 PA04,
 * bit 1 PA08)
 */
static void seq_run(const char *name, uint8_t mode, const uint8_t *steps,
                    unsigned int n)
{
    ccl_reset();
    CC->CCL_SEQCTRL[0] = mode;
    CC->CCL_LUTCTRL[1] = lut(IO, MASK, MASK, 0xAA, L_ENABLE);
    CC->CCL_LUTCTRL[0] = lut(IO, MASK, MASK, 0xAA, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    CC->CCL_CTRL = C_ENABLE;                    /* DS: write it twice */
    console_puts(" ");
    console_puts(name);
    console_putc(':');
    for (unsigned int i = 0; i < n; i++) {
        scope_pull(GA, 4, steps[i] & 1U);
        scope_pull(GA, 8, (steps[i] >> 1) & 1U);
        pins_settle();
        console_putc((char)('0' + out0()));
    }
    console_putc('/');
    console_putc((char)('0' + out1()));
}

static void case_sequencer(void)
{
    /* D/J/S on PA04 (bit 0), G/K/R on PA08 (bit 1) */
    static const uint8_t dff[] = { 0x1, 0x3, 0x1, 0x0, 0x2, 0x0, 0x3, 0x1 };
    static const uint8_t jk[] = { 0x0, 0x1, 0x0, 0x2, 0x0, 0x1, 0x3, 0x3 };

    console_puts("C6");
    seq_run("dff", DFF, dff, sizeof(dff));
    seq_run("jk", JK, jk, sizeof(jk));
    seq_run("latch", LATCH, dff, sizeof(dff));
    seq_run("rs", RS, jk, sizeof(jk));
    /* clear: disabling the even LUT */
    CC->CCL_LUTCTRL[0] = lut(IO, MASK, MASK, 0xAA, 0);
    pins_settle();
    put_bit("even_off", out0());
    nl();
}

static void case_event(void)
{
    console_puts("C7");
    /* LUT3 TRUTH 0xFF with LUTEO, then without */
    ccl_setup(0, 0, 0, 0, lut(MASK, MASK, MASK, 0xFF, L_ENABLE | L_LUTEO));
    put_bit("lut3_eo_1", out3());
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[3] = 0;
    CC->CCL_LUTCTRL[3] = lut(MASK, MASK, MASK, 0x00, L_ENABLE | L_LUTEO);
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
    put_bit("lut3_eo_0", out3());
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[3] = 0;
    CC->CCL_LUTCTRL[3] = lut(MASK, MASK, MASK, 0xFF, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    pins_settle();
    put_bit("lut3_no_eo_t_ff", out3());
    nl();
}

/* How soon after CTRL.ENABLE = 0 a LUTCTRL write takes effect */
static void case_disable(void)
{
    unsigned int n = 0;

    console_puts("C8");
    ccl_setup(0, 0, lut(MASK, MASK, MASK, 0x11, L_ENABLE), 0, 0);
    CC->CCL_CTRL = 0;
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x22, L_ENABLE);
    put_field("at_once", CC->CCL_LUTCTRL[1], 8);
    put_field("ctrl", CC->CCL_CTRL, 2);
    pins_settle();
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x33, L_ENABLE);
    put_field("after_60us", CC->CCL_LUTCTRL[1], 8);
    ccl_setup(0, 0, lut(MASK, MASK, MASK, 0x11, L_ENABLE), 0, 0);
    CC->CCL_CTRL = 0;
    do {
        CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x44, L_ENABLE);
        n++;
    } while ((CC->CCL_LUTCTRL[1] >> 24) != 0x44 && n < 100000U);
    put_field("tries", n, 8);
    /* and enabling: is the CCL output there at once? */
    ccl_setup(0, 0, 0, 0, 0);
    CC->CCL_CTRL = 0;
    pins_settle();
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0xFF, L_ENABLE);
    CC->CCL_CTRL = C_ENABLE;
    put_bit("out_at_enable", out1());
    pins_settle();
    put_bit("out_later", out1());
    nl();
}

/* LUTCTRL with CTRL.ENABLE = 0: which writes take while the LUT is on */
static void case_protect(void)
{
    console_puts("C9");
    ccl_reset();
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x11, L_ENABLE);
    put_field("set_with_enable", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x22, 0);
    put_field("with_disable", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x33, 0);
    put_field("while_off", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x44, L_ENABLE);
    put_field("off_to_on", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_LUTCTRL[1] = lut(MASK, MASK, MASK, 0x55, L_ENABLE);
    put_field("on_to_on", CC->CCL_LUTCTRL[1], 8);
    CC->CCL_SEQCTRL[0] = 2;
    put_field("seq_ccl_off", CC->CCL_SEQCTRL[0], 2);
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP cclprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_CCL_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[CCL_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    /* inputs with their pulls, outputs with their input buffers */
    scope_pmux(GA, 4, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, 8, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, 9, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GB, 10, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GB, 23, FUNC_I, 0);
    scope_pmux(GB, 11, FUNC_I, 0);
    scope_pmux(GB, 9, FUNC_I, 0);
    /* LUT3's event into PORT event 0: OUT action on PB02 */
    PORT_REGS->GROUP[GB].PORT_DIRSET = 1U << 2;
    PORT_REGS->GROUP[GB].PORT_PINCFG[2] = PORT_PINCFG_INEN_Msk;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = 2U | (0U << 5) | (1U << 7);
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_CCL_LUTOUT_3) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 1U;

    case_registers();
    case_masked();
    case_truth();
    case_gpio_pad();
    case_link();
    case_feedback();
    case_sequencer();
    case_event();
    case_disable();
    case_protect();

    ccl_reset();
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = 0;
    PORT_REGS->GROUP[GB].PORT_DIRCLR = 1U << 2;
    scope_release(GA, 4);
    scope_release(GA, 8);
    scope_release(GA, 9);
    scope_release(GB, 10);
    scope_release(GB, 23);
    scope_release(GB, 11);
    scope_release(GB, 9);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
