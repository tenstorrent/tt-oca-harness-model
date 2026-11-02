// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file secure_dma.cpp
 * @brief Secure DMA controller model, following secure_dma.sv
 */

#include "secure_dma.h"
#include "sep_axi_extension.h"
#include <algorithm>
#include <openssl/evp.h>

namespace {
// prim_mubi_pkg MuBi4 encodings
constexpr uint32_t MUBI4_TRUE = 0x6;
constexpr uint32_t MUBI4_FALSE = 0x9;

constexpr uint32_t CTRL_OPCODE_MASK = 0xFu;
constexpr uint32_t CTRL_HANDSHAKE = 1u << 4;
constexpr uint32_t CTRL_DIGEST_SWAP = 1u << 5;
constexpr uint32_t CTRL_INITIAL_TRANSFER = 1u << 8;
constexpr uint32_t CTRL_ABORT = 1u << 27;
constexpr uint32_t CTRL_GO = 1u << 31;

constexpr uint32_t STATUS_BUSY = 1u << 0;
constexpr uint32_t STATUS_DONE = 1u << 1;
constexpr uint32_t STATUS_ABORTED = 1u << 2;
constexpr uint32_t STATUS_ERROR = 1u << 3;
constexpr uint32_t STATUS_DIGEST_VALID = 1u << 4;
constexpr uint32_t STATUS_CHUNK_DONE = 1u << 5;
constexpr uint32_t STATUS_RW1C = STATUS_DONE | STATUS_ABORTED | STATUS_ERROR | STATUS_CHUNK_DONE;

constexpr uint32_t ERR_SRC_ADDR = 1u << 0;
constexpr uint32_t ERR_DST_ADDR = 1u << 1;
constexpr uint32_t ERR_OPCODE = 1u << 2;
constexpr uint32_t ERR_SIZE = 1u << 3;
constexpr uint32_t ERR_BUS = 1u << 4;
constexpr uint32_t ERR_BASE_LIMIT = 1u << 5;
constexpr uint32_t ERR_RANGE_VALID = 1u << 6;
constexpr uint32_t ERR_ASID = 1u << 7;

constexpr uint32_t OPC_COPY = 0x0;
constexpr uint32_t OPC_SHA256 = 0x1;
constexpr uint32_t OPC_SHA384 = 0x2;
constexpr uint32_t OPC_SHA512 = 0x3;
constexpr uint32_t OPC_ZERO = 0x4;

constexpr uint32_t ASID_OT = 0x7;
constexpr uint32_t ASID_SYS = 0x9;
constexpr uint32_t ASID_CTN = 0xA;

constexpr uint32_t WIDTH_2B = 0x1;
constexpr uint32_t WIDTH_4B = 0x2;

constexpr uint32_t ADDR_CFG_INCREMENT = 1u << 0;
constexpr uint32_t ADDR_CFG_WRAP = 1u << 1;

constexpr unsigned NUM_TRIGGERS = 11;
constexpr uint32_t TRIGGER_MASK = (1u << NUM_TRIGGERS) - 1;

bool is_hash_opcode(uint32_t opcode) {
  return opcode == OPC_SHA256 || opcode == OPC_SHA384 || opcode == OPC_SHA512;
}

bool is_valid_asid(uint32_t asid) {
  return asid == ASID_OT || asid == ASID_SYS || asid == ASID_CTN;
}

bool is_soc_asid(uint32_t asid) { return asid == ASID_SYS || asid == ASID_CTN; }

/// OT and CTN address spaces are 32 bits wide
bool is_32bit_asid(uint32_t asid) { return asid == ASID_OT || asid == ASID_CTN; }

uint32_t byte_swap_32(uint32_t word) {
  return ((word & 0x000000FFu) << 24) | ((word & 0x0000FF00u) << 8) |
         ((word & 0x00FF0000u) >> 8) | ((word & 0xFF000000u) >> 24);
}
} // namespace

