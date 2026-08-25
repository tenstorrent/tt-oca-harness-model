// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file km_kpv.h
 * @brief Key and Policy Vault (KPV) — internal storage for KM firmware.
 *
 * Pure C++ class (no SystemC, no TLM). Used exclusively by km_firmware_handler.
 *
 * The KPV holds 32 key slots × 512 bits (16 × 32-bit words) and one 32-bit
 * control register per slot. The vault sits behind the KM CPU's private
 * crossbar, so SEP has no direct access path at all — every entry point here is
 * a KM-firmware one.
 *
 * Control register bit layout (km_kpv.rdl KEY_CTRL):
 *   [0]     lock_write  — blocks key data writes until reset (write-one-to-set)
 *   [1]     lock_use    — blocks key data reads until reset (write-one-to-set)
 *   [2]     erase       — overwrites all 16 words and clears ctrl (self-clearing)
 *   [3]     reserved
 *   [6:4]   extend      — number of extra consecutive slots for wide keys
 *   [16:7]  reserved
 *   [20:17] last_dword  — last valid key word index [0..15]
 *   [31:21] reserved
 *
 * Sideload destination permissions are deliberately absent: in hardware the
 * KPV stores opaque blobs and the permitted destinations live in the firmware
 * key registry, so km_firmware_handler owns them.
 */
#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <functional>

namespace keymgr_tt {

/// Callback type: returns one random 32-bit word from the DRBG
using drbg_fn_t = std::function<uint32_t()>;

// ----------------------------------------------------------------------------
// kpv_ctrl_t — decoded control register for one slot
// ----------------------------------------------------------------------------
struct kpv_ctrl_t {
    bool    lock_write  = false;  ///< [0]     blocks key data writes; write-one-to-set
    bool    lock_use    = false;  ///< [1]     blocks key data reads; write-one-to-set
    uint8_t extend      = 0;     ///< [6:4]   zero-indexed extra consecutive slots
    uint8_t last_dword  = 0;     ///< [20:17] last valid word index [0..15]

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

    /// Widest key the firmware will place in the vault, in 32-bit words.
    /// The wire encoding is word-count-minus-1 in 7 bits, so 128 is the ceiling
    /// (ROM_KM_MAX_KEY_WORDS).
    static constexpr int MAX_KEY_WORDS = 128;

    /// Slots a single key may span (ROM_KM_MAX_KEY_WORDS / WORDS_PER_KEY).
    static constexpr int MAX_SLOTS_PER_KEY = MAX_KEY_WORDS / WORDS_PER_KEY;

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

    /// Clear key data to all-zero. Requires !lock_write.
    bool km_clear_key(int slot);

    /// Hardware ERASE (ctrl bit 2): overwrite all 16 words with DRBG output and
    /// clear the slot's control register. Not gated by lock_write or lock_use,
    /// matching the RTL erase path. `get_random` supplies the fill words; when
    /// null the slot is zero-filled.
    bool km_erase_key(int slot, const drbg_fn_t& get_random = nullptr);

    /// Find a base index for `count` consecutive available slots, where a slot
    /// is available when it is unlocked (neither lock_write nor lock_use) and
    /// unassigned. Mirrors slot_available() in rom_keymgmt.c.
    ///
    /// When `get_random` is supplied the scan starts at a random index and wraps
    /// once, as the firmware does; a run is never allowed to straddle the end of
    /// the vault. Without it the scan starts at 0, which keeps tests reproducible.
    /// Returns -1 if no run is available.
    int km_find_free_slots(int count, const drbg_fn_t& get_random = nullptr) const;

    /// Write ctrl geometry fields (extend, last_dword) from KM firmware.
    /// Requires !lock_write.  Returns false if slot is out of range or lock_write=1.
    bool km_write_ctrl(int slot, uint8_t extend, uint8_t last_dword);

    // -----------------------------------------------------------------------
    // Multi-slot key operations  (mirror rom_kpv.c)
    //
    // A key of `len` words occupies ceil(len/16) consecutive slots. The base
    // slot carries EXTEND = (len-1)/16; the others carry 0. LAST_DWORD is 15 on
    // every slot but the final one, which records the index of the last word it
    // holds. Reading a key reconstructs its length from those two fields alone.
    // -----------------------------------------------------------------------

    /// Slots spanned by a key of `len` words. 0 if `len` is out of range.
    static int slots_for_words(int len);

    /// Write a key across consecutive slots starting at `base`, setting the
    /// EXTEND and LAST_DWORD geometry on each. Fails if `len` is out of range or
    /// any spanned slot is write-locked, in which case nothing is written.
    bool km_write_key(int base, const uint32_t* words, int len);

    /// Read a key back, using the base slot's EXTEND and the final slot's
    /// LAST_DWORD to recover the length. Fails if any spanned slot has lock_use
    /// set or if a non-final slot's LAST_DWORD is not 15.
    bool km_read_key(int base, std::vector<uint32_t>& out) const;

    /// Assert lock_write across every slot spanned by the key at `base`.
    void km_write_lock_span(int base);

    /// Erase every slot spanned by the key at `base`. The span is read before
    /// the first erase, since erasing clears EXTEND.
    void km_erase_span(int base, const drbg_fn_t& get_random = nullptr);

    /// Mark a slot as containing a valid key (set by firmware after key write).
    void km_set_valid(int slot, bool valid);
    bool km_is_valid(int slot) const;

private:
    std::array<kpv_entry_t, NUM_SLOTS> m_keys  = {};
    std::array<kpv_ctrl_t,  NUM_SLOTS> m_ctrl  = {};
    std::array<bool,        NUM_SLOTS> m_valid  = {};

    bool valid_slot(int s) const { return s >= 0 && s < NUM_SLOTS; }
    bool valid_word(int w) const { return w >= 0 && w < WORDS_PER_KEY; }
};

} // namespace keymgr_tt
