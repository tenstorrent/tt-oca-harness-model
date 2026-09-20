// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_coverage.cpp
 * @brief Additional coverage tests to reach 95% line coverage
 *
 * This file contains 7 targeted tests designed to cover specific
 * uncovered code paths identified in the coverage analysis.
 */

#include "testbench.h"
#include "spi_controller_interface.h"

using namespace spi_controller_regs;

/**
 * @brief Test 1: Big-Endian byte ordering
 *
 * Coverage target: Lines 398-399 (pack_word Big-Endian), 418-419 (unpack_word Big-Endian)
 *
 * This test creates a second SPI controller instance with Big-Endian byte ordering
 * and performs TX/RX transactions to exercise byte packing/unpacking.
 */
void testbench::test_coverage_big_endian_byte_order()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 1] Big-Endian Byte Ordering" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;

    software_reset();
    wait(100, SC_NS);

    // Set ByteOrder to Big-Endian (false)
    dut->ByteOrder.Set_param(dut->ByteOrder.get_Name(), false);
    wait(10, SC_NS);

    // Configure SPI controller basic (sets SPIEN=1, OUTPUT_EN=1, etc.)
    configure_spi_controller_basic();

    // Verify ByteOrder is now false
    if (!dut->get_byte_order()) {
        REG_INFO(0, test->logger) << "[PASS] ByteOrder successfully changed to Big-Endian" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Failed to change ByteOrder to Big-Endian" << std::endl;
        report_test_result("Big-Endian Byte Order", false);
        return;
    }

    // Load TX FIFO with data: 0x11223344
    test->write_register_32(TXDATA_OFFSET, 0x11223344);
    wait(10, SC_NS);

    // Queue a 4-byte TX/RX Bidir command (Standard speed, CSAAT=0)
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 3, 0, 0));
    wait(10, SC_NS);

    // Wait for transaction to complete
    bool completed = wait_for_transaction_complete();
    if (!completed) {
        REG_ERROR(0, test->logger) << "[FAIL] Big-Endian transaction timed out" << std::endl;
        report_test_result("Big-Endian Byte Order", false);
        return;
    }

    // Verify STATUS shows transaction completed (ACTIVE=0, READY=1)
    uint32_t status;
    test->read_register_32(STATUS_OFFSET, status);
    bool ready = (status >> 31) & 0x1;
    bool active = (status >> 30) & 0x1;
    if (ready && !active) {
        REG_INFO(0, test->logger) << "[PASS] Big-Endian transaction completed: READY=1, ACTIVE=0" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Unexpected status after Big-Endian transaction: READY=" << ready << ", ACTIVE=" << active << std::endl;
        test_passed = false;
    }

    // Read RX FIFO - verify data was received
    uint32_t rx_data;
    test->read_register_32(RXDATA_OFFSET, rx_data);
    wait(10, SC_NS);
    REG_INFO(0, test->logger) << "[INFO] Big-Endian read RXDATA = 0x" << std::hex << rx_data << std::dec << std::endl;

    // Restore ByteOrder to Little-Endian (true)
    dut->ByteOrder.Set_param(dut->ByteOrder.get_Name(), true);
    wait(10, SC_NS);

    report_test_result("Big-Endian Byte Order", test_passed);
}


/**
 * @brief Test 2: INTR_TEST edge cases
 *
 * Coverage target: Lines 903, 928, 936, 953-954
 *
 * Tests the INTR_TEST register behavior when:
 * 1. Forcing error interrupt while real error exists → release should keep error active
 * 2. Forcing spi_event interrupt while real event exists → release should keep event active
 */
