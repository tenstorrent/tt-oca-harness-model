// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file key_manager_func003_test.cpp
 * @brief Boot state and info commands
 *
 * Tests covered:
 *   003a - CMD_STAT after boot: bit[0]=0 (no recoverable error active)
 *   003b - CMD_ROM_VER: RET_SUCCESS, version = 0x00010000 (ROM 1.0.0)
 *   003c - CMD_SRAM_VER: RET_FAILURE (SRAM firmware not loaded in model)
 *   003d - CMD_HW_VER: RET_SUCCESS, version = 0x00010000 (HW 1.0.0)
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

int key_manager_func003_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC003: Boot State & Info Commands ---\n";

    // Clean state
    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // ------------------------------------------------------------------
    // 003a: CMD_STAT — verify KM is initialized, SRAM not loaded
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_STAT, {});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "003a: CMD_STAT response received");
    bool parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,             "003a: CMD_STAT parses as RESP_CMD");
    CHECK_EQ(ret_code, 0,     "003a: CMD_STAT ret_code = RET_SUCCESS");
    CHECK(!(ret_arg & 0x1u), "003a: CMD_STAT status bit[0] = 0 (no recoverable error after boot)");

    // ------------------------------------------------------------------
    // 003b: CMD_ROM_VER — expect RET_SUCCESS, version 1.0.0
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ROM_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "003b: CMD_ROM_VER response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "003b: CMD_ROM_VER parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "003b: CMD_ROM_VER ret_code = RET_SUCCESS");
    constexpr uint32_t ROM_VER_EXPECTED = (1u << 16) | (0u << 8) | 0u;
    CHECK_EQ(ret_arg, ROM_VER_EXPECTED, "003b: ROM version = 0x00010000 (1.0.0)");

    // ------------------------------------------------------------------
    // 003c: CMD_SRAM_VER — model never loads SRAM, expect RET_FAILURE
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_SRAM_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "003c: CMD_SRAM_VER response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "003c: CMD_SRAM_VER parses as RESP_CMD");
    CHECK_EQ(ret_code, -1, "003c: CMD_SRAM_VER ret_code = RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 003d: CMD_HW_VER — expect RET_SUCCESS, version 1.0.0
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "003d: CMD_HW_VER response received");
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "003d: CMD_HW_VER parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "003d: CMD_HW_VER ret_code = RET_SUCCESS");
    constexpr uint32_t HW_VER_EXPECTED = (1u << 16) | (0u << 8) | 0u;
    CHECK_EQ(ret_arg, HW_VER_EXPECTED, "003d: HW version = 0x00010000 (1.0.0)");

    std::cout << "\n--- FUNC003 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
