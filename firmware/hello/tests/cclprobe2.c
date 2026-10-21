/*
 * CCL probe 2 (PIC32CM MC00, CCL U2225): the TCC, TC and SERCOM input
 * taps, filter and synchronizer latency, the edge detector, the event
 * input and the JK toggle rate (ccl-u2225.md E5-E7, E9, E12-E14). Prints
 * raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 cclprobe2
 *
 * Setup as cclprobe: outputs read on OUT0 PB23, OUT1 PB11, OUT2 PB09 and
 * LUT3 through its event on PB02 (PORT event 0); LUT1 IN3 = PA08 pulled
 * by PORT.OUT. Timing: GCLK_CCL on GCLK1 = OSCULP32K / 32 (1024 Hz, 977
 * us a cycle) and the output polled with SysTick, delays printed in us.
 * Counting: LUT0's event through resynchronized EVSYS channel 1 into TC4
 * (COUNT16, EVACT COUNT).
 *
 *   D1  TCC0/1/2 WO[0..2] statics (CCx 0 or above PER) on LUT0/1/2 IN0-2,
 *       LUT3 IN0-2 = TCC
 *   D2  TC0-TC4 WO[0] statics: LUTn IN0 = TC and ALTTC
 *   D3  latency PA08 -> OUT1: FILTSEL 0, SYNCH, FILTER (rise and fall)
 *   D4  EDGESEL with SYNCH: pulse width per rising edge, falling edges
 *   D5  JK with J = K = 1 on 1 MHz: OUT0 rising edges in 2 ms
 *   D6  EVENT input: software events on an asynchronous channel into
 *       LUTIN0, LUT0 buffer: edges counted, strobe width at 1024 Hz
 *   D7  SERCOM1 as input: disabled, enabled idle
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
#define EVENT           3U
#define IO              4U
#define TC              6U
#define ALTTC           7U
#define TCCIN           8U
#define SERCOM          9U
#define JK              2U
#define T4              (TC4_REGS->COUNT16)
#define CYCLE_US        977U

static tcc_registers_t *const tccs[3] = { TCC0_REGS, TCC1_REGS, TCC2_REGS };
static tc_registers_t *const tcs[5] = {
    TC0_REGS, TC1_REGS, TC2_REGS, TC3_REGS, TC4_REGS
};

static void nl(void)
{
    console_puts("\r\n");
}

static uint32_t lut(uint32_t in0, uint32_t in1, uint32_t in2, uint32_t truth,
                    uint32_t extra)
{
    return (in0 << 8) | (in1 << 12) | (in2 << 16) | (truth << 24) | extra;
}

static void ccl_setup(uint8_t seq0, uint32_t l0, uint32_t l1, uint32_t l2,
                      uint32_t l3)
{
    CC->CCL_CTRL = C_SWRST;
    for (volatile int i = 0; i < 10; i++) {
    }
    CC->CCL_SEQCTRL[0] = seq0;
    CC->CCL_LUTCTRL[0] = l0;
    CC->CCL_LUTCTRL[1] = l1;
    CC->CCL_LUTCTRL[2] = l2;
    CC->CCL_LUTCTRL[3] = l3;
    CC->CCL_CTRL = C_ENABLE;
    delay_us(60);
}

static unsigned int pin_in(unsigned int g, unsigned int pin)
{
    return (PORT_REGS->GROUP[g].PORT_IN >> pin) & 1U;
}

static unsigned int out(unsigned int n)
{
    static const uint8_t pins[4] = { 23, 11, 9, 2 };

    return pin_in(GB, pins[n]);
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

/* TCCn NPWM on GCLK0, PER 0xFF, WO[x] high for bit x of @v (x < 3) */
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

static void case_tcc(void)
{
    console_puts("D1");
    for (unsigned int n = 0; n < 4; n++) {
        unsigned int src = n < 3 ? n : 0;

        console_puts(" lut");
        console_put_dec(n);
        console_putc(':');
        for (unsigned int v = 0; v < 8; v++) {
            unsigned int got = 0;

            tcc_static(src, v);
            for (unsigned int x = 0; x < 3; x++) {
                static const uint8_t buf[3] = { 0xAA, 0xCC, 0xF0 };
                uint32_t l = lut(TCCIN, TCCIN, TCCIN, buf[x],
                                 L_ENABLE | L_LUTEO);

                ccl_setup(0, n == 0 ? l : 0, n == 1 ? l : 0, n == 2 ? l : 0,
                          n == 3 ? l : 0);
                got |= out(n) << x;
            }
            console_putc((char)('0' + got));
        }
    }
    for (unsigned int n = 0; n < 3; n++) {
        tccs[n]->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    }
    nl();
}

