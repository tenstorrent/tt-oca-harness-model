/**
 * @file dma.cpp
 * @brief DMA Controller model implementation
 *
 * Implements the DMA Controller functional behavior including port
 * initialization, reset handling, hardware handshake monitoring, and interrupt
 * generation.
 */

#include "secure_dma.h"
#include "sep_axi_extension.h"
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sstream>

// ============================================================================
// Constructor
// ============================================================================

secure_dma_model::secure_dma_model(sc_module_name n)
    : secure_dma_base(n, 0x150), // 0x150 = 336 bytes for register space
      ot_initiator_socket("ot_initiator_socket"),
      ctn_initiator_socket("ctn_initiator_socket"),
      sys_initiator_socket("sys_initiator_socket"),
      dma_done_intr("dma_done_intr"),
      dma_chunk_done_intr("dma_chunk_done_intr"),
      dma_error_intr("dma_error_intr"), alert_fatal_fault("alert_fatal_fault"),
      clk_i("clk_i"), rst_ni("rst_ni"), verbosity("verbosity", CSML_DEFAULT_VERBOSITY),
      logger(), m_dma_busy(false),
      m_dma_busy_next(false), m_intr_state(0x0), m_intr_state_next(0x0),
      m_error_code(0x0), m_error_code_next(0x0), m_current_src_addr(0x0),
      m_current_dst_addr(0x0), m_chunk_start_src_addr(0x0),
      m_chunk_start_dst_addr(0x0), m_bytes_remaining(0),
      m_current_chunk_bytes_remaining(0), m_hash_ctx(nullptr),
      m_hash_algorithm(0), m_hashing_active(false),
      m_last_asserted_trigger_index(-1) {

  // Initialize CSML logger
  logger.setMaxVerbosity(verbosity.get_param_value());
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  CSML_INFO(1, logger) << "DMA Controller model instantiated" << std::endl;

  // Initialize quantum keeper for temporal decoupling
  m_qk.reset();

  // Initialize transfer data buffer
  std::memset(m_transfer_data_buffer, 0, sizeof(m_transfer_data_buffer));

  // Initialize hash digest buffer
  std::memset(m_hash_digest, 0, sizeof(m_hash_digest));

  // Initialize trigger state tracking
  for (int i = 0; i < 11; i++) {
    m_prev_trigger_state[i] = false;
  }

  // Register all callbacks with CSML memory (consolidated from all
  // functionalities)
  register_all_callbacks();

  // Register SystemC threads
  SC_THREAD(reset_thread);
  sensitive << rst_ni;

  SC_THREAD(handshake_monitor_thread);
  // Make sensitive to all 11 lsio_trigger inputs for edge detection
  for (int i = 0; i < 11; i++) {
    sensitive << lsio_trigger[i];
  }

  SC_THREAD(transfer_engine_thread);
  // Note: Triggered by m_transfer_start_event from CONTROL.go write or
  // handshake trigger

  SC_METHOD(interrupt_driver_method);
  sensitive << m_interrupt_update_event;
  dont_initialize();
}

// ============================================================================
// Destructor
// ============================================================================

secure_dma_model::~secure_dma_model() {
  CSML_INFO(1, logger) << "DMA Controller model destroyed" << std::endl;
}

// ============================================================================
// Reset Thread
// ============================================================================

void secure_dma_model::reset_thread() {
  while (true) {
    wait();

    // Check for reset assertion (active-low)
    if (rst_ni.read() == false) {
      CSML_INFO(1, logger) << "Reset asserted - resetting all registers and state" << std::endl;

      // Reset all registers to default values
      reset_all_registers();

      // Reset internal state variables
      m_dma_busy = false;
      m_dma_busy_next = false;
      m_intr_state = 0x0;
      m_intr_state_next = 0x0;
      m_error_code = 0x0;
      m_error_code_next = 0x0;

      // Reset transfer engine state variables
      m_current_src_addr = 0x0;
      m_current_dst_addr = 0x0;
      m_chunk_start_src_addr = 0x0;
      m_chunk_start_dst_addr = 0x0;
      m_bytes_remaining = 0;
      m_current_chunk_bytes_remaining = 0;
      std::memset(m_transfer_data_buffer, 0, sizeof(m_transfer_data_buffer));

      // Reset SHA-2 hash engine state variables
      if (m_hash_ctx != nullptr) {
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(m_hash_ctx));
        m_hash_ctx = nullptr;
      }
      m_hash_algorithm = 0;
      m_hashing_active = false;
      std::memset(m_hash_digest, 0, sizeof(m_hash_digest));

      // Reset hardware handshake state variables
      m_last_asserted_trigger_index = -1;
      for (int i = 0; i < 11; i++) {
        m_prev_trigger_state[i] = false;
      }

      // Ensure any active transfer is aborted
      m_transfer_abort_event.notify(SC_ZERO_TIME);

      // Clear internal interrupt state (signals will be updated by
      // transfer_engine_thread)
      m_intr_state = 0x0;
      // Trigger interrupt driver to update output ports (clear interrupts)
      m_interrupt_update_event.notify(SC_ZERO_TIME);
      // Note: Interrupt output signals not cleared here to avoid multiple
      // driver conflict Transfer engine will update them on next operation
      alert_fatal_fault.write(
          false); // Alert signal (separate from interrupt outputs)

      // Reset quantum keeper
      m_qk.reset();

      CSML_INFO(1, logger) << "Reset complete" << std::endl;
    }
  }
}

// ============================================================================
// Hardware Handshake Monitor Thread
// ============================================================================

/**
 * @brief Hardware handshake monitoring thread
 *
 * Monitors lsio_trigger inputs and initiates DMA chunk transfers when enabled
 * triggers are asserted. Implements level-sensitive trigger detection with edge
 * detection logic to identify rising edges for chunk transfer initiation.
 *
 * Thread behavior:
 * 1. Wait for any lsio_trigger signal change (sensitivity list includes all 11
 * triggers)
 * 2. Read CONTROL.hardware_handshake_enable to check if handshaking mode active
 * 3. Read HANDSHAKE_INTR_ENABLE register for per-trigger enable mask
 * 4. For each trigger, detect rising edge by comparing current vs. previous
 * state
 * 5. If rising edge detected on enabled trigger AND handshake mode enabled:
 * a. Perform automatic interrupt clearing write (if CLEAR_INTR_SRC[i]=1)
 * b. Notify transfer engine to execute chunk transfer via
 * m_handshake_trigger_event c. Transfer engine will execute CHUNK_DATA_SIZE
 * bytes per trigger event
 * 6. Update previous trigger states for next edge detection cycle
 *
 * Per detailed design Section 1.7.1: "When an enabled lsio_trigger input line
 * asserts (indicating the peripheral FIFO has reached a threshold), the DMA
 * controller initiates a chunk transfer."
 *
 * Per functionality list "Level-Sensitive Interrupt Input Processing:
 * Monitors 11 level-sensitive trigger inputs continuously while hardware
 * handshake mode is enabled, detecting peripheral FIFO threshold conditions
 * signaled through trigger assertion."
 *
 * Integration with transfer engine :
 * - In normal mode: m_transfer_start_event triggered by CONTROL.go write
 * - In handshake mode: m_transfer_start_event triggered by this thread on
 * trigger rising edge
 * - Transfer engine executes ONE chunk per trigger event
 * - After chunk completion, engine returns to wait state until next trigger
 *
 * @note This thread is sensitive to all 11 lsio_trigger input ports
 * @note Edge detection prevents duplicate chunk initiations for level-held
 * triggers
 * @note Peripheral expected to deassert trigger after FIFO serviced (falling
 * edge)
 */
void secure_dma_model::handshake_monitor_thread() {
  while (true) {
    wait(); // Wait for any lsio_trigger signal change

    // Read CONTROL register to check if hardware handshake mode is enabled
    uint32_t control_reg = static_cast<uint32_t>(CONTROL);
    bool hardware_handshake_enable = (control_reg & (1U << 4)) != 0; // Bit 4

    if (!hardware_handshake_enable) {
      // Hardware handshake mode not enabled - ignore trigger inputs
      continue;
    }

    // Check if DMA is busy (transfer configuration already set)
    if (!m_dma_busy) {
      // DMA not active - cannot process triggers until go bit set
      continue;
    }

    // Read hardware handshake interrupt enable register (11-bit mask)
    uint32_t enable_mask = static_cast<uint32_t>(HANDSHAKE_INTR_ENABLE) & 0x7FF;

    // Scan all 11 trigger inputs for rising edges
    for (int i = 0; i < 11; i++) {
      bool current_state = lsio_trigger[i].read();
      bool prev_state = m_prev_trigger_state[i];

      // Detect rising edge (transition from low to high)
      bool rising_edge = current_state && !prev_state;

      // Update previous state for next cycle
      m_prev_trigger_state[i] = current_state;

      // Check if this trigger is enabled and has rising edge
      if (rising_edge && (enable_mask & (1 << i))) {
        CSML_INFO(1, logger) << "Hardware handshake trigger " << i << " rising edge detected (enabled)" << std::endl;

        // Store trigger index for debugging/logging
        m_last_asserted_trigger_index = i;

        // -----------------------------------------------------------------------
        // AUTOMATIC INTERRUPT CLEARING (if enabled for this trigger)
        // -----------------------------------------------------------------------

        if (!perform_interrupt_clearing_write(i)) {
          CSML_INFO(1, logger) << "Interrupt clearing write failed for trigger " << i << " (bus error or not enabled)" << std::endl;
          // Continue with chunk transfer despite clearing failure
          // (peripheral may auto-deassert or not require explicit clearing)
        }

        // -----------------------------------------------------------------------
        // CHUNK TRANSFER INITIATION
        // -----------------------------------------------------------------------

        // Notify transfer engine to execute one chunk transfer
        // Transfer engine is waiting on m_transfer_start_event in hardware
        // handshake mode
        m_handshake_trigger_event.notify(SC_ZERO_TIME);
        m_transfer_start_event.notify(
            SC_ZERO_TIME); // Also notify main transfer event

        CSML_INFO(2, logger) << "Chunk transfer initiated for trigger " << i << " via m_handshake_trigger_event" << std::endl;

        // Note: Only process ONE trigger per wake-up cycle
        // If multiple triggers assert simultaneously, they will be processed
        // in subsequent wait() cycles
        break;
      }
    }
  }
}

// ============================================================================
// Interrupt Update Method
// ============================================================================

void secure_dma_model::update_interrupts() {
  // Use internal m_intr_state rather than reading from register
  // This ensures we see the most up-to-date value
  uint32_t intr_state = m_intr_state;
  uint32_t intr_enable = static_cast<uint32_t>(INTR_ENABLE);

  CSML_INFO(3, logger) << "Interrupts updated - STATE: 0x" << std::hex << intr_state << " ENABLE: 0x" << intr_enable << std::dec << std::endl;

  // Trigger interrupt driver method (single-writer pattern)
  m_interrupt_update_event.notify(SC_ZERO_TIME);
}

// ============================================================================
// Interrupt Driver Method (Single-Writer Pattern for)
// ============================================================================

void secure_dma_model::interrupt_driver_method() {
  uint32_t intr_state = m_intr_state;
  uint32_t intr_enable = static_cast<uint32_t>(INTR_ENABLE);

  dma_done_intr.write(((intr_state & 0x1) != 0) && ((intr_enable & 0x1) != 0));
  dma_chunk_done_intr.write(((intr_state & 0x2) != 0) &&
                            ((intr_enable & 0x2) != 0));
  dma_error_intr.write(((intr_state & 0x4) != 0) && ((intr_enable & 0x4) != 0));

  interrupt_updated_event.notify(2, SC_NS);
}

// ============================================================================
// Register Write Callback Implementations
// ============================================================================

bool secure_dma_model::handle_write_CONTROL(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "CONTROL write: 0x" << std::hex << value << std::dec << std::endl;
//FIX: now ctrl reserved bits are not writable 
  auto store_control = [this, write_mask](uint32_t raw_value) {
    CONTROL = (raw_value & write_mask);
  };

  // Extract go bit (bit 31)
  bool go_bit = (value & (1U << 31)) != 0;

  // Extract abort bit (bit 27)
  bool abort_bit = (value & (1U << 27)) != 0;

  // Side-effect: go bit triggers DMA operation
  if (go_bit && !m_dma_busy) {
    CSML_INFO(1, logger) << "CONTROL.go=1: Initiating DMA transfer control sequence" << std::endl;

    // Automatic clearing of STATUS.done when new transfer starts
    uint32_t current_status = static_cast<uint32_t>(STATUS);
    if (current_status & (1U << 1)) { // Check if done bit is set
      current_status &= ~(1U << 1);   // Clear STATUS.done
      STATUS = current_status;
      clear_interrupt_state(true, false, false); // Clear INTR_STATE.dma_done
    CSML_INFO(2, logger) << "Auto-cleared STATUS.done on new transfer start" << std::endl;
    }

    // Configuration validation triggering
    // Call comprehensive validation (includes security checks)
    if (!validate_transfer_configuration(value)) {
      CSML_INFO(1, logger) << "Configuration validation FAILED - transfer cannot start" << std::endl;
      // Validation failure sets STATUS.error and ERROR_CODE automatically in
      // Remain in IDLE state (m_dma_busy stays false)
      // Validation already triggered error interrupt via

      // Clear go bit from register (validation failed, transfer did not start)
      value &= ~(1U << 31);
      store_control(value);
      return false; // Return false since we manually updated the register
    }

    // Validation passed - transition to BUSY state
    CSML_INFO(1, logger) << "Configuration validation PASSED - entering BUSY state" << std::endl;

    // Set busy state (locks CFG_REGWEN to 0x0)
    m_dma_busy_next = true;
    m_dma_busy = m_dma_busy_next;

    // Set STATUS.busy bit (bit 0)
    current_status = static_cast<uint32_t>(STATUS);
    current_status |= 0x1; // Set STATUS.busy
    STATUS = current_status;

    CSML_INFO(2, logger) << "DMA entered BUSY state - CFG_REGWEN now locked (0x0)" << std::endl;

    // Initialize SHA-2 hash engine if hashing enabled and initial_transfer=1
    uint32_t opcode = value & 0xF;                    // Bits 3:0
    bool initial_transfer = (value & (1U << 8)) != 0; // Bit 8
    bool is_hash_opcode = (opcode >= 0x1 && opcode <= 0x3);

    if (is_hash_opcode && initial_transfer) {
      // Initialize hash engine for new hash computation
      if (!hash_init(opcode)) {
        CSML_INFO(1, logger) << "Hash engine initialization FAILED" << std::endl;
        // Hash init failure - transition back to IDLE, set error
        m_dma_busy = false;
        m_dma_busy_next = false;
        current_status = static_cast<uint32_t>(STATUS);
        current_status &= ~0x1;      // Clear STATUS.busy
        current_status |= (1U << 3); // Set STATUS.error
        STATUS = current_status;
        m_error_code |= (1U << 2); // Set opcode_error (hash init failed)
        ERROR_CODE = m_error_code;
        set_interrupt_state(false, false, true); // Trigger error interrupt
        value &= ~(1U << 31);                    // Clear go bit
        store_control(value);
        return false;
      }
    CSML_INFO(2, logger) << "Hash engine initialized (initial_transfer=1)" << std::endl;
    } else if (is_hash_opcode && !initial_transfer) {
      // Continue existing hash computation (multi-chunk accumulation)
      if (!m_hashing_active) {
        CSML_INFO(1, logger) << "WARNING - Hash opcode with initial_transfer=0 but no active hash context" << std::endl;
        // Initialize hash anyway to prevent errors
        if (!hash_init(opcode)) {
          CSML_INFO(1, logger) << "Hash engine initialization FAILED" << std::endl;
          m_dma_busy = false;
          m_dma_busy_next = false;
          current_status = static_cast<uint32_t>(STATUS);
          current_status &= ~0x1;
          current_status |= (1U << 3);
          STATUS = current_status;
          m_error_code |= (1U << 2);
          ERROR_CODE = m_error_code;
          set_interrupt_state(false, false, true);
          value &= ~(1U << 31);
          store_control(value);
          return false;
        }
      }
    CSML_INFO(2, logger) << "Continuing multi-chunk hash accumulation (initial_transfer=0)" << std::endl;
    }

    // Start the transfer engine. In hardware-handshake mode the first chunk
    // MUST wait for the peripheral's watermark trigger before draining —
    // arming alone must not drain, or the engine would read an empty peripheral
    // FIFO (which returns zero data) before the source has produced anything.
    // The handshake_monitor_thread notifies m_transfer_start_event on the first
    // enabled rising edge, so simply defer here. In non-handshake
    // (memory-to-memory) mode the source is always ready, so start immediately.
    const bool hardware_handshake_enable = (value & (1U << 4)) != 0; // Bit 4
    if (!hardware_handshake_enable) {
      m_transfer_start_event.notify(SC_ZERO_TIME);
      CSML_INFO(2, logger) << "Transfer engine triggered via m_transfer_start_event" << std::endl;
    } else {
      // Triggers are level-sensitive: if an enabled trigger is already asserted at arm time,
      // start immediately (otherwise wait for handshake_monitor_thread's rising edge).
      const uint32_t enable_mask = static_cast<uint32_t>(HANDSHAKE_INTR_ENABLE) & 0x7FF;
      bool already_high = false;
      for (int i = 0; i < 11; i++) {
        if ((enable_mask & (1U << i)) && lsio_trigger[i].read()) {
          already_high = true;
          break;
        }
      }
      if (already_high) {
        m_transfer_start_event.notify(SC_ZERO_TIME);
        CSML_INFO(2, logger) << "Handshake mode: trigger already HIGH on arm; starting first chunk immediately" << std::endl;
      } else {
        CSML_INFO(2, logger) << "Handshake mode: deferring first chunk until first watermark trigger" << std::endl;
      }
    }
  }

  // Side-effect: abort bit initiates transfer abort
  if (abort_bit && m_dma_busy) {
    CSML_INFO(1, logger) << "CONTROL.abort=1: Initiating transfer abort" << std::endl;

    // Immediately halt transfer engine
    // Per detailed design: "The abort operation takes effect immediately after
    // the write. The DMA stops issuing new transactions."

    // Transition to IDLE state
    m_dma_busy_next = false;
    m_dma_busy = m_dma_busy_next;

    // Clear STATUS.busy bit (bit 0)
    uint32_t current_status = static_cast<uint32_t>(STATUS);
    current_status &= ~0x1; // Clear STATUS.busy

    // Set STATUS.aborted bit (bit 2)
    current_status |= (1U << 2);
    STATUS = current_status;

    // Clear go bit (transfer aborted)
    value &= ~(1U << 31);

    CSML_INFO(1, logger) << "Transfer aborted - STATUS.aborted set, " << "returned to IDLE state, CFG_REGWEN unlocked (0x6)" << std::endl;

    // Clear hash state and sha2_digest_valid on abort
    if (m_hashing_active) {
      if (m_hash_ctx != nullptr) {
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(m_hash_ctx));
        m_hash_ctx = nullptr;
      }
      m_hashing_active = false;
      // Clear STATUS.sha2_digest_valid (bit 4)
      current_status &= ~(1U << 4);
      STATUS = current_status;
    CSML_INFO(2, logger) << "Hash state cleared on abort - sha2_digest_valid=0" << std::endl;
    }

    // Signal transfer engine thread to abort
    m_transfer_abort_event.notify(SC_ZERO_TIME);
    CSML_INFO(2, logger) << "Transfer engine notified of abort via m_transfer_abort_event" << std::endl;

    // Note: Per detailed design: "Any transactions already issued to
    // OpenTitan-internal buses are guaranteed to complete before the abort
    // operation is considered finished." In this TLM model, we use blocking
    // b_transport calls, so all OT transactions are guaranteed to complete by
    // the time we reach this point (blocking semantics). For SoC interfaces
    // (CTN/System), no guarantees are provided per specification.
  }

  // Update the CONTROL register
  store_control(value);

  // Return false since we manually updated the register
  return false;
}

