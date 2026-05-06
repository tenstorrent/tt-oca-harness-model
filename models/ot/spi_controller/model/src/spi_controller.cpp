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

   /// Initialize CFG shadow array (one entry per chip select)
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
      sc_core::sc_time quantum(TimeKeeperQuantumNs.get_param_value(), sc_core::SC_NS);
      tlm::tlm_global_quantum::instance().set(quantum);
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

   error_irq.initialize(false);
   spi_event_irq.initialize(false);
   dma_trigger.initialize(false);

    if (!clk_i.get_interface()) {
        CSML_ERROR(0, logger) << name() << "clk_i port must be bound" << std::endl;
    }
    if (!spi_master.get_interface()) {
        CSML_ERROR(0, logger) << name() << "spi_master port must be bound" << std::endl;
    }
}

/**
 * @brief Register all memory-mapped register callbacks
 */
void spi_controller_ip::register_callbacks()
{
   // ===== Register Write Callbacks =====

   // INTR_STATUS (0x0) - W1C for interrupt state
   std::function<bool(uint32_t)> intr_state_handler;
   intr_state_handler = std::bind(
      &spi_controller_ip::handle_write_INTR_STATUS,
      this,
      std::placeholders::_1,
      INTR_STATUS.write_bit_mask
   );
   memory.register_write_callback(intr_state_handler, INTR_STATUS.offset);

   // INTR_ENABLE (0x4) - Interrupt enable control
   std::function<bool(uint32_t)> intr_enable_handler;
   intr_enable_handler = std::bind(
      &spi_controller_ip::handle_write_INTR_ENABLE,
      this,
      std::placeholders::_1,
      INTR_ENABLE.write_bit_mask
   );
   memory.register_write_callback(intr_enable_handler, INTR_ENABLE.offset);

   // INTR_TEST (0x8) - Interrupt testing
   std::function<bool(uint32_t)> intr_test_handler;
   intr_test_handler = std::bind(
      &spi_controller_ip::handle_write_INTR_TEST,
      this,
      std::placeholders::_1,
      INTR_TEST.write_bit_mask
   );
   memory.register_write_callback(intr_test_handler, INTR_TEST.offset);

   // CTRL (0xC) - System control (SPIEN, SW_RST, OUTPUT_EN, watermarks)
   std::function<bool(uint32_t)> control_handler;
   control_handler = std::bind(
      &spi_controller_ip::handle_write_CTRL,
      this,
      std::placeholders::_1,
      CTRL.write_bit_mask
   );
   memory.register_write_callback(control_handler, CTRL.offset);

   // CMD (0x1C) - Command register (pre-write callback for validation)
   std::function<bool(uint32_t)> cmd_write_handler;
   cmd_write_handler = std::bind(
      &spi_controller_ip::handle_write_CMD,
      this,
      std::placeholders::_1,
      CMD.write_bit_mask
   );
   memory.register_write_callback(cmd_write_handler, CMD.offset);

   // TXDATA (0x24) - Transmit FIFO (byte-enable aware)
   std::function<bool(uint32_t, uint8_t)> txdata_write_handler;
   txdata_write_handler = std::bind(
      &spi_controller_ip::handle_write_TXDATA,
      this,
      std::placeholders::_1,
      std::placeholders::_2,
      TXDATA.write_bit_mask
   );
   memory.register_write_callback_with_be(txdata_write_handler, TXDATA.offset);

   // ERROR_ENABLE (0x28) - Error interrupt masking
   std::function<bool(uint32_t)> error_enable_handler;
   error_enable_handler = std::bind(
      &spi_controller_ip::handle_write_ERROR_ENABLE,
      this,
      std::placeholders::_1,
      ERROR_ENABLE.write_bit_mask
   );
   memory.register_write_callback(error_enable_handler, ERROR_ENABLE.offset);

   // ERROR_STATUS (0x2C) - Error status (W1C semantics)
   std::function<bool(uint32_t)> error_status_write_handler;
   error_status_write_handler = std::bind(
      &spi_controller_ip::handle_write_ERROR_STATUS,
      this,
      std::placeholders::_1,
      ERROR_STATUS.write_bit_mask
   );
   memory.register_write_callback(error_status_write_handler, ERROR_STATUS.offset);

   // EVENT_ENABLE (0x30) - Event interrupt masking
   std::function<bool(uint32_t)> event_enable_handler;
   event_enable_handler = std::bind(
      &spi_controller_ip::handle_write_EVENT_ENABLE,
      this,
      std::placeholders::_1,
      EVENT_ENABLE.write_bit_mask
   );
   memory.register_write_callback(event_enable_handler, EVENT_ENABLE.offset);

   // CFG (0x14) - Per-device configuration with shadow array support
   std::function<bool(uint32_t)> configopts_write_handler;
   configopts_write_handler = std::bind(
      &spi_controller_ip::handle_write_CFG,
      this,
      std::placeholders::_1,
      CFG.write_bit_mask
   );
   memory.register_write_callback(configopts_write_handler, CFG.offset);

   // ===== Register Read Callbacks =====

   // STATUS (0x10) - Dynamic status computation
   std::function<bool(uint32_t&)> status_read_handler;
   status_read_handler = std::bind(
      &spi_controller_ip::handle_read_STATUS,
      this,
      std::placeholders::_1,
      STATUS.read_bit_mask
   );
   memory.register_read_callback(status_read_handler, STATUS.offset);

   // CFG (0x14) - Per-device configuration read from shadow array
   std::function<bool(uint32_t&)> configopts_read_handler;
   configopts_read_handler = std::bind(
      &spi_controller_ip::handle_read_CFG,
      this,
      std::placeholders::_1,
      CFG.read_bit_mask
   );
   memory.register_read_callback(configopts_read_handler, CFG.offset);

   // RXDATA (0x20) - Receive FIFO
   std::function<bool(uint32_t&)> rxdata_read_handler;
   rxdata_read_handler = std::bind(
      &spi_controller_ip::handle_read_RXDATA,
      this,
      std::placeholders::_1,
      RXDATA.read_bit_mask
   );
   memory.register_read_callback(rxdata_read_handler, RXDATA.offset);
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

    m_prev_ready = false;
    m_prev_active = false;
    m_prev_txempty = true;
    m_prev_rxfull = false;
    m_prev_txwm = false;
    m_prev_rxwm = false;

    // Clear INTR_TEST forced interrupt state
    m_intr_test_error_forced = false;
    m_intr_test_spi_event_forced = false;
}

