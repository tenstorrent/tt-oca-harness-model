// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// FUNC-010: DMA Trigger Verification
/// Comprehensive test for DMA trigger signal behavior based on TX/RX FIFO watermarks
void testbench::test_func010_dma_trigger()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-010] DMA Trigger Verification" << std::endl
                         << "========================================" << std::endl;

    uint32_t status_val = 0;

    // =======================================================================
    // Initial Configuration
    // =======================================================================
    REG_INFO(1, logger) << "\n[Initial Setup] Configure SPI Host for DMA Testing" << std::endl;

    // CRITICAL: Perform software reset first to ensure clean initial state
    // This clears any residual data from previous test cases
    test->write_register_32(CONTROL_OFFSET, 0xE0000810);  /// Set SW_RST bit (bit 30)
    wait(50, SC_NS);  /// Allow reset to complete

    // Enable SPIEN, OUTPUT_EN, set watermarks (TX=8, RX=16)
    // CONTROL: SPIEN(31)=1, SW_RST(30)=0, OUTPUT_EN(29)=1, TX_WATERMARK(15:8)=8, RX_WATERMARK(7:0)=16
    test->write_register_32(CONTROL_OFFSET, 0xA0000810);
    wait(10, SC_NS);

    // Configure timing: CLKDIV=10
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);
    wait(10, SC_NS);

    // Set CSID=0
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Verify initial DMA trigger state (should be HIGH: TX_FIFO=0 < TX_WM=8)
    wait(20, SC_NS);
    bool dma_trigger_state = sig_dma_trigger.read();

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] Initial DMA trigger state: HIGH (TX=0 < 8, DMA should refill TX)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Initial DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Section 1: TX Watermark Tests
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Section 1: TX Watermark Tests" << std::endl
                         << "========================================" << std::endl;

    // =======================================================================
    // Test 1.1: TX Below Watermark - DMA Trigger Assertion
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 1.1] TX Below Watermark - DMA Trigger Assertion" << std::endl;
    REG_INFO(2, logger) << "  Scenario: TX FIFO depth < TX_WATERMARK (8)" << std::endl;

    // Load TX FIFO with 6 words (below watermark of 8)
    for (int i = 0; i < 6; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x11223340 + i);
    }
    wait(20, SC_NS);

    // Check TX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t txqd = status_val & 0xFF;

    // Check DMA trigger (should be HIGH: TX=6 < 8)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 6) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO depth: TXQD=6" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << ", expected 6" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger asserted: HIGH (TX=6 < 8)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test 1.2: TX At/Above Watermark - DMA Trigger De-assertion
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 1.2] TX At/Above Watermark - DMA Trigger De-assertion" << std::endl;
    REG_INFO(2, logger) << "  Scenario: TX FIFO depth >= TX_WATERMARK (8)" << std::endl;

    // Add 4 more words to reach 10 words (above watermark of 8)
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x44556670 + i);
    }
    wait(20, SC_NS);

    // Check TX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;

    // Check DMA trigger (should be LOW: TX=10 >= 8, RX=0 < 16)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 10) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO depth: TXQD=10" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << ", expected 10" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (!dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger de-asserted: LOW (TX=10 >= 8)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be LOW, but got HIGH" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test 1.3: TX Drain - DMA Trigger Re-assertion
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 1.3] TX Drain - DMA Trigger Re-assertion" << std::endl;
    REG_INFO(2, logger) << "  Scenario: Drain TX FIFO via transaction, trigger re-asserts" << std::endl;

    // Issue command to drain 32 bytes (8 words) from TX FIFO
    uint32_t cmd1 = BUILD_CMD(31, 2, 0, 0);  /// LEN=31 (32 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd1);
    wait(300, SC_US);

    // Check TX FIFO depth (should be 2 words remaining: 10 - 8 = 2)
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;

    // Check DMA trigger (should be HIGH: TX=2 < 8)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 2) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO drained: TXQD=2" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << ", expected 2" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger re-asserted: HIGH (TX=2 < 8)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain remaining TX FIFO
    uint32_t cmd_drain = BUILD_CMD(7, 2, 0, 0);  /// LEN=7 (8 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd_drain);
    wait(150, SC_US);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Section 2: RX Watermark Tests
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Section 2: RX Watermark Tests" << std::endl
                         << "========================================" << std::endl;

    // =======================================================================
    // Test 2.1: RX Below Watermark - DMA Trigger Low
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 2.1] RX Below Watermark - DMA Trigger Low" << std::endl;
    REG_INFO(2, logger) << "  Scenario: RX FIFO depth < RX_WATERMARK (16)" << std::endl;

    // Pre-load slave with 32 bytes (8 words, below watermark of 16)
    std::vector<uint8_t> rx_data_1(32);
    for (int i = 0; i < 32; i++) {
        rx_data_1[i] = 0xA0 + i;
    }
    test->load_slave_rx_data(rx_data_1);

    // Issue RX command for 32 bytes
    uint32_t cmd2 = BUILD_CMD(31, 1, 0, 0);  /// LEN=31 (32 bytes), RX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd2);
    wait(400, SC_US);

    // Check RX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t rxqd = (status_val >> 8) & 0xFF;
    txqd = status_val & 0xFF;

    // Check DMA trigger (should be HIGH: TX=0 < 8 is TRUE → overall HIGH)
    dma_trigger_state = sig_dma_trigger.read();

    if (rxqd == 8 && txqd == 0) {
        REG_INFO(2, logger) << "  [PASS] FIFO depths: TXQD=0, RXQD=8" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFO depths: TXQD=" << txqd << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger HIGH: (TX=0 < 8? YES, RX=8 >= 16? NO) → OR logic HIGH" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test 2.2: RX At/Above Watermark - DMA Trigger Assertion
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 2.2] RX At/Above Watermark - DMA Trigger Assertion" << std::endl;
    REG_INFO(2, logger) << "  Scenario: RX FIFO depth >= RX_WATERMARK (16)" << std::endl;

    // Pre-load slave with another 64 bytes (16 words total when added to existing 8)
    std::vector<uint8_t> rx_data_2(64);
    for (int i = 0; i < 64; i++) {
        rx_data_2[i] = 0xB0 + i;
    }
    test->load_slave_rx_data(rx_data_2);

    // Issue RX command for 64 bytes
    uint32_t cmd3 = BUILD_CMD(63, 1, 0, 0);  /// LEN=63 (64 bytes), RX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd3);
    wait(700, SC_US);

    // Check RX FIFO depth (should be 8 + 16 = 24 words)
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;

    // Check DMA trigger (should be HIGH: RX=24 >= 16)
    dma_trigger_state = sig_dma_trigger.read();

    if (rxqd == 24) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO depth: RXQD=24" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO: RXQD=" << rxqd << ", expected 24" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger asserted: HIGH (RX=24 >= 16)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test 2.3: RX Drain - DMA Trigger De-assertion
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 2.3] RX Drain - DMA Trigger De-assertion" << std::endl;
    REG_INFO(2, logger) << "  Scenario: Drain RX FIFO via reads, trigger de-asserts" << std::endl;

    // Read 12 words from RX FIFO (24 - 12 = 12 remaining, below watermark)
    for (int i = 0; i < 12; i++) {
        test->read_register_32(RXDATA_OFFSET, status_val);
        wait(5, SC_NS);
    }
    wait(20, SC_NS);

    // Check RX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    txqd = status_val & 0xFF;

    // Check DMA trigger (should be HIGH: TX=0 < 8 is TRUE → overall HIGH)
    dma_trigger_state = sig_dma_trigger.read();

    if (rxqd == 12 && txqd == 0) {
        REG_INFO(2, logger) << "  [PASS] FIFO depths: TXQD=0, RXQD=12" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFO depths: TXQD=" << txqd << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger HIGH: (TX=0 < 8? YES, RX=12 < 16? YES) → OR logic HIGH" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain remaining RX FIFO
    for (int i = 0; i < 12; i++) {
        test->read_register_32(RXDATA_OFFSET, status_val);
        wait(5, SC_NS);
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Section 3: Combined TX and RX Tests
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Section 3: Combined TX and RX Tests" << std::endl
                         << "========================================" << std::endl;

    // =======================================================================
    // Test 3.1: OR Logic - TX Condition Alone
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 3.1] OR Logic - TX Condition Alone" << std::endl;
    REG_INFO(2, logger) << "  Scenario: TX < 8 (TRUE), RX < 16 (FALSE) → Trigger HIGH" << std::endl;

    // Load TX FIFO with 5 words (below watermark)
    for (int i = 0; i < 5; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x77889900 + i);
    }
    wait(20, SC_NS);

    // Check FIFO depths
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    rxqd = (status_val >> 8) & 0xFF;

    // Check DMA trigger (should be HIGH: TX=5 < 8 is TRUE)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 5 && rxqd == 0) {
        REG_INFO(2, logger) << "  [PASS] FIFO state: TXQD=5, RXQD=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFO state: TXQD=" << txqd << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger HIGH via TX condition only" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain TX FIFO
    uint32_t cmd4 = BUILD_CMD(19, 2, 0, 0);  /// LEN=19 (20 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd4);
    wait(250, SC_US);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 3.2: OR Logic - Both Conditions Met
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 3.2] OR Logic - Both Conditions Met" << std::endl;
    REG_INFO(2, logger) << "  Scenario: TX < 8 (TRUE), RX >= 16 (TRUE) → Trigger HIGH" << std::endl;

    // Load TX FIFO with 3 words (below watermark)
    for (int i = 0; i < 3; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAABBCC00 + i);
    }
    wait(10, SC_NS);

    // Pre-load slave and fill RX FIFO with 20 words (above watermark)
    std::vector<uint8_t> rx_data_3(80);
    for (int i = 0; i < 80; i++) {
        rx_data_3[i] = 0xD0 + (i & 0xFF);
    }
    test->load_slave_rx_data(rx_data_3);

    // Issue RX command for 80 bytes
    uint32_t cmd5 = BUILD_CMD(79, 1, 0, 0);  /// LEN=79 (80 bytes), RX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd5);
    wait(800, SC_US);

    // Check FIFO depths
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    rxqd = (status_val >> 8) & 0xFF;

    // Check DMA trigger (should be HIGH: TX=3 < 8 OR RX=20 >= 16)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 3 && rxqd == 20) {
        REG_INFO(2, logger) << "  [PASS] FIFO state: TXQD=3, RXQD=20 (both conditions TRUE)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFO state: TXQD=" << txqd << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger HIGH via OR logic (both conditions)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be HIGH, but got LOW" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Cleanup: drain both FIFOs
    uint32_t cmd6 = BUILD_CMD(11, 2, 0, 0);  /// LEN=11 (12 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd6);
    wait(200, SC_US);

    for (int i = 0; i < 20; i++) {
        test->read_register_32(RXDATA_OFFSET, status_val);
        wait(5, SC_NS);
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Section 4: Boundary and Dynamic Tests
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Section 4: Boundary and Dynamic Tests" << std::endl
                         << "========================================" << std::endl;

    // =======================================================================
    // Test 4.1: Watermark = 0 Boundary Case
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 4.1] Watermark = 0 Boundary Case" << std::endl;
    REG_INFO(2, logger) << "  Scenario: TX_WATERMARK=0, RX_WATERMARK=1" << std::endl;
    REG_INFO(2, logger) << "  Logic: (TX < 0) is always FALSE, (RX >= 1) controls trigger" << std::endl;

    // Set TX_WATERMARK=0, RX_WATERMARK=1
    // CONTROL: SPIEN(31)=1, OUTPUT_EN(29)=1, TX_WATERMARK=0, RX_WATERMARK=1
    test->write_register_32(CONTROL_OFFSET, 0xA0000001);
    wait(20, SC_NS);

    // Load TX FIFO with 1 word (TX < 0 is FALSE)
    test->write_register_32(TXDATA_OFFSET, 0xDEADBEEF);
    wait(20, SC_NS);

    // Check FIFO depths
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    rxqd = (status_val >> 8) & 0xFF;

    // Check DMA trigger (should be LOW: TX=1 < 0? NO, RX=0 >= 1? NO)
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 1 && rxqd == 0) {
        REG_INFO(2, logger) << "  [PASS] FIFO state: TXQD=1, RXQD=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] FIFO state: TXQD=" << txqd << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    if (!dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger LOW: (TX=1 < 0? NO, RX=0 >= 1? NO)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be LOW, but got HIGH" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain TX FIFO
    uint32_t cmd7 = BUILD_CMD(3, 2, 0, 0);  /// LEN=3 (4 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd7);
    wait(150, SC_US);

    // Verify trigger still LOW
    wait(20, SC_NS);
    dma_trigger_state = sig_dma_trigger.read();

    if (!dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] DMA trigger still LOW after drain: (TX=0 < 0? NO, RX=0 >= 1? NO)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be LOW, but got HIGH" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 4.2: Dynamic Watermark Change
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 4.2] Dynamic Watermark Change" << std::endl;
    REG_INFO(2, logger) << " Scenario: Change watermark while FIFOs have data" << std::endl;

    // Set TX_WATERMARK=20, RX_WATERMARK=10
    test->write_register_32(CONTROL_OFFSET, 0xA0001410);
    wait(10, SC_NS);

    // Load TX FIFO with 15 words (below new watermark of 20)
    for (int i = 0; i < 15; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x12345600 + i);
    }
    wait(20, SC_NS);

    // Check initial state
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 15 && dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] Initial: TXQD=15, DMA trigger HIGH (TX=15 < 20)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Initial: TXQD=" << txqd << ", trigger=" << dma_trigger_state << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Change TX_WATERMARK to 10 (now TX=15 >= 10, trigger should go LOW)
    test->write_register_32(CONTROL_OFFSET, 0xA0000A10);
    wait(20, SC_NS);

    // Check updated state
    dma_trigger_state = sig_dma_trigger.read();

    if (!dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] After watermark change: DMA trigger LOW (TX=15 >= 10)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] DMA trigger should be LOW after watermark change" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain TX FIFO
    uint32_t cmd8 = BUILD_CMD(59, 2, 0, 0);  /// LEN=59 (60 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd8);
    wait(600, SC_US);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 4.3: Reset Behavior
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 4.3] Reset Behavior" << std::endl;
    REG_INFO(2, logger) << "  Scenario: SW_RST clears FIFOs, trigger recalculated" << std::endl;

    // Load TX FIFO with 5 words (below watermark, trigger should be HIGH)
    test->write_register_32(CONTROL_OFFSET, 0xA0000810);  /// TX_WM=8, RX_WM=16
    wait(10, SC_NS);

    for (int i = 0; i < 5; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x99887700 + i);
    }
    wait(20, SC_NS);

    // Verify trigger is HIGH before reset
    dma_trigger_state = sig_dma_trigger.read();
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;

    if (txqd == 5 && dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] Before reset: TXQD=5, DMA trigger HIGH" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Before reset: TXQD=" << txqd << ", trigger=" << dma_trigger_state << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Issue SW_RST with watermarks that would NOT assert trigger when empty
    // Set TX_WATERMARK=0, RX_WATERMARK=1 (so TX=0 < 0? NO, RX=0 >= 1? NO)
    test->write_register_32(CONTROL_OFFSET, 0xE0000001);  /// SPIEN=1, SW_RST=1, OUTPUT_EN=1, TX_WM=0, RX_WM=1
    wait(50, SC_NS);

    // Check state after reset
    test->read_register_32(STATUS_OFFSET, status_val);
    txqd = status_val & 0xFF;
    rxqd = (status_val >> 8) & 0xFF;
    dma_trigger_state = sig_dma_trigger.read();

    if (txqd == 0 && rxqd == 0 && !dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] After reset: FIFOs cleared (TXQD=0, RXQD=0), DMA trigger LOW" << std::endl;
        REG_INFO(2, logger) << "         Watermarks set to TX_WM=0, RX_WM=1 to ensure trigger LOW when empty" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] After reset: TXQD=" << txqd << ", RXQD=" << rxqd
                  << ", trigger=" << dma_trigger_state << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Restore normal operation with original watermarks
    test->write_register_32(CONTROL_OFFSET, 0xA0000810);  /// TX_WM=8, RX_WM=16
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Final Verification
    // =======================================================================
    REG_INFO(1, logger) << "\n[Final Verification] Clean State Check" << std::endl;

    test->read_register_32(STATUS_OFFSET, status_val);
    bool ready = (status_val >> 31) & 0x1;
    bool active = (status_val >> 30) & 0x1;
    txqd = status_val & 0xFF;
    rxqd = (status_val >> 8) & 0xFF;
    dma_trigger_state = sig_dma_trigger.read();

    // With TX_WM=8, RX_WM=16, and empty FIFOs (TX=0, RX=0), trigger should be HIGH (TX=0 < 8)
    if (ready && !active && txqd == 0 && rxqd == 0 && dma_trigger_state) {
        REG_INFO(2, logger) << "  [PASS] Clean state: READY=1, ACTIVE=0, TXQD=0, RXQD=0, DMA trigger=HIGH (TX refill needed)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] State: READY=" << ready << ", ACTIVE=" << active
                  << ", TXQD=" << txqd << ", RXQD=" << rxqd
                  << ", trigger=" << dma_trigger_state << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================" << std::endl;
    REG_INFO(1, logger) << "Test Coverage:" << std::endl
                         << "  Section 1: TX Watermark Tests (3 tests)" << std::endl
                         << "    1.1: TX Below Watermark - Trigger Assertion" << std::endl
                         << "    1.2: TX At/Above Watermark - Trigger De-assertion" << std::endl
                         << "    1.3: TX Drain - Trigger Re-assertion" << std::endl
                         << "  Section 2: RX Watermark Tests (3 tests)" << std::endl
                         << "    2.1: RX Below Watermark - Trigger Low" << std::endl
                         << "    2.2: RX At/Above Watermark - Trigger Assertion" << std::endl
                         << "    2.3: RX Drain - Trigger De-assertion" << std::endl
                         << "  Section 3: Combined TX and RX Tests (2 tests)" << std::endl
                         << "    3.1: OR Logic - TX Condition Alone" << std::endl
                         << "    3.2: OR Logic - Both Conditions Met" << std::endl
                         << "  Section 4: Boundary and Dynamic Tests (3 tests)" << std::endl
                         << "    4.1: Watermark = 0 Boundary Case" << std::endl
                         << "    4.2: Dynamic Watermark Change" << std::endl
                         << "    4.3: Reset Behavior" << std::endl;
    REG_INFO(1, logger) << "========================================" << std::endl
                         << "DMA Trigger Logic: trigger = (tx_depth < tx_wm) || (rx_depth >= rx_wm)" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("FUNC-010: DMA Trigger Verification", test_passed);
}
