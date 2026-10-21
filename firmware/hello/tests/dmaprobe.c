/*
 * DMA trigger probe (PIC32CM MC00, DMAC U2223): the TSENS, SDADC and TCC
 * DMA requests the twin did not wire yet (DS 34.6.5.1, 37.6.4, 40.5.3).
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 dmaprobe
 *
 * DMAC channel 0, one beat per trigger, trigger numbers from the ATDF's
 * DMAC_ID_* parameters (dmac.h's are wrong from 0x0A up). TCC0 on
 * GCLK_TCC0 1 MHz, DIV1, NPWM, PER 99 (a 100 us period), CC1 50; capture
 * input PA02 (EXTINT2, its pull turned by PORT.OUT), HIGH sense, ASYNCH,
 * asynchronous EVSYS channel 0 to TCC0 MC_2. Each line: the buffer, the
 * write-back BTCNT and CHINTFLAG at the listed time.
 *
 *   X1  TSENS RESRDY (1), free running: 3 words from VALUE, after 50 ms
 *   X2  SDADC RESRDY (38), free running: 4 words from RESULT, after 50 ms
 *   X3  TCC0 OVF (10): a table of 4 to CCBUF0, after 250 and 600 us; CC0
 *   X4  TCC0 MC1 (12), compare: 3 beats RAM to RAM, after 250 and 600 us
 *   X5  TCC0 MC2 (13), capture (CPTEN2, MCEI2): 3 words from CC2, three
 *       input pulses 30 us apart
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "scope.h"

#define DM              DMAC_REGS
#define T0              TCC0_REGS
#define TS              TSENS_REGS
#define SD              SDADC_REGS
#define GA              0U
#define FUNC_A          0U
#define PIN_IN          2U
#define GEN_EIC2        16U
#define USER_MC2        13U
#define TRIG_TSENS      1U
#define TRIG_TCC0_OVF   10U
#define TRIG_TCC0_MC1   12U
#define TRIG_TCC0_MC2   13U
#define TRIG_SDADC      38U
#define HWORD           (1U << 8)
#define WORD            (2U << 8)
#define SRCINC          0x0400U
#define DSTINC          0x0800U
#define VALID           0x0001U

typedef struct {
    uint16_t btctrl;
    uint16_t btcnt;
    uint32_t src;
    uint32_t dst;
    uint32_t next;
} desc_t;

static desc_t desc[1] __attribute__((aligned(16)));
static volatile desc_t wb[1] __attribute__((aligned(16)));  /* DMA writes */
static volatile uint32_t buf[6];
static const uint32_t table[4] = { 10, 20, 30, 40 };
static volatile uint32_t word_src = 0x5A5A, word_dst;

static void nl(void)
{
    console_puts("\r\n");
}

static void sync0(void)
{
    for (uint32_t i = 0; T0->TCC_SYNCBUSY != 0U && i < 200000U; i++) {
    }
}

static void dma_go(uint32_t trig, uint16_t btctrl, uint16_t n, uint32_t src,
                   uint32_t dst)
{
    DM->DMAC_CTRL = 0;
    DM->DMAC_CTRL = DMAC_CTRL_SWRST_Msk;
    for (uint32_t i = 0; (DM->DMAC_CTRL & DMAC_CTRL_SWRST_Msk) && i < 100000U;
         i++) {
    }
    for (unsigned int k = 0; k < 6; k++) {
        buf[k] = 0xEEEEEEEEU;
    }
    desc[0].btctrl = btctrl;
    desc[0].btcnt = n;
    desc[0].src = src;
    desc[0].dst = dst;
    desc[0].next = 0;
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAC_CTRL_DMAENABLE_Msk | 0x0F00U;
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLB = DMAC_CHCTRLB_TRIGSRC(trig) | DMAC_CHCTRLB_TRIGACT_BEAT;
    DM->DMAC_CHCTRLA = DMAC_CHCTRLA_ENABLE_Msk;
}

static void put_state(void)
{
    DM->DMAC_CHID = 0;
    put_field("btcnt", wb[0].btcnt, 4);
    put_field("chintflag", DM->DMAC_CHINTFLAG, 2);
}

static void put_buf(unsigned int n)
{
    for (unsigned int k = 0; k < n; k++) {
        console_putc(' ');
        console_put_hex(buf[k], 8);
    }
}

static void tsens_case(void)
{
    TS->TSENS_CTRLA = TSENS_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; TS->TSENS_SYNCBUSY && i < 200000U; i++) {
    }
    dma_go(TRIG_TSENS, VALID | WORD | DSTINC, 3, (uint32_t)&TS->TSENS_VALUE,
           (uint32_t)&buf[3]);
    TS->TSENS_GAIN = 0x10000U;          /* 2.8 ms; GAIN 0 counts as 2^24 */
    TS->TSENS_CTRLC = TSENS_CTRLC_FREERUN_Msk;
    TS->TSENS_CTRLA = TSENS_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; TS->TSENS_SYNCBUSY && i < 200000U; i++) {
    }
    TS->TSENS_CTRLB = TSENS_CTRLB_START_Msk;
    delay_ms(50);
    console_puts("X1 tsens");
    put_buf(3);
    put_state();
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    nl();
    TS->TSENS_CTRLA = TSENS_CTRLA_SWRST_Msk;
}

