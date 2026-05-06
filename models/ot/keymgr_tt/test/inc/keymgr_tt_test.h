#pragma once
#include "keymgr_tt_basetest.h"
#include "csml_logger.h"
#include <tlm_utils/simple_target_socket.h>
#include <cstring>
#include <vector>
#include <cstdint>

// ============================================================================
// recording_engine_stub
//   Attach to DUT's hmac_key_socket / kmac_key_socket / aes_key_socket /
//   otbn_key_socket.  Records every TLM_WRITE in order so tests can verify
//   that the correct key material reached the crypto engine.
// ============================================================================
struct engine_write_t {
    uint64_t addr;
    uint32_t data;
};

class recording_engine_stub : public sc_module
{
public:
    tlm_utils::simple_target_socket<recording_engine_stub, 32> socket;
    std::vector<engine_write_t> writes;

    SC_HAS_PROCESS(recording_engine_stub);

    recording_engine_stub(sc_module_name n)
      : sc_module(n), socket("socket")
    {
        socket.register_b_transport(this, &recording_engine_stub::b_transport);
    }

    /// Clear recorded write log.
    void clear() { writes.clear(); }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& /*delay*/)
    {
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND &&
            trans.get_data_ptr() && trans.get_data_length() >= 4)
        {
            uint32_t data = 0;
            std::memcpy(&data, trans.get_data_ptr(), 4);
            writes.push_back({trans.get_address(), data});
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ============================================================================
// keymgr_tt_test
//   TLM initiator + register helpers + mailbox send/receive helpers.
// ============================================================================
class keymgr_tt_test : public keymgr_tt_basetest
{
public:
    SC_HAS_PROCESS(keymgr_tt_test);

    CsmlLogger logger;

    // Test drives reset; test monitors IRQ
    sc_out<bool> rst_no;
    sc_in<bool>  irq_i;

    keymgr_tt_test(sc_module_name name);

    // -----------------------------------------------------------------------
    // Register access helpers
    // -----------------------------------------------------------------------
    void register_read_32 (unsigned int offset, uint32_t &value);
    void register_write_32(unsigned int offset, uint32_t  value);

    // -----------------------------------------------------------------------
    // Test utilities
    // -----------------------------------------------------------------------
    void trigger_reset(unsigned int cycles = 2);

    // -----------------------------------------------------------------------
    // Mailbox helpers
    // -----------------------------------------------------------------------

    /// Build and send a correctly framed command into the inbound mailbox.
    ///   seq     — command sequence number [0..255]
    ///   cmd_id  — one of km_firmware_handler::cmd_id_t
    ///   payload — payload words (may be empty)
    /// Computes CRC-8/ROHC for header and CRC-32C Castagnoli for payload.
    void mb_send_command(uint8_t seq, uint8_t cmd_id,
                         const std::vector<uint32_t>& payload);

    /// Send a raw pre-built frame (header already encoded, no helper CRCs added).
    /// Useful for injecting deliberately malformed frames in framing tests.
    void mb_send_raw_frame(const std::vector<uint32_t>& frame_words);

    /// Read one complete response frame from the outbound mailbox.
    ///   Polls MB_IRQS bit[0] (OUTBOUND_DATA_AVAIL) up to timeout_ns (1 ns steps).
    ///   Returns true and fills frame_words on success; false on timeout.
    ///   Frame = [header, payload[0..N-1], crc32_word] where N = pay_len.
    ///   Header-only response (pay_len=0): frame = [header].
    bool mb_receive_frame(std::vector<uint32_t>& frame_words,
                          unsigned int timeout_ns = 2000);

    /// Parse a RESP_CMD frame into its semantic fields.
    ///   Returns true when frame is structurally a valid RESP_CMD.
    ///   resp_id_out     — response ID byte ([15:8] of header)
    ///   src_seq_out     — echoed request seq  (payload[0] & 0xFF)
    ///   echoed_cmd_out  — echoed command ID   (payload[1] & 0xFF)
    ///   ret_code_out    — return code          (int32_t payload[2])
    ///   ret_arg_out     — return argument      (payload[3], 0 when absent)
    static bool parse_resp_cmd(const std::vector<uint32_t>& frame,
                                uint8_t&  resp_id_out,
                                uint8_t&  src_seq_out,
                                uint8_t&  echoed_cmd_out,
                                int32_t&  ret_code_out,
                                uint32_t& ret_arg_out);

    // -----------------------------------------------------------------------
    // CRC helpers (same algorithm as km_firmware_handler)
    // -----------------------------------------------------------------------
    /// CRC-8/ROHC over bytes[0..2] of header_24bit (poly=0xE0 reflected, init=0xFF, XorOut=0x00).
    static uint8_t  crc8_header(uint32_t header_24bit);

    /// CRC-32C Castagnoli over payload words (poly=0x82F63B78 reflected, init/xor=0xFFFFFFFF).
    static uint32_t crc32_payload(const uint32_t* words, size_t count);

private:
    void initialize_signals();
};
