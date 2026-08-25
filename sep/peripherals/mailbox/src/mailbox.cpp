// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox.cpp
 * @brief Implementation of mailbox IP model methods
 *
 * Contains method implementations for the mailbox_ip class.
 * FIFOs are implemented using SystemC's sc_fifo<uint64_t> primitive channel.
 * Non-blocking nb_write()/nb_read() are used throughout since callbacks
 * are invoked from b_transport context (not SC_THREAD).
 */

#include "mailbox.h"

/**
 * @brief Constructor implementation for mailbox IP model
 *
 * Initializer list order matches member declaration order in mailbox.h:
 * b0 → b1 → fifo_0_to_1 → fifo_1_to_0 → logger
 *
 * Configuration is hardcoded internally:
 *   - Memory size per port: 0x50 (80 bytes = 10 registers × 8 bytes)
 *   - FIFO depth: 8 entries (m_mailbox_depth static constexpr)
 *   - Interrupt polarity: active-high (m_irq_act_high static constexpr)
 *
 * After construction, registers all functional callbacks on b0.memory
 * and b1.memory. CSML auto-registers default R/W handlers during csml_reg
 * construction; register_*_callback() replaces them (erase-then-insert).
 */
mailbox_ip::mailbox_ip(sc_module_name n, int log_verbosity)
    : sc_module(n)
    , b0("b0", 0x50)
    , b1("b1", 0x50)
    , socket0("socket0")
    , socket1("socket1")
    , logger()
    , verbosity("verbosity", log_verbosity)
{
    socket0.register_b_transport(this, &mailbox_ip::b_transport_port0);
    socket1.register_b_transport(this, &mailbox_ip::b_transport_port1);

    m_access_error[0] = false;
    m_access_error[1] = false;

    // =====================================================================
    // CSML Logger Configuration
    // =====================================================================
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat(
        "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    CSML_INFO(2, logger) << "Mailbox IP instantiated with:"
                         << " MailboxDepth=" << m_mailbox_depth
                         << " IrqActHigh=" << (m_irq_act_high ? "true" : "false")
                         << " MemorySize=0x50";

    // =====================================================================
    // FUNC_005: Initialize Error Flag Shadow State (Per-Port)
    // =====================================================================
    // Both flags reset to 0 (axil_mailbox.rdl: read_error = 0x0, write_error =
    // 0x0). A flag is raised only by an actual failed access at runtime.
    m_error_flag_read_error[0] = false;
    m_error_flag_write_error[0] = false;
    m_error_flag_read_error[1] = false;
    m_error_flag_write_error[1] = false;

    // =====================================================================
    // FUNC_006: Initialize Interrupt System Shadow State (Per-Port)
    // =====================================================================
    m_irqs_wtirq[0] = false;  m_irqs_wtirq[1] = false;
    m_irqs_rtirq[0] = false;  m_irqs_rtirq[1] = false;
    m_irqs_eirq[0]  = false;  m_irqs_eirq[1]  = false;

    m_wirqt_threshold[0] = 0; m_wirqt_threshold[1] = 0;
    m_rirqt_threshold[0] = 0; m_rirqt_threshold[1] = 0;

    m_irqen_wtirq[0] = false; m_irqen_wtirq[1] = false;
    m_irqen_rtirq[0] = false; m_irqen_rtirq[1] = false;
    m_irqen_eirq[0]  = false; m_irqen_eirq[1]  = false;


    // =====================================================================
    // FUNC_003: Register Port 0 (b0) Callbacks
    // =====================================================================
    // A write-to-full FIFO raises m_access_error, which bus_access() turns into
    // an error response to match the RTL's SLVERR.
    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_WRITE_DATA(0, value, b0.WRITE_DATA.write_bit_mask); },
        b0.WRITE_DATA.offset);

    // A read-from-empty FIFO raises m_access_error, which bus_access() turns
    // into an error response plus the 0xFEEDDEAD sentinel, matching RTL.
    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_READ_DATA(0, value, b0.READ_DATA.read_bit_mask); },
        b0.READ_DATA.offset);

    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_STATUS(0, value, b0.STATUS.read_bit_mask); },
        b0.STATUS.offset);

    // =====================================================================
    // FUNC_005: Register Port 0 ERROR_FLAGS Callback
    // =====================================================================
    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_ERROR_FLAGS(0, value, b0.ERROR_FLAGS.read_bit_mask); },
        b0.ERROR_FLAGS.offset);

    // =====================================================================
    // FUNC_006: Register Port 0 Interrupt System Callbacks
    // =====================================================================
    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_WIRQT(0, value, b0.WIRQT.write_bit_mask); },
        b0.WIRQT.offset);

    // WIRQT read callback (FUNC_002): required so reads return the saturated shadow-state
    // threshold, not the CSML backing-store default.  Detailed design specifies:
    // "Subsequent reads return the saturated value, not the originally written value."
    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_WIRQT(0, value, b0.WIRQT.read_bit_mask); },
        b0.WIRQT.offset);

    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_RIRQT(0, value, b0.RIRQT.write_bit_mask); },
        b0.RIRQT.offset);

    // RIRQT read callback (FUNC_002): required so reads return the saturated shadow-state
    // threshold, not the CSML backing-store default.
    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_RIRQT(0, value, b0.RIRQT.read_bit_mask); },
        b0.RIRQT.offset);

    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQS(0, value, b0.IRQS.read_bit_mask); },
        b0.IRQS.offset);

    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_IRQS(0, value, b0.IRQS.write_bit_mask); },
        b0.IRQS.offset);

    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_IRQEN(0, value, b0.IRQEN.write_bit_mask); },
        b0.IRQEN.offset);

    // IRQEN read callback: required because write callback updates shadow state only
    // (not CSML backing store). Without this, reads return stale reset value 0x0.
    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQEN(0, value, b0.IRQEN.read_bit_mask); },
        b0.IRQEN.offset);

    b0.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQP(0, value, b0.IRQP.read_bit_mask); },
        b0.IRQP.offset);

    // =====================================================================
    // FUNC_007: Register Port 0 CTRL Callback
    // =====================================================================
    b0.memory.register_write_callback(
        [this](DT value) { return this->handle_write_CTRL(0, value, b0.CTRL.write_bit_mask); },
        b0.CTRL.offset);

    // =====================================================================
    // FUNC_003: Register Port 1 (b1) Callbacks — symmetric to Port 0
    // =====================================================================
    // A write-to-full FIFO raises m_access_error, which bus_access() turns into
    // an error response to match the RTL's SLVERR.
    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_WRITE_DATA(1, value, b1.WRITE_DATA.write_bit_mask); },
        b1.WRITE_DATA.offset);

    // A read-from-empty FIFO raises m_access_error, which bus_access() turns
    // into an error response plus the 0xFEEDDEAD sentinel, matching RTL.
    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_READ_DATA(1, value, b1.READ_DATA.read_bit_mask); },
        b1.READ_DATA.offset);

    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_STATUS(1, value, b1.STATUS.read_bit_mask); },
        b1.STATUS.offset);

    // =====================================================================
    // FUNC_005: Register Port 1 ERROR_FLAGS Callback
    // =====================================================================
    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_ERROR_FLAGS(1, value, b1.ERROR_FLAGS.read_bit_mask); },
        b1.ERROR_FLAGS.offset);

    // =====================================================================
    // FUNC_006: Register Port 1 Interrupt System Callbacks
    // =====================================================================
    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_WIRQT(1, value, b1.WIRQT.write_bit_mask); },
        b1.WIRQT.offset);

    // WIRQT read callback (FUNC_002): symmetric to Port 0 — returns saturated shadow threshold
    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_WIRQT(1, value, b1.WIRQT.read_bit_mask); },
        b1.WIRQT.offset);

    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_RIRQT(1, value, b1.RIRQT.write_bit_mask); },
        b1.RIRQT.offset);

    // RIRQT read callback (FUNC_002): symmetric to Port 0 — returns saturated shadow threshold
    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_RIRQT(1, value, b1.RIRQT.read_bit_mask); },
        b1.RIRQT.offset);

    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQS(1, value, b1.IRQS.read_bit_mask); },
        b1.IRQS.offset);

    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_IRQS(1, value, b1.IRQS.write_bit_mask); },
        b1.IRQS.offset);

    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_IRQEN(1, value, b1.IRQEN.write_bit_mask); },
        b1.IRQEN.offset);

    // IRQEN read callback: symmetric to Port 0
    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQEN(1, value, b1.IRQEN.read_bit_mask); },
        b1.IRQEN.offset);

    b1.memory.register_read_callback(
        [this](DT& value) { return this->handle_read_IRQP(1, value, b1.IRQP.read_bit_mask); },
        b1.IRQP.offset);

    // =====================================================================
    // FUNC_007: Register Port 1 CTRL Callback
    // =====================================================================
    b1.memory.register_write_callback(
        [this](DT value) { return this->handle_write_CTRL(1, value, b1.CTRL.write_bit_mask); },
        b1.CTRL.offset);

    // =====================================================================
    // Reset Handler Registration (active-low asynchronous reset)
    // =====================================================================
    SC_METHOD(handle_reset);
    sensitive << rst_ni.neg();
    dont_initialize();

    // =====================================================================
    // Interrupt Driver Registration (Single-Writer Pattern)
    // =====================================================================
    SC_METHOD(irq_driver);
    sensitive << m_irq_update_event[0] << m_irq_update_event[1];
    dont_initialize();
}

