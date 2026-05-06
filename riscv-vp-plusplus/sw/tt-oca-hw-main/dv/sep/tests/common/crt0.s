# SPDX-License-Identifier: Apache-2.0
# Copyright 2020 Western Digital Corporation or its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# startup code to support HLL programs

#include "defines.h"
#include "tb.h"

/* Machine Interrupts */
#define MSTATUS_MIE     (1 << 3)
#define MIE_MSIE        (1 << 3)
#define MIE_MTIE        (1 << 7)
#define MIE_MEIE        (1 << 11)
#define STDOUT 0x80000000 
#define CLINT_BASE     0x02000000
#define CLINT_MSIP     CLINT_BASE + 0x0000
#define CLINT_MTIMECMP CLINT_BASE + 0x4000
#define CLINT_MTIME    CLINT_BASE + 0xBFF8

.section .text.init
.align 4
.global _start
_start:

    # setup stack befpore enabling interrupts
    la sp, STACK

    ## Set trap handler
    la t0, _trap
    csrw mtvec, t0

    # Clear pending interrupts, may not work
    csrw mip, zero

    # Set all 256 MB PMA regions as side-effect regions except for TCM/PIC internal (0),
    # Boot ROM/SRAM (1) and memory-mapped SPI Flash (3)
    li t0, 0xAAAAAA64
    #csrw 0x7c0, t0

   /* Enable machine interrupts: SW, timer, external */
    li   t0, (MIE_MSIE | MIE_MTIE | MIE_MEIE)
    csrw mie, t0
    li   t0, MSTATUS_MIE
    csrs mstatus, t0

    call main

    # Map exit code of main() to command to be written to tohost
    snez a0, a0
    bnez a0, _finish
    li   a0, 0xFF

.global _finish
_finish:

    #la t0, tohost
    li t0, STDOUT
    sb a0, 0(t0)  # DemoTB test termination
    .rept 10
    nop
    .endr
1:
#infinite loop to terminate bare metal programs
    wfi
    j 1b

.align 4
_trap:
    li   t0, MSTATUS_MIE
    csrc mstatus, t0

    csrr t0, mcause

    # ---- MSIP ----
    li   t1, 0x80000003
    bne  t0, t1, 1f
    la   t0, handle_msip
    jr   t0

1:
    # ---- MTIP ----
    li   t1, 0x80000007
    bne  t0, t1, 2f
    la   t0, handle_mtip
    jr   t0

2:
    # ---- Exception ----
    la   t0, handle_exception
    jr   t0

    # return for debug purposes
    mret 
    # ---- Save registers ----
    addi sp, sp, -32
    sw   ra, 28(sp)
    sw   t0, 24(sp)
    sw   t1, 20(sp)
    sw   a0, 16(sp)
    sw   a1, 12(sp)

    # Print "MTVEC="
    li   t1, STDOUT
    li   a0, 'M' ; sb a0, 0(t1)
    li   a0, 'T' ; sb a0, 0(t1)
    li   a0, 'V' ; sb a0, 0(t1)
    li   a0, 'E' ; sb a0, 0(t1)
    li   a0, 'C' ; sb a0, 0(t1)
    li   a0, '=' ; sb a0, 0(t1)

    csrr a0, mtvec
    call print_hex32

    li   t1, STDOUT
    li   a0, '\n'
    sb   a0, 0(t1)



    # Print "MEPC="
    li   t1, STDOUT
    li   a0, 'M' ; sb a0, 0(t1)
    li   a0, 'E' ; sb a0, 0(t1)
    li   a0, 'P' ; sb a0, 0(t1)
    li   a0, 'C' ; sb a0, 0(t1)
    li   a0, '=' ; sb a0, 0(t1)

    csrr a0, mepc
    call print_hex32

    li   t1, STDOUT
    li   a0, '\n'
    sb   a0, 0(t1)
   
    lw   a1, 12(sp)
    lw   a0, 16(sp)
    lw   t1, 20(sp)
    lw   t0, 24(sp)
    lw   ra, 28(sp)
    addi sp, sp, 32

##1:
##    wfi
##    j 1b

.global _exit
_exit:
    j _finish

.section .data.io
.global tohost
tohost: .word STDOUT

handle_msip:
    li t0, 0x2000000   # CLINT MSIP
    sw zero, 0(t0)     # clear interrupt

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

handle_mtip:
    # update mtimecmp here
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

handle_exception:
    # Skip the instruction that caused the exception
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    mret

print_hex_nibble:
    addi sp, sp, -8
    sw   ra, 4(sp)
    sw   t0, 0(sp)

    li   t0, 10
    blt  a0, t0, 1f
    addi a0, a0, 55     # 'A' - 10
    j    2f
1:
    addi a0, a0, 48     # '0'
2:
    li   t0, STDOUT
    sb   a0, 0(t0)

    lw   t0, 0(sp)
    lw   ra, 4(sp)
    addi sp, sp, 8
    ret

print_hex32:
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   t2, 8(sp)
    sw   t3, 4(sp)
    sw   a0, 0(sp)      # save original value

    li   t2, 28

1:
    lw   a0, 0(sp)      # restore original value each time
    srl  t3, a0, t2
    andi a0, t3, 0xF
    call print_hex_nibble

    addi t2, t2, -4
    bgez t2, 1b

    lw   a0, 0(sp)
    lw   t3, 4(sp)
    lw   t2, 8(sp)
    lw   ra, 12(sp)
    addi sp, sp, 16
    ret

#==============================================================================
# NMI Handler Trampoline (256-byte aligned)
#
# VeeR EL2 NMI: nmi_vec[31:1] holds handler address; CPU jumps there on NMI.
# Saves all caller-saved registers, calls through _nmi_handler_ptr, then mret.
#==============================================================================
.section .nmi_handler, "ax"
.balign 256
.global _nmi_handler
_nmi_handler:
    addi    sp, sp, -68
    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      t3, 16(sp)
    sw      t4, 20(sp)
    sw      t5, 24(sp)
    sw      t6, 28(sp)
    sw      a0, 32(sp)
    sw      a1, 36(sp)
    sw      a2, 40(sp)
    sw      a3, 44(sp)
    sw      a4, 48(sp)
    sw      a5, 52(sp)
    sw      a6, 56(sp)
    sw      a7, 60(sp)

    la      t0, _nmi_handler_ptr
    lw      t0, 0(t0)
    jalr    ra, 0(t0)

    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      t3, 16(sp)
    lw      t4, 20(sp)
    lw      t5, 24(sp)
    lw      t6, 28(sp)
    lw      a0, 32(sp)
    lw      a1, 36(sp)
    lw      a2, 40(sp)
    lw      a3, 44(sp)
    lw      a4, 48(sp)
    lw      a5, 52(sp)
    lw      a6, 56(sp)
    lw      a7, 60(sp)
    addi    sp, sp, 68

    mret

.section .text
.global _default_nmi_handler
_default_nmi_handler:
    li      a0, 1
    j       _finish

.section .data
.global _nmi_handler_ptr
_nmi_handler_ptr:
    .word _default_nmi_handler

