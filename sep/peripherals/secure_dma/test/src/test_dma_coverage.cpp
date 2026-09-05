// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include <sstream>

void testbench::run_coverage_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                       << "Coverage: uncovered model helper paths\n"
                       << "========================================\n"
                       << std::endl;
  m_model->logger.setMaxVerbosity(3);
  wait(20, SC_NS);
  test_cov_helper_error_guards();
  test_cov_hash_reset_and_inactive();
  test_cov_handshake_trigger_already_high();
  test_cov_invalid_asid_transaction();
  test_cov_bus_name_and_hash_helpers();
  test_cov_handshake_already_high_on_arm();
  test_cov_hash_reset_frees_context();
}

void testbench::test_cov_helper_error_guards() {
  std::string test_name = "Coverage: helper error guards";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (m_model->generate_byte_enable_mask(0, 7) != 0) {
    passed = false;
    msg << "invalid width should yield byte_enable 0; ";
  }
  if (m_model->validate_address_alignment(0, true, 8)) {
    passed = false;
    msg << "invalid width should fail alignment; ";
  }
  if (m_model->decode_asid(0x1) != secure_dma_model::BusInterface::INVALID) {
    passed = false;
    msg << "ASID 0x1 should decode INVALID; ";
  }

  (void)m_model->get_bus_name(secure_dma_model::BusInterface::OT_INTERNAL);
  (void)m_model->get_bus_name(secure_dma_model::BusInterface::CTN_BUS);
  (void)m_model->get_bus_name(secure_dma_model::BusInterface::SYSTEM_BUS);
  (void)m_model->get_bus_name(secure_dma_model::BusInterface::INVALID);
  (void)m_model->get_bus_name(static_cast<secure_dma_model::BusInterface>(99));

  if (m_model->validate_address_width_for_bus(
          0, secure_dma_model::BusInterface::INVALID, true)) {
    passed = false;
    msg << "INVALID bus should fail address-width check; ";
  }
  if (m_model->validate_address_width_for_bus(
          0, static_cast<secure_dma_model::BusInterface>(99), false)) {
    passed = false;
    msg << "unknown bus should fail address-width check; ";
  }

  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00);
  if (m_model->select_bus_for_transaction(true) !=
      secure_dma_model::BusInterface::INVALID) {
    passed = false;
    msg << "invalid src ASID should select INVALID bus; ";
  }

  m_model->clear_interrupt_state(false, false, true);

  if (!m_model->validate_security_policy(0x1000, 0x2000, 0x0, 0x0)) {
    passed = false;
    msg << "unknown→unknown security policy should allow; ";
  }

  if (!m_model->hash_init(0x1)) {
    passed = false;
    msg << "SHA-256 hash_init failed; ";
  }
  if (m_model->hash_init(0x4)) {
    passed = false;
    msg << "invalid hash opcode should fail; ";
  }

  report_test_result(test_name, passed, msg.str());
}

void testbench::test_cov_hash_reset_and_inactive() {
  std::string test_name = "Coverage: hash reset and inactive finalize";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (!m_model->hash_init(0x1)) {
    passed = false;
    msg << "hash_init SHA-256 failed; ";
  }

  // Reset while a hash context is live (reset_thread frees m_hash_ctx).
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (m_model->hash_update_data(nullptr, 4)) {
    passed = false;
    msg << "hash_update_data should fail when inactive; ";
  }
  if (m_model->hash_finalize()) {
    passed = false;
    msg << "hash_finalize should fail when inactive; ";
  }

  if (!m_model->hash_init(0x2)) {
    passed = false;
    msg << "hash_init SHA-384 failed; ";
  }
  m_model->m_hash_algorithm = 0;
  if (m_model->hash_finalize()) {
    passed = false;
    msg << "hash_finalize should fail on invalid algorithm; ";
  }

  report_test_result(test_name, passed, msg.str());
}

void testbench::test_cov_handshake_trigger_already_high() {
  std::string test_name = "Coverage: handshake trigger already HIGH";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10000000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 8;
  const uint32_t chunk_size = 4;
  const uint32_t trigger_index = 2;

  unsigned char data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  m_test->write_ot_memory_block(src_addr, data, total_size);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET,
                            (0x7 << 0) | (0x7 << 4));
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1u << trigger_index));

  // Assert trigger before arming so CONTROL.go takes the already-HIGH path.
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);

  const uint32_t control_val = (1u << 31) | (1u << 4); // go + handshake
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);

  bool transfer_done = false;
  uint32_t status = 0;
  for (int i = 0; i < 100 && !transfer_done; i++) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    transfer_done = (status & 0x2) != 0;
  }

  m_test->set_lsio_trigger(trigger_index, false);

  if (!transfer_done) {
    passed = false;
    msg << "STATUS.done not set with trigger already HIGH; ";
  }

  report_test_result(test_name, passed, msg.str());
}

void testbench::test_cov_invalid_asid_transaction() {
  std::string test_name = "Coverage: execute_single_transaction invalid ASID";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_model->m_current_src_addr = 0x10000000;
  m_model->m_current_dst_addr = 0x20000000;

  if (m_model->execute_single_transaction()) {
    passed = false;
    msg << "invalid source ASID should fail the transaction; ";
  }

  report_test_result(test_name, passed, msg.str());
}