// =============================================================================
// Bus access rules
// =============================================================================

void mailbox_ip::store_response_data(tlm::tlm_generic_payload& trans, uint64_t value)
{
    unsigned char* ptr = trans.get_data_ptr();
    const unsigned int len = trans.get_data_length();
    if (ptr == nullptr || len == 0) {
        return;
    }

    // A narrowed access addresses one half of the 64-bit register, so shift the
    // register value down to the byte lane the transaction starts at.
    const unsigned int shift = static_cast<unsigned int>(trans.get_address() & 0x7ULL) * 8u;
    const uint64_t lane = (shift >= 64u) ? 0ULL : (value >> shift);

    for (unsigned int i = 0; i < len; ++i) {
        ptr[i] = (i < 8u) ? static_cast<unsigned char>((lane >> (i * 8u)) & 0xFFu) : 0u;
    }
}

/**
 * @brief Apply the AXI-Lite decode and permission rules, then run the access
 *
 * csml_memory always answers TLM_OK_RESPONSE and silently ignores an illegal
 * access, whereas the RTL slave answers SLVERR for an out-of-range offset, a
 * write to a read-only register, a read of the write-only CTRL register, and
 * for FIFO overflow/underflow. Those rules are enforced here so that firmware
 * sees the same bus behaviour as on hardware.
 */
