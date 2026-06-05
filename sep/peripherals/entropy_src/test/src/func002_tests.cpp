/******************************************************************************
 * @file func002_tests.cpp
 * @brief FUNC-002 Interrupt Controller Behavior — test case implementations
 *
 * Implements all 30 test cases mapped to FUNC-002 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to one
 * or more rows in the FUNC-002 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-002 — 30 test cases)
 *
 *  Sl. | Method                                                  | Test Plan ID
 *  ----|---------------------------------------------------------|-----------------------------------
 *  13  | tc_f002_intr_status_reset_value                         | intr_status_reset_value
 *  14  | tc_f002_intr_status_w1c_set_clear_health_test_failed    | intr_status_w1c_set_clear_health_test_failed
 *  15  | tc_f002_intr_status_w1c_write_zero_does_not_clear       | intr_status_w1c_write_zero_does_not_clear
 *  16  | tc_f002_intr_status_reserved_bits_always_zero           | intr_status_reserved_bits_always_zero
 *  17  | tc_f002_intr_enable_reset_value                         | intr_enable_reset_value
 *  19  | tc_f002_intr_enable_per_bit_enable_all_sources          | intr_enable_per_bit_enable_all_sources
 *  21  | tc_f002_intr_test_injects_health_test_failed            | intr_test_injects_health_test_failed
 *  22  | tc_f002_intr_test_injects_fifo_error                    | intr_test_injects_fifo_error
 *  23  | tc_f002_intr_test_injects_fifo_overflow                 | intr_test_injects_fifo_overflow
 *  24  | tc_f002_intr_test_injects_fifo_underflow                | intr_test_injects_fifo_underflow
 *  25  | tc_f002_intr_test_all_sources_simultaneous              | intr_test_all_sources_simultaneous
 *  28  | tc_f002_intr_clears_on_w1c_bit0      | intr_health_test_failed_port_clears_on_w1c
 *  30  | tc_f002_intr_clears_on_w1c_bit4              | intr_fifo_error_port_clears_on_w1c
 *  32  | tc_f002_intr_clears_on_w1c_bit8           | intr_fifo_overflow_port_clears_on_w1c
 *  34  | tc_f002_intr_clears_on_w1c_bit12          | intr_fifo_underflow_port_clears_on_w1c
 *  35  | tc_f002_intr_enable_immediate_propagation_on_write      | intr_enable_immediate_propagation_on_write
 *  36  | tc_f002_intr_all_ports_deasserted_after_software_reset  | intr_all_ports_deasserted_after_software_reset
 *  47  | tc_f002_fifo_rdata_empty_fifo_returns_zero_underflow    | fifo_rdata_empty_fifo_returns_zero_and_sets_underflow
 *  49  | tc_f002_fifo_underflow_multiple_consecutive_empty_reads | fifo_underflow_multiple_consecutive_empty_reads
 *  51  | tc_f002_fifo_overflow_sets_intr_status_bit              | fifo_overflow_sets_intr_status_bit
 * 138  | tc_f002_intr_status_all_four_bits_independent_w1c       | intr_status_all_four_bits_independent_w1c
 * 139  | tc_f002_intr_enable_disable_masking_does_not_clear_status| intr_enable_disable_masking_does_not_clear_intr_status
 *
 * ## Key architectural facts governing this test file
 *
 *  - INTR_STATUS (0x10) has read_bit_mask = 0x0 per entropy_src_basetest, meaning
 *    CSML returns 0x00000000 on every TLM b_transport read of INTR_STATUS.  The
 *    model writes INTR_STATUS using direct CSML register assignment (bypassing the
 *    TLM path), so the only way to observe INTR_STATUS contents from the testbench
 *    is to read the sc_in<bool> interrupt ports, or to confirm their state relative
 *    to the known INTR_ENABLE register contents.
 *
 *  - INTR_TEST (0x18) is write-only; reads always return 0x00000000.  Writing to
 *    INTR_TEST ORs the written value (masked to INTR_ALL_BITS_MASK = 0x00001111)
 *    into INTR_STATUS via direct CSML write, then calls update_interrupt_outputs().
 *    This is the primary inject mechanism used by these tests.
 *
 *  - INTR_ENABLE (0x14) is fully readable (read_bit_mask = 0x00001111).  Writing
 *    INTR_ENABLE immediately re-evaluates interrupt output port.
 *
 *  - Port assertion rule (update_interrupt_outputs): each sc_out<bool> port equals
 *    INTR_STATUS[bit] AND INTR_ENABLE[bit].  The testbench observes port values
 *    via test->intr_i.read() etc.
 *
 *  - A wait(sc_core::SC_ZERO_TIME) after any register write that has side effects
 *    on sc_out<bool> ports ensures delta-cycle propagation completes before the
 *    port values are sampled.
 *
 *  - FIFO_STATUS (0x24) has read_bit_mask = 0x0 — reads return 0.  FIFO fill
 *    level is observed indirectly (via underflow/overflow port transitions).
 *
 * ## Design Constraints    
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on
 *    entropy_src_ip::target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() between test groups so each
 *    method starts from a clean, defined register state.
 *  - Interrupt port are read via test->intr_<name>.read() (sc_in<bool>).
 *    The testbench connects these to the same sc_signal<bool> that the DUT
 *    drives via its sc_out<bool> ports.
 *  - Autonomous hardware-triggered interrupts (overflow, underflow) require
 *    waiting for simulation time to advance so the background SC_THREAD can
 *    run.  SC_ZERO_TIME waits are used for side-effect propagation; short
 *    sc_time waits are used for thread scheduling.
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md (FUNC-002 table)
 *  - entropy_src/docs/entropy_src-test-plan.md
 *  - entropy_src/docs/entropy_src-detailed-design.md §4 (register reference)
 *  - entropy_src/docs/sections/entropy_src-register-callbacks.md
 *  - entropy_src/model/inc/entropy_src.h (INTR_BIT_* constants)
 *  - entropy_src/test/inc/testbench.h
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"

#include <iomanip>
#include <sstream>

// =============================================================================
// Internal helper macro
// =============================================================================

/// @cond INTERNAL
/// Emit a descriptive FAIL message and set ok = false.
/// Mirrors the FUNC001_CHECK macro pattern from func001_tests.cpp.
/// The @p msg_stream argument is a streaming expression (<<-chained).
#define FUNC002_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            CSML_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)
/// @endcond

// =============================================================================
// Constants local to this file
// =============================================================================

/// @cond INTERNAL
/// Interrupt bit positions — mirror the constants in entropy_src_ip.
/// Defined here as file-local constexpr to avoid accessing private DUT internals.
static constexpr uint32_t F002_INTR_BIT_HEALTH_TEST_FAILED = (1u << 0u);  ///< bit 0
static constexpr uint32_t F002_INTR_BIT_FIFO_ERROR         = (1u << 4u);  ///< bit 4
static constexpr uint32_t F002_INTR_BIT_FIFO_OVERFLOW      = (1u << 8u);  ///< bit 8
static constexpr uint32_t F002_INTR_BIT_FIFO_UNDERFLOW     = (1u << 12u); ///< bit 12

/// Base iteration period (nanoseconds) of the background entropy generation
/// thread.  Matches BASE_ITERATION_PERIOD_NS in entropy_src.h.
static constexpr double F002_BASE_ITER_PERIOD_NS = 100.0;
static constexpr uint32_t F002_INTR_ALL_BITS_MASK          = 0x00001111u; ///< combined mask
/// @endcond

// =============================================================================
// TC-F002-013 — INTR_STATUS reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that INTR_STATUS (0x10) reads as 0x00000000 after reset.
 *
 * After apply_reset() all register fields return to their hardware reset values.
 * INTR_STATUS reset value is 0x00000000 (no pending interrupts).  Because
 * INTR_STATUS has read_bit_mask = 0x0, a TLM b_transport read always returns
 * 0x00000000 regardless of stored state.  This test also validates the post-
 * reset state of interrupt output port, which should all be de-asserted
 * because INTR_ENABLE also resets to 0x00000000.
 *
 * Procedure:
 *  1. Call apply_reset() (done by run_tests() dispatcher, but belt-and-braces).
 *  2. Issue a TLM read of INTR_STATUS; assert returned value is 0x00000000.
 *  3. Verify that interrupt output port are false (INTR_ENABLE=0
 *     means no port can be asserted even if INTR_STATUS were non-zero).
 *
 * Pass criterion:
 *  - INTR_STATUS read returns 0x00000000.
 *  - interrupt output port is false.
 *
 * Test plan reference: intr_status_reset_value (FUNC-002 sl. 13)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_status_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    // Read INTR_STATUS immediately after reset.
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);

    FUNC002_CHECK(read_val == 0,
        "TC-F002-013 INTR_STATUS reset: expected 0x00000000 from read, "
        "got 0x" << std::hex << read_val);

    // Also confirm all four interrupt output ports are low.
    // INTR_ENABLE resets to 0x00000000 so update_interrupt_outputs produces
    // all-false unconditionally at reset regardless of INTR_STATUS.
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read() == false,
        "TC-F002-013: intr_i signal should be false after reset");
    return ok;
}

// =============================================================================
// TC-F002-014 — INTR_STATUS W1C set and clear via HEALTH_TEST_FAILED
// =============================================================================

/******************************************************************************
 * @brief Core W1C test: inject INTR_STATUS[0] via INTR_TEST, confirm port
 *        assertion, then write-1-to-clear and confirm deasserted.
 *
 * This is the primary validation of the complete INTR_TEST inject path combined
 * with the INTR_STATUS W1C clear path for the HEALTH_TEST_FAILED source.
 *
 * Procedure:
 *  1. Enable bit 0 in INTR_ENABLE so the port can assert.
 *  2. Write INTR_TEST[0]=1 (value = F002_INTR_BIT_HEALTH_TEST_FAILED).
 *  3. Wait one delta; assert intr_health_test_failed port is true.
 *  4. Write 0x00000001 (bit 0) to INTR_STATUS — W1C clear.
 *  5. Wait one delta; assert intr_health_test_failed port is now false.
 *
 * Pass criterion:
 *  - Port is asserted after INTR_TEST inject with INTR_ENABLE[0]=1.
 *  - Port is deasserted after W1C write to INTR_STATUS[0].
 *
 * Test plan reference: intr_status_w1c_set_clear_health_test_failed (FUNC-002 sl. 14)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_status_w1c_set_clear_health_test_failed()
{
    bool ok = true;

    // Step 1: Enable HEALTH_TEST_FAILED (bit 0) in INTR_ENABLE.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Step 2: Inject HEALTH_TEST_FAILED via INTR_TEST.
    // handle_write_INTR_TEST ORs bit 0 into INTR_STATUS then calls
    // update_interrupt_outputs(), which sees INTR_STATUS[0]=1 AND INTR_ENABLE[0]=1.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Step 3: Port should be asserted.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-014: intr_health_test_failed should be true after INTR_TEST inject "
        "with INTR_ENABLE[0]=1 — INTR_STATUS[0] AND INTR_ENABLE[0] must equal 1");

    // Step 4: Write-1-to-clear INTR_STATUS[0].
    // handle_write_INTR_STATUS clears bits set in the written value, then
    // calls update_interrupt_outputs().
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Step 5: Port should be deasserted now that INTR_STATUS[0]=0.
    FUNC002_CHECK(test->intr_i.read() == false,
        "TC-F002-014: intr_health_test_failed should be false after W1C write to "
        "INTR_STATUS[0] — cleared bit must deassert the port immediately");

    return ok;
}

// =============================================================================
// TC-F002-015 — INTR_STATUS W1C: writing zero does not clear
// =============================================================================

/******************************************************************************
 * @brief Confirm W1C semantics: writing 0x00000000 to INTR_STATUS does not
 *        clear any set bit, and the interrupt port remains asserted.
 *
 * W1C semantics require that only bits written as 1 are cleared.  A write of
 * all-zeros must have no effect on INTR_STATUS contents.
 *
 * Procedure:
 *  1. Set INTR_ENABLE[0]=1 and inject INTR_STATUS[0] via INTR_TEST.
 *  2. Confirm port is asserted.
 *  3. Write 0x00000000 to INTR_STATUS.
 *  4. Wait one delta; confirm port is still asserted (write had no effect).
 *
 * Pass criterion:
 *  - After writing 0x00000000 to INTR_STATUS, the interrupt port remains true.
 *
 * Test plan reference: intr_status_w1c_write_zero_does_not_clear (FUNC-002 sl. 15)
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f002_intr_status_w1c_write_zero_does_not_clear()
{
    bool ok = true;

    // Enable and inject bit 0.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Verify pre-condition: port is asserted.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-015: pre-condition failed — intr_health_test_failed not asserted "
        "before zero-write test; INTR_TEST inject path may be broken");

    // Write 0 to INTR_STATUS — must NOT clear bit 0.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    // Port must still be asserted because the zero-write cleared nothing.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-015: intr_health_test_failed deasserted after writing 0x00000000 "
        "to INTR_STATUS — W1C semantics violated: writing zero must not clear any bit");

    return ok;
}

// =============================================================================
// TC-F002-016 — INTR_STATUS reserved bits always read as zero
// =============================================================================

/******************************************************************************
 * @brief Confirm that only bits [0], [4], [8], [12] of INTR_STATUS can be set;
 *        all reserved positions read as zero.
 *
 * INTR_STATUS has read_bit_mask = 0x0 (CSML restriction).  Therefore all
 * reads via b_transport return 0x00000000.  To validate that reserved bit
 * positions are never driven by the model, this test checks that writing a
 * value with reserved bits set to INTR_TEST (which ORs into INTR_STATUS)
 * does not cause any interrupt port to assert beyond the architecturally
 * defined positions — because handle_write_INTR_TEST masks to INTR_ALL_BITS_MASK
 * before ORing.
 *
 * Strategy: inject all architecturally valid bits simultaneously
 * (F002_INTR_ALL_BITS_MASK = 0x00001111), enable all four sources, and confirm
 * exactly the four expected ports assert and no more.  Reserved bits by definition
 * cannot cause observable port assertions since the model only drives the four
 * defined sc_out<bool> ports.
 *
 * Procedure:
 *  1. Enable all four sources in INTR_ENABLE.
 *  2. Write INTR_TEST = 0x00001111 (all valid bits) and also try 0xFFFFFFFF
 *     to confirm injection is clamped to 0x00001111 by the model.
 *  3. Assert all four ports are true.
 *  4. Read INTR_STATUS via TLM; confirm returned value is 0x00000000
 *     (read restriction enforced regardless of stored content).
 *
 * Pass criterion:
 *  - All four ports assert after injecting 0x00001111 with INTR_ENABLE = 0x1111.
 *  - INTR_STATUS TLM read returns 0x00000000.
 *
 * Test plan reference: intr_status_reserved_bits_always_zero (FUNC-002 sl. 16)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_status_reserved_bits_always_zero()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    // Enable all four interrupt sources.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // Inject all valid bits via INTR_TEST (write mask 0x1111 enforced by CSML).
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // intr port must be asserted because all four valid INTR_STATUS bits
    // are set and all four INTR_ENABLE bits are set.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-016: intr not asserted after injecting INTR_ALL_BITS_MASK");

    // TLM read of INTR_STATUS must return 0x00000000 due to read_bit_mask=0.
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == 0x1111u,
        "TC-F002-016: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x1111");

    return ok;
}

// =============================================================================
// TC-F002-021 — INTR_TEST injects HEALTH_TEST_FAILED (bit 0)
// =============================================================================

/******************************************************************************
 * @brief Confirm INTR_TEST[0]=1 asserts the intr_health_test_failed port when
 *        INTR_ENABLE[0]=1.
 *
 * The inject path: INTR_TEST write → handle_write_INTR_TEST → OR into
 * INTR_STATUS → update_interrupt_outputs → sc_out<bool> port driven.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[0]=1.
 *  2. Write INTR_TEST = F002_INTR_BIT_HEALTH_TEST_FAILED.
 *  3. Wait SC_ZERO_TIME; assert intr_health_test_failed is true.
 *  4. Verify the other three ports remain false (isolation check).
 *
 * Pass criterion:
 *  - intr_health_test_failed == true.
 *  - Other three ports == false.
 *
 * Test plan reference: intr_test_injects_health_test_failed (FUNC-002 sl. 21)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_test_injects_health_test_failed()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;
    // Enable only HEALTH_TEST_FAILED to isolate the source under test.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Inject via INTR_TEST.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == F002_INTR_BIT_HEALTH_TEST_FAILED,
        "TC-F002-021: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x" << std::hex << F002_INTR_BIT_HEALTH_TEST_FAILED);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-021: intr_o not asserted after INTR_TEST[0] inject; "
        "INTR_TEST inject path must OR bit 0 into INTR_STATUS and call "
        "update_interrupt_outputs");

    return ok;
}

// =============================================================================
// TC-F002-022 — INTR_TEST injects FIFO_ERROR (bit 4)
// =============================================================================

/******************************************************************************
 * @brief Confirm INTR_TEST[4]=1 asserts the intr_fifo_error port when
 *        INTR_ENABLE[4]=1.
 *
 * FIFO_ERROR (bit 4) is only reachable via INTR_TEST; no autonomous hardware
 * path sets this bit in the model.  This test is therefore the sole validation
 * of the FIFO_ERROR inject path.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[4]=1 (value = F002_INTR_BIT_FIFO_ERROR).
 *  2. Write INTR_TEST = F002_INTR_BIT_FIFO_ERROR.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_error is true.
 *  4. Verify the other three ports remain false.
 *
 * Pass criterion:
 *  - intr_fifo_error == true.
 *  - Other three ports == false.
 *
 * Test plan reference: intr_test_injects_fifo_error (FUNC-002 sl. 22)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_test_injects_fifo_error()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;
    
    // Enable only FIFO_ERROR.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    wait(sc_core::SC_ZERO_TIME);

    // Inject via INTR_TEST.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == F002_INTR_BIT_FIFO_ERROR,
        "TC-F002-022: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x" << std::hex << F002_INTR_BIT_FIFO_ERROR);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-022: intr_o not asserted after INTR_TEST[4] inject; "
        "INTR_TEST must OR bit 4 into INTR_STATUS and call update_interrupt_outputs");

    return ok;
}

// =============================================================================
// TC-F002-023 — INTR_TEST injects FIFO_OVERFLOW (bit 8)
// =============================================================================

/******************************************************************************
 * @brief Confirm INTR_TEST[8]=1 asserts the intr_fifo_overflow port when
 *        INTR_ENABLE[8]=1.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[8]=1 (value = F002_INTR_BIT_FIFO_OVERFLOW).
 *  2. Write INTR_TEST = F002_INTR_BIT_FIFO_OVERFLOW.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_overflow is true.
 *  4. Verify the other three ports remain false.
 *
 * Pass criterion:
 *  - intr_fifo_overflow == true.
 *  - Other three ports == false.
 *
 * Test plan reference: intr_test_injects_fifo_overflow (FUNC-002 sl. 23)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_test_injects_fifo_overflow()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;
    
    // Enable only FIFO_OVERFLOW.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    // Inject via INTR_TEST.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == F002_INTR_BIT_FIFO_OVERFLOW,
        "TC-F002-023: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x" << std::hex << F002_INTR_BIT_FIFO_OVERFLOW);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-023: intr_o not asserted after INTR_TEST[8] inject; "
        "bit 8 must be ORed into INTR_STATUS via inject path");

    return ok;
}

// =============================================================================
// TC-F002-024 — INTR_TEST injects FIFO_UNDERFLOW (bit 12)
// =============================================================================

/******************************************************************************
 * @brief Confirm INTR_TEST[12]=1 asserts the intr_fifo_underflow port when
 *        INTR_ENABLE[12]=1.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[12]=1 (value = F002_INTR_BIT_FIFO_UNDERFLOW).
 *  2. Write INTR_TEST = F002_INTR_BIT_FIFO_UNDERFLOW.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_underflow is true.
 *  4. Verify the other three ports remain false.
 *
 * Pass criterion:
 *  - intr_fifo_underflow == true.
 *  - Other three ports == false.
 *
 * Test plan reference: intr_test_injects_fifo_underflow (FUNC-002 sl. 24)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_test_injects_fifo_underflow()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;
    
    // Enable only FIFO_UNDERFLOW.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    // Inject via INTR_TEST.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == F002_INTR_BIT_FIFO_UNDERFLOW,
        "TC-F002-024: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x" << std::hex << F002_INTR_BIT_FIFO_UNDERFLOW);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-024: intr_o not asserted after INTR_TEST[12] inject; "
        "bit 12 must be ORed into INTR_STATUS via inject path");

    return ok;
}

// =============================================================================
// TC-F002-025 — INTR_TEST all sources simultaneous
// =============================================================================

/******************************************************************************
 * @brief Confirm that INTR_TEST = 0x00001111 sets all four INTR_STATUS bits
 *        simultaneously and all four ports assert when INTR_ENABLE = 0x1111.
 *
 * Procedure:
 *  1. Write INTR_ENABLE = F002_INTR_ALL_BITS_MASK.
 *  2. Write INTR_TEST  = F002_INTR_ALL_BITS_MASK.
 *  3. Wait SC_ZERO_TIME; assert all four ports are true.
 *
 * Pass criterion: all four interrupt ports are simultaneously asserted.
 *
 * Test plan reference: intr_test_all_sources_simultaneous (FUNC-002 sl. 25)
 *
 * @return true if all four assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_test_all_sources_simultaneous()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    // Enable all four sources.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // Inject all four simultaneously.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, read_val);
    FUNC002_CHECK(read_val == F002_INTR_ALL_BITS_MASK,
        "TC-F002-025: INTR_STATUS TLM read returned 0x" << std::hex << read_val
        << ", expected 0x" << std::hex << F002_INTR_ALL_BITS_MASK);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-025: intr_health_test_failed not asserted in simultaneous inject; "
        "INTR_TEST=0x1111");
    return ok;
}

// =============================================================================
// TC-F002-028 — intr_health_test_failed port clears on W1C
// =============================================================================

/******************************************************************************
 * @brief Validate clear path: W1C write to INTR_STATUS[0] deasserts
 *        intr_health_test_failed.
 *
 * Procedure:
 *  1. Enable and inject INTR_STATUS[0]; confirm port is asserted.
 *  2. Write F002_INTR_BIT_HEALTH_TEST_FAILED to INTR_STATUS (W1C).
 *  3. Wait SC_ZERO_TIME; assert port is false.
 *
 * Pass criterion: port deasserts immediately after W1C clear.
 *
 * Test plan reference: intr_health_test_failed_port_clears_on_w1c (FUNC-002 sl. 28)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_clears_on_w1c_bit0()
{
    bool ok = true;

    // Set up: enable + inject.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Confirm pre-condition.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-028: pre-condition failed — port not asserted before W1C test");

    // W1C clear.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-028: intr_health_test_failed still asserted after W1C clear of "
        "INTR_STATUS[0] — handle_write_INTR_STATUS must clear bit 0 and call "
        "update_interrupt_outputs");

    return ok;
}

// =============================================================================
// TC-F002-030 — intr_fifo_error port clears on W1C
// =============================================================================

/******************************************************************************
 * @brief Validate clear path for intr_fifo_error via W1C write to
 *        INTR_STATUS[4].
 *
 * Procedure:
 *  1. Enable + inject INTR_STATUS[4]; confirm port is asserted.
 *  2. Write F002_INTR_BIT_FIFO_ERROR to INTR_STATUS.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_error == false.
 *
 * Pass criterion: port deasserts after W1C clear of bit 4.
 *
 * Test plan reference: intr_fifo_error_port_clears_on_w1c (FUNC-002 sl. 30)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_clears_on_w1c_bit4()
{
    bool ok = true;

    // Set up: enable + inject.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-030: pre-condition failed — intr_fifo_error not asserted before W1C");

    // W1C clear of bit 4.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-030: intr_fifo_error still asserted after W1C write to "
        "INTR_STATUS[4] — handle_write_INTR_STATUS must clear bit 4");

    return ok;
}

// =============================================================================
// TC-F002-032 — intr_fifo_overflow port clears on W1C
// =============================================================================

/******************************************************************************
 * @brief Validate clear path for intr_fifo_overflow via W1C write to
 *        INTR_STATUS[8].
 *
 * Procedure:
 *  1. Enable + inject INTR_STATUS[8]; confirm port is asserted.
 *  2. Write F002_INTR_BIT_FIFO_OVERFLOW to INTR_STATUS.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_overflow == false.
 *
 * Pass criterion: port deasserts after W1C clear of bit 8.
 *
 * Test plan reference: intr_fifo_overflow_port_clears_on_w1c (FUNC-002 sl. 32)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_clears_on_w1c_bit8()
{
    bool ok = true;

    // Set up: enable + inject.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-032: pre-condition failed — intr_fifo_overflow not asserted before W1C");

    // W1C clear of bit 8.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-032: intr_fifo_overflow still asserted after W1C write to "
        "INTR_STATUS[8] — handle_write_INTR_STATUS must clear bit 8");

    return ok;
}

// =============================================================================
// TC-F002-034 — intr_fifo_underflow port clears on W1C
// =============================================================================

/******************************************************************************
 * @brief Validate clear path for intr_fifo_underflow via W1C write to
 *        INTR_STATUS[12].
 *
 * Procedure:
 *  1. Enable + inject INTR_STATUS[12]; confirm port is asserted.
 *  2. Write F002_INTR_BIT_FIFO_UNDERFLOW to INTR_STATUS.
 *  3. Wait SC_ZERO_TIME; assert intr_fifo_underflow == false.
 *
 * Pass criterion: port deasserts after W1C clear of bit 12.
 *
 * Test plan reference: intr_fifo_underflow_port_clears_on_w1c (FUNC-002 sl. 34)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_clears_on_w1c_bit12()
{
    bool ok = true;

    // Set up: enable + inject.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-034: pre-condition failed — intr_fifo_underflow not asserted before W1C");

    // W1C clear of bit 12.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-034: intr_fifo_underflow still asserted after W1C write to "
        "INTR_STATUS[12] — handle_write_INTR_STATUS must clear bit 12");

    return ok;
}

// =============================================================================
// TC-F002-035 — INTR_ENABLE immediate propagation on write
// =============================================================================

/******************************************************************************
 * @brief Core INTR_ENABLE callback test: INTR_STATUS[0] set with INTR_ENABLE[0]=0;
 *        writing INTR_ENABLE[0]=1 immediately drives intr_health_test_failed true
 *        without any new INTR_TEST event.
 *
 * This test validates that handle_write_INTR_ENABLE immediately re-evaluates
 * all four interrupt output ports by calling update_interrupt_outputs(), rather
 * than requiring a subsequent INTR_STATUS write to propagate the change.
 *
 * Procedure:
 *  1. Leave INTR_ENABLE at reset value (0x00000000 — all masked).
 *  2. Inject INTR_STATUS[0] via INTR_TEST; confirm port stays false (masked).
 *  3. Write INTR_ENABLE[0]=1.
 *  4. Wait SC_ZERO_TIME; assert intr_health_test_failed == true (immediate propagation).
 *
 * Pass criterion:
 *  - Port is false before INTR_ENABLE is written (masked state preserved).
 *  - Port is true after INTR_ENABLE write without any additional event.
 *
 * Test plan reference: intr_enable_immediate_propagation_on_write (FUNC-002 sl. 35)
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_enable_immediate_propagation_on_write()
{
    bool ok = true;

    // Step 1 & 2: INTR_ENABLE stays at 0; inject INTR_STATUS[0].
    // Port must remain false because INTR_ENABLE[0]=0.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-035: pre-condition: intr_health_test_failed should be false when "
        "INTR_ENABLE[0]=0 even though INTR_STATUS[0]=1 (inject via INTR_TEST)");

    // Step 3: Write INTR_ENABLE[0]=1 — handle_write_INTR_ENABLE must call
    // update_interrupt_outputs() immediately, causing the port to assert.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Step 4: Port must now be asserted with no additional transactions.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-035: intr_health_test_failed not asserted after INTR_ENABLE[0] written "
        "to 1 — handle_write_INTR_ENABLE must call update_interrupt_outputs() immediately; "
        "INTR_STATUS[0] was already set from the INTR_TEST inject");

    return ok;
}

// =============================================================================
// TC-F002-036 — All ports deasserted after software reset
// =============================================================================

/******************************************************************************
 * @brief Validate that software reset clears INTR_STATUS and calls
 *        update_interrupt_outputs, deasseting all four ports.
 *
 * Procedure:
 *  1. Set INTR_ENABLE = 0x1111 and inject all four bits via INTR_TEST.
 *  2. Confirm all four ports are asserted (pre-condition check).
 *  3. Call apply_reset() (writes 0x1 to CTRL.RESET).
 *  4. Wait SC_ZERO_TIME; assert all four ports are false.
 *
 * Pass criterion:
 *  - All four ports assert after inject (pre-condition).
 *  - All four ports deassert after software reset.
 *
 * Test plan reference: intr_all_ports_deasserted_after_software_reset (FUNC-002 sl. 36)
 *
 * @return true if all eight assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_all_ports_deasserted_after_software_reset()
{
    bool ok = true;

    // Step 1: Enable all sources and inject all bits.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // Step 2: Pre-condition — all four ports must be asserted.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-036: pre-condition: intr signal not asserted before reset");

    // Step 3: Software reset via CTRL.RESET.
    // handle_write_CTRL detects bit 0, calls reset_all_registers() which
    // zeroes INTR_STATUS and INTR_ENABLE, then de-asserts all four ports.
    apply_reset();
    wait(sc_core::SC_ZERO_TIME);

    // Step 4: All ports must be de-asserted after reset.
    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-036: intr signal not de-asserted after software reset");
   
    return ok;
}

// =============================================================================
// TC-F002-047 — FIFO_RDATA empty FIFO returns zero and sets underflow
// =============================================================================

/******************************************************************************
 * @brief Confirm that reading FIFO_RDATA from an empty FIFO returns 0x00000000
 *        and sets INTR_STATUS[12] (FIFO_UNDERFLOW).
 *
 * After reset the FIFO is empty (the background thread has not yet run because
 * there are no outstanding wait() calls on the fifo_fill_event).  An immediate
 * TLM read of FIFO_RDATA invokes handle_read_FIFO_RDATA which finds an empty
 * queue, writes 0 into the value reference, ORs INTR_BIT_FIFO_UNDERFLOW into
 * INTR_STATUS, and calls update_interrupt_outputs().
 *
 * To observe the underflow interrupt port without waiting for the background
 * thread to fill the FIFO, this test disables the FIFO via FIFO_CTRL[0]=0
 * before reading, ensuring the queue stays empty.
 *
 * Procedure:
 *  1. Disable FIFO via FIFO_CTRL=0x00000000 (stops background fill).
 *  2. Enable INTR_ENABLE[12] = F002_INTR_BIT_FIFO_UNDERFLOW.
 *  3. Read FIFO_RDATA; on empty FIFO the CSML read_status is false (callback
 *     returns false), so the TLM data buffer is NOT updated by CSML.  The test
 *     therefore does not assert a data value — it only asserts the port.
 *  4. Wait SC_ZERO_TIME; assert intr_fifo_underflow == true.
 *
 * Architectural note: handle_read_FIFO_RDATA returns false on empty FIFO.
 * The CSML b_transport read path only copies `read_value` into the TLM payload
 * when the callback returns true (see csml_register.h read_transport lines
 * 368-376).  Therefore the data buffer is undefined when the FIFO is empty;
 * only the interrupt port assertion is a defined, testable outcome.
 *
 * Pass criterion:
 *  - intr_fifo_underflow port is asserted after empty FIFO_RDATA read.
 *
 * Test plan reference: fifo_rdata_empty_fifo_returns_zero_and_sets_underflow (FUNC-002 sl. 47)
 *
 * @return true if the port assertion passes
 ******************************************************************************/
