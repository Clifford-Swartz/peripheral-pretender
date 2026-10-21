/*
 * PDEC probe 2 (PIC32CM MC00, PDEC U2263): what pdecprobe left open
 * (CC0 was cleared by its SWRST): angular and revolution counters, the
 * index, MC0/MC1 against CC0/CC1, X2 and X4S, COUNTER and HALL modes,
 * the buffers. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 pdecprobe2
 *
 * Setup as pdecprobe: A = PA08, B = PA09 (function J, pulls), index = PA02
 * through EIC EXTINT2 and an asynchronous channel to EVU2; FILTER 63.
 * HALL uses the event path for C. Snaps: COUNT (READSYNC), STATUS,
 * INTFLAG, then cleared.
 *
 *   R1  PEREN, ANGULAR 0, CC0 = 3 | 2 << 9: 14 steps up, 6 down
 *   R2  index at A = B = 0 and at A = 1 (CC0 = 7 | 3 << 9), up and down
 *   R3  PEREN 0, ANGULAR 7, CC0 = 5, CC1 = 3: counting 0..7 and back
 *   R4  X2 (CONF 2): the A-leading cycle, the reverse, S0 alone with S1 0/1
 *   R5  X4S (CONF 1), CC0 angular TOP 7: index at the wrap, mid-revolution,
 *       missing
 *   R6  COUNTER: PRESC DIV1024 counts in 10 ms; PEREN CC0 = 4687, CC1 =
 *       1000: flags over 110 ms; without START
 *   R7  HALL: codes 1-3-2-6-4-5-1 and back, then 0 and 7; CC0 = 3
 *   R8  CCBUF0, CCBUFV0, LUPD, UPDATE
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define PD              PDEC_REGS
#define GA              0U
#define FUNC_J          9U
#define FUNC_A          0U
#define PIN_A           8U
#define PIN_B           9U
#define PIN_I           2U
#define GEN_EIC2        16U
#define USER_EVU(n)     (44U + (n))
#define ENABLE          (1U << 1)
#define MODE_HALL       (1U << 2)
#define MODE_COUNTER    (2U << 2)
#define CONF(n)         ((uint32_t)(n) << 8)
#define PEREN           (1U << 15)
#define PINEN(n)        (1U << (16 + (n)))
#define ANGULAR(n)      ((uint32_t)(n) << 24)
#define CMD_UPDATE      (2U << 5)
#define CMD_READSYNC    (3U << 5)
#define CMD_START       (4U << 5)
#define LUPD            (1U << 1)
#define EVEI(n)         (1U << (5 + (n)))

static void nl(void)
{
    console_puts("\r\n");
}

static void sync(void)
{
    for (uint32_t i = 0; PD->PDEC_SYNCBUSY && i < 200000U; i++) {
    }
}

static void cmd(uint8_t c)
{
    PD->PDEC_CTRLBSET = c;
    sync();
    for (uint32_t i = 0; (PD->PDEC_CTRLBSET & 0xE0U) && i < 200000U; i++) {
    }
}

/* SWRST, CC0/CC1, CTRLA, EVCTRL, FILTER 63, PRESC, enabled, started */
static void pdec_setup(uint32_t ctrla, uint16_t evctrl, uint32_t cc0,
                       uint32_t cc1, uint8_t presc, int start)
{
    PD->PDEC_CTRLA = 1U;
    sync();
    PD->PDEC_CC[0] = cc0;
    PD->PDEC_CC[1] = cc1;
    PD->PDEC_CTRLA = ctrla;
    PD->PDEC_EVCTRL = evctrl;
    PD->PDEC_FILTER = 63;
    PD->PDEC_PRESC = presc;
    sync();
    PD->PDEC_CTRLA = ctrla | ENABLE;
    sync();
    if (start) {
        cmd(CMD_START);
    }
    PD->PDEC_INTFLAG = 0xFF;
}

static void step(unsigned int a, unsigned int b)
{
    scope_pull(GA, PIN_A, a);
    scope_pull(GA, PIN_B, b);
    delay_us(200);
}

static void index_level(unsigned int v)
{
    scope_pull(GA, PIN_I, v);
    delay_us(200);
}

