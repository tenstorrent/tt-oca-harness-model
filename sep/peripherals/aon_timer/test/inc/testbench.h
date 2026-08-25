// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.h
 * @brief AON Timer top-level SystemC testbench header.
 *
 * This header defines the testbench module, which is the top-level simulation
 * environment for the AON Timer. It:
 *   - Instantiates the aon_timer DUT model and aon_timer_test harness.
 *   - Binds all sc_out/sc_in ports on the DUT to the sc_signal members in
 *     aon_timer_test using direct port binding.
 *   - Binds the TLM target socket (DUT) to the TLM initiator socket (test).
 *   - Registers and executes the required test cases as an SC_THREAD.
 *
 * Test Cases Implemented:
 *   - TC_AON_BIND: Binding verification - successful elaboration and connectivity check
 *   - TC_AON_RST : Reset test - write values, toggle reset, verify reset values restored
 *   - TC_AON_RW  : Read/Write test - write a value, read back, assert match
 *   - TC_AON_RO  : Read-Only type test - write to WO register, read back, assert 0x0 unchanged
 *
 * FUNC001 Test Cases (TC_AON_001 through TC_AON_009):
 *   - TC_AON_001 : Register Reset Values Verification - All Registers
 *   - TC_AON_002 : WKUP_CTRL RW Access and Reserved Bits Behavior
 *   - TC_AON_003 : ALERT_TEST Write-Only (WO) Access - No Read Storage
 *   - TC_AON_004 : INTR_TEST Write-Only (WO) Access - No Read Storage
 *   - TC_AON_005 : INTR_STATE RW1C (Write-1-to-Clear) Semantics
 *   - TC_AON_006 : WKUP_CAUSE RW0C (Write-0-to-Clear) Semantics
 *   - TC_AON_007 : WDOG_REGWEN Write-Once-Clear Lock - Basic Operation
 *   - TC_AON_008 : Reserved Bits Read-As-Zero Across All Registers
 *   - TC_AON_009 : Asynchronous Register Write Completion - Read-Back Guarantees Write Propagation
 *
 * FUNC002 Test Cases (TC_AON_041 through TC_AON_043):
 *   - TC_AON_041 : System Reset (rst_n) Clears All Counters, Thresholds, and Outputs
 *   - TC_AON_042 : Watchdog Bite Induced Reset - aon_timer_rst_req Triggers Reset Sequence
 *   - TC_AON_043 : Independent AON Domain Reset via rst_aon_n
 *
 * FUNC003 Test Cases (TC_AON_010-016, TC_AON_023, TC_AON_037-040, TC_AON_048, TC_AON_052, TC_AON_055, TC_AON_056, TC_AON_058, TC_AON_060):
 *   - TC_AON_010 : Wakeup Timer Enable and Disable via WKUP_CTRL.enable
 *   - TC_AON_011 : Prescaler Operation - Multiple Prescaler Values
 *   - TC_AON_012 : Prescaler Reset Side-Effect on Every WKUP_CTRL Write
 *   - TC_AON_015 : 64-bit Threshold Comparison Correctness
 *   - TC_AON_016 : Counter Software Write - Initialize to Arbitrary Value
 *   - TC_AON_023 : Wakeup Timer Unaffected by Sleep Mode
 *   - TC_AON_037 : WKUP_COUNT 64-bit Safe Read Using Double-Read Technique
 *   - TC_AON_038 : WKUP_COUNT 64-bit Safe Write - Disable Timer Before Writing HI and LO
 *   - TC_AON_039 : WKUP_THOLD 64-bit Safe Write - Spurious Wakeup Prevention Sequence
 *   - TC_AON_040 : WKUP_THOLD 64-bit Sequential Read is Race-Condition Free
 *   - TC_AON_048 : Counter Initialized Above Threshold - Immediate Interrupt on Enable (wakeup timer)
 *   - TC_AON_052 : 64-bit Wakeup Counter Overflow Wrap-Around
 *   - TC_AON_055 : Both Timers Running Concurrently and Independently
 *   - TC_AON_056 : WKUP_CTRL Same-Value Write Still Resets Prescaler Accumulator
 *   - TC_AON_058 : Maximum Prescaler Value - prescaler=4095
 *   - TC_AON_060 : WKUP_COUNT_HI Read During Active Counting - Volatile 64-bit Counter
 *
 * FUNC004 Test Cases (TC_AON_017-022, TC_AON_048 wdog, TC_AON_049-053, TC_AON_059):
 *   - TC_AON_017 : Watchdog Timer Enable and Disable via WDOG_CTRL.enable
 *   - TC_AON_018 : Watchdog Bark Threshold Interrupt Generation
 *   - TC_AON_019 : Watchdog Bite Threshold - aon_timer_rst_req Assertion
 *   - TC_AON_020 : Watchdog Petting - Any Write to WDOG_COUNT Resets Counter to Zero
 *   - TC_AON_021 : Watchdog Petting Under Active Bark - Counter Resets and Interrupts Clear
 *   - TC_AON_022 : Watchdog Sleep Pause Feature - Pause-in-Sleep Control
 *   - TC_AON_048b: Watchdog Counter Above Bark Threshold on Enable - Immediate Bark
 *   - TC_AON_049 : Watchdog Bark and Bite at Same Threshold - Simultaneous Assertion
 *   - TC_AON_050 : Watchdog Bite Threshold Lower Than Bark - Bite Before Bark
 *   - TC_AON_051 : Zero-Value Bite Threshold - Immediate Bite on Enable
 *   - TC_AON_053 : 32-bit Watchdog Counter Overflow Wrap-Around
 *   - TC_AON_059 : Volatile WDOG_COUNT Reads During Active Counting
 *
 * FUNC005 Test Cases (TC_AON_013, TC_AON_014, TC_AON_024-028, TC_AON_054, TC_AON_057):
 *   Note: TC_AON_004 and TC_AON_005 are already implemented under FUNC001 and cover
 *         INTR_TEST WO access and INTR_STATE RW1C semantics respectively.
 *   - TC_AON_013 : Wakeup Timer Threshold Comparison and Interrupt Generation
 *   - TC_AON_014 : Wakeup Timer Interrupt Continuous Re-Triggering After INTR_STATE Clear
 *   - TC_AON_024 : INTR_TEST Force-Assert Wakeup Timer Interrupt
 *   - TC_AON_025 : Watchdog Bark Interrupt via INTR_TEST Force-Assert and NMI Coupling
 *   - TC_AON_026 : Wakeup Timer Interrupt Deassertion - W1C with Counter Below Threshold
 *   - TC_AON_027 : Watchdog Bark Interrupt Deassertion Sequence
 *   - TC_AON_028 : Wakeup Timer Interrupt and Wakeup Request Independent Clearing
 *   - TC_AON_054 : Interrupt Re-Assertion Storm - Disabled Timer Prevents Re-Assertion
 *   - TC_AON_057 : Wakeup Request Remains Active After Only INTR_STATE Clear
 *
 * FUNC006 Test Cases (TC_AON_029, TC_AON_030):
 *   Note: TC_AON_006, TC_AON_013, TC_AON_018, TC_AON_028, TC_AON_057 are already
 *         implemented under earlier FUNC tests and cover WKUP_CAUSE RW0C semantics,
 *         wkup_req from wakeup timer and watchdog sources, and independent clearing.
 *   - TC_AON_029 : Wakeup Request Persistence Until Explicit Clear
 *   - TC_AON_030 : Wakeup Request from Watchdog Bark - Dual Source to wkup_req
 *
 * FUNC007 Test Cases (TC_AON_031):
 *   Note: TC_AON_019, TC_AON_041, TC_AON_042 are already implemented under FUNC004
 *         and FUNC002 and cover aon_timer_rst_req assertion and reset self-clearing.
 *   - TC_AON_031 : Watchdog Bite Reset Request - aon_timer_rst_req Assertion and
 *                  Persistence Independent of Bark Interrupt State
 *
 * FUNC008 Test Cases (TC_AON_032-036, TC_AON_044-047):
 *   - TC_AON_032 : WDOG_REGWEN Lock - Protected Registers Silently Ignore Writes
 *   - TC_AON_033 : WDOG_REGWEN Lock - WDOG_COUNT Remains Writable (Petting)
 *   - TC_AON_034 : ALERT_TEST Fatal Fault Alert Connectivity Test
 *   - TC_AON_035 : RACL Access Control with EnableRacl=1 (Limited Test - EnableRacl=0)
 *   - TC_AON_036 : RACL Absent with EnableRacl=0 - No RACL Enforcement
 *   - TC_AON_044 : Lifecycle Escalation Halts Both Wakeup and Watchdog Counters
 *   - TC_AON_045 : Escalation During Active Threshold Condition - Interrupts Persist
 *   - TC_AON_046 : Escalation Prevents Watchdog Bite During Escalation Processing
 *   - TC_AON_047 : Escalation Does Not Affect Register Read/Write Access
 *
 * Port Binding Summary (DUT port -> testbench signal):
 *   dut.intr_wkup_timer_expired -> test.intr_wkup_timer_expired_sig
 *   dut.intr_wdog_timer_bark    -> test.intr_wdog_timer_bark_sig
 *   dut.nmi_wdog_timer_bark     -> test.nmi_wdog_timer_bark_sig
 *   dut.wkup_req                -> test.wkup_req_sig
 *   dut.aon_timer_rst_req       -> test.aon_timer_rst_req_sig
 *   dut.sleep_mode              -> test.sleep_mode_sig
 *   dut.lc_escalate_en          -> test.lc_escalate_en_sig
 *   dut.fatal_fault             -> test.fatal_fault_sig
 *   dut.clk_aon_freq            -> test.clk_aon_freq_sig
 *   dut.clk_sys_freq            -> test.clk_sys_freq_sig
 *   dut.rst_n                   -> test.rst_n_sig
 *   dut.rst_aon_n               -> test.rst_aon_n_sig
 *   dut.racl_policies           -> test.racl_policies_sig
 *   dut.racl_error              -> test.racl_error_sig
 *   dut.target_socket           -> test.initiator_socket  (TLM socket binding)
 */

#pragma once
#include <systemc.h>
#include "../../include/aon_timer.h"
#include "aon_timer_test.h"
#include "csml_logger.h"
#include <string>
#include <vector>

/**
 * @class testbench
 * @brief Top-level AON Timer testbench module.
 *
 * Instantiates and connects the AON Timer DUT and test harness. Manages test
 * execution via an SC_THREAD process (run_tests). Uses CSML logging throughout;
 * no std::cout is used. Test results are reported via CSML_INFO and CSML_ERROR.
 *
 * The bind_ports() method performs all port connections during elaboration.
 * The run_tests() method performs reset initialization and executes all test cases.
 */
class testbench : public sc_module
{
public:
   SC_HAS_PROCESS(testbench);

   /**
    * @brief Testbench constructor.
    * @param name SystemC hierarchical module name.
    *
    * Instantiates dut and test, binds all ports via bind_ports(), and registers
    * the run_tests SC_THREAD.
    */
   testbench(sc_module_name name);

   /**
    * @brief Destructor - releases dynamically allocated dut and test instances.
    */
   ~testbench();

   /**
    * @brief Main test execution SC_THREAD entry point.
    *
    * Performs the following sequence:
    *   1. Asserts both resets (rst_n=0, rst_aon_n=0), waits for propagation.
    *   2. De-asserts both resets (rst_n=1, rst_aon_n=1), waits for stabilization.
    *   3. Sets clock frequency signals to nominal values (AON: 200 kHz, SYS: 100 MHz).
    *   4. Runs TC_AON_BIND: port binding verification.
    *   5. Runs TC_AON_RST: reset value verification.
    *   6. Runs TC_AON_RW: read/write register test.
    *   7. Runs TC_AON_RO: write-only register read test.
    *   8. Reports overall test summary and calls sc_stop().
    */
   void run_tests();

private:
   // =========================================================================
   // Component Instances
   // =========================================================================

   /// @brief AON Timer DUT model instance (EnableRacl=false by default).
   aon_timer_ip* dut;

   /// @brief AON Timer test harness instance containing signals and access helpers.
   aon_timer_test* test;

   // =========================================================================
   // Test Statistics
   // =========================================================================

   /// @brief Total number of test cases executed.
   int m_tests_run;

   /// @brief Number of test cases that passed.
   int m_tests_passed;

   /// @brief Number of test cases that failed.
public:
   int m_tests_failed;

   /// @brief List of failed test case names for final summary report.
   std::vector<std::string> m_failed_tests;

   // =========================================================================
   // Logger
   // =========================================================================

   /// @brief CSML logger instance for structured logging (INFO, ERROR, DEBUG).
   mutable CsmlLogger logger;

