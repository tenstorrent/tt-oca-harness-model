/**
 * @file km_mailbox.h
 * @brief Key Manager TT — Internal Mailbox model.
 *
 * Models the dual-FIFO mailbox between SEP host and KM firmware.
 *
 * Two channels:
 *   Inbound  (SEP → KM)  : SEP writes words via MB_WDATA; KM firmware reads them
 *   Outbound (KM → SEP)  : KM firmware writes words; SEP reads via MB_RDATA
 *
 * Message framing (from KeyManager.md / mailbox diagram 2026-03-04):
 *   - Each word is tagged with a separator flag (bit 31 of MB_WDATA/MB_RDATA)
 *   - When SEP or KM sets separator=1 on the last word of a message, the
 *     receiver is notified that a complete message is available.
 *
 * Usage in the model:
 *   - key_manager_model's b_transport override calls sep_write_wdata() /
 *     sep_read_rdata() when SEP accesses MB_WDATA / MB_RDATA.
 *   - km_firmware_handler waits on inbound_msg_event() and calls km_pop_word()
 *     to dequeue inbound messages; calls km_push_word() to send responses.
 *   - After km_push_word(), the model drives irq if MB_IRQEN.OUTBOUND_READY=1.
 *
 * This class uses sc_event for firmware-thread notification and therefore
 * must be constructed during SystemC elaboration (before sc_start).
 */
#pragma once
#include <cstdint>
#include <deque>
#include <systemc.h>

namespace keymgr_tt {

/// One word in the mailbox FIFO (data + separator flag)
struct mb_word_t {
    uint32_t data;       ///< Payload word (bits [30:0] of MB_WDATA / MB_RDATA)
    bool     separator;  ///< True on the last word of a framed message
};

// ----------------------------------------------------------------------------
// km_mailbox
// ----------------------------------------------------------------------------
class km_mailbox {
public:
    static constexpr unsigned int FIFO_DEPTH = 16;  ///< Max words per FIFO (from rom_defs.h ROM_KM_MAILBOX_FIFO_DEPTH)

    km_mailbox() = default;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /// Reset both FIFOs and all counters.
    void reset();

    // -----------------------------------------------------------------------
    // SEP-side access  (called from b_transport override in key_manager_model)
    // -----------------------------------------------------------------------

    /// SEP writes one word to MB_WDATA (full 32-bit data, no separator in word).
    /// Enqueues the word in the inbound FIFO with separator=false.
    /// @return false if FIFO is full (word is dropped and error flag is set).
    bool sep_write_wdata(uint32_t word);

    /// SEP writes MB_WSEP.set=1 before pushing the last word of a message.
    /// The next sep_write_wdata() call will mark that word as separator,
    /// then automatically clear this flag.
    void sep_set_wsep() { m_wsep_pending = true; }

    /// SEP reads one word from MB_RDATA (full 32-bit data, no separator in word).
    /// Pops one word from the outbound FIFO.
    /// After the call, outbound_last_was_sep() indicates if it was the last word.
    /// @param out  receives the full 32-bit popped word; 0 if empty.
    /// @return false if outbound FIFO is empty.
    bool sep_read_rdata(uint32_t &out);

    /// True if the most recent sep_read_rdata() call returned a separator word.
    bool outbound_last_was_sep() const { return m_outbound_last_sep; }

    /// True if the most recent km_pop_word() call returned a separator word.
    bool inbound_last_was_sep() const { return m_inbound_last_sep; }

    /// SEP writes MB_CTRL.FLUSH=1 → discard all words in both FIFOs.
    void sep_flush();

    // -----------------------------------------------------------------------
    // KM firmware flush  (KM-initiated flush, sets FLUSHED_BY_KM flag)
    // -----------------------------------------------------------------------

    /// KM firmware flushes both FIFOs (security/error response).
    /// Sets flushed_by_km_pending() so the model can raise MB_IRQS.FLUSHED_BY_KM[4].
    void km_flush();

    /// True when KM firmware has flushed since last cleared.
    bool flushed_by_km_pending() const { return m_flushed_by_km; }

    /// Clear the KM-flush pending flag (called after raising IRQ).
    void clear_flushed_by_km() { m_flushed_by_km = false; }

    // -----------------------------------------------------------------------
    // KM firmware access  (called from km_firmware_handler)
    // -----------------------------------------------------------------------

    /// True when at least one complete message (separator seen) is in the
    /// inbound FIFO.
    bool km_message_ready() const;

    /// Pop one word from the inbound FIFO.
    /// @param out  receives the dequeued word.
    /// @return false if inbound FIFO is empty.
    bool km_pop_word(mb_word_t &out);

    /// Push one word into the outbound FIFO (KM → SEP response).
    /// If separator=1 this is the last word of a message; notifies SEP via
    /// outbound_data_event().
    /// @return false if outbound FIFO is full.
    bool km_push_word(uint32_t data, bool separator = false);

    // -----------------------------------------------------------------------
    // Status queries  (used to update MB_STATUS register)
    // -----------------------------------------------------------------------

    bool         inbound_empty()  const;
    bool         inbound_full()   const;
    bool         outbound_empty() const;
    bool         outbound_full()  const;
    unsigned int inbound_count()  const;
    unsigned int outbound_count() const;

    /// Overflow/underflow flags (sticky, set on error, cleared on flush or explicit clear)
    bool inbound_overflow_pending()   const { return m_inbound_overflow; }
    bool outbound_overflow_pending()  const { return m_outbound_overflow; }
    bool inbound_underflow_pending()  const { return m_inbound_underflow; }
    bool outbound_underflow_pending() const { return m_outbound_underflow; }
    void clear_inbound_overflow()           { m_inbound_overflow   = false; }
    void clear_outbound_overflow()          { m_outbound_overflow  = false; }
    void clear_inbound_underflow()          { m_inbound_underflow  = false; }
    void clear_outbound_underflow()         { m_outbound_underflow = false; }

    // -----------------------------------------------------------------------
    // SystemC events  (firmware thread waits on these)
    // -----------------------------------------------------------------------

    /// Notified when a complete inbound message arrives (separator seen).
    sc_event& inbound_msg_event() { return m_inbound_ready; }

private:
    std::deque<mb_word_t> m_inbound;              ///< SEP → KM (deque for back() access)
    std::deque<mb_word_t> m_outbound;             ///< KM → SEP
    unsigned int          m_inbound_msgs      = 0;
    unsigned int          m_outbound_msgs     = 0;
    bool                  m_wsep_pending       = false; ///< next inbound write will be separator
    bool                  m_inbound_last_sep   = false; ///< last km_pop_word was separator
    bool                  m_outbound_last_sep  = false; ///< last sep_read_rdata was separator
    bool                  m_inbound_overflow   = false; ///< SEP wrote to full inbound FIFO
    bool                  m_outbound_overflow  = false; ///< KM wrote to full outbound FIFO
    bool                  m_inbound_underflow  = false; ///< KM read from empty inbound FIFO
    bool                  m_outbound_underflow = false; ///< SEP read from empty outbound FIFO
    bool                  m_flushed_by_km      = false; ///< KM firmware initiated a flush

    sc_event m_inbound_ready;   ///< pulsed when inbound message complete
};

} // namespace keymgr_tt