static void snap(void)
{
    cmd(CMD_READSYNC);
    console_puts(" c");
    console_put_hex(PD->PDEC_COUNT & 0xFFFFU, 4);
    console_puts("/s");
    console_put_hex(PD->PDEC_STATUS, 4);
    console_puts("/f");
    console_put_hex(PD->PDEC_INTFLAG, 2);
    PD->PDEC_INTFLAG = 0xFF;
    PD->PDEC_STATUS = 0x003F;
    sync();
}

static const uint8_t fwd[4] = { 1, 3, 2, 0 };          /* 10 11 01 00 */
static const uint8_t rev[4] = { 2, 3, 1, 0 };          /* 01 11 10 00 */

static void steps(const uint8_t *seq, unsigned int n, int snaps)
{
    for (unsigned int k = 0; k < n; k++) {
        step(seq[k % 4] & 1U, seq[k % 4] >> 1);
        if (snaps) {
            snap();
        }
    }
}

static void case_angular(void)
{
    console_puts("R1");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1) | PEREN | ANGULAR(0), 0,
               3U | (2U << 9), 0, 0, 1);
    steps(fwd, 14, 1);
    console_puts(" |");
    steps(rev, 6, 1);
    nl();
}

static void case_index(void)
{
    console_puts("R2");
    step(0, 0);
    index_level(0);
    pdec_setup(PINEN(0) | PINEN(1) | PEREN, EVEI(2), 7U | (3U << 9), 0, 0,
               1);
    steps(fwd, 6, 0);
    snap();                         /* 6, at A = 0 B = 1 */
    step(0, 0);
    snap();                         /* 7 */
    index_level(1);
    console_puts(" idx_hi");
    snap();
    index_level(0);
    console_puts(" idx_lo");
    snap();
    step(1, 0);
    index_level(1);
    console_puts(" idx_at_a1");
    snap();
    index_level(0);
    snap();
    step(0, 0);                     /* reverse: down */
    step(0, 1);
    snap();
    step(0, 0);
    index_level(1);
    console_puts(" idx_down");
    snap();
    index_level(0);
    snap();
    nl();
}

static void case_compare(void)
{
    console_puts("R3");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1) | ANGULAR(7), 0, 5, 3, 0, 1);
    steps(fwd, 7, 1);
    console_puts(" |");
    steps(rev, 7, 1);
    nl();
}

static void case_x2(void)
{
    console_puts("R4");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1) | CONF(2), 0, 0, 0, 0, 1);
    steps(fwd, 4, 1);
    console_puts(" |");
    steps(rev, 4, 1);
    console_puts(" | a_b0");
    step(1, 0);
    step(0, 0);
    step(1, 0);
    step(0, 0);
    snap();
    console_puts(" a_b1");
    step(0, 1);
    step(1, 1);
    step(0, 1);
    step(1, 1);
    step(0, 1);
    snap();
    nl();
}

static void case_x4s(void)
{
    console_puts("R5");
    step(0, 0);
    index_level(0);
    pdec_setup(PINEN(0) | PINEN(1) | PEREN | CONF(1), EVEI(2), 7U, 0, 0, 1);
    index_level(1);
    index_level(0);
    console_puts(" first_idx");
    snap();
    steps(fwd, 8, 0);               /* a revolution, back at 00 */
    index_level(1);
    index_level(0);
    console_puts(" idx_at_wrap");
    snap();
    steps(fwd, 4, 0);               /* half way */
    index_level(1);
    index_level(0);
    console_puts(" idx_mid");
    snap();
    steps(fwd, 12, 0);
    console_puts(" no_idx");
    snap();
    nl();
}