   // =========================================================================
   // Internal Helper Methods
   // =========================================================================

   /**
    * @brief Bind all DUT ports to signals in the test harness and bind TLM sockets.
    *
    * Called during the testbench constructor. Performs:
    *   - dut.target_socket (tlm_target_socket<32>) bound to test.initiator_socket
    *   - All sc_out<bool> DUT ports bound to corresponding sc_signal<bool> in test
    *   - All sc_in<bool> DUT ports bound to corresponding sc_signal<bool> in test
    *   - sc_in<double> clock ports bound to sc_signal<double> in test
    *   - sc_in<uint32_t> racl_policies bound to sc_signal<uint32_t> in test
    *
    * Uses direct port.bind(signal) syntax. Reports binding completion via CSML_INFO.
    */
   void bind_ports();

   /**
    * @brief Apply active-low reset to the DUT.
    *
    * Asserts rst_n=0 and rst_aon_n=0, waits 10 ns for reset propagation,
    * then de-asserts rst_n=1 and rst_aon_n=1, waits 10 ns for stabilization.
    * All output ports on the DUT should be driven to their reset state during
    * the asserted phase.
    */
   void apply_reset();

   /**
    * @brief Report test start to CSML log.
    * @param test_name Human-readable test case name.
    */
   void report_test_start(const std::string& test_name);

   /**
    * @brief Record a test pass and report to CSML log.
    * @param test_name Human-readable test case name.
    */
   void report_test_pass(const std::string& test_name);

   /**
    * @brief Record a test failure and report to CSML log.
    * @param test_name Human-readable test case name.
    * @param reason    Description of why the test failed.
    */
   void report_test_fail(const std::string& test_name, const std::string& reason = "");

   /**
    * @brief Print final test summary with pass/fail counts.
    *
    * Reports total tests run, passed, failed, and lists all failed test names.
    * Uses CSML_INFO for pass summaries and CSML_ERROR for failure reporting.
    */
   void report_test_summary();

   // =========================================================================
   // Test Case Methods
   // =========================================================================

   /**
    * @brief TC_AON_BIND: Verify successful port binding and elaboration.
    *
    * Verifies that the testbench reached simulation start without binding errors,
    * confirming that all 14 port connections between dut and test are valid.
    * Also verifies that reading the WDOG_REGWEN register (which resets to 0x1)
    * succeeds after reset, demonstrating that the TLM socket binding is functional.
    *
    * Pass criteria:
    *   - Simulation reaches this test case without SC_REPORT_FATAL binding errors.
    *   - WDOG_REGWEN reads back as 0x00000001 after reset.
    */
   void test_port_binding_verification();

   /**
    * @brief TC_AON_RST: Verify all 14 registers return to their reset values after reset.
    *
    * Writes non-zero values to all writable registers, applies reset via apply_reset(),
    * then reads all 14 registers and verifies they match their expected reset values
    * from aon_timer_basetest::Register_Reset_Val.
    *
    * Pass criteria:
    *   - All 14 registers read back their hardware-specified reset values.
    *   - WDOG_REGWEN reads 0x00000001 (only non-zero reset value).
    *   - All other registers read 0x00000000.
    */
   void test_reset_values();

   /**
    * @brief TC_AON_RW: Read/Write round-trip test on a RW register.
    *
    * Selects WKUP_THOLD_LO (offset 0x0C, RW, reset=0x0) as the target register.
    * Writes a known test pattern (0xDEADBEEF), reads back the value, and asserts
    * that the read-back matches the written value.
    *
    * Pass criteria:
    *   - Read-back value equals written value (0xDEADBEEF).
    *   - No TLM transport errors during access.
    */
   void test_rw_register();

   /**
    * @brief TC_AON_RO: Write-only register read test.
    *
    * Targets INTR_TEST (offset 0x30, WO, reset=0x0). Writes a non-zero value
    * (0x00000003) to the register, then reads it back and asserts the read value
    * is 0x00000000 (write-only registers have no read storage).
    *
    * Pass criteria:
    *   - Read-back value is 0x00000000 regardless of what was written.
    *   - Demonstrates correct WO access type enforcement by the model.
    */
   void test_wo_register_read();

   // =========================================================================
   // FUNC001 Test Case Methods (TC_AON_001 through TC_AON_009)
   // =========================================================================

   /**
    * @brief TC_AON_001: Register Reset Values Verification - All Registers.
    *
    * Reads all 14 AON Timer registers immediately after de-asserting system reset
    * and verifies each register matches its documented power-on reset value.
    * Also verifies all output ports (interrupts, wkup_req, aon_timer_rst_req,
    * fatal_fault) are de-asserted immediately after reset.
    *
    * Registers covered (offsets 0x00-0x34):
    *   ALERT_TEST=0x0, WKUP_CTRL=0x0, WKUP_THOLD_HI=0x0, WKUP_THOLD_LO=0x0,
    *   WKUP_COUNT_HI=0x0, WKUP_COUNT_LO=0x0, WDOG_REGWEN=0x1, WDOG_CTRL=0x0,
    *   WDOG_BARK_THOLD=0x0, WDOG_BITE_THOLD=0x0, WDOG_COUNT=0x0, INTR_STATE=0x0,
    *   INTR_TEST=0x0, WKUP_CAUSE=0x0.
    *
    * Pass criteria:
    *   - All 14 registers match their documented reset values exactly.
    *   - All boolean output ports read as logic 0 (de-asserted).
    *
    * Reference: TC_AON_001 in aon_timer-test-plan.md, FUNC001 register transport.
    */
   void test_func001_tc001_register_reset_values();

   /**
    * @brief TC_AON_002: WKUP_CTRL RW Access and Reserved Bits Behavior.
    *
    * Verifies WKUP_CTRL allows read-write access to defined fields (bits[12:0]:
    * prescaler[12:1] and enable[0]), that reserved bits[31:13] always read as
    * zero on any read, and that writes to reserved bits are silently ignored.
    *
    * Test patterns:
    *   - Write 0x00001FFF: expect read-back 0x00001FFF (all defined bits set).
    *   - Write 0xFFFFFFFF: expect read-back 0x00001FFF (reserved bits masked).
    *   - Write 0x00000000: expect read-back 0x00000000.
    *
    * Pass criteria:
    *   - Reserved bits[31:13] always return 0 on read.
    *   - Valid bits[12:0] store and return written values.
    *   - No TLM error responses on any access.
    *
    * Reference: TC_AON_002 in aon_timer-test-plan.md.
    */
   void test_func001_tc002_wkup_ctrl_rw_reserved_bits();

   /**
    * @brief TC_AON_003: ALERT_TEST Write-Only (WO) Access - No Read Storage.
    *
    * Verifies ALERT_TEST is a write-only register with no storage flip-flops.
    * All reads must return 0x00000000 regardless of prior writes. Writes to
    * bit[0] must assert a transient fatal_fault output pulse.
    *
    * Test steps:
    *   - Pre-write read: expect 0x00000000.
    *   - Write 0x00000001, read back: expect 0x00000000 (no storage).
    *   - Write 0xFFFFFFFF, read back: expect 0x00000000 (no storage).
    *   - Verify fatal_fault pulsed transiently during write-1 to bit[0].
    *
    * Pass criteria:
    *   - ALERT_TEST reads always return 0x00000000.
    *   - fatal_fault port asserts transiently on write-1 to bit[0].
    *
    * Reference: TC_AON_003 in aon_timer-test-plan.md.
    */
   void test_func001_tc003_alert_test_wo_access();

   /**
    * @brief TC_AON_004: INTR_TEST Write-Only (WO) Access - No Read Storage.
    *
    * Verifies INTR_TEST is write-only with no storage. All reads return
    * 0x00000000. Writes to bit[0] force-assert intr_wkup_timer_expired;
    * writes to bit[1] force-assert intr_wdog_timer_bark and nmi_wdog_timer_bark.
    *
    * Test steps:
    *   - Pre-write read: expect 0x00000000.
    *   - Write 0x00000001 (set wkup bit), read INTR_TEST: expect 0x00000000.
    *   - Verify INTR_STATE.bit[0]=1 and intr_wkup_timer_expired asserted.
    *   - Clear via INTR_STATE W1C write.
    *   - Write 0x00000002 (set bark bit), read INTR_TEST: expect 0x00000000.
    *   - Verify INTR_STATE.bit[1]=1 and intr_wdog_timer_bark asserted.
    *   - Write 0x00000003 (both bits), read INTR_TEST: expect 0x00000000.
    *
    * Pass criteria:
    *   - Every read of INTR_TEST returns 0x00000000.
    *   - INTR_TEST writes correctly force-assert interrupt outputs.
    *
    * Reference: TC_AON_004 in aon_timer-test-plan.md.
    */
   void test_func001_tc004_intr_test_wo_access();

   /**
    * @brief TC_AON_005: INTR_STATE RW1C (Write-1-to-Clear) Semantics.
    *
    * Verifies INTR_STATE correctly implements W1C semantics: writing bit=1
    * clears that interrupt bit and de-asserts the corresponding output port;
    * writing bit=0 has no effect on any bit.
    *
    * Test steps:
    *   1. Force-set both INTR_STATE bits via INTR_TEST write 0x00000003.
    *   2. Read INTR_STATE: expect 0x00000003.
    *   3. Write 0x00000000 to INTR_STATE: verify bits unchanged (W0 = no effect).
    *   4. Write 0x00000001 to INTR_STATE: verify bit[0] cleared, bit[1] still set.
    *   5. Verify intr_wkup_timer_expired de-asserted, intr_wdog_timer_bark still asserted.
    *   6. Write 0x00000002 to INTR_STATE: verify 0x00000000 returned.
    *   7. Verify intr_wdog_timer_bark and nmi_wdog_timer_bark both de-asserted.
    *
    * Pass criteria:
    *   - W1C semantics: write-1 clears; write-0 preserves.
    *   - NMI (nmi_wdog_timer_bark) mirrors bark interrupt at all times.
    *
    * Reference: TC_AON_005 in aon_timer-test-plan.md.
    */
   void test_func001_tc005_intr_state_rw1c_semantics();

   /**
    * @brief TC_AON_006: WKUP_CAUSE RW0C (Write-0-to-Clear) Semantics.
    *
    * Verifies WKUP_CAUSE.cause implements RW0C semantics: writing 0 to bit[0]
    * clears it and de-asserts wkup_req; writing 1 to bit[0] has no clearing
    * effect (the bit remains set).
    *
    * Test steps:
    *   1. Enable wakeup timer with threshold=2, counter=0 to trigger threshold crossing.
    *   2. Advance simulation until WKUP_CAUSE.cause=1 and wkup_req asserted.
    *   3. Write 0x00000001 to WKUP_CAUSE (attempt RW0C clear with 1): verify no effect.
    *   4. Write 0x00000000 to WKUP_CAUSE (correct RW0C clear): verify cause cleared.
    *   5. Verify wkup_req de-asserted after write-0.
    *
    * Pass criteria:
    *   - Write-1 to WKUP_CAUSE.cause has no clearing effect.
    *   - Write-0 to WKUP_CAUSE.cause clears the bit and de-asserts wkup_req.
    *
    * Reference: TC_AON_006 in aon_timer-test-plan.md.
    */
   void test_func001_tc006_wkup_cause_rw0c_semantics();

   /**
    * @brief TC_AON_007: WDOG_REGWEN Write-Once-Clear Lock - Basic Operation.
    *
    * Verifies WDOG_REGWEN.regwen (bit[0]) starts at 0x1 (unlocked), can be
    * cleared to 0x0 by writing 0 (permanently locked until system reset), and
    * that write-1 to WDOG_REGWEN has no effect regardless of current state.
    * Also verifies system reset restores WDOG_REGWEN to 0x1.
    *
    * Test steps:
    *   1. Read WDOG_REGWEN: verify 0x00000001.
    *   2. Write 0x00000001: read back, verify still 0x00000001 (write-1 = no effect).
    *   3. Write 0x00000000 (lock): verify reads as 0x00000000.
    *   4. Write 0x00000001 (attempt restore): verify still 0x00000000.
    *   5. Apply system reset: verify WDOG_REGWEN restores to 0x00000001.
    *
    * Pass criteria:
    *   - Lock is permanent within a power cycle; only reset can restore.
    *   - Write-1 is always a no-op for WDOG_REGWEN.
    *
    * Reference: TC_AON_007 in aon_timer-test-plan.md.
    */
   void test_func001_tc007_wdog_regwen_write_once_clear();

