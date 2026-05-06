/* Copyright 2025 Tenstorrent Inc.
 *
 * Linker script for VeeR EL2 bare-metal VP (Virtual Platform) environment.
 *
 * Memory Layout (matches riscv-vp-plusplus Args.hpp):
 *   ICCM (itcm): 0x01000000 - 0x0101FFFF (128 KiB) - Instructions
 *   DCCM (dtcm): 0x00000000 - 0x0000FFFF ( 64 KiB) - Data
 *
 * DCCM Layout (from low to high address):
 *   0x00000000: .data section start
 *   ...       : .bss section
 *   ...       : Free space (stack grows down into this)
 *   0x0000FBFC: STACK (grows downward)
 *   0x0000FC00: INTVEC_BASE (1KB vector table, 1024-byte aligned)
 *   0x0000FFFF: End of DCCM
 *
 * Use with: make PLATFORM=vp
 * Counterpart RTL script: exec_from_tcms.ld
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY {
    itcm (x)  : ORIGIN = 0x01000000, LENGTH = 0x20000  /* 128 KiB - VP ICCM */
    dtcm (rw) : ORIGIN = 0x00000000, LENGTH = 0x10000  /*  64 KiB - VP DCCM */
}

SECTIONS {
    /*=========================================================================
     * ICCM: Instruction Tightly Coupled Memory
     *========================================================================*/
    . = ORIGIN(itcm);

    .text : {
        . = ALIGN(4);
        *(.text.init)       /* Startup code first */
        *(.text._start)
        *(.text.init)
        *(.text)
        *(.text.*)
        . = ALIGN(4);

        /*
         * NMI handler - must be 256-byte aligned for VeeR nmi_vec.
         * Included in .text to ensure contiguous hex file output.
         */
        . = ALIGN(256);
        *(.nmi_handler)
        . = ALIGN(4);
    } > itcm

    /*=========================================================================
     * DCCM: Data Tightly Coupled Memory
     *========================================================================*/
    . = ORIGIN(dtcm);

    .data : {
        . = ALIGN(4);

        /* Global pointer for linker relaxation (placed early in .data) */
        PROVIDE(__global_pointer$ = . + 0x800);

        *(.data)
        *(.data.*)
        . = ALIGN(4);
        *(.sdata)
        *(.sdata.*)
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        *(.srodata)
        *(.srodata.*)
        . = ALIGN(4);
    } > dtcm

    .bss (NOLOAD) : {
        . = ALIGN(4);
        BSS_START = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        BSS_END = .;
    } > dtcm

    /*=========================================================================
     * Stack and Vector Table Layout
     *
     * The interrupt vector table MUST be at a 1024-byte (0x400) aligned
     * address for the VeeR EL2 meivt CSR. Placed at the end of DCCM.
     *
     * Layout:
     *   BSS_END     ... grows upward
     *   STACK       ... grows downward from 0x0000FBFC
     *   INTVEC_BASE ... fixed at 0x0000FC00 (1KB reserved to end of DCCM)
     *========================================================================*/

    /* Interrupt vector table: 256 entries x 4 bytes = 1024 bytes.
     * MUST be 1024-byte aligned for meivt CSR. */
    INTVEC_BASE = ORIGIN(dtcm) + LENGTH(dtcm) - 0x400;

    /* Stack pointer: just below vector table, word-aligned.
     * Stack grows DOWNWARD from here toward BSS_END. */
    STACK = INTVEC_BASE - 4;

    /* Aliases for compatibility */
    stack  = STACK;
    _stack = STACK;

    /* Sanity check: ensure BSS doesn't overlap with minimum stack space */
    ASSERT(BSS_END < (STACK - 0x1000), "BSS overlaps with minimum stack space!")
}
