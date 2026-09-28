// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file testbench.cpp
 * @brief KMAC SystemC testbench implementation
 *
 * This file implements the testbench constructor with complete port binding
 * between KMAC model and test harness, including TLM sockets, custom
 * interfaces, and control signals.
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "testbench.h"
#include "kmac_func023_test.h"
#include "kmac_func024_test.h"
#include "reg_logger.h"
#include "reg_param.h"

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// =============================================================================
// Test function implementations are now member functions defined in separate test files
// Declarations are in testbench.h

/******************************************************************************
 * @brief Testbench constructor
 *
 * Instantiates KMAC model and test harness, then binds all ports.
 ******************************************************************************/
testbench::testbench(sc_module_name name, bool en_masking)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
    , m_en_masking(en_masking)
{
    // Configure logger
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Instantiate KMAC model (DUT). The default build is masked.
    dut = new kmac_ip("kmac_dut", 0x1000, 3, m_en_masking);

    // Sync testbench logger verbosity with DUT (CCI ini may override build default)
    logger.setMaxVerbosity(dut->verbosity.get_param_value());

    REG_INFO(2, logger) << "Constructing KMAC testbench";

    // Instantiate test harness
    test = new kmac_test("kmac_test", 3);

    // Perform port binding
    bind_ports();

    // Initialize testbench
    initialize();

    // Initialize clock and reset signals
    clk_sig.write(false);            // Clock signal (initial low state)
    rst_ni_sig.write(true);          // Reset inactive (active-low)

    // Register test execution thread
    // Note: Reset sequence will be performed in run_tests() SC_THREAD
    // where wait() is allowed. Do not use wait() in constructor!
    SC_THREAD(run_tests);

    REG_INFO(2, logger) << "KMAC testbench construction complete";
}

/******************************************************************************
 * @brief Testbench destructor
 *
 * Cleans up allocated component instances.
 ******************************************************************************/
testbench::~testbench()
{
    delete dut;
    delete test;
}

/******************************************************************************
 * @brief Bind all ports between model and test harness
 *
 * Performs comprehensive port binding following SystemC TLM-2.0 best practices:
 * - TLM target socket (model) ← TLM initiator socket (test)
 * - sc_export (model) ← sc_port (test) for KeyMgr and App interfaces
 * - sc_port (model) ← sc_export (test) for Entropy interface
 * - sc_out/sc_in signal connections
 ******************************************************************************/
void testbench::bind_ports()
{
    REG_INFO(2, logger) << "Binding ports...";

    // =========================================================================
    // 1. TLM Target Socket Binding (MMIO Register Access)
    // =========================================================================
    // Test's initiator socket → Model's target socket
    test->initiator_socket.bind(dut->target_socket);
    REG_INFO(2, logger) << "  [BOUND] TLM initiator_socket → target_socket";

    // =========================================================================
    // 2. KeyMgr Sideload Interface Binding
    // =========================================================================
    // Model exports keymgr_keymgr_if, test provides via keymgr_key_port
    // Model's export ← Test's port (test port binds to test's channel)
    test->keymgr_socket.bind(dut->keymgr_tl_socket);
    REG_INFO(2, logger) << "  [BOUND] keymgr_key_export → keymgr_channel";

    // =========================================================================
    // 3. Application Interface Binding (Array)
    // =========================================================================
    // TLM pattern: Test's ports bind to model's exports
    // Model exports app_if[0..2], test ports connect to model exports
    for (unsigned int i = 0; i < 3; i++) {
        test->app_port[i].bind(dut->app_export[i]);
        REG_INFO(2, logger) << "  [BOUND] app_port[" << i << "] → app_export[" << i << "]";
    }

    // =========================================================================
    // 4. Control and Status Signal Binding
    // =========================================================================

    // Idle status: Model output → Signal → Test input
    dut->idle_o(idle_sig);
    test->idle_i(idle_sig);
    REG_INFO(2, logger) << "  [BOUND] idle_o ↔ idle_i via signal";

    // Interrupt outputs: three independent lines, matching the three PIC slots
    // sep.sv gives KMAC rather than a single OR-reduction
    dut->intr_kmac_done(intr_done_sig);
    dut->intr_fifo_empty(intr_fifo_empty_sig);
    dut->intr_kmac_err(intr_err_sig);
    REG_INFO(2, logger) << "  [BOUND] intr_kmac_done / intr_fifo_empty / "
                            "intr_kmac_err → signals";

    // Alert outputs: monitored so shadow-update and fatal faults are
    // observable at the port, not only in STATUS
    dut->alert_recov_operation_err(alert_recov_sig);
    dut->alert_fatal_fault(alert_fatal_sig);
    REG_INFO(2, logger) << "  [BOUND] alert_recov_operation_err / "
                            "alert_fatal_fault → signals";

    // Life cycle escalation: Test output → Signal → Model input
    test->lc_escalate_en_o(lc_escalate_en_sig);
    dut->lc_escalate_en_i(lc_escalate_en_sig);
    REG_INFO(2, logger) << "  [BOUND] lc_escalate_en_o ↔ lc_escalate_en_i via signal";

    // Reset: Test output → Signal → Model input
    test->rst_no(rst_ni_sig);
    dut->rst_ni(rst_ni_sig);
    REG_INFO(2, logger) << "  [BOUND] rst_no ↔ rst_ni via signal";

    // Primary clock: Test output → Signal → Model input
    test->clk_o(clk_sig);
    dut->clk_i(clk_sig);
    REG_INFO(2, logger) << "  [BOUND] clk_o ↔ clk_i via signal";

    REG_INFO(2, logger) << "Port binding complete";
}

