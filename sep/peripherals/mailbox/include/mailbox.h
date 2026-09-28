// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox.h
 * @brief Main mailbox IP SystemC TLM model class
 *
 * This file defines the mailbox_ip class which composes two mailbox_base
 * instances (b0, b1) with two sc_fifo channels for bidirectional FIFO
 * communication, interrupt generation, error detection, and threshold-based
 * status monitoring.
 */

#pragma once

#include "mailbox_base.h"
#include "reg_logger.h"
#include "reg_param.h"
#include <deque>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

/**
 * @class mailbox_ip
 * @brief SystemC TLM model for mailbox IP with dual-port bidirectional FIFO communication
 *
 * Architecture (Composition):
 * - b0: Port 0 pure register container (10 registers, 1 memory, no socket)
 * - b1: Port 1 pure register container (10 registers, 1 memory, no socket)
 * - socket0/socket1: TLM target sockets. mailbox_ip owns the b_transport entry
 *   point so it can apply the AXI-Lite access rules (SLVERR on illegal access,
 *   sentinel read data) that regmodel::Memory cannot express, before delegating the
 *   register access itself to b0.memory / b1.memory.
 * - m_fifo[0]: b0 writes → b1 reads  (Port 0 outbound / Port 1 inbound)
 * - m_fifo[1]: b1 writes → b0 reads  (Port 1 outbound / Port 0 inbound)
 *
 * All cross-port logic, interrupt shadow state, and sc_fifo management
 * reside in mailbox_ip. The mailbox_base instances are thin register containers.
 *
 * Implements functional TLM behavior for mailbox IP including:
 * - Bidirectional FIFO data transfer (cross-port write/read)
 * - Threshold-based interrupt generation (WTIRQ, RTIRQ, EIRQ)
 * - Error detection (write-to-full, read-from-empty)
 * - Status monitoring (empty, full, level-above-threshold)
 * - FIFO flush control with dual-port coordination
 */
class mailbox_ip : public sc_module
{
public:
   SC_HAS_PROCESS(mailbox_ip);

   typedef typename regmodel::Reg<64>::DT DT;

   // =========================================================================
   // Composition: Dual single-port register sets
   // =========================================================================

   /// @brief Port 0 pure register container (10 registers + independent regmodel::Memory, no socket)
   mailbox_base b0;

   /// @brief Port 1 pure register container (10 registers + independent regmodel::Memory, no socket)
   mailbox_base b1;

   /// @brief TLM target socket for Port 0
   tlm_utils::simple_target_socket<mailbox_ip, 32> socket0;

   /// @brief TLM target socket for Port 1
   tlm_utils::simple_target_socket<mailbox_ip, 32> socket1;

   // =========================================================================
   // Bidirectional FIFOs
   // =========================================================================

   /**
    * @brief The two cross-connected FIFOs, mirroring the pair of fifo_v3
    *        instances inside the RTL's axi_lite_mailbox.
    *
    * m_fifo[0]: Port 0 writes WRITE_DATA → Port 1 reads READ_DATA.
    * m_fifo[1]: Port 1 writes WRITE_DATA → Port 0 reads READ_DATA.
    *
    * A plain deque rather than sc_fifo: every access happens inside
    * b_transport, so the blocking interface is never needed, and sc_fifo's
    * split of occupancy across num_available()/num_free() only becomes
    * consistent at the next delta cycle. RTL compares a single `usage` value
    * against the thresholds and for STATUS, so the model needs one occupancy
    * number that is correct immediately, which fifo_level() provides.
    */
   std::deque<uint64_t> m_fifo[2];

   // =========================================================================
   // Ports
   // =========================================================================

   /// @brief Interrupt output signals for both ports (aggregates WTIRQ, RTIRQ, EIRQ per port)
   sc_out<bool> irq_o[2];

   /// @brief Active-low asynchronous reset signal
   sc_in<bool> rst_ni;

   /// @brief Abstract clock frequency input in Hz
   sc_in<double> clk_i;

   /// @brief RegLogger for diagnostic output
   RegLogger logger;

   regmodel::Param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug

   /**
    * @brief Constructor for mailbox IP model
    * @param n SystemC module hierarchical name
    * @param log_verbosity Logging verbosity level (0=error, 1=warn, 2=info, 3=debug)
    *
    * Configuration is hardcoded internally:
    *   - Memory size: 0x50 bytes per port (10 registers × 8 bytes)
    *   - FIFO depth: 8 entries
    *   - Interrupt polarity: active-high
    */
   mailbox_ip(sc_module_name n, int log_verbosity = REG_DEFAULT_VERBOSITY);

private:
   // =========================================================================
   // FIFO routing helpers (cross-connection semantics)
   // =========================================================================

