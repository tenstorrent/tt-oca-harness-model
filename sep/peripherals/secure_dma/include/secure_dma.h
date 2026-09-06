// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file dma.h
 * @brief DMA Controller top-level model class
 *
 * This file contains the main DMA Controller model class that extends the base
 * register infrastructure with functional behavior for data transfer operations.
 */

#pragma once
#include "secure_dma_base.h"
#include "reg_logger.h"
#include "reg_param.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

/**
 * @class secure_dma_model
 * @brief DMA Controller SystemC TLM model implementation
 *
 * This class implements the DMA Controller functionality including:
 * - Memory-to-memory, memory-to-peripheral, and peripheral-to-memory transfers
 * - Multiple addressing modes (increment, fixed, wrap)
 * - Hardware handshake with 11 trigger sources
 * - Inline SHA-2 hashing (SHA-256/384/512)
 * - Security enforcement with address range validation
 * - Interrupt generation (done, chunk_done, error)
 *
 * The model provides transaction-level accurate behavior for all software-visible
 * register operations and data transfer sequences.
 */
class secure_dma_model : public secure_dma_base
{
  friend class testbench;

public:
  SC_HAS_PROCESS(secure_dma_model);

  // =========================================================================
  // TLM Initiator Sockets for DMA Bus Master Operations
  // =========================================================================

  /// TLM initiator socket for OpenTitan internal memory access (32-bit)
  tlm_utils::simple_initiator_socket<secure_dma_model, 32> ot_initiator_socket;

  /// TLM initiator socket for SoC Control Network access (32-bit configurable)
  tlm_utils::simple_initiator_socket<secure_dma_model, 32> ctn_initiator_socket;

  /// TLM initiator socket for SoC system memory access (64-bit)
  tlm_utils::simple_initiator_socket<secure_dma_model, 64> sys_initiator_socket;

  // =========================================================================
  // Interrupt Output Ports
  // =========================================================================

  /// Transfer completion interrupt output (level-high, status type)
  sc_out<bool> dma_done_intr;

  /// Chunk completion interrupt output (level-high, status type, memory-to-memory only)
  sc_out<bool> dma_chunk_done_intr;

  /// Error condition interrupt output (level-high, status type)
  sc_out<bool> dma_error_intr;

  // =========================================================================
  // Interrupt Synchronization Event
  // =========================================================================

  /// Event notified when interrupt outputs are updated (for test synchronization)
  sc_event interrupt_updated_event;

  // =========================================================================
  // Alert Output Port
  // =========================================================================

  /// Fatal fault alert output (triggers on critical errors)
  sc_out<bool> alert_fatal_fault;

  // =========================================================================
  // Hardware Handshake Interrupt Input Ports
  // =========================================================================

  /// Hardware handshake trigger inputs from low-speed I/O peripherals [10:0]
  sc_in<bool> lsio_trigger[11];

  // =========================================================================
  // Clock and Reset Ports
  // =========================================================================

  /// Abstract clock frequency input for timing calculations
  sc_in<sc_time> clk_i;

  /// Active-low asynchronous reset input
  sc_in<bool> rst_ni;

  /**
  * @brief Constructor for DMA Controller model
  * @param n SystemC module name
  *
  * Initializes the DMA controller with default register values,
  * sets up the register infrastructure for TLM transactions,
  * initializes all ports, and registers SystemC processes.
  */
#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
  regmodel::Param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
  secure_dma_model(sc_module_name n);

  /**
  * @brief Destructor for DMA Controller model
  */
  ~secure_dma_model();

  friend class testbench;

private:
  /// Quantum keeper for temporal decoupling
  tlm_utils::tlm_quantumkeeper m_qk;

  /// RegLogger instance for model diagnostics
  RegLogger logger;

  // =========================================================================
  // Internal State Variables for Register Management
  // =========================================================================

  /// Internal DMA busy state (used for CFG_REGWEN computation)
  bool m_dma_busy;

  /// Internal DMA busy state next value (single-writer pattern)
  bool m_dma_busy_next;

  /// Internal INTR_STATE register storage (hardware-managed)
  uint32_t m_intr_state;

  /// Internal INTR_STATE next value (single-writer pattern)
  uint32_t m_intr_state_next;

  /// Internal ERROR_CODE register storage (cleared when STATUS.error is cleared)
  uint32_t m_error_code;

  /// Internal ERROR_CODE next value (single-writer pattern)
  uint32_t m_error_code_next;

  /// Event to trigger interrupt driver method (single-writer pattern)
  sc_event m_interrupt_update_event;

  // =========================================================================
  // Transfer Engine Internal State Variables
  // =========================================================================

  /// Event triggered when transfer should execute (go-bit assertion, handshake)
  sc_event m_transfer_start_event;

  /// Event triggered to abort transfer execution
  sc_event m_transfer_abort_event;

  /// Current source address pointer (64-bit for system bus support)
  uint64_t m_current_src_addr;

  /// Current destination address pointer (64-bit for system bus support)
  uint64_t m_current_dst_addr;

  /// Chunk start source address (for wrap mode)
  uint64_t m_chunk_start_src_addr;

  /// Chunk start destination address (for wrap mode)
  uint64_t m_chunk_start_dst_addr;

  /// Total bytes remaining in transfer
  uint32_t m_bytes_remaining;

  /// Bytes remaining in current chunk
  uint32_t m_current_chunk_bytes_remaining;

  /// Transfer data buffer for read/write transactions (max 4 bytes)
  unsigned char m_transfer_data_buffer[4];

  // =========================================================================
  // SHA-2 Hash Engine Internal State Variables
  // =========================================================================

  /// SHA-2 hash context pointer (managed by OpenSSL EVP interface)
  void* m_hash_ctx;

  /// Computed hash digest storage (max 64 bytes for SHA-512)
  unsigned char m_hash_digest[64];

  /// Current hash algorithm in use (0=none, 1=SHA256, 2=SHA384, 3=SHA512)
  uint32_t m_hash_algorithm;

  /// Flag indicating whether hashing is active for current transfer
  bool m_hashing_active;

  // =========================================================================
  // Hardware Handshaking Internal State Variables
  // =========================================================================

  /// Event triggered by lsio_trigger assertion in hardware handshake mode
  sc_event m_handshake_trigger_event;

  /// Most recently asserted trigger index (0-10)
  int m_last_asserted_trigger_index;

  /// Previous lsio_trigger input states for edge detection (11 triggers)
  bool m_prev_trigger_state[11];

  // =========================================================================
  // SystemC Threads and Methods
  // =========================================================================

  /**
  * @brief Reset monitoring thread
  *
  * Monitors rst_ni port and resets all registers and internal state
  * when reset is asserted (active-low).
  */
  void reset_thread();

  /**
  * @brief Hardware handshake monitoring thread
  *
  * Monitors lsio_trigger inputs and initiates DMA chunk transfers
  * when enabled triggers are asserted.
  */
  void handshake_monitor_thread();

  /**
  * @brief DMA transfer engine thread
  *
  * Autonomous transfer execution engine that performs sequential read-write
  * transaction pairs for each data word. Handles:
  * - Memory-to-memory, memory-to-peripheral, peripheral-to-memory modes
  * - Chunked transfer decomposition
  * - Address advancement per addressing mode
  * - Sequential read→write transaction generation via TLM b_transport
  * - STATUS register updates (busy, done, chunk_done)
  * - Interrupt generation (done, chunk_done, error)
  * - Abort handling
  *
  * Triggered by m_transfer_start_event (set by CONTROL.go write callback).
  * Monitors m_transfer_abort_event for emergency termination.
  *
  * Per detailed design Section 7.3: "Implements state machine with
  * SRC_READ → DST_WRITE → Address Update → Chunk Check cycles."
  */
  void transfer_engine_thread();

  /**
  * @brief Interrupt update method
  *
  * Updates interrupt output ports based on INTR_STATE and INTR_ENABLE
  * register values. Called whenever interrupt state changes.
  */
  void update_interrupts();

  /**
  * @brief Interrupt driver method (single-writer pattern for SystemC compliance)
  *
  * SC_METHOD sensitive to m_interrupt_update_event. Reads m_intr_state and
  * INTR_ENABLE register, computes interrupt outputs, and writes to interrupt
  * ports. This is the ONLY process that writes to interrupt output ports,
  * ensuring SystemC single-writer rule compliance.
  */
  void interrupt_driver_method();

  // =========================================================================
  // Register Write Callbacks
  // =========================================================================

