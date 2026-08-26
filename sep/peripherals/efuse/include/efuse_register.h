// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file efuse_register.h
 * @brief SEP eFuse OTP Controller — register type definitions (CSML format)
 *
 * Three MMIO register spaces (physical base 0x10930000):
 *   SEP_EFUSE_MAP          0x000–0x3FC  Shadow registers (fuse content)
 *   EFUSE_INTERFACE_CTRL   0x400–0x418  Raw OTP interface control
 *   EFUSE_MMR              0x500–0x56C  Token input / match result registers
 *
 * Register offsets confirmed from:
 *   knowledge-base/efuse/efuse/efuse_map.rdl
 *   knowledge-base/efuse/data/registers/rdl/efuse_interface_ctrl.rdl
 *   knowledge-base/efuse/data/registers/rdl/efuse_mmr.rdl
 * Base address deferred to VP integration time.
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "csml_register.h"

namespace sep_efuse {

// ============================================================================
// SEP_EFUSE_MAP register offsets  (physical base 0x10930000)
// ============================================================================
static constexpr unsigned int LOCKS_LO_OFFSET               = 0x000;
static constexpr unsigned int LOCKS_HI_OFFSET               = 0x004;
// LOCKS_SPARE (RDL @0x008, 32-bit). Not modelled as a register -- nothing reads
// it -- but the word MUST be reserved: omitting it is what previously placed
// every register from LC_STATE onward 4 bytes below the RDL, so the ROM reading
// CLASS_KEY at its real 0x068 got this model's word 1.
static constexpr unsigned int LOCKS_SPARE_OFFSET            = 0x008;
static constexpr unsigned int LC_STATE_OFFSET               = 0x00C;
static constexpr unsigned int SBOOT_DIS_OFFSET              = 0x010;
static constexpr unsigned int TRANSIENT_RMA_EN_OFFSET       = 0x014;
static constexpr unsigned int SIP_DIS_LO_OFFSET             = 0x018;
static constexpr unsigned int SIP_DIS_HI_OFFSET             = 0x01C;
static constexpr unsigned int SYS_DIS_LO_OFFSET             = 0x020;
static constexpr unsigned int SYS_DIS_HI_OFFSET             = 0x024;
static constexpr unsigned int RMA_SIP_TOKEN_OFFSET          = 0x028; ///< [8] × 4 bytes each
static constexpr unsigned int RMA_CHIPLET_TOKEN_OFFSET      = 0x048; ///< [8] × 4 bytes each
static constexpr unsigned int CLASS_KEY_OFFSET              = 0x068; ///< [8] × 4 bytes each
static constexpr unsigned int CHIPLET_PUBK_REVOKE_OFFSET    = 0x088;
static constexpr unsigned int BL1_VERSION_OFFSET            = 0x08C; ///< [8] × 4 bytes each
static constexpr unsigned int BL2_VERSION_OFFSET            = 0x0AC; ///< [8] × 4 bytes each
static constexpr unsigned int CHIPLET_UID_OFFSET            = 0x0CC; ///< [8] × 4 bytes each
static constexpr unsigned int SIP_PUBK_OFFSET               = 0x0EC; ///< [8] × 4 bytes each
static constexpr unsigned int SIP_UID_OFFSET                = 0x10C; ///< [8] × 4 bytes each
static constexpr unsigned int SYS_PUBK_OFFSET               = 0x12C; ///< [8] × 4 bytes each
static constexpr unsigned int SYS_UID_OFFSET                = 0x14C; ///< [8] × 4 bytes each
static constexpr unsigned int STATUS_RPT_OFFSET             = 0x16C;
static constexpr unsigned int SEP_ROM_CTRL_OFFSET           = 0x170;
static constexpr unsigned int SEP_SPI_CTRL_FIELD_EN_OFFSET  = 0x174;
static constexpr unsigned int SPI_DISCOVERY_CTRL_OFFSET     = 0x178;
static constexpr unsigned int SPI_PHY_DQ_TIMING_OFFSET      = 0x17C;
static constexpr unsigned int SPI_PHY_DQS_TIMING_OFFSET     = 0x180;
static constexpr unsigned int SPI_PHY_GATE_LPBK_OFFSET      = 0x184;
static constexpr unsigned int SPI_PHY_DLL_SLAVE_OFFSET      = 0x188;
static constexpr unsigned int SPI_PHY_DLL_MASTER_OFFSET     = 0x18C;
static constexpr unsigned int SPI_PHY_MISC_OFFSET           = 0x190;
static constexpr unsigned int SPI_RB_VALID_TIME_OFFSET      = 0x194;
static constexpr unsigned int PUBLIC_KEY_0_OFFSET           = 0x198; ///< [8] × 4 bytes each
static constexpr unsigned int PUBLIC_KEY_1_OFFSET           = 0x1B8; ///< [8] × 4 bytes each
static constexpr unsigned int RESERVED_0_OFFSET             = 0x1D8; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_1_OFFSET             = 0x218; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_2_OFFSET             = 0x258; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_3_OFFSET             = 0x298; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_4_OFFSET             = 0x2D8; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_5_OFFSET             = 0x318; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_6_OFFSET             = 0x358; ///< [16] × 4 bytes each
static constexpr unsigned int RESERVED_7_OFFSET             = 0x398; ///< [15] × 4 bytes each (absorbs the LOCKS_SPARE word)
static constexpr unsigned int RESERVED_LAST_256_OFFSET      = 0x3D4; ///< [8] × 4 bytes each
static constexpr unsigned int RESERVED_LAST_64_LO_OFFSET    = 0x3F4;
static constexpr unsigned int RESERVED_LAST_64_HI_OFFSET    = 0x3F8;
static constexpr unsigned int RESERVED_LAST_32_OFFSET       = 0x3FC;

// ============================================================================
// EFUSE_INTERFACE_CTRL register offsets
// ============================================================================
static constexpr unsigned int EFUSE_INTERFACE_CTRL_STATUS_OFFSET      = 0x400;
static constexpr unsigned int EFUSE_PROGRAM_CTRL_OFFSET               = 0x404;
static constexpr unsigned int EFUSE_READ_CTRL_OFFSET                  = 0x408;
static constexpr unsigned int EFUSE_PROGRAM_INTERFACE_RD_DATA_OFFSET  = 0x40C;
static constexpr unsigned int EFUSE_READ_INTERFACE_RD_DATA_OFFSET     = 0x410;
static constexpr unsigned int EFUSE_READ_REQ_TIMEOUT_OFFSET           = 0x414;
static constexpr unsigned int EFUSE_PROGRAM_REQ_TIMEOUT_OFFSET        = 0x418;

// ============================================================================
// EFUSE_MMR register offsets
// ============================================================================
static constexpr unsigned int RMA_SIP_TOKEN_I_OFFSET          = 0x500; ///< [8] × 4 bytes each
static constexpr unsigned int RMA_CHIPLET_TOKEN_I_OFFSET      = 0x520; ///< [8] × 4 bytes each
static constexpr unsigned int SEC_DISABLE_TOKEN_I_OFFSET      = 0x540; ///< [8] × 4 bytes each
static constexpr unsigned int TOKEN_EOP_OFFSET                = 0x560;
static constexpr unsigned int RMA_SIP_TOKEN_MATCH_OFFSET      = 0x564;
static constexpr unsigned int RMA_CHIPLET_TOKEN_MATCH_OFFSET  = 0x568;
static constexpr unsigned int SEC_DISABLE_TOKEN_MATCH_OFFSET  = 0x56C;

// ============================================================================
// Window sizes
//
// The eFuse peripheral answers on two disjoint address windows, as it does in
// silicon: efuse_interface_controller.sv decodes
// [EFUSE_MAP_REG_MAP_BASE_ADDR : EFUSE_MMR_REG_MAP_END_ADDR] onto its internal
// APB path and routes everything else on its slave to the shim's own AXI-Lite
// port (fuse_bank_ctrl_req_o).  The two are separate windows in the register
// map, not neighbours in one block: the shim lives in SEP_EXTERNAL while the
// map, interface CSRs and MMR stay in the sep_efuse window.
// ============================================================================
static constexpr unsigned int EFUSE_WINDOW_SIZE      = 0x570; ///< map + interface ctrl + MMR
static constexpr unsigned int SHIM_CTRL_WINDOW_SIZE  = 0x004; ///< SEP_EXTERNAL_EFUSE_SHIM_CTRL_REG_MAP_SIZE

// ============================================================================
// EFUSE_SHIM_CTRL register offsets
//
// Offsets are relative to the shim's own window base, which the address map
// places at SEP_EXTERNAL_EFUSE_SHIM_CTRL_BASE_ADDR = 0x20000000 — not inside
// the sep_efuse window.
//
// One register, not a block.  Earlier silicon exposed a Samsung physical-layer
// shim here: two status registers and fifteen timing controls, 0x44 in all.
// That is gone.  efuse_shim_ctrl.rdl now declares a single EFUSE_BANK_INIT_TIME
// and sep_addrmap_pkg.sv gives the window a size of 0x4, so the old registers
// are not merely unused — they do not decode.
// ============================================================================
static constexpr unsigned int EFUSE_BANK_INIT_TIME_OFFSET = 0x000;


// ============================================================================
// SEP_EFUSE_MAP REGISTER TYPES
// ============================================================================

/**
 * LOCKS_LO — Write-Lock / Read-Lock pairs for the lower shadow register set
 *
 * Each pair of bits gates access to one shadow register group. Bits are
 * write-set-only; once set they cannot be cleared (fuse-backed semantics).
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback); reads return accumulated value
 * Reset  : 0x00000000
 */
template<unsigned int N>
class LOCKS_LO_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    LOCKS_LO_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        LC_STATE_WRITE_LOCK        (reg_name + ".LC_STATE_WRITE_LOCK",         *this,  0, 1),
        LC_STATE_READ_LOCK         (reg_name + ".LC_STATE_READ_LOCK",          *this,  1, 1),
        SBOOT_DIS_WRITE_LOCK       (reg_name + ".SBOOT_DIS_WRITE_LOCK",        *this,  2, 1),
        SBOOT_DIS_READ_LOCK        (reg_name + ".SBOOT_DIS_READ_LOCK",         *this,  3, 1),
        TRANSIENT_RMA_EN_WRITE_LOCK(reg_name + ".TRANSIENT_RMA_EN_WRITE_LOCK", *this,  4, 1),
        TRANSIENT_RMA_EN_READ_LOCK (reg_name + ".TRANSIENT_RMA_EN_READ_LOCK",  *this,  5, 1),
        SIP_DIS_WRITE_LOCK         (reg_name + ".SIP_DIS_WRITE_LOCK",          *this,  6, 1),
        SIP_DIS_READ_LOCK          (reg_name + ".SIP_DIS_READ_LOCK",           *this,  7, 1),
        SYS_DIS_WRITE_LOCK         (reg_name + ".SYS_DIS_WRITE_LOCK",          *this,  8, 1),
        SYS_DIS_READ_LOCK          (reg_name + ".SYS_DIS_READ_LOCK",           *this,  9, 1),
        RMA_SIP_TOKEN_WRITE_LOCK   (reg_name + ".RMA_SIP_TOKEN_WRITE_LOCK",    *this, 10, 1),
        RMA_SIP_TOKEN_READ_LOCK    (reg_name + ".RMA_SIP_TOKEN_READ_LOCK",     *this, 11, 1),
        RMA_CHIPLET_TOKEN_WRITE_LOCK(reg_name + ".RMA_CHIPLET_TOKEN_WRITE_LOCK",*this, 12, 1),
        RMA_CHIPLET_TOKEN_READ_LOCK (reg_name + ".RMA_CHIPLET_TOKEN_READ_LOCK", *this, 13, 1),
        CLASS_KEY_WRITE_LOCK       (reg_name + ".CLASS_KEY_WRITE_LOCK",         *this, 14, 1),
        CLASS_KEY_READ_LOCK        (reg_name + ".CLASS_KEY_READ_LOCK",          *this, 15, 1),
        CHIPLET_PUBK_SEL_WRITE_LOCK(reg_name + ".CHIPLET_PUBK_SEL_WRITE_LOCK", *this, 16, 1),
        CHIPLET_PUBK_SEL_READ_LOCK (reg_name + ".CHIPLET_PUBK_SEL_READ_LOCK",  *this, 17, 1),
        BL1_VERSION_WRITE_LOCK     (reg_name + ".BL1_VERSION_WRITE_LOCK",       *this, 18, 1),
        BL1_VERSION_READ_LOCK      (reg_name + ".BL1_VERSION_READ_LOCK",        *this, 19, 1),
        BL2_VERSION_WRITE_LOCK     (reg_name + ".BL2_VERSION_WRITE_LOCK",       *this, 20, 1),
        BL2_VERSION_READ_LOCK      (reg_name + ".BL2_VERSION_READ_LOCK",        *this, 21, 1),
        CHIPLET_UID_WRITE_LOCK     (reg_name + ".CHIPLET_UID_WRITE_LOCK",       *this, 22, 1),
        CHIPLET_UID_READ_LOCK      (reg_name + ".CHIPLET_UID_READ_LOCK",        *this, 23, 1),
        SIP_PUBK_WRITE_LOCK        (reg_name + ".SIP_PUBK_WRITE_LOCK",          *this, 24, 1),
        SIP_PUBK_READ_LOCK         (reg_name + ".SIP_PUBK_READ_LOCK",           *this, 25, 1),
        SIP_UID_WRITE_LOCK         (reg_name + ".SIP_UID_WRITE_LOCK",           *this, 26, 1),
        SIP_UID_READ_LOCK          (reg_name + ".SIP_UID_READ_LOCK",            *this, 27, 1),
        SYS_PUBK_WRITE_LOCK        (reg_name + ".SYS_PUBK_WRITE_LOCK",          *this, 28, 1),
        SYS_PUBK_READ_LOCK         (reg_name + ".SYS_PUBK_READ_LOCK",           *this, 29, 1),
        SYS_UID_WRITE_LOCK         (reg_name + ".SYS_UID_WRITE_LOCK",           *this, 30, 1),
        SYS_UID_READ_LOCK          (reg_name + ".SYS_UID_READ_LOCK",            *this, 31, 1)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> LC_STATE_WRITE_LOCK;          ///< [0]   Prevents further writes to LC_STATE
    csml_bitfield<N> LC_STATE_READ_LOCK;           ///< [1]   Prevents reads of LC_STATE
    csml_bitfield<N> SBOOT_DIS_WRITE_LOCK;         ///< [2]   Prevents further writes to SBOOT_DIS
    csml_bitfield<N> SBOOT_DIS_READ_LOCK;          ///< [3]   Prevents reads of SBOOT_DIS
    csml_bitfield<N> TRANSIENT_RMA_EN_WRITE_LOCK;  ///< [4]   Prevents further writes to TRANSIENT_RMA_EN
    csml_bitfield<N> TRANSIENT_RMA_EN_READ_LOCK;   ///< [5]   Prevents reads of TRANSIENT_RMA_EN
    csml_bitfield<N> SIP_DIS_WRITE_LOCK;           ///< [6]   Prevents further writes to SIP_DIS_{LO,HI}
    csml_bitfield<N> SIP_DIS_READ_LOCK;            ///< [7]   Prevents reads of SIP_DIS_{LO,HI}
    csml_bitfield<N> SYS_DIS_WRITE_LOCK;           ///< [8]   Prevents further writes to SYS_DIS_{LO,HI}
    csml_bitfield<N> SYS_DIS_READ_LOCK;            ///< [9]   Prevents reads of SYS_DIS_{LO,HI}
    csml_bitfield<N> RMA_SIP_TOKEN_WRITE_LOCK;     ///< [10]  Prevents further writes to RMA_SIP_TOKEN
    csml_bitfield<N> RMA_SIP_TOKEN_READ_LOCK;      ///< [11]  Prevents reads of RMA_SIP_TOKEN
    csml_bitfield<N> RMA_CHIPLET_TOKEN_WRITE_LOCK; ///< [12]  Prevents further writes to RMA_CHIPLET_TOKEN
    csml_bitfield<N> RMA_CHIPLET_TOKEN_READ_LOCK;  ///< [13]  Prevents reads of RMA_CHIPLET_TOKEN
    csml_bitfield<N> CLASS_KEY_WRITE_LOCK;         ///< [14]  Prevents further writes to CLASS_KEY
    csml_bitfield<N> CLASS_KEY_READ_LOCK;          ///< [15]  Prevents reads of CLASS_KEY
    csml_bitfield<N> CHIPLET_PUBK_SEL_WRITE_LOCK;  ///< [16]  Prevents further writes to CHIPLET_PUBK_REVOKE
    csml_bitfield<N> CHIPLET_PUBK_SEL_READ_LOCK;   ///< [17]  Prevents reads of CHIPLET_PUBK_REVOKE
    csml_bitfield<N> BL1_VERSION_WRITE_LOCK;       ///< [18]  Prevents further writes to BL1_VERSION
    csml_bitfield<N> BL1_VERSION_READ_LOCK;        ///< [19]  Prevents reads of BL1_VERSION
    csml_bitfield<N> BL2_VERSION_WRITE_LOCK;       ///< [20]  Prevents further writes to BL2_VERSION
    csml_bitfield<N> BL2_VERSION_READ_LOCK;        ///< [21]  Prevents reads of BL2_VERSION
    csml_bitfield<N> CHIPLET_UID_WRITE_LOCK;       ///< [22]  Prevents further writes to CHIPLET_UID
    csml_bitfield<N> CHIPLET_UID_READ_LOCK;        ///< [23]  Prevents reads of CHIPLET_UID
    csml_bitfield<N> SIP_PUBK_WRITE_LOCK;          ///< [24]  Prevents further writes to SIP_PUBK
    csml_bitfield<N> SIP_PUBK_READ_LOCK;           ///< [25]  Prevents reads of SIP_PUBK
    csml_bitfield<N> SIP_UID_WRITE_LOCK;           ///< [26]  Prevents further writes to SIP_UID
    csml_bitfield<N> SIP_UID_READ_LOCK;            ///< [27]  Prevents reads of SIP_UID
    csml_bitfield<N> SYS_PUBK_WRITE_LOCK;          ///< [28]  Prevents further writes to SYS_PUBK
    csml_bitfield<N> SYS_PUBK_READ_LOCK;           ///< [29]  Prevents reads of SYS_PUBK
    csml_bitfield<N> SYS_UID_WRITE_LOCK;           ///< [30]  Prevents further writes to SYS_UID
    csml_bitfield<N> SYS_UID_READ_LOCK;            ///< [31]  Prevents reads of SYS_UID
};

