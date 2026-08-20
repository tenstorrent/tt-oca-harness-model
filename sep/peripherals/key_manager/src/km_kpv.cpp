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
    v |= (static_cast<uint32_t>(extend)     & 0x7u)  << 4;   // [6:4]
    v |= (static_cast<uint32_t>(last_dword) & 0xFu)  << 17;  // [20:17]
    return v;
}

void kpv_ctrl_t::from_uint32(uint32_t v)
{
    lock_write = (v >> 0)  & 0x1u;
    lock_use   = (v >> 1)  & 0x1u;
    extend     = (v >> 4)  & 0x7u;
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

bool km_kpv::km_clear_key(int slot)
{
    if (!valid_slot(slot)) return false;
    if (m_ctrl[slot].lock_write) return false;
    m_keys[slot].words.fill(0);
    m_valid[slot] = false;
    return true;
}

bool km_kpv::km_erase_key(int slot, const drbg_fn_t& get_random)
{
    if (!valid_slot(slot)) return false;
    // Erase bypasses lock_write and lock_use: in hardware the LFSR overwrite is
    // wired ahead of the lock checks so a locked slot can still be reclaimed.
    for (int w = 0; w < WORDS_PER_KEY; w++)
        m_keys[slot].words[w] = get_random ? get_random() : 0u;
    m_ctrl[slot]  = kpv_ctrl_t{};
    m_valid[slot] = false;
    return true;
}

int km_kpv::km_find_free_slots(int count, const drbg_fn_t& get_random) const
{
    if (count <= 0 || count > NUM_SLOTS) return -1;

    auto available = [this](int s) {
        return !m_ctrl[s].lock_write && !m_ctrl[s].lock_use && !m_valid[s];
    };

    const int start = get_random ? static_cast<int>(get_random() % NUM_SLOTS) : 0;

    for (int tried = 0; tried < NUM_SLOTS; tried++) {
        const int base = (start + tried) % NUM_SLOTS;
        if (base + count > NUM_SLOTS) continue;  // a run never wraps the vault

        bool ok = true;
        for (int i = base; i < base + count; i++) {
            if (!available(i)) { ok = false; break; }
        }
        if (ok) return base;
    }
    return -1;
}

bool km_kpv::km_write_ctrl(int slot, uint8_t extend, uint8_t last_dword)
{
    if (!valid_slot(slot)) return false;
    if (m_ctrl[slot].lock_write) return false;  // respect lock_write even for KM path

    m_ctrl[slot].extend     = extend     & 0x7u;
    m_ctrl[slot].last_dword = last_dword & 0xFu;
    return true;
}

// ----------------------------------------------------------------------------
// Multi-slot key operations
// ----------------------------------------------------------------------------

int km_kpv::slots_for_words(int len)
{
    if (len <= 0 || len > MAX_KEY_WORDS) return 0;
    return (len - 1) / WORDS_PER_KEY + 1;
}

bool km_kpv::km_write_key(int base, const uint32_t* words, int len)
{
    const int num_slots = slots_for_words(len);
    if (num_slots == 0 || words == nullptr) return false;
    if (!valid_slot(base) || base + num_slots > NUM_SLOTS) return false;

    // Check every slot before mutating any, so a rejected write leaves no
    // partially populated key behind.
    for (int s = base; s < base + num_slots; s++)
        if (m_ctrl[s].lock_write) return false;

    const uint8_t extend = static_cast<uint8_t>(num_slots - 1);

    int written = 0;
    for (int i = 0; i < num_slots; i++) {
        const int  slot     = base + i;
        const bool is_final = (i == num_slots - 1);

        // Only the base slot advertises the span; the rest report a span of one.
        const uint8_t slot_extend = (i == 0) ? extend : 0u;

        uint8_t last_dword;
        if (is_final) {
            const int rem = len % WORDS_PER_KEY;
            last_dword = static_cast<uint8_t>((rem == 0) ? WORDS_PER_KEY - 1 : rem - 1);
        } else {
            last_dword = static_cast<uint8_t>(WORDS_PER_KEY - 1);
        }

        m_ctrl[slot].extend     = slot_extend & 0x7u;
        m_ctrl[slot].last_dword = last_dword  & 0xFu;

        const int words_in_slot = is_final ? (len - written) : WORDS_PER_KEY;
        for (int w = 0; w < words_in_slot; w++)
            m_keys[slot].words[w] = words[written + w];

        written += words_in_slot;
    }
    return true;
}

bool km_kpv::km_read_key(int base, std::vector<uint32_t>& out) const
{
    out.clear();
    if (!valid_slot(base)) return false;

    const int num_slots = m_ctrl[base].extend + 1;
    if (base + num_slots > NUM_SLOTS) return false;

    // A locked slot anywhere in the span makes the whole key unreadable.
    for (int s = base; s < base + num_slots; s++)
        if (m_ctrl[s].lock_use) return false;

    // Every slot but the last must be full, otherwise the geometry is corrupt.
    for (int s = base; s < base + num_slots - 1; s++)
        if (m_ctrl[s].last_dword != WORDS_PER_KEY - 1) return false;

    const int len = WORDS_PER_KEY * (num_slots - 1)
                  + m_ctrl[base + num_slots - 1].last_dword + 1;

    out.reserve(static_cast<size_t>(len));
    int read = 0;
    for (int i = 0; i < num_slots; i++) {
        const int words_in_slot = (i == num_slots - 1) ? (len - read) : WORDS_PER_KEY;
        for (int w = 0; w < words_in_slot; w++)
            out.push_back(m_keys[base + i].words[w]);
        read += words_in_slot;
    }
    return true;
}

void km_kpv::km_write_lock_span(int base)
{
    if (!valid_slot(base)) return;
    const int num_slots = m_ctrl[base].extend + 1;
    for (int s = base; s < base + num_slots && s < NUM_SLOTS; s++)
        m_ctrl[s].lock_write = true;
}

void km_kpv::km_erase_span(int base, const drbg_fn_t& get_random)
{
    if (!valid_slot(base)) return;
    // Latch the span first: erasing the base slot clears its EXTEND field.
    const int num_slots = m_ctrl[base].extend + 1;
    for (int s = base; s < base + num_slots && s < NUM_SLOTS; s++)
        km_erase_key(s, get_random);
}

void km_kpv::km_set_valid(int slot, bool valid)
{
    if (valid_slot(slot)) m_valid[slot] = valid;
}

bool km_kpv::km_is_valid(int slot) const
{
    return valid_slot(slot) && m_valid[slot];
}

} // namespace keymgr_tt
