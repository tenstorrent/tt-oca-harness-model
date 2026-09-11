// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file testbench.h
 * @brief entropy_src SystemC testbench header
 *
 * Defines the top-level testbench SC_MODULE that instantiates the
 * entropy_src model (DUT) and entropy_src_test harness, performs complete
 * port binding between the two, and drives the test execution sequence.
 *
 * Port binding overview:
 *  - entropy_src_test::initiator_socket  →  entropy_src_ip::target_socket
 *    (CPU register access via TLM-2.0 b_transport; target_socket is the
 *     regmodel memory socket that serves as the reg_socket interface)
 *  - entropy_src::intr_i  ↔  sig_intr
 *  - entropy_src::intr_i          ↔  sig_intr
 *  - entropy_src::intr_i       ↔  sig_intr
 *  - entropy_src::intr_i      ↔  sig_intr
 *  - entropy_src_test::intr_i  ↔  sig_intr
 *  - entropy_src_test::intr_i          ↔  sig_intr
 *  - entropy_src_test::intr_i       ↔  sig_intr
 *  - entropy_src_test::intr_i      ↔  sig_intr
 *
 * Four test cases are executed in sequence by run_tests():
 *  1. RW Test    — write a value and read it back; assert they match.
 *  2. RO Test    — write to a read-only register; assert the value is unchanged.
 *  3. Binding    — verify that elaboration completed without errors (structural).
 *  4. Reset Test — write known values, trigger software reset, verify reset values.
 *
 * Reference:
 *   - entropy_src/docs/sections/entropy_src-port-interfaces.md
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "entropy_src.h"
#include "entropy_src_test.h"
#include "reg_logger.h"

#include <cstddef>
#include <cstdint>
#include <string>

/******************************************************************************
 * @class testbench
 * @brief Top-level entropy_src testbench
 *
 * Instantiates the entropy_src DUT and entropy_src_test harness, connects
 * all five port interfaces (one TLM socket and four interrupt signals), and
 * executes the mandatory set of four test cases.
 *
 * Test pass/fail tracking uses three integer counters:
 *   m_tests_run, m_tests_passed, m_tests_failed.
 * A test summary is printed via REG_INFO at the end of run_tests() before
 * sc_stop() is called.
 ******************************************************************************/
class testbench : public sc_module
{
public:
    RegLogger logger;

    SC_HAS_PROCESS(testbench);

    /**
     * @brief Constructor
     *
     * Instantiates the DUT and test harness, performs all port bindings,
     * initialises interrupt signals to false, and registers the run_tests
     * SC_THREAD.
     *
     * @param name SystemC module name
     */
    explicit testbench(sc_module_name name);

    /// @brief Destructor — releases heap-allocated DUT and test harness
    ~testbench();

    // =========================================================================
    // SC_THREAD
    // =========================================================================

    /**
     * @brief Main test execution SC_THREAD
     *
     * Registered as SC_THREAD in the constructor.  Execution sequence:
     *  1. Apply software reset to the DUT.
     *  2. Run RW register test (test_rw).
     *  3. Run RO register test (test_ro).
     *  4. Run port binding verification test (test_binding).
     *  5. Run reset functionality test (test_reset).
     *  6. Print test summary via REG_INFO.
     *  7. Call sc_stop().
     */
    void run_tests();

private:

    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * @brief Apply a software reset to the DUT via CTRL.RESET
     *
     * Writes 0x1 to CTRL (offset 0x04) through the test harness, which
     * triggers the entropy_src::handle_write_CTRL callback to execute the
     * full reset sequence.
     */
    void apply_reset();

    /**
     * @brief Apply a hardware reset to the DUT via rst_ni signal.
     *
     * Asserts rst_ni low, waits for a delta cycle, then releases rst_ni high
     * and waits for the background thread to re-start.
     */
    void apply_hw_reset();

    /**
     * @brief Bind all ports between the DUT and the test harness
     *
     * Connects:
     *   - test->initiator_socket  →  dut->reg_socket
     *   - dut->intr_i  ↔  sig_intr
     *   - dut->intr_i          ↔  sig_intr
     *   - dut->intr_i       ↔  sig_intr
     *   - dut->intr_i      ↔  sig_intr
     *   - test->intr_i ↔  sig_intr
     *   - test->intr_i         ↔  sig_intr
     *   - test->intr_i      ↔  sig_intr
     *   - test->intr_i     ↔  sig_intr
     */
    void bind_ports();

    // =========================================================================
    // Test cases
    // =========================================================================

    /**
     * @brief RW register test
     *
     * Objective: verify that writable register fields retain the written
     * value when read back.
     *
     * Procedure:
     *  - Write a known 32-bit pattern to CTRL (writable bits 0, 4, 8).
     *  - Read back CTRL and verify the read value matches the written value
     *    masked by the register's write/read mask (0x111).
     *  - Write and read back INTR_ENABLE (write mask 0x1111).
     *
     * Pass criterion: read_value == (write_value & write_read_mask) for each
     * register tested.
     *
     * @return true if all assertions pass
     */
    bool test_rw();

    /**
     * @brief RO register test
     *
     * Objective: verify that read-only register fields reject write attempts
     * and retain their reset value.
     *
     * Procedure:
     *  - Read the reset value of COMPONENT_ID (RO, reset = 0x01000001).
     *  - Attempt to write 0xDEADBEEF to COMPONENT_ID.
     *  - Read back COMPONENT_ID and verify it still holds 0x01000001.
     *
     * Pass criterion: read_after_write == reset_value (write had no effect).
     *
     * @return true if all assertions pass
     */
    bool test_ro();

    /**
     * @brief Port binding verification test
     *
     * Objective: confirm that all five port interfaces are properly bound
     * by performing a simple TLM transaction and observing a non-error
     * response status.
     *
     * Procedure:
     *  - Issue a 32-bit read of COMPONENT_ID via test->register_read_32.
     *  - Verify the returned value equals the expected reset value 0x01000001.
     *
     * Pass criterion: returned value equals 0x01000001 (confirms socket binding
     * and correct data path through the regmodel memory layer).
     *
     * @return true if the binding assertion passes
     */
    bool test_binding();

    /**
     * @brief Reset functionality test
     *
     * Objective: verify that writing CTRL.RESET = 1 restores all registers
     * to their hardware reset values and de-asserts the interrupt outputs.
     *
     * Procedure:
     *  1. Write known non-reset values to CTRL (0x110) and INTR_ENABLE (0xF).
     *  2. Trigger software reset by writing 0x1 to CTRL.
     *  3. Read back CTRL and INTR_ENABLE; verify they equal their reset values.
     *  4. Read the interrupt signal values via the sc_in<bool> ports; verify
     *     all are false.
     *
     * Pass criterion:
     *  - CTRL reads 0x0 after reset.
     *  - INTR_ENABLE reads 0x0 after reset.
     *  - All four interrupt output signals are false after reset.
     *
     * @return true if all assertions pass
     */
    bool test_reset();

    // =========================================================================
    // FUNC-001 test cases — TLM Register Transport Interface
    // =========================================================================

    /**
     * @brief TC-F001-001: COMPONENT_ID reset value
     *
     * Objective: verify that a TLM b_transport READ of COMPONENT_ID (0x00)
     * returns the build-time constant 0x01000001 immediately after reset.
     *
     * Pass criterion: read_value == 0x01000001.
     *
     * Test plan reference: component_id_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f001_component_id_reset_value();

    /**
     * @brief TC-F001-003: COMPONENT_ID write has no effect (RO enforcement)
     *
     * Objective: confirm that writing 0xDEADBEEF to the RO COMPONENT_ID
     * register leaves its stored value unchanged at 0x01000001.
     *
     * Procedure:
     *  - Record reset value of COMPONENT_ID.
     *  - Write 0xDEADBEEF to COMPONENT_ID via b_transport.
     *  - Read back COMPONENT_ID; assert it still equals the reset value.
     *
     * Pass criterion: value_after_write == COMPONENT_ID_RESET (0x01000001).
     *
     * Test plan reference: component_id_write_has_no_effect
     *
     * @return true if assertion passes
     */
    bool tc_f001_component_id_write_has_no_effect();

    /**
     * @brief TC-F001-009: STATUS register always zero
     *
     * Objective: confirm that STATUS (0x08) is RO with all-reserved bits;
     * write of 0xFFFFFFFF must be ignored, and the read must return 0x00000000.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to STATUS.
     *  - Read back STATUS; assert the returned value is 0x00000000.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: status_register_always_zero
     *
     * @return true if assertion passes
     */
    bool tc_f001_status_register_always_zero();

    /**
     * @brief TC-F001-018: INTR_ENABLE write mask validation
     *
     * Objective: validate that regmodel enforces the 0x00001111 write mask on
     * INTR_ENABLE (0x14), discarding bits outside that mask on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF (all-ones) to INTR_ENABLE.
     *  - Read back INTR_ENABLE; assert read_value == 0x00001111.
     *  - Write 0x00000000 to INTR_ENABLE.
     *  - Read back INTR_ENABLE; assert read_value == 0x00000000.
     *
     * Pass criterion: reserved bits read as zero; writeable bits retain value.
     *
     * Test plan reference: intr_enable_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f001_intr_enable_write_mask_validation();

    /**
     * @brief TC-F001-020: INTR_TEST is write-only — reads return zero
     *
     * Objective: confirm WO access semantics on INTR_TEST (0x18); regmodel must
     * return 0x00000000 on every read regardless of any prior write.
     *
     * Procedure:
     *  - Write 0x00001111 to INTR_TEST.
     *  - Read INTR_TEST; assert read_value == 0x00000000.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: intr_test_is_write_only_reads_zero
     *
     * @return true if assertion passes
     */
    bool tc_f001_intr_test_is_write_only_reads_zero();

