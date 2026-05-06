/**
 * @file keymgr_tt_register.h
 * @brief Key Manager TT - SEP-host-accessible register definitions (CSML format)
 *
 * Only registers visible to the SEP host processor are defined here:
 *   1. Mailbox SEP-side registers  (MB_*)    - SEP sends commands / receives responses
 *   2. KPVLP registers             (KPVLP_*) - SEP provisions keys into KPV slots
 *
 * KM-internal registers (KPV, KMCSR, DRBG, OTP, crypto engine key storage)
 * are NOT modelled here — they are abstracted inside km_firmware_handler.
 *
 * Register offsets confirmed from km_mailbox_sep_regs.h (auto-generated from
 * km_mailbox_sep.rdl). KPVLP offsets confirmed from km_kpv_kpvlp_regs.h.
 * Base address deferred to VP integration time.
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "csml_register.h"

namespace keymgr_tt {

// ============================================================================
// Mailbox SEP-side register offsets  (confirmed from km_mailbox_sep_regs.h)
// ============================================================================
static constexpr unsigned int MB_WDATA_OFFSET    = 0x00; ///< SEP_WRITE_DATA
static constexpr unsigned int MB_WSEP_OFFSET     = 0x04; ///< SEP_WRITE_SEPARATOR
static constexpr unsigned int MB_RDATA_OFFSET    = 0x08; ///< SEP_READ_DATA
static constexpr unsigned int MB_STATUS_OFFSET   = 0x0C; ///< SEP_STATUS
static constexpr unsigned int MB_IRQS_OFFSET     = 0x10; ///< SEP_IRQ_STATUS
static constexpr unsigned int MB_IRQEN_OFFSET    = 0x14; ///< SEP_IRQ_ENABLE
static constexpr unsigned int MB_CTRL_OFFSET     = 0x18; ///< SEP_CTRL

// ============================================================================
// KPVLP register offsets  (confirmed from km_kpv_kpvlp_regs.h)
//   Key data  : slot n, word w  ->  KPVLP_KEY_OFFSET(n, w)
//   Control   : slot n          ->  KPVLP_CTRL_OFFSET(n)
//   Status    : single register ->  KPVLP_STATUS_OFFSET
// ============================================================================
static constexpr unsigned int KPVLP_KEY_WORDS_PER_SLOT = 16; // 512 bits / 32
static constexpr unsigned int KPVLP_NUM_SLOTS          = 32;

inline constexpr unsigned int KPVLP_KEY_OFFSET(unsigned int slot, unsigned int word) {
    return 0x000 + slot * (KPVLP_KEY_WORDS_PER_SLOT * 4) + word * 4;
}
inline constexpr unsigned int KPVLP_CTRL_OFFSET(unsigned int slot) {
    return 0x800 + slot * 4;
}
static constexpr unsigned int KPVLP_STATUS_OFFSET = 0x880;


// ============================================================================
// MAILBOX REGISTERS  (offsets confirmed from km_mailbox_sep_regs.h)
// ============================================================================

/**
 * MB_WDATA — Mailbox Write Data (SEP → Inbound Queue push)
 *
 * SEP writes one 32-bit word per write. To mark the last word of a message,
 * SEP first writes 1 to MB_WSEP.set, then writes this register. The
 * MB_WSEP.set bit is cleared by hardware after the write completes.
 *
 * Access : Write-Only from SEP side (read returns 0)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MB_WDATA_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x0 (WO), write_mask=0xFFFFFFFF, reset=0x0
    MB_WDATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0xFFFFFFFF, 0x0),
        DATA(reg_name + ".DATA", *this, 0, 32)
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

    csml_bitfield<N> DATA;  ///< [31:0]  Full 32-bit message payload word
};

/**
 * MB_WSEP — Mailbox Write Separator (SEP → set next inbound write as separator)
 *
 * SEP writes 1 to bit[0] before writing the last word of a message to MB_WDATA.
 * Hardware clears this bit automatically after the next MB_WDATA write completes.
 *
 * Access : Read-Write (bit[0] only; hardware clears after use)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MB_WSEP_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x1 (RW), write_mask=0x1, reset=0x0
    MB_WSEP_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x0),
        SET      (reg_name + ".SET",       *this, 0,  1),
        reserved0(reg_name + ".reserved0", *this, 1, 31)
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

    csml_bitfield<N> SET;       ///< [0]    Write 1 to tag the next MB_WDATA push as separator
    csml_bitfield<N> reserved0; ///< [31:1]
};

/**
 * MB_RDATA — Mailbox Read Data (SEP ← Outbound Queue pop)
 *
 * SEP reads one 32-bit word per read. After the read, MB_STATUS.outbound_separator
 * reflects whether this was the last word of a KM response message.
 *
 * Access : Read-Only from SEP side
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MB_RDATA_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF (RO), write_mask=0x0, reset=0x0
    MB_RDATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x0, 0x0),
        DATA(reg_name + ".DATA", *this, 0, 32)
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

    csml_bitfield<N> DATA;  ///< [31:0]  Full 32-bit response payload word
};

/**
 * MB_STATUS — Mailbox FIFO Status  (SEP_STATUS)
 *
 * Read-only. Reflects fill state, depth, overflow/underflow flags, and
 * separator indicators for inbound and outbound FIFOs.
 *
 * Bit layout confirmed from km_mailbox_sep_regs.h:
 *   [0]     inbound_empty
 *   [1]     inbound_full
 *   [2]     outbound_empty
 *   [3]     outbound_full
 *   [11:4]  inbound_depth   (words currently in inbound FIFO)
 *   [19:12] outbound_depth  (words currently in outbound FIFO)
 *   [20]    inbound_overflow
 *   [21]    outbound_overflow
 *   [22]    inbound_underflow
 *   [23]    outbound_underflow
 *   [24]    inbound_separator   (last word KM popped from inbound was a separator)
 *   [25]    outbound_separator  (last word SEP popped from outbound was a separator)
 *   [31:26] reserved
 *
 * Access : Read-Only
 * Reset  : 0x00000005  (inbound_empty=1 at bit[0], outbound_empty=1 at bit[2])
 */