static void case_counter(void)
{
    uint32_t t0, a, b;

    console_puts("R6");
    pdec_setup(MODE_COUNTER, 0, 0, 0, 10, 0);   /* DIV1024: 46.875 kHz */
    cmd(CMD_READSYNC);
    a = PD->PDEC_COUNT;
    t0 = scope_now();
    scope_wait_until(t0, SCOPE_CYC(10000));
    cmd(CMD_READSYNC);
    put_field("no_start_10ms", PD->PDEC_COUNT - a, 4);
    put_field("status", PD->PDEC_STATUS, 4);
    cmd(CMD_START);
    cmd(CMD_READSYNC);
    a = PD->PDEC_COUNT;
    t0 = scope_now();
    scope_wait_until(t0, SCOPE_CYC(10000));
    cmd(CMD_READSYNC);
    b = PD->PDEC_COUNT;
    put_field("started_10ms", b - a, 4);
    pdec_setup(MODE_COUNTER | PEREN, 0, 4687, 1000, 10, 1);
    t0 = scope_now();
    {
        uint32_t seen[3] = { 0, 0, 0 };
        uint32_t t;

        while ((t = scope_since(t0)) < SCOPE_CYC(110000)) {
            uint8_t f = PD->PDEC_INTFLAG;

            for (unsigned int k = 0; k < 3; k++) {
                static const uint8_t bit[3] = { 0x01, 0x10, 0x20 };

                if ((f & bit[k]) && !seen[k]) {
                    seen[k] = t / (CPU_CLOCK_HZ / 1000U);
                    if (!seen[k]) {
                        seen[k] = 1;
                    }
                }
            }
        }
        put_field("ovf_ms", seen[0], 4);
        put_field("mc0_ms", seen[1], 4);
        put_field("mc1_ms", seen[2], 4);
    }
    nl();
}

/* HALL: C on PA02 through EVU2 (events), A and B on pins */
static void hall(unsigned int code)
{
    scope_pull(GA, PIN_A, code & 1U);
    scope_pull(GA, PIN_B, (code >> 1) & 1U);
    scope_pull(GA, PIN_I, (code >> 2) & 1U);
    delay_us(200);
}

static void case_hall(void)
{
    static const uint8_t seq[6] = { 3, 2, 6, 4, 5, 1 };
    static const uint8_t back[6] = { 5, 4, 6, 2, 3, 1 };

    console_puts("R7");
    hall(1);
    pdec_setup(MODE_HALL | PINEN(0) | PINEN(1), EVEI(2), 3, 0, 0, 1);
    snap();
    for (unsigned int k = 0; k < 6; k++) {
        hall(seq[k]);
        snap();
    }
    console_puts(" |");
    for (unsigned int k = 0; k < 6; k++) {
        hall(back[k]);
        snap();
    }
    console_puts(" | c0");
    hall(0);
    snap();
    hall(7);
    console_puts(" c7");
    snap();
    nl();
}

static void case_buffers(void)
{
    console_puts("R8");
    step(0, 0);
    pdec_setup(PINEN(0) | PINEN(1) | PEREN | ANGULAR(7), 0, 0x100, 0, 0, 1);
    PD->PDEC_CCBUF[0] = 0x200;
    sync();
    put_field("cc0", PD->PDEC_CC[0], 8);
    put_field("status", PD->PDEC_STATUS, 4);
    step(1, 0);
    put_field("cc0_step", PD->PDEC_CC[0], 8);
    put_field("status", PD->PDEC_STATUS, 4);
    PD->PDEC_CTRLBSET = LUPD;
    sync();
    PD->PDEC_CCBUF[0] = 0x300;
    sync();
    step(1, 1);
    put_field("cc0_lupd", PD->PDEC_CC[0], 8);
    cmd(CMD_UPDATE);
    put_field("cc0_update", PD->PDEC_CC[0], 8);
    put_field("status", PD->PDEC_STATUS, 4);
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP pdecprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_PDEC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[PDEC_GCLK_ID] &
                           GCLK_PCHCTRL_CHEN_Msk) && i < 200000U; i++) {
    }
    scope_pmux(GA, PIN_A, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_B, FUNC_J, PORT_PINCFG_PULLEN_Msk);
    scope_pmux(GA, PIN_I, FUNC_A, PORT_PINCFG_PULLEN_Msk);
    scope_pull(GA, PIN_I, 0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EIC_REGS->EIC_CONFIG0 = 0x4U << (4U * PIN_I);
    EIC_REGS->EIC_ASYNCH = 1U << PIN_I;
    EIC_REGS->EIC_EVCTRL = 1U << PIN_I;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; EIC_REGS->EIC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
    EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_EVGEN(GEN_EIC2) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 1U;
    scope_start();

    case_angular();
    case_index();
    case_compare();
    case_x2();
    case_x4s();
    case_counter();
    case_hall();
    case_buffers();

    PD->PDEC_CTRLA = 1U;
    sync();
    EVSYS_REGS->EVSYS_USER[USER_EVU(2)] = 0U;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    scope_release(GA, PIN_A);
    scope_release(GA, PIN_B);
    scope_release(GA, PIN_I);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = (1U << PIN_A) | (1U << PIN_B) |
                                       (1U << PIN_I);
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