secure_dma_model::secure_dma_model(sc_module_name n)
    : secure_dma_base(n, 0x150),
      ot_initiator_socket("ot_initiator_socket"),
      ctn_initiator_socket("ctn_initiator_socket"),
      sys_initiator_socket("sys_initiator_socket"),
      dma_done_intr("dma_done_intr"),
      dma_chunk_done_intr("dma_chunk_done_intr"),
      dma_error_intr("dma_error_intr"), alert_fatal_fault("alert_fatal_fault"),
      clk_i("clk_i"), rst_ni("rst_ni"), verbosity("verbosity", REG_DEFAULT_VERBOSITY),
      logger(), m_dma_busy(false), m_error_code(0), m_intr_test_q(0),
      m_abort_req(false), m_state(engine_state::IDLE), m_reset_count(0), m_transfer_byte(0), m_chunk_byte(0),
      m_transfer_width_q(0), m_src_addr_q(0), m_dst_addr_q(0), m_src_be_q(0),
      m_dst_be_q(0), m_read_data_q(0), m_hash_ctx(nullptr) {
  logger.setMaxVerbosity(verbosity.get_param_value());
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  register_all_callbacks();

  SC_THREAD(reset_thread);
  sensitive << rst_ni;

  SC_THREAD(engine_thread);

  SC_METHOD(interrupt_driver_method);
  sensitive << m_interrupt_update_event;
  dont_initialize();
}

secure_dma_model::~secure_dma_model() { hash_free(); }

// ============================================================================
// Reset and interrupts
// ============================================================================

void secure_dma_model::reset_thread() {
  while (true) {
    wait();
    if (rst_ni.read())
      continue;

    REG_INFO(1, logger) << "Reset asserted" << std::endl;
    reset_all_registers();
    m_dma_busy = false;
    m_error_code = 0;
    m_intr_test_q = 0;
    m_abort_req = false;
    m_state = engine_state::IDLE;
    m_control_q = control_state{};
    m_transfer_byte = 0;
    m_chunk_byte = 0;
    m_transfer_width_q = 0;
    m_src_addr_q = 0;
    m_dst_addr_q = 0;
    m_src_be_q = 0;
    m_dst_be_q = 0;
    m_read_data_q = 0;
    hash_free();
    ++m_reset_count;
    m_wake_event.notify(SC_ZERO_TIME);
    m_interrupt_update_event.notify(SC_ZERO_TIME);
    alert_fatal_fault.write(false);
  }
}

uint32_t secure_dma_model::intr_state() const {
  const uint32_t status = static_cast<uint32_t>(STATUS);
  const uint32_t events = ((status & STATUS_DONE) ? 0x1u : 0u) |
                          ((status & STATUS_CHUNK_DONE) ? 0x2u : 0u) |
                          ((status & STATUS_ERROR) ? 0x4u : 0u);
  return events | m_intr_test_q;
}

void secure_dma_model::update_interrupts() { m_interrupt_update_event.notify(SC_ZERO_TIME); }

void secure_dma_model::interrupt_driver_method() {
  const uint32_t pending = intr_state() & static_cast<uint32_t>(INTR_ENABLE);
  dma_done_intr.write((pending & 0x1u) != 0);
  dma_chunk_done_intr.write((pending & 0x2u) != 0);
  dma_error_intr.write((pending & 0x4u) != 0);
  interrupt_updated_event.notify(2, SC_NS);
}

void secure_dma_model::update_status(uint32_t set, uint32_t clear) {
  STATUS = (static_cast<uint32_t>(STATUS) | set) & ~clear;
  update_interrupts();
}

// ============================================================================
// Register callbacks
// ============================================================================

// Outside a data move, a CONTROL or STATUS write takes effect at once, as the
// next clock edge does in hardware, so a read that follows the write sees it.
bool secure_dma_model::handle_write_CONTROL(uint32_t value, uint32_t write_mask) {
  CONTROL = value & write_mask;
  if (value & CTRL_ABORT) {
    if (m_state == engine_state::ACTIVE)
      m_abort_req = true;
    else
      take_abort(false);
  } else {
    try_leave_idle();
  }
  m_wake_event.notify(SC_ZERO_TIME);
  return false;
}

bool secure_dma_model::handle_write_STATUS(uint32_t value, uint32_t write_mask) {
  const uint32_t status = static_cast<uint32_t>(STATUS);
  // Any STATUS write while the error flag is set clears ERROR_CODE, whether or
  // not it clears the flag.
  if (status & STATUS_ERROR) {
    m_error_code = 0;
    ERROR_CODE = 0;
  }
  update_status(0, value & STATUS_RW1C);
  if (m_state == engine_state::ERROR && !(static_cast<uint32_t>(STATUS) & STATUS_ERROR)) {
    CONTROL = static_cast<uint32_t>(CONTROL) & ~CTRL_GO;
    m_dma_busy = false;
    m_state = engine_state::IDLE;
  }
  m_wake_event.notify(SC_ZERO_TIME);
  return false;
}

bool secure_dma_model::handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask) {
  INTR_ENABLE = value & write_mask;
  update_interrupts();
  return false;
}

