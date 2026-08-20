#include "km_mailbox.h"

namespace keymgr_tt {

// ============================================================================
// Lifecycle
// ============================================================================

void km_mailbox::reset()
{
    m_inbound.clear();
    m_outbound.clear();

    m_inbound_msgs       = 0;
    m_outbound_msgs      = 0;
    m_wsep_pending       = false;
    m_inbound_last_sep   = false;
    m_outbound_last_sep  = false;
    m_inbound_overflow   = false;
    m_outbound_overflow  = false;
    m_inbound_underflow  = false;
    m_outbound_underflow = false;
    m_flushed_by_km      = false;
}

// ============================================================================
// SEP-side access
// ============================================================================

bool km_mailbox::sep_write_wdata(uint32_t word)
{
    if (inbound_full()) {
        m_inbound_overflow = true;
        return false;
    }

    // If MB_WSEP.set was written before this word, mark it as separator
    bool is_sep = m_wsep_pending;
    m_wsep_pending = false;  // hardware clears after use

    m_inbound.push_back({word, is_sep});

    if (is_sep) m_inbound_msgs++;

    // Wake the firmware on every word, not just on the separator. In hardware
    // INBOUND_READ_DATA_AVAIL tracks FIFO occupancy, and the firmware drains as
    // words arrive — which is what lets a message longer than the FIFO through.
    m_inbound_ready.notify(SC_ZERO_TIME);
    return true;
}

bool km_mailbox::sep_read_rdata(uint32_t &out)
{
    if (m_outbound.empty()) {
        out = 0;
        m_outbound_last_sep  = false;
        m_outbound_underflow = true;
        return false;
    }

    mb_word_t w = m_outbound.front();
    m_outbound.pop_front();

    if (w.separator && m_outbound_msgs > 0)
        m_outbound_msgs--;

    out = w.data;
    m_outbound_last_sep = w.separator;
    return true;
}

void km_mailbox::sep_flush()
{
    m_inbound.clear();
    m_inbound_msgs       = 0;
    m_wsep_pending       = false;
    m_inbound_last_sep   = false;
    m_inbound_overflow   = false;
    m_inbound_underflow  = false;

    m_outbound.clear();
    m_outbound_msgs      = 0;
    m_outbound_last_sep  = false;
    m_outbound_overflow  = false;
    m_outbound_underflow = false;
}

// ============================================================================
// KM firmware access
// ============================================================================

bool km_mailbox::km_message_ready() const
{
    return m_inbound_msgs > 0;
}

bool km_mailbox::km_pop_word(mb_word_t &out)
{
    if (m_inbound.empty()) {
        m_inbound_underflow = true;
        return false;
    }

    out = m_inbound.front();
    m_inbound.pop_front();

    if (out.separator && m_inbound_msgs > 0)
        m_inbound_msgs--;

    m_inbound_last_sep = out.separator;
    return true;
}

bool km_mailbox::km_push_word(uint32_t data, bool separator)
{
    if (outbound_full()) {
        m_outbound_overflow = true;
        return false;
    }

    m_outbound.push_back({data, separator});

    if (separator)
        m_outbound_msgs++;
    return true;
}

// ============================================================================
// Status queries
// ============================================================================

bool km_mailbox::inbound_empty() const
{
    return m_inbound.empty();
}

bool km_mailbox::inbound_full() const
{
    return m_inbound.size() >= FIFO_DEPTH;
}

bool km_mailbox::outbound_empty() const
{
    return m_outbound.empty();
}

bool km_mailbox::outbound_full() const
{
    return m_outbound.size() >= FIFO_DEPTH;
}

unsigned int km_mailbox::inbound_count() const
{
    return static_cast<unsigned int>(m_inbound.size());
}

unsigned int km_mailbox::outbound_count() const
{
    return static_cast<unsigned int>(m_outbound.size());
}

// ============================================================================
// KM firmware flush
// ============================================================================

void km_mailbox::km_flush()
{
    m_inbound.clear();
    m_inbound_msgs       = 0;
    m_wsep_pending       = false;
    m_inbound_last_sep   = false;
    m_inbound_overflow   = false;
    m_inbound_underflow  = false;

    m_outbound.clear();
    m_outbound_msgs      = 0;
    m_outbound_last_sep  = false;
    m_outbound_overflow  = false;
    m_outbound_underflow = false;

    m_flushed_by_km = true;  // picked up by fw_thread to raise FLUSHED_BY_KM IRQ
}

} // namespace keymgr_tt
