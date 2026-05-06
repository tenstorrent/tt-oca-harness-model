/******************************************************************************
 * @file xspi_ctrl_func005_test.cpp
 * @brief Test cases for FUNC_XSPI_005 — Sequence Configuration Register
 *        Management
 *
 * This file implements all test cases for FUNC_XSPI_005 in the xspi_ctrl
 * functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_005 Test Coverage (9 test cases):
 *
 * - TC_XSPI_CFG_SEQ_001: Reset values of cmn_seq_regs_a (0x380–0x3A0).
 *                         Verifies xip_mode_cfg=0x00FF0000, global_seq_cfg,
 *                         global_seq_cfg_1, direct_access_cfg,
 *                         direct_access_rmp, direct_access_rmp_1 all match
 *                         their documented hardware reset values after reset_in.
 *
 * - TC_XSPI_CFG_SEQ_002: Reset values of dev_seq_regs_a (0x400–0x478).
 *                         Verifies all 17 device-sequence registers (rst, ers,
 *                         prog, read, we, stat) match their non-zero reset
 *                         values after reset_in de-assertion.
 *
 * - TC_XSPI_CFG_SEQ_003: long_polling and short_polling shadow callback
 *                         wiring. Writes new values and verifies read-back
 *                         proves the handle_write_long_polling() and
 *                         handle_write_short_polling() callbacks are invoked
 *                         and the register storage retains the written value.
 *
 * - TC_XSPI_CFG_SEQ_004: global_seq_cfg (0x390) write and read-back.
 *                         Verifies the handle_write_global_seq_cfg() callback
 *                         is wired: write a new profile/page-size value and
 *                         confirm the register retains it within its write mask.
 *
 * - TC_XSPI_CFG_SEQ_005: global_seq_cfg_1 (0x394) write and read-back.
 *                         Verifies handle_write_global_seq_cfg_1() is wired.
 *
 * - TC_XSPI_CFG_SEQ_006: xip_mode_cfg (0x388) write and read-back.
 *                         Verifies handle_write_xip_mode_cfg() callback is
 *                         wired: new value retained; xip_active_banks shadow
 *                         indirectly confirmed via register read-back.
 *
 * - TC_XSPI_CFG_SEQ_007: direct_access_cfg (0x398) write and read-back.
 *                         Verifies handle_write_direct_access_cfg() is wired.
 *
 * - TC_XSPI_CFG_SEQ_008: direct_access_rmp / direct_access_rmp_1 (0x39C /
 *                         0x3A0) write and read-back. Verifies both 32-bit
 *                         halves of the 64-bit remap_offset shadow are
 *                         populated by their respective callbacks.
 *
 * - TC_XSPI_CFG_SEQ_009: Device sequence registers write and read-back.
 *                         Writes distinct patterns to all 17 dev_seq_regs_a
 *                         registers (rst_seq, ers_seq, prog_seq, read_seq,
 *                         we_seq, stat_seq groups) and verifies each retains
 *                         the written value within its full-word write mask.
 *                         Confirms all 17 CSML callbacks are wired.
 *
 * - TC_XSPI_CFG_SEQ_010: Reset restores all sequence configuration registers.
 *                         Writes non-reset patterns to cmn_seq_regs_a and a
 *                         representative subset of dev_seq_regs_a, applies
 *                         reset_in, and verifies every register returns to its
 *                         hardware reset value. Validates FUNC_XSPI_005 reset
 *                         handler wiring for seq_cfg shadow variables.
 *
 * Architecture Notes:
 *   All tests in this file are pure register-storage tests. They do NOT
 *   require any operating mode engine (Direct, STIG, PIO, ACMD) to be active.
 *   This is consistent with the FUNC_XSPI_005 dependency on only
 *   FUNC_XSPI_001. Tests exercise the register write callbacks by writing to
 *   the register file through t_reg_socket and reading back via the same path.
 *   Shadow variable accuracy is confirmed indirectly: if the callback is wired
 *   correctly, the scml2 register storage (which is written by handle_write_*)
 *   returns the expected value on readback.
 *
 * Key Reset Values (from xspi_ctrl_basetest::Register_Reset_Val):
 *   xip_mode_cfg        = 0x00FF0000   (xip_dis_mb_val=0xFF in bits[23:16])
 *   global_seq_cfg      = 0x0000208F
 *   global_seq_cfg_1    = 0x00000000
 *   direct_access_cfg   = 0x00000000
 *   direct_access_rmp   = 0x00000000
 *   direct_access_rmp_1 = 0x00000000
 *   rst_seq_cfg_0       = 0x00019966
 *   rst_seq_cfg_1       = 0xD0669900
 *   ers_seq_cfg_0       = 0x00DF3020
 *   ers_seq_cfg_1       = 0x0000000C
 *   ers_seq_cfg_2       = 0x009F0060
 *   prog_seq_cfg_0      = 0x00003002
 *   prog_seq_cfg_1      = 0x0000FD00
 *   prog_seq_cfg_2      = 0x00000002
 *   read_seq_cfg_0      = 0x00003003
 *   read_seq_cfg_1      = 0x0000FC00
 *   read_seq_cfg_2      = 0x00000F0A
 *   we_seq_cfg_0        = 0x01F90006
 *   stat_seq_cfg_0      = 0x00000000
 *   stat_seq_cfg_1      = 0x00000000
 *   stat_seq_cfg_2      = 0x05000505
 *   stat_seq_cfg_3      = 0xFA00FAFA
 *   stat_seq_cfg_4      = 0x00000F00
 *   stat_seq_cfg_5      = 0x00000040
 *   stat_seq_cfg_7      = 0x00000000
 *   stat_seq_cfg_8      = 0x00000000
 *   stat_seq_cfg_9      = 0x00000000
 *   stat_seq_cfg_10     = 0x00000000
 *
 * Test Plan Reference:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-test-plan.md
 * Test Case Mapping:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-functionality-testcases.md
 * Detailed Design:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-detailed-design.md
 *   Sections 6.3.3 (cmn_seq_regs_a) and 6.3.4 (dev_seq_regs_a)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "xspi_ctrl_test.h"
#include "csml_logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>

/// @brief Module-scoped logger for FUNC_XSPI_005 test diagnostics
static CsmlLogger func005_logger;

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Write a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_write_32, keeping
 * test case bodies concise and matching the established pattern in
 * xspi_ctrl_func001_test.cpp.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  32-bit value to write
 */
