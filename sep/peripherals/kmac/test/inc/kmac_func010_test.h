// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_func010_test.h
 * @brief Test declarations for FUNC-KMAC-010 (Message FIFO and Packer)
 *
 * This header declares test functions for FUNC-KMAC-010, which verifies the
 * internal message buffering system with automatic byte/halfword/word packing
 * to 64-bit datapath. The MSG_FIFO provides temporal decoupling between
 * software writes and Keccak absorption.
 *
 * Functionality Scope:
 * - MSG_FIFO depth tracking via STATUS.fifo_depth and STATUS.fifo_empty
 * - FIFO empty-to-nonempty and nonempty-to-empty transitions
 * - FIFO full condition detection and backpressure via temporal decoupling
 * - FIFO pass-through mode when SHA3 engine ready
 * - Address window abstraction (any address 0x800-0xFFC appends to FIFO)
 * - Byte/halfword/word write support with internal packer
 * - Partial entry flushing on PROCESS command with zero-padding
 * - Error detection: SwPushedMsgFifo (0x02) for invalid FIFO writes
 * - Register callbacks: handle_write_MSG_FIFO with packing and backpressure
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
 * Architecture Reference: kmac-architecture-behaviour-map.json
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"

// =============================================================================
// FUNC-KMAC-010 Test Function Declarations
// =============================================================================

