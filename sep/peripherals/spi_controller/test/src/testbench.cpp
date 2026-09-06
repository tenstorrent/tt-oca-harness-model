// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include <iomanip>

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

/// =============================================================================
/// Testbench Constructor
/// =============================================================================
testbench::testbench(sc_module_name name)
    : sc_module(name),
      m_tx_depth("TxDepth", 72),
      m_rx_depth("RxDepth", 64),
      m_byte_order("ByteOrder", true),
      m_tests_run(0),
      m_tests_passed(0),
      m_tests_failed(0)
{
    // Initialize logger format (verbosity synced from DUT after DUT construction)
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Instantiate DUT - parameters resolved via regmodel::Param (CCI preset or compile-time defaults)
    dut = new spi_controller_ip("spi_controller_dut");

    // Sync all loggers with DUT verbosity (CCI preset may have overridden the build default)
    int resolved_verbosity = dut->verbosity.get_param_value();
    logger.setMaxVerbosity(resolved_verbosity);

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "SPI CONTROLLER Testbench Configuration" << std::endl
                         << "========================================" << std::endl
                         << "NumCS:      " << dut->get_num_cs() << std::endl
                         << "TxDepth:    " << m_tx_depth.get_param_value() << " words (" << (m_tx_depth.get_param_value() * 4) << " bytes)" << std::endl
                         << "RxDepth:    " << m_rx_depth.get_param_value() << " words (" << (m_rx_depth.get_param_value() * 4) << " bytes)" << std::endl
                         << "ByteOrder:  " << (m_byte_order.get_param_value() ? "Little-Endian" : "Big-Endian") << std::endl
                         << "CmdDepth:   " << dut->get_cmd_depth() << std::endl
                         << "========================================\n" << std::endl;

    // Instantiate Test Model
    test = new spi_controller_test("spi_controller_test");

    // Sync test module logger and spi_if_dummy logger
    test->logger.setMaxVerbosity(resolved_verbosity);
    test->m_spi_if_impl.logger.setMaxVerbosity(resolved_verbosity);

    // Bind all ports
    bind_ports();

    // Register test execution thread
    SC_THREAD(run_tests);
}

/// =============================================================================
/// Testbench Destructor
/// =============================================================================
testbench::~testbench()
{
    delete dut;
    delete test;
}

/// =============================================================================
/// Port Binding Method
/// =============================================================================
void testbench::bind_ports()
{
    // =========================================================================
    // 1. Register Bus Interface (TLM Target Socket)
    // =========================================================================
    test->initiator_socket.bind(dut->target_socket);

    // =========================================================================
    // 2. SPI Master Interface
    // =========================================================================
    // DUT has sc_port<spi_if>, test has sc_export<spi_if>
    // Binding: dut.port(test.export)
    dut->spi_master(test->spi_master);



    // =========================================================================
    // 4. Interrupt Outputs
    // =========================================================================
    dut->irq_o(sig_irq);
    test->irq_o(sig_irq);

    dut->error_irq(sig_error_irq);
    test->error_irq(sig_error_irq);

    dut->spi_event_irq(sig_spi_event_irq);
    test->spi_event_irq(sig_spi_event_irq);

    // =========================================================================
    // 5. DMA Trigger Output
    // =========================================================================
    dut->dma_trigger(sig_dma_trigger);
    test->dma_trigger(sig_dma_trigger);

    // =========================================================================
    // 6. Clock Interface
    // =========================================================================
    test->clk_i(sig_clk_i);
    dut->clk_i(sig_clk_i);

    // =========================================================================
    // 7. Reset Interface
    // =========================================================================
    test->rst_ni(sig_rst_ni);
    dut->rst_ni(sig_rst_ni);
}