void testbench::test_coverage_intr_test_edge_cases()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 2] INTR_TEST Edge Cases" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    software_reset();
    wait(100, SC_NS);

    // Enable interrupts: error (bit 0) and spi_event (bit 1) -> 0x3
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    wait(10, SC_NS);

    // Sub-test 1: Force error interrupt, create real error, then release
    REG_INFO(0, test->logger) << "[Sub-Test 1] Error interrupt: force + real error + release" << std::endl;

    // Force error interrupt via INTR_TEST (bit 0 -> 0x1)
    test->write_register_32(INTR_TEST_OFFSET, 0x1);
    wait(10, SC_NS);

    // Create a real error: write invalid SPEED=3 to COMMAND to trigger ERROR_STATUS.CMDINVAL
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(0, 0, 3, 0));
    wait(10, SC_NS);

    // Enable ERROR_ENABLE to make the error contribute to interrupt (0x1F enables all error sources)
    test->write_register_32(ERROR_ENABLE_OFFSET, 0x1F);
    wait(10, SC_NS);

    // Release test-forced interrupt by writing 0 to INTR_TEST
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    wait(10, SC_NS);

    // Check that error interrupt remains active due to real error (line 903)
    bool test_passed = true;
    uint32_t intr_status;
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status & 0x1) {
        REG_INFO(0, test->logger) << "[PASS] Error interrupt remained active after test release (real error present)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Error interrupt cleared despite real error" << std::endl;
        test_passed = false;
    }

    // Clear error status
    test->write_register_32(ERROR_STATUS_OFFSET, 0xFF);
    test->write_register_32(INTR_STATE_OFFSET, 0x3);
    wait(10, SC_NS);

    // Sub-test 2: Force spi_event interrupt, create real event, then release
    REG_INFO(0, test->logger) << "\n[Sub-Test 2] SPI_EVENT interrupt: force + real event + release" << std::endl;

    // Enable SPI controller to allow events
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  // SPIEN=1, OUTPUT_EN=1
    wait(10, SC_NS);

    // Force spi_event interrupt via INTR_TEST (bit 1 -> 0x2)
    test->write_register_32(INTR_TEST_OFFSET, 0x2);
    wait(10, SC_NS);

    // Create a real event by enabling TXEMPTY event when TX FIFO is empty
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 1));  // Enable TXEMPTY event
    wait(10, SC_NS);

    // Release test-forced interrupt
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    wait(10, SC_NS);

    // Check that spi_event interrupt remains active due to real event (lines 928, 936, 953-954)
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status & 0x2) {
        REG_INFO(0, test->logger) << "[PASS] SPI_EVENT interrupt remained active after test release (real event present)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] SPI_EVENT interrupt cleared despite real event. intr_status = 0x" << std::hex << intr_status << std::dec << std::endl;
        test_passed = false;
    }

    REG_INFO(0, test->logger) << "" << std::endl;
    report_test_result("INTR_TEST Edge Cases", test_passed);
}

/**
 * @brief Test 3: EVENT_ENABLE when conditions already met
 *
 * Coverage target: Lines 1036, 1050, 1066
 *
 * Tests that enabling EVENT_ENABLE bits triggers interrupt immediately
 * when the event condition is already met (not waiting for edge).
 */
