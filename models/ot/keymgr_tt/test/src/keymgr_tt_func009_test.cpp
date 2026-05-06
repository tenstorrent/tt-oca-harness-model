/**
 * @file keymgr_tt_func009_test.cpp
 * @brief CMD_KEY_REVOKE — force-shred a KPV slot chain
 *
 * Tests covered:
 *   009a - Register key, CMD_KEY_REVOKE → RET_SUCCESS; ret_arg = key handle
 *   009b - CMD_KEY_REVOKE on already-revoked (non-valid) slot → RET_FAILURE
 *   009c - CMD_KEY_REVOKE with too-short payload → RET_INVALID_LEN
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

int keymgr_tt_func009_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC009: CMD_KEY_REVOKE ---\n";

    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    const uint8_t DEST_HMAC  = keymgr_tt::km_firmware_handler::DEST_HMAC;
    const uint8_t LAST_DWORD = 7;

    // Grant and register a key
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {0u});
    bool got = test->mb_receive_frame(frame);
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    (void)got; (void)parsed;
    uint32_t slot = ret_arg & 0x1Fu;

    // Write 8 key words
    std::vector<uint32_t> key_words;
    for (uint32_t w = 0; w <= LAST_DWORD; w++) {
        key_words.push_back(0xC0DE0000u | w);
        test->register_write_32(keymgr_tt_basetest::kpvlp_key_offset(slot, w), key_words.back());
    }
    uint32_t ctrl_word = (static_cast<uint32_t>(DEST_HMAC) << 9)
                       | (static_cast<uint32_t>(LAST_DWORD) << 17);
    test->register_write_32(keymgr_tt_basetest::kpvlp_ctrl_offset(slot), ctrl_word);
    uint32_t key_crc = keymgr_tt_test::crc32_payload(key_words.data(), key_words.size());

    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER,
                          {slot, static_cast<uint32_t>(LAST_DWORD),
                           static_cast<uint32_t>(DEST_HMAC), key_crc});
    got = test->mb_receive_frame(frame);
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed && ret_code == 0, "009-setup: key registered");
    uint32_t handle = ret_arg & 0xFFu;

    // ------------------------------------------------------------------
    // 009a: CMD_KEY_REVOKE → RET_SUCCESS; ret_arg = key handle
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE, {handle});
    got = test->mb_receive_frame(frame);
    CHECK(got, "009a: CMD_KEY_REVOKE response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,                        "009a: response parses as RESP_CMD");
    CHECK_EQ(ret_code,  0,               "009a: ret_code = RET_SUCCESS");
    CHECK_EQ(ret_arg & 0xFFu, handle,    "009a: ret_arg = revoked key handle");

    // ------------------------------------------------------------------
    // 009b: Revoke the same handle again — it is now non-valid → RET_FAILURE
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE, {handle});
    got = test->mb_receive_frame(frame);
    CHECK(got, "009b: second revoke response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "009b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "009b: revoke non-valid slot → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 009c: Too-short payload (0 words; need 1) → RET_INVALID_LEN
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "009c: empty payload response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "009c: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "009c: empty payload → RET_INVALID_LEN (-5)");

    std::cout << "\n--- FUNC009 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
