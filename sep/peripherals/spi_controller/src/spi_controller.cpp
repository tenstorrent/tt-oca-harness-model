// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_controller.cpp
 * @brief Implementation of SPI Controller TLM model
 * 
 * This file implements the SPI Host TLM model including:
 * - Main spi_controller class constructor and configuration
 * - Transaction processing with TLM loosely-timed modeling
 * - FIFO management and watermark handling
 * - Interrupt and DMA trigger generation
 * - Error detection and reporting
 * - Register callback implementations
 */

#include "spi_controller.h"
#include <iostream>


/// =============================================================================
/// SPI_HOST Main Implementation
/// =============================================================================

spi_controller_ip::spi_controller_ip(sc_module_name n, int log_verbosity)
   : spi_controller_base(n, "spi_controller", 0x100),
     spi_master("spi_master"),
     irq_o("irq_o"),
     error_irq("error_irq"),
     spi_event_irq("spi_event_irq"),
     dma_trigger("dma_trigger"),
     rst_ni("rst_ni"),
     clk_i("clk_i"),
     NumCS("NumCS", 1),
     TxDepth("TxDepth", 72),
     RxDepth("RxDepth", 64),
     ByteOrder("ByteOrder", true),
     CmdDepth("CmdDepth", 4),
     ClkPeriodNs("ClkPeriodNs", 10.0),
     TimeKeeperQuantumNs("TimeKeeperQuantumNs", 0.0),
     verbosity("verbosity", log_verbosity),
     m_fsm_state(fsm_state_e::IDLE),
     m_transaction_pending(false)
{
   logger.setMaxVerbosity(verbosity.get_param_value());
   logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
   logger.setFunctionTrace(false);

   /// Initialize CONFIGOPTS shadow array (one entry per chip select)
   /// Reset value = 0x0 per datasheet
   m_configopts_array.resize(get_num_cs(), 0x0);



   /// Register all callbacks
   register_callbacks();

   /// Register transaction processing thread
   SC_THREAD(spi_transaction_thread);

   /// Register signal update method (single driver for all output signals)
   SC_METHOD(update_output_signals_method);
   sensitive << m_update_signals_event;
   dont_initialize();

   if (TimeKeeperQuantumNs.get_param_value() > 0.0) {
      // Keep the keeper aligned, but do not steal the process-wide quantum
      // from a composing platform or ISS.
      auto& gq = tlm::tlm_global_quantum::instance();
      if (gq.get() == sc_core::SC_ZERO_TIME) {
         gq.set(sc_core::sc_time(TimeKeeperQuantumNs.get_param_value(),
                                 sc_core::SC_NS));
      }
      m_time_keeper.reset();
   }
}

/**
 * @brief SystemC end of elaboration phase
 */
void spi_controller_ip::end_of_elaboration()
{
   SC_METHOD(reset_process);
   sensitive << rst_ni.negedge_event();

   irq_o.initialize(false);
   error_irq.initialize(false);
   spi_event_irq.initialize(false);
   dma_trigger.initialize(false);

    if (!clk_i.get_interface()) {
        REG_ERROR(0, logger) << name() << "clk_i port must be bound" << std::endl;
    }
    if (!spi_master.get_interface()) {
        REG_ERROR(0, logger) << name() << "spi_master port must be bound" << std::endl;
    }
}

/**
 * @brief Register all memory-mapped register callbacks
 */
void spi_controller_ip::register_callbacks()
{
   // ===== Register Write Callbacks =====

   // INTR_STATE (0x0) - W1C for interrupt state
   auto intr_status_mask = INTR_STATE.write_bit_mask;
   memory.register_write_callback(
      [this, intr_status_mask](uint32_t value) { return handle_write_INTR_STATE(value, intr_status_mask); },
      INTR_STATE.offset);

   // INTR_ENABLE (0x4) - Interrupt enable control
   auto intr_enable_mask = INTR_ENABLE.write_bit_mask;
   memory.register_write_callback(
      [this, intr_enable_mask](uint32_t value) { return handle_write_INTR_ENABLE(value, intr_enable_mask); },
      INTR_ENABLE.offset);

   // INTR_TEST (0x8) - Interrupt testing
   auto intr_test_mask = INTR_TEST.write_bit_mask;
   memory.register_write_callback(
      [this, intr_test_mask](uint32_t value) { return handle_write_INTR_TEST(value, intr_test_mask); },
      INTR_TEST.offset);

   // CONTROL (0xC) - System control (SPIEN, SW_RST, OUTPUT_EN, watermarks)
   auto ctrl_mask = CONTROL.write_bit_mask;
   memory.register_write_callback(
      [this, ctrl_mask](uint32_t value) { return handle_write_CONTROL(value, ctrl_mask); },
      CONTROL.offset);

   // COMMAND (0x1C) - Command register (pre-write callback for validation)
   auto cmd_mask = COMMAND.write_bit_mask;
   memory.register_write_callback(
      [this, cmd_mask](uint32_t value) { return handle_write_COMMAND(value, cmd_mask); },
      COMMAND.offset);

   // TXDATA (0x24) - Transmit FIFO (byte-enable aware)
   auto txdata_mask = TXDATA.write_bit_mask;
   memory.register_write_callback_with_be(
      [this, txdata_mask](uint32_t value, uint8_t be) { return handle_write_TXDATA(value, be, txdata_mask); },
      TXDATA.offset);

   // ERROR_ENABLE (0x28) - Error interrupt masking
   auto error_enable_mask = ERROR_ENABLE.write_bit_mask;
   memory.register_write_callback(
      [this, error_enable_mask](uint32_t value) { return handle_write_ERROR_ENABLE(value, error_enable_mask); },
      ERROR_ENABLE.offset);

   // ERROR_STATUS (0x2C) - Error status (W1C semantics)
   auto error_status_mask = ERROR_STATUS.write_bit_mask;
   memory.register_write_callback(
      [this, error_status_mask](uint32_t value) { return handle_write_ERROR_STATUS(value, error_status_mask); },
      ERROR_STATUS.offset);

   // EVENT_ENABLE (0x30) - Event interrupt masking
   auto event_enable_mask = EVENT_ENABLE.write_bit_mask;
   memory.register_write_callback(
      [this, event_enable_mask](uint32_t value) { return handle_write_EVENT_ENABLE(value, event_enable_mask); },
      EVENT_ENABLE.offset);

   // CONFIGOPTS (0x14) - Per-device configuration with shadow array support
   auto cfg_write_mask = CONFIGOPTS.write_bit_mask;
   memory.register_write_callback(
      [this, cfg_write_mask](uint32_t value) { return handle_write_CONFIGOPTS(value, cfg_write_mask); },
      CONFIGOPTS.offset);

   // ===== Register Read Callbacks =====

   // STATUS (0x10) - Dynamic status computation
   auto status_read_mask = STATUS.read_bit_mask;
   memory.register_read_callback(
      [this, status_read_mask](uint32_t& value) { return handle_read_STATUS(value, status_read_mask); },
      STATUS.offset);

   // CONFIGOPTS (0x14) - Per-device configuration read from shadow array
   auto cfg_read_mask = CONFIGOPTS.read_bit_mask;
   memory.register_read_callback(
      [this, cfg_read_mask](uint32_t& value) { return handle_read_CONFIGOPTS(value, cfg_read_mask); },
      CONFIGOPTS.offset);

   // RXDATA (0x20) - Receive FIFO
   auto rxdata_mask = RXDATA.read_bit_mask;
   memory.register_read_callback(
      [this, rxdata_mask](uint32_t& value) { return handle_read_RXDATA(value, rxdata_mask); },
      RXDATA.offset);
}