/******************************************************************************
 * @brief Initialize testbench environment
 *
 * Performs initial setup of test harness channels and environment configuration.
 * Note: DUT registers are initialized to reset values in the DUT constructor.
 ******************************************************************************/
void testbench::initialize()
{
    REG_INFO(2, logger) << "Initializing testbench environment";

    // Initialize test harness channels
    test->clear_keymgr_key();

    // Note: app_port uses TLM pattern - no initialization needed
    // Test calls model's interface methods directly through ports

    // Note: Entropy register configuration moved to run_tests() (SC_THREAD context)
    // Cannot use wait() here as initialize() is called from constructor

    REG_INFO(2, logger) << "Testbench initialization complete";
}

/******************************************************************************
 * @brief Main test execution entry point
 *
 * Executes all test cases. This is an SC_THREAD registered in constructor.
 ******************************************************************************/
void testbench::run_tests()
{
    REG_INFO(2, logger) << "====================================================";
    REG_INFO(2, logger) << "  Starting KMAC Test Execution";
    REG_INFO(2, logger) << "====================================================";

    // Apply reset to DUT
    apply_reset();

    // An unmasked DUT cannot run the masked suite: every entropy-gated case
    // below assumes EnMasking=1. That build gets the sideload case instead.
    if (!m_en_masking) {
        test_sideload_unmasked_kmac();
        report_test_summary();
        wait(100, SC_NS);
        sc_stop();
        return;
    }

    // =========================================================================
    // Configure entropy subsystem for EnMasking=true
    // =========================================================================
    // When model instantiated with EnMasking=true (line 39), entropy_ready
    // must be set before START commands to prevent error 0x09
    // (SwHashingWithoutEntropyReady). This global configuration enables all
    // tests to run without per-test entropy setup.
    //
    // FUNC-KMAC-013 entropy tests explicitly reconfigure entropy and will
    // override these defaults (entropy mode locking allows test-specific config).
    //
    // Spec reference: kmac-detailed-design.md sections 1.5.1 (EDN Mode) and
    // 4.1.2 (SwHashingWithoutEntropyReady error 0x09)

    REG_INFO(2, logger) << "Configuring global entropy subsystem...";

    // Configure ENTROPY_PERIOD: wait_timer=5000, prescaler=0
    // Per spec line 523: "typical wait_timer value of 5000 with prescaler 0"
    uint32_t entropy_period = (5000 << 0) | (0 << 10);
    test->register_write_32(test->ENTROPY_PERIOD_OFFSET, entropy_period);
    wait(5, SC_NS);
    REG_INFO(2, logger) << "Configured ENTROPY_PERIOD: wait_timer=5000, prescaler=0";

    // Configure CFG_SHADOWED with entropy_mode=0x1 (edn_mode) and entropy_ready=1
    // Shadow register requires duplicate write sequence (spec line 754)
    // Bits: [24]=entropy_ready, [17:16]=entropy_mode
    uint32_t cfg_entropy = (0x1 << 16) | (0x1 << 24);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_entropy);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_entropy);  // Shadow duplicate write
    wait(5, SC_NS);
    REG_INFO(2, logger) << "Configured CFG_SHADOWED: entropy_mode=edn (0x1), entropy_ready=1";
    REG_INFO(2, logger) << "Global entropy configuration complete\n";

    // ==========================================================================
    // FUNC-KMAC-001: SHA3 Hash Operation Tests (Phase 1)
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-001: SHA3 Hash Operation (Phase 1) ***";

    // SHA3 Algorithm Selection Tests (TC-016 to TC-019)
    test_sha3_224_algorithm_selection();
    test_sha3_256_algorithm_selection();
    test_sha3_384_algorithm_selection();
    test_sha3_512_algorithm_selection();

    // UnexpectedModeStrength Error Tests (TC-028)
    test_sha3_invalid_strength_l128();

    // Configuration and Additional Validation Tests
    test_sha3_configuration_validation();
    test_sha3_reserved_kstrength_error();
    test_sha3_back_to_back_algorithm_switching();

    // Reset between test groups to prevent cascading failures
    apply_reset();
    REG_INFO(2, logger) << "Applied reset after FUNC-KMAC-001";

    // ==========================================================================
    // FUNC-KMAC-002: SHAKE Extendable Output Function Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-002: SHAKE Extendable Output Function ***";

    // Fixed Output Tests (TC-029, TC-030)
    test_shake128_fixed_output();
    test_shake256_fixed_output();

    // Extended Output Tests (TC-031, TC-032)
    test_shake128_extended_output();
    test_shake256_extended_output();

    // SHAKE Specific Tests (TC-033, TC-034)
    test_shake_padding_mechanism();
    test_shake_run_command_squeeze_state();

    // Invalid Strength Tests (TC-035, TC-036, TC-037)
    test_shake_invalid_strength_l224();
    test_shake_invalid_strength_l384();
    test_shake_invalid_strength_l512();

    // Corner Case Test (TC-182)
    test_corner_extended_output_many_runs();

    // ==========================================================================
    // FUNC-KMAC-003: cSHAKE Customizable Hash Function Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-003: cSHAKE Customizable Hash Function ***";

    // Empty Customization Tests (TC-038, TC-039)
    test_cshake128_with_empty_customization();
    test_cshake256_with_empty_customization();

    // Non-Empty Customization Tests (TC-040, TC-041)
    test_cshake128_with_function_name();
    test_cshake256_with_customization_string();

    // PREFIX and Padding Tests (TC-042, TC-043)
    test_cshake_prefix_expansion();
    test_cshake_padding_mechanism();

    // Extended Output Test (TC-044)
    test_cshake_extended_output();

    // ==========================================================================
    // FUNC-KMAC-004: KMAC Message Authentication Code Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-004: KMAC Message Authentication Code ***";

    // Key Length Tests (TC-045, TC-046, TC-047 to TC-051)
    test_kmac_128bit_key_256bit_output();
    test_kmac_256bit_key_256bit_output();
    test_kmac_key_length_128bit();
    test_kmac_key_length_192bit();
    test_kmac_key_length_256bit();
    test_kmac_key_length_384bit();
    test_kmac_key_length_512bit();

    // PREFIX and Function Name Tests (TC-052, TC-053)
    test_kmac_prefix_validation();
    test_kmac_incorrect_function_name_error();

    // Output Length Tests (TC-054, TC-055)
    test_kmac_output_length_encoding();
    test_kmac_extended_output();

    // Corner Case Tests (TC-172, TC-177, TC-178)
    test_corner_empty_message_kmac();
    test_corner_maximum_key_length_512bit();
    test_corner_minimum_key_length_128bit();

    // ==========================================================================
    // FUNC-KMAC-005: Software Key Management Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-005: Software Key Management ***";

    // Single-Share Key Configuration Tests (TC-056, TC-057)
    test_key_single_share_128bit();
    test_key_single_share_256bit();

    // Key Length Configuration Tests (TC-060)
    test_key_all_lengths_unmasked();

    // Key Zeroization Tests (TC-062, TC-063, TC-064)
    test_key_zeroization_on_reset();
    test_key_zeroization_on_done();
    test_key_zeroization_on_error();

    // CFG_REGWEN Protection Tests (TC-065)
    test_key_cfg_regwen_protection();

    // Security Escalation Tests (TC-192)
    test_security_key_zeroization_on_escalation();

    // ==========================================================================
    // FUNC-KMAC-006: KeyMgr Sideloaded Key Interface Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-006: KeyMgr Sideloaded Key Interface ***";

    // Sideload Enable and Key Length Tests (TC-068, TC-069, TC-070)
    test_key_sideload_enable_128bit();
    test_key_sideload_256bit_automatic_unmasking();
    test_key_sideload_keylength_override();

    // Additional Sideload Verification Tests
    test_key_sideload_toggle_switch();
    test_key_sideload_maximum_length_256bit();
    test_key_sideload_empty_message();
    test_key_sideload_back_to_back_operations();

    // Reset before application interface tests (prevent cascading failures)
    apply_reset();
    REG_INFO(2, logger) << "Applied reset before FUNC-KMAC-007 (App Interface tests)";

    // ==========================================================================
    // FUNC-KMAC-007: Application Interface - KeyMgr Hash Operations
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-007: Application Interface - KeyMgr Hash Operations ***";

    // KeyMgr Application Interface Tests (TC-093 to TC-103, TC-145)
    test_app_keymgr_kmac_operation();
    test_app_fixed_priority_arbitration();
    test_app_keymgr_data_interface();
    test_app_digest_two_share_output();
    test_app_sw_lockout_during_app_active();
    test_app_cmd_rejected_during_app_active();
    test_app_state_read_blocked_during_app_active();
    test_app_keymgr_automatic_output_length();
    test_app_empty_message_not_supported();
    test_err_code_swissuedcmdinappactive_0x03();

    // ==========================================================================
    // FUNC-KMAC-008: Application Interface - LC_CTRL Hash Operations
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-008: Application Interface - LC_CTRL Hash Operations ***";

    // LC_CTRL Application Interface Tests (TC-094, TC-096-099, TC-101)
    test_func_kmac_008_lc_ctrl_operations();

    // ==========================================================================
    // FUNC-KMAC-009: Application Interface - ROM_CTRL Hash Operations
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-009: Application Interface - ROM_CTRL Hash Operations ***";

    // Reset before ROM_CTRL tests to ensure clean FSM state after LC_CTRL tests
    apply_reset();
    wait(50, SC_NS);
    REG_INFO(2, logger) << "Applied reset before FUNC-KMAC-009 tests";

    // ROM_CTRL Application Interface Tests (TC-095 to TC-101)
    test_app_rom_ctrl_cshake256_operation();
    test_app_fixed_priority_arbitration_rom_ctrl();
    test_app_data_interface_rom_ctrl();
    test_app_digest_two_share_output_rom_ctrl();
    test_app_sw_lockout_during_app_active_rom_ctrl();
    test_app_state_read_blocked_during_app_active_rom_ctrl();

    // Additional ROM_CTRL Tests
    test_app_rom_ctrl_empty_message();
    test_app_rom_ctrl_back_to_back_operations();

    // Reset after application interface tests (prevent cascading failures)
    apply_reset();
    REG_INFO(2, logger) << "Applied reset after FUNC-KMAC-009 (App Interface tests complete)";

    // ==========================================================================
    // FUNC-KMAC-010: Message FIFO and Packer
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-010: Message FIFO and Packer ***";

    // Reset before FIFO tests to ensure clean state
    apply_reset();
    wait(50, SC_NS);

    // FIFO Status and Transition Tests (TC-120 to TC-123)
    test_fifo_depth_tracking();
    test_fifo_empty_status_on_reset();
    test_fifo_empty_to_nonempty_transition();
    test_fifo_nonempty_to_empty_transition();

    // FIFO Full Condition and Backpressure Tests (TC-124 to TC-126)
    test_fifo_full_condition();
    test_fifo_full_backpressure_blocking();
    test_fifo_pass_through_mode();

    // Address Window and Multi-Granularity Write Tests (TC-127 to TC-130)
    test_fifo_address_window_abstraction();
    test_fifo_byte_write_support();
    test_fifo_halfword_write_support();
    test_fifo_word_write_support();

    // Packer and Error Tests (TC-131 to TC-134)
    test_fifo_packer_partial_entry_on_process();
    test_fifo_write_before_start_error();
    test_fifo_write_after_process_error();
    test_fifo_write_during_app_active_error();

    // Additional FIFO Tests
    test_fifo_alternating_read_write();
    test_fifo_maximum_throughput();

    // ==========================================================================
    // FUNC-KMAC-011: State Machine and Command Processing Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-011: State Machine and Command Processing ***";

    // Reset before FSM tests to ensure clean state
    apply_reset();
    wait(50, SC_NS);

    // FSM State Transition Tests (TC-071 to TC-078)
    test_fsm_reset_to_idle();
    test_fsm_idle_to_absorb_on_start();
    test_fsm_absorb_to_squeeze_on_process();
    test_fsm_squeeze_to_idle_on_done();
    test_fsm_squeeze_persistent_on_run();
    test_fsm_status_bits_mutually_exclusive();
    test_fsm_transition_sequence_complete_operation();
    test_fsm_multiple_operations_back_to_back();

    // State-Dependent Operation Tests (TC-079 to TC-083)
    test_fsm_idle_state_operations_blocked();
    test_fsm_absorb_state_operations_permitted();
    test_fsm_absorb_state_operations_blocked();
    test_fsm_squeeze_state_operations_permitted();
    test_fsm_squeeze_state_operations_blocked();

    // Sparse Command Encoding Tests (TC-084 to TC-088)
    test_cmd_sparse_encoding_start_valid();
    test_cmd_sparse_encoding_process_valid();
    test_cmd_sparse_encoding_run_valid();
    test_cmd_sparse_encoding_done_valid();
    test_cmd_sparse_encoding_invalid();

    // Command Auxiliary Bits Tests (TC-089 to TC-092)
    test_cmd_entropy_req_bit();
    test_cmd_hash_cnt_clr_bit();
    test_cmd_err_processed_bit();
    test_cmd_self_clearing_behavior();

    // Error Detection Tests (TC-151)
    test_err_code_swcmdsequence_0x08();

    // Register Callback Tests (TC-159 to TC-162, TC-169)
    test_callback_cmd_write_start_side_effects();
    test_callback_cmd_write_process_side_effects();
    test_callback_cmd_write_run_side_effects();
    test_callback_cmd_write_done_side_effects();
    test_callback_status_read_dynamic();

    // Corner Case Tests (TC-183)
    test_corner_back_to_back_operations();

    // Additional Verification Tests
    test_cfg_regwen_auto_lock_unlock();
    test_idle_o_signal_updates();

    // Reset after FSM tests (prevent cascading failures)
    apply_reset();
    REG_INFO(2, logger) << "Applied reset after FUNC-KMAC-011 (FSM tests complete)";

    // ==========================================================================
    // FUNC-KMAC-012: Endianness Configuration Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-012: Endianness Configuration ***";

    // Reset before endianness tests to ensure clean state
    apply_reset();
    wait(50, SC_NS);

    // Endianness Tests (TC-024 to TC-027, TC-142, TC-143)
    test_sha3_msg_endianness_little();
    test_sha3_msg_endianness_big();
    test_sha3_state_endianness_little();
    test_sha3_state_endianness_big();
    test_state_endianness_word_granularity();
    test_state_msg_endianness_independent();

    // ==========================================================================
    // FUNC-KMAC-013: EDN Mode Entropy Management Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-013: EDN Mode Entropy Management ***";

    // Reset before entropy tests to ensure clean state
    apply_reset();
    wait(50, SC_NS);

    // EDN Entropy Tests (TC-105, TC-107 to TC-115, TC-089)
    test_entropy_mode_edn_request();
    test_entropy_ready_assertion();
    test_entropy_mode_lock_after_ready();
    test_entropy_timeout_edn_mode();
    test_entropy_timeout_recovery();
    test_entropy_period_prescaler();
    test_entropy_period_wait_timer();
    test_entropy_refresh_hash_cnt();
    test_entropy_refresh_threshold_trigger();
    test_entropy_refresh_threshold_zero_disable();
    test_cmd_entropy_req_bit();

    // ==========================================================================
    // FUNC-KMAC-014: Software Mode Entropy Management Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-014: Software Mode Entropy Management ***";

    // Software Entropy Tests (TC-106 and extensions)
    test_entropy_mode_sw_seed();
    test_entropy_sw_mode_six_write_sequence();
    test_entropy_sw_mode_activation_after_sixth_write();
    test_entropy_sw_mode_post_activation_write_rejection();

    // ==========================================================================
    // FUNC-KMAC-015: Idle Mode Entropy Management Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-015: Idle Mode Entropy Management ***";

    // Idle Entropy Mode Tests (TC-104, TC-116 to TC-119)
    test_entropy_mode_idle();
    test_entropy_incorrect_mode_error();
    test_entropy_hashing_without_ready_error();
    test_entropy_fast_process_blocking();
    test_entropy_fast_process_nonblocking();

    // Reset after entropy tests (prevent cascading failures)
    apply_reset();
    REG_INFO(2, logger) << "Applied reset after FUNC-KMAC-015 (Entropy tests complete)";

    // ==========================================================================
    // FUNC-KMAC-017: Dynamic Register Write Protection Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-017: Dynamic Register Write Protection ***";

    // Reset before reg protection tests to ensure clean state
    apply_reset();
    wait(50, SC_NS);

    // CFG_REGWEN Protection Tests (TC-008 to TC-011, TC-065, TC-159, TC-162)
    test_cfg_regwen_protection_enable();
    test_cfg_regwen_protection_disable();
    test_cfg_regwen_auto_clear_on_start();
    test_cfg_regwen_auto_set_on_done();
    test_key_protection_cfg_regwen();
    test_callback_cmd_write_start_side_effects();
    test_callback_cmd_write_done_side_effects();

    // ==========================================================================
    // FUNC-KMAC-018: STATE Window Access Control Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-018: STATE Window Access Control ***";

    // STATE Window Access Tests (TC-135 to TC-141, TC-101, TC-170)
    test_state_read_in_squeeze_state();
    test_state_read_in_idle_returns_zero();
    test_state_read_in_absorb_returns_zero();
    test_state_two_share_masked();
    test_state_single_share_unmasked();
    test_state_share_xor_for_unmasked_digest();
    test_state_byte_halfword_word_reads();
    test_app_state_read_blocked_during_app_active();
    test_callback_state_read_conditional_access();

    // ==========================================================================
    // FUNC-KMAC-020: Reset and Initialization Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-020: Reset and Initialization ***";
 
    // Register Reset Value Tests (TC-001)
    test_reset_all_registers_default_values();
 
    // Key Zeroization Tests (TC-062)
    test_reset_key_shares_cleared();

    // FSM Initialization Tests (TC-071)
    test_reset_fsm_idle_state();

    // FIFO Status Tests (TC-121)
    test_reset_fifo_empty_status();

    // Reset During Operation Tests (TC-184, TC-185)
    test_reset_during_absorb_state();
    test_reset_during_squeeze_state();

    // ==========================================================================
    // FUNC-KMAC-021: Life Cycle Escalation Response Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-021: Life Cycle Escalation Response ***";

    // Immediate Zeroization Tests (TC-190, TC-192)
    test_escalation_immediate_key_zeroization();
    test_escalation_key_share_zeroed();

    // FSM State Tests (TC-191)
    test_escalation_fsm_invalid_state();

    // Operation Abort Tests (TC-193)
    test_escalation_abort_in_progress_operation();

    // Recovery Tests (TC-194)
    test_escalation_reset_only_recovery();

    // ==========================================================================
    // FUNC-KMAC-022: Idle Status Signaling Tests
    // ==========================================================================

    REG_INFO(2, logger) << "\n*** FUNC-KMAC-022: Idle Status Signaling ***";

    // idle_o Signal Tests (TC-071, TC-072, TC-074)
    test_idle_o_high_after_reset();
    test_idle_o_low_during_absorb();
    test_idle_o_returns_high_on_done();

    // ==========================================================================
    // FUNC-KMAC-023/024: OpenSSL delegation and timing abstraction
    // ==========================================================================
    auto run_external = [this](const char* name, auto function, auto result) {
        report_test_start(name);
        function(test);
        if (result()) {
            report_test_pass(name);
        } else {
            report_test_fail(name, "external FUNC test reported a semantic failure");
        }
    };
    run_external("FUNC023 SHA3-224", test_sha3_224_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC023 SHA3-256", test_sha3_256_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC023 SHA3-384", test_sha3_384_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC023 SHA3-512", test_sha3_512_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC023 SHAKE128", test_shake128_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC023 SHAKE256", test_shake256_openssl_delegation,
                 kmac_func023_last_result);
    run_external("FUNC024 FIFO full", test_temporal_decoupling_fifo_full,
                 kmac_func024_last_result);
    run_external("FUNC024 FIFO wait", test_msg_fifo_temporal_decoupling_wait,
                 kmac_func024_last_result);
    run_external("FUNC024 entropy latency", test_functional_entropy_latency,
                 kmac_func024_last_result);
    run_external("FUNC024 rapid commands",
                 test_rapid_command_sequence_no_timing_dependency,
                 kmac_func024_last_result);

    // =========================================================================
    // FUNC-KMAC-025: Coverage Gap Tests (TC-200 to TC-216)
    // =========================================================================
    test_intr_test_forcing();
    test_intr_enable_write();
    test_key_share_write_non_idle();
    test_key_len_write_non_idle();
    test_regwen_protected_writes_non_idle();
    test_entropy_seed_wrong_mode();
    test_incorrect_entropy_mode_error();
    test_cshake_invalid_kstrength();
    test_unknown_mode_error();
    test_run_not_in_squeeze();
    test_run_sha3_mode_rejected();
    test_cfg_shadowed_mismatch_path();
    test_msg_fifo_write_escalation();
    test_key_share_write_escalation();
    test_kmac_large_customization();
    test_state_partial_read();
    test_run_kmac_exhaust_output();
    test_sideload_key_len_clamp();

    // =========================================================================
    // FUNC-KMAC-016 and FUNC-KMAC-019: shadow protection and error reporting
    // =========================================================================
    test_func_kmac_016_shadow_protection();
    test_func_kmac_019_error_reporting();
    test_func_kmac_026_rejection_paths();

    // Wait for simulation time to advance
    wait(1, SC_MS);

    // Print test summary
    report_test_summary();

    // Stop simulation
    wait(100, SC_NS);
    sc_stop();
}

