/*
 * NVMCTRL probe (PIC32CM MC00, NVMCTRL 03929): how the flash controller
 * programs and erases, for the model. Prints raw results, one experiment
 * per line; checks nothing. The plan and the answers: docs/research/
 * peripherals/nvmctrl-03929.md. Run: py -m tools.periph probe mc00
 * nvmctrlprobe
 *
 * Safety: nvm_cmd() refuses the security bit (SSB), the chip-erase hard
 * lock (SCEHL, permanent) and the user row commands (EAR, WAP), and main
 * array commands outside TEST_ROW, the last row of flash, far above this
 * image. The data flash (4 KiB) takes the rest. Lock commands are
 * volatile: the user row's lock bits come back at reset.
 *
 * Times are in CPU cycles at 48 MHz from SysTick run over its full 24 bits
 * (349 ms), so a CPU stalled on a flash fetch loses nothing.
 *
 *   E0   boot: PARAM, CTRLB, STATUS, INTFLAG, LOCK, ADDR, user row
 *   E1   command keys: no key, a reserved command, CTRLA readback
 *   E2   page buffer loads: PBLDATA, ADDR, LOAD; reads before the write;
 *        DFER and DFWP times
 *   E3   random-access loads (the data sheet's example)
 *   E4   writing over programmed bits
 *   E5   page buffer clear
 *   E6   automatic page write (MANW = 0), and a partial page
 *   E7   a command while busy
 *   E8   repeat times: DFER, DFWP
 *   E9   main array: ER and WP on TEST_ROW, ADDR from a load
 *   E10  lock region: ER and WP refused, unlock
 *   E11  power reduction mode
 *   E12  8- and 16-bit page buffer writes
 *   E13  ADDR written by hand
 *   E14  what 8- and 16-bit loads put in the page buffer
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

#define N               NVMCTRL_REGS
#define KEY             0xA500U
#define READY           NVMCTRL_INTFLAG_READY_Msk
#define ERROR           NVMCTRL_INTFLAG_ERROR_Msk
#define MANW            NVMCTRL_CTRLB_MANW_Msk
#define STATUS_ERRS     0x1EU           /* NVME LOCKE PROGE LOAD */

#define CMD_ER          0x02U
#define CMD_WP          0x04U
#define CMD_EAR         0x05U
#define CMD_WAP         0x06U
#define CMD_DFER        0x1AU
#define CMD_DFWP        0x1CU
#define CMD_LR          0x40U
#define CMD_UR          0x41U
#define CMD_SPRM        0x42U
#define CMD_CPRM        0x43U
#define CMD_PBC         0x44U
#define CMD_SSB         0x45U
#define CMD_INVALL      0x46U
#define CMD_SCEHL       0x7FU

#define DF              0x00400000U
#define DF_ROW(n)       (DF + 0x100U * (n))
#define TEST_ROW        0x0001FF00U
#define WORD(a)         (*(volatile uint32_t *)(a))

static volatile uint32_t expect_fault, faults;
static int addr_ok;     /* ADDR is the data flash offset / 2 (silicon) */

void hardfault_c(uint32_t *frame);
void hardfault_c(uint32_t *frame)
{
    if (!expect_fault) {
        console_puts("\r\nunexpected HardFault pc=0x");
        console_put_hex(frame[6], 8);
        console_puts("\r\nPP:EXIT 1\r\n");
        for (;;) {
        }
    }
    faults++;
    frame[6] += 2;                      /* skip the 16-bit store */
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile("mrs r0, msp\n"
                   "b hardfault_c\n");
}

static void nl(void)
{
    console_puts("\r\n");
}

static void put_dec(const char *name, uint32_t v)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    console_put_dec(v);
}