static void sdadc_case(void)
{
    SD->SDADC_CTRLA = SDADC_CTRLA_SWRST_Msk;
    for (uint32_t i = 0; SD->SDADC_SYNCBUSY && i < 200000U; i++) {
    }
    dma_go(TRIG_SDADC, VALID | WORD | DSTINC, 4, (uint32_t)&SD->SDADC_RESULT,
           (uint32_t)&buf[4]);
    SD->SDADC_REFCTRL = SDADC_REFCTRL_REFSEL_INTVCC;
    SD->SDADC_CTRLB = SDADC_CTRLB_PRESCALER_DIV32 | SDADC_CTRLB_OSR(0) |
                      SDADC_CTRLB_SKPCNT(2);
    SD->SDADC_INPUTCTRL = 1U;
    SD->SDADC_CTRLC = SDADC_CTRLC_FREERUN_Msk;
    for (uint32_t i = 0; SD->SDADC_SYNCBUSY && i < 200000U; i++) {
    }
    SD->SDADC_CTRLA = SDADC_CTRLA_ENABLE_Msk;
    for (uint32_t i = 0; SD->SDADC_SYNCBUSY && i < 200000U; i++) {
    }
    SD->SDADC_SWTRIG = SDADC_SWTRIG_START_Msk;
    delay_ms(50);
    console_puts("X2 sdadc");
    put_buf(4);
    put_state();
    put_field("intflag", SD->SDADC_INTFLAG, 2);
    nl();
    SD->SDADC_CTRLA = SDADC_CTRLA_SWRST_Msk;
}

static void tcc_setup(uint32_t ctrla, uint32_t evctrl)
{
    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    T0->TCC_CTRLA = ctrla;
    T0->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
    sync0();
    T0->TCC_PER = 99;
    T0->TCC_CC[0] = 5;
    T0->TCC_CC[1] = 50;
    T0->TCC_EVCTRL = evctrl;
    sync0();
}

static void tcc_ovf_case(void)
{
    uint32_t t0, a, b;

    tcc_setup(0, 0);
    dma_go(TRIG_TCC0_OVF, VALID | WORD | SRCINC, 4, (uint32_t)&table[4],
           (uint32_t)&T0->TCC_CCBUF[0]);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    scope_wait_until(t0, SCOPE_CYC(250));
    a = wb[0].btcnt;
    scope_wait_until(t0, SCOPE_CYC(600));
    b = wb[0].btcnt;
    console_puts("X3 tcc_ovf");
    put_field("btcnt_250us", a, 4);
    put_field("btcnt_600us", b, 4);
    put_state();
    put_field("cc0", T0->TCC_CC[0], 8);
    nl();
}

static void tcc_mc_case(void)
{
    uint32_t t0, a, b;

    tcc_setup(0, 0);
    dma_go(TRIG_TCC0_MC1, VALID | WORD, 3, (uint32_t)&word_src,
           (uint32_t)&word_dst);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    scope_wait_until(t0, SCOPE_CYC(250));
    a = wb[0].btcnt;
    scope_wait_until(t0, SCOPE_CYC(600));
    b = wb[0].btcnt;
    console_puts("X4 tcc_mc1");
    put_field("btcnt_250us", a, 4);
    put_field("btcnt_600us", b, 4);
    put_state();
    put_field("dst", word_dst, 8);
    nl();
}

static void tcc_capture_case(void)
{
    uint32_t t0;

    scope_pull(GA, PIN_IN, 0);
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 1U;
    tcc_setup(TCC_CTRLA_CPTEN2_Msk, TCC_EVCTRL_MCEI2_Msk);
    dma_go(TRIG_TCC0_MC2, VALID | WORD | DSTINC, 3, (uint32_t)&T0->TCC_CC[2],
           (uint32_t)&buf[3]);
    t0 = scope_now();
    T0->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    for (unsigned int k = 0; k < 3; k++) {
        scope_wait_until(t0, SCOPE_CYC(20 + 30 * k));
        scope_pull(GA, PIN_IN, 1);
        scope_wait_until(t0, SCOPE_CYC(25 + 30 * k));
        scope_pull(GA, PIN_IN, 0);
    }
    scope_wait_until(t0, SCOPE_CYC(150));
    console_puts("X5 tcc_capture");
    put_buf(3);
    put_state();
    put_field("intflag", T0->TCC_INTFLAG, 8);
    nl();
    EVSYS_REGS->EVSYS_USER[USER_MC2] = 0U;
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dmaprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_AHBMASK |= MCLK_AHBMASK_DMAC_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_TSENS_Msk |
                                MCLK_APBAMASK_EIC_Msk;
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk |
                                MCLK_APBCMASK_SDADC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(48) |
                                 GCLK_GENCTRL_GENEN_Msk;
    for (uint32_t i = 0; GCLK_REGS->GCLK_SYNCBUSY != 0U && i < 200000U;
         i++) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] = GCLK_PCHCTRL_GEN(1) |
                                            GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
                                             GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SDADC_GCLK_ID] = GCLK_PCHCTRL_GEN(0) |
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

    tsens_case();
    sdadc_case();
    scope_start();
    tcc_ovf_case();
    tcc_mc_case();
    tcc_capture_case();

    T0->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync0();
    DM->DMAC_CTRL = 0;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_SWRST_Msk;
    EVSYS_REGS->EVSYS_CHANNEL[0] = 0;
    scope_release(GA, PIN_IN);
    PORT_REGS->GROUP[GA].PORT_OUTCLR = 1U << PIN_IN;
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
