/**
 * @file keymgr_tt_func012_test.cpp
 * @brief Reset during operation — hardware reset clears all model state
 *
 * Tests covered:
 *   012a - Register a key; assert reset; verify KPVLP_STATUS = 0 after reset
 *          (all slots lose lock / unlock_sep bits — KPV shredded during boot)
 *   012b - After reset, firmware seq counter starts at 0; old seq is rejected
 *   012c - After reset, outbound FIFO is empty (any pending response discarded)
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

int keymgr_tt_func012_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC012: Reset During Operation ---\n";

    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // ---- Set up some state before reset ----
    // Request 2 slots
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {1u});
    bool got = test->mb_receive_frame(frame);
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "012-setup: CMD_KPVLP_SLOT_REQ success");

    uint32_t kpvlp_before = 0;
    test->register_read_32(keymgr_tt_basetest::KPVLP_STATUS_OFFSET, kpvlp_before);
    CHECK(kpvlp_before != 0u, "012-setup: KPVLP_STATUS non-zero (slots unlocked before reset)");

    // Advance seq counter a few steps
    seq++;  // pretend we sent another message (so seq != 0 after reset)
    seq++;

    // ------------------------------------------------------------------
    // 012a: Assert reset; verify KPVLP_STATUS = 0 after reset
    //   After reset, km_kpv::reset() clears all ctrl flags including unlock_sep.
    // ------------------------------------------------------------------
    test->trigger_reset();  // re-enters boot sequence

    uint32_t kpvlp_after = 0xFFFFFFFFu;
    test->register_read_32(keymgr_tt_basetest::KPVLP_STATUS_OFFSET, kpvlp_after);
    CHECK_EQ(kpvlp_after, 0u, "012a: KPVLP_STATUS = 0 after reset (all unlock_sep bits cleared)");

    // ------------------------------------------------------------------
    // 012b: After reset, firmware seq counter is 0; sending old seq > 0 → RET_CMD_NOSEQ
    //   We use seq=5 (leftover from before reset) → should fail.
    // ------------------------------------------------------------------
    test->mb_send_command(5u, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "012b: response received for stale seq after reset");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "012b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -3, "012b: stale seq after reset → RET_CMD_NOSEQ (-3)");

    // ------------------------------------------------------------------
    // 012c: After reset the outbound FIFO was flushed; verify MB_STATUS
    //   shows outbound_empty (bit[2]=1 in reset state).
    //   We need to drain the stale-seq response we just read (already done above),
    //   then verify the FIFO is empty.
    // ------------------------------------------------------------------
    uint32_t mb_status = 0;
    test->register_read_32(keymgr_tt_basetest::MB_STATUS_OFFSET, mb_status);
    // bit[2] = outbound_empty
    bool outbound_empty = (mb_status >> 2) & 0x1u;
    CHECK(outbound_empty, "012c: outbound FIFO empty after consuming the only response");

    // ------------------------------------------------------------------
    // Verify normal operation resumes with seq=0
    // ------------------------------------------------------------------
    test->mb_send_command(0u, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "012c-extra: CMD_HW_VER with seq=0 succeeds after reset");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed && ret_code == 0, "012c-extra: normal seq=0 command succeeds after reset");

    std::cout << "\n--- FUNC012 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
