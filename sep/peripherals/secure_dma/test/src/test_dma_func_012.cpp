// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_dma_func_012.cpp
 * @brief FUNC-012: Hardware Trigger Control Disabled test implementation
 *
 * This file implements test cases for DMA Controller hardware
 * trigger functionality when handshake mode is disabled.
 *
 * Test Coverage: 2 test cases - (1) transfers do not occur when
 * hardware_handshake_enable is not set; (2) abort terminates transfer loop
 * (transfer engine abort path coverage).
 *
 * Key Test Infrastructure Requirements:
 * - Helper method: set_lsio_trigger(trigger_index, assert_value) for trigger
 * control
 * - Memory initialization: write_ot_memory_block() for source data setup
 * - Memory verification: read_ot_memory_byte() for destination data checking
 * - STATUS.done polling (no fixed waits) for transfer completion detection
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>
#include <openssl/sha.h>

// =============================================================================
// FUNC-012 Test Orchestration
// =============================================================================

/**
 * @brief Execute all FUNC-012 test cases
 *
 * Runs test for hardware trigger control disabled scenario:
 * - Trigger enabled but handshake mode disabled
 * - Verify no transfer occurs when trigger is asserted
 */
void testbench::run_func012_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-012: Hardware Trigger Control Disabled Tests\n"
                       << "========================================\n"
                       << std::endl;

  wait(20, SC_NS);

  // Test cases
  test_hw_trigger_ctrl_off();
  test_abort_terminates_transfer_loop();
  test_hw_handshake_trigger_ignored_when_go_not_set();
  test_hash_initial_transfer_zero_no_context();
  test_hw_handshake_auto_clear_bus_error_halts();
  test_sha2_requires_four_byte_width_size_error();
  test_chunk_size_exceeds_total_size_warning(); 
  test_hash_init_frees_previous_context_after_failed_transfer();

  REG_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-012 Test Suite Complete\n"
                       << "========================================\n"
                       << std::endl;
}

// =============================================================================
// FUNC-012 TC001: Hardware Trigger with Control Disabled
// =============================================================================

/**
 * @brief Verify that hardware trigger does not initiate transfer when
 * hardware_handshake_enable is disabled in CONTROL register
 *
 * Test Objective:
 * - Confirm DMA does NOT respond to lsio_trigger assertions when
 * hardware_handshake_enable=0
 * - Verify trigger enable in HANDSHAKE_INTR_ENABLE alone is insufficient
 * - Verify no transfer occurs (STATUS.done remains 0)
 * - Verify destination memory remains unchanged
 * - Verify DMA stays in IDLE state
 
 *
 * Pass Criteria:
 * - DMA does not enter BUSY state
 * - STATUS.done remains 0
 * - No data transfer occurs
 * - Destination memory unchanged
 *
 * Architecture Reference: FUNC-011 extension, Test Plan test_hw_trigger_ctrl_off
 */