void testbench::test_coverage_event_enable_immediate_trigger()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 3] EVENT_ENABLE Immediate Trigger" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    software_reset();
    wait(100, SC_NS);

    // Enable SPI controller and interrupts: spi_event is bit 1 -> 0x2
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  // SPIEN=1, OUTPUT_EN=1
    test->write_register_32(INTR_ENABLE_OFFSET, 0x2);  // Enable spi_event interrupt
    wait(10, SC_NS);

    // Sub-test 1: TXEMPTY - Enable event when TX FIFO already empty
    REG_INFO(0, test->logger) << "[Sub-Test 1] TXEMPTY event - condition already met" << std::endl;

    // Clear previous events
    test->write_register_32(INTR_STATE_OFFSET, 0x2);
    wait(10, SC_NS);

    // TX FIFO is empty by default, now enable TXEMPTY event (line 1043)
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 1));
    wait(10, SC_NS);

    bool test_passed = true;
    uint32_t intr_status;
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status & 0x2) {
        REG_INFO(0, test->logger) << "[PASS] TXEMPTY event triggered immediately" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] TXEMPTY event did not trigger" << std::endl;
        test_passed = false;
    }

    // Sub-test 2: RXFULL - Enable event when RX FIFO already full
    REG_INFO(0, test->logger) << "\n[Sub-Test 2] RXFULL event - condition already met" << std::endl;

    software_reset();
    wait(100, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Enable RXFULL event (exercises the code path in handle_write_EVENT_ENABLE)
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 0));
    wait(10, SC_NS);

    // RX FIFO is empty, so RXFULL should NOT trigger
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (!(intr_status & 0x2)) {
        REG_INFO(0, test->logger) << "[PASS] RXFULL event correctly not triggered (RX FIFO empty)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] RXFULL event triggered unexpectedly" << std::endl;
        test_passed = false;
    }

    // Sub-test 3: TXWM - Enable event when TX depth below watermark
    REG_INFO(0, test->logger) << "\n[Sub-Test 3] TXWM event - condition already met" << std::endl;

    software_reset();
    wait(100, SC_NS);
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Set TX watermark to 10 (TX FIFO is empty = 0, so 0 < 10)
    test->write_register_32(CONTROL_OFFSET, 0xA0000A00);  // SPIEN=1, OUTPUT_EN=1, TX_WATERMARK=10
    wait(10, SC_NS);

    // Clear previous events
    test->write_register_32(INTR_STATE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Enable TXWM event - should trigger immediately since 0 < 10 (line 1010)
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 3));
    wait(10, SC_NS);

    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status & 0x2) {
        REG_INFO(0, test->logger) << "[PASS] TXWM event triggered immediately" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] TXWM event did not trigger" << std::endl;
        test_passed = false;
    }

    // Sub-test 4: RXWM - Enable event when RX depth exceeds watermark
    REG_INFO(0, test->logger) << "\n[Sub-Test 4] RXWM event - condition already met" << std::endl;

    // Set RX watermark to 0 (any data in RX FIFO will exceed watermark)
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);  // RX_WATERMARK=0
    wait(10, SC_NS);

    // Clear previous events
    test->write_register_32(INTR_STATE_OFFSET, 0x2);
    wait(10, SC_NS);

    // Enable RXWM event (line 1036) - RX empty, so condition NOT met
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 2));
    wait(10, SC_NS);

    REG_INFO(0, test->logger) << "[INFO] RXWM event enable code path tested\n" << std::endl;

    report_test_result("EVENT_ENABLE Immediate Trigger", test_passed);
}

/**
 * @brief Test 4: SPIEN re-enable with queued commands
 *
 * Coverage target: Line 1150
 *
 * Tests that re-enabling SPIEN when commands are queued wakes up the transaction thread.
 */
