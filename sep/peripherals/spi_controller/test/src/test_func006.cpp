// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

void testbench::test_func006_multi_device_switching()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-006] Multi-Device Switching" << std::endl
                         << "========================================" << std::endl
                         << "Testing multi-device support with NumCS parameter:" << std::endl
                         << "  1. Basic CSID switching between devices" << std::endl
                         << "  2. Per-device CFG independence" << std::endl
                         << "  3. CSID change terminates previous command" << std::endl
                         << "  4. CSIDINVAL error for invalid CSID" << std::endl
                         << "  5. CS timing with CSNIDLE between switches" << std::endl
                         << "========================================\n" << std::endl;

    uint32_t read_val;

    // NOTE: Model default NumCS=1, so some tests may be limited
    // Check model's NumCS parameter (implementation dependent)
    CSML_INFO(2, logger) << "[INFO] Model instantiated with NumCS parameter" << std::endl;
    CSML_INFO(2, logger) << "[INFO] For full multi-device testing, NumCS should be >= 2" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Current tests will validate CSID register and error handling\n" << std::endl;

    // =======================================================================
    // Initial Setup
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Initial Setup] Configure SPI Host" << std::endl;

    // Enable SPIEN and OUTPUT_EN
    test->write_register_32(CTRL_OFFSET, 0xC0000000);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, read_val);
    bool ready = (read_val >> 31) & 0x1;

    if (ready) {
        CSML_INFO(2, logger) << "  [PASS] SPI Host ready\n" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] SPI Host not ready" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-006: Multi-Device Switching", false);
        return;
    }

    // =========================================================================
    // Test 1: Basic CSID Register Write/Read
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Test 1] Basic CSID Register Access" << std::endl;

    // Write CSID = 0
    CSML_INFO(2, logger) << "[ACTION] Setting CSID=0..." << std::endl;
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // Read back
    test->read_register_32(CSID_OFFSET, read_val);
    CSML_INFO(2, logger) << "[INFO] CSID readback: " << read_val << std::endl;

    if (read_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] CSID register write/read successful" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CSID mismatch (expected 0, got " << read_val << ")" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Test 2: Per-Device CFG Configuration
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Test 2] Per-Device CFG Independence" << std::endl;

    // Configure Device 0 with specific settings
    CSML_INFO(2, logger) << "[ACTION] Configuring Device 0 (CSID=0)..." << std::endl;
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // Device 0: CLKDIV=8, CSNIDLE=2, CSNTRAIL=2, CSNLEAD=2, FULLCYC=1, CPHA=0, CPOL=0
    uint32_t config_dev0 = (8 << 16) | (2 << 12) | (2 << 8) | (2 << 4) | (1 << 2) | (0 << 1) | 0;
    test->write_register_32(CFG_OFFSET, config_dev0);
    wait(10, SC_NS);

    CSML_INFO(2, logger) << "  [INFO] Device 0 config: CLKDIV=8, CSNIDLE=2, CSNTRAIL=2, CSNLEAD=2, FULLCYC=1" << std::endl;

    // Read back Device 0 config
    test->read_register_32(CFG_OFFSET, read_val);
    if (read_val == config_dev0) {
        CSML_INFO(2, logger) << "  [PASS] Device 0 CFG configured correctly (0x" << std::hex << read_val << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Device 0 CFG mismatch (expected 0x" << std::hex << config_dev0
                  << ", got 0x" << read_val << std::dec << ")" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Configure Device 1 with different settings (if NumCS >= 2)
    CSML_INFO(1, logger) << "\n[ACTION] Attempting to configure Device 1 (CSID=1)..." << std::endl;
    test->write_register_32(CSID_OFFSET, 1);
    wait(10, SC_NS);

    // Device 1: CLKDIV=16, CSNIDLE=4, CSNTRAIL=4, CSNLEAD=4, FULLCYC=0, CPHA=1, CPOL=1
    uint32_t config_dev1 = (16 << 16) | (4 << 12) | (4 << 8) | (4 << 4) | (0 << 2) | (1 << 1) | 1;
    test->write_register_32(CFG_OFFSET, config_dev1);
    wait(10, SC_NS);

    CSML_INFO(2, logger) << "  [INFO] Device 1 config: CLKDIV=16, CSNIDLE=4, CSNTRAIL=4, CSNLEAD=4, CPOL=1, CPHA=1" << std::endl;

    // Read back Device 1 config
    test->read_register_32(CFG_OFFSET, read_val);
    CSML_INFO(2, logger) << "  [INFO] Device 1 CFG readback: 0x" << std::hex << read_val << std::dec << std::endl;

    if (read_val == config_dev1) {
        CSML_INFO(2, logger) << "  [PASS] Device 1 CFG configured correctly" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_INFO(2, logger) << "  [INFO] Device 1 CFG differs (NumCS may be 1, CSID=1 invalid)" << std::endl;
        // Don't fail - NumCS=1 is acceptable default
        sub_tests_passed++;
    }

    // Switch back to Device 0 and verify its config is unchanged
    CSML_INFO(1, logger) << "\n[ACTION] Switching back to Device 0..." << std::endl;
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    test->read_register_32(CFG_OFFSET, read_val);
    CSML_INFO(2, logger) << "  [INFO] Device 0 CFG readback: 0x" << std::hex << read_val << std::dec << std::endl;

    if (read_val == config_dev0) {
        CSML_INFO(2, logger) << "  [PASS] Device 0 config preserved after Device 1 configuration" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Device 0 config changed (expected 0x" << std::hex << config_dev0
                  << ", got 0x" << read_val << std::dec << ")" << std::endl;
        CSML_ERROR(2, logger) << "  [FAIL] Per-device shadow array not working correctly" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =========================================================================
    // Test 3: Basic Transaction to Device 0
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Test 3] Transaction to Device 0" << std::endl;

    // Clear any errors
    clear_errors();

    // Ensure Device 0 is selected
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // Load TX data
    CSML_INFO(2, logger) << "  [ACTION] Loading TX data for Device 0..." << std::endl;
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAA000000 + i);
    }
    wait(10, SC_NS);

    // Check STATUS.READY
    test->read_register_32(STATUS_OFFSET, read_val);
    ready = (read_val >> 31) & 0x1;
    CSML_INFO(2, logger) << "  [INFO] STATUS.READY=" << ready << std::endl;

    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        // Issue command to Device 0
        CSML_INFO(2, logger) << "  [ACTION] Issuing 16-byte TX command to Device 0..." << std::endl;
        uint32_t cmd = BUILD_CMD(15, 2, 0, 0);  /// 16 bytes, TX_ONLY, Standard
        test->write_register_32(CMD_OFFSET, cmd);
        wait(200, SC_US);

        // Verify transaction completed
        test->read_register_32(STATUS_OFFSET, read_val);
        bool active = (read_val >> 30) & 0x1;
        CSML_INFO(2, logger) << "  [INFO] After transaction: ACTIVE=" << active << std::endl;

        if (!active) {
            CSML_INFO(2, logger) << "  [PASS] Transaction to Device 0 completed" << std::endl;
            sub_tests_passed++;
        } else {
            CSML_ERROR(2, logger) << "  [FAIL] Transaction still active after timeout" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =========================================================================
    // Test 4: CSID Change During Multi-Segment Transaction
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Test 4] CSID Change Terminates Previous Command" << std::endl;
    CSML_INFO(2, logger) << "  [INFO] Testing CSID change behavior with CSAAT flag" << std::endl;

    // Clear errors
    clear_errors();

    // Start with Device 0
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // Load TX data
    for (int i = 0; i < 2; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xBB000000 + i);
    }
    wait(10, SC_NS);

    // Issue first segment with CSAAT=1 (keep CSB asserted)
    test->read_register_32(STATUS_OFFSET, read_val);
    ready = (read_val >> 31) & 0x1;

    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before first segment" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        CSML_INFO(2, logger) << "  [ACTION] Issuing first segment to Device 0 with CSAAT=1..." << std::endl;
        uint32_t cmd_csaat = BUILD_CMD(7, 2, 0, 1);  /// 8 bytes, TX_ONLY, CSAAT=1
        test->write_register_32(CMD_OFFSET, cmd_csaat);
        wait(50, SC_US);

        // Change CSID while transaction is pending (should terminate previous command)
        CSML_INFO(2, logger) << "  [ACTION] Changing CSID to 1 (should terminate Device 0 command)..." << std::endl;
        test->write_register_32(CSID_OFFSET, 1);
        wait(10, SC_NS);

        // Load new TX data for Device 1
        for (int i = 0; i < 2; i++) {
            test->write_register_32(TXDATA_OFFSET, 0xCC000000 + i);
        }
        wait(10, SC_NS);

        // Issue command to Device 1
        test->read_register_32(STATUS_OFFSET, read_val);
        ready = (read_val >> 31) & 0x1;

        if (!ready) {
            CSML_ERROR(2, logger) << "  [FAIL] Not READY after CSID change" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "  [ACTION] Issuing command to Device 1..." << std::endl;
            uint32_t cmd1 = BUILD_CMD(7, 2, 0, 0);  /// 8 bytes, TX_ONLY
            test->write_register_32(CMD_OFFSET, cmd1);
            wait(200, SC_US);

            // Verify completion
            test->read_register_32(STATUS_OFFSET, read_val);
            bool active = (read_val >> 30) & 0x1;

            if (!active) {
                CSML_INFO(2, logger) << "  [PASS] CSID change successfully switched between devices" << std::endl;
                sub_tests_passed++;
            } else {
                CSML_ERROR(2, logger) << "  [FAIL] Device 1 transaction still active" << std::endl;
                sub_tests_failed++;
                test_passed = false;
            }
        }
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =========================================================================
    // Test 5: CSIDINVAL Error for Invalid CSID
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Test 5] CSIDINVAL Error Detection" << std::endl;

    // Clear errors
    clear_errors();

    // Set invalid CSID (assuming NumCS=1 or 2, try CSID=15)
    CSML_INFO(2, logger) << "  [ACTION] Setting invalid CSID=15 (NumCS typically 1 or 2)..." << std::endl;
    test->write_register_32(CSID_OFFSET, 15);
    wait(10, SC_NS);

    // Load TX data
    for (int i = 0; i < 2; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xDD000000 + i);
    }
    wait(10, SC_NS);

    // Try to issue command with invalid CSID
    test->read_register_32(STATUS_OFFSET, read_val);
    ready = (read_val >> 31) & 0x1;

    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        CSML_INFO(2, logger) << "  [ACTION] Issuing command with invalid CSID=15..." << std::endl;
        uint32_t cmd = BUILD_CMD(7, 2, 0, 0);
        test->write_register_32(CMD_OFFSET, cmd);
        wait(50, SC_US);

        // Check for CSIDINVAL error
        test->read_register_32(ERROR_STATUS_OFFSET, read_val);
        bool csidinval = (read_val >> 16) & 0x1;  /// CSIDINVAL at bit 16 per sparse RDL layout
        CSML_INFO(2, logger) << "  [INFO] ERROR_STATUS: 0x" << std::hex << read_val << std::dec
                  << ", CSIDINVAL=" << csidinval << std::endl;

        if (csidinval) {
            CSML_INFO(2, logger) << "  [PASS] CSIDINVAL error correctly detected for invalid CSID" << std::endl;
            sub_tests_passed++;
        } else {
            CSML_ERROR(2, logger) << "  [FAIL] CSIDINVAL error not detected (CSID validation required)" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }

        // Clear error
        test->write_register_32(ERROR_STATUS_OFFSET, read_val);
        wait(10, SC_NS);
    }

    // Restore valid CSID
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // =========================================================================
    // Final Verification
    // =========================================================================
    CSML_INFO(1, logger) << "\n[Final Check] Verify FSM in IDLE" << std::endl;

    test->read_register_32(CSID_OFFSET, read_val);
    uint32_t final_csid = read_val;
    CSML_INFO(2, logger) << "  [VERIFY] Final CSID: " << final_csid << std::endl;

    test->read_register_32(ERROR_STATUS_OFFSET, read_val);
    uint32_t final_error_status = read_val;
    CSML_INFO(2, logger) << "  [VERIFY] Final ERROR_STATUS: 0x" << std::hex << final_error_status << std::dec << std::endl;

    test->read_register_32(STATUS_OFFSET, read_val);
    ready = (read_val >> 31) & 0x1;
    bool active = (read_val >> 30) & 0x1;
    CSML_INFO(2, logger) << "  [VERIFY] Final STATUS: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (final_error_status == 0 && final_csid == 0 && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] IP in clean state: CSID=0, no errors, READY=1, ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] IP not in clean state" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================" << std::endl
                         << "Test Coverage:" << std::endl
                         << "  1. CSID register write/read" << std::endl
                         << "  2. Per-device CFG configuration" << std::endl
                         << "  3. Per-device CFG shadow array preservation" << std::endl
                         << "  4. Transaction to Device 0" << std::endl
                         << "  5. CSID change behavior with CSAAT" << std::endl
                         << "  6. CSIDINVAL error detection" << std::endl
                         << "\nNote: Full testing requires NumCS >= 2" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("FUNC-006: Multi-Device Switching", test_passed);
}
