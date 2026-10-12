/*
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * micro:bit (nRF51822) memory map: 256 KB flash at 0, 16 KB RAM.
 */
MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 16K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text : { KEEP(*(.vectors)) *(.text*) *(.rodata*) } > FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