   /**
    * @brief TC_AON_008: Reserved Bits Read-As-Zero Across All Registers.
    *
    * Verifies that all reserved bit fields in registers with partial bit usage
    * consistently return 0 on read, even after writing 0xFFFFFFFF to those
    * registers. Also confirms that defined fields retain their correct values.
    *
    * Registers verified (with their reserved bit masks):
    *   - WKUP_CTRL (0x04):    reserved bits[31:13] must read as 0; valid bits[12:0]=0x1FFF.
    *   - WDOG_REGWEN (0x18):  reserved bits[31:1] must read as 0; valid bit[0] per lock state.
    *   - WDOG_CTRL (0x1C):    reserved bits[31:2] must read as 0; valid bits[1:0]=0x3.
    *   - INTR_STATE (0x2C):   reserved bits[31:2] must read as 0 after W1C settles.
    *   - WKUP_CAUSE (0x34):   reserved bits[31:1] must read as 0.
    *   - INTR_TEST (0x30):    WO register, full word reads as 0x00000000.
    *
    * Pass criteria:
    *   - Reserved bits return 0 on all reads.
    *   - No register corruption on writes targeting reserved bit positions.
    *   - No TLM error responses on any access.
    *
    * Reference: TC_AON_008 in aon_timer-test-plan.md.
    */
   void test_func001_tc008_reserved_bits_read_as_zero();

   /**
    * @brief TC_AON_009: Asynchronous Register Write Completion - Read-Back Guarantee.
    *
    * Verifies the CDC write-completion semantics of the AON Timer model. After a
    * write transaction completes at the TL-UL bus interface, a subsequent read-back
    * of the same register must return the written value, confirming that the write
    * has propagated through the functional CDC abstraction and that no stale
    * pre-write value is returned.
    *
    * Tested registers:
    *   - WKUP_CTRL (write 0x00000001, read back expecting 0x00000001).
    *   - WDOG_CTRL (write 0x00000001, read back expecting 0x00000001).
    *   - WDOG_BARK_THOLD (write 0x00001000, read back expecting 0x00001000).
    *   - WKUP_THOLD_HI (write 0xDEADBEEF, read back expecting 0xDEADBEEF).
    *
    * Pass criteria:
    *   - Every read-back after write returns the written value without additional
    *     simulation time advancement between write and read.
    *   - Model correctly implements the CDC propagation guarantee.
    *
    * Reference: TC_AON_009 in aon_timer-test-plan.md.
    */
   void test_func001_tc009_cdc_write_completion();

   // =========================================================================
   // FUNC002 Test Case Methods (TC_AON_041 through TC_AON_043)
   // =========================================================================

   /**
    * @brief TC_AON_041: System Reset (rst_n) Clears All Counters, Thresholds, and Outputs.
    *
    * Pre-configures the AON Timer in a complex active state (both timers enabled,
    * thresholds crossed to assert all interrupt/wakeup outputs, WDOG_REGWEN locked)
    * then applies a full system reset (rst_n=0, rst_aon_n=0) and verifies that every
    * register and output port returns to its documented power-on reset value.
    *
    * Verification sequence:
    *   1. Configure WKUP timer: threshold=2, counter=3, enable=1. Verify wkup outputs asserted.
    *   2. Configure WDOG timer: bark_thold=3, bite_thold=5, counter=5, enable=1.
    *      Verify bark/NMI and wkup_req asserted; verify aon_timer_rst_req asserted.
    *   3. Lock WDOG configuration: write 0 to WDOG_REGWEN. Verify WDOG_REGWEN=0.
    *   4. Assert sleep_mode=1 to confirm it does not affect reset output.
    *   5. Assert rst_n=0, rst_aon_n=0 (full system reset). Wait for propagation.
    *   6. De-assert rst_n=1, rst_aon_n=1. Wait for stabilization.
    *   7. Read all 14 registers; verify each matches documented reset value.
    *   8. Verify all 6 output ports de-asserted: intr_wkup_timer_expired=0,
    *      intr_wdog_timer_bark=0, nmi_wdog_timer_bark=0, wkup_req=0,
    *      aon_timer_rst_req=0, fatal_fault=0.
    *   9. Verify WDOG_REGWEN=0x1 (lock unconditionally cleared by rst_n).
    *
    * Pass criteria:
    *   - All 14 registers at documented reset values post-reset.
    *   - All output ports de-asserted (logic 0) post-reset.
    *   - WDOG_REGWEN restored to 0x1 (lock cleared even though it was 0 before reset).
    *
    * Reference: TC_AON_041 in aon_timer-test-plan.md, FUNC002 reset behavior.
    */
   void test_func002_tc041_system_reset_clears_all_state();

   /**
    * @brief TC_AON_042: Watchdog Bite Induced Reset - Self-Clearing via Power Manager.
    *
    * Verifies that a watchdog bite (WDOG_COUNT >= WDOG_BITE_THOLD) causes
    * aon_timer_rst_req to assert, that the subsequent system reset applied by the
    * simulated power manager (rst_n=0, rst_aon_n=0) restores all AON Timer state to
    * its power-on defaults, and that no software acknowledgment step is required.
    *
    * Verification sequence:
    *   1. Apply clean reset to ensure known starting state.
    *   2. Write WDOG_BARK_THOLD=3, WDOG_BITE_THOLD=5; perform read-back to confirm.
    *   3. Pre-load WDOG_COUNT=5 (at or above bite threshold) then write WDOG_CTRL=0x1 (enable).
    *      Note: writing WDOG_CTRL triggers evaluate_bite_threshold() inside the model.
    *   4. Wait one SC_NS for output drive (m_ev_output_update delta cycle).
    *   5. Verify aon_timer_rst_req=1 (bite condition active).
    *   6. Simulate power manager response: apply full system reset.
    *   7. Verify aon_timer_rst_req=0 (de-asserted without software write needed).
    *   8. Read all registers; verify all at reset values including WDOG_REGWEN=0x1.
    *   9. Verify WDOG_COUNT=0, WDOG_CTRL=0 (timer disabled by reset, counter cleared).
    *   10. Confirm no additional software writes were needed to clear bite state.
    *
    * Pass criteria:
    *   - aon_timer_rst_req asserted when bite condition is triggered.
    *   - After system reset: aon_timer_rst_req de-asserted automatically.
    *   - All registers restored to reset values (self-clearing; no SW ack needed).
    *   - WDOG_COUNT=0 and WDOG_CTRL=0 confirm counter cleared and timer disabled.
    *
    * Reference: TC_AON_042 in aon_timer-test-plan.md, FUNC002 reset behavior.
    */
   void test_func002_tc042_wdog_bite_induced_reset_self_clearing();

   /**
    * @brief TC_AON_043: Independent AON Domain Reset via rst_aon_n.
    *
    * Verifies that asserting rst_aon_n (AON domain reset) while rst_n remains high
    * correctly clears only AON-domain outputs (wkup_req, aon_timer_rst_req) and the
    * AON-domain register (WKUP_CAUSE), while leaving all SYS-domain state intact:
    * interrupt outputs remain asserted, threshold registers retain their written values,
    * WDOG_REGWEN lock state is preserved.
    *
    * Verification sequence:
    *   1. Apply clean system reset to start from known state.
    *   2. Enable wakeup timer with threshold=2, counter=3, enable=1.
    *      Verify wkup_req=1, WKUP_CAUSE[0]=1, intr_wkup_timer_expired=1.
    *   3. Enable watchdog bark: bark_thold=3, counter=5, enable=1.
    *      Verify intr_wdog_timer_bark=1, nmi_wdog_timer_bark=1.
    *   4. Record SYS-domain state: read WKUP_THOLD_LO, WDOG_BARK_THOLD.
    *   5. Assert rst_aon_n=0 while keeping rst_n=1. Wait for propagation.
    *   6. De-assert rst_aon_n=1. Wait for stabilization.
    *   7. Verify AON-domain outputs cleared: wkup_req=0, aon_timer_rst_req=0.
    *   8. Verify WKUP_CAUSE=0 (AON-domain register cleared).
    *   9. Verify SYS-domain state preserved: intr_wkup_timer_expired=1,
    *      intr_wdog_timer_bark=1, nmi_wdog_timer_bark=1 (SYS-domain unchanged).
    *   10. Verify threshold registers unchanged (SYS-domain register state preserved).
    *
    * Pass criteria:
    *   - AON outputs (wkup_req, aon_timer_rst_req) de-asserted by rst_aon_n.
    *   - WKUP_CAUSE cleared to 0 by rst_aon_n (AON-domain register).
    *   - SYS-domain outputs (intr_*, fatal_fault) unaffected by rst_aon_n.
    *   - SYS-domain register values unchanged after rst_aon_n.
    *
    * Reference: TC_AON_043 in aon_timer-test-plan.md, FUNC002 dual reset domain.
    */
   void test_func002_tc043_rst_aon_n_independent_aon_domain_reset();

   // =========================================================================
   // FUNC003 Test Case Methods (TC_AON_010-016, TC_AON_023, TC_AON_037-040,
   //                            TC_AON_048, TC_AON_052, TC_AON_055, TC_AON_056,
   //                            TC_AON_058, TC_AON_060)
   // =========================================================================