void testbench::test_hw_trigger_ctrl_off() {
  std::string test_name =
      "LISO FIFO trigger with hardware handshake disabled - no transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;
  const uint8_t sentinel = 0x5A; // Destination fill: no transfer should leave this

  // Source: known pattern (would be transferred if handshake were active)
  unsigned char src_data[64];
  for (uint32_t i = 0; i < total_size; i++) {
    src_data[i] = static_cast<unsigned char>(0x20 + i);
  }
  m_test->write_ot_memory_block(src_addr, src_data, total_size);

  // Destination: sentinel value (must remain unchanged)
  for (uint32_t i = 0; i < total_size; i++) {
    m_test->write_ot_memory_byte(dst_addr + i, sentinel);
  }

  // Configure transfer registers (same as a valid handshake transfer)
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002); // FOUR_BYTE
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001); // increment
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001); // increment

  // Enable trigger 0 in HANDSHAKE_INTR_ENABLE (would be used if handshake were enabled)
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1u << trigger_index));

  // CONTROL: go=0, hardware_handshake_enable=0 (handshake disabled)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x00000000);

  wait(10, SC_NS);

  // Assert LISO FIFO trigger (rising edge)
  m_test->set_lsio_trigger(trigger_index, true);
  wait(50, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);
  wait(100, SC_NS);

  // Verify STATUS: no busy, no done
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status & 0x1) {
    passed = false;
    msg << "STATUS.busy set (expected 0 when handshake disabled); ";
  }
  if (status & 0x2) {
    passed = false;
    msg << "STATUS.done set (expected 0 - no transfer); ";
  }

  // Verify destination unchanged (no data transferred)
  for (uint32_t i = 0; i < total_size && passed; i++) {
    uint8_t dst_byte = m_test->read_ot_memory_r_byte(dst_addr + i);
    if (dst_byte != sentinel) {
      passed = false;
      msg << "Destination byte at offset " << i << " changed (got 0x" << std::hex
          << static_cast<uint32_t>(dst_byte) << ", expected sentinel 0x5A); ";
      break;
    }
  }

  if (passed) {
    msg << "LISO FIFO trigger ignored when hardware_handshake_enable=0; no data transferred.";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-012 TC002: Abort Terminates Transfer Loop (transfer engine coverage)
// =============================================================================

/**
 * @brief Verify CONTROL.abort causes transfer engine to exit loop with
 *        "Abort detected - terminating transfer loop" path
 *
 * Test Objective:
 * - Cover the execute_transfer() path where m_transfer_abort_event.triggered()
 *   is true inside the transaction loop (dma.cpp ~2927-2932)
 * - Start transfer (go=1), wait(SC_ZERO_TIME) so engine enters loop and yields,
 *   then write abort=1 and wait(SC_ZERO_TIME) so engine sees triggered() in the
 *   same delta cycle (requires wait(SC_ZERO_TIME) at loop start in model)
 * - Verify STATUS.aborted set, STATUS.busy cleared, transfer terminated
 *
 * Pass Criteria:
 * - STATUS.aborted is set after abort
 * - STATUS.busy is cleared
 * - No STATUS.done (transfer aborted, not completed)
 *
 * Coverage: transfer_engine_thread / execute_transfer() abort-in-loop branch
 */
void testbench::test_abort_terminates_transfer_loop() {
  std::string test_name =
      "Abort terminates transfer loop (transfer engine abort path)";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Program transfer registers (non-handshake, normal COPY)
  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;
  const uint32_t total_size = 512;
  const uint32_t chunk_size = 256;

  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET,
                            (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2); // 4-byte
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
  wait(10, SC_NS);

  // Start transfer (go=1), yield so engine enters loop and hits wait(SC_ZERO_TIME)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1
  wait(SC_ZERO_TIME);

  // Write abort and yield so engine runs in same delta and sees triggered()
  // (covers "Abort detected - terminating transfer loop" in execute_transfer)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort=1
  wait(SC_ZERO_TIME);

  wait(20, SC_NS);

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);

  if ((status & 0x4) == 0) { // STATUS.aborted (bit 2)
    passed = false;
    msg << "STATUS.aborted not set after abort; ";
  }
  if ((status & 0x1) != 0) { // STATUS.busy (bit 0)
    passed = false;
    msg << "STATUS.busy not cleared after abort; ";
  }
  if ((status & 0x2) != 0) { // STATUS.done (bit 1) - should be 0 (aborted)
    passed = false;
    msg << "STATUS.done set (expected 0 for aborted transfer); ";
  }

  if (passed) {
    msg << "Abort terminates transfer loop: STATUS.aborted=1, busy=0 (engine "
           "abort path covered)";
  }

  report_test_result(test_name, passed, msg.str());
}


/**
 * @brief Verify that when hardware_handshake_enable=1 but go=0, lsio_trigger
 *        assertions are ignored (m_dma_busy is false - no transfer started).
 *
 * Covers handshake_monitor_thread() path: hardware_handshake_enable true,
 * !m_dma_busy -> continue (trigger not processed).
 */
