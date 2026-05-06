#include "keymgr_tt.h"
#include <cstring>
#include <openssl/rand.h>

keymgr_tt_model::keymgr_tt_model(sc_module_name name, int log_verbosity)
  : keymgr_tt_base(name),
    rst_ni("rst_ni"),
    wipe_ni("wipe_ni"),
    irq("irq"),
    hmac_key_socket("hmac_key_socket"),
    kmac_key_socket("kmac_key_socket"),
    aes_key_socket("aes_key_socket"),
    otbn_key_socket("otbn_key_socket"),
    verbosity("verbosity", log_verbosity)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    // Build firmware handler with DRBG and key-transfer callbacks
    m_firmware = std::make_unique<keymgr_tt::km_firmware_handler>(
        m_kpv,
        m_mailbox,
        [this]() { return this->get_random_word(); }
    );

    // Wire key-transfer callback to actual TLM socket writes
    m_firmware->set_key_transfer_callback(
        [this](uint8_t dest_mask, const uint32_t* words, int count, bool set_valid) {
            return this->key_transfer_via_socket(dest_mask, words, count, set_valid);
        }
    );

    SC_METHOD(reset_process);
    sensitive << rst_ni;
    dont_initialize();

    SC_METHOD(wipe_process);
    sensitive << wipe_ni;
    dont_initialize();

    SC_METHOD(irq_update_process);
    sensitive << m_irq_update_event;

    SC_THREAD(fw_thread);

    register_all_callbacks();
}

// =============================================================================
// SC_METHOD: reset
// =============================================================================

void keymgr_tt_model::reset_process()
{
    if (!rst_ni.read()) {
        reset_all_registers();
        m_kpv.reset();
        m_mailbox.reset();
        m_firmware->reset();
        m_irq_update_event.notify(SC_ZERO_TIME);  // drives irq low via irq_update_process
        CSML_INFO(1, logger) << "keymgr_tt: reset asserted — all registers cleared";
    }
}

// =============================================================================
// SC_METHOD: emergency wipe
// =============================================================================

void keymgr_tt_model::wipe_process()
{
    if (!wipe_ni.read()) {  // active-low: asserted when low
        CSML_INFO(1, logger) << "keymgr_tt: emergency wipe triggered";

        // Clear all KPV slots (flags + validity).
        m_kpv.reset();

        // Clear all four crypto engine key stores with zeros once.
        static const std::vector<uint32_t> zero_words(keymgr_tt::km_kpv::WORDS_PER_KEY, 0u);
        key_transfer_via_socket(
            keymgr_tt::km_firmware_handler::DEST_HMAC |
            keymgr_tt::km_firmware_handler::DEST_KMAC |
            keymgr_tt::km_firmware_handler::DEST_AES  |
            keymgr_tt::km_firmware_handler::DEST_OTBN,
            zero_words.data(),
            keymgr_tt::km_kpv::WORDS_PER_KEY,
            false);  // emergency wipe: clear key_valid (KEY_CTRL=0)

        // Notify SEP: RESP_UNRECOVERABLE_FAULT(WIPE_STATE=-1) then halt firmware.
        // Mirrors rom_isr.c wipe ISR behaviour: send fault, then enter halted state.
        m_firmware->send_wipe_fault();

        // send_wipe_fault() calls km_flush() internally which sets flushed_by_km_pending().
        // fw_thread is blocked on wait() and won't pick this up until the next inbound event,
        // so assert FLUSHED_BY_KM IRQ (bit 4) here directly.
        if (m_mailbox.flushed_by_km_pending()) {
            uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
            irqs |= (1u << 4);
            MB_IRQS = irqs;
            m_mailbox.clear_flushed_by_km();
        }
        update_level_irq_bits();

        CSML_INFO(1, logger) << "keymgr_tt: emergency wipe complete";
    }
}

// =============================================================================
// SC_THREAD: firmware boot + message loop
// =============================================================================