template<unsigned int N>
class MB_STATUS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x03FFFFFF (bits[25:0] readable), write_mask=0x00F00000 (bits[23:20] W1C), reset=0x5
    MB_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x03FFFFFFu, 0x00F00000u, 0x5u),
        INBOUND_EMPTY     (reg_name + ".INBOUND_EMPTY",      *this,  0,  1),
        INBOUND_FULL      (reg_name + ".INBOUND_FULL",       *this,  1,  1),
        OUTBOUND_EMPTY    (reg_name + ".OUTBOUND_EMPTY",     *this,  2,  1),
        OUTBOUND_FULL     (reg_name + ".OUTBOUND_FULL",      *this,  3,  1),
        INBOUND_DEPTH     (reg_name + ".INBOUND_DEPTH",      *this,  4,  8),
        OUTBOUND_DEPTH    (reg_name + ".OUTBOUND_DEPTH",     *this, 12,  8),
        INBOUND_OVERFLOW  (reg_name + ".INBOUND_OVERFLOW",   *this, 20,  1),
        OUTBOUND_OVERFLOW (reg_name + ".OUTBOUND_OVERFLOW",  *this, 21,  1),
        INBOUND_UNDERFLOW (reg_name + ".INBOUND_UNDERFLOW",  *this, 22,  1),
        OUTBOUND_UNDERFLOW(reg_name + ".OUTBOUND_UNDERFLOW", *this, 23,  1),
        INBOUND_SEPARATOR (reg_name + ".INBOUND_SEPARATOR",  *this, 24,  1),
        OUTBOUND_SEPARATOR(reg_name + ".OUTBOUND_SEPARATOR", *this, 25,  1),
        reserved0         (reg_name + ".reserved0",          *this, 26,  6)
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

    csml_bitfield<N> INBOUND_EMPTY;      ///< [0]     Inbound FIFO empty
    csml_bitfield<N> INBOUND_FULL;       ///< [1]     Inbound FIFO full
    csml_bitfield<N> OUTBOUND_EMPTY;     ///< [2]     Outbound FIFO empty
    csml_bitfield<N> OUTBOUND_FULL;      ///< [3]     Outbound FIFO full
    csml_bitfield<N> INBOUND_DEPTH;      ///< [11:4]  Words in inbound FIFO
    csml_bitfield<N> OUTBOUND_DEPTH;     ///< [19:12] Words in outbound FIFO
    csml_bitfield<N> INBOUND_OVERFLOW;   ///< [20]    Inbound overflow occurred
    csml_bitfield<N> OUTBOUND_OVERFLOW;  ///< [21]    Outbound overflow occurred
    csml_bitfield<N> INBOUND_UNDERFLOW;  ///< [22]    Inbound underflow occurred
    csml_bitfield<N> OUTBOUND_UNDERFLOW; ///< [23]    Outbound underflow occurred
    csml_bitfield<N> INBOUND_SEPARATOR;  ///< [24]    Last KM-popped inbound word was separator
    csml_bitfield<N> OUTBOUND_SEPARATOR; ///< [25]    Last SEP-popped outbound word was separator
    csml_bitfield<N> reserved0;          ///< [31:26]
};

/**
 * MB_IRQEN — Mailbox Interrupt Enable  (SEP_IRQ_ENABLE)
 *
 * Enables individual interrupt sources to the SEP CPU.
 * Bit layout confirmed from km_mailbox_sep_regs.h.
 *
 * Access : Read-Write
 * Reset  : 0x00000000 (all disabled)
 */