/**
 * LOCKS_HI — Write-Lock / Read-Lock pairs for the upper shadow register set
 *
 * Covers STATUS_RPT, SEP_ROM_CTRL, SEP_SPI_CTRL, PUBLIC_KEY hashes, and
 * eight RESERVED groups. Same WOSET semantics as LOCKS_LO.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback); reads return accumulated value
 * Reset  : 0x00000000
 */
template<unsigned int N>
class LOCKS_HI_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    LOCKS_HI_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        STATUS_RPT_WRITE_LOCK        (reg_name + ".STATUS_RPT_WRITE_LOCK",         *this,  0, 1),
        STATUS_RPT_READ_LOCK         (reg_name + ".STATUS_RPT_READ_LOCK",          *this,  1, 1),
        SEP_ROM_CTRL_WRITE_LOCK      (reg_name + ".SEP_ROM_CTRL_WRITE_LOCK",       *this,  2, 1),
        SEP_ROM_CTRL_READ_LOCK       (reg_name + ".SEP_ROM_CTRL_READ_LOCK",        *this,  3, 1),
        SEP_SPI_CTRL_WRITE_LOCK      (reg_name + ".SEP_SPI_CTRL_WRITE_LOCK",       *this,  4, 1),
        SEP_SPI_CTRL_READ_LOCK       (reg_name + ".SEP_SPI_CTRL_READ_LOCK",        *this,  5, 1),
        SEP_PUBLIC_KEY_HASH_0_WRITE_LOCK(reg_name + ".SEP_PUBLIC_KEY_HASH_0_WRITE_LOCK", *this, 6, 1),
        SEP_PUBLIC_KEY_HASH_0_READ_LOCK (reg_name + ".SEP_PUBLIC_KEY_HASH_0_READ_LOCK",  *this, 7, 1),
        SEP_PUBLIC_KEY_HASH_1_WRITE_LOCK(reg_name + ".SEP_PUBLIC_KEY_HASH_1_WRITE_LOCK", *this, 8, 1),
        SEP_PUBLIC_KEY_HASH_1_READ_LOCK (reg_name + ".SEP_PUBLIC_KEY_HASH_1_READ_LOCK",  *this, 9, 1),
        RESERVED_0_WRITE_LOCK        (reg_name + ".RESERVED_0_WRITE_LOCK",  *this, 10, 1),
        RESERVED_0_READ_LOCK         (reg_name + ".RESERVED_0_READ_LOCK",   *this, 11, 1),
        RESERVED_1_WRITE_LOCK        (reg_name + ".RESERVED_1_WRITE_LOCK",  *this, 12, 1),
        RESERVED_1_READ_LOCK         (reg_name + ".RESERVED_1_READ_LOCK",   *this, 13, 1),
        RESERVED_2_WRITE_LOCK        (reg_name + ".RESERVED_2_WRITE_LOCK",  *this, 14, 1),
        RESERVED_2_READ_LOCK         (reg_name + ".RESERVED_2_READ_LOCK",   *this, 15, 1),
        RESERVED_3_WRITE_LOCK        (reg_name + ".RESERVED_3_WRITE_LOCK",  *this, 16, 1),
        RESERVED_3_READ_LOCK         (reg_name + ".RESERVED_3_READ_LOCK",   *this, 17, 1),
        RESERVED_4_WRITE_LOCK        (reg_name + ".RESERVED_4_WRITE_LOCK",  *this, 18, 1),
        RESERVED_4_READ_LOCK         (reg_name + ".RESERVED_4_READ_LOCK",   *this, 19, 1),
        RESERVED_5_WRITE_LOCK        (reg_name + ".RESERVED_5_WRITE_LOCK",  *this, 20, 1),
        RESERVED_5_READ_LOCK         (reg_name + ".RESERVED_5_READ_LOCK",   *this, 21, 1),
        RESERVED_6_WRITE_LOCK        (reg_name + ".RESERVED_6_WRITE_LOCK",  *this, 22, 1),
        RESERVED_6_READ_LOCK         (reg_name + ".RESERVED_6_READ_LOCK",   *this, 23, 1),
        RESERVED_7_WRITE_LOCK        (reg_name + ".RESERVED_7_WRITE_LOCK",  *this, 24, 1),
        RESERVED_7_READ_LOCK         (reg_name + ".RESERVED_7_READ_LOCK",   *this, 25, 1),
        RESERVED_LAST_256_WRITE_LOCK (reg_name + ".RESERVED_LAST_256_WRITE_LOCK", *this, 26, 1),
        RESERVED_LAST_256_READ_LOCK  (reg_name + ".RESERVED_LAST_256_READ_LOCK",  *this, 27, 1),
        RESERVED_LAST_64_WRITE_LOCK  (reg_name + ".RESERVED_LAST_64_WRITE_LOCK",  *this, 28, 1),
        RESERVED_LAST_64_READ_LOCK   (reg_name + ".RESERVED_LAST_64_READ_LOCK",   *this, 29, 1),
        RESERVED_LAST_32_WRITE_LOCK  (reg_name + ".RESERVED_LAST_32_WRITE_LOCK",  *this, 30, 1),
        RESERVED_LAST_32_READ_LOCK   (reg_name + ".RESERVED_LAST_32_READ_LOCK",   *this, 31, 1)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> STATUS_RPT_WRITE_LOCK;          ///< [0]     Prevents further writes to STATUS_RPT
    csml_bitfield<N> STATUS_RPT_READ_LOCK;           ///< [1]     Prevents reads of STATUS_RPT
    csml_bitfield<N> SEP_ROM_CTRL_WRITE_LOCK;        ///< [2]     Prevents further writes to SEP_ROM_CTRL
    csml_bitfield<N> SEP_ROM_CTRL_READ_LOCK;         ///< [3]     Prevents reads of SEP_ROM_CTRL
    csml_bitfield<N> SEP_SPI_CTRL_WRITE_LOCK;        ///< [4]     Prevents further writes to SEP_SPI_CTRL_FIELD_EN
    csml_bitfield<N> SEP_SPI_CTRL_READ_LOCK;         ///< [5]     Prevents reads of SEP_SPI_CTRL_FIELD_EN
    csml_bitfield<N> SEP_PUBLIC_KEY_HASH_0_WRITE_LOCK; ///< [6]   Prevents further writes to PUBLIC_KEY_0
    csml_bitfield<N> SEP_PUBLIC_KEY_HASH_0_READ_LOCK;  ///< [7]   Prevents reads of PUBLIC_KEY_0
    csml_bitfield<N> SEP_PUBLIC_KEY_HASH_1_WRITE_LOCK; ///< [8]   Prevents further writes to PUBLIC_KEY_1
    csml_bitfield<N> SEP_PUBLIC_KEY_HASH_1_READ_LOCK;  ///< [9]   Prevents reads of PUBLIC_KEY_1
    csml_bitfield<N> RESERVED_0_WRITE_LOCK;          ///< [10]
    csml_bitfield<N> RESERVED_0_READ_LOCK;           ///< [11]
    csml_bitfield<N> RESERVED_1_WRITE_LOCK;          ///< [12]
    csml_bitfield<N> RESERVED_1_READ_LOCK;           ///< [13]
    csml_bitfield<N> RESERVED_2_WRITE_LOCK;          ///< [14]
    csml_bitfield<N> RESERVED_2_READ_LOCK;           ///< [15]
    csml_bitfield<N> RESERVED_3_WRITE_LOCK;          ///< [16]
    csml_bitfield<N> RESERVED_3_READ_LOCK;           ///< [17]
    csml_bitfield<N> RESERVED_4_WRITE_LOCK;          ///< [18]
    csml_bitfield<N> RESERVED_4_READ_LOCK;           ///< [19]
    csml_bitfield<N> RESERVED_5_WRITE_LOCK;          ///< [20]
    csml_bitfield<N> RESERVED_5_READ_LOCK;           ///< [21]
    csml_bitfield<N> RESERVED_6_WRITE_LOCK;          ///< [22]
    csml_bitfield<N> RESERVED_6_READ_LOCK;           ///< [23]
    csml_bitfield<N> RESERVED_7_WRITE_LOCK;          ///< [24]
    csml_bitfield<N> RESERVED_7_READ_LOCK;           ///< [25]
    csml_bitfield<N> RESERVED_LAST_256_WRITE_LOCK;   ///< [26]
    csml_bitfield<N> RESERVED_LAST_256_READ_LOCK;    ///< [27]
    csml_bitfield<N> RESERVED_LAST_64_WRITE_LOCK;    ///< [28]
    csml_bitfield<N> RESERVED_LAST_64_READ_LOCK;     ///< [29]
    csml_bitfield<N> RESERVED_LAST_32_WRITE_LOCK;    ///< [30]
    csml_bitfield<N> RESERVED_LAST_32_READ_LOCK;     ///< [31]
};

