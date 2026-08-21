#include "km_firmware_handler.h"

namespace keymgr_tt {

// =============================================================================
// Constructor
// =============================================================================

km_firmware_handler::km_firmware_handler(km_kpv&     kpv,
                                         km_mailbox& mailbox,
                                         drbg_fn_t   get_random)
  : m_kpv(kpv),
    m_mailbox(mailbox),
    m_get_random(get_random),
    m_key_xfer([](uint8_t, const uint32_t*, int, bool) { return true; })  // stub: no-op
{
    for (auto& e : m_handle_registry) e = km_handle_entry_t{};
}

// =============================================================================
// Reset
// =============================================================================

void km_firmware_handler::reset()
{
    m_initialized             = false;
    m_sram_loaded             = false;
    m_halted                  = false;
    m_recoverable_fault_active = false;
    m_next_rx_seq             = 0;
    m_tx_seq                  = 0;
    m_has_valid_rx            = false;
    m_last_valid_rx_seq       = 0;
    m_rom_locked              = false;
    // OTP_READ_LOCK_COLD lives in the cold reset domain and would survive a warm
    // reset. The model has a single reset input, so it is cleared here; warm/cold
    // domain separation is tracked as a separate fidelity gap.
    m_otp_read_lock_cold      = 0;
    for (auto& e : m_handle_registry) e = km_handle_entry_t{};
    m_next_handle             = 1;
    m_rx_buffer.clear();
}

// =============================================================================
// Boot sequence  (firmware.adoc §Boot Sequence)
// =============================================================================

void km_firmware_handler::boot()
{
    // Step 1: SRAM firmware check — model always starts in ROM mode
    m_sram_loaded = false;

    // Step 2: SRAM scrambler — not modeled (no physical SRAM in TLM model)

    // Steps 3-7: KPV and engine initialisation.
    //   Random overwrites (Fisher-Yates, KEY_SHRED_ITER passes) are DPA countermeasures
    //   not observable by SEP — not modeled. Clear engine registers once with zeros.
    static const uint8_t SHRED_ENGINES[4] = {
        DEST_HMAC, DEST_KMAC, DEST_AES, DEST_OTBN
    };
    static const std::vector<uint32_t> zero_words(km_kpv::WORDS_PER_KEY, 0u);
    for (uint8_t dest : SHRED_ENGINES)
        m_key_xfer(dest, zero_words.data(), km_kpv::WORDS_PER_KEY, false);  // shred: clear key_valid

    m_initialized = true;

    // Announce boot completion to SEP — zero-payload RESP_KM_READY (0x55)
    // Mirrors rom_boot.c: rom_msg_tx_send(ROM_KM_RESP_KM_READY, NULL, 0)
    push_message(RESP_KM_READY, m_tx_seq++, {});
}

// =============================================================================
// Main message processing loop
// =============================================================================

void km_firmware_handler::process_messages()
{
    // Halted after unrecoverable fault — do not process any further messages
    if (m_halted) return;

    // Keep draining while words are arriving. receive_message only reports a
    // frame once its separator lands, so a long message simply accumulates across
    // several passes instead of being discarded.
    while (!m_mailbox.inbound_empty()) {

        // ---- 1. Collect all words of the next complete message ----
        std::vector<uint32_t> raw;
        if (!receive_message(raw) || raw.empty())
            break;

        // ---- 2. Parse header ----
        uint32_t header  = raw[0];
        uint8_t  rx_seq  = (header >>  0) & 0xFFu;
        uint8_t  cmd_id  = (header >>  8) & 0xFFu;
        uint8_t  pay_len = (header >> 16) & 0xFFu;
        uint8_t  hdr_crc = (header >> 24) & 0xFFu;

        // ---- 3. Validate header CRC8 ----
        if (crc8_header(header & 0x00FFFFFFu) != hdr_crc) {
            send_resp_cmd(rx_seq, cmd_id, RET_HEADER_CRC);
            continue;
        }

        // ---- 4. Check sequence number ----
        if (rx_seq != m_next_rx_seq) {
            // Return arg is the last valid seq received, if any (firmware.adoc §cmd_noseq)
            if (m_has_valid_rx)
                send_resp_cmd_arg(rx_seq, cmd_id, RET_CMD_NOSEQ,
                                  static_cast<uint32_t>(m_last_valid_rx_seq));
            else
                send_resp_cmd(rx_seq, cmd_id, RET_CMD_NOSEQ);
            continue;
        }
        m_next_rx_seq++;

        // ---- 5. Validate frame length ----
        //   Expected: 1 header + pay_len payload + (1 CRC32 if pay_len>0)
        size_t expected = 1u + pay_len + (pay_len > 0 ? 1u : 0u);
        if (raw.size() != expected) {
            send_resp_cmd(rx_seq, cmd_id, RET_INVALID_LEN);
            continue;
        }

        // ---- 6. Extract payload ----
        std::vector<uint32_t> payload(raw.begin() + 1,
                                      raw.begin() + 1 + pay_len);

        // ---- 7. Validate payload CRC32 ----
        if (pay_len > 0) {
            uint32_t expected_crc = crc32_payload(payload.data(), pay_len);
            uint32_t received_crc = raw[1 + pay_len];
            if (expected_crc != received_crc) {
                send_resp_cmd(rx_seq, cmd_id, RET_PAYLOAD_CRC);
                continue;
            }
        }

        // ---- 8. Record last valid sequence (all checks passed) ----
        m_last_valid_rx_seq = rx_seq;
        m_has_valid_rx      = true;

        // ---- 9. Command-specific payload length check ----
        //   Every command but CMD_KEY_LOAD has a fixed payload length, and the
        //   dispatcher rejects a mismatch before the handler runs, reporting the
        //   received length as the return argument (rom_cmd_validate_payload_length).
        //   CMD_KEY_LOAD is variable-length and validates itself against KEY_SIZE.
        if (static_cast<cmd_id_t>(cmd_id) != CMD_KEY_LOAD) {
            int expected_len = -1;
            switch (static_cast<cmd_id_t>(cmd_id)) {
                case CMD_HW_VER:
                case CMD_ROM_VER:
                case CMD_SRAM_VER:
                case CMD_STAT:
                case CMD_RECOV_ACK:
                case CMD_EXEC_ROM:
                case CMD_SRAM_EXEC:            expected_len = 0; break;
                case CMD_SRAM_LOAD_EXEC:
                case CMD_KEY_REVOKE:
                case CMD_ENGINE_SHRED:
                case CMD_ABR_SK_TRANSFER:
                case CMD_OTP_READ_LOCK_COLD:   expected_len = 1; break;
                case CMD_KEY_GENERATE:
                case CMD_KEY_TRANSFER:         expected_len = 2; break;
                default:                       break;  // unknown: falls through to INVALID_CMD
            }
            if (expected_len >= 0 &&
                payload.size() != static_cast<size_t>(expected_len)) {
                send_resp_cmd_arg(rx_seq, cmd_id, RET_INVALID_LEN,
                                  static_cast<uint32_t>(payload.size()));
                continue;
            }
        }

        // ---- 10. Dispatch ----
        switch (static_cast<cmd_id_t>(cmd_id)) {
            case CMD_HW_VER:         handle_cmd_hw_ver        (rx_seq, payload); break;
            case CMD_ROM_VER:        handle_cmd_rom_ver       (rx_seq, payload); break;
            case CMD_SRAM_VER:       handle_cmd_sram_ver      (rx_seq, payload); break;
            case CMD_STAT:           handle_cmd_stat          (rx_seq, payload); break;
            case CMD_RECOV_ACK:      handle_cmd_recov_ack     (rx_seq, payload); break;
            case CMD_EXEC_ROM:       handle_cmd_exec_rom      (rx_seq, payload); break;
            case CMD_SRAM_LOAD_EXEC: handle_cmd_sram_load_exec(rx_seq, payload); break;
            case CMD_SRAM_EXEC:      handle_cmd_sram_exec     (rx_seq, payload); break;
            case CMD_KEY_GENERATE:   handle_cmd_key_generate  (rx_seq, payload); break;
            case CMD_KEY_REVOKE:     handle_cmd_key_revoke    (rx_seq, payload); break;
            case CMD_KEY_TRANSFER:   handle_cmd_key_transfer  (rx_seq, payload); break;
            case CMD_ENGINE_SHRED:   handle_cmd_engine_shred  (rx_seq, payload); break;
            case CMD_KEY_LOAD:       handle_cmd_key_load      (rx_seq, payload); break;
            case CMD_ABR_SK_TRANSFER:  handle_cmd_abr_sk_transfer (rx_seq, payload); break;
            case CMD_OTP_READ_LOCK_COLD:
                                     handle_cmd_otp_read_lock_cold(rx_seq, payload); break;
            default:
                send_resp_cmd(rx_seq, cmd_id, RET_INVALID_CMD);
                break;
        }
    }
}

// =============================================================================
// Message I/O helpers
// =============================================================================

bool km_firmware_handler::receive_message(std::vector<uint32_t>& words)
{
    // Drain whatever the FIFO holds into the reassembly buffer, and report a
    // message only once its separator arrives. A frame wider than the FIFO is
    // therefore delivered across several fills, exactly as the firmware handles
    // it with its SRAM message buffer.
    while (true) {
        mb_word_t w;
        if (!m_mailbox.km_pop_word(w)) {
            // Underflow here is just an empty FIFO, not a fault: the rest of the
            // message has not been written yet.
            m_mailbox.clear_inbound_underflow();
            return false;
        }

        if (m_rx_buffer.size() >= MSGBUF_WORDS) {
            // The sender has overrun the buffer without ever marking an end of
            // message. Drop the partial frame and report it.
            m_rx_buffer.clear();
            send_recoverable_fault(RFAULT_RX_BUFF_OFLOW);
            return false;
        }

        m_rx_buffer.push_back(w.data);
        if (w.separator) {
            words = m_rx_buffer;
            m_rx_buffer.clear();
            return true;
        }
    }
}

void km_firmware_handler::push_message(uint8_t resp_id, uint8_t seq,
                                       const std::vector<uint32_t>& payload)
{
    uint8_t  pay_len   = static_cast<uint8_t>(payload.size());
    uint32_t header_24 = static_cast<uint32_t>(seq)
                       | (static_cast<uint32_t>(resp_id) << 8)
                       | (static_cast<uint32_t>(pay_len) << 16);
    uint8_t  hdr_crc   = crc8_header(header_24);
    uint32_t header    = header_24 | (static_cast<uint32_t>(hdr_crc) << 24);

    bool has_payload = (pay_len > 0);

    if (!has_payload) {
        m_mailbox.km_push_word(header, /*separator=*/true);
    } else {
        m_mailbox.km_push_word(header, false);
        for (size_t i = 0; i < payload.size(); i++)
            m_mailbox.km_push_word(payload[i], false);
        uint32_t crc = crc32_payload(payload.data(), payload.size());
        m_mailbox.km_push_word(crc, /*separator=*/true);
    }
}

void km_firmware_handler::send_resp_cmd(uint8_t src_seq, uint8_t cmd_id,
                                        int32_t ret_code)
{
    // RESP_CMD payload: [seq_num, cmd_id, ret_code]  (no return arg)
    std::vector<uint32_t> p = {
        static_cast<uint32_t>(src_seq),
        static_cast<uint32_t>(cmd_id)  & 0xFFu,
        static_cast<uint32_t>(ret_code)
    };
    push_message(RESP_CMD, m_tx_seq++, p);
}

void km_firmware_handler::send_resp_cmd_arg(uint8_t src_seq, uint8_t cmd_id,
                                            int32_t ret_code, uint32_t ret_arg)
{
    // RESP_CMD payload: [seq_num, cmd_id, ret_code, ret_arg]
    std::vector<uint32_t> p = {
        static_cast<uint32_t>(src_seq),
        static_cast<uint32_t>(cmd_id)  & 0xFFu,
        static_cast<uint32_t>(ret_code),
        ret_arg
    };
    push_message(RESP_CMD, m_tx_seq++, p);
}

void km_firmware_handler::send_recoverable_fault(recoverable_fault_code_t code)
{
    // RESP_RECOVERABLE_FAULT payload: one word with FAULT_CODE in bits[7:0]
    // (firmware.adoc §0xFE - RECOVERABLE_FAULT)
    uint32_t payload_word = static_cast<uint32_t>(static_cast<uint8_t>(code));
    push_message(RESP_RECOVERABLE_FAULT, m_tx_seq++, { payload_word });
}

void km_firmware_handler::send_unrecoverable_fault(unrecoverable_fault_code_t code)
{
    // RESP_UNRECOVERABLE_FAULT payload: one word with FAULT_CODE in bits[7:0]
    // (firmware.adoc §0xFF - UNRECOVERABLE_FAULT)
    uint32_t payload_word = static_cast<uint32_t>(static_cast<uint8_t>(code));
    push_message(RESP_UNRECOVERABLE_FAULT, m_tx_seq++, { payload_word });

    // KM is halting — flush both mailbox FIFOs to prevent stale data leaking.
    // Raises FLUSHED_BY_KM IRQ bit on the SEP side so SEP knows to re-sync.
    m_mailbox.km_flush();
}

void km_firmware_handler::send_wipe_fault()
{
    // Emergency-wipe unrecoverable fault (UFAULT_WIPE_STATE = -1, rom_defs.h).
    send_unrecoverable_fault(UFAULT_WIPE_STATE);
    m_halted = true;
}

// =============================================================================
// Command handlers
// =============================================================================

// 0x00 — CMD_HW_VER
bool km_firmware_handler::handle_cmd_hw_ver(uint8_t seq,
                                            const std::vector<uint32_t>&)
{
    // Return arg: [23:16] HW_MAJOR | [15:8] HW_MINOR | [7:0] HW_PATCH
    uint32_t ver = (static_cast<uint32_t>(HW_MAJOR) << 16)
                 | (static_cast<uint32_t>(HW_MINOR) <<  8)
                 | (static_cast<uint32_t>(HW_PATCH));
    send_resp_cmd_arg(seq, CMD_HW_VER, RET_SUCCESS, ver);
    return true;
}

// 0x01 — CMD_ROM_VER
bool km_firmware_handler::handle_cmd_rom_ver(uint8_t seq,
                                             const std::vector<uint32_t>&)
{
    uint32_t ver = (static_cast<uint32_t>(ROM_MAJOR) << 16)
                 | (static_cast<uint32_t>(ROM_MINOR) <<  8)
                 | (static_cast<uint32_t>(ROM_PATCH));
    send_resp_cmd_arg(seq, CMD_ROM_VER, RET_SUCCESS, ver);
    return true;
}

// 0x02 — CMD_SRAM_VER
bool km_firmware_handler::handle_cmd_sram_ver(uint8_t seq,
                                              const std::vector<uint32_t>&)
{
    if (!m_sram_loaded) {
        send_resp_cmd(seq, CMD_SRAM_VER, RET_FAILURE);
    } else {
        // SRAM firmware version would come from SRAM image header — stub 0
        send_resp_cmd_arg(seq, CMD_SRAM_VER, RET_SUCCESS, 0u);
    }
    return true;
}

// 0x03 — CMD_STAT
bool km_firmware_handler::handle_cmd_stat(uint8_t seq,
                                          const std::vector<uint32_t>&)
{
    // ret_arg: [0] = recoverable_err (1 if a recoverable fault is pending)
    uint32_t status = m_recoverable_fault_active ? 1u : 0u;
    send_resp_cmd_arg(seq, CMD_STAT, RET_SUCCESS, status);
    return true;
}

// 0x04 — CMD_RECOV_ACK
bool km_firmware_handler::handle_cmd_recov_ack(uint8_t seq,
                                               const std::vector<uint32_t>&)
{
    // Mirrors rom_cmd.c:rom_cmd_recov_ack(): clear recoverable error flag, no return arg.
    m_recoverable_fault_active = false;
    send_resp_cmd(seq, CMD_RECOV_ACK, RET_SUCCESS);
    return true;
}

// 0x10 — CMD_EXEC_ROM
bool km_firmware_handler::handle_cmd_exec_rom(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    (void)p;
    // Latch ROM-only operation. The model has no mutable-firmware path, so this
    // only records the decision; it is what makes the subsequent SRAM commands
    // report a definite refusal rather than an unknown command.
    m_rom_locked = true;
    send_resp_cmd(seq, CMD_EXEC_ROM, RET_SUCCESS);
    return true;
}

// 0x11 — CMD_SRAM_LOAD_EXEC
bool km_firmware_handler::handle_cmd_sram_load_exec(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    (void)p;
    // Mutable firmware is not modelled: there is no SRAM image, no PicoRV32 to
    // run it on, and no ISS. Report failure so firmware sees a real refusal
    // instead of a silent success it cannot verify.
    send_resp_cmd(seq, CMD_SRAM_LOAD_EXEC, RET_FAILURE);
    return false;
}

// 0x12 — CMD_SRAM_EXEC
bool km_firmware_handler::handle_cmd_sram_exec(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    (void)p;
    send_resp_cmd(seq, CMD_SRAM_EXEC, RET_FAILURE);
    return false;
}

// Shared provisioning path for CMD_KEY_GENERATE and CMD_KEY_LOAD (rom_load_key).
km_firmware_handler::load_key_rc_t km_firmware_handler::load_key(
        const uint32_t* words, int len, uint8_t dest_valid, uint8_t& handle_out)
{
    // A key with no permitted destination could never be used, so the firmware
    // rejects it outright rather than storing something unusable.
    if (words == nullptr || dest_valid == 0) return LOAD_BAD_ARGS;

    const int num_slots = km_kpv::slots_for_words(len);
    if (num_slots == 0 || num_slots > km_kpv::MAX_SLOTS_PER_KEY) return LOAD_BAD_ARGS;

    const int base = m_kpv.km_find_free_slots(num_slots, m_get_random);
    if (base < 0) return LOAD_BAD_ARGS;

    // CRC the key material as supplied, before it reaches the vault, so the
    // stored digest covers what the caller actually asked for.
    const uint32_t key_crc = crc32_payload(words, static_cast<size_t>(len));

    const int handle = alloc_handle(static_cast<uint8_t>(base),
                                   static_cast<uint8_t>(num_slots),
                                   dest_valid, key_crc);
    if (handle < 0) return LOAD_NO_HANDLES;

    // Erase the run before writing, so no residue of a previous key survives
    // underneath a shorter one.
    for (int s = base; s < base + num_slots; s++)
        m_kpv.km_erase_key(s, m_get_random);

    if (!m_kpv.km_write_key(base, words, len)) {
        free_handle(static_cast<uint8_t>(handle));
        return LOAD_KPV_FAILED;
    }

    m_kpv.km_write_lock_span(base);
    for (int s = base; s < base + num_slots; s++)
        m_kpv.km_set_valid(s, true);

    handle_out = static_cast<uint8_t>(handle);
    return LOAD_OK;
}

// 0x22 — CMD_KEY_GENERATE
bool km_firmware_handler::handle_cmd_key_generate(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=KEY_SIZE (words-1) [6:0], [1]=DEST_VALID [7:0]
    //
    // KEY_SIZE counts words, not slots: rom_generate_key draws KEY_SIZE+1 words
    // from the DRBG and hands them to the same loader CMD_KEY_LOAD uses.
    //
    // The invalid-argument responses carry the index of the offending payload
    // word, matching rom_cmd_key_generate.
    if (p.size() < 2) {
        send_resp_cmd(seq, CMD_KEY_GENERATE, RET_INVALID_LEN);
        return false;
    }
    // Tested against the whole word so reserved bits above [6:0] are caught too.
    if (p[0] > 127u) {
        send_resp_cmd_arg(seq, CMD_KEY_GENERATE, RET_INVALID_ARG, 0u);
        return false;
    }
    if (p[1] == 0u || (p[1] & ~0xFFu) != 0u) {
        send_resp_cmd_arg(seq, CMD_KEY_GENERATE, RET_INVALID_ARG, 1u);
        return false;
    }

    const int     key_words  = static_cast<int>(p[0]) + 1;
    const uint8_t dest_valid = static_cast<uint8_t>(p[1] & 0xFFu);

    std::vector<uint32_t> key(static_cast<size_t>(key_words));
    for (int w = 0; w < key_words; w++) key[static_cast<size_t>(w)] = m_get_random();

    uint8_t handle = 0;
    if (load_key(key.data(), key_words, dest_valid, handle) != LOAD_OK) {
        // Slot-fit, KPV and handle-exhaustion failures are indistinguishable to
        // SEP: the firmware reports plain FAILURE with no argument for all three.
        send_resp_cmd(seq, CMD_KEY_GENERATE, RET_FAILURE);
        return false;
    }

    // Return arg: [7:0]=key_handle, [14:8]=KEY_SIZE (echoed), [23:16]=dest_valid
    uint32_t ret_arg = (static_cast<uint32_t>(handle) & 0xFFu)
                     | ((static_cast<uint32_t>(key_words - 1) & 0x7Fu) << 8)
                     | (static_cast<uint32_t>(dest_valid) << 16);
    send_resp_cmd_arg(seq, CMD_KEY_GENERATE, RET_SUCCESS, ret_arg);
    return true;
}

// 0x23 — CMD_KEY_REVOKE
bool km_firmware_handler::handle_cmd_key_revoke(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    if (p.size() < 1) {
        send_resp_cmd(seq, CMD_KEY_REVOKE, RET_INVALID_LEN);
        return false;
    }
    uint8_t handle = static_cast<uint8_t>(p[0] & 0xFFu);

    // Validate handle
    if (handle == 0 || !m_handle_registry[handle].valid) {
        send_resp_cmd(seq, CMD_KEY_REVOKE, RET_FAILURE);
        return false;
    }
    int slot = static_cast<int>(m_handle_registry[handle].base_slot);

    // Lock and invalidate every slot the key occupies. The span comes from the
    // registry rather than the vault's EXTEND field, so it stays correct even if
    // the slot geometry has since been erased.
    int slot_count = static_cast<int>(m_handle_registry[handle].num_slots);
    for (int s = slot; s < slot + slot_count && s < km_kpv::NUM_SLOTS; s++) {
        m_kpv.km_lock_use(s);         // prevent KM from re-reading the key
        m_kpv.km_lock_write(s);       // prevent further writes
        m_kpv.km_set_valid(s, false); // mark slot as no longer valid
    }

    // Free the handle
    free_handle(handle);

    // Return arg: [7:0] = revoked key handle
    send_resp_cmd_arg(seq, CMD_KEY_REVOKE, RET_SUCCESS,
                      static_cast<uint32_t>(handle) & 0xFFu);
    return true;
}

// 0x24 — CMD_KEY_TRANSFER
bool km_firmware_handler::handle_cmd_key_transfer(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=KEY_HANDLE [7:0], [1]=DEST_ENGINE [7:0]
    if (p.size() < 2) {
        send_resp_cmd(seq, CMD_KEY_TRANSFER, RET_FAILURE);
        return false;
    }
    uint8_t handle   = static_cast<uint8_t>(p[0] & 0xFFu);
    uint8_t dest_req = static_cast<uint8_t>(p[1] & 0xFFu);

    // Validate handle
    if (handle == 0 || !m_handle_registry[handle].valid) {
        send_resp_cmd(seq, CMD_KEY_TRANSFER, RET_FAILURE);
        return false;
    }
    int slot = static_cast<int>(m_handle_registry[handle].base_slot);

    // Validate requested destination against the policy recorded at key creation
    uint8_t allowed = m_handle_registry[handle].dest_valid;
    if ((dest_req & allowed) != dest_req) {
        send_resp_cmd(seq, CMD_KEY_TRANSFER, RET_FAILURE);
        return false;
    }

    // Read the key back across its whole span; the length is recovered from the
    // EXTEND and LAST_DWORD geometry written at load time.
    std::vector<uint32_t> key_words;
    if (!m_kpv.km_read_key(slot, key_words) || key_words.empty()) {
        send_resp_cmd(seq, CMD_KEY_TRANSFER, RET_FAILURE);
        return false;
    }

    // Transfer via callback (writes dual XOR shares to crypto engine key sockets)
    m_key_xfer(dest_req, key_words.data(), static_cast<int>(key_words.size()), true);

    // Set lock_use on all slots in the chain after transfer (one-shot delivery).
    // A second CMD_KEY_TRANSFER on the same handle fails, because km_read_key
    // refuses a span with lock_use set.
    int slot_count = static_cast<int>(m_handle_registry[handle].num_slots);
    for (int s = slot; s < slot + slot_count && s < km_kpv::NUM_SLOTS; s++)
        m_kpv.km_lock_use(s);

    // Return arg: [7:0]=key_handle, [15:8]=dest_engine
    uint32_t ret_arg = (static_cast<uint32_t>(handle)   & 0xFFu)
                     | (static_cast<uint32_t>(dest_req) << 8);
    send_resp_cmd_arg(seq, CMD_KEY_TRANSFER, RET_SUCCESS, ret_arg);
    return true;
}

// 0x25 — CMD_ENGINE_SHRED
bool km_firmware_handler::handle_cmd_engine_shred(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    if (p.size() < 1) {
        send_resp_cmd(seq, CMD_ENGINE_SHRED, RET_FAILURE);
        return false;
    }
    uint8_t dest_req = static_cast<uint8_t>(p[0] & 0xFFu);

    // Send one pass of zeros to the engine — random multi-pass overwrites are
    // DPA countermeasures not observable by SEP and not modeled.
    static const std::vector<uint32_t> zero_words(km_kpv::WORDS_PER_KEY, 0u);
    m_key_xfer(dest_req, zero_words.data(), km_kpv::WORDS_PER_KEY, false);  // shred: clear key_valid

    uint32_t ret_arg = static_cast<uint32_t>(dest_req);
    send_resp_cmd_arg(seq, CMD_ENGINE_SHRED, RET_SUCCESS, ret_arg);
    return true;
}

// 0x26 — CMD_KEY_LOAD
bool km_firmware_handler::handle_cmd_key_load(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=KEY_SIZE (words-1) [6:0], [1]=DEST_VALID [7:0], [2..]=key words
    // This replaces the retired KPVLP provisioning path: SEP hands the plaintext
    // key over the mailbox and never touches the vault directly.
    //
    // Validation runs in the payload's word order and every rejection is
    // INVALID_ARG carrying the offending word index, per rom_cmd_key_load.

    // Need at least KEY_SIZE, DEST_VALID and one key word.
    if (p.size() < 3) {
        send_resp_cmd_arg(seq, CMD_KEY_LOAD, RET_INVALID_ARG, 0u);
        return false;
    }
    // KEY_SIZE reserved bits [31:7] must be zero.
    if ((p[0] & ~0x7Fu) != 0u) {
        send_resp_cmd_arg(seq, CMD_KEY_LOAD, RET_INVALID_ARG, 0u);
        return false;
    }
    const int key_words = static_cast<int>(p[0] & 0x7Fu) + 1;

    // The frame must carry exactly the key it claims to: KEY_SIZE + 3 words.
    if (static_cast<int>(p.size()) != key_words + 2) {
        send_resp_cmd_arg(seq, CMD_KEY_LOAD, RET_INVALID_ARG, 0u);
        return false;
    }
    // DEST_VALID reserved bits [31:8] must be zero, and at least one destination
    // must be permitted or the key could never be used.
    if ((p[1] & ~0xFFu) != 0u || (p[1] & 0xFFu) == 0u) {
        send_resp_cmd_arg(seq, CMD_KEY_LOAD, RET_INVALID_ARG, 1u);
        return false;
    }
    const uint8_t dest_valid = static_cast<uint8_t>(p[1] & 0xFFu);

    uint8_t handle = 0;
    if (load_key(&p[2], key_words, dest_valid, handle) != LOAD_OK) {
        send_resp_cmd(seq, CMD_KEY_LOAD, RET_FAILURE);
        return false;
    }

    // Same return-argument layout as CMD_KEY_GENERATE. The low byte is the
    // handle, which is what the firmware driver feeds to CMD_KEY_TRANSFER.
    uint32_t ret_arg = (static_cast<uint32_t>(handle) & 0xFFu)
                     | ((static_cast<uint32_t>(key_words - 1) & 0x7Fu) << 8)
                     | (static_cast<uint32_t>(dest_valid) << 16);
    send_resp_cmd_arg(seq, CMD_KEY_LOAD, RET_SUCCESS, ret_arg);
    return true;
}

// 0x27 — CMD_ABR_SK_TRANSFER
bool km_firmware_handler::handle_cmd_abr_sk_transfer(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    (void)p;
    // Capturing the ML-KEM shared key requires Adams Bridge to have produced one.
    // That capture path is not wired yet, so refuse rather than invent key material.
    send_resp_cmd(seq, CMD_ABR_SK_TRANSFER, RET_FAILURE);
    return false;
}

// 0x28 — CMD_OTP_READ_LOCK_COLD
bool km_firmware_handler::handle_cmd_otp_read_lock_cold(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=LOCK_BITS [5:0] — one bit per OTP field, write-one-to-set.
    // Cold-domain, so the latched value survives a warm reset.
    if (p.size() < 1) {
        send_resp_cmd(seq, CMD_OTP_READ_LOCK_COLD, RET_INVALID_LEN);
        return false;
    }
    m_otp_read_lock_cold |= static_cast<uint8_t>(p[0] & 0x3Fu);
    send_resp_cmd_arg(seq, CMD_OTP_READ_LOCK_COLD, RET_SUCCESS,
                      static_cast<uint32_t>(m_otp_read_lock_cold));
    return true;
}

// =============================================================================
// Key handle registry helpers
// =============================================================================

int km_firmware_handler::alloc_handle(uint8_t base_slot, uint8_t num_slots,
                                     uint8_t dest_valid, uint32_t crc32)
{
    // Handles are issued monotonically and never recycled, so revoking a key
    // returns its slots to the pool but not its handle number. The registry is
    // spent once 255 handles have been issued, whatever the vault looks like.
    if (m_next_handle == 0) return -1;

    const uint8_t h = m_next_handle;
    m_handle_registry[h].valid      = true;
    m_handle_registry[h].base_slot  = base_slot;
    m_handle_registry[h].num_slots  = num_slots;
    m_handle_registry[h].dest_valid = dest_valid;
    m_handle_registry[h].crc32      = crc32;

    m_next_handle++;   // wraps to 0 after handle 255, marking exhaustion
    return static_cast<int>(h);
}

void km_firmware_handler::free_handle(uint8_t handle)
{
    if (handle != 0)
        m_handle_registry[handle].valid = false;
}

// =============================================================================
// CRC helpers
// =============================================================================

// CRC-8/ROHC (polynomial 0x07 reflected = 0xE0, init 0xFF, XorOut 0x00)
// Confirmed from rom_crc.c: rom_picorv32_crc8_rohc_update(), check value = 0xD0
uint8_t km_firmware_handler::crc8_header(uint32_t header_24bit)
{
    uint8_t crc = 0xFFu;  // init = 0xFF
    for (int b = 0; b < 3; b++) {
        crc ^= static_cast<uint8_t>((header_24bit >> (b * 8)) & 0xFFu);
        for (int i = 0; i < 8; i++) {
            if (crc & 1u) crc = static_cast<uint8_t>((crc >> 1) ^ 0xE0u);  // reflected poly
            else          crc = static_cast<uint8_t>(crc >> 1);
        }
    }
    return crc;  // XorOut = 0x00
}

// CRC-32C Castagnoli (polynomial 0x82F63B78 reflected, init 0xFFFFFFFF, XorOut 0xFFFFFFFF)
// Confirmed from rom_crc.c: rom_picorv32_crc32c_byte_update(), check value = 0xE3069283
uint32_t km_firmware_handler::crc32_payload(const uint32_t* words, size_t count)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < count; i++) {
        for (int b = 0; b < 4; b++) {
            uint8_t byte = static_cast<uint8_t>((words[i] >> (b * 8)) & 0xFFu);
            crc ^= byte;
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1u) crc = (crc >> 1) ^ 0x82F63B78u;  // Castagnoli
                else          crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace keymgr_tt
