/**
 * @file key_manager_func010_test.cpp
 * @brief CMD_ENGINE_SHRED — clear crypto engine key registers
 *
 * Tests covered:
 *   010a - CMD_ENGINE_SHRED(DEST_HMAC) → RET_SUCCESS; HMAC stub receives
 *          17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL; single pass, zeros)
 *   010b - CMD_ENGINE_SHRED(DEST_AES | DEST_KMAC) → both stubs receive writes
 *   010c - CMD_ENGINE_SHRED with empty payload → RET_FAILURE
 *   010d - CMD_ENGINE_SHRED(DEST_ABR_MLDSA_SEED) → ABR ML-DSA seed stub
 *          receives 17 writes
 *   010e - CMD_KEY_LOAD(8 words, DEST_ABR_MLDSA_SEED) + CMD_KEY_TRANSFER
 *          → ABR stub receives a 17-write commit (KEY_CTRL=1)
 *   010f - CMD_KEY_LOAD with empty payload → RET_INVALID_LEN
 *   010g - CMD_KEY_LOAD size/payload mismatch → RET_INVALID_LEN
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
    tb->abr_mldsa_seed_stub.clear();

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

    // ------------------------------------------------------------------
    // 010d: CMD_ENGINE_SHRED(DEST_ABR_MLDSA_SEED) → 17 writes to ABR stub
    // ------------------------------------------------------------------
    tb->abr_mldsa_seed_stub.clear();
    const uint8_t DEST_ABR_MLDSA = keymgr_tt::km_firmware_handler::DEST_ABR_MLDSA_SEED;
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED,
                          {static_cast<uint32_t>(DEST_ABR_MLDSA)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010d: CMD_ENGINE_SHRED(ABR ML-DSA seed) response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                              ret_code, ret_arg);
    CHECK(parsed,         "010d: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "010d: ret_code = RET_SUCCESS");
    CHECK_EQ(ret_arg, (uint32_t)DEST_ABR_MLDSA, "010d: ret_arg = DEST_ABR_MLDSA_SEED");
    CHECK_EQ((uint32_t)tb->abr_mldsa_seed_stub.writes.size(), SHRED_WRITES,
             "010d: ABR ML-DSA seed stub received 17 writes");

    // ------------------------------------------------------------------
    // 010e: CMD_KEY_LOAD + CMD_KEY_TRANSFER to ABR ML-DSA seed
    // ------------------------------------------------------------------
    tb->abr_mldsa_seed_stub.clear();
    {
        std::vector<uint32_t> load = { 7u, static_cast<uint32_t>(DEST_ABR_MLDSA) };
        for (uint32_t i = 0; i < 8; i++)
            load.push_back(0x1000u + i);
        test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_LOAD, load);
        got = test->mb_receive_frame(frame);
        CHECK(got, "010e: CMD_KEY_LOAD response received");
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
        CHECK(parsed,         "010e: KEY_LOAD parses as RESP_CMD");
        CHECK_EQ(ret_code, 0, "010e: KEY_LOAD ret_code = RET_SUCCESS");
        const uint32_t handle = ret_arg & 0xFFu;
        CHECK(handle != 0u, "010e: KEY_LOAD returned a non-zero handle");

        test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                              {handle, static_cast<uint32_t>(DEST_ABR_MLDSA)});
        got = test->mb_receive_frame(frame);
        CHECK(got, "010e: CMD_KEY_TRANSFER response received");
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
        CHECK(parsed,         "010e: KEY_TRANSFER parses as RESP_CMD");
        CHECK_EQ(ret_code, 0, "010e: KEY_TRANSFER ret_code = RET_SUCCESS");
        CHECK_EQ((uint32_t)tb->abr_mldsa_seed_stub.writes.size(), SHRED_WRITES,
                 "010e: ABR stub received 17 writes after KEY_TRANSFER");
    }

    // ------------------------------------------------------------------
    // 010f: CMD_KEY_LOAD empty payload → RET_INVALID_LEN
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_LOAD, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010f: empty KEY_LOAD response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                              ret_code, ret_arg);
    CHECK(parsed,          "010f: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "010f: empty payload → RET_INVALID_LEN (-5)");

    // ------------------------------------------------------------------
    // 010g: KEY_SIZE claims 4 words but only one word is supplied
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_LOAD,
                          {3u, static_cast<uint32_t>(DEST_ABR_MLDSA), 0xA5A5A5A5u});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010g: mismatched KEY_LOAD response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                              ret_code, ret_arg);
    CHECK(parsed,          "010g: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "010g: size/payload mismatch → RET_INVALID_LEN (-5)");

    // ------------------------------------------------------------------
    // 010h: KEY_SIZE_M1=12 → 13 words, over the 12-word firmware cap
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_LOAD,
                          {12u, static_cast<uint32_t>(DEST_ABR_MLDSA)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "010h: oversize KEY_LOAD response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                              ret_code, ret_arg);
    CHECK(parsed,          "010h: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "010h: KEY_SIZE > 12 → RET_INVALID_LEN (-5)");

    std::cout << "\n--- FUNC010 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
