/**
 * @file keymgr_tt_func007_test.cpp
 * @brief CMD_KEY_GENERATE — KM-internal key derivation via HMAC-SHA256 KDF
 *
 * Tests covered:
 *   007a - CMD_KEY_GENERATE(1 slot, DEST_HMAC) → RET_SUCCESS;
 *          ret_arg = key_handle [7:0], req_size [14:8], dest_valid [23:16]
 *   007b - A subsequent CMD_KEY_TRANSFER to HMAC delivers non-zero key material
 *          to the HMAC recording stub
 *   007c - Requesting more slots than available → RET_FAILURE
 *   007d - CMD_KEY_GENERATE with too-short payload → RET_INVALID_LEN
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

int keymgr_tt_func007_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC007: CMD_KEY_GENERATE ---\n";

    test->trigger_reset();
    // Boot will shred engines — clear stubs after reset so our assertions
    // see only the CMD_KEY_TRANSFER writes.
    tb->hmac_stub.clear();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    const uint8_t DEST_HMAC   = keymgr_tt::km_firmware_handler::DEST_HMAC;

    // ------------------------------------------------------------------
    // 007a: CMD_KEY_GENERATE(1 slot, DEST_HMAC) → RET_SUCCESS
    //   Payload: [0]=req_size-1(=0 for 1 word/slot) | [1]=DEST_VALID (2 words)
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE,
                          {0u, static_cast<uint32_t>(DEST_HMAC)});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "007a: CMD_KEY_GENERATE response received");
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,          "007a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0,  "007a: CMD_KEY_GENERATE ret_code = RET_SUCCESS");

    // Verify ret_arg field encoding:
    // [7:0]=key_handle | [14:8]=req_size | [23:16]=dest_valid
    uint32_t gen_handle  = ret_arg & 0xFFu;
    uint32_t gen_reqsize = (ret_arg >> 8)  & 0x7Fu;
    uint32_t gen_dest    = (ret_arg >> 16) & 0xFFu;
    CHECK(gen_handle >= 1u && gen_handle <= 255u, "007a: ret_arg key_handle in [1..255]");
    CHECK_EQ(gen_reqsize, 1u,                    "007a: ret_arg req_size = 1 (1 word/slot request echoed)");
    CHECK_EQ(gen_dest, (uint32_t)DEST_HMAC,      "007a: ret_arg DEST_VALID = DEST_HMAC");
    std::cout << "[INFO] 007a: generated key handle " << gen_handle << "\n";

    // ------------------------------------------------------------------
    // 007b: CMD_KEY_TRANSFER → HMAC stub receives key material
    //   DEST_ENGINE must be a subset of DEST_VALID (DEST_HMAC ⊆ DEST_HMAC OK).
    //   After transfer, verify the stub recorded ≥1 non-zero write.
    // ------------------------------------------------------------------
    tb->hmac_stub.clear();

    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {gen_handle, static_cast<uint32_t>(DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "007b: CMD_KEY_TRANSFER response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "007b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "007b: CMD_KEY_TRANSFER ret_code = RET_SUCCESS");

    // Dual XOR share delivery: SHARE0[w] at writes[w*2], SHARE1[w] at writes[w*2+1],
    //   KEY_CTRL=1 at writes[16]. Total: 8×2 + 1 = 17 writes.
    //   key[w] = SHARE0[w] XOR SHARE1[w] (must be non-zero for DRBG-generated key).
    size_t hmac_writes = tb->hmac_stub.writes.size();
    CHECK(hmac_writes > 0, "007b: HMAC stub received at least one write");
    CHECK_EQ((uint32_t)hmac_writes, 17u,
             "007b: HMAC stub received 17 writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL)");

    // Verify at least one non-zero key word (SHARE0[w] XOR SHARE1[w] != 0)
    bool has_nonzero = false;
    if (hmac_writes >= 16) {
        for (int w = 0; w < 8; w++) {
            uint32_t key_w = tb->hmac_stub.writes[w*2].data ^ tb->hmac_stub.writes[w*2+1].data;
            if (key_w != 0) { has_nonzero = true; break; }
        }
    }
    CHECK(has_nonzero, "007b: reconstructed key material (SHARE0 XOR SHARE1) is non-zero");

    // ------------------------------------------------------------------
    // 007c: Exhaust all remaining slots; next CMD_KEY_GENERATE → RET_FAILURE
    // ------------------------------------------------------------------
    bool got_failure = false;
    for (int attempt = 0; attempt < 10; attempt++) {
        // Request 8 slots each time (req_size-1=7 → 8 words/slot request)
        test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE,
                              {7u, static_cast<uint32_t>(DEST_HMAC)});
        got = test->mb_receive_frame(frame);
        parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                 ret_code, ret_arg);
        if (parsed && ret_code == -1) {
            got_failure = true;
            break;
        }
    }
    CHECK(got_failure, "007c: CMD_KEY_GENERATE returns RET_FAILURE when no free slots");

    // ------------------------------------------------------------------
    // 007d: Too-short payload (need 2 words, send 1) → RET_INVALID_LEN
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE,
                          {0u});  // only 1 word (need 2)
    got = test->mb_receive_frame(frame);
    CHECK(got, "007d: short payload response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "007d: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5, "007d: too-short payload → RET_INVALID_LEN (-5)");

    std::cout << "\n--- FUNC007 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
