// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file secure_dma_test.cpp
 * @brief DMA Controller test harness implementation
 *
 * Implements register access helper functions and memory simulation
 * for DMA Controller verification.
 */

#include "secure_dma_test.h"
#include "sep_axi_extension.h"
#include <cstring>
#include <iomanip>

namespace {

// The DMA packs the beat at data_ptr[0 .. length) and puts the word-lane
// mask in the byte-enable array (lane = address[1:0]). A packed byte i is
// lane (address[1:0] + i), not index i.
void copy_lanes(uint8_t* dst, const uint8_t* src, unsigned len,
                unsigned char* enables, unsigned enable_len, uint64_t addr)
{
  if (enables == nullptr || enable_len == 0) {
    std::memcpy(dst, src, len);
    return;
  }
  const unsigned lane0 = static_cast<unsigned>(addr & 0x3u);
  for (unsigned i = 0; i < len; ++i) {
    const unsigned lane = lane0 + i;
    if (lane >= enable_len || enables[lane] == TLM_BYTE_ENABLED)
      dst[i] = src[i];
  }
}

}

// ============================================================================
// Memory Address Windowing Constants
// ============================================================================

// Base addresses for memory regions (hardware address space)
constexpr uint64_t OT_MEMORY_BASE  = 0x10000000ULL;  // OpenTitan internal memory
constexpr uint64_t CTN_MEMORY_BASE = 0x20000000ULL;  // SoC Control Network memory
constexpr uint64_t SYS_MEMORY_BASE = 0x40000000ULL;  // SoC System memory

// ============================================================================
// Constructor
// ============================================================================

secure_dma_test::secure_dma_test(sc_module_name name)
    : secure_dma_basetest(name),
      ot_target_socket("ot_target_socket"),
      ctn_target_socket("ctn_target_socket"),
      sys_target_socket("sys_target_socket"),
      dma_done_intr_i("dma_done_intr_i"),
      dma_chunk_done_intr_i("dma_chunk_done_intr_i"),
      dma_error_intr_i("dma_error_intr_i"),
      alert_fatal_fault_i("alert_fatal_fault_i"),
      clk_o("clk_o"),
      rst_no("rst_no"),
      logger(),
      m_inject_ot_read_bus_error_once(false),
      m_inject_ot_write_bus_error_once(false) {

  // Initialize RegLogger
  logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  REG_INFO(1, logger) << "DMA test harness instantiated" << std::endl;

  // Register TLM callbacks for memory target sockets
  ot_target_socket.register_b_transport(this, &secure_dma_test::ot_b_transport);
  ctn_target_socket.register_b_transport(this, &secure_dma_test::ctn_b_transport);
  sys_target_socket.register_b_transport(this, &secure_dma_test::sys_b_transport);

  // Allocate simulated memory regions
  m_ot_memory_r.resize(1024 * 1024, 0); // 1MB OT source/read memory
  m_ot_memory_w.resize(1024 * 1024, 0); // 1MB OT destination/write memory
  m_ctn_memory_r.resize(1024 * 1024, 0); // 1MB CTN source/read memory
  m_ctn_memory_w.resize(1024 * 1024, 0); // 1MB CTN destination/write memory
  m_sys_memory_r.resize(4 * 1024 * 1024, 0); // 4MB SYS source/read memory
  m_sys_memory_w.resize(4 * 1024 * 1024, 0); // 4MB SYS destination/write memory

  REG_INFO(1, logger)
      << "Memory regions allocated (OT_R: 1MB, OT_W: 1MB, CTN_R: 1MB, "
         "CTN_W: 1MB, SYS_R: 4MB, SYS_W: 4MB)"
                       << std::endl;
}

// ============================================================================
// Destructor
// ============================================================================

secure_dma_test::~secure_dma_test() {
  REG_INFO(1, logger) << "DMA test harness destroyed" << std::endl;
}

// ============================================================================
// Register Access Helper Functions
// ============================================================================

void secure_dma_test::register_read_32(unsigned int offset, uint32_t &read_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_READ_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&read_value));
  trans.set_data_length(4);
  trans.set_streaming_width(4);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register read failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

void secure_dma_test::register_write_32(unsigned int offset, uint32_t write_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_WRITE_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&write_value));
  trans.set_data_length(4);
  trans.set_streaming_width(4);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register write failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

void secure_dma_test::register_read_16(unsigned int offset, uint16_t &read_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_READ_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&read_value));
  trans.set_data_length(2);
  trans.set_streaming_width(2);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register read failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

void secure_dma_test::register_write_16(unsigned int offset, uint16_t write_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_WRITE_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&write_value));
  trans.set_data_length(2);
  trans.set_streaming_width(2);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register write failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

