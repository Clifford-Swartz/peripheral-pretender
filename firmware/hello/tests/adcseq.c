/*
 * ADC timing, sequence, event, DMA and dual-mode check (PIC32CM MC00, ADC
 * U2247) against what silicon did in firmware/hello APP=adcprobe..adcprobe4
 * (docs/research/peripherals/adc-u2247.md).
 * Run: py -m tools.periph check mc00 adcseq
 *
 * Bare EV15N46A. ADC0 (and ADC1) on GCLK0 (48 MHz), REFCTRL INTVCC2,
 * inputs SCALEDVDDCORE (0x1A) and SCALEDVDDANA (0x1B), no pins. Times in
 * CPU cycles (48 MHz); DIV32 makes a CLK_ADC 32 of them. Times are
 * compared as differences between settings, and values as orders: the
 * board's VDDANA (5 V on the desk, 3.3 V on the twin) changes both. The
 * event input is PA02 (EXTINT2, its pull turned by PORT.OUT), HIGH sense,
 * ASYNCH, asynchronous EVSYS channel 0 to ADC0 START (user 28) or SYNC
 * (user 29).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define A0              ADC0_REGS
#define A1              ADC1_REGS
#define DM              DMAC_REGS
#define GA              0U
#define FUNC_A          0U
#define PIN_IN          2U                      /* PA02, EXTINT2 */
#define GEN_EIC2        16U
#define GEN_ADC0_RESRDY 67U
#define USER_ADC0_START 28U
#define USER_ADC0_SYNC  29U
#define TRIG_ADC0       36U     /* the ATDF's DMAC_ID_RESRDY; dmac.h's is wrong */
#define CORE            0x1AU
#define ANA             0x1BU
#define TMO             SCOPE_CYC(20000)
#define CLK             32U     /* CPU cycles per CLK_ADC at DIV32 */

typedef struct {
    uint16_t btctrl;
    uint16_t btcnt;
    uint32_t src;
    uint32_t dst;
    uint32_t next;
} desc_t;

static desc_t desc[1] __attribute__((aligned(16)));
static desc_t wb[1] __attribute__((aligned(16)));
static volatile uint16_t buf[6];

static void sync_adc(adc_registers_t *a)
{
    for (uint32_t i = 0; a->ADC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 2);
}

static void adc_setup(uint32_t ctrlc, uint32_t sampctrl, uint32_t avgctrl,
                      uint32_t seq, uint32_t evctrl)
{
    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A0);
    A0->ADC_CTRLB = ADC_CTRLB_PRESCALER_DIV32;
    A0->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
    A0->ADC_SAMPCTRL = sampctrl;
    A0->ADC_AVGCTRL = avgctrl;
    A0->ADC_INPUTCTRL = CORE;
    A0->ADC_SEQCTRL = seq;
    A0->ADC_EVCTRL = evctrl;
    A0->ADC_CTRLC = ctrlc;
    sync_adc(A0);
    A0->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
    sync_adc(A0);
    A0->ADC_INTFLAG = 0xFF;
}

static uint32_t wait_resrdy(adc_registers_t *a, uint32_t t0)
{
    while (!(a->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk)) {
        if (scope_since(t0) > TMO) {
            return 0xFFFFFF;
        }
    }
    return scope_since(t0);
}

/* A conversion's time, after a first one (the start-up) and a pause */
static uint32_t conv_time(uint32_t ctrlc, uint32_t sampctrl, uint32_t avgctrl,
                          uint32_t *first)
{
    uint32_t t0;

    adc_setup(ctrlc, sampctrl, avgctrl, 0, 0);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    *first = wait_resrdy(A0, t0);
    (void)A0->ADC_RESULT;
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    t0 = wait_resrdy(A0, t0);
    (void)A0->ADC_RESULT;
    return t0;
}

static void check_timing(void)
{
    uint32_t first, t12, t8, ts15, toff, tavg;

    t12 = conv_time(ADC_CTRLC_RESSEL_12BIT, 0, 0, &first);
    t8 = conv_time(ADC_CTRLC_RESSEL_8BIT, 0, 0, &first);
    ts15 = conv_time(ADC_CTRLC_RESSEL_12BIT, ADC_SAMPCTRL_SAMPLEN(15), 0,
                     &first);
    toff = conv_time(ADC_CTRLC_RESSEL_12BIT, ADC_SAMPCTRL_OFFCOMP_Msk, 0,
                     &first);
    tavg = conv_time(ADC_CTRLC_RESSEL_16BIT, 0, ADC_AVGCTRL_SAMPLENUM_16 |
                     ADC_AVGCTRL_ADJRES(4), &first);
    console_puts("timing");
    /* 12 bits, SAMPLEN 0: 14 CLK_ADC and about 40 cycles to start */
    put_dec_range("conv12", t12, 14 * CLK + 10, 14 * CLK + 80);
    /* The first after the enable waits for the 10 us start-up */
    put_dec_range("startup", first - tavg, 400, 560);
    /* 8 bits: 4 CLK_ADC fewer; SAMPLEN 15: 15 more; OFFCOMP: 3 more */
    put_dec_range("less_8b", t12 - t8, 4 * CLK - 40, 4 * CLK + 40);
    put_dec_range("more_s15", ts15 - t12, 15 * CLK - 40, 15 * CLK + 40);
    put_dec_range("more_offcomp", toff - t12, 3 * CLK - 40, 3 * CLK + 40);
    /* 16 samples: 16 x 13 + 1 CLK_ADC */
    put_dec_range("more_avg16", tavg - t12, 195 * CLK - 60, 195 * CLK + 60);
    end_line();
}