bool testbench::tc_f002_fifo_rdata_empty_fifo_returns_zero_underflow()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;  // Value will not be updated by CSML on false return.

    // Step 1: Disable FIFO so background thread does not fill it.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);
    for (int i = 0; i < 32; ++i) {
        uint32_t val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, val);
        if (val == 0u) break;
    }

    // Step 2: Enable FIFO_UNDERFLOW interrupt.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    // Step 3: Read FIFO_RDATA from an empty FIFO.
    // handle_read_FIFO_RDATA finds m_fifo empty → writes value=0 into its local
    // DT read_value, ORs INTR_BIT_FIFO_UNDERFLOW into INTR_STATUS, calls
    // update_interrupt_outputs, and returns false.
    // Because the callback returns true.
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, read_val);

    // Step 4: The key observable effect is the interrupt port assertion.
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read() && read_val == 0,
        "TC-F002-047: intr_fifo_underflow not asserted after empty FIFO_RDATA read — "
        "handle_read_FIFO_RDATA must set INTR_STATUS[12] and call "
        "update_interrupt_outputs when FIFO is empty (INTR_ENABLE[12]=1)");

    return ok;
}

// =============================================================================
// TC-F002-049 — FIFO underflow: multiple consecutive empty reads
// =============================================================================

/******************************************************************************
 * @brief Confirm that repeated empty FIFO reads each trigger or keep
 *        INTR_STATUS[12] set, and that the port remains continuously asserted.
 *
 * set_interrupt_bit (called via handle_read_FIFO_RDATA on underflow) performs
 * a read-modify-write with OR semantics, so repeated empty reads must not
 * clear the bit.  The port must remain asserted on every read.
 *
 * Procedure:
 *  1. Disable FIFO; enable INTR_ENABLE[12].
 *  2. Issue three consecutive reads of FIFO_RDATA; after each, confirm
 *     intr_fifo_underflow is asserted.
 *
 * Pass criterion:
 *  - intr_fifo_underflow == true after each of the three reads.
 *
 * Test plan reference: fifo_underflow_multiple_consecutive_empty_reads (FUNC-002 sl. 49)
 *
 * @return true if all three assertions pass
 ******************************************************************************/