void secure_dma_test::register_read_8(unsigned int offset, uint8_t &read_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_READ_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(&read_value);
  trans.set_data_length(1);
  trans.set_streaming_width(1);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register read failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

void secure_dma_test::register_write_8(unsigned int offset, uint8_t write_value) {
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  trans.set_command(tlm::TLM_WRITE_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(&write_value);
  trans.set_data_length(1);
  trans.set_streaming_width(1);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  initiator_socket->b_transport(trans, delay);

  if (trans.is_response_error()) {
    REG_ERROR(0, logger) << "Register write failed at offset 0x" << std::hex
                          << offset << std::dec << std::endl;
  }

  wait(delay);
}

// ============================================================================
// Reset Helper Function
// ============================================================================

void secure_dma_test::apply_reset(sc_time duration) {
  REG_INFO(1, logger) << "Applying reset for " << duration << std::endl;

  // Assert reset (active-low)
  rst_no.write(false);
  wait(duration);

  // De-assert reset
  rst_no.write(true);
  wait(sc_time(10, SC_NS)); // Stabilization time

  REG_INFO(1, logger) << "Reset sequence complete" << std::endl;
}

// ============================================================================
// Hardware Handshake Helper Function
// ============================================================================

void secure_dma_test::set_lsio_trigger(unsigned int trigger_index, bool assert_value) {
  if (trigger_index < 11) {
    lsio_trigger_o[trigger_index].write(assert_value);
    REG_INFO(2, logger) << "LSIO trigger " << trigger_index << " set to "
                         << (assert_value ? "HIGH" : "LOW") << std::endl;
  } else {
    REG_ERROR(0, logger) << "Invalid trigger index " << trigger_index
                          << " (valid range: 0-10)" << std::endl;
  }
}

// ============================================================================
// TLM Memory Target Callbacks
// ============================================================================

void secure_dma_test::ot_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
  tlm::tlm_command cmd = trans.get_command();
  sc_dt::uint64 addr = trans.get_address();
  unsigned char *ptr = trans.get_data_ptr();
  unsigned int len = trans.get_data_length();

  // One-shot fault injection for bus-error path testing.
  if (cmd == tlm::TLM_READ_COMMAND && m_inject_ot_read_bus_error_once) {
    m_inject_ot_read_bus_error_once = false;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    delay += sc_time(10, SC_NS);
    return;
  }
  if (cmd == tlm::TLM_WRITE_COMMAND && m_inject_ot_write_bus_error_once) {
    m_inject_ot_write_bus_error_once = false;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    delay += sc_time(10, SC_NS);
    return;
  }

  // Apply modulo addressing: map any address into available memory space
  // This allows tests to use realistic hardware addresses while fitting in limited memory
  uint64_t offset = addr % m_ot_memory_r.size();

  // Check if access wraps around the memory boundary
  if (offset + len > m_ot_memory_r.size()) {
    REG_ERROR(0, logger) << "OT memory access crosses boundary: addr=0x" << std::hex
                          << addr << " offset=0x" << offset
                          << " len=" << std::dec << len << std::endl;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return;
  }

  auto* ext = trans.get_extension<sep::sep_axi_extension>();
  if (ext == nullptr || ext->source_id != sep::OTHERS_SOURCE_ID)
    ++m_sideband_errors;
  unsigned char* enables = trans.get_byte_enable_ptr();
  unsigned enable_len = trans.get_byte_enable_length();

  if (cmd == tlm::TLM_READ_COMMAND) {
    // DMA source reads come from dedicated OT read memory.
    copy_lanes(ptr, &m_ot_memory_r[offset], len, enables, enable_len, addr);
    REG_INFO(3, logger) << "OT memory read: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  } else if (cmd == tlm::TLM_WRITE_COMMAND) {
    // DMA destination writes go to dedicated OT write memory.
    copy_lanes(&m_ot_memory_w[offset], ptr, len, enables, enable_len, addr);
    REG_INFO(3, logger) << "OT memory write: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  }

  trans.set_response_status(tlm::TLM_OK_RESPONSE);
  delay += sc_time(10, SC_NS); // Abstract memory access delay
}

void secure_dma_test::ctn_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
  tlm::tlm_command cmd = trans.get_command();
  sc_dt::uint64 addr = trans.get_address();
  unsigned char *ptr = trans.get_data_ptr();
  unsigned int len = trans.get_data_length();

  // Apply modulo addressing: map any address into available memory space
  // This allows tests to use realistic hardware addresses while fitting in limited memory
  uint64_t offset = addr % m_ctn_memory_r.size();

  // Check if access wraps around the memory boundary
  if (offset + len > m_ctn_memory_r.size()) {
    REG_ERROR(0, logger) << "CTN memory access crosses boundary: addr=0x" << std::hex
                          << addr << " offset=0x" << offset
                          << " len=" << std::dec << len << std::endl;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return;
  }

  auto* ext = trans.get_extension<sep::sep_axi_extension>();
  if (ext == nullptr || ext->source_id != sep::OTHERS_SOURCE_ID)
    ++m_sideband_errors;
  unsigned char* enables = trans.get_byte_enable_ptr();
  unsigned enable_len = trans.get_byte_enable_length();

  if (cmd == tlm::TLM_READ_COMMAND) {
    // DMA source reads come from dedicated CTN read memory.
    copy_lanes(ptr, &m_ctn_memory_r[offset], len, enables, enable_len, addr);
    REG_INFO(3, logger) << "CTN memory read: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  } else if (cmd == tlm::TLM_WRITE_COMMAND) {
    // DMA destination writes go to dedicated CTN write memory.
    copy_lanes(&m_ctn_memory_w[offset], ptr, len, enables, enable_len, addr);
    REG_INFO(3, logger) << "CTN memory write: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  }

  trans.set_response_status(tlm::TLM_OK_RESPONSE);
  delay += sc_time(10, SC_NS); // Abstract memory access delay
}

void secure_dma_test::sys_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
  tlm::tlm_command cmd = trans.get_command();
  sc_dt::uint64 addr = trans.get_address();
  unsigned char *ptr = trans.get_data_ptr();
  unsigned int len = trans.get_data_length();

  // Apply modulo addressing: map any address into available memory space
  // This allows tests to use realistic hardware addresses while fitting in limited memory
  uint64_t offset = addr % m_sys_memory_r.size();

  // Check if access wraps around the memory boundary
  if (offset + len > m_sys_memory_r.size()) {
    REG_ERROR(0, logger) << "System memory access crosses boundary: addr=0x" << std::hex
                          << addr << " offset=0x" << offset
                          << " len=" << std::dec << len << std::endl;
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return;
  }

  auto* ext = trans.get_extension<sep::sep_axi_extension>();
  if (ext == nullptr || ext->source_id != sep::OTHERS_SOURCE_ID)
    ++m_sideband_errors;
  unsigned char* enables = trans.get_byte_enable_ptr();
  unsigned enable_len = trans.get_byte_enable_length();

  if (cmd == tlm::TLM_READ_COMMAND) {
    // DMA source reads come from dedicated SYS read memory.
    copy_lanes(ptr, &m_sys_memory_r[offset], len, enables, enable_len, addr);
    REG_INFO(3, logger) << "SYS memory read: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  } else if (cmd == tlm::TLM_WRITE_COMMAND) {
    // DMA destination writes go to dedicated SYS write memory.
    copy_lanes(&m_sys_memory_w[offset], ptr, len, enables, enable_len, addr);
    REG_INFO(3, logger) << "SYS memory write: addr=0x" << std::hex << addr
                         << " len=" << std::dec << len << std::endl;
  }

  trans.set_response_status(tlm::TLM_OK_RESPONSE);
  delay += sc_time(10, SC_NS); // Abstract memory access delay
}

// ============================================================================
// Memory Initialization Helpers for Test Data Setup
// ============================================================================

void secure_dma_test::write_ot_memory_r_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ot_memory_r.size();

  m_ot_memory_r[offset] = data;

  REG_INFO(3, logger) << "OT_R memory write (test init): addr=0x" << std::hex << addr
                       << " offset=0x" << offset
                       << " data=0x" << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

void secure_dma_test::write_ot_memory_w_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ot_memory_w.size();

  m_ot_memory_w[offset] = data;

  REG_INFO(3, logger) << "OT_W memory write (test init): addr=0x" << std::hex << addr
                       << " offset=0x" << offset
                       << " data=0x" << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

void secure_dma_test::write_ot_memory_byte(uint64_t addr, uint8_t data) {
  // Backward-compatible alias: preload source/read memory.
  write_ot_memory_r_byte(addr, data);
}

void secure_dma_test::write_ot_memory_block(uint64_t addr, const unsigned char* data, size_t length) {
  REG_INFO(2, logger) << "OT memory block write (test init): addr=0x" << std::hex << addr
                       << " length=" << std::dec << length << " bytes" << std::endl;

  for (size_t i = 0; i < length; i++) {
    write_ot_memory_byte(addr + i, data[i]);
  }

  REG_INFO(2, logger) << "OT memory block write complete" << std::endl;
}

uint8_t secure_dma_test::read_ot_memory_r_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ot_memory_r.size();

  uint8_t data = m_ot_memory_r[offset];

  REG_INFO(3, logger) << "OT_R memory read (test verify): addr=0x" << std::hex << addr
                       << " offset=0x" << offset
                       << " data=0x" << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

uint8_t secure_dma_test::read_ot_memory_w_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ot_memory_w.size();

  uint8_t data = m_ot_memory_w[offset];

  REG_INFO(3, logger) << "OT_W memory read (test verify): addr=0x" << std::hex << addr
                       << " offset=0x" << offset
                       << " data=0x" << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

void secure_dma_test::write_ctn_memory_r_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ctn_memory_r.size();

  m_ctn_memory_r[offset] = data;

  REG_INFO(3, logger) << "CTN_R memory write (test init): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

void secure_dma_test::write_ctn_memory_w_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ctn_memory_w.size();

  m_ctn_memory_w[offset] = data;

  REG_INFO(3, logger) << "CTN_W memory write (test init): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

uint8_t secure_dma_test::read_ctn_memory_r_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ctn_memory_r.size();

  uint8_t data = m_ctn_memory_r[offset];

  REG_INFO(3, logger) << "CTN_R memory read (test verify): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

uint8_t secure_dma_test::read_ctn_memory_w_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_ctn_memory_w.size();

  uint8_t data = m_ctn_memory_w[offset];

  REG_INFO(3, logger) << "CTN_W memory read (test verify): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

void secure_dma_test::write_sys_memory_r_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_sys_memory_r.size();

  m_sys_memory_r[offset] = data;

  REG_INFO(3, logger) << "SYS_R memory write (test init): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

void secure_dma_test::write_sys_memory_w_byte(uint64_t addr, uint8_t data) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_sys_memory_w.size();

  m_sys_memory_w[offset] = data;

  REG_INFO(3, logger) << "SYS_W memory write (test init): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;
}