void keymgr_tt_model::fw_thread()
{
    while (true) {
        // Level-sensitive: wait while in reset (handles rst_ni already high at start)
        while (!rst_ni.read())
            wait(rst_ni.value_changed_event());

        CSML_INFO(1, logger) << "keymgr_tt: firmware boot starting";
        m_firmware->boot();
        CSML_INFO(1, logger) << "keymgr_tt: firmware ready";

        // Assert OUTBOUND_READ_DATA_AVAIL IRQ for the RESP_KM_READY message
        // boot() just pushed into the outbound FIFO.
        update_level_irq_bits();

        // Message processing loop — exits when reset is asserted again
        while (rst_ni.read()) {
            // Wait for either an inbound message or any reset level change
            wait(m_mailbox.inbound_msg_event() | rst_ni.value_changed_event());

            if (!rst_ni.read()) break;  // reset asserted — restart boot

            m_firmware->process_messages();

            // FLUSHED_BY_KM: if firmware flushed internally, raise IRQ bit[4]
            if (m_mailbox.flushed_by_km_pending()) {
                uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
                irqs |= (1u << 4);  // FLUSHED_BY_KM
                MB_IRQS = irqs;
                m_mailbox.clear_flushed_by_km();
            }

            // Update level-sensitive IRQ bits (outbound data avail + inbound space avail)
            update_level_irq_bits();
        }
    }
}

// =============================================================================
// SC_METHOD: IRQ output driver
// =============================================================================

void keymgr_tt_model::irq_update_process()
{
    uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
    uint32_t irqen = static_cast<uint32_t>(MB_IRQEN);
    irq.write((irqs & irqen) != 0);
}

void keymgr_tt_model::update_level_irq_bits()
{
    uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
    // bit[0]: OUTBOUND_READ_DATA_AVAIL — level high when outbound FIFO has data
    if (!m_mailbox.outbound_empty()) irqs |=  (1u << 0);
    else                             irqs &= ~(1u << 0);
    // bit[1]: INBOUND_WRITE_SPACE_AVAIL — level high when inbound FIFO has space
    if (!m_mailbox.inbound_full())   irqs |=  (1u << 1);
    else                             irqs &= ~(1u << 1);
    MB_IRQS = irqs;
    m_irq_update_event.notify(SC_ZERO_TIME);
}

// =============================================================================
// Random word generation — OpenSSL RAND_bytes
// =============================================================================

uint32_t keymgr_tt_model::get_random_word()
{
    uint32_t w = 0;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&w), sizeof(w)) != 1)
        CSML_WARN(1, logger) << "keymgr_tt: RAND_bytes failed";
    return w;
}

// =============================================================================
// Key transfer to crypto engine sockets
// =============================================================================

bool keymgr_tt_model::key_transfer_via_socket(uint8_t dest_mask,
                                               const uint32_t* words, int count,
                                               bool set_valid)
{
    // TLM sideload interface — dual XOR share (DPA countermeasure, from rom_sideload.c):
    //   SHARE0[0..N-1] at byte offsets 0x00..(N-1)*4       = DRBG random mask
    //   SHARE1[0..N-1] at byte offsets N*4..(2N-1)*4       = key[w] XOR SHARE0[w]
    //   KEY_CTRL=1     at key_ctrl_off                      = commit signal
    //
    // Confirmed register layout (key_manager_reg.svh, HMAC_WRAPPER_KEY_REG_MAP):
    //   HMAC/KMAC/AES: 8 words (256-bit)
    //     SHARE0[0..7] @ 0x00–0x1C, SHARE1[0..7] @ 0x20–0x3C, KEY_CTRL @ 0x40
    //   OTBN: 12 words (384-bit)
    //     SHARE0[0..11] @ 0x000–0x02C, SHARE1[0..11] @ 0x030–0x05C, KEY_CTRL @ 0x60
    struct engine_info {
        uint8_t  bit;
        tlm_utils::simple_initiator_socket<keymgr_tt_model, 32>* sock;
        uint32_t key_ctrl_off;  ///< byte offset of KEY_CTRL commit word
        int      max_words;     ///< sideload register file capacity per share
    };
    engine_info engines[] = {
        { 0x01u, &hmac_key_socket, 0x40u,  8 },
        { 0x02u, &kmac_key_socket, 0x40u,  8 },
        { 0x04u, &aes_key_socket,  0x40u,  8 },
        { 0x08u, &otbn_key_socket, 0x60u, 12 },
    };

    bool ok = true;
    for (auto& e : engines) {
        if (!(dest_mask & e.bit)) continue;

        int actual = (count < e.max_words) ? count : e.max_words;

        auto write_word = [&](uint32_t offset, uint32_t data) {
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(static_cast<sc_dt::uint64>(offset));
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            (*e.sock)->b_transport(trans, delay);
            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) ok = false;
        };

        // Dual XOR share write (rom_sideload.c: SHARE0=mask, SHARE1=key^mask)
        uint32_t share1_base = static_cast<uint32_t>(e.max_words) * 4u;
        for (int w = 0; w < actual; w++) {
            uint32_t mask = get_random_word();
            write_word(static_cast<uint32_t>(w) * 4u,          mask);          // SHARE0[w]
            write_word(share1_base + static_cast<uint32_t>(w) * 4u, words[w] ^ mask);  // SHARE1[w]
        }

        // KEY_CTRL=1 commits key; KEY_CTRL=0 clears key_valid (rom_sideload.h)
        write_word(e.key_ctrl_off, set_valid ? 0x00000001u : 0x00000000u);
    }
    return ok;
}