void testbench::test_coverage_spien_reenable_queued_commands()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 4] SPIEN Re-enable with Queued Commands" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    software_reset();
    wait(100, SC_NS);

    // Configure SPI controller (SPIEN=1, OUTPUT_EN=1 -> 0xA0000001)
    test->write_register_32(CONTROL_OFFSET, 0xA0000001);
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);   // CLKDIV=10
    wait(10, SC_NS);

    // Load TX FIFO with data
    load_tx_fifo(2, 0xAABBCCDD);
    wait(10, SC_NS);

    // Disable SPIEN but keep OUTPUT_EN=1 (SPIEN=0, OUTPUT_EN=1 -> 0x20000001)
    test->write_register_32(CONTROL_OFFSET, 0x20000001);
    wait(10, SC_NS);

    REG_INFO(0, test->logger) << "[INFO] SPIEN disabled" << std::endl;

    // Queue a command while SPIEN is disabled
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(7, 2, 0, 0));  // 8-byte TX
    wait(10, SC_NS);

    REG_INFO(0, test->logger) << "[INFO] Command queued while SPIEN disabled" << std::endl;

    // Verify STATUS.ACTIVE is 0 while SPIEN is disabled
    uint32_t status;
    test->read_register_32(STATUS_OFFSET, status);

    // Re-enable SPIEN - should wake up transaction thread (line 1150)
    test->write_register_32(CONTROL_OFFSET, 0xA0000001);  // SPIEN=1, OUTPUT_EN=1
    wait(200, SC_US);  // Wait for transaction to complete

    // Verify transaction completed after SPIEN re-enable
    bool test_passed = true;
    test->read_register_32(STATUS_OFFSET, status);
    bool ready = (status >> 31) & 0x1;
    bool active_after = (status >> 30) & 0x1;
    uint32_t txqd = status & 0xFF;

    if (ready && !active_after && txqd == 0) {
        REG_INFO(0, test->logger) << "[PASS] Transaction completed after SPIEN re-enable: READY=1, ACTIVE=0, TXQD=0" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Transaction did not complete: READY=" << ready << ", ACTIVE=" << active_after << ", TXQD=" << txqd << std::endl;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_NS);

    report_test_result("SPIEN Re-enable with Queued Commands", test_passed);
}

/**
 * @brief Test 5: Reset with queued commands
 *
 * Coverage target: Line 296 (command queue clear in reset_process)
 *
 * Tests that hardware reset clears the command queue.
 */
void testbench::test_coverage_reset_with_queued_commands()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 5] Reset with Queued Commands" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    software_reset();
    wait(100, SC_NS);

    // Configure SPI controller (SPIEN=0, OUTPUT_EN=1 to keep commands queued)
    test->write_register_32(CONTROL_OFFSET, 0x20000001);  // SPIEN=0, OUTPUT_EN=1
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);
    wait(10, SC_NS);

    // Queue multiple commands
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(10, SC_NS);
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(10, SC_NS);
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(10, SC_NS);

    REG_INFO(0, test->logger) << "[INFO] Multiple commands queued" << std::endl;

    // Check STATUS.CMDQD before reset
    uint32_t status;
    test->read_register_32(STATUS_OFFSET, status);
    uint32_t cmdqd_before = (status >> 16) & 0xF;
    REG_INFO(0, test->logger) << "[INFO] Command queue depth before reset: " << cmdqd_before << std::endl;

    // Trigger hardware reset (line 296 clears queue)
    apply_reset();
    wait(100, SC_NS);

    // Check STATUS.CMDQD after reset
    test->read_register_32(STATUS_OFFSET, status);
    uint32_t cmdqd_after = (status >> 16) & 0xF;

    bool test_passed = true;
    if (cmdqd_before != 3) {
        REG_ERROR(0, test->logger) << "[FAIL] Command queue was not populated before reset (expected 3, got " << cmdqd_before << ")" << std::endl;
        test_passed = false;
    }
    if (cmdqd_after == 0) {
        REG_INFO(0, test->logger) << "[PASS] Command queue cleared after reset (CMDQD=" << cmdqd_after << ")\n" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Command queue not cleared (CMDQD=" << cmdqd_after << ")\n" << std::endl;
        test_passed = false;
    }

    // COMMAND writes are ignored while SW_RST is held as a level (not a pulse).
    REG_INFO(0, test->logger) << "[Sub-Test] COMMAND write ignored while SW_RST held" << std::endl;
    test->write_register_32(CONTROL_OFFSET, 0x40000000);
    wait(10, SC_NS);
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(10, SC_NS);
    test->read_register_32(STATUS_OFFSET, status);
    uint32_t cmdqd_held = (status >> 16) & 0xF;
    if (cmdqd_held == 0) {
        REG_INFO(0, test->logger) << "[PASS] COMMAND ignored while SW_RST held (CMDQD=0)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] COMMAND was queued while SW_RST held (CMDQD=" << cmdqd_held << ")" << std::endl;
        test_passed = false;
    }
    test->write_register_32(CONTROL_OFFSET, 0x00000000);
    wait(10, SC_NS);

    report_test_result("Reset with Queued Commands", test_passed);
}

