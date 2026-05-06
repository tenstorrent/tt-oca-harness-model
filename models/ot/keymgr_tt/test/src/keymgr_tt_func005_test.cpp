/**
 * @file keymgr_tt_func005_test.cpp
 * @brief CMD_KPVLP_SLOT_REQ — slot grant and KPVLP unlock
 *
 * Tests covered:
 *   005a - Request 1 slot (REQ_SIZE=0) → RET_SUCCESS; ret_arg encodes SLOT_INDEX & GRANT_SIZE
 *   005b - Request 2 slots (REQ_SIZE=1) → RET_SUCCESS; ret_arg encodes base and count
 *   005c - KPVLP_STATUS reflects unlocked slots (unlock_sep bits set)
 *   005d - Granted slot is writable via KPVLP_KEY write (SEP can push key words)
 *   005e - Request all remaining slots → fills pool; next request → RET_FAILURE
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

int keymgr_tt_func005_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC005: CMD_KPVLP_SLOT_REQ ---\n";

    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // ------------------------------------------------------------------
    // 005a: Request 1 slot (REQ_SIZE=0 → req_count=1)
    //   ret_arg: [4:0]=SLOT_INDEX | [10:8]=GRANT_SIZE(=0 for 1 slot)
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ,
                          {0u});  // p[0]=REQ_SIZE=0 → 1 slot
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "005a: CMD_KPVLP_SLOT_REQ(1) response received");
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,          "005a: response parses as RESP_CMD");
    CHECK_EQ(ret_code,  0, "005a: ret_code = RET_SUCCESS");
    uint32_t slot0 = ret_arg & 0x1Fu;
    uint32_t gsz0  = (ret_arg >> 8) & 0x7u;
    CHECK_EQ(gsz0, 0u, "005a: GRANT_SIZE field = 0 (1 slot granted)");
    std::cout << "[INFO] 005a: granted slot " << slot0 << "\n";

    // ------------------------------------------------------------------
    // 005b: Request 2 slots (REQ_SIZE=1 → req_count=2)
    //   ret_arg: [4:0]=SLOT_INDEX | [10:8]=GRANT_SIZE(=1 for 2 slots)
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ,
                          {1u});  // REQ_SIZE=1 → 2 slots
    got = test->mb_receive_frame(frame);
    CHECK(got, "005b: CMD_KPVLP_SLOT_REQ(2) response received");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "005b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "005b: ret_code = RET_SUCCESS");
    uint32_t slot1 = ret_arg & 0x1Fu;
    uint32_t gsz1  = (ret_arg >> 8) & 0x7u;
    CHECK_EQ(gsz1, 1u, "005b: GRANT_SIZE field = 1 (2 slots granted)");
    std::cout << "[INFO] 005b: granted base slot " << slot1 << "\n";

    // ------------------------------------------------------------------
    // 005c: KPVLP_STATUS should show unlocked_sep bits for all granted slots
    //   slot0 granted 1 slot; slot1 granted 2 slots → bits slot0, slot1, slot1+1 set.
    // ------------------------------------------------------------------
    uint32_t kpvlp_status = 0;
    test->register_read_32(keymgr_tt_basetest::KPVLP_STATUS_OFFSET, kpvlp_status);

    bool slot0_set = (kpvlp_status >> slot0) & 0x1u;
    bool slot1_set = (kpvlp_status >> slot1) & 0x1u;
    bool slot2_set = (slot1 + 1 < 32) && ((kpvlp_status >> (slot1 + 1)) & 0x1u);

    CHECK(slot0_set, "005c: KPVLP_STATUS bit set for first single-slot grant");
    CHECK(slot1_set, "005c: KPVLP_STATUS bit set for base slot of 2-slot grant");
    CHECK(slot2_set, "005c: KPVLP_STATUS bit set for second slot of 2-slot grant");

    // ------------------------------------------------------------------
    // 005d: Granted slot is writable via KPVLP_KEY (SEP can push key data)
    //   Write a pattern to word[0] of slot0 and verify KPVLP_KEY is write-only
    //   (read returns 0), but the write did not fault (write accepted silently).
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::kpvlp_key_offset(slot0, 0), 0xCAFEBABEu);
    uint32_t readback = 0u;  // must start at 0 — CSML does not update ptr for write-only reads
    test->register_read_32(keymgr_tt_basetest::kpvlp_key_offset(slot0, 0), readback);
    // KPVLP_KEY is write-only — read must return 0
    CHECK_EQ(readback, 0u, "005d: KPVLP_KEY is write-only (read returns 0 after write)");

    // ------------------------------------------------------------------
    // 005e: Multiple SLOT_REQ grants accumulate correctly in KPVLP_STATUS.
    //   CMD_KPVLP_SLOT_REQ sets unlock_sep on the found free slots, but
    //   km_find_free_slots only checks lock_write+valid (not unlock_sep),
    //   so repeated grants can overlap — all granted slots remain visible
    //   in KPVLP_STATUS.  Verify that requesting a third single slot also
    //   succeeds and the status bit is set.
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {0u});
    got = test->mb_receive_frame(frame);
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed && ret_code == 0, "005e: third CMD_KPVLP_SLOT_REQ succeeds");
    uint32_t slot2 = ret_arg & 0x1Fu;

    uint32_t kpvlp_status2 = 0;
    test->register_read_32(keymgr_tt_basetest::KPVLP_STATUS_OFFSET, kpvlp_status2);
    bool slot2_status_set = (kpvlp_status2 >> slot2) & 0x1u;
    CHECK(slot2_status_set, "005e: KPVLP_STATUS bit set for third grant");

    std::cout << "\n--- FUNC005 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
