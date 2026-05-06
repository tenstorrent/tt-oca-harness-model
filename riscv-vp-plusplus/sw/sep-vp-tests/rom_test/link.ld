/* Linker script for SEP platform with unified memory */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ROM   (rx)  : ORIGIN = 0x10040000, LENGTH = 64K
    SRAM  (rwx) : ORIGIN = 0x10000000, LENGTH = 256K
}

SECTIONS
{
    .text : {
        *(.text.init)
        *(.text*)
    } > SRAM

    .rodata : {
        *(.rodata*)
    } > SRAM

    .data : {
        *(.data*)
    } > SRAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > SRAM

    . = ALIGN(16);
    _stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
