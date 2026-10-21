/*
 * NVMCTRL check (PIC32CM MC00, NVMCTRL 03929): the flash controller
 * against what silicon did in firmware/hello APP=nvmctrlprobe
 * (docs/research/peripherals/nvmctrl-03929.md, section 5). Register and
 * flash contents are checked exactly, times within a range. A failed
 * check names what it expected and ends its line in FAIL;
 * PP:EXIT <failures>. Run: py -m tools.periph check mc00 nvmctrl
 *
 * Safety, as in the probe: nvm_cmd() refuses SSB, SCEHL, EAR and WAP, and
 * main array commands outside TEST_ROW, the last row of flash. The data
 * flash takes the rest; locks are volatile.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

#define N               NVMCTRL_REGS
#define KEY             0xA500U
#define READY           NVMCTRL_INTFLAG_READY_Msk
#define ERROR           NVMCTRL_INTFLAG_ERROR_Msk
#define MANW            NVMCTRL_CTRLB_MANW_Msk
#define ST_LOAD         NVMCTRL_STATUS_LOAD_Msk
#define PROGE           NVMCTRL_STATUS_PROGE_Msk
#define LOCKE           NVMCTRL_STATUS_LOCKE_Msk
#define STATUS_ERRS     0x1EU

#define CMD_ER          0x02U
#define CMD_WP          0x04U
#define CMD_EAR         0x05U
#define CMD_WAP         0x06U
#define CMD_DFER        0x1AU
#define CMD_DFWP        0x1CU
#define CMD_LR          0x40U
#define CMD_UR          0x41U
#define CMD_PBC         0x44U
#define CMD_SSB         0x45U
#define CMD_SCEHL       0x7FU

#define DF              0x00400000U
#define DF_ROW(n)       (DF + 0x100U * (n))
#define TEST_ROW        0x0001FF00U
#define WORD(a)         (*(volatile uint32_t *)(a))
#define FF              0xFFFFFFFFU

/* Cycles at 48 MHz: microseconds * 48 */
#define US(n)           ((n) * 48U)

static volatile uint32_t expect_fault, faults;