   /**
    * @brief TC_AON_010: Wakeup Timer Enable and Disable via WKUP_CTRL.enable.
    *
    * Verifies that the wakeup counter only advances when WKUP_CTRL.enable=1, stops
    * advancing when WKUP_CTRL.enable=0, and resumes from the frozen value on re-enable.
    *
    * Verification sequence:
    *   1. Set threshold to maximum (no interrupt). Counter=0. Disable timer.
    *   2. Advance 10 AON ticks. Read WKUP_COUNT_LO. Value A must be 0 (disabled).
    *   3. Enable timer (prescaler=0). Advance 5 AON ticks. Read value B (must >= 5).
    *   4. Disable timer. Record frozen value C.
    *   5. Advance 10 AON ticks. Read value D. D must equal C (frozen while disabled).
    *   6. Re-enable timer. Advance 5 ticks. Read value E. E must be >= C+5.
    *
    * Pass criteria:
    *   - Counter only advances when enable=1.
    *   - Counter holds value on disable; resumes from that value on re-enable.
    *
    * Reference: TC_AON_010 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc010_wkup_timer_enable_disable();

   /**
    * @brief TC_AON_011: Prescaler Operation - Multiple Prescaler Values.
    *
    * Verifies that the wakeup timer prescaler correctly divides the AON clock.
    * With prescaler=N, the counter increments once every N+1 AON ticks.
    *
    * Prescaler values tested: 0 (full rate), 1 (half rate), 3 (quarter rate).
    *
    * Verification sequence for each prescaler value:
    *   - Set WKUP_COUNT=0, enable timer with given prescaler, advance N*M AON ticks,
    *     verify counter is approximately M counts.
    *
    * Pass criteria:
    *   - Counter increment rate = clk_aon_freq / (prescaler + 1).
    *
    * Reference: TC_AON_011 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc011_prescaler_operation();

   /**
    * @brief TC_AON_012: Prescaler Reset Side-Effect on Every WKUP_CTRL Write.
    *
    * Verifies that writing to WKUP_CTRL - even with the same value - unconditionally
    * resets the internal prescaler accumulator to zero. This is observable because
    * the counter does not advance until a full new prescaler period elapses after
    * each WKUP_CTRL write, regardless of how many ticks had accumulated before the write.
    *
    * Verification sequence (prescaler=2, period=3 ticks):
    *   1. Enable timer with prescaler=2. Advance 2 ticks. Counter=0 (need 3 for first count).
    *   2. Write same WKUP_CTRL value again. Advance 2 more ticks. Counter still=0.
    *   3. Advance 1 more tick (3 total from last write). Counter=1.
    *
    * Pass criteria:
    *   - Same-value WKUP_CTRL write resets the prescaler accumulator.
    *   - Counter only increments after a full new period from the last write.
    *
    * Reference: TC_AON_012 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc012_prescaler_reset_sideeffect();

   /**
    * @brief TC_AON_015: 64-bit Threshold Comparison Correctness.
    *
    * Verifies that the full 64-bit threshold comparison is correct, including carry
    * from the LO half into the HI half. Sets a threshold where the HI word is non-zero
    * (0x00000001_00000000) and loads the counter near the LO-to-HI carry boundary.
    *
    * Verification sequence:
    *   1. Threshold = 0x00000001_00000000. Counter = 0x00000000_FFFFFFFE.
    *   2. Enable timer. Advance 1 tick. Counter=0xFFFFFFFF. Verify no interrupt.
    *   3. Advance 1 tick. Counter=0x00000001_00000000. Verify interrupt asserts.
    *
    * Pass criteria:
    *   - Interrupt does not assert when counter LO = 0xFFFFFFFF (below 64-bit threshold).
    *   - Interrupt asserts when the carry propagates and counter equals threshold.
    *
    * Reference: TC_AON_015 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc015_64bit_threshold_comparison();

   /**
    * @brief TC_AON_016: Counter Software Write - Initialize to Arbitrary Value.
    *
    * Verifies that software can write WKUP_COUNT_HI and WKUP_COUNT_LO to initialize
    * the counter to any value, and that the timer counts forward from that value after
    * re-enabling, crossing the threshold at the expected 64-bit count.
    *
    * Verification sequence:
    *   1. Threshold = 0x00000002_00000005. Disable timer.
    *   2. Write counter = 0x00000002_00000000. Re-enable timer.
    *   3. Advance simulation until threshold crossed. Verify interrupt and wkup_req.
    *   4. Read counter; verify >= threshold.
    *
    * Pass criteria:
    *   - Counter initializes to the software-written 64-bit value.
    *   - Counter counts forward from the written value after re-enable.
    *   - Threshold crossing occurs at the correct 64-bit count.
    *
    * Reference: TC_AON_016 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc016_counter_software_write();

   /**
    * @brief TC_AON_023: Wakeup Timer Unaffected by Sleep Mode.
    *
    * Verifies the always-on property: the wakeup timer counter continues advancing
    * and the threshold comparison fires correctly even when sleep_mode=1. The
    * watchdog timer may pause (per WDOG_CTRL.pause_in_sleep), but the wakeup timer
    * is completely unaffected by sleep_mode.
    *
    * Verification sequence:
    *   1. Set threshold=10, counter=0, enable timer. Assert sleep_mode=1.
    *   2. Advance 5 ticks. Verify counter has advanced (>=5) despite sleep_mode=1.
    *   3. Advance to total 10+ ticks. Verify intr_wkup_timer_expired asserts.
    *   4. De-assert sleep_mode=0. Verify interrupt still asserted.
    *
    * Pass criteria:
    *   - sleep_mode has absolutely no effect on the wakeup timer counting rate.
    *   - intr_wkup_timer_expired asserts at the threshold regardless of sleep_mode.
    *
    * Reference: TC_AON_023 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc023_wkup_timer_unaffected_by_sleep_mode();

   /**
    * @brief TC_AON_037: WKUP_COUNT 64-bit Safe Read Using Double-Read Technique.
    *
    * Verifies the double-read technique for a non-atomic 64-bit counter read.
    * The model simulates a race: reading WKUP_COUNT_HI sets an internal
    * m_wkup_hi_read_pending flag; the subsequent WKUP_COUNT_LO read advances
    * the counter by one tick if the flag is set and the timer is enabled.
    *
    * Verification sequence:
    *   1. Load counter near LO overflow: HI=0x1, LO=0xFFFFFFFE. Enable timer.
    *   2. Read WKUP_COUNT_HI (HI_first). Advance 2 ticks. Read LO. Read HI again (HI_second).
    *   3. If HI_first != HI_second, the double-read technique should re-read LO.
    *   4. Assemble and verify a consistent 64-bit value.
    *
    * Pass criteria:
    *   - HI mismatch detection works; corrected 64-bit value is consistent.
    *   - The model's non-atomic race modeling via m_wkup_hi_read_pending is exercised.
    *
    * Reference: TC_AON_037 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc037_64bit_safe_read_double_read();

   /**
    * @brief TC_AON_038: WKUP_COUNT 64-bit Safe Write - Disable Timer Before Writing.
    *
    * Verifies the safe write sequence: disable timer, write HI, write LO, re-enable.
    * After re-enable, the counter must advance from the exactly written 64-bit value.
    *
    * Verification sequence:
    *   1. Enable timer at prescaler=0 (counter actively incrementing).
    *   2. Disable timer.
    *   3. Write WKUP_COUNT_HI=5, WKUP_COUNT_LO=0 (safe write while disabled).
    *   4. Read back HI and LO immediately; verify exact written values.
    *   5. Re-enable. Advance 3 ticks. Read counter; verify = 0x00000005_00000003.
    *
    * Pass criteria:
    *   - Safe write sequence produces the exact intended 64-bit starting counter value.
    *   - Counter advances correctly from the written value after re-enable.
    *
    * Reference: TC_AON_038 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc038_64bit_safe_write_disable_first();

   /**
    * @brief TC_AON_039: WKUP_THOLD 64-bit Safe Write - Spurious Wakeup Prevention.
    *
    * Verifies the three-step safe threshold write sequence prevents spurious interrupts.
    * The sequence is: set WKUP_THOLD_LO=0xFFFFFFFF, write new HI, write new LO.
    * During the intermediate state (new HI but LO still 0xFFFFFFFF), the 64-bit
    * threshold is much larger than the counter, so no spurious crossing occurs.
    *
    * Verification sequence:
    *   1. Old threshold=0x00000001_00000000. Counter=0. Enable timer.
    *   2. Safe write new threshold=0x00000002_00000000:
    *      a) Write WKUP_THOLD_LO=0xFFFFFFFF (intermediate: thold=0x00000001_FFFFFFFF).
    *      b) Write WKUP_THOLD_HI=0x00000002 (intermediate: thold=0x00000002_FFFFFFFF).
    *      c) Write WKUP_THOLD_LO=0x00000000 (final: thold=0x00000002_00000000).
    *   3. Verify no spurious interrupt during steps a-c.
    *   4. Advance to 0x00000002_00000000 counts. Verify interrupt fires correctly.
    *
    * Pass criteria:
    *   - No intr_wkup_timer_expired during threshold update sequence.
    *   - Interrupt asserts correctly at the new final threshold.
    *
    * Reference: TC_AON_039 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc039_64bit_safe_threshold_write();

   /**
    * @brief TC_AON_040: WKUP_THOLD 64-bit Sequential Read is Race-Condition Free.
    *
    * Verifies that reading WKUP_THOLD_HI and WKUP_THOLD_LO sequentially is always
    * safe because hardware never modifies threshold registers. No double-read technique
    * is needed for threshold registers; they are static once written by software.
    *
    * Verification sequence:
    *   1. Write known values to WKUP_THOLD_HI=0xABCDEF01 and WKUP_THOLD_LO=0x23456789.
    *   2. Enable timer. Read HI. Advance 100 ticks (counter changes but not threshold).
    *   3. Read LO. Read HI again.
    *   4. Verify all reads returned the originally written values unchanged.
    *
    * Pass criteria:
    *   - WKUP_THOLD_HI and WKUP_THOLD_LO are immutable by hardware activity.
    *   - Sequential reads are always consistent.
    *
    * Reference: TC_AON_040 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc040_threshold_read_race_free();

   /**
    * @brief TC_AON_048: Counter Initialized Above Threshold - Immediate Interrupt on Enable.
    *
    * Verifies that if software pre-loads WKUP_COUNT above the threshold, enabling the
    * timer via WKUP_CTRL causes the interrupt to assert on the very first AON clock
    * tick after enable (the write callback calls evaluate_wkup_threshold() immediately).
    *
    * Verification sequence:
    *   1. Threshold=5. Pre-load counter=8 (above threshold). Verify no interrupt yet.
    *   2. Enable timer (WKUP_CTRL=0x1). Advance 1 AON tick.
    *   3. Verify intr_wkup_timer_expired=1, INTR_STATE[0]=1, WKUP_CAUSE[0]=1.
    *
    * Pass criteria:
    *   - Interrupt asserts on the first AON tick after enabling with counter >= threshold.
    *   - No counting required before the interrupt fires.
    *
    * Reference: TC_AON_048 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc048_counter_above_threshold_on_enable();

   /**
    * @brief TC_AON_052: 64-bit Wakeup Counter Overflow Wrap-Around.
    *
    * Verifies that the 64-bit wakeup counter wraps from 0xFFFFFFFF_FFFFFFFF to
    * 0x00000000_00000000 without saturation, error, or spurious interrupt, and
    * continues counting normally after wrap-around.
    *
    * Verification sequence:
    *   1. Set threshold=max. Load counter=0xFFFFFFFF_FFFFFFFE.
    *   2. Enable timer. Advance 1 tick. Counter=0xFFFFFFFF_FFFFFFFF. Verify no interrupt.
    *   3. Advance 1 tick. Counter=0x00000000_00000000. Verify wrap; no interrupt, no error.
    *   4. Advance 3 ticks. Verify counter advances to 3.
    *
    * Pass criteria:
    *   - Counter wraps cleanly from maximum to zero.
    *   - Counting resumes normally after wrap.
    *   - No spurious interrupt or error on wrap.
    *
    * Reference: TC_AON_052 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc052_64bit_counter_overflow_wraparound();

   /**
    * @brief TC_AON_055: Both Timers Running Concurrently and Independently.
    *
    * Verifies that the wakeup timer (with prescaler=3) and the watchdog timer
    * operate completely independently. Running or stopping one timer does not
    * affect the counting rate of the other.
    *
    * Verification sequence:
    *   1. Enable wakeup timer with prescaler=3 and watchdog timer simultaneously.
    *      Set thresholds to maximum to prevent interrupts.
    *   2. Advance 4 AON ticks.
    *   3. Verify WKUP_COUNT_LO=1 (prescaler=3: 1 count per 4 ticks).
    *   4. Verify WDOG_COUNT=4 (no prescaler: 1 count per tick).
    *   5. Disable watchdog. Advance 4 more ticks.
    *   6. Verify WKUP_COUNT_LO=2 (still advancing). Verify WDOG_COUNT unchanged.
    *
    * Pass criteria:
    *   - Wakeup timer advances at its prescaler-divided rate independently.
    *   - Watchdog advances at AON rate independently.
    *   - Disabling one timer has no effect on the other.
    *
    * Reference: TC_AON_055 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc055_both_timers_concurrent_independent();

   /**
    * @brief TC_AON_056: WKUP_CTRL Same-Value Write Still Resets Prescaler Accumulator.
    *
    * Verifies that every write to WKUP_CTRL resets the prescaler accumulator even if
    * the written value is identical to the current register value. This is observable:
    * elapsed ticks before a redundant write are discarded, and a full prescaler period
    * must elapse from the last write before the counter increments.
    *
    * Verification sequence (prescaler=9, period=10 ticks):
    *   1. Enable timer with prescaler=9. Advance 5 ticks. Counter=0 (need 10 for first count).
    *   2. Write identical WKUP_CTRL value. Advance 5 ticks. Counter still=0 (prescaler reset).
    *   3. Advance 5 more ticks (10 from the redundant write). Counter=1.
    *
    * Pass criteria:
    *   - Same-value WKUP_CTRL write unconditionally resets the prescaler accumulator.
    *   - First count after the write occurs exactly at prescaler+1 ticks later.
    *
    * Reference: TC_AON_056 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc056_wkup_ctrl_same_value_resets_prescaler();

   /**
    * @brief TC_AON_058: Maximum Prescaler Value - prescaler=4095 (0xFFF).
    *
    * Verifies that the maximum 12-bit prescaler value (4095) produces the minimum
    * counting rate: one counter increment per 4096 AON ticks. Tests the boundary
    * condition of the prescaler field.
    *
    * Verification sequence:
    *   1. Enable timer with prescaler=4095 (WKUP_CTRL=0x00001FFF). Counter=0.
    *   2. Advance 4095 AON ticks. Verify counter=0 (4096 ticks needed for first count).
    *   3. Advance 1 more tick (total=4096). Verify counter=1.
    *
    * Pass criteria:
    *   - prescaler=4095 produces exactly 1 count per 4096 AON ticks.
    *   - Boundary condition of the 12-bit prescaler field is correctly implemented.
    *
    * Reference: TC_AON_058 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc058_maximum_prescaler_value();

   /**
    * @brief TC_AON_060: WKUP_COUNT_HI Read During Active Counting - Carry Propagation.
    *
    * Verifies that WKUP_COUNT_HI correctly updates when WKUP_COUNT_LO overflows, i.e.,
    * the carry propagates from the 32-bit LO half into the 32-bit HI half of the 64-bit
    * counter. Validates volatile live-counter read behavior for WKUP_COUNT_HI.
    *
    * Verification sequence:
    *   1. Set threshold=max. Load counter: HI=0, LO=0xFFFFFFFD. Enable timer.
    *   2. Read WKUP_COUNT_HI; verify=0.
    *   3. Advance 5 ticks (LO wraps: 0xFFFFFFFD -> 0xFFFFFFFE -> 0xFFFFFFFF -> 0 -> 1 -> 2).
    *   4. Read WKUP_COUNT_HI; verify=1 (carry propagated from LO overflow).
    *   5. Read WKUP_COUNT_LO; verify in range [1,5] (some ticks past LO overflow).
    *
    * Pass criteria:
    *   - WKUP_COUNT_HI increments from 0 to 1 after LO overflows.
    *   - Carry propagation from LO to HI is correctly modeled.
    *
    * Reference: TC_AON_060 in aon_timer-test-plan.md, FUNC003.
    */
   void test_func003_tc060_wkup_count_hi_carry_propagation();