/**
 * @brief Push data to TX FIFO
 */
bool spi_controller_ip::tx_fifo_push(uint32_t data)
{
    if (m_tx_fifo.size() >= get_tx_depth()) {
        return false;
    }
    m_tx_fifo.push_back(data);
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
        CSML_WARN(0, logger) << "[SPI_HOST/TIMING] Invalid clock period, using default 10ns (100 MHz)" << std::endl;
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
        CSML_ERROR(0, logger) << "[SPI_HOST/" << context << " ERROR] CSID (" << csid
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
            CSML_ERROR(0, logger) << "[SPI_HOST/TRANSACTION] clk_i port not bound or has invalid value (≤0 Hz). Cannot process SPI transactions without valid clock input" << std::endl;
            ERROR_STATUS.ACCESSINVAL = 1;
            update_error_interrupt_state();
            continue;
        }

        // Process all queued commands
        while (!m_command_queue.empty() && CTRL.SPIEN && CTRL.OUTPUT_EN) {
            set_fsm_state(fsm_state_e::ACTIVE);
            update_event_interrupt_state();

            spi_segment_t segment = m_command_queue.front().first;
            spi_config_t config = m_command_queue.front().second;

            // Process the transaction (returns false on error)
            bool transaction_success = process_single_transaction(segment, config);

            // SW_RST could have cleared the queue during transaction processing
            if (!m_command_queue.empty()) {
                m_command_queue.pop();
            }

            if (!transaction_success) {
                // Transaction failed - error already set, FSM already returned to IDLE
                // Stop processing remaining commands until error is cleared
                CSML_ERROR(0, logger) << "[SPI_HOST] Transaction failed, halting command processing" << std::endl;
                break;
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
            update_event_interrupt_state();
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
    // NOTE: RX FIFO pre-check now done synchronously in handle_write_CMD
    // for TLM LT compliance. No need for async check here since command
    // would have been rejected before reaching this point if insufficient space.
    // ===================================================================

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

       CSML_INFO(1, logger) << "[SPI_HOST] Pulling " << bytes_needed << " bytes from TX FIFO" << std::endl;

        while (byte_idx < bytes_needed) {
            uint32_t word;
            if (!tx_fifo_pop(word)) {
                // ========================================================================
                // This should NEVER happen due to TX FIFO pre-check in handle_write_CMD
                // The pre-check at CMD write ensures sufficient TX data is available.
                // If we reach here, it indicates an internal model bug.
                // ========================================================================
                CSML_ERROR(0, logger) << "[SPI_HOST/INTERNAL ERROR] "
                    << "TX FIFO underflow despite pre-check! This is a model bug. "
                    << "Segment requires " << bytes_needed << " bytes, but only " << byte_idx
                    << " bytes available. Setting ERROR_STATUS.CMDINVAL" << std::endl;

                ERROR_STATUS.CMDINVAL = 1;
                update_error_interrupt_state();

                // Abort transaction and return to IDLE state
                set_fsm_state(fsm_state_e::IDLE);
                update_event_interrupt_state();

                return false;
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
        update_event_interrupt_state();
    }

    // Calculate functional delay for this segment (loosely-timed modeling)
    double segment_delay = calculate_segment_delay(segment, config);

    // Initiate SPI transaction via spi_master port
    // In TLM loosely-timed model, start transaction immediately
    bool success = spi_master->spi_transaction(
        segment,
        config,
        tx_buffer,
        rx_buffer
    );

    // Wait AFTER transaction to model the time consumed
    // This properly represents loosely-timed behavior where we consume
    // the functional delay representing the transaction duration
    // Use time keeper for temporal decoupling
    m_time_keeper.inc(sc_time(segment_delay, SC_SEC));
    if (m_time_keeper.need_sync()) {
        m_time_keeper.sync();
    }

    CSML_INFO(1, logger) << "[SPI_HOST] SPI transaction completed after "
            << (segment_delay * 1e9) << " ns, success=" << success << std::endl;

    // For RX_ONLY and BIDIR, push received data into RX FIFO
    if (success && (segment.direction == spi_direction_e::RX_ONLY ||
                   segment.direction == spi_direction_e::BIDIR)) {

        uint32_t bytes_received = segment.len;
        uint32_t byte_idx = 0;

        CSML_INFO(2, logger) << "[SPI_HOST] Pushing " << bytes_received << " bytes into RX FIFO" << std::endl;

        while (byte_idx < bytes_received) {
            // Pack bytes into 32-bit word according to ByteOrder
            uint8_t bytes[4];
            uint32_t bytes_to_pack = std::min(4u, bytes_received - byte_idx);

            for (uint32_t i = 0; i < bytes_to_pack; i++) {
                bytes[i] = rx_buffer[byte_idx++];
            }

            uint32_t word = pack_word(bytes, bytes_to_pack);

            // ========================================================================
            // TLM LT FIX: Push to RX FIFO (pre-check guarantees space available)
            // In TLM LT, transactions are atomic and non-blocking.
            // The pre-check at function start ensures sufficient RX FIFO space.
            // If push fails here, it indicates an internal model bug.
            // ========================================================================
            if (!rx_fifo_push(word)) {
                // This should NEVER happen due to pre-check above
                CSML_ERROR(0, logger) << "[SPI_HOST/INTERNAL ERROR] "
                    << "RX FIFO push failed despite pre-check. This is a model bug!" << std::endl;
                ERROR_STATUS.OVERFLOW = 1;
                update_error_interrupt_state();
                set_fsm_state(fsm_state_e::IDLE);
                update_event_interrupt_state();
                return false;
            }
        }

        // Update FIFO status and check watermarks
        update_dma_trigger();
        update_event_interrupt_state();
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
        CSML_DEBUG(2, logger) << "[SPI_HOST] update_output_signals_method: Skipping during reset" << std::endl;
        return;
    }

    // Update error_irq
    bool error_irq_assert = (INTR_STATUS.error && INTR_ENABLE.error);
    error_irq.write(error_irq_assert);

    // Update spi_event_irq
    bool spi_event_irq_assert = (INTR_STATUS.spi_event && INTR_ENABLE.spi_event);
    spi_event_irq.write(spi_event_irq_assert);

    // Update dma_trigger
    uint32_t tx_depth = get_tx_fifo_depth();
    uint32_t rx_depth = get_rx_fifo_depth();
    uint32_t tx_watermark = CTRL.TX_WATERMARK;
    uint32_t rx_watermark = CTRL.RX_WATERMARK;
    bool tx_below_wm = (tx_depth < tx_watermark);
    bool rx_above_wm = (rx_depth > rx_watermark);  // FIX: Changed >= to > per RDL spec (EXCEEDS not MEETS)
    bool dma_trigger_assert = tx_below_wm || rx_above_wm;
    dma_trigger.write(dma_trigger_assert);
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
 * @brief Check if any enabled error is present and update INTR_STATUS.error
 */
void spi_controller_ip::update_error_interrupt_state()
{
    uint32_t error_status = ERROR_STATUS;
    uint32_t error_enable = ERROR_ENABLE;

    // Set INTR_STATUS.error if any enabled error is present
    bool has_enabled_error = ((error_status & error_enable) != 0);

    if (has_enabled_error && !INTR_STATUS.error) {
        INTR_STATUS.error = 1;
    }

    update_error_irq();
}

/**
 * @brief Notify to update DMA trigger output
 */
void spi_controller_ip::update_dma_trigger()
{
    m_update_signals_event.notify();
}

/**
 * @brief Update event interrupt state based on STATUS changes
 */
void spi_controller_ip::update_event_interrupt_state()
{
    uint32_t event_enable = EVENT_ENABLE;
    bool event_occurred = false;

    // Check for IDLE event (ACTIVE goes low)
    bool curr_active = (m_fsm_state == fsm_state_e::ACTIVE);
    if (m_prev_active && !curr_active && (event_enable & (1 << 20))) {
        event_occurred = true;
    }
    m_prev_active = curr_active;

    // Check for READY event (READY goes high)
    uint32_t error_status = ERROR_STATUS;
    bool has_errors = (error_status != 0);
    bool fsm_ready = (m_fsm_state == fsm_state_e::IDLE ||
                      m_fsm_state == fsm_state_e::IDLE_CSB_ACTIVE);
    bool curr_ready = (!has_errors && fsm_ready);

    if (!m_prev_ready && curr_ready && (event_enable & (1 << 16))) {
        event_occurred = true;
    }
    m_prev_ready = curr_ready;

    // Check for TXEMPTY event
    bool curr_txempty = is_tx_fifo_empty();
    if (!m_prev_txempty && curr_txempty && (event_enable & (1 << 4))) {
        event_occurred = true;
    }
    m_prev_txempty = curr_txempty;

    // Check for RXFULL event
    bool curr_rxfull = is_rx_fifo_full();
    if (!m_prev_rxfull && curr_rxfull && (event_enable & (1 << 0))) {
        event_occurred = true;
    }
    m_prev_rxfull = curr_rxfull;

    // Check for TXWM event (TX FIFO below watermark) - edge-triggered
    uint32_t tx_depth = get_tx_fifo_depth();
    uint32_t tx_watermark = CTRL.TX_WATERMARK;
    bool curr_txwm = (tx_depth < tx_watermark);
    if (!m_prev_txwm && curr_txwm && (event_enable & (1 << 12))) {
        event_occurred = true;
    }
    m_prev_txwm = curr_txwm;

    // Check for RXWM event (RX FIFO exceeds watermark) - edge-triggered
    uint32_t rx_depth = get_rx_fifo_depth();
    uint32_t rx_watermark = CTRL.RX_WATERMARK;
    bool curr_rxwm = (rx_depth > rx_watermark);
    if (!m_prev_rxwm && curr_rxwm && (event_enable & (1 << 8))) {
        event_occurred = true;
    }
    m_prev_rxwm = curr_rxwm;

    // Update INTR_STATUS.spi_event if any event occurred
    if (event_occurred && !INTR_STATUS.spi_event) {
        INTR_STATUS.spi_event = 1;
    }

    // Update the actual interrupt port
    update_spi_event_irq();
}

/**
 * @brief INTR_STATUS register write callback with W1C semantics
 */
bool spi_controller_ip::handle_write_INTR_STATUS(uint32_t value, uint32_t mask)
{
    // Read current INTR_STATUS value
    uint32_t current = INTR_STATUS;

    // Apply write bitmask before W1C logic
    uint32_t masked_value = value & mask;

    // W1C logic: Clear bits where write_value has 1s
    uint32_t new_value = current & ~masked_value;

    // Write back the new value
    INTR_STATUS = new_value;

    // Update both interrupt ports
    update_error_irq();
    update_spi_event_irq();

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

    // Update both interrupt ports based on new enable state
    update_error_irq();
    update_spi_event_irq();

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

    // Handle error bit: write 1 forces, write 0 releases
    if (INTR_TEST.error) {
        // Force error interrupt
        INTR_STATUS.error = 1;
        m_intr_test_error_forced = true;
        CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] Error interrupt forced" << std::endl;
    } else {
        // Release test-forced interrupt
        m_intr_test_error_forced = false;
        // Only clear INTR_STATUS.error if no real errors are present
        uint32_t error_status = ERROR_STATUS;
        uint32_t error_enable = ERROR_ENABLE;
        bool has_real_error = ((error_status & error_enable) != 0);
        if (!has_real_error) {
            INTR_STATUS.error = 0;
            CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] Error interrupt released (no real errors)" << std::endl;
        } else {
            CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] Error interrupt test released but real errors remain active" << std::endl;
        }
    }

    // Handle spi_event bit: write 1 forces, write 0 releases
    if (INTR_TEST.spi_event) {
        // Force spi_event interrupt
        INTR_STATUS.spi_event = 1;
        m_intr_test_spi_event_forced = true;
        CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] SPI_EVENT interrupt forced" << std::endl;
    } else {
        // Release test-forced interrupt
        m_intr_test_spi_event_forced = false;
        // Only clear INTR_STATUS.spi_event if no real events are present
        uint32_t event_enable = EVENT_ENABLE;
        bool has_real_event = false;

        // Check all possible event sources
        uint32_t tx_depth = get_tx_fifo_depth();
        uint32_t rx_depth = get_rx_fifo_depth();
        uint32_t tx_watermark = CTRL.TX_WATERMARK;
        uint32_t rx_watermark = CTRL.RX_WATERMARK;

        // Check RXFULL event (bit 0)
        if ((event_enable & (1 << 0)) && is_rx_fifo_full()) {
            has_real_event = true;
        }
        // Check TXEMPTY event (bit 4)
        if ((event_enable & (1 << 4)) && is_tx_fifo_empty()) {
            has_real_event = true;
        }
        // Check RXWM event (bit 8)
        if ((event_enable & (1 << 8)) && (rx_depth > rx_watermark)) {
            has_real_event = true;
        }
        // Check TXWM event (bit 12)
        if ((event_enable & (1 << 12)) && (tx_depth < tx_watermark)) {
            has_real_event = true;
        }
        // Check READY event (bit 16)
        if ((event_enable & (1 << 16)) && (m_fsm_state == fsm_state_e::IDLE)) {
            has_real_event = true;
        }
        // Check IDLE event (bit 20)
        if ((event_enable & (1 << 20)) && (m_fsm_state == fsm_state_e::IDLE) &&
            is_tx_fifo_empty() && is_cmd_queue_full() == false) {
            has_real_event = true;
        }

        if (!has_real_event) {
            INTR_STATUS.spi_event = 0;
            CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] SPI_EVENT interrupt released (no real events)" << std::endl;
        } else {
            CSML_DEBUG(2, logger) << "[SPI_HOST/INTR_TEST] SPI_EVENT interrupt test released but real events remain active" << std::endl;
        }
    }

    // Update interrupt ports
    update_error_irq();
    update_spi_event_irq();

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

    // CRITICAL FIX: When EVENT_ENABLE bits are set, check if conditions are already met
    // and set INTR_STATUS.spi_event immediately (not waiting for edge transition)
    // This handles the case where watermarks/events are already true when enabled

    bool event_already_met = false;

    // Check TXWM condition: TX FIFO depth < TX_WATERMARK
    if ((new_value & (1 << 12))) {
        uint32_t tx_depth = get_tx_fifo_depth();
        uint32_t tx_watermark = CTRL.TX_WATERMARK;
        if (tx_depth < tx_watermark) {
            event_already_met = true;
        }
    }

    // Check RXWM condition: RX FIFO depth > RX_WATERMARK
    if ((new_value & (1 << 8))) {
        uint32_t rx_depth = get_rx_fifo_depth();
        uint32_t rx_watermark = CTRL.RX_WATERMARK;
        if (rx_depth > rx_watermark) {
            event_already_met = true;
        }
    }

    // Check TXEMPTY condition
    if ((new_value & (1 << 4))) {
        if (is_tx_fifo_empty()) {
            event_already_met = true;
        }
    }

    // Check RXFULL condition
    if ((new_value & (1 << 0))) {
        if (is_rx_fifo_full()) {
            event_already_met = true;
        }
    }

    // Check IDLE condition (ACTIVE goes low)
    if ((new_value & (1 << 20))) {
        if (m_fsm_state != fsm_state_e::ACTIVE) {
            event_already_met = true;
        }
    }

    // Check READY condition (READY goes high)
    if ((new_value & (1 << 16))) {
        uint32_t error_status = ERROR_STATUS;
        bool has_errors = (error_status != 0);
        bool fsm_ready = (m_fsm_state == fsm_state_e::IDLE ||
                          m_fsm_state == fsm_state_e::IDLE_CSB_ACTIVE);
        if (!has_errors && fsm_ready) {
            event_already_met = true;
        }
    }

    // If any enabled event condition is already met, set INTR_STATUS.spi_event immediately
    if (event_already_met && !INTR_STATUS.spi_event) {
        INTR_STATUS.spi_event = 1;
    }

    // Also call update_event_interrupt_state to handle edge detection going forward
    update_event_interrupt_state();

    return true;
}