/**
 * LC_STATE — Life-Cycle state (differentially encoded)
 *
 * RTL encoding: {~raw[3:0], raw[3:0]} stored in bits [7:0].
 * Unprovisioned = 0xF0  ({4'hF, 4'h0}).
 * Write-set-only: transitions are one-way (WOSET via callback).
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x000000F0  (differentially encoded unprovisioned state)
 */
template<unsigned int N>
class LC_STATE_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0xF0
    LC_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x000000f0),
        lc_state (reg_name + ".lc_state",  *this, 0,  8),
        reserved0(reg_name + ".reserved0", *this, 8, 24)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> lc_state;  ///< [7:0]  Differentially encoded life-cycle state
    csml_bitfield<N> reserved0; ///< [31:8]
};

/**
 * SBOOT_DIS — Secure Boot Disable fuse
 *
 * Read-only shadow; loaded at elaboration from the CCI param of the same name.
 * Cannot be written by firmware (write_mask=0).
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SBOOT_DIS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SBOOT_DIS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
        disable_secure_boot(reg_name + ".disable_secure_boot", *this, 0,  1),
        reserved0          (reg_name + ".reserved0",           *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> disable_secure_boot; ///< [0]    1 = secure boot verification bypassed
    csml_bitfield<N> reserved0;           ///< [31:1]
};

/**
 * TRANSIENT_RMA_EN — Transient RMA Enable fuse
 *
 * Read-only shadow; loaded at elaboration from the CCI param of the same name.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class TRANSIENT_RMA_EN_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    TRANSIENT_RMA_EN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
        transient_rma_en(reg_name + ".transient_rma_en", *this, 0,  1),
        reserved0       (reg_name + ".reserved0",        *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> transient_rma_en; ///< [0]    1 = transient RMA mode enabled
    csml_bitfield<N> reserved0;        ///< [31:1]
};

/**
 * SIP_DIS_LO — SIP Functional-Disable fuse, lower word (WOSET)
 *
 * Debug and trace disable bits for the SIP domain.
 * Write-set-only: bits accumulate (WOSET via callback).
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SIP_DIS_LO_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    SIP_DIS_LO_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        sep_debug    (reg_name + ".sep_debug",       *this, 0,  1),
        soc_debug    (reg_name + ".soc_debug",       *this, 1,  1),
        ap_debug     (reg_name + ".ap_debug",        *this, 2,  1),
        ap_trace     (reg_name + ".ap_trace",        *this, 3,  1),
        sip_debug    (reg_name + ".sip_debug",       *this, 4,  1),
        debug_reserved(reg_name + ".debug_reserved", *this, 5, 27)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> sep_debug;     ///< [0]    Disable SEP debug access
    csml_bitfield<N> soc_debug;     ///< [1]    Disable SoC debug access
    csml_bitfield<N> ap_debug;      ///< [2]    Disable AP debug access
    csml_bitfield<N> ap_trace;      ///< [3]    Disable AP trace
    csml_bitfield<N> sip_debug;     ///< [4]    Disable SIP debug access
    csml_bitfield<N> debug_reserved;///< [31:5]
};

/**
 * SIP_DIS_HI — SIP Functional-Disable fuse, upper word (WOSET)
 *
 * Test-mode disable bits for the SIP domain.
 * Write-set-only: bits accumulate (WOSET via callback).
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SIP_DIS_HI_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    SIP_DIS_HI_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        fuse_test    (reg_name + ".fuse_test",     *this,  0,  1),
        sep_stest    (reg_name + ".sep_stest",     *this,  1,  1),
        sep_dtest    (reg_name + ".sep_dtest",     *this,  2,  1),
        ap_stest     (reg_name + ".ap_stest",      *this,  3,  1),
        ap_dtest     (reg_name + ".ap_dtest",      *this,  4,  1),
        test_reserved(reg_name + ".test_reserved", *this,  5, 11),
        func_reserved(reg_name + ".func_reserved", *this, 16, 16)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> fuse_test;     ///< [0]     Disable fuse test mode
    csml_bitfield<N> sep_stest;     ///< [1]     Disable SEP scan test
    csml_bitfield<N> sep_dtest;     ///< [2]     Disable SEP diagnostic test
    csml_bitfield<N> ap_stest;      ///< [3]     Disable AP scan test
    csml_bitfield<N> ap_dtest;      ///< [4]     Disable AP diagnostic test
    csml_bitfield<N> test_reserved; ///< [15:5]
    csml_bitfield<N> func_reserved; ///< [31:16]
};

/**
 * SYS_DIS_LO — SYS Functional-Disable fuse, lower word (WOSET)
 *
 * Same bit layout as SIP_DIS_LO; applies to the system domain.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SYS_DIS_LO_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    SYS_DIS_LO_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        sep_debug    (reg_name + ".sep_debug",       *this, 0,  1),
        soc_debug    (reg_name + ".soc_debug",       *this, 1,  1),
        ap_debug     (reg_name + ".ap_debug",        *this, 2,  1),
        ap_trace     (reg_name + ".ap_trace",        *this, 3,  1),
        sip_debug    (reg_name + ".sip_debug",       *this, 4,  1),
        debug_reserved(reg_name + ".debug_reserved", *this, 5, 27)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> sep_debug;     ///< [0]    Disable SEP debug access (SYS domain)
    csml_bitfield<N> soc_debug;     ///< [1]    Disable SoC debug access (SYS domain)
    csml_bitfield<N> ap_debug;      ///< [2]    Disable AP debug access (SYS domain)
    csml_bitfield<N> ap_trace;      ///< [3]    Disable AP trace (SYS domain)
    csml_bitfield<N> sip_debug;     ///< [4]    Disable SIP debug access (SYS domain)
    csml_bitfield<N> debug_reserved;///< [31:5]
};

/**
 * SYS_DIS_HI — SYS Functional-Disable fuse, upper word (WOSET)
 *
 * Same bit layout as SIP_DIS_HI; applies to the system domain.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SYS_DIS_HI_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    SYS_DIS_HI_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        fuse_test    (reg_name + ".fuse_test",     *this,  0,  1),
        sep_stest    (reg_name + ".sep_stest",     *this,  1,  1),
        sep_dtest    (reg_name + ".sep_dtest",     *this,  2,  1),
        ap_stest     (reg_name + ".ap_stest",      *this,  3,  1),
        ap_dtest     (reg_name + ".ap_dtest",      *this,  4,  1),
        test_reserved(reg_name + ".test_reserved", *this,  5, 11),
        func_reserved(reg_name + ".func_reserved", *this, 16, 16)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> fuse_test;     ///< [0]     Disable fuse test mode (SYS domain)
    csml_bitfield<N> sep_stest;     ///< [1]     Disable SEP scan test (SYS domain)
    csml_bitfield<N> sep_dtest;     ///< [2]     Disable SEP diagnostic test (SYS domain)
    csml_bitfield<N> ap_stest;      ///< [3]     Disable AP scan test (SYS domain)
    csml_bitfield<N> ap_dtest;      ///< [4]     Disable AP diagnostic test (SYS domain)
    csml_bitfield<N> test_reserved; ///< [15:5]
    csml_bitfield<N> func_reserved; ///< [31:16]
};

/**
 * RMA_SIP_TOKEN_type — RMA SIP Token word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP RMA SIP token (SHA-256 digest words).
 * Loaded at elaboration via backdoor from the CCI param of the same name.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class RMA_SIP_TOKEN_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    RMA_SIP_TOKEN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        token_digest(reg_name + ".token_digest", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> token_digest; ///< [31:0] OTP RMA SIP token word (SHA-256 digest)
};

/**
 * RMA_CHIPLET_TOKEN_type — RMA Chiplet Token word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP RMA chiplet token (SHA-256 digest words).
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class RMA_CHIPLET_TOKEN_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    RMA_CHIPLET_TOKEN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        token_digest(reg_name + ".token_digest", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> token_digest; ///< [31:0] OTP RMA chiplet token word (SHA-256 digest)
};

/**
 * CLASS_KEY_type — Classification Key word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP classification key material.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class CLASS_KEY_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    CLASS_KEY_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        key(reg_name + ".key", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> key; ///< [31:0] OTP classification key word
};

/**
 * CHIPLET_PUBK_REVOKE_type — Chiplet Public Key Revocation bitmap (WOSET)
 *
 * Each bit revokes one public key slot. Write-set-only per RTL WRITE_SET_ONLY
 * attribute; implemented via callback.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class CHIPLET_PUBK_REVOKE_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    CHIPLET_PUBK_REVOKE_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
        select(reg_name + ".select", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> select; ///< [31:0] Revocation bitmap; bit N=1 revokes public key slot N
};

/**
 * BL1_VERSION_type — BL1 Anti-Rollback Version word (8 × 32-bit array, WOSET)
 *
 * One-hot encoded anti-rollback counter for BL1. Write-set-only per RTL.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class BL1_VERSION_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    BL1_VERSION_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
        version(reg_name + ".version", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> version; ///< [31:0] BL1 anti-rollback version word (one-hot encoded)
};

/**
 * BL2_VERSION_type — BL2 Anti-Rollback Version word (8 × 32-bit array, WOSET)
 *
 * One-hot encoded anti-rollback counter for BL2. Write-set-only per RTL.
 *
 * Access : Write-Set-Only (WOSET via efuse_model callback)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class BL2_VERSION_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    BL2_VERSION_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
        version(reg_name + ".version", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> version; ///< [31:0] BL2 anti-rollback version word (one-hot encoded)
};

/**
 * CHIPLET_UID_type — Chiplet Unique ID word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP chiplet UID.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class CHIPLET_UID_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    CHIPLET_UID_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        uid(reg_name + ".uid", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> uid; ///< [31:0] Chiplet unique identifier word
};

/**
 * SIP_PUBK_type — SIP Public Key digest word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP SIP public key digest.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SIP_PUBK_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SIP_PUBK_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        key_digest(reg_name + ".key_digest", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> key_digest; ///< [31:0] SIP public key digest word
};

/**
 * SIP_UID_type — SIP Unique ID word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP SIP UID.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SIP_UID_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SIP_UID_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        uid(reg_name + ".uid", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> uid; ///< [31:0] SIP unique identifier word
};

/**
 * SYS_PUBK_type — SYS Public Key digest word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP system public key digest.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SYS_PUBK_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SYS_PUBK_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        key_digest(reg_name + ".key_digest", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> key_digest; ///< [31:0] SYS public key digest word
};

/**
 * SYS_UID_type — SYS Unique ID word (8 × 32-bit array)
 *
 * Read-only shadow of the OTP system UID.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SYS_UID_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SYS_UID_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        uid(reg_name + ".uid", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> uid; ///< [31:0] SYS unique identifier word
};

/**
 * STATUS_RPT — eFuse Sense Status Report
 *
 * Read-only shadow; reports ECC/parity status of the fuse sense operation.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class STATUS_RPT_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    STATUS_RPT_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
        rpt      (reg_name + ".rpt",       *this, 0,  2),
        reserved0(reg_name + ".reserved0", *this, 2, 30)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> rpt;      ///< [1:0]  ECC/parity sense report (0=pass, 1=single-bit, 2=multi-bit)
    csml_bitfield<N> reserved0;///< [31:2]
};

/**
 * SEP_ROM_CTRL — SEP ROM Endianness and Swap Control
 *
 * Read-only shadow; controls ROM byte-swap behavior for SEP firmware loading.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SEP_ROM_CTRL_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SEP_ROM_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
        rom_endianness_ctrl(reg_name + ".rom_endianness_ctrl", *this, 0,  1),
        rom_swap_ctrl      (reg_name + ".rom_swap_ctrl",       *this, 1,  5),
        reserved0          (reg_name + ".reserved0",           *this, 6, 26)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> rom_endianness_ctrl; ///< [0]    ROM endianness: 0=little, 1=big
    csml_bitfield<N> rom_swap_ctrl;       ///< [5:1]  Per-byte swap control mask
    csml_bitfield<N> reserved0;           ///< [31:6]
};

/**
 * SEP_SPI_CTRL_FIELD_EN_type — SPI Controller Field Enable fuse
 *
 * Read-only shadow; enables individual SPI controller configuration fields
 * and sets the SMU PLL system clock value.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class SEP_SPI_CTRL_FIELD_EN_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    SEP_SPI_CTRL_FIELD_EN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        spi_control_field_en    (reg_name + ".spi_control_field_en",     *this,  0,  8),
        smu_pll_sysclk          (reg_name + ".smu_pll_sysclk",           *this,  8, 11),
        spi_control_field_en_rsvd(reg_name + ".spi_control_field_en_rsvd",*this, 19, 13)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> spi_control_field_en;     ///< [7:0]   SPI control field enable bitmap
    csml_bitfield<N> smu_pll_sysclk;           ///< [18:8]  SMU PLL system clock setting
    csml_bitfield<N> spi_control_field_en_rsvd;///< [31:19]
};

/**
 * ro_stub_type — Generic Read-Only stub register
 *
 * Used for SPI PHY configuration registers, PUBLIC_KEY hashes, and all
 * RESERVED regions. Loaded at elaboration from the matching CCI param where
 * applicable; all-zeros otherwise.
 *
 * Access : Read-Only (shadow loaded via backdoor)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class ro_stub_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    ro_stub_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        data(reg_name + ".data", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> data; ///< [31:0] Register data (RO)
};

/**
 * EFUSE_BANK_INIT_TIME — the whole of EFUSE_SHIM_CTRL
 *
 * Cycles to wait for the OTP macro to initialise before sensing may begin
 * (efuse_shim_ctrl.rdl). The VP keeps the register because software reads and
 * writes it, but not its effect: sensing here is instantaneous, so there is no
 * initialisation window for the count to cover.
 *
 * Access : Read/Write (sw=rw, hw=r)
 * Reset  : 0x00000020
 */
