/*
 * ADC probe 2 (PIC32CM MC00, ADC U2247): conversion times run by run
 * (adcprobe averaged them, and a first conversion seemed long), the START
 * and FLUSH event inputs, the RESRDY event output, the RESRDY DMA trigger
 * and ADC1 as ADC0's client (CTRLA.SLAVEEN, CTRLC.DUALSEL). Prints raw
 * results; checks nothing.
 * Run: py -m tools.periph probe mc00 adcprobe2
 *
 * ADC0 on GCLK0 (48 MHz), REFCTRL INTVCC2, input SCALEDVDDCORE (0x1A) or a
 * sequence of 0x1A and SCALEDVDDANA (0x1B); 12 bits, SAMPLEN 0. Times in
 * CPU cycles (48 MHz). The event input is PA02 (EXTINT2, its pull turned
 * by PORT.OUT), HIGH sense, ASYNCH, asynchronous EVSYS channel 0 to
 * ADC0 START (user 28) or SYNC (the flush, user 29).
 *
 *   R1-R3  enable, then 5 single conversions each timed: DIV32, DIV256, DIV8
 *   R4     free running right after the enable: the first 4 RESRDYs
 *   R5     the same after one single conversion
 *   E1     START event (pull up at t0): pull to RESRDY, 3 times; E2 STARTINV
 *   E3     FLUSH (SYNC) event during free running: RESRDY gaps around it
 *   E4     RESRDYEO to EVSYS channel 1 (resynchronized): CHINTFLAG.EVD
 *   D1     DMAC channel 0 on ADC0_RESRDY, 6 halfwords from RESULT, the
 *          2-input sequence free running: the buffer, then INTFLAG
 *   M1     ADC1 client (SLAVEEN), DUALSEL BOTH: one START, both RESRDY
 *   M2     DUALSEL INTERLEAVE: 4 STARTs, which ADC each time
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
#define CORE            0x1AU
#define ANA             0x1BU
#define TMO             SCOPE_CYC(20000)

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

static void nl(void)
{
    console_puts("\r\n");
}

static void sync_adc(adc_registers_t *a)
{
    for (uint32_t i = 0; a->ADC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void adc_setup(uint32_t prescaler, uint32_t ctrlc, uint32_t seq,
                      uint32_t evctrl, int enable)
{
    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A0);
    A0->ADC_CTRLB = prescaler;
    A0->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
    A0->ADC_SAMPCTRL = 0;
    A0->ADC_INPUTCTRL = CORE;
    A0->ADC_SEQCTRL = seq;
    A0->ADC_EVCTRL = evctrl;
    A0->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT | ctrlc;
    sync_adc(A0);
    if (enable) {
        A0->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;
        sync_adc(A0);
    }
    A0->ADC_INTFLAG = 0xFF;
}

/* Cycles from @t0 until RESRDY of @a, or 0xFFFFFF */
static uint32_t wait_resrdy(adc_registers_t *a, uint32_t t0)
{
    while (!(a->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk)) {
        if (scope_since(t0) > TMO) {
            return 0xFFFFFF;
        }
    }
    return scope_since(t0);
}

static void runs(const char *label, uint32_t prescaler)
{
    uint32_t t[5];

    adc_setup(prescaler, 0, 0, 0, 1);
    for (unsigned int k = 0; k < 5; k++) {
        uint32_t t0 = scope_now();

        A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
        scope_wait_until(scope_now(), SCOPE_CYC(50));
    }
    console_puts(label);
    for (unsigned int k = 0; k < 5; k++) {
        console_putc(' ');
        console_put_dec(t[k]);
    }
    nl();
}

static void free_run(const char *label, int single_first)
{
    uint32_t t[4], t0;

    adc_setup(ADC_CTRLB_PRESCALER_DIV32, 0, 0, 0, 1);
    if (single_first) {
        t0 = scope_now();
        A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
        wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
    }
    A0->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT | ADC_CTRLC_FREERUN_Msk;
    sync_adc(A0);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    for (unsigned int k = 0; k < 4; k++) {
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
    }
    console_puts(label);
    for (unsigned int k = 0; k < 4; k++) {
        console_putc(' ');
        console_put_dec(t[k]);
    }
    nl();
}

static void event_start(const char *label, uint32_t inv)
{
    uint32_t t[3];

    scope_pull(GA, PIN_IN, inv ? 1 : 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_START] = 1U;
    adc_setup(ADC_CTRLB_PRESCALER_DIV32, 0, 0,
              ADC_EVCTRL_STARTEI_Msk | inv, 1);
    scope_wait_until(scope_now(), SCOPE_CYC(50));
    A0->ADC_INTFLAG = 0xFF;
    for (unsigned int k = 0; k < 3; k++) {
        uint32_t t0 = scope_now();

        scope_pull(GA, PIN_IN, !inv);
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
        scope_pull(GA, PIN_IN, inv ? 1 : 0);
        scope_wait_until(scope_now(), SCOPE_CYC(50));
        A0->ADC_INTFLAG = 0xFF;
    }
    EVSYS_REGS->EVSYS_USER[USER_ADC0_START] = 0U;
    console_puts(label);
    for (unsigned int k = 0; k < 3; k++) {
        console_putc(' ');
        console_put_dec(t[k]);
    }
    put_field("intflag", A0->ADC_INTFLAG, 2);
    nl();
}

