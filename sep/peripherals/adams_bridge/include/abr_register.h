// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_register.h
 * @brief regmodel register type definitions for the Adams Bridge (ABR) PQC engine.
 *
 * Offsets and field layouts are transcribed from the vendored RTL:
 *   tt-oca-hw/vendor/adams_bridge/src/abr_top/rtl/abr_reg.rdl
 *   tt-oca-hw/vendor/adams_bridge/src/abr_top/rtl/abr_reg.sv   (decode)
 *
 * The engine implements ML-DSA-87 and ML-KEM-1024 behind a single 64 KiB
 * aperture (SEP: 0x1094_0000..0x1094_FFFF, see sep_crypto_pkg::abr_rule).
 *
 * Access-type convention follows the RDL:
 *   - WO fields read back as 0 (read_bit_mask = 0).
 *   - RO fields reject writes  (write_bit_mask = 0).
 *   - Command fields are hardware-cleared once latched, so the model's
 *     functional layer overrides their write callbacks; the masks here only
 *     describe the bus-visible contract.
 */

#pragma once

#include "reg_file.h"

#include <string>

namespace abr {

/// Full 32-bit mask, used by plain data words.
static constexpr unsigned int MASK_ALL = 0xFFFFFFFFu;

// =============================================================================
// Generic data-word register types
//
// The bulk of the ABR aperture is unstructured key / signature / message
// material with no bitfields. Rather than declare one near-identical class per
// dword, these three types cover the read-only, write-only and read-write
// cases and are instantiated through regmodel::RegVector.
// =============================================================================

/// Read-only data word (hardware-produced results).
template <unsigned int N>
class RO_DATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    RO_DATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, MASK_ALL, 0x0u, 0x0u)
    {
        this->set_read_write_restrictions(memory);
    }

    RO_DATA_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }
};

/// Write-only data word (secrets and inputs; bus reads return 0).
template <unsigned int N>
class WO_DATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    WO_DATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x0u, MASK_ALL, 0x0u)
    {
        this->set_read_write_restrictions(memory);
    }

    WO_DATA_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }
};

/// Read-write data word (external memory windows).
template <unsigned int N>
class RW_DATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    RW_DATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, MASK_ALL, MASK_ALL, 0x0u)
    {
        this->set_read_write_restrictions(memory);
    }

    RW_DATA_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }
};

// =============================================================================
// ML-DSA control / status
// =============================================================================

/**
 * @brief MLDSA_CTRL @ 0x0010 — command and modifier flags.
 *
 * CTRL, PCR_SIGN, EXTERNAL_MU and STREAM_MSG are hardware-cleared once the
 * sequencer latches them, so software reads back 0.
 */
template <unsigned int N>
class MLDSA_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLDSA_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x0u, 0x7Fu, 0x0u),
          CTRL(reg_name + ".CTRL", *this, 0, 3),
          ZEROIZE(reg_name + ".ZEROIZE", *this, 3, 1),
          PCR_SIGN(reg_name + ".PCR_SIGN", *this, 4, 1),
          EXTERNAL_MU(reg_name + ".EXTERNAL_MU", *this, 5, 1),
          STREAM_MSG(reg_name + ".STREAM_MSG", *this, 6, 1),
          Reserved0(reg_name + ".Reserved0", *this, 7, 25)
    {
        this->set_read_write_restrictions(memory);
    }

    MLDSA_CTRL_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> CTRL;
    regmodel::Bitfield<N> ZEROIZE;
    regmodel::Bitfield<N> PCR_SIGN;
    regmodel::Bitfield<N> EXTERNAL_MU;
    regmodel::Bitfield<N> STREAM_MSG;
    regmodel::Bitfield<N> Reserved0;
};

/// MLDSA_STATUS @ 0x0014 — hardware-driven, read-only.
template <unsigned int N>
class MLDSA_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLDSA_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0xFu, 0x0u, 0x1u),
          READY(reg_name + ".READY", *this, 0, 1),
          VALID(reg_name + ".VALID", *this, 1, 1),
          MSG_STREAM_READY(reg_name + ".MSG_STREAM_READY", *this, 2, 1),
          ERROR(reg_name + ".ERROR", *this, 3, 1),
          Reserved0(reg_name + ".Reserved0", *this, 4, 28)
    {
        this->set_read_write_restrictions(memory);
    }

    MLDSA_STATUS_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> VALID;
    regmodel::Bitfield<N> MSG_STREAM_READY;
    regmodel::Bitfield<N> ERROR;
    regmodel::Bitfield<N> Reserved0;
};

/// MLDSA_MSG_STROBE @ 0x0158 — byte enables for streaming message writes.
template <unsigned int N>
class MLDSA_MSG_STROBE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLDSA_MSG_STROBE_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x0u, 0xFu, 0xFu),
          STROBE(reg_name + ".STROBE", *this, 0, 4),
          Reserved0(reg_name + ".Reserved0", *this, 4, 28)
    {
        this->set_read_write_restrictions(memory);
    }

    MLDSA_MSG_STROBE_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> STROBE;
    regmodel::Bitfield<N> Reserved0;
};