/******************************************************************************
 * @brief Apply reset to DUT
 *
 * Asserts active-low reset signal, waits for reset propagation, then deasserts.
 * Follows OTBN/HMAC reset pattern for proper hardware initialization.
 ******************************************************************************/
void testbench::apply_reset()
{
    REG_INFO(2, logger) << "Asserting reset (rst_ni = 0)";
    rst_ni_sig.write(false);
    wait(10, SC_NS);

    REG_INFO(2, logger) << "Deasserting reset (rst_ni = 1)";
    rst_ni_sig.write(true);
    wait(30, SC_NS);  // Wait for reset to complete including internal initialization

    REG_INFO(2, logger) << "Reset complete - DUT ready";
}



/******************************************************************************
 * @brief Ensure FSM is in IDLE state before running tests
 *
 * Checks FSM state and issues DONE command if in SQUEEZE state, or applies
 * reset if in an unrecoverable state. This provides test isolation when
 * tests run sequentially and a prior test left FSM in non-IDLE state.
 ******************************************************************************/
bool testbench::ensure_fsm_idle()
{
    // Read STATUS to check current FSM state
    uint32_t status = 0;
    test->register_read_32(test->STATUS_OFFSET, status);

    bool sha3_idle = (status & 0x1) != 0;
    bool sha3_absorb = (status & 0x2) != 0;
    bool sha3_squeeze = (status & 0x4) != 0;

    // If already IDLE, nothing to do
    if (sha3_idle) {
        return true;
    }

    // If in SQUEEZE state, issue DONE command to return to IDLE
    if (sha3_squeeze) {
        REG_INFO(2, logger) << "ensure_fsm_idle: FSM in SQUEEZE, issuing DONE command (0x16)";
        test->register_write_32(test->CMD_OFFSET, 0x16);  // DONE command
        wait(50, SC_NS);  // Wait for FSM transition

        // Verify FSM is now IDLE
        test->register_read_32(test->STATUS_OFFSET, status);
        sha3_idle = (status & 0x1) != 0;
        if (sha3_idle) {
            return true;
        }
    }

    // If in ABSORB state or DONE failed, try applying reset
    if (sha3_absorb || !sha3_idle) {
        REG_INFO(2, logger) << "ensure_fsm_idle: FSM not in IDLE (absorb=" << sha3_absorb
                             << "), applying reset";
        apply_reset();
        wait(50, SC_NS);

        // Verify FSM is now IDLE
        test->register_read_32(test->STATUS_OFFSET, status);
        sha3_idle = (status & 0x1) != 0;
    }

    return sha3_idle;
}