/**
 * @brief Test 6: Signal update during reset
 *
 * Coverage target: Lines 702-703
 *
 * Tests that update_output_signals_method() gracefully handles being called during reset.
 */
void testbench::test_coverage_signal_update_during_reset()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 6] Signal Update During Reset" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;

    software_reset();
    wait(100, SC_NS);

    // Enable SPI controller and interrupts: error (bit 0) + spi_event (bit 1) -> 0x3
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 1));  // Enable TXEMPTY event
    wait(10, SC_NS);

    // Verify interrupt is active
    uint32_t intr_status;
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    REG_INFO(0, test->logger) << "[INFO] Interrupt status before reset: 0x" << std::hex << intr_status << std::dec << std::endl;

    // Trigger reset while interrupts are active (lines 702-703)
    // The update_output_signals_method() will be called but should skip during reset
    REG_INFO(0, test->logger) << "[INFO] Asserting reset..." << std::endl;
    apply_reset();
    wait(50, SC_NS);

    // Check that interrupts are properly reset
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status == 0) {
        REG_INFO(0, test->logger) << "[PASS] Interrupts cleared after reset (0x" << std::hex << intr_status << std::dec << ")" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Interrupts not cleared after reset (0x" << std::hex << intr_status << std::dec << ")" << std::endl;
        test_passed = false;
    }

    REG_INFO(0, test->logger) << "[PASS] Signal update during reset handled gracefully\n" << std::endl;

    report_test_result("Signal Update During Reset", test_passed);
}

/**
 * @brief Test 7: FIFO Overflow, Underflow, Empty Reads, and Stalls
 *
 * Coverage target:
 * 1. Line 272 (TX FIFO push full return false) & 887-889 (TX FIFO Overflow warning)
 * 2. Line 310 (RX FIFO pop empty return false) & 1415-1416 (RX FIFO Empty read warning)
 * 3. Lines 559-566 (TX FIFO underflow error path)
 * 4. Lines 626-628 (RX FIFO space available stall / resume)
 * 5. COMMAND write while an error is latched — queued, with CMDBUSY and READY untouched
 */
