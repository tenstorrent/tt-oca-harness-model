#pragma once

#include <systemc.h>
#include "spi_controller.h"
#include "spi_controller_test.h"
#include "csml_logger.h"
#include "csml_parameter.h"

/// =============================================================================
/// SPI Controller Register Offset Definitions (Common for all tests)
/// Updated to match RDL specification (spi_controller_ip.rdl)
/// =============================================================================
namespace spi_controller_regs {
    /// Interrupt Registers
    constexpr uint32_t INTR_STATUS_OFFSET    = 0x00;
    constexpr uint32_t INTR_ENABLE_OFFSET   = 0x04;
    constexpr uint32_t INTR_TEST_OFFSET     = 0x08;

    /// Control and Status Registers
    constexpr uint32_t CTRL_OFFSET          = 0x10;
    constexpr uint32_t STATUS_OFFSET        = 0x14;
    constexpr uint32_t CFG_OFFSET           = 0x18;
    constexpr uint32_t CSID_OFFSET          = 0x1C;

    /// Command and Data Registers
    constexpr uint32_t CMD_OFFSET           = 0x20;
    constexpr uint32_t RXDATA_OFFSET        = 0x24;
    constexpr uint32_t TXDATA_OFFSET        = 0x28;

    /// Error and Event Registers
    constexpr uint32_t ERROR_ENABLE_OFFSET  = 0x2C;
    constexpr uint32_t ERROR_STATUS_OFFSET  = 0x30;
    constexpr uint32_t EVENT_ENABLE_OFFSET  = 0x34;
}

/// =============================================================================
/// CMD Register Helper Macro (RDL Specification Compliant)
/// =============================================================================
/// Constructs CMD register value per RDL spec (spi_controller_ip.rdl):
///   [13:12] DIRECTION (0=Dummy, 1=Rx, 2=Tx, 3=Bidir)
///   [11:10] SPEED (0=Standard, 1=Dual, 2=Quad)
///   [9]     CSAAT (Chip Select Active After Transfer)
///   [8:0]   LEN (Segment length in bytes, 0-255)
///
/// Usage: BUILD_CMD(len, direction, speed, csaat)
/// Example: BUILD_CMD(3, 2, 0, 1) = 4-byte TX, Standard SPI, CSB held low
#define BUILD_CMD(len, direction, speed, csaat) \
    (((direction) << 12) | ((speed) << 10) | ((csaat) << 9) | (len))

/// =============================================================================
/// Testbench Module - Integrated Test Execution
/// =============================================================================
/// The testbench module instantiates the DUT (spi_controller_ip) and the test model
/// (spi_controller_test), connects all their ports, and contains all test execution logic
/// =============================================================================
/**
 * @class testbench
 * @brief Main testbench class that instantiates DUT and test model
 */
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    /// Module instances
    spi_controller_ip* dut;                /// Device Under Test
    spi_controller_test* test;          /// Test model

    testbench(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~testbench();

private:
    /// Signal declarations for connecting DUT and test ports

    /// Interrupt signals (DUT output -> Test input)
    sc_signal<bool> sig_error_irq;
    sc_signal<bool> sig_spi_event_irq;

    /// DMA trigger signal (DUT output -> Test input)
    sc_signal<bool> sig_dma_trigger;

    /// Clock signal (Test output -> DUT input)
    sc_signal<bool> sig_clk_i;

    /// Reset signal (Test output -> DUT input)
    sc_signal<bool> sig_rst_ni;

    /// Testbench configuration parameters (read from CCI broker, same values as DUT)
    csml_param<uint32_t> m_tx_depth;
    csml_param<uint32_t> m_rx_depth;
    csml_param<bool>     m_byte_order;

    /// Test result tracking
    uint32_t m_tests_run;
    uint32_t m_tests_passed;
public:
    uint32_t m_tests_failed;

    /// CSML Logger instance
    CsmlLogger logger;

    /**
     * @brief Helper method to bind all ports
     */
    void bind_ports();

    /**
     * @brief Test result reporting helper
     * @param test_name Name of the test
     * @param passed true if test passed, false otherwise
     */
    void report_test_result(const char* test_name, bool passed);

    /// =========================================================================
    /// Common Helper Functions for Tests
    /// =========================================================================

    /**
     * @brief Perform software reset
     */
    void software_reset();

    /**
     * @brief Configure SPI controller with basic settings
     * @param clkdiv Clock divider value
     * @param csid Chip select ID
     */
    void configure_spi_controller_basic(uint32_t clkdiv = 10, uint32_t csid = 0);

    /**
     * @brief Clear all interrupts
     */
    void clear_interrupts();

    /**
     * @brief Clear all error flags
     */
    void clear_errors();

    /**
     * @brief Check if the host is ready
     * @param context Context string for logging
     * @return true if ready, false otherwise
     */
    bool check_ready(const char* context = "operation");

    /**
     * @brief Load TX FIFO with test data
     * @param num_words Number of words to load
     * @param base_value Base value for test pattern
     */
    void load_tx_fifo(int num_words, uint32_t base_value = 0x11223344);

    /**
     * @brief Drain RX FIFO
     * @param num_words Number of words to read
     */
    void drain_rx_fifo(int num_words);

    /**
     * @brief Print test summary
     * @param passed Number of tests passed
     * @param failed Number of tests failed
     */
    void print_test_summary(int passed, int failed);

    /**
     * @brief Wait for transaction to complete
     * @param timeout_us Timeout in microseconds
     * @return true if completed, false if timed out
     */
    bool wait_for_transaction_complete(uint32_t timeout_us = 500);

    /// =========================================================================
    /// Test Execution Thread
    /// =========================================================================

    /**
     * @brief Main test execution thread
     */
    void run_tests();

    /**
     * @brief Initialize test environment
     */
    void initialize_environment();

    /**
     * @brief Apply reset to DUT
     */
    void apply_reset();

    /**
     * @brief Configure clock
     */
    void configure_clock();

    /// =========================================================================
    /// Test Functions - Comprehensive Functional Verification
    /// =========================================================================

    /**
     * @brief Test comprehensive reset functionality
     */
    void test_func000_comprehensive_reset();

    /**
     * @brief Test flash fast read sequence
     */
    void test_func001_flash_fast_read_sequence();

    /**
     * @brief Test speed mode validation
     */
    void test_func002_speed_mode_validation();

    /**
     * @brief Test FIFO stall conditions
     */
    void test_func003_fifo_stall_conditions();

    /**
     * @brief Test interrupt driven TX/RX
     */
    void test_func004_interrupt_driven_txrx();

    /**
     * @brief Test error recovery flow
     */
    void test_func005_error_recovery_flow();

    /**
     * @brief Test multi-device switching
     */
    void test_func006_multi_device_switching();

    /**
     * @brief Test multi-segment CSAAT
     */
    void test_func007_multi_segment_csaat();

    // REMOVED: test_func008_passthrough_mode() - Passthrough functionality no longer exists

    /**
     * @brief Test control flow
     */
    void test_func008_control_flow();

    /**
     * @brief Test command queue depth
     */
    void test_func009_command_queue_depth();

    /**
     * @brief Test DMA trigger
     */
    void test_func010_dma_trigger();
};