/* SysTick over 24 bits, no interrupt: cycles count down */
static void cyc_start(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0x00FFFFFFU;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static uint32_t cyc(void)
{
    return SysTick->VAL;
}

static uint32_t cyc_since(uint32_t t0)
{
    return (t0 - SysTick->VAL) & 0x00FFFFFFU;
}

static uint32_t wait_ready(void)
{
    uint32_t t0 = cyc();

    while (!(N->NVMCTRL_INTFLAG & READY)) {
        if (cyc_since(t0) > 0x00F00000U) {
            return 0xFFFFFFFFU;
        }
    }
    return cyc_since(t0);
}

static void clear_errors(void)
{
    N->NVMCTRL_STATUS = STATUS_ERRS;
    N->NVMCTRL_INTFLAG = ERROR;
}

/* Refuses what could brick the part or touch outside the test areas */
static void nvm_cmd(uint32_t cmd)
{
    uint32_t a = N->NVMCTRL_ADDR * 2U;

    if (cmd == CMD_SSB || cmd == CMD_SCEHL || cmd == CMD_EAR ||
        cmd == CMD_WAP ||
        ((cmd == CMD_ER || cmd == CMD_WP) &&
         (a < TEST_ROW || a >= TEST_ROW + 0x100U))) {
        console_puts("\r\nnvm_cmd: refused 0x");
        console_put_hex(cmd, 2);
        console_puts(" addr=0x");
        console_put_hex(a, 8);
        console_puts("\r\nPP:EXIT 1\r\n");
        for (;;) {
        }
    }
    N->NVMCTRL_CTRLA = KEY | cmd;
}

/*
 * Command at @addr (main array: byte address / 2; data flash: offset / 2),
 * then the cycles until READY, from before the command: a main array
 * operation stalls the next instruction fetch until it is done.
 */
static uint32_t nvm_at(uint32_t cmd, uint32_t addr)
{
    uint32_t t0;

    N->NVMCTRL_ADDR = (addr >= DF ? addr - DF : addr) / 2U;
    t0 = cyc();
    nvm_cmd(cmd);
    (void)wait_ready();
    return cyc_since(t0);
}

/* Command at ADDR as loaded, cycles until READY from before it */
static uint32_t nvm_timed(uint32_t cmd)
{
    uint32_t t0 = cyc();

    nvm_cmd(cmd);
    (void)wait_ready();
    return cyc_since(t0);
}

static void put_words(const char *name, uint32_t addr, unsigned n)
{
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    for (unsigned i = 0; i < n; i++) {
        console_put_hex(WORD(addr + 4U * i), 8);
        console_putc(i + 1 < n ? ',' : ' ');
    }
}

static void e0_boot(void)
{
    console_puts("E0 boot");
    put_field("param", N->NVMCTRL_PARAM, 8);
    put_field("ctrla", N->NVMCTRL_CTRLA, 4);
    put_field("ctrlb", N->NVMCTRL_CTRLB, 8);
    put_field("status", N->NVMCTRL_STATUS, 4);
    put_field("intflag", N->NVMCTRL_INTFLAG, 2);
    put_field("lock", N->NVMCTRL_LOCK, 4);
    put_field("addr", N->NVMCTRL_ADDR, 8);
    put_field("pbl0", N->NVMCTRL_PBLDATA0, 8);
    put_field("pbl1", N->NVMCTRL_PBLDATA1, 8);
    put_field("user0", USER_FUSES_REGS->FUSES_USER_WORD_0, 8);
    put_field("user1", USER_FUSES_REGS->FUSES_USER_WORD_1, 8);
    put_field("df0", WORD(DF), 8);
    nl();
}

static void e1_keys(void)
{
    uint32_t st1, if1, ca1, st2, if2, ca2, ca3;

    clear_errors();
    N->NVMCTRL_CTRLA = CMD_PBC;         /* no key */
    st1 = N->NVMCTRL_STATUS;
    if1 = N->NVMCTRL_INTFLAG;
    ca1 = N->NVMCTRL_CTRLA;
    clear_errors();
    N->NVMCTRL_CTRLA = KEY | 0x03U;     /* reserved */
    (void)wait_ready();
    st2 = N->NVMCTRL_STATUS;
    if2 = N->NVMCTRL_INTFLAG;
    ca2 = N->NVMCTRL_CTRLA;
    clear_errors();
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    ca3 = N->NVMCTRL_CTRLA;
    console_puts("E1 keys");
    put_field("nokey_status", st1, 4);
    put_field("nokey_intflag", if1, 2);
    put_field("nokey_ctrla", ca1, 4);
    put_field("rsvd_status", st2, 4);
    put_field("rsvd_intflag", if2, 2);
    put_field("rsvd_ctrla", ca2, 4);
    put_field("pbc_ctrla", ca3, 4);
    put_field("status", N->NVMCTRL_STATUS, 4);
    nl();
}

static void e2_loads(void)
{
    uint32_t er, wp, a1, l1, p0a, p1a, p0b, p1b, p0c, p1c, rd, st, a3, st2;

    clear_errors();
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    er = nvm_at(CMD_DFER, DF_ROW(0));
    WORD(DF_ROW(0)) = 0x11111111U;
    a1 = N->NVMCTRL_ADDR;
    addr_ok = a1 == 0U;
    l1 = N->NVMCTRL_STATUS;
    p0a = N->NVMCTRL_PBLDATA0;
    p1a = N->NVMCTRL_PBLDATA1;
    WORD(DF_ROW(0) + 4U) = 0x22222222U;
    p0b = N->NVMCTRL_PBLDATA0;
    p1b = N->NVMCTRL_PBLDATA1;
    WORD(DF_ROW(0) + 8U) = 0x33333333U;
    p0c = N->NVMCTRL_PBLDATA0;
    p1c = N->NVMCTRL_PBLDATA1;
    a3 = N->NVMCTRL_ADDR;
    rd = WORD(DF_ROW(0));               /* before the write */
    wp = nvm_timed(CMD_DFWP);
    st = N->NVMCTRL_STATUS;
    st2 = N->NVMCTRL_INTFLAG;
    console_puts("E2 loads");
    put_dec("dfer_cyc", er);
    put_field("addr1", a1, 8);
    put_field("status1", l1, 4);
    put_field("pbl_a", p1a, 8);
    put_field("", p0a, 8);
    put_field("pbl_b", p1b, 8);
    put_field("", p0b, 8);
    put_field("pbl_c", p1c, 8);
    put_field("", p0c, 8);
    put_field("addr3", a3, 8);
    put_field("rd_before", rd, 8);
    put_dec("dfwp_cyc", wp);
    put_field("status", st, 4);
    put_field("intflag", st2, 2);
    put_words("row0", DF_ROW(0), 5);
    nl();
}

static void e3_random(void)
{
    (void)nvm_at(CMD_DFER, DF_ROW(1));
    WORD(DF_ROW(1) + 8U) = 1U;
    WORD(DF_ROW(1) + 4U) = 2U;
    WORD(DF_ROW(1) + 12U) = 3U;
    nvm_cmd(CMD_DFWP);
    (void)wait_ready();
    console_puts("E3 random");
    put_words("row1", DF_ROW(1), 4);
    nl();
}

static void e4_over(void)
{
    /* Row 0 word 0 holds 0x11111111 from E2 */
    WORD(DF_ROW(0)) = 0x0F0F0F0FU;
    WORD(DF_ROW(0) + 12U) = 0xF0F0F0F0U;
    nvm_cmd(CMD_DFWP);
    (void)wait_ready();
    console_puts("E4 over");
    put_field("status", N->NVMCTRL_STATUS, 4);
    put_words("row0", DF_ROW(0), 4);
    nl();
}

static void e5_pbc(void)
{
    uint32_t l1, l2, p0;

    clear_errors();
    WORD(DF_ROW(0) + 16U) = 0x12345678U;
    l1 = N->NVMCTRL_STATUS;
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    l2 = N->NVMCTRL_STATUS;
    p0 = N->NVMCTRL_PBLDATA0;
    nvm_cmd(CMD_DFWP);
    (void)wait_ready();
    console_puts("E5 pbc");
    put_field("status_loaded", l1, 4);
    put_field("status_pbc", l2, 4);
    put_field("pbl0_pbc", p0, 8);
    put_words("row0_16", DF_ROW(0) + 16U, 1);
    nl();
}

static void e6_auto(void)
{
    uint32_t rdy15, st15, rdy16, t0, aw, st16;

    (void)nvm_at(CMD_DFER, DF_ROW(2));
    clear_errors();
    N->NVMCTRL_CTRLB &= ~MANW;
    for (unsigned i = 0; i < 15U; i++) {
        WORD(DF_ROW(2) + 4U * i) = 0xA0000000U + i;
    }
    rdy15 = N->NVMCTRL_INTFLAG;
    st15 = N->NVMCTRL_STATUS;
    t0 = cyc();
    WORD(DF_ROW(2) + 60U) = 0xA000000FU;
    rdy16 = N->NVMCTRL_INTFLAG;
    while (!(N->NVMCTRL_INTFLAG & READY)) {
    }
    aw = cyc_since(t0);
    st16 = N->NVMCTRL_STATUS;
    /* A partial page: 4 words of page 1, then nothing */
    for (unsigned i = 0; i < 4U; i++) {
        WORD(DF_ROW(2) + 64U + 4U * i) = 0xB0000000U + i;
    }
    N->NVMCTRL_CTRLB |= MANW;
    console_puts("E6 auto");
    put_field("intflag15", rdy15, 2);
    put_field("status15", st15, 4);
    put_field("intflag16", rdy16, 2);
    put_dec("auto_cyc", aw);
    put_field("status16", st16, 4);
    put_words("page0", DF_ROW(2), 2);
    put_words("page0_15", DF_ROW(2) + 60U, 1);
    put_field("partial_status", N->NVMCTRL_STATUS, 4);
    put_words("page1", DF_ROW(2) + 64U, 2);
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    nl();
}

static void e7_busy(void)
{
    uint32_t r0, st, rdy;

    clear_errors();
    N->NVMCTRL_ADDR = DF_ROW(3) / 2U;
    nvm_cmd(CMD_DFER);
    r0 = N->NVMCTRL_INTFLAG;
    N->NVMCTRL_CTRLA = KEY | CMD_PBC;   /* while busy */
    st = N->NVMCTRL_STATUS;
    rdy = wait_ready();
    console_puts("E7 busy");
    put_field("intflag_after_cmd", r0, 2);
    put_field("status_busy_cmd", st, 4);
    put_field("intflag", N->NVMCTRL_INTFLAG, 2);
    put_dec("rest_cyc", rdy);
    nl();
}

static void e8_times(void)
{
    uint32_t er[3], wp[3];

    for (unsigned i = 0; i < 3U; i++) {
        er[i] = nvm_at(CMD_DFER, DF_ROW(4 + i));
        WORD(DF_ROW(4 + i)) = 0x5A5A5A5AU;
        wp[i] = nvm_timed(CMD_DFWP);
    }
    console_puts("E8 times");
    put_dec("dfer", er[0]);
    put_dec("dfer", er[1]);
    put_dec("dfer", er[2]);
    put_dec("dfwp", wp[0]);
    put_dec("dfwp", wp[1]);
    put_dec("dfwp", wp[2]);
    nl();
}

static void e9_main(void)
{
    uint32_t er, wp, a, er2;

    clear_errors();
    er = nvm_at(CMD_ER, TEST_ROW);
    WORD(TEST_ROW + 4U) = 0xC0DE0001U;
    a = N->NVMCTRL_ADDR;
    WORD(TEST_ROW) = 0xC0DE0000U;
    wp = nvm_timed(CMD_WP);
    console_puts("E9 main");
    put_dec("er_cyc", er);
    put_field("addr_load", a, 8);
    put_dec("wp_cyc", wp);
    put_field("status", N->NVMCTRL_STATUS, 4);
    put_words("test", TEST_ROW, 3);
    er2 = nvm_at(CMD_ER, TEST_ROW);
    put_dec("er2_cyc", er2);
    put_words("erased", TEST_ROW, 2);
    nl();
}

static void e10_lock(void)
{
    uint32_t l0, l1, lr, st_er, if_er, st_wp, l2, ur;

    clear_errors();
    l0 = N->NVMCTRL_LOCK;
    WORD(TEST_ROW) = 0x0000AAAAU;
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    /* Program a word, so that a refused erase shows */
    WORD(TEST_ROW) = 0x12345678U;
    N->NVMCTRL_ADDR = TEST_ROW / 2U;
    nvm_cmd(CMD_WP);
    (void)wait_ready();
    lr = nvm_at(CMD_LR, TEST_ROW);
    l1 = N->NVMCTRL_LOCK;
    clear_errors();
    (void)nvm_at(CMD_ER, TEST_ROW);
    st_er = N->NVMCTRL_STATUS;
    if_er = N->NVMCTRL_INTFLAG;
    clear_errors();
    WORD(TEST_ROW + 4U) = 0x00000000U;
    N->NVMCTRL_ADDR = TEST_ROW / 2U;
    nvm_cmd(CMD_WP);
    (void)wait_ready();
    st_wp = N->NVMCTRL_STATUS;
    clear_errors();
    ur = nvm_at(CMD_UR, TEST_ROW);
    l2 = N->NVMCTRL_LOCK;
    console_puts("E10 lock");
    put_field("lock0", l0, 4);
    put_dec("lr_cyc", lr);
    put_field("lock_lr", l1, 4);
    put_field("er_status", st_er, 4);
    put_field("er_intflag", if_er, 2);
    put_field("wp_status", st_wp, 4);
    put_words("locked", TEST_ROW, 2);
    put_dec("ur_cyc", ur);
    put_field("lock_ur", l2, 4);
    (void)nvm_at(CMD_ER, TEST_ROW);
    put_words("unlocked_erase", TEST_ROW, 2);
    nl();
}

static void e11_prm(void)
{
    uint32_t s1, s2, s3;

    clear_errors();
    nvm_cmd(CMD_SPRM);
    s1 = N->NVMCTRL_STATUS;
    (void)WORD(DF_ROW(0));
    s2 = N->NVMCTRL_STATUS;
    nvm_cmd(CMD_CPRM);
    s3 = N->NVMCTRL_STATUS;
    console_puts("E11 prm");
    put_field("sprm", s1, 4);
    put_field("after_read", s2, 4);
    put_field("cprm", s3, 4);
    nl();
}

static void e12_narrow(void)
{
    uint32_t f8, st8, f16, st16;
    volatile uint8_t *p8 = (volatile uint8_t *)DF_ROW(7);
    volatile uint16_t *p16 = (volatile uint16_t *)(DF_ROW(7) + 8U);

    clear_errors();
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    clear_errors();
    expect_fault = 1;
    faults = 0;
    __asm volatile("strb %1, [%0]" : : "l"(p8), "l"(0x5AU) : "memory");
    f8 = faults;
    st8 = N->NVMCTRL_STATUS;
    __asm volatile("strh %1, [%0]" : : "l"(p16), "l"(0x5A5AU) : "memory");
    f16 = faults;
    st16 = N->NVMCTRL_STATUS;
    expect_fault = 0;
    console_puts("E12 narrow");
    put_dec("faults8", f8);
    put_field("status8", st8, 4);
    put_dec("faults16", f16 - f8);
    put_field("status16", st16, 4);
    put_field("pbl0", N->NVMCTRL_PBLDATA0, 8);
    put_field("pbl1", N->NVMCTRL_PBLDATA1, 8);
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    nl();
}

/* What an 8-bit (faulting) and a 16-bit load put in the page buffer */
static void e14_narrow_data(void)
{
    uint32_t f, st, p0, p1, q0, q1;
    volatile uint8_t *p8 = (volatile uint8_t *)(DF_ROW(8) + 1U);
    volatile uint16_t *p16 = (volatile uint16_t *)(DF_ROW(8) + 10U);

    (void)nvm_at(CMD_DFER, DF_ROW(8));
    nvm_cmd(CMD_PBC);
    (void)wait_ready();
    clear_errors();
    expect_fault = 1;
    faults = 0;
    __asm volatile("strb %1, [%0]" : : "l"(p8), "l"(0x5AU) : "memory");
    expect_fault = 0;
    f = faults;
    st = N->NVMCTRL_STATUS;
    p0 = N->NVMCTRL_PBLDATA0;
    p1 = N->NVMCTRL_PBLDATA1;
    __asm volatile("strh %1, [%0]" : : "l"(p16), "l"(0x1234U) : "memory");
    q0 = N->NVMCTRL_PBLDATA0;
    q1 = N->NVMCTRL_PBLDATA1;
    nvm_cmd(CMD_DFWP);
    (void)wait_ready();
    console_puts("E14 narrow_data");
    put_dec("faults8", f);
    put_field("status8", st, 4);
    put_field("pbl8", p1, 8);
    put_field("", p0, 8);
    put_field("pbl16", q1, 8);
    put_field("", q0, 8);
    put_words("row8", DF_ROW(8), 4);
    nl();
}

static void e13_addr(void)
{
    uint32_t er, a;

    /* Row 4 holds 0x5A5A5A5A from E8: erase it through ADDR alone */
    N->NVMCTRL_ADDR = (DF_ROW(4) + 0x40U) / 2U;
    a = N->NVMCTRL_ADDR;
    nvm_cmd(CMD_DFER);
    er = wait_ready();
    console_puts("E13 addr");
    put_field("addr", a, 8);
    put_dec("dfer_cyc", er);
    put_words("row4", DF_ROW(4), 1);
    N->NVMCTRL_ADDR = 0xFFFFFFFFU;
    put_field("addr_ff", N->NVMCTRL_ADDR, 8);
    nl();
}

int main(void)
{
    sys_initialize();
    console_puts("PP nvmctrlprobe: " BOARD_BANNER "\r\n");
    cyc_start();
    e0_boot();
    e1_keys();
    e2_loads();
    e3_random();
    e4_over();
    e5_pbc();
    e6_auto();
    e7_busy();
    e8_times();
    if (addr_ok) {
        e9_main();
        e10_lock();
    } else {
        console_puts("E9 skipped: ADDR is not the byte address / 2\r\n");
    }
    e11_prm();
    e12_narrow();
    e13_addr();
    e14_narrow_data();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