/**
 * @brief Process reset signal and reset all state
 */
void spi_controller_ip::reset_process()
{
    reset_all_registers();

    for (size_t i = 0; i < m_configopts_array.size(); i++) {
        m_configopts_array[i] = 0x0;
    }

    // Reset FSM state and transaction tracking
    m_fsm_state = fsm_state_e::IDLE;
    m_transaction_pending = false;


    // Flush all FIFOs
    flush_fifos();

    // Clear command queue
    while (!m_command_queue.empty()) {
        m_command_queue.pop();
    }

    m_tx_below_wm_prev = false;

    // Clear INTR_TEST forced interrupt state
    m_intr_test_error_forced = false;
    m_intr_test_spi_event_forced = false;
}

/**
 * @brief Push data to TX FIFO
 */
bool spi_controller_ip::tx_fifo_push(uint32_t data)
{
    if (is_tx_fifo_full()) {
        return false;
    }
    m_tx_fifo.push_back(data);
    m_tx_data_available.notify(SC_ZERO_TIME);
    return true;
}

/**
 * @brief Pop data from TX FIFO
 */
bool spi_controller_ip::tx_fifo_pop(uint32_t& data)
{
    if (m_tx_fifo.empty()) {
        return false;
    }
    data = m_tx_fifo.front();
    m_tx_fifo.pop_front();
    return true;
}

/**
 * @brief Push data to RX FIFO
 */
bool spi_controller_ip::rx_fifo_push(uint32_t data)
{
    if (m_rx_fifo.size() >= get_rx_depth()) {
        return false;
    }
    m_rx_fifo.push_back(data);
    return true;
}

/**
 * @brief Pop data from RX FIFO
 */
bool spi_controller_ip::rx_fifo_pop(uint32_t& data)
{
    if (m_rx_fifo.empty()) {
        return false;
    }
    data = m_rx_fifo.front();
    m_rx_fifo.pop_front();
    // No drain-event needed: DMA's execute_transfer now does wait(SC_ZERO_TIME)
    // before reading lsio_trigger, which lets all pending signal updates settle.
    // The chunked push loop is gone; SPI pushes word-by-word and waits on
    // m_rx_space_available_event when the FIFO is full.
    return true;
}

/**
 * @brief Flush both TX and RX FIFOs
 */
void spi_controller_ip::flush_fifos()
{
    m_tx_fifo.clear();
    m_rx_fifo.clear();
}

/**
 * @brief Pack bytes into 32-bit word according to byte ordering
 */
uint32_t spi_controller_ip::pack_word(const uint8_t* bytes, size_t count)
{
    uint32_t word = 0;

    if (get_byte_order()) {
        // Little-Endian: LSB first
        for (size_t i = 0; i < count && i < 4; i++) {
            word |= (static_cast<uint32_t>(bytes[i]) << (i * 8));
        }
    } else {
        // Big-Endian: MSB first
        for (size_t i = 0; i < count && i < 4; i++) {
            word |= (static_cast<uint32_t>(bytes[i]) << ((3 - i) * 8));
        }
    }

    return word;
}

/**
 * @brief Unpack 32-bit word into bytes according to byte ordering
 */
void spi_controller_ip::unpack_word(uint32_t word, uint8_t* bytes, size_t count)
{
    if (get_byte_order()) {
        // Little-Endian: LSB first
        for (size_t i = 0; i < count && i < 4; i++) {
            bytes[i] = static_cast<uint8_t>((word >> (i * 8)) & 0xFF);
        }
    } else {
        // Big-Endian: MSB first
        for (size_t i = 0; i < count && i < 4; i++) {
            bytes[i] = static_cast<uint8_t>((word >> ((3 - i) * 8)) & 0xFF);
        }
    }
}

/**
 * @brief Calculate functional delay for a segment transaction
 */
double spi_controller_ip::calculate_segment_delay(const spi_segment_t& segment, const spi_config_t& config)
{
   // Get core clock period using accessor (works for both CCI and non-CCI modes)

    sc_time clk_period = get_clk_period();

    if (clk_period == SC_ZERO_TIME) {
        REG_WARN(0, logger) << "[SPI_HOST/TIMING] Invalid clock period, using default 10ns (100 MHz)" << std::endl;
        clk_period = sc_time(10, SC_NS);
    }

    // SCK period = 2 * (CLKDIV + 1) * T_clk
    sc_time sck_period = clk_period * (2 * (config.clkdiv + 1));
    sc_time sck_half_period = sck_period / 2.0;

    // Segment time = number of bits * SCK period
    uint32_t bits_per_cycle = 8;
    switch (segment.speed) {
        case spi_speed_e::STANDARD: bits_per_cycle = 8; break;
        case spi_speed_e::DUAL:     bits_per_cycle = 4; break;
        case spi_speed_e::QUAD:     bits_per_cycle = 2; break;
    }

    uint32_t total_cycles = segment.len * bits_per_cycle;

    // ====================================================================
    // Calculate timing margins (all specified in HALF-cycles per datasheet)
    // ====================================================================
    // Per OpenTitan SPI_HOST datasheet:
    // CSNLEAD:  (CSNLEAD+1) half-cycles between CSB falling and first SCK edge
    // CSNTRAIL: (CSNTRAIL+1) half-cycles between last SCK edge and CSB rising
    // CSNIDLE:  (CSNIDLE+1) half-cycles with CSB high between transactions
    // ====================================================================

    uint32_t csnlead_half_cycles = config.csnlead + 1;

    uint32_t csntrail_half_cycles = config.csntrail + 1;

    // CSNIDLE: (CSNIDLE+1) half-cycles with CSB high between transactions
    // Note: CSNIDLE only applies when segment.csaat = 0 (CSB deasserted)
    // When CSAAT=1, CSB stays low for next segment, so no idle time
    uint32_t csnidle_half_cycles = segment.csaat ? 0 : (config.csnidle + 1);

     uint32_t total_margin_half_cycles = csnlead_half_cycles + csntrail_half_cycles + csnidle_half_cycles;

    // Total time = data transfer time + timing margins
    sc_time data_time = sck_period * total_cycles;

    sc_time margin_time = sck_half_period * total_margin_half_cycles;

    sc_time total_time = data_time + margin_time;

    return total_time.to_seconds();
}

