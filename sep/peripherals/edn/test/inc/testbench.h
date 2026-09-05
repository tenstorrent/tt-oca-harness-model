// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.h
 * @brief EDN SystemC testbench header
 *
 * Top-level testbench that instantiates EDN model and test harness,
 * performs port binding, and executes verification tests.
 */

#pragma once
#include <systemc.h>
#include "../../include/edn.h"
#include "edn_test.h"
#include "reg_logger.h"

/**
 * @class testbench
 * @brief Top-level EDN testbench module
 *
 * Instantiates and binds EDN model (DUT) with test harness, manages
 * test execution, and reports results.
 *
 * Architecture:
 * - dut: EDN model instance (Device Under Test)
 * - test: EDN test harness instance
 * - Intermediate signals for port connections
 * - Test execution and result tracking
 */
class testbench : public sc_module
{
  public:
    /// Logger instance for test reporting
    RegLogger logger;

    SC_HAS_PROCESS(testbench);

    /**
     * @brief Testbench constructor
     * @param name SystemC module name
     *
     * Instantiates DUT and test harness, binds all ports, and registers
     * test execution thread.
     */
    testbench(sc_module_name name, int suite_id = 4);


    /**
     * @brief Testbench destructor
     *
     * Cleans up allocated instances.
     */
    ~testbench();

  private:
    /**
     * @brief Bind all ports between DUT and test harness
     *
     * Performs comprehensive port binding:
     * - TLM register socket (test → DUT)
     * - CSRNG command interface (DUT → test)
     * - CSRNG genbits interface (test → DUT)
     * - Endpoint interfaces (8 endpoints with signals)
     * - Interrupt signals (DUT → test)
     * - Alert signals (DUT → test)
     * - Clock and reset (test → DUT)
     */
    void bind_ports();

    /**
     * @brief Initialize testbench state
     *
     * Sets initial clock frequency, reset state, and endpoint signals.
     */
    void initialize();

    /**
     * @brief Main test execution thread
     *
     * Runs test sequence:
     * 1. Apply reset
     * 2. Verify register reset values
     * 3. Test register read/write access
     * 4. Test port connectivity
     * 5. Report results
     */
    void run_tests();

    /**
     * @brief Test register reset values
     *
     * Verifies all 17 registers have correct reset values after reset.
     */
    void test_register_reset_values();

    /**
     * @brief Test register read/write access
     *
     * Verifies register access types (RO, WO, RW) for all registers.
     */
    void test_register_access();

    /**
     * @brief Test port binding connectivity
     *
     * Verifies all ports are properly bound and functional.
     */
    void test_port_binding();

    /**
     * @brief Report test results
     *
     * Logs summary of passed/failed tests and exits simulation.
     */
    void report_results();

    /**
     * @brief Report test start banner
     * @param test_name Name of the test
     */
    void report_test_start(const std::string& test_name);

    /**
     * @brief Report test pass
     * @param test_name Name of the test
     */
    void report_test_pass(const std::string& test_name);

    /**
     * @brief Report test fail with reason
     * @param test_name Name of the test
     * @param reason Failure reason
     */
    void report_test_fail(const std::string& test_name, const std::string& reason);

    /**
     * @brief Report overall test summary
     */
    void report_test_summary();

    // =========================================================================
    // Component Instances
    // =========================================================================

    /// EDN model instance (Device Under Test)
    edn_ip* dut;

    /// EDN test harness instance
    edn_test* test;

    /// Currently executing test suite ID
    int m_suite_id;

    // =========================================================================
    // Intermediate Signals for Port Connections
    // =========================================================================

   // Interrupt signals
    sc_signal<bool> intr_cmd_req_done_sig;
    sc_signal<bool> intr_fatal_err_sig;

    // Alert signals
    sc_signal<bool> alert_recov_sig;
    sc_signal<bool> alert_fatal_sig;

    // Clock and reset signals
    sc_signal<double> clk_sig;
    sc_signal<bool> rst_ni_sig;

    // =========================================================================
    // Test Tracking
    // =========================================================================

    /// Number of tests executed
    unsigned int m_tests_run;

    /// Number of tests passed
    unsigned int m_tests_passed;

    /// Number of tests failed
public:
    unsigned int m_tests_failed;

    /// Number of tests skipped (require functionality implementation)
    unsigned int m_tests_skipped;

    /// Failed test tracking
    std::vector<std::string> m_failed_tests;
};