/// =============================================================================
/// Main Test Thread - Runs All Tests
/// =============================================================================
void testbench::run_tests()
{
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "SPI CONTROLLER Testbench Starting" << std::endl
                         << "========================================" << std::endl;

    // Initialize test environment
    initialize_environment();

    // Run test cases
    REG_INFO(1, logger) << "\n[TESTBENCH] Running test cases..." << std::endl
                         << "\n========================================" << std::endl
                         << "Starting Top 10 Critical Functional Tests" << std::endl
                         << "========================================\n" << std::endl;

    test_func000_comprehensive_reset();
    test_func001_flash_fast_read_sequence();
    test_func002_speed_mode_validation();
    test_func003_fifo_stall_conditions();
    test_func004_interrupt_driven_txrx();
    test_func005_error_recovery_flow();
    test_func006_multi_device_switching();
    test_func007_multi_segment_csaat();
    // test_func008_passthrough_mode();  // REMOVED: Passthrough functionality removed
    test_func008_control_flow();
    test_func009_command_queue_depth();
    test_func010_dma_trigger();

    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Starting Code Coverage Targeted Tests" << std::endl
                         << "========================================\n" << std::endl;

    test_coverage_big_endian_byte_order();
    test_coverage_intr_test_edge_cases();
    test_coverage_event_enable_immediate_trigger();
    test_coverage_spien_reenable_queued_commands();
    test_coverage_reset_with_queued_commands();
    test_coverage_signal_update_during_reset();
    test_coverage_fifo_overflow_underflow();

    // Regression reproduction for the OT-SPI second-read TX-command drop.
    test_repro_second_read_tx_drop();


    // Print final test summary
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "       TEST SUITE SUMMARY" << std::endl
                         << "========================================" << std::endl
                         << "Total Tests:  " << m_tests_run << std::endl
                         << "Passed:       " << m_tests_passed << " (PASS)" << std::endl
                         << "Failed:       " << m_tests_failed << " (FAIL)" << std::endl;

    if (m_tests_run > 0) {
        double success_rate = (100.0 * m_tests_passed) / m_tests_run;
        REG_INFO(1, logger) << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
    }

    REG_INFO(1, logger) << "========================================" << std::endl;

    if (m_tests_failed > 0) {
        REG_ERROR(1, logger) << "[OVERALL RESULT: FAILED - " << m_tests_failed << " test(s) failed]" << std::endl;
    } else if (m_tests_passed > 0) {
        REG_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]" << std::endl;
    } else {
        REG_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]" << std::endl;
    }

    REG_INFO(1, logger) << "========================================\n" << std::endl;

    // Stop simulation
    sc_stop();
    wait(SC_ZERO_TIME);
}

/// =============================================================================
/// Initialize Test Environment
/// =============================================================================
void testbench::initialize_environment()
{
    REG_INFO(1, logger) << "[TESTBENCH] Initializing test environment..." << std::endl;

    // Configure clock
    configure_clock();

    // Apply reset
    apply_reset();

    REG_INFO(1, logger) << "[TESTBENCH] Initialization complete\n" << std::endl;
}

/// =============================================================================
/// Configure Clock Signal
/// =============================================================================
void testbench::configure_clock()
{
    // Set functional clock frequency (e.g., 50 MHz)
    double clk_freq_hz = 100e6;  /// 50 MHz
    test->clk_i.write(1);
    REG_INFO(1, logger) << "[TESTBENCH] Clock configured: " << (clk_freq_hz / 1e6) << " MHz" << std::endl;
}

/// =============================================================================
/// Apply Reset Sequence
/// =============================================================================
void testbench::apply_reset()
{
    REG_INFO(1, logger) << "[TESTBENCH] Applying reset..." << std::endl;

    // Assert reset (active-low)
    test->rst_ni.write(false);
    wait(100, SC_NS);

    // De-assert reset
    test->rst_ni.write(true);
    wait(100, SC_NS);

    REG_INFO(1, logger) << "[TESTBENCH] Reset complete" << std::endl;
}

/// =============================================================================
/// Test Result Reporting Helper
/// =============================================================================
void testbench::report_test_result(const char* test_name, bool passed)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "\n========================================" << std::endl
                             << "[*** TEST PASSED ***] " << test_name << std::endl
                             << "========================================\n" << std::endl;
    } else {
        m_tests_failed++;
        REG_ERROR(1, logger) << "\n========================================" << std::endl
                              << "[XXX TEST FAILED XXX] " << test_name << std::endl
                              << "========================================\n" << std::endl;

        REG_ERROR(1, logger) << "\nError: TEST_FAILURE: " << test_name << std::endl
                              << "In file: test/src/testbench.cpp:" << __LINE__ << std::endl
                              << "In process: " << sc_core::sc_get_current_process_handle().name()
                              << " @ " << sc_time_stamp() << std::endl;

        // Stop simulation on first failure
        sc_stop();
        // CCI mode: Exit immediately to avoid CCI cleanup bug
        wait(SC_ZERO_TIME);  /// Allow sc_stop() to take effect
#ifdef __COVERAGE__
        __gcov_dump();  /// Flush coverage data before quick_exit
#endif
        std::quick_exit(1);  /// Exit with failure code, bypassing destructors
    }
}