    /**
     * @brief TC-F001-044: FIFO_STATUS is read-only
     *
     * Objective: confirm RO enforcement on FIFO_STATUS (0x24); write of
     * 0xFFFFFFFF must have no observable effect.
     *
     * Procedure:
     *  - Record the current reset value of FIFO_STATUS (expected 0x00000000).
     *  - Write 0xFFFFFFFF to FIFO_STATUS.
     *  - Read back FIFO_STATUS; assert value equals the recorded reset value.
     *
     * Pass criterion: read_value == FIFO_STATUS_RESET (0x00000000).
     *
     * Test plan reference: fifo_status_is_read_only
     *
     * @return true if assertion passes
     */
    bool tc_f001_fifo_status_is_read_only();

    /**
     * @brief TC-F001-065: HEALTH_TEST_STATUS is read-only
     *
     * Objective: confirm RO enforcement on HEALTH_TEST_STATUS (0x40); write
     * of 0xFFFFFFFF must have no observable effect.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to HEALTH_TEST_STATUS.
     *  - Read back HEALTH_TEST_STATUS; assert read_value == 0x00000000.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: health_test_status_is_read_only
     *
     * @return true if assertion passes
     */
    bool tc_f001_health_test_status_is_read_only();

    /**
     * @brief TC-F001-068: REPETITION_TEST_COUNT is read-only
     *
     * Objective: confirm RO enforcement on REPETITION_TEST_COUNT (0x44);
     * write of 0xFFFFFFFF must be ignored.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to REPETITION_TEST_COUNT.
     *  - Read back REPETITION_TEST_COUNT; assert read_value == 0x00000000.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: repetition_test_count_is_read_only
     *
     * @return true if assertion passes
     */
    bool tc_f001_repetition_test_count_is_read_only();

    /**
     * @brief TC-F001-107: GENERATOR_HEALTH_STATUS registers are read-only
     *
     * Objective: confirm RO enforcement on GENERATOR_0_HEALTH_STATUS (0xC0)
     * as a representative member of the 12-register per-generator block.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to GENERATOR_0_HEALTH_STATUS.
     *  - Read back GENERATOR_0_HEALTH_STATUS; assert read_value == 0x00000000.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: generator_health_status_registers_are_read_only
     *
     *
     * @return true if assertion passes
     */
    bool tc_f001_generator_health_status_is_read_only();

    /**
     * @brief TC-F001-119: RW pattern test — CTRL write mask 0x03FF0111
     *
     * Objective: validate that CTRL (0x04) write mask 0x03FF0111 is correctly
     * enforced; reserved bits outside the mask always read as zero.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF (all-ones) to CTRL.
     *  - Read back CTRL; assert read_value == 0x03FF0111.
     *  - Write 0x00000000 (all-zeros) to CTRL; confirm readback == 0x00000000.
     *  - Write pattern 0x01010101 to CTRL; confirm readback ==
     *    (0x01010101 & 0x03FF0111) = 0x01010101 & mask.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_ctrl();

    /**
     * @brief TC-F001-120: RW pattern test — DEBUG_CTRL write mask 0x000007FF
     *
     * Objective: validate that DEBUG_CTRL (0x0C) write mask 0x000007FF is
     * correctly enforced using checkerboard patterns.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to DEBUG_CTRL; assert readback == 0x000007FF.
     *  - Write 0x55555555 to DEBUG_CTRL; assert readback ==
     *    (0x55555555 & 0x000007FF).
     *  - Write 0xAAAAAAAA to DEBUG_CTRL; assert readback ==
     *    (0xAAAAAAAA & 0x000007FF).
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_debug_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_debug_ctrl();

    /**
     * @brief TC-F001-121: RW pattern test — INTR_ENABLE write mask 0x00001111
     *
     * Objective: validate that INTR_ENABLE (0x14) write mask 0x00001111 is
     * correctly enforced using checkerboard patterns.
     *
     * Procedure:
     *  - Write 0x55555555 to INTR_ENABLE; assert readback ==
     *    (0x55555555 & 0x00001111) = 0x00001111.
     *  - Write 0xAAAAAAAA to INTR_ENABLE; assert readback ==
     *    (0xAAAAAAAA & 0x00001111) = 0x00000000.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_intr_enable
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_intr_enable();

    /**
     * @brief TC-F001-122: RW pattern test — HEALTH_TEST_CTRL write mask
     *        0x0000FFFF
     *
     * Objective: validate that HEALTH_TEST_CTRL (0x30) write mask 0x0000FFFF
     * is correctly enforced using boundary patterns.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to HEALTH_TEST_CTRL; assert readback == 0x0000FFFF.
     *  - Write 0x00000000 to HEALTH_TEST_CTRL; assert readback == 0x00000000.
     *
     * Note: HEALTH_TEST_CTRL has a write callback; the read-back value is the
     * stored register value after regmodel masking, not a direct mirror.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_health_test_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_health_test_ctrl();

    /**
     * @brief TC-F001-123: RW pattern test — APT_PROPORTION registers write
     *        mask 0x000003FF
     *
     * Objective: validate write mask 0x000003FF on all four APT_PROPORTION
     * registers (0x60, 0x64, 0x68, 0x6C) simultaneously.
     *
     * Procedure (for each of the four registers):
     *  - Write 0xFFFFFFFF; assert readback == 0x000003FF.
     *  - Write reset value; assert readback == reset value.
     *
     * Pass criterion: (read_value & ~0x000003FF) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_apt_proportion_registers
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_apt_proportion_registers();

    /**
     * @brief TC-F001-124: RW pattern test — RING_OSC_ENABLE write mask
     *        0x00FFFFFF
     *
     * Objective: validate RING_OSC_ENABLE (0x90) write mask 0x00FFFFFF.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to RING_OSC_ENABLE; assert readback == 0x00FFFFFF.
     *  - Write 0x00000000 to RING_OSC_ENABLE; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_ring_osc_enable
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_ring_osc_enable();

    /**
     * @brief TC-F001-125: RW pattern test — RING_OSC_CTRL write mask 0x00000FFF
     *
     * Objective: validate RING_OSC_CTRL (0x98) write mask 0x00000FFF.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to RING_OSC_CTRL; assert readback == 0x00000FFF.
     *  - Write 0x00000000 to RING_OSC_CTRL; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_ring_osc_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_ring_osc_ctrl();

    /**
     * @brief TC-F001-126: RW pattern test — DECORRELATOR_CTRL write mask
     *        0xFFFFFFFF (fully writable)
     *
     * Objective: validate that DECORRELATOR_CTRL (0xA0) with write mask
     * 0xFFFFFFFF retains all 32 written bits using checkerboard patterns.
     *
     * Procedure:
     *  - Write 0x55555555; assert readback == 0x55555555.
     *  - Write 0xAAAAAAAA; assert readback == 0xAAAAAAAA.
     *
     * Pass criterion: read_value == write_value for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_decorrelator_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_decorrelator_ctrl();

    /**
     * @brief TC-F001-127: RW pattern test — DECORRELATOR_MASK write mask
     *        0x000000FF
     *
     * Objective: validate DECORRELATOR_MASK (0xA4) write mask 0x000000FF.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to DECORRELATOR_MASK; assert readback == 0x000000FF.
     *  - Write 0x00000000 to DECORRELATOR_MASK; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_decorrelator_mask
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_decorrelator_mask();

    /**
     * @brief TC-F001-128: RW pattern test — STARTUP_CTRL write mask 0x0000FFFF
     *
     * Objective: validate STARTUP_CTRL (0xB0) write mask 0x0000FFFF using
     * boundary patterns.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF to STARTUP_CTRL; assert readback == 0x0000FFFF.
     *  - Write 0x0000ABCD to STARTUP_CTRL; assert readback == 0x0000ABCD.
     *  - Write 0x00000000 to STARTUP_CTRL; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~mask) == 0 for all patterns.
     *
     * Test plan reference: rw_register_pattern_test_startup_ctrl
     *
     * @return true if all assertions pass
     */
    bool tc_f001_rw_pattern_test_startup_ctrl();