/******************************************************************************
 * @brief Configure CFG_SHADOWED with entropy preservation
 *
 * Properly configures CFG_SHADOWED register while preserving entropy
 * configuration required when EnMasking=true. This ensures all tests
 * comply with KMAC specification requirements.
 ******************************************************************************/
void testbench::configure_cfg_shadowed_with_entropy(
    uint32_t mode,
    uint32_t kstrength,
    uint32_t kmac_en,
    uint32_t sideload,
    uint32_t msg_endianness,
    uint32_t state_endianness,
    uint32_t msg_mask)
{
    // Build CFG_SHADOWED value with ALL required fields
    // Per KMAC spec: When EnMasking=1, must include entropy_mode=0x1 and entropy_ready=1
    uint32_t cfg_val = ((kmac_en & 0x1) << 0) |           // Bit 0: kmac_en
                       ((kstrength & 0x7) << 1) |          // Bits 3:1: kstrength
                       ((mode & 0x3) << 4) |               // Bits 5:4: mode
                       ((msg_endianness & 0x1) << 8) |     // Bit 8: msg_endianness
                       ((state_endianness & 0x1) << 9) |   // Bit 9: state_endianness
                       ((sideload & 0x1) << 12) |          // Bit 12: sideload
                       (0x1 << 16) |                       // Bits 17:16: entropy_mode=0x1 (edn_mode)
                       ((msg_mask & 0x1) << 20) |          // Bit 20: msg_mask
                       (0x1 << 24);                        // Bit 24: entropy_ready=1

    // Shadow register duplicate write sequence (per spec line 754)
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/******************************************************************************
 * @brief Run test case by name
 *
 * Executes specified test case and returns pass/fail status.
 ******************************************************************************/
bool testbench::run_test(const std::string& test_name)
{
    REG_INFO(2, logger) << "Running test: " << test_name;

    // Reset before each test
    initialize();

    // Test execution will be implemented in test case files
    // For now, return success
    REG_INFO(2, logger) << "Test " << test_name << " execution placeholder";

    return true;
}

/******************************************************************************
 * @brief Report test start
 *
 * Announces test start with formatted header.
 ******************************************************************************/
void testbench::report_test_start(const std::string& test_name)
{
    // Ensure clean state before every test (prevents cascading failures)
    apply_reset();
    wait(50, SC_NS);

    REG_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================" << std::endl;
}

/******************************************************************************
 * @brief Report test pass and update statistics
 *
 * Increments pass counter and logs test pass.
 ******************************************************************************/
void testbench::report_test_pass(const std::string& test_name)
{
    m_tests_passed++;
    m_tests_run++;
    REG_INFO(1, logger) << test_name << ": PASS" << std::endl;
}

/******************************************************************************
 * @brief Report test fail and update statistics
 *
 * Increments fail counter, logs test fail, and records failed test name.
 ******************************************************************************/
void testbench::report_test_fail(const std::string& test_name,
                                 const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    if (reason.empty()) {
        REG_WARN(1, logger) << test_name << ": FAIL" << std::endl;
    } else {
        REG_WARN(1, logger) << test_name << ": FAIL - " << reason << std::endl;
    }
}

/******************************************************************************
 * @brief Report test result and update statistics
 *
 * Records test outcome and prints formatted pass/fail message.
 ******************************************************************************/
void testbench::report_test_result(const char* test_name, bool passed)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "\n========================================\n"
                             << "[*** TEST PASSED ***] " << test_name << "\n"
                             << "========================================\n" << std::endl;
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(0, logger) << "\n========================================\n"
                              << "[XXX TEST FAILED XXX] " << test_name << "\n"
                              << "========================================\n" << std::endl;
    }
}

