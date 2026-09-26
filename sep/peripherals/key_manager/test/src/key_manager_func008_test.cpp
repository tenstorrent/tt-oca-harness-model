// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file key_manager_func008_test.cpp
 * @brief CMD_KEY_TRANSFER — moving KPV key data to crypto engine key registers
 *
 * Tests covered:
 *   008a - Load key via CMD_KEY_LOAD; CMD_KEY_TRANSFER to HMAC; verify key via SHARE0 XOR SHARE1
 *   008b - Transfer to destination not in DEST_VALID → RET_FAILURE
 *   008c - Transfer invalid (non-valid) slot → RET_FAILURE
 *   008d - After the first transfer lock_use is set, so a second transfer of the
 *          same handle is refused with RET_FAILURE and reaches no engine
 *   008e - CMD_KEY_TRANSFER with the wrong payload length → RET_INVALID_LEN
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

// Helper: provision a key through CMD_KEY_LOAD.
// Returns the allocated handle, or -1 on failure.
static int register_key_in_kpv(key_manager_test* test,
                                uint8_t& seq,
                                const std::vector<uint32_t>& key_words,
                                uint8_t dest_valid)
{
    uint8_t handle = 0;
    if (test->mb_load_key(seq, key_words, dest_valid, handle) != 0) return -1;
    return static_cast<int>(handle);
}

int key_manager_func008_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
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

    // Load the key with DEST_VALID = HMAC only
    int handle = register_key_in_kpv(test, seq, known_key, DEST_HMAC);
    CHECK(handle >= 0, "008-setup: key loaded successfully");
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
    bool parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,         "008a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "008a: ret_code = RET_SUCCESS");
    // Dual XOR share: SHARE0[w] at writes[w*2], SHARE1[w] at writes[w*2+1].
    //   KEY_CTRL=1 at writes[16]. Total: 8×2 + 1 = 17 writes.
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), 17u,
             "008a: HMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // Address order is part of the oracle: SHARE0[w] at w*4, then SHARE1[w]
    // at 0x20+w*4. Index pairing alone would still reconstruct a key if the
    // model wrote the two shares of each word back to back, but a run of all
    // SHARE0 followed by all SHARE1 lands the words at different addresses.
    bool key_match = tb->hmac_stub.writes.size() == 17
                  && tb->hmac_stub.writes[16].addr == 0x40u
                  && tb->hmac_stub.writes[16].data == 1u;
    if (key_match) {
        for (size_t w = 0; w < known_key.size(); w++) {
            if (tb->hmac_stub.writes[w * 2].addr != w * 4u ||
                tb->hmac_stub.writes[w * 2 + 1].addr != 0x20u + w * 4u) {
                key_match = false;
                break;
            }
            uint32_t reconstructed = tb->hmac_stub.writes[w * 2].data
                                   ^ tb->hmac_stub.writes[w * 2 + 1].data;
            if (reconstructed != known_key[w]) { key_match = false; break; }
        }
    }
    CHECK(key_match, "008a: address-ordered SHARE0 XOR SHARE1 matches the registered key");

    // ------------------------------------------------------------------
    // 008b: Transfer to KMAC (not in DEST_VALID) → RET_FAILURE
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(DEST_KMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "008b: transfer to disallowed engine response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
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
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008c: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "008c: invalid (non-valid) slot → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 008d: lock_use makes a transfer one-shot. The first transfer (008a) set
    //   lock_use across the key's slots, and reading a key back requires the whole
    //   span to be readable, so a second transfer fails outright and nothing
    //   further reaches the engine.
    // ------------------------------------------------------------------
    tb->hmac_stub.clear();
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "008d: second transfer response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008d: second transfer parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "008d: second transfer on a locked key → RET_FAILURE (-1)");
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), 0u,
             "008d: no key material reaches the engine on the refused transfer");

    // ------------------------------------------------------------------
    // 008e: CMD_KEY_TRANSFER takes exactly two payload words. A mismatch is
    //   caught by the dispatcher's length check before the handler runs, and the
    //   return argument reports the length that arrived.
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle)});  // only 1 word
    got = test->mb_receive_frame(frame);
    CHECK(got, "008e: short payload response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "008e: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "008e: wrong payload length → RET_INVALID_LEN (-5)");
    CHECK_EQ(ret_arg,  1u, "008e: ret_arg echoes the payload length received");

    std::cout << "\n--- FUNC008 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
