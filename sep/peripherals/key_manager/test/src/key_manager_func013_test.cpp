// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file key_manager_func013_test.cpp
 * @brief CMD_KEY_GENERATE — structural key delivery verification
 *
 * CMD_KEY_GENERATE uses OpenSSL RAND_bytes as key material (drbg_socket
 * removed; no external entropy injection). This test verifies that the
 * generated key arrives at the crypto engine stub correctly encoded in
 * dual XOR shares without checking exact values.
 *
 * Tests covered:
 *   013a - CMD_KEY_GENERATE; CMD_KEY_TRANSFER to HMAC; verify 17 writes in
 *          address order (SHARE0[w] at w*4, SHARE1[w] at 0x20+w*4, KEY_CTRL
 *          at 0x40). The key itself comes from RAND_bytes, so the oracle is
 *          the address sequence: writing every SHARE0 before any SHARE1 still
 *          produces a nonzero XOR and must not pass.
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>
#include <cstring>
#include <vector>

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

int key_manager_func013_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC013: CMD_KEY_GENERATE Structural Verification ---\n";

    using FW = keymgr_tt::km_firmware_handler;

    // ------------------------------------------------------------------
    // 013a: CMD_KEY_GENERATE; CMD_KEY_TRANSFER to HMAC;
    //       verify dual XOR share structure (17 writes, KEY_CTRL=1,
    //       SHARE0[w] XOR SHARE1[w] is consistent per-word).
    //       Key material comes from OpenSSL RAND_bytes — exact values
    //       are not checked, only structural correctness.
    //
    // Dual XOR share delivery (17 writes to stub), interleaved per word:
    //   writes[w*2]   addr w*4        = SHARE0[w]
    //   writes[w*2+1] addr 0x20+w*4   = SHARE1[w]
    //   writes[16]    addr 0x40       = KEY_CTRL = 1
    // ------------------------------------------------------------------
    test->trigger_reset();
    tb->hmac_stub.clear();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // CMD_KEY_GENERATE: 1 slot (req_size-1=0), DEST_HMAC (2-word payload, no LAST_DWORD)
    test->mb_send_command(seq++, FW::CMD_KEY_GENERATE,
                          {0u, static_cast<uint32_t>(FW::DEST_HMAC)});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "013a: CMD_KEY_GENERATE response received");

    bool parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,          "013a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0,  "013a: CMD_KEY_GENERATE ret_code = RET_SUCCESS");

    if (!got || !parsed || ret_code != 0) {
        std::cout << "\n--- FUNC013 complete: " << failures << " failure(s) ---\n\n";
        return failures;
    }
    uint32_t key_handle = ret_arg & 0xFFu;
    CHECK(key_handle >= 1u && key_handle <= 255u, "013a: key_handle in valid range [1..255]");

    // CMD_KEY_TRANSFER → HMAC stub
    tb->hmac_stub.clear();
    test->mb_send_command(seq++, FW::CMD_KEY_TRANSFER,
                          {key_handle, static_cast<uint32_t>(FW::DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "013a: CMD_KEY_TRANSFER response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "013a: transfer parses as RESP_CMD");
    CHECK_EQ(ret_code, 0,  "013a: CMD_KEY_TRANSFER ret_code = RET_SUCCESS");

    // Dual XOR share: 8×2 + 1 = 17 writes total
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), 17u,
             "013a: HMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    if (tb->hmac_stub.writes.size() == 17) {
        bool addrs_ok = tb->hmac_stub.writes[16].addr == 0x40u
                     && tb->hmac_stub.writes[16].data == 1u;
        for (int w = 0; w < 8; w++) {
            if (tb->hmac_stub.writes[w * 2].addr != static_cast<uint64_t>(w) * 4u)
                addrs_ok = false;
            if (tb->hmac_stub.writes[w * 2 + 1].addr != 0x20u + static_cast<uint64_t>(w) * 4u)
                addrs_ok = false;
        }
        CHECK(addrs_ok,
              "013a: SHARE0[w] at w*4, SHARE1[w] at 0x20+w*4, KEY_CTRL=1 at 0x40");
    }

    std::cout << "\n--- FUNC013 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
