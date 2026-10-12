/*
 * Linker script for the mps2-an385 runner test images.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 *
 * QEMU loads the ELF with -kernel; the AN385 "flash" is SSRAM at 0x0 and
 * the data RAM starts at 0x20000000. The startup code only zeroes .bss, so
 * initialised .data is rejected at link time.
 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text : {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : { *(.data*) } > RAM

    .bss (NOLOAD) : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    _estack = ORIGIN(RAM) + LENGTH(RAM);
}

ASSERT(SIZEOF(.data) == 0, "test images must not use initialised .data")
