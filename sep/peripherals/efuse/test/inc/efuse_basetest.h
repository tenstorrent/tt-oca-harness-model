// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
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
        // Shadow
        LOCKS_LO_OFFSET              = 0x000,
        LOCKS_HI_OFFSET              = 0x004,
        LC_STATE_OFFSET              = 0x008,
        SBOOT_DIS_OFFSET             = 0x00C,
        TRANSIENT_RMA_EN_OFFSET      = 0x010,
        SIP_DIS_LO_OFFSET            = 0x014,
        SIP_DIS_HI_OFFSET            = 0x018,
        SYS_DIS_LO_OFFSET            = 0x01C,
        SYS_DIS_HI_OFFSET            = 0x020,
        RMA_SIP_TOKEN_OFFSET         = 0x024,
        RMA_CHIPLET_TOKEN_OFFSET     = 0x044,
        CLASS_KEY_OFFSET             = 0x064,
        CHIPLET_PUBK_REVOKE_OFFSET   = 0x084,
        BL1_VERSION_OFFSET           = 0x088,
        BL2_VERSION_OFFSET           = 0x0A8,
        CHIPLET_UID_OFFSET           = 0x0C8,
        SIP_PUBK_OFFSET              = 0x0E8,
        SIP_UID_OFFSET               = 0x108,
        SYS_PUBK_OFFSET              = 0x128,
        SYS_UID_OFFSET               = 0x148,
        STATUS_RPT_OFFSET            = 0x168,
        SEP_ROM_CTRL_OFFSET          = 0x16C,
        SEP_SPI_CTRL_FIELD_EN_OFFSET = 0x170,
        SPI_DISCOVERY_CTRL_OFFSET    = 0x174,
        SPI_PHY_DQ_TIMING_OFFSET     = 0x178,
        SPI_PHY_DQS_TIMING_OFFSET    = 0x17C,
        SPI_PHY_GATE_LPBK_OFFSET     = 0x180,
        SPI_PHY_DLL_SLAVE_OFFSET     = 0x184,
        SPI_PHY_DLL_MASTER_OFFSET    = 0x188,
        SPI_PHY_MISC_OFFSET          = 0x18C,
        SPI_RB_VALID_TIME_OFFSET     = 0x190,
        PUBLIC_KEY_0_OFFSET          = 0x194,
        PUBLIC_KEY_1_OFFSET          = 0x1B4,
        RESERVED_0_OFFSET            = 0x1D4,
        RESERVED_1_OFFSET            = 0x214,
        RESERVED_2_OFFSET            = 0x254,
        RESERVED_3_OFFSET            = 0x294,
        RESERVED_4_OFFSET            = 0x2D4,
        RESERVED_5_OFFSET            = 0x314,
        RESERVED_6_OFFSET            = 0x354,
        RESERVED_7_OFFSET            = 0x394,
        RESERVED_LAST_256_OFFSET     = 0x3D4,
        RESERVED_LAST_64_LO_OFFSET   = 0x3F4,
        RESERVED_LAST_64_HI_OFFSET   = 0x3F8,
        RESERVED_LAST_32_OFFSET      = 0x3FC,
        // EFUSE_INTERFACE_CTRL
        EFUSE_INTF_STATUS_OFFSET     = 0x400,
        EFUSE_PROGRAM_CTRL_OFFSET      = 0x404,
        EFUSE_READ_CTRL_OFFSET       = 0x408,
        EFUSE_PROG_INTF_RD_DATA_OFFSET = 0x40C,
        EFUSE_READ_INTF_RD_DATA_OFFSET = 0x410,
        EFUSE_READ_REQ_TIMEOUT_OFFSET  = 0x414,
        EFUSE_PROG_REQ_TIMEOUT_OFFSET  = 0x418,
        // EFUSE_MMR
        RMA_SIP_TOKEN_I_OFFSET         = 0x500,
        RMA_CHIPLET_TOKEN_I_OFFSET     = 0x520,
        SEC_DISABLE_TOKEN_I_OFFSET     = 0x540,
        TOKEN_EOP_OFFSET               = 0x560,
        RMA_SIP_TOKEN_MATCH_OFFSET     = 0x564,
        RMA_CHIPLET_TOKEN_MATCH_OFFSET = 0x568,
        SEC_DISABLE_TOKEN_MATCH_OFFSET = 0x56C,
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
        SIP_PUBK_READ              = 0xffffffff,
        SIP_UID_READ               = 0xffffffff,
        SYS_PUBK_READ              = 0xffffffff,
        SYS_UID_READ               = 0xffffffff,
        STATUS_RPT_READ            = 0xffffffff,
        SEP_ROM_CTRL_READ          = 0xffffffff,
        SEP_SPI_CTRL_FIELD_EN_READ = 0xffffffff,
        SPI_DISCOVERY_CTRL_READ    = 0xffffffff,
        SPI_PHY_DQ_TIMING_READ     = 0xffffffff,
        SPI_PHY_DQS_TIMING_READ    = 0xffffffff,
        SPI_PHY_GATE_LPBK_READ     = 0xffffffff,
        SPI_PHY_DLL_SLAVE_READ     = 0xffffffff,
        SPI_PHY_DLL_MASTER_READ    = 0xffffffff,
        SPI_PHY_MISC_READ          = 0xffffffff,
        SPI_RB_VALID_TIME_READ     = 0xffffffff,
        PUBLIC_KEY_0_READ          = 0xffffffff,
        PUBLIC_KEY_1_READ          = 0xffffffff,
        RESERVED_0_READ            = 0xffffffff,
        RESERVED_1_READ            = 0xffffffff,
        RESERVED_2_READ            = 0xffffffff,
        RESERVED_3_READ            = 0xffffffff,
        RESERVED_4_READ            = 0xffffffff,
        RESERVED_5_READ            = 0xffffffff,
        RESERVED_6_READ            = 0xffffffff,
        RESERVED_7_READ            = 0xffffffff,
        RESERVED_LAST_256_READ     = 0xffffffff,
        RESERVED_LAST_64_LO_READ   = 0xffffffff,
        RESERVED_LAST_64_HI_READ   = 0xffffffff,
        RESERVED_LAST_32_READ      = 0xffffffff,
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
        SIP_PUBK_WRITE              = 0x0,
        SIP_UID_WRITE               = 0x0,
        SYS_PUBK_WRITE              = 0x0,
        SYS_UID_WRITE               = 0x0,
        STATUS_RPT_WRITE            = 0x0,
        SEP_ROM_CTRL_WRITE          = 0x0,
        SEP_SPI_CTRL_FIELD_EN_WRITE = 0x0,
        SPI_DISCOVERY_CTRL_WRITE    = 0x0,
        SPI_PHY_DQ_TIMING_WRITE     = 0x0,
        SPI_PHY_DQS_TIMING_WRITE    = 0x0,
        SPI_PHY_GATE_LPBK_WRITE     = 0x0,
        SPI_PHY_DLL_SLAVE_WRITE     = 0x0,
        SPI_PHY_DLL_MASTER_WRITE    = 0x0,
        SPI_PHY_MISC_WRITE          = 0x0,
        SPI_RB_VALID_TIME_WRITE     = 0x0,
        PUBLIC_KEY_0_WRITE          = 0x0,
        PUBLIC_KEY_1_WRITE          = 0x0,
        RESERVED_0_WRITE            = 0x0,
        RESERVED_1_WRITE            = 0x0,
        RESERVED_2_WRITE            = 0x0,
        RESERVED_3_WRITE            = 0x0,
        RESERVED_4_WRITE            = 0x0,
        RESERVED_5_WRITE            = 0x0,
        RESERVED_6_WRITE            = 0x0,
        RESERVED_7_WRITE            = 0x0,
        RESERVED_LAST_256_WRITE     = 0x0,
        RESERVED_LAST_64_LO_WRITE   = 0x0,
        RESERVED_LAST_64_HI_WRITE   = 0x0,
        RESERVED_LAST_32_WRITE      = 0x0,
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
        SIP_PUBK_RESET              = 0x00000000,
        SIP_UID_RESET               = 0x00000000,
        SYS_PUBK_RESET              = 0x00000000,
        SYS_UID_RESET               = 0x00000000,
        STATUS_RPT_RESET            = 0x00000000,
        SEP_ROM_CTRL_RESET          = 0x00000000,
        SEP_SPI_CTRL_FIELD_EN_RESET = 0x00000000,
        SPI_DISCOVERY_CTRL_RESET    = 0x00000000,
        SPI_PHY_DQ_TIMING_RESET     = 0x00000000,
        SPI_PHY_DQS_TIMING_RESET    = 0x00000000,
        SPI_PHY_GATE_LPBK_RESET     = 0x00000000,
        SPI_PHY_DLL_SLAVE_RESET     = 0x00000000,
        SPI_PHY_DLL_MASTER_RESET    = 0x00000000,
        SPI_PHY_MISC_RESET          = 0x00000000,
        SPI_RB_VALID_TIME_RESET     = 0x00000000,
        PUBLIC_KEY_0_RESET          = 0x00000000,
        PUBLIC_KEY_1_RESET          = 0x00000000,
        RESERVED_0_RESET            = 0x00000000,
        RESERVED_1_RESET            = 0x00000000,
        RESERVED_2_RESET            = 0x00000000,
        RESERVED_3_RESET            = 0x00000000,
        RESERVED_4_RESET            = 0x00000000,
        RESERVED_5_RESET            = 0x00000000,
        RESERVED_6_RESET            = 0x00000000,
        RESERVED_7_RESET            = 0x00000000,
        RESERVED_LAST_256_RESET     = 0x00000000,
        RESERVED_LAST_64_LO_RESET   = 0x00000000,
        RESERVED_LAST_64_HI_RESET   = 0x00000000,
        RESERVED_LAST_32_RESET      = 0x00000000,
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
