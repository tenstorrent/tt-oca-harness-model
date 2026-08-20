/**
 * @file key_manager_func012_test.cpp
 * @brief Reset during operation — hardware reset clears all model state
 *
 * Tests covered:
 *   012a - Load a key; assert reset; verify the handle no longer resolves
 *          (handle registry and KPV are both cleared during boot)
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

int key_manager_func012_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* /*tb*/)
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
    // Load a key so there is a live handle and an occupied KPV slot.
    uint8_t handle = 0;
    ret_code = test->mb_load_key(seq, {0x1234ABCDu, 0x5678EF00u},
                                 keymgr_tt::km_firmware_handler::DEST_HMAC, handle);
    CHECK(ret_code == 0 && handle != 0, "012-setup: CMD_KEY_LOAD success");

    bool got = false, parsed = false;

    // Advance seq counter a few steps
    seq++;  // pretend we sent another message (so seq != 0 after reset)
    seq++;

    // ------------------------------------------------------------------
    // 012a: Assert reset; the pre-reset handle must no longer resolve.
    //   The vault and the handle registry are both cleared on the way through
    //   boot, so transferring the old handle can only fail.
    // ------------------------------------------------------------------
    test->trigger_reset();  // re-enters boot sequence

    test->mb_send_command(0u, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER,
                          {static_cast<uint32_t>(handle),
                           static_cast<uint32_t>(keymgr_tt::km_firmware_handler::DEST_HMAC)});
    got = test->mb_receive_frame(frame);
    CHECK(got, "012a: response received for stale handle after reset");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "012a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "012a: pre-reset handle is no longer valid → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 012b: After reset, firmware seq counter is 0; sending old seq > 0 → RET_CMD_NOSEQ
    //   We use seq=5 (leftover from before reset) → should fail.
    // ------------------------------------------------------------------
    test->mb_send_command(5u, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "012b: response received for stale seq after reset");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
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
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, mb_status);
    // bit[2] = outbound_empty
    bool outbound_empty = (mb_status >> 2) & 0x1u;
    CHECK(outbound_empty, "012c: outbound FIFO empty after consuming the only response");

    // ------------------------------------------------------------------
    // Verify normal operation resumes on the next expected sequence number.
    // 012a already consumed seq=0 after the reset, so the counter now expects 1;
    // the rejected stale command in 012b did not advance it.
    // ------------------------------------------------------------------
    test->mb_send_command(1u, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "012c-extra: CMD_HW_VER on the next expected seq succeeds after reset");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed && ret_code == 0, "012c-extra: normal command succeeds after reset");

    std::cout << "\n--- FUNC012 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
