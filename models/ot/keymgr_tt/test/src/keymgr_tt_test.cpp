#include "keymgr_tt_test.h"

keymgr_tt_test::keymgr_tt_test(sc_module_name name)
  : keymgr_tt_basetest(name),
    rst_no("rst_no"),
    irq_i("irq_i")
{
    initialize_signals();
}

void keymgr_tt_test::initialize_signals()
{
    rst_no.initialize(true);   // de-asserted (active-low)
}

// =============================================================================
// Register access
// =============================================================================

void keymgr_tt_test::register_read_32(unsigned int offset, uint32_t &value)
{
    // Offsets >= KPVLP_KEY_BASE (0x1000) go to kpvlp_socket; strip the 0x1000 base.
    bool is_kpvlp = (offset >= static_cast<unsigned int>(KPVLP_KEY_BASE));
    unsigned int socket_offset = is_kpvlp ? (offset - KPVLP_KEY_BASE) : offset;

    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command        (tlm::TLM_READ_COMMAND);
    trans.set_address        (socket_offset);
    trans.set_data_ptr       (reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length    (4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed    (false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    if (is_kpvlp) kpvlp_initiator_socket->b_transport(trans, delay);
    else          initiator_socket->b_transport(trans, delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
        CSML_WARN(1, logger) << "register_read_32: bad response at offset 0x" << std::hex << offset;
}

void keymgr_tt_test::register_write_32(unsigned int offset, uint32_t value)
{
    bool is_kpvlp = (offset >= static_cast<unsigned int>(KPVLP_KEY_BASE));
    unsigned int socket_offset = is_kpvlp ? (offset - KPVLP_KEY_BASE) : offset;

    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command        (tlm::TLM_WRITE_COMMAND);
    trans.set_address        (socket_offset);
    trans.set_data_ptr       (reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length    (4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed    (false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    if (is_kpvlp) kpvlp_initiator_socket->b_transport(trans, delay);
    else          initiator_socket->b_transport(trans, delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
        CSML_WARN(1, logger) << "register_write_32: bad response at offset 0x" << std::hex << offset;
}

void keymgr_tt_test::trigger_reset(unsigned int cycles)
{
    rst_no.write(false);
    wait(cycles, SC_NS);
    rst_no.write(true);
    wait(1, SC_NS);
    // Flush the RESP_KM_READY boot announcement from the outbound FIFO.
    // MB_CTRL bit[2] = FLUSH: calls sep_flush() (clears both FIFOs)
    // then update_level_irq_bits() → MB_IRQS = 0x2 (bit[1]=INBOUND_WRITE_SPACE_AVAIL).
    register_write_32(MB_CTRL_OFFSET, 0x4u);
}

// =============================================================================
// CRC helpers
// =============================================================================

// CRC-8/ROHC: polynomial 0xE0 (0x07 reflected), init 0xFF, XorOut=0x00.
// Processes bytes [7:0], [15:8], [23:16] of header_24bit in LSB-first order.
uint8_t keymgr_tt_test::crc8_header(uint32_t header_24bit)
{
    uint8_t crc = 0xFFu;  // ROHC init
    for (int b = 0; b < 3; b++) {
        crc ^= static_cast<uint8_t>((header_24bit >> (b * 8)) & 0xFFu);
        for (int i = 0; i < 8; i++) {
            if (crc & 1u) crc = static_cast<uint8_t>((crc >> 1) ^ 0xE0u);
            else          crc = static_cast<uint8_t>(crc >> 1);
        }
    }
    return crc;  // XorOut=0x00, no final XOR
}

// CRC-32C Castagnoli (poly=0x82F63B78): init=0xFFFFFFFF, XorOut=0xFFFFFFFF.
// Processes each 32-bit word as 4 bytes in little-endian order.
uint32_t keymgr_tt_test::crc32_payload(const uint32_t* words, size_t count)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < count; i++) {
        for (int b = 0; b < 4; b++) {
            uint8_t byte = static_cast<uint8_t>((words[i] >> (b * 8)) & 0xFFu);
            crc ^= byte;
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1u) crc = (crc >> 1) ^ 0x82F63B78u;
                else          crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// =============================================================================
// Mailbox helpers
// =============================================================================

void keymgr_tt_test::mb_send_command(uint8_t seq, uint8_t cmd_id,
                                     const std::vector<uint32_t>& payload)
{
    uint32_t header_24 = static_cast<uint32_t>(seq)
                       | (static_cast<uint32_t>(cmd_id) << 8)
                       | (static_cast<uint32_t>(payload.size() & 0xFFu) << 16);
    uint8_t  hdr_crc   = crc8_header(header_24);
    uint32_t header    = header_24 | (static_cast<uint32_t>(hdr_crc) << 24);

    if (payload.empty()) {
        // No payload: header is the only word; tag it as separator.
        register_write_32(MB_WSEP_OFFSET, 1u);
        register_write_32(MB_WDATA_OFFSET, header);
    } else {
        uint32_t crc32_val = crc32_payload(payload.data(), payload.size());

        register_write_32(MB_WDATA_OFFSET, header);
        for (const uint32_t w : payload)
            register_write_32(MB_WDATA_OFFSET, w);

        // CRC32 word is the last — tag it as separator.
        register_write_32(MB_WSEP_OFFSET, 1u);
        register_write_32(MB_WDATA_OFFSET, crc32_val);
    }
}

void keymgr_tt_test::mb_send_raw_frame(const std::vector<uint32_t>& frame_words)
{
    if (frame_words.empty()) return;
    for (size_t i = 0; i < frame_words.size(); i++) {
        if (i == frame_words.size() - 1)
            register_write_32(MB_WSEP_OFFSET, 1u);  // tag last word as separator
        register_write_32(MB_WDATA_OFFSET, frame_words[i]);
    }
}

bool keymgr_tt_test::mb_receive_frame(std::vector<uint32_t>& frame_words,
                                      unsigned int timeout_ns)
{
    frame_words.clear();

    // Poll MB_IRQS bit[0] (OUTBOUND_DATA_AVAIL) until data arrives or timeout.
    uint32_t irqs    = 0;
    unsigned elapsed = 0;
    while (elapsed < timeout_ns) {
        register_read_32(MB_IRQS_OFFSET, irqs);
        if (irqs & 0x1u) break;
        wait(1, SC_NS);
        elapsed++;
    }
    if (!(irqs & 0x1u))
        return false;  // timeout

    // Read header word; parse payload_len.
    uint32_t header = 0;
    register_read_32(MB_RDATA_OFFSET, header);
    frame_words.push_back(header);

    uint8_t pay_len = static_cast<uint8_t>((header >> 16) & 0xFFu);

    // Read payload words + CRC32 word (if any payload).
    unsigned int extra = static_cast<unsigned int>(pay_len) + (pay_len > 0u ? 1u : 0u);
    for (unsigned int i = 0; i < extra; i++) {
        uint32_t word = 0;
        register_read_32(MB_RDATA_OFFSET, word);
        frame_words.push_back(word);
    }
    return true;
}

bool keymgr_tt_test::parse_resp_cmd(const std::vector<uint32_t>& frame,
                                    uint8_t&  resp_id_out,
                                    uint8_t&  src_seq_out,
                                    uint8_t&  echoed_cmd_out,
                                    int32_t&  ret_code_out,
                                    uint32_t& ret_arg_out)
{
    if (frame.empty()) return false;

    uint32_t header  = frame[0];
    resp_id_out      = static_cast<uint8_t>((header >>  8) & 0xFFu);
    uint8_t pay_len  = static_cast<uint8_t>((header >> 16) & 0xFFu);

    // Expected frame size: 1 header + pay_len payload + (pay_len>0 ? 1 CRC32 : 0)
    size_t expected_size = 1u + pay_len + (pay_len > 0 ? 1u : 0u);
    if (frame.size() != expected_size) return false;

    // RESP_CMD has resp_id == 0x00 and at least 3 payload words.
    if (resp_id_out != 0x00u) return false;
    if (pay_len < 3u)         return false;

    // payload: [0]=src_seq, [1]=cmd_id, [2]=ret_code, [3]=ret_arg (optional)
    src_seq_out    = static_cast<uint8_t>(frame[1] & 0xFFu);
    echoed_cmd_out = static_cast<uint8_t>(frame[2] & 0xFFu);
    ret_code_out   = static_cast<int32_t>(frame[3]);
    ret_arg_out    = (pay_len >= 4u) ? frame[4] : 0u;

    return true;
}
