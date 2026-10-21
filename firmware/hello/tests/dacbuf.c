/*
 * DAC buffer check (PIC32CM MC00, DAC U2214): DATABUF, START events,
 * EMPTY and UNDERRUN, the EMPTY event and DMA request, dithering, against
 * what silicon did in firmware/hello APP=dacprobe..dacprobe3
 * (docs/research/peripherals/dac-u2214.md).
 * Run: py -m tools.periph check mc00 dacbuf
 *
 * Bare EV15N46A. DAC on GCLK0 against VDDANA with EOEN; VOUT is PA02 =
 * ADC0 AIN0, read back against VDDANA as well, so the ratio holds on any
 * VDDANA: a DAC code reads code * 4095 / 1023 within 5 % (or 24 codes).
 * START events are software events on asynchronous EVSYS channel 1 (user
 * DAC_START); the EMPTY event goes to resynchronized channel 2 (EVD).
 * DMA: channel 0, trigger 39 (the ATDF's DMAC_ID_EMPTY). SYNCBUSY after a
 * DATABUF write stays set until a START: waits on it are short and
 * bounded.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"
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
#define SB_DATA_BUF     (DAC_SYNCBUSY_DATA_Msk | DAC_SYNCBUSY_DATABUF_Msk)
#define EVD2            (1U << 18)
#define TRIG_DAC_EMPTY  39U
#define SHORT_WAIT      20000U

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

static void put_exact(const char *name, uint32_t value, uint32_t expect)
{
    put_range(name, value, expect, expect, 2);
}

static void sync_short(uint32_t mask)
{
    for (uint32_t n = 0; (DA->DAC_SYNCBUSY & mask) && n < SHORT_WAIT; n++) {
    }
}

/* SWRST, CTRLB and EVCTRL, then enabled unless @en is 0 */
static void dac_setup(uint8_t ctrlb, uint8_t evctrl, int en)
{
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    sync_short(0xFFFFFFFFU);
    DA->DAC_CTRLB = ctrlb;
    DA->DAC_EVCTRL = evctrl;
    if (en) {
        DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
        sync_short(DAC_SYNCBUSY_ENABLE_Msk);
        for (uint32_t n = 0; !(DA->DAC_STATUS & DAC_STATUS_READY_Msk) &&
                             n < WAIT_LOOPS; n++) {
        }
    }
}

/* " NAME=<adc>", checked against DAC code @code (in sixteenths: @code16) */
static void put_vout16(const char *name, uint32_t code16)
{
    uint32_t expect = code16 * 4095U / (1023U * 16U);
    uint32_t tol = expect / 20U > 24U ? expect / 20U : 24U;
    uint32_t v;

    delay_us(200);                          /* settle */
    v = adc_read(VOUT_AIN, ADC_REFCTRL_REFSEL_INTVCC2);
    put_dec_range(name, v, expect > tol ? expect - tol : 0, expect + tol);
}

static void put_vout(const char *name, uint32_t code)
{
    put_vout16(name, code << 4);
}

static void start(void)
{
    EV->EVSYS_SWEVT = 1U << 1;
    delay_us(20);
}

/* INTFLAG, then cleared */
static uint32_t flags(void)
{
    uint32_t f = DA->DAC_INTFLAG;

    DA->DAC_INTFLAG = EMPTY | UNDERRUN;
    return f;
}

static void check_enable(void)
{
    dac_setup(OUT_VDD, 0, 0);
    console_puts("enable");
    put_exact("swrst", DA->DAC_INTFLAG, 0);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    sync_short(DAC_SYNCBUSY_ENABLE_Msk);
    put_exact("enabled", flags(), EMPTY);
    DA->DAC_CTRLA = 0;
    sync_short(DAC_SYNCBUSY_ENABLE_Msk);
    put_exact("disabled", DA->DAC_INTFLAG, 0);
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    sync_short(DAC_SYNCBUSY_ENABLE_Msk);
    put_exact("again", DA->DAC_INTFLAG, EMPTY);
    DA->DAC_DATA = 200;
    sync_short(DAC_SYNCBUSY_DATA_Msk);
    put_exact("data_clears", DA->DAC_INTFLAG, 0);
    put_vout("vout_200", 200);
    end_line();
}

static void check_databuf(void)
{
    dac_setup(OUT_VDD, STARTEI, 1);
    flags();
    console_puts("databuf");
    DA->DAC_DATABUF = 700;
    delay_us(50);
    put_exact("syncbusy", DA->DAC_SYNCBUSY, SB_DATA_BUF);
    put_exact("flags", DA->DAC_INTFLAG, 0);
    DA->DAC_DATA = 100;                     /* dropped while busy */
    DA->DAC_DATABUF = 900;                  /* dropped too */
    put_vout("vout_held", 0);
    start();
    put_exact("start_syncbusy", DA->DAC_SYNCBUSY, 0);
    put_exact("start_flags", flags(), EMPTY);
    put_vout("vout_700", 700);
    start();
    put_exact("underrun", flags(), EMPTY | UNDERRUN);
    put_vout("vout_kept", 700);
    end_line();
}

