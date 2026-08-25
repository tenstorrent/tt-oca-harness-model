/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/* Copyright 2025 Tenstorrent Inc.
 *
 * BL1 linker script: produces a flat binary loadable by rom_handoff_bl1().
 *
 * Key difference from exec_from_tcms.ld:
 *   .data/.rodata VMA is in DCCM but LMA is in ICCM (via AT> itcm).
 *   objcopy -O binary produces a single contiguous blob (text + data LMA).
 *   rom_handoff copies the entire blob to ICCM; crt0_bl1.s then copies
 *   the data portion from ICCM to DCCM at runtime.
 */

OUTPUT_ARCH( "riscv" )

MEMORY {
    itcm(rwx):    ORIGIN = 0xC0000000, LENGTH = 0x40000  /* 256 KiB - VeeR ICCM (CPU-internal) */
    dtcm(rw):     ORIGIN = 0xC0040000, LENGTH = 0x20000  /* 128 KiB - VeeR DCCM (CPU-internal) */
}

ENTRY(_start)

SECTIONS {

    . = ORIGIN(itcm);

    .text : {
        . = ALIGN(4);
        *(.text.init)
        *(.text._start)
        *(.text)
        *(.text.*)
    } > itcm

    /* Initialized data: VMA in DCCM, LMA appended after .text in ICCM.
     * crt0_bl1.s copies from __data_lma (ICCM) to __data_start (DCCM). */
    .data : ALIGN(4) {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)
        . = ALIGN(4);
        __data_end = .;
    } > dtcm AT> itcm

    __data_lma = LOADADDR(.data);

    /* Uninitialized data (zeroed by crt0_bl1.s). */
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        . = ALIGN(4);
        __bss_end = .;
    } > dtcm

    /* Stack at end of DTCM */
    . = ORIGIN(dtcm) + LENGTH(dtcm) - 16;
    . = ALIGN(16);
    STACK = .;
    stack = .;
    _stack = .;
}