void mailbox_ip::bus_access(unsigned int port, tlm::tlm_generic_payload& trans,
                            sc_core::sc_time& delay)
{
    const uint64_t addr = trans.get_address();
    const uint64_t reg = addr & ~0x7ULL;
    const bool is_write = (trans.get_command() == tlm::TLM_WRITE_COMMAND);

    mailbox_base& regs = (port == 0) ? b0 : b1;

    // Offsets beyond CTRL are unmapped. RTL decodes nothing above 0x4F in the
    // 0x800 port aperture and answers SLVERR — an error from a target that
    // exists, so a generic error rather than an address error, which the SEP
    // bus treats as a missing target and turns into a fatal.
    if (reg > 0x48ULL) {
        CSML_WARN(1, logger) << "[MBX] Port " << port << " access to unmapped offset 0x"
                             << std::hex << addr << std::dec;
        store_response_data(trans, 0);
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // Reading WRITE_DATA is the one write-only access RTL allows: it completes
    // with OKAY and returns a fixed sentinel rather than erroring.
    if (!is_write && reg == 0x00ULL) {
        store_response_data(trans, SENTINEL_WRITE_DATA_READ);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    const bool read_only = (reg == 0x08ULL) || (reg == 0x10ULL) ||
                           (reg == 0x18ULL) || (reg == 0x40ULL);
    const bool write_only = (reg == 0x48ULL);

    if (is_write && read_only) {
        CSML_WARN(1, logger) << "[MBX] Port " << port << " write to read-only offset 0x"
                             << std::hex << reg << std::dec;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (!is_write && write_only) {
        CSML_WARN(1, logger) << "[MBX] Port " << port << " read of write-only offset 0x"
                             << std::hex << reg << std::dec;
        store_response_data(trans, 0);
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    m_access_error[port] = false;
    regs.memory.b_transport(trans, delay);

    if (m_access_error[port]) {
        m_access_error[port] = false;
        // Underflow returns a recognisable pattern; overflow has no read data.
        if (!is_write) {
            store_response_data(trans, SENTINEL_READ_EMPTY);
        }
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
}

/**
 * @brief Reset handler implementation (FUNC_001: System Reset and Initialization)
 *
 * On rst_ni assertion (active low):
 * 1. Reset all registers in both port instances via reset_registers()
 * 2. Notify interrupt driver to de-assert irq_o outputs
 * 3. Drain both sc_fifo channels to empty state (no clear() — drain with nb_read loop)
 * 4. Reset all shadow state to RDL-specified reset values
 */
void mailbox_ip::handle_reset()
{
    if (!rst_ni.read()) {
        CSML_INFO(1, logger) << "Mailbox IP reset sequence initiated (rst_ni = 0)";

        // =====================================================================
        // 1. Register Initialization (10 registers per port to RDL reset values)
        // =====================================================================
        b0.reset_registers();
        b1.reset_registers();

        // =====================================================================
        // 2. Interrupt Output Initialization (notify driver to de-assert)
        // =====================================================================
        m_irq_update_event[0].notify();
        m_irq_update_event[1].notify();

        // =====================================================================
        // 3. FIFO State Initialization
        // =====================================================================
        m_fifo[0].clear();
        m_fifo[1].clear();

        // =====================================================================
        // 4. Shadow State Initialization (FUNC_005, FUNC_006)
        // =====================================================================
        // Reset error flag shadow state to RDL-specified reset values:
        // read_error = 0x0, write_error = 0x0
        m_error_flag_read_error[0] = false;
        m_error_flag_write_error[0] = false;
        m_error_flag_read_error[1] = false;
        m_error_flag_write_error[1] = false;

        // Reset interrupt status shadow state (IRQS reset value = 0x0, per-port)
        m_irqs_wtirq[0] = false;  m_irqs_wtirq[1] = false;
        m_irqs_rtirq[0] = false;  m_irqs_rtirq[1] = false;
        m_irqs_eirq[0]  = false;  m_irqs_eirq[1]  = false;

        // Reset threshold shadow state (per-port)
        m_wirqt_threshold[0] = 0; m_wirqt_threshold[1] = 0;
        m_rirqt_threshold[0] = 0; m_rirqt_threshold[1] = 0;

        // Reset interrupt enable shadow state (IRQEN reset value = 0x0, per-port)
        m_irqen_wtirq[0] = false; m_irqen_wtirq[1] = false;
        m_irqen_rtirq[0] = false; m_irqen_rtirq[1] = false;
        m_irqen_eirq[0]  = false; m_irqen_eirq[1]  = false;

        m_access_error[0] = false;
        m_access_error[1] = false;
    }
}

// =============================================================================
// FUNC_003: WRITE_DATA Register Write Callback Implementation
// =============================================================================

/**
 * @brief WRITE_DATA register write callback — FIFO enqueue operation
 *
 * Uses non-blocking nb_write() since callback runs in b_transport context.
 * Full check via num_free() == 0; enqueue via nb_write().
 * Threshold checks use num_available() on the sc_fifo directly.
 */
bool mailbox_ip::handle_write_WRITE_DATA(unsigned int port, DT value, DT write_mask)
{
    // =========================================================================
    // 1. Pre-Check: Validate FIFO Not Full
    // =========================================================================
    auto& wfifo = write_fifo_for(port);
    if (fifo_level(wfifo) >= m_mailbox_depth) {
        // FIFO is full — overflow error condition
        CSML_WARN(1, logger) << "[MBX] Port " << port << " WRITE_DATA: FIFO write-to-full error";
        m_error_flag_write_error[port] = true;
        m_irqs_eirq[port] = true;
        m_access_error[port] = true;
        update_irqp_and_output(port);
        return false;
    }

    // =========================================================================
    // 2. Enqueue Data to Outbound FIFO
    // =========================================================================
    wfifo.push_back(static_cast<uint64_t>(value));
    CSML_INFO(2, logger) << "[MBX] Port " << port << " WRITE_DATA: enqueued 0x" 
                         << std::hex << value << std::dec;

    // =========================================================================
    // 3. Threshold Comparison and Interrupt Generation (FUNC_006, per-port)
    // =========================================================================
    unsigned int peer_port = 1 - port;

    if (fifo_level(wfifo) > m_wirqt_threshold[port]) {
        m_irqs_wtirq[port] = true;
    }

    // The FIFO this port writes into is the peer's read FIFO, so the same push
    // may also cross the peer's read threshold.
    if (fifo_level(read_fifo_for(peer_port)) > m_rirqt_threshold[peer_port]) {
        m_irqs_rtirq[peer_port] = true;
    }

    // =========================================================================
    // 4. Update Interrupt Outputs for Both Ports
    // =========================================================================
    update_irqp_and_output(port);
    update_irqp_and_output(peer_port);

    return true;
}

// =============================================================================
// FUNC_003: READ_DATA Register Read Callback Implementation
// =============================================================================

/**
 * @brief READ_DATA register read callback — FIFO dequeue operation
 *
 * Uses non-blocking nb_read() since callback runs in b_transport context.
 * Empty check via num_available() == 0; dequeue via nb_read().
 */
bool mailbox_ip::handle_read_READ_DATA(unsigned int port, DT& value, DT read_mask)
{
    // =========================================================================
    // 1. Pre-Check: Validate FIFO Not Empty
    // =========================================================================
    auto& rfifo = read_fifo_for(port);
    if (rfifo.empty()) {
        // FIFO is empty — underflow error condition
        CSML_WARN(1, logger) << "[MBX] Port " << port << " READ_DATA: FIFO read-from-empty error";
        m_error_flag_read_error[port] = true;
        m_irqs_eirq[port] = true;
        m_access_error[port] = true;
        update_irqp_and_output(port);
        return false;  // bus_access() substitutes the 0xFEEDDEAD sentinel
    }

    // =========================================================================
    // 2. Dequeue Data from Inbound FIFO
    // =========================================================================
    unsigned int peer_port = 1 - port;
    const uint64_t data = rfifo.front();
    rfifo.pop_front();
    value = static_cast<DT>(data);
    CSML_INFO(2, logger) << "[MBX] Port " << port << " READ_DATA: dequeued 0x" 
                         << std::hex << value << std::dec;

    // =========================================================================
    // 3. Update Interrupt Outputs for Both Ports
    // =========================================================================
    // IRQS sticky bits do not auto-clear on FIFO level change, but IRQP must
    // be recomputed since sc_fifo state changed (threshold conditions may differ).
    update_irqp_and_output(port);
    update_irqp_and_output(peer_port);

    return true;
}

// =============================================================================
// FUNC_004: STATUS Register Read Callback Implementation
// =============================================================================

/**
 * @brief STATUS register read callback — live FIFO status flags
 *
 * Computes all four status bits directly from sc_fifo state.
 * No persistent storage — recomputed on every read.
 */
bool mailbox_ip::handle_read_STATUS(unsigned int port, DT& value, DT read_mask)
{
    value = 0;

    const unsigned int rfill = fifo_level(read_fifo_for(port));
    const unsigned int wfill = fifo_level(write_fifo_for(port));

    // STATUS[0]: empty flag (inbound FIFO has no data to read)
    if (rfill == 0)
        value |= 0x1;

    // STATUS[1]: full flag (outbound FIFO has no free space)
    if (wfill >= m_mailbox_depth)
        value |= 0x2;

    // STATUS[2]: write_level_above_thresh (outbound FIFO usage exceeds WIRQT)
    if (wfill > m_wirqt_threshold[port])
        value |= 0x4;

    // STATUS[3]: read_level_above_thresh (inbound FIFO fill exceeds RIRQT)
    if (rfill > m_rirqt_threshold[port])
        value |= 0x8;

    // Reserved bits [63:4] remain 0
    return true;
}

// =============================================================================
// FUNC_005: ERROR_FLAGS Register Read Callback Implementation
// =============================================================================

/**
 * @brief ERROR_FLAGS register read callback — clear-on-read error status
 *
 * Returns current per-port error flag shadow state then atomically clears it.
 * Clearing ERROR_FLAGS does NOT clear IRQS[2] — that requires explicit W1C.
 */
bool mailbox_ip::handle_read_ERROR_FLAGS(unsigned int port, DT& value, DT read_mask)
{
    // =========================================================================
    // 1. Compose Return Value from Shadow State
    // =========================================================================
    value = 0;
    if (m_error_flag_read_error[port])  value |= 0x1;  // Bit [0]: read_error
    if (m_error_flag_write_error[port]) value |= 0x2;  // Bit [1]: write_error

    // =========================================================================
    // 2. Atomically Clear Shadow State (Clear-on-Read Side-Effect)
    // =========================================================================
    m_error_flag_read_error[port]  = false;
    m_error_flag_write_error[port] = false;

    return true;
}

// =============================================================================
// FUNC_006: WIRQT Register Write Callback Implementation
// =============================================================================

/**
 * @brief WIRQT register write callback — write threshold configuration
 *
 * Applies saturation logic and triggers immediate threshold comparison
 * using sc_fifo's num_available() on the outbound FIFO.
 */
bool mailbox_ip::handle_write_WIRQT(unsigned int port, DT value, DT write_mask)
{
    // =========================================================================
    // 1. Extract Threshold Value (Bits [7:0]) and Apply Saturation
    // =========================================================================
    uint64_t threshold = value & 0xFF;
    if (threshold >= m_mailbox_depth) {
        threshold = m_mailbox_depth - 1;
    }

    // =========================================================================
    // 2. Store Saturated Threshold to Per-Port Shadow State
    // =========================================================================
    m_wirqt_threshold[port] = static_cast<uint8_t>(threshold);
    CSML_INFO(2, logger) << "[MBX] Port " << port << " WIRQT: write threshold set to " 
                         << static_cast<unsigned int>(threshold);

    // =========================================================================
    // 3. Immediate Threshold Comparison (Retroactive Triggering)
    // =========================================================================
    if (fifo_level(write_fifo_for(port)) > m_wirqt_threshold[port]) {
        m_irqs_wtirq[port] = true;
    }

    update_irqp_and_output(port);
    return true;
}

// =============================================================================
// FUNC_006: RIRQT Register Write Callback Implementation
// =============================================================================

/**
 * @brief RIRQT register write callback — read threshold configuration
 *
 * Applies saturation logic and triggers immediate threshold comparison
 * using sc_fifo's num_available() on the inbound FIFO.
 */
bool mailbox_ip::handle_write_RIRQT(unsigned int port, DT value, DT write_mask)
{
    // =========================================================================
    // 1. Extract Threshold Value (Bits [7:0]) and Apply Saturation
    // =========================================================================
    uint64_t threshold = value & 0xFF;
    if (threshold >= m_mailbox_depth) {
        threshold = m_mailbox_depth - 1;
    }

    // =========================================================================
    // 2. Store Saturated Threshold to Per-Port Shadow State
    // =========================================================================
    m_rirqt_threshold[port] = static_cast<uint8_t>(threshold);
    CSML_INFO(2, logger) << "[MBX] Port " << port << " RIRQT: read threshold set to " 
                         << static_cast<unsigned int>(threshold);

    // =========================================================================
    // 3. Immediate Threshold Comparison (Retroactive Triggering)
    // =========================================================================
    if (fifo_level(read_fifo_for(port)) > m_rirqt_threshold[port]) {
        m_irqs_rtirq[port] = true;
    }

    update_irqp_and_output(port);
    return true;
}

// =============================================================================
// FUNC_002: WIRQT Register Read Callback Implementation
// =============================================================================

/**
 * @brief WIRQT register read callback — return saturated threshold from shadow state
 *
 * The write callback (handle_write_WIRQT) applies saturation and stores the
 * result to m_wirqt_threshold[port] without updating the CSML memory backing
 * store.  This read callback retrieves the correct post-saturation value so
 * that software reads reflect actual hardware behaviour as required by the
 * detailed design specification:
 *   "Subsequent reads return the saturated value, not the originally written value."
 *
 * Bit layout of returned value:
 * - Bits [7:0]  : m_wirqt_threshold[port]  (saturated 8-bit threshold)
 * - Bits [63:8] : 0x0  (reserved, always read as zero)
 *
 * FUNC_002 Test Verification: TC006 (test_reg_wirqt_rw) steps 2–5 explicitly
 * verify that reads return the written (and saturated) threshold value.
 *
 * @param port      Port index (0 or 1) that initiated the read transaction
 * @param value     Reference populated with the current saturated threshold
 * @param read_mask Bitfield read mask (not used; reserved bits returned as 0)
 * @return true     Read always succeeds for an RW register
 */
bool mailbox_ip::handle_read_WIRQT(unsigned int port, DT& value, DT read_mask)
{
    // Return saturated threshold in bits [7:0]; reserved bits [63:8] = 0
    value = static_cast<DT>(m_wirqt_threshold[port]);
    return true;
}

// =============================================================================
// FUNC_002: RIRQT Register Read Callback Implementation
// =============================================================================

/**
 * @brief RIRQT register read callback — return saturated threshold from shadow state
 *
 * The write callback (handle_write_RIRQT) applies saturation and stores the
 * result to m_rirqt_threshold[port] without updating the CSML memory backing
 * store.  This read callback retrieves the correct post-saturation value so
 * that software reads reflect actual hardware behaviour as required by the
 * detailed design specification:
 *   "Subsequent reads return the saturated value."
 *
 * Bit layout of returned value:
 * - Bits [7:0]  : m_rirqt_threshold[port]  (saturated 8-bit threshold)
 * - Bits [63:8] : 0x0  (reserved, always read as zero)
 *
 * FUNC_002 Test Verification: TC007 (test_reg_rirqt_rw) steps 1–4 explicitly
 * verify that reads return the written (and saturated) threshold value.
 *
 * @param port      Port index (0 or 1) that initiated the read transaction
 * @param value     Reference populated with the current saturated threshold
 * @param read_mask Bitfield read mask (not used; reserved bits returned as 0)
 * @return true     Read always succeeds for an RW register
 */
bool mailbox_ip::handle_read_RIRQT(unsigned int port, DT& value, DT read_mask)
{
    // Return saturated threshold in bits [7:0]; reserved bits [63:8] = 0
    value = static_cast<DT>(m_rirqt_threshold[port]);
    return true;
}

// =============================================================================
// FUNC_006: IRQS Register Read Callback Implementation
// =============================================================================

/**
 * @brief IRQS register read callback — return shadow hardware state
 */
bool mailbox_ip::handle_read_IRQS(unsigned int port, DT& value, DT read_mask)
{
    value = 0;
    if (m_irqs_wtirq[port]) value |= 0x1;  // Bit [0]: wtirq
    if (m_irqs_rtirq[port]) value |= 0x2;  // Bit [1]: rtirq
    if (m_irqs_eirq[port])  value |= 0x4;  // Bit [2]: eirq
    return true;
}

// =============================================================================
// FUNC_006: IRQS Register Write Callback Implementation
// =============================================================================

/**
 * @brief IRQS register write callback — write-1-to-clear interrupt acknowledgment
 *
 * Writing 1 to a bit position clears that IRQS bit; writing 0 has no effect.
 */
bool mailbox_ip::handle_write_IRQS(unsigned int port, DT value, DT write_mask)
{
    DT masked_value = value & write_mask;

    if (masked_value & 0x1) m_irqs_wtirq[port] = false;  // Clear wtirq
    if (masked_value & 0x2) m_irqs_rtirq[port] = false;  // Clear rtirq
    if (masked_value & 0x4) m_irqs_eirq[port]  = false;  // Clear eirq

    update_irqp_and_output(port);
    return true;
}

// =============================================================================
// FUNC_006: IRQEN Register Write Callback Implementation
// =============================================================================

/**
 * @brief IRQEN register write callback — interrupt enable mask update
 *
 * Extracts bits [2:0] and stores to per-port shadow state.
 * Dynamic enable changes have immediate retroactive effect on IRQP and irq_o.
 */
bool mailbox_ip::handle_write_IRQEN(unsigned int port, DT value, DT write_mask)
{
    m_irqen_wtirq[port] = (value & 0x1) != 0;  // Bit [0]: write threshold enable
    m_irqen_rtirq[port] = (value & 0x2) != 0;  // Bit [1]: read threshold enable
    m_irqen_eirq[port]  = (value & 0x4) != 0;  // Bit [2]: error interrupt enable

    update_irqp_and_output(port);
    return true;
}

// =============================================================================
// FUNC_006: IRQEN Register Read Callback Implementation
// =============================================================================

/**
 * @brief IRQEN register read callback — return shadow hardware enable state
 *
 * Required because handle_write_IRQEN updates shadow variables only (not
 * the CSML memory backing store). Reads return live shadow state.
 */
bool mailbox_ip::handle_read_IRQEN(unsigned int port, DT& value, DT read_mask)
{
    value = 0;
    if (m_irqen_wtirq[port]) value |= 0x1;  // Bit [0]: wtirq enable
    if (m_irqen_rtirq[port]) value |= 0x2;  // Bit [1]: rtirq enable
    if (m_irqen_eirq[port])  value |= 0x4;  // Bit [2]: eirq enable
    return true;
}

// =============================================================================
// FUNC_006: IRQP Register Read Callback Implementation
// =============================================================================

/**
 * @brief IRQP register read callback — hardware-computed interrupt pending
 *
 * Combinational: IRQP = IRQS & IRQEN, recomputed on every read.
 */
bool mailbox_ip::handle_read_IRQP(unsigned int port, DT& value, DT read_mask)
{
    uint64_t irqp_val = 0;
    if (m_irqs_wtirq[port] && m_irqen_wtirq[port]) irqp_val |= 0x1;
    if (m_irqs_rtirq[port] && m_irqen_rtirq[port]) irqp_val |= 0x2;
    if (m_irqs_eirq[port]  && m_irqen_eirq[port])  irqp_val |= 0x4;
    value = irqp_val;
    return true;
}

// =============================================================================
// FUNC_006: Interrupt State Update and Event Notification
// =============================================================================

/**
 * @brief Notify interrupt driver to update irq_o output for specified port
 *
 * Does NOT write to irq_o[port] directly (single-writer compliance).
 * Notifies m_irq_update_event[port] to wake irq_driver() SC_METHOD.
 */
void mailbox_ip::update_irqp_and_output(unsigned int port)
{
    m_irq_update_event[port].notify();
}

// =============================================================================
// FUNC_006: Interrupt Driver Process (Single-Writer to irq_o signals)
// =============================================================================

/**
 * @brief Interrupt driver SC_METHOD — single writer to irq_o signals
 *
 * The ONLY SystemC process that writes to irq_o[port] signals.
 * Triggered by m_irq_update_event[0] or m_irq_update_event[1].
 * Computes IRQP = IRQS & IRQEN for each port and drives irq_o[port]
 * according to trigger mode (level/edge) and polarity (active-high/low).
 */
void mailbox_ip::irq_driver()
{
    for (unsigned int port = 0; port < 2; port++) {
        // =====================================================================
        // 1. Compute IRQP = IRQS & IRQEN (Bitwise AND, per-port)
        // =====================================================================
        uint64_t irqp_val = 0;
        if (m_irqs_wtirq[port] && m_irqen_wtirq[port]) irqp_val |= 0x1;
        if (m_irqs_rtirq[port] && m_irqen_rtirq[port]) irqp_val |= 0x2;
        if (m_irqs_eirq[port]  && m_irqen_eirq[port])  irqp_val |= 0x4;

        // =====================================================================
        // 2. Determine Interrupt Pending and Polarity
        // =====================================================================
        bool irq_pending   = (irqp_val != 0);
        bool active_level  = m_irq_act_high;
        bool inactive_level = !m_irq_act_high;

        // =====================================================================
        // 3. Drive Interrupt Output (Level-Triggered)
        // =====================================================================
        irq_o[port].write(irq_pending ? active_level : inactive_level);
    }
}

// =============================================================================
// FUNC_007: CTRL Register Write Callback Implementation
// =============================================================================

/**
 * @brief CTRL register write callback — software-controlled FIFO flush
 *
 * Extracts wflush[0] and rflush[1] bits, drains the appropriate sc_fifo
 * channels via nb_read() loop (sc_fifo has no clear() method), then
 * updates interrupt outputs for both ports.
 *
 * wflush=1: drain write_fifo_for(port) — empties this port's outbound FIFO
 *           (which is peer port's inbound FIFO)
 * rflush=1: drain read_fifo_for(port) — empties this port's inbound FIFO
 *           (which is peer port's outbound FIFO)
 */
bool mailbox_ip::handle_write_CTRL(unsigned int port, DT value, DT write_mask)
{
    bool wflush = (value & 0x1) != 0;
    bool rflush = (value & 0x2) != 0;
    unsigned int peer_port = (port == 0) ? 1 : 0;

    // =========================================================================
    // Execute Write FIFO Flush (if wflush=1)
    // =========================================================================
    if (wflush) {
        CSML_INFO(1, logger) << "[MBX] Port " << port << " CTRL: flushing write FIFO";
        write_fifo_for(port).clear();
    }

    // =========================================================================
    // Execute Read FIFO Flush (if rflush=1)
    // =========================================================================
    if (rflush) {
        CSML_INFO(1, logger) << "[MBX] Port " << port << " CTRL: flushing read FIFO";
        read_fifo_for(port).clear();
    }

    // CTRL is a strobe: the flush happens on the write and no value is retained,
    // so clear the backing store rather than leaving the written bits visible.
    regs_for(port).CTRL.reset();

    // =========================================================================
    // Update Interrupt Status for Both Ports
    // =========================================================================
    update_irqp_and_output(port);
    update_irqp_and_output(peer_port);

    return true;
}
