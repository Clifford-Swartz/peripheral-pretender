/*
 * SDADC check (PIC32CM MC00, SDADC U2260), needing no wiring: AIN1
 * (INP1 = PB09 = Arduino A1, INN1 = PB08 = Arduino A0) is left as it is,
 * and the checks look at what the hardware does to whatever it reads.
 * Prints the reset values of the correction registers, then RESULT
 * (24 bits signed, against INTVCC, OSR 1024) with each correction, as
 * silicon does it: RESULT = (code + OFFSETCORR) * GAINCORR >> SHIFTCORR,
 * saturated, expected from the uncorrected "base" reading:
 *
 *   offset+, offset-  OFFSETCORR +-0x100000: added
 *   gain2             GAINCORR 2 (OFFSETCORR 0x100000)
 *   gain2shift1       the same, SHIFTCORR 1
 *   sat               GAINCORR 0x3FFF: 0x7FFFFF
 *   refctrl_en        REFCTRL written while enabled: ignored
 *   ctrlb_en          CTRLB written while enabled: ignored
 *   resrdy            INTFLAG.RESRDY set, cleared by reading RESULT
 *   freerun           a RESULT read starts the next free-running result
 *
 * Ends with "PP:EXIT n", n the failed checks.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "adc0.h"

#define SD_OSR1024      4U
#define SD_TOL          0x1000          /* open inputs wander a little */

static int fails;

static void sd_sync(void)
{
    for (uint32_t n = 0; SDADC_REGS->SDADC_SYNCBUSY != 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "sdadc syncbusy";
            return;
        }
    }
}

static int sd_wait(void)
{
    for (uint32_t n = 0;
         (SDADC_REGS->SDADC_INTFLAG & SDADC_INTFLAG_RESRDY_Msk) == 0U; n++) {
        if (n == WAIT_LOOPS) {
            stuck = "sdadc resrdy";
            return 0;
        }
    }
    return 1;
}

/* One conversion: RESULT, or 0xFFFFFFFF on a timeout */
static uint32_t sd_convert(void)
{
    SDADC_REGS->SDADC_INTFLAG = SDADC_INTFLAG_RESRDY_Msk |
                                SDADC_INTFLAG_OVERRUN_Msk;
    SDADC_REGS->SDADC_SWTRIG = SDADC_SWTRIG_START_Msk;
    return sd_wait() ? SDADC_REGS->SDADC_RESULT : 0xFFFFFFFFUL;
}

static void sd_config(uint8_t refsel, unsigned int osr)
{
    SDADC_REGS->SDADC_CTRLA = 0U;
    sd_sync();
    SDADC_REGS->SDADC_REFCTRL = refsel;
    SDADC_REGS->SDADC_CTRLB = (uint16_t)(SDADC_CTRLB_PRESCALER_DIV32 |
                                         SDADC_CTRLB_OSR(osr) |
                                         SDADC_CTRLB_SKPCNT(2));
    SDADC_REGS->SDADC_INPUTCTRL = 1U;               /* AIN1 */
    sd_sync();
    SDADC_REGS->SDADC_CTRLA = SDADC_CTRLA_ENABLE_Msk;
    sd_sync();
}

static void sd_corr(uint32_t offset, uint16_t gain, uint8_t shift)
{
    SDADC_REGS->SDADC_OFFSETCORR = offset & 0xFFFFFFU;
    sd_sync();
    SDADC_REGS->SDADC_GAINCORR = gain;
    sd_sync();
    SDADC_REGS->SDADC_SHIFTCORR = shift;
    sd_sync();
}

static void verdict(int ok)
{
    if (stuck) {
        console_puts(" stuck at ");
        console_puts(stuck);
        stuck = 0;
        ok = 0;
    }
    console_puts(ok ? " ok\r\n" : " FAIL\r\n");
    fails += !ok;
}

static int32_t sext24(uint32_t v)
{
    return (int32_t)(v << 8) >> 8;
}

/* Settle once, then the conversion that counts, as a signed value */
static int32_t sd_measure(void)
{
    (void)sd_convert();
    return sext24(sd_convert());
}

/* "name result=.. expect=.. ok|FAIL", within SD_TOL */
static void check_result(const char *name, int32_t expect)
{
    int32_t res = sd_measure();

    if (expect > 0x7FFFFF) {
        expect = 0x7FFFFF;
    }
    console_puts(name);
    put_field("result", (uint32_t)res & 0xFFFFFFU, 8);
    put_field("expect", (uint32_t)expect & 0xFFFFFFU, 8);
    verdict(res >= expect - SD_TOL && res <= expect + SD_TOL);
}

/* "name wrote=.. read=.. ok|FAIL": the write was ignored */
static void check_kept(const char *name, uint32_t wrote, uint32_t read,
                       uint32_t was)
{
    console_puts(name);
    put_field("wrote", wrote, 4);
    put_field("read", read, 4);
    verdict(read == was);
}

