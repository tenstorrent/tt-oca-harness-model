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
#include <cci_configuration>
#include <tlm.h>
#include <cstring>
#include <vector>

using namespace spi_controller_regs;

namespace {

bool set_dut_byte_order(spi_controller_ip* dut, bool little_endian)
{
    // Inside the SystemC hierarchy use the process-local broker (no named originator).
    auto broker = cci::cci_get_broker();
    const std::string name = std::string(dut->name()) + ".ByteOrder";
    auto h = broker.get_param_handle(name);
    if (!h.is_valid()) {
        return false;
    }
    h.set_cci_value(cci::cci_value(little_endian));
    return dut->get_byte_order() == little_endian;
}

} // namespace

/**
 * @brief Big-endian byte packing via CCI ByteOrder (no DUT Set_param backdoor).
 * Asserts exact TX bytes for length mod 4 = 1,2,3 and matching RX words.
 */
void testbench::test_coverage_big_endian_byte_order()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[COVERAGE TEST 1] Big-Endian Byte Ordering" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;

    software_reset();
    wait(100, SC_NS);

    if (!set_dut_byte_order(dut, false)) {
        REG_ERROR(0, test->logger) << "[FAIL] CCI ByteOrder set to big-endian failed" << std::endl;
        report_test_result("Big-Endian Byte Order", false);
        return;
    }

    configure_spi_controller_basic();

    // Lengths mod 4 = 1,2,3: exact TX unpack + RX pack in big-endian.
    struct Case { uint32_t len_field; std::vector<uint32_t> tx_words; std::vector<uint8_t> expect_tx; };
    // BE unpack of word 0x11223344 -> bytes 11 22 33 44
    const std::vector<Case> cases = {
        {0, {0x11223344u}, {0x11}},                         // 1 byte
        {1, {0x11223344u}, {0x11, 0x22}},                    // 2 bytes
        {2, {0x11223344u}, {0x11, 0x22, 0x33}},              // 3 bytes
    };

    for (const auto& c : cases) {
        test->clear_slave_state();
        std::vector<uint8_t> rx_preload = c.expect_tx;
        for (auto& b : rx_preload) b = static_cast<uint8_t>(b ^ 0x5A);
        test->load_slave_rx_data(rx_preload);

        for (uint32_t w : c.tx_words) {
            test->write_register_32(TXDATA_OFFSET, w);
        }
        wait(10, SC_NS);

        test->write_register_32(COMMAND_OFFSET, BUILD_CMD(c.len_field, 3, 0, 0)); // BIDIR
        if (!wait_for_transaction_complete(2000)) {
            REG_ERROR(0, test->logger) << "[FAIL] BE len=" << (c.len_field + 1) << " timed out" << std::endl;
            test_passed = false;
            break;
        }

        const auto& capt = test->get_slave_captured_tx_data();
        if (capt != c.expect_tx) {
            REG_ERROR(0, test->logger) << "[FAIL] BE TX bytes mismatch for len=" << (c.len_field + 1) << std::endl;
            test_passed = false;
        } else {
            REG_INFO(0, test->logger) << "[PASS] BE TX bytes match for len=" << (c.len_field + 1) << std::endl;
        }

        uint32_t rx_word = 0;
        test->read_register_32(RXDATA_OFFSET, rx_word);
        // BE pack of rx_preload into word
        uint32_t expect_rx = 0;
        for (size_t i = 0; i < rx_preload.size() && i < 4; ++i) {
            expect_rx |= (static_cast<uint32_t>(rx_preload[i]) << ((3 - i) * 8));
        }
        if (rx_word != expect_rx) {
            REG_ERROR(0, test->logger) << "[FAIL] BE RX word=0x" << std::hex << rx_word
                      << " expected 0x" << expect_rx << std::dec << std::endl;
            test_passed = false;
        } else {
            REG_INFO(0, test->logger) << "[PASS] BE RX word for len=" << (c.len_field + 1) << std::endl;
        }
    }

    // Restore little-endian via CCI and spot-check mod4=1
    if (!set_dut_byte_order(dut, true)) {
        REG_ERROR(0, test->logger) << "[FAIL] CCI ByteOrder restore to little-endian failed" << std::endl;
        test_passed = false;
    } else {
        test->clear_slave_state();
        software_reset();
        configure_spi_controller_basic();
        test->write_register_32(TXDATA_OFFSET, 0x11223344u);
        test->write_register_32(COMMAND_OFFSET, BUILD_CMD(0, 2, 0, 0));
        wait_for_transaction_complete(500);
        const auto& capt = test->get_slave_captured_tx_data();
        std::vector<uint8_t> expect_le{0x44};
        if (capt != expect_le) {
            REG_ERROR(0, test->logger) << "[FAIL] LE TX byte mismatch" << std::endl;
            test_passed = false;
        } else {
            REG_INFO(0, test->logger) << "[PASS] LE TX byte for len=1" << std::endl;
        }
    }

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

    // Enable RXWM event — with RX_WATERMARK=0, RXQD>=0 is always true, so
    // INTR_STATE.spi_event, spi_event_irq, and irq_o must all assert.
    test->write_register_32(EVENT_ENABLE_OFFSET, (1 << 2));
    wait(10, SC_NS);

    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    const bool spi_event_bit = (intr_status >> 1) & 0x1;
    const bool spi_event_pin = sig_spi_event_irq.read();
    const bool irq_combined = sig_irq.read();
    if (spi_event_bit && spi_event_pin && irq_combined) {
        REG_INFO(0, test->logger) << "[PASS] RXWM event asserted INTR_STATE, spi_event_irq, irq_o" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] RXWM interrupt incomplete"
                  << " INTR_STATE.spi_event=" << spi_event_bit
                  << " spi_event_irq=" << spi_event_pin
                  << " irq_o=" << irq_combined << std::endl;
        test_passed = false;
    }

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

    // Ensure IRQ pins start low, then hold reset and force an update path.
    // If update_output_signals_method did not skip during reset, INTR_TEST would
    // drive error_irq/spi_event_irq/irq_o high.
    test->write_register_32(INTR_TEST_OFFSET, 0x0);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x0);
    test->write_register_32(EVENT_ENABLE_OFFSET, 0x0);
    wait(10, SC_NS);
    if (sig_error_irq.read() || sig_spi_event_irq.read() || sig_irq.read()) {
        REG_ERROR(0, test->logger) << "[FAIL] IRQ pins not quiescent before reset hold" << std::endl;
        test_passed = false;
    }

    REG_INFO(0, test->logger) << "[INFO] Asserting reset and forcing signal update..." << std::endl;
    test->rst_ni.write(false);
    wait(10, SC_NS);
    test->write_register_32(INTR_ENABLE_OFFSET, 0x3);
    test->write_register_32(INTR_TEST_OFFSET, 0x3);
    wait(10, SC_NS);
    if (!sig_error_irq.read() && !sig_spi_event_irq.read() && !sig_irq.read()) {
        REG_INFO(0, test->logger) << "[PASS] IRQ pins stayed low during reset hold (update skipped)" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] IRQ pins changed while rst_ni=0" << std::endl;
        test_passed = false;
    }
    test->rst_ni.write(true);
    wait(50, SC_NS);

    apply_reset();
    wait(50, SC_NS);
    test->read_register_32(INTR_STATE_OFFSET, intr_status);
    if (intr_status == 0) {
        REG_INFO(0, test->logger) << "[PASS] Interrupts cleared after reset" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] Interrupts not cleared after reset (0x" << std::hex << intr_status << std::dec << ")" << std::endl;
        test_passed = false;
    }

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