static void check_no_startei(void)
{
    dac_setup(OUT_VDD, 0, 1);
    DA->DAC_DATA = 200;
    sync_short(DAC_SYNCBUSY_DATA_Msk);
    flags();
    console_puts("no_startei");
    DA->DAC_DATABUF = 600;
    delay_us(50);
    put_exact("syncbusy", DA->DAC_SYNCBUSY, SB_DATA_BUF);
    put_vout("vout", 200);
    start();
    put_exact("start_flags", flags(), 0);
    put_vout("vout_start", 200);
    end_line();
}

static void check_before_enable(void)
{
    dac_setup(OUT_VDD, STARTEI, 0);
    DA->DAC_DATABUF = 600;
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;
    delay_us(50);
    console_puts("before_enable");
    put_exact("syncbusy", DA->DAC_SYNCBUSY, SB_DATA_BUF);
    put_exact("flags", flags(), EMPTY);
    start();
    put_exact("start_flags", flags(), EMPTY);
    put_vout("vout_600", 600);
    end_line();
}

static void check_invei_protect(void)
{
    dac_setup(OUT_VDD, STARTEI | INVEI, 1);
    DA->DAC_DATABUF = 300;
    flags();
    console_puts("invei");
    start();                                /* a pulse: it has both edges */
    put_exact("start_flags", flags(), EMPTY);
    put_vout("vout_300", 300);
    DA->DAC_EVCTRL = EMPTYEO;
    put_exact("evctrl_protected", DA->DAC_EVCTRL, STARTEI | INVEI);
    DA->DAC_CTRLB = DAC_CTRLB_EOEN_Msk;
    put_exact("ctrlb_protected", DA->DAC_CTRLB, OUT_VDD);
    end_line();
}

/* 1 when channel 2 saw an EMPTY event since the last call */
static uint32_t evd(void)
{
    uint32_t f;

    delay_us(20);
    f = !!(EV->EVSYS_INTFLAG & EVD2);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    return f;
}

static void check_empty_event(void)
{
    EV->EVSYS_CHANNEL[2] = EVSYS_CHANNEL_EVGEN(EVENT_ID_GEN_DAC_EMPTY) |
                           EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
    delay_us(20);
    EV->EVSYS_INTFLAG = 0xFFFFFFFFU;
    dac_setup(OUT_VDD, STARTEI | EMPTYEO, 1);
    console_puts("empty_event");
    put_exact("enable", evd(), 1);
    DA->DAC_DATABUF = 500;
    put_exact("databuf", evd(), 0);
    start();
    put_exact("start", evd(), 1);
    start();
    put_exact("underrun", evd(), 1);
    end_line();
    EV->EVSYS_CHANNEL[2] = 0;
}

static void check_dma(void)
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
    DA->DAC_CTRLA = DAC_CTRLA_ENABLE_Msk;   /* EMPTY: the first beat */
    delay_us(50);
    console_puts("dma");
    put_exact("enable_syncbusy", DA->DAC_SYNCBUSY, SB_DATA_BUF);
    for (unsigned int k = 0; k < 4; k++) {
        static const char *const names[] = { "v1", "v2", "v3", "v4" };

        start();
        put_vout(names[k], table[k]);
    }
    put_exact("last_flags", flags(), EMPTY);
    start();
    put_exact("underrun", flags(), EMPTY | UNDERRUN);
    DM->DMAC_CHID = 0;
    put_exact("tcmpl", DM->DMAC_CHINTFLAG & DMAC_CHINTFLAG_TCMPL_Msk,
              DMAC_CHINTFLAG_TCMPL_Msk);
    end_line();
    DM->DMAC_CHCTRLA = 0;
    DM->DMAC_CTRL = 0;
}

static void check_dither(void)
{
    uint32_t e = 0, u = 0;

    dac_setup(OUT_VDD | DAC_CTRLB_DITHER_Msk, STARTEI, 1);
    DA->DAC_DATABUF = (300U << 4) | 8U;
    flags();
    console_puts("dither");
    for (unsigned int k = 0; k < 32; k++) {
        uint32_t f;

        start();
        f = flags();
        e |= (uint32_t)!!(f & EMPTY) << k;
        u |= (uint32_t)!!(f & UNDERRUN) << k;
        if (k == 0) {
            put_vout16("vout_a", (300U << 4) | 8U);
            DA->DAC_DATABUF = (600U << 4) | 4U;
        } else if (k == 1) {
            put_vout16("vout_b", (600U << 4) | 4U);
        }
    }
    put_range("empty", e, 0x80008000U, 0x80008000U, 8);
    put_range("underrun", u, 0x00010000U, 0x00010000U, 8);
    end_line();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP dacbuf: " BOARD_BANNER "\r\n");
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

    check_enable();
    check_databuf();
    check_no_startei();
    check_before_enable();
    check_invei_protect();
    check_empty_event();
    check_dma();
    check_dither();

    EV->EVSYS_USER[EVENT_ID_USER_DAC_START] = 0U;
    EV->EVSYS_CHANNEL[1] = 0;
    DA->DAC_CTRLA = DAC_CTRLA_SWRST_Msk;
    sync_short(0xFFFFFFFFU);
    test_exit();
}
