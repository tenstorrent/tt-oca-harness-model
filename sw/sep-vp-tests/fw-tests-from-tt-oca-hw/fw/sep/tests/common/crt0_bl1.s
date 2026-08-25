# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# BL1 startup code for flat-binary payloads loaded by rom_handoff_bl1().
#
# After rom_handoff copies the entire flat binary into ICCM:
#   1. Set trap handler & PMA regions
#   2. Set stack pointer
#   3. Copy .data from ICCM (LMA) to DCCM (VMA)
#   4. Zero .bss in DCCM
#   5. Call main()
#
# Copyright 2025 Tenstorrent Inc.

#include "defines.h"
#include "tb.h"

.section .text.init
.align 4
.global _start
_start:

    # Set trap handler
    la x1, _trap
    csrw mtvec, x1

    # Set all 256 MB PMA regions as side-effect regions except for TCM/PIC internal (0),
    # Boot ROM/SRAM (1) and memory-mapped SPI Flash (3)
    li t0, 0xAAAAAA64
    csrw 0x7c0, t0

    la sp, STACK

    # --- Copy .data from ICCM (LMA) to DCCM (VMA) ---
    la t0, __data_lma      # source:      LMA in ICCM (after .text)
    la t1, __data_start    # destination: VMA in DCCM
    la t2, __data_end
    bgeu t1, t2, .Ldata_done
.Ldata_copy:
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    bltu t1, t2, .Ldata_copy
.Ldata_done:

    # --- Zero .bss ---
    la t0, __bss_start
    la t1, __bss_end
    bgeu t0, t1, .Lbss_done
.Lbss_zero:
    sw zero, 0(t0)
    addi t0, t0, 4
    bltu t0, t1, .Lbss_zero
.Lbss_done:

    call main

    # Map exit code of main() to command to be written to tohost
    snez a0, a0
    bnez a0, _finish
    li   a0, 0xFF

.global _finish
_finish:

    li t0, STDOUT
    sb a0, 0(t0)  # DemoTB test termination
    .rept 10
    nop
    .endr

.align 4
_trap:

    li a0, 1 # failure
    j _finish

.global _exit
._exit:
    j _finish

.section .data.io
.global tohost
tohost: .word STDOUT