template<unsigned int N>
class EFUSE_BANK_INIT_TIME_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x20
    EFUSE_BANK_INIT_TIME_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000020),
        init_time(reg_name + ".init_time", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> init_time; ///< [31:0] OTP macro initialisation time, in cycles
};


// ============================================================================
// EFUSE_INTERFACE_CTRL REGISTER TYPES  (offsets 0x400–0x418)
// Source: knowledge-base/efuse/data/registers/rdl/efuse_interface_ctrl.rdl
// ============================================================================

/**
 * EFUSE_INTERFACE_CTRL_STATUS — eFuse Interface Controller Status
 *
 * efuse_sense_done is permanently 1 in the VP (fuse sense completes at
 * end_of_elaboration). All other bits remain 0; firmware writes are rejected.
 *
 * Access : Read-Only (write_mask=0x0)
 * Reset  : 0x00000001  (efuse_sense_done=1)
 */
template<unsigned int N>
class EFUSE_INTERFACE_CTRL_STATUS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x700, reset=0x1
    //
    // The error bits [6:4] are status: hardware sets them and software cannot
    // write them. Software clears them through the separate W1S clear bits
    // [10:8], which is why the write mask is 0x700 rather than 0 -- a fully RO
    // register makes the clear a bus error, and firmware doing the documented
    // clear-then-retry takes a store fault instead.
    EFUSE_INTERFACE_CTRL_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x00000700, 0x00000001),
        efuse_sense_done  (reg_name + ".efuse_sense_done",   *this,  0,  1),
        reserved0         (reg_name + ".reserved0",          *this,  1,  3),
        efuse_req_error   (reg_name + ".efuse_req_error",    *this,  4,  1),
        efuse_program_addr_error(reg_name + ".efuse_program_addr_error", *this, 5, 1),
        efuse_read_addr_error   (reg_name + ".efuse_read_addr_error",    *this, 6, 1),
        reserved1         (reg_name + ".reserved1",          *this,  7,  1),
        efuse_req_err_clear(reg_name + ".efuse_req_err_clear",*this,  8,  1),
        efuse_program_addr_error_clear(reg_name + ".efuse_program_addr_error_clear", *this, 9, 1),
        efuse_read_addr_error_clear  (reg_name + ".efuse_read_addr_error_clear",    *this, 10, 1),
        reserved2         (reg_name + ".reserved2",          *this, 11, 21)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> efuse_sense_done;   ///< [0]    1 = OTP sense operation complete (always 1 in VP)
    csml_bitfield<N> reserved0;          ///< [3:1]
    csml_bitfield<N> efuse_req_error;    ///< [4]    1 = last OTP request was refused (locked or gated)
    csml_bitfield<N> efuse_program_addr_error; ///< [5] 1 = program address was out of range
    csml_bitfield<N> efuse_read_addr_error;    ///< [6] 1 = read address was out of range
    csml_bitfield<N> reserved1;          ///< [7]
    csml_bitfield<N> efuse_req_err_clear;      ///< [8]  Write 1 to clear efuse_req_error
    csml_bitfield<N> efuse_program_addr_error_clear; ///< [9]  Write 1 to clear bit [5]
    csml_bitfield<N> efuse_read_addr_error_clear;    ///< [10] Write 1 to clear bit [6]
    csml_bitfield<N> reserved2;          ///< [31:11]
};