/******************************************************************************
 * @brief Print comprehensive test summary with statistics
 *
 * Displays total tests run, passed, failed, success rate, and failed test list.
 ******************************************************************************/
void testbench::report_test_summary()
{
    std::stringstream ss;

    REG_INFO(1, logger) << "\n========================================" << std::endl;
    REG_INFO(1, logger) << "  KMAC Test Summary" << std::endl;
    REG_INFO(1, logger) << "========================================" << std::endl;

    ss << "Total Tests:  " << m_tests_run;
    REG_INFO(1, logger) << ss.str() << std::endl;

    ss.str("");
    ss << "Passed:       " << m_tests_passed << " (PASS)";
    REG_INFO(1, logger) << ss.str() << std::endl;

    ss.str("");
    ss << "Failed:       " << m_tests_failed << " (FAIL)";
    REG_INFO(1, logger) << ss.str() << std::endl;

    if (m_tests_run > 0) {
        double success_rate = (100.0 * m_tests_passed) / m_tests_run;
        ss.str("");
        ss << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%";
        REG_INFO(1, logger) << ss.str() << std::endl;
    }

    REG_INFO(1, logger) << "========================================" << std::endl;

    // Show list of failed tests if any
    if (m_tests_failed > 0) {
        REG_ERROR(0, logger) << "\nFailed Tests:" << std::endl;
        for (const auto& test : m_failed_tests) {
            ss.str("");
            ss << "  - " << test;
            REG_ERROR(0, logger) << ss.str() << std::endl;
        }
        ss.str("");
        ss << "\n[OVERALL RESULT: FAILED - " << m_tests_failed << " test(s) failed]";
        REG_ERROR(0, logger) << ss.str() << std::endl;
    } else if (m_tests_passed > 0) {
        REG_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]" << std::endl;
    } else {
        REG_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]" << std::endl;
    }

    REG_INFO(1, logger) << "========================================\n" << std::endl;
}