/**
 * @brief CTRL register write callback
 */
bool spi_controller_ip::handle_write_CTRL(uint32_t value, uint32_t mask)
{
    // Apply write bitmask and update register
    uint32_t current = CTRL;
    uint32_t new_value = (value & mask) | (current & ~mask);
    CTRL = new_value;

    // Extract control fields
    CSML_INFO(2, logger) << "  SPIEN: " << (uint32_t)CTRL.SPIEN << std::endl
                         << "  SW_RST: " << (uint32_t)CTRL.SW_RST << std::endl
                         << "  OUTPUT_EN: " << (uint32_t)CTRL.OUTPUT_EN << std::endl
                         << "  TX_WATERMARK: " << (int)CTRL.TX_WATERMARK << std::endl
                         << "  RX_WATERMARK: " << (int)CTRL.RX_WATERMARK << std::endl;

    // Track previous SPIEN state to detect 0->1 transitions
    bool prev_spien = (current >> 31) & 0x1;
    bool new_spien = CTRL.SPIEN;

    // Handle SW_RST first (highest priority)
    if (CTRL.SW_RST) {
        CSML_INFO(2, logger) << "  [RESET] Software reset triggered!" << std::endl;

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
        INTR_STATUS = 0;

        // Update interrupt ports
        update_error_irq();
        update_spi_event_irq();

        // Update DMA trigger
        update_dma_trigger();

        // CRITICAL FIX: Wake up any stalled transactions
        // This allows them to detect SW_RST and abort cleanly
        m_rx_space_available_event.notify();

         // Auto-clear SW_RST
        CTRL.SW_RST = 0;

        CSML_INFO(2, logger) << "  [RESET] Software reset complete" << std::endl;
    }

    // Handle SPIEN (enable/disable FSM operation)
    if (new_spien) {
        CSML_INFO(2, logger) << "  [CTRL] SPI Host enabled" << std::endl;
        // If SPIEN changed from 0 to 1, and there are commands queued, wake up transaction thread
        if (!prev_spien && !m_command_queue.empty()) {
            m_transaction_event.notify();
        }
        else {
            CSML_INFO(2, logger) << "  [CTRL] SPI Host disabled" << std::endl;
        }
        // FSM should not process new transactions
        // Ongoing transactions may continue or be aborted depending on implementation
    }

    // Watermark changes affect interrupt and DMA trigger generation
    update_dma_trigger();
    update_event_interrupt_state();

    return true;
}