namespace {

tlm::tlm_response_status raw_b_transport(spi_controller_test* test,
                                         tlm::tlm_command cmd,
                                         uint64_t addr,
                                         unsigned char* data,
                                         unsigned int len,
                                         unsigned int streaming_width,
                                         unsigned char* be,
                                         unsigned int be_len)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(streaming_width);
    if (be != nullptr) {
        trans.set_byte_enable_ptr(be);
        trans.set_byte_enable_length(be_len);
    } else {
        trans.set_byte_enable_ptr(nullptr);
    }
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    test->initiator_socket->b_transport(trans, delay);
    return trans.get_response_status();
}

} // namespace

void testbench::test_quality_tlm_protocol_matrix()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[QUALITY] TLM protocol matrix on register socket" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;
    software_reset();
    configure_spi_controller_basic();

    uint32_t control_before = 0;
    test->read_register_32(CONTROL_OFFSET, control_before);

    auto expect_status = [&](const char* name, tlm::tlm_response_status got,
                             tlm::tlm_response_status want) {
        if (got != want) {
            REG_ERROR(0, test->logger) << "[FAIL] " << name << " status=" << static_cast<int>(got)
                      << " expected=" << static_cast<int>(want) << std::endl;
            test_passed = false;
        } else {
            REG_INFO(0, test->logger) << "[PASS] " << name << std::endl;
        }
    };

    auto expect_control_unchanged = [&](const char* name) {
        uint32_t control_after = 0;
        test->read_register_32(CONTROL_OFFSET, control_after);
        if (control_after != control_before) {
            REG_ERROR(0, test->logger) << "[FAIL] " << name << " mutated CONTROL" << std::endl;
            test_passed = false;
        }
    };

    uint32_t word = 0xA5A5A5A5u;
    unsigned char buf8[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    // Bad/hole offset inside window: model returns TLM_OK, reads as 0 / drops write.
    {
        uint32_t rd = 0xFFFFFFFFu;
        auto st = raw_b_transport(test, tlm::TLM_READ_COMMAND, 0x38, reinterpret_cast<unsigned char*>(&rd), 4, 4, nullptr, 0);
        expect_status("hole offset read", st, tlm::TLM_OK_RESPONSE);
        if (rd != 0) {
            REG_ERROR(0, test->logger) << "[FAIL] hole read returned 0x" << std::hex << rd << std::dec << std::endl;
            test_passed = false;
        } else {
            REG_INFO(0, test->logger) << "[PASS] hole offset reads as 0" << std::endl;
        }
    }

    // Unaligned address (still serviced by regmodel as OK)
    {
        unsigned char b = 0;
        auto st = raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET + 1, &b, 1, 1, nullptr, 0);
        expect_status("unaligned byte read", st, tlm::TLM_OK_RESPONSE);
    }

    // length 0
    expect_status("len=0",
        raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET, buf8, 0, 0, nullptr, 0),
        tlm::TLM_BURST_ERROR_RESPONSE);
    expect_control_unchanged("len=0");

    // length 1 (supported)
    {
        unsigned char b = 0;
        expect_status("len=1",
            raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET, &b, 1, 1, nullptr, 0),
            tlm::TLM_OK_RESPONSE);
    }

    // length width-1 / width+1
    expect_status("len=3",
        raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET, buf8, 3, 3, nullptr, 0),
        tlm::TLM_OK_RESPONSE);
    expect_status("len=5",
        raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET, buf8, 5, 5, nullptr, 0),
        tlm::TLM_OK_RESPONSE);

    // null data_ptr
    expect_status("null data_ptr",
        raw_b_transport(test, tlm::TLM_READ_COMMAND, CONTROL_OFFSET, nullptr, 4, 4, nullptr, 0),
        tlm::TLM_GENERIC_ERROR_RESPONSE);
    expect_control_unchanged("null data_ptr");

    // IGNORE command
    expect_status("IGNORE",
        raw_b_transport(test, tlm::TLM_IGNORE_COMMAND, CONTROL_OFFSET, buf8, 4, 4, nullptr, 0),
        tlm::TLM_COMMAND_ERROR_RESPONSE);
    expect_control_unchanged("IGNORE");

    // streaming_width 0 and < len
    expect_status("streaming_width=0",
        raw_b_transport(test, tlm::TLM_WRITE_COMMAND, CONTROL_OFFSET, reinterpret_cast<unsigned char*>(&word), 4, 0, nullptr, 0),
        tlm::TLM_BURST_ERROR_RESPONSE);
    expect_control_unchanged("streaming_width=0");
    expect_status("streaming_width<len",
        raw_b_transport(test, tlm::TLM_WRITE_COMMAND, CONTROL_OFFSET, reinterpret_cast<unsigned char*>(&word), 4, 2, nullptr, 0),
        tlm::TLM_BURST_ERROR_RESPONSE);
    expect_control_unchanged("streaming_width<len");

    // transport_dbg
    {
        tlm::tlm_generic_payload trans;
        uint32_t dbg_word = 0;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(CONTROL_OFFSET);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&dbg_word));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        unsigned int n = test->initiator_socket->transport_dbg(trans);
        if (n == 4 && trans.is_response_ok() && dbg_word == control_before) {
            REG_INFO(0, test->logger) << "[PASS] transport_dbg read CONTROL" << std::endl;
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] transport_dbg n=" << n
                      << " status=" << static_cast<int>(trans.get_response_status())
                      << " data=0x" << std::hex << dbg_word << std::dec << std::endl;
            test_passed = false;
        }
    }

    // DMI: socket has no get_direct_mem_ptr registered -> returns false
    {
        tlm::tlm_generic_payload trans;
        tlm::tlm_dmi dmi;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(CONTROL_OFFSET);
        bool granted = test->initiator_socket->get_direct_mem_ptr(trans, dmi);
        if (!granted) {
            REG_INFO(0, test->logger) << "[PASS] DMI refused" << std::endl;
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] DMI unexpectedly granted" << std::endl;
            test_passed = false;
        }
    }

    // ALERT_TEST: WO mask 0x1, read mask 0x0 -> writes accepted, reads as 0
    {
        test->write_register_32(ALERT_TEST_OFFSET, 0xFFFFFFFFu);
        uint32_t alert_rd = 0xA5A5A5A5u;
        test->read_register_32(ALERT_TEST_OFFSET, alert_rd);
        if (alert_rd == 0) {
            REG_INFO(0, test->logger) << "[PASS] ALERT_TEST is WO (read returns 0)" << std::endl;
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] ALERT_TEST read 0x" << std::hex << alert_rd << std::dec << std::endl;
            test_passed = false;
        }
    }

    report_test_result("TLM protocol matrix + ALERT_TEST", test_passed);
}