    /**
     * @brief TC-F001-131: RO write has no effect — Markov test registers
     *
     * Objective: confirm RO enforcement on MARKOV_TEST_COUNTS_0 (0x80),
     * MARKOV_TEST_COUNTS_1 (0x84), and MARKOV_TEST_PROBABILITIES (0x88).
     *
     * Procedure (for each of the three registers):
     *  - Write 0xFFFFFFFF.
     *  - Read back; assert read_value == 0x00000000.
     *
     * Pass criterion: all three registers return 0x00000000 after the write.
     *
     * Test plan reference: ro_register_write_has_no_effect_markov_counts
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f001_ro_write_has_no_effect_markov_counts();

    // =========================================================================
    // FUNC-002 test cases — Interrupt Controller Behavior
    // =========================================================================

    /**
     * @brief TC-F002-013: INTR_STATUS reset value
     *
     * Objective: verify INTR_STATUS (0x10) and all four interrupt output ports
     * are in their de-asserted reset state immediately after apply_reset().
     *
     * Pass criterion:
     *  - INTR_STATUS TLM read returns 0x00000000.
     *  - All four interrupt sc_in<bool> ports read as false.
     *
     * Test plan reference: intr_status_reset_value
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_status_reset_value();

    /**
     * @brief TC-F002-014: INTR_STATUS W1C set and clear — HEALTH_TEST_FAILED
     *
     * Objective: inject INTR_STATUS[0] via INTR_TEST, confirm port assertion,
     * then W1C-clear INTR_STATUS[0] and confirm deasserted.
     *
     * Pass criterion:
     *  - intr_i == true after inject with INTR_ENABLE[0]=1.
     *  - intr_i == false after W1C write to INTR_STATUS[0].
     *
     * Test plan reference: intr_status_w1c_set_clear_health_test_failed
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_status_w1c_set_clear_health_test_failed();

    /**
     * @brief TC-F002-015: INTR_STATUS W1C — writing zero does not clear
     *
     * Objective: confirm W1C semantics: writing 0x00000000 to INTR_STATUS
     * does not clear any pending bit and the interrupt port remains asserted.
     *
     * Pass criterion: intr_i remains true after zero-write.
     *
     * Test plan reference: intr_status_w1c_write_zero_does_not_clear
     *
     * @return true if assertion passes
     */
    bool tc_f002_intr_status_w1c_write_zero_does_not_clear();

    /**
     * @brief TC-F002-016: INTR_STATUS reserved bits always read as zero
     *
     * Objective: confirm only bits [0], [4], [8], [12] of INTR_STATUS can be
     * set; all reserved positions read as zero (TLM read_bit_mask=0 enforced).
     *
     * Pass criterion:
     *  - All four ports assert after injecting INTR_ALL_BITS_MASK with INTR_ENABLE=0x1111.
     *  - INTR_STATUS TLM read returns 0x00000000 (read restriction).
     *
     * Test plan reference: intr_status_reserved_bits_always_zero
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_status_reserved_bits_always_zero();

    /**
     * @brief TC-F002-017: INTR_ENABLE reset value
     *
     * Objective: confirm INTR_ENABLE (0x14) reads as 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: intr_enable_reset_value
     *
     * @return true if assertion passes
     */


    /**
     * @brief TC-F002-021: INTR_TEST injects HEALTH_TEST_FAILED (bit 0)
     *
     * Objective: confirm INTR_TEST[0]=1 asserts intr_i port
     * when INTR_ENABLE[0]=1 via the handle_write_INTR_TEST inject path.
     *
     * Pass criterion:
     *  - intr_i == true.
     *  - Other three ports == false.
     *
     * Test plan reference: intr_test_injects_health_test_failed
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_test_injects_health_test_failed();

    /**
     * @brief TC-F002-022: INTR_TEST injects FIFO_ERROR (bit 4)
     *
     * Objective: confirm INTR_TEST[4]=1 asserts intr_i port when
     * INTR_ENABLE[4]=1.  FIFO_ERROR is only reachable via INTR_TEST.
     *
     * Pass criterion:
     *  - intr_i == true.
     *  - Other three ports == false.
     *
     * Test plan reference: intr_test_injects_fifo_error
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_test_injects_fifo_error();

    /**
     * @brief TC-F002-023: INTR_TEST injects FIFO_OVERFLOW (bit 8)
     *
     * Objective: confirm INTR_TEST[8]=1 asserts intr_i port when
     * INTR_ENABLE[8]=1.
     *
     * Pass criterion:
     *  - intr_i == true.
     *  - Other three ports == false.
     *
     * Test plan reference: intr_test_injects_fifo_overflow
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_test_injects_fifo_overflow();

    /**
     * @brief TC-F002-024: INTR_TEST injects FIFO_UNDERFLOW (bit 12)
     *
     * Objective: confirm INTR_TEST[12]=1 asserts intr_i port when
     * INTR_ENABLE[12]=1.
     *
     * Pass criterion:
     *  - intr_i == true.
     *  - Other three ports == false.
     *
     * Test plan reference: intr_test_injects_fifo_underflow
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_test_injects_fifo_underflow();

    /**
     * @brief TC-F002-025: INTR_TEST all sources simultaneous
     *
     * Objective: confirm INTR_TEST = 0x00001111 sets all four INTR_STATUS bits
     * simultaneously and all four ports assert when INTR_ENABLE = 0x1111.
     *
     * Pass criterion: all four interrupt ports are simultaneously asserted.
     *
     * Test plan reference: intr_test_all_sources_simultaneous
     *
     * @return true if all four assertions pass
     */
    bool tc_f002_intr_test_all_sources_simultaneous();

    /**
     * @brief TC-F002-028: intr_o clears on W1C of INTR_STATUS[0]
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_clears_on_w1c_bit0();

    /**
     * @brief TC-F002-030: intr_o clears on W1C of INTR_STATUS[4]
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_clears_on_w1c_bit4();

    /**
     * @brief TC-F002-032: intr_o clears on W1C of INTR_STATUS[8]
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_clears_on_w1c_bit8();

    /**
     * @brief TC-F002-034: intr_o clears on W1C of INTR_STATUS[12]
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_clears_on_w1c_bit12();

    /**
     * @brief TC-F002-035: INTR_ENABLE immediate propagation on write
     *
     * Objective: validate that handle_write_INTR_ENABLE calls
     * update_interrupt_outputs() immediately, asserting the port without any
     * additional INTR_STATUS event.
     *
     * Pass criterion:
     *  - Port is false before INTR_ENABLE write (INTR_STATUS[0] set but masked).
     *  - Port is true after INTR_ENABLE write without re-injecting via INTR_TEST.
     *
     * Test plan reference: intr_enable_immediate_propagation_on_write
     *
     * @return true if both assertions pass
     */
    bool tc_f002_intr_enable_immediate_propagation_on_write();

    /**
     * @brief TC-F002-036: All interrupt ports deasserted after software reset
     *
     * Objective: validate that software reset clears INTR_STATUS, resets
     * INTR_ENABLE, and calls update_interrupt_outputs, deasseting all four ports.
     *
     * Pass criterion:
     *  - All four ports asserted before reset (pre-condition).
     *  - All four ports deasserted after reset.
     *
     * Test plan reference: intr_all_ports_deasserted_after_software_reset
     *
     * @return true if all eight assertions pass
     */
    bool tc_f002_intr_all_ports_deasserted_after_software_reset();

    /**
     * @brief TC-F002-047: FIFO_RDATA empty FIFO sets underflow interrupt
     *
     * Objective: confirm that reading FIFO_RDATA from an empty FIFO sets
     * INTR_STATUS[12] (FIFO_UNDERFLOW) via the autonomous set path in
     * handle_read_FIFO_RDATA and causes the interrupt port to assert.
     *
     * Architectural note: handle_read_FIFO_RDATA returns false on empty FIFO.
     * regmodel only copies read_value to the TLM payload when the callback returns
     * true (see reg_file.h).  The TLM data buffer is therefore undefined
     * on underflow; only the interrupt port assertion is tested here.
     *
     * Pass criterion:
     *  - intr_i port asserts after empty FIFO read (with INTR_ENABLE[12]=1).
     *
     * Test plan reference: fifo_rdata_empty_fifo_returns_zero_and_sets_underflow
     *
     * @return true if the port assertion passes
     */
    bool tc_f002_fifo_rdata_empty_fifo_returns_zero_underflow();


    /**
     * @brief TC-F002-049: FIFO underflow — multiple consecutive empty reads
     *
     * Objective: confirm that repeated empty FIFO reads each trigger or keep
     * INTR_STATUS[12] set (OR semantics in set_interrupt_bit).
     *
     * Pass criterion: intr_i == true after each of three reads.
     *
     * Test plan reference: fifo_underflow_multiple_consecutive_empty_reads
     *
     * @return true if all three assertions pass
     */
    bool tc_f002_fifo_underflow_multiple_consecutive_empty_reads();

    /**
     * @brief TC-F002-051: FIFO overflow sets INTR_STATUS[8]
     *
     * Objective: confirm background entropy_generation_thread fills the FIFO
     * (FIFO_DEPTH=127) and that the interrupt controller handles bit 8 (FIFO_OVERFLOW)
     * correctly via the inject→W1C round-trip.
     *
     * Architectural note: the background thread cannot directly drive the interrupt
     * sc_out<bool> ports after apply_reset() has established run_tests as the first
     * driver (SystemC E115 dual-driver constraint).  FIFO liveness is verified by
     * reading FIFO_RDATA; bit-8 behavior is verified via INTR_TEST inject.
     *
     * Pass criterion:
     *  - No FIFO_UNDERFLOW on FIFO_RDATA read (FIFO was non-empty after fill period).
     *  - intr_i asserts after INTR_TEST[8] inject with INTR_ENABLE[8]=1.
     *  - intr_i deasserts after W1C clear.
     *
     * Test plan reference: fifo_overflow_sets_intr_status_bit
     *
     * @return true if all assertions pass
     */
    bool tc_f002_fifo_overflow_sets_intr_status_bit();