/// MLDSA_CTX_CONFIG @ 0x015C — signing context length in bytes.
template <unsigned int N>
class MLDSA_CTX_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLDSA_CTX_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x0u, 0xFFu, 0x0u),
          CTX_SIZE(reg_name + ".CTX_SIZE", *this, 0, 8),
          Reserved0(reg_name + ".Reserved0", *this, 8, 24)
    {
        this->set_read_write_restrictions(memory);
    }

    MLDSA_CTX_CONFIG_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> CTX_SIZE;
    regmodel::Bitfield<N> Reserved0;
};

// =============================================================================
// ML-KEM control / status
// =============================================================================

/// MLKEM_CTRL @ 0x9010 — command and zeroize (no ML-DSA-style modifiers).
template <unsigned int N>
class MLKEM_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLKEM_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x0u, 0xFu, 0x0u),
          CTRL(reg_name + ".CTRL", *this, 0, 3),
          ZEROIZE(reg_name + ".ZEROIZE", *this, 3, 1),
          Reserved0(reg_name + ".Reserved0", *this, 4, 28)
    {
        this->set_read_write_restrictions(memory);
    }

    MLKEM_CTRL_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> CTRL;
    regmodel::Bitfield<N> ZEROIZE;
    regmodel::Bitfield<N> Reserved0;
};

/// MLKEM_STATUS @ 0x9014 — no MSG_STREAM_READY, so ERROR sits at bit 2.
template <unsigned int N>
class MLKEM_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    MLKEM_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x7u, 0x0u, 0x1u),
          READY(reg_name + ".READY", *this, 0, 1),
          VALID(reg_name + ".VALID", *this, 1, 1),
          ERROR(reg_name + ".ERROR", *this, 2, 1),
          Reserved0(reg_name + ".Reserved0", *this, 3, 29)
    {
        this->set_read_write_restrictions(memory);
    }

    MLKEM_STATUS_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> VALID;
    regmodel::Bitfield<N> ERROR;
    regmodel::Bitfield<N> Reserved0;
};

// =============================================================================
// Key Vault interface (kv_def.rdl)
// =============================================================================

/// KV read control — read_en is hardware-cleared once the copy completes.
template <unsigned int N>
class KV_RD_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    KV_RD_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x7Fu, 0x7Fu, 0x0u),
          read_en(reg_name + ".read_en", *this, 0, 1),
          read_entry(reg_name + ".read_entry", *this, 1, 5),
          pcr_hash_extend(reg_name + ".pcr_hash_extend", *this, 6, 1),
          Reserved0(reg_name + ".Reserved0", *this, 7, 25)
    {
        this->set_read_write_restrictions(memory);
    }

    KV_RD_CTRL_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> read_en;
    regmodel::Bitfield<N> read_entry;
    regmodel::Bitfield<N> pcr_hash_extend;
    regmodel::Bitfield<N> Reserved0;
};

/// KV read status — ERROR is an 8-bit enum (0 = SUCCESS, 1 = KV_READ_FAIL).
template <unsigned int N>
class KV_RD_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    KV_RD_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x3FFu, 0x0u, 0x1u),
          READY(reg_name + ".READY", *this, 0, 1),
          VALID(reg_name + ".VALID", *this, 1, 1),
          ERROR(reg_name + ".ERROR", *this, 2, 8),
          Reserved0(reg_name + ".Reserved0", *this, 10, 22)
    {
        this->set_read_write_restrictions(memory);
    }

    KV_RD_STATUS_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> VALID;
    regmodel::Bitfield<N> ERROR;
    regmodel::Bitfield<N> Reserved0;
};

/// KV write control for the ML-KEM shared-key export path.
template <unsigned int N>
class KV_WR_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    KV_WR_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x7FFFu, 0x7FFFu, 0x0u),
          write_en(reg_name + ".write_en", *this, 0, 1),
          write_entry(reg_name + ".write_entry", *this, 1, 5),
          hmac_key_dest_valid(reg_name + ".hmac_key_dest_valid", *this, 6, 1),
          hmac_block_dest_valid(reg_name + ".hmac_block_dest_valid", *this, 7, 1),
          mldsa_seed_dest_valid(reg_name + ".mldsa_seed_dest_valid", *this, 8, 1),
          ecc_pkey_dest_valid(reg_name + ".ecc_pkey_dest_valid", *this, 9, 1),
          ecc_seed_dest_valid(reg_name + ".ecc_seed_dest_valid", *this, 10, 1),
          aes_key_dest_valid(reg_name + ".aes_key_dest_valid", *this, 11, 1),
          mlkem_seed_dest_valid(reg_name + ".mlkem_seed_dest_valid", *this, 12, 1),
          mlkem_msg_dest_valid(reg_name + ".mlkem_msg_dest_valid", *this, 13, 1),
          dma_data_dest_valid(reg_name + ".dma_data_dest_valid", *this, 14, 1),
          Reserved0(reg_name + ".Reserved0", *this, 15, 17)
    {
        this->set_read_write_restrictions(memory);
    }

    KV_WR_CTRL_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> write_en;
    regmodel::Bitfield<N> write_entry;
    regmodel::Bitfield<N> hmac_key_dest_valid;
    regmodel::Bitfield<N> hmac_block_dest_valid;
    regmodel::Bitfield<N> mldsa_seed_dest_valid;
    regmodel::Bitfield<N> ecc_pkey_dest_valid;
    regmodel::Bitfield<N> ecc_seed_dest_valid;
    regmodel::Bitfield<N> aes_key_dest_valid;
    regmodel::Bitfield<N> mlkem_seed_dest_valid;
    regmodel::Bitfield<N> mlkem_msg_dest_valid;
    regmodel::Bitfield<N> dma_data_dest_valid;
    regmodel::Bitfield<N> Reserved0;
};

