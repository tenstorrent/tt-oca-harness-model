#include "km_kpv.h"

namespace keymgr_tt {

// ============================================================================
// kpv_ctrl_t helpers
// ============================================================================

uint32_t kpv_ctrl_t::to_uint32() const
{
    uint32_t v = 0;
    if (lock_write) v |= (1u << 0);
    if (lock_use)   v |= (1u << 1);
    if (unlock_sep) v |= (1u << 2);
    v |= (static_cast<uint32_t>(extend)     & 0x7u)  << 4;   // [6:4]
    v |= (static_cast<uint32_t>(dest_valid) & 0xFFu) << 9;   // [16:9]
    v |= (static_cast<uint32_t>(last_dword) & 0xFu)  << 17;  // [20:17]
    return v;
}

void kpv_ctrl_t::from_uint32(uint32_t v)
{
    lock_write = (v >> 0)  & 0x1u;
    lock_use   = (v >> 1)  & 0x1u;
    unlock_sep = (v >> 2)  & 0x1u;
    extend     = (v >> 4)  & 0x7u;
    dest_valid = (v >> 9)  & 0xFFu;
    last_dword = (v >> 17) & 0xFu;
}

// ============================================================================
// km_kpv — lifecycle
// ============================================================================

void km_kpv::reset()
{
    for (int i = 0; i < NUM_SLOTS; i++) {
        m_keys[i].words.fill(0);
        m_ctrl[i] = kpv_ctrl_t{};
        m_valid[i] = false;
    }
}

// ============================================================================
// km_kpv — KM firmware access
// ============================================================================

bool km_kpv::km_write_key_word(int slot, int word, uint32_t data)
{
    if (!valid_slot(slot) || !valid_word(word)) return false;
    if (m_ctrl[slot].lock_write) return false;
    m_keys[slot].words[word] = data;
    return true;
}

bool km_kpv::km_read_key_word(int slot, int word, uint32_t &out) const
{
    if (!valid_slot(slot) || !valid_word(word)) { out = 0; return false; }
    if (m_ctrl[slot].lock_use) { out = 0; return false; }
    out = m_keys[slot].words[word];
    return true;
}

const kpv_ctrl_t& km_kpv::ctrl(int slot) const
{
    // Caller is responsible for passing a valid slot index.
    return m_ctrl[slot];
}

bool km_kpv::km_lock_write(int slot)
{
    if (!valid_slot(slot)) return false;
    m_ctrl[slot].lock_write = true;  // write-one-only: never cleared until reset
    return true;
}

bool km_kpv::km_lock_use(int slot)
{
    if (!valid_slot(slot)) return false;
    m_ctrl[slot].lock_use = true;    // write-one-only: never cleared until reset
    return true;
}

bool km_kpv::km_grant_kpvlp(int base_slot, int count)
{
    if (base_slot < 0 || count <= 0 || base_slot + count > NUM_SLOTS) return false;
    for (int i = base_slot; i < base_slot + count; i++)
        m_ctrl[i].unlock_sep = true;
    return true;
}

bool km_kpv::km_clear_key(int slot)
{
    if (!valid_slot(slot)) return false;
    if (m_ctrl[slot].lock_write) return false;
    m_keys[slot].words.fill(0);
    m_valid[slot] = false;
    return true;
}

int km_kpv::km_find_free_slots(int count) const
{
    if (count <= 0 || count > NUM_SLOTS) return -1;
    for (int base = 0; base <= NUM_SLOTS - count; base++) {
        bool ok = true;
        for (int i = base; i < base + count; i++) {
            if (m_ctrl[i].lock_write || m_valid[i]) { ok = false; break; }
        }
        if (ok) return base;
    }
    return -1;
}

bool km_kpv::km_write_ctrl(int slot, uint8_t extend, uint8_t dest_valid, uint8_t last_dword)
{
    if (!valid_slot(slot)) return false;
    if (m_ctrl[slot].lock_write) return false;  // respect lock_write even for KM path

    m_ctrl[slot].extend     = extend    & 0x7u;
    m_ctrl[slot].dest_valid = dest_valid & 0xFFu;
    m_ctrl[slot].last_dword = last_dword & 0xFu;
    return true;
}

void km_kpv::km_set_valid(int slot, bool valid)
{
    if (valid_slot(slot)) m_valid[slot] = valid;
}

bool km_kpv::km_is_valid(int slot) const
{
    return valid_slot(slot) && m_valid[slot];
}

// ============================================================================
// km_kpv — KPVLP SEP-facing access
// ============================================================================

bool km_kpv::kpvlp_write_key_word(int slot, int word, uint32_t data)
{
    if (!valid_slot(slot) || !valid_word(word)) return false;
    if (!m_ctrl[slot].unlock_sep)  return false;
    if ( m_ctrl[slot].lock_write)  return false;
    m_keys[slot].words[word] = data;
    return true;
}

bool km_kpv::kpvlp_write_ctrl(int slot, uint32_t ctrl_word)
{
    if (!valid_slot(slot))         return false;
    if (!m_ctrl[slot].unlock_sep)  return false;
    if ( m_ctrl[slot].lock_write)  return false;

    // Only update KPVLP-writable fields (mask = 0x001FFE70):
    //   extend     [6:4]   = bits  6..4
    //   dest_valid [16:9]  = bits 16..9
    //   last_dword [20:17] = bits 20..17
    m_ctrl[slot].extend     = (ctrl_word >> 4)  & 0x7u;
    m_ctrl[slot].dest_valid = (ctrl_word >> 9)  & 0xFFu;
    m_ctrl[slot].last_dword = (ctrl_word >> 17) & 0xFu;
    return true;
}

uint32_t km_kpv::kpvlp_status() const
{
    uint32_t status = 0;
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (m_ctrl[i].unlock_sep)
            status |= (1u << i);
    }
    return status;
}

} // namespace keymgr_tt
