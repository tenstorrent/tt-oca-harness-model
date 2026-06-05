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
    for (auto& e : m_handle_registry) e = km_handle_entry_t{};
}

// =============================================================================
// Boot sequence  (KM_FW.md §Boot Sequence)
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

    while (m_mailbox.km_message_ready()) {

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
            // Return arg is the last valid seq received, if any (KM_FW.md §cmd_noseq)
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

        // ---- 9. Dispatch ----
        switch (static_cast<cmd_id_t>(cmd_id)) {
            case CMD_HW_VER:         handle_cmd_hw_ver        (rx_seq, payload); break;
            case CMD_ROM_VER:        handle_cmd_rom_ver       (rx_seq, payload); break;
            case CMD_SRAM_VER:       handle_cmd_sram_ver      (rx_seq, payload); break;
            case CMD_STAT:           handle_cmd_stat          (rx_seq, payload); break;
            case CMD_RECOV_ACK:      handle_cmd_recov_ack     (rx_seq, payload); break;
            case CMD_KPVLP_SLOT_REQ: handle_cmd_kpvlp_slot_req(rx_seq, payload); break;
            case CMD_KEY_REGISTER:   handle_cmd_key_register  (rx_seq, payload); break;
            case CMD_KEY_GENERATE:   handle_cmd_key_generate  (rx_seq, payload); break;
            case CMD_KEY_REVOKE:     handle_cmd_key_revoke    (rx_seq, payload); break;
            case CMD_KEY_TRANSFER:   handle_cmd_key_transfer  (rx_seq, payload); break;
            case CMD_ENGINE_SHRED:   handle_cmd_engine_shred  (rx_seq, payload); break;
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
    words.clear();
    while (true) {
        mb_word_t w;
        if (!m_mailbox.km_pop_word(w)) return false;
        words.push_back(w.data);
        if (w.separator) return true;
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
    // (KM_FW.md §0xFE - RECOVERABLE_FAULT)
    uint32_t payload_word = static_cast<uint32_t>(static_cast<uint8_t>(code));
    push_message(RESP_RECOVERABLE_FAULT, m_tx_seq++, { payload_word });
}

void km_firmware_handler::send_unrecoverable_fault(unrecoverable_fault_code_t code)
{
    // RESP_UNRECOVERABLE_FAULT payload: one word with FAULT_CODE in bits[7:0]
    // (KM_FW.md §0xFF - UNRECOVERABLE_FAULT)
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

// 0x20 — CMD_KPVLP_SLOT_REQ
bool km_firmware_handler::handle_cmd_kpvlp_slot_req(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    if (p.size() < 1) {
        send_resp_cmd(seq, CMD_KPVLP_SLOT_REQ, RET_INVALID_LEN);
        return false;
    }
    int req_count = static_cast<int>((p[0] & 0x7u) + 1);  // REQ_SIZE+1 [2:0] (3 bits, max 8 slots)

    int base = m_kpv.km_find_free_slots(req_count);
    if (base < 0) {
        send_resp_cmd(seq, CMD_KPVLP_SLOT_REQ, RET_FAILURE);
        return false;
    }

    m_kpv.km_grant_kpvlp(base, req_count);

    // Return arg (KM_FW.md line 387): RESERVED | GRANT_SIZE | RESERVED | SLOT_INDEX
    //   [4:0]  SLOT_INDEX  base slot (5 bits for 32 slots)
    //   [10:8] GRANT_SIZE  req_count-1 (3 bits, same width as REQ_SIZE in command)
    uint32_t ret_arg = (static_cast<uint32_t>(base) & 0x1Fu)
                     | ((static_cast<uint32_t>(req_count - 1) & 0x7u) << 8);
    send_resp_cmd_arg(seq, CMD_KPVLP_SLOT_REQ, RET_SUCCESS, ret_arg);
    return true;
}

// 0x21 — CMD_KEY_REGISTER
bool km_firmware_handler::handle_cmd_key_register(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=SLOT_INDEX [4:0], [1]=KEY_SIZE_M1 [6:0], [2]=DEST_VALID [7:0], [3]=KEY_CRC
    if (p.size() < 4) {
        send_resp_cmd(seq, CMD_KEY_REGISTER, RET_INVALID_LEN);
        return false;
    }
    int      slot       = static_cast<int>(p[0] & 0x1Fu);
    int      key_size   = static_cast<int>((p[1] & 0x7Fu) + 1);  // actual key word count
    uint8_t  dest_valid = static_cast<uint8_t>(p[2] & 0xFFu);
    uint32_t key_crc    = p[3];

    // Derive extend and last_dword from key_size
    int     num_slots  = (key_size + km_kpv::WORDS_PER_KEY - 1) / km_kpv::WORDS_PER_KEY;
    uint8_t extend     = static_cast<uint8_t>(num_slots - 1);
    uint8_t last_dword = static_cast<uint8_t>((key_size - 1) % km_kpv::WORDS_PER_KEY);

    // Validate slot range
    if (slot >= km_kpv::NUM_SLOTS || slot + num_slots > km_kpv::NUM_SLOTS) {
        send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_INVALID_ARG, 0u);
        return false;
    }

    // Compare derived ctrl fields against what SEP wrote via KPVLP.
    // p[1] (KEY_SIZE_M1) implies extend and last_dword; p[2] = dest_valid.
    {
        const kpv_ctrl_t& actual = m_kpv.ctrl(slot);
        if (actual.extend != extend) {
            send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_INVALID_ARG, 1u);  // KEY_SIZE_M1 field
            return false;
        }
        if (actual.dest_valid != dest_valid) {
            send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_INVALID_ARG, 2u);  // DEST_VALID field
            return false;
        }
        if (actual.last_dword != last_dword) {
            send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_INVALID_ARG, 1u);  // KEY_SIZE_M1 field
            return false;
        }
    }

    // Validate CRC32 of key data across all slots in the chain.
    std::vector<uint32_t> key_words;
    for (int s = slot; s < slot + num_slots; s++) {
        for (int w = 0; w <= static_cast<int>(last_dword); w++) {
            uint32_t word = 0;
            m_kpv.km_read_key_word(s, w, word);
            key_words.push_back(word);
        }
    }
    uint32_t computed_crc = crc32_payload(key_words.data(), key_words.size());
    if (computed_crc != key_crc) {
        send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_INVALID_ARG, 3u);  // KEY_CRC field index
        return false;
    }

    // All checks passed — lock all slots and mark valid
    for (int s = slot; s < slot + num_slots; s++) {
        m_kpv.km_lock_write(s);
        m_kpv.km_set_valid(s, true);
    }

    // Allocate an opaque key handle
    int handle = alloc_handle(static_cast<uint8_t>(slot));
    if (handle < 0) {
        send_resp_cmd(seq, CMD_KEY_REGISTER, RET_FAILURE);
        return false;
    }

    // Return arg: [7:0] = key_handle
    send_resp_cmd_arg(seq, CMD_KEY_REGISTER, RET_SUCCESS,
                      static_cast<uint32_t>(handle) & 0xFFu);
    return true;
}