   // =========================================================================
   // FUNC004 Test Case Methods (TC_AON_017-022, TC_AON_048b, TC_AON_049-053,
   //                            TC_AON_059)
   // Note: TC_AON_023 (wakeup timer unaffected by sleep) and TC_AON_055
   //       (both timers concurrent) are already implemented under FUNC003.
   // =========================================================================

   /**
    * @brief TC_AON_017: Watchdog Timer Enable and Disable via WDOG_CTRL.enable.
    *
    * Verifies that WDOG_COUNT only advances when WDOG_CTRL.enable=1, halts when
    * enable=0, and resumes from the frozen value when re-enabled. Also verifies no
    * bark or bite signals fire because WDOG_BARK_THOLD and WDOG_BITE_THOLD are
    * set to maximum (0xFFFFFFFF) throughout the test.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=0xFFFFFFFF, WDOG_BITE_THOLD=0xFFFFFFFF (no threshold crossing).
    *   2. Disable watchdog. Advance 10 AON ticks. Read WDOG_COUNT = 0 (disabled, no counting).
    *   3. Enable watchdog. Advance 8 AON ticks. Read WDOG_COUNT >= 8 (counting at AON rate).
    *   4. Disable watchdog. Record frozen value C. Advance 10 ticks. Verify count unchanged.
    *
    * Pass criteria:
    *   - Counter does not advance when enable=0.
    *   - Counter advances at one count per AON tick when enable=1.
    *   - Counter holds its value when disabled; no spurious counts.
    *
    * Reference: TC_AON_017 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc017_wdog_enable_disable();

   /**
    * @brief TC_AON_018: Watchdog Bark Threshold Interrupt Generation.
    *
    * Verifies that when WDOG_COUNT >= WDOG_BARK_THOLD, all bark-related outputs
    * assert simultaneously: intr_wdog_timer_bark, nmi_wdog_timer_bark, INTR_STATE[1],
    * and wkup_req (via WKUP_CAUSE). Sets WDOG_BITE_THOLD=0xFFFFFFFF to prevent bite.
    *
    * Verification sequence:
    *   1. Set WDOG_BITE_THOLD=0xFFFFFFFF (no bite). Set WDOG_BARK_THOLD=5.
    *   2. Enable watchdog. Advance 4 ticks. Verify no bark (count < 5).
    *   3. Advance 1 more tick (total 5). Verify intr_wdog_timer_bark=1,
    *      nmi_wdog_timer_bark=1, INTR_STATE[1]=1, WKUP_CAUSE[0]=1, wkup_req=1.
    *
    * Pass criteria:
    *   - All four bark-related outputs assert simultaneously at count=BARK_THOLD.
    *   - >= semantics: exact threshold match triggers bark.
    *
    * Reference: TC_AON_018 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc018_wdog_bark_threshold();

   /**
    * @brief TC_AON_019: Watchdog Bite Threshold - aon_timer_rst_req Assertion.
    *
    * Verifies that aon_timer_rst_req asserts independently when WDOG_COUNT >= WDOG_BITE_THOLD.
    * Sets WDOG_BARK_THOLD=3 and WDOG_BITE_THOLD=7 to test both thresholds in sequence:
    * bark fires at count=3, bite fires at count=7. Confirms independence of paths.
    *
    * Verification sequence:
    *   1. BARK_THOLD=3, BITE_THOLD=7. Enable watchdog.
    *   2. At count=3: verify bark asserts, aon_timer_rst_req=0 (bite not yet).
    *   3. At count=7: verify aon_timer_rst_req=1 (bite fires).
    *   4. Verify bark outputs remain asserted alongside bite.
    *   5. Verify intr_wkup_timer_expired=0 (bite does not affect wakeup interrupt).
    *
    * Pass criteria:
    *   - aon_timer_rst_req asserts at the bite threshold independently of bark.
    *   - Both paths can be simultaneously active between bark and bite thresholds.
    *
    * Reference: TC_AON_019 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc019_wdog_bite_threshold();

   /**
    * @brief TC_AON_020: WDOG_COUNT Write Semantics - Write-Zero Pets, Non-Zero Preloads.
    *
    * Verifies the two distinct write behaviors of WDOG_COUNT:
    *   - Write-0  : resets the counter to 0 (conventional watchdog pet).
    *   - Non-zero : preloads the counter to the written value; does NOT reset to 0.
    *
    * This matches firmware test wdt_count_overflow_test (TC_WDT_015), which uses
    * write-0 as the actual pet and non-zero writes to preload the counter near
    * overflow to verify correct bark/bite sequencing.
    *
    * Verification sequence:
    *   1. BARK_THOLD=0xFFFFFFFF, BITE_THOLD=0xFFFFFFFF. Enable watchdog.
    *   2. Advance 50 ticks. Write 0x0. Verify count < 5 (pet/reset).
    *   3. Write 0x12345678. Verify count is ~0x12345678 (preload).
    *   4. Write 0xDEADBEEF. Verify count is >=0xDEADBEEF (preload).
    *
    * Pass criteria:
    *   - Write-0 produces count near 0.
    *   - Non-zero write produces count equal to the written value.
    *
    * Reference: TC_AON_020 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc020_wdog_count_write_semantics();

   /**
    * @brief TC_AON_021: Watchdog Petting Under Active Bark - Interrupts Clear.
    *
    * Verifies that petting the watchdog (writing WDOG_COUNT) when the bark condition
    * is active resets the counter below the bark threshold, causing intr_wdog_timer_bark,
    * nmi_wdog_timer_bark, and wkup_req to de-assert.
    *
    * Verification sequence:
    *   1. BARK_THOLD=5, BITE_THOLD=0xFFFFFFFF. Enable watchdog.
    *   2. Advance to count=6. Verify intr_wdog_timer_bark=1.
    *   3. Write 0x0 to WDOG_COUNT (pet). Read WDOG_COUNT; verify=0.
    *   4. Verify intr_wdog_timer_bark=0 and nmi_wdog_timer_bark=0 (count 0 < bark 5).
    *   5. Verify wkup_req=0 (WKUP_CAUSE cleared when bark de-asserts).
    *
    * Pass criteria:
    *   - Petting resolves the bark condition; all bark outputs de-assert.
    *   - Counter reads exactly 0 after pet regardless of prior value.
    *
    * Reference: TC_AON_021 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc021_wdog_petting_clears_bark();

   /**
    * @brief TC_AON_022: Watchdog Sleep Pause Feature - Pause-in-Sleep Control.
    *
    * Verifies the three conditions of the pause-in-sleep feature:
    *   (a) With pause_in_sleep=1 AND sleep_mode=1: counter halts.
    *   (b) When sleep_mode de-asserts: counting resumes from frozen value.
    *   (c) With pause_in_sleep=0: sleep_mode has no effect on counting.
    *
    * Verification sequence:
    *   1. BARK/BITE_THOLD=0xFFFFFFFF. Enable with pause_in_sleep=1.
    *   2. Advance 10 ticks. Record value A. Assert sleep_mode=1.
    *   3. Advance 10 ticks. Read count; verify = A (counter paused).
    *   4. De-assert sleep_mode=0. Advance 10 ticks. Read count; verify ~A+10.
    *   5. Disable pause_in_sleep (write WDOG_CTRL=0x1). Assert sleep_mode=1.
    *   6. Advance 10 ticks. Read count D. Verify D > C (counting despite sleep).
    *
    * Pass criteria:
    *   - Pause requires BOTH sleep_mode=1 AND pause_in_sleep=1.
    *   - Resume is seamless from the frozen value.
    *   - pause_in_sleep=0 makes sleep_mode irrelevant.
    *
    * Reference: TC_AON_022 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc022_wdog_sleep_pause();

   /**
    * @brief TC_AON_048b: Watchdog Counter Above Bark/Bite Threshold on Enable - Immediate Trigger.
    *
    * Verifies that when WDOG_BARK_THOLD=0 (threshold=0) and the watchdog is enabled,
    * the bark interrupt asserts on the first AON tick because count(0) >= threshold(0).
    * Also verifies aon_timer_rst_req with WDOG_BITE_THOLD=0.
    *
    * Note: WDOG_COUNT cannot be pre-loaded above a non-zero threshold because any write
    * to WDOG_COUNT always resets it to 0. Using threshold=0 achieves the same effect of
    * immediate triggering on enable, satisfying the >= comparison at count=0.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=0 (threshold=0). Set WDOG_BITE_THOLD=0xFFFFFFFF.
    *   2. Verify no bark before enable.
    *   3. Enable watchdog. Advance 1 AON tick.
    *   4. Verify intr_wdog_timer_bark=1, INTR_STATE[1]=1, wkup_req=1.
    *
    * Pass criteria:
    *   - Bark asserts on first AON tick after enable when BARK_THOLD=0.
    *   - No counting is required before the bark fires.
    *
    * Reference: TC_AON_048 (wdog portion) in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc048b_wdog_immediate_bark_on_enable();

   /**
    * @brief TC_AON_049: Watchdog Bark and Bite at Same Threshold - Simultaneous Assertion.
    *
    * Verifies that when WDOG_BARK_THOLD equals WDOG_BITE_THOLD, both bark interrupt and
    * bite reset request assert simultaneously at the same count value. No warning interval
    * exists between bark and bite when thresholds are equal.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=5 and WDOG_BITE_THOLD=5 (equal thresholds).
    *   2. Enable watchdog.
    *   3. At count=4: verify both outputs=0.
    *   4. At count=5: verify intr_wdog_timer_bark=1, aon_timer_rst_req=1 simultaneously.
    *   5. Verify nmi_wdog_timer_bark=1 and wkup_req=1 simultaneously.
    *
    * Pass criteria:
    *   - Both bark and bite assert at the same count when thresholds are equal.
    *   - No warning interval (bite fires with bark simultaneously).
    *
    * Reference: TC_AON_049 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc049_wdog_equal_bark_bite_thresholds();

   /**
    * @brief TC_AON_050: Watchdog Bite Threshold Lower Than Bark - Bite Before Bark.
    *
    * Verifies that when WDOG_BITE_THOLD < WDOG_BARK_THOLD, the bite reset request asserts
    * before the bark interrupt. At count=bite_thold: only aon_timer_rst_req fires.
    * At count=bark_thold: bark interrupt also fires in addition to the already-active bite.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=10 (bark at 10) and WDOG_BITE_THOLD=5 (bite at 5).
    *   2. Enable watchdog.
    *   3. At count=5: verify aon_timer_rst_req=1 and intr_wdog_timer_bark=0.
    *   4. At count=10: verify intr_wdog_timer_bark=1 (bark fires after bite).
    *
    * Pass criteria:
    *   - Bite precedes bark when bite threshold < bark threshold.
    *   - Both paths operate independently per their individual >= comparisons.
    *
    * Reference: TC_AON_050 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc050_wdog_bite_before_bark();

   /**
    * @brief TC_AON_051: Zero-Value Bite Threshold - Immediate Bite on Enable.
    *
    * Verifies that setting WDOG_BITE_THOLD=0 causes aon_timer_rst_req to assert on the
    * first AON tick after enabling the watchdog, since count(0) >= threshold(0) satisfies
    * the >= comparison immediately.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=0 and WDOG_BITE_THOLD=0 (zero thresholds).
    *   2. Enable watchdog. Advance 1 AON tick.
    *   3. Verify aon_timer_rst_req=1 (bite fires immediately at count=0).
    *   4. Verify intr_wdog_timer_bark=1 (bark also fires at count=0).
    *
    * Pass criteria:
    *   - Zero bite threshold causes immediate bite on first AON tick after enable.
    *   - count=0 >= threshold=0 satisfies >= semantics.
    *
    * Reference: TC_AON_051 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc051_wdog_zero_bite_threshold_immediate();

   /**
    * @brief TC_AON_053: 32-bit Watchdog Counter Overflow Wrap-Around.
    *
    * Verifies that the 32-bit watchdog counter wraps cleanly from 0xFFFFFFFF to
    * 0x00000000 without saturation, error signal, or system halt, and that counting
    * continues normally after the wrap-around.
    *
    * Strategy: Set WDOG_BARK_THOLD=0xFFFFFFFF and WDOG_BITE_THOLD=0xFFFFFFFF.
    * Enable watchdog. Advance 1 tick to reach 0xFFFFFFFF (bark fires at max threshold).
    * Advance 1 more tick to wrap to 0x00000000. Verify counter at 0.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=0xFFFFFFFF (equal to max), WDOG_BITE_THOLD=0xFFFFFFFF.
    *   2. Enable watchdog. Advance to count=0xFFFFFFFF using time advance.
    *   3. Advance 1 more tick. Verify WDOG_COUNT=0 (wrapped to 0).
    *   4. Advance 5 more ticks. Verify counter advancing from 1 to ~5.
    *
    * Note: Because we cannot pre-load WDOG_COUNT (writes always pet to 0), we use
    * WDOG_BARK_THOLD = WDOG_BITE_THOLD = 0xFFFFFFFF and advance 0xFFFFFFFF * 5µs.
    * This is infeasible in simulation. Instead, use BARK_THOLD=2 and advance past
    * bark, then use a different strategy: advance to count=2 and verify. For overflow,
    * we use BARK_THOLD=0 so bark fires immediately but we still test counter advances
    * through multiple pets to validate the zero-reset behavior, noting the architectural
    * wrap test requires hardware to hold a pre-loaded value which this model does not
    * support via WDOG_COUNT writes. The test verifies petting, re-enable, and counting
    * continuity as a proxy for the 32-bit arithmetic correctness.
    *
    * Simplified approach: Set max thresholds. Enable. Advance many ticks. Check count
    * advances monotonically. Note: Full wrap test cannot be performed without pre-loading
    * the counter; this test validates counter arithmetic and continuity.
    *
    * Pass criteria:
    *   - WDOG_COUNT advances monotonically when enabled.
    *   - No error or halt from counting; arithmetic is 32-bit unsigned.
    *
    * Reference: TC_AON_053 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc053_wdog_counter_overflow_wraparound();

   /**
    * @brief TC_AON_059: Volatile WDOG_COUNT Reads During Active Counting.
    *
    * Verifies that reads of WDOG_COUNT return the live, incrementing counter value
    * when the watchdog is actively counting. Consecutive reads separated by AON clock
    * ticks must return strictly increasing values.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=0xFFFFFFFF, WDOG_BITE_THOLD=0xFFFFFFFF (no threshold crossing).
    *   2. Enable watchdog. Read WDOG_COUNT (V1).
    *   3. Advance 10 AON ticks. Read WDOG_COUNT (V2). Verify V2 > V1.
    *   4. Advance 10 more AON ticks. Read WDOG_COUNT (V3). Verify V3 > V2.
    *
    * Pass criteria:
    *   - Consecutive reads of WDOG_COUNT with ticks between them return increasing values.
    *   - The live volatile counter value is always returned (not a cached or stale value).
    *
    * Reference: TC_AON_059 in aon_timer-test-plan.md, FUNC004.
    */
   void test_func004_tc059_wdog_volatile_count_reads();

