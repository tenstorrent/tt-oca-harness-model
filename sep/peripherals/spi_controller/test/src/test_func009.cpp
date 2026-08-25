// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// FUNC-009: Command Queue Depth (CMDQD) Testing
void testbench::test_func009_command_queue_depth()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-009] Command Queue Depth Testing" << std::endl
                         << "========================================" << std::endl
                         << "Tests: CMDQD monitoring, 4-segment depth, overflow" << std::endl
                         << "========================================\n" << std::endl;

    uint32_t status_val = 0;

    // =======================================================================
    // Test 10.1: CMDQD Monitoring - Single Segment
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Test 9.1] CMDQD Monitoring - Single Segment" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Initial Configuration
    CSML_INFO(2, logger) << "[9.1.1] Initial Configuration..." << std::endl;
    test->write_register_32(CTRL_OFFSET, 0xE0000000);  /// SPIEN=1, OUTPUT_EN=1
    test->write_register_32(CFG_OFFSET, 0x0000000A);  /// CLKDIV=10
    test->write_register_32(CSID_OFFSET, 0x0);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x11);      /// ERROR at bit 0, SPI_EVENT at bit 4
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x11111);  /// Sparse: bits 0,4,8,12,16
    wait(10, SC_NS);

    // Step 2: Verify CMDQD=0 initially
    CSML_INFO(2, logger) << "[9.1.2] Verifying initial CMDQD=0..." << std::endl;
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t cmdqd = (status_val >> 16) & 0xF;
    bool ready = (status_val >> 31) & 0x1;

    if (cmdqd == 0 && ready) {
        CSML_INFO(2, logger) << "  [PASS] Initial state: CMDQD=0, READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Initial state: CMDQD=" << cmdqd << ", READY=" << ready << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 3: Load TX data
    test->write_register_32(TXDATA_OFFSET, 0x11223344);
    wait(10, SC_NS);

    // Step 4: Submit one command segment
    CSML_INFO(2, logger) << "[9.1.3] Submitting 1 command segment..." << std::endl;
    uint32_t cmd = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX, Standard, CSAAT=0
    test->write_register_32(CMD_OFFSET, cmd);
    wait(5, SC_NS);

    // Step 5: Check CMDQD immediately after submission (should be 1 or 0 if processed quickly)
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    bool active = (status_val >> 30) & 0x1;

    CSML_INFO(2, logger) << "  [INFO] After submission: CMDQD=" << cmdqd << ", ACTIVE=" << active << std::endl;

    // Wait for segment to complete
    wait(50, SC_US);

    // Step 6: Verify CMDQD=0 after completion
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    active = (status_val >> 30) & 0x1;
    ready = (status_val >> 31) & 0x1;

    if (cmdqd == 0 && !active && ready) {
        CSML_INFO(2, logger) << "  [PASS] After completion: CMDQD=0, ACTIVE=0, READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] After completion: CMDQD=" << cmdqd << ", ACTIVE=" << active << ", READY=" << ready << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors in single segment test" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 10.2: CMDQD with Multi-Segment Transaction (4 segments)
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Test 9.2] CMDQD with 4-Segment Transaction" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Load TX data for all segments
    CSML_INFO(2, logger) << "[9.2.1] Loading TX FIFO for 4 segments..." << std::endl;
    for (int i = 0; i < 8; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAA000000 + i);
    }
    wait(10, SC_NS);

    // Step 2: Submit all 4 command segments rapidly (CSAAT=1 for first 3)
    CSML_INFO(2, logger) << "[9.2.2] Submitting 4 command segments..." << std::endl;

    // Segment 1: 4 bytes TX, CSAAT=1
    uint32_t cmd1 = BUILD_CMD(3, 2, 0, 1);
    test->write_register_32(CMD_OFFSET, cmd1);
    wait(2, SC_NS);

    // Check CMDQD after 1st segment
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 1: CMDQD=" << cmdqd << std::endl;

    // Segment 2: 4 bytes TX, CSAAT=1
    uint32_t cmd2 = BUILD_CMD(3, 2, 0, 1);
    test->write_register_32(CMD_OFFSET, cmd2);
    wait(2, SC_NS);

    // Check CMDQD after 2nd segment
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 2: CMDQD=" << cmdqd << std::endl;

    // Segment 3: 4 bytes TX, CSAAT=1
    uint32_t cmd3 = BUILD_CMD(3, 2, 0, 1);
    test->write_register_32(CMD_OFFSET, cmd3);
    wait(2, SC_NS);

    // Check CMDQD after 3rd segment
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 3: CMDQD=" << cmdqd << std::endl;

    // Segment 4: 4 bytes TX, CSAAT=0 (final segment)
    uint32_t cmd4 = BUILD_CMD(3, 2, 0, 0);
    test->write_register_32(CMD_OFFSET, cmd4);
    wait(2, SC_NS);

    // Check CMDQD after 4th segment
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    ready = (status_val >> 31) & 0x1;
    CSML_INFO(2, logger) << "  [INFO] After segment 4: CMDQD=" << cmdqd << ", READY=" << ready << std::endl;

    // The command FIFO is 4 deep, so after submitting 4 segments:
    // - CMDQD should be 4 OR segments may have started processing
    // - READY should be 0 if FIFO is full
    if (cmdqd <= 4) {
        CSML_INFO(2, logger) << "  [PASS] CMDQD within valid range: " << cmdqd << " <= 4" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CMDQD exceeds FIFO depth: CMDQD=" << cmdqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 3: Wait for all segments to complete
    CSML_INFO(2, logger) << "[9.2.3] Waiting for all segments to complete..." << std::endl;
    wait(300, SC_US);

    // Step 4: Verify CMDQD=0 after all segments complete
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    active = (status_val >> 30) & 0x1;
    ready = (status_val >> 31) & 0x1;

    if (cmdqd == 0 && !active && ready) {
        CSML_INFO(2, logger) << "  [PASS] All segments completed: CMDQD=0, ACTIVE=0, READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Not all segments completed: CMDQD=" << cmdqd
                  << ", ACTIVE=" << active << ", READY=" << ready << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors in 4-segment test" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 10.3: Command FIFO Overflow (5th Segment) - CMDBUSY Error
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Test 9.3] Command FIFO Overflow - 5th Segment" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Clear any previous errors
    CSML_INFO(2, logger) << "[9.3.1] Clearing previous errors..." << std::endl;
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, status_val);  /// W1C
        wait(10, SC_NS);
    }

    // Step 2: Load TX data
    CSML_INFO(2, logger) << "[9.3.2] Loading TX FIFO for overflow test..." << std::endl;
    for (int i = 0; i < 10; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xBB000000 + i);
    }
    wait(10, SC_NS);

    // Step 3: Rapidly submit 4 segments to fill command FIFO
    CSML_INFO(2, logger) << "[9.3.3] Filling command FIFO with 4 segments..." << std::endl;
    for (int i = 0; i < 4; i++) {
        uint32_t cmd_seg = BUILD_CMD(3, 2, 0, 1);  /// 4 bytes TX, CSAAT=1
        test->write_register_32(CMD_OFFSET, cmd_seg);
        wait(1, SC_NS);  /// Minimal delay to queue rapidly
    }

    // Check CMDQD and READY
    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    ready = (status_val >> 31) & 0x1;
    CSML_INFO(2, logger) << "  [INFO] After 4 segments: CMDQD=" << cmdqd << ", READY=" << ready << std::endl;

    // Step 4: Attempt to write 5th segment (should trigger CMDBUSY error)
    CSML_INFO(2, logger) << "[9.3.4] Attempting 5th segment (should trigger CMDBUSY)..." << std::endl;
    uint32_t cmd5 = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX, CSAAT=0
    test->write_register_32(CMD_OFFSET, cmd5);
    wait(10, SC_US);

    // Step 5: Check for CMDBUSY error
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    bool cmdbusy = status_val & 0x1;

    if (cmdbusy) {
        CSML_INFO(2, logger) << "  [PASS] CMDBUSY error detected on 5th segment" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CMDBUSY error not detected: ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 6: Verify error interrupt asserted
    bool error_irq = sig_error_irq.read();
    if (error_irq) {
        CSML_INFO(2, logger) << "  [PASS] error_irq asserted for CMDBUSY" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] error_irq not asserted" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 7: Clear CMDBUSY error
    CSML_INFO(2, logger) << "[9.3.5] Clearing CMDBUSY error..." << std::endl;
    test->write_register_32(ERROR_STATUS_OFFSET, 0x1);  /// W1C for CMDBUSY
    wait(10, SC_NS);

    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] CMDBUSY error cleared" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS not cleared: 0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 8: Wait for queued segments to complete
    CSML_INFO(2, logger) << "[9.3.6] Waiting for queued segments to drain..." << std::endl;
    wait(300, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    cmdqd = (status_val >> 16) & 0xF;
    ready = (status_val >> 31) & 0x1;

    if (cmdqd == 0 && ready) {
        CSML_INFO(2, logger) << "  [PASS] Command queue drained: CMDQD=0, READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue not drained: CMDQD=" << cmdqd << ", READY=" << ready << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 10.4: CMDQD Decrement as Segments Complete
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Test 9.4] CMDQD Decrement During Execution" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Clear errors
    clear_errors();

    // Step 2: Load TX data
    CSML_INFO(2, logger) << "[9.4.1] Loading TX FIFO..." << std::endl;
    for (int i = 0; i < 6; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xCC000000 + i);
    }
    wait(10, SC_NS);

    // Step 3: Submit 3 segments with short delays
    CSML_INFO(2, logger) << "[9.4.2] Submitting 3 segments with monitoring..." << std::endl;

    // Segment 1
    uint32_t cmd_a = BUILD_CMD(3, 2, 0, 1);
    test->write_register_32(CMD_OFFSET, cmd_a);
    wait(5, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t cmdqd_1 = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 1: CMDQD=" << cmdqd_1 << std::endl;

    // Segment 2
    uint32_t cmd_b = BUILD_CMD(3, 2, 0, 1);
    test->write_register_32(CMD_OFFSET, cmd_b);
    wait(5, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t cmdqd_2 = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 2: CMDQD=" << cmdqd_2 << std::endl;

    // Segment 3 (final)
    uint32_t cmd_c = BUILD_CMD(3, 2, 0, 0);
    test->write_register_32(CMD_OFFSET, cmd_c);
    wait(5, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t cmdqd_3 = (status_val >> 16) & 0xF;
    CSML_INFO(2, logger) << "  [INFO] After segment 3: CMDQD=" << cmdqd_3 << std::endl;

    // Wait for all to complete
    wait(200, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t cmdqd_final = (status_val >> 16) & 0xF;
    ready = (status_val >> 31) & 0x1;

    if (cmdqd_final == 0 && ready) {
        CSML_INFO(2, logger) << "  [PASS] CMDQD decremented to 0 after completion" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CMDQD not 0: CMDQD=" << cmdqd_final << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors in decrement test" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "FUNC-009 Test Summary" << std::endl
                         << "========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================\n" << std::endl;

    test->clear_slave_state();

    // Report final test result
    report_test_result("FUNC-009: Command Queue Depth Testing", test_passed);
}