/******************************************************************************
 * External Test Orchestrator Functions
 ******************************************************************************/
// Defined in kmac_func008_test.cpp
extern void kmac_func008_test_main(kmac_test* test);

/******************************************************************************
 * @brief Run one FUNC-016/019 test function and report its outcome
 *
 * These test groups report through KMAC_CHECK, which only counts failures.
 * This wrapper turns that count into the pass/fail bookkeeping the rest of the
 * suite uses, and resets the DUT between cases so an error state left by one
 * does not leak into the next.
 ******************************************************************************/
void testbench::run_checked_case(const char* label,
                                 void (*body)(kmac_test*))
{
    report_test_start(label);
    apply_reset();
    kmac_check_reset();

    body(test);

    if (kmac_check_failures == 0) {
        report_test_pass(label);
    } else {
        report_test_fail(label, std::to_string(kmac_check_failures) +
                                    " check(s) failed");
    }
}

/******************************************************************************
 * @brief FUNC-KMAC-016: Configuration Shadow Register Protection
 *
 * Covers both shadowed registers, CFG_SHADOWED and
 * ENTROPY_REFRESH_THRESHOLD_SHADOWED, along with the recoverable update-error
 * alert each raises on a mismatched second write.
 ******************************************************************************/
void testbench::test_func_kmac_016_shadow_protection()
{
    run_checked_case("TC-012: test_shadow_register_cfg_shadowed_duplicate_write",
                     test_shadow_register_cfg_shadowed_duplicate_write);
    run_checked_case("TC-013: test_shadow_register_cfg_shadowed_mismatch",
                     test_shadow_register_cfg_shadowed_mismatch);
    run_checked_case("TC-014: test_shadow_register_entropy_threshold_duplicate_write",
                     test_shadow_register_entropy_threshold_duplicate_write);
    run_checked_case("TC-015: test_shadow_register_entropy_threshold_mismatch",
                     test_shadow_register_entropy_threshold_mismatch);
    run_checked_case("TC-158: test_alert_recov_ctrl_update_err_bit",
                     test_alert_recov_ctrl_update_err_bit);
    run_checked_case("TC-163: test_callback_cfg_shadowed_write_validation",
                     test_callback_cfg_shadowed_write_validation);
    run_checked_case("TC-165: test_callback_entropy_refresh_threshold_validation",
                     test_callback_entropy_refresh_threshold_validation);
}