/**
 * @brief Set FSM state
 */
void spi_controller_ip::set_fsm_state(fsm_state_e new_state)
{
    m_fsm_state = new_state;
}

/**
 * @brief Validate CSID is within range [0, NumCS-1]
 */
bool spi_controller_ip::validate_csid(uint32_t csid, const char* context)
{
    if (csid >= get_num_cs()) {
        REG_ERROR(0, logger) << "[SPI_HOST/" << context << " ERROR] CSID (" << csid
                  << ") >= NumCS (" << get_num_cs() << ")" << std::endl;
        ERROR_STATUS.CSIDINVAL = 1;
        update_error_interrupt_state();
        return false;
    }
    return true;
}

/**
 * @brief SPI transaction processing thread
 */
void spi_controller_ip::spi_transaction_thread()
{
    while (true) {
        // Wait for transaction event (no race condition with event-based synchronization)
        wait(m_transaction_event);

        if (!clk_i.read()) {
            REG_ERROR(0, logger) << "[SPI_HOST/TRANSACTION] clk_i port not bound or has invalid value (≤0 Hz). Cannot process SPI transactions without valid clock input" << std::endl;
            ERROR_STATUS.ACCESSINVAL = 1;
            update_error_interrupt_state();
            continue;
        }

        // Process all queued commands.
        // RTL: en = en_sw & ~enb_error — core is disabled when ERROR_STATUS has any set bit.
        // Commands stay queued until firmware clears errors; READY remains based on FIFO space.
        while (!m_command_queue.empty() && CONTROL.SPIEN && CONTROL.OUTPUT_EN &&
               !CONTROL.SW_RST && (static_cast<uint32_t>(ERROR_STATUS) == 0)) {
            set_fsm_state(fsm_state_e::ACTIVE);
            update_spi_event_intr_status();

            spi_segment_t segment = m_command_queue.front().first;
            spi_config_t config = m_command_queue.front().second;

            // Process the transaction (returns false on error)
            bool transaction_success = process_single_transaction(segment, config);

            // Pop the segment we just processed — UNLESS a SW_RST cleared the
            // queue while this segment was in flight (m_sw_reset_abort, armed by
            // SW_RST only when a transaction is ACTIVE). In that case the segment
            // we processed was already flushed and the current front is a NEW
            // command (e.g. the opcode+address of the read firmware issues right
            // after flushing for the next transfer); popping it would silently
            // drop that command. m_sw_reset_abort is cleared at the next
            // process_single_transaction() entry, so the new command pops normally.
            if (!m_sw_reset_abort && !m_command_queue.empty()) {
                m_command_queue.pop();
            }

            if (!transaction_success) {
                // The transaction did not complete. Two disjoint causes:
                //  (a) a real error (e.g. TX underflow) set ERROR_STATUS — the
                //      while-guard below is now false, so the loop exits and the
                //      core stays halted until firmware clears the error, then
                //      handle_write_ERROR_STATUS re-kicks m_transaction_event.
                //  (b) a SW_RST aborted an in-flight blocked segment — ERROR_STATUS
                //      is clear and the queue was flushed, possibly re-populated
                //      with the next read's commands. We must re-evaluate the queue
                //      rather than break: the COMMAND-write notifications that queued
                //      those commands may have been lost (the thread was parked on
                //      a FIFO wait, not m_transaction_event), so breaking to
                //      wait(m_transaction_event) would strand them.
                // 'continue' handles both: the guard exits on a real error and
                // keeps draining on a clean SW_RST abort.
                REG_INFO(1, logger) << "[SPI_HOST] Transaction did not complete (error or SW_RST abort); "
                                     << "re-evaluating command queue" << std::endl;
                continue;
            }

            // Determine next state based on CSAAT flag
            if (segment.csaat) {
                // Multi-segment transaction - keep CSB low and wait for next segment
                set_fsm_state(fsm_state_e::IDLE_CSB_ACTIVE);
            } else {
                // Single segment or last segment - return to IDLE and deassert CSB
                set_fsm_state(fsm_state_e::IDLE);
            }

            // Check for IDLE event
            update_spi_event_intr_status();
        }
    }
}

/**
 * @brief Process a single SPI transaction segment
 * Returns true on success, false on error (error status already set)
 */
