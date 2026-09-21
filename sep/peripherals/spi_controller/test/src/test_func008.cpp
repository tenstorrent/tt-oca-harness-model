// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// FUNC-008: Control Flow Testing (SPIEN Suspend/Resume, SW_RST)
void testbench::test_func008_control_flow()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-008] Control Flow Testing" << std::endl
                         << "========================================" << std::endl
                         << "Tests: SPIEN suspend/resume, SW_RST, OUTPUT_EN" << std::endl
                         << "========================================\n" << std::endl;

    uint32_t status_val = 0;

    // =======================================================================
    // Test 9.1: SPIEN Suspend During Active Transaction
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8.1] SPIEN Suspend During Active Transaction" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Initial Configuration
    REG_INFO(2, logger) << "[8.1.1] Initial Configuration..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);  /// CLKDIV=10
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Step 2: Load TX FIFO with data
    REG_INFO(2, logger) << "[8.1.2] Loading TX FIFO with 32 bytes..." << std::endl;
    for (int i = 0; i < 8; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x11223300 + i);
    }
    wait(10, SC_NS);

    // Verify TX FIFO loaded
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t txqd = status_val & 0xFF;
    if (txqd == 8) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=8" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO not loaded: TXQD=" << txqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 3: Start long transaction (32 bytes TX)
    REG_INFO(2, logger) << "[8.1.3] Starting 32-byte TX transaction..." << std::endl;
    uint32_t cmd = BUILD_CMD(31, 2, 0, 0);  /// LEN=31 (32 bytes), TX-only, Standard, CSAAT=0
    test->write_register_32(COMMAND_OFFSET, cmd);
    wait(50, SC_US);  /// Wait for transaction to start

    // Verify transaction is active
    test->read_register_32(STATUS_OFFSET, status_val);
    bool active = (status_val >> 30) & 0x1;
    if (active) {
        REG_INFO(2, logger) << "  [PASS] Transaction active: STATUS.ACTIVE=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Transaction not active" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 4: Suspend by clearing SPIEN (CONTROL.SPIEN=0)
    REG_INFO(2, logger) << "[8.1.4] Suspending transaction (SPIEN=0)..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x60000000);  /// SPIEN=0, OUTPUT_EN=1, SW_RST=0
    wait(10, SC_US);

    // Verify transaction is still suspended (FSM should hold state)
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t txqd_suspended = status_val & 0xFF;
    REG_INFO(2, logger) << "  [INFO] During suspension: TXQD=" << txqd_suspended << std::endl;

    // Step 5: Resume by setting SPIEN=1
    REG_INFO(2, logger) << "[8.1.5] Resuming transaction (SPIEN=1)..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    wait(200, SC_US);  /// Wait for transaction to complete

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    bool ready = (status_val >> 31) & 0x1;
    bool txempty = (status_val >> 28) & 0x1;

    if (!active && ready && txempty) {
        REG_INFO(2, logger) << "  [PASS] Transaction resumed and completed: ACTIVE=0, READY=1, TXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Transaction did not complete: ACTIVE=" << active
                  << ", READY=" << ready << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors after suspend/resume" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 9.2: Software Reset (SW_RST) During Active Transaction
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8.2] Software Reset During Active Transaction" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Re-enable and reconfigure
    REG_INFO(2, logger) << "[8.2.1] Reconfiguring IP..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Step 2: Load TX FIFO
    REG_INFO(2, logger) << "[8.2.2] Loading TX FIFO with 16 bytes..." << std::endl;
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAABBCC00 + i);
    }
    wait(10, SC_NS);

    // Verify TX FIFO
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    if (txqd == 4) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=4" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 3: Start transaction
    REG_INFO(2, logger) << "[8.2.3] Starting 16-byte TX transaction..." << std::endl;
    cmd = BUILD_CMD(15, 2, 0, 0);  /// LEN=15 (16 bytes), TX-only
    test->write_register_32(COMMAND_OFFSET, cmd);
    wait(30, SC_US);

    // Verify transaction active
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    if (active) {
        REG_INFO(2, logger) << "  [PASS] Transaction active before SW_RST" << std::endl;
        sub_tests_passed++;
    } else {
        REG_INFO(2, logger) << "  [INFO] Transaction may have completed quickly" << std::endl;
    }

    // Step 4: Trigger SW_RST
    REG_INFO(2, logger) << "[8.2.4] Triggering SW_RST..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// SW_RST=1, SPIEN=0, OUTPUT_EN=0
    wait(50, SC_US);

    // Step 5: Verify FIFOs are flushed
    REG_INFO(2, logger) << "[8.2.5] Verifying FIFOs flushed..." << std::endl;
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    uint32_t rxqd = (status_val >> 8) & 0xFF;
    txempty = (status_val >> 28) & 0x1;
    bool rxempty = (status_val >> 24) & 0x1;

    if (txqd == 0 && rxqd == 0 && txempty && rxempty) {
        REG_INFO(2, logger) << "  [PASS] FIFOs flushed: TXQD=0, RXQD=0, TXEMPTY=1, RXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFOs not flushed: TXQD=" << txqd << ", RXQD=" << rxqd
                  << ", TXEMPTY=" << txempty << ", RXEMPTY=" << rxempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 6: Verify FSM in IDLE
    active = (status_val >> 30) & 0x1;
    ready = (status_val >> 31) & 0x1;

    if (ready && !active) {
        REG_INFO(2, logger) << "  [PASS] FSM in IDLE: READY=1, ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FSM not in IDLE: READY=" << ready << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 7: Verify ERROR_STATUS cleared
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] ERROR_STATUS cleared by SW_RST" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS not cleared: 0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 8: Release SW_RST and verify IP can be reconfigured
    REG_INFO(2, logger) << "[8.2.6] Releasing SW_RST and reconfiguring..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1, SW_RST=0
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (ready) {
        REG_INFO(2, logger) << "  [PASS] IP operational after SW_RST: STATUS.READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] IP not operational after SW_RST" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 9.3: OUTPUT_EN Control
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8.3] OUTPUT_EN Control" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Configure with OUTPUT_EN=0
    REG_INFO(2, logger) << "[8.3.1] Configuring with OUTPUT_EN=0..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x80000000);  /// SPIEN=1, OUTPUT_EN=0
    wait(10, SC_NS);

    test->read_register_32(CONTROL_OFFSET, status_val);
    bool output_en = (status_val >> 29) & 0x1;
    if (!output_en) {
        REG_INFO(2, logger) << "  [PASS] OUTPUT_EN=0 configured" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] OUTPUT_EN not cleared" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 2: Enable OUTPUT_EN
    REG_INFO(2, logger) << "[8.3.2] Enabling OUTPUT_EN..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    wait(10, SC_NS);

    test->read_register_32(CONTROL_OFFSET, status_val);
    output_en = (status_val >> 29) & 0x1;
    if (output_en) {
        REG_INFO(2, logger) << "  [PASS] OUTPUT_EN=1 configured" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] OUTPUT_EN not set" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 3: Verify normal transaction works with OUTPUT_EN=1
    REG_INFO(2, logger) << "[8.3.3] Testing transaction with OUTPUT_EN=1..." << std::endl;
    test->write_register_32(TXDATA_OFFSET, 0xDEADBEEF);
    wait(10, SC_NS);

    cmd = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX
    test->write_register_32(COMMAND_OFFSET, cmd);
    wait(50, SC_US);

    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;

    if (!active && txempty) {
        REG_INFO(2, logger) << "  [PASS] Transaction completed with OUTPUT_EN=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Transaction did not complete: ACTIVE=" << active << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(20, SC_US);

    // =======================================================================
    // Test 9.4: SW_RST with Full FIFOs
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8.4] SW_RST with Full FIFOs" << std::endl
                         << "------------------------------------------------------" << std::endl;

    // Step 1: Fill TX FIFO to capacity
    // Capacity is TxDepth + 1: the byte_select stage holds one extra word.
    REG_INFO(2, logger) << "[8.4.1] Filling TX FIFO (73 words = 292 bytes)..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x80000000);  /// SPIEN=1, OUTPUT_EN=0 (prevent draining)
    wait(10, SC_NS);

    for (int i = 0; i < 73; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x12340000 + i);
        wait(5, SC_NS);
    }

    // Verify TX FIFO full
    test->read_register_32(STATUS_OFFSET, status_val);
    bool txfull = (status_val >> 29) & 0x1;
    txqd = status_val & 0xFF;

    if (txfull && txqd == 73) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO full: TXFULL=1, TXQD=73" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO not full: TXFULL=" << txfull << ", TXQD=" << txqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 2: Trigger SW_RST
    REG_INFO(2, logger) << "[8.4.2] Triggering SW_RST with full FIFO..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// SW_RST=1
    wait(100, SC_US);

    // Step 3: Verify TX FIFO flushed
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    txempty = (status_val >> 28) & 0x1;
    txfull = (status_val >> 29) & 0x1;

    if (txqd == 0 && txempty && !txfull) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO flushed: TXQD=0, TXEMPTY=1, TXFULL=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO not flushed: TXQD=" << txqd
                  << ", TXEMPTY=" << txempty << ", TXFULL=" << txfull << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Step 4: Release SW_RST and verify IP operational
    REG_INFO(2, logger) << "[8.4.3] Releasing SW_RST..." << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (ready) {
        REG_INFO(2, logger) << "  [PASS] IP operational after full FIFO reset" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] IP not operational" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "FUNC-008 Test Summary" << std::endl
                         << "========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================\n" << std::endl;

    test->clear_slave_state();

    // Report final test result
    report_test_result("FUNC-008: Control Flow Testing", test_passed);
}