template<unsigned int N>
class MB_IRQEN_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x1F, write_mask=0x1F, reset=0x0
    MB_IRQEN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1Fu, 0x1Fu, 0x0),
        OUTBOUND_READ_DATA_AVAIL_EN (reg_name + ".OUTBOUND_READ_DATA_AVAIL_EN",  *this, 0, 1),
        INBOUND_WRITE_SPACE_AVAIL_EN(reg_name + ".INBOUND_WRITE_SPACE_AVAIL_EN", *this, 1, 1),
        INBOUND_OVERFLOW_EN         (reg_name + ".INBOUND_OVERFLOW_EN",          *this, 2, 1),
        OUTBOUND_UNDERFLOW_EN       (reg_name + ".OUTBOUND_UNDERFLOW_EN",        *this, 3, 1),
        FLUSHED_BY_KM_EN            (reg_name + ".FLUSHED_BY_KM_EN",             *this, 4, 1),
        reserved0                   (reg_name + ".reserved0",                    *this, 5, 27)
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

    csml_bitfield<N> OUTBOUND_READ_DATA_AVAIL_EN;  ///< [0] Enable IRQ: KM wrote response to outbound
    csml_bitfield<N> INBOUND_WRITE_SPACE_AVAIL_EN; ///< [1] Enable IRQ: inbound FIFO has free space
    csml_bitfield<N> INBOUND_OVERFLOW_EN;          ///< [2] Enable IRQ: inbound FIFO overflow
    csml_bitfield<N> OUTBOUND_UNDERFLOW_EN;        ///< [3] Enable IRQ: outbound FIFO underflow
    csml_bitfield<N> FLUSHED_BY_KM_EN;             ///< [4] Enable IRQ: KM firmware flushed mailbox
    csml_bitfield<N> reserved0;                    ///< [31:5]
};

/**
 * MB_IRQS — Mailbox Interrupt Status  (SEP_IRQ_STATUS, Write-1-to-Clear)
 *
 * Sticky interrupt status flags. SEP writes 1 to clear each bit.
 * Bit layout confirmed from km_mailbox_sep_regs.h.
 *
 * Access : Read-Write (W1C)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MB_IRQS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x1F, write_mask=0x1C (bits[1:0] are level-sensitive RO, bits[4:2] are W1C), reset=0x0
    MB_IRQS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1Fu, 0x1Cu, 0x0),
        OUTBOUND_READ_DATA_AVAIL(reg_name + ".OUTBOUND_READ_DATA_AVAIL",  *this, 0, 1),
        INBOUND_WRITE_SPACE_AVAIL(reg_name + ".INBOUND_WRITE_SPACE_AVAIL",*this, 1, 1),
        INBOUND_OVERFLOW        (reg_name + ".INBOUND_OVERFLOW",          *this, 2, 1),
        OUTBOUND_UNDERFLOW      (reg_name + ".OUTBOUND_UNDERFLOW",        *this, 3, 1),
        FLUSHED_BY_KM           (reg_name + ".FLUSHED_BY_KM",             *this, 4, 1),
        reserved0               (reg_name + ".reserved0",                 *this, 5, 27)
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

    csml_bitfield<N> OUTBOUND_READ_DATA_AVAIL;  ///< [0] KM wrote response to outbound FIFO (W1C)
    csml_bitfield<N> INBOUND_WRITE_SPACE_AVAIL; ///< [1] Inbound FIFO has write space (W1C)
    csml_bitfield<N> INBOUND_OVERFLOW;          ///< [2] Write to full inbound FIFO (W1C)
    csml_bitfield<N> OUTBOUND_UNDERFLOW;        ///< [3] Read from empty outbound FIFO (W1C)
    csml_bitfield<N> FLUSHED_BY_KM;             ///< [4] KM firmware flushed mailbox (W1C)
    csml_bitfield<N> reserved0;                 ///< [31:5]
};

/**
 * MB_CTRL — Mailbox Control  (SEP_CTRL)
 *
 * Controls overflow/underflow response behavior and mailbox flush.
 * Bit layout confirmed from km_mailbox_sep_regs.h.
 *
 * Access : Read-Write
 * Reset  : 0x00000000
 */
template<unsigned int N>
class MB_CTRL_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x7, write_mask=0x7, reset=0x0
    MB_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x7u, 0x7u, 0x0),
        INBOUND_OVERFLOW_RESP (reg_name + ".INBOUND_OVERFLOW_RESP",  *this, 0, 1),
        OUTBOUND_UNDERFLOW_RESP(reg_name + ".OUTBOUND_UNDERFLOW_RESP",*this, 1, 1),
        FLUSH                 (reg_name + ".FLUSH",                  *this, 2, 1),
        reserved0             (reg_name + ".reserved0",              *this, 3, 29)
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

    csml_bitfield<N> INBOUND_OVERFLOW_RESP;  ///< [0] Configure overflow response behavior
    csml_bitfield<N> OUTBOUND_UNDERFLOW_RESP;///< [1] Configure underflow response behavior
    csml_bitfield<N> FLUSH;                  ///< [2] Write 1 to flush both inbound and outbound FIFOs
    csml_bitfield<N> reserved0;              ///< [31:3]
};