/**
 * @brief STATUS register read callback with dynamic status computation
 */
bool spi_controller_ip::handle_read_STATUS(uint32_t& value, uint32_t mask)
{
    // Check if any error bits are set in ERROR_STATUS
    uint32_t error_status = ERROR_STATUS;
    bool has_errors = (error_status != 0);

    // Set READY bit per datasheet
    if (!has_errors && !is_cmd_queue_full()) {
        STATUS.READY = 1;
    } else {
        STATUS.READY = 0;
    }

    // Set ACTIVE bit - high while FSM is processing, or while commands are queued and
    // the controller is enabled (command queued but transaction thread not yet scheduled).
    // This prevents wait_for_idle from returning before a freshly-written CMD is processed.
    bool cmd_pending = (!m_command_queue.empty() && CTRL.SPIEN && CTRL.OUTPUT_EN);
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
    uint32_t tx_watermark = CTRL.TX_WATERMARK;
    uint32_t rx_watermark = CTRL.RX_WATERMARK;
    STATUS.TXWM = (STATUS.TXQD < tx_watermark) ? 1 : 0;
    STATUS.RXWM = (STATUS.RXQD > rx_watermark) ? 1 : 0;

    // Return the computed status value with read bitmask applied
    value = STATUS & mask;

    return true;
}