bool secure_dma_model::handle_write_INTR_TEST(uint32_t value, uint32_t write_mask) {
  m_intr_test_q = value & write_mask;
  update_interrupts();
  return false;
}

bool secure_dma_model::handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask) {
  if (value & 0x1) {
    alert_fatal_fault.write(true);
    wait(SC_ZERO_TIME);
    alert_fatal_fault.write(false);
  }
  return false;
}

bool secure_dma_model::handle_write_RANGE_REGWEN(uint32_t value, uint32_t write_mask) {
  // prim_subreg_arb MuBi4 W0C stores mubi4_and_hi(q, wd): AND on the bits that
  // are set in True, OR on the others, so any value but True locks.
  const uint32_t q = static_cast<uint32_t>(RANGE_REGWEN) & 0xF;
  const uint32_t wd = value & 0xF;
  RANGE_REGWEN = ((q & wd) & MUBI4_TRUE) | ((q | wd) & ~MUBI4_TRUE & 0xF);
  return false;
}

bool secure_dma_model::handle_write_HANDSHAKE_INTR_ENABLE(uint32_t value, uint32_t write_mask) {
  if (!m_dma_busy) {
    HANDSHAKE_INTR_ENABLE.handle_write(value, write_mask);
    m_wake_event.notify(SC_ZERO_TIME);
  }
  return false;
}

bool secure_dma_model::handle_read_CFG_REGWEN(uint32_t &value, uint32_t read_mask) {
  value = m_dma_busy ? MUBI4_FALSE : MUBI4_TRUE;
  return true;
}

bool secure_dma_model::handle_read_STATUS(uint32_t &value, uint32_t read_mask) {
  value = (static_cast<uint32_t>(STATUS) & ~STATUS_BUSY) | (m_dma_busy ? STATUS_BUSY : 0u);
  return true;
}

bool secure_dma_model::handle_read_ERROR_CODE(uint32_t &value, uint32_t read_mask) {
  value = m_error_code;
  return true;
}

bool secure_dma_model::handle_read_INTR_STATE(uint32_t &value, uint32_t read_mask) {
  value = intr_state();
  return true;
}

void secure_dma_model::register_all_callbacks() {
  auto on_write = [this](regmodel::Reg<32> &reg, bool (secure_dma_model::*fn)(uint32_t, uint32_t)) {
    memory.register_write_callback(
        [this, fn, &reg](uint32_t value) { return (this->*fn)(value, reg.write_bit_mask); },
        reg.offset);
  };
  auto on_read = [this](regmodel::Reg<32> &reg, bool (secure_dma_model::*fn)(uint32_t &, uint32_t)) {
    memory.register_read_callback(
        [this, fn, &reg](uint32_t &value) { return (this->*fn)(value, reg.read_bit_mask); },
        reg.offset);
  };
  // Writes that CFG_REGWEN (busy) or RANGE_REGWEN can block.
  auto gated = [this](regmodel::Reg<32> &reg, const std::function<bool()> &locked) {
    memory.register_write_callback(
        [&reg, locked](uint32_t value) {
          if (!locked())
            reg.handle_write(value, reg.write_bit_mask);
          return false;
        },
        reg.offset);
  };
  const auto cfg_locked = [this] { return m_dma_busy; };
  const auto range_locked = [this] {
    return (static_cast<uint32_t>(RANGE_REGWEN) & 0xF) != MUBI4_TRUE;
  };

  on_write(CONTROL, &secure_dma_model::handle_write_CONTROL);
  on_write(STATUS, &secure_dma_model::handle_write_STATUS);
  on_write(INTR_ENABLE, &secure_dma_model::handle_write_INTR_ENABLE);
  on_write(INTR_TEST, &secure_dma_model::handle_write_INTR_TEST);
  on_write(ALERT_TEST, &secure_dma_model::handle_write_ALERT_TEST);
  on_write(RANGE_REGWEN, &secure_dma_model::handle_write_RANGE_REGWEN);
  on_write(HANDSHAKE_INTR_ENABLE, &secure_dma_model::handle_write_HANDSHAKE_INTR_ENABLE);

  for (regmodel::Reg<32> *reg :
       {static_cast<regmodel::Reg<32> *>(&ENABLED_MEMORY_RANGE_BASE),
        static_cast<regmodel::Reg<32> *>(&ENABLED_MEMORY_RANGE_LIMIT),
        static_cast<regmodel::Reg<32> *>(&RANGE_VALID)})
    gated(*reg, range_locked);

  for (regmodel::Reg<32> *reg :
       {static_cast<regmodel::Reg<32> *>(&SRC_ADDR_LO), static_cast<regmodel::Reg<32> *>(&SRC_ADDR_HI),
        static_cast<regmodel::Reg<32> *>(&DST_ADDR_LO), static_cast<regmodel::Reg<32> *>(&DST_ADDR_HI),
        static_cast<regmodel::Reg<32> *>(&ADDR_SPACE_ID),
        static_cast<regmodel::Reg<32> *>(&TOTAL_DATA_SIZE),
        static_cast<regmodel::Reg<32> *>(&CHUNK_DATA_SIZE),
        static_cast<regmodel::Reg<32> *>(&TRANSFER_WIDTH),
        static_cast<regmodel::Reg<32> *>(&SRC_CONFIG), static_cast<regmodel::Reg<32> *>(&DST_CONFIG),
        static_cast<regmodel::Reg<32> *>(&CLEAR_INTR_SRC),
        static_cast<regmodel::Reg<32> *>(&CLEAR_INTR_BUS)})
    gated(*reg, cfg_locked);
  for (unsigned i = 0; i < NUM_TRIGGERS; ++i) {
    gated(INTR_SRC_ADDR[i], cfg_locked);
    gated(INTR_SRC_WR_VAL[i], cfg_locked);
  }

  on_read(CFG_REGWEN, &secure_dma_model::handle_read_CFG_REGWEN);
  on_read(STATUS, &secure_dma_model::handle_read_STATUS);
  on_read(ERROR_CODE, &secure_dma_model::handle_read_ERROR_CODE);
  on_read(INTR_STATE, &secure_dma_model::handle_read_INTR_STATE);
}