bool spi_controller_ip::process_single_transaction(const spi_segment_t& segment, const spi_config_t& config)
{
    // ===================================================================
    // NOTE: RX FIFO pre-check now done synchronously in handle_write_COMMAND
    // for TLM LT compliance. No need for async check here since command
    // would have been rejected before reaching this point if insufficient space.
    // ===================================================================

    // Fresh transaction: clear any stale SW_RST abort request.
    m_sw_reset_abort = false;

    // Prepare TX/RX buffers for the entire segment
    uint8_t tx_buffer[512];
    uint8_t rx_buffer[512];
    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_buffer, 0, sizeof(rx_buffer));

    // For TX_ONLY and BIDIR, pull data from TX FIFO
    if (segment.direction == spi_direction_e::TX_ONLY ||
        segment.direction == spi_direction_e::BIDIR) {

        uint32_t bytes_needed = segment.len;
        uint32_t byte_idx = 0;

       REG_INFO(1, logger) << "[SPI_HOST] Pulling " << bytes_needed << " bytes from TX FIFO" << std::endl;

        while (byte_idx < bytes_needed) {
            uint32_t word;
            if (!tx_fifo_pop(word)) {
                // TX FIFO empty during an active transaction.
                // Assert dma_trigger (FIFO is below watermark) and wait for the
                // FIFO to reach the watermark — signaled by m_tx_fifo_at_watermark
                // when dma_trigger transitions HIGH→LOW in update_output_signals_method.
                //
                // Waiting for the watermark event (not per-word) is essential for
                // DMA hardware handshake: SPI must not consume words until a full
                // chunk has accumulated in the FIFO, so that the subsequent LOW→HIGH
                // transition is visible to the DMA handshake_monitor_thread as a
                // rising edge on lsio_trigger.
                update_dma_trigger();
                update_spi_event_intr_status();
                // When TX_WATERMARK=0 (no DMA), wake on any TX word pushed.
                // When TX_WATERMARK>0 (DMA), wait for watermark chunk (HIGH→LOW on dma_trigger).
                if (CONTROL.TX_WATERMARK > 0) {
                    wait(m_tx_fifo_at_watermark);
                } else {
                    wait(m_tx_data_available);
                }

                // SW_RST while blocked: abort this transaction cleanly.
                if (m_sw_reset_abort) {
                    set_fsm_state(fsm_state_e::IDLE);
                    return false;
                }

                if (!tx_fifo_pop(word)) {
                    REG_ERROR(0, logger) << "[SPI_HOST] TX FIFO underflow: "
                        << "no data after watermark event (watermark not reached). Segment requires "
                        << bytes_needed << " bytes, only " << byte_idx << " available." << std::endl;
                    ERROR_STATUS.underflow = 1;
                    update_error_interrupt_state();
                    set_fsm_state(fsm_state_e::IDLE);
                    update_spi_event_intr_status();
                    return false;
                }
            }

            // Unpack word into bytes according to ByteOrder
            uint8_t bytes[4];
            uint32_t bytes_to_unpack = std::min(4u, bytes_needed - byte_idx);
            unpack_word(word, bytes, bytes_to_unpack);

            for (uint32_t i = 0; i < bytes_to_unpack; i++) {
                tx_buffer[byte_idx++] = bytes[i];
            }

        }

        // Update FIFO status and check watermarks
        update_dma_trigger();
        update_spi_event_intr_status();
    }

    // Calculate functional delay for this segment (loosely-timed modeling)
    double segment_delay = calculate_segment_delay(segment, config);

    // Initiate SPI transaction via spi_master port
    bool success = spi_master->spi_transaction(segment, config, tx_buffer, rx_buffer);

    REG_INFO(1, logger) << "[SPI_HOST] SPI transaction completed after "
            << (segment_delay * 1e9) << " ns, success=" << success << std::endl;

    // For RX_ONLY and BIDIR, push received data into RX FIFO BEFORE the timing delay.
    //
    // In TLM LT, data is logically available as soon as bits start arriving on MISO;
    // the segment_delay models bus occupancy (bit-clock time), not data latency.
    // Pushing data first lets the DMA handshake run concurrently with the timing advance.
    //
    // Words are pushed one at a time. When the FIFO is full, SPI waits for
    // m_rx_space_available_event (fired from handle_read_RXDATA when DMA pops a word).
    // dma_trigger is updated after every push so the DMA handshake_monitor_thread sees
    // natural LOW→HIGH rising edges as the FIFO level crosses RX_WATERMARK.
    // DMA's execute_transfer does wait(SC_ZERO_TIME) before reading lsio_trigger at each
    // chunk boundary, ensuring signal updates have propagated before the check.
    if (success && (segment.direction == spi_direction_e::RX_ONLY ||
                   segment.direction == spi_direction_e::BIDIR)) {

        uint32_t bytes_received = segment.len;
        uint32_t byte_idx = 0;

        REG_INFO(1, logger) << "[SPI_HOST] Pushing " << bytes_received
                             << " bytes into RX FIFO" << std::endl;

        while (byte_idx < bytes_received) {
            uint8_t bytes[4];
            uint32_t bytes_to_pack = std::min(4u, bytes_received - byte_idx);
            for (uint32_t i = 0; i < bytes_to_pack; i++) {
                bytes[i] = rx_buffer[byte_idx++];
            }
            uint32_t word = pack_word(bytes, bytes_to_pack);

            while (!rx_fifo_push(word)) {
                // FIFO full: assert trigger so DMA/SW knows to drain, then wait for space.
                update_dma_trigger();
                update_spi_event_intr_status();
                wait(m_rx_space_available_event);

                // SW_RST while blocked on a full FIFO: abort this transaction.
                if (m_sw_reset_abort) {
                    set_fsm_state(fsm_state_e::IDLE);
                    return false;
                }
            }

            update_dma_trigger();
            update_spi_event_intr_status();
        }

        update_dma_trigger();
        update_spi_event_intr_status();
    }

    // Model bus-occupancy timing AFTER data is available (standard LT pattern)
    m_time_keeper.inc(sc_time(segment_delay, SC_SEC));
    if (m_time_keeper.need_sync()) {
        m_time_keeper.sync();
    }

    return true;
}

/**
 * @brief Update all output signals (interrupts and DMA trigger)
 */
void spi_controller_ip::update_output_signals_method()
{
    // Check if we're in reset - don't write to ports during reset
    if (!rst_ni.read()) {
        REG_DEBUG(2, logger) << "[SPI_HOST] update_output_signals_method: Skipping during reset" << std::endl;
        return;
    }

    // Update error_irq
    bool error_irq_assert = (INTR_STATE.error && INTR_ENABLE.error);
    error_irq.write(error_irq_assert);

    // Update spi_event_irq
    bool spi_event_irq_assert = (INTR_STATE.spi_event && INTR_ENABLE.spi_event);
    spi_event_irq.write(spi_event_irq_assert);

    // The IP has a single interrupt pin: irq_o = error_intr || spi_event_intr.
    // error_irq/spi_event_irq are kept as observability signals only.
    irq_o.write(error_irq_assert || spi_event_irq_assert);

    // Update dma_trigger
    uint32_t tx_depth = get_tx_fifo_depth();
    uint32_t rx_depth = get_rx_fifo_depth();
    uint32_t tx_watermark = CONTROL.TX_WATERMARK;
    uint32_t rx_watermark = CONTROL.RX_WATERMARK;
    bool tx_below_wm = (tx_depth < tx_watermark);
    // OT SPI Host RTL uses rx_qd >= rx_watermark for both lsio_trigger_o and
    // STATUS.RXWM. "At or above" allows DMA chunk_size == watermark to work:
    // SPI pushes exactly watermark words → trigger fires → DMA drains → trigger clears.
    //
    // The empty-FIFO term is a modelling guard, not part of the RTL expression.
    // RTL re-evaluates the comparison every clock, so a watermark of zero holding
    // rx_wm high over an empty FIFO is harmless there. Here the signal is refreshed
    // by a deferred method, so it only settles when the core yields at a quantum
    // boundary — firmware that writes CONTROL twice within one quantum (the usual
    // SW_RST-then-configure sequence momentarily leaves RX_WATERMARK at zero) would
    // otherwise leave a stale high for the secure DMA to sample at arm time and
    // drain a chunk from an empty FIFO. For every watermark of one or more the term
    // is implied by the comparison, so this only diverges where the FIFO has nothing
    // for the DMA to move. STATUS.RXWM and the RXWM event are computed on demand and
    // stay RTL-exact.
    bool rx_above_wm = (rx_depth >= rx_watermark) && (rx_depth > 0);
    bool dma_trigger_assert = tx_below_wm || rx_above_wm;

    dma_trigger.write(dma_trigger_assert);

    // Detect the TX watermark condition going false: the TX FIFO has just been
    // filled to or above TX_WATERMARK (DMA delivered a full chunk), which lets
    // process_single_transaction start consuming. This tracks the TX half alone,
    // not the combined trigger — the RX half is asserted whenever the RX level is
    // at or above its watermark, which at RX_WATERMARK=0 is permanently true.
    // (RX DMA: m_rx_fifo_drained_event is notified in rx_fifo_pop when FIFO goes empty —
    //  firing it here on trigger HIGH→LOW is too early: DMA reads word 1 of 4, FIFO drops
    //  3<4 → trigger LOW, but 3 words remain unread → premature SPI push → no rising edge.)
    if (!tx_below_wm && m_tx_below_wm_prev) {
        m_tx_fifo_at_watermark.notify(SC_ZERO_TIME);
    }
    m_tx_below_wm_prev = tx_below_wm;
}

