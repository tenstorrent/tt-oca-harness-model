// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_func024_test.h
 * @brief Test declarations for FUNC-KMAC-024 (Temporal Decoupling and Timing Abstraction)
 *
 * This header declares test functions for FUNC-KMAC-024 which validates
 * non-cycle-accurate timing model using TLM-2.0 b_transport temporal
 * decoupling for nominal delays. Tests functional backpressure, entropy
 * latency, and rapid command sequencing without cycle-level timing dependencies.
 *
 * Functionality Scope:
 * - FIFO full backpressure using sc_core::wait temporal decoupling
 * - MSG_FIFO write handler temporal decoupling wait mechanism
 * - Functional entropy request latency abstraction
 * - Rapid command sequences without cycle-accurate timing enforcement
 * - TLM b_transport delay modeling
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
 * Detailed Design: kmac-detailed-design.md (Section 7.8)
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"
#include <systemc.h>

// =============================================================================
// FUNC-KMAC-024 Test Function Declarations
// =============================================================================

/**
 * @brief TC-125: Verify temporal decoupling wait models functional FIFO full stall
 *
 * Validates that MSG_FIFO full condition triggers sc_core::wait() call to
 * implement functional backpressure without cycle-accurate timing. Tests
 * that software write to MSG_FIFO blocks when FIFO reaches maximum depth
 * (MsgFifoDepth parameter, typically 10 entries), model calls wait() with
 * nominal delay, and write completes after FIFO drains.
 *
 * Pass Criteria:
 * - STATUS.fifo_full = 1 when FIFO reaches depth limit
 * - TLM b_transport blocks during MSG_FIFO write when full
 * - Wait duration non-zero but not cycle-accurate
 * - MSG_FIFO write completes successfully after space available
 * - STATUS.fifo_depth decrements as SHA3 engine consumes data
 * - No WaitTimerExpired or other errors during backpressure
 *
 * Test Sequence:
 * 1. Configure SHA3-256 mode
 * 2. Issue START command
 * 3. Write MSG_FIFO rapidly until full (10 entries)
 * 4. Attempt 11th write - should block via temporal decoupling
 * 5. Monitor STATUS.fifo_full transitions
 * 6. Verify write completes without error
 *
 * @param test Pointer to KMAC test harness
 */
void test_temporal_decoupling_fifo_full(kmac_test* test);

/**
 * @brief TC-167: Verify handle_write_MSG_FIFO uses temporal decoupling wait when FIFO full
 *
 * Validates that MSG_FIFO write callback (handle_write_MSG_FIFO) correctly
 * implements temporal decoupling wait mechanism when FIFO full condition
 * detected. Tests internal implementation detail of backpressure modeling.
 *
 * Pass Criteria:
 * - handle_write_MSG_FIFO detects fifo_depth >= MsgFifoDepth
 * - Callback invokes sc_core::wait(delay, SC_NS) with calculated delay
 * - Delay calculation uses clk_i frequency input (approximate)
 * - After wait, FIFO space available and write proceeds
 * - Packer correctly accumulates byte/halfword/word writes to 64-bit
 * - No data corruption during backpressure event
 *
 * Test Sequence:
 * 1. Configure SHA3-256, START command
 * 2. Fill FIFO to depth-1 (9 entries for depth=10)
 * 3. Issue slow MSG_FIFO write (byte granularity) to trigger packer
 * 4. Write 10th entry - should block briefly
 * 5. Write 11th entry - longer block as FIFO drains
 * 6. Verify all data correctly packed and absorbed
 *
 * @param test Pointer to KMAC test harness
 */
void test_msg_fifo_temporal_decoupling_wait(kmac_test* test);

/**
 * @brief TC-106: Verify functional entropy request latency without cycle accuracy
 *
 * Validates that EDN mode entropy request implements functional latency
 * abstraction using TLM temporal decoupling without cycle-accurate modeling.
 * Tests ENTROPY_PERIOD timeout mechanism uses functional timing.
 *
 * Pass Criteria:
 * - entropy_ready assertion with entropy_mode=edn_mode triggers request
 * - Model waits with nominal delay (microseconds range, not cycle-exact)
 * - ENTROPY_PERIOD.wait_timer provides functional timeout boundary
 * - If timeout expires, WaitTimerExpired error (0x04) asserts
 * - If entropy arrives before timeout, PRNG seeded successfully
 * - Timeout does not use cycle-accurate prescaler/wait_timer arithmetic
 * - Delay approximates expected latency using clk_edn_i frequency
 *
 * Test Sequence:
 * 1. Configure ENTROPY_PERIOD with wait_timer=100, prescaler=0
 * 2. Set CFG_SHADOWED entropy_mode=edn_mode (0x1)
 * 3. Assert entropy_ready
 * 4. Issue START command requiring entropy
 * 5. Verify entropy request completes with functional delay
 * 6. Test timeout case by disabling entropy channel availability
 *
 * @param test Pointer to KMAC test harness
 */
void test_functional_entropy_latency(kmac_test* test);

/**
 * @brief TC-186: Verify rapid command sequence handled correctly (no cycle-level timing dependencies)
 *
 * Validates that rapid command sequences (START, PROCESS, RUN, DONE) execute
 * correctly without cycle-level timing constraints. Tests TLM functional
 * abstraction allows back-to-back commands with only logical sequencing
 * requirements.
 *
 * Pass Criteria:
 * - START → PROCESS → DONE sequence completes in rapid succession
 * - No artificial cycle delays between FSM state transitions
 * - Commands processed based on logical readiness, not clock cycles
 * - Multiple back-to-back operations (e.g., 10 consecutive hashes) succeed
 * - FSM transitions occur immediately upon command write
 * - STATUS bits update functionally without cycle-accurate delays
 * - Empty message hash (START immediately followed by PROCESS) works
 *
 * Test Sequence:
 * 1. Configure SHA3-256 mode
 * 2. Issue START command
 * 3. Immediately issue PROCESS (empty message)
 * 4. Immediately read STATE window
 * 5. Immediately issue DONE command
 * 6. Repeat sequence 10 times rapidly
 * 7. Verify all digests correct and no timing-related errors
 *
 * @param test Pointer to KMAC test harness
 */
void test_rapid_command_sequence_no_timing_dependency(kmac_test* test);
