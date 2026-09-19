// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
// Register offsets come from the model's own header rather than a copy kept
// here. 
//
// The READ/WRITE/RESET values below are genuine test data -- expected access
// masks and reset values -- and have no counterpart in the model header, so they
// stay.
#include "efuse_register.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class efuse_basetest : public sc_module
{
public:
    tlm_utils::simple_initiator_socket<efuse_basetest, 32> initiator_socket;
    // Second window: EFUSE_SHIM_CTRL, which the register map places in
    // SEP_EXTERNAL rather than next to the block's other registers.
    tlm_utils::simple_initiator_socket<efuse_basetest, 32> shim_initiator_socket;

    // -------------------------------------------------------------------------
    // Register offsets — byte addresses within the model's TLM windows
    // sep_efuse window (initiator_socket):
    //   Shadow registers:      0x000–0x3FC
    //   EFUSE_INTERFACE_CTRL:  0x400–0x418
    //   EFUSE_MMR:             0x500–0x56C
    // EFUSE_SHIM_CTRL window (shim_initiator_socket): 0x000–0x043.  Its offsets
    // restart at zero, so the SHIM_* names below repeat values used above.
    // -------------------------------------------------------------------------
    enum Register_offset
    {
        // EFUSE_SHIM_CTRL — offset into the shim window, not the one above
        EFUSE_BANK_INIT_TIME_OFFSET    = 0x000
    };

    enum Register_Read_Access
    {
        // Shadow (all fully readable)
        LOCKS_LO_READ              = 0xffffffff,
        LOCKS_HI_READ              = 0xffffffff,
        LC_STATE_READ              = 0xffffffff,
        SBOOT_DIS_READ             = 0xffffffff,
        TRANSIENT_RMA_EN_READ      = 0xffffffff,
        SIP_DIS_LO_READ            = 0xffffffff,
        SIP_DIS_HI_READ            = 0xffffffff,
        SYS_DIS_LO_READ            = 0xffffffff,
        SYS_DIS_HI_READ            = 0xffffffff,
        RMA_SIP_TOKEN_READ         = 0xffffffff,
        RMA_CHIPLET_TOKEN_READ     = 0xffffffff,
        CLASS_KEY_READ             = 0xffffffff,
        CHIPLET_PUBK_REVOKE_READ   = 0xffffffff,
        BL1_VERSION_READ           = 0xffffffff,
        BL2_VERSION_READ           = 0xffffffff,
        CHIPLET_UID_READ           = 0xffffffff,
        SIP_PUBK_HASH0_READ              = 0xffffffff,
        SIP_UID_READ               = 0xffffffff,
        SYS_PUBK_HASH_READ              = 0xffffffff,
        SYS_UID_READ               = 0xffffffff,
        STATUS_RPT_READ            = 0xffffffff,
        SEP_ROM_CTRL_READ          = 0xffffffff,
        SYSCLK_FREQ_MHZ_READ       = 0xffffffff,
        CHIPLET_PUBK_HASH0_READ        = 0xffffffff,
        CHIPLET_PUBK_HASH1_READ        = 0xffffffff,
        REQUIRED_SIGNERS_READ        = 0xffffffff,
        REQUIRED_ALGS_READ        = 0xffffffff,
        CHIPLET_PUBK_PQC_HASH0_READ        = 0xffffffff,
        CHIPLET_PUBK_PQC_HASH1_READ        = 0xffffffff,
        SIP_PUBK_PQC_HASH0_READ        = 0xffffffff,
        SYS_PUBK_PQC_HASH_READ        = 0xffffffff,
        SIP_PUBK_HASH1_READ        = 0xffffffff,
        SIP_PUBK_PQC_HASH1_READ        = 0xffffffff,
        SEP_CHIPLET_ID_READ        = 0xffffffff,
        SEP_SIP_ID_READ        = 0xffffffff,
        SEP_SYS_ID_READ        = 0xffffffff,
        SPARE0_READ        = 0xffffffff,
        SPARE1_READ        = 0xffffffff,
        SPARE2_READ        = 0xffffffff,
        SPARE3_READ        = 0xffffffff,
        SPARE4_READ        = 0xffffffff,
        SPARE5_READ        = 0xffffffff,
        SPARE6_READ        = 0xffffffff,
        SPARE7_READ        = 0xffffffff,
        // INTERFACE_CTRL
        EFUSE_INTF_STATUS_READ     = 0xffffffff,
        EFUSE_PROGRAM_CTRL_READ      = 0xffffffff,
        EFUSE_READ_CTRL_READ       = 0xffffffff,
        EFUSE_PROG_INTF_RD_DATA_READ = 0xffffffff,
        EFUSE_READ_INTF_RD_DATA_READ = 0xffffffff,
        EFUSE_READ_REQ_TIMEOUT_READ  = 0xffffffff,
        EFUSE_PROG_REQ_TIMEOUT_READ  = 0xffffffff,
        // EFUSE_MMR
        RMA_SIP_TOKEN_I_READ         = 0xffffffff,
        RMA_CHIPLET_TOKEN_I_READ     = 0xffffffff,
        SEC_DISABLE_TOKEN_I_READ     = 0xffffffff,
        TOKEN_EOP_READ               = 0x0,          // write-only
        RMA_SIP_TOKEN_MATCH_READ     = 0x3f,
        RMA_CHIPLET_TOKEN_MATCH_READ = 0x3f,
        SEC_DISABLE_TOKEN_MATCH_READ = 0x3f
    };

    enum Register_Write_Access
    {
        // Shadow — WOSET registers allow write (OR-accumulate via callback)
        LOCKS_LO_WRITE              = 0xffffffff,
        LOCKS_HI_WRITE              = 0xffffffff,
        LC_STATE_WRITE              = 0xffffffff,  // WOSET (write_mask=0xffff, callback enforces OR)
        SBOOT_DIS_WRITE             = 0x0,
        TRANSIENT_RMA_EN_WRITE      = 0x0,
        SIP_DIS_LO_WRITE            = 0xffffffff,
        SIP_DIS_HI_WRITE            = 0xffffffff,
        SYS_DIS_LO_WRITE            = 0xffffffff,
        SYS_DIS_HI_WRITE            = 0xffffffff,
        RMA_SIP_TOKEN_WRITE         = 0x0,
        RMA_CHIPLET_TOKEN_WRITE     = 0x0,
        CLASS_KEY_WRITE             = 0x0,
        CHIPLET_PUBK_REVOKE_WRITE   = 0xffffffff,  // WOSET
        BL1_VERSION_WRITE           = 0xffffffff,  // WOSET
        BL2_VERSION_WRITE           = 0xffffffff,  // WOSET
        CHIPLET_UID_WRITE           = 0x0,
        SIP_PUBK_HASH0_WRITE              = 0x0,
        SIP_UID_WRITE               = 0x0,
        SYS_PUBK_HASH_WRITE              = 0x0,
        SYS_UID_WRITE               = 0x0,
        STATUS_RPT_WRITE            = 0x0,
        SEP_ROM_CTRL_WRITE          = 0x0,
        SYSCLK_FREQ_MHZ_WRITE       = 0x0,
        CHIPLET_PUBK_HASH0_WRITE       = 0x0,
        CHIPLET_PUBK_HASH1_WRITE       = 0x0,
        REQUIRED_SIGNERS_WRITE       = 0x0,
        REQUIRED_ALGS_WRITE       = 0x0,
        CHIPLET_PUBK_PQC_HASH0_WRITE       = 0x0,
        CHIPLET_PUBK_PQC_HASH1_WRITE       = 0x0,
        SIP_PUBK_PQC_HASH0_WRITE       = 0x0,
        SYS_PUBK_PQC_HASH_WRITE       = 0x0,
        SIP_PUBK_HASH1_WRITE       = 0x0,
        SIP_PUBK_PQC_HASH1_WRITE       = 0x0,
        SEP_CHIPLET_ID_WRITE       = 0x0,
        SEP_SIP_ID_WRITE       = 0x0,
        SEP_SYS_ID_WRITE       = 0x0,
        SPARE0_WRITE       = 0x0,
        SPARE1_WRITE       = 0x0,
        SPARE2_WRITE       = 0x0,
        SPARE3_WRITE       = 0x0,
        SPARE4_WRITE       = 0x0,
        SPARE5_WRITE       = 0x0,
        SPARE6_WRITE       = 0x0,
        SPARE7_WRITE       = 0x0,
        // INTERFACE_CTRL
        EFUSE_INTF_STATUS_WRITE     = 0x0,          // RO: efuse_sense_done always 1
        EFUSE_PROGRAM_CTRL_WRITE      = 0xffffffff,
        EFUSE_READ_CTRL_WRITE       = 0xffffffff,
        EFUSE_PROG_INTF_RD_DATA_WRITE = 0x0,        // hw=w
        EFUSE_READ_INTF_RD_DATA_WRITE = 0x0,        // hw=w
        EFUSE_READ_REQ_TIMEOUT_WRITE  = 0xffffffff,
        EFUSE_PROG_REQ_TIMEOUT_WRITE  = 0xffffffff,
        // EFUSE_MMR
        RMA_SIP_TOKEN_I_WRITE         = 0xffffffff,
        RMA_CHIPLET_TOKEN_I_WRITE     = 0xffffffff,
        SEC_DISABLE_TOKEN_I_WRITE     = 0xffffffff,
        TOKEN_EOP_WRITE               = 0x00010101,  // singlepulse: bits [0], [8], [16]
        RMA_SIP_TOKEN_MATCH_WRITE     = 0x3f,        // R/W in VP for config preset
        RMA_CHIPLET_TOKEN_MATCH_WRITE = 0x3f,
        SEC_DISABLE_TOKEN_MATCH_WRITE = 0x3f
    };

    enum Register_Reset_Val
    {
        // Shadow
        LOCKS_LO_RESET              = 0x00000000,
        LOCKS_HI_RESET              = 0x00000000,
        LC_STATE_RESET              = 0x000000f0,
        SBOOT_DIS_RESET             = 0x00000000,
        TRANSIENT_RMA_EN_RESET      = 0x00000000,
        SIP_DIS_LO_RESET            = 0x00000000,
        SIP_DIS_HI_RESET            = 0x00000000,
        SYS_DIS_LO_RESET            = 0x00000000,
        SYS_DIS_HI_RESET            = 0x00000000,
        RMA_SIP_TOKEN_RESET         = 0x00000000,
        RMA_CHIPLET_TOKEN_RESET     = 0x00000000,
        CLASS_KEY_RESET             = 0x00000000,
        CHIPLET_PUBK_REVOKE_RESET   = 0x00000000,
        BL1_VERSION_RESET           = 0x00000000,
        BL2_VERSION_RESET           = 0x00000000,
        CHIPLET_UID_RESET           = 0x00000000,
        SIP_PUBK_HASH0_RESET              = 0x00000000,
        SIP_UID_RESET               = 0x00000000,
        SYS_PUBK_HASH_RESET              = 0x00000000,
        SYS_UID_RESET               = 0x00000000,
        STATUS_RPT_RESET            = 0x00000000,
        SEP_ROM_CTRL_RESET          = 0x00000000,
        SYSCLK_FREQ_MHZ_RESET       = 0x00000000,
        CHIPLET_PUBK_HASH0_RESET       = 0x00000000,
        CHIPLET_PUBK_HASH1_RESET       = 0x00000000,
        REQUIRED_SIGNERS_RESET       = 0x00000000,
        REQUIRED_ALGS_RESET       = 0x00000000,
        CHIPLET_PUBK_PQC_HASH0_RESET       = 0x00000000,
        CHIPLET_PUBK_PQC_HASH1_RESET       = 0x00000000,
        SIP_PUBK_PQC_HASH0_RESET       = 0x00000000,
        SYS_PUBK_PQC_HASH_RESET       = 0x00000000,
        SIP_PUBK_HASH1_RESET       = 0x00000000,
        SIP_PUBK_PQC_HASH1_RESET       = 0x00000000,
        SEP_CHIPLET_ID_RESET       = 0x00000000,
        SEP_SIP_ID_RESET       = 0x00000000,
        SEP_SYS_ID_RESET       = 0x00000000,
        SPARE0_RESET       = 0x00000000,
        SPARE1_RESET       = 0x00000000,
        SPARE2_RESET       = 0x00000000,
        SPARE3_RESET       = 0x00000000,
        SPARE4_RESET       = 0x00000000,
        SPARE5_RESET       = 0x00000000,
        SPARE6_RESET       = 0x00000000,
        SPARE7_RESET       = 0x00000000,
        // INTERFACE_CTRL
        EFUSE_INTF_STATUS_RESET     = 0x00000001,  // efuse_sense_done=1
        EFUSE_PROGRAM_CTRL_RESET      = 0x00000000,
        EFUSE_READ_CTRL_RESET       = 0x00000000,
        EFUSE_PROG_INTF_RD_DATA_RESET = 0x00000000,
        EFUSE_READ_INTF_RD_DATA_RESET = 0x00000000,
        EFUSE_READ_REQ_TIMEOUT_RESET  = 0x00800000,
        EFUSE_PROG_REQ_TIMEOUT_RESET  = 0x00800000,
        // EFUSE_MMR
        RMA_SIP_TOKEN_I_RESET         = 0x00000000,
        RMA_CHIPLET_TOKEN_I_RESET     = 0x00000000,
        SEC_DISABLE_TOKEN_I_RESET     = 0x00000000,
        TOKEN_EOP_RESET               = 0x00000000,
        RMA_SIP_TOKEN_MATCH_RESET     = 0x00000000,
        RMA_CHIPLET_TOKEN_MATCH_RESET = 0x00000000,
        SEC_DISABLE_TOKEN_MATCH_RESET = 0x00000000
    };

    struct Register_Property_t
    {
        unsigned int reg_offset;
        unsigned int read_mask;
        unsigned int write_mask;
        unsigned int reg_reset;
        std::string  reg_name;
    };

    efuse_basetest(sc_module_name name) : sc_module(name) {}
};