/**
 * @brief Notify to update error_irq output
 */
void spi_controller_ip::update_error_irq()
{
    m_update_signals_event.notify();
}

/**
 * @brief Notify to update spi_event_irq output
 */
void spi_controller_ip::update_spi_event_irq()
{
    m_update_signals_event.notify();
}

/**
 * @brief Check if any enabled error is present and update INTR_STATE.error
 */
void spi_controller_ip::update_error_interrupt_state()
{
    uint32_t error_status = ERROR_STATUS;
    uint32_t error_enable = ERROR_ENABLE;

    // RTL: INTR_STATE.error.next = (ERROR_STATUS.intr || INTR_TEST.error) && INTR_ENABLE.error
    // Level-sensitive: set OR clear based on current state, gated by INTR_ENABLE.
    //
    // ACCESSINVAL is a bus error with no ERROR_ENABLE bit — the RTL's error_mask
    // hardcodes that lane to 1, so it escalates whatever the enable register says.
    // Masking it against ERROR_ENABLE would silence it permanently.
    constexpr uint32_t ACCESSINVAL_MASK = 1u << 5;
    bool has_enabled_error =
        ((error_status & (error_enable | ACCESSINVAL_MASK)) != 0);
    bool error_intr = (has_enabled_error || m_intr_test_error_forced) && (bool)INTR_ENABLE.error;
    INTR_STATE.error = error_intr ? 1 : 0;

    update_error_irq();
}

/**
 * @brief Level-sensitive recompute of INTR_STATE.spi_event per RTL equation:
 *   spi_event_intr = (|(event_vector & event_mask) || INTR_TEST.spi_event) && INTR_ENABLE.spi_event
 */
void spi_controller_ip::update_spi_event_intr_status()
{
    bool any_event = false;

    if (EVENT_ENABLE.RXFULL && is_rx_fifo_full())  any_event = true;
    if (EVENT_ENABLE.TXEMPTY && is_tx_fifo_empty()) any_event = true;
    if (EVENT_ENABLE.RXWM) {
        if (get_rx_fifo_depth() >= (uint32_t)CONTROL.RX_WATERMARK)
            any_event = true;
    }
    if (EVENT_ENABLE.TXWM) {
        if (get_tx_fifo_depth() < (uint32_t)CONTROL.TX_WATERMARK)
            any_event = true;
    }
    // RTL: READY = ~command_busy, independent of error state
    if (EVENT_ENABLE.READY && (m_fsm_state == fsm_state_e::IDLE) && !is_cmd_queue_full()) any_event = true;
    if (EVENT_ENABLE.IDLE && (m_fsm_state != fsm_state_e::ACTIVE)) any_event = true;

    bool spi_intr = (any_event || m_intr_test_spi_event_forced) && (bool)INTR_ENABLE.spi_event;
    INTR_STATE.spi_event = spi_intr ? 1 : 0;
    update_spi_event_irq();
}

/**
 * @brief Notify to update DMA trigger output
 */
void spi_controller_ip::update_dma_trigger()
{
    m_update_signals_event.notify();
}

/**
 * @brief INTR_STATE register write callback
 *
 * The RDL declares both fields sw=r, hw=w, and the RTL drives them as a live view
 * of the gated interrupt lines rather than a sticky latch. Software writes are
 * therefore accepted on the bus and discarded, as they are in silicon — the bits
 * only clear when the underlying condition does.
 */
bool spi_controller_ip::handle_write_INTR_STATE(uint32_t value, uint32_t mask)
{
    (void)value;
    (void)mask;
    return true;
}

/**
 * @brief INTR_ENABLE register write callback
 */
bool spi_controller_ip::handle_write_INTR_ENABLE(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = INTR_ENABLE;
    uint32_t new_value = (value & mask) | (current & ~mask);
    INTR_ENABLE = new_value;

    // RTL: INTR_STATE.next = (source || INTR_TEST) && INTR_ENABLE — recompute both bits.
    update_error_interrupt_state();
    update_spi_event_intr_status();

    return true;
}

/**
 * @brief INTR_TEST register write callback for interrupt testing
 */
bool spi_controller_ip::handle_write_INTR_TEST(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = INTR_TEST;
    uint32_t new_value = (value & mask) | (current & ~mask);
    INTR_TEST = new_value;

    // RTL: error_intr     = (ERROR_STATUS.intr || INTR_TEST.error)     && INTR_ENABLE.error
    //      spi_event_intr = (status_spi_event  || INTR_TEST.spi_event) && INTR_ENABLE.spi_event
    // Update forced flags then recompute INTR_STATE via level-sensitive helpers.
    // Helpers handle both set and clear, and gate by INTR_ENABLE — so INTR_TEST injection
    // is blocked when INTR_ENABLE is 0, matching RTL behaviour.
    m_intr_test_error_forced     = (bool)INTR_TEST.error;
    m_intr_test_spi_event_forced = (bool)INTR_TEST.spi_event;

    update_error_interrupt_state();
    update_spi_event_intr_status();

    return true;
}

/**
 * @brief ERROR_STATUS register write callback with W1C semantics
 */
bool spi_controller_ip::handle_write_ERROR_STATUS(uint32_t value, uint32_t mask)
{
    // Read current ERROR_STATUS value
    uint32_t current = ERROR_STATUS;

    // Apply write bitmask before W1C logic
    uint32_t masked_value = value & mask;

    // W1C logic: Clear bits where write_value has 1s
    uint32_t new_value = current & ~masked_value;

    // Write back the new value
    ERROR_STATUS = new_value;

    // Update error interrupt state and port
    update_error_interrupt_state();

    // RTL: en = en_sw & ~enb_error — core resumes when all errors are cleared.
    // The transaction thread exits its inner loop when ERROR_STATUS becomes non-zero.
    // Re-kick it here so it drains any commands that were queued before/during the error.
    if (new_value == 0 && !m_command_queue.empty() && CONTROL.SPIEN && CONTROL.OUTPUT_EN) {
        m_transaction_event.notify();
    }

    return true;
}

/**
 * @brief ERROR_ENABLE register write callback
 */
bool spi_controller_ip::handle_write_ERROR_ENABLE(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = ERROR_ENABLE;
    uint32_t new_value = (value & mask) | (current & ~mask);
    ERROR_ENABLE = new_value;

    // Update error interrupt state based on new mask
    update_error_interrupt_state();

    return true;
}

