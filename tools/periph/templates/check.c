/*
 * $MODULE check ($DEVICE, $MODULE $IP): the peripheral against what
 * silicon did in firmware/hello APP=${name}probe (docs/research/
 * peripherals/$module-$ip.md). Behaviour is checked exactly; what follows
 * the part, its supply or its temperature within a range. A failed check
 * names what it expected and ends its line in FAIL; PP:EXIT <failures>.
 * Run: py -m tools.periph check <board> $name (twin and silicon must pass;
 * the silicon transcript becomes the golden).
 *
 * Copyright (c) $year Clifford Swartz. MIT License (see LICENSE).
 */

#include "check.h"

static void check_reset(void)
{
    console_puts("reset");
    /* put_checked("ctrla", ${MODULE}_REGS->${MODULE}_CTRLA, 0, 2); ... */
    end_line();
}

int main(void)
{
    sys_initialize();
    systick_start();
    console_puts("PP $name: " BOARD_BANNER "\r\n");
    check_reset();
    test_exit();
}
