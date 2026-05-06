/*
 * This Synopsys software and associated documentation (hereinafter the
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you. The
 * Software IS NOT an item of Licensed Software or a Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Products
 * with Synopsys or any supplement thereto. Synopsys is a registered trademark
 * of Synopsys, Inc. Other names included in the SOFTWARE may be the
 * trademarks of their respective owners.
 *
 * The contents of this file are dual-licensed; you may select either version
 * 2 of the GNU General Public License ("GPL") or the BSD-3-Clause license
 * ("BSD-3-Clause"). The GPL is included in the COPYING file accompanying the
 * SOFTWARE. The BSD License is copied below.
 *
 * BSD-3-Clause License:
 * Copyright (c) 2018 Synopsys, Inc. and/or its affiliates.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer, without
 *    modification.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of the above-listed copyright holders may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PKA_PKA_HW_H_
#define PKA_PKA_HW_H_

/* PKA general parameters */
#define PKA_MAX_OPERAND_SIZE    512 /* 4096 bits */
#define PKA_ECC521_OPERAND_SIZE  66 /* 528 bits */
#define PKA_OPERAND_BANK_SIZE 0x400

/* PKA register offsets */
#define PKA_REG_CTRL            0x00
#define PKA_REG_ENTRY_PNT       0x04
#define PKA_REG_RTN_CODE        0x08
#define PKA_REG_BUILD_CONFIG    0x0C
#define PKA_REG_STACK_PNTR      0x10
#define PKA_REG_INSTR_SINCE_GO  0x14
#define PKA_REG_CONFIG          0x1C
#define PKA_REG_STAT            0x20
#define PKA_REG_FLAGS           0x24
#define PKA_REG_WATCHDOG        0x28
#define PKA_REG_CYCLES_SINCE_GO 0x2C
#define PKA_REG_INDEX_I         0x30
#define PKA_REG_INDEX_J         0x34
#define PKA_REG_INDEX_K         0x38
#define PKA_REG_INDEX_L         0x3C
#define PKA_REG_IRQ_EN          0x40
#define PKA_REG_JMP_PROB        0x44
#define PKA_REG_JMP_PROB_LFSR   0x48
#define PKA_REG_BANK_SW_A       0x50
#define PKA_REG_BANK_SW_B       0x54
#define PKA_REG_BANK_SW_C       0x58
#define PKA_REG_BANK_SW_D       0x5C

/* PKA large data register base offsets */ 
#define PKA_REGION_A_BASE 0x0400
#define PKA_REGION_B_BASE 0x0800
#define PKA_REGION_C_BASE 0x0C00
#define PKA_REGION_D_BASE 0x1000

/* PKA Firmware register base offset */
#define PKA_FW_AREA 0x4000

/* PKA CTRL register bit fields */
#define PKA_CTRL_GO                31
#define PKA_CTRL_STOP_RQST         27
#define PKA_CTRL_M521_MODE         16
#define PKA_CTRL_M521_MODE_BITS     5
#define PKA_CTRL_BASE_RADIX         8
#define PKA_CTRL_BASE_RADIX_BITS    3
#define PKA_CTRL_PARTIAL_RADIX      0
#define PKA_CTRL_PARTIAL_RADIX_BITS 8
#define PKA_CTRL_M521_ECC521        9

enum size_CTRL_base_radix {
   base_radix_none=0, base_radix_1, base_radix_256, base_radix_512,
   base_radix_1024, base_radix_2048, base_radix_4096
};

/* PKA RTN_CODE register bit fields */
#define PKA_RC_BUSY          31
#define PKA_RC_IRQ           30
#define PKA_RC_WR_PENDING    29
#define PKA_RC_ZERO          28
#define PKA_RC_REASON        16
#define PKA_RC_REASON_BITS    8
#define PKA_RC_REASON_MASK   0xFF0000

/* PKA BUILD_CONFIG register bit fields */
#define PKA_BC_FMT_TYPE       30
#define PKA_BC_FMT_TYPE_BITS   2
#define PKA_BC_ALU_SZ         19
#define PKA_BC_ALU_SZ_MASK    0x180000
#define PKA_BC_ALU_SZ_BITS     2
#define PKA_BC_RSA_SZ         16
#define PKA_BC_RSA_SZ_MASK    0x70000
#define PKA_BC_RSA_SZ_BITS     3
#define PKA_BC_ECC_SZ         14
#define PKA_BC_ECC_SZ_MASK    0xC000
#define PKA_BC_ECC_SZ_BITS     2
#define PKA_BC_FW_ROM_SZ      11
#define PKA_BC_FW_ROM_SZ_BITS  3
#define PKA_BC_FW_RAM_SZ       8
#define PKA_BC_FW_RAM_SZ_BITS  3
#define PKA_BC_BANK_SW_D       6
#define PKA_BC_BANK_SW_D_BITS  2
#define PKA_BC_BANK_SW_C       4
#define PKA_BC_BANK_SW_C_BITS  2
#define PKA_BC_BANK_SW_B       2
#define PKA_BC_BANK_SW_B_BITS  2
#define PKA_BC_BANK_SW_A       0
#define PKA_BC_BANK_SW_A_BITS  2

enum size_pka_rsa_bld_cfg {
   RSA_none=0, RSA_512, RSA_1024, RSA_2048, RSA_4096
};
enum size_pka_ecc_bld_cfg {
   ECC_none=0, ECC_256, ECC_512, ECC_1024
};

enum pka_alu_width_bld_cfg {
   ALU_32_BITS=0, ALU_64_BITS, ALU_128_BITS, ALU_RESERVED
};

/* PKA STAT and IRQ_EN bits */
#define PKA_STAT_DONE   30
#define PKA_IRQ_EN_STAT 30

/* PKA FLAGS register bit fields */
#define PKA_FLAG_ZERO   0
#define PKA_FLAG_MEMBIT 1
#define PKA_FLAG_BORROW 2
#define PKA_FLAG_CARRY  3
#define PKA_FLAG_F0     4
#define PKA_FLAG_F1     5
#define PKA_FLAG_F2     6
#define PKA_FLAG_F3     7

/* PKA CONFIG register bit fields */
#define PKA_CONF_BYTESWAP 26

/* PKA JMP_PROB register bit fields */
#define PKA_DTA_JUMP_PROBABILITY       0
#define PKA_DTA_JUMP_PROBABILITY_BITS 13

/* PKA Operand Register Type */
enum pka_op_reg_type {
   A0=0, A1, A2, A3, A4, A5, A6, A7,
   B0, B1, B2, B3, B4, B5, B6, B7,
   C0, C1, C2, C3, C4, C5, C6, C7,
   D0, D1, D2, D3, D4, D5, D6, D7
};

#endif
