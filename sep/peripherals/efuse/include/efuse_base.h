// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "efuse_register.h"
#include <tlm_utils/simple_target_socket.h>

/**
 * Combined SEP eFuse base module.
 *
 * Two csml_memory instances, each with its own target socket, because the block
 * occupies two disjoint windows in the register map rather than one contiguous
 * range.  `memory` holds the sep_efuse window:
 *   0x000–0x3FC  Shadow registers      (SEP_EFUSE_MAP)
 *   0x400–0x418  Interface control     (EFUSE_INTERFACE_CTRL)
 *   0x419–0x4FF  Gap — returns 0
 *   0x500–0x56C  Token MMR             (EFUSE_MMR)
 * for 0x570 bytes, matching the range efuse_interface_controller.sv decodes onto
 * its internal APB path.
 *
 * `shim_memory` holds EFUSE_SHIM_CTRL (0x000–0x003, one register), which the
 * address map places at 0x20000000 inside SEP_EXTERNAL.  Silicon reaches it
 * over a second AXI-Lite port on the same block (fuse_bank_ctrl_req_o), so two
 * sockets on one module is the faithful shape, not a VP convenience.
 *
 * Both base addresses are assigned by the VP integrator at PortMapping time.
 */
class efuse_base : public sc_module
{
public:
    typedef typename csml_reg<32>::DT DT;

    efuse_base(sc_module_name name, unsigned int memory_size, unsigned int shim_memory_size)
        : sc_module(name),
          memory(std::string(name) + ".Memory", memory_size / sizeof(unsigned int)),
          shim_memory(std::string(name) + ".ShimMemory", shim_memory_size / sizeof(unsigned int)),

