# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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
    #li a0, 1
    #sw a0, 0(t0) # Whisper test termination
    #beq x0, x0, _finish
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