/// =============================================================================
/// Common Helper Functions for Tests
/// =============================================================================

/**
 * @brief Software Reset - clears all FIFOs and resets FSM
 */
void testbench::software_reset()
{
    test->write_register_32(spi_controller_regs::CTRL_OFFSET, 0x40000000);  /// Set SW_RST bit (bit 30)
    wait(50, SC_NS);  /// Allow reset to complete
}

/**
 * @brief Basic SPI Controller Configuration
 */
void testbench::configure_spi_controller_basic(uint32_t clkdiv, uint32_t csid)
{
    test->write_register_32(spi_controller_regs::CTRL_OFFSET, 0xE0000000);  /// SPIEN=1, OUTPUT_EN=1
    test->write_register_32(spi_controller_regs::CFG_OFFSET, clkdiv);
    test->write_register_32(spi_controller_regs::CSID_OFFSET, csid);
    wait(10, SC_NS);
}

/**
 * @brief Quiet the interrupt lines by removing their causes
 *
 * INTR_STATUS is read-only and tracks its inputs live, so it cannot be written
 * clear. Drop the software-forced sources instead: INTR_TEST and any latched
 * ERROR_STATUS bit. A level event that is still true (say TXEMPTY while idle)
 * keeps INTR_STATUS.spi_event asserted — mask EVENT_ENABLE to silence those.
 */
void testbench::clear_interrupts()
{
    test->write_register_32(spi_controller_regs::INTR_TEST_OFFSET, 0x0);
    clear_errors();
    wait(10, SC_NS);
}

/**
 * @brief Clear all error flags
 */
void testbench::clear_errors()
{
    uint32_t status_val;
    test->read_register_32(spi_controller_regs::ERROR_STATUS_OFFSET, status_val);
    if (status_val != 0) {
        test->write_register_32(spi_controller_regs::ERROR_STATUS_OFFSET, status_val);  /// W1C
        wait(10, SC_NS);
    }
}

/**
 * @brief Check if STATUS.READY is asserted
 */
bool testbench::check_ready(const char* context)
{
    uint32_t status_val;
    test->read_register_32(spi_controller_regs::STATUS_OFFSET, status_val);
    bool ready = (status_val >> 31) & 0x1;
    if (!ready) {
        REG_WARN(1, logger) << "  [FAIL] STATUS.READY=0 during " << context << std::endl;
    }
    return ready;
}

/**
 * @brief Load TX FIFO with data
 */
void testbench::load_tx_fifo(int num_words, uint32_t base_value)
{
    for (int i = 0; i < num_words; i++) {
        test->write_register_32(spi_controller_regs::TXDATA_OFFSET, base_value + i);
    }
    wait(10, SC_NS);
}

/**
 * @brief Drain RX FIFO by reading
 */
void testbench::drain_rx_fifo(int num_words)
{
    uint32_t dummy;
    for (int i = 0; i < num_words; i++) {
        test->read_register_32(spi_controller_regs::RXDATA_OFFSET, dummy);
        wait(5, SC_NS);
    }
}

/**
 * @brief Print test summary
 */
void testbench::print_test_summary(int passed, int failed)
{
    REG_INFO(1, logger) << "\n========================================" << std::endl
                         << "Sub-tests Passed: " << passed << std::endl
                         << "Sub-tests Failed: " << failed << std::endl
                         << "========================================\n" << std::endl;
}

/**
 * @brief Wait for transaction to complete (ACTIVE=0, READY=1)
 */
bool testbench::wait_for_transaction_complete(uint32_t timeout_us)
{
    uint32_t elapsed_us = 0;
    uint32_t status_val;

    while (elapsed_us < timeout_us) {
        test->read_register_32(spi_controller_regs::STATUS_OFFSET, status_val);
        bool ready = (status_val >> 31) & 0x1;
        bool active = (status_val >> 30) & 0x1;

        if (ready && !active) {
            return true;  /// Transaction complete
        }

        wait(10, SC_US);
        elapsed_us += 10;
    }

    return false;  /// Timeout
}

/// =============================================================================
/// sc_main - Entry Point for SystemC Simulation
/// =============================================================================
int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    // regmodel::load_config_file() registers the global CCI broker and, if a filename
    // is provided, applies preset parameter values from the INI file.
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");
    sc_start();

    // WORKAROUND: CCI library cleanup bug causes segfault on normal exit.
    // Use quick_exit to bypass destructors and avoid crash.
#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(0);
    return 0;
}
