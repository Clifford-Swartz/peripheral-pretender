/*
 * DAC probe (PIC32CM MC00, DAC U2214): DATABUF, the START event, EMPTY and
 * UNDERRUN, the EMPTY event and DMA request, dithering (DS 39.6.3-39.6.8).
 * Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 dacprobe
 *
 * Bare EV15N46A. DAC on GCLK0 (48 MHz) against VDDANA with EOEN; VOUT is
 * PA02 = ADC0 AIN0, read back as a 12-bit code against INTVCC2 (VDDANA),
 * so a DAC code reads about 4 times as much. START events are software
 * events on asynchronous EVSYS channel 1 (user DAC_START). The EMPTY
 * event goes to resynchronized channel 2 (EVD flag, GCLK_EVSYS_2 on
 * GCLK0). DMA: channel 0, trigger 39 (the ATDF's DMAC_ID_EMPTY).
 *
 *   D1  INTFLAG and STATUS after SWRST, after the enable (STARTEI 0 / 1)
 *   D2  DATABUF written with STARTEI 0: VOUT, INTFLAG
 *   D3  STARTEI 1: DATA write, DATABUF write (EMPTY?), START: VOUT, EMPTY
 *   D4  START with DATABUF empty: UNDERRUN, VOUT kept
 *   D5  INTFLAG.EMPTY written 1; DATABUF written again
 *   D6  INVEI with a software event
 *   D7  EVCTRL written while enabled (enable-protected?)
 *   D8  EMPTYEO: EVD at the enable and after each START
 *   D9  DMA on EMPTY: a table of 4 into DATABUF, 4 STARTs
 *   D10 DITHER: EMPTY and UNDERRUN after each of 20 STARTs
 *   D11 SYNCBUSY: loops until DATA / DATABUF / ENABLE writes are synced
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define DA              DAC_REGS
#define EV              EVSYS_REGS
#define DM              DMAC_REGS
#define VOUT_AIN        0U
#define EMPTY           DAC_INTFLAG_EMPTY_Msk
#define UNDERRUN        DAC_INTFLAG_UNDERRUN_Msk
#define STARTEI         DAC_EVCTRL_STARTEI_Msk
#define EMPTYEO         DAC_EVCTRL_EMPTYEO_Msk
#define INVEI           DAC_EVCTRL_INVEI_Msk
#define OUT_VDD         (DAC_CTRLB_EOEN_Msk | DAC_CTRLB_REFSEL_VDDANA)
#define EVD2            (1U << 18)
#define TRIG_DAC_EMPTY  39U

typedef struct {
    uint16_t btctrl;
    uint16_t btcnt;
    uint32_t src;
    uint32_t dst;
    uint32_t next;
} desc_t;

static desc_t desc[1] __attribute__((aligned(16)));
static volatile desc_t wb[1] __attribute__((aligned(16)));   /* DMA writes */
static const uint16_t table[4] = { 100, 300, 500, 700 };

static void nl(void)
{
    console_puts("\r\n");
}

static uint32_t dac_sync(void)
{
    uint32_t n = 0;

    while (DA->DAC_SYNCBUSY != 0U && n < WAIT_LOOPS) {
        n++;
    }
    return n;
}

/* SWRST, then CTRLB and EVCTRL, enabled unless @en is 0 */
static void dac_setup(uint8_t ctrlb, uint8_t evctrl, int en)
{
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    dac_sync();
    DA->DAC_CTRLB = ctrlb;
    DA->DAC_EVCTRL = evctrl;
    if (en) {
        DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
        dac_sync();
        for (uint32_t n = 0; !(DA->DAC_STATUS & DAC_STATUS_READY_Msk) &&
                             n < WAIT_LOOPS; n++) {
        }
    }
}

static uint16_t vout(void)
{
    delay_us(200);                          /* settle */
    return adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2);
}

static void start(void)
{
    EV->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
}

static void put_flags(const char *name)
{
    put_field(name, DA->DAC_INTFLAG, 2);
}