// =============================================================================
// Callback registration
// =============================================================================

void keymgr_tt_model::register_all_callbacks()
{
    // MB_WDATA — write: push word into inbound mailbox FIFO
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            return this->handle_write_MB_WDATA(value);
        };
        mb_memory.register_write_callback(cb, MB_WDATA.offset);
    }

    // MB_WSEP — write: arm separator flag for next MB_WDATA write
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            return this->handle_write_MB_WSEP(value);
        };
        mb_memory.register_write_callback(cb, MB_WSEP.offset);
    }

    // MB_RDATA — read: pop word from outbound mailbox FIFO
    {
        std::function<bool(uint32_t &)> cb = [this](uint32_t &value) {
            return this->handle_read_MB_RDATA(value);
        };
        mb_memory.register_read_callback(cb, MB_RDATA.offset);
    }

    // MB_STATUS — read: return live FIFO counts
    {
        std::function<bool(uint32_t &)> cb = [this](uint32_t &value) {
            return this->handle_read_MB_STATUS(value);
        };
        mb_memory.register_read_callback(cb, MB_STATUS.offset);
    }

    // MB_STATUS — write: W1C for overflow/underflow bits[23:20]
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            return this->handle_write_MB_STATUS(value);
        };
        mb_memory.register_write_callback(cb, MB_STATUS.offset);
    }

    // MB_IRQS — write: W1C logic + re-evaluate irq
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            return this->handle_write_MB_IRQS(value);
        };
        mb_memory.register_write_callback(cb, MB_IRQS.offset);
    }

    // MB_IRQEN — write: store masked value, then re-evaluate irq output
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            MB_IRQEN = value & MB_IRQEN.write_bit_mask;
            m_irq_update_event.notify(SC_ZERO_TIME);
            return true;
        };
        mb_memory.register_write_callback(cb, MB_IRQEN.offset);
    }

    // MB_CTRL — write: flush FIFO(s)
    {
        std::function<bool(uint32_t)> cb = [this](uint32_t value) {
            return this->handle_write_MB_CTRL(value);
        };
        mb_memory.register_write_callback(cb, MB_CTRL.offset);
    }

    // KPVLP_KEY[slot][word] — write: forward to km_kpv
    for (unsigned int slot = 0; slot < keymgr_tt::KPVLP_NUM_SLOTS; slot++) {
        for (unsigned int word = 0; word < keymgr_tt::KPVLP_KEY_WORDS_PER_SLOT; word++) {
            unsigned int idx = slot * keymgr_tt::KPVLP_KEY_WORDS_PER_SLOT + word;
            std::function<bool(uint32_t)> cb = [this, slot, word](uint32_t value) {
                return this->handle_write_KPVLP_KEY(slot, word, value);
            };
            kpvlp_memory.register_write_callback(cb, KPVLP_KEY[idx].offset);
        }
    }

    // KPVLP_CTRL[slot] — write: forward to km_kpv
    for (unsigned int slot = 0; slot < keymgr_tt::KPVLP_NUM_SLOTS; slot++) {
        std::function<bool(uint32_t)> cb = [this, slot](uint32_t value) {
            return this->handle_write_KPVLP_CTRL(slot, value);
        };
        kpvlp_memory.register_write_callback(cb, KPVLP_CTRL[slot].offset);
    }

    // KPVLP_STATUS — read: return live km_kpv unlock_sep bitmask
    {
        std::function<bool(uint32_t &)> cb = [this](uint32_t &value) {
            return this->handle_read_KPVLP_STATUS(value);
        };
        kpvlp_memory.register_read_callback(cb, KPVLP_STATUS.offset);
    }
}

// =============================================================================
// Mailbox callback handlers
// =============================================================================