          // ── Shadow registers ─────────────────────────────────────────────
          LOCKS_LO         (std::string(name) + ".LOCKS_LO",          memory, 0x000/4),
          LOCKS_HI         (std::string(name) + ".LOCKS_HI",          memory, 0x004/4),
          LC_STATE         (std::string(name) + ".LC_STATE",           memory, 0x00C/4),
          SBOOT_DIS        (std::string(name) + ".SBOOT_DIS",          memory, 0x010/4),
          TRANSIENT_RMA_EN (std::string(name) + ".TRANSIENT_RMA_EN",   memory, 0x014/4),
          SIP_DIS_LO       (std::string(name) + ".SIP_DIS_LO",         memory, 0x018/4),
          SIP_DIS_HI       (std::string(name) + ".SIP_DIS_HI",         memory, 0x01C/4),
          SYS_DIS_LO       (std::string(name) + ".SYS_DIS_LO",         memory, 0x020/4),
          SYS_DIS_HI       (std::string(name) + ".SYS_DIS_HI",         memory, 0x024/4),
          RMA_SIP_TOKEN    (std::string(name) + ".RMA_SIP_TOKEN",      memory, 0x028/4, 1),
          RMA_CHIPLET_TOKEN(std::string(name) + ".RMA_CHIPLET_TOKEN",  memory, 0x048/4, 1),
          CLASS_KEY        (std::string(name) + ".CLASS_KEY",           memory, 0x068/4, 1),
          CHIPLET_PUBK_REVOKE(std::string(name) + ".CHIPLET_PUBK_REVOKE", memory, 0x088/4),
          BL1_VERSION      (std::string(name) + ".BL1_VERSION",         memory, 0x08C/4, 1),
          BL2_VERSION      (std::string(name) + ".BL2_VERSION",         memory, 0x0AC/4, 1),
          CHIPLET_UID      (std::string(name) + ".CHIPLET_UID",         memory, 0x0CC/4, 1),
          SIP_PUBK_HASH0         (std::string(name) + ".SIP_PUBK_HASH0",            memory, 0x0EC/4, 1),
          SIP_UID          (std::string(name) + ".SIP_UID",             memory, 0x10C/4, 1),
          SYS_PUBK_HASH         (std::string(name) + ".SYS_PUBK_HASH",            memory, 0x12C/4, 1),
          SYS_UID          (std::string(name) + ".SYS_UID",             memory, 0x14C/4, 1),
          STATUS_RPT       (std::string(name) + ".STATUS_RPT",          memory, 0x16C/4),
          SEP_ROM_CTRL     (std::string(name) + ".SEP_ROM_CTRL",        memory, 0x170/4),
          SEP_SPI_CTRL_FIELD_EN(std::string(name) + ".SEP_SPI_CTRL_FIELD_EN", memory, 0x174/4),
          SPI_DISCOVERY_CTRL(std::string(name) + ".SPI_DISCOVERY_CTRL", memory, 0x178/4),
          SPI_PHY_DQ_TIMING (std::string(name) + ".SPI_PHY_DQ_TIMING",  memory, 0x17C/4),
          SPI_PHY_DQS_TIMING(std::string(name) + ".SPI_PHY_DQS_TIMING", memory, 0x180/4),
          SPI_PHY_GATE_LPBK (std::string(name) + ".SPI_PHY_GATE_LPBK",  memory, 0x184/4),
          SPI_PHY_DLL_SLAVE (std::string(name) + ".SPI_PHY_DLL_SLAVE",  memory, 0x188/4),
          SPI_PHY_DLL_MASTER(std::string(name) + ".SPI_PHY_DLL_MASTER", memory, 0x18C/4),
          SPI_PHY_MISC      (std::string(name) + ".SPI_PHY_MISC",        memory, 0x190/4),
          SPI_RB_VALID_TIME (std::string(name) + ".SPI_RB_VALID_TIME",   memory, 0x194/4),
          CHIPLET_PUBK_HASH0     (std::string(name) + ".CHIPLET_PUBK_HASH0", memory, 0x198/4, 1),
          CHIPLET_PUBK_HASH1     (std::string(name) + ".CHIPLET_PUBK_HASH1", memory, 0x1B8/4, 1),
          CHIPLET_PUBK_PQC_HASH0 (std::string(name) + ".CHIPLET_PUBK_PQC_HASH0", memory, 0x1E0/4, 1),
          CHIPLET_PUBK_PQC_HASH1 (std::string(name) + ".CHIPLET_PUBK_PQC_HASH1", memory, 0x200/4, 1),
          SIP_PUBK_PQC_HASH0     (std::string(name) + ".SIP_PUBK_PQC_HASH0", memory, 0x220/4, 1),
          SYS_PUBK_PQC_HASH      (std::string(name) + ".SYS_PUBK_PQC_HASH", memory, 0x240/4, 1),
          SIP_PUBK_HASH1         (std::string(name) + ".SIP_PUBK_HASH1", memory, 0x260/4, 1),
          SIP_PUBK_PQC_HASH1     (std::string(name) + ".SIP_PUBK_PQC_HASH1", memory, 0x280/4, 1),
          SEP_CHIPLET_ID         (std::string(name) + ".SEP_CHIPLET_ID", memory, 0x2A0/4, 1),
          SEP_SIP_ID             (std::string(name) + ".SEP_SIP_ID", memory, 0x2C0/4, 1),
          SEP_SYS_ID             (std::string(name) + ".SEP_SYS_ID", memory, 0x2E0/4, 1),
          SPARE0                 (std::string(name) + ".SPARE0", memory, 0x300/4, 1),
          SPARE1                 (std::string(name) + ".SPARE1", memory, 0x320/4, 1),
          SPARE2                 (std::string(name) + ".SPARE2", memory, 0x340/4, 1),
          SPARE3                 (std::string(name) + ".SPARE3", memory, 0x360/4, 1),
          SPARE4                 (std::string(name) + ".SPARE4", memory, 0x380/4, 1),
          SPARE5                 (std::string(name) + ".SPARE5", memory, 0x3A0/4, 1),
          SPARE6                 (std::string(name) + ".SPARE6", memory, 0x3C0/4, 1),
          SPARE7                 (std::string(name) + ".SPARE7", memory, 0x3E0/4, 1),
          REQUIRED_SIGNERS       (std::string(name) + ".REQUIRED_SIGNERS", memory, 0x1D8/4),
          REQUIRED_ALGS          (std::string(name) + ".REQUIRED_ALGS", memory, 0x1DC/4),