/**
 * @brief EVENT_ENABLE register write callback
 */
bool spi_controller_ip::handle_write_EVENT_ENABLE(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = EVENT_ENABLE;
    uint32_t new_value = (value & mask) | (current & ~mask);
    EVENT_ENABLE = new_value;

    // The mask is part of the combinational equation, so enabling a bit whose
    // condition already holds asserts the interrupt straight away, and disabling
    // the last asserted bit drops it.
    update_spi_event_intr_status();

    return true;
}

/**
 * @brief CONTROL register write callback
 */
bool spi_controller_ip::handle_write_CONTROL(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = CONTROL;
    uint32_t new_value = (value & mask) | (current & ~mask);
    CONTROL = new_value;

    // Extract control fields
    REG_INFO(2, logger) << "  SPIEN: " << (uint32_t)CONTROL.SPIEN << std::endl
                         << "  SW_RST: " << (uint32_t)CONTROL.SW_RST << std::endl
                         << "  OUTPUT_EN: " << (uint32_t)CONTROL.OUTPUT_EN << std::endl
                         << "  TX_WATERMARK: " << (int)CONTROL.TX_WATERMARK << std::endl
                         << "  RX_WATERMARK: " << (int)CONTROL.RX_WATERMARK << std::endl;

    // Track previous SPIEN state to detect 0->1 transitions
    bool prev_spien = (current >> 31) & 0x1;
    bool new_spien = CONTROL.SPIEN;

    // Handle SW_RST first (highest priority)
    if (CONTROL.SW_RST) {
        REG_INFO(2, logger) << "  [RESET] Software reset triggered!" << std::endl;

        // Capture whether a transaction is in flight BEFORE we overwrite the FSM
        // state below. The transaction thread holds fsm_state ACTIVE for the whole
        // span between reading m_command_queue.front() and the post-process pop, so
        // ACTIVE here means "the segment currently being processed is still the
        // queue front we are about to clear". This must be sampled first: the
        // set_fsm_state(IDLE) on the next line would otherwise make the arming
        // check below always false (the original bug — the abort was never armed,
        // so a queue clear + re-populate during an in-flight segment let the
        // post-process pop drop the freshly queued command).
        bool was_active = (m_fsm_state == fsm_state_e::ACTIVE);

        // Reset FSM state
        set_fsm_state(fsm_state_e::IDLE);

        // Clear transaction pending flag
        m_transaction_pending = false;

        // Flush all FIFOs
        flush_fifos();

        // Clear command queue
        while (!m_command_queue.empty()) {
            m_command_queue.pop();
        }

        // Clear all error status bits
        ERROR_STATUS = 0;

        // Clear interrupt states
        INTR_STATE = 0;

        // Update interrupt ports
        update_error_irq();
        update_spi_event_irq();

        // Update DMA trigger
        update_dma_trigger();

        // Wake any stalled transaction so it can abort cleanly on reset. Only
        // arm the abort (and the TX-side wakes) when a transaction was actually
        // in flight (was_active, sampled above before the FSM was forced IDLE):
        // arming it while the engine is idle would leave m_sw_reset_abort set and
        // swallow the *next* command's transaction — e.g. the read firmware
        // issues immediately after this SW_RST. The abort is consumed exactly
        // once: by the post-process pop guard (which then skips popping the
        // freshly queued command that took the flushed segment's place) and is
        // cleared again at the next process_single_transaction() entry.
        m_rx_space_available_event.notify();
        if (was_active) {
            m_sw_reset_abort = true;
            m_tx_data_available.notify();
            m_tx_fifo_at_watermark.notify();
        }

        // CONTROL.SW_RST is a level: the core, FIFOs and command queue stay held
        // in reset until software clears the bit. Do not auto-clear.

        REG_INFO(2, logger) << "  [RESET] Software reset held (release SW_RST to resume)" << std::endl;
    }

    // Handle SPIEN (enable/disable FSM operation)
    if (new_spien) {
        REG_INFO(2, logger) << "  [CONTROL] SPI Host enabled" << std::endl;
        // If SPIEN changed from 0 to 1, and there are commands queued, wake up transaction thread
        if (!prev_spien && !m_command_queue.empty()) {
            m_transaction_event.notify();
        }
        else {
            REG_INFO(2, logger) << "  [CONTROL] SPI Host disabled" << std::endl;
        }
        // FSM should not process new transactions
        // Ongoing transactions may continue or be aborted depending on implementation
    }

    // Watermark changes affect interrupt and DMA trigger generation
    update_dma_trigger();
    update_spi_event_intr_status();

    return true;
}

/**
 * @brief STATUS register read callback with dynamic status computation
 */
bool spi_controller_ip::handle_read_STATUS(uint32_t& value, uint32_t mask)
{
    // RTL: STATUS.READY = ~command_busy = command FIFO has space (independent of error state).
    // enb_error disables the core but does not affect READY; firmware can still queue commands.
    if (!is_cmd_queue_full()) {
        STATUS.READY = 1;
    } else {
        STATUS.READY = 0;
    }

    // Set ACTIVE bit - high while FSM is processing, or while commands are queued and
    // the controller is enabled (command queued but transaction thread not yet scheduled).
    // This prevents wait_for_idle from returning before a freshly-written COMMAND is processed.
    bool cmd_pending = (!m_command_queue.empty() && CONTROL.SPIEN && CONTROL.OUTPUT_EN);
    if (m_fsm_state == fsm_state_e::ACTIVE || cmd_pending) {
        STATUS.ACTIVE = 1;
    } else {
        STATUS.ACTIVE = 0;
    }

    // Set FIFO full flags
    STATUS.TXFULL = is_tx_fifo_full() ? 1 : 0;
    STATUS.RXFULL = is_rx_fifo_full() ? 1 : 0;

    // Set FIFO empty flags
    STATUS.TXEMPTY = is_tx_fifo_empty() ? 1 : 0;
    STATUS.RXEMPTY = is_rx_fifo_empty() ? 1 : 0;

    // Set FIFO stall conditions
    STATUS.TXSTALL = (m_fsm_state == fsm_state_e::ACTIVE && is_tx_fifo_empty()) ? 1 : 0;

    // RXSTALL: Active transaction but RX FIFO full (waiting for space)
    STATUS.RXSTALL = (m_fsm_state == fsm_state_e::ACTIVE && is_rx_fifo_full()) ? 1 : 0;

    // BYTEORDER bit reflects the ByteOrder parameter (1 = Little-Endian)
    STATUS.BYTEORDER = get_byte_order() ? 1 : 0;

    // Set FIFO depth fields (number of entries currently in FIFOs)
    STATUS.TXQD = get_tx_fifo_depth();
    STATUS.RXQD = get_rx_fifo_depth();
    STATUS.CMDQD = get_cmd_queue_depth();

    // Set watermark flags
    uint32_t tx_watermark = CONTROL.TX_WATERMARK;
    uint32_t rx_watermark = CONTROL.RX_WATERMARK;
    // RTL compares unconditionally, so a zero RX watermark leaves RXWM permanently
    // asserted (rx_qd >= 0 is always true).
    STATUS.TXWM = (STATUS.TXQD < tx_watermark) ? 1 : 0;
    STATUS.RXWM = (STATUS.RXQD >= rx_watermark) ? 1 : 0;

    // Return the computed status value with read bitmask applied
    value = STATUS & mask;

    return true;
}