// ============================================================================
// Control state machine
// ============================================================================

bool secure_dma_model::trigger_asserted() {
  const uint32_t enabled = static_cast<uint32_t>(HANDSHAKE_INTR_ENABLE) & TRIGGER_MASK;
  for (unsigned i = 0; i < NUM_TRIGGERS; ++i)
    if ((enabled & (1u << i)) && lsio_trigger[i].read())
      return true;
  return false;
}

void secure_dma_model::capture_state(uint32_t control) {
  m_control_q.handshake_en = (control & CTRL_HANDSHAKE) != 0;
  m_control_q.range_valid = (static_cast<uint32_t>(RANGE_VALID) & 0x1) != 0;
  m_control_q.range_base = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_BASE);
  m_control_q.range_limit = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_LIMIT);
  if (control & CTRL_INITIAL_TRANSFER) {
    m_transfer_byte = 0;
    m_control_q.opcode = control & CTRL_OPCODE_MASK;
    m_control_q.digest_swap = (control & CTRL_DIGEST_SWAP) != 0;
  }
}

void secure_dma_model::leave_idle(uint32_t control) {
  uint32_t clear = 0;
  if (!m_dma_busy) {
    clear |= STATUS_DONE | STATUS_ERROR | STATUS_ABORTED | STATUS_CHUNK_DONE;
    m_error_code = 0;
    ERROR_CODE = 0;
  }
  if (control & CTRL_INITIAL_TRANSFER) {
    clear |= STATUS_DIGEST_VALID;
    for (unsigned i = 0; i < 16; ++i)
      SHA2_DIGEST[i] = 0;
    CONTROL = static_cast<uint32_t>(CONTROL) & ~CTRL_INITIAL_TRANSFER;
  }
  m_dma_busy = true;
  update_status(0, clear);
}

bool secure_dma_model::try_leave_idle() {
  if (m_state != engine_state::IDLE)
    return false;
  const uint32_t control = static_cast<uint32_t>(CONTROL);
  if (!(control & CTRL_GO) && !m_dma_busy)
    return false;
  if (!m_dma_busy)
    capture_state(control);
  if (m_control_q.handshake_en && !trigger_asserted())
    return false;
  m_chunk_byte = 0;
  leave_idle(control);
  m_state = engine_state::ACTIVE;
  return true;
}

void secure_dma_model::engine_thread() {
  sc_event_or_list wake;
  wake |= m_wake_event;
  for (unsigned i = 0; i < NUM_TRIGGERS; ++i)
    wake |= lsio_trigger[i].value_changed_event();

  while (true) {
    if (m_state != engine_state::ACTIVE && !try_leave_idle()) {
      wait(wake);
      continue;
    }
    if (m_control_q.handshake_en && (static_cast<uint32_t>(CLEAR_INTR_SRC) & TRIGGER_MASK) &&
        !clear_interrupt_sources())
      continue;
    run_chunk();
    // The return to idle takes a delta, which lets a trigger the chunk serviced
    // fall before the idle state samples it.
    wait(SC_ZERO_TIME);
  }
}