/**
 * @brief CMD register write callback with validation and error detection
 */
bool spi_controller_ip::handle_write_CMD(uint32_t value, uint32_t mask)
{
   CSML_INFO(1, logger) << "[SPI_HOST] CMD register pre-write callback triggered" << std::endl;
   CSML_DEBUG(2, logger) << "  Value to write: 0x" << std::hex << value << std::dec << std::endl;

    // Apply write bitmask to get the actual value being written
    uint32_t masked_value = value & mask;

    // Extract CMD fields per RDL spec (spi_controller.rdl):
    // [13:12] DIRECTION, [11:10] SPEED, [9] CSAAT, [8:0] LEN
    uint32_t cmd_len = (masked_value) & 0x1FF;
    bool     cmd_csaat = (masked_value >> 9) & 0x1;
    uint8_t  cmd_speed = (masked_value >> 10) & 0x3;
    uint8_t  cmd_direction = (masked_value >> 12) & 0x3;

    CSML_DEBUG(2, logger) << "  LEN: " << cmd_len << " (actual bytes: " << (cmd_len + 1) << ")" << std::endl;
    CSML_DEBUG(2, logger) << "  CSAAT: " << cmd_csaat << std::endl;
    CSML_DEBUG(2, logger) << "  SPEED: " << (int)cmd_speed << " (0=Std, 1=Dual, 2=Quad)" << std::endl;
    CSML_DEBUG(2, logger) << "  DIRECTION: " << (int)cmd_direction << " (0=Dummy, 1=Rx, 2=Tx, 3=Bidir)" << std::endl;

    // ========================================================================
    // Validation 0: Check if command queue is full
    // ========================================================================
    if (is_cmd_queue_full()) {
        CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] Command FIFO full (depth=" << get_cmd_queue_depth()
                  << "/" << get_cmd_depth() << "). Cannot accept new command segment. Setting ERROR_STATUS.CMDBUSY" << std::endl;

        ERROR_STATUS.CMDBUSY = 1;
        update_error_interrupt_state();

        return false;
    }

    // Validation 1: Check if ready to accept commands
    // Per datasheet: "CMDBUSY: Indicates a write to CMD when STATUS.READY = 0"
    // Per datasheet: "STATUS.READY indicates that there is room in the command FIFO"
    // This validation must match the READY logic in handle_read_STATUS()
    uint32_t error_status_check = ERROR_STATUS;
    bool has_errors = (error_status_check != 0);

    // NOTE: Validation 0 above already checked is_cmd_queue_full(), so this check
    // for errors is technically the only remaining condition. However, we keep this
    // validation for completeness and to match the STATUS.READY semantics exactly.
    if (has_errors) {
        CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] CMD written when STATUS.READY=0 (errors present). "
                  << "ERROR_STATUS=0x" << std::hex << error_status_check << std::dec
                  << ". Setting ERROR_STATUS.CMDBUSY" << std::endl;

        // Set ERROR_STATUS.CMDBUSY
        ERROR_STATUS.CMDBUSY = 1;

        // Update error interrupt state and port
        update_error_interrupt_state();

        return false;
    }

    // Validation 2: Check CSID < NumCS
    uint32_t csid_val = CSID;

    if (!validate_csid(csid_val, "CMD")) {
        return false;
    }

    // Validation 3: Check valid SPEED (0-2 valid, 3 is reserved)
    if (cmd_speed > 2) {
        CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] Invalid SPEED value: " << (int)cmd_speed
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
        CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] Bidirectional mode only supported with Standard SPI. "
                  << "DIRECTION=3 (Bidir) with SPEED=" << (int)cmd_speed
                  << " (not Standard). Setting ERROR_STATUS.CMDINVAL" << std::endl;

        // Set ERROR_STATUS.CMDINVAL
        ERROR_STATUS.CMDINVAL = 1;

        // Update error interrupt state and port
        update_error_interrupt_state();

        // DO NOT write to CMD register - reject invalid value
        return false;
    }

    // Validation 5: TLM LT RX FIFO Pre-check (synchronous overflow detection)
    // In TLM LT atomic modeling, we must verify RX FIFO space BEFORE accepting the command.
    // This check must be done synchronously in CMD write handler, not asynchronously in transaction thread.
    if (cmd_direction == 1 || cmd_direction == 3) {  // RX_ONLY or BIDIR
        uint32_t bytes_to_receive = cmd_len + 1;  // LEN is 0-based, so add 1
        uint32_t words_needed = (bytes_to_receive + 3) / 4;  // Round up to 32-bit words
        uint32_t rx_space_available = get_rx_depth() - get_rx_fifo_depth();

        if (words_needed > rx_space_available) {
            CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] Insufficient RX FIFO space for transaction. "
                << "Command requires " << words_needed << " words (" << bytes_to_receive << " bytes), "
                << "but only " << rx_space_available << " words available in RX FIFO (capacity: "
                << get_rx_depth() << " words). "
                << "In TLM LT atomic model, software must ensure sufficient RX space before CMD write. "
                << "Setting ERROR_STATUS.OVERFLOW" << std::endl;

            ERROR_STATUS.OVERFLOW = 1;
            update_error_interrupt_state();

            return false;  // Reject command
        }

        CSML_INFO(2, logger) << "[SPI_HOST/CMD] RX FIFO pre-check passed: "
            << words_needed << " words needed, "
            << rx_space_available << " available" << std::endl;
    }

    // Validation 6: TLM LT TX FIFO Pre-check (synchronous underflow detection)
    // In TLM LT atomic modeling, we must verify TX FIFO data BEFORE accepting the command.
    // This check must be done synchronously in CMD write handler, not asynchronously in transaction thread.
    // This prevents mid-transaction aborts and ensures full atomicity.
    if (cmd_direction == 2 || cmd_direction == 3) {  // TX_ONLY or BIDIR
        uint32_t bytes_to_transmit = cmd_len + 1;  // LEN is 0-based, so add 1
        uint32_t words_needed = (bytes_to_transmit + 3) / 4;  // Round up to 32-bit words
        uint32_t tx_data_available = get_tx_fifo_depth();

        if (words_needed > tx_data_available) {
            CSML_ERROR(0, logger) << "[SPI_HOST/CMD ERROR] Insufficient TX FIFO data for transaction. "
                << "Command requires " << words_needed << " words (" << bytes_to_transmit << " bytes), "
                << "but only " << tx_data_available << " words available in TX FIFO. "
                << "In TLM LT atomic model, software must pre-load sufficient TX data before CMD write. "
                << "Setting ERROR_STATUS.UNDERFLOW" << std::endl;

            ERROR_STATUS.UNDERFLOW = 1;
            update_error_interrupt_state();

            return false;  // Reject command
        }

        CSML_INFO(2, logger) << "[SPI_HOST/CMD] TX FIFO pre-check passed: "
            << words_needed << " words needed, "
            << tx_data_available << " available" << std::endl;
    }

    // Write the validated value to CMD register
    // This is the ONLY path that writes to CMD - validation failures above don't write
    CMD = masked_value;

    // Build segment descriptor from validated CMD fields
    m_current_segment.len = cmd_len + 1;
    m_current_segment.direction = static_cast<spi_direction_e>(cmd_direction);
    m_current_segment.speed = static_cast<spi_speed_e>(cmd_speed);
    m_current_segment.csaat = cmd_csaat;
    m_current_segment.csid = static_cast<uint8_t>(csid_val);

    // Capture current CFG into shadow array for this CSID
    // This allows per-device timing configuration
    uint8_t csid = static_cast<uint8_t>(csid_val);
    if (csid < m_configopts_array.size()) {
        m_configopts_array[csid] = CFG;
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

    CSML_INFO(2, logger) << "  [SUCCESS] SPI transaction queued and thread notified" << std::endl;

    return true;
}

