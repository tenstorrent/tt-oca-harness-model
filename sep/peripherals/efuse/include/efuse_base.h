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
          LOCKS_LO         (std::string(name) + ".LOCKS_LO",          memory, sep_efuse::LOCKS_LO_OFFSET/4),
          LOCKS_HI         (std::string(name) + ".LOCKS_HI",          memory, sep_efuse::LOCKS_HI_OFFSET/4),
          LC_STATE         (std::string(name) + ".LC_STATE",           memory, sep_efuse::LC_STATE_OFFSET/4),
          SBOOT_DIS        (std::string(name) + ".SBOOT_DIS",          memory, sep_efuse::SBOOT_DIS_OFFSET/4),
          TRANSIENT_RMA_EN (std::string(name) + ".TRANSIENT_RMA_EN",   memory, sep_efuse::TRANSIENT_RMA_EN_OFFSET/4),
          SIP_DIS_LO       (std::string(name) + ".SIP_DIS_LO",         memory, sep_efuse::SIP_DIS_LO_OFFSET/4),
          SIP_DIS_HI       (std::string(name) + ".SIP_DIS_HI",         memory, sep_efuse::SIP_DIS_HI_OFFSET/4),
          SYS_DIS_LO       (std::string(name) + ".SYS_DIS_LO",         memory, sep_efuse::SYS_DIS_LO_OFFSET/4),
          SYS_DIS_HI       (std::string(name) + ".SYS_DIS_HI",         memory, sep_efuse::SYS_DIS_HI_OFFSET/4),
          RMA_SIP_TOKEN    (std::string(name) + ".RMA_SIP_TOKEN",      memory, sep_efuse::RMA_SIP_TOKEN_OFFSET/4, 1),
          RMA_CHIPLET_TOKEN(std::string(name) + ".RMA_CHIPLET_TOKEN",  memory, sep_efuse::RMA_CHIPLET_TOKEN_OFFSET/4, 1),
          CLASS_KEY        (std::string(name) + ".CLASS_KEY",           memory, sep_efuse::CLASS_KEY_OFFSET/4, 1),
          CHIPLET_PUBK_REVOKE(std::string(name) + ".CHIPLET_PUBK_REVOKE", memory, sep_efuse::CHIPLET_PUBK_REVOKE_OFFSET/4),
          BL1_VERSION      (std::string(name) + ".BL1_VERSION",         memory, sep_efuse::BL1_VERSION_OFFSET/4, 1),
          BL2_VERSION      (std::string(name) + ".BL2_VERSION",         memory, sep_efuse::BL2_VERSION_OFFSET/4, 1),
          CHIPLET_UID      (std::string(name) + ".CHIPLET_UID",         memory, sep_efuse::CHIPLET_UID_OFFSET/4, 1),
          SIP_PUBK_HASH0         (std::string(name) + ".SIP_PUBK_HASH0",            memory, sep_efuse::SIP_PUBK_HASH0_OFFSET/4, 1),
          SIP_UID          (std::string(name) + ".SIP_UID",             memory, sep_efuse::SIP_UID_OFFSET/4, 1),
          SYS_PUBK_HASH         (std::string(name) + ".SYS_PUBK_HASH",            memory, sep_efuse::SYS_PUBK_HASH_OFFSET/4, 1),
          SYS_UID          (std::string(name) + ".SYS_UID",             memory, sep_efuse::SYS_UID_OFFSET/4, 1),
          STATUS_RPT       (std::string(name) + ".STATUS_RPT",          memory, sep_efuse::STATUS_RPT_OFFSET/4),
          SEP_ROM_CTRL     (std::string(name) + ".SEP_ROM_CTRL",        memory, sep_efuse::SEP_ROM_CTRL_OFFSET/4),
          SEP_SPI_CTRL_FIELD_EN(std::string(name) + ".SEP_SPI_CTRL_FIELD_EN", memory, sep_efuse::SEP_SPI_CTRL_FIELD_EN_OFFSET/4),
          SPI_DISCOVERY_CTRL(std::string(name) + ".SPI_DISCOVERY_CTRL", memory, sep_efuse::SPI_DISCOVERY_CTRL_OFFSET/4),
          SPI_PHY_DQ_TIMING (std::string(name) + ".SPI_PHY_DQ_TIMING",  memory, sep_efuse::SPI_PHY_DQ_TIMING_OFFSET/4),
          SPI_PHY_DQS_TIMING(std::string(name) + ".SPI_PHY_DQS_TIMING", memory, sep_efuse::SPI_PHY_DQS_TIMING_OFFSET/4),
          SPI_PHY_GATE_LPBK (std::string(name) + ".SPI_PHY_GATE_LPBK",  memory, sep_efuse::SPI_PHY_GATE_LPBK_OFFSET/4),
          SPI_PHY_DLL_SLAVE (std::string(name) + ".SPI_PHY_DLL_SLAVE",  memory, sep_efuse::SPI_PHY_DLL_SLAVE_OFFSET/4),
          SPI_PHY_DLL_MASTER(std::string(name) + ".SPI_PHY_DLL_MASTER", memory, sep_efuse::SPI_PHY_DLL_MASTER_OFFSET/4),
          SPI_PHY_MISC      (std::string(name) + ".SPI_PHY_MISC",        memory, sep_efuse::SPI_PHY_MISC_OFFSET/4),
          SPI_RB_VALID_TIME (std::string(name) + ".SPI_RB_VALID_TIME",   memory, sep_efuse::SPI_RB_VALID_TIME_OFFSET/4),
          CHIPLET_PUBK_HASH0     (std::string(name) + ".CHIPLET_PUBK_HASH0", memory, sep_efuse::CHIPLET_PUBK_HASH0_OFFSET/4, 1),
          CHIPLET_PUBK_HASH1     (std::string(name) + ".CHIPLET_PUBK_HASH1", memory, sep_efuse::CHIPLET_PUBK_HASH1_OFFSET/4, 1),
          CHIPLET_PUBK_PQC_HASH0 (std::string(name) + ".CHIPLET_PUBK_PQC_HASH0", memory, sep_efuse::CHIPLET_PUBK_PQC_HASH0_OFFSET/4, 1),
          CHIPLET_PUBK_PQC_HASH1 (std::string(name) + ".CHIPLET_PUBK_PQC_HASH1", memory, sep_efuse::CHIPLET_PUBK_PQC_HASH1_OFFSET/4, 1),
          SIP_PUBK_PQC_HASH0     (std::string(name) + ".SIP_PUBK_PQC_HASH0", memory, sep_efuse::SIP_PUBK_PQC_HASH0_OFFSET/4, 1),
          SYS_PUBK_PQC_HASH      (std::string(name) + ".SYS_PUBK_PQC_HASH", memory, sep_efuse::SYS_PUBK_PQC_HASH_OFFSET/4, 1),
          SIP_PUBK_HASH1         (std::string(name) + ".SIP_PUBK_HASH1", memory, sep_efuse::SIP_PUBK_HASH1_OFFSET/4, 1),
          SIP_PUBK_PQC_HASH1     (std::string(name) + ".SIP_PUBK_PQC_HASH1", memory, sep_efuse::SIP_PUBK_PQC_HASH1_OFFSET/4, 1),
          SEP_CHIPLET_ID         (std::string(name) + ".SEP_CHIPLET_ID", memory, sep_efuse::SEP_CHIPLET_ID_OFFSET/4, 1),
          SEP_SIP_ID             (std::string(name) + ".SEP_SIP_ID", memory, sep_efuse::SEP_SIP_ID_OFFSET/4, 1),
          SEP_SYS_ID             (std::string(name) + ".SEP_SYS_ID", memory, sep_efuse::SEP_SYS_ID_OFFSET/4, 1),
          SPARE0                 (std::string(name) + ".SPARE0", memory, sep_efuse::SPARE0_OFFSET/4, 1),
          SPARE1                 (std::string(name) + ".SPARE1", memory, sep_efuse::SPARE1_OFFSET/4, 1),
          SPARE2                 (std::string(name) + ".SPARE2", memory, sep_efuse::SPARE2_OFFSET/4, 1),
          SPARE3                 (std::string(name) + ".SPARE3", memory, sep_efuse::SPARE3_OFFSET/4, 1),
          SPARE4                 (std::string(name) + ".SPARE4", memory, sep_efuse::SPARE4_OFFSET/4, 1),
          SPARE5                 (std::string(name) + ".SPARE5", memory, sep_efuse::SPARE5_OFFSET/4, 1),
          SPARE6                 (std::string(name) + ".SPARE6", memory, sep_efuse::SPARE6_OFFSET/4, 1),
          SPARE7                 (std::string(name) + ".SPARE7", memory, sep_efuse::SPARE7_OFFSET/4, 1),
          REQUIRED_SIGNERS       (std::string(name) + ".REQUIRED_SIGNERS", memory, sep_efuse::REQUIRED_SIGNERS_OFFSET/4),
          REQUIRED_ALGS          (std::string(name) + ".REQUIRED_ALGS", memory, sep_efuse::REQUIRED_ALGS_OFFSET/4),

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