/**
 * EFUSE_PROGRAM_CTRL — eFuse Programming Control
 *
 * Burns one OTP bit per `efuse_program_go` pulse, with optional read-back
 * verification. The command completes inside the triggering write, so
 * `program_busy` is never observed set; `program_done` and `program_status`
 * carry the result.
 *
 * Access : Read-Write, with [26:24] hardware-owned (sw=r)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class EFUSE_PROGRAM_CTRL_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    EFUSE_PROGRAM_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        efuse_addr            (reg_name + ".efuse_addr",             *this,  0, 16),
        efuse_data            (reg_name + ".efuse_data",             *this, 16,  1),
        efuse_program_go      (reg_name + ".efuse_program_go",       *this, 17,  1),
        efuse_program_read_back(reg_name + ".efuse_program_read_back",*this, 18,  1),
        reserved0             (reg_name + ".reserved0",              *this, 19,  5),
        program_busy          (reg_name + ".program_busy",           *this, 24,  1),
        program_done          (reg_name + ".program_done",           *this, 25,  1),
        program_status        (reg_name + ".program_status",         *this, 26,  1),
        program_enable        (reg_name + ".program_enable",         *this, 27,  1),
        reserved1             (reg_name + ".reserved1",              *this, 28,  4)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> efuse_addr;             ///< [15:0]  OTP cell address to program
    csml_bitfield<N> efuse_data;             ///< [16]    Data bit to program into the cell
    csml_bitfield<N> efuse_program_go;       ///< [17]    Write 1 to initiate programming (singlepulse)
    csml_bitfield<N> efuse_program_read_back;///< [18]    1 = read back and verify after programming
    csml_bitfield<N> reserved0;              ///< [23:19]
    csml_bitfield<N> program_busy;           ///< [24]    1 = programming operation in progress (hw=w)
    csml_bitfield<N> program_done;           ///< [25]    1 = programming operation complete (hw=w)
    csml_bitfield<N> program_status;         ///< [26]    0 = pass, 1 = fail (hw=w)
    csml_bitfield<N> program_enable;         ///< [27]    1 = enable programming path
    csml_bitfield<N> reserved1;              ///< [31:28]
};

/**
 * EFUSE_READ_CTRL — eFuse Read Control
 *
 * Controls raw OTP cell readback via the interface controller. In the VP,
 * fuse readback is not modelled; this register is a simple R/W stub.
 *
 * Access : Read-Write (VP stub; readback not modelled)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class EFUSE_READ_CTRL_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    EFUSE_READ_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
        efuse_addr  (reg_name + ".efuse_addr",   *this,  0, 16),
        efuse_read_go(reg_name + ".efuse_read_go",*this, 16,  1),
        reserved0   (reg_name + ".reserved0",    *this, 17,  7),
        read_busy   (reg_name + ".read_busy",    *this, 24,  1),
        read_done   (reg_name + ".read_done",    *this, 25,  1),
        read_status (reg_name + ".read_status",  *this, 26,  1),
        reserved1   (reg_name + ".reserved1",    *this, 27,  1),
        read_enable (reg_name + ".read_enable",  *this, 28,  1),
        reserved2   (reg_name + ".reserved2",    *this, 29,  3)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> efuse_addr;   ///< [15:0]  OTP cell address to read
    csml_bitfield<N> efuse_read_go;///< [16]    Write 1 to initiate readback (singlepulse)
    csml_bitfield<N> reserved0;    ///< [23:17]
    csml_bitfield<N> read_busy;    ///< [24]    1 = readback operation in progress (hw=w)
    csml_bitfield<N> read_done;    ///< [25]    1 = readback operation complete (hw=w)
    csml_bitfield<N> read_status;  ///< [26]    0 = pass, 1 = fail (hw=w)
    csml_bitfield<N> reserved1;    ///< [27]
    csml_bitfield<N> read_enable;  ///< [28]    1 = enable readback path
    csml_bitfield<N> reserved2;    ///< [31:29]
};

/**
 * EFUSE_READ_DATA_type — eFuse Read Data output register
 *
 * Holds the data returned by the OTP interface controller after a read or
 * program-readback operation. Hardware-written only (hw=w); always 0 in VP
 * since raw fuse access is not modelled.
 *
 * Access : Read-Only (hw=w; VP always returns 0x0)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class EFUSE_READ_DATA_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0x0, reset=0x0
    EFUSE_READ_DATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
        dout(reg_name + ".dout", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> dout; ///< [31:0] Data output from OTP interface controller
};

/**
 * EFUSE_READ_REQ_TIMEOUT_type — eFuse Read Request Timeout
 *
 * Configures how many cycles the interface controller waits for a read
 * response before asserting efuse_req_error.
 *
 * Access : Read-Write
 * Reset  : 0x00800000  (timeout_cycles=0x800000, timeout_enable=0)
 */
