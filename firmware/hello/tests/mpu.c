/*
 * MPU test (Cortex-M0+ r0p1 of the PIC32CM): MPU_TYPE, register read-back
 * (RNR, RBAR with VALID/REGION, RASR), and faults from a read-only region,
 * an XN region, a subregion, the background region (PRIVDEFENA) and, in
 * unprivileged mode (CONTROL.nPRIV), a privileged-only region.
 *
 * ARMv6-M has no MemManage exception: an MPU fault is a HardFault. The
 * handler records the fault and either skips the faulting 16-bit load or
 * store, or returns to the caller of a function that could not execute.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define AP_NONE         0U
#define AP_PRIV_RW      1U      /* unprivileged: no access */
#define AP_FULL         3U
#define AP_RO           6U      /* read-only, privileged and unprivileged */

#define RASR(ap, xn, srd, size) \
    ((uint32_t)(xn) << 28 | (uint32_t)(ap) << 24 | (uint32_t)(srd) << 8 | \
     ((uint32_t)(size) - 1U) << 1 | 1U)

enum { SKIP_INSN, RETURN_LR };

static volatile uint32_t faults;
static volatile uint32_t fault_pc;
static volatile uint32_t fault_action;

/* 256-byte aligned test area in SRAM, and a "bx lr" to execute from it */
static volatile uint32_t area[128] __attribute__((aligned(512)));

void hard_fault_c(uint32_t *frame)
{
    faults++;
    fault_pc = frame[6];
    if (fault_action == RETURN_LR) {
        frame[6] = frame[5] & ~1U;
    } else {
        frame[6] += 2;          /* the 16-bit ldr/str */
    }
}

void HardFault_Handler(void) __attribute__((naked));
void HardFault_Handler(void)
{
    __asm volatile(
        "movs r0, #4\n"
        "mov r1, lr\n"
        "tst r0, r1\n"
        "beq 1f\n"
        "mrs r0, psp\n"
        "b 2f\n"
        "1: mrs r0, msp\n"
        "2: ldr r2, =hard_fault_c\n"
        "bx r2\n");
}

/* SVC from unprivileged code: back to privileged */
void SVCall_Handler(void)
{
    __set_CONTROL(__get_CONTROL() & ~CONTROL_nPRIV_Msk);
}

static uint32_t load(volatile uint32_t *p)
{
    uint32_t v;

    __asm volatile("ldr %0, [%1]" : "=l"(v) : "l"(p) : "memory");
    return v;
}

static void store(volatile uint32_t *p, uint32_t v)
{
    __asm volatile("str %0, [%1]" : : "l"(v), "l"(p) : "memory");
}

static void eol(void)
{
    console_puts("\r\n");
}

static void region(unsigned int n, uint32_t base, uint32_t rasr)
{
    MPU->RNR = n;
    MPU->RBAR = base;
    MPU->RASR = rasr;
}

static void mpu_off(void)
{
    MPU->CTRL = 0;
    for (unsigned int n = 0; n < 8U; n++) {
        MPU->RNR = n;
        MPU->RASR = 0;
    }
    __DSB();
    __ISB();
}

static void mpu_on(uint32_t ctrl)
{
    MPU->CTRL = ctrl | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
}

static uint32_t faults_of(void (*fn)(void))
{
    uint32_t before = faults;

    fn();
    return faults - before;
}

static void do_store(void)
{
    store(&area[0], 0x1234);
}

static void do_load(void)
{
    (void)load(&area[0]);
}

static void do_load_sub1(void)
{
    (void)load(&area[8]);       /* byte 32: subregion 1 of a 256 B region */
}

static void do_load_port(void)
{
    (void)load((volatile uint32_t *)&PORT_REGS->GROUP[0].PORT_DIR);
}

static void do_exec(void)
{
    void (*fn)(void) = (void (*)(void))((uint32_t)&area[64] | 1U);

    fn();
}

static void test_regs(void)
{
    mpu_off();
    console_puts("regs:");
    put_checked("TYPE", MPU->TYPE, 0x800, 8);
    put_checked("CTRL", MPU->CTRL, 0, 1);
    MPU->RNR = 5;
    put_checked("RNR", MPU->RNR, 5, 1);
    MPU->RBAR = 0x20001000;
    put_checked("RBAR", MPU->RBAR, 0x20001005, 8);
    /* VALID: the REGION field selects the region and updates RNR */
    MPU->RBAR = 0x20002000 | MPU_RBAR_VALID_Msk | 3U;
    put_checked("RNR-valid", MPU->RNR, 3, 1);
    put_checked("RBAR-valid", MPU->RBAR, 0x20002003, 8);
    /* No TEX field; ADDR is [31:8] (regions of 256 bytes and up) */
    MPU->RASR = 0xFFFFFFFF;
    put_checked("RASR-ones", MPU->RASR, 0x1707FF3F, 8);
    MPU->RBAR = 0xFFFFFFE0 | 0U;
    put_checked("RBAR-ones", MPU->RBAR, 0xFFFFFF03, 8);
    mpu_off();
    eol();
}