bool keymgr_tt_model::handle_write_MB_WDATA(uint32_t value)
{
    uint32_t masked = value & MB_WDATA.write_bit_mask;
    MB_WDATA = masked;
    bool ok = m_mailbox.sep_write_wdata(masked);

    // MB_WSEP.set self-clears after the write (hardware behavior)
    MB_WSEP = 0u;

    if (!ok && m_mailbox.inbound_overflow_pending()) {
        // Raise INBOUND_OVERFLOW IRQ
        uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
        irqs |= (1u << 2);  // INBOUND_OVERFLOW
        MB_IRQS = irqs;
        m_irq_update_event.notify(SC_ZERO_TIME);
    }

    // Update MB_STATUS inbound fields (confirmed bit layout from km_mailbox_sep_regs.h)
    uint32_t status = static_cast<uint32_t>(MB_STATUS);
    // Clear inbound bits [1:0], depth [11:4], overflow [20], inbound_underflow [22]
    status &= ~((0x3u) | (0xFFu << 4) | (1u << 20) | (1u << 22));
    if (m_mailbox.inbound_empty())           status |= (1u << 0);
    if (m_mailbox.inbound_full())            status |= (1u << 1);
    status |= (m_mailbox.inbound_count() & 0xFFu) << 4;
    if (m_mailbox.inbound_overflow_pending())  status |= (1u << 20);
    if (m_mailbox.inbound_underflow_pending()) status |= (1u << 22);
    MB_STATUS = status;

    // Update level-sensitive IRQ bits (inbound may now be full → INBOUND_WRITE_SPACE_AVAIL may go 0)
    update_level_irq_bits();
    return true;
}

bool keymgr_tt_model::handle_write_MB_WSEP(uint32_t value)
{
    uint32_t masked = value & MB_WSEP.write_bit_mask;
    MB_WSEP = masked;
    if (masked & (1u << 0))
        m_mailbox.sep_set_wsep();
    return true;
}

bool keymgr_tt_model::handle_read_MB_RDATA(uint32_t &value)
{
    uint32_t word = 0;
    bool ok = m_mailbox.sep_read_rdata(word);
    value = word & MB_RDATA.read_bit_mask;
    MB_RDATA = value;

    if (!ok && m_mailbox.outbound_underflow_pending()) {
        // Raise OUTBOUND_UNDERFLOW IRQ
        uint32_t irqs = static_cast<uint32_t>(MB_IRQS);
        irqs |= (1u << 3);  // OUTBOUND_UNDERFLOW
        MB_IRQS = irqs;
        m_irq_update_event.notify(SC_ZERO_TIME);
    }

    // Update MB_STATUS outbound fields
    uint32_t status = static_cast<uint32_t>(MB_STATUS);
    // Clear outbound bits [3:2], depth [19:12], outbound_overflow [21], underflow [23], separator [25]
    status &= ~((0xCu) | (0xFFu << 12) | (1u << 21) | (1u << 23) | (1u << 25));
    if (m_mailbox.outbound_empty())             status |= (1u << 2);
    if (m_mailbox.outbound_full())              status |= (1u << 3);
    status |= (m_mailbox.outbound_count() & 0xFFu) << 12;
    if (m_mailbox.outbound_overflow_pending())  status |= (1u << 21);
    if (m_mailbox.outbound_underflow_pending()) status |= (1u << 23);
    if (m_mailbox.outbound_last_was_sep())      status |= (1u << 25);
    MB_STATUS = status;

    // Update level-sensitive IRQ bits (outbound may now be empty → OUTBOUND_READ_DATA_AVAIL goes 0)
    update_level_irq_bits();
    return true;
}

bool keymgr_tt_model::handle_read_MB_STATUS(uint32_t &value)
{
    uint32_t status = 0;
    // Bits [3:0]: empty/full flags
    if (m_mailbox.inbound_empty())   status |= (1u << 0);
    if (m_mailbox.inbound_full())    status |= (1u << 1);
    if (m_mailbox.outbound_empty())  status |= (1u << 2);
    if (m_mailbox.outbound_full())   status |= (1u << 3);
    // Bits [11:4]: inbound depth, [19:12]: outbound depth
    status |= (m_mailbox.inbound_count()  & 0xFFu) << 4;
    status |= (m_mailbox.outbound_count() & 0xFFu) << 12;
    // Bits [20-25]: overflow/underflow/separator flags
    if (m_mailbox.inbound_overflow_pending())    status |= (1u << 20);
    if (m_mailbox.outbound_overflow_pending())   status |= (1u << 21);
    if (m_mailbox.inbound_underflow_pending())   status |= (1u << 22);
    if (m_mailbox.outbound_underflow_pending())  status |= (1u << 23);
    if (m_mailbox.inbound_last_was_sep())        status |= (1u << 24);
    if (m_mailbox.outbound_last_was_sep())       status |= (1u << 25);
    value = status & MB_STATUS.read_bit_mask;
    MB_STATUS = value;
    return true;
}

