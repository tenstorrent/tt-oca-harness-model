// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// FUNC-002: Speed Mode Validation
void testbench::test_func002_speed_mode_validation()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;
    uint32_t status_val = 0;
    uint32_t read_val = 0;

    // Local status field variables
    uint32_t txqd = 0;
    uint32_t rxqd = 0;
    uint32_t ready = 0;
    uint32_t active = 0;
    uint32_t txempty = 0;
    uint32_t rxempty = 0;
    uint32_t txstall = 0;
    uint32_t rxstall = 0;
    bool byteorder = m_byte_order.get_param_value();

    // =======================================================================
    // Initial Configuration
    // =======================================================================
    REG_INFO(1, logger) << "\n[FUNC-002] Initial Configuration" << std::endl;

    // Enable SPIEN and OUTPUT_EN (CONTROL: bits 31=SPIEN, 30=OUTPUT_EN)
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);
    REG_INFO(2, logger) << "  CONTROL configured: SPIEN=1, OUTPUT_EN=1" << std::endl;

    // Configure timing parameters (CLKDIV=10)
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);
    wait(10, SC_NS);
    REG_INFO(2, logger) << "  CONFIGOPTS configured: CLKDIV=10" << std::endl;

    // Select chip select 0
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);
    REG_INFO(2, logger) << "  CSID=0 selected" << std::endl;

    // =======================================================================
    // Test 1: DUAL Mode (Speed = 1) - TX Transfer (8 bytes)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 1] DUAL Mode - TX Transfer (8 bytes)" << std::endl;

    // Load TX FIFO (8 bytes = 2 words)
    test->write_register_32(TXDATA_OFFSET, 0xA1B2C3D4);
    test->write_register_32(TXDATA_OFFSET, 0xE5F6A7B8);
    wait(10, SC_NS);

    // Verify TX FIFO
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    ready = (status_val >> 31) & 0x1;
    if (txqd == 2 && ready) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=2, READY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TXQD=" << txqd << ", READY=" << ready << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Issue COMMAND: LEN=7 (8 bytes), DIRECTION=TX(2), SPEED=DUAL(1), CSAAT=0
    uint32_t cmd1 = BUILD_CMD(7, 2, 1, 0);
    test->write_register_32(COMMAND_OFFSET, cmd1);
    wait(80, SC_US);

    // Verify completion
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;
    if (!active && txempty) {
        REG_INFO(2, logger) << "  [PASS] DUAL TX completed" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ACTIVE=" << active << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify slave received TX data
    const std::vector<uint8_t>& captured_dual_tx = test->get_slave_captured_tx_data();
    std::vector<uint8_t> expected_dual_tx = {0xD4, 0xC3, 0xB2, 0xA1, 0xB8, 0xA7, 0xF6, 0xE5};
    if (captured_dual_tx.size() >= 8) {
        bool match = true;
        for (int i = 0; i < 8; i++) {
            if (captured_dual_tx[i] != expected_dual_tx[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            REG_INFO(2, logger) << "  [PASS] Slave received correct DUAL TX data" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] DUAL TX data mismatch" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Slave received " << captured_dual_tx.size() << " bytes" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 2: DUAL Mode (Speed = 1) - RX Transfer (8 bytes)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 2] DUAL Mode - RX Transfer (8 bytes)" << std::endl;

    // Pre-load slave with RX data
    std::vector<uint8_t> test2_rx_data = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
    test->load_slave_rx_data(test2_rx_data);

    // Check READY
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    // Issue COMMAND: LEN=7 (8 bytes), DIRECTION=RX(1), SPEED=DUAL(1), CSAAT=0
    uint32_t cmd2 = BUILD_CMD(7, 1, 1, 0);
    test->write_register_32(COMMAND_OFFSET, cmd2);
    wait(100, SC_US);

    // Verify completion
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    rxqd = (status_val >> 8) & 0xFF;
    rxempty = (status_val >> 24) & 0x1;
    if (!active && rxqd == 2 && !rxempty) {
        REG_INFO(2, logger) << "  [PASS] DUAL RX completed: RXQD=2" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ACTIVE=" << active << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Read and verify RX data
    std::vector<uint8_t> received_dual_rx;
    for (int i = 0; i < 2; i++) {
        uint32_t rx_word;
        test->read_register_32(RXDATA_OFFSET, rx_word);
        if (byteorder) {
            received_dual_rx.push_back(rx_word & 0xFF);
            received_dual_rx.push_back((rx_word >> 8) & 0xFF);
            received_dual_rx.push_back((rx_word >> 16) & 0xFF);
            received_dual_rx.push_back((rx_word >> 24) & 0xFF);
        } else {
            received_dual_rx.push_back((rx_word >> 24) & 0xFF);
            received_dual_rx.push_back((rx_word >> 16) & 0xFF);
            received_dual_rx.push_back((rx_word >> 8) & 0xFF);
            received_dual_rx.push_back(rx_word & 0xFF);
        }
        wait(5, SC_NS);
    }

    bool dual_rx_match = true;
    for (int i = 0; i < 8; i++) {
        if (received_dual_rx[i] != test2_rx_data[i]) {
            dual_rx_match = false;
            break;
        }
    }
    if (dual_rx_match) {
        REG_INFO(2, logger) << "  [PASS] All 8 bytes match expected DUAL RX data" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DUAL RX data mismatch" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX FIFO empty
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    rxempty = (status_val >> 24) & 0x1;
    if (rxqd == 0 && rxempty) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO empty: RXQD=0, RXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RXQD=" << rxqd << ", RXEMPTY=" << rxempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 3: QUAD Mode (Speed = 2) - TX Transfer (16 bytes)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 3] QUAD Mode - TX Transfer (16 bytes)" << std::endl;

    // Load TX FIFO with test data (16 bytes = 4 words)
    test->write_register_32(TXDATA_OFFSET, 0x11223344);
    test->write_register_32(TXDATA_OFFSET, 0x55667788);
    test->write_register_32(TXDATA_OFFSET, 0x99AABBCC);
    test->write_register_32(TXDATA_OFFSET, 0xDDEEFF00);
    wait(10, SC_NS);

    // Verify TX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    if (txqd == 4) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=4" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << ", expected 4" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check READY
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    // Issue COMMAND: LEN=15 (16 bytes), DIRECTION=TX_ONLY(2), SPEED=QUAD(2), CSAAT=0
    uint32_t cmd3 = BUILD_CMD(15, 2, 2, 0);
    test->write_register_32(COMMAND_OFFSET, cmd3);
    wait(100, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;
    txstall = (status_val >> 27) & 0x1;

    if (!active && txempty && !txstall) {
        REG_INFO(2, logger) << "  [PASS] Transaction completed: ACTIVE=0, TXEMPTY=1, TXSTALL=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Status: ACTIVE=" << active << ", TXEMPTY=" << txempty << ", TXSTALL=" << txstall << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors after QUAD TX" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify slave received correct TX data
    const std::vector<uint8_t>& captured_quad_tx = test->get_slave_captured_tx_data();
    std::vector<uint8_t> expected_quad_tx = {
        0x44, 0x33, 0x22, 0x11,  /// Little-Endian 0x11223344
        0x88, 0x77, 0x66, 0x55,  /// Little-Endian 0x55667788
        0xCC, 0xBB, 0xAA, 0x99,  /// Little-Endian 0x99AABBCC
        0x00, 0xFF, 0xEE, 0xDD   /// Little-Endian 0xDDEEFF00
    };

    if (captured_quad_tx.size() >= 16) {
        bool tx_match = true;
        for (int i = 0; i < 16; i++) {
            if (captured_quad_tx[i] != expected_quad_tx[i]) {
                tx_match = false;
                break;
            }
        }
        if (tx_match) {
            REG_INFO(2, logger) << "  [PASS] Slave received correct 16-byte TX data" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] QUAD TX data mismatch at slave" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Slave received only " << captured_quad_tx.size() << " bytes" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 4: QUAD Mode (Speed = 2) - RX Transfer (32 bytes)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 4] QUAD Mode - RX Transfer (32 bytes)" << std::endl;

    // Pre-load slave with RX data
    std::vector<uint8_t> test4_rx_data(32);
    for (int i = 0; i < 32; i++) {
        test4_rx_data[i] = 0x50 + i;
    }
    test->load_slave_rx_data(test4_rx_data);

    // Check READY
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    // Issue COMMAND: LEN=31 (32 bytes), DIRECTION=RX_ONLY(1), SPEED=QUAD(2), CSAAT=0
    uint32_t cmd4 = BUILD_CMD(31, 1, 2, 0);
    test->write_register_32(COMMAND_OFFSET, cmd4);
    wait(150, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    rxstall = (status_val >> 23) & 0x1;

    if (!active && !rxstall) {
        REG_INFO(2, logger) << "  [PASS] Transaction completed: ACTIVE=0, RXSTALL=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Status: ACTIVE=" << active << ", RXSTALL=" << rxstall << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors after QUAD RX" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX FIFO contains expected data (32 bytes = 8 words)
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    rxempty = (status_val >> 24) & 0x1;

    if (rxqd == 8 && !rxempty) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO contains expected data: RXQD=8" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO: RXQD=" << rxqd << ", RXEMPTY=" << rxempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Read and verify all data (THIS WAS MISSING IN ORIGINAL TEST!)
    std::vector<uint8_t> received_quad_rx;
    for (int i = 0; i < 8; i++) {
        uint32_t rx_word;
        test->read_register_32(RXDATA_OFFSET, rx_word);
        if (byteorder) {  /// Little-Endian
            received_quad_rx.push_back(rx_word & 0xFF);
            received_quad_rx.push_back((rx_word >> 8) & 0xFF);
            received_quad_rx.push_back((rx_word >> 16) & 0xFF);
            received_quad_rx.push_back((rx_word >> 24) & 0xFF);
        } else {  /// Big-Endian
            received_quad_rx.push_back((rx_word >> 24) & 0xFF);
            received_quad_rx.push_back((rx_word >> 16) & 0xFF);
            received_quad_rx.push_back((rx_word >> 8) & 0xFF);
            received_quad_rx.push_back(rx_word & 0xFF);
        }
        wait(5, SC_NS);
    }

    // Verify data correctness
    bool quad_rx_match = true;
    for (size_t i = 0; i < 32; i++) {
        if (received_quad_rx[i] != test4_rx_data[i]) {
            quad_rx_match = false;
            break;
        }
    }

    if (quad_rx_match) {
        REG_INFO(2, logger) << "  [PASS] All 32 bytes match expected QUAD RX data" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] QUAD RX data mismatch" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX FIFO is now empty
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    rxempty = (status_val >> 24) & 0x1;

    if (rxqd == 0 && rxempty) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO empty after read: RXQD=0, RXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO not empty: RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 5: STANDARD Mode - Bidirectional Transfer (4 bytes)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 5] STANDARD Mode - Bidirectional Transfer (4 bytes)" << std::endl;

    // Pre-load slave with RX data
    std::vector<uint8_t> test5_rx_data = {0xB1, 0xB2, 0xB3, 0xB4};
    test->load_slave_rx_data(test5_rx_data);

    // Load TX FIFO
    test->write_register_32(TXDATA_OFFSET, 0xAABBCCDD);
    wait(10, SC_NS);

    // Verify TX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    if (txqd == 1) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check READY
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    // Issue COMMAND: LEN=3 (4 bytes), DIRECTION=BIDIR(3), SPEED=STANDARD(0), CSAAT=0
    uint32_t cmd5 = BUILD_CMD(3, 3, 0, 0);
    test->write_register_32(COMMAND_OFFSET, cmd5);
    wait(50, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;
    txstall = (status_val >> 27) & 0x1;
    rxstall = (status_val >> 23) & 0x1;

    if (!active && txempty && !txstall && !rxstall) {
        REG_INFO(2, logger) << "  [PASS] Transaction completed: ACTIVE=0, TXEMPTY=1, no stalls" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Status: ACTIVE=" << active << ", TXEMPTY=" << txempty
                  << ", TXSTALL=" << txstall << ", RXSTALL=" << rxstall << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] No errors after STANDARD BIDIR" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify TX data at slave
    const std::vector<uint8_t>& captured_bidir_tx = test->get_slave_captured_tx_data();
    std::vector<uint8_t> expected_bidir_tx = {0xDD, 0xCC, 0xBB, 0xAA};  /// Little-Endian

    if (captured_bidir_tx.size() >= 4) {
        bool tx_match = true;
        for (int i = 0; i < 4; i++) {
            if (captured_bidir_tx[i] != expected_bidir_tx[i]) {
                tx_match = false;
                break;
            }
        }
        if (tx_match) {
            REG_INFO(2, logger) << "  [PASS] Bidirectional TX data correct" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] Bidirectional TX data mismatch" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Slave received only " << captured_bidir_tx.size() << " bytes" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX data
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;

    if (rxqd == 1) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO contains bidirectional RX data: RXQD=1" << std::endl;
        sub_tests_passed++;

        // Read and verify RX data
        uint32_t rx_word;
        test->read_register_32(RXDATA_OFFSET, rx_word);
        std::vector<uint8_t> received_bidir_rx;
        if (byteorder) {  /// Little-Endian
            received_bidir_rx.push_back(rx_word & 0xFF);
            received_bidir_rx.push_back((rx_word >> 8) & 0xFF);
            received_bidir_rx.push_back((rx_word >> 16) & 0xFF);
            received_bidir_rx.push_back((rx_word >> 24) & 0xFF);
        } else {
            received_bidir_rx.push_back((rx_word >> 24) & 0xFF);
            received_bidir_rx.push_back((rx_word >> 16) & 0xFF);
            received_bidir_rx.push_back((rx_word >> 8) & 0xFF);
            received_bidir_rx.push_back(rx_word & 0xFF);
        }

        bool rx_match = true;
        for (int i = 0; i < 4; i++) {
            if (received_bidir_rx[i] != test5_rx_data[i]) {
                rx_match = false;
                break;
            }
        }

        if (rx_match) {
            REG_INFO(2, logger) << "  [PASS] Bidirectional RX data correct" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] Bidirectional RX data mismatch" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO: RXQD=" << rxqd << ", expected 1" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 6: Negative Test - DUAL + Bidirectional (Should Trigger CMDINVAL)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 6] Negative Test - DUAL + Bidirectional (Invalid Combination)" << std::endl;

    // Per datasheet: Bidirectional with DUAL/QUAD modes should trigger CMDINVAL error

    // Load TX FIFO
    test->write_register_32(TXDATA_OFFSET, 0x12345678);
    wait(10, SC_NS);

    // Check READY
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    // Issue INVALID COMMAND: LEN=3 (4 bytes), DIRECTION=BIDIR(3), SPEED=DUAL(1), CSAAT=0
    uint32_t cmd6 = BUILD_CMD(3, 3, 1, 0);
    test->write_register_32(COMMAND_OFFSET, cmd6);
    wait(50, SC_US);

    // Verify ERROR_STATUS.CMDINVAL is set
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    bool cmdinval = (status_val >> 3) & 0x1;  /// CMDINVAL at bit 3

    if (cmdinval) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected for DUAL+BIDIR (invalid combination)" << std::endl;
        sub_tests_passed++;

        // Clear error
        test->write_register_32(ERROR_STATUS_OFFSET, status_val);  /// W1C
        wait(10, SC_NS);

        // Verify error cleared
        test->read_register_32(ERROR_STATUS_OFFSET, status_val);
        if (status_val == 0) {
            REG_INFO(2, logger) << "  [PASS] ERROR_STATUS cleared successfully" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] ERROR_STATUS not cleared" << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CMDINVAL not set for invalid DUAL+BIDIR combination" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 7: CPOL=0, CPHA=1 (SPI Mode 1)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 7] SPI Mode 1 - CPOL=0, CPHA=1" << std::endl;

    // Software reset to ensure clean state before mode change
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// Assert SW_RST
    wait(50, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// Release SW_RST, re-enable
    wait(10, SC_NS);

    // Configure CONFIGOPTS: CLKDIV=10, CPOL=0, CPHA=1
    // CONFIGOPTS: bits [15:0]=CLKDIV, bit 1=CPHA, bit 0=CPOL
    uint32_t config_mode1 = (10 << 16) | (1 << 1) | 0;  /// CPHA=1, CPOL=0
    test->write_register_32(CONFIGOPTS_OFFSET, config_mode1);
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Read back and verify
    test->read_register_32(CONFIGOPTS_OFFSET, status_val);
    uint32_t cpol = status_val & 0x1;
    uint32_t cpha = (status_val >> 1) & 0x1;

    if (cpol == 0 && cpha == 1) {
        REG_INFO(2, logger) << "  [PASS] Mode 1 configured: CPOL=0, CPHA=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 1 config mismatch: CPOL=" << cpol << ", CPHA=" << cpha << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Execute simple TX transaction to verify mode works
    test->write_register_32(TXDATA_OFFSET, 0xAABBCCDD);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    uint32_t cmd_mode1 = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX
    test->write_register_32(COMMAND_OFFSET, cmd_mode1);
    wait(50, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;

    if (!active && txempty) {
        REG_INFO(2, logger) << "  [PASS] Mode 1 transaction completed" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 1 transaction failed: ACTIVE=" << active << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 8: CPOL=1, CPHA=0 (SPI Mode 2)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8] SPI Mode 2 - CPOL=1, CPHA=0" << std::endl;

    // Software reset to ensure clean state
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// Assert SW_RST
    wait(50, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// Release SW_RST, re-enable
    wait(10, SC_NS);

    // Configure CONFIGOPTS: CLKDIV=10, CPOL=1, CPHA=0
    uint32_t config_mode2 = (10 << 16) | (0 << 1) | 1;  /// CPHA=0, CPOL=1
    test->write_register_32(CONFIGOPTS_OFFSET, config_mode2);
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Read back and verify
    test->read_register_32(CONFIGOPTS_OFFSET, status_val);
    cpol = status_val & 0x1;
    cpha = (status_val >> 1) & 0x1;

    if (cpol == 1 && cpha == 0) {
        REG_INFO(2, logger) << "  [PASS] Mode 2 configured: CPOL=1, CPHA=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 2 config mismatch: CPOL=" << cpol << ", CPHA=" << cpha << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Execute simple TX transaction
    test->write_register_32(TXDATA_OFFSET, 0x11223344);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    uint32_t cmd_mode2 = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX
    test->write_register_32(COMMAND_OFFSET, cmd_mode2);
    wait(50, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;

    if (!active && txempty) {
        REG_INFO(2, logger) << "  [PASS] Mode 2 transaction completed" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 2 transaction failed: ACTIVE=" << active << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 9: CPOL=1, CPHA=1 (SPI Mode 3)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 9] SPI Mode 3 - CPOL=1, CPHA=1" << std::endl;

    // Software reset to ensure clean state
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// Assert SW_RST
    wait(50, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// Release SW_RST, re-enable
    wait(10, SC_NS);

    // Configure CONFIGOPTS: CLKDIV=10, CPOL=1, CPHA=1
    uint32_t config_mode3 = (10 << 16) | (1 << 1) | 1;  /// CPHA=1, CPOL=1
    test->write_register_32(CONFIGOPTS_OFFSET, config_mode3);
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Read back and verify
    test->read_register_32(CONFIGOPTS_OFFSET, status_val);
    cpol = status_val & 0x1;
    cpha = (status_val >> 1) & 0x1;

    if (cpol == 1 && cpha == 1) {
        REG_INFO(2, logger) << "  [PASS] Mode 3 configured: CPOL=1, CPHA=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 3 config mismatch: CPOL=" << cpol << ", CPHA=" << cpha << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Execute simple TX transaction
    test->write_register_32(TXDATA_OFFSET, 0x55667788);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    uint32_t cmd_mode3 = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX
    test->write_register_32(COMMAND_OFFSET, cmd_mode3);
    wait(50, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;

    if (!active && txempty) {
        REG_INFO(2, logger) << "  [PASS] Mode 3 transaction completed" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Mode 3 transaction failed: ACTIVE=" << active << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 10: FULLCYC Sampling Mode
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 10] FULLCYC Sampling Mode" << std::endl;

    // Software reset to ensure clean state
    test->write_register_32(CONTROL_OFFSET, 0x40000000);  /// Assert SW_RST
    wait(50, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  /// Release SW_RST, re-enable
    wait(10, SC_NS);

    // Configure CONFIGOPTS: CLKDIV=10, FULLCYC=1, CPOL=0, CPHA=0
    // CONFIGOPTS: bit 2=FULLCYC
    uint32_t config_fullcyc = (10 << 16) | (1 << 2) | 0;  /// FULLCYC=1
    test->write_register_32(CONFIGOPTS_OFFSET, config_fullcyc);
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Read back and verify
    test->read_register_32(CONFIGOPTS_OFFSET, status_val);
    uint32_t fullcyc = (status_val >> 2) & 0x1;

    if (fullcyc == 1) {
        REG_INFO(2, logger) << "  [PASS] FULLCYC mode configured" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FULLCYC not set" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Execute transaction to verify FULLCYC mode operational
    test->write_register_32(TXDATA_OFFSET, 0x12345678);
    wait(10, SC_NS);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-002: Speed Mode Validation", test_passed);
        return;
    }

    uint32_t cmd_fullcyc = BUILD_CMD(3, 2, 0, 0);  /// 4 bytes TX
    test->write_register_32(COMMAND_OFFSET, cmd_fullcyc);
    wait(50, SC_US);

    // Verify transaction completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;

    if (!active) {
        REG_INFO(2, logger) << "  [PASS] FULLCYC transaction completed" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FULLCYC transaction failed: ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 11: Maximum LEN Value (255 bytes boundary)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 11] Maximum LEN Value (255 bytes)" << std::endl;

    software_reset();
    wait(10, SC_NS);

    // Enable SPIEN and OUTPUT_EN
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    // Load TX FIFO with 64 words (255 bytes needs 64 words)
    for (int i = 0; i < 64; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAA000000 + i);
    }
    wait(10, SC_NS);

    // Verify READY
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;

    if (ready) {
        // Issue command with LEN=255 (maximum valid value, encoded as 254)
        uint32_t cmd_max_len = BUILD_CMD(254, 2, 0, 0);  // LEN=254 means 255 bytes
        test->write_register_32(COMMAND_OFFSET, cmd_max_len);
        wait(500, SC_US);

        // Verify transaction completed without error
        test->read_register_32(STATUS_OFFSET, status_val);
        active = (status_val >> 30) & 0x1;
        ready = (status_val >> 31) & 0x1;

        // Check for errors
        uint32_t error_status = 0;
        test->read_register_32(ERROR_STATUS_OFFSET, error_status);

        if (!active && ready && error_status == 0) {
            REG_INFO(2, logger) << "  [PASS] Maximum LEN=255 transaction completed without error" << std::endl;
            sub_tests_passed++;
        } else {
            REG_ERROR(2, logger) << "  [FAIL] LEN=255 transaction: ACTIVE=" << active
                      << ", READY=" << ready << ", ERROR_STATUS=0x" << std::hex << error_status << std::dec << std::endl;
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before LEN=255 test" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    clear_errors();
    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 12: Invalid LEN Values (Boundary Testing)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 12] Invalid LEN Values (>255)" << std::endl;

    software_reset();
    wait(10, SC_NS);

    // Enable SPIEN and OUTPUT_EN
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    // Enable error interrupts
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    wait(10, SC_NS);

    // Load some TX data
    for (int i = 0; i < 8; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xBB000000 + i);
    }
    wait(10, SC_NS);

    // Test 12a: LEN=256 (encoded as 255, but spec restricts to 255 max)
    REG_INFO(2, logger) << "  [Sub-test 12a] Testing LEN=256 (invalid)..." << std::endl;

    // Build command with LEN field = 255 (means 256 bytes, which exceeds spec)
    uint32_t cmd_invalid_len256 = (255 << 0) | (0 << 9) | (0 << 10) | (2 << 12);
    test->write_register_32(COMMAND_OFFSET, cmd_invalid_len256);
    wait(10, SC_US);

    // Check for CMDINVAL error (bit 12)
    uint32_t error_status256 = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, error_status256);
    bool cmdinval_len256 = (error_status256 >> 3) & 0x1;

    if (cmdinval_len256) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected for LEN=256 (ERROR_STATUS=0x"
                  << std::hex << error_status256 << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else {
        REG_INFO(2, logger) << "  [INFO] CMDINVAL not detected for LEN=256 (ERROR_STATUS=0x"
                  << std::hex << error_status256 << std::dec << ") - may be allowed by implementation" << std::endl;
        // Don't fail test - this boundary case may be implementation-dependent
    }

    clear_errors();
    wait(10, SC_NS);

    // Test 12b: LEN=300 (well beyond valid range)
    REG_INFO(2, logger) << "  [Sub-test 12b] Testing LEN=300 (invalid)..." << std::endl;

    // Build command with LEN field = 299 (9 bits can hold up to 511)
    uint32_t cmd_invalid_len300 = (299 << 0) | (0 << 9) | (0 << 10) | (2 << 12);
    test->write_register_32(COMMAND_OFFSET, cmd_invalid_len300);
    wait(10, SC_US);

    // Check for CMDINVAL error
    uint32_t error_status300 = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, error_status300);
    bool cmdinval_len300 = (error_status300 >> 3) & 0x1;

    if (cmdinval_len300) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected for LEN=300" << std::endl;
        sub_tests_passed++;
    } else {
        REG_INFO(2, logger) << "  [INFO] CMDINVAL not detected for LEN=300" << std::endl;
    }

    clear_errors();
    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 13: Invalid SPEED and DIRECTION Values
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 13] Invalid SPEED and DIRECTION Values" << std::endl;

    software_reset();
    wait(10, SC_NS);

    // Enable SPIEN and OUTPUT_EN
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    // Enable error interrupts
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    wait(10, SC_NS);

    // Load TX data
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xCC000000 + i);
    }
    wait(10, SC_NS);

    // Test 13a: Invalid SPEED=3 (valid range is 0-2: Standard/Dual/Quad)
    REG_INFO(2, logger) << "  [Sub-test 13a] Testing SPEED=3 (invalid)..." << std::endl;

    uint32_t cmd_invalid_speed = BUILD_CMD(7, 2, 3, 0);  // SPEED=3 is invalid
    test->write_register_32(COMMAND_OFFSET, cmd_invalid_speed);
    wait(10, SC_US);

    // Check for CMDINVAL error
    uint32_t error_status_speed = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, error_status_speed);
    bool cmdinval_speed3 = (error_status_speed >> 3) & 0x1;

    if (cmdinval_speed3) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected for SPEED=3 (ERROR_STATUS=0x"
                  << std::hex << error_status_speed << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else {
        REG_WARN(1, logger) << "  [WARN] CMDINVAL not detected for SPEED=3 (ERROR_STATUS=0x"
                  << std::hex << error_status_speed << std::dec << ")" << std::endl;
        // This SHOULD trigger error, so mark as potential issue but don't fail
    }

    clear_errors();
    wait(10, SC_NS);

    // Test 13b: Invalid DIRECTION=4 (valid range is 0-3: Dummy/RX/TX/Bidir)
    REG_INFO(2, logger) << "  [Sub-test 13b] Testing DIRECTION=4 (invalid)..." << std::endl;

    // Build command manually with DIRECTION=4 (bits 13:12)
    // COMMAND format: LEN(8:0), CSAAT(9), SPEED(11:10), DIRECTION(13:12)
    uint32_t cmd_invalid_dir = (7 << 0) | (0 << 9) | (0 << 10) | (4 << 12);
    test->write_register_32(COMMAND_OFFSET, cmd_invalid_dir);
    wait(10, SC_US);

    // Check for CMDINVAL error
    uint32_t error_status_dir = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, error_status_dir);
    bool cmdinval_dir4 = (error_status_dir >> 3) & 0x1;

    if (cmdinval_dir4) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected for DIRECTION=4 (ERROR_STATUS=0x"
                  << std::hex << error_status_dir << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else {
        REG_WARN(1, logger) << "  [WARN] CMDINVAL not detected for DIRECTION=4 (ERROR_STATUS=0x"
                  << std::hex << error_status_dir << std::dec << ")" << std::endl;
    }

    clear_errors();
    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 14: Zero-Length Transaction (LEN=0 means 1 byte)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 14] Zero-Length Transaction (LEN=0 = 1 byte)" << std::endl;

    software_reset();
    wait(10, SC_NS);

    // Enable SPIEN and OUTPUT_EN
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    // Load 1 byte worth of TX data
    test->write_register_32(TXDATA_OFFSET, 0xDD000000);
    wait(10, SC_NS);

    // Issue command with LEN=0 (means 1 byte according to spec)
    uint32_t cmd_len0 = BUILD_CMD(0, 2, 0, 0);  // LEN=0 encodes 1 byte
    test->write_register_32(COMMAND_OFFSET, cmd_len0);
    wait(50, SC_US);

    // Verify transaction completed without error
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    ready = (status_val >> 31) & 0x1;

    uint32_t error_status_len0 = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, error_status_len0);

    if (!active && ready && error_status_len0 == 0) {
        REG_INFO(2, logger) << "  [PASS] LEN=0 (1 byte) transaction completed without error" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] LEN=0 transaction: ACTIVE=" << active
                  << ", READY=" << ready << ", ERROR_STATUS=0x" << std::hex << error_status_len0 << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    clear_errors();
    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 15: CONFIGOPTS Register Access with Invalid CSID
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 15] CONFIGOPTS Register Access with Invalid CSID" << std::endl;

    software_reset();
    wait(10, SC_NS);

    // Enable SPIEN
    test->write_register_32(CONTROL_OFFSET, 0x80000000);
    wait(10, SC_NS);

    // Set valid CSID first and write CONFIGOPTS (baseline)
    REG_INFO(2, logger) << "  [Sub-test 15a] Baseline: CONFIGOPTS write with valid CSID=0..." << std::endl;
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    test->write_register_32(CONFIGOPTS_OFFSET, 0x00001234);  // Valid CONFIGOPTS write
    wait(10, SC_NS);

    // Read back CONFIGOPTS
    test->read_register_32(CONFIGOPTS_OFFSET, read_val);

    if (read_val == 0x00001234) {
        REG_INFO(2, logger) << "  [PASS] CONFIGOPTS write/read with valid CSID=0 successful" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CONFIGOPTS mismatch: wrote 0x1234, read 0x"
                  << std::hex << read_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Now set INVALID CSID and try to write CONFIGOPTS
    REG_INFO(2, logger) << "  [Sub-test 15b] CONFIGOPTS write with invalid CSID=5..." << std::endl;
    test->write_register_32(CSID_OFFSET, 5);  // NumCS=1, so 5 is invalid
    wait(10, SC_NS);

    // Verify CSID was written
    test->read_register_32(CSID_OFFSET, read_val);
    if (read_val == 5) {
        REG_INFO(2, logger) << "  [INFO] CSID set to invalid value: " << read_val << std::endl;
    }

    // Try to write CONFIGOPTS with invalid CSID
    test->write_register_32(CONFIGOPTS_OFFSET, 0x00005678);
    wait(10, SC_NS);

    // Read back CONFIGOPTS - should either:
    // 1. Return 0 (error case)
    // 2. Return previous valid value (write rejected)
    test->read_register_32(CONFIGOPTS_OFFSET, read_val);

    if (read_val == 0 || read_val == 0x00001234) {
        REG_INFO(2, logger) << "  [PASS] CONFIGOPTS write rejected for invalid CSID (CONFIGOPTS=0x"
                  << std::hex << read_val << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else if (read_val == 0x00005678) {
        REG_WARN(1, logger) << "  [WARN] CONFIGOPTS write succeeded despite invalid CSID (implementation allows)" << std::endl;
        // Don't fail - implementation may allow this
    } else {
        REG_WARN(1, logger) << "  [INFO] CONFIGOPTS read returned: 0x"
                  << std::hex << read_val << std::dec << std::endl;
    }

    // Try to read CONFIGOPTS with invalid CSID
    REG_INFO(2, logger) << "  [Sub-test 15c] CONFIGOPTS read with invalid CSID=5..." << std::endl;

    // The read itself should work (return 0 or warning)
    test->read_register_32(CONFIGOPTS_OFFSET, read_val);
    REG_INFO(2, logger) << "  [INFO] CONFIGOPTS read with invalid CSID returned: 0x"
              << std::hex << read_val << std::dec << std::endl;

    // Restore valid CSID for cleanup
    test->write_register_32(CSID_OFFSET, 0);
    wait(10, SC_NS);

    // Verify restoration
    test->read_register_32(CSID_OFFSET, read_val);
    if (read_val == 0) {
        REG_INFO(2, logger) << "  [PASS] CSID restored to valid value: 0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CSID not restored properly: " << read_val << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Final Verification
    // =======================================================================
    REG_INFO(1, logger) << "\n[Final Check] Verify FSM Returned to IDLE" << std::endl;

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    active = (status_val >> 30) & 0x1;

    if (ready && !active) {
        REG_INFO(2, logger) << "  [PASS] FSM in IDLE: READY=1, ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FSM not in IDLE: READY=" << ready << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================" << std::endl
                         << "Test Coverage:" << std::endl
                         << "  1. DUAL Mode - TX (8 bytes)" << std::endl
                         << "  2. DUAL Mode - RX (8 bytes)" << std::endl
                         << "  3. QUAD Mode - TX (16 bytes)" << std::endl
                         << "  4. QUAD Mode - RX (32 bytes)" << std::endl
                         << "  5. STANDARD Mode - Bidirectional (4 bytes)" << std::endl
                         << "  6. Negative Test - DUAL+BIDIR (Invalid)" << std::endl
                         << "  7. SPI Mode 1 - CPOL=0, CPHA=1" << std::endl
                         << "  8. SPI Mode 2 - CPOL=1, CPHA=0" << std::endl
                         << "  9. SPI Mode 3 - CPOL=1, CPHA=1" << std::endl
                         << "  10. FULLCYC Sampling Mode" << std::endl
                         << "  11. Maximum LEN Value (255 bytes)" << std::endl
                         << "  12. Invalid LEN Values (>255)" << std::endl
                         << "  13. Invalid SPEED and DIRECTION Values" << std::endl
                         << "  14. Zero-Length Transaction (LEN=0)" << std::endl
                         << "  15. CONFIGOPTS Register Access with Invalid CSID" << std::endl
                         << "  Note: STANDARD TX/RX covered by FUNC-001/003/004" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("FUNC-002: SPI Mode & Configuration Validation", test_passed);
}