   /**
    * @brief Return port p's outbound FIFO (what p writes into)
    *
    * Cross-connection: Port 0 outbound = Port 1 inbound (m_fifo[0])
    *                   Port 1 outbound = Port 0 inbound (m_fifo[1])
    */
   std::deque<uint64_t>& write_fifo_for(unsigned int p) { return m_fifo[p]; }

   /**
    * @brief Return port p's inbound FIFO (what p reads from)
    *
    * Cross-connection: Port 0 inbound = Port 1 outbound (m_fifo[1])
    *                   Port 1 inbound = Port 0 outbound (m_fifo[0])
    */
   std::deque<uint64_t>& read_fifo_for(unsigned int p)  { return m_fifo[1 - p]; }

   /// @brief Return the register container belonging to port @p p
   mailbox_base& regs_for(unsigned int p) { return (p == 0) ? b0 : b1; }

   /// @brief Occupancy of a FIFO, the model's equivalent of the RTL `usage` signal
   unsigned int fifo_level(const std::deque<uint64_t>& f) const
   {
      return static_cast<unsigned int>(f.size());
   }

   // =========================================================================
   // Bus access rules (AXI-Lite behaviour the register library cannot express)
   // =========================================================================

   /**
    * @brief Common b_transport entry point for both ports
    *
    * Applies the decode and access-permission rules that RTL implements in
    * axi_lite_mailbox.sv before handing the access to the register library:
    * out-of-range offsets, writes to read-only registers and reads of CTRL all
    * complete with an error response, and the two sentinel read values are
    * produced here. FIFO overflow/underflow is detected by the register
    * callbacks, which raise m_access_error for this method to turn into an
    * error response.
    */
   void bus_access(unsigned int port, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

   void b_transport_port0(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
   {
      bus_access(0, trans, delay);
   }

   void b_transport_port1(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
   {
      bus_access(1, trans, delay);
   }

   /// @brief Write @p value into the transaction data buffer, honouring length and byte lane
   static void store_response_data(tlm::tlm_generic_payload& trans, uint64_t value);

   /**
    * @brief Set by the WRITE_DATA / READ_DATA callbacks when the FIFO access failed
    *
    * RTL answers a write-to-full or read-from-empty with SLVERR. The register
    * callbacks cannot set the TLM response themselves, so they raise this flag
    * and bus_access() converts it.
    */
   bool m_access_error[2];

   /// @brief Read data returned by RTL for a read of the write-only WRITE_DATA register
   static constexpr uint64_t SENTINEL_WRITE_DATA_READ = 0xFEEDC0DEULL;

   /// @brief Read data returned by RTL for a read of READ_DATA while the FIFO is empty
   static constexpr uint64_t SENTINEL_READ_EMPTY = 0xFEEDDEADULL;

   /**
    * @brief Reset handler (FUNC_001: System Reset and Initialization Behavior)
    *
    * Implements complete asynchronous active-low reset functionality as specified in
    * mailbox-architecture-behaviour-map.json reset_behavior section.
    *
    * Monitors rst_ni signal and performs complete state initialization when asserted.
    * Called by SystemC kernel on negative edge of rst_ni (active-low assertion).
    *
    * Reset Actions:
    * - All 10 registers per port initialized to RDL-specified reset values (0x0 for all)
    * - Interrupt outputs (irq_o[0], irq_o[1]) de-asserted to inactive level
    * - Inactive level respects IrqActHigh polarity (active-high: 0, active-low: 1)
    * - Both sc_fifo channels drained to empty state
    * - All shadow state (error flags, IRQS, IRQEN, thresholds) reset to 0
    *
    * Architecture Compliance:
    * - Single-Writer Rule: Only updates internal state, interrupt drivers respond
    * - Reset executes instantaneously in zero time (TLM abstraction)
    * - Asynchronous behavior: Triggers on rst_ni edge regardless of clock state
    */
   void handle_reset();

   /**
    * @brief Interrupt driver process (FUNC_006: Single-Writer Interrupt Output Driver)
    *
    * This is the ONLY SystemC process that writes to irq_o[port] signals.
    * Implements single-writer compliance for SystemC signal driver rules.
    *
    * Operation:
    * - Triggered by m_irq_update_event[0] or m_irq_update_event[1]
    * - Determines which port triggered the event
    * - Reads shadow state to compute IRQP and interrupt output level
    * - Writes to irq_o[port] based on trigger mode and polarity
    *
    * Event-Driven Architecture:
    * - handle_reset() notifies events after state update
    * - Callback handlers (via update_irqp_and_output()) notify events after state update
    * - This process responds to events and performs the actual signal write
    *
    * Single-Writer Compliance:
    * - Prevents "multiple driver" SystemC errors
    * - Separates state update (callbacks) from signal write (this process)
    * - Standard SystemC pattern for outputs updated from multiple contexts
    */
   void irq_driver();

   // =========================================================================
   // FUNC_003: Bidirectional FIFO Data Transfer Engine - Callback Handlers
   // =========================================================================

   /**
    * @brief WRITE_DATA register write callback (FUNC_003)
    *
    * Implements FIFO enqueue operation for cross-port data transfer as specified in
    * mailbox-architecture-behaviour-map.json registers.WRITE_DATA.write_effects.
    *
    * Operation Sequence:
    * 1. Pre-check: Validate write FIFO has free space (write_fifo_for(port)->num_free() > 0)
    * 2. If full: Set ERROR_FLAGS.write_error[1], set IRQS.eirq[2], return false
    * 3. If space available: nb_write 64-bit data to outbound sc_fifo
    * 4. Post-operation: Data becomes available in peer port's inbound sc_fifo
    * 5. Threshold comparison and interrupt update for both ports
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value 64-bit data value to enqueue to FIFO
    * @param write_mask Bitfield write mask (unused, all 64 bits writable)
    * @return true if enqueue succeeded, false if FIFO full
    */
   bool handle_write_WRITE_DATA(unsigned int port, DT value, DT write_mask);

   /**
    * @brief READ_DATA register read callback (FUNC_003)
    *
    * Implements FIFO dequeue operation for cross-port data transfer as specified in
    * mailbox-architecture-behaviour-map.json registers.READ_DATA.read_effects.
    *
    * Operation Sequence:
    * 1. Pre-check: Validate inbound sc_fifo has data (read_fifo_for(port)->num_available() > 0)
    * 2. If empty: Set ERROR_FLAGS.read_error[0], set IRQS.eirq[2], return false
    * 3. If data available: nb_read oldest entry from inbound sc_fifo
    * 4. Post-operation: Frees space in peer port's outbound sc_fifo
    * 5. Update IRQP and irq_o for both ports
    *
    * @param port Port index (0 or 1) that initiated read transaction
    * @param value Reference to return dequeued 64-bit data value
    * @param read_mask Bitfield read mask (unused, all 64 bits readable)
    * @return true if dequeue succeeded, false if FIFO empty
    */
   bool handle_read_READ_DATA(unsigned int port, DT& value, DT read_mask);

   /**
    * @brief STATUS register read callback (FUNC_004)
    *
    * Returns live hardware-controlled FIFO status flags computed from current
    * sc_fifo state. All bits are volatile and reflect real-time hardware.
    *
    * @param port Port index (0 or 1) set by b_transport context
    * @param value Reference to store computed STATUS value
    * @param read_mask Bit mask for read operation (unused, all bits readable)
    * @return true (read always succeeds for RO register)
    *
    * STATUS bit mapping:
    * - [0]: empty flag = (read_fifo_for(port)->num_available() == 0)
    * - [1]: full flag = (write_fifo_for(port)->num_free() == 0)
    * - [2]: write_level_above_thresh = (write_fifo_for(port)->num_available() > WIRQT)
    * - [3]: read_level_above_thresh = (read_fifo_for(port)->num_available() > RIRQT)
    * - [63:4]: Reserved (always 0)
    */
   bool handle_read_STATUS(unsigned int port, DT& value, DT read_mask);

   // =========================================================================
   // FUNC_005: Error Detection and Reporting Mechanism - Callback Handler
   // =========================================================================

   /**
    * @brief ERROR_FLAGS register read callback (FUNC_005)
    *
    * Implements clear-on-read semantics for error flag register as specified in
    * mailbox-architecture-behaviour-map.json registers.ERROR_FLAGS.read_effects.
    *
    * Operation Sequence:
    * 1. Retrieve current ERROR_FLAGS value from shadow state
    * 2. Return value to caller via reference parameter
    * 3. Atomically clear both shadow state bits to false (clear-on-read side-effect)
    *
    * Clear-on-Read Behavior:
    * - Reading ERROR_FLAGS is a destructive operation (register state changes)
    * - Current value returned, then all error bits cleared atomically
    * - Multiple errors before read accumulate by OR'ing bits (no counter)
    * - Subsequent read after clear returns 0x0 (until new errors occur)
    *
    * Reset Behavior:
    * - Reset initializes ERROR_FLAGS to 0x0 (no errors at reset, per RDL spec)
    * - An error flag is set only when a failed access is *attempted* at runtime
    *
    * @param port Port index (0 or 1) set by b_transport context
    * @param value Reference to return current ERROR_FLAGS value before clearing
    * @param read_mask Bit mask for read operation (unused)
    * @return true (read always succeeds for RO register)
    *
    * ERROR_FLAGS bit mapping:
    * - [0]: read_error (read-from-empty error flag)
    * - [1]: write_error (write-to-full error flag)
    * - [63:2]: Reserved (always 0)
    */
   bool handle_read_ERROR_FLAGS(unsigned int port, DT& value, DT read_mask);

   // =========================================================================
   // FUNC_007: Software-Controlled FIFO Management - Callback Handler
   // =========================================================================

   /**
    * @brief CTRL register write callback (FUNC_007)
    *
    * Implements software-controlled FIFO flush operations through CTRL register as
    * specified in mailbox-architecture-behaviour-map.json registers.CTRL.write_effects.
    *
    * Operation Sequence:
    * 1. Extract flush command bits: wflush[0] and rflush[1] from write value
    * 2. Execute write FIFO flush if wflush=1: drain write_fifo_for(port) via nb_read loop
    * 3. Execute read FIFO flush if rflush=1: drain read_fifo_for(port) via nb_read loop
    * 4. Update interrupt status for both current and peer ports
    *
    * sc_fifo Drain Pattern:
    * - sc_fifo has no clear() method; drain by nb_read() until num_available() == 0
    * - Non-blocking nb_read() used since b_transport is not SC_THREAD context
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value Control register value: Bit[0]=wflush, Bit[1]=rflush, Bits[63:2]=reserved
    * @param write_mask Bitfield write mask (unused)
    * @return true (write always succeeds)
    */
   bool handle_write_CTRL(unsigned int port, DT value, DT write_mask);

   // =========================================================================
   // Configuration Constants (hardcoded per design specification)
   // =========================================================================

   /// @brief FIFO depth (MailboxDepth = 8 entries)
   static constexpr unsigned int m_mailbox_depth = 8;

   /// @brief Interrupt polarity (active-high)
   static constexpr bool m_irq_act_high = true;

   // =========================================================================
   // FUNC_005: Error Detection - Shadow Hardware State
   // =========================================================================

   /**
    * @brief Shadow state for ERROR_FLAGS register hardware (per-port)
    *
    * Represents the actual hardware flip-flops for error flag storage.
    * Modified by callbacks on error conditions, read by ERROR_FLAGS read callback.
    * Clear-on-read behavior: Reading ERROR_FLAGS returns this state then clears it.
    */
   bool m_error_flag_read_error[2];   ///< Hardware state for ERROR_FLAGS.read_error bit [0] (per-port)
   bool m_error_flag_write_error[2];  ///< Hardware state for ERROR_FLAGS.write_error bit [1] (per-port)

   // =========================================================================
   // FUNC_006: Interrupt System - Shadow Hardware State
   // =========================================================================

   /**
    * @brief Shadow state for IRQS register hardware (per-port)
    *
    * Represents the actual hardware flip-flops for interrupt status storage.
    * Sticky write-1-to-clear behavior managed through these shadow variables.
    */
   bool m_irqs_wtirq[2];  ///< Hardware state for IRQS.wtirq bit [0] per port
   bool m_irqs_rtirq[2];  ///< Hardware state for IRQS.rtirq bit [1] per port
   bool m_irqs_eirq[2];   ///< Hardware state for IRQS.eirq bit [2] per port

   /**
    * @brief Shadow state for interrupt enable register (IRQEN) (per-port)
    *
    * Stores enable mask bits to avoid register bitfield access during callback execution.
    * Used for IRQP computation in update_irqp_and_output().
    */
   bool m_irqen_wtirq[2];  ///< Hardware state for IRQEN.wtirq bit [0] per port
   bool m_irqen_rtirq[2];  ///< Hardware state for IRQEN.rtirq bit [1] per port
   bool m_irqen_eirq[2];   ///< Hardware state for IRQEN.eirq bit [2] per port

   /**
    * @brief Shadow state for threshold registers (WIRQT, RIRQT) (per-port)
    *
    * Stores saturated threshold values after hardware validation.
    * Used for threshold comparisons in FIFO operations.
    */
   uint8_t m_wirqt_threshold[2];  ///< Hardware state for WIRQT threshold value [7:0] per port
   uint8_t m_rirqt_threshold[2];  ///< Hardware state for RIRQT threshold value [7:0] per port


   /**
    * @brief Interrupt update notification events (FUNC_006: Event-Driven Single-Writer Pattern)
    *
    * SystemC events used to notify irq_driver() process when interrupt state changes.
    * Callbacks update shadow state then notify these events; irq_driver() SC_METHOD
    * is the single writer to irq_o[port] signals.
    */
   sc_event m_irq_update_event[2];  ///< Interrupt update events per port

   // =========================================================================
   // FUNC_006: Interrupt System - Callback Handlers
   // =========================================================================

   /**
    * @brief WIRQT register write callback (FUNC_006)
    *
    * Implements write threshold configuration with saturation logic and immediate
    * threshold comparison as specified in architecture map.
    *
    * Operation Sequence:
    * 1. Extract threshold value from bits [7:0] of write data
    * 2. Apply saturation: If value >= MailboxDepth, clamp to (MailboxDepth-1)
    * 3. Store saturated value to m_wirqt_threshold[port] shadow state
    * 4. Immediate threshold comparison: If write_fifo_for(port)->num_available() > WIRQT, set IRQS[0]
    * 5. Update IRQP and irq_o output for current port
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value Threshold value to write (bits [7:0] used, [63:8] reserved)
    * @param write_mask Bitfield write mask (unused)
    * @return true (write always succeeds)
    */
   bool handle_write_WIRQT(unsigned int port, DT value, DT write_mask);

   /**
    * @brief RIRQT register write callback (FUNC_006)
    *
    * Implements read threshold configuration with saturation logic and immediate
    * threshold comparison as specified in architecture map.
    *
    * Operation Sequence:
    * 1. Extract threshold value from bits [7:0] of write data
    * 2. Apply saturation: If value >= MailboxDepth, clamp to (MailboxDepth-1)
    * 3. Store saturated value to m_rirqt_threshold[port] shadow state
    * 4. Immediate threshold comparison: If read_fifo_for(port)->num_available() > RIRQT, set IRQS[1]
    * 5. Update IRQP and irq_o output for current port
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value Threshold value to write (bits [7:0] used, [63:8] reserved)
    * @param write_mask Bitfield write mask (unused)
    * @return true (write always succeeds)
    */
   bool handle_write_RIRQT(unsigned int port, DT value, DT write_mask);

   /**
    * @brief WIRQT register read callback (FUNC_002 / FUNC_006)
    *
    * Returns the hardware-validated (saturated) write threshold from shadow state.
    * Required because handle_write_WIRQT stores the saturated value to
    * m_wirqt_threshold[port] only; the regmodel backing store retains the raw
    * unmodified value.  Without this callback reads would return the regmodel default
    * (0x0 after reset), not the saturated value mandated by the detailed design:
    * "Subsequent reads return the saturated value, not the originally written value."
    *
    * Bit mapping returned:
    * - Bits [7:0]: saturated threshold value (m_wirqt_threshold[port])
    * - Bits [63:8]: always 0 (reserved)
    *
    * Architecture Map Reference: registers.WIRQT.read_effects — shadow readback
    * FUNC_002 Test Coverage: TC006 (test_reg_wirqt_rw) steps 2–5 verify readback.
    *
    * @param port     Port index (0 or 1) that initiated read transaction
    * @param value    Reference to return the current saturated threshold value
    * @param read_mask Bitfield read mask (unused; masking applied internally)
    * @return true (read always succeeds for RW register)
    */
   bool handle_read_WIRQT(unsigned int port, DT& value, DT read_mask);

   /**
    * @brief RIRQT register read callback (FUNC_002 / FUNC_006)
    *
    * Returns the hardware-validated (saturated) read threshold from shadow state.
    * Required because handle_write_RIRQT stores the saturated value to
    * m_rirqt_threshold[port] only; the regmodel backing store retains the raw
    * unmodified value.  Without this callback reads would return the regmodel default
    * (0x0 after reset), not the saturated value mandated by the detailed design:
    * "Subsequent reads return the saturated value."
    *
    * Bit mapping returned:
    * - Bits [7:0]: saturated threshold value (m_rirqt_threshold[port])
    * - Bits [63:8]: always 0 (reserved)
    *
    * Architecture Map Reference: registers.RIRQT.read_effects — shadow readback
    * FUNC_002 Test Coverage: TC007 (test_reg_rirqt_rw) steps 1–4 verify readback.
    *
    * @param port     Port index (0 or 1) that initiated read transaction
    * @param value    Reference to return the current saturated threshold value
    * @param read_mask Bitfield read mask (unused; masking applied internally)
    * @return true (read always succeeds for RW register)
    */
   bool handle_read_RIRQT(unsigned int port, DT& value, DT read_mask);

   /**
    * @brief IRQS register read callback (FUNC_006)
    *
    * Returns current interrupt status from shadow hardware state.
    *
    * @param port Port index (0 or 1) that initiated read transaction
    * @param value Reference to return current IRQS value from shadow state
    * @param read_mask Bitfield read mask (unused)
    * @return true (read always succeeds)
    */
   bool handle_read_IRQS(unsigned int port, DT& value, DT read_mask);

   /**
    * @brief IRQS register write callback (FUNC_006)
    *
    * Implements write-1-to-clear interrupt acknowledgment mechanism as specified
    * in architecture map registers.IRQS.write_effects.
    *
    * Operation Sequence:
    * 1. For each bit [0,1,2]: If write_data[bit] == 1, clear IRQS[bit] in shadow state
    * 2. If write_data[bit] == 0, leave IRQS[bit] unchanged (sticky)
    * 3. Update IRQP = IRQS & IRQEN and irq_o output
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value Write data containing clear bits (1=clear, 0=no-op)
    * @param write_mask Bitfield write mask (unused)
    * @return true (write always succeeds)
    */
   bool handle_write_IRQS(unsigned int port, DT value, DT write_mask);

   /**
    * @brief IRQEN register write callback (FUNC_006)
    *
    * Implements interrupt enable mask update as specified in architecture map
    * registers.IRQEN.write_effects. Dynamic enable changes have immediate retroactive
    * effect on IRQP and irq_o.
    *
    * @param port Port index (0 or 1) that initiated write transaction
    * @param value Enable mask to write (bits [2:0] used)
    * @param write_mask Bitfield write mask (unused)
    * @return true (write always succeeds)
    */
   bool handle_write_IRQEN(unsigned int port, DT value, DT write_mask);

   /**
    * @brief IRQEN register read callback (FUNC_006)
    *
    * Returns current interrupt enable state from shadow hardware variables.
    * Required because handle_write_IRQEN updates only shadow state
    * (m_irqen_*[port]) and not the regmodel::Memory backing store.
    *
    * IRQEN Shadow State Mapping (per port):
    * - Bit [0]: m_irqen_wtirq[port]
    * - Bit [1]: m_irqen_rtirq[port]
    * - Bit [2]: m_irqen_eirq[port]
    * - Bits [63:3]: Reserved (always 0)
    *
    * @param port Port index (0 or 1) that initiated read transaction
    * @param value Reference to return current IRQEN shadow state
    * @param read_mask Bitfield read mask (unused)
    * @return true (read always succeeds for RW register)
    */
   bool handle_read_IRQEN(unsigned int port, DT& value, DT read_mask);

   /**
    * @brief IRQP register read callback (FUNC_006)
    *
    * Returns hardware-computed interrupt pending status: IRQP = IRQS & IRQEN.
    * Combinational logic — recomputed on every read, no persistent state.
    *
    * @param port Port index (0 or 1) set by b_transport context
    * @param value Reference to return computed IRQP value
    * @param read_mask Bitfield read mask (unused)
    * @return true (read always succeeds for RO register)
    */
   bool handle_read_IRQP(unsigned int port, DT& value, DT read_mask);

   // =========================================================================
   // FUNC_006: Interrupt Output Generation Helper
   // =========================================================================

   /**
    * @brief Notify irq_driver() to update irq_o output for specified port
    *
    * Does NOT write to irq_o[port] directly. Notifies m_irq_update_event[port]
    * to trigger the irq_driver() SC_METHOD, which is the single writer to all
    * irq_o signals (single-writer compliance, prevents SystemC E115 errors).
    *
    * @param port Port index (0 or 1) for which to notify interrupt update
    */
   void update_irqp_and_output(unsigned int port);
};