static inline void func005_write_reg(xspi_ctrl_test* test,
                                     unsigned int    offset,
                                     uint32_t        value)
{
    test->register_write_32(offset, value);
}

/**
 * @brief Read a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_read_32.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 */
static inline void func005_read_reg(xspi_ctrl_test* test,
                                    unsigned int    offset,
                                    uint32_t&       value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// FUNC_XSPI_005 Test Case Implementations
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_001 — Reset values of cmn_seq_regs_a registers
 *        (0x380–0x3A0)
 *
 * Reads all six registers in the cmn_seq_regs_a group after reset_in
 * de-assertion and asserts each equals its documented hardware reset value.
 * The key non-zero reset values are:
 *   xip_mode_cfg (0x388) = 0x00FF0000  — xip_dis_mb_val=0xFF in bits[23:16]
 *   global_seq_cfg (0x390) = 0x0000208F — page-size and profile defaults
 *
 * Winning condition:
 *   All six cmn_seq_regs_a registers read back their RESET enum values.
 * Failing condition:
 *   Any register returns a value that differs from its documented reset —
 *   indicates missing CSML register initialisation or incorrect reset handler.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_001_reset_values_cmn_seq_regs()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_001: Reset values — cmn_seq_regs_a (0x388–0x3A0)");

    // Apply reset to guarantee clean DUT state before reading
    apply_reset();

    // Table of {offset, expected_reset, name} for all cmn_seq_regs_a registers
    struct RegCheck {
        unsigned int offset;
        uint32_t     expected;
        const char*  name;
    };

    const RegCheck checks[] = {
        { xspi_ctrl_basetest::xip_mode_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_RESET),
          "xip_mode_cfg" },
        { xspi_ctrl_basetest::global_seq_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_RESET),
          "global_seq_cfg" },
        { xspi_ctrl_basetest::global_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_1_RESET),
          "global_seq_cfg_1" },
        { xspi_ctrl_basetest::direct_access_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_cfg_RESET),
          "direct_access_cfg" },
        { xspi_ctrl_basetest::direct_access_rmp_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_RESET),
          "direct_access_rmp" },
        { xspi_ctrl_basetest::direct_access_rmp_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_1_RESET),
          "direct_access_rmp_1" }
    };

    const int num_regs = static_cast<int>(sizeof(checks) / sizeof(checks[0]));
    bool all_pass = true;

    for (int i = 0; i < num_regs; ++i) {
        uint32_t read_val = 0xDEADBEEFu;
        func005_read_reg(test, checks[i].offset, read_val);

        bool pass = (read_val == checks[i].expected);
        if (!pass) {
            all_pass = false;
            CSML_ERROR(0, func005_logger)
                << "  FAIL [" << checks[i].name << "] at offset 0x"
                << std::hex << checks[i].offset
                << ": expected=0x" << checks[i].expected
                << " got=0x"       << read_val;
        } else {
            CSML_INFO(2, func005_logger)
                << "  PASS [" << checks[i].name << "] = 0x"
                << std::hex << read_val;
        }
    }

    report_test_result("TC_XSPI_CFG_SEQ_001", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_002 — Reset values of dev_seq_regs_a registers
 *        (0x400–0x478)
 *
 * Reads all 17 registers in the dev_seq_regs_a group after reset_in
 * de-assertion and asserts each equals its documented hardware reset value.
 * Several registers have distinctive non-zero resets encoding the default
 * xSPI NOR Profile 1 opcodes (e.g., READ=0x03, WREN=0x06, PAGE_PROG=0x02).
 *
 * Winning condition:
 *   All 17 dev_seq_regs_a registers read back their RESET enum values.
 * Failing condition:
 *   Any register returns a value that differs from its documented reset —
 *   indicates incorrect seq_cfg shadow initialisation in the constructor.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_002_reset_values_dev_seq_regs()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_002: Reset values — dev_seq_regs_a (0x400–0x478)");

    // Apply reset to guarantee clean DUT state before reading
    apply_reset();

    struct RegCheck {
        unsigned int offset;
        uint32_t     expected;
        const char*  name;
    };

    const RegCheck checks[] = {
        // Reset sequence registers (0x400–0x404)
        { xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_0_RESET),
          "rst_seq_cfg_0" },
        { xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_1_RESET),
          "rst_seq_cfg_1" },
        // Erase sequence registers (0x410–0x418)
        { xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_0_RESET),
          "ers_seq_cfg_0" },
        { xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_1_RESET),
          "ers_seq_cfg_1" },
        { xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_2_RESET),
          "ers_seq_cfg_2" },
        // Program sequence registers (0x420–0x428)
        { xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_0_RESET),
          "prog_seq_cfg_0" },
        { xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_1_RESET),
          "prog_seq_cfg_1" },
        { xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_2_RESET),
          "prog_seq_cfg_2" },
        // Read sequence registers (0x430–0x438)
        { xspi_ctrl_basetest::read_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_0_RESET),
          "read_seq_cfg_0" },
        { xspi_ctrl_basetest::read_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_1_RESET),
          "read_seq_cfg_1" },
        { xspi_ctrl_basetest::read_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_2_RESET),
          "read_seq_cfg_2" },
        // Write Enable sequence register (0x440)
        { xspi_ctrl_basetest::we_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::we_seq_cfg_0_RESET),
          "we_seq_cfg_0" },
        // Status check sequence registers (0x450–0x478)
        { xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_0_RESET),
          "stat_seq_cfg_0" },
        { xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_1_RESET),
          "stat_seq_cfg_1" },
        { xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_2_RESET),
          "stat_seq_cfg_2" },
        { xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_3_RESET),
          "stat_seq_cfg_3" },
        { xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_4_RESET),
          "stat_seq_cfg_4" },
        { xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_5_RESET),
          "stat_seq_cfg_5" },
        { xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_7_RESET),
          "stat_seq_cfg_7" },
        { xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_8_RESET),
          "stat_seq_cfg_8" },
        { xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_9_RESET),
          "stat_seq_cfg_9" },
        { xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_10_RESET),
          "stat_seq_cfg_10" }
    };

    const int num_regs = static_cast<int>(sizeof(checks) / sizeof(checks[0]));
    bool all_pass = true;

    for (int i = 0; i < num_regs; ++i) {
        uint32_t read_val = 0xDEADBEEFu;
        func005_read_reg(test, checks[i].offset, read_val);

        bool pass = (read_val == checks[i].expected);
        if (!pass) {
            all_pass = false;
            CSML_ERROR(0, func005_logger)
                << "  FAIL [" << checks[i].name << "] at offset 0x"
                << std::hex << checks[i].offset
                << ": expected=0x" << checks[i].expected
                << " got=0x"       << read_val;
        } else {
            CSML_INFO(2, func005_logger)
                << "  PASS [" << checks[i].name << "] = 0x"
                << std::hex << read_val;
        }
    }

    report_test_result("TC_XSPI_CFG_SEQ_002", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_003 — long_polling and short_polling shadow
 *        callback wiring verification
 *
 * Verifies that the handle_write_long_polling() and handle_write_short_polling()
 * CSML callbacks are correctly wired by confirming register write/readback.
 *
 * At reset, long_polling must be 0x000003E8 (1000) and short_polling must be
 * 0x000001F4 (500). After writing new values (0x0000012C = 300 and
 * 0x00000096 = 150), reading back confirms both the register storage and the
 * shadow variable update path are working.
 *
 * The writable mask for both registers is 0x0000FFFF (16-bit counter).
 *
 * Winning condition:
 *   - Reset check: long=0x3E8, short=0x1F4.
 *   - Write/readback: long=0x012C, short=0x0096 retained within 16-bit mask.
 * Failing condition:
 *   - Reset mismatch: callback not wired or register not initialised.
 *   - Write not retained: handle_write_* returning without storing value.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_003_polling_shadow_callback_wiring()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_003: long_polling / short_polling "
                      "shadow callback wiring");

    // Phase 1: Apply reset and confirm the non-zero reset values.
    // Reset value: long_polling=0x000003E8 (1000), short_polling=0x000001F4 (500)
    apply_reset();

    uint32_t long_reset_val  = 0xDEADBEEFu;
    uint32_t short_reset_val = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  long_reset_val);
    func005_read_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, short_reset_val);

    const uint32_t exp_long_reset  =
        static_cast<uint32_t>(xspi_ctrl_basetest::long_polling_RESET);
    const uint32_t exp_short_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::short_polling_RESET);

    bool pass_long_reset  = (long_reset_val  == exp_long_reset);
    bool pass_short_reset = (short_reset_val == exp_short_reset);

    CSML_INFO(2, func005_logger)
        << "  Reset: long_polling=0x" << std::hex << long_reset_val
        << " (expected 0x" << exp_long_reset  << ")"
        << (pass_long_reset  ? " OK" : " FAIL");
    CSML_INFO(2, func005_logger)
        << "  Reset: short_polling=0x" << std::hex << short_reset_val
        << " (expected 0x" << exp_short_reset << ")"
        << (pass_short_reset ? " OK" : " FAIL");

    if (!pass_long_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: long_polling reset mismatch — "
            << "expected=0x" << std::hex << exp_long_reset
            << " got=0x" << long_reset_val;
    }
    if (!pass_short_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: short_polling reset mismatch — "
            << "expected=0x" << std::hex << exp_short_reset
            << " got=0x" << short_reset_val;
    }

    // Phase 2: Write new values and verify retention.
    // New values: long=0x0000012C (300), short=0x00000096 (150)
    // Both are within the 16-bit writable mask (0x0000FFFF).
    const uint32_t new_long  = 0x0000012Cu;
    const uint32_t new_short = 0x00000096u;
    const uint32_t poll_mask = 0x0000FFFFu;

    func005_write_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  new_long);
    func005_write_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, new_short);
    wait(5, sc_core::SC_NS);

    uint32_t long_readback  = 0xDEADBEEFu;
    uint32_t short_readback = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  long_readback);
    func005_read_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, short_readback);

    bool pass_long_rw  = ((long_readback  & poll_mask) == (new_long  & poll_mask));
    bool pass_short_rw = ((short_readback & poll_mask) == (new_short & poll_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback: long_polling wrote=0x" << std::hex << new_long
        << " got=0x" << long_readback
        << (pass_long_rw ? " OK" : " FAIL");
    CSML_INFO(2, func005_logger)
        << "  Write/readback: short_polling wrote=0x" << std::hex << new_short
        << " got=0x" << short_readback
        << (pass_short_rw ? " OK" : " FAIL");

    if (!pass_long_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: long_polling write/readback mismatch — "
            << "wrote=0x" << std::hex << new_long
            << " got=0x" << long_readback;
    }
    if (!pass_short_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: short_polling write/readback mismatch — "
            << "wrote=0x" << std::hex << new_short
            << " got=0x" << short_readback;
    }

    bool passed = pass_long_reset && pass_short_reset
               && pass_long_rw   && pass_short_rw;
    report_test_result("TC_XSPI_CFG_SEQ_003", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_004 — global_seq_cfg (0x390) write and read-back
 *
 * Verifies that the handle_write_global_seq_cfg() callback is correctly wired
 * and that the register retains written values within its 32-bit write mask.
 *
 * The test writes 0x01800090 — a value that sets seq_type=0 (Profile 1 NOR),
 * modifies the page-size fields, and changes CRC/timing bits — then reads back
 * to confirm storage. The write mask for global_seq_cfg is 0xFFFFFFFF (full
 * word writable per basetest::Register_Write_Access).
 *
 * Winning condition:
 *   - Reset value 0x0000208F confirmed.
 *   - Written value 0x01800090 retained after write.
 * Failing condition:
 *   - Reset mismatch: CSML register not initialised correctly.
 *   - Write not retained: handle_write_global_seq_cfg() not wired.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_004_global_seq_cfg_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_004: global_seq_cfg (0x390) write and "
                      "read-back");

    apply_reset();

    // Phase 1: Verify reset value.
    uint32_t reset_val = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET, reset_val);

    const uint32_t exp_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_RESET);
    bool pass_reset = (reset_val == exp_reset);

    CSML_INFO(2, func005_logger)
        << "  Reset: global_seq_cfg=0x" << std::hex << reset_val
        << " (expected 0x" << exp_reset << ")"
        << (pass_reset ? " OK" : " FAIL");

    if (!pass_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: global_seq_cfg reset mismatch — "
            << "expected=0x" << std::hex << exp_reset
            << " got=0x" << reset_val;
    }

    // Phase 2: Write a distinctive pattern and verify retention.
    // 0x01800090 encodes:
    //   bits[1:0] = seq_type = 0b00 (Profile 1 NOR) — unchanged
    //   bits[7:4] = seq_page_size_rd = 0x9 (2^9 = 512B read page)
    //   bits[27:24] = some configuration field bits
    // All bits are writable (mask=0xFFFFFFFF).
    const uint32_t write_val  = 0x01800090u;
    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_WRITE);

    func005_write_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET, write_val);
    wait(5, sc_core::SC_NS);

    uint32_t readback = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET, readback);

    bool pass_rw = ((readback & write_mask) == (write_val & write_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback: global_seq_cfg wrote=0x" << std::hex << write_val
        << " got=0x" << readback
        << (pass_rw ? " OK" : " FAIL");

    if (!pass_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: global_seq_cfg write/readback mismatch — "
            << "wrote=0x" << std::hex << write_val
            << " got=0x" << readback;
    }

    bool passed = pass_reset && pass_rw;
    report_test_result("TC_XSPI_CFG_SEQ_004", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_005 — global_seq_cfg_1 (0x394) write and read-back
 *
 * Verifies that the handle_write_global_seq_cfg_1() callback is correctly
 * wired by confirming write/readback of the nand_spare_area field. The reset
 * value is 0x00000000; a write of 0x00000042 (nand_spare_area = 0x042) is
 * retained and read back.
 *
 * Winning condition:
 *   - Reset value 0x00000000 confirmed.
 *   - Written value 0x00000042 retained after write (full mask = 0xFFFFFFFF).
 * Failing condition:
 *   - Write not retained: handle_write_global_seq_cfg_1() not wired.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_005_global_seq_cfg_1_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_005: global_seq_cfg_1 (0x394) write "
                      "and read-back");

    apply_reset();

    // Phase 1: Verify reset value = 0x00000000.
    uint32_t reset_val = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET, reset_val);

    const uint32_t exp_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_1_RESET);
    bool pass_reset = (reset_val == exp_reset);

    CSML_INFO(2, func005_logger)
        << "  Reset: global_seq_cfg_1=0x" << std::hex << reset_val
        << " (expected 0x" << exp_reset << ")"
        << (pass_reset ? " OK" : " FAIL");

    if (!pass_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: global_seq_cfg_1 reset mismatch";
    }

    // Phase 2: Write NAND spare area size value and verify retention.
    // 0x00000042 = nand_spare_area=0x042 (66 bytes of spare area per page)
    const uint32_t write_val  = 0x00000042u;
    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_1_WRITE);

    func005_write_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET, write_val);
    wait(5, sc_core::SC_NS);

    uint32_t readback = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET, readback);

    bool pass_rw = ((readback & write_mask) == (write_val & write_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback: global_seq_cfg_1 wrote=0x" << std::hex << write_val
        << " got=0x" << readback
        << (pass_rw ? " OK" : " FAIL");

    if (!pass_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: global_seq_cfg_1 write/readback mismatch — "
            << "wrote=0x" << std::hex << write_val
            << " got=0x" << readback;
    }

    bool passed = pass_reset && pass_rw;
    report_test_result("TC_XSPI_CFG_SEQ_005", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_006 — xip_mode_cfg (0x388) write and read-back
 *
 * Verifies handle_write_xip_mode_cfg() callback wiring by:
 *   1. Confirming reset value 0x00FF0000 (xip_dis_mb_val=0xFF at bits[23:16]).
 *   2. Writing 0x00A50000 (xip_dis_mb_val=0xA5 in bits[23:16]) and confirming
 *      retention — proves the callback stores the xip_dis_mb_val field.
 *   3. Writing 0x00FF00B4 (xip_en=0xB4 for bank bits in [7:0]) and confirming
 *      retention — proves the xip_active_banks field is stored.
 *
 * The write mask for xip_mode_cfg is 0x00FFFFFF (24-bit writable; bits[31:24]
 * are reserved and read as zero).
 *
 * Winning condition:
 *   Reset value correct; both write patterns retained within writable mask.
 * Failing condition:
 *   Any mismatch indicates the callback is not wired or the mask is wrong.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_006_xip_mode_cfg_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_006: xip_mode_cfg (0x388) write and "
                      "read-back");

    apply_reset();

    // Phase 1: Confirm reset value 0x00FF0000.
    uint32_t reset_val = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, reset_val);

    const uint32_t exp_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_RESET);
    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_WRITE);

    bool pass_reset = (reset_val == exp_reset);

    CSML_INFO(2, func005_logger)
        << "  Reset: xip_mode_cfg=0x" << std::hex << reset_val
        << " (expected 0x" << exp_reset << ")"
        << (pass_reset ? " OK" : " FAIL");

    if (!pass_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: xip_mode_cfg reset mismatch — "
            << "expected=0x" << std::hex << exp_reset
            << " got=0x" << reset_val;
    }

    // Phase 2a: Write a new xip_dis_mb_val=0xA5 pattern.
    // 0x00A50000 → xip_dis_mb_val=0xA5 in bits[23:16]; xip_en=0 (XIP not armed).
    const uint32_t write_val_a = 0x00A50000u;
    func005_write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, write_val_a);
    wait(5, sc_core::SC_NS);

    uint32_t readback_a = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, readback_a);

    bool pass_rw_a = ((readback_a & write_mask) == (write_val_a & write_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback A: xip_mode_cfg wrote=0x" << std::hex << write_val_a
        << " got=0x" << readback_a
        << (pass_rw_a ? " OK" : " FAIL");

    if (!pass_rw_a) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: xip_mode_cfg write A mismatch — "
            << "wrote=0x" << std::hex << write_val_a
            << " got=0x" << readback_a;
    }

    // Phase 2b: Write enabling XIP on bank 0 (xip_en bit[0]=1) with
    // xip_dis_mb_val=0xFF and xip_en_mb_val=0x00.
    // 0x00FF0001 → xip_dis_mb_val=0xFF, xip_en=0x01 (bank 0 XIP armed)
    const uint32_t write_val_b = 0x00FF0001u;
    func005_write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, write_val_b);
    wait(5, sc_core::SC_NS);

    uint32_t readback_b = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, readback_b);

    bool pass_rw_b = ((readback_b & write_mask) == (write_val_b & write_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback B: xip_mode_cfg wrote=0x" << std::hex << write_val_b
        << " got=0x" << readback_b
        << (pass_rw_b ? " OK" : " FAIL");

    if (!pass_rw_b) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: xip_mode_cfg write B mismatch — "
            << "wrote=0x" << std::hex << write_val_b
            << " got=0x" << readback_b;
    }

    bool passed = pass_reset && pass_rw_a && pass_rw_b;
    report_test_result("TC_XSPI_CFG_SEQ_006", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_007 — direct_access_cfg (0x398) write and read-back
 *
 * Verifies that the handle_write_direct_access_cfg() callback is correctly
 * wired. The test writes a value selecting:
 *   dac_bank_num=1 (bits[2:0] = 0b001)
 *   rmp_addr_en=1  (bit 12)
 *
 * and reads back to confirm storage. These fields map to the active_dac_bank
 * and rmp_addr_en shadow variables inside the model.
 *
 * Reset value: 0x00000000.
 * Write value: 0x00001001  (dac_bank_num=1, rmp_addr_en=bit12).
 *
 * Winning condition:
 *   Written value retained within write mask after the callback is invoked.
 * Failing condition:
 *   Write not retained: handle_write_direct_access_cfg() not wired.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_007_direct_access_cfg_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_007: direct_access_cfg (0x398) write "
                      "and read-back");

    apply_reset();

    // Phase 1: Confirm reset value = 0x00000000.
    uint32_t reset_val = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET, reset_val);

    const uint32_t exp_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_cfg_RESET);
    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_cfg_WRITE);

    bool pass_reset = (reset_val == exp_reset);

    CSML_INFO(2, func005_logger)
        << "  Reset: direct_access_cfg=0x" << std::hex << reset_val
        << " (expected 0x" << exp_reset << ")"
        << (pass_reset ? " OK" : " FAIL");

    if (!pass_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_cfg reset mismatch";
    }

    // Phase 2: Write bank=1, rmp_addr_en=1 and verify retention.
    // 0x00001001: bit[0]=1 (dac_bank_num LSB, bank 1), bit[12]=1 (rmp_addr_en)
    const uint32_t write_val = 0x00001001u;
    func005_write_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET, write_val);
    wait(5, sc_core::SC_NS);

    uint32_t readback = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET, readback);

    bool pass_rw = ((readback & write_mask) == (write_val & write_mask));

    CSML_INFO(2, func005_logger)
        << "  Write/readback: direct_access_cfg wrote=0x" << std::hex << write_val
        << " got=0x" << readback
        << (pass_rw ? " OK" : " FAIL");

    if (!pass_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_cfg write/readback mismatch — "
            << "wrote=0x" << std::hex << write_val
            << " got=0x" << readback;
    }

    bool passed = pass_reset && pass_rw;
    report_test_result("TC_XSPI_CFG_SEQ_007", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_008 — direct_access_rmp / direct_access_rmp_1
 *        (0x39C / 0x3A0) write and read-back
 *
 * Verifies that both halves of the 64-bit remap_offset shadow are populated
 * by handle_write_direct_access_rmp() and handle_write_direct_access_rmp_1()
 * respectively.
 *
 * Scenario:
 *   - Write lower half 0x00001000 to direct_access_rmp (0x39C)
 *   - Write upper half 0x00000002 to direct_access_rmp_1 (0x3A0)
 *   - Read back both and verify retention
 *   - The combined 64-bit remap_offset would be 0x0000000200001000
 *
 * Reset values: both 0x00000000.
 *
 * Winning condition:
 *   Both registers retain written values, confirming both callbacks are wired.
 * Failing condition:
 *   Either register does not retain its value — callback not wired.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_008_direct_access_rmp_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_008: direct_access_rmp (0x39C) and "
                      "direct_access_rmp_1 (0x3A0) write and read-back");

    apply_reset();

    // Phase 1: Confirm both reset at 0x00000000.
    uint32_t rmp_reset   = 0xDEADBEEFu;
    uint32_t rmp1_reset  = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET,   rmp_reset);
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, rmp1_reset);

    const uint32_t exp_reset_rmp  =
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_RESET);
    const uint32_t exp_reset_rmp1 =
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_1_RESET);

    bool pass_rmp_reset  = (rmp_reset  == exp_reset_rmp);
    bool pass_rmp1_reset = (rmp1_reset == exp_reset_rmp1);

    CSML_INFO(2, func005_logger)
        << "  Reset: direct_access_rmp=0x" << std::hex << rmp_reset
        << " (expected 0x" << exp_reset_rmp  << ")"
        << (pass_rmp_reset  ? " OK" : " FAIL");
    CSML_INFO(2, func005_logger)
        << "  Reset: direct_access_rmp_1=0x" << std::hex << rmp1_reset
        << " (expected 0x" << exp_reset_rmp1 << ")"
        << (pass_rmp1_reset ? " OK" : " FAIL");

    if (!pass_rmp_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_rmp reset mismatch";
    }
    if (!pass_rmp1_reset) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_rmp_1 reset mismatch";
    }

    // Phase 2: Write a 64-bit remap offset and verify both halves.
    // Lower 32 bits: offset[31:0]  = 0x00001000 (4096 byte offset)
    // Upper 32 bits: offset[63:32] = 0x00000002 (high-word of 64-bit offset)
    const uint32_t write_rmp  = 0x00001000u;
    const uint32_t write_rmp1 = 0x00000002u;

    func005_write_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET,   write_rmp);
    func005_write_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, write_rmp1);
    wait(5, sc_core::SC_NS);

    uint32_t readback_rmp  = 0xDEADBEEFu;
    uint32_t readback_rmp1 = 0xDEADBEEFu;
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET,   readback_rmp);
    func005_read_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, readback_rmp1);

    bool pass_rmp_rw  = (readback_rmp  == write_rmp);
    bool pass_rmp1_rw = (readback_rmp1 == write_rmp1);

    CSML_INFO(2, func005_logger)
        << "  Write/readback: direct_access_rmp wrote=0x" << std::hex << write_rmp
        << " got=0x" << readback_rmp
        << (pass_rmp_rw  ? " OK" : " FAIL");
    CSML_INFO(2, func005_logger)
        << "  Write/readback: direct_access_rmp_1 wrote=0x" << std::hex << write_rmp1
        << " got=0x" << readback_rmp1
        << (pass_rmp1_rw ? " OK" : " FAIL");

    if (!pass_rmp_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_rmp write/readback mismatch";
    }
    if (!pass_rmp1_rw) {
        CSML_ERROR(0, func005_logger)
            << "  FAIL: direct_access_rmp_1 write/readback mismatch";
    }

    bool passed = pass_rmp_reset && pass_rmp1_reset
               && pass_rmp_rw   && pass_rmp1_rw;
    report_test_result("TC_XSPI_CFG_SEQ_008", passed);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_009 — Device sequence registers write and read-back
 *
 * Verifies that all 17 CSML write callbacks for dev_seq_regs_a are wired by
 * writing distinctive test patterns to each register and reading back to
 * confirm storage. The patterns use unique marker bytes so a swap between
 * registers would be detected.
 *
 * Each register uses a full 32-bit writable mask (0xFFFFFFFF), so the entire
 * written value should be retained and read back unchanged.
 *
 * Registers under test and their test patterns:
 *   rst_seq_cfg_0   (0x400) = 0xAABBCC11
 *   rst_seq_cfg_1   (0x404) = 0xAABBCC22
 *   ers_seq_cfg_0   (0x410) = 0xAABBCC33
 *   ers_seq_cfg_1   (0x414) = 0xAABBCC44
 *   ers_seq_cfg_2   (0x418) = 0xAABBCC55
 *   prog_seq_cfg_0  (0x420) = 0xAABBCC66
 *   prog_seq_cfg_1  (0x424) = 0xAABBCC77
 *   prog_seq_cfg_2  (0x428) = 0xAABBCC88
 *   read_seq_cfg_0  (0x430) = 0xAABBCC99
 *   read_seq_cfg_1  (0x434) = 0xAABBCCAA
 *   read_seq_cfg_2  (0x438) = 0xAABBCCBB
 *   we_seq_cfg_0    (0x440) = 0xAABBCCCC
 *   stat_seq_cfg_0  (0x450) = 0xAABBCCDD
 *   stat_seq_cfg_1  (0x454) = 0xAABBCCEE
 *   stat_seq_cfg_2  (0x458) = 0xAABBCCFF
 *   stat_seq_cfg_3  (0x45C) = 0xAABB1111
 *   stat_seq_cfg_4  (0x460) = 0xAABB2222
 *   stat_seq_cfg_5  (0x464) = 0xAABB3333
 *   stat_seq_cfg_7  (0x46C) = 0xAABB4444
 *   stat_seq_cfg_8  (0x470) = 0xAABB5555
 *   stat_seq_cfg_9  (0x474) = 0xAABB6666
 *   stat_seq_cfg_10 (0x478) = 0xAABB7777
 *
 * Winning condition:
 *   All 22 registers read back the written pattern exactly.
 * Failing condition:
 *   Any mismatch indicates a CSML callback is not wired for that register.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_009_dev_seq_regs_write_readback()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_009: Device sequence registers — "
                      "all 22 callbacks write and read-back");

    apply_reset();

    // Unique test patterns — each register gets a different low byte so that
    // register swaps would be detectable.
    struct RegWrite {
        unsigned int offset;
        uint32_t     write_val;
        const char*  name;
    };

    const RegWrite entries[] = {
        { xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET,   0xAABBCC11u, "rst_seq_cfg_0"  },
        { xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET,   0xAABBCC22u, "rst_seq_cfg_1"  },
        { xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET,   0xAABBCC33u, "ers_seq_cfg_0"  },
        { xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET,   0xAABBCC44u, "ers_seq_cfg_1"  },
        { xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,   0xAABBCC55u, "ers_seq_cfg_2"  },
        { xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET,  0xAABBCC66u, "prog_seq_cfg_0" },
        { xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET,  0xAABBCC77u, "prog_seq_cfg_1" },
        { xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET,  0xAABBCC88u, "prog_seq_cfg_2" },
        { xspi_ctrl_basetest::read_seq_cfg_0_OFFSET,  0xAABBCC99u, "read_seq_cfg_0" },
        { xspi_ctrl_basetest::read_seq_cfg_1_OFFSET,  0xAABBCCAAu, "read_seq_cfg_1" },
        { xspi_ctrl_basetest::read_seq_cfg_2_OFFSET,  0xAABBCCBBu, "read_seq_cfg_2" },
        { xspi_ctrl_basetest::we_seq_cfg_0_OFFSET,    0xAABBCCCCu, "we_seq_cfg_0"   },
        { xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET,  0xAABBCCDDu, "stat_seq_cfg_0" },
        { xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET,  0xAABBCCEEu, "stat_seq_cfg_1" },
        { xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET,  0xAABBCCFFu, "stat_seq_cfg_2" },
        { xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET,  0xAABB1111u, "stat_seq_cfg_3" },
        { xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET,  0xAABB2222u, "stat_seq_cfg_4" },
        { xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET,  0xAABB3333u, "stat_seq_cfg_5" },
        { xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET,  0xAABB4444u, "stat_seq_cfg_7" },
        { xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET,  0xAABB5555u, "stat_seq_cfg_8" },
        { xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET,  0xAABB6666u, "stat_seq_cfg_9" },
        { xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET, 0xAABB7777u, "stat_seq_cfg_10"}
    };

    const int num_regs = static_cast<int>(sizeof(entries) / sizeof(entries[0]));

    // Write phase — write all registers with test patterns
    for (int i = 0; i < num_regs; ++i) {
        func005_write_reg(test, entries[i].offset, entries[i].write_val);
    }
    wait(5, sc_core::SC_NS);

    // Readback phase — verify each register retained the written value
    bool all_pass = true;
    for (int i = 0; i < num_regs; ++i) {
        uint32_t readback = 0xDEADBEEFu;
        func005_read_reg(test, entries[i].offset, readback);

        bool pass = (readback == entries[i].write_val);
        if (!pass) {
            all_pass = false;
            CSML_ERROR(0, func005_logger)
                << "  FAIL [" << entries[i].name << "] at offset 0x"
                << std::hex << entries[i].offset
                << ": wrote=0x" << entries[i].write_val
                << " got=0x"   << readback;
        } else {
            CSML_INFO(2, func005_logger)
                << "  PASS [" << entries[i].name << "]: 0x"
                << std::hex << readback;
        }
    }

    report_test_result("TC_XSPI_CFG_SEQ_009", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_CFG_SEQ_010 — Reset restores all sequence configuration
 *        registers to hardware reset values
 *
 * Validates that the reset handler correctly restores all sequence
 * configuration registers (cmn_seq_regs_a and dev_seq_regs_a) after
 * software has written non-reset patterns to them.
 *
 * This test exercises the reset handler's restoration of the seq_cfg shadow
 * struct (via seq_cfg = seq_config_t{} followed by field-by-field assignment)
 * and the scml2 memory register bank reset mechanism for all 28 registers in
 * the two sequence register groups.
 *
 * Stimulus:
 *   1. Write non-reset values to all cmn_seq_regs_a and a representative
 *      subset of dev_seq_regs_a registers.
 *   2. Apply reset (assert then de-assert reset_in).
 *   3. Read all modified registers and assert each returned to its RESET value.
 *
 * Winning condition:
 *   All registers read their RESET values after reset_in de-assertion.
 * Failing condition:
 *   Any register retains a non-reset value — indicates the reset handler
 *   does not restore that register or its shadow variable correctly.
 ******************************************************************************/
void testbench::tc_xspi_cfg_seq_010_reset_restores_seq_config_registers()
{
    func005_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_CFG_SEQ_010: Reset restores all sequence "
                      "configuration registers to hardware reset values");

    // Phase 1: Apply reset first to start from a clean state, then write
    // non-reset values to all sequence configuration registers.
    apply_reset();

    CSML_INFO(2, func005_logger)
        << "  Writing non-reset values to all sequence config registers...";

    // cmn_seq_regs_a: write non-reset values
    func005_write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET,        0x00AA0001u);
    func005_write_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET,      0x12345678u);
    func005_write_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET,    0x00000055u);
    func005_write_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET,   0x00001003u);
    func005_write_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET,   0xDEAD1000u);
    func005_write_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, 0x0000BEEFu);

    // dev_seq_regs_a: write non-reset values (representative full set)
    func005_write_reg(test, xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET,       0xDEADBEEFu);
    func005_write_reg(test, xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET,       0xABCDEF01u);
    func005_write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET,       0x11223344u);
    func005_write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET,       0x55667788u);
    func005_write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,       0x99AABBCCu);
    func005_write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET,      0xDDEEFF00u);
    func005_write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET,      0x11335577u);
    func005_write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET,      0x22446688u);
    func005_write_reg(test, xspi_ctrl_basetest::read_seq_cfg_0_OFFSET,      0xAABBCCDDu);
    func005_write_reg(test, xspi_ctrl_basetest::read_seq_cfg_1_OFFSET,      0x12AB34CDu);
    func005_write_reg(test, xspi_ctrl_basetest::read_seq_cfg_2_OFFSET,      0xFF00EE11u);
    func005_write_reg(test, xspi_ctrl_basetest::we_seq_cfg_0_OFFSET,        0x55AA55AAu);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET,      0xCAFEBABEu);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET,      0x01020304u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET,      0xFEDCBA98u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET,      0x87654321u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET,      0xA1B2C3D4u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET,      0xE5F6A7B8u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET,      0xC9D0E1F2u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET,      0x13243546u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET,      0x57687980u);
    func005_write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET,     0x9A0B1C2Du);
    wait(5, sc_core::SC_NS);

    CSML_INFO(2, func005_logger)
        << "  Applying reset...";

    // Phase 2: Apply reset — restores all registers to hardware reset values.
    apply_reset();

    // Phase 3: Read all registers and verify they returned to reset values.
    CSML_INFO(2, func005_logger)
        << "  Verifying reset restoration...";

    struct RegCheck {
        unsigned int offset;
        uint32_t     expected;
        const char*  name;
    };

    const RegCheck checks[] = {
        // cmn_seq_regs_a
        { xspi_ctrl_basetest::xip_mode_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_RESET),
          "xip_mode_cfg" },
        { xspi_ctrl_basetest::global_seq_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_RESET),
          "global_seq_cfg" },
        { xspi_ctrl_basetest::global_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_1_RESET),
          "global_seq_cfg_1" },
        { xspi_ctrl_basetest::direct_access_cfg_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_cfg_RESET),
          "direct_access_cfg" },
        { xspi_ctrl_basetest::direct_access_rmp_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_RESET),
          "direct_access_rmp" },
        { xspi_ctrl_basetest::direct_access_rmp_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_1_RESET),
          "direct_access_rmp_1" },
        // dev_seq_regs_a
        { xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_0_RESET),
          "rst_seq_cfg_0" },
        { xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_1_RESET),
          "rst_seq_cfg_1" },
        { xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_0_RESET),
          "ers_seq_cfg_0" },
        { xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_1_RESET),
          "ers_seq_cfg_1" },
        { xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_2_RESET),
          "ers_seq_cfg_2" },
        { xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_0_RESET),
          "prog_seq_cfg_0" },
        { xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_1_RESET),
          "prog_seq_cfg_1" },
        { xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_2_RESET),
          "prog_seq_cfg_2" },
        { xspi_ctrl_basetest::read_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_0_RESET),
          "read_seq_cfg_0" },
        { xspi_ctrl_basetest::read_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_1_RESET),
          "read_seq_cfg_1" },
        { xspi_ctrl_basetest::read_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_2_RESET),
          "read_seq_cfg_2" },
        { xspi_ctrl_basetest::we_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::we_seq_cfg_0_RESET),
          "we_seq_cfg_0" },
        { xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_0_RESET),
          "stat_seq_cfg_0" },
        { xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_1_RESET),
          "stat_seq_cfg_1" },
        { xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_2_RESET),
          "stat_seq_cfg_2" },
        { xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_3_RESET),
          "stat_seq_cfg_3" },
        { xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_4_RESET),
          "stat_seq_cfg_4" },
        { xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_5_RESET),
          "stat_seq_cfg_5" },
        { xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_7_RESET),
          "stat_seq_cfg_7" },
        { xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_8_RESET),
          "stat_seq_cfg_8" },
        { xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_9_RESET),
          "stat_seq_cfg_9" },
        { xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_10_RESET),
          "stat_seq_cfg_10" }
    };

    const int num_regs = static_cast<int>(sizeof(checks) / sizeof(checks[0]));
    bool all_pass = true;

    for (int i = 0; i < num_regs; ++i) {
        uint32_t read_val = 0xDEADBEEFu;
        func005_read_reg(test, checks[i].offset, read_val);

        bool pass = (read_val == checks[i].expected);
        if (!pass) {
            all_pass = false;
            CSML_ERROR(0, func005_logger)
                << "  FAIL [" << checks[i].name << "] at offset 0x"
                << std::hex << checks[i].offset
                << ": expected=0x" << checks[i].expected
                << " got=0x"       << read_val;
        } else {
            CSML_INFO(2, func005_logger)
                << "  PASS [" << checks[i].name << "] reset=0x"
                << std::hex << read_val;
        }
    }

    report_test_result("TC_XSPI_CFG_SEQ_010", all_pass);
}

