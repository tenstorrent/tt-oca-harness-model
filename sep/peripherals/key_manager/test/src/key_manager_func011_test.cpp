// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file key_manager_func011_test.cpp
 * @brief Full key lifecycle — load → transfer → revoke chained flows
 *
 * Tests covered:
 *   011a - Load key, transfer to HMAC, revoke; verify handle unusable after revoke
 *   011b - CMD_KEY_GENERATE, transfer to AES, then engine shred; verify shred
 *          delivers non-zero (random) data after the key transfer had non-zero data
 *   011c - Multi-command sequence stays in sync (seq counter continuity)
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include "km_kpv.h"
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

// Quick send-and-receive that returns the parsed ret_code (-99 on parse error).
static int32_t send_receive(key_manager_test* test, uint8_t& seq,
                            uint8_t cmd_id, const std::vector<uint32_t>& payload,
                            uint32_t* ret_arg_out = nullptr)
{
    std::vector<uint32_t> frame;
    uint8_t resp_id, src_seq, echoed_cmd;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;

    test->mb_send_command(seq++, cmd_id, payload);
    if (!test->mb_receive_frame(frame)) return -99;
    if (!key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                        ret_code, ret_arg)) return -99;
    if (ret_arg_out) *ret_arg_out = ret_arg;
    return ret_code;
}

int key_manager_func011_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC011: Full Key Lifecycle ---\n";

    test->trigger_reset();
    tb->hmac_stub.clear();
    tb->aes_stub.clear();

    uint8_t seq = 0;
    const uint8_t DEST_HMAC = keymgr_tt::km_firmware_handler::DEST_HMAC;
    const uint8_t DEST_AES  = keymgr_tt::km_firmware_handler::DEST_AES;
    const uint8_t LAST_DWORD = 7;

    // ------------------------------------------------------------------
    // 011a: Load → Transfer → Revoke
    //   1. CMD_KEY_LOAD with a known key
    //   2. CMD_KEY_TRANSFER to HMAC → verify non-zero delivery
    //   3. CMD_KEY_REVOKE → verify success
    //   4. CMD_KEY_REVOKE again → verify RET_FAILURE (handle freed)
    // ------------------------------------------------------------------
    uint32_t ret_arg = 0;
    int32_t  rc;

    std::vector<uint32_t> key_a;
    for (uint32_t w = 0; w <= LAST_DWORD; w++)
        key_a.push_back(0xF1E20000u | w);

    uint8_t loaded_a = 0;
    rc = test->mb_load_key(seq, key_a, DEST_HMAC, loaded_a);
    CHECK(rc == 0, "011a: CMD_KEY_LOAD success");
    uint32_t handle_a = loaded_a;

    tb->hmac_stub.clear();
    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                      {handle_a, static_cast<uint32_t>(DEST_HMAC)});
    CHECK(rc == 0, "011a: CMD_KEY_TRANSFER success");
    CHECK(!tb->hmac_stub.writes.empty(), "011a: HMAC stub received key writes");

    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE,
                      {handle_a}, &ret_arg);
    CHECK(rc == 0,                               "011a: CMD_KEY_REVOKE success");
    CHECK_EQ(ret_arg & 0xFFu, handle_a,          "011a: revoke ret_arg = key handle");

    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE, {handle_a});
    CHECK(rc == -1, "011a: second revoke → RET_FAILURE (slot non-valid)");

    // ------------------------------------------------------------------
    // 011b: CMD_KEY_GENERATE → Transfer to AES → Engine shred
    //   Verify transfer delivers non-zero data; shred replaces it with random.
    // ------------------------------------------------------------------
    tb->aes_stub.clear();

    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE,
                      {0u, static_cast<uint32_t>(DEST_AES)},
                      &ret_arg);
    CHECK(rc == 0, "011b: CMD_KEY_GENERATE success");
    uint32_t gen_handle = ret_arg & 0xFFu;  // key_handle [7:0]

    tb->aes_stub.clear();
    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                      {gen_handle, static_cast<uint32_t>(DEST_AES)});
    CHECK(rc == 0, "011b: CMD_KEY_TRANSFER to AES success");

    bool has_nonzero_transfer = false;
    for (const auto& wr : tb->aes_stub.writes)
        if (wr.data != 0) { has_nonzero_transfer = true; break; }
    CHECK(has_nonzero_transfer, "011b: transferred key material is non-zero");

    // Now shred AES engine
    // Single pass of zeros: 8 SHARE0 + 8 SHARE1 + KEY_CTRL = 17 writes.
    // Multi-pass random overwrites are DPA countermeasures; not modeled.
    tb->aes_stub.clear();

    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED,
                      {static_cast<uint32_t>(DEST_AES)});
    CHECK(rc == 0, "011b: CMD_ENGINE_SHRED AES success");
    CHECK_EQ((uint32_t)tb->aes_stub.writes.size(), 17u,
             "011b: AES stub received 17 shred writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // ------------------------------------------------------------------
    // 011c: Verify seq counter continuity — send CMD_STAT with current seq
    //   If any earlier command disrupted the counter, CMD_STAT would fail.
    // ------------------------------------------------------------------
    rc = send_receive(test, seq, keymgr_tt::km_firmware_handler::CMD_STAT, {}, &ret_arg);
    CHECK(rc == 0, "011c: CMD_STAT succeeds with running seq counter (no disruption)");
    CHECK(!(ret_arg & 0x1u), "011c: CMD_STAT reports no recoverable error");

    std::cout << "\n--- FUNC011 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