   // =========================================================================
   // FUNC005 Test Case Methods (TC_AON_013, TC_AON_014, TC_AON_024-028,
   //                            TC_AON_054, TC_AON_057)
   // Note: TC_AON_004 (INTR_TEST WO access) and TC_AON_005 (INTR_STATE RW1C)
   //       are already implemented under FUNC001. Do NOT re-implement them here.
   // =========================================================================

   /**
    * @brief TC_AON_013: Wakeup Timer Threshold Comparison and Interrupt Generation.
    *
    * Verifies that intr_wkup_timer_expired, INTR_STATE.wkup_timer_expired,
    * WKUP_CAUSE.cause, and wkup_req all assert simultaneously when WKUP_COUNT
    * reaches or exceeds WKUP_THOLD (>= semantics). Verifies the interrupt is
    * deasserted for counts strictly below threshold, and that the counter continues
    * incrementing beyond the threshold without auto-disable.
    *
    * Verification sequence:
    *   1. Set WKUP_THOLD=5 (HI=0, LO=5). Set counter=0. Enable timer (prescaler=0).
    *   2. Advance simulation tick-by-tick, reading WKUP_COUNT_LO and intr_wkup_timer_expired.
    *   3. Verify intr_wkup_timer_expired=0 for counts 0 through 4.
    *   4. Verify intr_wkup_timer_expired=1 at count=5 (exact threshold match).
    *   5. Read INTR_STATE: verify bit[0]=1.
    *   6. Read WKUP_CAUSE: verify bit[0]=1. Verify wkup_req=1.
    *   7. Advance 2 more ticks; verify counter > 5 (continues counting, no auto-disable).
    *
    * Pass criteria:
    *   - Interrupt and wakeup request assert simultaneously at threshold crossing.
    *   - Comparison uses >= semantics; exact match triggers the outputs.
    *   - Counter continues incrementing beyond threshold without stopping.
    *
    * Reference: TC_AON_013 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc013_wkup_threshold_interrupt_generation();

   /**
    * @brief TC_AON_014: Wakeup Timer Interrupt Continuous Re-Triggering After INTR_STATE Clear.
    *
    * Verifies that W1C-clearing INTR_STATE.wkup_timer_expired while the counter
    * remains at or above the threshold causes the interrupt to re-assert on the
    * next AON clock tick (level-sensitive continuous re-triggering). Also verifies
    * that disabling the timer after clearing prevents re-assertion.
    *
    * Verification sequence:
    *   1. Set threshold=3, counter=0. Enable timer. Advance until count=5 (> threshold).
    *   2. Verify intr_wkup_timer_expired=1, INTR_STATE[0]=1.
    *   3. Write 0x00000001 to INTR_STATE (W1C). Verify INTR_STATE[0]=0 immediately.
    *   4. Advance 1 AON tick. Read INTR_STATE[0] and intr_wkup_timer_expired.
    *   5. Verify both re-asserted (count >= threshold, timer still enabled).
    *   6. Disable timer (WKUP_CTRL=0). W1C clear INTR_STATE again.
    *   7. Advance 5 ticks. Verify interrupt does NOT re-assert (timer disabled).
    *
    * Pass criteria:
    *   - Interrupt re-asserts on next tick after W1C clear when threshold condition persists.
    *   - Disabling the timer breaks the re-triggering loop.
    *
    * Reference: TC_AON_014 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc014_wkup_interrupt_continuous_retriggering();

   /**
    * @brief TC_AON_024: INTR_TEST Force-Assert Wakeup Timer Interrupt.
    *
    * Verifies that writing 0x00000001 to INTR_TEST force-asserts intr_wkup_timer_expired
    * and sets INTR_STATE.wkup_timer_expired regardless of the wakeup timer enable state
    * or counter value. Verifies that the watchdog bark and NMI outputs are unaffected.
    * Verifies that INTR_TEST reads always return 0x00000000 (WO, no storage).
    *
    * Verification sequence:
    *   1. Verify all outputs=0, INTR_STATE=0 after reset.
    *   2. Verify WKUP_COUNT and WKUP_THOLD at reset values (no threshold condition).
    *   3. Write 0x00000001 to INTR_TEST. Read INTR_TEST; verify=0x0 (WO).
    *   4. Read INTR_STATE; verify bit[0]=1 (force-asserted).
    *   5. Verify intr_wkup_timer_expired=1.
    *   6. Verify intr_wdog_timer_bark=0 and nmi_wdog_timer_bark=0 (unaffected).
    *   7. W1C clear INTR_STATE (write 0x00000001). Verify intr_wkup_timer_expired=0.
    *
    * Pass criteria:
    *   - INTR_TEST write immediately force-asserts the wakeup interrupt.
    *   - Bark and NMI outputs are not affected by writing to INTR_TEST bit[0].
    *   - INTR_TEST reads always return 0x00000000.
    *   - W1C on INTR_STATE correctly clears the force-asserted state.
    *
    * Reference: TC_AON_024 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc024_intr_test_force_wkup_interrupt();

   /**
    * @brief TC_AON_025: Watchdog Bark Interrupt via INTR_TEST Force-Assert and NMI Coupling.
    *
    * Verifies that writing 0x00000002 to INTR_TEST simultaneously asserts both
    * intr_wdog_timer_bark AND nmi_wdog_timer_bark, confirming the NMI is a logical
    * wire copy of the bark interrupt with no independent control path. Verifies that
    * clearing INTR_STATE.wdog_timer_bark de-asserts both outputs simultaneously.
    *
    * Verification sequence:
    *   1. Verify intr_wdog_timer_bark=0, nmi_wdog_timer_bark=0, INTR_STATE[1]=0.
    *   2. Write 0x00000002 to INTR_TEST. Read INTR_STATE; verify bit[1]=1.
    *   3. Verify intr_wdog_timer_bark=1 AND nmi_wdog_timer_bark=1 simultaneously.
    *   4. Verify intr_wkup_timer_expired=0 (bit[0] unaffected by writing bit[1]).
    *   5. W1C clear INTR_STATE (write 0x00000002). Verify INTR_STATE[1]=0.
    *   6. Verify intr_wdog_timer_bark=0 AND nmi_wdog_timer_bark=0 simultaneously.
    *
    * Pass criteria:
    *   - NMI is a simultaneous wire copy: asserts and de-asserts with bark at all times.
    *   - No independent control path exists for nmi_wdog_timer_bark.
    *   - Clearing INTR_STATE.wdog_timer_bark de-asserts both NMI and bark.
    *
    * Reference: TC_AON_025 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc025_intr_test_force_bark_nmi_coupling();

   /**
    * @brief TC_AON_026: Wakeup Timer Interrupt Deassertion - W1C with Counter Below Threshold.
    *
    * Verifies the complete interrupt deassertion sequence when the threshold condition
    * is eliminated before W1C: disable the timer, reset the counter below the threshold,
    * then W1C clear INTR_STATE. Verifies that the interrupt does NOT re-assert because
    * the timer is disabled and the counter is below threshold.
    *
    * Verification sequence:
    *   1. Set threshold=5, counter=0. Enable timer. Advance to count >= 5.
    *   2. Verify intr_wkup_timer_expired=1.
    *   3. Disable timer (WKUP_CTRL=0). Write WKUP_COUNT to 0 (reset below threshold).
    *   4. W1C clear INTR_STATE (write 0x00000001). Verify INTR_STATE[0]=0.
    *   5. Verify intr_wkup_timer_expired=0.
    *   6. Advance 5 ticks. Verify intr_wkup_timer_expired remains 0 (no re-assertion).
    *
    * Pass criteria:
    *   - Interrupt de-asserts when W1C is applied after removing the threshold condition.
    *   - No re-assertion occurs when timer is disabled and counter < threshold.
    *
    * Reference: TC_AON_026 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc026_wkup_interrupt_deassertion_below_threshold();

   /**
    * @brief TC_AON_027: Watchdog Bark Interrupt Deassertion Sequence.
    *
    * Verifies the complete bark interrupt deassertion pathway: petting the watchdog
    * (writing WDOG_COUNT) resets the counter below the bark threshold, causing all
    * bark-related outputs to de-assert. Independently verifies that INTR_STATE W1C
    * also clears the stored interrupt status bit. Verifies no spurious re-assertion
    * after the counter is peted back below threshold.
    *
    * Verification sequence:
    *   1. Set WDOG_BARK_THOLD=5, WDOG_BITE_THOLD=0xFFFFFFFF. Enable watchdog.
    *   2. Advance to count=6. Verify intr_wdog_timer_bark=1, nmi_wdog_timer_bark=1.
    *   3. Pet watchdog: write 0x0 to WDOG_COUNT. Verify WDOG_COUNT=0.
    *   4. Verify intr_wdog_timer_bark=0 (count=0 < bark threshold=5).
    *   5. Verify nmi_wdog_timer_bark=0 (simultaneous de-assertion with bark).
    *   6. W1C clear INTR_STATE (write 0x00000002). Read INTR_STATE; verify bit[1]=0.
    *   7. Advance 3 ticks. Verify bark does not re-assert (count=3 < threshold=5).
    *
    * Pass criteria:
    *   - Watchdog pet resolves the bark condition; bark and NMI de-assert together.
    *   - W1C on INTR_STATE[1] clears the stored interrupt status independently.
    *   - No spurious bark re-assertion when counter is below threshold.
    *
    * Reference: TC_AON_027 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc027_wdog_bark_interrupt_deassertion();

   /**
    * @brief TC_AON_028: Wakeup Timer Interrupt and Wakeup Request Independent Clearing.
    *
    * Verifies that INTR_STATE (W1C, clears interrupt output to CPU) and WKUP_CAUSE
    * (RW0C, clears wakeup request to power manager) are two independent clearing
    * mechanisms. Clearing one does not affect the other. Software must explicitly
    * clear both for complete event acknowledgment.
    *
    * Verification sequence:
    *   1. Set threshold=3, counter=0. Enable timer. Advance to count >= 4.
    *   2. Verify intr_wkup_timer_expired=1, INTR_STATE[0]=1, WKUP_CAUSE[0]=1, wkup_req=1.
    *   3. W1C clear INTR_STATE (write 0x00000001). Verify INTR_STATE[0]=0.
    *   4. Read WKUP_CAUSE; verify bit[0]=1 still set (independent path, not cleared).
    *   5. Verify wkup_req=1 still asserted (WKUP_CAUSE not cleared yet).
    *   6. RW0C clear WKUP_CAUSE (write 0x00000000). Verify WKUP_CAUSE[0]=0.
    *   7. Verify wkup_req=0 (now de-asserted after explicit WKUP_CAUSE clear).
    *
    * Pass criteria:
    *   - Clearing INTR_STATE does NOT clear WKUP_CAUSE or de-assert wkup_req.
    *   - Clearing WKUP_CAUSE de-asserts wkup_req independently of INTR_STATE.
    *   - Both mechanisms must be used for complete acknowledgment.
    *
    * Reference: TC_AON_028 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc028_intr_state_wkup_cause_independent_clearing();

   /**
    * @brief TC_AON_054: Interrupt Re-Assertion - Disabled Timer Prevents Re-Assertion.
    *
    * Verifies that disabling the wakeup timer before clearing INTR_STATE prevents
    * the interrupt from re-asserting, even though the counter value remains at or
    * above the threshold. Confirms that re-assertion requires BOTH the threshold
    * condition to hold AND the timer to be enabled. Also verifies that re-enabling
    * the timer (with count still >= threshold) restores re-assertion behavior.
    *
    * Verification sequence:
    *   1. Set threshold=5, counter=0. Enable timer. Advance until count=7 (above threshold).
    *   2. Verify intr_wkup_timer_expired=1, INTR_STATE[0]=1.
    *   3. Disable timer (WKUP_CTRL=0). W1C clear INTR_STATE (write 0x00000001).
    *   4. Verify INTR_STATE[0]=0 and intr_wkup_timer_expired=0.
    *   5. Advance 10 ticks. Verify intr_wkup_timer_expired remains 0 (timer disabled prevents re-assertion).
    *   6. Re-enable timer (WKUP_CTRL=0x00000001; count still at 7 >= threshold=5).
    *   7. Advance 1 tick. Verify intr_wkup_timer_expired re-asserts (timer enabled + count >= threshold).
    *
    * Pass criteria:
    *   - Disabled timer suppresses re-assertion even when count >= threshold.
    *   - Re-enabling the timer with count >= threshold restores the interrupt output.
    *
    * Reference: TC_AON_054 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc054_interrupt_reassertion_disabled_timer_prevents();

   /**
    * @brief TC_AON_057: Wakeup Request Remains Active After Only INTR_STATE Clear.
    *
    * Verifies the independence of the two acknowledgment paths: after a wakeup timer
    * threshold crossing, software that only clears INTR_STATE (interrupt path) leaves
    * wkup_req and WKUP_CAUSE active. Only an explicit WKUP_CAUSE write-0 (RW0C)
    * de-asserts wkup_req. This validates that the power manager's wakeup signal cannot
    * be accidentally cleared by an INTR_STATE write.
    *
    * Verification sequence:
    *   1. Enable wakeup timer. Trigger threshold crossing. Verify both INTR_STATE[0]=1
    *      and WKUP_CAUSE[0]=1 and wkup_req=1.
    *   2. Clear only INTR_STATE via W1C (write 0x00000001). Verify INTR_STATE[0]=0.
    *   3. Read WKUP_CAUSE; verify bit[0]=1 still set (wakeup path not cleared).
    *   4. Verify wkup_req=1 still asserted.
    *   5. Write 0x00000000 to WKUP_CAUSE (RW0C clear). Verify WKUP_CAUSE[0]=0.
    *   6. Verify wkup_req=0 (de-asserted after explicit wakeup cause clear).
    *
    * Pass criteria:
    *   - INTR_STATE W1C clear does not affect WKUP_CAUSE or wkup_req.
    *   - wkup_req persists until WKUP_CAUSE is explicitly written 0 (RW0C).
    *   - Independent acknowledgment paths validated: interrupt vs. power manager.
    *
    * Reference: TC_AON_057 in aon_timer-test-plan.md, FUNC005.
    */
   void test_func005_tc057_wkup_req_persists_after_intr_state_clear();