  /**
  * @brief Write callback for CONTROL register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Side-effects:
  * - go bit (bit 31): Triggers DMA operation (sets m_dma_busy, locks CFG_REGWEN)
  * - abort bit (bit 27): Initiates transfer abort (to be implemented in future)
  * - Validates opcode field and other configuration parameters
  *
  * Per architecture map: Triggers state machine transition from IDLE to active.
  * Per register callbacks doc: Hardware auto-clears go bit after completion.
  */
  bool handle_write_CONTROL(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for STATUS register (RW1C clearing behavior)
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Side-effects:
  * - Writing 1 to done/chunk_done/error/aborted bits clears them
  * - Clearing error also clears ERROR_CODE register
  * - Updates INTR_STATE register to reflect cleared interrupts
  * - De-asserts corresponding interrupt outputs
  *
  * Per register callbacks doc: RW1C semantics for bits 1,2,3,5.
  * busy and sha2_digest_valid are read-only.
  */
  bool handle_write_STATUS(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for INTR_ENABLE register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write
  *
  * Side-effects:
  * - Updates interrupt output gating based on new enable mask
  * - Bit 0 (dma_done): Gates dma_done_intr output
  * - Bit 1 (dma_chunk_done): Gates dma_chunk_done_intr output
  * - Bit 2 (dma_error): Gates dma_error_intr output
  * - Status bits in INTR_STATE remain set independently of enable mask
  *
  * Per Interrupt outputs = INTR_STATE AND INTR_ENABLE (per-bit)
  * Per functionality list: Independent enable bits gate interrupt outputs while preserving status bits.
  */
  bool handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for INTR_TEST register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write (transient, not stored)
  *
  * Side-effects:
  * - Bit 0: Forces INTR_STATE.dma_done
  * - Bit 1: Forces INTR_STATE.dma_chunk_done
  * - Bit 2: Forces INTR_STATE.dma_error
  * - Generates corresponding interrupt outputs for testing
  *
  * Per register callbacks doc: Used for testing alert generation mechanism.
  */
  bool handle_write_INTR_TEST(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for ALERT_TEST register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write (transient, not stored)
  *
  * Side-effects:
  * - Bit 0: Triggers fatal_fault alert output pulse
  *
  * Per register callbacks doc: Used for testing alert generation mechanism.
  */
  bool handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for RANGE_REGWEN register (RW0C lock)
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Side-effects:
  * - Writing 0x0 (kMultiBitBool4False) locks memory range registers permanently
  * - Once locked, cannot be unlocked until reset
  * - Protects: ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID
  *
  * Per register callbacks doc: Write-0-to-lock mechanism, reset value 0x6.
  * Per architecture map: RW0C access type prevents accidental unlocking.
  */
  bool handle_write_RANGE_REGWEN(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for ENABLED_MEMORY_RANGE_BASE register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces RANGE_REGWEN lock protection. Blocks writes when RANGE_REGWEN is locked (MuBi4False).
  */
  bool handle_write_ENABLED_MEMORY_RANGE_BASE(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for ENABLED_MEMORY_RANGE_LIMIT register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces RANGE_REGWEN lock protection. Blocks writes when RANGE_REGWEN is locked (MuBi4False).
  */
  bool handle_write_ENABLED_MEMORY_RANGE_LIMIT(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for RANGE_VALID register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces RANGE_REGWEN lock protection. Blocks writes when RANGE_REGWEN is locked (MuBi4False).
  */
  bool handle_write_RANGE_VALID(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for SRC_ADDR_LO register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  * Per Configuration registers locked during active transfers.
  */
  bool handle_write_SRC_ADDR_LO(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for SRC_ADDR_HI register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_SRC_ADDR_HI(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for DST_ADDR_LO register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_DST_ADDR_LO(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for DST_ADDR_HI register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_DST_ADDR_HI(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for ADDR_SPACE_ID register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_ADDR_SPACE_ID(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for TOTAL_DATA_SIZE register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_TOTAL_DATA_SIZE(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for CHUNK_DATA_SIZE register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_CHUNK_DATA_SIZE(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for TRANSFER_WIDTH register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_TRANSFER_WIDTH(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for SRC_CONFIG register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_SRC_CONFIG(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for DST_CONFIG register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_DST_CONFIG(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for HANDSHAKE_INTR_ENABLE register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_HANDSHAKE_INTR_ENABLE(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for CLEAR_INTR_SRC register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_CLEAR_INTR_SRC(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for CLEAR_INTR_BUS register
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_CLEAR_INTR_BUS(uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for INTR_SRC_ADDR array registers
  * @param index Array index (0-10)
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_INTR_SRC_ADDR(unsigned int index, uint32_t value, uint32_t write_mask);

  /**
  * @brief Write callback for INTR_SRC_WR_VAL array registers
  * @param index Array index (0-10)
  * @param value Written value
  * @param write_mask Writable bit mask
  * @return true to allow write, false to block
  *
  * Enforces CFG_REGWEN lock protection. Blocks writes when CFG_REGWEN is locked (MuBi4False).
  */
  bool handle_write_INTR_SRC_WR_VAL(unsigned int index, uint32_t value, uint32_t write_mask);

  // =========================================================================
  // Register Read Callbacks
  // =========================================================================

  /**
  * @brief Read callback for CFG_REGWEN register
  * @param value Reference to store read value
  * @param read_mask Readable bit mask
  * @return true (always succeeds)
  *
  * Returns hardware-managed lock status:
  * - 0x0 (locked) when DMA is busy
  * - 0x6 (unlocked) when DMA is idle
  *
  * Per register callbacks doc: Read-only register automatically updated by hardware.
  * Per architecture map: Hardware-managed automatic locking mechanism.
  */
  bool handle_read_CFG_REGWEN(uint32_t& value, uint32_t read_mask);

  /**
  * @brief Read callback for STATUS register
  * @param value Reference to store read value
  * @param read_mask Readable bit mask
  * @return true (always succeeds)
  *
  * Returns current DMA operational status reflecting real-time hardware state:
  * - busy bit: DMA actively executing transfer
  * - done/aborted/error/chunk_done: Event status bits
  * - sha2_digest_valid: Digest registers contain valid hash
  *
  * Per register callbacks doc: Values reflect real-time hardware state.
  */
  bool handle_read_STATUS(uint32_t& value, uint32_t read_mask);

  /**
  * @brief Read callback for ERROR_CODE register
  * @param value Reference to store read value
  * @param read_mask Readable bit mask
  * @return true (always succeeds)
  *
  * Returns error classification when STATUS.error is set.
  * Reflects detected error conditions from internal m_error_code variable.
  * Cleared when STATUS.error is cleared via write callback.
  *
  * Per register callbacks doc: Cleared when STATUS.error is cleared.
  */
  bool handle_read_ERROR_CODE(uint32_t& value, uint32_t read_mask);

  /**
  * @brief Read callback for INTR_STATE register
  * @param value Reference to store read value
  * @param read_mask Readable bit mask
  * @return true (always succeeds)
  *
  * Returns current interrupt state from internal m_intr_state variable:
  * - Bit 0: dma_done
  * - Bit 1: dma_chunk_done
  * - Bit 2: dma_error
  *
  * Per register callbacks doc: Read-only status updated by hardware events.
  */
  bool handle_read_INTR_STATE(uint32_t& value, uint32_t read_mask);

  /**
  * @brief Read callback for SHA2_DIGEST array registers
  * @param index Array index (0-15)
  * @param value Reference to store read value
  * @param read_mask Readable bit mask
  * @return true (always succeeds)
  *
  * Returns hash digest output registers.
  * Valid when STATUS.sha2_digest_valid is set.
  * Number of valid registers depends on hash algorithm.
  *
  * Per register callbacks doc: Digest remains valid until next transfer.
  */
  bool handle_read_SHA2_DIGEST(unsigned int index, uint32_t& value, uint32_t read_mask);

  // =========================================================================
  // Helper Methods for Lock Enforcement
  // =========================================================================

  /**
  * @brief Check if configuration registers are locked by CFG_REGWEN
  * @return true if locked (DMA busy), false if unlocked (DMA idle)
  *
  * Used by write callbacks for CFG_REGWEN-protected registers to enforce lock.
  */
  bool is_cfg_locked() const;

  /**
  * @brief Check if memory range registers are locked by RANGE_REGWEN
  * @return true if locked, false if unlocked
  *
  * Used by write callbacks for RANGE_REGWEN-protected registers.
  */
  bool is_range_locked() const;

  /**
  * @brief Register all callbacks with regmodel memory (consolidated)
  *
  * Registers write and read callbacks for all registers across
  * all functionalities. Called from constructor.
  */
  void register_all_callbacks();

  // =========================================================================
  // Interrupt Management Helper Methods
  // =========================================================================

  /**
  * @brief Set interrupt state bits and update interrupt outputs
  * @param done Set dma_done interrupt (bit 0)
  * @param chunk_done Set dma_chunk_done interrupt (bit 1)
  * @param error Set dma_error interrupt (bit 2)
  *
  * Updates m_intr_state internal variable and calls update_interrupts()
  * to propagate to output ports based on INTR_ENABLE gating.
  *
  * Per Status-type level-sensitive interrupts that remain asserted
  * until software explicitly clears via STATUS register RW1C mechanism.
  */
  void set_interrupt_state(bool done, bool chunk_done, bool error);

  /**
  * @brief Clear interrupt state bits and update interrupt outputs
  * @param done Clear dma_done interrupt (bit 0)
  * @param chunk_done Clear dma_chunk_done interrupt (bit 1)
  * @param error Clear dma_error interrupt (bit 2)
  *
  * Clears corresponding bits in m_intr_state and updates interrupt outputs.
  * Used for automatic clearing of STATUS.done and STATUS.chunk_done.
  *
  * Per Automatic clearing occurs when new transfer starts (done)
  * or when next chunk begins (chunk_done).
  */
  void clear_interrupt_state(bool done, bool chunk_done, bool error);

  // =========================================================================
  // Transfer Granularity Control Helper Methods
  // =========================================================================

  /**
  * @brief Get transfer width in bytes from TRANSFER_WIDTH register
  * @return Transfer width: 1 (ONE_BYTE), 2 (TWO_BYTE), 4 (FOUR_BYTE), or 0 (invalid)
  *
  * Decodes TRANSFER_WIDTH.transaction_width field:
  * - 0x0 = 1 byte (ONE_BYTE)
  * - 0x1 = 2 bytes (TWO_BYTE)
  * - 0x2 = 4 bytes (FOUR_BYTE)
  * - 0x3 = invalid (returns 0)
  *
  * Per Invalid width value should trigger size_error during validation.
  */
  uint32_t get_transfer_width_bytes();

  /**
  * @brief Validate address alignment for configured transfer width
  * @param addr Address to validate (full 64-bit address)
  * @param is_source true for source address, false for destination
  * @param width_bytes Transfer width in bytes (1, 2, or 4)
  * @return true if alignment is valid, false if misaligned
  *
  * Alignment requirements per specification:
  * - 1-byte: No alignment requirement (always valid)
  * - 2-byte: address[0] must be 0 (halfword-aligned)
  * - 4-byte: address[1:0] must be 00 (word-aligned)
  *
  * Sets ERROR_CODE.src_addr_error or ERROR_CODE.dst_addr_error on failure.
  *
  * Per detailed design: "Misaligned addresses trigger src_addr_error or
  * dst_addr_error in the ERROR_CODE register, and the DMA operation is
  * aborted before any transactions occur."
  */
  bool validate_address_alignment(uint64_t addr, bool is_source, uint32_t width_bytes);

  /**
  * @brief Validate transfer width configuration
  * @param pending_control_value Pending CONTROL register value to validate
  * @return true if valid, false if invalid
  *
  * Validates:
  * 1. TRANSFER_WIDTH encoding (0x0, 0x1, 0x2 valid; 0x3 invalid)
  * 2. SHA-2 inline hashing constraint (requires FOUR_BYTE width)
  *
  * Sets ERROR_CODE.size_error on validation failure.
  *
  * Per "Invalid TRANSFER_WIDTH encodings (value 0x3) and transfer
  * width mismatches with inline hashing (non-FOUR_BYTE width with SHA
  * operations) trigger size_error."
  */
  bool validate_transfer_width(uint32_t pending_control_value);

  /**
  * @brief Generate TLM byte enable mask for sub-word transfers
  * @param addr Address for the transfer (determines byte lane selection)
  * @param width_bytes Transfer width in bytes (1, 2, or 4)
  * @return Byte enable mask (4-bit for 32-bit bus, each bit enables one byte lane)
  *
  * Byte enable generation per specification:
  * - 1-byte: 1 bit active at position determined by address[1:0]
  * - 2-byte: 2 bits active at position determined by address[1]
  * - 4-byte: All 4 bits active (0xF)
  *
  * Examples:
  * - 1-byte at addr=0x1000: byte_enable=0x1 (lane 0)
  * - 1-byte at addr=0x1003: byte_enable=0x8 (lane 3)
  * - 2-byte at addr=0x1000: byte_enable=0x3 (lanes 0-1)
  * - 2-byte at addr=0x1002: byte_enable=0xC (lanes 2-3)
  * - 4-byte at any addr: byte_enable=0xF (all lanes)
  *
  * Per functionality list: "Produces accurate byte-enable strobes for
  * sub-word write transactions based on address alignment and transfer width."
  */
  uint8_t generate_byte_enable_mask(uint64_t addr, uint32_t width_bytes);

  /**
  * @brief Extract sub-word data from full-width read value
  * @param read_data Full 32-bit word read from memory
  * @param addr Address of the transfer (determines byte lane)
  * @param width_bytes Transfer width in bytes (1, 2, or 4)
  * @return Extracted sub-word value (right-aligned)
  *
  * Sub-word extraction per specification:
  * - 1-byte: Extracts byte from lane indicated by address[1:0]
  * - 2-byte: Extracts halfword from lanes indicated by address[1]
  * - 4-byte: Returns full word (no extraction needed)
  *
  * Examples:
  * - 1-byte from addr[1:0]=00: Extract bits [7:0]
  * - 1-byte from addr[1:0]=11: Extract bits [31:24]
  * - 2-byte from addr[1]=0: Extract bits [15:0]
  * - 2-byte from addr[1]=1: Extract bits [31:16]
  *
  * Per functionality list: "Extracts correct byte lane from full-width
  * read data based on address LSBs."
  */
  uint32_t extract_subword_from_read(uint32_t read_data, uint64_t addr, uint32_t width_bytes);

  /**
  * @brief Replicate sub-word data across full bus width for writes
  * @param write_data Sub-word write data (right-aligned)
  * @param addr Address of the transfer (determines byte lane)
  * @param width_bytes Transfer width in bytes (1, 2, or 4)
  * @return Replicated 32-bit word with data positioned in correct lane
  *
  * Data replication per specification:
  * - 1-byte: Replicate byte to all 4 lanes (used with byte enable mask)
  * - 2-byte: Replicate halfword to both halfwords (used with byte enable mask)
  * - 4-byte: Use data as-is (no replication needed)
  *
  * Note: Actual byte lane selection is done via byte enable mask, but
  * replication ensures correct data appears in target lane.
  *
  * Per functionality list: "Replicates narrow write data across all byte
  * lanes with appropriate byte-enable masks."
  */
  uint32_t replicate_subword_for_write(uint32_t write_data, uint64_t addr, uint32_t width_bytes);

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * primarily provides validation and data manipulation helper
  * methods. No additional register callbacks beyond previous functionality are required.
  * This function is reserved for future use if side-effect callbacks are
  * needed for TRANSFER_WIDTH register.
  */

  // =========================================================================
  // Addressing Mode Management Helper Methods
  // =========================================================================

  /**
  * @brief Decode source addressing mode from SRC_CONFIG register
  * @param increment Output: true if address should increment, false if fixed
  * @param wrap Output: true if address should wrap at chunk boundaries, false otherwise
  *
  * Decodes SRC_CONFIG register bits:
  * - Bit 0 (increment): 1=address advances, 0=address fixed (FIFO mode)
  * - Bit 1 (wrap): 1=wrap to chunk start, 0=linear progression
  *
  * Addressing modes:
  * - Fixed: increment=0, wrap=X (address constant for all transactions)
  * - Incrementing: increment=1, wrap=0 (address += transfer_width per transaction)
  * - Wrapping: increment=1, wrap=1 (address advances with wrap to chunk base)
  *
  * Per detailed design: "When set with increment=1, address wraps back to the
  * programmed start address after each chunk completes."
  */
  void get_source_addressing_mode(bool& increment, bool& wrap);

  /**
  * @brief Decode destination addressing mode from DST_CONFIG register
  * @param increment Output: true if address should increment, false if fixed
  * @param wrap Output: true if address should wrap at chunk boundaries, false otherwise
  *
  * Decodes DST_CONFIG register bits (same encoding as SRC_CONFIG):
  * - Bit 0 (increment): 1=address advances, 0=address fixed (FIFO mode)
  * - Bit 1 (wrap): 1=wrap to chunk start, 0=linear progression
  *
  * Per detailed design: "Addressing modes configured independently for source
  * and destination via SRC_CONFIG and DST_CONFIG registers."
  */
  void get_destination_addressing_mode(bool& increment, bool& wrap);

  /**
  * @brief Advance source address after read transaction
  * @param current_addr Current 64-bit source address (updated in place)
  * @param chunk_start_addr Chunk start address for wrap mode
  * @param chunk_size Size of chunk in bytes (for wrap calculation)
  * @param width_bytes Transfer width in bytes (increment amount)
  *
  * Applies source addressing mode behavior:
  * - Fixed mode (increment=0): Address unchanged
  * - Incrementing mode (increment=1, wrap=0): Address += width_bytes
  * - Wrapping mode (increment=1, wrap=1): Address += width_bytes with chunk wrap
  *
  * Wrapping calculation:
  * - If (current_addr + width_bytes - chunk_start_addr) >= chunk_size:
  *  current_addr = chunk_start_addr (wrap back to chunk base)
  * - Else: current_addr += width_bytes (normal increment)
  *
  * Per detailed design: "SRC_ADDR advances by TRANSFER_WIDTH after each read
  * transaction if SRC_CONFIG.increment=1."
  */
  void advance_source_address(uint64_t& current_addr, uint64_t chunk_start_addr,
                uint32_t chunk_size, uint32_t width_bytes);

  /**
  * @brief Advance destination address after write transaction
  * @param current_addr Current 64-bit destination address (updated in place)
  * @param chunk_start_addr Chunk start address for wrap mode
  * @param chunk_size Size of chunk in bytes (for wrap calculation)
  * @param width_bytes Transfer width in bytes (increment amount)
  *
  * Applies destination addressing mode behavior (same logic as source):
  * - Fixed mode (increment=0): Address unchanged
  * - Incrementing mode (increment=1, wrap=0): Address += width_bytes
  * - Wrapping mode (increment=1, wrap=1): Address += width_bytes with chunk wrap
  *
  * Per detailed design: "DST_ADDR advances by TRANSFER_WIDTH after each write
  * transaction if DST_CONFIG.increment=1."
  */
  void advance_destination_address(uint64_t& current_addr, uint64_t chunk_start_addr,
                   uint32_t chunk_size, uint32_t width_bytes);

  /**
  * @brief Update SRC_ADDR_LO and SRC_ADDR_HI registers with current address
  * @param current_addr Current 64-bit source address to write to registers
  *
  * Splits 64-bit address and writes to register pair:
  * - SRC_ADDR_LO = current_addr[31:0]
  * - SRC_ADDR_HI = current_addr[63:32]
  *
  * Per detailed design: "During an active transfer, reading these registers may
  * return updated values reflecting current transfer progress."
  *
  * Note: Called during transfer execution to provide software-visible address progress.
  */
  void update_src_addr_registers(uint64_t current_addr);

  /**
  * @brief Update DST_ADDR_LO and DST_ADDR_HI registers with current address
  * @param current_addr Current 64-bit destination address to write to registers
  *
  * Splits 64-bit address and writes to register pair:
  * - DST_ADDR_LO = current_addr[31:0]
  * - DST_ADDR_HI = current_addr[63:32]
  *
  * Per detailed design: "DST_ADDR advances by TRANSFER_WIDTH after each write
  * transaction. Addresses wrap to chunk start address after chunk completion
  * if wrap mode is enabled."
  */
  void update_dst_addr_registers(uint64_t current_addr);

  /**
   * @brief Advance the visible address registers at a chunk boundary
   *
   * Applies the hardware writeback rule: only the fixed-address configuration
   * (increment=0, wrap=0) rewrites SRC_ADDR/DST_ADDR, adding one whole
   * CHUNK_DATA_SIZE per chunk. Increment and wrap modes leave the registers at
   * the value software programmed.
   *
   * @param chunk_size Configured CHUNK_DATA_SIZE in bytes
   */
  void apply_chunk_end_address_writeback(uint32_t chunk_size);

  /**
  * @brief Validate address wrap boundaries
  * @param base_addr Base address for wrapping region
  * @param chunk_size Size of wrapping region in bytes
  * @return true if parameters are valid for wrapping, false if invalid
  *
  * Validates wrapping configuration:
  * - Chunk size must be non-zero
  * - Base address must be aligned to transfer width
  * - Base + chunk_size must not overflow 64-bit address space
  *
  * Sets ERROR_CODE.size_error if validation fails.
  *
  * Per "Ensures address stays within [base, limit] range for
  * circular buffer mode."
  */
  bool validate_wrap_boundaries(uint64_t base_addr, uint32_t chunk_size);

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * provides addressing mode management and address advancement logic.
  * No additional register callbacks beyond previous functionality are required (SRC_CONFIG
  * and DST_CONFIG already registered). This function logs initialization.
  */

  // =========================================================================
  // Multi-Bus Interface Transaction Routing Helper Methods
  // =========================================================================

  /**
  * @brief Bus interface enumeration for routing decisions
  */
  enum class BusInterface {
   OT_INTERNAL,  // OpenTitan 32-bit internal bus (ASID 0x7)
   CTN_BUS,    // Control Network 32/64-bit bus (ASID 0xA)
   SYSTEM_BUS,   // System 64-bit custom bus (ASID 0x9)
   INVALID     // Invalid ASID or error condition
  };

  /**
  * @brief Decode and validate ASID value from ADDR_SPACE_ID register
  * @param asid_value 4-bit ASID value to decode
  * @return Decoded BusInterface enumeration (INVALID if ASID is invalid)
  *
  * ASID Mapping per specification:
  * - 0x7 (OT_ADDR): OpenTitan 32-bit internal bus → OT_INTERNAL
  * - 0x9 (SYS_ADDR): SoC 64-bit system bus → SYSTEM_BUS
  * - 0xA (SOC_ADDR): SoC control network (32/64-bit) → CTN_BUS
  * - All other values: Invalid → INVALID
  *
  * Per functionality list: "Routes source read and destination write
  * transactions to appropriate bus interfaces based on multibit-encoded
  * ASID values (0x7=OT_ADDR, 0x9=SYS_ADDR, 0xA=SOC_ADDR)."
  *
  * Per detailed design: "Invalid multibit-encoded values trigger asid_error
  * flag in ERROR_CODE register."
  */
  BusInterface decode_asid(uint32_t asid_value);

  /**
  * @brief Validate ASID value and set error if invalid
  * @param asid_value 4-bit ASID value to validate
  * @param is_source true for source ASID, false for destination ASID
  * @return true if ASID is valid, false if invalid
  *
  * Validates ASID against multibit encoding specification:
  * - Valid values: 0x7 (OT_ADDR), 0x9 (SYS_ADDR), 0xA (SOC_ADDR)
  * - Invalid values: All others (0x0-0x6, 0x8, 0xB-0xF)
  *
  * Sets ERROR_CODE.asid_error (bit 7) on validation failure.
  *
  * Per detailed design: "Source and destination ASID values in
  * ADDR_SPACE_ID register determine which bus interface is used.
  * Invalid multibit-encoded values set asid_error flag."
  */
  bool validate_asid(uint32_t asid_value, bool is_source);

  /**
  * @brief Select bus interface for transaction based on ASID
  * @param is_source true for source (read) transaction, false for destination (write)
  * @return Selected BusInterface enumeration (INVALID if ASID is invalid)
  *
  * Reads ADDR_SPACE_ID register and selects appropriate bus interface:
  * - For source transactions: Uses src_asid field (bits [3:0])
  * - For destination transactions: Uses dst_asid field (bits [7:4])
  *
  * Per functionality list: "ASID-Based Bus Selection: Routes source read
  * and destination write transactions to appropriate bus interfaces based
  * on multibit-encoded ASID values configured in ADDR_SPACE_ID register."
  */
  BusInterface select_bus_for_transaction(bool is_source);

  /**
  * @brief Validate address width constraint for selected bus interface
  * @param addr 64-bit address to validate
  * @param bus_interface Target bus interface for the transaction
  * @param is_source true for source address, false for destination address
  * @return true if address meets bus width constraints, false if violation
  *
  * Address width enforcement per specification:
  * - OT_INTERNAL: Upper 32 bits must be zero (32-bit address space)
  * - CTN_BUS: Support both 32-bit and 64-bit (configurable)
  *  - For 32-bit CTN: Upper 32 bits must be zero
  *  - For 64-bit CTN: Full 64-bit address supported
  * - SYSTEM_BUS: Full 64-bit address space supported
  *
  * Sets ERROR_CODE.src_addr_error or ERROR_CODE.dst_addr_error on violation.
  *
  * Per functionality list: "Address Width Enforcement: Validates address
  * constraints per interface: 32-bit upper address must be zero for OT
  * Private and 32-bit CTN configurations; full 64-bit addressing enabled
  * for System bus and 64-bit CTN configurations."
  */
  bool validate_address_width_for_bus(uint64_t addr, BusInterface bus_interface, bool is_source);

  /**
  * @brief Create TLM generic payload for bus transaction
  * @param trans Reference to TLM generic payload to populate
  * @param cmd Transaction command (TLM_READ_COMMAND or TLM_WRITE_COMMAND)
  * @param addr 64-bit transaction address
  * @param data Pointer to data buffer (read destination or write source)
  * @param length Data length in bytes (transfer width: 1, 2, or 4)
  * @param byte_enable_mask Byte enable mask for sub-word transfers
  *
  * Populates TLM-2.0 generic payload attributes per specification:
  * - Command: TLM_READ_COMMAND for source reads, TLM_WRITE_COMMAND for dest writes
  * - Address: Full 64-bit address (validated separately for bus constraints)
  * - Data pointer: Buffer for read data or write data
  * - Data length: Transfer width from (1, 2, or 4 bytes)
  * - Byte enables: Mask from byte enable generation
  * - Streaming width: Set equal to data length (no streaming)
  * - Response status: TLM_INCOMPLETE_RESPONSE (initial state)
  *
  * Per functionality list: "Implements transaction-level protocol mapping
  * between TLM generic payloads and protocol-specific bus semantics."
  */
  void create_tlm_transaction(tlm::tlm_generic_payload& trans,
                tlm::tlm_command cmd,
                uint64_t addr,
                unsigned char* data,
                uint32_t length,
                uint8_t byte_enable_mask);

  /**
  * @brief Get bus interface name for logging and debugging
  * @param bus_interface Bus interface enumeration
  * @return String representation of bus interface name
  *
  * Returns human-readable names for logging:
  * - OT_INTERNAL → "OT Internal (32-bit TL-UL)"
  * - CTN_BUS → "CTN (Control Network)"
  * - SYSTEM_BUS → "System Bus (64-bit)"
  * - INVALID → "INVALID"
  */
  const char* get_bus_name(BusInterface bus_interface);

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * provides bus routing and transaction preparation logic.
  * No additional register callbacks beyond previous functionality are required
  * (ADDR_SPACE_ID already registered with CFG_REGWEN protection).
  * This function logs initialization.
  */

  // =========================================================================
  // Error Detection and Reporting Helper Methods
  // =========================================================================

  /**
  * @brief Validate opcode field in CONTROL register
  * @param pending_control_value Pending CONTROL register value to validate
  * @return true if opcode is valid, false if invalid
  *
  * Validates CONTROL.opcode field (bits [3:0]) against supported values:
  * - 0x0: Memory copy (no hashing)
  * - 0x1: SHA-256 hash
  * - 0x2: SHA-384 hash
  * - 0x3: SHA-512 hash
  * - 0x4-0xF: Reserved/invalid
  *
  * Sets ERROR_CODE.opcode_error (bit 2) if opcode is invalid.
  *
  * Per detailed design: "CONTROL.opcode contains an invalid value (not 0x0,
  * 0x1, 0x2, or 0x3)."
  *
  * Per functionality list: "Invalid Opcode Detection: Validates CONTROL.opcode
  * field against supported values (0x0=COPY, 0x1=SHA256, 0x2=SHA384,
  * 0x3=SHA512), setting ERROR_CODE.opcode_error for reserved opcode encodings."
  */
  bool validate_opcode(uint32_t pending_control_value);

  /**
  * @brief Validate transfer size configuration
  * @return true if sizes are valid, false if invalid
  *
  * Validates transfer size configuration:
  * - TOTAL_DATA_SIZE != 0 (zero total size is invalid)
  * - CHUNK_DATA_SIZE != 0 (zero chunk size is invalid)
  * - CHUNK_DATA_SIZE <= TOTAL_DATA_SIZE (chunk cannot exceed total)
  *
  * Sets ERROR_CODE.size_error (bit 3) on validation failure.
  *
  * Per detailed design: "TOTAL_DATA_SIZE register is 0. Sets size_error.
  * A transfer with zero bytes is meaningless and not permitted."
  * "CHUNK_DATA_SIZE register is 0. Sets size_error."
  *
  * Per functionality list: "Invalid Transfer Size Detection: Detects
  * zero-size configurations (TOTAL_DATA_SIZE=0 or CHUNK_DATA_SIZE=0),
  * transfer width mismatches with inline hashing (non-FOUR_BYTE width
  * with SHA operations), and invalid TRANSFER_WIDTH encodings (value 0x3),
  * setting ERROR_CODE.size_error."
  */
  bool validate_transfer_size();

  /**
  * @brief Comprehensive pre-transfer validation
  * @param pending_control_value Pending CONTROL register value to validate
  * @return true if all validations pass, false if any error detected
  *
  * Orchestrates all pre-transfer validation checks before starting a transfer.
  * Called when CONTROL.go bit is written. Validates:
  * 1. Opcode validity (via validate_opcode)
  * 2. Transfer width (via validate_transfer_width from )
  * 3. Transfer sizes (via validate_transfer_size)
  * 4. Source address alignment (via validate_address_alignment from )
  * 5. Destination address alignment (via validate_address_alignment from )
  * 6. Source ASID validity (via validate_asid from )
  * 7. Destination ASID validity (via validate_asid from )
  * 8. Source address width (via validate_address_width_for_bus from )
  * 9. Destination address width (via validate_address_width_for_bus from )
  *
  * Populates ERROR_CODE register with specific error bits on validation failure.
  * Sets STATUS.error bit when any error is detected.
  *
  * Per detailed design: "Configuration validation before transfer initiation"
  * and "Misaligned addresses trigger src_addr_error or dst_addr_error in the
  * ERROR_CODE register, and the DMA operation is aborted before any
  * transactions occur."
  *
  * Per functionality list: "Implements comprehensive error detection spanning
  * configuration validation errors (address alignment, ASID encoding, size
  * constraints) and runtime errors (bus transaction failures). Provides
  * detailed error classification through multi-bit ERROR_CODE register,
  * immediate transfer termination upon error, and error interrupt generation
  * with mandatory software acknowledgment."
  */
  bool validate_transfer_configuration(uint32_t pending_control_value);

  /**
  * @brief Handle bus transaction error response
  * @param response_status TLM response status from b_transport
  * @return true if response is OK, false if error occurred
  *
  * Processes TLM response status from bus transactions and maps errors to
  * ERROR_CODE register. Handles:
  * - TLM_OK_RESPONSE: Normal completion, no error
  * - TLM_ADDRESS_ERROR_RESPONSE: Address decode error → bus_error
  * - TLM_COMMAND_ERROR_RESPONSE: Invalid command → bus_error
  * - TLM_GENERIC_ERROR_RESPONSE: Generic bus error → bus_error
  * - TLM_INCOMPLETE_RESPONSE: Transaction incomplete → bus_error
  *
  * Sets ERROR_CODE.bus_error (bit 4) on any error response.
  * Sets STATUS.error bit and triggers dma_error interrupt.
  *
  * Per detailed design: "A read or write transaction on any bus interface
  * (OT, CTN, or System) returns an error response. This indicates the target
  * device or memory reported a problem such as: Access permission denial,
  * Address decode error (address not mapped to any device), Device-specific
  * error condition, Timeout or no response from target."
  * "Bus errors set the bus_error flag in ERROR_CODE. The DMA halts the
  * transfer immediately upon receiving a bus error response."
  *
  * Per functionality list: "Bus Error Detection and Propagation: Monitors
  * TLM response status from all bus transactions, detecting
  * TLM_GENERIC_ERROR_RESPONSE and TLM_ADDRESS_ERROR_RESPONSE, setting
  * ERROR_CODE.bus_error and immediately halting transfer execution."
  *
  * Note: This method will be called by (Transfer Engine) after each
  * TLM b_transport call to check for bus errors.
  */
  bool handle_bus_error(tlm::tlm_response_status response_status);

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * provides error detection and reporting logic. No additional
  * register callbacks beyond , , , and 
  * are required (all validation is performed by helper methods called
  * during transfer initiation and execution).
  *
  * This function logs initialization.
  */

  // =========================================================================
  // Security Isolation and Access Control Helper Methods
  // =========================================================================

  /**
  * @brief Validate memory range configuration registers
  * @return true if range configuration is valid, false if invalid
  *
  * Validates ENABLED_MEMORY_RANGE configuration:
  * - ENABLED_MEMORY_RANGE_BASE ≤ ENABLED_MEMORY_RANGE_LIMIT
  * - RANGE_VALID must be set for cross-boundary transfers
  *
  * Sets ERROR_CODE.base_limit_error (bit 5) if BASE > LIMIT.
  * Sets ERROR_CODE.range_valid_error (bit 6) if RANGE_VALID not set when required.
  *
  * Per detailed design: "ENABLED_MEMORY_RANGE_BASE is greater than
  * ENABLED_MEMORY_RANGE_LIMIT. Sets base_limit_error. This indicates
  * an invalid memory range configuration."
  *
  * Per functionality list: "Base/Limit Configuration Error Detection:
  * Validates ENABLED_MEMORY_RANGE_BASE ≤ ENABLED_MEMORY_RANGE_LIMIT
  * relationship, detecting invalid range configurations and setting
  * ERROR_CODE.base_limit_error before transfer initiation."
  */
  bool validate_range_configuration();

  /**
  * @brief Check if address is within DMA-enabled memory range
  * @param addr 32-bit OT internal address to check
  * @return true if address is within [BASE, LIMIT] inclusive, false otherwise
  *
  * Checks if the given address falls within the configured DMA-enabled
  * memory range: ENABLED_MEMORY_RANGE_BASE ≤ addr ≤ ENABLED_MEMORY_RANGE_LIMIT
  *
  * Used to determine if an OT internal address is in OT DMA-enabled memory
  * (staging area) or OT Private memory (outside range).
  *
  * Per detailed design: "OT DMA Enabled Memory: Memory within the OpenTitan
  * secure perimeter designated as a staging area for DMA operations. This
  * memory region is configured via the ENABLED_MEMORY_RANGE_BASE and
  * ENABLED_MEMORY_RANGE_LIMIT registers."
  */
  bool check_address_in_dma_range(uint32_t addr);

  /**
  * @brief Validate security policy for transfer
  * @param src_addr 64-bit source address
  * @param dst_addr 64-bit destination address
  * @param src_asid Source ASID value (0x7=OT, 0x9=SYS, 0xA=SOC)
  * @param dst_asid Destination ASID value (0x7=OT, 0x9=SYS, 0xA=SOC)
  * @return true if transfer is allowed by security policy, false if prohibited
  *
  * Enforces three-tier security policy matrix:
  *
  * **Memory Region Definitions:**
  * - OT Private: OT_ADDR (0x7) outside DMA range
  * - OT DMA Enabled: OT_ADDR (0x7) within [BASE, LIMIT]
  * - SoC Memory: SYS_ADDR (0x9) or SOC_ADDR (0xA)
  *
  * **Allowed Transfers:**
  * - OT Private ↔ OT Private (both in OT, both outside or within range)
  * - OT Private → OT DMA Enabled
  * - OT DMA Enabled → OT Private
  * - OT DMA Enabled ↔ OT DMA Enabled
  * - OT DMA Enabled ↔ SoC (staging for cross-boundary)
  * - SoC ↔ SoC (external memory only)
  *
  * **Prohibited Transfers:**
  * - OT Private → SoC (prevents data leakage to untrusted memory)
  * - SoC → OT Private (prevents unauthorized access to secure memory)
  *
  * Sets ERROR_CODE.src_addr_error or ERROR_CODE.dst_addr_error for prohibited
  * transfers. Sets ERROR_CODE.range_valid_error if RANGE_VALID not set when
  * security boundary is crossed.
  *
  * Per detailed design: "This matrix ensures that untrusted SoC memory
  * cannot directly access OT Private Memory, and that OT Private Memory
  * contents cannot leak directly to SoC memory. All cross-boundary
  * transfers must stage through OT DMA Enabled Memory, providing an
  * opportunity for firmware to perform security validation."
  *
  * Per functionality list: "Security Policy Enforcement Matrix: Implements
  * comprehensive transfer validation matrix checking source/destination
  * ASID combinations against allowed patterns: OT-to-OT, OT-DMA-to-OT,
  * OT-DMA-to-SoC, SoC-to-OT-DMA, SoC-to-SoC allowed; OT-Private-to-SoC
  * and SoC-to-OT-Private prohibited."
  */
  bool validate_security_policy(uint64_t src_addr, uint64_t dst_addr,
                  uint32_t src_asid, uint32_t dst_asid);

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * provides security isolation and access control logic.
  * No additional register callbacks beyond previous functionality are required
  * (ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID,
  * and RANGE_REGWEN already registered with lock protection).
  *
  * Security policy validation is performed by validate_security_policy()
  * called during transfer configuration validation ( integration).
  *
  * This function logs initialization.
  */

  // =========================================================================
  // Transfer Control and Abort
  // =========================================================================

  /**
  * @brief Register callbacks with regmodel memory (if needed)
  *
  * implements Transfer Control and Abort functionality through
  * enhanced behavior in existing CONTROL and STATUS register callbacks.
  * No new callbacks are required - behavior is integrated into:
  * - handle_write_CONTROL(): Implements go-bit transfer initiation with
  *  validation triggering and abort-bit emergency termination
  * - handle_read_CFG_REGWEN(): Returns lock status based on m_dma_busy state
  * - handle_read_STATUS(): Returns busy/done/aborted status bits
  *
  * Key behaviors implemented:
  * 1. Software-Initiated Transfer Control (go bit):
  *  - Triggers validate_transfer_configuration() ( + )
  *  - On validation pass: Sets m_dma_busy, locks CFG_REGWEN, sets STATUS.busy
  *  - On validation fail: Remains IDLE, sets STATUS.error, clears go bit
  *
  * 2. Transfer Abort Capability (abort bit):
  *  - Immediately halts transfer engine (stops issuing new transactions)
  *  - Clears m_dma_busy, unlocks CFG_REGWEN (returns to 0x6)
  *  - Sets STATUS.aborted bit, clears STATUS.busy bit
  *  - Clears go bit from CONTROL register
  *
  * 3. State Machine Lifecycle Management:
  *  - IDLE state: m_dma_busy=false, CFG_REGWEN=0x6, STATUS.busy=0
  *  - BUSY state: m_dma_busy=true, CFG_REGWEN=0x9, STATUS.busy=1
  *  - Transitions: IDLE→BUSY (go bit), BUSY→IDLE (abort or completion)
  *
  * 4. Configuration Validation Triggering:
  *  - Automatically calls validate_transfer_configuration() on go-bit write
  *  - Validation includes: opcode, width, size, alignment, ASID, security policy
  *  - Prevents transfer start if any validation fails
  *
  * 5. Transaction Drain Guarantees:
  *  - OT internal bus: Guaranteed completion (blocking b_transport semantics)
  *  - SoC buses (CTN/System): No guarantees per specification
  *
  * Per detailed design Section 7.3: "DMA operates as a state machine with
  * IDLE, CONFIG_CHECK, TRANSFER_SETUP, and execution states. go bit triggers
  * state machine transition. abort bit immediately terminates operation with
  * transaction completion guarantees for OpenTitan-internal buses."
  *
  * Per functionality list "Provides software-initiated transfer
  * control through go-bit start mechanism and abort-bit emergency termination.
  * Implements state machine lifecycle management including idle-to-active
  * transitions, configuration validation triggering, and abort-to-idle recovery
  * with transaction completion guarantees for OpenTitan-internal operations."
  *
  * Integration with other functionalities:
  * - CFG_REGWEN locking mechanism (is_cfg_locked() checks m_dma_busy)
  * - STATUS register RW1C clearing (done/aborted bits)
  * - validate_transfer_configuration() called on go-bit write
  * - Security validation integrated into validation
  * - Transfer engine execution (will be triggered by go-bit in future)
  */

  // =========================================================================
  // DMA Transfer Engine Operation
  // =========================================================================

  /**
  * @brief Execute single read-write transaction pair ( helper)
  * @return true if transaction successful, false if error occurred
  *
  * Performs one read-write cycle:
  * 1. Reads data from source address using source bus interface
  * 2. Writes data to destination address using destination bus interface
  * 3. Handles TLM response errors
  *
  * Uses m_transfer_data_buffer for transaction data storage.
  * Updates ERROR_CODE and triggers error interrupt on bus errors.
  *
  * Per detailed design Section 7.3: "Sequential read-from-source followed
  * by write-to-destination transaction pairs for each data word."
  */
  bool execute_single_transaction();

  /**
  * @brief Execute complete DMA transfer with chunked decomposition ( core)
  * @return true if transfer completed successfully, false if aborted
  *
  * Main transfer execution loop implementing:
  * - Transfer setup: Initialize address pointers, byte counters
  * - Transaction loop: Sequential read-write pairs until transfer complete
  * - Address advancement: Use methods per addressing mode
  * - Chunk tracking: Monitor chunk boundaries, trigger chunk_done interrupts
  * - Progress tracking: Decrement byte counters, update address registers
  * - Completion handling: Set STATUS.done, trigger done interrupt
  * - Abort monitoring: Check m_transfer_abort_event, handle graceful termination
  *
  * Per detailed design: "DMA engine processes TOTAL_DATA_SIZE bytes in
  * CHUNK_DATA_SIZE increments, generating chunk_done interrupt after each
  * chunk completion (non-handshake mode only)."
  *
  * Integration points:
  * - get_transfer_width_bytes() for transaction sizing
  * - advance_source_address(), advance_destination_address()
  * - select_bus_for_transaction(), validate_address_width_for_bus()
  * - set_interrupt_state(), clear_interrupt_state()
  */
  bool execute_transfer();

  /**
  * @brief Register callbacks and initialize transfer engine
  *
  * implements DMA Transfer Engine Operation through:
  * - transfer_engine_thread(): Autonomous transaction sequencing
  * - Integration with go-bit triggering via m_transfer_start_event
  * - Integration with abort via m_transfer_abort_event
  *
  * No additional register callbacks required beyond previous functionality through.
  *
  * Key behaviors:
  * 1. Memory-to-Memory Transfer Mode:
  *  - Both source and destination addresses increment (typical case)
  *  - Sequential read-write pairs from source to destination
  *
  * 2. Memory-to-Peripheral Transfer Mode:
  *  - Source address increments, destination address fixed
  *  - Reads sequential memory, writes to constant peripheral FIFO address
  *
  * 3. Peripheral-to-Memory Transfer Mode:
  *  - Source address fixed, destination address increments
  *  - Reads constant peripheral FIFO address, writes to sequential memory
  *
  * 4. Chunked Transfer Mechanism:
  *  - Decomposes TOTAL_DATA_SIZE into CHUNK_DATA_SIZE increments
  *  - Generates chunk_done interrupt after each chunk (non-handshake mode)
  *  - Updates address registers dynamically for software visibility
  *
  * 5. Transfer Progress Tracking:
  *  - Maintains m_bytes_remaining, m_current_chunk_bytes_remaining
  *  - Updates SRC_ADDR_HI:LO, DST_ADDR_HI:LO after each transaction
  *
  * 6. Autonomous Operation:
  *  - Operates independently after go-bit assertion
  *  - No CPU intervention required during transfer execution
  *  - Respects abort requests via m_transfer_abort_event
  *
  * 7. Completion Notification:
  *  - Sets STATUS.done on transfer completion
  *  - Triggers dma_done interrupt via 
  *  - Clears m_dma_busy, unlocks CFG_REGWEN (returns to IDLE)
  *
  * Per detailed design Section 7.3: "Transfer engine implements state machine
  * with SRC_READ → DST_WRITE → Address Update → Chunk Check → Completion cycles."
  *
  * Per functionality list "Implements core data movement engine for
  * autonomous read-modify-write transaction sequencing with chunk-based transfer
  * decomposition, sequential transaction generation, and progress tracking."
  *
  * Integration with other functionalities:
  * - Reads all transfer configuration registers
  * - Triggers interrupts (done, chunk_done, error)
  * - Uses get_transfer_width_bytes() for transaction sizing
  * - Uses advance_source_address(), advance_destination_address()
  * - Uses select_bus_for_transaction(), TLM socket routing
  * - Error detection and ERROR_CODE population
  * - Security validation already performed before transfer starts
  * - Triggered by go-bit, respects abort requests
  */

  // =========================================================================
  // Inline SHA-2 Hash Computation
  // =========================================================================

  /**
  * @brief Initialize SHA-2 hash engine for new transfer
  * @param opcode Hash algorithm opcode (0x1=SHA256, 0x2=SHA384, 0x3=SHA512)
  * @return true if initialization successful, false if invalid opcode
  *
  * Initializes SHA-2 hash context based on selected algorithm:
  * - Creates new EVP_MD_CTX context using OpenSSL
  * - Selects appropriate hash algorithm (SHA256/384/512)
  * - Initializes hash state to algorithm's initial values (IV)
  * - Clears any accumulated data from previous computation
  * - Sets m_hashing_active flag to true
  * - Clears STATUS.sha2_digest_valid bit
  *
  * Called when CONTROL.go is written with:
  * - CONTROL.opcode in {0x1, 0x2, 0x3} (hash modes)
  * - CONTROL.initial_transfer = 1 (reset hash state)
  *
  * Per detailed design Section 7.6: "Reset SHA-2 internal state to algorithm's
  * initial values (IV) and clear any accumulated data."
  *
  * Per functionality list "Hash State Initialization Control provides
  * software control over hash state reset through CONTROL.initial_transfer bit."
  *
  * @note Must be called before any hash_update_data() calls
  * @note Uses OpenSSL EVP_DigestInit_ex() for algorithm-agnostic initialization
  */
  bool hash_init(uint32_t opcode);

  /**
  * @brief Feed data word to SHA-2 hash engine during transfer
  * @param data Pointer to 4-byte data word read from source
  * @param length Number of bytes to hash (must be 4 for inline hashing)
  * @return true if update successful, false on error
  *
  * Incrementally updates SHA-2 hash state with transferred data:
  * - Feeds 4-byte data word to hash engine's update function
  * - Maintains hash state across multiple update calls
  * - No digest output produced (incremental accumulation only)
  *
  * Called during transfer execution for each read transaction when:
  * - m_hashing_active = true (hash mode transfer)
  * - After successful source read, before destination write
  * - Data is in m_transfer_data_buffer (4 bytes)
  *
  * Multi-chunk behavior:
  * - Maintains hash state across chunk boundaries
  * - Accumulates data from all chunks when initial_transfer=0
  * - Only first chunk initializes hash (initial_transfer=1)
  *
  * Per detailed design Section 7.6: "Feed each 4-byte data word read from
  * source to hash engine's update function. Hash state is updated incrementally
  * without producing output."
  *
  * Per functionality list "Hash Streaming Across Multiple Transfers
  * supports multi-chunk hash accumulation by maintaining internal hash state
  * between chunks when initial_transfer=0."
  *
  * @note Uses OpenSSL EVP_DigestUpdate() for incremental hashing
  * @note Does NOT modify transferred data (pass-through to destination)
  */
  bool hash_update_data(const unsigned char* data, uint32_t length);

  /**
  * @brief Finalize SHA-2 hash computation and store digest
  * @return true if finalization successful, false on error
  *
  * Completes SHA-2 hash computation and stores digest in registers:
  * - Calls hash engine's finalization function to complete computation
  * - Retrieves digest (32 bytes SHA-256, 48 bytes SHA-384, 64 bytes SHA-512)
  * - Applies endianness conversion if CONTROL.digest_swap=1 (byte-swap per word)
  * - Stores digest words into SHA2_DIGEST_0 through SHA2_DIGEST_15 registers
  * - Sets STATUS.sha2_digest_valid = 1 to indicate digest validity
  * - Frees hash context and clears m_hashing_active flag
  *
  * Called on transfer completion (m_bytes_remaining == 0) when:
  * - m_hashing_active = true (hash mode transfer)
  * - All data has been fed to hash engine via hash_update_data()
  *
  * Endianness control:
  * - If CONTROL.digest_swap=1: Byte-swap each 32-bit digest word to big-endian
  * - If CONTROL.digest_swap=0: Store digest words in little-endian (native)
  * - Register order unchanged: SHA2_DIGEST_0 always contains first 4 bytes
  *
  * Digest register usage:
  * - SHA-256: Updates SHA2_DIGEST_0 through SHA2_DIGEST_7 (8 registers)
  * - SHA-384: Updates SHA2_DIGEST_0 through SHA2_DIGEST_11 (12 registers)
  * - SHA-512: Updates SHA2_DIGEST_0 through SHA2_DIGEST_15 (16 registers)
  *
  * Per detailed design Section 7.6: "Call hash engine's finalization function,
  * retrieve digest, apply endianness conversion if digest_swap=1, store into
  * SHA2_DIGEST registers, set sha2_digest_valid=1."
  *
  * Per functionality list "Digest Byte-Swap Endianness Control provides
  * configurable per-register byte order conversion through CONTROL.digest_swap bit."
  *
  * @note Uses OpenSSL EVP_DigestFinal_ex() for finalization
  * @note Digest persists until next transfer with initial_transfer=1
  */
  bool hash_finalize();

  /**
  * @brief Apply byte-swap endianness conversion to 32-bit word ( helper)
  * @param word 32-bit word to byte-swap
  * @return Byte-swapped 32-bit word
  *
  * Converts between little-endian and big-endian representations:
  * - Input: 0xAABBCCDD
  * - Output: 0xDDCCBBAA
  *
  * Used when CONTROL.digest_swap=1 to convert each SHA2_DIGEST register
  * value to big-endian format before storing.
  *
  * Per detailed design Section 1.8.2: "digest_swap bit controls endianness
  * of each individual 32-bit digest register. When set to 1, each register's
  * bytes are converted to big-endian order."
  */
  uint32_t byte_swap_32(uint32_t word);

  /**
  * @brief Register (Inline SHA-2 Hash Computation) callbacks
  *
  * integrates SHA-2 hash computation into transfer datapath through:
  * - Hash initialization in handle_write_CONTROL() when initial_transfer=1
  * - Hash state reset control via CONTROL.initial_transfer bit
  * - Incremental hash updates in execute_single_transaction()
  * - Hash finalization in execute_transfer() on transfer completion
  * - Digest register population with endianness control
  * - STATUS.sha2_digest_valid management (set on completion, clear on init)
  * - 4-byte transfer width enforcement via validation
  *
  * No new register callbacks required beyond existing infrastructure.
  * Hash computation integrates transparently into transfer engine.
  *
  * Key behaviors:
  * 1. SHA-256 Hash Computation (opcode=0x1):
  *  - Produces 256-bit digest in SHA2_DIGEST_0-7
  *  - Requires TRANSFER_WIDTH=FOUR_BYTE (enforced by )
  *
  * 2. SHA-384 Hash Computation (opcode=0x2):
  *  - Produces 384-bit digest in SHA2_DIGEST_0-11
  *  - Requires TRANSFER_WIDTH=FOUR_BYTE
  *
  * 3. SHA-512 Hash Computation (opcode=0x3):
  *  - Produces 512-bit digest in SHA2_DIGEST_0-15
  *  - Requires TRANSFER_WIDTH=FOUR_BYTE
  *
  * 4. Hash State Initialization Control:
  *  - CONTROL.initial_transfer=1: Resets hash state (first chunk)
  *  - CONTROL.initial_transfer=0: Continues hash accumulation (subsequent chunks)
  *
  * 5. Multi-Chunk Hash Accumulation:
  *  - Maintains hash state between chunks for large data sets
  *  - Only finalizes digest after last chunk (m_bytes_remaining==0)
  *
  * 6. Digest Endianness Control:
  *  - CONTROL.digest_swap=1: Byte-swap each 32-bit digest word (big-endian)
  *  - CONTROL.digest_swap=0: Native byte order (little-endian)
  *
  * 7. Digest Validity Indication:
  *  - STATUS.sha2_digest_valid=1 when digest is ready
  *  - STATUS.sha2_digest_valid=0 cleared on new transfer with initial_transfer=1
  *
  * 8. Abort Handling:
  *  - Transfer abort clears sha2_digest_valid (digest invalid)
  *  - Frees hash context, prevents partial digest exposure
  *
  * Per detailed design Section 1.8.2: "As DMA reads data from source, it feeds
  * each data word to SHA-2 accelerator while simultaneously writing to destination.
  * Hash state is updated incrementally with each transferred word."
  *
  * Per functionality list "Provides concurrent cryptographic hash
  * computation during data transfer operations through integrated SHA-2 accelerator
  * supporting SHA-256, SHA-384, and SHA-512 algorithms."
  *
  * Integration with other functionalities:
  * - Uses CONTROL.opcode, initial_transfer, digest_swap fields
  * - Enforces FOUR_BYTE transfer width constraint via validation
  * - size_error triggered if hash opcode used with non-4-byte width
  * - Hash initialization on go-bit, abort handling
  * - Hash updates in transfer loop, finalization on completion
  *
  * @note Uses OpenSSL EVP API for SHA-2 computation (libcrypto)
  * @note Hash context allocated on initialization, freed on finalization/abort
  */

  // =========================================================================
  // Hardware Handshaking Mechanism Helper Functions
  // =========================================================================

  /**
  * @brief Perform automatic interrupt clearing write for trigger source
  * @param trigger_index Index of trigger source (0-10)
  * @return true if clearing write successful, false on error
  *
  * Executes configurable write transaction to peripheral interrupt register:
  * 1. Reads CLEAR_INTR_SRC register - checks if automatic clearing enabled for this trigger
  * 2. If CLEAR_INTR_SRC[trigger_index]=1:
  *  a. Reads CLEAR_INTR_BUS[trigger_index] to select bus (0=CTN/System, 1=OT-internal)
  *  b. Reads INTR_SRC_ADDR[trigger_index] for clearing write destination address
  *  c. Reads INTR_SRC_WR_VAL[trigger_index] for clearing write data value
  *  d. Creates TLM write transaction to INTR_SRC_ADDR with INTR_SRC_WR_VAL
  *  e. Routes transaction via selected bus interface socket
  * 3. Returns false if bus error occurs, true otherwise
  *
  * Used in hardware handshake mode to automatically acknowledge peripheral interrupts
  * without software intervention, allowing continuous autonomous chunk transfers.
  *
  * Per detailed design Section 1.7.3: "When an enabled lsio_trigger input asserts
  * and CLEAR_INTR_SRC is enabled for that trigger, the DMA automatically performs
  * a write transaction to the configured address with the configured data value."
  *
  * Per functionality list "Configurable Interrupt Acknowledgment Mechanism:
  * Implements optional automatic interrupt clearing through programmable write
  * transactions: configures destination address, write data, and bus selection
  * per trigger source."
  *
  * @note This is NOT a register callback - it is a helper function called by
  *    handshake_monitor_thread() when trigger assertion detected
  * @note Clearing write occurs BEFORE chunk transfer initiation
  * @note Bus selection: 0=use ADDR_SPACE_ID routing, 1=force OT-internal bus
  */
  bool perform_interrupt_clearing_write(int trigger_index);

  /**
   * @brief Latch ERROR_CODE.bus_error and return the engine to idle
   *
   * Used when a failed automatic interrupt-clearing write has to stop the
   * transfer before any chunk data moves.
   */
  void halt_transfer_on_bus_error();

  /**
  * @brief Register (Hardware Handshaking Mechanism) callbacks
  *
  * enables autonomous peripheral FIFO servicing through level-sensitive
  * trigger inputs from low-speed I/O devices. Implements trigger-driven chunk
  * transfer initiation, optional automatic peripheral interrupt acknowledgment
  * writes, and continuous operation until total size completion.
  *
  * Key behaviors:
  * 1. Level-Sensitive Trigger Input Monitoring:
  *  - Monitors 11 lsio_trigger[10:0] input ports continuously
  *  - Sensitive to all trigger input signal changes
  *  - Detects rising edges when hardware handshake mode enabled
  *
  * 2. Hardware Handshake Enable/Disable Control:
  *  - CONTROL.hardware_handshake_enable bit activates handshaking mode
  *  - HANDSHAKE_INTR_ENABLE[10:0] enables individual trigger sources
  *  - Trigger assertion initiates chunk transfer only if both bits set
  *
  * 3. Autonomous Chunk Transfer Initiation:
  *  - Trigger assertion → automatic chunk transfer start (no software intervention)
  *  - Transfer engine executes CHUNK_DATA_SIZE bytes per trigger event
  *  - Waits for next trigger assertion before starting next chunk
  *
  * 4. Configurable Interrupt Acknowledgment:
  *  - CLEAR_INTR_SRC[N]=1 enables automatic clearing for trigger N
  *  - CLEAR_INTR_BUS[N] selects bus (0=CTN/System, 1=OT-internal)
  *  - INTR_SRC_ADDR_N specifies peripheral interrupt register address
  *  - INTR_SRC_WR_VAL_N specifies clearing write data value
  *  - Clearing write executed BEFORE chunk transfer begins
  *
  * 5. Continuous Operation Until Total Size Completion:
  *  - Go bit remains set throughout hardware handshake mode
  *  - DMA processes triggers until TOTAL_DATA_SIZE bytes transferred
  *  - STATUS.done asserted on completion, but go bit persists
  *  - Software must explicitly clear go bit to stop handshaking
  *
  * 6. Chunk_done Interrupt Suppression:
  *  - chunk_done interrupt NOT generated in hardware handshake mode
  *  - Only done interrupt asserted on TOTAL_DATA_SIZE completion
  *  - Per detailed design: "chunk_done only asserted in memory-to-memory mode"
  *
  * Per detailed design Section 1.7.1: "When an enabled lsio_trigger input line
  * asserts, the DMA controller initiates a chunk transfer. The DMA reads or writes
  * CHUNK_DATA_SIZE bytes between the peripheral and memory. The peripheral is
  * expected to automatically deassert its trigger signal once the FIFO condition
  * is resolved."
  *
  * Per functionality list "Hardware Handshake Trigger Mechanism for
  * Peripheral FIFO Service: Responds to lsio_trigger[10:0] input assertions by
  * autonomously initiating chunk transfers, enabling DMA-driven FIFO drain/fill
  * operations without per-chunk software intervention."
  *
  * Integration with other functionalities:
  * - Uses HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS registers
  * - Uses INTR_SRC_ADDR[0-10], INTR_SRC_WR_VAL[0-10] register arrays
  * - Suppresses chunk_done interrupt generation in handshake mode
  * - Uses bus routing logic for interrupt clearing writes
  * - Go-bit persistence (does NOT auto-clear in handshake mode)
  * - Trigger event notifies transfer engine to execute chunk
  *
  * @note handshake_monitor_thread() sensitivity list includes all 11 lsio_trigger ports
  * @note Trigger inputs are level-sensitive (active-high), not edge-triggered
  * @note No register callbacks needed - all configuration registers are storage-only
  */
};