static void case_reset(void)
{
    dac_setup(OUT_VDD, 0, 0);
    console_puts("D1");
    put_flags("swrst");
    put_field("status", DA->DAC_STATUS, 2);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    put_flags("at_enable");
    dac_sync();
    delay_us(100);
    put_flags("enabled");
    put_field("status", DA->DAC_STATUS, 2);
    dac_setup(OUT_VDD, STARTEI, 0);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    delay_us(100);
    put_flags("enabled_startei");
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    put_flags("cleared");
    DA->DAC_CTRLA = 0;
    dac_sync();
    put_flags("disabled");
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    delay_us(100);
    put_flags("reenabled");
    nl();
}

static void case_databuf_no_ei(void)
{
    dac_setup(OUT_VDD, 0, 1);
    DA->DAC_DATA = 200;
    dac_sync();
    console_puts("D2");
    put_field("vout_data200", vout(), 4);
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    DA->DAC_DATABUF = 600;
    dac_sync();
    put_flags("after_databuf");
    put_field("vout", vout(), 4);
    start();
    put_flags("after_start");
    put_field("vout", vout(), 4);
    nl();
}

static void case_start(void)
{
    dac_setup(OUT_VDD, STARTEI, 1);
    console_puts("D3");
    put_flags("enabled");
    DA->DAC_DATA = 100;
    dac_sync();
    put_field("vout_data100", vout(), 4);
    put_flags("after_data");
    DA->DAC_DATABUF = 700;
    dac_sync();
    put_flags("after_databuf");
    put_field("vout", vout(), 4);
    start();
    put_flags("after_start");
    put_field("vout", vout(), 4);
    nl();

    console_puts("D4");
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    start();
    put_flags("start_empty");
    put_field("vout", vout(), 4);
    nl();

    console_puts("D5");
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    DA->DAC_DATABUF = 400;
    dac_sync();
    put_flags("databuf");
    start();
    put_flags("start");
    DA->DAC_INTFLAG = EMPTY;
    put_flags("w1c_empty");
    put_field("vout", vout(), 4);
    DA->DAC_DATABUF = 800;
    dac_sync();
    DA->DAC_DATABUF = 900;              /* a second write before START */
    dac_sync();
    start();
    put_flags("two_writes_start");
    put_field("vout", vout(), 4);
    start();
    put_flags("again");
    put_field("vout", vout(), 4);
    nl();
}

static void case_invei(void)
{
    dac_setup(OUT_VDD, STARTEI | INVEI, 1);
    DA->DAC_DATABUF = 300;
    dac_sync();
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    console_puts("D6");
    start();
    put_flags("start");
    put_field("vout", vout(), 4);
    nl();
}

static void case_protect(void)
{
    dac_setup(OUT_VDD, 0, 1);
    DA->DAC_EVCTRL = EMPTYEO | STARTEI;
    console_puts("D7");
    put_field("evctrl", DA->DAC_EVCTRL, 2);
    DA->DAC_CTRLB = DAC_CTRLB_EOEN_Msk;
    put_field("ctrlb", DA->DAC_CTRLB, 2);
    nl();
}

static void case_empty_event(void)
{
    EV->EVSYS_CHANNEL[2] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_DAC_EMPTY) |
                           EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    delay_us(20);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    dac_setup(OUT_VDD, STARTEI | EMPTYEO, 1);
    delay_us(20);
    console_puts("D8");
    put_field("evd_enable", EV->EVSYS_INTFLAG & EVD2, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    DA->DAC_DATABUF = 500;
    dac_sync();
    delay_us(20);
    put_field("evd_databuf", EV->EVSYS_INTFLAG & EVD2, 8);
    start();
    put_field("evd_start", EV->EVSYS_INTFLAG & EVD2, 8);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    start();
    put_field("evd_underrun", EV->EVSYS_INTFLAG & EVD2, 8);
    put_flags("intflag");
    nl();
    EV->EVSYS_CHANNEL[2] = 0;
}

