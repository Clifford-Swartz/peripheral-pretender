/*
 * DMAC probe, second round (PIC32CM MC00, DMAC U2223): when the first
 * descriptor is fetched, round robin with both channels fetched, and
 * software requests on a suspended channel. See dmacprobe.c.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define DM              DMAC_REGS
#define DMAENABLE       0x0002U
#define CRCENABLE       0x0004U
#define LVLEN_ALL       0x0F00U
#define CH_ENABLE       0x02U
#define EVACT(x)        ((uint32_t)(x) << 0)
#define EVIE            (1U << 3)
#define TRIGACT(x)      ((uint32_t)(x) << 22)
#define CMD(x)          ((uint32_t)(x) << 24)
#define VALID           0x0001U
#define BYTE            (VALID | 0x0400U | 0x0800U | 0x0008U)

typedef struct {
    uint16_t btctrl, btcnt;
    uint32_t src, dst, next;
} desc_t;

static desc_t desc[12] __attribute__((aligned(16)));
static volatile desc_t wb[12] __attribute__((aligned(16)));
static uint8_t src[64], dst[64];
static volatile uint32_t word, log32[32];

static void restart(void)
{
    volatile uint32_t *p = (volatile uint32_t *)wb;

    DM->DMAC_CTRL = 0U;
    for (uint32_t n = 0; (DM->DMAC_CTRL & (DMAENABLE | CRCENABLE)) &&
         n < 100000U; n++) {
    }
    DM->DMAC_CTRL = 1U;
    for (unsigned int i = 0; i < sizeof(wb) / 4U; i++) {
        p[i] = 0xCCCCCCCCU;
    }
    DM->DMAC_BASEADDR = (uint32_t)desc;
    DM->DMAC_WRBADDR = (uint32_t)wb;
    DM->DMAC_CTRL = DMAENABLE | LVLEN_ALL;
}

static void cfg(unsigned int ch, uint32_t b, uint16_t n, uint16_t bt,
                uint32_t s, uint32_t d)
{
    DM->DMAC_CHID = (uint8_t)ch;
    DM->DMAC_CHCTRLA = 0U;
    DM->DMAC_CHCTRLA = 1U;
    DM->DMAC_CHCTRLB = b;
    desc[ch].btctrl = bt;
    desc[ch].btcnt = n;
    desc[ch].src = s;
    desc[ch].dst = d;
    desc[ch].next = 0U;
}

static void settle(void)
{
    for (volatile uint32_t i = 0; i < 2000U; i++) {
    }
}

static void moved(void)
{
    unsigned int n = 0;

    for (unsigned int i = 0; i < 8U; i++) {
        n += dst[i] == (uint8_t)i;
    }
    put_field("moved", n, 1);
    DM->DMAC_CHID = 0;
    put_field("f", DM->DMAC_CHINTFLAG, 1);
    put_field("st", DM->DMAC_CHSTATUS, 1);
    put_field("sw", DM->DMAC_SWTRIGCTRL, 3);
}

int main(void)
{
    volatile uint16_t w[6];

    sys_initialize();
    systick_start();
    console_puts("PP dmacprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk;
    for (unsigned int i = 0; i < sizeof(src); i++) {
        src[i] = (uint8_t)i;
    }

    /* A: write-back copy at 0, 1, 2, 4, 8, 16 loads after ENABLE */
    restart();
    cfg(0, TRIGACT(0), 8, BYTE, (uint32_t)src + 8, (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    w[0] = wb[0].btctrl;
    w[1] = wb[0].btctrl;
    w[2] = wb[0].btctrl;
    for (volatile int i = 0; i < 2; i++) {
    }
    w[3] = wb[0].btctrl;
    for (volatile int i = 0; i < 8; i++) {
    }
    w[4] = wb[0].btctrl;
    for (volatile int i = 0; i < 32; i++) {
    }
    w[5] = wb[0].btctrl;
    console_puts("A fetch");
    for (unsigned int i = 0; i < 6U; i++) {
        put_field("w", w[i], 4);
    }
    /* reading a DMAC register first */
    restart();
    cfg(0, TRIGACT(0), 8, BYTE, (uint32_t)src + 8, (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    (void)DM->DMAC_CHSTATUS;
    put_field("after_chstatus", wb[0].btctrl, 4);
    console_puts("\r\n");

    /* B: round robin, both fetched before the trigger, three times */
    console_puts("B rr");
    for (unsigned int r = 0; r < 3U; r++) {
        restart();
        DM->DMAC_PRICTRL = 0x80U;
        word = 0;
        for (unsigned int i = 0; i < 32U; i++) {
            log32[i] = 0xEE;
            src[32 + i] = (uint8_t)(i + 1U);
        }
        cfg(0, TRIGACT(3), 32, VALID | 0x0200U | 0x0800U, (uint32_t)&word,
            (uint32_t)log32 + 128);
        cfg(1, TRIGACT(3), 32, VALID | 0x0400U, (uint32_t)src + 64,
            (uint32_t)&word);
        DM->DMAC_CHID = 0;
        DM->DMAC_CHCTRLA = CH_ENABLE;
        DM->DMAC_CHID = 1;
        DM->DMAC_CHCTRLA = CH_ENABLE;
        settle();
        DM->DMAC_SWTRIGCTRL = 3U;
        settle();
        put_field("l0", log32[0], 2);
        put_field("l1", log32[1], 2);
        put_field("l31", log32[31], 2);
        put_field("pri", DM->DMAC_PRICTRL, 8);
        for (unsigned int i = 0; i < 32U; i++) {
            src[32 + i] = (uint8_t)(32U + i);
        }
    }
    console_puts("\r\n");

    /* C: software trigger on a suspended channel, RESUME command */
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
    restart();
    cfg(0, TRIGACT(2), 8, BYTE, (uint32_t)src + 8, (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    settle();
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(1);
    settle();
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts("C suspended");
    moved();
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(2);
    settle();
    console_puts(" | resumed");
    moved();
    console_puts("\r\n");

    /* D: the same with a RESUME event (EVSYS SWEVT on channel 1) */
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0) |
                                   EVSYS_CHANNEL_PATH_ASYNCHRONOUS;
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 2U;
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
    restart();
    cfg(0, TRIGACT(2) | EVIE | EVACT(5), 8, BYTE, (uint32_t)src + 8,
        (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    settle();
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(1);
    settle();
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts("D suspended");
    moved();
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    settle();
    console_puts(" | event");
    moved();
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts(" | swtrig");
    moved();
    console_puts("\r\n");
    /* E: as D, without settle between ENABLE and SUSPEND */
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
    restart();
    cfg(0, TRIGACT(2) | EVIE | EVACT(5), 8, BYTE, (uint32_t)src + 8,
        (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(1);
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts("E early_suspend");
    moved();
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    settle();
    console_puts(" | event");
    moved();
    console_puts("\r\n");
    /* F: no request pending when the RESUME event comes */
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
    restart();
    cfg(0, TRIGACT(2) | EVIE | EVACT(5), 8, BYTE, (uint32_t)src + 8,
        (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    settle();
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(1);
    settle();
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    settle();
    console_puts("F idle_event");
    moved();
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts(" | swtrig");
    moved();
    console_puts("\r\n");
    /* G: as D with TRIGSRC = TC0 OVF (TC0 stopped) */
    for (unsigned int i = 0; i < sizeof(dst); i++) {
        dst[i] = 0xA5;
    }
    restart();
    cfg(0, (0x15U << 8) | TRIGACT(2) | EVIE | EVACT(5), 8, BYTE,
        (uint32_t)src + 8, (uint32_t)dst + 8);
    DM->DMAC_CHID = 0;
    DM->DMAC_CHCTRLA = CH_ENABLE;
    settle();
    DM->DMAC_CHCTRLB = (DM->DMAC_CHCTRLB & ~CMD(3)) | CMD(1);
    settle();
    DM->DMAC_SWTRIGCTRL |= 1U;
    settle();
    console_puts("G trigsrc_tc");
    moved();
    EVSYS_REGS->EVSYS_SWEVT = 1U << 1;
    settle();
    console_puts(" | event");
    moved();
    console_puts("\r\n");
    EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_DMAC_CH_0] = 0U;
    restart();
    DM->DMAC_CTRL = 0U;
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