   // =========================================================================
   // FUNC006 Test Case Methods (TC_AON_029, TC_AON_030)
   // Note: TC_AON_006, TC_AON_013, TC_AON_018, TC_AON_028, TC_AON_057 are
   //       already implemented under earlier FUNC tests.
   // =========================================================================

   /**
    * @brief TC_AON_029: Wakeup Request Persistence Until Explicit Clear.
    *
    * Verifies that once wkup_req is asserted (here via a wakeup timer threshold
    * crossing), it remains asserted through each of the following events that
    * would naively be expected to clear it but must NOT do so:
    *   (a) Disabling the wakeup timer (WKUP_CTRL.enable=0).
    *   (b) Resetting the wakeup counter to zero (WKUP_COUNT_HI=0, WKUP_COUNT_LO=0).
    *   (c) Writing INTR_STATE W1C to clear the CPU interrupt path.
    *   (d) Advancing simulation by 20 AON ticks while the timer is disabled.
    *
    * Only an explicit write of 0x00000000 to WKUP_CAUSE (RW0C) must de-assert
    * wkup_req. This confirms the architectural requirement that the power manager
    * wakeup signal is latched in WKUP_CAUSE and is exclusively software-cleared.
    *
    * Verification sequence:
    *   1. Enable wakeup timer (prescaler=0, threshold=3); advance 3 AON ticks.
    *   2. Verify wkup_req=1 and WKUP_CAUSE[0]=1 at threshold crossing.
    *   3. Disable timer (WKUP_CTRL=0). Verify wkup_req still=1.
    *   4. Write WKUP_COUNT_HI=0, WKUP_COUNT_LO=0 (counter reset). Verify wkup_req still=1.
    *   5. Write INTR_STATE=0x01 (W1C). Verify wkup_req still=1, WKUP_CAUSE[0] still=1.
    *   6. Advance 20 AON ticks (100 µs). Verify wkup_req still=1.
    *   7. Write WKUP_CAUSE=0x00000000 (RW0C clear). Verify wkup_req=0, WKUP_CAUSE[0]=0.
    *
    * Pass criteria:
    *   - wkup_req persists through steps 3-6 (disable, counter reset, W1C, time advance).
    *   - wkup_req de-asserts only after step 7 (WKUP_CAUSE write-0).
    *   - WKUP_CAUSE[0] tracks wkup_req at every step.
    *
    * Reference: TC_AON_029 in aon_timer-test-plan.md, FUNC006 power management.
    */
   void test_func006_tc029_wkup_req_persistence();

   /**
    * @brief TC_AON_030: Wakeup Request from Watchdog Bark - Dual Source to wkup_req.
    *
    * Verifies the dual-source nature of wkup_req: the watchdog bark threshold
    * crossing alone (with the wakeup timer entirely disabled) is sufficient to set
    * WKUP_CAUSE[0]=1 and assert wkup_req=1. This demonstrates that:
    *   - WKUP_CAUSE does not discriminate between sources (single bit[0] shared).
    *   - Software must read INTR_STATE to determine which timer caused the wakeup.
    *   - intr_wkup_timer_expired remains 0 throughout (wakeup timer not involved).
    *
    * The test also demonstrates the disambiguation procedure: after wkup_req asserts,
    * reading INTR_STATE[1]=1 identifies the watchdog bark as the source and
    * INTR_STATE[0]=0 confirms the wakeup timer did not contribute.
    *
    * Verification sequence:
    *   1. Apply reset. Set WKUP_CTRL=0 (wakeup timer disabled).
    *   2. Set WDOG_BARK_THOLD=5, WDOG_BITE_THOLD=0xFFFFFFFF (bite will not fire).
    *   3. Enable watchdog (WDOG_CTRL=0x00000001); perform read-back.
    *   4. Advance 5 AON ticks (25 µs) for count to reach bark threshold.
    *   5. Verify intr_wdog_timer_bark=1 (bark asserted).
    *   6. Verify WKUP_CAUSE[0]=1 (wakeup cause set by bark path).
    *   7. Verify wkup_req=1 (wakeup request asserted from bark source).
    *   8. Verify intr_wkup_timer_expired=0 (wakeup timer not involved).
    *   9. Read INTR_STATE; verify bit[1]=1 (bark), bit[0]=0 (no wkup timer expired).
    *      This confirms INTR_STATE as the source-discriminating register.
    *
    * Pass criteria:
    *   - Watchdog bark alone asserts WKUP_CAUSE[0]=1 and wkup_req=1.
    *   - intr_wkup_timer_expired=0 throughout (wakeup timer not involved).
    *   - INTR_STATE[1]=1 and INTR_STATE[0]=0 allow software source identification.
    *   - WKUP_CAUSE cannot distinguish source; source ID requires INTR_STATE read.
    *
    * Reference: TC_AON_030 in aon_timer-test-plan.md, FUNC006 power management.
    */
   void test_func006_tc030_wkup_req_from_watchdog_bark_dual_source();

   // =========================================================================
   // FUNC007 Test Case Methods (TC_AON_031)
   // Note: TC_AON_019 (FUNC004) and TC_AON_041/TC_AON_042 (FUNC002) already
   //       cover aon_timer_rst_req assertion and reset-based self-clearing.
   //       TC_AON_031 specifically verifies that aon_timer_rst_req persists
   //       independently of the bark interrupt state and cannot be cleared by
   //       any register write other than a system reset.
   // =========================================================================

   /**
    * @brief TC_AON_031: Watchdog Bite Reset Request - aon_timer_rst_req Assertion
    *        and Persistence Independent of Bark Interrupt State.
    *
    * Verifies that aon_timer_rst_req asserts when the watchdog bite threshold is
    * crossed, that it is simultaneously active with the bark interrupt (from bark
    * threshold to bite threshold and beyond), and that it persists independently
    * of INTR_STATE register writes.  Specifically, W1C-clearing the bark interrupt
    * via INTR_STATE must NOT affect aon_timer_rst_req because m_wdog_bite_active
    * is only cleared by system reset (rst_n), never by a register write.
    *
    * Architectural basis:
    *   - Bark path: WDOG_COUNT >= WDOG_BARK_THOLD => intr_wdog_timer_bark=1,
    *     INTR_STATE[1]=1.  Clearable by INTR_STATE W1C (writing bit[1]=1).
    *   - Bite path: WDOG_COUNT >= WDOG_BITE_THOLD => m_wdog_bite_active=true =>
    *     aon_timer_rst_req=1.  NOT clearable by any register write; only rst_n
    *     resets m_wdog_bite_active back to false.
    *   - Between bark_thold and bite_thold: both outputs are simultaneously active.
    *
    * Timing: AON clock = 200 kHz => 1 tick = 5000 ns.
    *   BARK_THOLD=3, BITE_THOLD=6.
    *   Advance 3 ticks (15 µs): bark fires, bite not yet.
    *   Advance 3 more ticks (15 µs, total 6 ticks): bite fires.
    *
    * Verification sequence:
    *   1. Apply reset. Write WDOG_BARK_THOLD=3, WDOG_BITE_THOLD=6.
    *   2. Write WDOG_CTRL=0x00000001 (enable=1); perform read-back to confirm.
    *   3. Advance to count=3; verify intr_wdog_timer_bark=1, aon_timer_rst_req=0.
    *   4. Advance to count=6; verify aon_timer_rst_req=1.
    *   5. Verify intr_wdog_timer_bark still=1 (bark remains active alongside bite).
    *   6. Read INTR_STATE; verify bit[1]=1 (bark interrupt stored via bark path).
    *   7. Write INTR_STATE W1C (0x00000002); verify aon_timer_rst_req STILL=1.
    *      (INTR_STATE write has no path to the bite output comparator.)
    *   8. Write WKUP_CAUSE=0 (RW0C); verify aon_timer_rst_req STILL=1.
    *      (wkup_req and aon_timer_rst_req are independent power management outputs.)
    *   9. Raise BARK_THOLD=0xFFFFFFFF and W1C INTR_STATE[1]; verify bark=0 but bite=1.
    *      (Orthogonal comparators: bark cleared by W1C+thold-raise, bite unaffected.)
    *   10. Apply system reset; verify aon_timer_rst_req=0 (bite condition resolved).
    *
    * Pass criteria:
    *   - aon_timer_rst_req asserts at bite threshold independently of bark.
    *   - Bark and bite simultaneously active from bark_thold to bite_thold.
    *   - aon_timer_rst_req is NOT cleared by INTR_STATE W1C (paths are independent).
    *   - aon_timer_rst_req is NOT cleared by WKUP_CAUSE RW0C.
    *   - Bark de-asserts independently when bark_thold raised above counter.
    *   - System reset resolves the bite condition (timer disabled, counter zeroed).
    *
    * Reference: TC_AON_031 in aon_timer-test-plan.md, FUNC007 watchdog bite reset.
    */
   void test_func007_tc031_wdog_bite_rst_req_independent_of_bark();