static void case_dma(void)
{
    DM->DMAC_CTRL = 0;
    DM->DMAC_CTRL = DMAC_CTRL_SWRST_Msk;
    for (uint32_t i = 0; (DM->DMAC_CTRL & DMAC_CTRL_SWRST_Msk) && i < 100000U;
         i++) {
    }
    desc[0].btctrl = 0x0001U | (1U << 8) | 0x0400U;    /* VALID, HWORD, SRCINC */
    desc[0].btcnt = 4;
    desc[0].src = (uint32_t)&table[4];
    desc[0].dst = (uint32_t)&DA->DAC_DATABUF;
    desc[0].next = 0;
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAC_CTRL_DMAENABLE_Msk | 0x0F00U;
    dac_setup(OUT_VDD, STARTEI, 0);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLB = DMAC_CHCTRLB_TRIGSRC(TRIG_DAC_EMPTY) |
                       DMAC_CHCTRLB_TRIGACT_BEAT;
    DM->DMAC_CHCTRLA = DMAC_CHCTRLA_ENABLE_Msk;
    delay_us(20);
    console_puts("D9");
    put_field("before_enable", DM->DMAC_CHINTFLAG, 2);
    put_field("ch_btcnt", DM->DMAC_ACTIVE >> 16, 4);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    dac_sync();
    delay_us(20);
    put_flags("enabled");
    put_field("active", DM->DMAC_ACTIVE, 8);
    for (unsigned int k = 0; k < 5; k++) {
        start();
        console_puts(" s");
        console_put_dec(k + 1);
        put_field("vout", vout(), 4);
        put_flags("f");
    }
    DM->DMAC_CHID = 0;
    put_field("chintflag", DM->DMAC_CHINTFLAG, 2);
    put_field("wb_btcnt", wb[0].btcnt, 4);
    nl();
    DM->DMAC_CHCTRLA = 0;
    DM->DMAC_CTRL = 0;
}

static void case_dither(void)
{
    uint32_t e = 0, u = 0;

    dac_setup(OUT_VDD | DAC_CTRLB_DITHER_Msk, STARTEI, 1);
    DA->DAC_DATABUF = (300U << 4) | 8U;
    dac_sync();
    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    for (unsigned int k = 0; k < 20; k++) {
        start();
        e |= (uint32_t)!!(DA->DAC_INTFLAG & EMPTY) << k;
        u |= (uint32_t)!!(DA->DAC_INTFLAG & UNDERRUN) << k;
        DA->DAC_INTFLAG = EMPTY | UNDERRUN;
        if (k == 0) {
            DA->DAC_DATABUF = (600U << 4) | 4U;
            dac_sync();
        }
    }
    console_puts("D10");
    put_field("empty_bits", e, 8);
    put_field("underrun_bits", u, 8);
    put_field("vout", vout(), 4);
    nl();
}

static void case_sync(void)
{
    uint32_t a, b, c;

    dac_setup(OUT_VDD, 0, 0);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    a = dac_sync();
    DA->DAC_DATA = 10;
    b = dac_sync();
    dac_setup(OUT_VDD, STARTEI, 1);
    DA->DAC_DATABUF = 10;
    c = dac_sync();
    console_puts("D11");
    put_field("enable", a, 4);
    put_field("data", b, 4);
    put_field("databuf", c, 4);
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dacprobe: " BOARD_BANNER "\r\n");
    adc0_init(0U, 2U);                              /* PA02: AIN0 / VOUT */
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_DAC_Msk |
                                MCLK_APBCMASK_EVSYS_Msk;
    MCLK_REGS->MCLK_AHBMASK |= MCLK_AHBMASK_DMAC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_2] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[DAC_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    EV->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                           EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 2U;   /* channel 1 */

    case_reset();
    case_databuf_no_ei();
    case_start();
    case_invei();
    case_protect();
    case_empty_event();
    case_dma();
    case_dither();
    case_sync();

    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 0U;
    EV->EVSYS_CHANNEL[1] = 0;
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    dac_sync();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