    /**
     * @brief TC-F002-138: All four INTR_STATUS bits independent W1C
     *
     * Objective: validate that all four INTR_STATUS bits can be independently
     * cleared via individual W1C writes without affecting other bits.
     *
     * Procedure: inject all four, then clear sequentially — each clear should
     * affect only the targeted port.
     *
     * Pass criterion: each W1C clears only the targeted port; no cross-contamination.
     *
     * Test plan reference: intr_status_all_four_bits_independent_w1c
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_status_all_four_bits_independent_w1c();

    /**
     * @brief TC-F002-139: INTR_ENABLE disable masking does not clear INTR_STATUS
     *
     * Objective: confirm that clearing INTR_ENABLE deasserts the port but does
     * NOT clear the INTR_STATUS bit; re-enabling INTR_ENABLE immediately
     * re-asserts the port without a new INTR_TEST inject.
     *
     * Pass criterion:
     *  - Port deasserts when INTR_ENABLE[0] is cleared.
     *  - Port re-asserts when INTR_ENABLE[0] is set (without re-injecting).
     *
     * Test plan reference: intr_enable_disable_masking_does_not_clear_intr_status
     *
     * @return true if all assertions pass
     */
    bool tc_f002_intr_enable_disable_masking_does_not_clear_status();

    // =========================================================================
    // FUNC-003 test cases — Peripheral Configuration Register Retention
    // =========================================================================

    /**
     * @brief TC-F003-010: DEBUG_CTRL reset value
     *
     * Objective: verify DEBUG_CTRL (0x0C) reads 0x00000000 immediately after
     * apply_reset(), confirming the reset callback restores the register to its
     * hardware default.
     *
     * Pass criterion: read_value == 0x00000000 (DEBUG_CTRL_RESET).
     *
     * Test plan reference: debug_ctrl_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_debug_ctrl_reset_value();

    /**
     * @brief TC-F003-011: DEBUG_CTRL write mask 0x000007FF — SELECT_SIGNAL field
     *
     * Objective: validate that regmodel enforces the 0x000007FF write mask on
     * DEBUG_CTRL, discarding bits [31:11] on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x000007FF.
     *  - Write 0x00000000; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x000007FF) == 0 for all patterns.
     *
     * Test plan reference: debug_ctrl_write_mask_select_signal
     *
     * @return true if all assertions pass
     */
    bool tc_f003_debug_ctrl_write_mask_select_signal();

    /**
     * @brief TC-F003-086: RING_OSC_ENABLE reset value
     *
     * Objective: verify RING_OSC_ENABLE (0x90) reads 0x00FFFFFF after reset,
     * confirming all 24 per-RO enable bits are set at power-on.
     *
     * Pass criterion: read_value == 0x00FFFFFF (RING_OSC_ENABLE_RESET).
     *
     * Test plan reference: ring_osc_enable_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_ring_osc_enable_reset_value();

    /**
     * @brief TC-F003-087: RING_OSC_ENABLE write mask 0x00FFFFFF
     *
     * Objective: validate that regmodel enforces the 0x00FFFFFF write mask on
     * RING_OSC_ENABLE, discarding bits [31:24] on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x00FFFFFF.
     *  - Write 0x00000000; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x00FFFFFF) == 0 for all patterns.
     *
     * Test plan reference: ring_osc_enable_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f003_ring_osc_enable_write_mask_validation();

    /**
     * @brief TC-F003-088: RING_OSC_ENABLE partial-disable readback
     *
     * Objective: confirm that individual RO enable bits within RING_OSC_ENABLE
     * can be selectively cleared and confirmed via readback.
     *
     * Procedure:
     *  - Write 0x00000000 (disable all); assert readback == 0x00000000.
     *  - Write 0x00000FFF (enable lower 12); assert readback == 0x00000FFF.
     *
     * Pass criterion: each partial-enable pattern is retained exactly.
     *
     * Test plan reference: ring_osc_enable_partial_disable_readback
     *
     * @return true if all assertions pass
     */
    bool tc_f003_ring_osc_enable_partial_disable_readback();

    /**
     * @brief TC-F003-089: RING_OSC_TUNE reset value
     *
     * Objective: verify RING_OSC_TUNE (0x94) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000 (RING_OSC_TUNE_RESET).
     *
     * Test plan reference: ring_osc_tune_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_ring_osc_tune_reset_value();

    /**
     * @brief TC-F003-090: RING_OSC_TUNE write mask 0x00FFFFFF
     *
     * Objective: validate that regmodel enforces the 0x00FFFFFF write mask on
     * RING_OSC_TUNE, discarding bits [31:24] on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x00FFFFFF.
     *  - Write 0x00000000; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x00FFFFFF) == 0 for all patterns.
     *
     * Test plan reference: ring_osc_tune_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f003_ring_osc_tune_write_mask_validation();

    /**
     * @brief TC-F003-091: RING_OSC_TUNE write/readback retention
     *
     * Objective: confirm RING_OSC_TUNE retains written values across two
     * consecutive write-read cycles (no callbacks erase the stored value).
     *
     * Procedure:
     *  - Write 0x00AABBCC; assert readback == 0x00AABBCC.
     *  - Write 0x00555AAA; assert readback == 0x00555AAA (overwrite retained).
     *
     * Pass criterion: last-written value is always retained.
     *
     * Test plan reference: ring_osc_tune_write_readback_retained
     *
     * @return true if all assertions pass
     */
    bool tc_f003_ring_osc_tune_write_readback_retained();

    /**
     * @brief TC-F003-092: RING_OSC_CTRL reset value
     *
     * Objective: verify RING_OSC_CTRL (0x98) reads 0x00000FFF after reset,
     * confirming all 12 sample-clock-select bits are set at power-on.
     *
     * Pass criterion: read_value == 0x00000FFF (RING_OSC_CTRL_RESET).
     *
     * Test plan reference: ring_osc_ctrl_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_ring_osc_ctrl_reset_value();

    /**
     * @brief TC-F003-093: RING_OSC_CTRL write mask 0x00000FFF
     *
     * Objective: validate that regmodel enforces the 0x00000FFF write mask on
     * RING_OSC_CTRL, discarding bits [31:12] on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x00000FFF.
     *  - Write 0x00000000; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x00000FFF) == 0 for all patterns.
     *
     * Test plan reference: ring_osc_ctrl_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f003_ring_osc_ctrl_write_mask_validation();

    /**
     * @brief TC-F003-094: DECORRELATOR_CTRL reset value
     *
     * Objective: verify DECORRELATOR_CTRL (0xA0) reads 0x0003F000 after reset,
     * confirming SAMPLE_CLK_DIV=63 (bits[17:12]) is set at power-on.
     *
     * Pass criterion: read_value == 0x0003F000 (DECORRELATOR_CTRL_RESET).
     *
     * Test plan reference: decorrelator_ctrl_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_decorrelator_ctrl_reset_value();

    /**
     * @brief TC-F003-095: DECORRELATOR_CTRL full write/readback (mask 0xFFFFFFFF)
     *
     * Objective: verify all 32 bits of DECORRELATOR_CTRL (0xA0) are writable
     * and retainable — the register has a full 32-bit write mask.
     *
     * Procedure:
     *  - Write 0xDEADBEEF; assert readback == 0xDEADBEEF.
     *  - Write 0x55AA55AA; assert readback == 0x55AA55AA.
     *
     * Pass criterion: read_value == write_value (no masking loss).
     *
     * Test plan reference: decorrelator_ctrl_full_write_readback
     *
     * @return true if all assertions pass
     */
    bool tc_f003_decorrelator_ctrl_full_write_readback();

    /**
     * @brief TC-F003-096: DECORRELATOR_MASK reset value
     *
     * Objective: verify DECORRELATOR_MASK (0xA4) reads 0x000000FF after reset,
     * confirming all eight entropy byte lanes are enabled at power-on.
     *
     * Pass criterion: read_value == 0x000000FF (DECORRELATOR_MASK_RESET).
     *
     * Test plan reference: decorrelator_mask_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f003_decorrelator_mask_reset_value();

    /**
     * @brief TC-F003-097: DECORRELATOR_MASK write mask 0x000000FF
     *
     * Objective: validate that regmodel enforces the 0x000000FF write mask on
     * DECORRELATOR_MASK, discarding bits [31:8] on every write.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x000000FF.
     *  - Write 0x00000000; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x000000FF) == 0 for all patterns.
     *
     * Test plan reference: decorrelator_mask_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f003_decorrelator_mask_write_mask_validation();

    /**
     * @brief TC-F003-137: Software reset restores all six register defaults
     *
     * Objective: verify that the software reset sequence (CTRL[0]=1 via
     * apply_reset()) restores all six configuration registers to their
     * documented hardware reset defaults, even after arbitrary non-default
     * values have been written.
     *
     * Procedure:
     *  1. Write non-default patterns to all six registers.
     *  2. Trigger apply_reset().
     *  3. Read each register; assert read_value == register reset default.
     *
     * Pass criterion: all six registers return their documented reset defaults.
     *
     * Test plan reference: config_regs_reset_restores_defaults
     *
     * @return true if all six assertions pass
     */
    bool tc_f003_config_regs_reset_restores_defaults();