/**
 * @brief TXDATA register write callback with byte-enable support
 */
bool spi_controller_ip::handle_write_TXDATA(uint32_t value, uint8_t byte_enable, uint32_t mask)
{
    // ========================================================================
    // STRICT byte-enable validation per SPI Controller specification
    // Per CLAUDE.md: "Valid patterns are 0x1, 0x3, 0xF (contiguous from byte 0).
    //                 Invalid patterns include 0x2, 0x4, 0x5, 0x6, 0x7, 0x8,
    //                 0x9, 0xA, 0xB, 0xC, 0xD, 0xE"
    // ONLY allow contiguous byte-enables starting from byte 0:
    //   - 0x1 = 0b0001 = byte 0 only
    //   - 0x3 = 0b0011 = bytes 0-1 (half-word)
    //   - 0xF = 0b1111 = bytes 0-3 (full word)
    // ========================================================================
    if (byte_enable != 0x1 && byte_enable != 0x3 && byte_enable != 0xF) {
        CSML_ERROR(0, logger) << "[SPI_HOST/TXDATA ERROR] Invalid byte-enable pattern: 0x"
                              << std::hex << (int)byte_enable << std::dec
                              << ". Valid patterns: 0x1 (byte 0), 0x3 (bytes 0-1), 0xF (all bytes). "
                              << "Byte-enables must be contiguous starting from byte 0. "
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
        CSML_ERROR(0, logger) << "[SPI_HOST/TXDATA ERROR] TX FIFO overflow - FIFO is full. Setting ERROR_STATUS.OVERFLOW" << std::endl;
        ERROR_STATUS.OVERFLOW = 1;
        update_error_interrupt_state();
        return false;
    }

    // Push 32-bit word to TX FIFO
    bool success = tx_fifo_push(masked_value);

    if (success) {
        // Update DMA trigger based on watermark
        update_dma_trigger();

        // Update event interrupt state (TXEMPTY, TXWM events)
        update_event_interrupt_state();
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
        CSML_WARN(1, logger) << "  [WARNING] RX FIFO underflow - FIFO is empty, returning 0" << std::endl;
        value = 0;
        ERROR_STATUS.UNDERFLOW = 1;
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
        update_event_interrupt_state();
    } else {
        value = 0;
        CSML_WARN(1, logger) << "  Failed to pop from RX FIFO, returning 0" << std::endl;
    }

    return true;
}