bool secure_dma_model::handle_write_STATUS(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "STATUS write (RW1C): 0x" << std::hex << value << std::dec << std::endl;

  // Read current STATUS register value
  uint32_t current_status = static_cast<uint32_t>(STATUS);

  // RW1C semantics: Writing 1 clears the bit
  // Bit 1: done
  if (value & (1U << 1)) {
    current_status &= ~(1U << 1);
    m_intr_state &= ~(1U << 0); // Clear INTR_STATE.dma_done
  CSML_INFO(2, logger) << "STATUS.done cleared via RW1C" << std::endl;
  }

  // Bit 2: aborted
  if (value & (1U << 2)) {
    current_status &= ~(1U << 2);
  CSML_INFO(2, logger) << "STATUS.aborted cleared via RW1C" << std::endl;
  }

  // Bit 3: error (also clears ERROR_CODE)
  if (value & (1U << 3)) {
    current_status &= ~(1U << 3);
    m_intr_state &= ~(1U << 2); // Clear INTR_STATE.dma_error
    m_error_code = 0x0;         // Clear ERROR_CODE register
    m_error_code_next = 0x0;
  CSML_INFO(2, logger) << "STATUS.error cleared via RW1C (ERROR_CODE also cleared)" << std::endl;
  }

  // Bit 5: chunk_done
  if (value & (1U << 5)) {
    current_status &= ~(1U << 5);
    m_intr_state &= ~(1U << 1); // Clear INTR_STATE.dma_chunk_done
  CSML_INFO(2, logger) << "STATUS.chunk_done cleared via RW1C" << std::endl;
  }

  // Update STATUS register with cleared bits
  // Bits 0 (busy) and 4 (sha2_digest_valid) are read-only, not affected
  STATUS = current_status;

  // Update interrupt outputs
  update_interrupts();

  // Return false to prevent default write (we've already updated the register)
  return false;
}

/**
 * @brief Write callback for INTR_ENABLE register
 *
 * Handles updates to the interrupt enable register. CSML framework
 * automatically masks reserved bits via register definition (only bits [2:0]
 * writable). This callback updates interrupt outputs after the enable mask
 * changes.
 *
 * @param value Value written to the register
 * @param write_mask Write mask from register definition
 * @return true to allow CSML to store the value
 */
bool secure_dma_model::handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "INTR_ENABLE write: 0x" << std::hex << value << std::dec << std::endl;

  // Manually write register value before updating interrupts
  // This ensures update_interrupts() reads the NEW value, not the old value
  INTR_ENABLE = value & write_mask;

  // Update interrupt outputs based on new enable mask
  // Interrupt outputs are gated by enable bits: output = INTR_STATE AND
  // INTR_ENABLE
  update_interrupts();

  CSML_INFO(2, logger) << "Interrupt outputs updated based on new INTR_ENABLE mask" << std::endl;

  // Return false because we already wrote the register manually
  return false;
}

bool secure_dma_model::handle_write_INTR_TEST(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "INTR_TEST write: 0x" << std::hex << value << std::dec << std::endl;

  // Force interrupt state bits for testing
  // Bit 0: dma_done
  if (value & 0x1) {
    m_intr_state |= 0x1;
  CSML_INFO(2, logger) << "INTR_TEST: Forcing INTR_STATE.dma_done" << std::endl;
  }

  // Bit 1: dma_chunk_done
  if (value & 0x2) {
    m_intr_state |= 0x2;
  CSML_INFO(2, logger) << "INTR_TEST: Forcing INTR_STATE.dma_chunk_done" << std::endl;
  }

  // Bit 2: dma_error
  if (value & 0x4) {
    m_intr_state |= 0x4;
  CSML_INFO(2, logger) << "INTR_TEST: Forcing INTR_STATE.dma_error" << std::endl;
  }

  // Update interrupt outputs
  update_interrupts();

  // Return false - INTR_TEST is write-only transient, not stored
  return false;
}

bool secure_dma_model::handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "ALERT_TEST write: 0x" << std::hex << value << std::dec << std::endl;

  // Bit 0: Trigger fatal_fault alert
  if (value & 0x1) {
    CSML_INFO(1, logger) << "ALERT_TEST: Triggering fatal_fault alert" << std::endl;
    alert_fatal_fault.write(true);

    // Alert is a pulse - de-assert after one delta cycle
    wait(SC_ZERO_TIME);
    alert_fatal_fault.write(false);
  }

  // Return false - ALERT_TEST is write-only transient, not stored
  return false;
}

bool secure_dma_model::handle_write_RANGE_REGWEN(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "RANGE_REGWEN write: 0x" << std::hex << value << std::dec << std::endl;

  uint32_t current_regwen = static_cast<uint32_t>(RANGE_REGWEN);

  // RW0C semantics: Only allow transition from 0x6 (unlocked) to 0x0 (locked)
  // Once locked, cannot be unlocked until reset
  if (current_regwen == 0x6) {
    // Currently unlocked - allow write if value is 0x0
    if ((value & 0xF) == 0x0) {
      CSML_INFO(1, logger) << "RANGE_REGWEN: Locking memory range registers (0x6 -> 0x0)" << std::endl;
      RANGE_REGWEN = 0x0;
      return false; // We've updated the register
    } else if ((value & 0xF) == 0x6) {
      // Writing 0x6 when already 0x6 - no change
      CSML_INFO(3, logger) << "RANGE_REGWEN: Write 0x6 when unlocked - no change" << std::endl;
      return false;
    }
  } else {
    // Already locked (0x0) - ignore all writes
    CSML_INFO(2, logger) << "RANGE_REGWEN: Already locked - write ignored" << std::endl;
    return false;
  }

  // Allow default write for other cases
  return true;
}