// ============================================================================
// KPVLP REGISTERS
// ============================================================================

/**
 * KPVLP_KEY_type — KPVLP Key Data Word
 *
 * Write-only. SEP writes key material for one 32-bit word of a KPV slot.
 * 32 slots × 16 words each = 512 instances.
 * Access conditioned on: unlock_sep=1 AND lock_write=0 for the target slot.
 *
 * Access : Write-Only (reads return 0 — key material must never be readable by SEP)
 * Reset  : 0x00000000
 */
template<unsigned int N>
class KPVLP_KEY_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0x0 (WO, key data must not be readable), write_mask=0xFFFFFFFF, reset=0x0
    KPVLP_KEY_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0xFFFFFFFF, 0x0),
        VAL(reg_name + ".VAL", *this, 0, 32)
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

    csml_bitfield<N> VAL; ///< [31:0] 32-bit key data word
};

/**
 * KPVLP_CTRL_type — KPVLP Slot Control Register
 *
 * Write-only. SEP sets policy fields for a KPV key slot via KPVLP.
 * One per slot (32 instances). Writable fields are a subset of the full
 * KPV slot control register (KM CPU-side has broader access).
 *
 * Access : Write-Only for SEP (reads return 0)
 * Reset  : 0x00000000
 *
 * Bit layout matches KPV slot control register (KeyManager.md):
 *   [3:0]   reserved  (lock_write, lock_use, unlock_sep, clear — KM CPU only)
 *   [6:4]   EXTEND    — zero-indexed consecutive-slot count for wide keys
 *   [8:7]   reserved
 *   [16:9]  DEST_VALID — bitmask: HMAC[9] KMAC[10] AES[11] OTBN[12] rsvd[16:13]
 *   [20:17] LAST_DWORD — last valid key word index [1..15]
 *   [31:21] reserved
 */
template<unsigned int N>
class KPVLP_CTRL_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // SEP-writable bits: EXTEND[6:4]=0x70, DEST_VALID[16:9]=0x1FE00, LAST_DWORD[20:17]=0x1E0000
    // write_mask = 0x70 | 0x1FE00 | 0x1E0000 = 0x1FFE70
    // read_mask  = 0x0  (write-only from SEP side)
    KPVLP_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0x1FFE70, 0x0),
        reserved0  (reg_name + ".reserved0",   *this,  0,  4),  // [3:0]  KM-CPU-only bits
        EXTEND     (reg_name + ".EXTEND",       *this,  4,  3),  // [6:4]
        reserved1  (reg_name + ".reserved1",    *this,  7,  2),  // [8:7]
        DEST_VALID (reg_name + ".DEST_VALID",   *this,  9,  8),  // [16:9]
        LAST_DWORD (reg_name + ".LAST_DWORD",   *this, 17,  4),  // [20:17]
        reserved2  (reg_name + ".reserved2",    *this, 21, 11)   // [31:21]
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

    csml_bitfield<N> reserved0;   ///< [3:0]   KM-CPU-only (lock_write, lock_use, unlock_sep, clear)
    csml_bitfield<N> EXTEND;      ///< [6:4]   Consecutive slots for wide keys (0=single slot)
    csml_bitfield<N> reserved1;   ///< [8:7]
    csml_bitfield<N> DEST_VALID;  ///< [16:9]  Permitted destination engines bitmask
    csml_bitfield<N> LAST_DWORD;  ///< [20:17] Last valid key word index [1..15]
    csml_bitfield<N> reserved2;   ///< [31:21]
};

/**
 * KPVLP_STATUS_type — KPVLP Slot Unlock Status
 *
 * Read-only. One bit per KPV slot indicating whether unlock_sep is set
 * (i.e. KM firmware has granted SEP access to that slot for provisioning).
 * SEP reads this before attempting a KPVLP write.
 *
 * Access : Read-Only
 * Reset  : 0x00000000 (no slots unlocked at reset)
 */
template<unsigned int N>
class KPVLP_STATUS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    // read_mask=0xFFFFFFFF (RO), write_mask=0x0, reset=0x0
    KPVLP_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x0, 0x0),
        UNLOCK_SEP(reg_name + ".UNLOCK_SEP", *this, 0, 32)
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

    csml_bitfield<N> UNLOCK_SEP; ///< [31:0] One bit per slot: 1 = slot is unlocked for SEP write
};

} // namespace keymgr_tt