/**
 * @brief CFG register write callback with per-device shadow array
 */
bool spi_controller_ip::handle_write_CFG(uint32_t value, uint32_t mask)
{
    // Apply write bitmask
    uint32_t masked_value = value & mask;

    // Get current CSID value
    uint32_t csid = CSID;

    // Check CSID range (without setting error - CFG access doesn't trigger CSIDINVAL)
    // Per datasheet: CSIDINVAL only set on CMD write, not CFG access
    if (csid >= get_num_cs()) {
        CSML_WARN(1, logger) << "[SPI_HOST/CFG_WRITE] Invalid CSID (" << csid
                  << ") >= NumCS (" << get_num_cs() << "). Ignoring write." << std::endl;
        return false;  // Silently reject, don't set error
    }

    // Store masked value into shadow array for this CSID
    m_configopts_array[csid] = masked_value;

    // Also update the actual register storage for readback consistency
    CFG = masked_value;

    return true;
}

/**
 * @brief CFG register read callback from per-device shadow array
 */
bool spi_controller_ip::handle_read_CFG(uint32_t& value, uint32_t mask)
{
    // Get current CSID value
    uint32_t csid = CSID;

    // Check CSID range (without setting error - CFG access doesn't trigger CSIDINVAL)
    // Per datasheet: CSIDINVAL only set on CMD write, not CFG access
    if (csid >= get_num_cs()) {
        CSML_WARN(1, logger) << "[SPI_HOST/CFG_READ] Invalid CSID (" << csid
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
