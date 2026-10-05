// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// =============================================================================
/// Comprehensive Reset Test - Combines RST-001 through RST-004
/// =============================================================================
/// This single test covers all critical reset functionality with 4 sub-tests:
///   1. Hardware reset and register defaults
///   2. Software reset with FIFO flush
///   3. Post-reset reconfiguration
///   4. Error clearing via software reset
/// =============================================================================
void testbench::test_func000_comprehensive_reset()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST RESET] Comprehensive Reset Test" << std::endl
                         << "========================================" << std::endl;

    uint32_t read_val = 0;

    // =========================================================================
    // Sub-Test 1: Hardware Reset Verification
    // =========================================================================
    REG_INFO(1, logger) << "\n[Sub-Test 1] Hardware Reset - Register Defaults" << std::endl;

    // Write non-default values
    test->write_register_32(CONTROL_OFFSET, 0x80000000);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);  /// ERROR at bit 0, SPI_EVENT at bit 1
    test->write_register_32(CONFIGOPTS_OFFSET, 0x12345678);
    wait(10, SC_NS);

    // Apply hardware reset
    test->rst_ni.write(false);
    wait(100, SC_NS);
    test->rst_ni.write(true);
    wait(100, SC_NS);

    // Verify key registers reset to defaults
    test->read_register_32(CONTROL_OFFSET, read_val);
    if (read_val == 0x7F) {
        REG_INFO(2, logger) << "  [PASS] CONTROL reset to 0x7F" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CONTROL=0x" << std::hex << read_val << ", expected 0x7F" << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify STATUS shows IDLE
    test->read_register_32(STATUS_OFFSET, read_val);
    bool ready = (read_val >> 31) & 0x1;
    bool active = (read_val >> 30) & 0x1;
    bool txempty = (read_val >> 28) & 0x1;
    bool rxempty = (read_val >> 24) & 0x1;

    if (ready && !active && txempty && rxempty) {
        REG_INFO(2, logger) << "  [PASS] FSM in IDLE: READY=1, ACTIVE=0, FIFOs empty" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FSM not IDLE after reset" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Sub-Test 2: Software Reset with FIFO Flush
    // =========================================================================
    REG_INFO(1, logger) << "\n[Sub-Test 2] Software Reset - FIFO Flush" << std::endl;

    // Enable IP and fill TX FIFO
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    test->write_register_32(TXDATA_OFFSET, 0x11223344);
    test->write_register_32(TXDATA_OFFSET, 0x55667788);
    wait(10, SC_NS);

    // Verify FIFO has data
    test->read_register_32(STATUS_OFFSET, read_val);
    uint32_t txqd_before = read_val & 0xFF;

    // Trigger SW_RST
    test->write_register_32(CONTROL_OFFSET, 0x40000000);
    wait(100, SC_NS);

    // Verify FIFOs flushed
    test->read_register_32(STATUS_OFFSET, read_val);
    uint32_t txqd_after = read_val & 0xFF;
    txempty = (read_val >> 28) & 0x1;
    rxempty = (read_val >> 24) & 0x1;

    if (txqd_after == 0 && txempty && rxempty) {
        REG_INFO(2, logger) << "  [PASS] FIFOs flushed: TXQD " << txqd_before << "→0, both empty" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFOs not flushed: TXQD=" << txqd_after << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify FSM returned to IDLE
    ready = (read_val >> 31) & 0x1;
    active = (read_val >> 30) & 0x1;

    if (ready && !active) {
        REG_INFO(2, logger) << "  [PASS] FSM returned to IDLE" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FSM not IDLE: READY=" << ready << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Sub-Test 3: Post-Reset Reconfiguration
    // =========================================================================
    REG_INFO(1, logger) << "\n[Sub-Test 3] Post-Reset Reconfiguration" << std::endl;

    // Configure CONTROL with watermarks
    // Note: Bit 28 is reserved and not writable, so use 0xA0001020 instead of 0xE0001020
    test->write_register_32(CONTROL_OFFSET, 0xA0001020);
    wait(10, SC_NS);
    test->read_register_32(CONTROL_OFFSET, read_val);

    if (read_val == 0xA0001020) {
        REG_INFO(2, logger) << "  [PASS] CONTROL reconfigured successfully" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CONTROL=0x" << std::hex << read_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Configure CONFIGOPTS with timing
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0221000A);
    wait(10, SC_NS);
    test->read_register_32(CONFIGOPTS_OFFSET, read_val);

    if (read_val == 0x0221000A) {
        REG_INFO(2, logger) << "  [PASS] CONFIGOPTS configured (CLKDIV=10)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CONFIGOPTS=0x" << std::hex << read_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Sub-Test 4: Error Clearing via Software Reset
    // =========================================================================
    REG_INFO(1, logger) << "\n[Sub-Test 4] Error Clearing via SW_RST" << std::endl;

    // Force a definite error (invalid SPEED=3 -> CMDINVAL), then clear via SW_RST.
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x1);
    wait(10, SC_NS);
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(0, 0, 3, 0));  /// SPEED=3 invalid
    wait(50, SC_NS);

    test->read_register_32(ERROR_STATUS_OFFSET, read_val);
    bool cmdinval = ((read_val >> 3) & 0x1) != 0;
    if (cmdinval) {
        REG_INFO(2, logger) << "  [PASS] Error triggered: ERROR_STATUS=0x" << std::hex << read_val << std::dec << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Expected CMDINVAL before SW_RST clear, ERROR_STATUS=0x"
                  << std::hex << read_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Apply SW_RST to clear errors, then release so later tests can run.
    test->write_register_32(CONTROL_OFFSET, 0x40000000);
    wait(100, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0x00000000);

    // Verify ERROR_STATUS cleared
    test->read_register_32(ERROR_STATUS_OFFSET, read_val);
    if (read_val == 0x0) {
        REG_INFO(2, logger) << "  [PASS] ERROR_STATUS cleared after SW_RST" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << read_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify interrupts deasserted
    if (!sig_error_irq.read() && !sig_spi_event_irq.read()) {
        REG_INFO(2, logger) << "  [PASS] Interrupts deasserted" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Interrupts still asserted" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Test Summary
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================" << std::endl
                         << "Coverage:" << std::endl
                         << "  1. Hardware reset & register defaults" << std::endl
                         << "  2. Software reset & FIFO flush" << std::endl
                         << "  3. Post-reset reconfiguration" << std::endl
                         << "  4. Error clearing via SW_RST" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("RESET: Comprehensive Reset Test", test_passed);
}