bool secure_dma_model::clear_interrupt_sources() {
  const uint64_t resets = m_reset_count;
  const uint32_t sources = static_cast<uint32_t>(CLEAR_INTR_SRC) & TRIGGER_MASK;
  const uint32_t host_bus = static_cast<uint32_t>(CLEAR_INTR_BUS);
  for (unsigned i = 0; i < NUM_TRIGGERS; ++i) {
    if (!(sources & (1u << i)))
      continue;
    if (m_abort_req) {
      take_abort(false);
      return false;
    }
    uint32_t data = static_cast<uint32_t>(INTR_SRC_WR_VAL[i]);
    const auto rsp =
        transport((host_bus & (1u << i)) ? BusInterface::OT_INTERNAL : BusInterface::CTN_BUS,
                  tlm::TLM_WRITE_COMMAND, static_cast<uint32_t>(INTR_SRC_ADDR[i]), data, 0xF);
    if (resets != m_reset_count)
      return false;
    if (m_abort_req) {
      take_abort(false);
      return false;
    }
    if (rsp != tlm::TLM_OK_RESPONSE) {
      REG_INFO(1, logger) << "Interrupt clearing write " << i << " failed" << std::endl;
      enter_error(ERR_BUS);
      return false;
    }
  }
  return true;
}