/******************************************************************************
 * @brief FUNC-KMAC-019: Error Detection and Reporting
 *
 * Walks the ERR_CODE enumeration and the alert bits, including the error
 * recovery sequence driven by CMD.err_processed.
 ******************************************************************************/
void testbench::test_func_kmac_019_error_reporting()
{
    run_checked_case("TC-140: test_err_code_keynotvalid_0x01",
                     test_err_code_keynotvalid_0x01);
    run_checked_case("TC-141: test_err_code_swpushedmsgfifo_0x02",
                     test_err_code_swpushedmsgfifo_0x02);
    run_checked_case("TC-142: test_err_code_swissuedcmdinappactive_0x03_detailed",
                     test_err_code_swissuedcmdinappactive_0x03_detailed);
    run_checked_case("TC-147: test_err_code_waittimerexpired_0x04",
                     test_err_code_waittimerexpired_0x04);
    run_checked_case("TC-148: test_err_code_incorrectentropymode_0x05",
                     test_err_code_incorrectentropymode_0x05);
    run_checked_case("TC-149: test_err_code_unexpectedmodestrength_0x06",
                     test_err_code_unexpectedmodestrength_0x06);
    run_checked_case("TC-150: test_err_code_incorrectfunctionname_0x07",
                     test_err_code_incorrectfunctionname_0x07);
    run_checked_case("TC-151: test_err_code_swcmdsequence_0x08_detailed",
                     test_err_code_swcmdsequence_0x08_detailed);
    run_checked_case("TC-152: test_err_code_swhashingwithoutentropy_0x09",
                     test_err_code_swhashingwithoutentropy_0x09);
    run_checked_case("TC-153: test_err_code_sha3control_0x80",
                     test_err_code_sha3control_0x80);
    run_checked_case("TC-154: test_err_code_persistence_across_interrupt_clear",
                     test_err_code_persistence_across_interrupt_clear);
    run_checked_case("TC-155: test_err_code_cleared_on_done",
                     test_err_code_cleared_on_done);
    run_checked_case("TC-156: test_error_recovery_sequence",
                     test_error_recovery_sequence);
    run_checked_case("TC-157: test_alert_fatal_fault_bit",
                     test_alert_fatal_fault_bit);
    run_checked_case("TC-158-Detailed: test_alert_recov_ctrl_update_err_bit_detailed",
                     test_alert_recov_ctrl_update_err_bit_detailed);
}

