/*
 * Minimal Cortex-M3 startup for the mps2-an385 runner test images.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */
#include <stdint.h>

extern uint32_t _sbss, _ebss, _estack;
extern int main(void);

void Reset_Handler(void)
{
    for (uint32_t *p = &_sbss; p < &_ebss; p++) {
        *p = 0;
    }
    main();
    for (;;) {
    }
}

static void Default_Handler(void)
{
    for (;;) {
    }
}

__attribute__((section(".isr_vector"), used))
static const void *const vectors[16] = {
    &_estack,
    (const void *)Reset_Handler,
    (const void *)Default_Handler,     /* NMI */
    (const void *)Default_Handler,     /* HardFault */
    (const void *)Default_Handler,     /* MemManage */
    (const void *)Default_Handler,     /* BusFault */
    (const void *)Default_Handler,     /* UsageFault */
};