static void check_freerun_sequence(void)
{
    uint32_t t[4], st[4], r[4], t0, f;

    adc_setup(ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk, 0, 0, 0, 0);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    (void)A0->ADC_RESULT;
    t0 = scope_now();
    for (unsigned int k = 0; k < 4; k++) {
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
    }
    console_puts("freerun");
    /* Back to back: 13 CLK_ADC */
    put_dec_range("period", (t[3] - t[0]) / 3U, 13 * CLK - 20, 13 * CLK + 20);
    end_line();

    /* A sequence of 0x1A then 0x1B, read as they come */
    adc_setup(ADC_CTRLC_RESSEL_12BIT, 0, 0, (1U << CORE) | (1U << ANA), 0);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    for (unsigned int k = 0; k < 2; k++) {
        t[k] = wait_resrdy(A0, t0);
        st[k] = A0->ADC_SEQSTATUS;
        r[k] = A0->ADC_RESULT;
    }
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    console_puts("sequence");
    put_exact("seqstatus1", st[0], ADC_SEQSTATUS_SEQBUSY_Msk | CORE);
    put_exact("seqstatus2", st[1], ANA);
    put_exact("core_below_ana", r[0] < r[1], 1);
    put_dec_range("apart", t[1] - t[0], 13 * CLK - 40, 13 * CLK + 40);
    put_exact("ended", A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk, 0);
    /* AIN0, AIN1, 0x1A, 0x1B unread: OVERRUN, the last input and result */
    adc_setup(ADC_CTRLC_RESSEL_12BIT, 0, 0, 3U | (1U << CORE) | (1U << ANA),
              0);
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    scope_wait_until(scope_now(), SCOPE_CYC(500));
    f = A0->ADC_INTFLAG;
    put_exact("unread_flags", f & 3U, 3);
    put_exact("unread_seqstatus", A0->ADC_SEQSTATUS, ANA);
    put_exact("last_is_ana", A0->ADC_RESULT == r[1], 1);
    /* With FREERUN the sequence repeats, SEQBUSY staying set */
    adc_setup(ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk, 0, 0,
              (1U << CORE) | (1U << ANA), 0);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    (void)A0->ADC_RESULT;
    t0 = scope_now();
    for (unsigned int k = 0; k < 4; k++) {
        wait_resrdy(A0, t0);
        st[k] = A0->ADC_SEQSTATUS;
        (void)A0->ADC_RESULT;
    }
    put_exact("repeat_busy", (st[0] & st[1] & st[2] & st[3]) >> 7, 1);
    put_exact("repeat_turns", (st[0] ^ st[1]) & 0x1FU, CORE ^ ANA);
    end_line();
}

static void check_events(void)
{
    uint32_t t0, a, b, tf = 0, t[4];

    /* START event, and with STARTINV on the falling edge */
    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_START] = 1U;
    adc_setup(ADC_CTRLC_RESSEL_12BIT, 0, 0, 0, ADC_EVCTRL_STARTEI_Msk);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    t0 = scope_now();
    scope_pull(GA, PIN_IN, 1);
    a = wait_resrdy(A0, t0);
    (void)A0->ADC_RESULT;
    adc_setup(ADC_CTRLC_RESSEL_12BIT, 0, 0, 0, ADC_EVCTRL_STARTEI_Msk |
              ADC_EVCTRL_STARTINV_Msk);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    t0 = scope_now();
    scope_pull(GA, PIN_IN, 0);
    b = wait_resrdy(A0, t0);
    (void)A0->ADC_RESULT;
    EVSYS_REGS->EVSYS_USER[USER_ADC0_START] = 0U;
    console_puts("events");
    /* The pull takes about 2 us more than SWTRIG */
    put_dec_range("start", a, 14 * CLK + 10, 14 * CLK + 200);
    put_dec_range("start_inv", b, 14 * CLK + 10, 14 * CLK + 200);

    /* SYNC (flush) 200 cycles into a free-running conversion: it starts
     * again, the gap one start and conversion long */
    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_SYNC] = 1U;
    adc_setup(ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk, 0, 0, 0,
              ADC_EVCTRL_FLUSHEI_Msk);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    (void)A0->ADC_RESULT;
    t0 = scope_now();
    for (unsigned int k = 0; k < 4; k++) {
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
        if (k == 1) {
            scope_wait_until(t0, t[k] + 200U);
            tf = scope_since(t0);
            scope_pull(GA, PIN_IN, 1);
        }
    }
    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_SYNC] = 0U;
    put_dec_range("flush_gap", t[2] - tf, 13 * CLK, 15 * CLK + 200);
    put_dec_range("after_flush", t[3] - t[2], 13 * CLK - 20, 13 * CLK + 20);

    /* RESRDYEO through a resynchronized channel: EVSYS EVD1 */
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(GEN_ADC0_RESRDY) |
                                   EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                                   EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    adc_setup(ADC_CTRLC_RESSEL_12BIT, 0, 0, 0, ADC_EVCTRL_RESRDYEO_Msk);
    EVSYS_REGS->EVSYS_INTFLAG = 0xFFFFFFFFU;
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    wait_resrdy(A0, scope_now());
    scope_wait_until(scope_now(), SCOPE_CYC(20));
    put_exact("resrdy_event", (EVSYS_REGS->EVSYS_INTFLAG >> 17) & 1U, 1);
    EVSYS_REGS->EVSYS_CHANNEL[1] = 0;
    /* Read it: silicon's DMA would take a result left behind at once */
    (void)A0->ADC_RESULT;
    end_line();
}