uint32_t secure_dma_model::setup_beat() {
  uint32_t errors = 0;
  const uint32_t width_enc = static_cast<uint32_t>(TRANSFER_WIDTH) & 0x3;
  uint32_t width = 0;
  switch (width_enc) {
  case 0x0: width = 1; break;
  case WIDTH_2B: width = 2; break;
  case WIDTH_4B: width = 4; break;
  default: errors |= ERR_SIZE; break;
  }
  m_transfer_width_q = width;

  const uint32_t src_lo = static_cast<uint32_t>(SRC_ADDR_LO);
  const uint32_t src_hi = static_cast<uint32_t>(SRC_ADDR_HI);
  const uint32_t dst_lo = static_cast<uint32_t>(DST_ADDR_LO);
  const uint32_t dst_hi = static_cast<uint32_t>(DST_ADDR_HI);
  const uint32_t total = static_cast<uint32_t>(TOTAL_DATA_SIZE);
  const uint32_t chunk = static_cast<uint32_t>(CHUNK_DATA_SIZE);

  // The register address starts a transfer, every beat in fixed mode, and every
  // chunk in wrap mode; otherwise the beat follows the previous one, across
  // chunks and across starts.
  auto next_address = [this, width](uint32_t config, uint64_t reg, uint64_t prev) {
    if (m_transfer_byte == 0 || !(config & ADDR_CFG_INCREMENT) ||
        (m_chunk_byte == 0 && (config & ADDR_CFG_WRAP)))
      return reg;
    return prev + width;
  };
  m_src_addr_q = next_address(static_cast<uint32_t>(SRC_CONFIG),
                              (static_cast<uint64_t>(src_hi) << 32) | src_lo, m_src_addr_q);
  m_dst_addr_q = next_address(static_cast<uint32_t>(DST_CONFIG),
                              (static_cast<uint64_t>(dst_hi) << 32) | dst_lo, m_dst_addr_q);

  const uint32_t remaining = std::min(total - m_transfer_byte, chunk - m_chunk_byte);
  const unsigned src_lane = static_cast<unsigned>(m_src_addr_q & 0x3);
  const unsigned dst_lane = static_cast<unsigned>(m_dst_addr_q & 0x3);
  switch (width) {
  case 1:
    m_src_be_q = static_cast<uint8_t>((0x1u << src_lane) & 0xF);
    m_dst_be_q = static_cast<uint8_t>((0x1u << dst_lane) & 0xF);
    break;
  case 2: {
    const uint32_t lanes = remaining >= 2 ? 0x3u : 0x1u;
    m_src_be_q = static_cast<uint8_t>((lanes << src_lane) & 0xF);
    m_dst_be_q = static_cast<uint8_t>((lanes << dst_lane) & 0xF);
    break;
  }
  case 4:
    m_dst_be_q = remaining == 1 ? 0x1 : remaining == 2 ? 0x3 : remaining == 3 ? 0x7 : 0xF;
    m_src_be_q = m_dst_be_q;
    break;
  default:
    m_src_be_q = 0;
    m_dst_be_q = 0;
    break;
  }

  const uint32_t opcode = m_control_q.opcode;
  const bool zero_fill = opcode == OPC_ZERO;
  const uint32_t asid = static_cast<uint32_t>(ADDR_SPACE_ID);
  const uint32_t src_asid = asid & 0xF;
  const uint32_t dst_asid = (asid >> 4) & 0xF;
  const uint64_t base = m_control_q.range_base;
  const uint64_t limit = m_control_q.range_limit;

  if (chunk == 0 || total == 0)
    errors |= ERR_SIZE;
  if (opcode > OPC_ZERO)
    errors |= ERR_OPCODE;
  if (zero_fill && m_control_q.handshake_en)
    errors |= ERR_OPCODE;
  if (is_hash_opcode(opcode) && width_enc != WIDTH_4B)
    errors |= ERR_SIZE;
  if (!is_valid_asid(src_asid) || !is_valid_asid(dst_asid))
    errors |= ERR_ASID;
  if (limit < base)
    errors |= ERR_BASE_LIMIT;
  if ((width_enc == WIDTH_4B && (src_lo & 0x3)) || (width_enc == WIDTH_2B && (src_lo & 0x1)))
    errors |= ERR_SRC_ADDR;
  if ((width_enc == WIDTH_4B && (dst_lo & 0x3)) || (width_enc == WIDTH_2B && (dst_lo & 0x1)))
    errors |= ERR_DST_ADDR;
  // A transfer between SoC and OT memory must stay inside the enabled range on
  // the OT side, for a whole chunk from the register address.
  if (is_soc_asid(src_asid) && dst_asid == ASID_OT &&
      (dst_lo > limit || dst_lo < base || static_cast<uint64_t>(dst_lo) + chunk > limit))
    errors |= ERR_DST_ADDR;
  if (is_soc_asid(dst_asid) && src_asid == ASID_OT &&
      (src_lo > limit || src_lo < base || static_cast<uint64_t>(src_lo) + chunk > limit))
    errors |= ERR_SRC_ADDR;
  if (zero_fill) {
    // No byte past TOTAL_DATA_SIZE, and every beat inside the enabled range.
    if (m_transfer_byte >= total)
      errors |= ERR_SIZE;
    const uint64_t beat_end = m_dst_addr_q - 1 + std::min(remaining, width);
    if (m_dst_addr_q < base || beat_end > limit)
      errors |= ERR_DST_ADDR;
  }
  if (is_32bit_asid(src_asid) && src_hi != 0)
    errors |= ERR_SRC_ADDR;
  if (is_32bit_asid(dst_asid) && dst_hi != 0)
    errors |= ERR_DST_ADDR;
  if (!m_control_q.range_valid)
    errors |= ERR_RANGE_VALID;
  return errors;
}

