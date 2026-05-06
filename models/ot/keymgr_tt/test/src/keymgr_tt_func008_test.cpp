/**
 * @file keymgr_tt_func008_test.cpp
 * @brief CMD_KEY_TRANSFER — moving KPV key data to crypto engine key registers
 *
 * Tests covered:
 *   008a - Register key in KPV; CMD_KEY_TRANSFER to HMAC; verify key via SHARE0 XOR SHARE1
 *   008b - Transfer to destination not in DEST_VALID → RET_FAILURE
 *   008c - Transfer invalid (non-valid) slot → RET_FAILURE
 *   008d - After first transfer, lock_use is set; second transfer sends all-zero key words
 *          (model behaviour: lock_use prevents km_read_key_word from returning data)
 *   008e - CMD_KEY_TRANSFER with too-short payload → RET_FAILURE
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { std::cout << "[FAIL] " << msg << std::endl; failures++; } \
        else          { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

#define CHECK_EQ(got, expected, msg) \
    do { \
        if ((got) != (expected)) { \
            std::cout << "[FAIL] " << msg \
                      << "  got=0x" << std::hex << (uint32_t)(got) \
                      << "  expected=0x" << (uint32_t)(expected) << std::dec << std::endl; \
            failures++; \
        } else { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

// Helper: grant a slot, write key words, write ctrl, compute CRC, register key.
// Returns base slot index or -1 on failure.
static int register_key_in_kpv(keymgr_tt_test* test,
                                uint8_t& seq,
                                const std::vector<uint32_t>& key_words,
                                uint8_t extend,
                                uint8_t dest_valid,
                                uint8_t last_dword)
{
    // Request 1 slot (extend must be 0 for single-slot keys in this helper)
    std::vector<uint32_t> frame;
    uint8_t resp_id, src_seq, echoed_cmd;
    int32_t ret_code;
    uint32_t ret_arg;

    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {0u});
    if (!test->mb_receive_frame(frame)) return -1;
    if (!keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                        ret_code, ret_arg)) return -1;
    if (ret_code != 0) return -1;

    int slot = static_cast<int>(ret_arg & 0x1Fu);

    // Write key words via KPVLP_KEY
    for (unsigned int w = 0; w < key_words.size(); w++)
        test->register_write_32(keymgr_tt_basetest::kpvlp_key_offset(slot, w), key_words[w]);

    // Write KPVLP_CTRL
    uint32_t ctrl_word = (static_cast<uint32_t>(extend)     << 4)
                       | (static_cast<uint32_t>(dest_valid)  << 9)
                       | (static_cast<uint32_t>(last_dword)  << 17);
    test->register_write_32(keymgr_tt_basetest::kpvlp_ctrl_offset(slot), ctrl_word);

    // Compute CRC of key words
    uint32_t crc = keymgr_tt_test::crc32_payload(key_words.data(), key_words.size());

    // Send CMD_KEY_REGISTER (4-word payload: slot, key_size-1, dest_valid, crc)
    uint8_t key_size_m1 = last_dword;  // key_size-1 = last_dword (for single-slot keys)
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER,
                          {static_cast<uint32_t>(slot),
                           static_cast<uint32_t>(key_size_m1),
                           static_cast<uint32_t>(dest_valid),
                           crc});
    if (!test->mb_receive_frame(frame)) return -1;
    if (!keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                        ret_code, ret_arg)) return -1;
    return (ret_code == 0) ? static_cast<int>(ret_arg & 0xFFu) : -1;
}

int keymgr_tt_func008_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC008: CMD_KEY_TRANSFER ---\n";

    test->trigger_reset();
    tb->hmac_stub.clear();
    tb->kmac_stub.clear();
    tb->aes_stub.clear();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    const uint8_t DEST_HMAC  = keymgr_tt::km_firmware_handler::DEST_HMAC;
    const uint8_t DEST_KMAC  = keymgr_tt::km_firmware_handler::DEST_KMAC;

    // Build a known key pattern for slot 0 (8 words, last_dword=7)
    const uint8_t LAST_DWORD = 7;
    std::vector<uint32_t> known_key;
    for (uint32_t w = 0; w <= LAST_DWORD; w++)
        known_key.push_back(0xB0C0D000u | w);

    // Register the key with DEST_VALID = HMAC only
    int handle = register_key_in_kpv(test, seq, known_key, 0, DEST_HMAC, LAST_DWORD);
    CHECK(handle >= 0, "008-setup: key registered successfully");
    if (handle < 0) {
        std::cout << "--- FUNC008 aborted (setup failed) ---\n\n";
        return failures + 1;
    }

    // ------------------------------------------------------------------
    // 008a: CMD_KEY_TRANSFER to HMAC → verify key via SHARE0 XOR SHARE1
    // ------------------------------------------------------------------
    tb->hmac_stub.clear();
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(DEST_HMAC)});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "008a: CMD_KEY_TRANSFER response received");
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,         "008a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "008a: ret_code = RET_SUCCESS");
    // Dual XOR share: SHARE0[w] at writes[w*2], SHARE1[w] at writes[w*2+1].
    //   KEY_CTRL=1 at writes[16]. Total: 8×2 + 1 = 17 writes.
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), 17u,
             "008a: HMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // Dual XOR share: SHARE0[w] at writes[w*2], SHARE1[w] at writes[w*2+1].
    // Verify key[w] = SHARE0[w] XOR SHARE1[w] matches known_key[w].
    bool key_match = true;
    if (tb->hmac_stub.writes.size() >= 16) {
        for (size_t w = 0; w < 8 && w < known_key.size(); w++) {
            uint32_t reconstructed = tb->hmac_stub.writes[w*2].data
                                   ^ tb->hmac_stub.writes[w*2+1].data;
            if (reconstructed != known_key[w]) { key_match = false; break; }
        }
    }
    CHECK(key_match, "008a: reconstructed key (SHARE0 XOR SHARE1) matches registered key");

    // ------------------------------------------------------------------
    // 008b: Transfer to KMAC (not in DEST_VALID) → RET_FAILURE
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(DEST_KMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "008b: transfer to disallowed engine response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "008b: KMAC not in DEST_VALID → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 008c: Transfer invalid (never-registered) slot 31 → RET_FAILURE
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {31u, static_cast<uint32_t>(DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "008c: transfer of invalid slot response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008c: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "008c: invalid (non-valid) slot → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 008d: lock_use one-shot: second transfer on same handle sends zero key data.
    //   The first transfer (008a) set lock_use on the slot.
    //   km_read_key_word now returns 0 for all words (lock_use=1).
    //   The second CMD_KEY_TRANSFER still returns RET_SUCCESS but delivers zeros.
    // ------------------------------------------------------------------
    tb->hmac_stub.clear();
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "008d: second transfer response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "008d: second transfer parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "008d: second transfer returns RET_SUCCESS (lock_use does not block FW dispatch)");

    // With lock_use: km_read_key_word returns 0 → key[w]=0 → SHARE1[w]=SHARE0[w].
    // Verify SHARE0[w] XOR SHARE1[w] == 0 for all w.
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), 17u,
             "008d: second transfer still delivers 17 writes to engine");
    bool all_zero = true;
    if (tb->hmac_stub.writes.size() >= 16) {
        for (size_t w = 0; w < 8; w++) {
            uint32_t xor_val = tb->hmac_stub.writes[w*2].data ^ tb->hmac_stub.writes[w*2+1].data;
            if (xor_val != 0u) { all_zero = false; break; }
        }
    }
    CHECK(all_zero, "008d: lock_use — SHARE0 XOR SHARE1 = 0 on second transfer (key words zero)");

    // ------------------------------------------------------------------
    // 008e: Too-short payload (need 2 words, send 1) → RET_FAILURE
    //   Note: the firmware handler uses RET_FAILURE (not RET_INVALID_LEN) for
    //   CMD_KEY_TRANSFER short payload (p.size() < 2 check).
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle)});  // only 1 word
    got = test->mb_receive_frame(frame);
    CHECK(got, "008e: short payload response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008e: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "008e: short payload → RET_FAILURE (-1)");

    std::cout << "\n--- FUNC008 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
