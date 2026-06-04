/******************************************************************************
 * @file kmac_func009_test.h
 * @brief Test declarations for FUNC-KMAC-009 (Application Interface - ROM_CTRL)
 *
 * This header declares test functions for FUNC-KMAC-009, which verifies the
 * ROM Controller hardware-driven cSHAKE256 operation interface. The ROM_CTRL
 * application interface enables boot-time ROM integrity checking without
 * software intervention.
 *
 * Functionality Scope:
 * - ROM_CTRL application interface (app_export[2]) protocol verification
 * - cSHAKE256 with compile-time prefix "ROM_CTRL" operation
 * - Fixed-priority arbitration (ROM_CTRL lowest priority, index 2)
 * - 64-bit data transfer with strobe and last beat indicator
 * - Two-share digest output (share0, share1)
 * - Software MMIO lockout during application interface active state
 * - STATE window read blocking during application operation (key protection)
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Functionality Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md
 * Architecture Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-architecture-behaviour-map.json
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"

// =============================================================================
// FUNC-KMAC-009 Test Function Declarations
// =============================================================================

/**
 * @brief TC-095: ROM_CTRL cSHAKE256 operation test
 *
 * Verifies that the ROM_CTRL application interface (app_export[2]) correctly
 * performs cSHAKE256 hash operation with compile-time prefix "ROM_CTRL".
 * Tests the complete application interface protocol from request to digest
 * delivery.
 *
 * Test Sequence:
 * 1. Verify FSM in IDLE state
 * 2. Initiate ROM_CTRL app interface request with test message
 * 3. Monitor STATUS.sha3_absorb and sha3_squeeze transitions
 * 4. Wait for operation completion
 * 5. Retrieve two-share digest output (share0, share1)
 * 6. Verify digest correctness against OpenSSL reference
 *
 * Pass Criteria:
 * - FSM transitions: IDLE → ABSORB → SQUEEZE → IDLE
 * - STATUS.sha3_absorb = 1 during message absorption
 * - STATUS.sha3_squeeze = 1 during digest generation
 * - Digest output matches OpenSSL cSHAKE256(message, "", "ROM_CTRL")
 * - FSM returns to IDLE after completion
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_rom_ctrl_cshake256_operation(kmac_test* test);

/**
 * @brief TC-096: Fixed-priority arbitration test (ROM_CTRL priority)
 *
 * Verifies fixed-priority arbitration when multiple application interfaces
 * request KMAC simultaneously. ROM_CTRL has lowest priority (index 2),
 * KeyMgr has highest priority (index 0), LC_CTRL has medium priority (index 1).
 *
 * Test Sequence:
 * 1. Configure all three app interfaces with simultaneous requests
 * 2. Monitor which interface is granted access first
 * 3. Verify KeyMgr request serviced first (highest priority)
 * 4. After KeyMgr completion, verify LC_CTRL serviced second
 * 5. After LC_CTRL completion, verify ROM_CTRL serviced last
 *
 * Pass Criteria:
 * - Service order: KeyMgr → LC_CTRL → ROM_CTRL
 * - Each operation completes successfully
 * - No request is dropped or lost
 * - STATUS reflects correct active interface at each stage
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_fixed_priority_arbitration_rom_ctrl(kmac_test* test);

/**
 * @brief TC-097: Application interface 64-bit data transfer protocol test
 *
 * Verifies the 64-bit data transfer protocol used by all application
 * interfaces, including strobe signal and last beat indicator. Uses ROM_CTRL
 * interface as test vehicle but protocol applies to all app interfaces.
 *
 * Test Sequence:
 * 1. Initiate ROM_CTRL app interface with multi-beat message (>64 bits)
 * 2. Monitor data transfers with valid/strobe signals
 * 3. Verify strobe indicates valid data beats
 * 4. Verify last indicator asserts on final data beat
 * 5. Confirm message absorbed correctly
 *
 * Pass Criteria:
 * - Strobe signal correctly indicates valid data
 * - Last indicator asserts only on final beat
 * - Multi-beat message transferred without corruption
 * - Message length correctly determined from last indicator
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_data_interface_rom_ctrl(kmac_test* test);

/**
 * @brief TC-098: Two-share digest output test
 *
 * Verifies that application interface returns digest in two shares (share0,
 * share1) for first-order masking protection. Software/hardware must XOR
 * shares to obtain unmasked digest.
 *
 * Test Sequence:
 * 1. Initiate ROM_CTRL app interface operation with known message
 * 2. Wait for digest generation completion
 * 3. Retrieve share0 and share1 from app interface
 * 4. XOR share0 and share1 to obtain unmasked digest
 * 5. Verify unmasked digest matches OpenSSL reference
 *
 * Pass Criteria:
 * - share0 and share1 are non-zero and different
 * - share0 XOR share1 matches OpenSSL reference digest
 * - Both shares have correct length (256 bits for cSHAKE256)
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_digest_two_share_output_rom_ctrl(kmac_test* test);

/**
 * @brief TC-099: Software MMIO lockout during app active test
 *
 * Verifies that software MMIO access is completely blocked when any
 * application interface is active. Prevents software from interfering with
 * hardware-driven operations.
 *
 * Test Sequence:
 * 1. Initiate ROM_CTRL app interface operation
 * 2. While app interface active, attempt software register writes:
 *    - CFG_SHADOWED write (should be ignored)
 *    - CMD.start write (should trigger error)
 *    - MSG_FIFO write (should trigger error)
 * 3. Verify all writes rejected appropriately
 * 4. Check ERR_CODE for expected error codes
 *
 * Pass Criteria:
 * - CFG_SHADOWED write silently ignored (CFG_REGWEN protection)
 * - CMD write generates SwIssuedCmdInAppActive error (0x03)
 * - MSG_FIFO write generates SwPushedMsgFifo error (0x02)
 * - Application operation completes unaffected
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_sw_lockout_during_app_active_rom_ctrl(kmac_test* test);

/**
 * @brief TC-101: STATE window read blocking during app active test
 *
 * Verifies that STATE window reads return 0 when application interface is
 * active. This prevents software from observing sideloaded key material or
 * intermediate digest values during hardware-driven operations.
 *
 * Test Sequence:
 * 1. Initiate ROM_CTRL app interface operation with sideloaded key (if KMAC mode)
 * 2. While operation in SQUEEZE state, attempt STATE window reads
 * 3. Verify all STATE reads return 0x00000000
 * 4. Wait for operation completion
 * 5. Verify normal STATE access still works in IDLE state
 *
 * Pass Criteria:
 * - All STATE window addresses (0x400-0x5FC) return 0 during app active
 * - Both state share (0x400-0x4C7) and mask share (0x500-0x5C7) return 0
 * - Protection applies to entire STATE window
 * - Normal STATE access restored after app interface completes
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_state_read_blocked_during_app_active_rom_ctrl(kmac_test* test);

/**
 * @brief Additional Test: ROM_CTRL empty message handling
 *
 * Verifies ROM_CTRL application interface behavior with empty message
 * (zero-length input). Tests edge case of cSHAKE256 with only prefix
 * and no application data.
 *
 * Test Sequence:
 * 1. Initiate ROM_CTRL app interface with zero-length message
 * 2. Monitor FSM transitions
 * 3. Verify digest generation completes
 * 4. Compare digest with OpenSSL cSHAKE256("", "", "ROM_CTRL")
 *
 * Pass Criteria:
 * - Operation completes without error
 * - Digest matches OpenSSL reference for empty message
 * - FSM returns to IDLE state
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_rom_ctrl_empty_message(kmac_test* test);

/**
 * @brief Additional Test: ROM_CTRL back-to-back operations
 *
 * Verifies ROM_CTRL application interface can perform multiple consecutive
 * operations without software intervention or reconfiguration.
 *
 * Test Sequence:
 * 1. Perform first ROM_CTRL app interface operation
 * 2. Wait for completion
 * 3. Immediately initiate second operation with different message
 * 4. Verify both operations produce correct digests
 *
 * Pass Criteria:
 * - Both operations complete successfully
 * - No spurious errors between operations
 * - Digests match OpenSSL references for respective messages
 * - No software intervention required
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_rom_ctrl_back_to_back_operations(kmac_test* test);
