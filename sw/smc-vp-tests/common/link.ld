/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/common/link.ld
 *
 * Bare-metal RV64 linker script for smc-vp firmware tests.
 * Code/data live in the cluster's fast-mem window at 0x80000000; the stack
 * grows down from the top of the window (0x90000000, set in start.S).
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    .text : {
        *(.text.init)
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > RAM

    .data : {
        *(.data*)
        *(.sdata*)
    } > RAM

    .bss : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    . = ALIGN(16);
    _end = .;
}
