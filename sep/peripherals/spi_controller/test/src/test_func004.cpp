// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"

/// FUNC-004: Interrupt-Driven TX/RX
void testbench::test_func004_interrupt_driven_txrx()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-004] Interrupt-Driven TX/RX" << std::endl
                         << "========================================" << std::endl;

    uint32_t status_val = 0;
    bool byteorder = m_byte_order.get_param_value();
    bool error_bit = false;
    bool error_irq = false;
    bool spi_event_bit = false;
    bool spi_event_irq = false;

    // =======================================================================
    // Initial Configuration
    // =======================================================================
    REG_INFO(1, logger) << "\n[Initial Setup] Configure SPI Host and Interrupts" << std::endl;

    // CRITICAL: Perform software reset first to ensure clean initial state
    software_reset();

    // Enable SPIEN, OUTPUT_EN, set RX watermark only (TX watermark will be set later)
    // CONTROL: SPIEN(31)=1, SW_RST(30)=0, OUTPUT_EN(29)=1, TX_WATERMARK(15:8)=0, RX_WATERMARK(7:0)=8
    test->write_register_32(CONTROL_OFFSET, 0xA0000008);
    wait(10, SC_NS);

    // Configure timing and chip select
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);  /// CLKDIV=10
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Enable both ERROR and SPI_EVENT interrupts
    // INTR_ENABLE packed: bit 0=ERROR, bit 1=SPI_EVENT
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    wait(10, SC_NS);

    // Enable all error interrupt sources (CMDINVAL, OVERFLOW, UNDERFLOW, CSIDINVAL, CMDBUSY)
    // ERROR_ENABLE packed: bits 0..4
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);
    wait(10, SC_NS);

    // Enable event interrupt sources (RXFULL, TXEMPTY, RXWM, TXWM, READY, IDLE)
    // EVENT_ENABLE packed: bits 0..5
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x3F);
    wait(10, SC_NS);

    // Verify STATUS.READY
    test->read_register_32(STATUS_OFFSET, status_val);
    bool ready = (status_val >> 31) & 0x1;

    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] STATUS.READY=0, cannot proceed" << std::endl;
        report_test_result("FUNC-004: Interrupt-Driven TX/RX", false);
        return;
    }
    REG_INFO(2, logger) << "  [PASS] SPI Host configured with interrupts enabled\n" << std::endl;
    sub_tests_passed++;

    // =======================================================================
    // Test 1: TX Watermark Interrupt (TXWM)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 1] TX Watermark Interrupt (TXWM)" << std::endl;

    // Clear any pending interrupts
    clear_interrupts();

    // CRITICAL: Load TX FIFO BEFORE setting watermark to ensure proper edge detection
    // Load TX FIFO with 4 words first (at or above future watermark)
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x11223340 + i);
    }
    wait(10, SC_NS);

    // Now set TX_WATERMARK=4 (TX FIFO depth=4, so 4 < 4 is false, m_prev_txwm=false)
    // CONTROL: SPIEN(31)=1, SW_RST(30)=0, OUTPUT_EN(29)=1, TX_WATERMARK(15:8)=4, RX_WATERMARK(7:0)=8
    test->write_register_32(CONTROL_OFFSET, 0xA0000408);
    wait(10, SC_NS);

    // Now issue a command to consume 1 word, dropping FIFO from 4→3 (below watermark)
    uint32_t cmd_trigger = BUILD_CMD(3, 2, 0, 0);  /// LEN=3 (4 bytes = 1 word), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd_trigger);
    wait(50, SC_US);  /// Allow transaction to complete

    wait(20, SC_NS);  /// Allow time for watermark event to propagate

    // Check TX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t txqd = status_val & 0xFF;

    if (txqd == 3) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO below watermark: TXQD=3 < TX_WATERMARK=4" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXQD=" << txqd << ", expected 3" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check if TXWM event triggered interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;  /// SPI_EVENT at bit 1
    spi_event_irq = sig_spi_event_irq.read();

    bool irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] TXWM interrupt asserted"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TXWM interrupt asserted (INTR_STATE.spi_event incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Clear interrupt and drain TX FIFO
    test->write_register_32(INTR_STATE_OFFSET, 0x2);  /// Clear SPI_EVENT at bit 4
    wait(10, SC_NS);

    // Issue command to drain remaining TX FIFO (3 words = 12 bytes remaining)
    uint32_t cmd1 = BUILD_CMD(11, 2, 0, 0);  /// LEN=11 (12 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd1);
    wait(200, SC_US);

    // Verify TX FIFO drained
    test->read_register_32(STATUS_OFFSET, status_val);
    bool txempty = (status_val >> 28) & 0x1;

    if (txempty) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO drained after transaction: TXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO not empty after transaction" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 2: RX Watermark Interrupt (RXWM)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 2] RX Watermark Interrupt (RXWM)" << std::endl;

    // Clear interrupts
    clear_interrupts();

    // Pre-load slave with sufficient data to fill RX FIFO beyond watermark
    std::vector<uint8_t> rx_data(64);
    for (int i = 0; i < 64; i++) {
        rx_data[i] = 0xA0 + i;
    }
    test->load_slave_rx_data(rx_data);

    // Issue RX command for 64 bytes (16 words) - exceeds watermark of 8
    uint32_t cmd2 = BUILD_CMD(63, 1, 0, 0);  /// LEN=63 (64 bytes), RX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd2);
    wait(600, SC_US);

    // Check RX FIFO depth
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t rxqd = (status_val >> 8) & 0xFF;
    bool active = (status_val >> 30) & 0x1;

    if (rxqd >= 8 && !active) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO crossed watermark: RXQD=" << rxqd << " >= 8" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO: RXQD=" << rxqd << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check RXWM interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;  /// SPI_EVENT at bit 1
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] RXWM interrupt asserted"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RXWM interrupt asserted incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX data integrity (read and check first 32 bytes)
    std::vector<uint8_t> received_data;
    for (int i = 0; i < 8; i++) {
        uint32_t rx_word;
        test->read_register_32(RXDATA_OFFSET, rx_word);

        if (byteorder) {  /// Little-Endian
            received_data.push_back(rx_word & 0xFF);
            received_data.push_back((rx_word >> 8) & 0xFF);
            received_data.push_back((rx_word >> 16) & 0xFF);
            received_data.push_back((rx_word >> 24) & 0xFF);
        } else {  /// Big-Endian
            received_data.push_back((rx_word >> 24) & 0xFF);
            received_data.push_back((rx_word >> 16) & 0xFF);
            received_data.push_back((rx_word >> 8) & 0xFF);
            received_data.push_back(rx_word & 0xFF);
        }
        wait(5, SC_NS);
    }

    bool rx_data_match = true;
    for (size_t i = 0; i < 32; i++) {
        if (received_data[i] != rx_data[i]) {
            rx_data_match = false;
            break;
        }
    }

    if (rx_data_match) {
        REG_INFO(2, logger) << "  [PASS] RX data integrity verified (32 bytes)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX data mismatch" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain remaining RX FIFO
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    for (uint32_t i = 0; i < rxqd; i++) {
        test->read_register_32(RXDATA_OFFSET, status_val);
        wait(5, SC_NS);
    }

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x2);  /// Clear SPI_EVENT at bit 4
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 3: IDLE Event Interrupt
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 3] IDLE Event Interrupt" << std::endl;

    // Clear interrupts
    clear_interrupts();

    // Load TX FIFO
    for (int i = 0; i < 8; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x55667780 + i);
    }
    wait(10, SC_NS);

    // Verify READY before command
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;

    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-004: Interrupt-Driven TX/RX", test_passed);
        return;
    }

    // Issue TX command
    uint32_t cmd3 = BUILD_CMD(31, 2, 0, 0);  /// LEN=31 (32 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd3);
    wait(250, SC_US);

    // Check transaction completed (ACTIVE 1->0 triggers IDLE event)
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;

    if (!active) {
        REG_INFO(2, logger) << "  [PASS] Transaction completed: ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Transaction still active" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check IDLE event interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;  /// SPI_EVENT at bit 1
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] IDLE event interrupt asserted"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] IDLE event interrupt asserted incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x2);  /// Clear SPI_EVENT at bit 4
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 4: Error Interrupt (CMDINVAL)
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 4] Error Interrupt (CMDINVAL)" << std::endl;

    // Clear interrupts
    clear_interrupts();

    // Clear any existing errors
    clear_errors();

    // Issue invalid command (SPEED=3, max is 2)
    uint32_t cmd_invalid = BUILD_CMD(7, 2, 3, 0);
    test->write_register_32(COMMAND_OFFSET, cmd_invalid);
    wait(50, SC_US);

    // Check ERROR_STATUS for CMDINVAL (bit 3)
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    bool cmdinval = (status_val >> 3) & 0x1;

    if (cmdinval) {
        REG_INFO(2, logger) << "  [PASS] CMDINVAL error detected (ERROR_STATUS=0x"
                  << std::hex << status_val << std::dec << ")" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] CMDINVAL not set" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check error interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    error_bit = status_val & 0x1;
    error_irq = sig_error_irq.read();

    irq_combined = sig_irq.read();
    if (error_bit && error_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] Error interrupt asserted"
                  << " (INTR_STATE.error=1, error_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Error interrupt asserted (INTR_STATE.error incomplete"
                  << " INTR_STATE.error=" << error_bit
                  << " error_irq=" << error_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Clear error using W1C semantics
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    test->write_register_32(ERROR_STATUS_OFFSET, status_val);
    wait(10, SC_NS);

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x1);
    wait(10, SC_NS);

    // Verify error cleared
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);

    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] Error cleared successfully" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Error not cleared: ERROR_STATUS=0x"
                  << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 5: TXEMPTY Event Interrupt
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 5] TXEMPTY Event Interrupt" << std::endl;

    // Clear interrupts
    clear_interrupts();

    // CRITICAL FIX: Ensure TX FIFO is completely empty before starting this test
    // Use software reset to flush any residual data from previous tests
    test->write_register_32(CONTROL_OFFSET, 0xE0000408);  /// Set SW_RST bit (bit 30)
    wait(50, SC_NS);  /// Allow reset to complete

    // Reconfigure after reset: SPIEN=1, OUTPUT_EN=1, TX_WATERMARK=4, RX_WATERMARK=8
    test->write_register_32(CONTROL_OFFSET, 0xA0000408);
    wait(10, SC_NS);

    // Re-enable interrupts after reset
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);  /// ERROR at bit 0, SPI_EVENT at bit 1
    wait(10, SC_NS);
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);  /// contiguous bits 0..4
    wait(10, SC_NS);
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x3F);  /// contiguous bits 0..4,20
    wait(10, SC_NS);

    // Load TX FIFO with exactly enough data (4 words = 16 bytes)
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xAABBCC00 + i);
    }
    wait(10, SC_NS);

    // Verify READY
    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;

    if (!ready) {
        REG_ERROR(2, logger) << "  [FAIL] Not READY before command" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-004: Interrupt-Driven TX/RX", test_passed);
        return;
    }

    // Issue TX command to drain FIFO completely
    uint32_t cmd4 = BUILD_CMD(15, 2, 0, 0);  /// LEN=15 (16 bytes), TX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd4);
    wait(200, SC_US);

    // Check TXEMPTY flag
    test->read_register_32(STATUS_OFFSET, status_val);
    txempty = (status_val >> 28) & 0x1;
    txqd = status_val & 0xFF;

    if (txempty && txqd == 0) {
        REG_INFO(2, logger) << "  [PASS] TX FIFO emptied: TXEMPTY=1, TXQD=0" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TX FIFO: TXEMPTY=" << txempty << ", TXQD=" << txqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check TXEMPTY event interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;  /// SPI_EVENT at bit 1
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] TXEMPTY event interrupt asserted"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] TXEMPTY event interrupt asserted incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x2);  /// Clear SPI_EVENT at bit 4
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 6: RXFULL Event Interrupt
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 6] RXFULL Event Interrupt" << std::endl;

    // Clear interrupts
    clear_interrupts();

    // Pre-load slave with RX FIFO capacity data (64 words = 256 bytes)
    std::vector<uint8_t> rxfull_data(m_rx_depth.get_param_value() * 4);
    for (size_t i = 0; i < rxfull_data.size(); i++) {
        rxfull_data[i] = 0xC0 + (i & 0xFF);
    }
    test->load_slave_rx_data(rxfull_data);

    // Issue RX command to fill FIFO to capacity
    uint32_t cmd5 = BUILD_CMD((m_rx_depth.get_param_value() * 4 - 1), 1, 0, 0);  /// RX_ONLY
    test->write_register_32(COMMAND_OFFSET, cmd5);
    wait(1, SC_MS);

    // Check RXFULL flag and RXQD
    test->read_register_32(STATUS_OFFSET, status_val);
    bool rxfull = (status_val >> 25) & 0x1;
    rxqd = (status_val >> 8) & 0xFF;

    if (rxfull && rxqd >= m_rx_depth.get_param_value() - 2) {
        REG_INFO(2, logger) << "  [PASS] RX FIFO full: RXFULL=1, RXQD=" << rxqd << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RX FIFO: RXFULL=" << rxfull << ", RXQD=" << rxqd << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check RXFULL event interrupt
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;  /// SPI_EVENT at bit 1
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] RXFULL event interrupt asserted"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] RXFULL event interrupt asserted incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drain RX FIFO
    for (uint32_t i = 0; i < rxqd; i++) {
        test->read_register_32(RXDATA_OFFSET, status_val);
        wait(5, SC_NS);
    }

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x2);  /// Clear SPI_EVENT at bit 4
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Test 7: INTR_TEST Register - Force Interrupt via Test Mechanism
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 7] INTR_TEST Register - Force Interrupts" << std::endl;

    // Establish a quiet baseline. INTR_STATE follows its causes, so masking
    // every event is what makes SPI_EVENT drop: level conditions such as IDLE
    // and TXEMPTY are still true here and would otherwise hold it asserted.
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x0);
    clear_interrupts();
    wait(10, SC_NS);

    // Verify interrupts are clear
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    bool error_irq_clear = sig_error_irq.read();
    bool spi_event_irq_clear = sig_spi_event_irq.read();

    if ((status_val == 0) && !error_irq_clear && !spi_event_irq_clear) {
        REG_INFO(2, logger) << "  [PASS] Interrupts cleared before INTR_TEST" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Interrupts not clear: INTR_STATE=0x"
                  << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Write to INTR_TEST to force ERROR interrupt (bit 0)
    test->write_register_32(INTR_TEST_OFFSET, 0x1);
    wait(10, SC_NS);

    // Check if ERROR interrupt was forced
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    error_bit = status_val & 0x1;  // Reuse error_bit from Test 4
    error_irq = sig_error_irq.read();

    irq_combined = sig_irq.read();
    if (error_bit && error_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] INTR_TEST forced ERROR interrupt"
                  << " (INTR_STATE.error=1, error_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] INTR_TEST forced ERROR interrupt (INTR_STATE.error incomplete"
                  << " INTR_STATE.error=" << error_bit
                  << " error_irq=" << error_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drop the forced source; INTR_STATE.error follows it down.
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    wait(10, SC_NS);

    // Write to INTR_TEST to force SPI_EVENT interrupt (bit 1)
    test->write_register_32(INTR_TEST_OFFSET, 0x2);
    wait(10, SC_NS);

    // Check if SPI_EVENT interrupt was forced
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] INTR_TEST forced SPI_EVENT interrupt"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] INTR_TEST forced SPI_EVENT interrupt (INTR_STATE.spi_event incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Drop the forced source
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    wait(10, SC_NS);

    // =======================================================================
    // Test 8: INTR_STATE is read-only — software writes are ignored
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 8] INTR_STATE Read-Only Semantics" << std::endl;

    // Force both interrupts simultaneously using INTR_TEST
    test->write_register_32(INTR_TEST_OFFSET, 0x3);  // Bits 0 and 1
    wait(10, SC_NS);

    // Verify both interrupts are set
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    error_bit = status_val & 0x1;
    spi_event_bit = (status_val >> 1) & 0x1;

    if (error_bit && spi_event_bit) {
        REG_INFO(2, logger) << "  [PASS] Both interrupts set: ERROR=1, SPI_EVENT=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Interrupts not both set: ERROR="
                  << error_bit << ", SPI_EVENT=" << spi_event_bit << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Attempt a write-1-to-clear. The RDL declares both fields sw=r/hw=w, so the
    // bus accepts the write and the hardware discards it.
    test->write_register_32(INTR_STATE_OFFSET, 0x3);
    wait(10, SC_NS);

    test->read_register_32(INTR_STATE_OFFSET, status_val);
    error_bit = status_val & 0x1;
    spi_event_bit = (status_val >> 1) & 0x1;

    if (error_bit && spi_event_bit) {
        REG_INFO(2, logger) << "  [PASS] Write to INTR_STATE ignored: ERROR=1, SPI_EVENT=1" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Write to INTR_STATE took effect: ERROR="
                  << error_bit << ", SPI_EVENT=" << spi_event_bit << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Removing the cause is the only way to clear the bits. EVENT_ENABLE is still
    // masked from Test 7, so SPI_EVENT will not re-assert on a level condition.
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    wait(10, SC_NS);

    test->read_register_32(INTR_STATE_OFFSET, status_val);
    if (status_val == 0) {
        REG_INFO(2, logger) << "  [PASS] Interrupts cleared once INTR_TEST was released" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Interrupts not cleared: INTR_STATE=0x"
                  << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test 9: EVENT_ENABLE Masking - Individual Event Control
    // =======================================================================
    REG_INFO(1, logger) << "\n[Test 9] EVENT_ENABLE Register - Event Masking" << std::endl;

    // Clear all interrupts
    clear_interrupts();
    wait(10, SC_NS);

    // Disable all event interrupts by writing 0 to EVENT_ENABLE
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x0);
    wait(10, SC_NS);

    // Load TX FIFO
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x12345670 + i);
    }
    wait(10, SC_NS);

    // Issue TX command to drain FIFO (should trigger TXEMPTY but it's masked)
    uint32_t cmd_mask = BUILD_CMD(15, 2, 0, 0);
    test->write_register_32(COMMAND_OFFSET, cmd_mask);
    wait(200, SC_US);

    // Check that no SPI_EVENT interrupt was triggered (masked out)
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;
    spi_event_irq = sig_spi_event_irq.read();

    if (!spi_event_bit && !spi_event_irq) {
        REG_INFO(2, logger) << "  [PASS] EVENT_ENABLE=0 masked all events (no interrupt)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] Event interrupt triggered despite mask: spi_event_bit="
                  << spi_event_bit << ", spi_event_irq=" << spi_event_irq << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Now enable only TXEMPTY event (bit 1)
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Clear any existing interrupts
    clear_interrupts();
    wait(10, SC_NS);

    // Load TX FIFO again
    for (int i = 0; i < 4; i++) {
        test->write_register_32(TXDATA_OFFSET, 0xABCDEF00 + i);
    }
    wait(10, SC_NS);

    // Issue TX command to drain FIFO (should trigger TXEMPTY and now it's unmasked)
    uint32_t cmd_unmask = BUILD_CMD(15, 2, 0, 0);
    test->write_register_32(COMMAND_OFFSET, cmd_unmask);
    wait(200, SC_US);

    // Check that SPI_EVENT interrupt was triggered (unmasked TXEMPTY)
    test->read_register_32(INTR_STATE_OFFSET, status_val);
    spi_event_bit = (status_val >> 1) & 0x1;
    spi_event_irq = sig_spi_event_irq.read();

    irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_irq && irq_combined) {
        REG_INFO(2, logger) << "  [PASS] EVENT_ENABLE unmasked TXEMPTY event (interrupt triggered)"
                  << " (INTR_STATE.spi_event=1, spi_event_irq=1, irq_o=1)" << std::endl;
        sub_tests_passed++;
    } else {
        REG_ERROR(2, logger) << "  [FAIL] EVENT_ENABLE unmasked TXEMPTY event (interrupt triggered) incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_irq
                  << " irq_o=" << irq_combined << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Clear interrupt
    test->write_register_32(INTR_STATE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Restore full EVENT_ENABLE for remaining tests
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x3F);
    wait(10, SC_NS);

    test->clear_slave_state();
    wait(10, SC_NS);

    // =======================================================================
    // Final Check
    // =======================================================================
    REG_INFO(1, logger) << "\n[Final Check] Verify FSM in IDLE" << std::endl;

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
                         << "  1. TX Watermark Interrupt (TXWM)" << std::endl
                         << "  2. RX Watermark Interrupt (RXWM)" << std::endl
                         << "  3. IDLE Event Interrupt" << std::endl
                         << "  4. Error Interrupt (CMDINVAL)" << std::endl
                         << "  5. TXEMPTY Event Interrupt" << std::endl
                         << "  6. RXFULL Event Interrupt" << std::endl
                         << "  7. INTR_TEST Register (Force Interrupts)" << std::endl
                         << "  8. INTR_STATE W1C Semantics" << std::endl
                         << "  9. EVENT_ENABLE Masking" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("FUNC-004: Interrupt-Driven TX/RX", test_passed);
}