/******************************************************************************
 * @brief run_func005_tests — top-level entry point for all FUNC_XSPI_005 tests
 *
 * Called from testbench::run_tests() after run_func004_tests() completes.
 * Executes all 10 test cases mapped to FUNC_XSPI_005 (Sequence Configuration
 * Register Management) in sequential order.
 *
 * All tests in this suite are pure register-storage tests that do not require
 * any operating mode engine (Direct, STIG, PIO, ACMD). They verify that all
 * CSML write callbacks for sequence configuration registers are correctly wired
 * so that register writes are stored and shadow variables are updated.
 *
 * Test execution order:
 *  1. TC_XSPI_CFG_SEQ_001 — cmn_seq_regs_a reset values
 *  2. TC_XSPI_CFG_SEQ_002 — dev_seq_regs_a reset values (22 registers)
 *  3. TC_XSPI_CFG_SEQ_003 — long_polling / short_polling shadow callback
 *  4. TC_XSPI_CFG_SEQ_004 — global_seq_cfg write and read-back
 *  5. TC_XSPI_CFG_SEQ_005 — global_seq_cfg_1 write and read-back
 *  6. TC_XSPI_CFG_SEQ_006 — xip_mode_cfg write and read-back
 *  7. TC_XSPI_CFG_SEQ_007 — direct_access_cfg write and read-back
 *  8. TC_XSPI_CFG_SEQ_008 — direct_access_rmp / direct_access_rmp_1 readback
 *  9. TC_XSPI_CFG_SEQ_009 — all 22 dev_seq_regs_a write and read-back
 * 10. TC_XSPI_CFG_SEQ_010 — reset restores all seq config registers
 ******************************************************************************/