/* Results are printed with the MPU off: the console is a peripheral */
static void test_faults(void)
{
    uint32_t base = (uint32_t)area;
    uint32_t r[11], pc;

    fault_action = SKIP_INSN;
    /* Off: nothing faults */
    r[0] = faults_of(do_store);

    /* Read-only 256 bytes over the test area, background on */
    region(0, base, RASR(AP_RO, 0, 0, 8));
    mpu_on(MPU_CTRL_PRIVDEFENA_Msk);
    r[1] = faults_of(do_load);
    r[2] = faults_of(do_store);
    pc = fault_pc;

    /* No access, but subregion 0 disabled: the background map applies */
    region(0, base, RASR(AP_NONE, 0, 0x01, 8));
    r[3] = faults_of(do_load);
    r[4] = faults_of(do_load_sub1);
    region(0, base, RASR(AP_NONE, 0, 0xFE, 8));
    r[5] = faults_of(do_load);
    r[6] = faults_of(do_load_sub1);

    /* XN over the area, whose second half holds "bx lr" */
    area[64] = 0x47704770;
    region(0, base, RASR(AP_FULL, 1, 0, 9));
    fault_action = RETURN_LR;
    r[7] = faults_of(do_exec);
    region(0, base, RASR(AP_FULL, 0, 0, 9));
    r[8] = faults_of(do_exec);
    fault_action = SKIP_INSN;

    /* No background: flash and SRAM regions only, PORT faults */
    region(0, 0x00000000, RASR(AP_FULL, 0, 0, 18));         /* 256 KiB */
    region(1, 0x20000000, RASR(AP_FULL, 0, 0, 14));         /* 16 KiB */
    mpu_on(0);
    r[9] = faults_of(do_load);
    r[10] = faults_of(do_load_port);
    mpu_off();

    console_puts("faults:");
    put_checked("off-store", r[0], 0, 1);
    put_checked("ro-load", r[1], 0, 1);
    put_checked("ro-store", r[2], 1, 1);
    put_field("pc", pc, 8);
    put_checked("srd0-sub0", r[3], 0, 1);
    put_checked("srd0-sub1", r[4], 1, 1);
    put_checked("srd1-sub0", r[5], 1, 1);
    put_checked("srd1-sub1", r[6], 0, 1);
    put_checked("xn-exec", r[7], 1, 1);
    put_checked("x-exec", r[8], 0, 1);
    put_checked("bg-sram", r[9], 0, 1);
    put_checked("bg-port", r[10], 1, 1);
    eol();
}

static void test_unprivileged(void)
{
    uint32_t base = (uint32_t)area, ctrl, unpriv, priv;

    fault_action = SKIP_INSN;
    /*
     * The background map is for privileged accesses only: unprivileged
     * code needs regions for its code and stack (higher regions win)
     */
    region(0, 0x00000000, RASR(AP_FULL, 0, 0, 18));         /* flash */
    region(1, 0x20000000, RASR(AP_FULL, 0, 0, 14));         /* SRAM */
    region(2, base, RASR(AP_PRIV_RW, 0, 0, 8));
    mpu_on(MPU_CTRL_PRIVDEFENA_Msk);
    priv = faults_of(do_load);
    __set_CONTROL(__get_CONTROL() | CONTROL_nPRIV_Msk);
    __ISB();
    ctrl = __get_CONTROL();
    unpriv = faults_of(do_load);
    __asm volatile("svc #0" ::: "memory");
    mpu_off();
    console_puts("unprivileged:");
    put_checked("priv-load", priv, 0, 1);
    put_checked("CONTROL", ctrl, CONTROL_nPRIV_Msk, 1);
    put_checked("unpriv-load", unpriv, 1, 1);
    put_checked("back", __get_CONTROL() & CONTROL_nPRIV_Msk, 0, 1);
    eol();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP mpu: " BOARD_BANNER "\r\n");

    test_regs();
    test_faults();
    test_unprivileged();
    console_puts("total:");
    put_field("faults", faults, 2);
    eol();
    test_exit();
}