    // =========================================================================
    // FUNC-004 test cases — Background Entropy Generation Process
    // =========================================================================

    /**
     * @brief TC-F004-039: FIFO fill gate — enable/disable via FIFO_CTRL
     *
     * Objective: verify that FIFO_CTRL[0]=0 stops background entropy fill and
     * that FIFO_CTRL[0]=1 resumes it by firing m_fifo_fill_event.
     *
     * Pass criterion:
     *  - Non-zero word from FIFO_RDATA before disable.
     *  - FIFO_RDATA returns 0x00000000 after disable (FIFO drained, thread blocked).
     *  - Non-zero word from FIFO_RDATA after re-enable.
     *
     * Test plan reference: fifo_ctrl_enable_disable_fifo
     *
     * @return true if all assertions pass
     */
    bool tc_f004_fifo_ctrl_enable_disable_fifo();

    /**
     * @brief TC-F004-041: Thread liveness — FIFO fills when enabled
     *
     * Objective: primary liveness test; confirm that the background SC_THREAD
     * is running and pushing PRNG words into the FIFO after reset.
     *
     * Pass criterion: FIFO_RDATA returns at least one non-zero word within
     * F004_POLL_LIMIT SC_ZERO_TIME polling iterations.
     *
     * Test plan reference: fifo_status_level_increments_with_background_fill
     *
     *
     * @return true if liveness assertion passes
     */
    bool tc_f004_fifo_status_level_increments_with_background_fill();

    /**
     * @brief TC-F004-053: FIFO fill halts when disabled during active operation
     *
     * Objective: confirm that writing FIFO_CTRL=0 while in RUNNING transitions
     * the thread to WAITING_FOR_ENABLE and stops all FIFO pushes.
     *
     * Pass criterion: all FIFO_RDATA reads after disable return 0x00000000.
     *
     * Test plan reference: fifo_fill_halts_when_disabled_during_operation
     *
     *
     * @return true if assertions pass
     */
    bool tc_f004_fifo_fill_halts_when_disabled_during_operation();

    /**
     * @brief TC-F004-054: FIFO fill resumes after re-enable
     *
     * Objective: confirm that writing FIFO_CTRL=1 after a disable fires
     * m_fifo_fill_event and causes fill to resume.
     *
     * Pass criterion: FIFO_RDATA returns a non-zero word within F004_POLL_LIMIT
     * SC_ZERO_TIME yields after re-enable.
     *
     * Test plan reference: fifo_fill_resumes_after_reenable
     *
     * @return true if assertion passes
     */
    bool tc_f004_fifo_fill_resumes_after_reenable();

    /**
     * @brief TC-F004-006: CTRL DOWNSAMPLE_RATE field readback and rate effect
     *
     * Objective: verify that CTRL[25:16] DOWNSAMPLE_RATE is writable/readable
     * and that a non-zero rate produces a measurably slower fill.
     *
     * Pass criterion:
     *  - CTRL readback after writing rate=1 shows bits [25:16] = 0x001.
     *  - Reserved bits masked: (read & ~0x03FF0111) == 0.
     *  - At DOWNSAMPLE_RATE=0, at least one non-zero FIFO word produced in
     *    256 SC_ZERO_TIME iterations.
     *
     * Test plan reference: ctrl_downsample_rate_readback
     *
     * @return true if all assertions pass
     */
    bool tc_f004_ctrl_downsample_rate_readback();

    /**
     * @brief TC-F004-101: STARTUP_CTRL non-zero delay applied after FIFO wake
     *
     * Objective: verify that a non-zero STARTUP_CTRL[15:0] value produces an
     * observable hold-off before fill begins on the next FIFO re-enable.
     *
     * Procedure: write STARTUP_CTRL = 1000 ns, disable FIFO, re-enable FIFO,
     * confirm FIFO_RDATA = 0 during hold-off, then confirm fill starts after
     * waiting 2000 ns.
     *
     * Pass criterion:
     *  - FIFO_RDATA = 0x00000000 during startup hold-off.
     *  - FIFO_RDATA returns non-zero after the hold-off expires.
     *
     * Test plan reference: startup_ctrl_nonzero_delay_applied_after_reset
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f004_startup_ctrl_nonzero_delay_applied_after_reset();

    /**
     * @brief TC-F004-102: STARTUP_CTRL zero delay — no hold-off
     *
     * Objective: verify that with STARTUP_CTRL[15:0] = 0 the thread enters
     * RUNNING immediately after a FIFO re-enable without any startup delay.
     *
     * Pass criterion: FIFO_RDATA returns a non-zero word within 16 SC_ZERO_TIME
     * yields of re-enabling the FIFO.
     *
     * Test plan reference: startup_ctrl_zero_delay_no_holdoff
     *
     * @return true if assertion passes
     */
    bool tc_f004_startup_ctrl_zero_delay_no_holdoff();

    /**
     * @brief TC-F004-103: STARTUP_CTRL write during steady-state does not
     *        disrupt fill
     *
     * Objective: confirm that writing STARTUP_CTRL while the thread is in
     * RUNNING does not pause or restart the thread; the delay is consumed only
     * at the next FIFO disable/re-enable cycle.
     *
     * Pass criterion:
     *  - Non-zero FIFO_RDATA words produced both before and after the
     *    STARTUP_CTRL write.
     *  - STARTUP_CTRL readback equals the written value.
     *
     * Test plan reference: startup_ctrl_delay_consumed_only_at_next_reset
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f004_startup_ctrl_delay_consumed_only_at_next_reset();

    /**
     * @brief TC-F004-113: Software reset stabilization hold-off observable
     *
     * Objective: confirm that a non-zero STARTUP_CTRL value causes an
     * observable hold-off on the next FIFO wake, and that a subsequent reset
     * clears the delay so that fill is immediate.
     *
     * Pass criterion:
     *  - FIFO_RDATA = 0x00000000 during the hold-off window.
     *  - Fill starts after the hold-off.
     *  - After a second reset (STARTUP_CTRL cleared) fill is immediate.
     *
     * Test plan reference: software_reset_stabilization_holdoff_observable
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f004_software_reset_stabilization_holdoff_observable();

    /**
     * @brief TC-F004-115: Software reset with FIFO disabled — thread does not
     *        fill
     *
     * Objective: confirm that disabling the FIFO immediately after a software
     * reset causes the thread to block in WAITING_FOR_ENABLE and produce no
     * data.
     *
     * Pass criterion: FIFO_RDATA returns 0x00000000 across 32 consecutive
     * SC_ZERO_TIME polls after disabling the FIFO post-reset.
     *
     * Test plan reference: software_reset_fifo_disabled_background_does_not_fill
     *
     *
     * @return true if assertion passes
     */
    bool tc_f004_software_reset_fifo_disabled_does_not_fill();

    /**
     * @brief TC-F004-140: Primary liveness test — continuous entropy generation
     *
     * Objective: confirm that the background SC_THREAD produces entropy data
     * continuously by taking two time-separated measurements.
     *
     * Pass criterion:
     *  - Snapshot T1: non-zero word from FIFO_RDATA within 64 SC_ZERO_TIME
     *    iterations.
     *  - Snapshot T2 (after 500 ns advance): non-zero word within 64
     *    SC_ZERO_TIME iterations.
     *
     * Test plan reference: background_process_entropy_continuously_generated
     *
     *
     * @return true if both assertions pass
     */
    bool tc_f004_background_process_entropy_continuously_generated();

    // =========================================================================
    // FUNC-005 test cases — Health Test Subsystem Behavior
    // =========================================================================

    /**
     * @brief TC-F005-055: HEALTH_TEST_CTRL reset value
     *
     * Objective: verify HEALTH_TEST_CTRL (0x30) reads 0x00000F07 immediately
     * after apply_reset(), confirming ENABLE=0x07 and REPETITION_LIMIT=0x0F.
     *
     * Pass criterion: read_value == 0x00000F07.
     *
     * Test plan reference: health_test_ctrl_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_health_test_ctrl_reset_value();

    /**
     * @brief TC-F005-056: HEALTH_TEST_CTRL write mask 0x0000FFFF
     *
     * Objective: validate that regmodel enforces the 0x0000FFFF write mask on
     * HEALTH_TEST_CTRL; reserved bits [31:16] always read as zero.
     *
     * Pass criterion: (read_value & ~0x0000FFFF) == 0 for all write patterns.
     *
     * Test plan reference: health_test_ctrl_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f005_health_test_ctrl_write_mask_validation();

    /**
     * @brief TC-F005-057: HEALTH_TEST_CTRL disable stops health test activity
     *
     * Objective: confirm writing ENABLE=0x00 halts all counter increments while
     * the FIFO fill path continues unaffected (orthogonal gates).
     *
     * Pass criterion:
     *  - Thread produces FIFO data before and after disable.
     *  - HEALTH_TEST_CTRL ENABLE reads as 0x00 after write.
     *  - HEALTH_TEST_CTRL reads expected value after re-enable.
     *
     * Test plan reference: health_test_ctrl_disable_all_tests
     *
     * @return true if all assertions pass
     */
    bool tc_f005_health_test_ctrl_disable_all_tests();

