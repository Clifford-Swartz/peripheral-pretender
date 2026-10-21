/*
 * NVMCTRL probe 2 (PIC32CM MC00, NVMCTRL 03929): the two behaviours that
 * moved when nvmctrl.c's code moved (docs/research/peripherals/
 * nvmctrl-03929.md, 5.2): when STATUS.PRM is seen after SPRM, and what a
 * command written while one runs does. Prints raw results; checks nothing.
 * Run: py -m tools.periph probe mc00 nvmctrlprobe2
 *
 * Only the data flash is programmed; nvm_cmd() refuses everything else.
 *
 *   E1  PRM, main array cache on: right after SPRM, and after more code
 *   E2  PRM, cache off
 *   E3  PRM from RAM: twice, after a flash data read, after a fetch
 *   E4  PBC right after DFER (page buffer loaded): from flash
 *   E5  the same from RAM, no gap
 *   E6  from RAM with gaps of 1, 4, 16 and 64 cycles
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

#define N               NVMCTRL_REGS
#define KEY             0xA500U
#define READY           NVMCTRL_INTFLAG_READY_Msk
#define CMD_DFER        0x1AU
#define CMD_SPRM        0x42U
#define CMD_CPRM        0x43U
#define CMD_PBC         0x44U
#define DF              0x00400000U
#define DF_ROW(n)       (DF + 0x100U * (n))
#define WORD(a)         (*(volatile uint32_t *)(a))
#define CACHEDIS_MAIN   NVMCTRL_CTRLB_CACHEDIS(1)
#define RAMFUNC         __attribute__((section(".ramfunc"), noinline, \
                                       long_call))

static void nl(void)
{
    console_puts("\r\n");
}

static void wait_ready(void)
{
    for (uint32_t i = 0; !(N->NVMCTRL_INTFLAG & READY) && i < 10000000U;
         i++) {
    }
}

static void nvm_cmd(uint32_t cmd)
{
    if (cmd != CMD_DFER && cmd != CMD_SPRM && cmd != CMD_CPRM &&
        cmd != CMD_PBC) {
        console_puts("\r\nnvm_cmd: refused\r\nPP:EXIT 1\r\n");
        for (;;) {
        }
    }
    N->NVMCTRL_CTRLA = KEY | cmd;
}

static void clear_errors(void)
{
    N->NVMCTRL_STATUS = 0x1EU;
    N->NVMCTRL_INTFLAG = NVMCTRL_INTFLAG_ERROR_Msk;
}

static void e1_e2_prm(void)
{
    uint32_t s1, s2, s3, ctrlb = N->NVMCTRL_CTRLB;

    nvm_cmd(CMD_SPRM);
    s1 = N->NVMCTRL_STATUS;
    for (volatile uint32_t i = 0; i < 100U; i++) {
    }
    s2 = N->NVMCTRL_STATUS;
    nvm_cmd(CMD_CPRM);
    console_puts("E1 prm_cache_on");
    put_field("now", s1, 4);
    put_field("later", s2, 4);
    nl();
    N->NVMCTRL_CTRLB = ctrlb | CACHEDIS_MAIN;
    nvm_cmd(CMD_SPRM);
    s3 = N->NVMCTRL_STATUS;
    nvm_cmd(CMD_CPRM);
    N->NVMCTRL_CTRLB = ctrlb;
    console_puts("E2 prm_cache_off");
    put_field("now", s3, 4);
    nl();
}

/* From RAM: no fetch from flash until it returns */
RAMFUNC static void ram_prm(uint32_t *out)
{
    N->NVMCTRL_CTRLA = KEY | CMD_SPRM;
    out[0] = N->NVMCTRL_STATUS;
    out[1] = N->NVMCTRL_STATUS;
    (void)WORD(DF_ROW(0));
    out[2] = N->NVMCTRL_STATUS;
    (void)WORD(0x00000100U);
    out[3] = N->NVMCTRL_STATUS;
}

static void e3_prm_ram(void)
{
    uint32_t v[4];

    ram_prm(v);
    v[3] = v[3];
    nvm_cmd(CMD_CPRM);
    console_puts("E3 prm_ram");
    put_field("now", v[0], 4);
    put_field("again", v[1], 4);
    put_field("after_df_read", v[2], 4);
    put_field("after_main_read", v[3], 4);
    put_field("back_in_flash", N->NVMCTRL_STATUS, 4);
    nl();
}

/* A word in the page buffer at row @row, which sets ADDR there */
static void load_row(unsigned row)
{
    nvm_cmd(CMD_PBC);
    wait_ready();
    clear_errors();
    WORD(DF_ROW(row)) = 0x12345678U;
}

static void put_busy(const char *name, uint32_t st_now, uint32_t if_now)
{
    wait_ready();
    console_puts(name);
    put_field("status_now", st_now, 4);
    put_field("intflag_now", if_now, 2);
    put_field("status_done", N->NVMCTRL_STATUS, 4);
    put_field("intflag_done", N->NVMCTRL_INTFLAG, 2);
    nl();
}

static void e4_busy_flash(void)
{
    uint32_t st, fl;

    load_row(9);
    N->NVMCTRL_CTRLA = KEY | CMD_DFER;
    N->NVMCTRL_CTRLA = KEY | CMD_PBC;
    st = N->NVMCTRL_STATUS;
    fl = N->NVMCTRL_INTFLAG;
    put_busy("E4 busy_flash", st, fl);
}

/* DFER, @gap delay loops, PBC; STATUS and INTFLAG right after */
RAMFUNC static void ram_busy(uint32_t gap, uint32_t *out)
{
    N->NVMCTRL_CTRLA = KEY | CMD_DFER;
    for (uint32_t i = 0; i < gap; i++) {
        __asm volatile("nop");
    }
    N->NVMCTRL_CTRLA = KEY | CMD_PBC;
    out[0] = N->NVMCTRL_STATUS;
    out[1] = N->NVMCTRL_INTFLAG;
}

static void e5_e6_busy_ram(void)
{
    static const uint32_t gaps[] = { 0, 1, 4, 16, 64 };
    uint32_t v[2];

    for (unsigned i = 0; i < 5U; i++) {
        load_row(10 + i);
        ram_busy(gaps[i], v);
        console_puts(i == 0 ? "E5 busy_ram" : "E6 busy_ram");
        put_field("gap", gaps[i], 2);
        wait_ready();
        put_field("status_now", v[0], 4);
        put_field("intflag_now", v[1], 2);
        put_field("status_done", N->NVMCTRL_STATUS, 4);
        put_field("intflag_done", N->NVMCTRL_INTFLAG, 2);
        nl();
    }
}

int main(void)
{
    sys_initialize();
    console_puts("PP nvmctrlprobe2: " BOARD_BANNER "\r\n");
    console_puts("E0 boot");
    put_field("ctrlb", N->NVMCTRL_CTRLB, 8);
    nl();
    e1_e2_prm();
    e3_prm_ram();
    e4_busy_flash();
    e5_e6_busy_ram();
    nvm_cmd(CMD_PBC);
    wait_ready();
    console_puts("PP:EXIT 0\r\n");
    console_flush();
    for (;;) {
    }
}
