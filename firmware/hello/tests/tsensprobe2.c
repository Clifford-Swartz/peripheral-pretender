/*
 * TSENS probe, second round (PIC32CM MC00, TSENS U2261): what disabling
 * does to a conversion and to FREERUN, and when STARTs are lost. Each line
 * lists the RESRDY times (us after the line's first action) seen in a
 * window, with the STARTs issued at fixed times. See tsensprobe.c.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define TS              TSENS_REGS
#define TEMP_LOG        ((volatile const uint32_t *)0x00806030U)
#define ENABLE          0x02U
#define FREERUN         0x10U
#define RESRDY          0x01U

static uint32_t cal, gain, offset;
static uint32_t wraps, last_val;
static volatile uint32_t isr_t[8], isr_f[8];
static volatile unsigned int isr_n;
static volatile uint32_t j_isr[8];
static volatile unsigned int j_n;
static volatile int j_mode;

static uint32_t tc0_now(void);

void TSENS_Handler(void)
{
    uint8_t f = TS->TSENS_INTFLAG & TS->TSENS_INTENSET;

    TS->TSENS_INTFLAG = f;
    if (j_mode) {
        if (j_n < 8U) {
            j_isr[j_n++] = tc0_now();
        }
        return;
    }
    if (isr_n < 8U) {
        isr_t[isr_n] = wraps * 1000U +
                       (SysTick->LOAD + 1U - SysTick->VAL) / 48U;
        isr_f[isr_n++] = f;
    }
}

static void sync(void)
{
    for (uint32_t n = 0; TS->TSENS_SYNCBUSY != 0U && n < 1000000U; n++) {
    }
}

static void ctrla(uint8_t v)
{
    TS->TSENS_CTRLA = v;
    sync();
}

static void clock_zero(void)
{
    SysTick->VAL = 0U;
    (void)SysTick->CTRL;
    wraps = 0;
}

static uint32_t now_us(void)
{
    uint32_t v = SysTick->VAL;

    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        wraps++;
        v = SysTick->VAL;
    }
    last_val = v;
    return wraps * 1000U + (SysTick->LOAD + 1U - v) / 48U;
}

/*
 * For @window_us: issue START at each time in @starts (us, ascending,
 * ~0 ends), print each RESRDY time and the flags; read VALUE if @read
 */
static void watch(uint32_t window_us, const uint32_t *starts, int read)
{
    unsigned int si = 0, n = 0;
    uint32_t t, rt[8], rf[8];

    while ((t = now_us()) < window_us) {
        if (starts && starts[si] != ~0U && t >= starts[si]) {
            TS->TSENS_CTRLB = 1U;
            si++;
        }
        if (TS->TSENS_INTFLAG & RESRDY) {
            if (n < 8U) {
                rt[n] = t;
                rf[n++] = TS->TSENS_INTFLAG;
            }
            if (read) {
                (void)TS->TSENS_VALUE;
            } else {
                TS->TSENS_INTFLAG = RESRDY;
            }
        }
    }
    for (unsigned int i = 0; i < n; i++) {     /* printing loses wraps */
        put_field("r", rt[i], 5);
        put_field("f", rf[i], 1);
    }
    clock_zero();
}

static void setup(uint8_t ctrlc)
{
    ctrla(0U);
    TS->TSENS_GAIN = gain;
    TS->TSENS_OFFSET = offset;
    TS->TSENS_CAL = cal;
    TS->TSENS_CTRLC = ctrlc;
    (void)TS->TSENS_VALUE;
    TS->TSENS_INTFLAG = 0x0FU;
    ctrla(ENABLE);
}

static void quiet(void)
{
    static const uint32_t none[] = { ~0U };

    setup(0U);
    clock_zero();
    watch(10000, none, 1);                  /* let anything finish */
    (void)TS->TSENS_VALUE;
    TS->TSENS_INTFLAG = 0x0FU;
}

/* TC0 as a free-running 16-bit clock at 48 MHz / 64 (0.75 ticks per us) */
static void tc0_start(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                           GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    TC0_REGS->COUNT16.TC_CTRLA = TC_CTRLA_PRESCALER_DIV64 |
                                 TC_CTRLA_ENABLE_Msk;
    while (TC0_REGS->COUNT16.TC_SYNCBUSY != 0U) {
    }
}

static uint32_t tc0_now(void)
{
    TC0_REGS->COUNT16.TC_CTRLBSET = TC_CTRLBSET_CMD_READSYNC;
    while (TC0_REGS->COUNT16.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) {
    }
    return TC0_REGS->COUNT16.TC_COUNT;
}

/* tsens.c's interrupt check: STARTs with delay_ms(5); TC0 times */
static void irq_delay_ms(const char *name)
{
    uint32_t st[3];

    j_n = 0;
    j_mode = 1;
    TS->TSENS_INTENSET = RESRDY;
    NVIC_ClearPendingIRQ(TSENS_IRQn);
    NVIC_EnableIRQ(TSENS_IRQn);
    for (unsigned int i = 0; i < 3U; i++) {
        st[i] = tc0_now();
        TS->TSENS_CTRLB = 1U;
        delay_ms(5);
    }
    NVIC_DisableIRQ(TSENS_IRQn);
    TS->TSENS_INTENCLR = 0x0FU;
    j_mode = 0;
    console_puts(name);
    for (unsigned int i = 0; i < 3U; i++) {
        put_field("s", ((st[i] - st[0]) & 0xFFFFU) * 4U / 3U, 5);
    }
    for (unsigned int i = 0; i < j_n; i++) {
        put_field("i", ((j_isr[i] - st[0]) & 0xFFFFU) * 4U / 3U, 5);
    }
    put_field("end", ((tc0_now() - st[0]) & 0xFFFFU) * 4U / 3U, 5);
    put_field("intflag", TS->TSENS_INTFLAG, 2);
    console_puts("\r\n");
}