void secure_dma_model::run_chunk() {
  const uint64_t resets = m_reset_count;
  const uint32_t opcode = m_control_q.opcode;
  const bool zero_fill = opcode == OPC_ZERO;
  const bool hashing = is_hash_opcode(opcode);
  const uint32_t asid = static_cast<uint32_t>(ADDR_SPACE_ID);
  const BusInterface src_bus = bus_for_asid(asid & 0xF);
  const BusInterface dst_bus = bus_for_asid((asid >> 4) & 0xF);

  while (true) {
    wait(SC_ZERO_TIME);
    if (resets != m_reset_count)
      return;
    if (m_abort_req) {
      take_abort(false);
      return;
    }

    const uint32_t errors = setup_beat();
    if (errors) {
      REG_INFO(1, logger) << "Beat setup error 0x" << std::hex << errors << std::dec << std::endl;
      enter_error(errors);
      return;
    }
    if (hashing && m_transfer_byte == 0)
      hash_start();

    if (zero_fill) {
      m_read_data_q = 0;
    } else {
      uint32_t word = 0;
      const auto rsp = transport(src_bus, tlm::TLM_READ_COMMAND, m_src_addr_q & ~uint64_t{0x3},
                                 word, m_src_be_q);
      if (resets != m_reset_count)
        return;
      if (m_abort_req) {
        take_abort(false);
        return;
      }
      if (rsp != tlm::TLM_OK_RESPONSE) {
        REG_INFO(1, logger) << "Read error at 0x" << std::hex << m_src_addr_q << std::dec << std::endl;
        enter_error(ERR_BUS);
        return;
      }
      // Steer the beat's bytes to every lane, so the write enables pick them
      // at any destination alignment.
      if (m_transfer_width_q == 1) {
        const unsigned lane = m_src_be_q & 0x8 ? 3 : m_src_be_q & 0x4 ? 2 : m_src_be_q & 0x2 ? 1 : 0;
        m_read_data_q = ((word >> (8 * lane)) & 0xFFu) * 0x01010101u;
      } else if (m_transfer_width_q == 2) {
        const uint32_t half = (m_src_be_q & 0x3) ? (word & 0xFFFFu) : (word >> 16);
        m_read_data_q = half | (half << 16);
      } else {
        m_read_data_q = word;
      }
      if (hashing)
        hash_feed(m_read_data_q, m_dst_be_q);
    }

    uint32_t data = m_read_data_q;
    const auto rsp = transport(dst_bus, tlm::TLM_WRITE_COMMAND, m_dst_addr_q & ~uint64_t{0x3}, data,
                               m_dst_be_q);
    if (resets != m_reset_count)
      return;
    if (m_abort_req) {
      take_abort(true);
      return;
    }
    if (rsp != tlm::TLM_OK_RESPONSE) {
      REG_INFO(1, logger) << "Write error at 0x" << std::hex << m_dst_addr_q << std::dec << std::endl;
      enter_error(ERR_BUS);
      return;
    }

    m_transfer_byte += m_transfer_width_q;
    m_chunk_byte += m_transfer_width_q;

    if (m_transfer_byte >= static_cast<uint32_t>(TOTAL_DATA_SIZE)) {
      if (hashing)
        hash_finish();
      address_writeback();
      CONTROL = static_cast<uint32_t>(CONTROL) & ~CTRL_GO;
      m_dma_busy = false;
      m_state = engine_state::IDLE;
      update_status(STATUS_DONE, 0);
      return;
    }
    if (m_chunk_byte >= static_cast<uint32_t>(CHUNK_DATA_SIZE)) {
      address_writeback();
      m_state = engine_state::IDLE;
      // A handshake transfer stays busy and waits for the next trigger.
      if (!m_control_q.handshake_en) {
        CONTROL = static_cast<uint32_t>(CONTROL) & ~CTRL_GO;
        m_dma_busy = false;
        update_status(STATUS_CHUNK_DONE, 0);
      }
      return;
    }
  }
}

void secure_dma_model::enter_error(uint32_t errors) {
  const uint64_t resets = m_reset_count;
  m_error_code = errors;
  ERROR_CODE = errors;
  m_abort_req = false;
  m_state = engine_state::ERROR;
  update_status(STATUS_ERROR, 0);
  // The STATUS write that clears the error, or an abort, leaves the state.
  while (m_state == engine_state::ERROR) {
    wait(m_wake_event);
    if (resets != m_reset_count)
      return;
  }
}

void secure_dma_model::take_abort(bool data_move) {
  REG_INFO(1, logger) << "Abort" << std::endl;
  m_abort_req = false;
  m_state = engine_state::IDLE;
  CONTROL = static_cast<uint32_t>(CONTROL) & ~(CTRL_GO | CTRL_ABORT);
  if (data_move)
    address_writeback();
  m_dma_busy = false;
  update_status(STATUS_ABORTED, 0);
}

void secure_dma_model::address_writeback() {
  const uint32_t chunk = static_cast<uint32_t>(CHUNK_DATA_SIZE);
  auto advance = [chunk](uint32_t config, regmodel::Reg<32> &lo, regmodel::Reg<32> &hi) {
    if (config & (ADDR_CFG_INCREMENT | ADDR_CFG_WRAP))
      return;
    const uint64_t addr =
        ((static_cast<uint64_t>(static_cast<uint32_t>(hi)) << 32) | static_cast<uint32_t>(lo)) +
        chunk;
    lo = static_cast<uint32_t>(addr);
    hi = static_cast<uint32_t>(addr >> 32);
  };
  advance(static_cast<uint32_t>(SRC_CONFIG), SRC_ADDR_LO, SRC_ADDR_HI);
  advance(static_cast<uint32_t>(DST_CONFIG), DST_ADDR_LO, DST_ADDR_HI);
}

// ============================================================================
// Bus ports
// ============================================================================

secure_dma_model::BusInterface secure_dma_model::bus_for_asid(uint32_t asid) const {
  switch (asid) {
  case ASID_SYS: return BusInterface::SYSTEM_BUS;
  case ASID_CTN: return BusInterface::CTN_BUS;
  default: return BusInterface::OT_INTERNAL;
  }
}