void hardfault_c(uint32_t *frame);
void hardfault_c(uint32_t *frame)
{
    if (!expect_fault) {
        console_puts("\r\nunexpected HardFault FAIL\r\nPP:EXIT 1\r\n");
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

static void wait_ready(void)
{
    uint32_t t0 = cyc();

    while (!(N->NVMCTRL_INTFLAG & READY) && cyc_since(t0) < 0x00F00000U) {
    }
}

static void clear_errors(void)
{
    N->NVMCTRL_STATUS = STATUS_ERRS;
    N->NVMCTRL_INTFLAG = ERROR;
}

static void nvm_cmd(uint32_t cmd)
{
    uint32_t a = N->NVMCTRL_ADDR * 2U;

    if (cmd == CMD_SSB || cmd == CMD_SCEHL || cmd == CMD_EAR ||
        cmd == CMD_WAP ||
        ((cmd == CMD_ER || cmd == CMD_WP) &&
         (a < TEST_ROW || a >= TEST_ROW + 0x100U))) {
        console_puts("\r\nnvm_cmd: refused FAIL\r\nPP:EXIT 1\r\n");
        for (;;) {
        }
    }
    N->NVMCTRL_CTRLA = KEY | cmd;
}

/* Main array: byte address / 2; data flash: offset / 2. Cycles to READY */
static uint32_t nvm_at(uint32_t cmd, uint32_t addr)
{
    uint32_t t0;

    N->NVMCTRL_ADDR = (addr >= DF ? addr - DF : addr) / 2U;
    t0 = cyc();
    nvm_cmd(cmd);
    wait_ready();
    return cyc_since(t0);
}

static uint32_t nvm_timed(uint32_t cmd)
{
    uint32_t t0 = cyc();

    nvm_cmd(cmd);
    wait_ready();
    return cyc_since(t0);
}

static void check_words(const char *name, uint32_t addr, const uint32_t *w,
                        unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        put_checked(name, WORD(addr + 4U * i), w[i], 8);
    }
}

static void check_keys(void)
{
    uint32_t st1, if1, ca1, st2, ca2, ca3;

    clear_errors();
    N->NVMCTRL_CTRLA = CMD_PBC;         /* no key */
    st1 = N->NVMCTRL_STATUS;
    if1 = N->NVMCTRL_INTFLAG;
    ca1 = N->NVMCTRL_CTRLA;
    clear_errors();
    N->NVMCTRL_CTRLA = KEY | 0x03U;     /* reserved */
    wait_ready();
    st2 = N->NVMCTRL_STATUS;
    ca2 = N->NVMCTRL_CTRLA;
    clear_errors();
    nvm_cmd(CMD_PBC);
    wait_ready();
    ca3 = N->NVMCTRL_CTRLA;
    console_puts("keys");
    put_checked("param", N->NVMCTRL_PARAM, 0x04030800, 8);
    put_checked("nokey_status", st1, PROGE, 4);
    put_checked("nokey_intflag", if1, READY | ERROR, 2);
    put_checked("nokey_ctrla", ca1, 0x0000, 4);
    put_checked("rsvd_status", st2, PROGE, 4);
    put_checked("rsvd_ctrla", ca2, 0x0000, 4);
    put_checked("pbc_ctrla", ca3, CMD_PBC, 4);
    end_line();
}

static void check_loads(void)
{
    static const uint32_t row0[] = { 0x11111111, 0x22222222, 0x33333333,
                                     FF };
    uint32_t er, wp, a1, st1, p[6], a3, rd, st;

    clear_errors();
    er = nvm_at(CMD_DFER, DF_ROW(0));
    WORD(DF_ROW(0)) = 0x11111111U;
    a1 = N->NVMCTRL_ADDR;
    st1 = N->NVMCTRL_STATUS;
    p[0] = N->NVMCTRL_PBLDATA1;
    p[1] = N->NVMCTRL_PBLDATA0;
    WORD(DF_ROW(0) + 4U) = 0x22222222U;
    p[2] = N->NVMCTRL_PBLDATA1;
    p[3] = N->NVMCTRL_PBLDATA0;
    WORD(DF_ROW(0) + 8U) = 0x33333333U;
    p[4] = N->NVMCTRL_PBLDATA1;
    p[5] = N->NVMCTRL_PBLDATA0;
    a3 = N->NVMCTRL_ADDR;
    rd = WORD(DF_ROW(0));
    wp = nvm_timed(CMD_DFWP);
    st = N->NVMCTRL_STATUS;
    console_puts("loads");
    put_dec_range("dfer_cyc", er, US(700), US(850));
    put_checked("addr1", a1, 0, 8);
    put_checked("status1", st1, ST_LOAD, 4);
    put_checked("pbl_a1", p[0], FF, 8);
    put_checked("pbl_a0", p[1], 0x11111111, 8);
    put_checked("pbl_b1", p[2], 0x22222222, 8);
    put_checked("pbl_b0", p[3], 0x11111111, 8);
    put_checked("pbl_c1", p[4], FF, 8);
    put_checked("pbl_c0", p[5], 0x33333333, 8);
    put_checked("addr3", a3, 4, 8);
    put_checked("rd_before", rd, FF, 8);
    put_dec_range("dfwp_cyc", wp, US(110), US(150));
    put_checked("status", st, 0, 4);
    check_words("row0", DF_ROW(0), row0, 4);
    end_line();
}

static void check_buffer(void)
{
    static const uint32_t row1[] = { FF, 2, FF, 3 };
    static const uint32_t row0[] = { 0x01010101, 0x22222222, 0x33333333,
                                     0xF0F0F0F0 };
    uint32_t l1, l2, p0;

    /* Random-access loads (the data sheet's example) */
    (void)nvm_at(CMD_DFER, DF_ROW(1));
    WORD(DF_ROW(1) + 8U) = 1U;
    WORD(DF_ROW(1) + 4U) = 2U;
    WORD(DF_ROW(1) + 12U) = 3U;
    (void)nvm_timed(CMD_DFWP);
    /* Programming only clears bits */
    WORD(DF_ROW(0)) = 0x0F0F0F0FU;
    WORD(DF_ROW(0) + 12U) = 0xF0F0F0F0U;
    (void)nvm_timed(CMD_DFWP);
    /* Page buffer clear */
    clear_errors();
    WORD(DF_ROW(0) + 16U) = 0x12345678U;
    l1 = N->NVMCTRL_STATUS;
    nvm_cmd(CMD_PBC);
    wait_ready();
    l2 = N->NVMCTRL_STATUS;
    p0 = N->NVMCTRL_PBLDATA0;
    (void)nvm_timed(CMD_DFWP);
    console_puts("buffer");
    check_words("random", DF_ROW(1), row1, 4);
    check_words("and", DF_ROW(0), row0, 4);
    put_checked("loaded", l1, ST_LOAD, 4);
    put_checked("pbc", l2, 0, 4);
    put_checked("pbl0_pbc", p0, FF, 8);
    put_checked("after_pbc", WORD(DF_ROW(0) + 16U), FF, 8);
    end_line();
}

static void check_auto(void)
{
    uint32_t if15, st15, if16, t0, aw, st16, stp;

    (void)nvm_at(CMD_DFER, DF_ROW(2));
    clear_errors();
    N->NVMCTRL_CTRLB &= ~MANW;
    for (unsigned i = 0; i < 15U; i++) {
        WORD(DF_ROW(2) + 4U * i) = 0xA0000000U + i;
    }
    if15 = N->NVMCTRL_INTFLAG;
    st15 = N->NVMCTRL_STATUS;
    t0 = cyc();
    WORD(DF_ROW(2) + 60U) = 0xA000000FU;
    if16 = N->NVMCTRL_INTFLAG;
    wait_ready();
    aw = cyc_since(t0);
    st16 = N->NVMCTRL_STATUS;
    for (unsigned i = 0; i < 4U; i++) {
        WORD(DF_ROW(2) + 64U + 4U * i) = 0xB0000000U + i;
    }
    N->NVMCTRL_CTRLB |= MANW;
    stp = N->NVMCTRL_STATUS;
    console_puts("auto");
    put_checked("intflag15", if15, READY, 2);
    put_checked("status15", st15, ST_LOAD, 4);
    put_checked("intflag16", if16, 0, 2);
    put_dec_range("auto_cyc", aw, US(110), US(190));
    put_checked("status16", st16, 0, 4);
    put_checked("w0", WORD(DF_ROW(2)), 0xA0000000, 8);
    put_checked("w15", WORD(DF_ROW(2) + 60U), 0xA000000F, 8);
    put_checked("partial_status", stp, ST_LOAD, 4);
    put_checked("partial", WORD(DF_ROW(2) + 64U), FF, 8);
    nvm_cmd(CMD_PBC);
    wait_ready();
    end_line();
}

/*
 * DFER, then PBC while it runs, from RAM: from flash, the fetch timing
 * around the two writes moves with the code's layout, and so did whether
 * silicon flagged the second (nvmctrlprobe2.c)
 */
__attribute__((section(".ramfunc"), noinline, long_call))
static void ram_busy(uint32_t *out)
{
    N->NVMCTRL_CTRLA = KEY | CMD_DFER;
    out[0] = N->NVMCTRL_INTFLAG;
    N->NVMCTRL_CTRLA = KEY | CMD_PBC;
    out[1] = N->NVMCTRL_STATUS;
}

static void check_busy(void)
{
    uint32_t v[2];

    nvm_cmd(CMD_PBC);
    wait_ready();
    clear_errors();
    WORD(DF_ROW(3)) = 0x12345678U;      /* loaded; ADDR at row 3 */
    ram_busy(v);
    wait_ready();
    console_puts("busy");
    put_checked("intflag_after_cmd", v[0], 0, 2);
    put_checked("status_busy_cmd", v[1], PROGE | ST_LOAD, 4);
    put_checked("intflag", N->NVMCTRL_INTFLAG, READY | ERROR, 2);
    put_checked("pbc_ignored", N->NVMCTRL_STATUS, PROGE | ST_LOAD, 4);
    nvm_cmd(CMD_PBC);
    wait_ready();
    end_line();
}

static void check_main(void)
{
    static const uint32_t test[] = { 0xC0DE0000, 0xC0DE0001, FF };
    uint32_t er, wp, a, er2;

    clear_errors();
    er = nvm_at(CMD_ER, TEST_ROW);
    WORD(TEST_ROW + 4U) = 0xC0DE0001U;
    a = N->NVMCTRL_ADDR;
    WORD(TEST_ROW) = 0xC0DE0000U;
    wp = nvm_timed(CMD_WP);
    console_puts("main");
    put_dec_range("er_cyc", er, US(700), US(850));
    put_checked("addr_load", a, (TEST_ROW + 4U) / 2U, 8);
    put_dec_range("wp_cyc", wp, US(150), US(190));
    put_checked("status", N->NVMCTRL_STATUS, 0, 4);
    check_words("test", TEST_ROW, test, 3);
    er2 = nvm_at(CMD_ER, TEST_ROW);
    put_dec_range("er2_cyc", er2, US(700), US(850));
    put_checked("erased", WORD(TEST_ROW), FF, 8);
    end_line();
}

static void check_lock(void)
{
    uint32_t l0, l1, st_er, if_er, st_wp, l2;

    clear_errors();
    l0 = N->NVMCTRL_LOCK;
    WORD(TEST_ROW) = 0x12345678U;
    (void)nvm_timed(CMD_WP);
    (void)nvm_at(CMD_LR, TEST_ROW);
    l1 = N->NVMCTRL_LOCK;
    clear_errors();
    (void)nvm_at(CMD_ER, TEST_ROW);
    st_er = N->NVMCTRL_STATUS;
    if_er = N->NVMCTRL_INTFLAG;
    clear_errors();
    WORD(TEST_ROW + 4U) = 0x00000000U;
    N->NVMCTRL_ADDR = TEST_ROW / 2U;
    (void)nvm_timed(CMD_WP);
    st_wp = N->NVMCTRL_STATUS;
    clear_errors();
    nvm_cmd(CMD_PBC);
    wait_ready();
    (void)nvm_at(CMD_UR, TEST_ROW);
    l2 = N->NVMCTRL_LOCK;
    console_puts("lock");
    put_checked("lock0", l0, 0xFFFF, 4);
    put_checked("lock_lr", l1, 0x7FFF, 4);
    put_checked("er_status", st_er, LOCKE, 4);
    put_checked("er_intflag", if_er, READY | ERROR, 2);
    put_checked("wp_status", st_wp, LOCKE | ST_LOAD, 4);
    put_checked("kept", WORD(TEST_ROW), 0x12345678, 8);
    put_checked("kept1", WORD(TEST_ROW + 4U), FF, 8);
    put_checked("lock_ur", l2, 0xFFFF, 4);
    (void)nvm_at(CMD_ER, TEST_ROW);
    put_checked("unlocked_erase", WORD(TEST_ROW), FF, 8);
    end_line();
}

static void check_narrow(void)
{
    static const uint32_t row8[] = { 0xFFFF5A5A, FF, 0x1234FFFF, FF };
    uint32_t f8, st8, p0, f16, q0;
    volatile uint8_t *p8 = (volatile uint8_t *)(DF_ROW(8) + 1U);
    volatile uint16_t *p16 = (volatile uint16_t *)(DF_ROW(8) + 10U);

    (void)nvm_at(CMD_DFER, DF_ROW(8));
    nvm_cmd(CMD_PBC);
    wait_ready();
    clear_errors();
    expect_fault = 1;
    faults = 0;
    __asm volatile("strb %1, [%0]" : : "l"(p8), "l"(0x5AU) : "memory");
    f8 = faults;
    st8 = N->NVMCTRL_STATUS;
    p0 = N->NVMCTRL_PBLDATA0;
    __asm volatile("strh %1, [%0]" : : "l"(p16), "l"(0x1234U) : "memory");
    expect_fault = 0;
    f16 = faults - f8;
    q0 = N->NVMCTRL_PBLDATA0;
    (void)nvm_timed(CMD_DFWP);
    console_puts("narrow");
    put_checked("faults8", f8, 1, 1);
    put_checked("status8", st8, ST_LOAD, 4);
    put_checked("pbl8", p0, 0xFFFF5A5A, 8);
    put_checked("faults16", f16, 0, 1);
    put_checked("pbl16", q0, 0x1234FFFF, 8);
    check_words("row8", DF_ROW(8), row8, 4);
    end_line();
}

static void check_misc(void)
{
    uint32_t a, written;

    /* STATUS.PRM after SPRM moves with the code's layout: see the qtest */
    /* ADDR is 16 bits: the data flash base folds away, DFER still works */
    (void)nvm_at(CMD_DFER, DF_ROW(4));
    WORD(DF_ROW(4)) = 0x5A5A5A5AU;
    (void)nvm_timed(CMD_DFWP);
    written = WORD(DF_ROW(4));
    N->NVMCTRL_ADDR = DF_ROW(4) / 2U;
    a = N->NVMCTRL_ADDR;
    nvm_cmd(CMD_DFER);
    wait_ready();
    N->NVMCTRL_ADDR = 0xFFFFFFFFU;
    console_puts("misc");
    put_checked("written", written, 0x5A5A5A5A, 8);
    put_checked("addr", a, (DF_ROW(4) / 2U) & 0xFFFFU, 8);
    put_checked("row4", WORD(DF_ROW(4)), FF, 8);
    put_checked("addr_ff", N->NVMCTRL_ADDR, 0xFFFF, 8);
    nvm_cmd(CMD_PBC);
    wait_ready();
    end_line();
}

/*
 * Code programmed into flash runs, and runs anew after reprogramming
 * (silicon: commands invalidate the NVM cache; the twin: translated code)
 */
static void check_exec(void)
{
    uint32_t (*fn)(void) = (uint32_t (*)(void))(TEST_ROW | 1U);
    uint32_t r1, r2;

    (void)nvm_at(CMD_ER, TEST_ROW);
    WORD(TEST_ROW) = 0x47702001U;       /* movs r0, #1; bx lr */
    (void)nvm_timed(CMD_WP);
    r1 = fn();
    (void)nvm_at(CMD_ER, TEST_ROW);
    WORD(TEST_ROW) = 0x47702002U;       /* movs r0, #2; bx lr */
    (void)nvm_timed(CMD_WP);
    r2 = fn();
    (void)nvm_at(CMD_ER, TEST_ROW);
    console_puts("exec");
    put_checked("first", r1, 1, 1);
    put_checked("reprogrammed", r2, 2, 1);
    end_line();
}

int main(void)
{
    sys_initialize();
    console_puts("PP nvmctrl: " BOARD_BANNER "\r\n");
    cyc_start();
    check_keys();
    check_loads();
    check_buffer();
    check_auto();
    check_busy();
    check_main();
    check_lock();
    check_narrow();
    check_misc();
    check_exec();
    test_exit();
}
