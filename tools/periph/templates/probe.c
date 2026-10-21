/*
 * $MODULE probe ($DEVICE, $MODULE $IP): what the peripheral does on
 * silicon, for the model. Prints raw results, one experiment per line;
 * checks nothing. The plan and the answers: docs/research/peripherals/
 * $module-$ip.md. Run: py -m tools.periph probe <board> ${name}probe
 *
 *   E0  reset values and writable masks
 *   E1  ...
 *
 * Copyright (c) $year Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

static void nl(void)
{
    console_puts("\r\n");
}

static void e0_reset(void)
{
    console_puts("E0 reset");
    /* put_field("ctrla", ${MODULE}_REGS->${MODULE}_CTRLA, 2); ... */
    nl();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP ${name}probe: " BOARD_BANNER "\r\n");
    e0_reset();
    console_puts("PP:EXIT 0\r\n");
    for (;;) {
    }
}