/**
 * @brief COMMAND register write callback with validation and error detection
 */
bool spi_controller_ip::handle_write_COMMAND(uint32_t value, uint32_t mask)
{
   REG_INFO(1, logger) << "[SPI_HOST] COMMAND register pre-write callback triggered" << std::endl;
   REG_DEBUG(2, logger) << "  Value to write: 0x" << std::hex << value << std::dec << std::endl;

    // Apply write bitmask to get the actual value being written
    uint32_t masked_value = value & mask;

    // Extract COMMAND fields per upstream spi_host:
    // [24:5] LEN, [4:3] DIRECTION, [2:1] SPEED, [0] CSAAT
    uint32_t cmd_len = (masked_value >> 5) & 0xFFFFF;
    bool     cmd_csaat = masked_value & 0x1;
    uint8_t  cmd_speed = (masked_value >> 1) & 0x3;
    uint8_t  cmd_direction = (masked_value >> 3) & 0x3;

    REG_DEBUG(2, logger) << "  LEN: " << cmd_len << " (actual bytes: " << (cmd_len + 1) << ")" << std::endl;
    REG_DEBUG(2, logger) << "  CSAAT: " << cmd_csaat << std::endl;
    REG_DEBUG(2, logger) << "  SPEED: " << (int)cmd_speed << " (0=Std, 1=Dual, 2=Quad)" << std::endl;
    REG_DEBUG(2, logger) << "  DIRECTION: " << (int)cmd_direction << " (0=Dummy, 1=Rx, 2=Tx, 3=Bidir)" << std::endl;

    if (CONTROL.SW_RST) {
        // Command queue is held in reset while SW_RST is asserted.
        return true;
    }

    // The transaction engine uses fixed 512-byte segment buffers.  LEN is
    // zero-based, so reject any command representing more than 512 bytes
    // before narrowing into spi_segment_t::len or queueing work.
    if (cmd_len >= 512u) {
        REG_ERROR(0, logger) << "[SPI_HOST/COMMAND] segment length "
                             << (cmd_len + 1u)
                             << " exceeds the 512-byte model limit" << std::endl;
        ERROR_STATUS.CMDINVAL = 1;
        update_error_interrupt_state();
        return false;
    }

    // ========================================================================
    // Validation 0: the command queue must have room.
    //
    // This is the only "busy" condition the RTL knows: command_busy_o is just
    // ~command_ready from the queue, STATUS.READY is ~command_busy, and
    // error_busy_o (CMDBUSY) is command_valid & command_busy. A latched error
    // does not block a COMMAND write — it disables the core through
    // en = en_sw & ~enb_error, so the command sits in the queue until software
    // clears ERROR_STATUS. process_single_transaction models that gate and
    // handle_write_ERROR_STATUS restarts the engine when the last error clears.
    // ========================================================================
    if (is_cmd_queue_full()) {
        REG_ERROR(0, logger) << "[SPI_HOST/COMMAND ERROR] Command FIFO full (depth=" << get_cmd_queue_depth()
                  << "/" << get_cmd_depth() << "). Cannot accept new command segment. Setting ERROR_STATUS.CMDBUSY" << std::endl;

        ERROR_STATUS.CMDBUSY = 1;
        update_error_interrupt_state();

        return false;
    }

    // Validation 2: Check CSID < NumCS
    uint32_t csid_val = CSID;

    if (!validate_csid(csid_val, "COMMAND")) {
        return false;
    }

    // Validation 3: Check valid SPEED (0-2 valid, 3 is reserved)
    if (cmd_speed > 2) {
        REG_ERROR(0, logger) << "[SPI_HOST/COMMAND ERROR] Invalid SPEED value: " << (int)cmd_speed
                  << " (valid: 0-2). Setting ERROR_STATUS.CMDINVAL" << std::endl;

        // Set ERROR_STATUS.CMDINVAL
        ERROR_STATUS.CMDINVAL = 1;

        // Update error interrupt state and port
        update_error_interrupt_state();

        return false;
    }

    // Validation 4: Check Bidirectional only allowed with Standard SPI
    // DIRECTION=3 (Bidirectional) only valid with SPEED=0 (Standard)
    // Per datasheet: "Bidirectional data transfers are not applicable for Dual- or Quad-mode segments"
    if (cmd_direction == 3 && cmd_speed != 0) {
        REG_ERROR(0, logger) << "[SPI_HOST/COMMAND ERROR] Bidirectional mode only supported with Standard SPI. "
                  << "DIRECTION=3 (Bidir) with SPEED=" << (int)cmd_speed
                  << " (not Standard). Setting ERROR_STATUS.CMDINVAL" << std::endl;

        // Set ERROR_STATUS.CMDINVAL
        ERROR_STATUS.CMDINVAL = 1;

        // Update error interrupt state and port
        update_error_interrupt_state();

        // DO NOT write to COMMAND register - reject invalid value
        return false;
    }

    // Validation 5: RX segment length is accepted after the common 512-byte
    // bound above. Hardware streams an RX
    // segment that exceeds free FIFO space by stalling on
    // m_rx_space_available_event rather than setting OVERFLOW.
    if (cmd_direction == 1 || cmd_direction == 3) {  // RX_ONLY or BIDIR
        uint32_t bytes_to_receive = cmd_len + 1;  // LEN is 0-based, so add 1
        REG_INFO(2, logger) << "[SPI_HOST/COMMAND] RX segment accepted: " << bytes_to_receive
            << " bytes (streams under back-pressure if it exceeds free FIFO space)" << std::endl;
    }

    // TX FIFO pre-check removed: DMA hardware handshake fills the FIFO after COMMAND
    // is issued. process_single_transaction waits on m_tx_data_available when
    // the FIFO is empty, allowing DMA to fill it before the SPI thread consumes.

    // Write the validated value to COMMAND register
    // This is the ONLY path that writes to COMMAND - validation failures above don't write
    COMMAND = masked_value;

    // Build segment descriptor from validated COMMAND fields
    m_current_segment.len = cmd_len + 1;
    m_current_segment.direction = static_cast<spi_direction_e>(cmd_direction);
    m_current_segment.speed = static_cast<spi_speed_e>(cmd_speed);
    m_current_segment.csaat = cmd_csaat;
    m_current_segment.csid = static_cast<uint8_t>(csid_val);

    // Capture current CONFIGOPTS into shadow array for this CSID
    // This allows per-device timing configuration
    uint8_t csid = static_cast<uint8_t>(csid_val);
    if (csid < m_configopts_array.size()) {
        m_configopts_array[csid] = CONFIGOPTS;
    }

    // Build configuration from shadow array (indexed by CSID)
    // This retrieves the device-specific timing configuration
    uint32_t config_word = m_configopts_array[csid];

    m_current_config.clkdiv = config_word & 0xFFFF;
    m_current_config.csnidle = (config_word >> 16) & 0xF;
    m_current_config.csntrail = (config_word >> 20) & 0xF;
    m_current_config.csnlead = (config_word >> 24) & 0xF;

    // Queue the segment descriptor and configuration
    m_command_queue.push(std::make_pair(m_current_segment, m_current_config));

    // Mark transaction as pending
    m_transaction_pending = true;

    // Trigger the transaction thread
    m_transaction_event.notify();

    REG_INFO(2, logger) << "  [SUCCESS] SPI transaction queued and thread notified" << std::endl;

    return true;
}