int main(void)
{
    uint32_t vdd_mv, before, after;
    uint16_t ctrlb;
    int32_t base;

    sys_initialize();
    systick_start();
    console_puts("PP sdadc: " BOARD_BANNER "\r\n");

    adc0_init(1U, 7U);                              /* PB07: unused pin */
    vdd_mv = adc_vddana_mv();
    console_puts("vddana");
    put_field("mv", vdd_mv, 4);
    console_puts("\r\n");

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SDADC_Msk;
    GCLK_REGS->GCLK_PCHCTRL[SDADC_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[SDADC_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    /* PB08 (INN1), PB09 (INP1) to the SDADC: function B */
    PORT_REGS->GROUP[1].PORT_PINCFG[8] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PINCFG[9] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PMUX[4] = (uint8_t)(PORT_PMUX_PMUXE_B |
                                                 PORT_PMUX_PMUXO_B);

    console_puts("reset");
    put_field("ctrlb", SDADC_REGS->SDADC_CTRLB, 4);
    put_field("offsetcorr", SDADC_REGS->SDADC_OFFSETCORR, 8);
    put_field("gaincorr", SDADC_REGS->SDADC_GAINCORR, 4);
    put_field("shiftcorr", SDADC_REGS->SDADC_SHIFTCORR, 2);
    put_field("anactrl", SDADC_REGS->SDADC_ANACTRL, 2);
    verdict(SDADC_REGS->SDADC_CTRLB == 0x2000U &&
            SDADC_REGS->SDADC_OFFSETCORR == 0U &&
            SDADC_REGS->SDADC_GAINCORR == 1U &&
            SDADC_REGS->SDADC_SHIFTCORR == 0U);

    sd_config(SDADC_REFCTRL_REFSEL_INTVCC, SD_OSR1024);
    sd_corr(0U, 1U, 0U);
    base = sd_measure();
    console_puts("base");
    put_field("result", (uint32_t)base & 0xFFFFFFU, 8);
    console_puts("\r\n");
    sd_corr(0x100000U, 1U, 0U);
    check_result("offset+", base + 0x100000);
    sd_corr((uint32_t)-0x100000, 1U, 0U);
    check_result("offset-", base - 0x100000);
    sd_corr(0x100000U, 2U, 0U);
    check_result("gain2", (base + 0x100000) * 2);
    sd_corr(0x100000U, 2U, 1U);
    check_result("gain2shift1", base + 0x100000);
    sd_corr(0x100000U, 0x3FFFU, 0U);
    check_result("sat", 0x7FFFFF);
    sd_corr(0U, 1U, 0U);

    /* Writes while enabled are ignored */
    SDADC_REGS->SDADC_REFCTRL = SDADC_REFCTRL_REFSEL_INTREF;
    sd_sync();
    check_kept("refctrl_en", SDADC_REFCTRL_REFSEL_INTREF,
               SDADC_REGS->SDADC_REFCTRL, SDADC_REFCTRL_REFSEL_INTVCC);
    ctrlb = SDADC_REGS->SDADC_CTRLB;
    SDADC_REGS->SDADC_CTRLB = (uint16_t)(ctrlb ^ SDADC_CTRLB_SKPCNT(1));
    sd_sync();
    check_kept("ctrlb_en", (uint16_t)(ctrlb ^ SDADC_CTRLB_SKPCNT(1)),
               SDADC_REGS->SDADC_CTRLB, ctrlb);

    sd_config(SDADC_REFCTRL_REFSEL_INTVCC, SD_OSR1024);
    SDADC_REGS->SDADC_INTFLAG = SDADC_INTFLAG_RESRDY_Msk;
    SDADC_REGS->SDADC_SWTRIG = SDADC_SWTRIG_START_Msk;
    (void)sd_wait();
    before = SDADC_REGS->SDADC_INTFLAG & SDADC_INTFLAG_RESRDY_Msk;
    (void)SDADC_REGS->SDADC_RESULT;
    after = SDADC_REGS->SDADC_INTFLAG & SDADC_INTFLAG_RESRDY_Msk;
    console_puts("resrdy");
    put_field("before", before, 2);
    put_field("after_read", after, 2);
    verdict(before && !after);

    SDADC_REGS->SDADC_CTRLC = SDADC_CTRLC_FREERUN_Msk;
    sd_sync();
    (void)sd_wait();
    (void)SDADC_REGS->SDADC_RESULT;
    before = (uint32_t)sd_wait();
    console_puts("freerun");
    put_field("resrdy_again", before, 2);
    put_field("result", SDADC_REGS->SDADC_RESULT, 8);
    verdict(before != 0U);
    SDADC_REGS->SDADC_CTRLC = 0U;
    sd_sync();

    console_puts(fails ? "PP:EXIT 1\r\n" : "PP:EXIT 0\r\n");
    for (;;) {
    }
}