/**
 * @brief TC-120: FIFO depth tracking test
 *
 * Verifies that STATUS.fifo_depth accurately reflects the number of occupied
 * 64-bit entries in MSG_FIFO. Tests incrementing depth on writes and
 * decrementing on absorption by SHA3 engine.
 *
 * Test Sequence:
 * 1. Verify initial FIFO empty (STATUS.fifo_depth = 0)
 * 2. Configure SHA3-256 mode
 * 3. Issue START command
 * 4. Write multiple MSG_FIFO entries and monitor depth after each write
 * 5. Issue PROCESS command and verify depth decrements during absorption
 * 6. Verify depth returns to 0 after complete absorption
 *
 * Pass Criteria:
 * - STATUS.fifo_depth increments by 1 for each 64-bit (8-byte) entry written
 * - Partial writes (< 8 bytes) don't increment until next write completes entry
 * - Depth accurately reflects FIFO occupancy at all times
 * - Depth returns to 0 after PROCESS completes
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_depth_tracking(kmac_test* test);

/**
 * @brief TC-121: FIFO empty status on reset test
 *
 * Verifies that STATUS.fifo_empty = 1 immediately after reset, indicating
 * MSG_FIFO is empty and ready to accept writes.
 *
 * Test Sequence:
 * 1. Apply hardware reset
 * 2. Read STATUS register
 * 3. Verify fifo_empty bit (bit 14) = 1
 * 4. Verify fifo_depth = 0
 *
 * Pass Criteria:
 * - STATUS.fifo_empty = 1 immediately after reset
 * - STATUS.fifo_depth = 0 after reset
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_empty_status_on_reset(kmac_test* test);

/**
 * @brief TC-122: FIFO empty-to-nonempty transition test
 *
 * Verifies STATUS.fifo_empty transitions from 1 to 0 when MSG_FIFO receives
 * first data write after being empty.
 *
 * Test Sequence:
 * 1. Verify FIFO initially empty (fifo_empty = 1)
 * 2. Configure SHA3-256 and issue START
 * 3. Write first MSG_FIFO entry
 * 4. Read STATUS and verify fifo_empty = 0
 *
 * Pass Criteria:
 * - fifo_empty transitions from 1 → 0 after first write
 * - Transition occurs immediately after write completes
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_empty_to_nonempty_transition(kmac_test* test);

/**
 * @brief TC-123: FIFO nonempty-to-empty transition test
 *
 * Verifies STATUS.fifo_empty transitions from 0 to 1 when MSG_FIFO is
 * completely drained by SHA3 engine absorption.
 *
 * Test Sequence:
 * 1. Write message to MSG_FIFO (verify fifo_empty = 0)
 * 2. Issue PROCESS command to start absorption
 * 3. Monitor STATUS.fifo_empty during absorption
 * 4. Verify fifo_empty = 1 after complete drainage
 *
 * Pass Criteria:
 * - fifo_empty transitions from 0 → 1 when FIFO fully drained
 * - Transition occurs after last entry absorbed by SHA3 engine
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_nonempty_to_empty_transition(kmac_test* test);

/**
 * @brief TC-124: FIFO full condition test
 *
 * Verifies STATUS.fifo_full asserts when MSG_FIFO reaches maximum depth
 * (configured by MsgFifoDepth parameter, typically 10 entries).
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write MSG_FIFO entries until STATUS.fifo_full = 1
 * 3. Verify fifo_depth = MsgFifoDepth
 * 4. Verify subsequent write blocks (backpressure test)
 *
 * Pass Criteria:
 * - STATUS.fifo_full = 1 when fifo_depth reaches MsgFifoDepth
 * - fifo_full = 0 when fifo_depth < MsgFifoDepth
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_full_condition(kmac_test* test);

/**
 * @brief TC-125: FIFO full backpressure blocking test
 *
 * Verifies that MSG_FIFO writes block (temporal decoupling wait) when FIFO
 * is full, until space becomes available through SHA3 engine absorption.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Fill MSG_FIFO to capacity (fifo_full = 1)
 * 3. Attempt additional write and verify it blocks
 * 4. Issue PROCESS to drain FIFO
 * 5. Verify blocked write completes once space available
 *
 * Pass Criteria:
 * - Write to full FIFO blocks (wait statement in TLM b_transport)
 * - Write completes once FIFO has space
 * - No data loss or corruption
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_full_backpressure_blocking(kmac_test* test);

/**
 * @brief TC-126: FIFO pass-through mode test
 *
 * Verifies MSG_FIFO operates in pass-through mode when SHA3 engine is ready
 * and FIFO is empty. Data written to MSG_FIFO bypasses buffering and goes
 * directly to Keccak absorption.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write single MSG_FIFO entry when engine ready
 * 3. Verify FIFO remains empty (pass-through absorbed immediately)
 * 4. Write multiple entries rapidly and verify buffering occurs
 *
 * Pass Criteria:
 * - Single writes when engine ready don't increment fifo_depth
 * - Rapid writes cause buffering (fifo_depth > 0)
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_pass_through_mode(kmac_test* test);

/**
 * @brief TC-127: FIFO address window abstraction test
 *
 * Verifies that any write to address range 0x800-0xFFC appends to MSG_FIFO
 * regardless of specific address within window. This allows burst writes
 * with auto-increment addressing.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write to MSG_FIFO at various addresses: 0x800, 0x810, 0x900, 0xFFC
 * 3. Verify all writes append to FIFO sequentially
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - All addresses in 0x800-0xFFC range behave identically
 * - Address within window doesn't affect data order
 * - Digest matches expected value for sequential message
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_address_window_abstraction(kmac_test* test);

/**
 * @brief TC-128: FIFO byte-write support test
 *
 * Verifies MSG_FIFO supports byte-granularity writes (8-bit TLM transactions)
 * with internal packer accumulating bytes into 64-bit entries.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write message to MSG_FIFO using 8-bit writes
 * 3. Verify fifo_depth increments only after 8 bytes accumulated
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - Byte writes accepted without error
 * - Packer accumulates 8 bytes before incrementing fifo_depth
 * - Digest matches expected value (byte ordering preserved)
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_byte_write_support(kmac_test* test);

/**
 * @brief TC-129: FIFO halfword-write support test
 *
 * Verifies MSG_FIFO supports halfword-granularity writes (16-bit TLM
 * transactions) with internal packer.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write message using 16-bit writes
 * 3. Verify packer accumulates 4 halfwords into 64-bit entry
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - Halfword writes accepted without error
 * - Packer accumulates 4 halfwords before incrementing fifo_depth
 * - Digest matches expected value (halfword ordering preserved)
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_halfword_write_support(kmac_test* test);

/**
 * @brief TC-130: FIFO word-write support test
 *
 * Verifies MSG_FIFO supports word-granularity writes (32-bit TLM
 * transactions) with internal packer.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write message using 32-bit writes
 * 3. Verify packer accumulates 2 words into 64-bit entry
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - Word writes accepted without error
 * - Packer accumulates 2 words before incrementing fifo_depth
 * - Digest matches expected value (word ordering preserved)
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_word_write_support(kmac_test* test);

/**
 * @brief TC-131: FIFO packer partial entry on PROCESS test
 *
 * Verifies internal packer flushes partial 64-bit entry with zero-padding
 * when PROCESS command issued. Ensures message tail is not lost.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write message with non-multiple-of-8 length (e.g., 13 bytes)
 * 3. Verify packer holds partial entry (5 bytes)
 * 4. Issue PROCESS command
 * 5. Verify partial entry flushed with zero-padding
 * 6. Verify correct digest (partial entry included in hash)
 *
 * Pass Criteria:
 * - PROCESS flushes partial entry from packer
 * - Zero-padding applied to complete 64-bit entry
 * - Digest matches expected value with correct message length
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_packer_partial_entry_on_process(kmac_test* test);

/**
 * @brief TC-132: FIFO write before START error test
 *
 * Verifies SwPushedMsgFifo error (0x02) is generated when MSG_FIFO is
 * written before START command (FSM in IDLE state).
 *
 * Test Sequence:
 * 1. Verify FSM in IDLE state
 * 2. Attempt MSG_FIFO write without issuing START
 * 3. Read ERR_CODE register
 * 4. Verify ERR_CODE[31:24] = 0x02 (SwPushedMsgFifo)
 * 5. Verify INTR_STATE.kmac_err = 1
 *
 * Pass Criteria:
 * - ERR_CODE = 0x02 after invalid FIFO write
 * - kmac_err interrupt flag set
 * - FSM remains in IDLE (operation not started)
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_write_before_start_error(kmac_test* test);

/**
 * @brief TC-133: FIFO write after PROCESS error test
 *
 * Verifies SwPushedMsgFifo error (0x02) is generated when MSG_FIFO is
 * written after PROCESS command (FSM in SQUEEZE state).
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write valid message to MSG_FIFO
 * 3. Issue PROCESS command (transition to SQUEEZE)
 * 4. Attempt additional MSG_FIFO write
 * 5. Verify ERR_CODE = 0x02
 *
 * Pass Criteria:
 * - ERR_CODE = 0x02 after post-PROCESS write
 * - Operation continues despite error (digest still valid)
 * - Error recovery via err_processed works
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_write_after_process_error(kmac_test* test);

/**
 * @brief TC-134: FIFO write during app active error test
 *
 * Verifies SwPushedMsgFifo error (0x02) is generated when software attempts
 * MSG_FIFO write while application interface is active.
 *
 * Test Sequence:
 * 1. Initiate KeyMgr application interface operation
 * 2. While app active, attempt software MSG_FIFO write
 * 3. Verify ERR_CODE = 0x02
 * 4. Verify app operation unaffected
 *
 * Pass Criteria:
 * - ERR_CODE = 0x02 for SW write during app active
 * - Application operation completes successfully
 * - Software lockout enforced
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_write_during_app_active_error(kmac_test* test);

/**
 * @brief TC-166: Register callback MSG_FIFO write packing test
 *
 * Verifies handle_write_MSG_FIFO callback correctly packs byte/halfword/word
 * writes into 64-bit datapath entries.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write mixed-size data: bytes, halfwords, words
 * 3. Monitor fifo_depth increments
 * 4. Issue PROCESS and verify correct digest
 * 5. Verify packing preserved byte order
 *
 * Pass Criteria:
 * - All write sizes handled correctly
 * - Byte ordering preserved (little-endian)
 * - Digest matches expected value
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_msg_fifo_write_packing(kmac_test* test);

/**
 * @brief TC-167: Register callback MSG_FIFO backpressure test
 *
 * Verifies handle_write_MSG_FIFO implements temporal decoupling wait when
 * FIFO is full, blocking TLM b_transport until space available.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Fill MSG_FIFO to capacity
 * 3. Initiate write in separate thread (blocks on full FIFO)
 * 4. Issue PROCESS to drain FIFO
 * 5. Verify blocked write completes
 *
 * Pass Criteria:
 * - b_transport blocks on full FIFO (temporal decoupling wait)
 * - Write completes after FIFO space available
 * - No deadlock or timeout
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_msg_fifo_write_backpressure(kmac_test* test);

/**
 * @brief Additional Test: FIFO corner case - alternating read/write
 *
 * Verifies FIFO handles interleaved write and absorption correctly,
 * testing dynamic FIFO behavior during active operation.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write data in small bursts with absorption happening between bursts
 * 3. Monitor fifo_depth fluctuation
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - FIFO handles dynamic fill/drain without error
 * - fifo_depth tracks correctly during fluctuation
 * - Digest matches expected value
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_alternating_read_write(kmac_test* test);

/**
 * @brief Additional Test: FIFO maximum throughput test
 *
 * Verifies FIFO can sustain maximum write throughput (back-to-back writes)
 * without data loss or corruption.
 *
 * Test Sequence:
 * 1. Configure SHA3-256 and issue START
 * 2. Write large message with back-to-back writes (no delays)
 * 3. Monitor FIFO status throughout
 * 4. Issue PROCESS and verify correct digest
 *
 * Pass Criteria:
 * - All writes complete without error
 * - No data loss despite maximum throughput
 * - Digest matches expected value for large message
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_maximum_throughput(kmac_test* test);
