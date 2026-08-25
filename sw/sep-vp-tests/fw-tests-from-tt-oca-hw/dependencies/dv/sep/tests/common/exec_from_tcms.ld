/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/* Copyright 2025 Tenstorrent Inc. */

OUTPUT_ARCH( "riscv" )


MEMORY {
    dtcm(rw):     ORIGIN = 0x0,        LENGTH = 0x10000  /* 64 KiB */
    itcm(x):      ORIGIN = 0x01000000, LENGTH = 0x20000  /* 128 KiB */
    bootrom(rx):  ORIGIN = 0x10000000, LENGTH = 0x10000  /* 64 KiB (Assumed) */
    sram(rwx):    ORIGIN = 0x10100000, LENGTH = 0x40000  /* 256 KiB (Assumed) */
}

/* ENTRY(ORIGIN(itcm)) */
ENTRY(_start)

SECTIONS {

    /* TODO: .vectors? */

    . = ORIGIN(itcm);

    .text : {
        . = ALIGN(4);
        *(.text._start)
        *(.text)
        *(.text.*)
    } > itcm

    . = ORIGIN(dtcm);

    .data : {
        . = ALIGN(4);
        *(.data)
        . = ALIGN(4);
        *(.data.*)
        . = ALIGN(4);
        *(.sdata)
        . = ALIGN(4);
        *(.sdata.*)
        . = ALIGN(4);
        *(.rodata)
        . = ALIGN(4);
        *(.rodata.*)
        . = ALIGN(4);
        *(.srodata)
        . = ALIGN(4);
        *(.srodata.*)
    } > dtcm

    .bss : {
        BSS_START = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        BSS_END = .;

        /* 1.5 KB heap after BSS
        . = . + 4;
        . = ALIGN(4);
        __heap_start = .;  /* — points at the start of the heap available for sbrk. */
        __heap_end = . + 0x600;  /* — points at the end of the heap available for sbrk. */

        /* Place stack at the end of the DTCM, there probably is a better way to do this */
        . = ORIGIN(dtcm) + LENGTH(dtcm) - 16;
        . = ALIGN(16);
        STACK = .;
        stack = .;
        _stack = .;
    } > dtcm

}
