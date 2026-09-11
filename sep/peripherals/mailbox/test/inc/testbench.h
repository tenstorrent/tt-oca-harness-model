// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/**
 * @file testbench.h
 * @brief Mailbox SystemC testbench header
 *
 * This file defines the top-level testbench that instantiates the mailbox model
 * and test harness, performs port binding for dual TLM sockets and signals,
 * and executes comprehensive register access test cases.
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 */

#pragma once

#include <systemc.h>
#include "../../include/mailbox.h"
#include "mailbox_test.h"
#include "reg_logger.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

/**
 * @class testbench
 * @brief Top-level mailbox testbench
 *
 * Instantiates mailbox model and test harness, binds all port interfaces
 * (dual TLM sockets for slave ports, interrupt signals, clock, reset),
 * and manages test execution for comprehensive register access validation.
 */
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    /**
     * @brief Testbench constructor
     * @param name Module name
     * @param log_verbosity Logging verbosity level (0=error, 1=warn, 2=info, 3=debug)
     */
#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
    testbench(sc_module_name name, int log_verbosity = REG_DEFAULT_VERBOSITY);

    /// @brief Destructor
    ~testbench();

    /**
     * @brief Main test execution entry point
     *
     * Executes all test cases. Registered as SC_THREAD in constructor.
     */
    void run_tests();


    // =========================================================================
    // Test Result Reporting Helpers
    // =========================================================================


    /**
     * @brief Report test result and update statistics
     * @param test_name Name of the test
     * @param passed True if test passed, false otherwise
     */
    void report_test_result(const char* test_name, bool passed);

    /**
     * @brief Print comprehensive test summary with statistics
     */
    void report_test_summary();

        /// @brief Number of tests failed
    int m_tests_failed;