          // ── EFUSE_INTERFACE_CTRL ─────────────────────────────────────────
          EFUSE_INTERFACE_CTRL_STATUS    (std::string(name) + ".EFUSE_INTERFACE_CTRL_STATUS",     memory, 0x400/4),
          EFUSE_PROGRAM_CTRL             (std::string(name) + ".EFUSE_PROGRAM_CTRL",              memory, 0x404/4),
          EFUSE_READ_CTRL                (std::string(name) + ".EFUSE_READ_CTRL",                 memory, 0x408/4),
          EFUSE_PROGRAM_INTERFACE_RD_DATA(std::string(name) + ".EFUSE_PROGRAM_INTERFACE_RD_DATA", memory, 0x40C/4),
          EFUSE_READ_INTERFACE_RD_DATA   (std::string(name) + ".EFUSE_READ_INTERFACE_RD_DATA",    memory, 0x410/4),
          EFUSE_READ_REQ_TIMEOUT         (std::string(name) + ".EFUSE_READ_REQ_TIMEOUT",          memory, 0x414/4),
          EFUSE_PROGRAM_REQ_TIMEOUT      (std::string(name) + ".EFUSE_PROGRAM_REQ_TIMEOUT",       memory, 0x418/4),

          // ── EFUSE_MMR ────────────────────────────────────────────────────
          RMA_SIP_TOKEN_I    (std::string(name) + ".RMA_SIP_TOKEN_I",     memory, 0x500/4, 1),
          RMA_CHIPLET_TOKEN_I(std::string(name) + ".RMA_CHIPLET_TOKEN_I", memory, 0x520/4, 1),
          SEC_DISABLE_TOKEN_I(std::string(name) + ".SEC_DISABLE_TOKEN_I", memory, 0x540/4, 1),
          TOKEN_EOP              (std::string(name) + ".TOKEN_EOP",               memory, 0x560/4),
          RMA_SIP_TOKEN_MATCH    (std::string(name) + ".RMA_SIP_TOKEN_MATCH",     memory, 0x564/4),
          RMA_CHIPLET_TOKEN_MATCH(std::string(name) + ".RMA_CHIPLET_TOKEN_MATCH", memory, 0x568/4),
          SEC_DISABLE_TOKEN_MATCH(std::string(name) + ".SEC_DISABLE_TOKEN_MATCH", memory, 0x56C/4),

          // ── EFUSE_SHIM_CTRL (separate window; see class comment) ─────────
          EFUSE_BANK_INIT_TIME    (std::string(name) + ".EFUSE_BANK_INIT_TIME",     shim_memory, 0x000/4)
    {
        memory.bind_to_socket(target_socket);
        shim_memory.bind_to_socket(shim_target_socket);
    }

    csml_memory<32> memory;
    csml_memory<32> shim_memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> shim_target_socket;