bool keymgr_tt_model::handle_write_MB_STATUS(uint32_t value)
{
    // W1C for bits[23:20]: inbound_overflow[20], outbound_overflow[21],
    //                       inbound_underflow[22], outbound_underflow[23]
    uint32_t cleared = value & MB_STATUS.write_bit_mask;  // 0x00F00000
    if (cleared & (1u << 20)) m_mailbox.clear_inbound_overflow();
    if (cleared & (1u << 21)) m_mailbox.clear_outbound_overflow();
    if (cleared & (1u << 22)) m_mailbox.clear_inbound_underflow();
    if (cleared & (1u << 23)) m_mailbox.clear_outbound_underflow();
    // Re-read live state so STATUS reflects cleared flags
    uint32_t dummy = 0;
    return handle_read_MB_STATUS(dummy);
}

bool keymgr_tt_model::handle_write_MB_IRQS(uint32_t value)
{
    // W1C: writing 1 clears the bit (only bits[4:2]; bits[1:0] are level-sensitive RO)
    uint32_t current = static_cast<uint32_t>(MB_IRQS);
    uint32_t cleared = value & MB_IRQS.write_bit_mask;  // 0x1C
    uint32_t updated = current & ~cleared;
    MB_IRQS = updated;

    // Clear corresponding sticky flags in mailbox when SEP acknowledges
    if (cleared & (1u << 2)) m_mailbox.clear_inbound_overflow();
    if (cleared & (1u << 3)) m_mailbox.clear_outbound_underflow();
    // Note: FLUSHED_BY_KM[4] is a pure IRQ status flag — no mailbox state to clear.

    m_irq_update_event.notify(SC_ZERO_TIME);
    return true;
}

bool keymgr_tt_model::handle_write_MB_CTRL(uint32_t value)
{
    uint32_t masked = value & MB_CTRL.write_bit_mask;
    MB_CTRL = masked;

    // FLUSH (bit 2): flush both inbound and outbound FIFOs; self-clears
    if (masked & (1u << 2)) {
        m_mailbox.sep_flush();
        MB_CTRL = masked & ~(1u << 2);  // self-clear FLUSH bit
        update_level_irq_bits();        // FIFOs now empty: bit[1]=1 (space avail), bit[0]=0
    }
    // bits[1:0] (INBOUND_OVERFLOW_RESP, OUTBOUND_UNDERFLOW_RESP) configure behavior — stored as-is
    return true;
}

// =============================================================================
// KPVLP callback handlers
// =============================================================================

bool keymgr_tt_model::handle_write_KPVLP_KEY(unsigned int slot, unsigned int word, uint32_t value)
{
    unsigned int idx = slot * keymgr_tt::KPVLP_KEY_WORDS_PER_SLOT + word;
    KPVLP_KEY[idx] = value;  // update CSML storage (WO register, read returns 0)
    if (!m_kpv.kpvlp_write_key_word(slot, word, value)) {
        CSML_WARN(1, logger) << "KPVLP_KEY write to slot " << slot
                             << " word " << word << " rejected (unlock_sep=0 or lock_write=1)";
    }
    return true;
}

bool keymgr_tt_model::handle_write_KPVLP_CTRL(unsigned int slot, uint32_t value)
{
    uint32_t masked = value & KPVLP_CTRL[slot].write_bit_mask;
    KPVLP_CTRL[slot] = masked;
    if (!m_kpv.kpvlp_write_ctrl(slot, masked)) {
        CSML_WARN(1, logger) << "KPVLP_CTRL write to slot " << slot
                             << " rejected (unlock_sep=0 or lock_write=1)";
    }
    return true;
}

bool keymgr_tt_model::handle_read_KPVLP_STATUS(uint32_t &value)
{
    value = m_kpv.kpvlp_status() & KPVLP_STATUS.read_bit_mask;
    KPVLP_STATUS = value;  // keep CSML storage in sync
    return true;
}