    /**
     * @brief TC-F005-059: HEALTH_TEST_CTRL REPETITION_LIMIT independent writability
     *
     * Objective: verify bits [15:8] (REPETITION_LIMIT) are independently writable
     * and retained without disturbing ENABLE[7:0].
     *
     * Pass criterion: each write/read cycle returns the exact value written.
     *
     * Test plan reference: health_test_ctrl_repetition_limit_readback
     *
     * @return true if all assertions pass
     */
    bool tc_f005_health_test_ctrl_repetition_limit_readback();

    /**
     * @brief TC-F005-060: MARKOV_TEST_PROB_THRESHOLDS reset value
     *
     * Objective: verify MARKOV_TEST_PROB_THRESHOLDS (0x38) reads 0x64646464
     * after apply_reset() (four threshold bytes each = 100 decimal).
     *
     * Pass criterion: read_value == 0x64646464.
     *
     * Test plan reference: markov_test_prob_thresholds_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_markov_test_prob_thresholds_reset_value();

    /**
     * @brief TC-F005-061: MARKOV_TEST_PROB_THRESHOLDS full write/readback
     *
     * Objective: confirm 32-bit write mask (0xFFFFFFFF) allows all patterns to
     * be retained exactly (pure regmodel storage, no callbacks).
     *
     * Pass criterion: read_value == write_value for all four test patterns.
     *
     * Test plan reference: markov_test_prob_thresholds_full_write_readback
     *
     * @return true if all assertions pass
     */
    bool tc_f005_markov_test_prob_thresholds_full_write_readback();

    /**
     * @brief TC-F005-062: MARKOV_TEST_PROB_THRESHOLDS individual field readback
     *
     * Objective: verify the four byte-wide threshold fields can be independently
     * set and read back with no aliasing between fields.
     *
     * Pass criterion: each byte-pattern is retained exactly across field boundaries.
     *
     * Test plan reference: markov_test_prob_thresholds_individual_field_readback
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f005_markov_test_prob_thresholds_individual_field_readback();

    /**
     * @brief TC-F005-063: HEALTH_TEST_STATUS clears to zero after reset
     *
     * Objective: verify HEALTH_TEST_STATUS (0x40) reads 0x00000000 immediately
     * after apply_reset().
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: health_test_status_reset_to_zero_after_reset
     *
     * @return true if assertion passes
     */
    bool tc_f005_health_test_status_reset_to_zero_after_reset();

    /**
     * @brief TC-F005-065: HEALTH_TEST_STATUS is read-only
     *
     * Objective: confirm RO enforcement on HEALTH_TEST_STATUS (0x40); write
     * of 0xFFFFFFFF has no observable effect on subsequent reads.
     *
     * Pass criterion: read_value == 0x00000000 before and after the write.
     *
     * Test plan reference: health_test_status_is_read_only
     *
     * @return true if both assertions pass
     */
    bool tc_f005_health_test_status_is_read_only();

    /**
     * @brief TC-F005-066: REPETITION_TEST_COUNT clears to zero after reset
     *
     * Objective: verify REPETITION_TEST_COUNT (0x44) reads 0x00000000
     * immediately after apply_reset() (reset action 4 clears all counters).
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: repetition_test_count_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_repetition_test_count_reset_to_zero();

    /**
     * @brief TC-F005-068: REPETITION_TEST_COUNT is read-only
     *
     * Objective: confirm RO enforcement on REPETITION_TEST_COUNT (0x44); write
     * of 0xFFFFFFFF has no observable effect.
     *
     * Pass criterion: read_value == 0x00000000 before and after the write.
     *
     * Test plan reference: repetition_test_count_is_read_only
     *
     * @return true if both assertions pass
     */
    bool tc_f005_repetition_test_count_is_read_only();

    /**
     * @brief TC-F005-069: APT_PATTERN_COUNT_1BIT clears to zero after reset
     *
     * Objective: verify APT_PATTERN_COUNT_1BIT (0x50) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: apt_pattern_count_1bit_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_pattern_count_1bit_reset_to_zero();

    /**
     * @brief TC-F005-070: APT_PATTERN_COUNT_2BIT clears to zero after reset
     *
     * Objective: verify APT_PATTERN_COUNT_2BIT (0x54) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: apt_pattern_count_2bit_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_pattern_count_2bit_reset_to_zero();

    /**
     * @brief TC-F005-071: APT_PATTERN_COUNT_3BIT clears to zero after reset
     *
     * Objective: verify APT_PATTERN_COUNT_3BIT (0x58) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: apt_pattern_count_3bit_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_pattern_count_3bit_reset_to_zero();

    /**
     * @brief TC-F005-072: APT_PATTERN_COUNT_4BIT clears to zero after reset
     *
     * Objective: verify APT_PATTERN_COUNT_4BIT (0x5C) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: apt_pattern_count_4bit_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_pattern_count_4bit_reset_to_zero();

    /**
     * @brief TC-F005-075: APT_PROPORTION_1BIT reset value
     *
     * Objective: verify APT_PROPORTION_1BIT (0x60) reads 0x00000200 (LIMIT=512)
     * after apply_reset().
     *
     * Pass criterion: read_value == 0x00000200.
     *
     * Test plan reference: apt_proportion_1bit_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_proportion_1bit_reset_value();

    /**
     * @brief TC-F005-076: APT_PROPORTION_2BIT reset value
     *
     * Objective: verify APT_PROPORTION_2BIT (0x64) reads 0x00000080 (LIMIT=128)
     * after apply_reset().
     *
     * Pass criterion: read_value == 0x00000080.
     *
     * Test plan reference: apt_proportion_2bit_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_proportion_2bit_reset_value();

    /**
     * @brief TC-F005-077: APT_PROPORTION_3BIT reset value
     *
     * Objective: verify APT_PROPORTION_3BIT (0x68) reads 0x00000040 (LIMIT=64)
     * after apply_reset().
     *
     * Pass criterion: read_value == 0x00000040.
     *
     * Test plan reference: apt_proportion_3bit_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_proportion_3bit_reset_value();

    /**
     * @brief TC-F005-078: APT_PROPORTION_4BIT reset value
     *
     * Objective: verify APT_PROPORTION_4BIT (0x6C) reads 0x00000020 (LIMIT=32)
     * after apply_reset().
     *
     * Pass criterion: read_value == 0x00000020.
     *
     * Test plan reference: apt_proportion_4bit_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f005_apt_proportion_4bit_reset_value();

    /**
     * @brief TC-F005-079: APT_PROPORTION_1BIT write mask 0x000003FF
     *
     * Objective: validate that regmodel enforces the 0x000003FF write mask on
     * APT_PROPORTION_1BIT; reserved bits [31:10] always read as zero.
     *
     * Pass criterion: (read_value & ~0x000003FF) == 0 for all write patterns.
     *
     * Test plan reference: apt_proportion_1bit_write_mask_validation
     *
     * @return true if all assertions pass
     */
    bool tc_f005_apt_proportion_1bit_write_mask_validation();

    /**
     * @brief TC-F005-082: MARKOV_TEST_COUNTS_0 clears to zero after reset
     *
     * Objective: verify MARKOV_TEST_COUNTS_0 (0x80) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: markov_test_counts_0_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_markov_test_counts_0_reset_to_zero();

    /**
     * @brief TC-F005-083: MARKOV_TEST_COUNTS_1 clears to zero after reset
     *
     * Objective: verify MARKOV_TEST_COUNTS_1 (0x84) reads 0x00000000 after reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: markov_test_counts_1_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_markov_test_counts_1_reset_to_zero();

    /**
     * @brief TC-F005-084: MARKOV_TEST_PROBABILITIES clears to zero after reset
     *
     * Objective: verify MARKOV_TEST_PROBABILITIES (0x88) reads 0x00000000 after
     * reset.
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: markov_test_probabilities_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_markov_test_probabilities_reset_to_zero();

    /**
     * @brief TC-F005-104: GENERATOR_0_HEALTH_STATUS clears to zero after reset
     *
     * Objective: verify GENERATOR_0_HEALTH_STATUS (0xC0) reads 0x00000000 after
     * apply_reset().
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: generator_0_health_status_reset_to_zero
     *
     * @return true if assertion passes
     */
    bool tc_f005_generator_0_health_status_reset_to_zero();

    /**
     * @brief TC-F005-105: GENERATOR_1 through GENERATOR_11 health status all zero
     *        after reset
     *
     * Objective: verify all 11 GENERATOR_1–11_HEALTH_STATUS registers (0xC4–0xEC)
     * each read 0x00000000 after apply_reset().
     *
     * Pass criterion: all 11 registers return 0x00000000.
     *
     * Test plan reference: generator_1_to_11_health_status_reset_to_zero
     *
     * @return true if all 11 assertions pass
     */
    bool tc_f005_generator_1_to_11_health_status_reset_to_zero();

    // =========================================================================
    // FUNC-006 test cases — FIFO-Based Entropy Data Queue Operation
    // =========================================================================

