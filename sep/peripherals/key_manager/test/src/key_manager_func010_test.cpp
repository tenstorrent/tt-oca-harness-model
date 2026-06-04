/**
 * @file key_manager_func010_test.cpp
 * @brief CMD_ENGINE_SHRED — clear crypto engine key registers
 *
 * Tests covered:
 *   010a - CMD_ENGINE_SHRED(DEST_HMAC) → RET_SUCCESS; HMAC stub receives
 *          17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL; single pass, zeros)
 *   010b - CMD_ENGINE_SHRED(DEST_AES | DEST_KMAC) → both stubs receive writes
 *   010c - CMD_ENGINE_SHRED with empty payload → RET_FAILURE
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

int key_manager_func010_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC010: CMD_ENGINE_SHRED ---\n";

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
    const uint8_t DEST_AES   = keymgr_tt::km_firmware_handler::DEST_AES;
    // Single pass of zeros: SHARE0[0..7] + SHARE1[0..7] + KEY_CTRL = 17 writes.
    // Multi-pass random overwrites are DPA countermeasures; not modeled.
    const uint32_t SHRED_WRITES = 17u;

    // ------------------------------------------------------------------
    // 010a: CMD_ENGINE_SHRED(DEST_HMAC) → RET_SUCCESS; 17 writes to stub
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED,
                          {static_cast<uint32_t>(DEST_HMAC)});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "010a: CMD_ENGINE_SHRED(HMAC) response received");
    bool parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,         "010a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "010a: ret_code = RET_SUCCESS");
    CHECK_EQ(ret_arg, (uint32_t)DEST_HMAC, "010a: ret_arg = DEST_HMAC bitmask");
    CHECK_EQ((uint32_t)tb->hmac_stub.writes.size(), SHRED_WRITES,
             "010a: HMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // ------------------------------------------------------------------
    // 010b: CMD_ENGINE_SHRED(DEST_AES | DEST_KMAC) → both stubs receive writes
    // ------------------------------------------------------------------
    tb->kmac_stub.clear();
    tb->aes_stub.clear();

    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED,
                          {static_cast<uint32_t>(DEST_AES | DEST_KMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010b: CMD_ENGINE_SHRED(AES|KMAC) response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "010b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "010b: ret_code = RET_SUCCESS");
    CHECK_EQ((uint32_t)tb->kmac_stub.writes.size(), SHRED_WRITES,
             "010b: KMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");
    CHECK_EQ((uint32_t)tb->aes_stub.writes.size(), SHRED_WRITES,
             "010b: AES stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // ------------------------------------------------------------------
    // 010c: CMD_ENGINE_SHRED with empty payload → RET_FAILURE
    //   (firmware handler: p.size() < 1 → send_resp_cmd RET_FAILURE)
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010c: empty payload response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "010c: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "010c: empty payload → RET_FAILURE (-1)");

    std::cout << "\n--- FUNC010 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