void testbench::test_hw_handshake_trigger_ignored_when_go_not_set() {
  std::string test_name =
      "Hardware handshake trigger ignored when go=0 (DMA not busy)";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;
  const uint8_t sentinel = 0x5A;

  // Source data (OT_R)
  unsigned char src_data[64];
  for (uint32_t i = 0; i < total_size; i++)
    src_data[i] = static_cast<unsigned char>(0x20 + i);
  m_test->write_ot_memory_block(src_addr, src_data, total_size);

  // Destination (OT_W) - must remain unchanged
  for (uint32_t i = 0; i < total_size; i++)
    m_test->write_ot_memory_w_byte(dst_addr + i, sentinel);

  // Transfer config
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1u << trigger_index));

  // Handshake enabled, go=0 -> m_dma_busy stays false; trigger must be ignored
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x00000010);

  wait(10, SC_NS);

  m_test->set_lsio_trigger(trigger_index, true);
  wait(50, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);
  wait(100, SC_NS);

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status & 0x1) { passed = false; msg << "STATUS.busy set; "; }
  if (status & 0x2) { passed = false; msg << "STATUS.done set; "; }

  for (uint32_t i = 0; i < total_size && passed; i++) {
    uint8_t dst_byte = m_test->read_ot_memory_byte(dst_addr + i);
    if (dst_byte != sentinel) {
      passed = false;
      msg << "Destination offset " << i << " changed (got 0x" << std::hex
          << static_cast<uint32_t>(dst_byte) << "); ";
      break;
    }
  }

  if (passed)
    msg << "Trigger ignored when go=0 (m_dma_busy false); ";
  report_test_result(test_name, passed, msg.str());
}


/**
 * @brief Verify hash transfer with initial_transfer=0 and no prior hash
 *        (m_hashing_active false): model logs WARNING and initializes hash anyway.
 *
 * Covers handle_write_CONTROL path: is_hash_opcode && !initial_transfer,
 * !m_hashing_active -> WARNING and hash_init() recovery.
 *
 * Pass Criteria:
 * - Transfer completes with STATUS.done=1, sha2_digest_valid=1
 * - Digest matches OpenSSL reference (single 64-byte source)
 */