/* Three STARTs 5 ms apart with the RESRDY interrupt; ISR times */
static void irq_three(const char *name)
{
    static const uint32_t at[] = { 0, 5000, 10000 };
    unsigned int si = 0;

    isr_n = 0;
    TS->TSENS_INTENSET = RESRDY;
    NVIC_ClearPendingIRQ(TSENS_IRQn);
    NVIC_EnableIRQ(TSENS_IRQn);
    clock_zero();
    while (now_us() < 15000U) {
        if (si < 3U && now_us() >= at[si]) {
            TS->TSENS_CTRLB = 1U;
            si++;
        }
    }
    NVIC_DisableIRQ(TSENS_IRQn);
    TS->TSENS_INTENCLR = 0x0FU;
    console_puts(name);
    for (unsigned int i = 0; i < isr_n; i++) {
        put_field("i", isr_t[i], 5);
        put_field("f", isr_f[i], 1);
    }
    console_puts("\r\n");
}

int main(void)
{
    static const uint32_t at0[] = { 0, ~0U };
    static const uint32_t three[] = { 0, 5000, 10000, ~0U };
    static const uint32_t burst[] = { 0, 100, 3600, ~0U };
    uint32_t lo = TEMP_LOG[0], hi = TEMP_LOG[1];

    sys_initialize();
    systick_start();
    console_puts("PP tsensprobe2: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_TSENS_Msk;
    GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                             GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TSENS_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }
    cal = (lo & 0x3FU) << 8 | ((lo >> 6) & 0x3FU);
    gain = ((hi & 0xFU) << 20) | (lo >> 12);
    offset = hi >> 4;

    /* A: three STARTs 5 ms apart from idle */
    quiet();
    console_puts("A three");
    clock_zero();
    watch(15000, three, 1);
    console_puts("\r\n");

    /* B: right after enable, STARTs at 0, 0.1 and 3.6 ms */
    quiet();
    setup(0U);
    console_puts("B burst");
    clock_zero();
    watch(9000, burst, 1);
    console_puts("\r\n");

    /* C: START while disabled (idle), then enable: anything? */
    quiet();
    ctrla(0U);
    TS->TSENS_CTRLB = 1U;
    ctrla(ENABLE);
    console_puts("C start_off");
    clock_zero();
    watch(8000, 0, 1);
    console_puts("\r\n");

    /* D: disable 1 ms into a conversion for 10 ms, enable */
    quiet();
    TS->TSENS_CTRLB = 1U;
    clock_zero();
    while (now_us() < 1000U) {
    }
    ctrla(0U);
    console_puts("D disable_mid");
    watch(11000, 0, 1);
    console_puts(" | enabled:");
    ctrla(ENABLE);
    clock_zero();
    watch(20000, 0, 1);
    console_puts("\r\n");

    /* E: FREERUN running, disable for 10 ms, then enable with CTRLC = 0 */
    setup(FREERUN);
    clock_zero();
    while (now_us() < 5000U) {
        if (TS->TSENS_INTFLAG & RESRDY) {
            (void)TS->TSENS_VALUE;
        }
    }
    ctrla(0U);
    TS->TSENS_CTRLC = 0U;
    console_puts("E freerun_off");
    watch(15000, 0, 1);
    console_puts(" | enabled:");
    ctrla(ENABLE);
    clock_zero();
    watch(30000, 0, 1);
    console_puts("\r\n");

    /* F: the same, then three STARTs 5 ms apart right after enable */
    setup(FREERUN);
    clock_zero();
    while (now_us() < 5000U) {
        if (TS->TSENS_INTFLAG & RESRDY) {
            (void)TS->TSENS_VALUE;
        }
    }
    setup(0U);
    console_puts("F after_freerun");
    clock_zero();
    watch(15000, three, 1);
    console_puts("\r\n");

    /* G: unread results: flags as results arrive with VALUE not read */
    quiet();
    console_puts("G unread");
    clock_zero();
    watch(12000, three, 0);
    console_puts("\r\n");

    /* H: START at once after enable from idle */
    quiet();
    ctrla(0U);
    ctrla(ENABLE);
    console_puts("H start_at_enable");
    clock_zero();
    watch(5000, at0, 1);
    console_puts("\r\n");
    /* I: interrupts, from idle and after FREERUN */
    quiet();
    irq_three("I1 idle");
    setup(FREERUN);
    clock_zero();
    while (now_us() < 5000U) {
        if (TS->TSENS_INTFLAG & RESRDY) {
            (void)TS->TSENS_VALUE;
        }
    }
    setup(0U);
    irq_three("I2 after_freerun");

    /* J: tsens.c's interrupt check as it runs there, and from quiet */
    tc0_start();
    setup(0U);
    irq_delay_ms("J1 setup_then");
    quiet();
    irq_delay_ms("J2 quiet_then");
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
