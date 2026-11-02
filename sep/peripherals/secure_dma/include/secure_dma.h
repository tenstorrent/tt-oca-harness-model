// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file secure_dma.h
 * @brief Secure DMA controller model
 *
 * The model follows the control state machine of secure_dma.sv: a transfer
 * leaves DmaIdle on CONTROL.go (or, in hardware handshake mode, on an asserted
 * trigger), sets up and moves one bus beat at a time, and returns to DmaIdle at
 * every chunk boundary. A configuration or bus error parks it in DmaError until
 * software clears STATUS.error. CONTROL.abort returns it to DmaIdle from any
 * state.
 */

#pragma once
#include "secure_dma_base.h"
#include "reg_logger.h"
#include "reg_param.h"
#include <tlm_utils/simple_initiator_socket.h>

/**
 * @class secure_dma_model
 * @brief Secure DMA controller SystemC TLM model
 *
 * Every bus beat is a 4-byte, word-aligned transaction with per-lane byte
 * enables, as the TL-UL and system ports issue it. Sub-word reads are steered
 * to every byte lane, so the destination's byte enables pick the data at any
 * alignment.
 */
class secure_dma_model : public secure_dma_base
{
  friend class testbench;

public:
  SC_HAS_PROCESS(secure_dma_model);

  /// OpenTitan internal bus (ASID 0x7)
  tlm_utils::simple_initiator_socket<secure_dma_model, 32> ot_initiator_socket;

  /// SoC control network (ASID 0xA)
  tlm_utils::simple_initiator_socket<secure_dma_model, 32> ctn_initiator_socket;

  /// SoC system bus (ASID 0x9)
  tlm_utils::simple_initiator_socket<secure_dma_model, 64> sys_initiator_socket;

  /// Status-type interrupts: INTR_STATE & INTR_ENABLE
  sc_out<bool> dma_done_intr;
  sc_out<bool> dma_chunk_done_intr;
  sc_out<bool> dma_error_intr;

  /// Notified each time the interrupt outputs are driven
  sc_event interrupt_updated_event;

  /// Fatal alert, pulsed by ALERT_TEST
  sc_out<bool> alert_fatal_fault;

  /// Hardware handshake triggers, level sensitive, masked by HANDSHAKE_INTR_ENABLE
  sc_in<bool> lsio_trigger[11];

  sc_in<sc_time> clk_i;

  /// Active-low reset
  sc_in<bool> rst_ni;

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
  regmodel::Param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
  secure_dma_model(sc_module_name n);
  ~secure_dma_model();

private:
  /// Bus port a beat or clearing write is issued on
  enum class BusInterface { OT_INTERNAL, CTN_BUS, SYSTEM_BUS };

  /// RTL control_q: configuration captured when a transfer leaves DmaIdle
  struct control_state {
    uint32_t opcode = 0;        ///< captured only by a start with initial_transfer
    bool digest_swap = false;   ///< captured only by a start with initial_transfer
    bool handshake_en = false;
    bool range_valid = false;
    uint32_t range_base = 0;
    uint32_t range_limit = 0;
  };

  RegLogger logger;

  /// STATUS.busy
  bool m_dma_busy;

  uint32_t m_error_code;

  /// prim_intr_hw test_q: the last value written to INTR_TEST
  uint32_t m_intr_test_q;

  /// CONTROL.abort written during a data move, not yet handled by the engine
  bool m_abort_req;

  /// DmaIdle, the data-moving states, or DmaError
  enum class engine_state { IDLE, ACTIVE, ERROR };
  engine_state m_state;

  /// Bumped by every reset; the engine drops a transfer that a reset overtook
  uint64_t m_reset_count;

  sc_event m_interrupt_update_event;

  /// CONTROL, STATUS and HANDSHAKE_INTR_ENABLE writes and resets wake the engine
  sc_event m_wake_event;

  control_state m_control_q;

  /// transfer_byte_q: bytes moved since the last start with initial_transfer
  uint32_t m_transfer_byte;

  /// chunk_byte_q: bytes moved in the current chunk
  uint32_t m_chunk_byte;

  /// Bytes per beat, captured at beat setup
  uint32_t m_transfer_width_q;

  /// Beat addresses, kept across chunks and across starts
  uint64_t m_src_addr_q;
  uint64_t m_dst_addr_q;

  /// Beat byte enables
  uint8_t m_src_be_q;
  uint8_t m_dst_be_q;

  /// read_return_data_q: the last word read, steered to every lane
  uint32_t m_read_data_q;

  /// OpenSSL EVP_MD_CTX of the inline SHA-2 engine, or nullptr
  void* m_hash_ctx;

  void reset_thread();
  void engine_thread();
  void interrupt_driver_method();
  void update_interrupts();

  bool handle_write_CONTROL(uint32_t value, uint32_t write_mask);
  bool handle_write_STATUS(uint32_t value, uint32_t write_mask);
  bool handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask);
  bool handle_write_INTR_TEST(uint32_t value, uint32_t write_mask);
  bool handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask);
  bool handle_write_RANGE_REGWEN(uint32_t value, uint32_t write_mask);
  bool handle_write_HANDSHAKE_INTR_ENABLE(uint32_t value, uint32_t write_mask);

  bool handle_read_CFG_REGWEN(uint32_t& value, uint32_t read_mask);
  bool handle_read_STATUS(uint32_t& value, uint32_t read_mask);
  bool handle_read_ERROR_CODE(uint32_t& value, uint32_t read_mask);
  bool handle_read_INTR_STATE(uint32_t& value, uint32_t read_mask);

  void register_all_callbacks();

  /// Sets then clears STATUS bits and refreshes the interrupt outputs
  void update_status(uint32_t set, uint32_t clear);

  /// INTR_STATE: STATUS.{error, chunk_done, done} | test_q
  uint32_t intr_state() const;

  /// An enabled trigger is asserted
  bool trigger_asserted();

  /// DmaIdle with busy clear: capture control_q, and on initial_transfer the operation
  void capture_state(uint32_t control);

  /// The DmaIdle exit: clear_status, clear_sha_status, initial_transfer and busy
  void leave_idle(uint32_t control);

  /// DmaIdle: leave it if go (or busy) is set and, in handshake mode, a trigger
  /// is asserted. True when it left.
  bool try_leave_idle();

  /// DmaClearIntrSrc: one write per CLEAR_INTR_SRC bit, in index order. False
  /// when the transfer stopped.
  bool clear_interrupt_sources();

  /// The beats of one chunk, from DmaAddrSetup to the return to DmaIdle
  void run_chunk();

  /// DmaAddrSetup: the beat addresses and byte enables, and the errors that stop the beat
  uint32_t setup_beat();

  /// DmaError: latch the errors and hold until STATUS.error clears or an abort
  void enter_error(uint32_t errors);

  /// The abort return to DmaIdle
  void take_abort(bool data_move);

  /// A fixed-address side (increment=0, wrap=0) advances its address register by
  /// CHUNK_DATA_SIZE when a data move returns to DmaIdle
  void address_writeback();

  /// Issue one beat or clearing write. @p data is the little-endian bus word.
  tlm::tlm_response_status transport(BusInterface bus, tlm::tlm_command cmd, uint64_t addr,
                                     uint32_t& data, uint8_t byte_enables);

  BusInterface bus_for_asid(uint32_t asid) const;

  void hash_start();
  void hash_feed(uint32_t word, uint8_t byte_enables);
  void hash_finish();
  void hash_free();
};