private:
    // =========================================================================
    // Component Instances
    // =========================================================================

    /// @brief Mailbox model instance (DUT)
    mailbox_ip* dut;

    /// @brief Mailbox test harness instance for port 0
    mailbox_test* test_port0;

    /// @brief Mailbox test harness instance for port 1
    mailbox_test* test_port1;

    // =========================================================================
    // Interconnect Signals
    // =========================================================================

    /// @brief Interrupt output signal for port 0 (aggregates WTIRQ, RTIRQ, EIRQ)
    sc_signal<bool> irq_port0_sig;

    /// @brief Interrupt output signal for port 1 (aggregates WTIRQ, RTIRQ, EIRQ)
    sc_signal<bool> irq_port1_sig;

    /// @brief Abstract clock frequency signal (Hz)
    sc_signal<double> clk_sig;

    /// @brief Active-low asynchronous reset signal
    sc_signal<bool> rst_ni_sig;

    // =========================================================================
    // Test Statistics
    // =========================================================================

    /// @brief Number of tests executed
    int m_tests_run;

    /// @brief Number of tests passed
    int m_tests_passed;



    /// @brief List of failed test names
    std::vector<std::string> m_failed_tests;

    // =========================================================================
    // Test Case Methods - Register Access Validation
    // =========================================================================

    /**
     * @brief Test register reset values for all 10 registers
     *
     * Validates that all registers return correct reset values after power-on
     * and after asynchronous reset assertion.
     */
    void test_register_reset_values();

    /**
     * @brief Test read-only register write protection
     *
     * Validates that READ_DATA, STATUS, ERROR_FLAGS, and IRQP registers
     * reject write attempts (read value unchanged after write).
     */
    void test_read_only_register_protection();

    /**
     * @brief Test write-only register read protection
     *
     * Validates that WRITE_DATA and CTRL registers return error or
     * undefined value on read attempts (no valid data returned).
     */
    void test_write_only_register_protection();


    /**
     * @brief Test read-write register access (stub implementation)
     *
     * Validates that WIRQT, RIRQT, IRQS, and IRQEN registers support
     * both read and write operations correctly.
     */
    void test_read_write_register_access();


    /**
     * @brief Test asynchronous reset behavior
     *
     * Writes values to registers, asserts reset, validates all
     * registers return to reset values and FIFO clears.
     */
    void test_asynchronous_reset_behavior();


    // =========================================================================
    // Test Orchestration Methods
    // =========================================================================

    /**
     * @brief Run all FUNC-001 test cases
     *
     * Executes comprehensive test suite for FUNC-001: System Reset and Initialization Behavior
     * Includes Test IDs: 1, 61, 62 from mailbox-test-plan.md
     */
    void run_func001_tests();

    // FUNC-001 Test Cases (Test IDs: 1, 61, 62)
    void test_reset_fifo_interrupt_state();

    // =========================================================================
    // FUNC-002 Callback-Level Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run FUNC-002 callback-level tests (TC042–TC045).
     *
     * TC002–TC013 and TC030 were removed because their register-access
     * coverage duplicates tests already in testbench.cpp.  The four
     * unique callback-level tests retained here exercise behaviour not
     * covered by the basic suite:
     *  - TC042: IRQS write-1-to-clear (write-0 is a strict no-op)
     *  - TC043: IRQEN dynamic masking + retroactive IRQP recomputation
     *  - TC044: WIRQT saturation + immediate retroactive threshold trigger
     *  - TC045: RIRQT saturation + immediate retroactive threshold trigger
     */
    void run_func002_comprehensive_tests();

    // Unique callback-level tests (Test IDs: 42-45)
    void test_callback_irqs_write1clear();              ///< TC042: IRQS W1C callback
    void test_callback_irqen_masking();                 ///< TC043: IRQEN masking + retroactive IRQP
    void test_callback_wirqt_saturation_immediate();    ///< TC044: WIRQT saturation + retroactive trigger
    void test_callback_rirqt_saturation_immediate();    ///< TC045: RIRQT saturation + retroactive trigger

    // =========================================================================
    // FUNC-003 Comprehensive Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run all FUNC-003 comprehensive test cases from test plan
     *
     * Executes all 16 test cases mapped to FUNC-003: Bidirectional FIFO Data Transfer Engine
     * Includes Test IDs: 14-19, 39-40, 47-49, 51-52, 55-57 from mailbox-functionality-testcases.md
     *
     * Test Coverage:
     * - Unidirectional data transfer (Port 0→1, Port 1→0)
     * - Bidirectional simultaneous operation
     * - Data transfer lengths (minimum, typical, maximum)
     * - WRITE_DATA enqueue callback verification
     * - READ_DATA dequeue callback verification
     * - Cross-port data integrity with known patterns
     * - Independent FIFO paths
     * - Cross-port STATUS flag coherence
     * - Boundary cases (min/max FIFO depth, all-zeros/all-ones patterns)
     * - Configuration parameter validation (MailboxDepth variation)
     */
    void run_func003_tests();

    // FUNC-003 Test Cases (Test IDs: 14-19, 39-40, 47-49, 51-52, 55-57)
    void test_data_write_port0_read_port1();        ///< TC011: Unidirectional Port 0→1
    void test_data_write_port1_read_port0();        ///< TC012: Unidirectional Port 1→0
    void test_data_bidirectional_simultaneous();    ///< TC013: Bidirectional simultaneous
    void test_data_transfer_min_length();           ///< TC014: Single entry transfer
    void test_data_transfer_typical_length();       ///< TC015: Multiple entry transfer
    void test_data_transfer_max_length();           ///< TC016: Full FIFO depth transfer
    void test_callback_write_data_enqueue();        ///< TC017: WRITE_DATA callback
    void test_callback_read_data_dequeue();         ///< TC018: READ_DATA callback
    void test_crossport_data_integrity();           ///< TC019: Data integrity patterns
    void test_crossport_status_coherence();         ///< TC020: Cross-port STATUS flags
    void test_boundary_fifo_depth_min();            ///< TC021: Minimum depth operation
    void test_boundary_fifo_depth_max();            ///< TC022: Maximum depth operation

    // =========================================================================
    // FUNC-004 Comprehensive Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run all FUNC-004 comprehensive test cases from test plan
     *
     * Executes all 6 test cases mapped to FUNC-004: FIFO Status Monitoring System
     * Includes Test IDs: 4, 20-23, 49 from mailbox-functionality-testcases.md
     *
     * Test Coverage:
     * - STATUS register read-only access validation
     * - Empty flag (STATUS[0]) reflects read FIFO state
     * - Full flag (STATUS[1]) reflects write FIFO capacity
     * - Write threshold flag (STATUS[2]) indicates usage > WIRQT
     * - Read threshold flag (STATUS[3]) indicates fill > RIRQT
     * - Cross-port STATUS flag coherence
     * - Hardware-controlled register behavior
     * - Reserved bit masking
     */
    void run_func004_tests();

    // FUNC-004 Test Cases (Test IDs: 4, 20-23, 49)
    // Note: test_reg_status_ro() declared in FUNC-002 section (shared test)
    void test_status_empty_flag();                  ///< TC023: Verify STATUS[0] empty flag
    void test_status_full_flag();                   ///< TC024: Verify STATUS[1] full flag
    void test_status_write_threshold_flag();        ///< TC025: Verify STATUS[2] write threshold flag
    void test_status_read_threshold_flag();         ///< TC026: Verify STATUS[3] read threshold flag
    // Note: test_crossport_status_coherence() declared in FUNC-003 section (shared test)

    // =========================================================================
    // FUNC-005 Comprehensive Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run all FUNC-005 comprehensive test cases from test plan
     *
     * Executes all 9 test cases mapped to FUNC-005: Error Detection and Reporting Mechanism
     * Includes Test IDs: 5, 24-30, 41 from mailbox-functionality-testcases.md
     *
     * Test Coverage:
     * - Write-to-full FIFO error detection (ERROR_FLAGS[1])
     * - Read-from-empty FIFO error detection (ERROR_FLAGS[0])
     * - AXI RESP_SLVERR generation for invalid operations
     * - ERROR_FLAGS persistent error recording
     * - Clear-on-read behavior for ERROR_FLAGS
     * - Error flag accumulation (multiple errors)
     * - Error interrupt (EIRQ) generation (IRQS[2])
     * - IRQP computation for error interrupts
     * - ERROR_FLAGS read callback validation
     * - Dual-port error handling consistency
     *
     * NOTE: Tests 5 and 30 are implemented in test_mailbox_func002.cpp
     * Tests 26 and 29 have dependencies on FUNC_006 (Interrupt System)
     */
    void run_func005_tests();

    // FUNC-005 Test Cases (Test IDs: 5, 24-30, 41)
    // Note: test_reg_error_flags_ro() declared in FUNC-002 section (shared test - TC002)
    void test_error_write_to_full();                ///< TC027: Verify write-to-full error detection
    void test_error_read_from_empty();              ///< TC028: Verify read-from-empty error detection
    void test_interrupt_eirq_port0();               ///< TC029: Verify EIRQ interrupt (Port 0) - FUNC_006 dependency
    void test_error_flag_accumulation();            ///< TC030: Verify multiple errors accumulate
    void test_error_flag_clear_on_read();           ///< TC031: Verify clear-on-read atomicity
    void test_interrupt_eirq_port1();               ///< TC032: Verify EIRQ interrupt (Port 1) - FUNC_006 dependency

    // =========================================================================
    // FUNC-006 Comprehensive Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run all FUNC-006 comprehensive test cases from test plan
     *
     * Executes all 23 test cases mapped to FUNC-006: Programmable Threshold-Based Interrupt System
     * Includes Test IDs: 6-10, 24-29, 31-35, 42-45, 53-54, 58-60 from mailbox-functionality-testcases.md
     *
     * Test Coverage:
     * - WIRQT/RIRQT threshold configuration with saturation logic
     * - IRQS write-1-to-clear functionality (sticky status bits)
     * - IRQEN interrupt enable/disable control
     * - IRQP hardware-computed pending status (IRQP = IRQS & IRQEN)
     * - irq_o output signal generation (level-triggered and edge-triggered modes)
     * - Three interrupt types per port (WTIRQ, RTIRQ, EIRQ)
     * - Threshold-based FIFO interrupts (transmit and receive)
     * - Retroactive threshold triggering
     * - Zero threshold behavior
     * - Maximum threshold boundary conditions
     * - Strictly greater-than comparison logic
     * - Interrupt trigger modes (level/edge) - signal-level validation deferred
     * - Interrupt polarity (active-high/low) - signal-level validation deferred
     * - Error interrupt integration (IRQS[2] from FUNC_005)
     *
     * NOTE: Tests 6-10, 42-45 are implemented in test_mailbox_func002.cpp
     * NOTE: Tests 26, 29 are implemented in test_mailbox_func005.cpp
     * NOTE: Tests 58-60 require signal-level validation (integration testbench)
     */
    void run_func006_tests();

    // FUNC-006 Test Cases (Test IDs: 6-10, 24-29, 31-35, 42-45, 53-54, 58-60)
    // Note: test_reg_wirqt_rw() declared in FUNC-002 section (shared test - TC004)
    // Note: test_reg_rirqt_rw() declared in FUNC-002 section (shared test - TC004)
    // Note: test_reg_irqs_rw_write1clear() declared in FUNC-002 section (shared test - TC004)
    // Note: test_reg_irqen_rw() declared in FUNC-002 section (shared test - TC004)
    // Note: test_reg_irqp_ro_computed() declared in FUNC-002 section (shared test - TC004)
    void test_interrupt_wtirq_port0();              ///< TC027: Verify WTIRQ interrupt (Port 0)
    void test_interrupt_rtirq_port0();              ///< TC028: Verify RTIRQ interrupt (Port 0)
    // Note: test_interrupt_eirq_port0() declared in FUNC-005 section (shared test - TC029)
    void test_interrupt_wtirq_port1();              ///< TC030: Verify WTIRQ interrupt (Port 1)
    void test_interrupt_rtirq_port1();              ///< TC031: Verify RTIRQ interrupt (Port 1)
    // Note: test_interrupt_eirq_port1() declared in FUNC-005 section (shared test - TC032)
    void test_threshold_saturation_wirqt();         ///< TC037: Verify WIRQT saturation logic
    void test_threshold_saturation_rirqt();         ///< TC038: Verify RIRQT saturation logic
    void test_threshold_zero_wirqt();               ///< TC039: Verify zero WIRQT threshold
    void test_threshold_zero_rirqt();               ///< TC040: Verify zero RIRQT threshold
    void test_threshold_retroactive_trigger();      ///< TC041: Verify retroactive threshold triggering
    // Note: TC042-TC045 declared in FUNC-002 section above (test_callback_irqs_write1clear etc.)
    void test_boundary_threshold_max_value();       ///< TC042: Verify threshold maximum value
    void test_boundary_threshold_equal_usage();     ///< TC043: Verify threshold equal usage comparison
    void test_config_interrupt_level_triggered();   ///< TC044: Verify level-triggered mode (stub - integration level)
    void test_config_interrupt_polarity();          ///< TC045: Verify interrupt polarity (stub - integration level)

    // =========================================================================
    // FUNC-007 Comprehensive Test Suite Entry Point
    // =========================================================================

    /**
     * @brief Run all FUNC-007 comprehensive test cases from test plan
     *
     * Executes all 6 test cases mapped to FUNC-007: Software-Controlled FIFO Management
     * Includes Test IDs: 11, 36-38, 46, 50 from mailbox-functionality-testcases.md
     *
     * Test Coverage:
     * - CTRL register write-only access with self-clearing behavior (TC003)
     * - Write FIFO flush operation (CTRL[0]=1) - Per-FIFO granularity
     * - Read FIFO flush operation (CTRL[1]=1) - Per-FIFO granularity
     * - Dual-port flush OR coordination (either port can flush either FIFO)
     * - STATUS flag updates after flush operations
     * - Self-clearing register behavior (CTRL resets to 0x0)
     * - Cross-port flush effects (Port 0 flush affects Port 1 FIFO state)
     * - Atomic flush execution (immediate FIFO clearing)
     * - Data loss confirmation (flushed data permanently discarded)
     *
     * NOTE: TC003 (test_reg_ctrl_wo_selfclearing) is implemented in
     * test_mailbox_func002.cpp as it is shared with FUNC-002.
     */
    void run_func007_tests();

    // FUNC-007 Test Cases (Test IDs: 11, 36-38)
    // Note: test_reg_ctrl_wo_selfclearing() declared in FUNC-002 section (shared test - TC003)
    void test_ctrl_flush_write_fifo();              ///< TC046: Verify write FIFO flush operation
    void test_ctrl_flush_read_fifo();               ///< TC047: Verify read FIFO flush operation
    void test_ctrl_flush_dual_port_or();            ///< TC048: Verify dual-port flush OR coordination

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Read register with TLM response status return
     * @param port Port number (0 or 1)
     * @param offset Register address offset
     * @param read_value Reference to store read data
     * @return TLM response status (TLM_OK_RESPONSE or TLM_GENERIC_ERROR_RESPONSE)
     */
    tlm::tlm_response_status mailbox_read(unsigned int port, unsigned int offset, uint64_t& read_value);

    /**
     * @brief Write register with TLM response status return
     * @param port Port number (0 or 1)
     * @param offset Register address offset
     * @param write_value Data to write
     * @return TLM response status (TLM_OK_RESPONSE or TLM_GENERIC_ERROR_RESPONSE)
     */
    tlm::tlm_response_status mailbox_write(unsigned int port, unsigned int offset, uint64_t write_value);

    /**
     * @brief Bind all ports between model and test harness
     *
     * Binds dual TLM target sockets (slv_reqs_i[0], slv_reqs_i[1]),
     * interrupt outputs (irq_o[0], irq_o[1]), clock (clk_i), and
     * reset (rst_ni) signals.
     */
    void bind_ports();

    /**
     * @brief Initialize testbench environment
     *
     * Sets up clock frequency, reset signal initial values, and
     * prepares test environment for execution.
     */
    void initialize();

    /**
     * @brief Apply reset to DUT
     *
     * Asserts active-low reset (rst_ni=0), waits for reset duration,
     * then deasserts (rst_ni=1). Ensures proper DUT initialization.
     */
    void apply_reset();

    /// @brief Logger instance for structured logging
    mutable RegLogger logger;
};