/// KV write status.
template <unsigned int N>
class KV_WR_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    KV_WR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x3FFu, 0x0u, 0x1u),
          READY(reg_name + ".READY", *this, 0, 1),
          VALID(reg_name + ".VALID", *this, 1, 1),
          ERROR(reg_name + ".ERROR", *this, 2, 8),
          Reserved0(reg_name + ".Reserved0", *this, 10, 22)
    {
        this->set_read_write_restrictions(memory);
    }

    KV_WR_STATUS_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> VALID;
    regmodel::Bitfield<N> ERROR;
    regmodel::Bitfield<N> Reserved0;
};

// =============================================================================
// Caliptra interrupt block (intr_block_rf @ 0x8100)
// =============================================================================

/// global_intr_en_r @ 0x8100.
template <unsigned int N>
class GLOBAL_INTR_EN_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    GLOBAL_INTR_EN_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x3u, 0x3u, 0x0u),
          error_en(reg_name + ".error_en", *this, 0, 1),
          notif_en(reg_name + ".notif_en", *this, 1, 1),
          Reserved0(reg_name + ".Reserved0", *this, 2, 30)
    {
        this->set_read_write_restrictions(memory);
    }

    GLOBAL_INTR_EN_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> error_en;
    regmodel::Bitfield<N> notif_en;
    regmodel::Bitfield<N> Reserved0;
};

/// Single-bit enable register (error_intr_en_r @ 0x8104, notif_intr_en_r @ 0x8108).
template <unsigned int N>
class INTR_EN_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    INTR_EN_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x1u, 0x1u, 0x0u),
          en(reg_name + ".en", *this, 0, 1),
          Reserved0(reg_name + ".Reserved0", *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    INTR_EN_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> en;
    regmodel::Bitfield<N> Reserved0;
};

/// Aggregated interrupt status (error_global_intr_r, notif_global_intr_r) — RO.
template <unsigned int N>
class GLOBAL_INTR_STS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    GLOBAL_INTR_STS_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x1u, 0x0u, 0x0u),
          agg_sts(reg_name + ".agg_sts", *this, 0, 1),
          Reserved0(reg_name + ".Reserved0", *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    GLOBAL_INTR_STS_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> agg_sts;
    regmodel::Bitfield<N> Reserved0;
};

/**
 * @brief Sticky W1C interrupt status.
 *
 * The functional layer overrides the write callback to implement write-1-to-
 * clear; the mask here keeps the bus contract (bit 0 readable and writable).
 */
template <unsigned int N>
class INTERNAL_INTR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    INTERNAL_INTR_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x1u, 0x1u, 0x0u),
          sts(reg_name + ".sts", *this, 0, 1),
          Reserved0(reg_name + ".Reserved0", *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    INTERNAL_INTR_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> sts;
    regmodel::Bitfield<N> Reserved0;
};

/// Self-clearing W1S test trigger (error_intr_trig_r, notif_intr_trig_r).
template <unsigned int N>
class INTR_TRIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    INTR_TRIG_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x1u, 0x1u, 0x0u),
          trig(reg_name + ".trig", *this, 0, 1),
          Reserved0(reg_name + ".Reserved0", *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    INTR_TRIG_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> trig;
    regmodel::Bitfield<N> Reserved0;
};

/// Saturating interrupt event counter.
template <unsigned int N>
class INTR_COUNT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    INTR_COUNT_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, MASK_ALL, MASK_ALL, 0x0u),
          cnt(reg_name + ".cnt", *this, 0, 32)
    {
        this->set_read_write_restrictions(memory);
    }

    INTR_COUNT_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> cnt;
};

/// Hardware incrementor mirror — RO pulse.
template <unsigned int N>
class INTR_COUNT_INCR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    INTR_COUNT_INCR_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset, 0x1u, 0x0u, 0x0u),
          pulse(reg_name + ".pulse", *this, 0, 1),
          Reserved0(reg_name + ".Reserved0", *this, 1, 31)
    {
        this->set_read_write_restrictions(memory);
    }

    INTR_COUNT_INCR_type &operator=(DT value)
    {
        regmodel::Reg<N>::operator=(value);
        return *this;
    }

    regmodel::Bitfield<N> pulse;
    regmodel::Bitfield<N> Reserved0;
};

} // namespace abr