template<unsigned int N>
class EFUSE_READ_REQ_TIMEOUT_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x00800000
    EFUSE_READ_REQ_TIMEOUT_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00800000),
        timeout_cycles(reg_name + ".timeout_cycles", *this,  0, 28),
        timeout_enable(reg_name + ".timeout_enable", *this, 28,  1),
        reserved0     (reg_name + ".reserved0",      *this, 29,  3)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> timeout_cycles; ///< [27:0]  Timeout count in clock cycles
    csml_bitfield<N> timeout_enable; ///< [28]    1 = timeout detection enabled
    csml_bitfield<N> reserved0;      ///< [31:29]
};

/**
 * EFUSE_PROGRAM_REQ_TIMEOUT_type — eFuse Program Request Timeout
 *
 * Configures how many cycles the interface controller waits for a program
 * response before asserting efuse_req_error.
 *
 * Access : Read-Write
 * Reset  : 0x00800000  (timeout_cycles=0x800000, timeout_enable=0)
 */
template<unsigned int N>
class EFUSE_PROGRAM_REQ_TIMEOUT_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x00800000
    EFUSE_PROGRAM_REQ_TIMEOUT_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00800000),
        timeout_cycles(reg_name + ".timeout_cycles", *this,  0, 28),
        timeout_enable(reg_name + ".timeout_enable", *this, 28,  1),
        reserved0     (reg_name + ".reserved0",      *this, 29,  3)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> timeout_cycles; ///< [27:0]  Timeout count in clock cycles
    csml_bitfield<N> timeout_enable; ///< [28]    1 = timeout detection enabled
    csml_bitfield<N> reserved0;      ///< [31:29]
};