bool secure_dma_model::handle_write_ENABLED_MEMORY_RANGE_BASE(uint32_t value,
                                                       uint32_t write_mask) {
  CSML_INFO(2, logger) << "ENABLED_MEMORY_RANGE_BASE write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if memory range registers are locked by RANGE_REGWEN
  if (is_range_locked()) {
    CSML_INFO(2, logger) << "ENABLED_MEMORY_RANGE_BASE: Write blocked - RANGE_REGWEN locked" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  ENABLED_MEMORY_RANGE_BASE = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_ENABLED_MEMORY_RANGE_LIMIT(uint32_t value,
                                                        uint32_t write_mask) {
  CSML_INFO(2, logger) << "ENABLED_MEMORY_RANGE_LIMIT write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if memory range registers are locked by RANGE_REGWEN
  if (is_range_locked()) {
    CSML_INFO(2, logger) << "ENABLED_MEMORY_RANGE_LIMIT: Write blocked - RANGE_REGWEN locked" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  ENABLED_MEMORY_RANGE_LIMIT = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_RANGE_VALID(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "RANGE_VALID write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if memory range registers are locked by RANGE_REGWEN
  if (is_range_locked()) {
    CSML_INFO(2, logger) << "RANGE_VALID: Write blocked - RANGE_REGWEN locked" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  RANGE_VALID = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_SRC_ADDR_LO(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "SRC_ADDR_LO write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "SRC_ADDR_LO: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  SRC_ADDR_LO = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_SRC_ADDR_HI(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "SRC_ADDR_HI write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "SRC_ADDR_HI: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  SRC_ADDR_HI = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_DST_ADDR_LO(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "DST_ADDR_LO write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "DST_ADDR_LO: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  DST_ADDR_LO = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_DST_ADDR_HI(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "DST_ADDR_HI write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "DST_ADDR_HI: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  DST_ADDR_HI = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_ADDR_SPACE_ID(uint32_t value,
                                           uint32_t write_mask) {
  CSML_INFO(2, logger) << "ADDR_SPACE_ID write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "ADDR_SPACE_ID: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  ADDR_SPACE_ID = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_TOTAL_DATA_SIZE(uint32_t value,
                                             uint32_t write_mask) {
  CSML_INFO(2, logger) << "TOTAL_DATA_SIZE write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "TOTAL_DATA_SIZE: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  TOTAL_DATA_SIZE = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_CHUNK_DATA_SIZE(uint32_t value,
                                             uint32_t write_mask) {
  CSML_INFO(2, logger) << "CHUNK_DATA_SIZE write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "CHUNK_DATA_SIZE: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  CHUNK_DATA_SIZE = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_TRANSFER_WIDTH(uint32_t value,
                                            uint32_t write_mask) {
  CSML_INFO(2, logger) << "TRANSFER_WIDTH write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "TRANSFER_WIDTH: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  TRANSFER_WIDTH = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_SRC_CONFIG(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "SRC_CONFIG write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "SRC_CONFIG: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  SRC_CONFIG = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_DST_CONFIG(uint32_t value, uint32_t write_mask) {
  CSML_INFO(2, logger) << "DST_CONFIG write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "DST_CONFIG: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  DST_CONFIG = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_HANDSHAKE_INTR_ENABLE(uint32_t value,
                                                   uint32_t write_mask) {
  CSML_INFO(2, logger) << "HANDSHAKE_INTR_ENABLE write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "HANDSHAKE_INTR_ENABLE: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  HANDSHAKE_INTR_ENABLE = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_CLEAR_INTR_SRC(uint32_t value,
                                            uint32_t write_mask) {
  CSML_INFO(2, logger) << "CLEAR_INTR_SRC write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "CLEAR_INTR_SRC: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  CLEAR_INTR_SRC = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_CLEAR_INTR_BUS(uint32_t value,
                                            uint32_t write_mask) {
  CSML_INFO(2, logger) << "CLEAR_INTR_BUS write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "CLEAR_INTR_BUS: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  CLEAR_INTR_BUS = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_INTR_SRC_ADDR(unsigned int index, uint32_t value,
                                           uint32_t write_mask) {
  CSML_INFO(2, logger) << "INTR_SRC_ADDR[" << index << "] write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "INTR_SRC_ADDR[" << index << "]: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  INTR_SRC_ADDR[index] = value;
  return false; // Return false since we manually updated
}

bool secure_dma_model::handle_write_INTR_SRC_WR_VAL(unsigned int index, uint32_t value,
                                             uint32_t write_mask) {
  CSML_INFO(2, logger) << "INTR_SRC_WR_VAL[" << index << "] write: 0x" << std::hex << value << std::dec << std::endl;

  // Check if configuration registers are locked by CFG_REGWEN
  if (is_cfg_locked()) {
    CSML_INFO(2, logger) << "INTR_SRC_WR_VAL[" << index << "]: Write blocked - CFG_REGWEN locked (DMA busy)" << std::endl;
    return false; // Block write
  }

  // Allow write if unlocked - manually update register
  INTR_SRC_WR_VAL[index] = value;
  return false; // Return false since we manually updated
}

// ============================================================================
// Register Read Callback Implementations
// ============================================================================

bool secure_dma_model::handle_read_CFG_REGWEN(uint32_t &value, uint32_t read_mask) {
  // Return hardware-managed lock status based on DMA busy state
  // 0x0 = locked (busy), 0x6 = unlocked (idle)
  value = m_dma_busy ? 0x0 : 0x6;

  CSML_INFO(3, logger) << "CFG_REGWEN read: 0x" << std::hex << value << " (DMA " << (m_dma_busy ? "BUSY" : "IDLE") << ")" << std::dec << std::endl;

  return true;
}

bool secure_dma_model::handle_read_STATUS(uint32_t &value, uint32_t read_mask) {
  // Read current STATUS register and update busy bit dynamically
  value = static_cast<uint32_t>(STATUS);

  // Update bit 0 (busy) based on internal state
  if (m_dma_busy) {
    value |= 0x1;
  } else {
    value &= ~0x1;
  }

  CSML_INFO(3, logger) << "STATUS read: 0x" << std::hex << value << std::dec << std::endl;

  return true;
}

bool secure_dma_model::handle_read_ERROR_CODE(uint32_t &value, uint32_t read_mask) {
  // Return internal error code storage
  value = m_error_code;

  CSML_INFO(3, logger) << "ERROR_CODE read: 0x" << std::hex << value << std::dec << std::endl;

  return true;
}

bool secure_dma_model::handle_read_INTR_STATE(uint32_t &value, uint32_t read_mask) {
  // Return internal interrupt state storage
  value = m_intr_state;

  CSML_INFO(3, logger) << "INTR_STATE read: 0x" << std::hex << value << std::dec << std::endl;

  return true;
}

bool secure_dma_model::handle_read_SHA2_DIGEST(unsigned int index, uint32_t &value,
                                        uint32_t read_mask) {
  // Read from SHA2_DIGEST array register
  value = static_cast<uint32_t>(SHA2_DIGEST[index]);

  CSML_INFO(3, logger) << "SHA2_DIGEST[" << index << "] read: 0x" << std::hex << value << std::dec << std::endl;

  // TODO: will implement actual SHA-2 hash engine
  // For now, just return stored value

  return true;
}

// ============================================================================
// Helper Methods
// ============================================================================

bool secure_dma_model::is_cfg_locked() const {
  // CFG_REGWEN is locked when DMA is busy
  return m_dma_busy;
}

bool secure_dma_model::is_range_locked() const {
  // RANGE_REGWEN is locked when register value is 0x0
  uint32_t regwen_value = static_cast<uint32_t>(RANGE_REGWEN);
  return (regwen_value & 0xF) == 0x0;
}

// ============================================================================
// Consolidated Callback Registration
// ============================================================================

void secure_dma_model::register_all_callbacks() {
  // CONTROL
  std::function<bool(uint32_t)> control_write = [this](uint32_t value) {
    return this->handle_write_CONTROL(value, CONTROL.write_bit_mask);
  };
  memory.register_write_callback(control_write, CONTROL.offset);

  // STATUS
  std::function<bool(uint32_t)> status_write = [this](uint32_t value) {
    return this->handle_write_STATUS(value, STATUS.write_bit_mask);
  };
  memory.register_write_callback(status_write, STATUS.offset);

  // INTR_TEST
  std::function<bool(uint32_t)> intr_test_write = [this](uint32_t value) {
    return this->handle_write_INTR_TEST(value, INTR_TEST.write_bit_mask);
  };
  memory.register_write_callback(intr_test_write, INTR_TEST.offset);

  // ALERT_TEST
  std::function<bool(uint32_t)> alert_test_write = [this](uint32_t value) {
    return this->handle_write_ALERT_TEST(value, ALERT_TEST.write_bit_mask);
  };
  memory.register_write_callback(alert_test_write, ALERT_TEST.offset);

  // RANGE_REGWEN
  std::function<bool(uint32_t)> range_regwen_write = [this](uint32_t value) {
    return this->handle_write_RANGE_REGWEN(value, RANGE_REGWEN.write_bit_mask);
  };
  memory.register_write_callback(range_regwen_write, RANGE_REGWEN.offset);

  // ENABLED_MEMORY_RANGE_BASE
  std::function<bool(uint32_t)> range_base_write = [this](uint32_t value) {
    return this->handle_write_ENABLED_MEMORY_RANGE_BASE(
        value, ENABLED_MEMORY_RANGE_BASE.write_bit_mask);
  };
  memory.register_write_callback(range_base_write,
                                 ENABLED_MEMORY_RANGE_BASE.offset);

  // ENABLED_MEMORY_RANGE_LIMIT
  std::function<bool(uint32_t)> range_limit_write = [this](uint32_t value) {
    return this->handle_write_ENABLED_MEMORY_RANGE_LIMIT(
        value, ENABLED_MEMORY_RANGE_LIMIT.write_bit_mask);
  };
  memory.register_write_callback(range_limit_write,
                                 ENABLED_MEMORY_RANGE_LIMIT.offset);

  // RANGE_VALID
  std::function<bool(uint32_t)> range_valid_write = [this](uint32_t value) {
    return this->handle_write_RANGE_VALID(value, RANGE_VALID.write_bit_mask);
  };
  memory.register_write_callback(range_valid_write, RANGE_VALID.offset);

  // SRC_ADDR_LO
  std::function<bool(uint32_t)> src_addr_lo_write = [this](uint32_t value) {
    return this->handle_write_SRC_ADDR_LO(value, SRC_ADDR_LO.write_bit_mask);
  };
  memory.register_write_callback(src_addr_lo_write, SRC_ADDR_LO.offset);

  // SRC_ADDR_HI
  std::function<bool(uint32_t)> src_addr_hi_write = [this](uint32_t value) {
    return this->handle_write_SRC_ADDR_HI(value, SRC_ADDR_HI.write_bit_mask);
  };
  memory.register_write_callback(src_addr_hi_write, SRC_ADDR_HI.offset);

  // DST_ADDR_LO
  std::function<bool(uint32_t)> dst_addr_lo_write = [this](uint32_t value) {
    return this->handle_write_DST_ADDR_LO(value, DST_ADDR_LO.write_bit_mask);
  };
  memory.register_write_callback(dst_addr_lo_write, DST_ADDR_LO.offset);

  // DST_ADDR_HI
  std::function<bool(uint32_t)> dst_addr_hi_write = [this](uint32_t value) {
    return this->handle_write_DST_ADDR_HI(value, DST_ADDR_HI.write_bit_mask);
  };
  memory.register_write_callback(dst_addr_hi_write, DST_ADDR_HI.offset);

  // ADDR_SPACE_ID
  std::function<bool(uint32_t)> addr_space_id_write = [this](uint32_t value) {
    return this->handle_write_ADDR_SPACE_ID(value,
                                            ADDR_SPACE_ID.write_bit_mask);
  };
  memory.register_write_callback(addr_space_id_write, ADDR_SPACE_ID.offset);

  // TOTAL_DATA_SIZE
  std::function<bool(uint32_t)> total_data_size_write = [this](uint32_t value) {
    return this->handle_write_TOTAL_DATA_SIZE(value,
                                              TOTAL_DATA_SIZE.write_bit_mask);
  };
  memory.register_write_callback(total_data_size_write, TOTAL_DATA_SIZE.offset);

  // CHUNK_DATA_SIZE
  std::function<bool(uint32_t)> chunk_data_size_write = [this](uint32_t value) {
    return this->handle_write_CHUNK_DATA_SIZE(value,
                                              CHUNK_DATA_SIZE.write_bit_mask);
  };
  memory.register_write_callback(chunk_data_size_write, CHUNK_DATA_SIZE.offset);

  // TRANSFER_WIDTH
  std::function<bool(uint32_t)> transfer_width_write = [this](uint32_t value) {
    return this->handle_write_TRANSFER_WIDTH(value,
                                             TRANSFER_WIDTH.write_bit_mask);
  };
  memory.register_write_callback(transfer_width_write, TRANSFER_WIDTH.offset);

  // SRC_CONFIG
  std::function<bool(uint32_t)> src_config_write = [this](uint32_t value) {
    return this->handle_write_SRC_CONFIG(value, SRC_CONFIG.write_bit_mask);
  };
  memory.register_write_callback(src_config_write, SRC_CONFIG.offset);

  // DST_CONFIG
  std::function<bool(uint32_t)> dst_config_write = [this](uint32_t value) {
    return this->handle_write_DST_CONFIG(value, DST_CONFIG.write_bit_mask);
  };
  memory.register_write_callback(dst_config_write, DST_CONFIG.offset);

  // HANDSHAKE_INTR_ENABLE
  std::function<bool(uint32_t)> handshake_intr_enable_write =
      [this](uint32_t value) {
        return this->handle_write_HANDSHAKE_INTR_ENABLE(
            value, HANDSHAKE_INTR_ENABLE.write_bit_mask);
      };
  memory.register_write_callback(handshake_intr_enable_write,
                                 HANDSHAKE_INTR_ENABLE.offset);

  // CLEAR_INTR_SRC
  std::function<bool(uint32_t)> clear_intr_src_write = [this](uint32_t value) {
    return this->handle_write_CLEAR_INTR_SRC(value,
                                             CLEAR_INTR_SRC.write_bit_mask);
  };
  memory.register_write_callback(clear_intr_src_write, CLEAR_INTR_SRC.offset);

  // CLEAR_INTR_BUS
  std::function<bool(uint32_t)> clear_intr_bus_write = [this](uint32_t value) {
    return this->handle_write_CLEAR_INTR_BUS(value,
                                             CLEAR_INTR_BUS.write_bit_mask);
  };
  memory.register_write_callback(clear_intr_bus_write, CLEAR_INTR_BUS.offset);

  // INTR_SRC_ADDR array
  for (unsigned int i = 0; i < 11; i++) {
    std::function<bool(uint32_t)> intr_src_addr_write = [this,
                                                         i](uint32_t value) {
      return this->handle_write_INTR_SRC_ADDR(i, value,
                                              INTR_SRC_ADDR[i].write_bit_mask);
    };
    memory.register_write_callback(intr_src_addr_write,
                                   INTR_SRC_ADDR[i].offset);
  }

  // INTR_SRC_WR_VAL array
  for (unsigned int i = 0; i < 11; i++) {
    std::function<bool(uint32_t)> intr_src_wr_val_write = [this,
                                                           i](uint32_t value) {
      return this->handle_write_INTR_SRC_WR_VAL(
          i, value, INTR_SRC_WR_VAL[i].write_bit_mask);
    };
    memory.register_write_callback(intr_src_wr_val_write,
                                   INTR_SRC_WR_VAL[i].offset);
  }

  // CFG_REGWEN (read)
  std::function<bool(uint32_t &)> cfg_regwen_read = [this](uint32_t &value) {
    return this->handle_read_CFG_REGWEN(value, CFG_REGWEN.read_bit_mask);
  };
  memory.register_read_callback(cfg_regwen_read, CFG_REGWEN.offset);

  // STATUS (read)
  std::function<bool(uint32_t &)> status_read = [this](uint32_t &value) {
    return this->handle_read_STATUS(value, STATUS.read_bit_mask);
  };
  memory.register_read_callback(status_read, STATUS.offset);

  // ERROR_CODE (read)
  std::function<bool(uint32_t &)> error_code_read = [this](uint32_t &value) {
    return this->handle_read_ERROR_CODE(value, ERROR_CODE.read_bit_mask);
  };
  memory.register_read_callback(error_code_read, ERROR_CODE.offset);

  // INTR_STATE (read)
  std::function<bool(uint32_t &)> intr_state_read = [this](uint32_t &value) {
    return this->handle_read_INTR_STATE(value, INTR_STATE.read_bit_mask);
  };
  memory.register_read_callback(intr_state_read, INTR_STATE.offset);

  // SHA2_DIGEST array (read)
  for (unsigned int i = 0; i < 16; i++) {
    std::function<bool(uint32_t &)> sha2_digest_read = [this,
                                                        i](uint32_t &value) {
      return this->handle_read_SHA2_DIGEST(i, value,
                                           SHA2_DIGEST[i].read_bit_mask);
    };
    memory.register_read_callback(sha2_digest_read, SHA2_DIGEST[i].offset);
  }

  // INTR_ENABLE
  std::function<bool(uint32_t)> intr_enable_write = [this](uint32_t value) {
    return this->handle_write_INTR_ENABLE(value, INTR_ENABLE.write_bit_mask);
  };
  memory.register_write_callback(intr_enable_write, INTR_ENABLE.offset);

  CSML_INFO(1, logger) << "All DMA callbacks registered (32 write, 5 read)" << std::endl;
}

// ============================================================================
// Callback Registration

// ============================================================================
// Interrupt Management Callback Registration

// ============================================================================
// Interrupt Management Helper Methods
// ============================================================================

void secure_dma_model::set_interrupt_state(bool done, bool chunk_done, bool error) {
  // Update internal interrupt state bits
  if (done) {
    m_intr_state |= (1U << 0); // Set dma_done bit
    // Also set STATUS.done bit
    uint32_t current_status = static_cast<uint32_t>(STATUS);
    current_status |= (1U << 1);
    STATUS = current_status;
  CSML_INFO(2, logger) << "Set INTR_STATE.dma_done and STATUS.done" << std::endl;
  }

  if (chunk_done) {
    m_intr_state |= (1U << 1); // Set dma_chunk_done bit
    // Also set STATUS.chunk_done bit
    uint32_t current_status = static_cast<uint32_t>(STATUS);
    current_status |= (1U << 5);
    STATUS = current_status;
  CSML_INFO(2, logger) << "Set INTR_STATE.dma_chunk_done and STATUS.chunk_done" << std::endl;
  }

  if (error) {
    m_intr_state |= (1U << 2); // Set dma_error bit
    // Also set STATUS.error bit
    uint32_t current_status = static_cast<uint32_t>(STATUS);
    current_status |= (1U << 3);
    STATUS = current_status;
  CSML_INFO(2, logger) << "Set INTR_STATE.dma_error and STATUS.error" << std::endl;
  }

  // Update interrupt output ports (gated by INTR_ENABLE)
  update_interrupts();
}

void secure_dma_model::clear_interrupt_state(bool done, bool chunk_done, bool error) {
  // Clear internal interrupt state bits
  if (done) {
    m_intr_state &= ~(1U << 0); // Clear dma_done bit
  CSML_INFO(3, logger) << "Cleared INTR_STATE.dma_done" << std::endl;
  }

  if (chunk_done) {
    m_intr_state &= ~(1U << 1); // Clear dma_chunk_done bit
  CSML_INFO(3, logger) << "Cleared INTR_STATE.dma_chunk_done" << std::endl;
  }

  if (error) {
    m_intr_state &= ~(1U << 2); // Clear dma_error bit
  CSML_INFO(3, logger) << "Cleared INTR_STATE.dma_error" << std::endl;
  }

  // Update interrupt output ports (gated by INTR_ENABLE)
  update_interrupts();
}

// ============================================================================
// Transfer Granularity Control Helper Methods
// ============================================================================

/**
 * @brief Get transfer width in bytes from TRANSFER_WIDTH register
 *
 * Decodes TRANSFER_WIDTH.transaction_width field to actual byte count.
 * Used throughout transfer engine for alignment validation, byte enable
 * generation, and address advancement.
 *
 * @return Transfer width in bytes (1, 2, 4), or 0 if invalid encoding
 */
uint32_t secure_dma_model::get_transfer_width_bytes() {
  // Read TRANSFER_WIDTH register (bits [1:0] are transaction_width field)
  uint32_t transfer_width_reg = static_cast<uint32_t>(TRANSFER_WIDTH);
  uint32_t width_encoding = transfer_width_reg & 0x3; // Extract bits [1:0]

  // Decode width encoding per
  // 0x0 = ONE_BYTE (1 byte)
  // 0x1 = TWO_BYTE (2 bytes)
  // 0x2 = FOUR_BYTE (4 bytes)
  // 0x3 = Invalid
  switch (width_encoding) {
  case 0x0:
    return 1; // ONE_BYTE
  case 0x1:
    return 2; // TWO_BYTE
  case 0x2:
    return 4; // FOUR_BYTE
  case 0x3:
  default:
    return 0; // Invalid
  }
}

/**
 * @brief Validate address alignment for configured transfer width
 *
 * Enforces alignment requirements before transfer initiation. Misaligned
 * addresses trigger appropriate error codes and abort the transfer.
 *
 * Per detailed design: "Misaligned addresses trigger src_addr_error or
 * dst_addr_error in the ERROR_CODE register, and the DMA operation is
 * aborted before any transactions occur."
 *
 * @param addr Full 64-bit address to validate
 * @param is_source true for source address validation, false for destination
 * @param width_bytes Transfer width in bytes (1, 2, or 4)
 * @return true if aligned correctly, false if misaligned
 */
bool secure_dma_model::validate_address_alignment(uint64_t addr, bool is_source,
                                           uint32_t width_bytes) {
  bool is_aligned = false;

  // Alignment requirements per
  switch (width_bytes) {
  case 1:
    // 1-byte: No alignment requirement (any byte-aligned address is valid)
    is_aligned = true;
    break;

  case 2:
    // 2-byte: Halfword-aligned (address[0] must be 0)
    is_aligned = ((addr & 0x1) == 0);
    break;

  case 4:
    // 4-byte: Word-aligned (address[1:0] must be 00)
    is_aligned = ((addr & 0x3) == 0);
    break;

  default:
    // Invalid width (should have been caught by validate_transfer_width)
    is_aligned = false;
    break;
  }

  if (!is_aligned) {
    // Set appropriate error code
    if (is_source) {
      m_error_code |= (1U << 0); // Set ERROR_CODE.src_addr_error (bit 0)
    CSML_INFO(1, logger) << "Source address misalignment detected - addr: 0x" << std::hex << addr << ", width: " << std::dec << width_bytes << " bytes (ERROR_CODE.src_addr_error set)" << std::endl;
    } else {
      m_error_code |= (1U << 1); // Set ERROR_CODE.dst_addr_error (bit 1)
    CSML_INFO(1, logger) << "Destination address misalignment detected - addr: 0x" << std::hex << addr << ", width: " << std::dec << width_bytes << " bytes (ERROR_CODE.dst_addr_error set)" << std::endl;
    }

    // Propagate error code to ERROR_CODE register storage
    ERROR_CODE = m_error_code;
  }

  return is_aligned;
}

/**
 * @brief Validate transfer width configuration
 *
 * Performs comprehensive validation of TRANSFER_WIDTH register:
 * 1. Checks for invalid encoding (0x3)
 * 2. Enforces SHA-2 inline hashing constraint (requires FOUR_BYTE width)
 *
 * Per "Invalid TRANSFER_WIDTH encodings (value 0x3) and transfer
 * width mismatches with inline hashing trigger size_error."
 *
 * @return true if valid, false if invalid (sets ERROR_CODE.size_error)
 */
bool secure_dma_model::validate_transfer_width(uint32_t pending_control_value) {
  uint32_t transfer_width_reg = static_cast<uint32_t>(TRANSFER_WIDTH);
  uint32_t width_encoding = transfer_width_reg & 0x3; // Extract bits [1:0]

  // Check 1: Invalid encoding (0x3)
  if (width_encoding == 0x3) {
    m_error_code |= (1U << 3); // Set ERROR_CODE.size_error (bit 3)
    ERROR_CODE = m_error_code;
    CSML_INFO(1, logger) << "Invalid TRANSFER_WIDTH encoding (0x3) detected " << "(ERROR_CODE.size_error set)" << std::endl;
    return false;
  }

  // Check 2: SHA-2 inline hashing constraint (requires FOUR_BYTE width = 0x2)
  uint32_t opcode =
      pending_control_value & 0xF; // Extract opcode field (bits [3:0])

  // Opcode values: 0x0=COPY, 0x1=SHA256, 0x2=SHA384, 0x3=SHA512
  bool is_hashing_opcode = (opcode >= 0x1 && opcode <= 0x3);

  if (is_hashing_opcode && width_encoding != 0x2) {
    m_error_code |= (1U << 3); // Set ERROR_CODE.size_error (bit 3)
    ERROR_CODE = m_error_code;
    CSML_INFO(1, logger) << "SHA-2 inline hashing requires FOUR_BYTE width - " << "current width encoding: 0x" << std::hex << width_encoding << ", opcode: 0x" << opcode << std::dec << " (ERROR_CODE.size_error set)" << std::endl;
    return false;
  }

  // All validation checks passed
  CSML_INFO(3, logger) << "Transfer width validation passed - width encoding: 0x" << std::hex << width_encoding << ", opcode: 0x" << opcode << std::dec << std::endl;
  return true;
}

/**
 * @brief Generate TLM byte enable mask for sub-word transfers
 *
 * Creates byte enable mask that selects the appropriate byte lanes for
 * 1-byte and 2-byte transfers on the 32-bit bus. Used in TLM generic
 * payload byte_enable_ptr for write transactions.
 *
 * Per "Produces accurate byte-enable strobes for sub-word write
 * transactions based on address alignment and transfer width."
 *
 * @param addr Address of the transfer (LSBs determine byte lane)
 * @param width_bytes Transfer width in bytes (1, 2, or 4)
 * @return 4-bit byte enable mask (bit 0=lane 0, bit 3=lane 3)
 */
uint8_t secure_dma_model::generate_byte_enable_mask(uint64_t addr,
                                             uint32_t width_bytes) {
  uint8_t byte_enable = 0x0;

  switch (width_bytes) {
  case 1: {
    // 1-byte transfer: Enable single byte lane based on address[1:0]
    uint32_t byte_lane = addr & 0x3; // Extract bits [1:0]
    byte_enable = (1U << byte_lane);
    CSML_INFO(3, logger) << "1-byte byte_enable = 0x" << std::hex << static_cast<uint32_t>(byte_enable) << " (lane " << std::dec << byte_lane << ")" << std::endl;
    break;
  }

  case 2: {
    // 2-byte transfer: Enable 2 consecutive byte lanes based on address[1]
    uint32_t halfword_lane = (addr >> 1) & 0x1; // Extract bit [1]
    if (halfword_lane == 0) {
      byte_enable = 0x3; // Lanes 0-1 (bits [15:0])
    } else {
      byte_enable = 0xC; // Lanes 2-3 (bits [31:16])
    }
    CSML_INFO(3, logger) << "2-byte byte_enable = 0x" << std::hex << static_cast<uint32_t>(byte_enable) << " (halfword " << std::dec << halfword_lane << ")" << std::endl;
    break;
  }

  case 4:
    // 4-byte transfer: Enable all 4 byte lanes
    byte_enable = 0xF;
    CSML_INFO(3, logger) << "4-byte byte_enable = 0x" << std::hex << static_cast<uint32_t>(byte_enable) << std::dec << std::endl;
    break;

  default:
    // Invalid width - return 0 (no lanes enabled)
    byte_enable = 0x0;
    CSML_INFO(1, logger) << "Invalid width_bytes (" << width_bytes << ") - byte_enable = 0x0" << std::endl;
    break;
  }

  return byte_enable;
}

/**
 * @brief Register callbacks (reserved for future use)
 *
 * primarily provides validation and data manipulation helper
 * methods rather than register side-effect callbacks. The TRANSFER_WIDTH
 * register write callback is already registered in with CFG_REGWEN
 * protection. No additional callbacks are needed at this time.
 *
 * This function is reserved for future extensions if TRANSFER_WIDTH write
 * requires additional validation or side-effects beyond lock protection.
 */

// ============================================================================
// Addressing Mode Management Helper Methods
// ============================================================================

/**
 * @brief Decode source addressing mode from SRC_CONFIG register
 *
 * Reads SRC_CONFIG register and extracts addressing mode configuration.
 * Independent control of increment and wrap enables flexible addressing:
 * - FIFO access (fixed addressing)
 * - Linear memory access (incrementing addressing)
 * - Circular buffer access (wrapping addressing)
 *
 * @param increment Output: true if address advances per transaction, false if
 * fixed
 * @param wrap Output: true if address wraps at chunk boundaries, false if
 * linear
 */
void secure_dma_model::get_source_addressing_mode(bool &increment, bool &wrap) {
  // Read SRC_CONFIG register
  uint32_t src_config_value = static_cast<uint32_t>(SRC_CONFIG);

  // Extract addressing mode bits
  increment = (src_config_value & 0x1) != 0; // Bit 0: increment
  wrap = (src_config_value & 0x2) != 0;      // Bit 1: wrap

  CSML_INFO(3, logger) << "Source addressing mode - increment=" << increment << ", wrap=" << wrap << std::endl;
}

/**
 * @brief Decode destination addressing mode from DST_CONFIG register
 *
 * Reads DST_CONFIG register and extracts addressing mode configuration.
 * Same encoding as SRC_CONFIG for symmetric source/destination control.
 *
 * @param increment Output: true if address advances per transaction, false if
 * fixed
 * @param wrap Output: true if address wraps at chunk boundaries, false if
 * linear
 */
void secure_dma_model::get_destination_addressing_mode(bool &increment, bool &wrap) {
  // Read DST_CONFIG register
  uint32_t dst_config_value = static_cast<uint32_t>(DST_CONFIG);

  // Extract addressing mode bits
  increment = (dst_config_value & 0x1) != 0; // Bit 0: increment
  wrap = (dst_config_value & 0x2) != 0;      // Bit 1: wrap

  CSML_INFO(3, logger) << "Destination addressing mode - increment=" << increment << ", wrap=" << wrap << std::endl;
}

/**
 * @brief Advance source address after read transaction
 *
 * Implements three addressing modes:
 * 1. Fixed: Address unchanged (increment=0)
 * 2. Incrementing: Address += width_bytes (increment=1, wrap=0)
 * 3. Wrapping: Address advances with circular wrap to chunk base (increment=1,
 * wrap=1)
 *
 * For wrapping mode, uses modulo arithmetic to wrap address within chunk:
 * offset = (current_addr - chunk_start_addr + width_bytes) % chunk_size
 * current_addr = chunk_start_addr + offset
 *
 * Per detailed design: "If wrap mode is enabled: Addresses wrap to chunk start
 * address after chunk completion."
 *
 * @param current_addr Current 64-bit source address (modified in place)
 * @param chunk_start_addr Base address of current chunk for wrap calculation
 * @param chunk_size Size of chunk in bytes (wrap boundary)
 * @param width_bytes Transfer width in bytes (increment amount)
 */
void secure_dma_model::advance_source_address(uint64_t &current_addr,
                                       uint64_t chunk_start_addr,
                                       uint32_t chunk_size,
                                       uint32_t width_bytes) {
  // Get source addressing mode
  bool increment = false;
  bool wrap = false;
  get_source_addressing_mode(increment, wrap);

  uint64_t original_addr = current_addr;

  if (!increment) {
    // Fixed mode: Address unchanged (FIFO access pattern)
    CSML_INFO(3, logger) << "Source address FIXED mode - addr unchanged: 0x" << std::hex << current_addr << std::dec << std::endl;
    return;
  }

  // Increment mode: Address advances by transfer width
  current_addr += width_bytes;

  if (wrap) {
    // Wrapping mode: Check if address exceeded chunk boundary
    uint64_t offset_from_start = current_addr - chunk_start_addr;

    if (offset_from_start >= chunk_size) {
      // Wrap back to chunk start address
      current_addr = chunk_start_addr + (offset_from_start % chunk_size);

    CSML_INFO(2, logger) << "Source address WRAPPED - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (chunk_start=0x" << std::hex << chunk_start_addr << ", chunk_size=" << std::dec << chunk_size << " bytes)" << std::endl;
    } else {
  CSML_INFO(3, logger) << "Source address INCREMENTED (wrap mode) - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (+0x" << std::hex << width_bytes << ")" << std::endl;
    }
  } else {
    // Linear increment mode: No wrapping, continuous linear progression
    CSML_INFO(3, logger) << "Source address INCREMENTED (linear mode) - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (+0x" << std::hex << width_bytes << ")" << std::endl;
  }
}

/**
 * @brief Advance destination address after write transaction
 *
 * Implements same three addressing modes as source (fixed, incrementing,
 * wrapping). Logic identical to advance_source_address() but operates on
 * destination address using DST_CONFIG register settings.
 *
 * Per detailed design: "DST_ADDR advances by TRANSFER_WIDTH after each write
 * transaction if DST_CONFIG.increment=1."
 *
 * @param current_addr Current 64-bit destination address (modified in place)
 * @param chunk_start_addr Base address of current chunk for wrap calculation
 * @param chunk_size Size of chunk in bytes (wrap boundary)
 * @param width_bytes Transfer width in bytes (increment amount)
 */
void secure_dma_model::advance_destination_address(uint64_t &current_addr,
                                            uint64_t chunk_start_addr,
                                            uint32_t chunk_size,
                                            uint32_t width_bytes) {
  // Get destination addressing mode
  bool increment = false;
  bool wrap = false;
  get_destination_addressing_mode(increment, wrap);

  uint64_t original_addr = current_addr;

  if (!increment) {
    // Fixed mode: Address unchanged (FIFO access pattern)
    CSML_INFO(3, logger) << "Destination address FIXED mode - addr unchanged: 0x" << std::hex << current_addr << std::dec << std::endl;
    return;
  }

  // Increment mode: Address advances by transfer width
  current_addr += width_bytes;

  if (wrap) {
    // Wrapping mode: Check if address exceeded chunk boundary
    uint64_t offset_from_start = current_addr - chunk_start_addr;

    if (offset_from_start >= chunk_size) {
      // Wrap back to chunk start address
      current_addr = chunk_start_addr + (offset_from_start % chunk_size);

    CSML_INFO(2, logger) << "Destination address WRAPPED - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (chunk_start=0x" << std::hex << chunk_start_addr << ", chunk_size=" << std::dec << chunk_size << " bytes)" << std::endl;
    } else {
  CSML_INFO(3, logger) << "Destination address INCREMENTED (wrap mode) - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (+0x" << std::hex << width_bytes << ")" << std::endl;
    }
  } else {
    // Linear increment mode: No wrapping, continuous linear progression
    CSML_INFO(3, logger) << "Destination address INCREMENTED (linear mode) - 0x" << std::hex << original_addr << " -> 0x" << current_addr << std::dec << " (+0x" << std::hex << width_bytes << ")" << std::endl;
  }
}

/**
 * @brief Update SRC_ADDR_LO and SRC_ADDR_HI registers with current address
 *
 * Writes current source address back to address registers for software
 * visibility. Enables software to monitor transfer progress by reading address
 * registers during active transfers.
 *
 * Per detailed design: "During an active transfer, reading these registers may
 * return updated values reflecting current transfer progress."
 *
 * @param current_addr Current 64-bit source address to write to registers
 */
void secure_dma_model::update_src_addr_registers(uint64_t current_addr) {
  // Split 64-bit address into lower and upper 32-bit halves
  uint32_t addr_lo = static_cast<uint32_t>(current_addr & 0xFFFFFFFFULL);
  uint32_t addr_hi =
      static_cast<uint32_t>((current_addr >> 32) & 0xFFFFFFFFULL);

  // Write to SRC_ADDR_LO and SRC_ADDR_HI registers
  SRC_ADDR_LO = addr_lo;
  SRC_ADDR_HI = addr_hi;

  CSML_INFO(3, logger) << "Updated SRC_ADDR registers - HI:LO = 0x" << std::hex << addr_hi << ":0x" << addr_lo << std::dec << std::endl;
}

/**
 * @brief Update DST_ADDR_LO and DST_ADDR_HI registers with current address
 *
 * Writes current destination address back to address registers for software
 * visibility. Parallel functionality to update_src_addr_registers() for
 * destination side.
 *
 * Per detailed design: "Addresses wrap to chunk start address after chunk
 * completion if wrap mode is enabled. Software can read current address at any
 * time during transfer."
 *
 * @param current_addr Current 64-bit destination address to write to registers
 */
void secure_dma_model::update_dst_addr_registers(uint64_t current_addr) {
  // Split 64-bit address into lower and upper 32-bit halves
  uint32_t addr_lo = static_cast<uint32_t>(current_addr & 0xFFFFFFFFULL);
  uint32_t addr_hi =
      static_cast<uint32_t>((current_addr >> 32) & 0xFFFFFFFFULL);

  // Write to DST_ADDR_LO and DST_ADDR_HI registers
  DST_ADDR_LO = addr_lo;
  DST_ADDR_HI = addr_hi;

  CSML_INFO(3, logger) << "Updated DST_ADDR registers - HI:LO = 0x" << std::hex << addr_hi << ":0x" << addr_lo << std::dec << std::endl;
}


/**
 * @brief Register callbacks (reserved for future use)
 *
 * provides addressing mode management and address advancement logic.
 * SRC_CONFIG and DST_CONFIG register write callbacks are already registered in
 * with CFG_REGWEN protection. No additional callbacks are needed.
 *
 * This function logs initialization and is reserved for future extensions if
 * additional side-effects are required.
 */

// ============================================================================
// Multi-Bus Interface Transaction Routing Implementation
// ============================================================================

/**
 * @brief Decode and validate ASID value from ADDR_SPACE_ID register
 *
 * Maps 4-bit ASID values to bus interface enumeration according to multibit
 * encoding specification. Returns INVALID for any non-standard ASID value.
 *
 * ASID Mapping:
 * - 0x7 (OT_ADDR) → OT_INTERNAL (OpenTitan 32-bit internal bus)
 * - 0x9 (SYS_ADDR) → SYSTEM_BUS (SoC 64-bit system bus)
 * - 0xA (SOC_ADDR) → CTN_BUS (SoC control network)
 * - All others → INVALID
 *
 * Per detailed design: "The Address Space Identifier (ASID) value for this
 * interface is 0x7 (multibit encoded) for OT Internal, 0xA for CTN, and
 * 0x9 for System bus."
 */
secure_dma_model::BusInterface secure_dma_model::decode_asid(uint32_t asid_value) {
  // Mask to 4 bits
  asid_value &= 0xF;

  switch (asid_value) {
  case 0x7:
    CSML_INFO(3, logger) << "ASID 0x7 decoded as OT_INTERNAL" << std::endl;
    return BusInterface::OT_INTERNAL;

  case 0x9:
    CSML_INFO(3, logger) << "ASID 0x9 decoded as SYSTEM_BUS" << std::endl;
    return BusInterface::SYSTEM_BUS;

  case 0xA:
    CSML_INFO(3, logger) << "ASID 0xA decoded as CTN_BUS" << std::endl;
    return BusInterface::CTN_BUS;

  default:
    CSML_INFO(1, logger) << "Invalid ASID 0x" << std::hex << asid_value << std::dec << " (not 0x7, 0x9, or 0xA)" << std::endl;
    return BusInterface::INVALID;
  }
}

/**
 * @brief Validate ASID value and set error if invalid
 *
 * Checks ASID against valid multibit-encoded values (0x7, 0x9, 0xA).
 * Sets ERROR_CODE.asid_error (bit 7) if invalid ASID detected.
 *
 * Per detailed design: "The DMA controller enforces strict ASID validation,
 * rejecting invalid multibit-encoded values and setting the asid_error flag
 * in the ERROR_CODE register."
 */
bool secure_dma_model::validate_asid(uint32_t asid_value, bool is_source) {
  // Mask to 4 bits
  asid_value &= 0xF;

  // Check for valid ASID values
  if (asid_value == 0x7 || asid_value == 0x9 || asid_value == 0xA) {
    CSML_INFO(3, logger) << "" << (is_source ? "Source" : "Destination") << " ASID 0x" << std::hex << asid_value << std::dec << " is valid" << std::endl;
    return true;
  }

  // Invalid ASID - set ERROR_CODE.asid_error (bit 7)
  m_error_code |= (1U << 7);
  ERROR_CODE = m_error_code;

  CSML_INFO(1, logger) << "Invalid " << (is_source ? "source" : "destination") << " ASID 0x" << std::hex << asid_value << std::dec << " detected (ERROR_CODE.asid_error set)" << std::endl;

  return false;
}

/**
 * @brief Select bus interface for transaction based on ASID
 *
 * Reads ADDR_SPACE_ID register and extracts appropriate ASID field:
 * - Source transactions: Uses src_asid (bits [3:0])
 * - Destination transactions: Uses dst_asid (bits [7:4])
 *
 * Validates ASID and returns corresponding bus interface enumeration.
 *
 * Per functionality list: "Routes source read and destination write
 * transactions to appropriate bus interfaces based on multibit-encoded
 * ASID values configured in ADDR_SPACE_ID register."
 */
secure_dma_model::BusInterface secure_dma_model::select_bus_for_transaction(bool is_source) {
  // Read ADDR_SPACE_ID register
  uint32_t addr_space_id = static_cast<uint32_t>(ADDR_SPACE_ID);

  // Extract appropriate ASID field
  uint32_t asid_value;
  if (is_source) {
    // Source ASID: bits [3:0]
    asid_value = addr_space_id & 0xF;
  CSML_INFO(2, logger) << "Selecting bus for SOURCE transaction (src_asid=0x" << std::hex << asid_value << std::dec << ")" << std::endl;
  } else {
    // Destination ASID: bits [7:4]
    asid_value = (addr_space_id >> 4) & 0xF;
  CSML_INFO(2, logger) << "Selecting bus for DESTINATION transaction (dst_asid=0x" << std::hex << asid_value << std::dec << ")" << std::endl;
  }

  // Validate ASID
  if (!validate_asid(asid_value, is_source)) {
    return BusInterface::INVALID;
  }

  // Decode ASID to bus interface
  BusInterface bus = decode_asid(asid_value);

  if (bus != BusInterface::INVALID) {
  CSML_INFO(2, logger) << "Selected bus: " << get_bus_name(bus) << std::endl;
  }

  return bus;
}

/**
 * @brief Validate address width constraint for selected bus interface
 *
 * Enforces address space constraints per bus interface:
 * - OT_INTERNAL (32-bit): Upper 32 bits must be zero
 * - CTN_BUS (configurable): For 32-bit mode, upper 32 bits must be zero
 * - SYSTEM_BUS (64-bit): Full 64-bit addressing supported
 *
 * Sets ERROR_CODE.src_addr_error (bit 0) or ERROR_CODE.dst_addr_error (bit 1)
 * on address width constraint violation.
 *
 * Per functionality list: "Address Width Enforcement: Validates address
 * constraints per interface: 32-bit upper address must be zero for OT
 * Private and 32-bit CTN configurations."
 */
bool secure_dma_model::validate_address_width_for_bus(uint64_t addr,
                                               BusInterface bus_interface,
                                               bool is_source) {
  uint32_t upper_32 = static_cast<uint32_t>(addr >> 32);

  switch (bus_interface) {
  case BusInterface::OT_INTERNAL:
    // OT Internal bus: Must be 32-bit address (upper 32 bits = 0)
    if (upper_32 != 0) {
      // Set src_addr_error or dst_addr_error
      if (is_source) {
        m_error_code |= (1U << 0); // ERROR_CODE.src_addr_error
      } else {
        m_error_code |= (1U << 1); // ERROR_CODE.dst_addr_error
      }
      ERROR_CODE = m_error_code;

      CSML_INFO(1, logger) << "Address width violation for OT_INTERNAL bus - " << (is_source ? "source" : "destination") << " address upper 32 bits non-zero (0x" << std::hex << upper_32 << std::dec << ") (ERROR_CODE." << (is_source ? "src_addr_error" : "dst_addr_error") << " set)" << std::endl;
      return false;
    }
    CSML_INFO(3, logger) << "Address width validation passed for OT_INTERNAL (32-bit)" << std::endl;
    return true;

  case BusInterface::CTN_BUS:
    // CTN bus: Configurable 32-bit or 64-bit
    // Allowing full 64-bit addressing to support high-memory CTN targets
    CSML_INFO(3, logger) << "Address width validation passed for CTN_BUS (64-bit mode)" << std::endl;
    return true;

  case BusInterface::SYSTEM_BUS:
    // System bus: Full 64-bit address space supported
    CSML_INFO(3, logger) << "Address width validation passed for SYSTEM_BUS (64-bit) - " << "addr=0x" << std::hex << addr << std::dec << std::endl;
    return true;

  case BusInterface::INVALID:
    // Invalid bus interface - error already logged
    return false;

  default:
    CSML_INFO(1, logger) << "Unknown bus interface in address width validation" << std::endl;
    return false;
  }
}

/**
 * @brief Create TLM generic payload for bus transaction
 *
 * Populates TLM-2.0 generic payload with transaction attributes for bus access.
 * Creates properly formatted payload for blocking transport interface.
 *
 * TLM Payload Configuration:
 * - Command: TLM_READ_COMMAND or TLM_WRITE_COMMAND
 * - Address: Full 64-bit address (validated separately for bus constraints)
 * - Data pointer: Buffer for read destination or write source
 * - Data length: Transfer width (1, 2, or 4 bytes)
 * - Byte enables: Mask for sub-word transfers
 * - Streaming width: Equal to data length (no streaming)
 * - Response status: TLM_INCOMPLETE_RESPONSE initially
 *
 * Per functionality list: "Implements transaction-level protocol mapping
 * between TLM generic payloads and protocol-specific bus semantics."
 *
 * Note: This method prepares the payload but does NOT issue the transaction.
 * Actual b_transport calls will be made by (Transfer Engine).
 */
void secure_dma_model::create_tlm_transaction(tlm::tlm_generic_payload &trans,
                                       tlm::tlm_command cmd, uint64_t addr,
                                       unsigned char *data, uint32_t length,
                                       uint8_t byte_enable_mask) {
  // Set transaction command (READ or WRITE)
  trans.set_command(cmd);

  // Set transaction address (full 64-bit)
  trans.set_address(addr);

  // Set data pointer (buffer for read data or write data source)
  trans.set_data_ptr(data);

  // Set data length (transfer width in bytes: 1, 2, or 4)
  trans.set_data_length(length);

  // Set streaming width (equal to data length - no streaming)
  trans.set_streaming_width(length);

  // Set byte enable pointer for sub-word transfers
  // Note: TLM requires persistent byte enable array
  // For simplicity, we use a static byte enable buffer (not thread-safe for
  // concurrent transactions) This is acceptable since DMA enforces single
  // outstanding transaction architecture
  static unsigned char byte_enable_buffer[4];
  byte_enable_buffer[0] = (byte_enable_mask & 0x1) ? 0xFF : 0x00;
  byte_enable_buffer[1] = (byte_enable_mask & 0x2) ? 0xFF : 0x00;
  byte_enable_buffer[2] = (byte_enable_mask & 0x4) ? 0xFF : 0x00;
  byte_enable_buffer[3] = (byte_enable_mask & 0x8) ? 0xFF : 0x00;

  trans.set_byte_enable_ptr(byte_enable_buffer);
  trans.set_byte_enable_length(4); // Always 4 bytes for 32-bit bus

  // Set DMI allowed (false for DMA - no direct memory interface)
  trans.set_dmi_allowed(false);

  // Set response status to incomplete (will be updated by target)
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  // Stamp the AXI sideband the outbound/inbound filters key off. The DMA is not
  // given a source ID of its own in RTL -- sep.sv:895 wires its TL-UL bridge
  // with .TlUserRsvd('0), so its traffic reaches the filters as
  // OTHERS_SOURCE_ID.
  auto* axi_ext = trans.get_extension<sep::sep_axi_extension>();
  if (!axi_ext) {
    axi_ext = new sep::sep_axi_extension();
    trans.set_extension(axi_ext);
  }
  axi_ext->source_id = sep::OTHERS_SOURCE_ID;

  CSML_INFO(3, logger) << "TLM transaction created - " << (cmd == tlm::TLM_READ_COMMAND ? "READ" : "WRITE") << " addr=0x" << std::hex << addr << " length=" << std::dec << length << " byte_enable=0x" << std::hex << static_cast<uint32_t>(byte_enable_mask) << std::dec << std::endl;
}

/**
 * @brief Get bus interface name for logging and debugging
 *
 * Returns human-readable name strings for bus interface enumeration values.
 * Used for diagnostic logging and error messages.
 */
const char *secure_dma_model::get_bus_name(BusInterface bus_interface) {
  switch (bus_interface) {
  case BusInterface::OT_INTERNAL:
    return "OT Internal (32-bit TL-UL)";
  case BusInterface::CTN_BUS:
    return "CTN (Control Network)";
  case BusInterface::SYSTEM_BUS:
    return "System Bus (64-bit)";
  case BusInterface::INVALID:
    return "INVALID";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Register callbacks with CSML memory
 *
 * provides multi-bus interface transaction routing and TLM
 * payload preparation logic. No additional register callbacks are required
 * beyond (ADDR_SPACE_ID register already registered with CFG_REGWEN
 * protection).
 *
 * Helper methods will be called by the transfer engine during
 * transaction execution to select bus interfaces, validate address widths,
 * and prepare TLM payloads.
 *
 * This function logs initialization and is reserved for future extensions if
 * additional side-effects are required.
 */

// =============================================================================
// Error Detection and Reporting Implementation
// =============================================================================

/**
 * @brief Validate opcode field in CONTROL register
 *
 * Validates CONTROL.opcode field (bits [3:0]) against supported opcode values.
 * DMA supports four operation modes:
 * - 0x0: Memory copy (no hashing)
 * - 0x1: SHA-256 hash computation
 * - 0x2: SHA-384 hash computation
 * - 0x3: SHA-512 hash computation
 * - 0x4-0xF: Reserved/invalid
 *
 * Per detailed design: "CONTROL.opcode contains an invalid value (not 0x0,
 * 0x1, 0x2, or 0x3)."
 *
 * @return true if opcode is valid, false if invalid (sets
 * ERROR_CODE.opcode_error)
 */
bool secure_dma_model::validate_opcode(uint32_t pending_control_value) {
  uint32_t opcode =
      pending_control_value & 0xF; // Extract opcode field (bits [3:0])

  // Valid opcodes: 0x0 (COPY), 0x1 (SHA256), 0x2 (SHA384), 0x3 (SHA512)
  if (opcode <= 0x3) {
    CSML_INFO(3, logger) << "Opcode validation passed - opcode: 0x" << std::hex << opcode << std::dec << std::endl;
    return true;
  }

  // Invalid opcode detected (0x4-0xF)
  m_error_code |= (1U << 2); // Set ERROR_CODE.opcode_error (bit 2)
  ERROR_CODE = m_error_code;

  CSML_INFO(1, logger) << "Invalid opcode detected - opcode: 0x" << std::hex << opcode << std::dec << " (valid range: 0x0-0x3)" << " (ERROR_CODE.opcode_error set)" << std::endl;
  return false;
}

/**
 * @brief Validate transfer size configuration
 *
 * Validates transfer size configuration to ensure meaningful DMA operations.
 * Checks:
 * 1. TOTAL_DATA_SIZE != 0 (zero total size is meaningless)
 * 2. CHUNK_DATA_SIZE != 0 (zero chunk size prevents transfer)
 * 3. CHUNK_DATA_SIZE <= TOTAL_DATA_SIZE (chunk cannot exceed total)
 *
 * Per detailed design:
 * - "TOTAL_DATA_SIZE register is 0. Sets size_error. A transfer with zero
 * bytes is meaningless and not permitted."
 * - "CHUNK_DATA_SIZE register is 0. Sets size_error."
 *
 * @return true if sizes are valid, false if invalid (sets
 * ERROR_CODE.size_error)
 */
bool secure_dma_model::validate_transfer_size() {
  uint32_t total_size = static_cast<uint32_t>(TOTAL_DATA_SIZE);
  uint32_t chunk_size = static_cast<uint32_t>(CHUNK_DATA_SIZE);

  // Check 1: TOTAL_DATA_SIZE must be non-zero
  if (total_size == 0) {
    m_error_code |= (1U << 3); // Set ERROR_CODE.size_error (bit 3)
    ERROR_CODE = m_error_code;

    CSML_INFO(1, logger) << "Transfer size validation failed - " << "TOTAL_DATA_SIZE is zero" << " (ERROR_CODE.size_error set)" << std::endl;
    return false;
  }

  // Check 2: CHUNK_DATA_SIZE must be non-zero
  if (chunk_size == 0) {
    m_error_code |= (1U << 3); // Set ERROR_CODE.size_error (bit 3)
    ERROR_CODE = m_error_code;

    CSML_INFO(1, logger) << "Transfer size validation failed - " << "CHUNK_DATA_SIZE is zero" << " (ERROR_CODE.size_error set)" << std::endl;
    return false;
  }

  // Check 3: CHUNK_DATA_SIZE should not exceed TOTAL_DATA_SIZE
  // Note: This is a logical check - chunk size should be <= total size
  if (chunk_size > total_size) {
    CSML_INFO(2, logger) << "Warning - CHUNK_DATA_SIZE (" << chunk_size << ") exceeds TOTAL_DATA_SIZE (" << total_size << ") - will transfer only total_size bytes" << std::endl;
    // This is not an error condition per spec, just a warning
    // The hardware will transfer min(chunk_size, total_size)
  }

  CSML_INFO(3, logger) << "Transfer size validation passed - " << "total: " << total_size << " bytes, " << "chunk: " << chunk_size << " bytes" << std::endl;
  return true;
}

/**
 * @brief Comprehensive pre-transfer validation
 *
 * Orchestrates all pre-transfer validation checks to ensure configuration
 * validity before initiating DMA transfer. This method consolidates validation
 * from multiple functionalities  into a single
 * comprehensive check.
 *
 * Validation sequence:
 * 1. Opcode validity
 * 2. Transfer width encoding and SHA-2 constraint
 * 3. Transfer size constraints
 * 4. Source address alignment
 * 5. Destination address alignment
 * 6. Source ASID validity
 * 7. Destination ASID validity
 * 8. Source address width for selected bus
 * 9. Destination address width for selected bus
 *
 * Per detailed design: "Configuration validation before transfer initiation"
 * ensures all parameters are valid before starting any bus transactions.
 * "Misaligned addresses trigger src_addr_error or dst_addr_error in the
 * ERROR_CODE register, and the DMA operation is aborted before any
 * transactions occur."
 *
 * @return true if all validations pass, false if any error detected
 */
bool secure_dma_model::validate_transfer_configuration(
    uint32_t pending_control_value) {
  bool all_valid = true;

  CSML_INFO(2, logger) << "Starting comprehensive pre-transfer validation" << std::endl;

  // Clear previous error code (new transfer validation)
  m_error_code = 0;
  ERROR_CODE = 0;

  // Validation 1: Opcode validity
  if (!validate_opcode(pending_control_value)) {
    all_valid = false;
  CSML_INFO(1, logger) << "Opcode validation failed" << std::endl;
  }

  // Validation 2: Transfer width
  if (!validate_transfer_width(pending_control_value)) {
    all_valid = false;
  CSML_INFO(1, logger) << "Transfer width validation failed" << std::endl;
  }

  // Validation 3: Transfer size constraints
  if (!validate_transfer_size()) {
    all_valid = false;
  CSML_INFO(1, logger) << "Transfer size validation failed" << std::endl;
  }

  // Get transfer width for alignment checks
  uint32_t width_bytes = get_transfer_width_bytes();
  if (width_bytes == 0) {
    // Transfer width validation already failed, skip alignment checks
    all_valid = false;
  CSML_INFO(1, logger) << "Skipping alignment checks due to invalid transfer width" << std::endl;
  } else {
    // Validation 4: Source address alignment
    uint64_t src_addr = (static_cast<uint64_t>(SRC_ADDR_HI) << 32) |
                        static_cast<uint32_t>(SRC_ADDR_LO);
    if (!validate_address_alignment(src_addr, true, width_bytes)) {
      all_valid = false;
    CSML_INFO(1, logger) << "Source address alignment validation failed" << std::endl;
    }

    // Validation 5: Destination address alignment
    uint64_t dst_addr = (static_cast<uint64_t>(DST_ADDR_HI) << 32) |
                        static_cast<uint32_t>(DST_ADDR_LO);
    if (!validate_address_alignment(dst_addr, false, width_bytes)) {
      all_valid = false;
  CSML_INFO(1, logger) << "Destination address alignment validation failed" << std::endl;
    }
  }

  // Validation 6 & 7: ASID validity
  uint32_t addr_space_id = static_cast<uint32_t>(ADDR_SPACE_ID);
  uint32_t src_asid = addr_space_id & 0xF;        // bits [3:0]
  uint32_t dst_asid = (addr_space_id >> 4) & 0xF; // bits [7:4]

  if (!validate_asid(src_asid, true)) {
    all_valid = false;
  CSML_INFO(1, logger) << "Source ASID validation failed" << std::endl;
  }

  if (!validate_asid(dst_asid, false)) {
    all_valid = false;
  CSML_INFO(1, logger) << "Destination ASID validation failed" << std::endl;
  }

  // Validation 8 & 9: Address width for bus interface
  // Only perform if ASID validation passed
  if (validate_asid(src_asid, true)) {
    BusInterface src_bus = select_bus_for_transaction(true);
    uint64_t src_addr = (static_cast<uint64_t>(SRC_ADDR_HI) << 32) |
                        static_cast<uint32_t>(SRC_ADDR_LO);

    if (!validate_address_width_for_bus(src_addr, src_bus, true)) {
      all_valid = false;
  CSML_INFO(1, logger) << "Source address width validation failed" << std::endl;
    }
  }

  if (validate_asid(dst_asid, false)) {
    BusInterface dst_bus = select_bus_for_transaction(false);
    uint64_t dst_addr = (static_cast<uint64_t>(DST_ADDR_HI) << 32) |
                        static_cast<uint32_t>(DST_ADDR_LO);

    if (!validate_address_width_for_bus(dst_addr, dst_bus, false)) {
      all_valid = false;
  CSML_INFO(1, logger) << "Destination address width validation failed" << std::endl;
    }
  }

  // Validation 10: Security policy enforcement
  // Only perform if ASID validation passed for both source and destination
  if (validate_asid(src_asid, true) && validate_asid(dst_asid, false)) {
    uint64_t src_addr = (static_cast<uint64_t>(SRC_ADDR_HI) << 32) |
                        static_cast<uint32_t>(SRC_ADDR_LO);
    uint64_t dst_addr = (static_cast<uint64_t>(DST_ADDR_HI) << 32) |
                        static_cast<uint32_t>(DST_ADDR_LO);

    if (!validate_security_policy(src_addr, dst_addr, src_asid, dst_asid)) {
      all_valid = false;
  CSML_INFO(1, logger) << "Security policy validation failed" << std::endl;
    }
  }

  // Summary logging
  if (all_valid) {
  CSML_INFO(2, logger) << "All pre-transfer validations passed - " << "transfer can proceed" << std::endl;
  } else {
    CSML_INFO(1, logger) << "Pre-transfer validation FAILED - " << "ERROR_CODE: 0x" << std::hex << m_error_code << std::dec << " - transfer cannot start" << std::endl;

    // Set STATUS.error bit when validation fails
    uint32_t status_reg = static_cast<uint32_t>(STATUS);
    status_reg |= (1U << 3); // Set STATUS.error (bit 3)
    STATUS = status_reg;

    // Trigger error interrupt via
    set_interrupt_state(false, false, true);
  }

  return all_valid;
}


// ============================================================================
// Security Isolation and Access Control Implementation
// ============================================================================

/**
 * @brief Validate memory range configuration registers
 *
 * Validates ENABLED_MEMORY_RANGE configuration to ensure memory range is
 * properly configured for security boundary enforcement. Checks:
 * 1. BASE ≤ LIMIT (valid range relationship)
 * 2. RANGE_VALID is set when cross-boundary transfers are attempted
 *
 * Per detailed design: "ENABLED_MEMORY_RANGE_BASE is greater than
 * ENABLED_MEMORY_RANGE_LIMIT. Sets base_limit_error. This indicates
 * an invalid memory range configuration."
 *
 * @return true if range configuration is valid, false if invalid
 */
bool secure_dma_model::validate_range_configuration() {
  // Read ENABLED_MEMORY_RANGE registers
  uint32_t range_base = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_BASE);
  uint32_t range_limit = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_LIMIT);

  // Check if BASE > LIMIT (invalid configuration)
  if (range_base > range_limit) {
    m_error_code |= (1U << 5); // Set ERROR_CODE.base_limit_error (bit 5)
    ERROR_CODE = m_error_code;

    CSML_INFO(1, logger) << "Range configuration validation failed - " << "BASE (0x" << std::hex << range_base << ") > LIMIT (0x" << range_limit << ")" << std::dec << " (ERROR_CODE.base_limit_error set)" << std::endl;
    return false;
  }

  CSML_INFO(3, logger) << "Range configuration validation passed - " << "BASE=0x" << std::hex << range_base << ", LIMIT=0x" << range_limit << std::dec << std::endl;
  return true;
}

/**
 * @brief Check if address is within DMA-enabled memory range
 *
 * Determines if a given OT internal address falls within the configured
 * DMA-enabled memory staging area. Addresses within the range are considered
 * OT DMA-enabled memory; addresses outside are OT Private memory.
 *
 * Per detailed design: "OT DMA Enabled Memory: Memory within the OpenTitan
 * secure perimeter designated as a staging area for DMA operations."
 *
 * @param addr 32-bit OT internal address to check
 * @return true if address is within [BASE, LIMIT] inclusive, false otherwise
 */
bool secure_dma_model::check_address_in_dma_range(uint32_t addr) {
  uint32_t range_base = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_BASE);
  uint32_t range_limit = static_cast<uint32_t>(ENABLED_MEMORY_RANGE_LIMIT);

  // Check if address is within [BASE, LIMIT] inclusive
  bool in_range = (addr >= range_base) && (addr <= range_limit);

  CSML_INFO(3, logger) << "Address 0x" << std::hex << addr << (in_range ? " IS" : " IS NOT") << " within DMA range [0x" << range_base << ", 0x" << range_limit << "]" << std::dec << std::endl;

  return in_range;
}

/**
 * @brief Validate security policy for transfer
 *
 * Enforces three-tier security policy matrix preventing unauthorized data
 * movement between OpenTitan internal memory and SoC memory spaces.
 *
 * Memory Region Classification:
 * - OT Private: OT_ADDR (0x7) outside [BASE, LIMIT] - fully protected
 * - OT DMA Enabled: OT_ADDR (0x7) within [BASE, LIMIT] - staging area
 * - SoC Memory: SYS_ADDR (0x9) or SOC_ADDR (0xA) - external/untrusted
 *
 * Security Policy Matrix:
 * - OT Private → OT Private: Allowed
 * - OT Private → OT DMA: Allowed
 * - OT Private → SoC: PROHIBITED (prevents data leakage)
 * - OT DMA → OT Private: Allowed
 * - OT DMA → OT DMA: Allowed
 * - OT DMA → SoC: Allowed (secure staging)
 * - SoC → OT Private: PROHIBITED (prevents unauthorized access)
 * - SoC → OT DMA: Allowed (secure ingress)
 * - SoC → SoC: Allowed
 *
 * Per detailed design: "This matrix ensures that untrusted SoC memory
 * cannot directly access OT Private Memory, and that OT Private Memory
 * contents cannot leak directly to SoC memory. All cross-boundary transfers
 * must stage through OT DMA Enabled Memory."
 *
 * @param src_addr 64-bit source address
 * @param dst_addr 64-bit destination address
 * @param src_asid Source ASID value (0x7=OT, 0x9=SYS, 0xA=SOC)
 * @param dst_asid Destination ASID value (0x7=OT, 0x9=SYS, 0xA=SOC)
 * @return true if transfer allowed, false if prohibited (sets ERROR_CODE)
 */
bool secure_dma_model::validate_security_policy(uint64_t src_addr, uint64_t dst_addr,
                                         uint32_t src_asid, uint32_t dst_asid) {
  // Mask ASIDs to 4 bits
  src_asid &= 0xF;
  dst_asid &= 0xF;

  // Determine source memory region
  bool src_is_ot = (src_asid == 0x7);
  bool src_is_soc = (src_asid == 0x9 || src_asid == 0xA);
  bool src_in_dma_range = false;

  if (src_is_ot) {
    // Extract lower 32 bits for OT internal address
    uint32_t src_addr_32 = static_cast<uint32_t>(src_addr & 0xFFFFFFFFULL);
    src_in_dma_range = check_address_in_dma_range(src_addr_32);
  }

  // Determine destination memory region
  bool dst_is_ot = (dst_asid == 0x7);
  bool dst_is_soc = (dst_asid == 0x9 || dst_asid == 0xA);
  bool dst_in_dma_range = false;

  if (dst_is_ot) {
    // Extract lower 32 bits for OT internal address
    uint32_t dst_addr_32 = static_cast<uint32_t>(dst_addr & 0xFFFFFFFFULL);
    dst_in_dma_range = check_address_in_dma_range(dst_addr_32);
  }

  // Classify source and destination regions
  std::string src_region;
  std::string dst_region;

  if (src_is_ot) {
    src_region = src_in_dma_range ? "OT_DMA_ENABLED" : "OT_PRIVATE";
  } else if (src_is_soc) {
    src_region = "SOC_MEMORY";
  } else {
    src_region = "UNKNOWN";
  }

  if (dst_is_ot) {
    dst_region = dst_in_dma_range ? "OT_DMA_ENABLED" : "OT_PRIVATE";
  } else if (dst_is_soc) {
    dst_region = "SOC_MEMORY";
  } else {
    dst_region = "UNKNOWN";
  }

  CSML_INFO(2, logger) << "Security policy check - " << src_region << " → " << dst_region << std::endl;

  // Check if transfer crosses security boundary (OT ↔ SoC)
  bool crosses_boundary =
      (src_is_ot && dst_is_soc) || (src_is_soc && dst_is_ot);

  if (crosses_boundary) {
    // Verify RANGE_VALID is set for cross-boundary transfers
    uint32_t range_valid = static_cast<uint32_t>(RANGE_VALID) & 0x1;
    if (range_valid == 0) {
      m_error_code |= (1U << 6); // Set ERROR_CODE.range_valid_error (bit 6)
      ERROR_CODE = m_error_code;

      CSML_INFO(1, logger) << "Security policy violation - " << "RANGE_VALID not set for cross-boundary transfer " << src_region << " → " << dst_region << " (ERROR_CODE.range_valid_error set)" << std::endl;
      return false;
    }

    // Validate range configuration (BASE ≤ LIMIT)
    if (!validate_range_configuration()) {
      // Error already logged and ERROR_CODE set by
      // validate_range_configuration()
      return false;
    }
  }

  // Enforce security policy matrix

  // Case 1: OT Private → SoC (PROHIBITED)
  if (src_is_ot && !src_in_dma_range && dst_is_soc) {
    m_error_code |= (1U << 0); // Set ERROR_CODE.src_addr_error (bit 0)
    ERROR_CODE = m_error_code;

    CSML_INFO(1, logger) << "Security policy violation - " << "OT_PRIVATE → SOC_MEMORY transfer PROHIBITED " << "(prevents data leakage to untrusted memory) " << "(ERROR_CODE.src_addr_error set)" << std::endl;
    return false;
  }

  // Case 2: SoC → OT Private (PROHIBITED)
  if (src_is_soc && dst_is_ot && !dst_in_dma_range) {
    m_error_code |= (1U << 1); // Set ERROR_CODE.dst_addr_error (bit 1)
    ERROR_CODE = m_error_code;

    CSML_INFO(1, logger) << "Security policy violation - " << "SOC_MEMORY → OT_PRIVATE transfer PROHIBITED " << "(prevents unauthorized access to secure memory) " << "(ERROR_CODE.dst_addr_error set)" << std::endl;
    return false;
  }

  // Case 3: SoC → OT DMA (Allowed, but validate destination is in range)
  if (src_is_soc && dst_is_ot && dst_in_dma_range) {
    CSML_INFO(2, logger) << "Security policy check passed - " << "SOC_MEMORY → OT_DMA_ENABLED transfer ALLOWED " << "(secure ingress through staging area)" << std::endl;
    return true;
  }

  // Case 4: OT DMA → SoC (Allowed, but validate source is in range)
  if (src_is_ot && src_in_dma_range && dst_is_soc) {
    CSML_INFO(2, logger) << "Security policy check passed - " << "OT_DMA_ENABLED → SOC_MEMORY transfer ALLOWED " << "(secure egress through staging area)" << std::endl;
    return true;
  }

  // Case 5: OT ↔ OT (Always allowed, internal transfers)
  if (src_is_ot && dst_is_ot) {
    CSML_INFO(2, logger) << "Security policy check passed - " << "OT internal transfer ALLOWED " << "(" << src_region << " → " << dst_region << ")" << std::endl;
    return true;
  }

  // Case 6: SoC ↔ SoC (Always allowed, external transfers)
  if (src_is_soc && dst_is_soc) {
    CSML_INFO(2, logger) << "Security policy check passed - " << "SOC_MEMORY → SOC_MEMORY transfer ALLOWED " << "(external memory only, no security boundary)" << std::endl;
    return true;
  }

  // If we reach here, transfer is allowed (default allow for valid
  // configurations)
  CSML_INFO(2, logger) << "Security policy check passed - " << src_region << " → " << dst_region << " transfer ALLOWED" << std::endl;
  return true;
}

/**
 * @brief Register callbacks with CSML memory
 *
 * provides security isolation and access control enforcement.
 * No additional register callbacks are required beyond
 * (ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID,
 * and RANGE_REGWEN registers already registered with write protection).
 *
 * Security validation is performed by validate_security_policy() called
 * during transfer configuration validation (integrated with's
 * validate_transfer_configuration() method).
 *
 * This function logs initialization and confirms security policy enforcement
 * is ready for use by the transfer engine.
 */

// ============================================================================
// Transfer Control and Abort Implementation
// ============================================================================

/**
 * @brief Register callbacks with CSML memory
 *
 * implements Transfer Control and Abort functionality through
 * enhanced behavior in existing CONTROL and STATUS register callbacks.
 * No new callbacks are required - behavior is integrated into:
 * - handle_write_CONTROL(): Implements go-bit transfer initiation with
 * validation triggering and abort-bit emergency termination
 * - handle_read_CFG_REGWEN(): Returns lock status based on m_dma_busy state
 * - handle_read_STATUS(): Returns busy/done/aborted status bits
 *
 * Key behaviors implemented:
 * 1. Software-Initiated Transfer Control (go bit):
 * - Triggers validate_transfer_configuration() (+)
 * - On validation pass: Sets m_dma_busy, locks CFG_REGWEN, sets STATUS.busy
 * - On validation fail: Remains IDLE, sets STATUS.error, clears go bit
 *
 * 2. Transfer Abort Capability (abort bit):
 * - Immediately halts transfer engine (stops issuing new transactions)
 * - Clears m_dma_busy, unlocks CFG_REGWEN (returns to 0x6)
 * - Sets STATUS.aborted bit, clears STATUS.busy bit
 * - Clears go bit from CONTROL register
 *
 * 3. State Machine Lifecycle Management:
 * - IDLE state: m_dma_busy=false, CFG_REGWEN=0x6, STATUS.busy=0
 * - BUSY state: m_dma_busy=true, CFG_REGWEN=0x0, STATUS.busy=1
 * - Transitions: IDLE→BUSY (go bit), BUSY→IDLE (abort or completion)
 *
 * 4. Configuration Validation Triggering:
 * - Automatically calls validate_transfer_configuration() on go-bit write
 * - Validation includes: opcode, width, size, alignment, ASID, security policy
 * - Prevents transfer start if any validation fails
 *
 * 5. Transaction Drain Guarantees:
 * - OT internal bus: Guaranteed completion (blocking b_transport semantics)
 * - SoC buses (CTN/System): No guarantees per specification
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
 */

// ============================================================================
// DMA Transfer Engine Operation Implementation
// ============================================================================

/**
 * @brief DMA transfer engine thread - core autonomous transfer execution
 *
 * Main transfer engine thread that waits for transfer start events and executes
 * complete DMA transfers with chunked decomposition, address advancement, and
 * progress tracking.
 *
 * Thread lifecycle:
 * - IDLE: Wait for m_transfer_start_event
 * - ACTIVE: Execute complete transfer via execute_transfer()
 * - COMPLETION: Return to IDLE, wait for next transfer
 *
 * Abort handling:
 * - Monitors m_transfer_abort_event during transfer execution
 * - Gracefully terminates transfer, sets STATUS.aborted
 *
 * Per detailed design Section 7.3: "Transfer engine implements autonomous
 * read-write transaction sequencing with chunk-based progress tracking."
 */
void secure_dma_model::transfer_engine_thread() {
  while (true) {
    // Wait for transfer start event (triggered by CONTROL.go write)
    wait(m_transfer_start_event);

    CSML_INFO(1, logger) << "Transfer engine activated - starting transfer execution" << std::endl;

    // Execute complete transfer (returns false if aborted, true if completed)
    bool completed = execute_transfer();

    if (completed) {
    CSML_INFO(1, logger) << "Transfer completed successfully" << std::endl;
    } else {
    CSML_INFO(1, logger) << "Transfer aborted by software" << std::endl;
    }

    // Transfer complete or aborted - thread returns to waiting state
  }
}

/**
 * @brief Execute complete DMA transfer with chunked decomposition (core)
 * @return true if transfer completed successfully, false if aborted
 *
 * Implements complete transfer state machine:
 * 1. Transfer Setup: Initialize address pointers and byte counters
 * 2. Transaction Loop: Sequential read-write pairs until completion
 * 3. Address Advancement: Update source/destination per addressing mode
 * 4. Chunk Tracking: Monitor boundaries, generate chunk_done interrupts
 * 5. Completion Handling: Set STATUS.done, unlock CFG_REGWEN, return to IDLE
 *
 * Integrates with:
 * - get_transfer_width_bytes()
 * - advance_source_address(), advance_destination_address()
 * - select_bus_for_transaction()
 * - set_interrupt_state()
 */
bool secure_dma_model::execute_transfer() {
  // -------------------------------------------------------------------------
  // TRANSFER SETUP: Initialize internal state variables
  // -------------------------------------------------------------------------

  // Read configuration registers
  uint32_t total_size = static_cast<uint32_t>(TOTAL_DATA_SIZE);
  uint32_t chunk_size = static_cast<uint32_t>(CHUNK_DATA_SIZE);
  uint32_t width_bytes = get_transfer_width_bytes();

  // Initialize 64-bit address pointers
  m_current_src_addr = (static_cast<uint64_t>(SRC_ADDR_HI) << 32) |
                       static_cast<uint32_t>(SRC_ADDR_LO);
  m_current_dst_addr = (static_cast<uint64_t>(DST_ADDR_HI) << 32) |
                       static_cast<uint32_t>(DST_ADDR_LO);

  // Store chunk start addresses for wrap mode
  m_chunk_start_src_addr = m_current_src_addr;
  m_chunk_start_dst_addr = m_current_dst_addr;

  // Initialize byte counters
  m_bytes_remaining = total_size;
  m_current_chunk_bytes_remaining =
      (chunk_size < m_bytes_remaining) ? chunk_size : m_bytes_remaining;

  // Read hardware handshake mode
  uint32_t control_reg = static_cast<uint32_t>(CONTROL);
  bool hardware_handshake_enable = (control_reg & (1U << 4)) != 0;

  CSML_INFO(2, logger) << "Transfer setup complete - " << "SRC=0x" << std::hex << m_current_src_addr << " DST=0x" << m_current_dst_addr << " TOTAL=" << std::dec << total_size << " CHUNK=" << chunk_size << " WIDTH=" << width_bytes << " bytes" << std::endl;

  // -------------------------------------------------------------------------
  // TRANSACTION LOOP: Execute sequential read-write pairs
  // -------------------------------------------------------------------------
  // In hardware handshake mode: Execute ONE CHUNK per trigger event, wait
  // between chunks In normal mode: Execute ALL chunks continuously until
  // completion

  bool clear_chunk_done_on_next_chunk_start = false;


  while (m_bytes_remaining > 0) {
    // Yield so a concurrent CONTROL.abort write can be observed )
    wait(SC_ZERO_TIME);

    // Check for reset assertion (active-low)
    if (rst_ni.read() == false) {
      CSML_INFO(1, logger) << "Reset detected during transfer - terminating" << std::endl;
      return false; // Transfer aborted by reset
    }

    // Check for abort request
    if (m_transfer_abort_event.triggered()) {
      CSML_INFO(1, logger) << "Abort detected - terminating transfer loop" << std::endl;
      // Abort already handled in CONTROL callback (STATUS.aborted set, busy
      // cleared)
      return false; // Transfer aborted
    }

    // Execute single read-write transaction pair
    bool transaction_success = execute_single_transaction();

    if (!transaction_success) {
      // Bus error occurred - ERROR_CODE and STATUS.error already set
      CSML_INFO(1, logger) << "Bus error - aborting transfer" << std::endl;

      // Transition to IDLE state
      m_dma_busy = false;
      m_dma_busy_next = false;

      // Clear STATUS.busy bit
      uint32_t status_reg = static_cast<uint32_t>(STATUS);
      status_reg &= ~0x1;
      STATUS = status_reg;

      // Clear go bit from CONTROL
      control_reg = static_cast<uint32_t>(CONTROL);
      control_reg &= ~(1U << 31);
      CONTROL = control_reg;

      return false; // Transfer failed
    }

    if (clear_chunk_done_on_next_chunk_start && !hardware_handshake_enable) {
      uint32_t status_reg = static_cast<uint32_t>(STATUS);
      if (status_reg & (1U << 5)) {
        status_reg &= ~(1U << 5);  // clear STATUS.chunk_done
        STATUS = status_reg;
      }
      clear_interrupt_state(false, true, false); // clear INTR_STATE.dma_chunk_done
      clear_chunk_done_on_next_chunk_start = false;
    
    CSML_INFO(2, logger) << "Auto-cleared STATUS.chunk_done/INTR_STATE.dma_chunk_done at next chunk start" << std::endl;
    }


    // -----------------------------------------------------------------------
    // ADDRESS ADVANCEMENT: Update addresses per addressing mode
    // -----------------------------------------------------------------------

    advance_source_address(m_current_src_addr, m_chunk_start_src_addr,
                           chunk_size, width_bytes);
    advance_destination_address(m_current_dst_addr, m_chunk_start_dst_addr,
                                chunk_size, width_bytes);

    // Update address registers for software visibility
    update_src_addr_registers(m_current_src_addr);
    update_dst_addr_registers(m_current_dst_addr);

    // -----------------------------------------------------------------------
    // BYTE COUNTING: Decrement counters
    // -----------------------------------------------------------------------

    m_bytes_remaining -= width_bytes;
    m_current_chunk_bytes_remaining -= width_bytes;

    CSML_INFO(3, logger) << "Transaction complete - bytes_remaining=" << m_bytes_remaining << ", chunk_bytes_remaining=" << m_current_chunk_bytes_remaining << std::endl;

    // -----------------------------------------------------------------------
    // CHUNK BOUNDARY CHECK: Handle chunk completion
    // -----------------------------------------------------------------------

    if (m_current_chunk_bytes_remaining == 0 && m_bytes_remaining > 0) {
      // Chunk completed, more data remains
      CSML_INFO(2, logger) << "Chunk completed (" << chunk_size << " bytes transferred)" << std::endl;

      // Apply wrap mode if enabled
      bool src_increment = false, src_wrap = false;
      bool dst_increment = false, dst_wrap = false;
      get_source_addressing_mode(src_increment, src_wrap);
      get_destination_addressing_mode(dst_increment, dst_wrap);

      if (src_wrap && src_increment) {
        m_current_src_addr = m_chunk_start_src_addr;
      CSML_INFO(3, logger) << "Source address wrapped to chunk start: 0x" << std::hex << m_current_src_addr << std::dec << std::endl;
      } else {
        // Update chunk start for next chunk (non-wrap mode)
        m_chunk_start_src_addr = m_current_src_addr;
      }

      if (dst_wrap && dst_increment) {
        m_current_dst_addr = m_chunk_start_dst_addr;
      CSML_INFO(3, logger) << "Destination address wrapped to chunk start: 0x" << std::hex << m_current_dst_addr << std::dec << std::endl;
      } else {
        // Update chunk start for next chunk (non-wrap mode)
        m_chunk_start_dst_addr = m_current_dst_addr;
      }

      // Generate chunk_done interrupt (non-handshake mode only)
      if (!hardware_handshake_enable) {
        uint32_t status_reg = static_cast<uint32_t>(STATUS);
        status_reg |= (1U << 5); // Set STATUS.chunk_done (bit 5)
        STATUS = status_reg;

        set_interrupt_state(false, true, false); // Set chunk_done interrupt
      CSML_INFO(2, logger) << "Chunk done interrupt generated (STATUS.chunk_done=1)" << std::endl;
      }

      clear_chunk_done_on_next_chunk_start = true;

      // Reload chunk byte counter for next chunk
      m_current_chunk_bytes_remaining =
          (chunk_size < m_bytes_remaining) ? chunk_size : m_bytes_remaining;
      
      // In hardware handshake mode, wait for next trigger before continuing
      if (hardware_handshake_enable) {
        // Yield one delta so any pending signal updates from the peripheral
        // (e.g. dma_trigger going LOW as FIFO drains below watermark) propagate
        // before we read lsio_trigger. Without this, lsio_trigger.read() returns
        // the stale HIGH from the chunk just drained, causing DMA to skip the
        // wait(m_handshake_trigger_event) and read from an empty FIFO.
        wait(SC_ZERO_TIME);

        // RTL: trigger is level-sensitive — if lsio_trigger is already HIGH
        // (peripheral refilled FIFO before we checked), proceed immediately.
        uint32_t enable_mask = static_cast<uint32_t>(HANDSHAKE_INTR_ENABLE) & 0x7FF;
        bool already_high = false;
        for (int i = 0; i < 11; i++) {
          if ((enable_mask & (1 << i)) && lsio_trigger[i].read()) {
            m_last_asserted_trigger_index = i;
            already_high = true;
            break;
          }
        }
        if (already_high) {
        CSML_INFO(2, logger) << "Chunk complete - lsio_trigger already HIGH, proceeding immediately" << std::endl;
        } else {
          CSML_INFO(2, logger) << "Chunk complete in handshake mode - waiting for next trigger" << std::endl;
          // Wait for next handshake trigger event (from handshake_monitor_thread)
          wait(m_handshake_trigger_event);
      CSML_INFO(2, logger) << "Next trigger received - continuing transfer" << std::endl;
        }
      } else {
  CSML_INFO(2, logger) << "Next chunk started - chunk_bytes=" << m_current_chunk_bytes_remaining << std::endl;
      }
    }
  }

  // -------------------------------------------------------------------------
  // TRANSFER COMPLETION: All bytes transferred successfully
  // -------------------------------------------------------------------------

  CSML_INFO(1, logger) << "All data transferred successfully - " << total_size << " bytes completed" << std::endl;

  // -------------------------------------------------------------------------
  // HASH FINALIZATION (if hashing active)
  // -------------------------------------------------------------------------

  if (m_hashing_active) {
    // Finalize hash computation and store digest
    if (!hash_finalize()) {
      CSML_INFO(1, logger) << "Hash finalization failed" << std::endl;
      // Hash finalization failed - set error but transfer data is still valid
      m_error_code |= (1U << 2); // Set opcode_error (hash operation failed)
      ERROR_CODE = m_error_code;

      uint32_t status_reg_temp = static_cast<uint32_t>(STATUS);
      status_reg_temp |= (1U << 3); // Set STATUS.error
      STATUS = status_reg_temp;

      set_interrupt_state(false, false, true); // Trigger error interrupt
      // Continue to set done status (data transfer succeeded even if hash
      // failed)
    } else {
  CSML_INFO(2, logger) << "Hash computation finalized - digest valid" << std::endl;
    }
  }

  // Set STATUS.done bit (bit 1)
  uint32_t status_reg = static_cast<uint32_t>(STATUS);
  status_reg |= (1U << 1);
  STATUS = status_reg;

  // Generate done interrupt via
  set_interrupt_state(true, false, false);
  CSML_INFO(2, logger) << "Transfer done interrupt generated (STATUS.done=1)" << std::endl;

  // Transition to IDLE state (unlock CFG_REGWEN)
  m_dma_busy = false;
  m_dma_busy_next = false;

  // Clear STATUS.busy bit (bit 0)
  status_reg = static_cast<uint32_t>(STATUS);
  status_reg &= ~0x1;
  STATUS = status_reg;

  // Clear go bit from CONTROL register (non-handshake mode)
  if (!hardware_handshake_enable) {
    control_reg = static_cast<uint32_t>(CONTROL);
    control_reg &= ~(1U << 31);
    CONTROL = control_reg;
  CSML_INFO(2, logger) << "CONTROL.go auto-cleared (non-handshake mode)" << std::endl;
  }

  CSML_INFO(1, logger) << "Transfer engine returned to IDLE - CFG_REGWEN unlocked (0x6)" << std::endl;

  return true; // Transfer completed successfully
}


/**
 * @brief Execute single read-write transaction pair (helper)
 * @return true if transaction successful, false if bus error occurred
 *
 * Performs one complete read-write cycle:
 * 1. Read data from source address using source bus interface
 * 2. Write data to destination address using destination bus interface
 * 3. Handle TLM response errors, update ERROR_CODE if needed
 *
 * Integrates with:
 * - get_transfer_width_bytes(), generate_byte_enable_mask()
 * - select_bus_for_transaction(), create_tlm_transaction()
 * - set_interrupt_state() on error
 */
bool secure_dma_model::execute_single_transaction() {
  uint32_t width_bytes = get_transfer_width_bytes();

  // -------------------------------------------------------------------------
  // SOURCE READ TRANSACTION
  // -------------------------------------------------------------------------

  // Select source bus interface
  BusInterface src_bus = select_bus_for_transaction(true);

  // Generate byte enable mask for sub-word transfers
  uint8_t byte_enable_mask = generate_byte_enable_mask(
      static_cast<uint32_t>(m_current_src_addr), width_bytes);

  // Create TLM payload for read transaction
  tlm::tlm_generic_payload trans;
  create_tlm_transaction(trans, tlm::TLM_READ_COMMAND, m_current_src_addr,
                         m_transfer_data_buffer, width_bytes, byte_enable_mask);

  sc_time delay = SC_ZERO_TIME;

  // Issue read transaction on appropriate bus socket
  switch (src_bus) {
  case BusInterface::OT_INTERNAL:
    ot_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::CTN_BUS:
    ctn_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::SYSTEM_BUS:
    sys_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::INVALID:
  default:
    CSML_INFO(1, logger) << "Invalid source bus interface" << std::endl;
    m_error_code |= (1U << 7); // Set asid_error
    ERROR_CODE = m_error_code;
    set_interrupt_state(false, false, true); // Trigger error interrupt
    return false;
  }

  // Check source read response
  if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
    CSML_INFO(1, logger) << "Source read bus error at address 0x" << std::hex << m_current_src_addr << std::dec << std::endl;
    m_error_code |= (1U << 4); // Set bus_error (bit 4)
    ERROR_CODE = m_error_code;

    uint32_t status_reg = static_cast<uint32_t>(STATUS);
    status_reg |= (1U << 3); // Set STATUS.error
    STATUS = status_reg;

    set_interrupt_state(false, false, true); // Trigger error interrupt
    return false;
  }

  wait(delay); // Apply annotated timing

  CSML_INFO(3, logger) << "Source read successful - SRC=0x" << std::hex << m_current_src_addr << " DATA=0x";
  for (unsigned int i = 0; i < width_bytes; i++) {
  CSML_INFO(3, logger) << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(m_transfer_data_buffer[i]);
  }
  CSML_INFO(3, logger) << std::dec << std::endl;

  // -------------------------------------------------------------------------
  // HASH UPDATE (if hashing active)
  // -------------------------------------------------------------------------

  if (m_hashing_active) {
    // Feed transferred data to hash engine
    if (!hash_update_data(m_transfer_data_buffer, width_bytes)) {
      CSML_INFO(1, logger) << "Hash update failed during transfer" << std::endl;
      m_error_code |= (1U << 2); // Set opcode_error (hash operation failed)
      ERROR_CODE = m_error_code;

      uint32_t status_reg = static_cast<uint32_t>(STATUS);
      status_reg |= (1U << 3); // Set STATUS.error
      STATUS = status_reg;

      set_interrupt_state(false, false, true); // Trigger error interrupt
      return false;
    }
  CSML_INFO(3, logger) << "Hash updated with " << width_bytes << " bytes" << std::endl;
  }

  // -------------------------------------------------------------------------
  // DESTINATION WRITE TRANSACTION
  // -------------------------------------------------------------------------

  // Select destination bus interface
  BusInterface dst_bus = select_bus_for_transaction(false);

  // Generate byte enable mask for destination write
  byte_enable_mask = generate_byte_enable_mask(
      static_cast<uint32_t>(m_current_dst_addr), width_bytes);

  // Create TLM payload for write transaction
  create_tlm_transaction(trans, tlm::TLM_WRITE_COMMAND, m_current_dst_addr,
                         m_transfer_data_buffer, width_bytes, byte_enable_mask);

  delay = SC_ZERO_TIME;

  // Issue write transaction on appropriate bus socket
  switch (dst_bus) {
  case BusInterface::OT_INTERNAL:
    ot_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::CTN_BUS:
    ctn_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::SYSTEM_BUS:
    sys_initiator_socket->b_transport(trans, delay);
    break;

  case BusInterface::INVALID:
  default:
    CSML_INFO(1, logger) << "Invalid destination bus interface" << std::endl;
    m_error_code |= (1U << 7); // Set asid_error
    ERROR_CODE = m_error_code;
    set_interrupt_state(false, false, true); // Trigger error interrupt
    return false;
  }

  // Check destination write response
  if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
    CSML_INFO(1, logger) << "Destination write bus error at address 0x" << std::hex << m_current_dst_addr << std::dec << std::endl;
    m_error_code |= (1U << 4); // Set bus_error (bit 4)
    ERROR_CODE = m_error_code;

    uint32_t status_reg = static_cast<uint32_t>(STATUS);
    status_reg |= (1U << 3); // Set STATUS.error
    STATUS = status_reg;

    set_interrupt_state(false, false, true); // Trigger error interrupt
    return false;
  }

  wait(delay); // Apply annotated timing

  CSML_INFO(3, logger) << "Destination write successful - DST=0x" << std::hex << m_current_dst_addr << std::dec << std::endl;

  return true; // Transaction pair completed successfully
}

/**
 * @brief Register callbacks and initialize transfer engine
 *
 * implements DMA Transfer Engine Operation through:
 * - transfer_engine_thread(): Autonomous transaction sequencing SC_THREAD
 * - execute_transfer(): Complete transfer execution with chunking
 * - execute_single_transaction(): Read-write transaction pair helper
 * - Integration with via m_transfer_start_event and m_transfer_abort_event
 *
 * No additional register callbacks required beyond through.
 * Transfer execution triggered by CONTROL.go write , which notifies
 * m_transfer_start_event to wake up transfer_engine_thread.
 */

// ============================================================================
// Inline SHA-2 Hash Computation Implementation
// ============================================================================

/**
 * @brief Initialize SHA-2 hash engine for new transfer
 * @param opcode Hash algorithm opcode (0x1=SHA256, 0x2=SHA384, 0x3=SHA512)
 * @return true if initialization successful, false if invalid opcode
 *
 * Initializes SHA-2 hash context based on selected algorithm using OpenSSL EVP
 * API. Clears STATUS.sha2_digest_valid and prepares hash engine for incremental
 * updates.
 *
 * Per detailed design Section 7.6: "Reset SHA-2 internal state to algorithm's
 * initial values (IV). Clear any accumulated data."
 *
 * Integration with architecture map:
 * - Triggered when CONTROL.initial_transfer=1
 * - Clears STATUS.sha2_digest_valid (bit 4)
 * - Prepares for hash_update_data() calls during transfer
 */
bool secure_dma_model::hash_init(uint32_t opcode) {
  // Free existing hash context if present
  if (m_hash_ctx != nullptr) {
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(m_hash_ctx));
    m_hash_ctx = nullptr;
  }

  // Create new hash context
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    CSML_INFO(1, logger) << "Failed to create EVP_MD_CTX for hash engine" << std::endl;
    return false;
  }

  // Select hash algorithm based on opcode
  const EVP_MD *md = nullptr;
  switch (opcode) {
  case 0x1: // SHA-256
    md = EVP_sha256();
    m_hash_algorithm = 0x1;
    CSML_INFO(2, logger) << "Initializing SHA-256 hash engine" << std::endl;
    break;

  case 0x2: // SHA-384
    md = EVP_sha384();
    m_hash_algorithm = 0x2;
    CSML_INFO(2, logger) << "Initializing SHA-384 hash engine" << std::endl;
    break;

  case 0x3: // SHA-512
    md = EVP_sha512();
    m_hash_algorithm = 0x3;
    CSML_INFO(2, logger) << "Initializing SHA-512 hash engine" << std::endl;
    break;

  default:
    CSML_INFO(1, logger) << "Invalid hash opcode 0x" << std::hex << opcode << std::dec << std::endl;
    EVP_MD_CTX_free(ctx);
    return false;
  }

  // Initialize hash context with selected algorithm
  if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
    CSML_INFO(1, logger) << "Failed to initialize hash algorithm" << std::endl;
    EVP_MD_CTX_free(ctx);
    return false;
  }

  // Store hash context and activate hashing
  m_hash_ctx = static_cast<void *>(ctx);
  m_hashing_active = true;

  // Clear STATUS.sha2_digest_valid (bit 4)
  uint32_t status_reg = static_cast<uint32_t>(STATUS);
  status_reg &= ~(1U << 4);
  STATUS = status_reg;

  // Clear digest buffer
  std::memset(m_hash_digest, 0, sizeof(m_hash_digest));

  CSML_INFO(2, logger) << "Hash engine initialized successfully" << std::endl;

  return true;
}

/**
 * @brief Feed data word to SHA-2 hash engine during transfer
 * @param data Pointer to 4-byte data word read from source
 * @param length Number of bytes to hash (must be 4 for inline hashing)
 * @return true if update successful, false on error
 *
 * Incrementally updates SHA-2 hash state with transferred data using OpenSSL
 * EVP API. Called for each read transaction during hash-enabled transfer.
 *
 * Per detailed design Section 7.6: "Feed each 4-byte data word read from source
 * to hash engine's update function. Hash state is updated incrementally without
 * producing output."
 *
 * Integration with architecture map:
 * - Called in execute_single_transaction() after successful read
 * - Maintains hash state across chunk boundaries (multi-chunk accumulation)
 * - Does not modify transferred data (transparent pass-through)
 */
bool secure_dma_model::hash_update_data(const unsigned char *data, uint32_t length) {
  if (!m_hashing_active || m_hash_ctx == nullptr) {
    CSML_INFO(1, logger) << "hash_update_data called but hashing not active" << std::endl;
    return false;
  }

  EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(m_hash_ctx);

  // Update hash context with data
  if (EVP_DigestUpdate(ctx, data, length) != 1) {
    CSML_INFO(1, logger) << "Failed to update hash with data" << std::endl;
    return false;
  }

  CSML_INFO(3, logger) << "Hash updated with " << length << " bytes: 0x";
  for (unsigned int i = 0; i < length; i++) {
  CSML_INFO(3, logger) << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
  }
  CSML_INFO(3, logger) << std::dec << std::endl;

  return true;
}

/**
 * @brief Finalize SHA-2 hash computation and store digest
 * @return true if finalization successful, false on error
 *
 * Completes SHA-2 hash computation, applies endianness conversion if requested,
 * stores digest in SHA2_DIGEST registers, and sets STATUS.sha2_digest_valid.
 *
 * Per detailed design Section 7.6: "Call hash engine's finalization function,
 * retrieve digest, apply endianness conversion if digest_swap=1, store into
 * SHA2_DIGEST registers, set sha2_digest_valid=1."
 *
 * Integration with architecture map:
 * - Called on transfer completion (m_bytes_remaining==0)
 * - Updates SHA2_DIGEST_0-7 (SHA-256), 0-11 (SHA-384), or 0-15 (SHA-512)
 * - Sets STATUS.sha2_digest_valid (bit 4)
 * - Applies CONTROL.digest_swap endianness control
 * - Frees hash context and clears m_hashing_active
 */
bool secure_dma_model::hash_finalize() {
  if (!m_hashing_active || m_hash_ctx == nullptr) {
    CSML_INFO(1, logger) << "hash_finalize called but hashing not active" << std::endl;
    return false;
  }

  EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(m_hash_ctx);

  // Finalize hash computation and retrieve digest
  unsigned int digest_length = 0;
  if (EVP_DigestFinal_ex(ctx, m_hash_digest, &digest_length) != 1) {
    CSML_INFO(1, logger) << "Failed to finalize hash computation" << std::endl;
    EVP_MD_CTX_free(ctx);
    m_hash_ctx = nullptr;
    m_hashing_active = false;
    return false;
  }

  CSML_INFO(2, logger) << "Hash computation finalized - digest length=" << digest_length << " bytes" << std::endl;

  // Free hash context
  EVP_MD_CTX_free(ctx);
  m_hash_ctx = nullptr;
  m_hashing_active = false;

  // Read CONTROL register to check digest_swap bit (bit 5 per register map)
  uint32_t control_reg = static_cast<uint32_t>(CONTROL);
  bool digest_swap = (control_reg & (1U << 5)) != 0;

  // Determine number of digest registers to populate based on algorithm
  unsigned int num_digest_regs = 0;
  switch (m_hash_algorithm) {
  case 0x1: // SHA-256: 8 registers (256 bits / 32 bits per register)
    num_digest_regs = 8;
    break;
  case 0x2: // SHA-384: 12 registers (384 bits / 32 bits per register)
    num_digest_regs = 12;
    break;
  case 0x3: // SHA-512: 16 registers (512 bits / 32 bits per register)
    num_digest_regs = 16;
    break;
  default:
    CSML_INFO(1, logger) << "Invalid hash algorithm value" << std::endl;
    return false;
  }

  // Store digest into SHA2_DIGEST registers with optional endianness conversion
  for (unsigned int i = 0; i < num_digest_regs; i++) {
    // OpenSSL EVP_DigestFinal_ex outputs SHA-256 in big-endian byte order
    // (FIPS 180-4): byte 0 is the MSByte of the hash word.
    // Assemble as a big-endian uint32_t so the native (digest_swap=0) register
    // value matches hardware convention (H0 stored as-is). digest_swap=1 then
    // byte-reverses each word so that casting SHA2_DIGEST[] to uint8_t* on a
    // little-endian CPU yields bytes in SW-hash-comparable order.
    uint32_t digest_word =
        (static_cast<uint32_t>(m_hash_digest[i * 4 + 0]) << 24) |
        (static_cast<uint32_t>(m_hash_digest[i * 4 + 1]) << 16) |
        (static_cast<uint32_t>(m_hash_digest[i * 4 + 2]) << 8)  |
        (static_cast<uint32_t>(m_hash_digest[i * 4 + 3]) << 0);

    // Apply byte-swap endianness conversion if digest_swap=1
    if (digest_swap) {
      digest_word = byte_swap_32(digest_word);
    }

    // Store into SHA2_DIGEST register
    SHA2_DIGEST[i] = digest_word;

  CSML_INFO(3, logger) << "SHA2_DIGEST[" << i << "] = 0x" << std::hex << digest_word << std::dec << (digest_swap ? " (byte-swapped)" : " (native)") << std::endl;
  }

  // Set STATUS.sha2_digest_valid (bit 4)
  uint32_t status_reg = static_cast<uint32_t>(STATUS);
  status_reg |= (1U << 4);
  STATUS = status_reg;

  CSML_INFO(2, logger) << "Digest stored in SHA2_DIGEST registers" << " - STATUS.sha2_digest_valid=1" << std::endl;

  return true;
}

/**
 * @brief Apply byte-swap endianness conversion to 32-bit word (helper)
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
uint32_t secure_dma_model::byte_swap_32(uint32_t word) {
  return ((word & 0x000000FFU) << 24) | ((word & 0x0000FF00U) << 8) |
         ((word & 0x00FF0000U) >> 8) | ((word & 0xFF000000U) >> 24);
}

/**
 * @brief Register (Inline SHA-2 Hash Computation) callbacks
 *
 * integrates SHA-2 hash computation into transfer datapath through:
 * - Hash initialization in handle_write_CONTROL() when initial_transfer=1
 * - Incremental hash updates in execute_single_transaction()
 * - Hash finalization in execute_transfer() on transfer completion
 * - Digest register population with endianness control
 * - STATUS.sha2_digest_valid management
 *
 * No new register callbacks required beyond existing infrastructure.
 * Hash computation integrates transparently into transfer engine.
 *
 * Per functionality list "Provides concurrent cryptographic hash
 * computation during data transfer operations through integrated SHA-2
 * accelerator supporting SHA-256, SHA-384, and SHA-512 algorithms."
 */

// ============================================================================
// Hardware Handshaking Mechanism Implementation
// ============================================================================

/**
 * @brief Perform automatic interrupt clearing write for trigger source
 * @param trigger_index Index of trigger source (0-10)
 * @return true if clearing write successful or not needed, false on error
 *
 * Executes configurable write transaction to peripheral interrupt register for
 * automatic interrupt acknowledgment in hardware handshake mode.
 *
 * Steps:
 * 1. Check if automatic clearing enabled for this trigger
 * (CLEAR_INTR_SRC[trigger_index])
 * 2. If enabled:
 * a. Read CLEAR_INTR_BUS[trigger_index] for bus selection (0=CTN/System, 1=OT)
 * b. Read INTR_SRC_ADDR[trigger_index] for destination address
 * c. Read INTR_SRC_WR_VAL[trigger_index] for write data value
 * d. Create TLM write transaction with 4-byte width
 * e. Route transaction via selected bus interface socket
 * f. Return false if bus error occurs
 *
 * Per detailed design Section 1.7.3: "When an enabled lsio_trigger input
 * asserts and CLEAR_INTR_SRC is enabled for that trigger, the DMA automatically
 * performs a write transaction to the configured address with the configured
 * data value."
 *
 * Integration with bus routing:
 * - CLEAR_INTR_BUS[i]=0: Use ADDR_SPACE_ID-based routing (CTN or System)
 * - CLEAR_INTR_BUS[i]=1: Force OT-internal bus routing
 *
 * @note Clearing write executed BEFORE chunk transfer initiation
 * @note Returns true if clearing not enabled (no error, just not needed)
 * @note Uses blocking b_transport (LT TLM semantics)
 */
bool secure_dma_model::perform_interrupt_clearing_write(int trigger_index) {

  // Read CLEAR_INTR_SRC register to check if clearing enabled for this trigger
  uint32_t clear_intr_src =
      static_cast<uint32_t>(CLEAR_INTR_SRC) & 0x7FF; // 11 bits
  if (!(clear_intr_src & (1 << trigger_index))) {
    // Automatic clearing not enabled for this trigger - return success (no-op)
    CSML_INFO(3, logger) << "Automatic clearing not enabled for trigger " << trigger_index << " (CLEAR_INTR_SRC[" << trigger_index << "]=0)" << std::endl;
    return true;
  }

  CSML_INFO(2, logger) << "Performing automatic interrupt clearing write for trigger " << trigger_index << std::endl;

  // Read bus selection for this trigger (CLEAR_INTR_BUS[trigger_index])
  uint32_t clear_intr_bus =
      static_cast<uint32_t>(CLEAR_INTR_BUS) & 0x7FF; // 11 bits
  bool use_ot_bus =
      (clear_intr_bus & (1 << trigger_index)) != 0; // 0=CTN/System, 1=OT

  // Read destination address for clearing write (INTR_SRC_ADDR[trigger_index])
  uint32_t clearing_addr = static_cast<uint32_t>(INTR_SRC_ADDR[trigger_index]);

  // Read write data value for clearing write (INTR_SRC_WR_VAL[trigger_index])
  uint32_t clearing_data =
      static_cast<uint32_t>(INTR_SRC_WR_VAL[trigger_index]);

  CSML_INFO(2, logger) << "Clearing write - addr=0x" << std::hex << clearing_addr << ", data=0x" << clearing_data << std::dec << ", bus=" << (use_ot_bus ? "OT-internal" : "CTN/System") << std::endl;

  // Create TLM generic payload for clearing write transaction
  tlm::tlm_generic_payload trans;
  trans.set_command(tlm::TLM_WRITE_COMMAND);
  trans.set_address(clearing_addr);

  // Prepare write data buffer (4 bytes, little-endian)
  unsigned char data_buffer[4];
  data_buffer[0] = (clearing_data >> 0) & 0xFF;
  data_buffer[1] = (clearing_data >> 8) & 0xFF;
  data_buffer[2] = (clearing_data >> 16) & 0xFF;
  data_buffer[3] = (clearing_data >> 24) & 0xFF;

  trans.set_data_ptr(data_buffer);
  trans.set_data_length(4);
  trans.set_streaming_width(4);
  trans.set_byte_enable_ptr(nullptr); // All bytes enabled
  trans.set_byte_enable_length(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  // This payload is built by hand rather than through create_tlm_transaction,
  // so it needs the same OTHERS_SOURCE_ID stamp applied there.
  auto* axi_ext      = new sep::sep_axi_extension();
  axi_ext->source_id = sep::OTHERS_SOURCE_ID;
  trans.set_extension(axi_ext);

  // Annotate timing delay for clearing write transaction
  sc_time delay = SC_ZERO_TIME;

  // Select bus interface based on CLEAR_INTR_BUS[trigger_index]
  if (use_ot_bus) {
    // Use OT-internal bus (32-bit)
    CSML_INFO(3, logger) << "Routing clearing write via OT-internal bus" << std::endl;
    ot_initiator_socket->b_transport(trans, delay);
  } else {
    // Use CTN or System bus based on ADDR_SPACE_ID configuration
    // For simplicity, assume CTN bus (32-bit) for clearing writes
    // (Peripheral interrupt registers typically in CTN address space)
    CSML_INFO(3, logger) << "Routing clearing write via CTN bus" << std::endl;
    ctn_initiator_socket->b_transport(trans, delay);
  }

  // Check transaction response status
  if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
    CSML_INFO(1, logger) << "Interrupt clearing write failed - bus error for trigger " << trigger_index << " (address 0x" << std::hex << clearing_addr << std::dec << ")" << std::endl;
    return false;
  }

  CSML_INFO(2, logger) << "Interrupt clearing write successful for trigger " << trigger_index << std::endl;

  // Add delay to quantum keeper for timing annotation
  m_qk.inc(delay);

  return true;
}

/**
 * @brief Register (Hardware Handshaking Mechanism) callbacks
 *
 * does not require new register callbacks beyond infrastructure.
 * All hardware handshake configuration registers are storage-only:
 * - HANDSHAKE_INTR_ENABLE: Trigger enable mask (11 bits)
 * - CLEAR_INTR_SRC: Automatic clearing enable mask (11 bits)
 * - CLEAR_INTR_BUS: Bus selection for clearing writes (11 bits)
 * - INTR_SRC_ADDR[0-10]: Clearing write addresses (11 registers)
 * - INTR_SRC_WR_VAL[0-10]: Clearing write data values (11 registers)
 *
 * Hardware handshake functionality implemented through:
 * - handshake_monitor_thread(): Monitors lsio_trigger inputs, detects rising
 * edges
 * - perform_interrupt_clearing_write(): Executes automatic peripheral interrupt
 * clearing
 * - Integration with transfer_engine_thread(): Chunk transfer execution on
 * trigger events
 * - Go-bit persistence logic in handle_write_CONTROL(): Does not auto-clear in
 * handshake mode
 * - Chunk_done suppression in execute_transfer(): Not generated in handshake
 * mode
 *
 * Per detailed design Section 1.7.1: "Software sets the
 * hardware_handshake_enable bit in the CONTROL register and configures the
 * HANDSHAKE_INTR_ENABLE register to enable specific lsio_trigger input lines
 * corresponding to peripherals."
 *
 * Per functionality list "Hardware Handshake Trigger Mechanism for Peripheral
 * FIFO Service: Responds to lsio_trigger[10:0] input assertions by autonomously
 * initiating chunk transfers, enabling DMA-driven FIFO drain/fill operations
 * without per-chunk software intervention."
 */