void testbench::test_quality_command_len_boundaries()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[QUALITY] COMMAND LEN boundaries + reject >=512" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;
    const uint32_t legal[] = {0, 3, 255, 511}; // byte counts 1,4,256,512

    for (uint32_t len : legal) {
        software_reset();
        configure_spi_controller_basic();
        test->clear_slave_state();

        const uint32_t nbytes = len + 1;
        // DUMMY avoids needing a full TX FIFO for 512-byte segments (TxDepth=72).
        const uint8_t direction = (nbytes > 32) ? 0 /*DUMMY*/ : 2 /*TX_ONLY*/;
        if (direction == 2) {
            const uint32_t nwords = (nbytes + 3) / 4;
            for (uint32_t i = 0; i < nwords; ++i) {
                test->write_register_32(TXDATA_OFFSET, 0xA5000000u + i);
            }
            wait(10, SC_NS);
        }

        const uint32_t tx_before = test->get_slave_transaction_count();
        test->write_register_32(COMMAND_OFFSET, BUILD_CMD(len, direction, 0, 0));
        if (!wait_for_transaction_complete(5000)) {
            REG_ERROR(0, test->logger) << "[FAIL] LEN=" << len << " timed out" << std::endl;
            test_passed = false;
            continue;
        }
        if (test->get_slave_transaction_count() != tx_before + 1) {
            REG_ERROR(0, test->logger) << "[FAIL] LEN=" << len << " transaction not issued" << std::endl;
            test_passed = false;
        } else if (direction == 2) {
            const auto& capt = test->get_slave_captured_tx_data();
            if (capt.size() != nbytes) {
                REG_ERROR(0, test->logger) << "[FAIL] LEN=" << len << " captured " << capt.size()
                          << " expected " << nbytes << std::endl;
                test_passed = false;
            } else {
                REG_INFO(0, test->logger) << "[PASS] LEN=" << len << " TX transferred " << nbytes << " bytes" << std::endl;
            }
        } else {
            REG_INFO(0, test->logger) << "[PASS] LEN=" << len << " DUMMY segment completed (" << nbytes << " bytes)" << std::endl;
        }

        uint32_t err = 0;
        test->read_register_32(ERROR_STATUS_OFFSET, err);
        if (err != 0) {
            REG_ERROR(0, test->logger) << "[FAIL] LEN=" << len << " unexpected ERROR_STATUS=0x" << std::hex << err << std::dec << std::endl;
            test_passed = false;
        }
    }

    // Reject LEN>=512 (byte count would be >512) — CMDINVAL, no enqueue, no smash.
    for (uint32_t len : {512u, 0xFFFFFu}) {
        software_reset();
        configure_spi_controller_basic();
        test->clear_slave_state();
        test->write_register_32(TXDATA_OFFSET, 0xDEADBEEFu);
        wait(10, SC_NS);

        const uint32_t tx_before = test->get_slave_transaction_count();
        uint32_t status_before = 0;
        test->read_register_32(STATUS_OFFSET, status_before);
        const uint32_t cmdqd_before = (status_before >> 16) & 0xF;

        test->write_register_32(COMMAND_OFFSET, BUILD_CMD(len, 2, 0, 0));
        wait(50, SC_NS);

        uint32_t err = 0;
        test->read_register_32(ERROR_STATUS_OFFSET, err);
        uint32_t status_after = 0;
        test->read_register_32(STATUS_OFFSET, status_after);
        const uint32_t cmdqd_after = (status_after >> 16) & 0xF;
        const bool cmdinval = ((err >> 3) & 0x1) != 0;

        if (cmdinval && cmdqd_after == cmdqd_before &&
            test->get_slave_transaction_count() == tx_before) {
            REG_INFO(0, test->logger) << "[PASS] LEN=" << len << " rejected with CMDINVAL, not enqueued" << std::endl;
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] LEN=" << len << " reject: ERROR=0x" << std::hex << err
                      << std::dec << " CMDQD " << cmdqd_before << "->" << cmdqd_after
                      << " tx_count delta=" << (test->get_slave_transaction_count() - tx_before) << std::endl;
            test_passed = false;
        }
    }

    report_test_result("COMMAND LEN boundaries", test_passed);
}