    // Shadow registers
    sep_efuse::LOCKS_LO_type<32>          LOCKS_LO;
    sep_efuse::LOCKS_HI_type<32>          LOCKS_HI;
    sep_efuse::LC_STATE_type<32>          LC_STATE;
    sep_efuse::SBOOT_DIS_type<32>         SBOOT_DIS;
    sep_efuse::TRANSIENT_RMA_EN_type<32>  TRANSIENT_RMA_EN;
    sep_efuse::SIP_DIS_LO_type<32>        SIP_DIS_LO;
    sep_efuse::SIP_DIS_HI_type<32>        SIP_DIS_HI;
    sep_efuse::SYS_DIS_LO_type<32>        SYS_DIS_LO;
    sep_efuse::SYS_DIS_HI_type<32>        SYS_DIS_HI;
    csml_reg_vector<sep_efuse::RMA_SIP_TOKEN_type<32>,    8> RMA_SIP_TOKEN;
    csml_reg_vector<sep_efuse::RMA_CHIPLET_TOKEN_type<32>,8> RMA_CHIPLET_TOKEN;
    csml_reg_vector<sep_efuse::CLASS_KEY_type<32>,        8> CLASS_KEY;
    sep_efuse::CHIPLET_PUBK_REVOKE_type<32>               CHIPLET_PUBK_REVOKE;
    csml_reg_vector<sep_efuse::BL1_VERSION_type<32>,      8> BL1_VERSION;
    csml_reg_vector<sep_efuse::BL2_VERSION_type<32>,      8> BL2_VERSION;
    csml_reg_vector<sep_efuse::CHIPLET_UID_type<32>,      8> CHIPLET_UID;
    csml_reg_vector<sep_efuse::SIP_PUBK_type<32>,         8> SIP_PUBK_HASH0;
    csml_reg_vector<sep_efuse::SIP_UID_type<32>,          8> SIP_UID;
    csml_reg_vector<sep_efuse::SYS_PUBK_type<32>,         8> SYS_PUBK_HASH;
    csml_reg_vector<sep_efuse::SYS_UID_type<32>,          8> SYS_UID;
    sep_efuse::STATUS_RPT_type<32>            STATUS_RPT;
    sep_efuse::SEP_ROM_CTRL_type<32>          SEP_ROM_CTRL;
    sep_efuse::SEP_SPI_CTRL_FIELD_EN_type<32> SEP_SPI_CTRL_FIELD_EN;
    sep_efuse::ro_stub_type<32>               SPI_DISCOVERY_CTRL;
    sep_efuse::ro_stub_type<32>               SPI_PHY_DQ_TIMING;
    sep_efuse::ro_stub_type<32>               SPI_PHY_DQS_TIMING;
    sep_efuse::ro_stub_type<32>               SPI_PHY_GATE_LPBK;
    sep_efuse::ro_stub_type<32>               SPI_PHY_DLL_SLAVE;
    sep_efuse::ro_stub_type<32>               SPI_PHY_DLL_MASTER;
    sep_efuse::ro_stub_type<32>               SPI_PHY_MISC;
    sep_efuse::ro_stub_type<32>               SPI_RB_VALID_TIME;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  CHIPLET_PUBK_HASH0;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  CHIPLET_PUBK_HASH1;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  CHIPLET_PUBK_PQC_HASH0;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  CHIPLET_PUBK_PQC_HASH1;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SIP_PUBK_PQC_HASH0;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SYS_PUBK_PQC_HASH;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SIP_PUBK_HASH1;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SIP_PUBK_PQC_HASH1;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SEP_CHIPLET_ID;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SEP_SIP_ID;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SEP_SYS_ID;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE0;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE1;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE2;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE3;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE4;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE5;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE6;
    csml_reg_vector<sep_efuse::ro_stub_type<32>, 8>  SPARE7;
    sep_efuse::ro_stub_type<32>                     REQUIRED_SIGNERS;
    sep_efuse::ro_stub_type<32>                     REQUIRED_ALGS;

    // EFUSE_INTERFACE_CTRL registers
    sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_type<32>  EFUSE_INTERFACE_CTRL_STATUS;
    sep_efuse::EFUSE_PROGRAM_CTRL_type<32>           EFUSE_PROGRAM_CTRL;
    sep_efuse::EFUSE_READ_CTRL_type<32>              EFUSE_READ_CTRL;
    sep_efuse::EFUSE_READ_DATA_type<32>              EFUSE_PROGRAM_INTERFACE_RD_DATA;
    sep_efuse::EFUSE_READ_DATA_type<32>              EFUSE_READ_INTERFACE_RD_DATA;
    sep_efuse::EFUSE_READ_REQ_TIMEOUT_type<32>       EFUSE_READ_REQ_TIMEOUT;
    sep_efuse::EFUSE_PROGRAM_REQ_TIMEOUT_type<32>    EFUSE_PROGRAM_REQ_TIMEOUT;

    // EFUSE_MMR registers
    csml_reg_vector<sep_efuse::MMR_TOKEN_I_type<32>, 8> RMA_SIP_TOKEN_I;
    csml_reg_vector<sep_efuse::MMR_TOKEN_I_type<32>, 8> RMA_CHIPLET_TOKEN_I;
    csml_reg_vector<sep_efuse::MMR_TOKEN_I_type<32>, 8> SEC_DISABLE_TOKEN_I;
    sep_efuse::TOKEN_EOP_type<32>               TOKEN_EOP;
    sep_efuse::TOKEN_MATCH_type<32>             RMA_SIP_TOKEN_MATCH;
    sep_efuse::TOKEN_MATCH_type<32>             RMA_CHIPLET_TOKEN_MATCH;
    sep_efuse::TOKEN_MATCH_type<32>             SEC_DISABLE_TOKEN_MATCH;

    // EFUSE_SHIM_CTRL — one register, the OTP macro initialisation time
    sep_efuse::EFUSE_BANK_INIT_TIME_type<32>              EFUSE_BANK_INIT_TIME;

    void reset_all_registers();
};