/* TCm COUNT8 NPWM, PER 0x7F: WO[0] high if @high */
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

static void case_tc(void)
{
    console_puts("D2");
    for (unsigned int m = 0; m < 5; m++) {
        for (unsigned int k = 0; k < 5; k++) {
            tc_static(k, k == m);
        }
        delay_us(30);
        console_puts(" tc");
        console_put_dec(m);
        console_puts("_high:");
        for (unsigned int n = 0; n < 4; n++) {
            for (unsigned int alt = 0; alt < 2; alt++) {
                uint32_t l = lut(alt ? ALTTC : TC, MASK, MASK, 0xAA,
                                 L_ENABLE | L_LUTEO);

                ccl_setup(0, n == 0 ? l : 0, n == 1 ? l : 0, n == 2 ? l : 0,
                          n == 3 ? l : 0);
                console_putc((char)('0' + out(n)));
            }
            console_putc(n < 3 ? ',' : ' ');
        }
    }
    for (unsigned int k = 0; k < 5; k++) {
        tcs[k]->COUNT8.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    }
    nl();
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

static void case_latency(void)
{
    static const uint32_t filt[3] = { 0, L_SYNCH, L_FILTER };
    static const char *const name[3] = { "none", "synch", "filter" };

    console_puts("D3");
    gclk_ccl(1);
    for (unsigned int f = 0; f < 3; f++) {
        uint32_t lo = 0xFFFFFFFFU, hi = 0;

        ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA, L_ENABLE | filt[f]), 0, 0);
        scope_pull(GA, 8, 0);
        delay_us(10000);
        for (unsigned int k = 0; k < 6; k++) {
            uint32_t a, b;

            delay_us(137 * k);                  /* vary the phase */
            a = latency(1);
            delay_us(8000);
            b = latency(0);
            delay_us(8000);
            lo = a < lo ? a : lo;
            hi = a > hi ? a : hi;
            lo = b < lo ? b : lo;
            hi = b > hi ? b : hi;
        }
        console_putc(' ');
        console_puts(name[f]);
        put_field("min_us", lo, 4);
        put_field("max_us", hi, 4);
    }
    nl();
}