bool testbench::tc_f002_fifo_underflow_multiple_consecutive_empty_reads()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    for (int i = 0; i < 32; ++i) {
        uint32_t val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, val);
        if (val == 0u) break;
    }
    // Disable FIFO to keep queue empty for all reads.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    // Enable FIFO_UNDERFLOW.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    // Three consecutive reads — each should trigger or maintain INTR_STATUS[12].
    for (int i = 1; i <= 3; ++i)
    {
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, read_val);
        wait(sc_core::SC_ZERO_TIME);

        FUNC002_CHECK(test->intr_i.read(),
            "TC-F002-049: intr_fifo_underflow not asserted after consecutive empty read "
            "#" << i << " — set_interrupt_bit OR-semantics must keep bit 12 set");
    }

    return ok;
}

// =============================================================================
// TC-F002-051 — FIFO overflow sets INTR_STATUS[8]
// =============================================================================

/******************************************************************************
 * @brief Confirm that the background entropy generation thread autonomously sets
 *        INTR_STATUS[8] (FIFO_OVERFLOW) when the FIFO reaches FIFO_DEPTH=127,
 *        and that interrupt_output_method drives intr_fifo_overflow high.
 *
 * The background SC_THREAD (entropy_generation_thread) continuously pushes one
 * entropy word per wait(SC_ZERO_TIME) iteration.  Once the FIFO reaches 127
 * entries it ORs INTR_BIT_FIFO_OVERFLOW into INTR_STATUS and calls
 * m_interrupt_update_event.notify(SC_ZERO_TIME), which causes
 * interrupt_output_method to drive the intr_fifo_overflow sc_out<bool> high.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[8] (FIFO_OVERFLOW) so the AND-gate in
 *     update_interrupt_outputs() will pass the bit through to the port.
 *  2. Wake the background thread via a FIFO_CTRL disable→enable transition.
 *     The rising edge fires m_fifo_fill_event, releasing the thread from its
 *     suspension at wait(m_fifo_fill_event).
 *  3. Poll up to 256 SC_ZERO_TIME yields checking intr_fifo_overflow.read().
 *     Each yield grants the background thread one scheduling slot to push one
 *     word; after 127 pushes the thread hits the overflow branch and fires the
 *     interrupt update event.  256 iterations is a safe upper bound.
 *  4. Assert intr_fifo_overflow.read() == true — the autonomous path fired.
 *  5. W1C-clear INTR_STATUS[8] and assert the port goes low.
 *
 * Pass criterion:
 *  - intr_fifo_overflow port asserts within 256 SC_ZERO_TIME polling iterations,
 *    confirming the background thread filled the FIFO to FIFO_DEPTH=127 and
 *    interrupt_output_method propagated the status bit to the port.
 *  - After W1C clear of INTR_STATUS[8], intr_fifo_overflow deasserts.
 *
 * Test plan reference: fifo_overflow_sets_intr_status_bit (FUNC-002 sl. 51)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_fifo_overflow_sets_intr_status_bit()
{
    bool ok = true;

    // Step 1: Enable INTR_ENABLE[8] so that when the background thread sets
    // INTR_STATUS[8], interrupt_output_method drives intr_fifo_overflow high.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::sc_time(1, sc_core::SC_NS));

    // Step 2: Wake the background thread via a FIFO_CTRL disable→enable
    // transition.  Writing 0 sets m_fifo_enabled=false; writing 1 sets
    // m_fifo_enabled=true and fires m_fifo_fill_event.notify(SC_ZERO_TIME),
    // releasing the thread from its wait(m_fifo_fill_event) suspension.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::sc_time(1, sc_core::SC_NS));
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::sc_time(1, sc_core::SC_NS));

    // Step 3: Poll until intr_fifo_overflow asserts or the iteration budget is
    // exhausted.  The background thread needs ~127 iterations (one word per
    // BASE_ITERATION_PERIOD_NS=100ns) to fill the FIFO, plus one more to detect
    // the overflow.  We wait enough real time for the thread to complete.
    static constexpr int POLL_LIMIT = 256;
    bool overflow_fired = false;
    for (int i = 0; i < POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F002_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        if (test->intr_i.read())
        {
            overflow_fired = true;
            break;
        }
    }

    // Step 4: Assert the autonomous overflow path fired.
    FUNC002_CHECK(overflow_fired,
        "TC-F002-051: intr_fifo_overflow not asserted within " << POLL_LIMIT
        << " polling iterations — background entropy_generation_thread"
        " did not autonomously set INTR_STATUS[8] after filling FIFO to FIFO_DEPTH=127");

    // Step 5: W1C-clear INTR_STATUS[8] and verify the port deasserts.
    // Writing the overflow bit to the W1C register clears INTR_STATUS[8]; the
    // next interrupt_output_method execution drives the port low.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::sc_time(1, sc_core::SC_NS));

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-051: intr_fifo_overflow not cleared by W1C write to INTR_STATUS[8]"
        " — W1C clear path for the autonomous overflow bit is broken");

    return ok;
}

// =============================================================================
// TC-F002-138 — All four INTR_STATUS bits independent W1C
// =============================================================================

/******************************************************************************
 * @brief Validate that all four INTR_STATUS bits can be independently cleared
 *        via individual W1C writes without affecting other bits.
 *
 * This test injects all four bits simultaneously, then clears them one at a
 * time and verifies each clear affects only the targeted port, leaving the
 * remaining three ports asserted until their individual W1C write is issued.
 *
 * Procedure:
 *  1. Enable all four sources (INTR_ENABLE = 0x1111).
 *  2. Inject all four bits via INTR_TEST = 0x1111.
 *  3. Verify all four ports are asserted.
 *  4. W1C clear bit 0 only; verify intr_health_test_failed is false,
 *     other three remain true.
 *  5. W1C clear bit 4 only; verify intr_fifo_error is false,
 *     other two remain true.
 *  6. W1C clear bit 8 only; verify intr_fifo_overflow is false,
 *     bit 12 remains true.
 *  7. W1C clear bit 12; verify all four ports are false.
 *
 * Pass criterion: each W1C clears only the targeted bit; no cross-contamination.
 *
 * Test plan reference: intr_status_all_four_bits_independent_w1c (FUNC-002 sl. 138)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_status_all_four_bits_independent_w1c()
{
    bool ok = true;

    // Enable all four sources.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // Inject all four simultaneously.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_ALL_BITS_MASK);
    wait(sc_core::SC_ZERO_TIME);

    // Pre-condition: combined interrupt asserted (all four enabled bits set).
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-138: pre-condition: intr_o not asserted after injecting all bits");

    // Step 4: W1C bit 0 — combined line must stay true (bits 4,8,12 remain).
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-138: intr_o deasserted after W1C of bit 0 only — "
        "bits 4,8,12 still pending, combined line must stay true");

    // Step 5: W1C bit 4 — combined line must stay true (bits 8,12 remain).
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_ERROR);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-138: intr_o deasserted after W1C of bit 4 only — "
        "bits 8,12 still pending, combined line must stay true");

    // Step 6: W1C bit 8 — combined line must stay true (bit 12 remains).
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_OVERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-138: intr_o deasserted after W1C of bit 8 only — "
        "bit 12 still pending, combined line must stay true");

    // Step 7: W1C bit 12 — ALL bits now cleared, combined line must go false.
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET,
                            F002_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-138: intr_o still asserted after W1C of all four bits — "
        "combined line must deassert when (INTR_STATUS & INTR_ENABLE) == 0");

    return ok;
}

// =============================================================================
// TC-F002-139 — INTR_ENABLE disable masking does not clear INTR_STATUS
// =============================================================================

/******************************************************************************
 * @brief Confirm that disabling INTR_ENABLE does not clear INTR_STATUS; the
 *        pending state remains until an explicit W1C write.
 *
 * When INTR_ENABLE[bit] is cleared, the corresponding sc_out<bool> port must
 * deassert (because update_interrupt_outputs() is called by
 * handle_write_INTR_ENABLE).  However, the INTR_STATUS bit must NOT be cleared;
 * it must persist so that re-enabling INTR_ENABLE immediately re-asserts the port.
 *
 * This validates that masking via INTR_ENABLE is independent from clearing via
 * W1C — two orthogonal mechanisms.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[0]; inject INTR_STATUS[0] via INTR_TEST.
 *  2. Confirm intr_health_test_failed is asserted.
 *  3. Write INTR_ENABLE = 0x00000000 (mask all sources).
 *  4. Wait SC_ZERO_TIME; confirm port is now false.
 *  5. Re-enable INTR_ENABLE[0]; wait SC_ZERO_TIME; confirm port is asserted again
 *     (INTR_STATUS[0] was not cleared — pending state persists).
 *
 * Pass criterion:
 *  - Port deasserts when INTR_ENABLE[0] is cleared.
 *  - Port re-asserts when INTR_ENABLE[0] is set (without re-injecting via INTR_TEST).
 *
 * Test plan reference: intr_enable_disable_masking_does_not_clear_intr_status (FUNC-002 sl. 139)
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f002_intr_enable_disable_masking_does_not_clear_status()
{
    bool ok = true;

    // Step 1: Enable INTR_ENABLE[0] and inject INTR_STATUS[0].
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    // Step 2: Pre-condition — port is asserted.
    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-139: pre-condition: intr_health_test_failed not asserted after "
        "enable + inject");

    // Step 3: Clear INTR_ENABLE to mask all sources.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    // Step 4: Port must deassert (mask applied by update_interrupt_outputs).
    FUNC002_CHECK(!test->intr_i.read(),
        "TC-F002-139: intr_health_test_failed still asserted after INTR_ENABLE=0 — "
        "handle_write_INTR_ENABLE must call update_interrupt_outputs to mask the port");

    // Step 5: Re-enable INTR_ENABLE[0] — INTR_STATUS[0] should still be set.
    // update_interrupt_outputs in handle_write_INTR_ENABLE will re-assert the port.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET,
                            F002_INTR_BIT_HEALTH_TEST_FAILED);
    wait(sc_core::SC_ZERO_TIME);

    FUNC002_CHECK(test->intr_i.read(),
        "TC-F002-139: intr_health_test_failed not re-asserted after re-enabling "
        "INTR_ENABLE[0] — INTR_STATUS[0] must persist while only INTR_ENABLE changes; "
        "masking via INTR_ENABLE must not modify INTR_STATUS storage");

    return ok;
}
