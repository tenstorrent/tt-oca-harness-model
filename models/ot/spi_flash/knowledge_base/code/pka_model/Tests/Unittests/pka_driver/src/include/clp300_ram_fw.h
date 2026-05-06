//---------------------------------------------------------------------
//
//             (C) COPYRIGHT 2002 - 2019 SYNOPSYS, INC.
//                       ALL RIGHTS RESERVED
//
//  This software and the associated documentation are confidential and
//  proprietary to Synopsys, Inc.  Your use or disclosure of this
//  software is subject to the terms and conditions of a written
//  license agreement between you, or your company, and Synopsys, Inc.
//
//  The entire notice above must be reproduced on all authorized copies.
//
//---------------------------------------------------------------------
//
// Description:
//
//   ELP - Public Key Accelerator (aka CLUE)
//
//   CASM source entry point file for use with external H/W sequencer engines
//
// Created from:
//
//   Assembly Source file:     //dwh/security_ip_hw/dev/branches/pka/elp_pka_2_10_dev/hw/clue/src/clp300_ram_fw.casm
//   Assembly Source revision: #1
//
// Auto-created by:
//
//   Assembler:                
//   Assembler revision:       #3
//
//---------------------------------------------------------------------
//
// Language:         C header
// Type:             Macro Definitions
//
// Filename:         $Ide:  $
// Current Revision: $Revision:  $
// Last Updated:     $DateTime:  $
// Change:           $Change:  $
//
//---------------------------------------------------------------------
//
#ifndef ELP_CLUE_C_ENTRY_POINT_FILE
#define ELP_CLUE_C_ENTRY_POINT_FILE

#define ELP_CLUE_ENTRY_BIT_SERIAL_MOD                              0x14
#define ELP_CLUE_ENTRY_BIT_SERIAL_MOD_DP                           0x13
#define ELP_CLUE_ENTRY_C25519_PMULT                                0x2e
#define ELP_CLUE_ENTRY_CALC_MP                                     0x10
#define ELP_CLUE_ENTRY_CALC_R_INV                                  0x11
#define ELP_CLUE_ENTRY_CALC_R_SQR                                  0x12
#define ELP_CLUE_ENTRY_CED25519_MODEXP                             0x30
#define ELP_CLUE_ENTRY_CED25519_MODINV                             0x34
#define ELP_CLUE_ENTRY_CED25519_MODMULT                            0x31
#define ELP_CLUE_ENTRY_CED25519_MODSQR                             0x32
#define ELP_CLUE_ENTRY_CRT                                         0x18
#define ELP_CLUE_ENTRY_CRT_KEY_SETUP                               0x17
#define ELP_CLUE_ENTRY_ED25519_OBP_MONTMULT                        0x33
#define ELP_CLUE_ENTRY_ED25519_PADD                                0x2c
#define ELP_CLUE_ENTRY_ED25519_PDBL                                0x2d
#define ELP_CLUE_ENTRY_ED25519_PMULT                               0x2b
#define ELP_CLUE_ENTRY_ED25519_PVER                                0x2f
#define ELP_CLUE_ENTRY_ED25519_SHAMIR                              0x36
#define ELP_CLUE_ENTRY_ED25519_XRECOVER                            0x35
#define ELP_CLUE_ENTRY_IS_A_M3                                     0x22
#define ELP_CLUE_ENTRY_IS_P_EQUAL_Q                                0x20
#define ELP_CLUE_ENTRY_IS_P_REFLECT_Q                              0x21
#define ELP_CLUE_ENTRY_MODADD                                      0xb
#define ELP_CLUE_ENTRY_MODDIV                                      0xd
#define ELP_CLUE_ENTRY_MODEXP                                      0x16
#define ELP_CLUE_ENTRY_MODINV                                      0xe
#define ELP_CLUE_ENTRY_MODMULT                                     0xa
#define ELP_CLUE_ENTRY_MODMULT_521                                 0x29
#define ELP_CLUE_ENTRY_MODSUB                                      0xc
#define ELP_CLUE_ENTRY_MULT                                        0x15
#define ELP_CLUE_ENTRY_M_521_MONTMULT                              0x28
#define ELP_CLUE_ENTRY_PADD                                        0x1c
#define ELP_CLUE_ENTRY_PADD_521                                    0x26
#define ELP_CLUE_ENTRY_PADD_STD_PRJ                                0x1d
#define ELP_CLUE_ENTRY_PDBL                                        0x1a
#define ELP_CLUE_ENTRY_PDBL_521                                    0x25
#define ELP_CLUE_ENTRY_PDBL_STD_PRJ                                0x1b
#define ELP_CLUE_ENTRY_PMULT                                       0x19
#define ELP_CLUE_ENTRY_PMULT_521                                   0x24
#define ELP_CLUE_ENTRY_PVER                                        0x1e
#define ELP_CLUE_ENTRY_PVER_521                                    0x27
#define ELP_CLUE_ENTRY_REDUCE                                      0xf
#define ELP_CLUE_ENTRY_SHAMIR                                      0x23
#define ELP_CLUE_ENTRY_SHAMIR_521                                  0x2a
#define ELP_CLUE_ENTRY_STD_PRJ_TO_AFFINE                           0x1f

#endif // ELP_CLUE_C_ENTRY_POINT_FILE