static void case_edge(void)
{
    uint32_t t0, w = 0, fall = 0;

    console_puts("D4");
    ccl_setup(0, 0, lut(IO, MASK, MASK, 0xAA,
                        L_ENABLE | L_SYNCH | L_EDGESEL), 0, 0);
    scope_pull(GA, 8, 0);
    delay_us(10000);
    put_field("idle", out(1), 1);
    scope_pull(GA, 8, 1);
    t0 = scope_now();
    while (!out(1) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    t0 = scope_now();
    while (out(1) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    w = scope_since(t0) / (CPU_CLOCK_HZ / 1000000U);
    put_field("pulse_us", w, 4);
    delay_us(10000);
    put_field("held_high", out(1), 1);
    scope_pull(GA, 8, 0);
    t0 = scope_now();
    while (scope_since(t0) < SCOPE_CYC(10000)) {
        fall |= out(1);
    }
    put_field("on_fall", fall, 1);
    nl();
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

static void case_jk(void)
{
    uint32_t t0, n;

    console_puts("D5");
    gclk_ccl(2);                                /* 1 MHz */
    counter_start();
    ccl_setup(JK, lut(MASK, MASK, MASK, 0xFF, L_ENABLE | L_LUTEO),
              lut(MASK, MASK, MASK, 0xFF, L_ENABLE), 0, 0);
    CC->CCL_CTRL = C_ENABLE;
    n = counter_read();
    t0 = scope_now();
    scope_wait_until(t0, SCOPE_CYC(2000));
    put_field("edges_2ms", counter_read() - n, 8);
    nl();
}

static void case_event(void)
{
    uint32_t t0, n, w;

    console_puts("D6");
    gclk_ccl(2);                                /* 1 MHz */
    counter_start();
    EVSYS_REGS->EVSYS_CHANNEL[2] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_CCL_LUTIN_0] = 3U;     /* ch 2 */
    ccl_setup(0, lut(EVENT, MASK, MASK, 0xAA,
                     L_ENABLE | L_LUTEI | L_LUTEO), 0, 0, 0);
    n = counter_read();
    for (unsigned int k = 0; k < 10; k++) {
        EVSYS_REGS->EVSYS_SWEVT = 1U << 2;
        delay_us(50);
    }
    put_field("swevt_10", counter_read() - n, 4);
    /* strobe width at 1024 Hz */
    gclk_ccl(1);
    delay_us(5000);
    EVSYS_REGS->EVSYS_SWEVT = 1U << 2;
    t0 = scope_now();
    while (!out(0) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    put_field("rise_us", scope_since(t0) / (CPU_CLOCK_HZ / 1000000U), 4);
    t0 = scope_now();
    while (out(0) && scope_since(t0) < SCOPE_CYC(20000)) {
    }
    w = scope_since(t0) / (CPU_CLOCK_HZ / 1000000U);
    put_field("strobe_us", w, 4);
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_CCL_LUTIN_0] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[2] = 0;
    nl();
}

static void case_sercom(void)
{
    sercom_usart_int_registers_t *u = &SERCOM1_REGS->USART_INT;

    console_puts("D7");
    ccl_setup(0, 0, lut(SERCOM, MASK, MASK, 0xAA, L_ENABLE), 0, 0);
    put_field("disabled", out(1), 1);
    u->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; u->SERCOM_SYNCBUSY && i < 200000U; i++) {
    }
    u->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                      SERCOM_USART_INT_CTRLA_DORD_Msk |
                      SERCOM_USART_INT_CTRLA_TXPO(0) |
                      SERCOM_USART_INT_CTRLA_RXPO(1);
    u->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_TXEN_Msk;
    u->SERCOM_BAUD = 63019;                     /* 115200 at 48 MHz */
    u->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; u->SERCOM_SYNCBUSY && i < 200000U; i++) {
    }
    delay_us(100);
    put_field("idle", out(1), 1);
    u->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_SWRST_Msk;
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP cclprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_CCL_Msk |
                                MCLK_APBCMASK_EVSYS_Msk |
                                MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_TCC1_Msk |
                                MCLK_APBCMASK_TCC2_Msk |
                                MCLK_APBCMASK_TC0_Msk | MCLK_APBCMASK_TC1_Msk |
                                MCLK_APBCMASK_TC2_Msk | MCLK_APBCMASK_TC3_Msk |
                                MCLK_APBCMASK_TC4_Msk |
                                MCLK_APBCMASK_SERCOM1_Msk;
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
            TC2_GCLK_ID, TC4_GCLK_ID, SERCOM1_GCLK_ID_CORE,
            EVSYS_GCLK_ID_1
        };

        for (unsigned int i = 0; i < sizeof(ids); i++) {
            GCLK_REGS->GCLK_PCHCTRL[ids[i]] = GCLK_PCHCTRL_GEN(0) |
                                              GCLK_PCHCTRL_CHEN_Msk;
        }
    }
    scope_pmux(GA, 8, FUNC_I, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GB, 23, FUNC_I, 0);
    scope_pmux(GB, 11, FUNC_I, 0);
    scope_pmux(GB, 9, FUNC_I, 0);
    PORT_REGS->GROUP[GB].PORT_DIRSET = 1U << 2;
    PORT_REGS->GROUP[GB].PORT_PINCFG[2] = PORT_PINCFG_INEN_Msk;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = 2U | (0U << 5) | (1U << 7);
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_CCL_LUTOUT_3) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 1U;
    /* LUT0's event counted by TC4 */
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_CCL_LUTOUT_0) |
                                   EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                                   EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC4_EVU] = 2U;
    scope_start();

    case_tcc();
    case_tc();
    case_latency();
    case_edge();
    case_jk();
    case_event();
    case_sercom();

    CC->CCL_CTRL = C_SWRST;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_PORT_EV_0] = 0U;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TC4_EVU] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    EVSYS_REGS->EVSYS_CHANNEL[1] = 0;
    PORT_REGS->GROUP[GB].PORT_EVCTRL = 0;
    PORT_REGS->GROUP[GB].PORT_DIRCLR = 1U << 2;
    scope_release(GA, 8);
    scope_release(GB, 23);
    scope_release(GB, 11);
    scope_release(GB, 9);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