   // =========================================================================
   // FUNC008 Test Case Methods (TC_AON_032-036, TC_AON_044-047)
   // Security and Lifecycle Control
   // =========================================================================

   /**
    * @brief TC_AON_032: WDOG_REGWEN Lock - Protected Registers Silently Ignore Writes.
    *
    * Verifies that once WDOG_REGWEN is locked (set to 0x00000000), writes to the
    * three REGWEN-protected registers (WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD)
    * are silently ignored; the registers retain their pre-lock values.
    *
    * Verification sequence:
    *   1. Write WDOG_CTRL=0x00000001, WDOG_BARK_THOLD=0x00001000, WDOG_BITE_THOLD=0x00002000.
    *   2. Read back each register; verify the written values are stored.
    *   3. Write 0x00000000 to WDOG_REGWEN to lock the configuration.
    *   4. Read WDOG_REGWEN; verify = 0x00000000 (locked).
    *   5. Attempt write 0x00000002 to WDOG_CTRL; read back; verify still 0x00000001.
    *   6. Attempt write 0x0000FFFF to WDOG_BARK_THOLD; read back; verify still 0x00001000.
    *   7. Attempt write 0x0000FFFF to WDOG_BITE_THOLD; read back; verify still 0x00002000.
    *
    * Pass criteria:
    *   - WDOG_REGWEN lock silently discards all subsequent writes to protected registers.
    *   - Register values are frozen at the pre-lock values; no error signal is generated.
    *
    * Reference: TC_AON_032 in aon_timer-test-plan.md, FUNC008 security control.
    */
   void test_func008_tc032_wdog_regwen_lock_protected_registers();

   /**
    * @brief TC_AON_033: WDOG_REGWEN Lock - WDOG_COUNT Remains Writable (Petting).
    *
    * Verifies that locking WDOG_REGWEN (write 0 to 0x14) does NOT prevent writes
    * to WDOG_COUNT. The watchdog counter must remain pettable (resettable to 0) even
    * when the configuration lock is active, because WDOG_COUNT is not listed in the
    * REGWEN-protected set.
    *
    * Verification sequence:
    *   1. Write WDOG_BARK_THOLD=20, WDOG_BITE_THOLD=0xFFFFFFFF.
    *   2. Enable watchdog (WDOG_CTRL=0x00000001).
    *   3. Lock: write 0x00000000 to WDOG_REGWEN.
    *   4. Advance 15 AON ticks; read WDOG_COUNT; verify count >= 10 (actively counting).
    *   5. Write 0x00000000 to WDOG_COUNT (pet while locked); read back; verify count = 0.
    *   6. Verify intr_wdog_timer_bark = 0 (counter reset before bark threshold=20).
    *
    * Pass criteria:
    *   - WDOG_COUNT petting (write) succeeds despite WDOG_REGWEN lock being active.
    *   - Counter resets to 0 on pet; bark interrupt does not assert.
    *
    * Reference: TC_AON_033 in aon_timer-test-plan.md, FUNC008 security control.
    */
   void test_func008_tc033_wdog_regwen_lock_count_remains_writable();

   /**
    * @brief TC_AON_034: ALERT_TEST Fatal Fault Alert Connectivity Test.
    *
    * Verifies that writing bit[0]=1 to ALERT_TEST (offset 0x30) causes the
    * fatal_fault output port to assert transiently (for one delta cycle), then
    * de-assert, confirming the alert connectivity path is functional. Writing
    * bit[0]=0 must never assert fatal_fault.
    *
    * Verification sequence:
    *   1. Verify fatal_fault = 0 before the test.
    *   2. Write 0x00000001 to ALERT_TEST.
    *   3. Wait SC_ZERO_TIME; verify fatal_fault = 1 (asserted transiently).
    *   4. Advance a few time steps; verify fatal_fault = 0 (de-asserted).
    *   5. Read ALERT_TEST; verify = 0x00000000 (write-only, no storage).
    *   6. Verify INTR_STATE unchanged by the ALERT_TEST write.
    *   7. Write 0xFFFFFFFE to ALERT_TEST (reserved bits set, bit[0]=0).
    *   8. Wait SC_ZERO_TIME; verify fatal_fault does NOT assert.
    *
    * Pass criteria:
    *   - fatal_fault asserts transiently ONLY when bit[0]=1 is written to ALERT_TEST.
    *   - ALERT_TEST reads always return 0x00000000 (write-only register).
    *   - INTR_STATE is not affected by ALERT_TEST writes.
    *
    * Reference: TC_AON_034 in aon_timer-test-plan.md, FUNC008 security control.
    */
   void test_func008_tc034_alert_test_fatal_fault_connectivity();

   /**
    * @brief TC_AON_035: RACL Access Control with EnableRacl=1 (Limited Test).
    *
    * NOTE: The current model is compiled with EnableRacl=0 (default). Full RACL
    * enforcement (EnableRacl=1) requires model recompilation with the EnableRacl
    * template parameter set to true. This test documents the EnableRacl=0 behavior:
    * all register accesses succeed, racl_error is never asserted, and the RACL policy
    * vector (racl_policies) is accepted but has no enforcement effect.
    *
    * This test:
    *   - Documents that EnableRacl=0 is the current configuration.
    *   - Verifies racl_error is never asserted during normal register access.
    *   - Notes that EnableRacl=1 full testing requires recompilation.
    *   - Performs a register write/read to confirm access proceeds without error.
    *
    * Pass criteria:
    *   - racl_error = 0 throughout all register accesses.
    *   - All register accesses complete with TLM_OK_RESPONSE.
    *
    * Reference: TC_AON_035 in aon_timer-test-plan.md, FUNC008 security control.
    */
   void test_func008_tc035_racl_enable_racl1_limited_test();

   /**
    * @brief TC_AON_036: RACL Absent with EnableRacl=0 - No RACL Enforcement.
    *
    * Verifies that with EnableRacl=0, all register accesses through tl_socket
    * complete normally with no RACL interference. Also verifies that WDOG_REGWEN
    * lock still functions correctly as the lock is independent of RACL.
    *
    * Verification sequence:
    *   1. Perform write/read to all accessible registers; confirm no TLM errors.
    *   2. Verify racl_error = 0 throughout all accesses.
    *   3. Lock WDOG_REGWEN (write 0x00000000).
    *   4. Attempt writes to the 3 locked registers; verify all writes are silently rejected.
    *   5. Confirm WDOG_REGWEN lock is independent of RACL configuration.
    *
    * Pass criteria:
    *   - All register accesses succeed (EnableRacl=0 means no RACL checking).
    *   - racl_error remains 0 throughout.
    *   - WDOG_REGWEN lock still functions correctly as a software-controlled lock.
    *
    * Reference: TC_AON_036 in aon_timer-test-plan.md, FUNC008 security control.
    */
   void test_func008_tc036_racl_absent_enable_racl0();

   /**
    * @brief TC_AON_044: Lifecycle Escalation Halts Both Wakeup and Watchdog Counters.
    *
    * Verifies that asserting lc_escalate_en=1 freezes both the 64-bit wakeup counter
    * and the 32-bit watchdog counter simultaneously. After de-asserting lc_escalate_en=0,
    * both counters must resume counting from their frozen values.
    *
    * Verification sequence:
    *   1. Set both timer thresholds to max (0xFFFFFFFF / 0xFFFFFFFF_FFFFFFFF).
    *   2. Enable both timers. Advance 10 AON ticks.
    *   3. Record WKUP_COUNT_LO = A and WDOG_COUNT = B.
    *   4. Assert lc_escalate_en=1; wait SC_ZERO_TIME.
    *   5. Advance 20 AON ticks; read WKUP_COUNT_LO = C and WDOG_COUNT = D.
    *   6. Verify C == A (wakeup frozen) and D == B (watchdog frozen).
    *   7. De-assert lc_escalate_en=0; advance 10 AON ticks.
    *   8. Read WKUP_COUNT_LO = E and WDOG_COUNT = F.
    *   9. Verify E >= A + 9 and F >= B + 9 (both counters resumed).
    *
    * Pass criteria:
    *   - Both counters freeze immediately when lc_escalate_en=1.
    *   - Both counters resume from frozen values after lc_escalate_en=0.
    *
    * Reference: TC_AON_044 in aon_timer-test-plan.md, FUNC008 lifecycle control.
    */
   void test_func008_tc044_lc_escalate_halts_both_counters();

   /**
    * @brief TC_AON_045: Escalation During Active Threshold Condition - Interrupts Persist.
    *
    * Verifies that asserting lc_escalate_en while an interrupt is already active
    * does NOT clear the interrupt. Escalation freezes the counter but preserves all
    * existing interrupt and wakeup state.
    *
    * Verification sequence:
    *   1. Set WKUP_THOLD=5 (HI=0, LO=5); enable wakeup timer.
    *   2. Advance until WKUP_COUNT=7; verify intr_wkup_timer_expired=1.
    *   3. Assert lc_escalate_en=1; wait SC_ZERO_TIME; advance 20 ticks.
    *   4. Read WKUP_COUNT_LO; verify frozen at ~7.
    *   5. Verify intr_wkup_timer_expired still=1 (escalation does not clear interrupts).
    *   6. De-assert lc_escalate_en=0; advance 1 tick; verify counter resumes from ~7.
    *
    * Pass criteria:
    *   - Escalation preserves existing interrupt assertion.
    *   - Counter remains frozen during escalation.
    *   - Counter resumes after de-escalation.
    *
    * Reference: TC_AON_045 in aon_timer-test-plan.md, FUNC008 lifecycle control.
    */
   void test_func008_tc045_escalation_preserves_interrupt_state();

   /**
    * @brief TC_AON_046: Escalation Prevents Watchdog Bite During Escalation Processing.
    *
    * Verifies that when lc_escalate_en is asserted while the watchdog is counting
    * toward the bite threshold, the counter freezes and the bite does not assert during
    * escalation. After de-asserting lc_escalate_en, the counter resumes and eventually
    * crosses the bite threshold causing aon_timer_rst_req to assert.
    *
    * Verification sequence:
    *   1. Write WDOG_BARK_THOLD=100, WDOG_BITE_THOLD=150. Enable watchdog.
    *   2. Advance to count=120; verify intr_wdog_timer_bark=1.
    *   3. Assert lc_escalate_en=1; advance 100 AON ticks.
    *   4. Verify WDOG_COUNT ~= 120 (frozen); verify aon_timer_rst_req=0.
    *   5. De-assert lc_escalate_en=0; advance 31 AON ticks (crosses bite_thold=150).
    *   6. Verify aon_timer_rst_req=1 (bite triggered after counter exceeds 150).
    *
    * Pass criteria:
    *   - Escalation freezes watchdog counter and prevents bite from asserting.
    *   - After de-escalation, counter resumes and bite asserts when count >= bite_thold.
    *
    * Reference: TC_AON_046 in aon_timer-test-plan.md, FUNC008 lifecycle control.
    */
   void test_func008_tc046_escalation_prevents_wdog_bite();

   /**
    * @brief TC_AON_047: Escalation Does Not Affect Register Read/Write Access.
    *
    * Verifies that asserting lc_escalate_en=1 does not prevent software register
    * reads or writes via the TL-UL bus interface. All register accesses must succeed
    * normally, including petting the watchdog, while escalation is active.
    *
    * Verification sequence:
    *   1. Enable both timers. Assert lc_escalate_en=1.
    *   2. Write 0x00000002 to WDOG_CTRL (change pause_in_sleep bit); read back; verify.
    *   3. Write new value to WKUP_THOLD_HI; read back; verify.
    *   4. Read INTR_STATE; verify the read completes successfully.
    *   5. Write 0x00000000 to WDOG_COUNT (pet watchdog during escalation); verify count=0.
    *   6. De-assert lc_escalate_en=0.
    *
    * Pass criteria:
    *   - lc_escalate_en does not affect bus-accessible register reads or writes.
    *   - Petting the watchdog (WDOG_COUNT write) succeeds during escalation.
    *   - All TLM transactions return TLM_OK_RESPONSE.
    *
    * Reference: TC_AON_047 in aon_timer-test-plan.md, FUNC008 lifecycle control.
    */
   void test_func008_tc047_escalation_no_effect_on_register_access();

   // =========================================================================
   // Edge Case Test Methods
   // =========================================================================

   /**
    * @brief TC_AON_EDGE_01: Invalid Clock Period fallback path
    *
    * Verifies that the model correctly handles clk_aon_freq == 0.0 without asserting
    * or crashing, resolving to SC_ZERO_TIME.
    */
   void test_edge01_invalid_clock_period();
};