// 0x22 — CMD_KEY_GENERATE
bool km_firmware_handler::handle_cmd_key_generate(
        uint8_t seq, const std::vector<uint32_t>& p)
{
    // Payload: [0]=REQ_SIZE_M1 [6:0], [1]=DEST_VALID [7:0]
    // KDF is not yet defined; key material comes from pure DRBG output.
    if (p.size() < 2) {
        send_resp_cmd(seq, CMD_KEY_GENERATE, RET_INVALID_LEN);
        return false;
    }
    int     req_size   = static_cast<int>((p[0] & 0x7Fu) + 1);  // number of key slots
    uint8_t dest_valid = static_cast<uint8_t>(p[1] & 0xFFu);

    // Each generated slot holds KEY_GEN_WORDS DRBG words (standard 256-bit key)
    static constexpr uint8_t KEY_GEN_LAST_DWORD = static_cast<uint8_t>(KEY_GEN_WORDS - 1);

    int base = m_kpv.km_find_free_slots(req_size);
    if (base < 0) {
        send_resp_cmd(seq, CMD_KEY_GENERATE, RET_FAILURE);
        return false;
    }

    uint8_t extend = static_cast<uint8_t>(req_size - 1);
    for (int s = base; s < base + req_size; s++) {
        // Fill with pure DRBG output — no KDF (deferred to next release)
        for (int w = 0; w < KEY_GEN_WORDS; w++)
            m_kpv.km_write_key_word(s, w, m_get_random());
        m_kpv.km_write_ctrl(s, extend, dest_valid, KEY_GEN_LAST_DWORD);
        m_kpv.km_lock_write(s);
        m_kpv.km_set_valid(s, true);
    }

    // Allocate an opaque key handle
    int handle = alloc_handle(static_cast<uint8_t>(base));
    if (handle < 0) {
        send_resp_cmd(seq, CMD_KEY_GENERATE, RET_FAILURE);
        return false;
    }

    // Return arg: [7:0]=key_handle, [14:8]=req_size (echoed), [23:16]=dest_valid
    uint32_t ret_arg = (static_cast<uint32_t>(handle)   & 0xFFu)
                     | ((static_cast<uint32_t>(req_size) & 0x7Fu) << 8)
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

    // Lock and invalidate all slots in the chain
    int slot_count = static_cast<int>(m_kpv.ctrl(slot).extend) + 1;
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

    // Validate requested destination against DEST_VALID policy
    uint8_t allowed = m_kpv.ctrl(slot).dest_valid;
    if ((dest_req & allowed) != dest_req) {
        send_resp_cmd(seq, CMD_KEY_TRANSFER, RET_FAILURE);
        return false;
    }

    // Collect key words from base slot and all extended slots
    int slot_count = static_cast<int>(m_kpv.ctrl(slot).extend) + 1;
    int last_w     = m_kpv.ctrl(slot).last_dword;
    int words_per  = (last_w > 0) ? last_w + 1 : km_kpv::WORDS_PER_KEY;
    std::vector<uint32_t> key_words;
    key_words.reserve(static_cast<size_t>(slot_count * words_per));
    for (int s = slot; s < slot + slot_count && s < km_kpv::NUM_SLOTS; s++) {
        for (int w = 0; w < words_per; w++) {
            uint32_t word = 0;
            m_kpv.km_read_key_word(s, w, word);
            key_words.push_back(word);
        }
    }
    int count = static_cast<int>(key_words.size());

    // Transfer via callback (writes dual XOR shares to crypto engine key sockets)
    m_key_xfer(dest_req, key_words.data(), count, true);  // key load: set key_valid

    // Set lock_use on all slots in the chain after transfer (one-shot delivery).
    // A second CMD_KEY_TRANSFER on the same handle will return zero key words
    // (km_read_key_word returns 0 when lock_use=1).
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

// =============================================================================
// Key handle registry helpers
// =============================================================================

int km_firmware_handler::alloc_handle(uint8_t base_slot)
{
    for (int h = 1; h < 256; h++) {
        if (!m_handle_registry[h].valid) {
            m_handle_registry[h].valid     = true;
            m_handle_registry[h].base_slot = base_slot;
            return h;
        }
    }
    return -1;  // all 255 handles in use
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