void testbench::test_hash_initial_transfer_zero_no_context() {
  std::string test_name =
      "Hash initial_transfer=0 with no active hash context";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t transfer_size = 64;
  unsigned char test_data[64];
  for (uint32_t i = 0; i < transfer_size; i++) {
    test_data[i] = static_cast<unsigned char>(i);
  }

  unsigned char reference_digest[SHA256_DIGEST_LENGTH];
  if (!compute_sha256_reference(test_data, transfer_size, reference_digest)) {
    passed = false;
    msg << "OpenSSL reference computation failed; ";
  }

  m_test->write_ot_memory_block(0x10000000, test_data, transfer_size);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET,
                            transfer_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET,
                            transfer_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  // Start with go=1, initial_transfer=0, opcode=0x1 (no prior hash -> WARNING path)
  uint32_t control_val = 0x80000001 | 0x20; // go=1, initial_transfer=0, opcode=SHA256, digest_swap=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  bool transfer_done = false;
  uint32_t status_after = 0;
  for (int i = 0; i < 100 && !transfer_done; i++) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status_after);
    transfer_done = (status_after & 0x2) != 0;
  }

  if (!transfer_done) {
    passed = false;
    msg << "Transfer timeout - STATUS.done not set; ";
  }

  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status_after);
  if ((status_after & 0x10) == 0) {
    passed = false;
    msg << "STATUS.sha2_digest_valid not set after hash completion; ";
  }

  unsigned char dma_digest[SHA256_DIGEST_LENGTH];
  for (int i = 0; i < 8; i++) {
    uint32_t digest_word = 0;
    m_test->register_read_32(secure_dma_basetest::SHA2_DIGEST_OFFSET + (i * 4),
                             digest_word);
    dma_digest[i * 4 + 0] = (digest_word >> 0) & 0xFF;
    dma_digest[i * 4 + 1] = (digest_word >> 8) & 0xFF;
    dma_digest[i * 4 + 2] = (digest_word >> 16) & 0xFF;
    dma_digest[i * 4 + 3] = (digest_word >> 24) & 0xFF;
  }

  if (!compare_digests(dma_digest, reference_digest, SHA256_DIGEST_LENGTH)) {
    passed = false;
    msg << "SHA-256 digest mismatch; "
        << "Expected: " << format_digest_hex(reference_digest, SHA256_DIGEST_LENGTH)
        << " Got: " << format_digest_hex(dma_digest, SHA256_DIGEST_LENGTH)
        << "; ";
  }

  if (passed) {
    msg << "initial_transfer=0 with no prior hash: WARNING path and hash_init "
           "recovery verified.";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify that a bus error on the automatic interrupt-clearing write
 *        halts the transfer.
 *
 * - Covers handshake_monitor_thread path: perform_interrupt_clearing_write()
 *   returns false (bus error) -> halt_transfer_on_bus_error().
 * - Mirrors secure_dma.sv DmaClearIntrSrc / DmaWaitIntrSrcResponse, where
 *   intr_clear_tlul_rsp_error raises next_error[DmaBusErr] and moves the FSM to
 *   DmaError, so the chunk is never moved.
 * - Uses inject_ot_write_bus_error_once() so the clearing write gets
 *   TLM_ADDRESS_ERROR_RESPONSE.
 */
void testbench::test_hw_handshake_auto_clear_bus_error_halts() {
  std::string test_name =
      "Hardware handshake: interrupt clear bus error -> halt transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;
  const uint32_t intr_clear_addr = 0x10001100;
  const uint32_t intr_clear_value = 0x00000001;

  const unsigned char fifo_value = 0xC0;
  for (uint32_t i = 0; i < 4; i++) {
    m_test->write_ot_memory_byte(src_addr + i, fifo_value);
  }

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1u << trigger_index));
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_SRC_OFFSET,
                            (1u << trigger_index));
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_BUS_OFFSET,
                            (1u << trigger_index));

  m_test->register_write_32(secure_dma_basetest::INTR_SRC_ADDR_OFFSET +
                                (trigger_index * 4),
                            intr_clear_addr);
  m_test->register_write_32(secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET +
                                (trigger_index * 4),
                            intr_clear_value);

  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  uint32_t control_val = 0x80010110;
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Inject bus error for the next OT write (the clearing write).
  m_test->inject_ot_write_bus_error_once();

  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);

  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x8) // error
      break;
    poll_count++;
  }

  if ((status & 0x8) == 0) {
    passed = false;
    msg << "STATUS.error not set after clearing-write bus error; ";
  }
  if (status & 0x2) {
    passed = false;
    msg << "STATUS.done set even though the transfer should have halted; ";
  }
  if (status & 0x1) {
    passed = false;
    msg << "STATUS.busy not cleared after halting on bus error; ";
  }

  uint32_t error_code = 0;
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & 0x10) == 0) {
    passed = false;
    msg << "ERROR_CODE.bus_error not set (ERROR_CODE=0x" << std::hex
        << error_code << std::dec << "); ";
  }

  // The clearing write happens before the chunk moves, so the destination must
  // be untouched.
  uint8_t dst_byte = m_test->read_ot_memory_w_byte(dst_addr);
  if (dst_byte == fifo_value) {
    passed = false;
    msg << "Destination was written despite the halt; ";
  }

  if (passed) {
    REG_INFO(1, logger) << "PASSED: " << test_name << std::endl;
  } else {
    REG_ERROR(0, logger) << "FAILED: " << test_name << " " << msg.str()
                         << std::endl;
  }
  report_test_result(test_name, passed, msg.str());}


  // =============================================================================
// SHA-2 opcode with non-FOUR_BYTE width: ERROR_CODE.size_error set
// Covers validate_transfer_width: is_hashing_opcode && width_encoding != 0x2
// =============================================================================

void testbench::test_sha2_requires_four_byte_width_size_error() {
  std::string test_name =
      "test_sha2_requires_four_byte_width_size_error";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // TRANSFER_WIDTH = ONE_BYTE (0x0) - hashing requires FOUR_BYTE (0x2)
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000000);
  // Minimal transfer config so we reach transfer-width validation
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10001000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10002000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);

  // CONTROL: go=1, opcode=0x1 (SHA256) -> triggers validation
  uint32_t control_val = 0x80000001u; // bit 31 = go, bits [3:0] = 0x1 (SHA256)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(20, SC_NS);

  uint32_t error_code = 0;
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & (1U << 3)) == 0) {
    passed = false;
    msg << "ERROR_CODE.size_error (bit 3) expected set for SHA256 + ONE_BYTE; ";
  }

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & (1U << 3)) == 0) {
    passed = false;
    msg << "STATUS.error (bit 3) expected set; ";
  }

  // Repeat with TWO_BYTE width and opcode SHA384 (0x2) to cover same branch
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000001); // TWO_BYTE
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10001000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10002000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);

  control_val = 0x80000002u; // go=1, opcode=0x2 (SHA384)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(20, SC_NS);

  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & (1U << 3)) == 0) {
    passed = false;
    msg << "ERROR_CODE.size_error expected set for SHA384 + TWO_BYTE; ";
  }

  if (passed) {
    REG_INFO(1, logger) << "PASSED: " << test_name << std::endl;
  } else {
    REG_ERROR(0, logger) << "FAILED: " << test_name << " " << msg.str()
                          << std::endl;
  }
  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// CHUNK_DATA_SIZE > TOTAL_DATA_SIZE: warning only, transfer proceeds