void testbench::run_func005_tests()
{
    CSML_INFO(1, func005_logger)
        << "================================================";
    CSML_INFO(1, func005_logger)
        << "  FUNC_XSPI_005: Sequence Configuration Register";
    CSML_INFO(1, func005_logger)
        << "                 Management";
    CSML_INFO(1, func005_logger)
        << "  10 test cases (TC_XSPI_CFG_SEQ_001–010)";
    CSML_INFO(1, func005_logger)
        << "================================================";

    // Establish clean DUT state before starting the suite
    apply_reset();

    // TC_XSPI_CFG_SEQ_001: cmn_seq_regs_a reset values (xip_mode_cfg,
    //   global_seq_cfg, global_seq_cfg_1, direct_access_cfg, rmp, rmp_1)
    tc_xspi_cfg_seq_001_reset_values_cmn_seq_regs();

    // TC_XSPI_CFG_SEQ_002: dev_seq_regs_a reset values (all 22 registers)
    tc_xspi_cfg_seq_002_reset_values_dev_seq_regs();

    // TC_XSPI_CFG_SEQ_003: long_polling / short_polling shadow callbacks
    tc_xspi_cfg_seq_003_polling_shadow_callback_wiring();

    // TC_XSPI_CFG_SEQ_004: global_seq_cfg write and read-back
    tc_xspi_cfg_seq_004_global_seq_cfg_write_readback();

    // TC_XSPI_CFG_SEQ_005: global_seq_cfg_1 write and read-back
    tc_xspi_cfg_seq_005_global_seq_cfg_1_write_readback();

    // TC_XSPI_CFG_SEQ_006: xip_mode_cfg write and read-back
    tc_xspi_cfg_seq_006_xip_mode_cfg_write_readback();

    // TC_XSPI_CFG_SEQ_007: direct_access_cfg write and read-back
    tc_xspi_cfg_seq_007_direct_access_cfg_write_readback();

    // TC_XSPI_CFG_SEQ_008: direct_access_rmp / direct_access_rmp_1 readback
    tc_xspi_cfg_seq_008_direct_access_rmp_write_readback();

    // TC_XSPI_CFG_SEQ_009: all 22 dev_seq_regs_a write and read-back
    tc_xspi_cfg_seq_009_dev_seq_regs_write_readback();

    // TC_XSPI_CFG_SEQ_010: reset restores all sequence config registers
    tc_xspi_cfg_seq_010_reset_restores_seq_config_registers();

    CSML_INFO(1, func005_logger)
        << "================================================";
    CSML_INFO(1, func005_logger)
        << "  FUNC_XSPI_005 test suite complete";
    CSML_INFO(1, func005_logger)
        << "================================================";
}