/******************************************************************************
 * @brief FUNC-KMAC-026: Rejection and boundary paths
 *
 * Branches taken when a command or configuration is refused, and the output
 * window boundaries of the extendable-output modes.
 ******************************************************************************/
void testbench::test_func_kmac_026_rejection_paths()
{
    run_checked_case("TC-220: test_entropy_req_outside_idle",
                     test_entropy_req_outside_idle);
    run_checked_case("TC-221: test_refresh_threshold_non_edn",
                     test_refresh_threshold_non_edn);
    run_checked_case("TC-222: test_threshold_write_during_escalation",
                     test_threshold_write_during_escalation);
    run_checked_case("TC-223: test_app_request_during_escalation",
                     test_app_request_during_escalation);
    run_checked_case("TC-224: test_msg_fifo_write_during_app_operation",
                     test_msg_fifo_write_during_app_operation);
    run_checked_case("TC-225: test_run_invalid_kstrength",
                     test_run_invalid_kstrength);
    run_checked_case("TC-226: test_run_exhausts_shake_output",
                     test_run_exhausts_shake_output);
    run_checked_case("TC-227: test_run_exhausts_kmac_output",
                     test_run_exhausts_kmac_output);
    run_checked_case("TC-228: test_state_share1_masking_disabled",
                     test_state_share1_masking_disabled);
    run_checked_case("TC-229: test_state_read_straddles_digest_end",
                     test_state_read_straddles_digest_end);
    run_checked_case("TC-230: test_keymgr_read_rejected",
                     test_keymgr_read_rejected);
    run_checked_case("TC-231: test_invalid_key_len_on_kmac_start",
                     test_invalid_key_len_on_kmac_start);
    run_checked_case("TC-232: test_escalate_with_msg_fifo_data",
                     test_escalate_with_msg_fifo_data);
    run_checked_case("TC-233: test_empty_app_message",
                     test_empty_app_message);
    run_checked_case("TC-234: test_long_customization_string",
                     test_long_customization_string);
    run_checked_case("TC-235: test_second_app_blocked_while_first_active",
                     test_second_app_blocked_while_first_active);
}

/******************************************************************************
 * @brief FUNC-KMAC-008: Application Interface - LC_CTRL Hash Operations
 *
 * Wrapper function that calls the external test orchestrator for FUNC-KMAC-008.
 * This orchestrator runs all LC_CTRL application interface test cases including:
 * - cSHAKE128 operation verification (TC-094)
 * - Fixed-priority arbitration testing (TC-096)
 * - 64-bit data interface testing (TC-097)
 * - Two-share digest output verification (TC-098)
 * - Software lockout enforcement (TC-099)
 * - STATE window read protection (TC-101)
 ******************************************************************************/
void testbench::test_func_kmac_008_lc_ctrl_operations()
{
    REG_INFO(2, logger) << "Invoking FUNC-KMAC-008 test orchestrator";
    kmac_func008_test_main(test);
    REG_INFO(2, logger) << "FUNC-KMAC-008 test orchestrator complete";
}

/******************************************************************************
 * @brief SystemC main entry point
 *
 * Instantiates testbench and executes all test cases.
 ******************************************************************************/
int sc_main(int argc, char* argv[])
{
    // Create local logger for sc_main
    RegLogger logger;
    logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // usage: kmac_testbench [config.ini] [--unmasked]
    //
    // --unmasked builds the DUT with EnMasking=false and runs the unmasked
    // sideload case instead of the masked suite. EnMasking is a const
    // constructor argument on the model, so it has to be decided out here.
    const char* config_path = nullptr;
    bool en_masking = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--unmasked") {
            en_masking = false;
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "kmac_testbench: unknown option '" << arg << "'\n"
                      << "usage: kmac_testbench [config.ini] [--unmasked]\n";
            return 1;
        } else if (config_path == nullptr) {
            config_path = argv[i];
        } else {
            std::cerr << "kmac_testbench: unexpected extra argument '" << arg
                      << "' (config file already set to '" << config_path << "')\n";
            return 1;
        }
    }
    regmodel::load_config_file(config_path);

    REG_INFO(2, logger) << "====================================================" << std::endl;
    REG_INFO(2, logger) << "  KMAC SystemC TLM Testbench" << std::endl;
    REG_INFO(2, logger) << "====================================================" << std::endl;

    // Heap-allocated so the teardown path can be run explicitly below.
    testbench* tb = new testbench("kmac_testbench", en_masking);

    REG_INFO(2, logger) << "Starting simulation..." << std::endl;

    sc_start();

    REG_INFO(2, logger) << "====================================================" << std::endl;
    REG_INFO(2, logger) << "  Simulation Complete" << std::endl;
    REG_INFO(2, logger) << "====================================================" << std::endl;

    const unsigned int failed = tb->m_tests_failed;

    // quick_exit skips destructors, so the model's cleanup path (OpenSSL
    // context teardown and the app handler arrays) never ran and could not
    // regress unnoticed. Destroy the hierarchy explicitly first; the run is
    // over, so nothing else touches it.
    delete tb;

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(failed > 0 ? 1 : 0);
    return 0;
}