uint8_t secure_dma_test::read_sys_memory_r_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_sys_memory_r.size();

  uint8_t data = m_sys_memory_r[offset];

  REG_INFO(3, logger) << "SYS_R memory read (test verify): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

uint8_t secure_dma_test::read_sys_memory_w_byte(uint64_t addr) {
  // Apply modulo addressing to wrap within allocated memory bounds
  uint64_t offset = addr % m_sys_memory_w.size();

  uint8_t data = m_sys_memory_w[offset];

  REG_INFO(3, logger) << "SYS_W memory read (test verify): addr=0x" << std::hex
                       << addr << " offset=0x" << offset << " data=0x"
                       << std::setw(2) << std::setfill('0') << (int)data
                       << std::dec << std::endl;

  return data;
}

void secure_dma_test::write_sys_memory_r_qword(uint64_t addr, uint64_t data) {
  // Little-endian placement to match byte-addressable memory model.
  for (uint32_t i = 0; i < 8; ++i) {
    uint8_t byte = static_cast<uint8_t>((data >> (8 * i)) & 0xFFu);
    write_sys_memory_r_byte(addr + i, byte);
  }
}

void secure_dma_test::write_sys_memory_w_qword(uint64_t addr, uint64_t data) {
  // Little-endian placement to match byte-addressable memory model.
  for (uint32_t i = 0; i < 8; ++i) {
    uint8_t byte = static_cast<uint8_t>((data >> (8 * i)) & 0xFFu);
    write_sys_memory_w_byte(addr + i, byte);
  }
}

uint64_t secure_dma_test::read_sys_memory_r_qword(uint64_t addr) {
  uint64_t data = 0;
  // Little-endian reconstruction from byte-addressable memory model.
  for (uint32_t i = 0; i < 8; ++i) {
    uint64_t byte = static_cast<uint64_t>(read_sys_memory_r_byte(addr + i));
    data |= (byte << (8 * i));
  }
  return data;
}

uint64_t secure_dma_test::read_sys_memory_w_qword(uint64_t addr) {
  uint64_t data = 0;
  // Little-endian reconstruction from byte-addressable memory model.
  for (uint32_t i = 0; i < 8; ++i) {
    uint64_t byte = static_cast<uint64_t>(read_sys_memory_w_byte(addr + i));
    data |= (byte << (8 * i));
  }
  return data;
}

void secure_dma_test::inject_ot_read_bus_error_once() {
  m_inject_ot_read_bus_error_once = true;
}

void secure_dma_test::inject_ot_write_bus_error_once() {
  m_inject_ot_write_bus_error_once = true;
}

uint8_t secure_dma_test::read_ot_memory_byte(uint64_t addr) {
  // Backward-compatible alias: read destination/write memory.
  return read_ot_memory_w_byte(addr);
}