    /**
     * @brief TC-F006-037: FIFO_CTRL reset value
     *
     * Objective: verify that FIFO_CTRL (0x20) reads 0x00000001 after reset
     * (FIFO ENABLE = 1 by default).
     *
     * Pass criterion: read_value == 0x00000001.
     *
     * Test plan reference: fifo_ctrl_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f006_fifo_ctrl_reset_value();

    /**
     * @brief TC-F006-038: FIFO_CTRL write mask — only bit 0 writable
     *
     * Objective: verify that FIFO_CTRL write mask 0x00000001 is enforced;
     * bits [31:1] must always read as zero.
     *
     * Procedure:
     *  - Write 0xFFFFFFFF; assert readback == 0x00000001.
     *  - Write 0xFFFFFFFE; assert readback == 0x00000000.
     *
     * Pass criterion: (read_value & ~0x1) == 0 for all patterns.
     *
     * Test plan reference: fifo_ctrl_write_mask_only_bit0_writable
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_ctrl_write_mask_only_bit0_writable();

    /**
     * @brief TC-F006-039: FIFO_CTRL gate — FIFO_CTRL[0]=0 stops fill,
     *        FIFO_CTRL[0]=1 resumes fill
     *
     * Objective: confirm the FIFO enable/disable gate mechanism.
     *
     * Pass criterion:
     *  - Non-zero FIFO_RDATA before disable.
     *  - All FIFO_RDATA reads return 0x00000000 after disable and drain.
     *  - Non-zero FIFO_RDATA after re-enable.
     *
     * Test plan reference: fifo_ctrl_enable_disable_fifo
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_ctrl_enable_disable_fifo();

    /**
     * @brief TC-F006-040: FIFO_STATUS reset value
     *
     * Objective: verify FIFO_STATUS (0x24) reads 0x00000000 after reset
     * (LEVEL=0, WPTR=0, RPTR=0, reserved=0).
     *
     * Pass criterion: read_value == 0x00000000.
     *
     * Test plan reference: fifo_status_reset_value
     *
     * @return true if assertion passes
     */
    bool tc_f006_fifo_status_reset_value();

    /**
     * @brief TC-F006-041: FIFO_STATUS.LEVEL increments with background fill
     *
     * Objective: confirm FIFO_STATUS.LEVEL increases as the background thread
     * pushes values (verified indirectly via non-zero FIFO_RDATA pops).
     *
     * Pass criterion:
     *  - At least one non-zero FIFO_RDATA pop within poll limit.
     *  - FIFO_STATUS TLM read returns 0x00000000 (regmodel read_mask=0x0).
     *
     * Test plan reference: fifo_status_level_increments_with_background_fill
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_status_level_increments_with_background_fill();

    /**
     * @brief TC-F006-042: FIFO_STATUS.LEVEL decrements on FIFO_RDATA read
     *
     * Objective: confirm each FIFO_RDATA read decrements LEVEL by 1 and
     * increments RPTR by 1 (destructive pop semantics).
     *
     * Pass criterion:
     *  - At least one word popped (non-zero FIFO_RDATA).
     *  - After draining, next read returns 0x00000000 (underflow).
     *  - intr_i asserts after the empty read with INTR_ENABLE[12]=1.
     *
     * Test plan reference: fifo_status_level_decrements_on_fifo_rdata_read
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_status_level_decrements_on_fifo_rdata_read();

    /**
     * @brief TC-F006-043: FIFO_STATUS.LEVEL saturates at FIFO_DEPTH (127)
     *
     * Objective: confirm the FIFO does not grow beyond 127 entries; overflow
     * is detected via intr_i port assertion.
     *
     * Pass criterion:
     *  - intr_i asserts within F006_FILL_POLL_LIMIT iterations.
     *  - FIFO_STATUS TLM read returns 0x00000000.
     *
     * Test plan reference: fifo_status_level_at_maximum_depth
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_status_level_at_maximum_depth();

    /**
     * @brief TC-F006-044: FIFO_STATUS is read-only
     *
     * Objective: confirm RO enforcement on FIFO_STATUS (0x24); write of
     * 0xFFFFFFFF must have no observable effect.
     *
     * Pass criterion: FIFO_STATUS reads 0x00000000 before and after the write.
     *
     * Test plan reference: fifo_status_is_read_only
     *
     * @return true if assertions pass
     */
    bool tc_f006_fifo_status_is_read_only();

    /**
     * @brief TC-F006-045: FIFO_RDATA returns non-zero entropy when non-empty
     *
     * Objective: confirm handle_read_FIFO_RDATA pops and returns a non-zero
     * PRNG word when the queue is non-empty.
     *
     * Pass criterion: at least one FIFO_RDATA read returns != 0x00000000.
     *
     * Test plan reference: fifo_rdata_returns_nonzero_entropy_when_nonempty
     *
     *
     * @return true if assertion passes
     */
    bool tc_f006_fifo_rdata_returns_nonzero_entropy_when_nonempty();

    /**
     * @brief TC-F006-046: Successive FIFO_RDATA reads yield different values
     *
     * Objective: confirm successive FIFO_RDATA reads return different PRNG
     * values (FIFO pops correctly from the queue).
     *
     * Pass criterion: at least two FIFO_RDATA reads return different non-zero
     * values.
     *
     * Test plan reference: fifo_rdata_successive_reads_yield_different_values
     *
     *
     * @return true if assertion passes
     */
    bool tc_f006_fifo_rdata_successive_reads_yield_different_values();

    /**
     * @brief TC-F006-047: Empty FIFO read returns zero and sets underflow
     *
     * Objective: core underflow test — read FIFO_RDATA when the queue is empty;
     * must return 0x00000000 and set INTR_STATUS[12]; intr_i asserts
     * when INTR_ENABLE[12]=1.
     *
     * Pass criterion:
     *  - FIFO_RDATA returns 0x00000000 on empty-FIFO read.
     *  - intr_i == true after the read with INTR_ENABLE[12]=1.
     *
     * Test plan reference: fifo_rdata_empty_fifo_returns_zero_and_sets_underflow
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_rdata_empty_fifo_returns_zero_and_sets_underflow();

    /**
     * @brief TC-F006-048: intr_i port assertion on empty read
     *
     * Objective: verify the full underflow interrupt output path:
     * empty-read event → INTR_STATUS[12] → AND with INTR_ENABLE[12]
     * → intr_i sc_out<bool>.
     *
     * Pass criterion:
     *  - intr_i == false before empty read.
     *  - intr_i == true after empty read.
     *  - intr_i == false after W1C clear.
     *
     * Test plan reference: fifo_underflow_interrupt_port_on_empty_read
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_underflow_interrupt_port_on_empty_read();

    /**
     * @brief TC-F006-050: Drain FIFO to empty and verify LEVEL zero
     *
     * Objective: confirm draining all FIFO entries reduces LEVEL to 0;
     * the subsequent read triggers underflow (returns 0x00000000).
     *
     * Pass criterion:
     *  - Underflow detected after drain.
     *  - Second post-drain read also returns 0x00000000.
     *
     * Test plan reference: fifo_drain_to_empty_and_verify_level_zero
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_drain_to_empty_and_verify_level_zero();

    /**
     * @brief TC-F006-051: Overflow sets INTR_STATUS[8] (FIFO_OVERFLOW)
     *
     * Objective: core overflow test — background push into full FIFO sets
     * INTR_STATUS[8]; LEVEL stays at FIFO_DEPTH.
     *
     * Pass criterion:
     *  - intr_i asserts (INTR_ENABLE[8]=1).
     *  - FIFO_STATUS TLM read returns 0x00000000.
     *  - Non-zero word can be popped post-overflow.
     *
     * Test plan reference: fifo_overflow_sets_intr_status_bit
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_overflow_sets_intr_status_bit();

    /**
     * @brief TC-F006-053: FIFO fill halts when FIFO_CTRL[0]=0 mid-operation
     *
     * Objective: confirm FIFO_CTRL[0]=0 during fill freezes the FIFO level;
     * no new words appear after drain.
     *
     * Pass criterion:
     *  - At least one word drained (FIFO was non-empty at disable time).
     *  - No new words appear after drain with FIFO_CTRL[0]=0.
     *
     * Test plan reference: fifo_fill_halts_when_disabled_during_operation
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_fill_halts_when_disabled_during_operation();

    /**
     * @brief TC-F006-054: FIFO fill resumes after re-enable
     *
     * Objective: confirm FIFO_CTRL[0]=1 after a disable wakes the background
     * thread and fill resumes.
     *
     * Pass criterion: non-zero FIFO_RDATA obtained after re-enable within
     *                 F006_POLL_LIMIT iterations.
     *
     * Test plan reference: fifo_fill_resumes_after_reenable
     *
     * @return true if assertion passes
     */
    bool tc_f006_fifo_fill_resumes_after_reenable();

    /**
     * @brief TC-F006-129: RO write has no effect on FIFO_STATUS
     *
     * Objective: explicit write-then-read check confirming FIFO_STATUS (0x24)
     * is fully read-only; writing 0xFFFFFFFF leaves the value at 0x00000000.
     *
     * Pass criterion: FIFO_STATUS == 0x00000000 before and after the write.
     *
     * Test plan reference: ro_register_write_has_no_effect_fifo_status
     *
     *
     * @return true if assertions pass
     */
    bool tc_f006_ro_write_has_no_effect_fifo_status();

