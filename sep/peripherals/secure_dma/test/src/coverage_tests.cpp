// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file coverage_tests.cpp
 * @brief Extra edge-path tests for secure_dma.cpp (error / helper / handshake).
 */

#include "testbench.h"

void testbench::test_cov_bus_name_and_hash_helpers()
{
  std::string test_name = "Coverage: bus-name + hash helper error paths";
  bool passed = true;

  const char *ot = m_model->get_bus_name(secure_dma_model::BusInterface::OT_INTERNAL);
  const char *ctn = m_model->get_bus_name(secure_dma_model::BusInterface::CTN_BUS);
  const char *sys = m_model->get_bus_name(secure_dma_model::BusInterface::SYSTEM_BUS);
  const char *inv = m_model->get_bus_name(secure_dma_model::BusInterface::INVALID);
  const char *unk = m_model->get_bus_name(
      static_cast<secure_dma_model::BusInterface>(99));
  if (!ot || !ctn || !sys || !inv || !unk) {
    passed = false;
  }

  if (m_model->decode_asid(0x0) != secure_dma_model::BusInterface::INVALID ||
      m_model->decode_asid(0x5) != secure_dma_model::BusInterface::INVALID) {
    passed = false;
  }

  m_model->clear_interrupt_state(false, false, true);
  if (m_model->validate_address_alignment(0x0, true, 8u)) {
    passed = false;
  }
  if (m_model->generate_byte_enable_mask(0x0, 8u) != 0u) {
    passed = false;
  }
  if (m_model->validate_address_width_for_bus(
          0, secure_dma_model::BusInterface::INVALID, true) ||
      m_model->validate_address_width_for_bus(
          0, static_cast<secure_dma_model::BusInterface>(99), false)) {
    passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x0);
  if (m_model->execute_single_transaction()) {
    passed = false;
  }
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x7);
  if (m_model->execute_single_transaction()) {
    passed = false;
  }

  unsigned char dummy[4] = {0x11, 0x22, 0x33, 0x44};
  if (m_model->hash_update_data(dummy, 4)) {
    passed = false;
  }
  if (m_model->hash_finalize()) {
    passed = false;
  }
  if (m_model->hash_init(0x4)) {
    passed = false;
  }

  // First init succeeds; second call frees the previous context.
  if (!m_model->hash_init(0x1)) {
    passed = false;
  }
  if (!m_model->hash_init(0x2)) {
    passed = false;
  }

  // Finalize with a bogus algorithm id after a successful init.
  m_model->m_hash_algorithm = 0x7;
  if (m_model->hash_finalize()) {
    passed = false;
  }

  // Re-exercise verbose register reads.
  uint32_t rd = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, rd);
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, rd);
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, rd);
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, rd);
  m_test->register_read_32(secure_dma_basetest::SHA2_DIGEST_OFFSET, rd);

  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x7);
  m_test->register_write_32(secure_dma_basetest::ALERT_TEST_OFFSET, 0x1);
  wait(SC_ZERO_TIME);

  report_test_result(test_name, passed,
                     passed ? "helpers and invalid-hash paths exercised"
                            : "helper/hash error path failed");
}

void testbench::test_cov_handshake_already_high_on_arm()
{
  std::string test_name = "Coverage: handshake trigger already HIGH at arm";
  bool passed = true;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t transfer_size = 32;
  const uint32_t chunk_size = 16;
  unsigned char test_data[32];
  for (uint32_t i = 0; i < transfer_size; i++) {
    test_data[i] = static_cast<unsigned char>(0xA0 + i);
  }
  m_test->write_ot_memory_block(0x10000000, test_data, transfer_size);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, transfer_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x0);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x0);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u) | (0x7u << 4));
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, 0x1);

  m_test->set_lsio_trigger(0, true);
  wait(SC_ZERO_TIME);

  // go=1, hardware_handshake_enable=1, opcode=copy
  const uint32_t control_val = (1u << 31) | (1u << 4);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);

  bool transfer_done = false;
  uint32_t status = 0;
  for (int i = 0; i < 100 && !transfer_done; i++) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    transfer_done = (status & 0x2) != 0;
  }

  m_test->set_lsio_trigger(0, false);
  if (!transfer_done) {
    passed = false;
  }

  report_test_result(test_name, passed,
                     passed ? "armed with trigger already HIGH; first chunk started"
                            : "transfer did not complete with trigger pre-asserted");
}

void testbench::test_cov_hash_reset_frees_context()
{
  std::string test_name = "Coverage: reset frees an active hash context";
  bool passed = true;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (!m_model->hash_init(0x3)) {
    passed = false;
  }
  if (m_model->m_hash_ctx == nullptr) {
    passed = false;
  }

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (m_model->m_hash_ctx != nullptr || m_model->m_hashing_active) {
    passed = false;
  }

  report_test_result(test_name, passed,
                     passed ? "reset_thread released the SHA context"
                            : "hash context still live after reset");
}