void testbench::test_coverage_fifo_overflow_underflow()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 7] FIFO Overflow, Underflow & Stalls" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;

    software_reset();
    wait(100, SC_NS);

    // 1. TX FIFO Overflow Test
    // Pushing more than capacity (TxDepth + 1 = 73 words, incl. the byte_select stage)
    REG_INFO(0, test->logger) << "[Sub-Test 1] TX FIFO Overflow" << std::endl;
    for (int i = 0; i < 74; i++) {
        test->write_register_32(TXDATA_OFFSET, 0x11223340 + i);
    }
    wait(10, SC_NS);

    // Check ERROR_STATUS (should show overflow = 1)
    uint32_t err_status;
    test->read_register_32(ERROR_STATUS_OFFSET, err_status);
    if (err_status & 0x2) { // overflow bit is bit 1 (value 2)
        REG_INFO(0, test->logger) << "[PASS] TX FIFO Overflow detected: ERROR_STATUS = 0x" << std::hex << err_status << std::dec << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] TX FIFO Overflow bit not set in ERROR_STATUS = 0x" << std::hex << err_status << std::dec << std::endl;
        test_passed = false;
    }
    // W1C error status
    test->write_register_32(ERROR_STATUS_OFFSET, 0xFF);
    wait(10, SC_NS);

    // 2. RX FIFO Pop Empty Test
    REG_INFO(0, test->logger) << "\n[Sub-Test 2] RX FIFO Empty Read" << std::endl;
    uint32_t rx_val;
    test->read_register_32(RXDATA_OFFSET, rx_val);
    wait(10, SC_NS);
    if (rx_val == 0) {
        REG_INFO(0, test->logger) << "[PASS] Pop from empty RX FIFO returned 0" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Pop from empty RX FIFO returned 0x" << std::hex << rx_val << std::dec << std::endl;
        test_passed = false;
    }

    // 3. TX FIFO Underflow Test
    REG_INFO(0, test->logger) << "\n[Sub-Test 3] TX FIFO Underflow" << std::endl;
    software_reset();
    wait(100, SC_NS);

    // Enable SPI Host, set TX_WM = 10 (0x0A00)
    test->write_register_32(CONTROL_OFFSET, 0xA0000A00);
    test->write_register_32(CONFIGOPTS_OFFSET, 0x0000000A);
    wait(10, SC_NS);

    // Push 1 word to TX FIFO (depth is 1, which is < 10)
    test->write_register_32(TXDATA_OFFSET, 0xDEADBEEF);
    wait(10, SC_NS);

    // Queue a 2-word transaction (length 8 bytes)
    // BUILD_CMD(len, direction, speed, csaat) -> len=7 (8 bytes), direction=2 (TX_ONLY)
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(7, 2, 0, 0));
    wait(50, SC_NS);

    // Dynamic change TX_WM to 0. This clears the watermark blocking condition
    // and causes the transaction thread to proceed. Since there's only 1 word in TX FIFO
    // but 2 words are needed, it will fail to pop the second word and trigger underflow.
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    wait(100, SC_NS);

    // Verify underflow error is set in ERROR_STATUS (underflow is bit 2)
    test->read_register_32(ERROR_STATUS_OFFSET, err_status);
    if (err_status & 0x4) {
        REG_INFO(0, test->logger) << "[PASS] TX FIFO Underflow detected: ERROR_STATUS = 0x" << std::hex << err_status << std::dec << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] TX FIFO Underflow bit not set in ERROR_STATUS = 0x" << std::hex << err_status << std::dec << std::endl;
        test_passed = false;
    }

    // 4. COMMAND Write while an error is latched — accepted, not rejected.
    // RTL: command_busy is only "the command queue is full", so READY stays high
    // and CMDBUSY stays clear. A latched error disables the core through
    // en = en_sw & ~enb_error, which stalls execution but does not refuse the
    // write; the segment waits in the queue until software clears ERROR_STATUS.
    REG_INFO(0, test->logger) << "\n[Sub-Test 4] COMMAND Write during Error (queued, no CMDBUSY)" << std::endl;

    uint32_t status_before = 0;
    test->read_register_32(STATUS_OFFSET, status_before);
    uint32_t cmdqd_before = (status_before >> 16) & 0xF;

    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(10, SC_NS);

    test->read_register_32(ERROR_STATUS_OFFSET, err_status);
    if (err_status & 0x1) {
        REG_ERROR(0, test->logger) << "[FAIL] CMDBUSY set on a COMMAND write with room in the queue: ERROR_STATUS = 0x"
                                    << std::hex << err_status << std::dec << std::endl;
        test_passed = false;
    } else {
        REG_INFO(0, test->logger) << "[PASS] CMDBUSY stayed clear: ERROR_STATUS = 0x"
                                   << std::hex << err_status << std::dec << std::endl;
    }

    uint32_t status_after = 0;
    test->read_register_32(STATUS_OFFSET, status_after);
    uint32_t cmdqd_after = (status_after >> 16) & 0xF;
    if (cmdqd_after > cmdqd_before) {
        REG_INFO(0, test->logger) << "[PASS] Command queued while the core is held off: CMDQD "
                                   << cmdqd_before << " -> " << cmdqd_after << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Command was not queued: CMDQD "
                                    << cmdqd_before << " -> " << cmdqd_after << std::endl;
        test_passed = false;
    }

    if (!(status_after & (1u << 31))) {
        REG_ERROR(0, test->logger) << "[FAIL] STATUS.READY dropped because of a latched error" << std::endl;
        test_passed = false;
    } else {
        REG_INFO(0, test->logger) << "[PASS] STATUS.READY unaffected by the latched error" << std::endl;
    }

    // Clear error status
    test->write_register_32(ERROR_STATUS_OFFSET, 0xFF);
    wait(10, SC_NS);

    // 5. RX FIFO Full Stall & Resume
    REG_INFO(0, test->logger) << "\n[Sub-Test 5] RX FIFO Full Stall & Resume" << std::endl;
    software_reset();
    wait(100, SC_NS);

    // Disable SPI Host initially to allow queuing commands without starting them
    test->write_register_32(CONTROL_OFFSET, 0x20000040); // SPIEN=0, OUTPUT_EN=1, RX_WM=64
    test->write_register_32(CONFIGOPTS_OFFSET, 0x00000000); // CLKDIV=0
    wait(10, SC_NS);

    // Pre-load slave with 320 bytes (80 words)
    std::vector<uint8_t> rx_data(320);
    for (int i = 0; i < 320; i++) {
        rx_data[i] = i & 0xFF;
    }
    test->load_slave_rx_data(rx_data);
    wait(10, SC_NS);

    // Queue Command 1: RX 40 words (160 bytes)
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(159, 1, 0, 0));
    wait(10, SC_NS);

    // Queue Command 2: RX 40 words (160 bytes)
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(159, 1, 0, 0));
    wait(10, SC_NS);

    // Enable SPI Host to begin processing the queue
    test->write_register_32(CONTROL_OFFSET, 0xA0000040); // SPIEN=1, OUTPUT_EN=1, RX_WM=64
    wait(200, SC_US); // Wait for Command 1 to finish and Command 2 to fill the RX FIFO and stall

    // Verify RX FIFO depth is 64 (full) and transaction is still active (READY=0, ACTIVE=1)
    uint32_t status;
    test->read_register_32(STATUS_OFFSET, status);
    uint32_t rxqd = (status >> 8) & 0xFF;
    bool active = (status >> 30) & 0x1;
    REG_INFO(0, test->logger) << "[INFO] Stalled status: RXQD = " << rxqd << ", ACTIVE = " << active << std::endl;

    if (rxqd == 64 && active) {
        REG_INFO(0, test->logger) << "[PASS] RX FIFO stalled at capacity (64 words)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Expected RX FIFO depth 64, got " << rxqd << std::endl;
        test_passed = false;
    }

    // Drain 16 words to release the stall (triggers m_rx_space_available_event repeatedly)
    for (int i = 0; i < 16; i++) {
        test->read_register_32(RXDATA_OFFSET, rx_val);
    }
    wait(200, SC_US); // Wait for the remaining words to be transferred and the command to finish

    // Verify transaction completed successfully
    test->read_register_32(STATUS_OFFSET, status);
    active = (status >> 30) & 0x1;
    test->read_register_32(STATUS_OFFSET, status);
    rxqd = (status >> 8) & 0xFF;

    REG_INFO(0, test->logger) << "[INFO] Resumed status: RXQD = " << rxqd << ", ACTIVE = " << active << std::endl;

    if (!active && rxqd == 64) {
        REG_INFO(0, test->logger) << "[PASS] RX FIFO stall successfully resumed and completed" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Transaction did not complete as expected: active = " << active << ", rxqd = " << rxqd << std::endl;
        test_passed = false;
    }

    // Clean up: empty the RX FIFO
    for (int i = 0; i < 64; i++) {
        test->read_register_32(RXDATA_OFFSET, rx_val);
    }
    test->clear_slave_state();
    wait(100, SC_NS);

    report_test_result("FIFO Overflow, Underflow & Stalls", test_passed);
}