// Covers validate_transfer_size() warning path (chunk_size > total_size)
// =============================================================================

void testbench::test_chunk_size_exceeds_total_size_warning() {
  std::string test_name =
      "CHUNK_DATA_SIZE exceeds TOTAL_DATA_SIZE - warning only, transfer completes";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t total_size = 64;
  const uint32_t chunk_size = 128;  // chunk > total
  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;

  for (uint32_t i = 0; i < 4; i++)
    m_test->write_ot_memory_byte(src_addr + i, 0xA0 + i);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);   // FOUR_BYTE
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  // Start transfer (go=1, opcode=COPY); validation logs warning, does not fail
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000u);  // go=1, opcode=0
  wait(10, SC_NS);

  uint32_t error_code = 0;
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & (1U << 3)) != 0) {
    passed = false;
    msg << "ERROR_CODE.size_error set (chunk>total is warning only, not error); ";
  }

  // Poll for completion (transfer should complete with total_size bytes)
  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 200) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2) break;  // done
    poll_count++;
  }

  if ((status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set; ";
  }

  // Only total_size bytes transferred
  uint8_t v = m_test->read_ot_memory_w_byte(dst_addr);
  if (v != 0xA0) {
    passed = false;
    msg << "Destination data mismatch (expected 0xA0); ";
  }

  if (passed) {
    REG_INFO(1, logger) << "PASSED: " << test_name << std::endl;
  } else {
    REG_ERROR(0, logger) << "FAILED: " << test_name << " " << msg.str()
                          << std::endl;
  }
  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// hash_init() frees existing m_hash_ctx when starting a new hashing transfer
// after a previous hashing transfer failed mid-way (covers 3344-3347)
// =============================================================================

void testbench::test_hash_init_frees_previous_context_after_failed_transfer() {
  std::string test_name =
      "hash_init frees previous context when starting after failed hashing transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t src_addr = 0x10001000;
  const uint32_t dst_addr = 0x10002000;

  for (uint32_t i = 0; i < total_size; i++)
    m_test->write_ot_memory_r_byte(src_addr + i, static_cast<uint8_t>(0x50 + (i & 0xF)));

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  // Cause first source read to fail so transfer stops without calling hash_finalize
  m_test->inject_ot_read_bus_error_once();

  // Start first hashing transfer: hash_init sets m_hash_ctx; first transaction fails
  uint32_t control_val = 0x80000100u | 0x1u;  // go, initial_transfer, SHA256
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(50, SC_NS);

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & (1U << 3)) == 0) {
    passed = false;
    msg << "First transfer should set STATUS.error (bus error); ";
  }

  // Wait until DMA is idle (busy cleared)
  uint32_t poll = 0;
  while (poll < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if ((status & 0x1) == 0) break;  // busy clear
    poll++;
  }

  // Start second hashing transfer: hash_init runs again with m_hash_ctx != nullptr
  // and frees it (covers the block at 3344-3347)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  poll = 0;
  while (poll < 500) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2) break;  // done
    poll++;
  }

  if ((status & 0x2) == 0) {
    passed = false;
    msg << "Second transfer should complete (STATUS.done); ";
  }

  if (passed) {
    REG_INFO(1, logger) << "PASSED: " << test_name << std::endl;
  } else {
    REG_ERROR(0, logger) << "FAILED: " << test_name << " " << msg.str()
                          << std::endl;
  }
  report_test_result(test_name, passed, msg.str());
}