tlm::tlm_response_status secure_dma_model::transport(BusInterface bus, tlm::tlm_command cmd,
                                                     uint64_t addr, uint32_t &data,
                                                     uint8_t byte_enables) {
  unsigned char buf[4];
  for (unsigned i = 0; i < 4; ++i)
    buf[i] = static_cast<unsigned char>(data >> (8 * i));
  unsigned char lanes[4];
  for (unsigned i = 0; i < 4; ++i)
    lanes[i] = (byte_enables & (1u << i)) ? TLM_BYTE_ENABLED : TLM_BYTE_DISABLED;

  // The extension outlives the payload and is detached before either is
  // destroyed: set_extension does not take ownership.
  sep::sep_axi_extension axi_ext;
  axi_ext.source_id = sep::OTHERS_SOURCE_ID;
  tlm::tlm_generic_payload trans;
  struct extension_detach {
    tlm::tlm_generic_payload &payload;
    ~extension_detach() { payload.clear_extension<sep::sep_axi_extension>(); }
  } detach{trans};
  trans.set_extension(&axi_ext);

  trans.set_command(cmd);
  trans.set_address(addr);
  trans.set_data_ptr(buf);
  trans.set_data_length(4);
  trans.set_streaming_width(4);
  if (byte_enables == 0xF) {
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
  } else {
    trans.set_byte_enable_ptr(lanes);
    trans.set_byte_enable_length(4);
  }
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  sc_time delay = SC_ZERO_TIME;
  switch (bus) {
  case BusInterface::OT_INTERNAL: ot_initiator_socket->b_transport(trans, delay); break;
  case BusInterface::CTN_BUS: ctn_initiator_socket->b_transport(trans, delay); break;
  case BusInterface::SYSTEM_BUS: sys_initiator_socket->b_transport(trans, delay); break;
  }
  wait(delay);

  if (cmd == tlm::TLM_READ_COMMAND) {
    data = 0;
    for (unsigned i = 0; i < 4; ++i)
      data |= static_cast<uint32_t>(buf[i]) << (8 * i);
  }
  return trans.get_response_status();
}

// ============================================================================
// Inline SHA-2
// ============================================================================

void secure_dma_model::hash_free() {
  if (m_hash_ctx != nullptr) {
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(m_hash_ctx));
    m_hash_ctx = nullptr;
  }
}

void secure_dma_model::hash_start() {
  hash_free();
  const EVP_MD *md = m_control_q.opcode == OPC_SHA512   ? EVP_sha512()
                     : m_control_q.opcode == OPC_SHA384 ? EVP_sha384()
                                                        : EVP_sha256();
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx == nullptr || EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    SC_REPORT_FATAL("secure_dma", "SHA-2 context initialisation failed");
    return;
  }
  m_hash_ctx = ctx;
}

void secure_dma_model::hash_feed(uint32_t word, uint8_t byte_enables) {
  if (m_hash_ctx == nullptr)
    hash_start();
  unsigned char bytes[4];
  unsigned count = 0;
  for (unsigned i = 0; i < 4; ++i)
    if (byte_enables & (1u << i))
      bytes[count++] = static_cast<unsigned char>(word >> (8 * i));
  EVP_DigestUpdate(static_cast<EVP_MD_CTX *>(m_hash_ctx), bytes, count);
}

void secure_dma_model::hash_finish() {
  if (m_hash_ctx == nullptr)
    hash_start();
  unsigned char digest[EVP_MAX_MD_SIZE] = {};
  unsigned int length = 0;
  EVP_DigestFinal_ex(static_cast<EVP_MD_CTX *>(m_hash_ctx), digest, &length);
  hash_free();

  // Each register holds one big-endian digest word; digest_swap byte-swaps it.
  // Registers past the digest length read zero.
  for (unsigned i = 0; i < 16; ++i) {
    uint32_t word = 0;
    if (4 * i < length)
      word = (static_cast<uint32_t>(digest[4 * i]) << 24) |
             (static_cast<uint32_t>(digest[4 * i + 1]) << 16) |
             (static_cast<uint32_t>(digest[4 * i + 2]) << 8) |
             static_cast<uint32_t>(digest[4 * i + 3]);
    SHA2_DIGEST[i] = m_control_q.digest_swap ? byte_swap_32(word) : word;
  }
  update_status(STATUS_DIGEST_VALID, 0);
}