static void event_flush(void)
{
    uint32_t t[6], t0, tf = 0;

    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_SYNC] = 1U;
    adc_setup(ADC_CTRLB_PRESCALER_DIV32, ADC_CTRLC_FREERUN_Msk, 0,
              ADC_EVCTRL_FLUSHEI_Msk, 1);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    for (unsigned int k = 0; k < 6; k++) {
        t[k] = wait_resrdy(A0, t0);
        (void)A0->ADC_RESULT;
        if (k == 2) {
            /* Flush about 200 cycles into the next conversion */
            scope_wait_until(t0, t[k] + 200U);
            tf = scope_since(t0);
            scope_pull(GA, PIN_IN, 1);
        }
    }
    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_ADC0_SYNC] = 0U;
    console_puts("E3 flush");
    put_field("flush_at", tf, 6);
    for (unsigned int k = 0; k < 6; k++) {
        console_putc(' ');
        console_put_dec(t[k]);
    }
    nl();
}

static void event_out(void)
{
    uint32_t before, after;

    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(GEN_ADC0_RESRDY) |
                                   EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                                   EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    adc_setup(ADC_CTRLB_PRESCALER_DIV32, 0, 0, ADC_EVCTRL_RESRDYEO_Msk, 1);
    EVSYS_REGS->EVSYS_INTFLAG = 0xFFFFFFFFU;
    before = EVSYS_REGS->EVSYS_INTFLAG;
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    wait_resrdy(A0, scope_now());
    scope_wait_until(scope_now(), SCOPE_CYC(20));
    after = EVSYS_REGS->EVSYS_INTFLAG;
    EVSYS_REGS->EVSYS_CHANNEL[1] = 0;
    console_puts("E4 resrdyeo");
    put_field("evsys_intflag_before", before, 8);
    put_field("after", after, 8);
    nl();
}

static void dma(void)
{
    uint32_t t0;

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
    DM->DMAC_CHCTRLB = DMAC_CHCTRLB_TRIGSRC_ADC0_RESRDY |
                       DMAC_CHCTRLB_TRIGACT_BEAT;
    DM->DMAC_CHCTRLA = DMAC_CHCTRLA_ENABLE_Msk;
    adc_setup(ADC_CTRLB_PRESCALER_DIV32, ADC_CTRLC_FREERUN_Msk,
              (1U << CORE) | (1U << ANA), 0, 1);
    t0 = scope_now();
    A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
    scope_wait_until(t0, SCOPE_CYC(200));
    console_puts("D1 dma");
    for (unsigned int k = 0; k < 6; k++) {
        console_putc(' ');
        console_put_hex(buf[k], 4);
    }
    put_field("adc_intflag", A0->ADC_INTFLAG, 2);
    put_field("chintflag", DM->DMAC_CHINTFLAG, 2);
    nl();
    DM->DMAC_CHCTRLA = 0;
    DM->DMAC_CTRL = 0;
}

static void dual(const char *label, uint32_t dualsel, unsigned int starts)
{
    uint32_t who[4], r0[4], r1[4];

    A1->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A1);
    A1->ADC_REFCTRL = ADC_REFCTRL_REFSEL_INTVCC2;
    A1->ADC_INPUTCTRL = ANA;
    A1->ADC_CTRLC = ADC_CTRLC_RESSEL_12BIT;
    sync_adc(A1);
    A1->ADC_CTRLA = ADC_CTRLA_SLAVEEN_Msk;
    sync_adc(A1);
    adc_setup(ADC_CTRLB_PRESCALER_DIV32, dualsel, 0, 0, 1);
    A1->ADC_INTFLAG = 0xFF;
    for (unsigned int k = 0; k < starts; k++) {
        uint32_t t0 = scope_now();

        A0->ADC_SWTRIG = ADC_SWTRIG_START_Msk;
        scope_wait_until(t0, SCOPE_CYC(100));
        who[k] = (A0->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk ? 1U : 0U) |
                 (A1->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk ? 2U : 0U);
        r0[k] = A0->ADC_RESULT;
        r1[k] = A1->ADC_RESULT;
        A0->ADC_INTFLAG = 0xFF;
        A1->ADC_INTFLAG = 0xFF;
    }
    console_puts(label);
    for (unsigned int k = 0; k < starts; k++) {
        console_puts(" |");
        console_put_dec(who[k]);
        put_field("r0", r0[k], 4);
        put_field("r1", r1[k], 4);
    }
    put_field("adc1_ctrla", A1->ADC_CTRLA, 2);
    nl();
    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A0);
    A1->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A1);
}

int main(void)
{
    sys_initialize();
    console_puts("PP adcprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk |
                                MCLK_APBCMASK_ADC1_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[ADC0_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
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

    runs("R1 div32", ADC_CTRLB_PRESCALER_DIV32);
    runs("R2 div256", ADC_CTRLB_PRESCALER_DIV256);
    runs("R3 div8", ADC_CTRLB_PRESCALER_DIV8);
    free_run("R4 freerun_fresh", 0);
    free_run("R5 freerun_after_single", 1);
    event_start("E1 start_event", 0);
    event_start("E2 start_event_inv", ADC_EVCTRL_STARTINV_Msk);
    event_flush();
    event_out();
    dma();
    dual("M1 dual_both", ADC_CTRLC_DUALSEL_BOTH, 2);
    dual("M2 dual_interleave", ADC_CTRLC_DUALSEL_INTERLEAVE, 4);

    A0->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    sync_adc(A0);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
