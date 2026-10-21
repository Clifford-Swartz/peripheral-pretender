/*
 * QTest for the $MODULE of the $DEVICE ($MACHINE)
 *
 * Peripheral Pretender: a personal project. AI-assisted code;
 * not for submission to upstream QEMU.
 *
 * The board side of the model and its timing on stepped virtual time;
 * firmware/hello APP=$name checks the rest against silicon.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"

#define MACHINE         "-M $MACHINE"
#define BASE            $BASE

static void test_reset(void)
{
    QTestState *qts = qtest_init(MACHINE);

    g_assert_cmphex(qtest_readb(qts, BASE + 0x00), ==, 0);
    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("$MACHINE-$name/reset", test_reset);
    return g_test_run();
}