static void check_dma(void)
{
    MCLK_REGS->MCLK_AHBMASK |= MCLK_AHBMASK_DMAC_Msk;
    DM->DMAC_CTRL = 0;
    DM->DMAC_CTRL = DMAC_CTRL_SWRST_Msk;
    for (uint32_t i = 0; (DM->DMAC_CTRL & DMAC_CTRL_SWRST_Msk) && i < 100000U;
         i++) {
    }
    for (unsigned int k = 0; k < 6; k++) {
        buf[k] = 0xEEEE;
    }
    desc[0].btctrl = 0x0001U | (1U << 8) | 0x0800U;   /* VALID, HWORD, DSTINC */
    desc[0].btcnt = 6;
    desc[0].src = (uint32_t)&A0->ADC_RESULT;
    desc[0].dst = (uint32_t)&buf[6];
    desc[0].next = 0;
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAC_CTRL_DMAENABLE_Msk | 0x0F00U;
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLB = DMAC_CHCTRLB_TRIGSRC(TRIG_ADC0) |
                       DMAC_CHCTRLB_TRIGACT_BEAT;
    DM->DMAC_CHCTRLA = DMAC_CHCTRLA_ENABLE_Msk;
    adc_setup(ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk, 0, 0,
              (1U << CORE) | (1U << ANA), 0);
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    scope_wait_until(scope_now(), SCOPE_CYC(200));
    console_puts("dma");
    /* The free-running sequence, one result a beat: 0x1A, 0x1B, ... */
    put_exact("tcmpl", DM->DMAC_CHINTFLAG & DMAC_CHINTFLAG_TCMPL_Msk,
              DMAC_CHINTFLAG_TCMPL_Msk);
    put_exact("turns", buf[0] < buf[1] && buf[2] < buf[3] &&
              buf[4] < buf[5], 1);
    put_exact("steady", buf[0] + 8U > buf[2] && buf[2] + 8U > buf[0] &&
              buf[1] + 8U > buf[5] && buf[5] + 8U > buf[1], 1);
    end_line();
    DM->DMAC_CHCTRLA = 0;
    DM->DMAC_CTRL = 0;
}

static void check_dual(void)
{
    uint32_t who[4];

    console_puts("dual");
    for (unsigned int mode = 0; mode < 2; mode++) {
        unsigned int n = mode ? 4 : 2;

        A1->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
        sync_adc(A1);
        A1->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
        A1->ADC_INPUTCTRL = ANA;
        A1->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT;
        sync_adc(A1);
        A1->ADC_CTRLA = ADC_CTRLA_SLAVEEN_Msk;
        sync_adc(A1);
        adc_setup(ADC_CTRLC_RESSEL_12BIT | (mode ?
                  ADC_CTRLC_DUALSEL_INTERLEAVE : ADC_CTRLC_DUALSEL_BOTH), 0, 0,
                  0, 0);
        A1->ADC_INTFLAG = 0xFF;
        scope_wait_until(scope_now(), SCOPE_CYC(50));
        for (unsigned int k = 0; k < n; k++) {
            uint32_t t0 = scope_now();

            A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
            scope_wait_until(t0, SCOPE_CYC(100));
            who[k] = (A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk ? 1U : 0U) |
                     (A1->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk ? 2U : 0U);
            (void)A0->ADC_RESULT;
            (void)A1->ADC_RESULT;
        }
        if (!mode) {
            put_exact("both", who[0] & who[1], 3);
        } else {
            put_exact("interleave", who[0] | who[1] << 2 | who[2] << 4 |
                      who[3] << 6, 0x99);
        }
        A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
        sync_adc(A0);
        A1->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
        sync_adc(A1);
    }
    end_line();
}

int main(void)
{
    sys_initialize();
    console_puts("PP adcseq: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk |
                                MCLK_APBCMASK_ADC1_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC1_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                           GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_1] = GCLK_PCHCTRL_GEN(0) |
                                               GCLK_PCHCTRL_CHEN_Msk;
    for (uint32_t i = 0; !(GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] &
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
    scope_start();

    check_timing();
    check_freerun_sequence();
    check_events();
    check_dma();
    check_dual();

    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    test_exit();
}