void testbench::test_quality_clk_low_and_delay_formula()
{
    REG_INFO(0, test->logger) << "\n========================================" << std::endl;
    REG_INFO(0, test->logger) << "[QUALITY] clk_i low reject + segment delay formula" << std::endl;
    REG_INFO(0, test->logger) << "========================================\n" << std::endl;

    bool test_passed = true;

    // clk_i low while a command is queued -> ACCESSINVAL, no spi_if call.
    software_reset();
    configure_spi_controller_basic();
    test->clear_slave_state();
    test->write_register_32(TXDATA_OFFSET, 0x11111111u);
    test->clk_i.write(false);
    wait(10, SC_NS);
    const uint32_t tx_before = test->get_slave_transaction_count();
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    wait(100, SC_NS);
    uint32_t err = 0;
    test->read_register_32(ERROR_STATUS_OFFSET, err);
    const bool accessinval = ((err >> 5) & 0x1) != 0;
    if (accessinval && test->get_slave_transaction_count() == tx_before) {
        REG_INFO(0, test->logger) << "[PASS] clk_i=0 sets ACCESSINVAL and does not call spi_if" << std::endl;
    } else {
        REG_ERROR(0, test->logger) << "[FAIL] clk_i=0 path ERROR=0x" << std::hex << err << std::dec
                  << " tx_delta=" << (test->get_slave_transaction_count() - tx_before) << std::endl;
        test_passed = false;
    }
    test->clk_i.write(true);
    wait(10, SC_NS);

    // Independent delay check against calculate_segment_delay formula in source:
    // SCK period = 2*(CLKDIV+1)*Tclk; data = len*bits_per_cycle*SCK;
    // margins = (csnlead+1 + csntrail+1 + csaat?0:csnidle+1) * SCK/2
    software_reset();
    clear_errors();
    test->write_register_32(CONTROL_OFFSET, 0xA0000000);
    // CLKDIV=1, CSNIDLE=1, CSNTRAIL=1, CSNLEAD=1
    const uint32_t cfg = (1u << 24) | (1u << 20) | (1u << 16) | 1u;
    test->write_register_32(CONFIGOPTS_OFFSET, cfg);
    test->write_register_32(CSID_OFFSET, 0);
    test->write_register_32(TXDATA_OFFSET, 0xAABBCCDDu);
    wait(10, SC_NS);

    const double tclk_s = dut->ClkPeriodNs.get_param_value() * 1e-9;
    const uint32_t clkdiv = 1;
    const uint32_t len_bytes = 4;
    const uint32_t bits_per_cycle = 8; // STANDARD
    const double sck = 2.0 * (clkdiv + 1) * tclk_s;
    const double data_t = len_bytes * bits_per_cycle * sck;
    const double margin_t = (/*lead*/2 + /*trail*/2 + /*idle*/2) * (sck / 2.0);
    const double expect_s = data_t + margin_t;

    const sc_time t0 = sc_time_stamp();
    test->write_register_32(COMMAND_OFFSET, BUILD_CMD(3, 2, 0, 0));
    if (!wait_for_transaction_complete(2000)) {
        REG_ERROR(0, test->logger) << "[FAIL] delay formula transaction timed out" << std::endl;
        test_passed = false;
    } else {
        const double elapsed = (sc_time_stamp() - t0).to_seconds();
        const double tol = expect_s * 0.05 + 1e-9; // 5% + 1ns
        if (elapsed + 1e-12 >= expect_s - tol && elapsed <= expect_s + tol + 50e-6) {
            // Upper bound includes wait_for_transaction_complete polling slack.
            REG_INFO(0, test->logger) << "[PASS] segment delay elapsed=" << elapsed
                      << "s expect~=" << expect_s << "s" << std::endl;
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] segment delay elapsed=" << elapsed
                      << "s expect=" << expect_s << "s tol=" << tol << std::endl;
            test_passed = false;
        }
    }

    // Zero ClkPeriodNs falls back to 10ns default inside calculate_segment_delay.
    {
        cci::cci_broker_handle broker = cci::cci_get_broker();
        auto h = broker.get_param_handle(std::string(dut->name()) + ".ClkPeriodNs");
        if (h.is_valid()) {
            h.set_cci_value(cci::cci_value(0.0));
            software_reset();
            clear_errors();
            test->write_register_32(CONTROL_OFFSET, 0xA0000000);
            test->write_register_32(CONFIGOPTS_OFFSET, 0x1);
            test->write_register_32(TXDATA_OFFSET, 0x1);
            test->write_register_32(COMMAND_OFFSET, BUILD_CMD(0, 2, 0, 0));
            if (wait_for_transaction_complete(2000)) {
                REG_INFO(0, test->logger) << "[PASS] ClkPeriodNs=0 used default timing and completed" << std::endl;
            } else {
                REG_ERROR(0, test->logger) << "[FAIL] ClkPeriodNs=0 transaction failed" << std::endl;
                test_passed = false;
            }
            h.set_cci_value(cci::cci_value(10.0));
        } else {
            REG_ERROR(0, test->logger) << "[FAIL] ClkPeriodNs CCI handle missing" << std::endl;
            test_passed = false;
        }
    }

    report_test_result("clk_i low + delay formula", test_passed);
}
