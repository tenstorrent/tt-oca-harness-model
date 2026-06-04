/**
 * @file km_kpv.h
 * @brief Key and Policy Vault (KPV) — internal storage for KM firmware.
 *
 * Pure C++ class (no SystemC, no TLM). Used exclusively by km_firmware_handler.
 *
 * The KPV holds 32 key slots × 512 bits (16 × 32-bit words) and one 32-bit
 * control register per slot. All access-control rules from the spec are
 * enforced here:
 *
 *   KM firmware access  : km_*() methods — full read/write subject to lock bits
 *   SEP KPVLP access    : kpvlp_*() methods — restricted write-only subject to
 *                         unlock_sep && !lock_write
 *
 * Control register bit layout (matches KeyManager.md):
 *   [0]     lock_write  — prevents KM firmware writes until reset (write-one-only)
 *   [1]     lock_use    — prevents KM firmware reads until reset (write-one-only)
 *   [2]     unlock_sep  — allows KPVLP writes (set by KM via CMD_KPVLP_SLOT_REQ)
 *   [3]     clear       — clears key to zero if !lock_write (self-clearing)
 *   [6:4]   extend      — number of extra consecutive slots for wide keys (e.g. RSA)
 *   [8:7]   reserved
 *   [16:9]  dest_valid  — permitted destination engine bitmask
 *   [20:17] last_dword  — last valid key word index [1..15]
 *   [31:21] reserved
 *
 * KPVLP may only write fields: extend [6:4], dest_valid [16:9], last_dword [20:17]
 * i.e. write mask = 0x001FFE70
 */
#pragma once
#include <cstdint>
#include <array>
#include <functional>

namespace keymgr_tt {

/// Callback type: returns one random 32-bit word from the DRBG
using drbg_fn_t = std::function<uint32_t()>;

// ----------------------------------------------------------------------------
// kpv_ctrl_t — decoded control register for one slot
// ----------------------------------------------------------------------------
struct kpv_ctrl_t {
    bool    lock_write  = false;  ///< [0]     prevents KM writes; write-one-only
    bool    lock_use    = false;  ///< [1]     prevents KM reads; write-one-only
    bool    unlock_sep  = false;  ///< [2]     grants SEP KPVLP access to this slot
    uint8_t extend      = 0;     ///< [6:4]   zero-indexed extra consecutive slots
    uint8_t dest_valid  = 0;     ///< [16:9]  permitted destination engines bitmask
    uint8_t last_dword  = 0;     ///< [20:17] last valid word index [1..15]

    uint32_t to_uint32()        const;
    void     from_uint32(uint32_t v);
};

// ----------------------------------------------------------------------------
// kpv_entry_t — one 512-bit key slot
// ----------------------------------------------------------------------------
struct kpv_entry_t {
    std::array<uint32_t, 16> words = {};
};

// ----------------------------------------------------------------------------
// km_kpv — Key and Policy Vault
// ----------------------------------------------------------------------------
class km_kpv {
public:
    static constexpr int NUM_SLOTS     = 32;
    static constexpr int WORDS_PER_KEY = 16;  ///< 512 bits per slot

    km_kpv() = default;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /// Reset all slots: clear data, default control, mark invalid.
    void reset();

    // -----------------------------------------------------------------------
    // KM firmware access  (km_* methods)
    // -----------------------------------------------------------------------

    /// Write one word to a key slot. Requires !lock_write.
    bool km_write_key_word(int slot, int word, uint32_t data);

    /// Read one word from a key slot. Requires !lock_use.
    /// @param out  receives the word value; set to 0 on locked read.
    /// @return true on success, false on lock_use or bad index.
    bool km_read_key_word(int slot, int word, uint32_t &out) const;

    /// Return the decoded control register for a slot (read-only view).
    const kpv_ctrl_t& ctrl(int slot) const;

    /// Assert lock_write on a slot (write-one-only; no unlock until reset).
    bool km_lock_write(int slot);

    /// Assert lock_use on a slot (write-one-only; no unlock until reset).
    bool km_lock_use(int slot);

    /// Grant KPVLP access to `count` consecutive slots starting at `base_slot`.
    /// Sets unlock_sep on each slot.
    bool km_grant_kpvlp(int base_slot, int count);

    /// Clear key data to all-zero. Requires !lock_write.
    bool km_clear_key(int slot);

    /// Find the first base index for `count` consecutive free
    /// (!lock_write && !valid) slots. Returns -1 if none found.
    int km_find_free_slots(int count) const;

    /// Write ctrl policy fields (extend, dest_valid, last_dword) via KM firmware
    /// internal path.  Does NOT require unlock_sep; bypasses the KPVLP access check.
    /// Used by CMD_KEY_GENERATE after writing DRBG key material.
    /// Requires !lock_write.  Returns false if slot is out of range or lock_write=1.
    bool km_write_ctrl(int slot, uint8_t extend, uint8_t dest_valid, uint8_t last_dword);

    /// Mark a slot as containing a valid key (set by firmware after key write).
    void km_set_valid(int slot, bool valid);
    bool km_is_valid(int slot) const;

    // -----------------------------------------------------------------------
    // KPVLP SEP-facing access  (kpvlp_* methods)
    // -----------------------------------------------------------------------

    /// Write one key word via the KPVLP port.
    /// Conditions: unlock_sep=1 AND lock_write=0 for the target slot.
    bool kpvlp_write_key_word(int slot, int word, uint32_t data);

    /// Write ctrl fields (EXTEND, DEST_VALID, LAST_DWORD only) via KPVLP.
    /// Conditions: unlock_sep=1 AND lock_write=0.
    /// Bits outside the KPVLP write mask (0x001FFE70) are silently ignored.
    bool kpvlp_write_ctrl(int slot, uint32_t ctrl_word);

    /// Return KPVLP_STATUS: bitmask of unlock_sep across all 32 slots.
    /// Bit i = 1 means slot i is currently accessible via KPVLP.
    uint32_t kpvlp_status() const;

private:
    std::array<kpv_entry_t, NUM_SLOTS> m_keys  = {};
    std::array<kpv_ctrl_t,  NUM_SLOTS> m_ctrl  = {};
    std::array<bool,        NUM_SLOTS> m_valid  = {};

    bool valid_slot(int s) const { return s >= 0 && s < NUM_SLOTS; }
    bool valid_word(int w) const { return w >= 0 && w < WORDS_PER_KEY; }
};

} // namespace keymgr_tt