// ============================================================================
// EFUSE_MMR REGISTER TYPES  (offsets 0x500–0x56C)
// Source: knowledge-base/efuse/data/registers/rdl/efuse_mmr.rdl
// ============================================================================

/**
 * MMR_TOKEN_I_type — Token Input word (8 × 32-bit arrays × 3 token types)
 *
 * Firmware writes 256-bit token candidates here before triggering TOKEN_EOP.
 * RTL feeds these words into a SHA-256 engine to compute the match result.
 *
 * Access : Read-Write (firmware writes input; reads back last written value)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MMR_TOKEN_I_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF, write_mask=0xFFFFFFFF, reset=0x0
    MMR_TOKEN_I_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
        token_word(reg_name + ".token_word", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> token_word; ///< [31:0] One 32-bit word of the 256-bit token input
};

/**
 * TOKEN_EOP_type — Token End-of-Packet / Hash Trigger (singlepulse)
 *
 * Each go-bit triggers the SHA-256 comparison for its token type. Bits
 * are singlepulse (hardware clears after one cycle). Read returns 0.
 * The comparison runs to completion inside the triggering write, so the
 * matching result register is already updated by the next read.
 *
 * Access : Write-Only (read_mask=0x0; singlepulse bits)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class TOKEN_EOP_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x0, write_mask=0x00010101, reset=0x0
    TOKEN_EOP_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0x00010101, 0x0),
        rma_sip_token_go       (reg_name + ".rma_sip_token_go",        *this,  0,  1),
        reserved0              (reg_name + ".reserved0",               *this,  1,  7),
        rma_chiplet_token_go   (reg_name + ".rma_chiplet_token_go",    *this,  8,  1),
        reserved1              (reg_name + ".reserved1",               *this,  9,  7),
        secure_disable_token_go(reg_name + ".secure_disable_token_go", *this, 16,  1),
        reserved2              (reg_name + ".reserved2",               *this, 17, 15)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> rma_sip_token_go;        ///< [0]     Write 1 to trigger RMA SIP token hash comparison
    csml_bitfield<N> reserved0;               ///< [7:1]
    csml_bitfield<N> rma_chiplet_token_go;    ///< [8]     Write 1 to trigger RMA chiplet token hash comparison
    csml_bitfield<N> reserved1;               ///< [15:9]
    csml_bitfield<N> secure_disable_token_go; ///< [16]    Write 1 to trigger secure-disable token comparison
    csml_bitfield<N> reserved2;               ///< [31:17]
};

/**
 * TOKEN_MATCH_type — Token Match Result register
 *
 * Hardware-written triple-redundant comparator result (hw=w, sw=r): the model
 * writes it when TOKEN_EOP triggers a comparison, and software can only read it.
 * Encoding: 0x15 (6'b010101)=match, 0x2A (6'b101010)=mismatch, 0x3F=error,
 * 0x00=no comparison completed yet, which is also the reset value.
 *
 * Access : Read-Only to software
 * Reset  : 0x00000000
 */
template<unsigned int N>
class TOKEN_MATCH_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x3F, write_mask=0x0, reset=0x0
    TOKEN_MATCH_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x3f, 0x0, 0x0),
        token_match_status(reg_name + ".token_match_status", *this, 0,  6),
        reserved0         (reg_name + ".reserved0",          *this, 6, 26)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> token_match_status; ///< [5:0]  0x15=match, 0x2A=mismatch, 0x3F=error
    csml_bitfield<N> reserved0;          ///< [31:6]
};

} // namespace sep_efuse
