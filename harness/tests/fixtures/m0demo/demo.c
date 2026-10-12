/*
 * Minimal Cortex-M0 image for the harness self-tests on QEMU's microbit
 * machine: three calls to step(), then done() spins forever.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */
#include <stdint.h>

volatile uint32_t counter;
volatile uint32_t marks[4];

void step(uint32_t n)
{
    counter += n;
    marks[n & 3] = counter;
}

void done(void)
{
    for (;;) {
    }
}

int main(void)
{
    for (uint32_t i = 1; i <= 3; i++) {
        step(i);
    }
    done();
    return 0;
}

void Reset_Handler(void)
{
    counter = 0;
    for (int i = 0; i < 4; i++) {
        marks[i] = 0;
    }
    main();
}

/* Initial SP = top of the micro:bit's 16 KB RAM, then the reset vector. */
__attribute__((section(".vectors"), used))
static void (*const vectors[])(void) = {
    (void (*)(void))0x20004000,
    Reset_Handler,
};