/**
 * @brief TXDATA register write callback with byte-enable support
 */
bool spi_controller_ip::handle_write_TXDATA(uint32_t value, uint8_t byte_enable, uint32_t mask)
{
    // ========================================================================
    // Byte-enable validation, matching the RTL's access_valid case statement:
    // any single byte, either aligned half-word, or the full word. Anything
    // else (a split or unaligned pattern) raises ACCESSINVAL.
    // ========================================================================
    const bool be_valid = (byte_enable == 0x1) || (byte_enable == 0x2) ||
                          (byte_enable == 0x4) || (byte_enable == 0x8) ||
                          (byte_enable == 0x3) || (byte_enable == 0x6) ||
                          (byte_enable == 0xC) || (byte_enable == 0xF);
    if (!be_valid) {
        REG_ERROR(0, logger) << "[SPI_HOST/TXDATA ERROR] Invalid byte-enable pattern: 0x"
                              << std::hex << (int)byte_enable << std::dec
                              << ". Valid patterns: 0x1/0x2/0x4/0x8 (single byte), "
                              << "0x3/0x6/0xC (aligned half-word), 0xF (full word). "
                              << "Setting ERROR_STATUS.ACCESSINVAL" << std::endl;
        ERROR_STATUS.ACCESSINVAL = 1;
        update_error_interrupt_state();
        return false;
    }

    // Apply byte-enable mask to value
    uint32_t byte_masked_value = 0;
    if (byte_enable & 0x1) byte_masked_value |= (value & 0x000000FF);
    if (byte_enable & 0x2) byte_masked_value |= (value & 0x0000FF00);
    if (byte_enable & 0x4) byte_masked_value |= (value & 0x00FF0000);
    if (byte_enable & 0x8) byte_masked_value |= (value & 0xFF000000);

    // Apply write bitmask
    uint32_t masked_value = byte_masked_value & mask;

    // Check if TX FIFO is full
    if (is_tx_fifo_full()) {
        REG_ERROR(0, logger) << "[SPI_HOST/TXDATA ERROR] TX FIFO overflow - FIFO is full. Setting ERROR_STATUS.OVERFLOW" << std::endl;
        ERROR_STATUS.overflow = 1;
        update_error_interrupt_state();
        return false;
    }

    // Push 32-bit word to TX FIFO
    bool success = tx_fifo_push(masked_value);

    if (success) {
        // Update DMA trigger based on watermark
        update_dma_trigger();

        // Update event interrupt state (TXEMPTY, TXWM events)
        update_spi_event_intr_status();
    }

    return success;
}

/**
 * @brief RXDATA register read callback
 */
bool spi_controller_ip::handle_read_RXDATA(uint32_t& value, uint32_t mask)
{
    // Check if RX FIFO is empty
    if (is_rx_fifo_empty()) {
        REG_WARN(1, logger) << "  [WARNING] RX FIFO underflow - FIFO is empty, returning 0" << std::endl;
        value = 0;
        ERROR_STATUS.underflow = 1;
        update_error_interrupt_state();
        return true;
    }

    // Pop 32-bit word from RX FIFO
    uint32_t data;
    bool success = rx_fifo_pop(data);

    if (success) {
        // Apply read bitmask
        value = data & mask;

        // CRITICAL FIX: Signal that space is now available in RX FIFO
        // This wakes up any stalled transaction waiting for RX FIFO space
        m_rx_space_available_event.notify();

        // Update DMA trigger based on watermark
        update_dma_trigger();

        // Update event interrupt state (RXEMPTY, RXWM events)
        update_spi_event_intr_status();
    } else {
        value = 0;
        REG_WARN(1, logger) << "  Failed to pop from RX FIFO, returning 0" << std::endl;
    }

    return true;
}

/**
 * @brief CONFIGOPTS register write callback with per-device shadow array
 */
bool spi_controller_ip::handle_write_CONFIGOPTS(uint32_t value, uint32_t mask)
{
    // Apply write bitmask
    uint32_t masked_value = value & mask;

    // Get current CSID value
    uint32_t csid = CSID;

    // Check CSID range (without setting error - CONFIGOPTS access doesn't trigger CSIDINVAL)
    // Per datasheet: CSIDINVAL only set on COMMAND write, not CONFIGOPTS access
    if (csid >= get_num_cs()) {
        REG_WARN(1, logger) << "[SPI_HOST/CONFIGOPTS_WRITE] Invalid CSID (" << csid
                  << ") >= NumCS (" << get_num_cs() << "). Ignoring write." << std::endl;
        return false;  // Silently reject, don't set error
    }

    // Store masked value into shadow array for this CSID
    m_configopts_array[csid] = masked_value;

    // Also update the actual register storage for readback consistency
    CONFIGOPTS = masked_value;

    return true;
}

/**
 * @brief CONFIGOPTS register read callback from per-device shadow array
 */
bool spi_controller_ip::handle_read_CONFIGOPTS(uint32_t& value, uint32_t mask)
{
    // Get current CSID value
    uint32_t csid = CSID;

    // Check CSID range (without setting error - CONFIGOPTS access doesn't trigger CSIDINVAL)
    // Per datasheet: CSIDINVAL only set on COMMAND write, not CONFIGOPTS access
    if (csid >= get_num_cs()) {
        REG_WARN(1, logger) << "[SPI_HOST/CONFIGOPTS_READ] Invalid CSID (" << csid
                  << ") >= NumCS (" << get_num_cs() << "). Returning 0." << std::endl;
        value = 0;
        return true;  // Return 0, don't set error
    }

    // Retrieve value from shadow array for this CSID
    uint32_t config_value = m_configopts_array[csid];

    // Apply read bitmask
    value = config_value & mask;

    return true;
}