    /**
     * @brief TC-F006-136: FIFO_STATUS.WPTR advances with background push
     *
     * Objective: confirm FIFO_STATUS.WPTR[13:7] increments from 0 as background
     * pushes occur (verified indirectly via sequential non-zero FIFO_RDATA pops).
     *
     * Pass criterion:
     *  - At least 2 non-zero pops obtained (wptr advanced >= 2 times).
     *  - FIFO_STATUS TLM read returns 0x00000000.
     *
     * Test plan reference: fifo_wptr_advances_with_background_push
     *
     * @return true if all assertions pass
     */
    bool tc_f006_fifo_wptr_advances_with_background_push();

    /**
     * @brief TC-F006-137: Confirm FIFO_STATUS.RPTR advances with FIFO read
     *
     * @return true if assertions pass
     */
    bool tc_f006_fifo_rptr_advances_with_read();

    // =========================================================================
    // FUNC-007 test cases — Software Reset Sequence
    // =========================================================================

    /**
     * @brief TC-F007-002: COMPONENT_ID immune to software reset
     *
     * Objective: confirm COMPONENT_ID (0x00) retains 0x01000001 after a full
     * software reset sequence (CTRL[0]=1), verifying the FUNC-007 reset immunity
     * guarantee for the build-time constant register.
     *
     * Pass criterion:
     *  - pre_reset  == 0x00000000 (regmodel read_mask=0: buffer unchanged).
     *  - post_reset == 0x00000000 (regmodel read_mask=0: buffer unchanged).
     *  - No regmodel write-restriction error emitted (reset did not touch COMPONENT_ID).
     *
     * Test plan reference: component_id_immune_to_software_reset
     *
     * @return true if both assertions pass
     */
    bool tc_f007_component_id_immune_to_software_reset();

    /**
     * @brief TC-F007-004: CTRL (0x04) reads 0x00000000 after software reset
     *        (CTRL reset value)
     *
     * Objective: verify reset action (8) — handle_write_CTRL performs a regmodel
     * internal write of 0x00000000 to CTRL after the stabilization hold-off,
     * self-clearing the RESET bit and all other CTRL fields.
     *
     * Pass criterion:
     *  - CTRL holds non-zero value before reset.
     *  - CTRL reads 0x00000000 after reset.
     *
     * Test plan reference: ctrl_reset_value
     *
     * @return true if both assertions pass
     */
    bool tc_f007_ctrl_reset_value();

    /**
     * @brief TC-F007-036: All four interrupt output ports deasserted after
     *        software reset
     *
     * Objective: confirm reset actions (5) and (6) — INTR_STATUS cleared and
     * update_interrupt_outputs called — deassert all four sc_out<bool> ports.
     *
     * Pass criterion:
     *  - All four ports true before reset (pre-condition via INTR_TEST inject).
     *  - All four ports false after reset.
     *
     * Test plan reference: intr_all_ports_deasserted_after_software_reset
     *
     *
     * @return true if all eight assertions pass
     */
    bool tc_f007_intr_all_ports_deasserted_after_software_reset();

    /**
     * @brief TC-F007-109: Software reset clears all eight named health test
     *        counter registers to 0x00000000 (reset action 4)
     *
     * Objective: confirm HEALTH_TEST_STATUS, REPETITION_TEST_COUNT, all four
     * APT_PATTERN_COUNTs, and MARKOV_TEST_COUNTS_0 / _1 each return
     * 0x00000000 immediately after apply_reset().
     *
     * Pass criterion: all eight registers return 0x00000000.
     *
     * Test plan reference: software_reset_clears_all_health_test_counters
     *
     *
     * @return true if all eight assertions pass
     */
    bool tc_f007_software_reset_clears_all_health_test_counters();

    /**
     * @brief TC-F007-110: Software reset clears all 12 GENERATOR_k_HEALTH_STATUS
     *        registers to 0x00000000 (continuation of reset action 4)
     *
     * Objective: confirm GENERATOR_0 through GENERATOR_11_HEALTH_STATUS
     * (offsets 0xC0–0xEC) each return 0x00000000 immediately after apply_reset().
     *
     * Pass criterion: all 12 registers return 0x00000000.
     *
     * Test plan reference: software_reset_clears_per_generator_health_status
     *
     *
     * @return true if all 12 assertions pass
     */
    bool tc_f007_software_reset_clears_per_generator_health_status();

    /**
     * @brief TC-F007-111: Software reset clears INTR_STATUS and deasserts all
     *        four interrupt output ports (reset actions 5 and 6)
     *
     * Objective: confirm that reset action (5) clears INTR_STATUS and action (6)
     * calls update_interrupt_outputs, deassessing all four sc_out<bool> ports.
     *
     * Pass criterion:
     *  - All four ports true before reset.
     *  - All four ports false after reset.
     *  - INTR_STATUS TLM read returns 0x00000000.
     *
     * Test plan reference: software_reset_clears_intr_status
     *
     * @return true if all assertions pass
     */
    bool tc_f007_software_reset_clears_intr_status();

    /**
     * @brief TC-F007-112: CTRL self-clears to 0x00000000 after stabilization
     *        hold-off completes (reset action 8)
     *
     * Objective: confirm that action (8) writes 0x00000000 to CTRL via regmodel
     * internal write after the stabilization delay, clearing RESET[0] and all
     * other CTRL fields simultaneously.
     *
     * Pass criterion:
     *  - CTRL holds 0x03FF0110 before reset (all non-RESET bits set).
     *  - CTRL reads 0x00000000 after reset.
     *
     * Test plan reference: software_reset_ctrl_self_clears
     *
     * @return true if both assertions pass
     */
    bool tc_f007_software_reset_ctrl_self_clears();

    /**
     * @brief TC-F007-116: Core RW registers return to regmodel reset defaults after
     *        software reset
     *
     * Objective: verify that CTRL, DEBUG_CTRL, HEALTH_TEST_CTRL,
     * MARKOV_TEST_PROB_THRESHOLDS, and FIFO_CTRL each return to their documented
     * hardware reset defaults after apply_reset().  Additionally verifies that
     * INTR_ENABLE is intentionally NOT cleared by reset (per FUNC-007 spec) —
     * it retains its last-written value across the reset sequence.
     *
     * Pass criterion: every register equals its reset default after apply_reset().
     *
     * Test plan reference: software_reset_rw_registers_restored_to_defaults
     *
     *
     * @return true if all assertions pass
     */
    bool tc_f007_software_reset_rw_registers_restored_to_defaults();

    // =========================================================================
    // Result reporting
    // =========================================================================

    /**
     * @brief Record a test result and log it via REG_INFO
     *
     * Increments m_tests_run and either m_tests_passed or m_tests_failed
     * depending on @p passed.  Emits a one-line REG_INFO log entry.
     *
     * @param test_name Human-readable test identifier
     * @param passed    true if the test passed, false if it failed
     */
    void record_result(const std::string& test_name, bool passed);

    // =========================================================================
    // Instantiated sub-modules (heap allocated)
    // =========================================================================

    /// @brief DUT instance (entropy_src_ip model)
    entropy_src_ip* dut;

    /// @brief Test harness instance
    entropy_src_test* test;

    // =========================================================================
    // Interconnect signals
    // =========================================================================

    /// @brief Wire connecting dut->intr_o to test->intr_i
    sc_core::sc_signal<bool> sig_intr;

    /// @brief Wire connecting testbench to dut->rst_ni (active-low hardware reset)
    sc_core::sc_signal<bool> sig_rst_n;

    // =========================================================================
    // Test bookkeeping
    // =========================================================================

    /// @brief Total number of test cases executed
    int m_tests_run;

    /// @brief Number of test cases that passed
    int m_tests_passed;

    /// @brief Number of test cases that failed
public:
    int m_tests_failed;



    /**
     * @brief tc_f001_new_regs_reset_values: Verify reset values of 22 new registers
     *
     * @return true if all assertions pass
     */
    bool tc_f001_new_regs_reset_values();

    /**
     * @brief tc_f001_new_regs_rw_access: Verify RW access for new RW registers
     *
     * @return true if all assertions pass
     */
    bool tc_f001_new_regs_rw_access();

    /**
     * @brief tc_f001_new_regs_ro_enforcement: Verify RO enforcement for new RO registers
     *
     * @return true if all assertions pass
     */
    bool tc_f001_new_regs_ro_enforcement();

    // =========================================================================
    // Hardware reset test cases
    // =========================================================================

    /**
     * @brief TC-F004-HW-001: Verify hardware reset returns all regs to defaults
     * @return true if all assertions pass
     */
    bool tc_f004_hw_reset_returns_regs_to_defaults();

    /**
     * @brief TC-F004-HW-002: Verify hardware reset during active FIFO filling
     * @return true if all assertions pass
     */
    bool tc_f004_hw_reset_during_fifo_filling();

    // Coverage tests (test_coverage.cpp)
    bool tc_cov_boot_rst_n_with_startup_delay();
    bool tc_cov_fifo_reenable_startup_delay();
    bool tc_cov_sw_reset_during_reenable_startup_delay();
    bool tc_cov_hw_reset_rederive_state();
    bool tc_cov_new_rdl_register_access();
    bool tc_cov_fips_lock_w1s();
    bool tc_cov_boot_phase_done_gate();
    bool tc_cov_irq_overflow_underflow();
    bool tc_cov_reset_while_fifo_disabled();
    bool tc_cov_verbose_callbacks_and_recovery();

};
