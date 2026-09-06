// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn.cpp
 * @brief OTBN TLM model implementation
 * 
 * Implements the behavioral logic for the OTBN cryptographic accelerator including:
 * - Constructor and destructor with component initialization
 * - Register access callbacks for command execution and state management
 * - Memory access callbacks with state-based access control
 * - Algorithm execution framework and DMEM/IMEM secure wipe operations
 * - CRC-32-IEEE checksum calculation for memory integrity
 * - Key Manager TLM transport for sideload key programming
 * - Interface channel implementations for EDN, OTP, and Life Cycle interfaces
 * - Reset handling and interrupt/alert management
 * 
 * The implementation follows the OTBN specification with TLM-appropriate abstractions
 * for timing, algorithms, and external interfaces.
 */

#include "otbn.h"
#include "otbn_algorithm_rsa_2048.h"
#include "otbn_algorithm_rsa_2048_key_enabled.h"
#include "otbn_algorithm_summation.h"
#include "otbn_algorithm_otbn_loop.h"
#include "otbn_algorithm_rnd_test.h"
#include "otbn_algorithm_p256_ecdsa.h"
#include "otbn_algorithm_smoke.h"
#include "otbn_algorithm_rsa_3072.h"
#include "otbn_algorithm_callback_cov.h"
#include <cstdlib>  ///< For srand() and rand() per otbn_plan.md specification

/**
 * @name Interface Channel Implementations
 * @{
 * 
 * These classes implement the response interfaces that the model provides.
 */

/**
 * @brief OTP Key Response Channel implementation
 * 
 * Provides scrambling keys, nonces, and seeds from OTP controller.
 * Returns mock values for TLM functional modeling.
 */
class otbn_ip::otp_key_rsp_channel : public otp_key_rsp_if, public sc_module {
public:
    SC_HAS_PROCESS(otp_key_rsp_channel);
    otp_key_rsp_channel(sc_module_name name) : sc_module(name) {}

    bool key_available() override {
        return true;
    }

    void get_scramble_key(uint32_t key[4], uint32_t& nonce, uint32_t& seed) override {
        // Return mock 128-bit scrambling key with nonce and seed
        key[0] = 0x12345678;
        key[1] = 0x9ABCDEF0;
        key[2] = 0x11223344;
        key[3] = 0x55667788;
        nonce = 0xAABBCCDD;
        seed = 0xEEFF0011;
    }
};

/**
 * @brief Life Cycle Escalate Response Channel implementation
 * 
 * Acknowledges Life Cycle escalation signals.
 * NOTE: Removed - lc_escalate_rsp is now sc_out<bool>, no channel needed
 */
// class otbn_ip::lc_escalate_rsp_channel : public lc_ctrl_rsp_if, public sc_module {
// public:
//     SC_HAS_PROCESS(lc_escalate_rsp_channel);
//     lc_escalate_rsp_channel(sc_module_name name) : sc_module(name) {}
//
//     void acknowledge() override {
//         // Acknowledge escalation signal (no action needed in TLM model)
//     }
// };

/**
 * @brief Life Cycle RMA Response Channel implementation
 * 
 * Acknowledges Life Cycle RMA signals.
 * NOTE: Removed - lc_rma_rsp is now sc_out<bool>, no channel needed
 */
// class otbn_ip::lc_rma_rsp_channel : public lc_ctrl_rsp_if, public sc_module {
// public:
//     SC_HAS_PROCESS(lc_rma_rsp_channel);
//     lc_rma_rsp_channel(sc_module_name name) : sc_module(name) {}
//
//     void acknowledge() override {
//         // Acknowledge RMA signal (no action needed in TLM model)
//     }
// };

// Constructor implementation
otbn_ip::otbn_ip(sc_module_name n, unsigned int memory_size,
           unsigned int urnd_seed, int log_verbosity)
   : otbn_base(n, memory_size),
     intr_done("intr_done"),
     alert_fatal("alert_fatal"),
     alert_recov("alert_recov"),
     clk_core("clk_core"),
     rst_n("rst_n"),
     keymgr_tl_socket("keymgr_tl_socket"),
     otp_key_req("otp_key_req"),
     otp_key_rsp("otp_key_rsp"),
     lc_escalate_req("lc_escalate_req"),
     lc_escalate_rsp("lc_escalate_rsp"),
     lc_rma_req("lc_rma_req"),
     lc_rma_rsp("lc_rma_rsp"),
     verbosity("verbosity", log_verbosity),
     algorithm_type("algorithm_type", "rsa_2048"),
     current_state(OTBN_STATE_BUSY_SEC_WIPE_INT),  // Hardware starts in busy state (internal wipe)
     insn_count_value(0),
     err_bits_accumulator(0),
     operation_done(false),
     key_registered(false),
     load_checksum_crc(0xFFFFFFFF),  // CRC-32-IEEE internal initial value (all ones)
     urnd_prng_seed(urnd_seed),  // Store URND seed parameter
     pending_intr_done(false),
     pending_alert_fatal(false),
     pending_alert_recov(false)
{
   // Initialize logger
   logger.setMaxVerbosity(verbosity.get_param_value());
   logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
   logger.setFunctionTrace(false);
   REG_FUNC_TRACE(logger);

   // Initialize URND PRNG with configurable seed (per otbn_plan.md specification)
   std::srand(urnd_prng_seed);
   REG_INFO(1, logger) << "[OTBN] URND PRNG initialized with seed: 0x" << std::hex << urnd_prng_seed << std::dec;

   // Initialize WDR (Wide Data Registers) to zero
   // Per otbn_plan.md line 11: "The model implements 256-bit WDR"
   for (int i = 0; i < NUM_WDR_REGISTERS; i++) {
       for (int j = 0; j < 4; j++) {
           wdr_registers[i][j] = 0;
       }
   }
   // Create interface channel implementations for sc_exports
   otp_key_rsp_impl = new otp_key_rsp_channel("otp_key_rsp_impl");

   // Bind sc_exports to their implementations
   otp_key_rsp.bind(*otp_key_rsp_impl);

   // Register Key Manager TLM target socket callback (per otbn_plan.md line 21)
   keymgr_tl_socket.register_b_transport(this, &otbn_ip::keymgr_b_transport);

   // NOTE: sc_ports (req interfaces) will be bound in testbench
   // NOTE: sc_out ports will be bound in testbench

   // Create algorithm based on CCI-overridden algorithm_type
   select_algorithm(algorithm_type.get_param_value());

   // Configure algorithm debug/info message streams (per spec Section 9.3)
   if (current_algorithm) {
       // Configure algorithm debug/info streams — covers message_objects() for each algo type
       current_algorithm->message_objects(std::cout, std::cout);

       // Register CSR/WDR callbacks (per otbn_plan.md algorithm interface)
       current_algorithm->register_csr_read_cb(
           [this](uint32_t addr, uint32_t* data) -> otbn_algorithm::status_t {
               return this->csr_read_handler(addr, data);
           }
       );
       current_algorithm->register_csr_write_cb(
           [this](uint32_t addr, uint32_t data) -> otbn_algorithm::status_t {
               return this->csr_write_handler(addr, data);
           }
       );
       current_algorithm->register_wdr_read_cb(
           [this](uint32_t addr, uint64_t* data) -> otbn_algorithm::status_t {
               return this->wdr_read_handler(addr, data);
           }
       );
       current_algorithm->register_wdr_write_cb(
           [this](uint32_t addr, uint64_t* data) -> otbn_algorithm::status_t {
               return this->wdr_write_handler(addr, data);
           }
       );
        current_algorithm->register_rnd_read_cb(
            [this](uint32_t* data) -> otbn_algorithm::status_t {
                return this->rnd_read_handler(data);
            }
        );
        current_algorithm->register_key_status_cb(
            [this]() -> bool {
                return this->key_registered;
            }
        );
        current_algorithm->register_err_bits_write_cb(
            [this](uint32_t err_bits) {
                this->err_bits_accumulator |= err_bits;
            }
        );
       REG_INFO(1, logger) << "[OTBN] Registered CSR/WDR callbacks for algorithm";
   }

   // ============================================================================
   // Register Callbacks
   // ============================================================================
   // NOTE: Only register callbacks for registers that need side-effects beyond
   // simple read/write. regmodel automatically handles storage for all registers.

   // CMD register - Write-only, execute commands
   memory.register_write_callback(
       [this](uint32_t value) -> bool { return this->cmd_write_callback(value); },
       CMD.offset
   );

   // CTRL register - Writable only when IDLE (conditional access)
   memory.register_write_callback(
       [this](uint32_t value) -> bool {
           // Only allow writes when IDLE (per spec Section 6.2)
           if (current_state != OTBN_STATE_IDLE) {
               return false;  // Silently ignore write
           }
           CTRL = value;  // Store value
           return true;
       },
       CTRL.offset
   );

   // STATUS register - Read-only, returns current state
   memory.register_read_callback(
       [this](uint32_t& value) -> bool { return this->status_read_callback(value); },
       STATUS.offset
   );

   // INSN_CNT register - Read-only, returns instruction count
   memory.register_read_callback(
       [this](uint32_t& value) -> bool { return this->insn_cnt_read_callback(value); },
       INSN_CNT.offset
   );

   // INSN_CNT register - Write callback for clearing (per spec Section 6.2)
   memory.register_write_callback(
       [this](uint32_t value) -> bool {
           // Can only clear when IDLE or LOCKED (per spec Section 6.2)
           if (current_state == OTBN_STATE_IDLE || current_state == OTBN_STATE_LOCKED) {
               insn_count_value = 0;  // Clear to zero
               return true;
           }
           return false;  // Ignore write during BUSY states
       },
       INSN_CNT.offset
   );

   // ERR_BITS register - Read-only (returns accumulator), Write for W1C in IDLE
   memory.register_read_callback(
       [this](uint32_t& value) -> bool { return this->err_bits_read_callback(value); },
       ERR_BITS.offset
   );
   memory.register_write_callback(
       [this](uint32_t value) -> bool { return this->err_bits_write_callback(value); },
       ERR_BITS.offset
   );

   // FATAL_ALERT_CAUSE register - Read-only, returns persisted fatal error cause
   // (per spec Section 6.2: "Returns persisted fatal error cause from internal state")
   memory.register_read_callback(
       [this](uint32_t& value) -> bool { return this->fatal_alert_cause_read_callback(value); },
       FATAL_ALERT_CAUSE.offset
   );

   // INTR_STATE register - W1C with interrupt output update
   memory.register_write_callback(
       [this](uint32_t value) -> bool { return this->intr_state_write_callback(value); },
       INTR_STATE.offset
   );

   // INTR_ENABLE - Needs callback to update interrupt output when enabled
   memory.register_write_callback(
       [this](uint32_t value) -> bool {
           // Update register storage manually since we override default callback
           INTR_ENABLE = value;
           // Update interrupt output based on new enable value (deferred)
           uint32_t intr_state_val = INTR_STATE;
           pending_intr_done = (intr_state_val & value & 0x1) != 0;
           request_interrupt_update();
           return true;
       },
       INTR_ENABLE.offset
   );

   // INTR_TEST register - Write-only, sets INTR_STATE bits
   memory.register_write_callback(
       [this](uint32_t value) -> bool { return this->intr_test_write_callback(value); },
       INTR_TEST.offset
   );

   // ALERT_TEST register - Write-only, pulses alerts
   memory.register_write_callback(
       [this](uint32_t value) -> bool { return this->alert_test_write_callback(value); },
       ALERT_TEST.offset
   );

   // LOAD_CHECKSUM register - Read callback returns CRC-32 checksum
   memory.register_read_callback(
       [this](uint32_t& value) -> bool { return this->load_checksum_read_callback(value); },
       LOAD_CHECKSUM.offset
   );

   // LOAD_CHECKSUM register - Write callback to clear/set checksum accumulator
   // (per spec Section 6.2: "Host typically writes 0x00000000 to initialize before loading memory")
   memory.register_write_callback(
       [this](uint32_t value) -> bool {
           // Any write to the checksum register resets internal CRC to initial value
           load_checksum_crc = 0xFFFFFFFF;
           // Update the register to store the value written (register contains inverted internal CRC)
           LOAD_CHECKSUM = 0;
           return true;
       },
       LOAD_CHECKSUM.offset
   );

   // IMEM callbacks - Need state checking for access
   // Cache offsets to avoid triggering recursive operator[] during callback registration
   // IMEM base offset is 0x4000 bytes = 4096 words
   const unsigned int IMEM_BASE_WORD_OFFSET = (0x4000 + 0x00) / sizeof(unsigned int);
   for (int i = 0; i < 2048; i++) {
       unsigned int word_offset = IMEM_BASE_WORD_OFFSET + i;
       memory.register_read_callback(
           [this, i](uint32_t& val) -> bool { return this->imem_read_callback(val, i); },
           word_offset
       );
       memory.register_write_callback(
           [this, i](uint32_t val) -> bool { return this->imem_write_callback(val, i); },
           word_offset
       );
   }

   // DMEM callbacks - Need state checking for access
   // Cache offsets to avoid triggering recursive operator[] during callback registration
   // DMEM base offset is 0x8000 bytes = 8192 words
   // Register callbacks for all 1024 words (full 4 KiB) so the protected region
   // (indices 768-1023) is handled by dmem_write_callback / dmem_read_callback
   // rather than falling back to the default regmodel::Reg raw-access callbacks.
   const unsigned int DMEM_BASE_WORD_OFFSET = (0x8000 + 0x00) / sizeof(unsigned int);
   for (int i = 0; i < 1024; i++) {
       unsigned int word_offset = DMEM_BASE_WORD_OFFSET + i;
       memory.register_read_callback(
           [this, i](uint32_t& val) -> bool { return this->dmem_read_callback(val, i); },
           word_offset
       );
       memory.register_write_callback(
           [this, i](uint32_t val) -> bool { return this->dmem_write_callback(val, i); },
           word_offset
       );
   }

   // TLM Note: Constructor initializes directly to IDLE state
   // (internal secure wipe assumed to complete atomically at power-on)
   // The reset_handler will handle explicit reset pulses during simulation

   // Initialize LOAD_CHECKSUM register to match internal CRC value
   // Internal CRC is 0xFFFFFFFF, register should contain inverted value (0x00000000)
   LOAD_CHECKSUM = ~load_checksum_crc;

   // Register SC_METHOD for reset handler (sensitive to rst_n changes)
   SC_METHOD(reset_handler);
   sensitive << rst_n;
   dont_initialize();

   // Register SC_METHOD to monitor lc_escalate_req / lc_rma_req and trigger event
   SC_METHOD(lc_escalate_monitor_method);
   sensitive << lc_escalate_req;
   sensitive << lc_rma_req;
   dont_initialize();

   // Register SC_THREAD for Life Cycle Controller monitoring
   SC_THREAD(lc_monitor_thread);
   sensitive << lc_escalate_event;


   // Register SC_METHODs for deferred output updates (to avoid multiple driver conflicts)
   SC_METHOD(interrupt_update_method);
   sensitive << interrupt_update_event;
   dont_initialize();

   SC_METHOD(alert_update_method);
   sensitive << alert_update_event;
   dont_initialize();
}

/**
 * @brief Destructor
 * 
 * Cleans up all allocated resources:
 * - Deletes current algorithm instance
 * - Deletes all interface channel implementations
 */
otbn_ip::~otbn_ip() {
   REG_FUNC_TRACE(logger);
   if (current_algorithm) {
       delete current_algorithm;
   }

   // Clean up interface channel implementations
   if (otp_key_rsp_impl) delete otp_key_rsp_impl;
}

// ============================================================================
// Register Write Callbacks
// ============================================================================

/**
 * @brief Handle write to CMD register
 * @param value Write value (command byte)
 * @return true if write was handled, false if command was invalid
 * 
 * Processes command writes to the CMD register. Valid commands:
 * - 0xd8: EXECUTE - Start algorithm execution
 * - 0xc3: SEC_WIPE_DMEM - Secure wipe data memory
 * - 0x1e: SEC_WIPE_IMEM - Secure wipe instruction memory
 * 
 * Commands are only accepted when STATUS == IDLE. Writes during BUSY
 * or LOCKED states are silently ignored (no error).
 */
bool otbn_ip::cmd_write_callback(uint32_t value) {
   REG_FUNC_TRACE(logger);
   
   // CMD can only be written when IDLE 
   // LOCKED state is terminal - commands must be SILENTLY IGNORED (no error)
   // Writes during non-IDLE states must be SILENTLY IGNORED (no error)
   if (current_state == OTBN_STATE_LOCKED) {
    REG_INFO(1, logger) << "[OTBN] CMD write ignored in LOCKED state";
       return true;  // Silent ignore in LOCKED (terminal state), no state change
   }
   if (current_state != OTBN_STATE_IDLE) {
       return true;  // Silent ignore in BUSY states, no error bit set
   }

   switch (value & 0xFF) {
       case 0xd8: // EXECUTE
           current_state = OTBN_STATE_BUSY_EXECUTE;
           REG_DEBUG(2, logger) << "Current State: BUSY_EXECUTE";
           sc_spawn(sc_bind(&otbn_ip::execute_algorithm, this));
           break;
       case 0xc3: // SEC_WIPE_DMEM
           current_state = OTBN_STATE_BUSY_SEC_WIPE_DMEM;
           REG_DEBUG(2, logger) << "Current State: BUSY_SEC_WIPE_DMEM";
           sc_spawn(sc_bind(&otbn_ip::secure_wipe_dmem, this));
           break;
       case 0x1e: // SEC_WIPE_IMEM
           current_state = OTBN_STATE_BUSY_SEC_WIPE_IMEM;
           REG_DEBUG(2, logger) << "Current State: BUSY_SEC_WIPE_IMEM";
           sc_spawn(sc_bind(&otbn_ip::secure_wipe_imem, this));
           break;
       default:
           // Unrecognized command value - silently ignore (per spec Section 4.4.2)
           return false;
   }

   // Store command value in register
   CMD = value;
   return true;
}

/**
 * @brief Handle write to INTR_STATE register
 * @param value Write value (W1C mask)
 * @return true (write handled in callback)
 * 
 * Implements write-1-to-clear (W1C) behavior for interrupt bits.
 * Writing 1 to a bit clears the corresponding interrupt in INTR_STATE.
 * Updates interrupt output ports after clearing.
 */
bool otbn_ip::intr_state_write_callback(uint32_t value) {
   REG_FUNC_TRACE(logger);
   // W1C: Write-1-to-clear
   uint32_t current_val = INTR_STATE;
   current_val &= ~value;
   INTR_STATE = current_val;

   // Update interrupt output (deferred)
   uint32_t enable_val = INTR_ENABLE;
   pending_intr_done = (current_val & enable_val & 0x1) != 0;
   request_interrupt_update();
   return true;
}

/**
 * @brief Handle write to INTR_TEST register
 * @param value Write value (test mask)
 * @return true (write handled in callback)
 * 
 * Forces interrupt assertion in INTR_STATE for testing purposes.
 * Writing 1 to a bit forces the corresponding interrupt to be set.
 * Updates interrupt output ports after setting.
 */
bool otbn_ip::intr_test_write_callback(uint32_t value) {
   REG_FUNC_TRACE(logger);
   // Writing 1 sets INTR_STATE bit
   uint32_t current_val = INTR_STATE;
   current_val |= (value & 0x1);
   INTR_STATE = current_val;

   // Update interrupt output (deferred)
   uint32_t enable_val = INTR_ENABLE;
   pending_intr_done = (current_val & enable_val & 0x1) != 0;
   request_interrupt_update();
   return true;
}

/**
 * @brief Handle write to ALERT_TEST register
 * @param value Write value (alert test mask)
 * @return true (write handled in callback)
 * 
 * Triggers alert outputs for testing purposes:
 * - Bit 0: fatal - Asserts fatal alert (stays asserted until reset)
 * - Bit 1: recov - Asserts recoverable alert (auto-clears after brief pulse)
 */
bool otbn_ip::alert_test_write_callback(uint32_t value) {
   REG_FUNC_TRACE(logger);
   // Assert alerts based on bits written (deferred)
   if (value & 0x1) {
       pending_alert_fatal = true;
       request_alert_update();
       // Fatal alert stays asserted until reset
   }
   if (value & 0x2) {
       pending_alert_recov = true;
       request_alert_update();
       // Auto-clear recoverable alert after brief pulse
       sc_spawn([this]() {
           wait(1, SC_NS); 
           pending_alert_recov = false;
           request_alert_update();
       });
   }
   return true;
}

/**
 * @brief Handle write to ERR_BITS register
 * @param value Write value (W1C mask)
 * @return true if write was handled, false if state doesn't allow clearing
 * 
 * Implements write-1-to-clear (W1C) behavior for error bits.
 * Can only clear error bits when STATUS is IDLE or LOCKED.
 * LOCKED state allows clearing to diagnose error before reset.
 */
bool otbn_ip::err_bits_write_callback(uint32_t value) {
   REG_FUNC_TRACE(logger);
   // W1C: Can clear in IDLE or LOCKED state 
   // LOCKED state allows clearing to diagnose error before reset
   if (current_state == OTBN_STATE_IDLE || current_state == OTBN_STATE_LOCKED) {
       err_bits_accumulator &= ~value;
       return true;
   }
   return false;
}

/**
 * @name Register Read Callbacks
 * @{
 */

/**
 * @brief Handle read from STATUS register
 * @param value Reference to return value
 * @return true (callback handled the read)
 * 
 * Returns the current OTBN state machine state:
 * - 0x00: IDLE
 * - 0x01: BUSY_EXECUTE
 * - 0x02: BUSY_SEC_WIPE_DMEM
 * - 0x03: BUSY_SEC_WIPE_IMEM
 * - 0x04: BUSY_SEC_WIPE_INT
 * - 0xFF: LOCKED
 */
bool otbn_ip::status_read_callback(uint32_t& value) {
   REG_FUNC_TRACE(logger);
   value = static_cast<uint32_t>(current_state);
   return true;
}

/**
 * @brief Handle read from INSN_CNT register
 * @param value Reference to return value
 * @return true (callback handled the read)
 * 
 * Returns the instruction count from the last algorithm execution.
 * Updated after algorithm completes. Can be cleared when IDLE or LOCKED.
 */
bool otbn_ip::insn_cnt_read_callback(uint32_t& value) {
   REG_FUNC_TRACE(logger);
   value = insn_count_value;
   return true;
}

/**
 * @brief Handle read from ERR_BITS register
 * @param value Reference to return value
 * @return true (callback handled the read)
 * 
 * Returns the accumulated error bits. Error bits are sticky and
 * can only be cleared when STATUS is IDLE or LOCKED.
 */
bool otbn_ip::err_bits_read_callback(uint32_t& value) {
   REG_FUNC_TRACE(logger);
   value = err_bits_accumulator;
   return true;
}

/**
 * @brief Handle read from FATAL_ALERT_CAUSE register
 * @param value Reference to return value
 * @return true (callback handled the read)
 * 
 * Returns the persisted fatal error cause that triggered the fatal alert
 * and LOCKED state. Value persists until reset.
 */
bool otbn_ip::fatal_alert_cause_read_callback(uint32_t& value) {
   REG_FUNC_TRACE(logger);
   // Return persisted fatal error cause value
   // (per spec Section 6.2: fatal error cause persists until reset)
   value = FATAL_ALERT_CAUSE;
   return true;
}

/**
 * @brief Handle read from LOAD_CHECKSUM register
 * @param value Reference to return value
 * @return true (callback handled the read)
 * 
 * Returns the CRC-32-IEEE checksum of IMEM and DMEM writes.
 * Checksum is updated on each memory write. Returns inverted CRC
 * for compatibility with standard CRC tools.
 */
bool otbn_ip::load_checksum_read_callback(uint32_t& value) {
   REG_FUNC_TRACE(logger);
   // CRC-32-IEEE checksum of memory writes
   // Model handles inversion internally - register stores inverted CRC, but we return internal CRC directly
   // Apply final XOR with 0xFFFFFFFF to match Python's binascii.crc32 (which applies final XOR internally)
   value = load_checksum_crc ^ 0xFFFFFFFF;
   return true;
}
/** @} */

/**
 * @name Memory Access Callbacks
 * @{
 */

/**
 * @brief Handle write to IMEM register
 * @param value Write value (32-bit instruction word)
 * @param index IMEM word index (0-2047)
 * @return true if write was allowed, false if blocked
 * 
 * IMEM can only be written when STATUS is IDLE or BUSY_SEC_WIPE_INT (boot-time).
 * Writes during BUSY_EXECUTE set ILLEGAL_BUS_ACCESS error and trigger fatal alert.
 * Writes update the LOAD_CHECKSUM CRC-32-IEEE accumulator.
 */
bool otbn_ip::imem_write_callback(uint32_t value, uint32_t index) {
   REG_FUNC_TRACE(logger);
   if (index >= 2048u) {
       return false;
   }
   // IMEM can ONLY be written in IDLE or BUSY_SEC_WIPE_INT (boot-time)
   // (per spec Section 4.5, 5.2)
   if (current_state != OTBN_STATE_IDLE &&
       current_state != OTBN_STATE_BUSY_SEC_WIPE_INT) {
       err_bits_accumulator |= otbn_err_bits::ILLEGAL_BUS_ACCESS;
        REG_ERROR(0, logger) << "ILLEGAL_BUS_ACCESS during BUSY_EXECUTE is FATAL";
       // ILLEGAL_BUS_ACCESS during BUSY_EXECUTE is FATAL
       if (current_state == OTBN_STATE_BUSY_EXECUTE) {
           current_state = OTBN_STATE_LOCKED;
           REG_ERROR(0, logger) << "Setting current state to LOCKED";
           internal_secure_wipe();
           secure_wipe_dmem();
           secure_wipe_imem();
           load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
           FATAL_ALERT_CAUSE = otbn_fatal_cause::ILLEGAL_BUS_ACCESS;
           pending_alert_fatal = true;
           request_alert_update();
       }
       // Otherwise silent block (returns false, write ignored)
       return false;
   }
   // Allow write and store in register
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   // IMEM base offset is 0x4000 bytes = 4096 words, index is 0-2047
   unsigned int word_offset = (0x4000 / sizeof(unsigned int)) + index;
   memory.memory_block[word_offset] = value;

   // Update LOAD_CHECKSUM (CRC-32-IEEE) on IMEM writes
   // Update internal CRC variable, then update register to reflect the change
   
   load_checksum_crc = crc32_ieee_update(load_checksum_crc, value, true, index);
   // Update register with inverted value of internal CRC
   LOAD_CHECKSUM = ~load_checksum_crc;

   return true;
}

/**
 * @brief Handle read from IMEM register
 * @param value Reference to return value
 * @param index IMEM word index (0-2047)
 * @return true (callback handled the read)
 * 
 * IMEM can only be read when STATUS is IDLE or BUSY_SEC_WIPE_INT (boot-time).
 * Reads during BUSY states return 0 and set ILLEGAL_BUS_ACCESS error.
 * Reads during BUSY_EXECUTE trigger fatal alert and LOCKED state.
 * LOCKED state returns 0 for security (no error).
 */
bool otbn_ip::imem_read_callback(uint32_t& value, uint32_t index) {
   REG_FUNC_TRACE(logger);
   if (index >= 2048u) {
       value = 0;
       return true;
   }
   // Per spec Section 4.5: "reads return zero" when not IDLE
   // LOCKED state: Return 0 for security (no error, silent read)
   if (current_state == OTBN_STATE_LOCKED) {
       value = 0;
       return true;  // Silent return 0, no error
   }

   // IMEM can ONLY be read in IDLE or BUSY_SEC_WIPE_INT (boot-time)
   // (per spec Section 4.5, 5.2)
   if (current_state != OTBN_STATE_IDLE &&
       current_state != OTBN_STATE_BUSY_SEC_WIPE_INT) {
       value = 0;  // Return 0 on blocked read
       err_bits_accumulator |= otbn_err_bits::ILLEGAL_BUS_ACCESS;

       // ILLEGAL_BUS_ACCESS during BUSY_EXECUTE is FATAL
       if (current_state == OTBN_STATE_BUSY_EXECUTE) {
           current_state = OTBN_STATE_LOCKED;
           internal_secure_wipe();
           secure_wipe_dmem();
           secure_wipe_imem();
           load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
           FATAL_ALERT_CAUSE = otbn_fatal_cause::ILLEGAL_BUS_ACCESS;
           pending_alert_fatal = true;
           request_alert_update();
       }
       // Otherwise silent block (returns 0). Return true so regmodel uses the
       // callback value (0) instead of falling back to underlying memory.
       return true;
   }
   // Read stored value (allowed in IDLE and SEC_WIPE_INT)
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   // IMEM base offset is 0x4000 bytes = 4096 words, index is 0-2047
   unsigned int word_offset = (0x4000 / sizeof(unsigned int)) + index;
   value = memory.memory_block[word_offset];
   return true;
}

/**
 * @brief Handle write to DMEM register
 * @param value Write value (32-bit data word)
 * @param index DMEM word index (0-767 for host-accessible region)
 * @return true if write was allowed, false if blocked
 * 
 * DMEM can only be written when STATUS is IDLE or BUSY_SEC_WIPE_INT (boot-time).
 * Only first 3KB (768 words) are host-accessible via register interface.
 * Writes to protected region (index >= 768) are silently ignored.
 * Writes during BUSY_EXECUTE set ILLEGAL_BUS_ACCESS error and trigger fatal alert.
 * Writes update the LOAD_CHECKSUM CRC-32-IEEE accumulator.
 */
bool otbn_ip::dmem_write_callback(uint32_t value, uint32_t index) {
   REG_FUNC_TRACE(logger);
   // DMEM can ONLY be written in IDLE or BUSY_SEC_WIPE_INT (boot-time)
   // (per spec Section 4.5, 5.2)
   if (current_state != OTBN_STATE_IDLE &&
       current_state != OTBN_STATE_BUSY_SEC_WIPE_INT) {
       err_bits_accumulator |= otbn_err_bits::ILLEGAL_BUS_ACCESS;

       // ILLEGAL_BUS_ACCESS during BUSY_EXECUTE is FATAL
       if (current_state == OTBN_STATE_BUSY_EXECUTE) {
           current_state = OTBN_STATE_LOCKED;
           internal_secure_wipe();
           secure_wipe_dmem();
           secure_wipe_imem();
           load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
           FATAL_ALERT_CAUSE = otbn_fatal_cause::ILLEGAL_BUS_ACCESS;

           pending_alert_fatal = true;
           request_alert_update();
       }
       // Otherwise silent block (returns false, write ignored)
       return false;
   }

   // Protected DMEM region check (per spec Section 3.2.1)
   // Host can only access first 3 KiB (768 words) through register interface
   // Last 1 KiB reserved for algorithm-only sensitive data.
   // Return false so regmodel does not store the value in its backing memory.
   if (index >= 768) {
       return false;  // Silently block write to protected region, no error
   }

   // Allow write and store in register
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   // DMEM base offset is 0x8000 bytes = 8192 words, index is 0-767
   if (index < 768) {
       unsigned int word_offset = (0x8000 / sizeof(unsigned int)) + index;
       memory.memory_block[word_offset] = value;
   }

   // Update LOAD_CHECKSUM (CRC-32-IEEE) on DMEM writes
   // (per spec Section 6.2: DMEM writes update checksum with {imem=0, idx, wdata})
   // Update internal CRC variable, then update register to reflect the change
   load_checksum_crc = crc32_ieee_update(load_checksum_crc, value, false, index);
   // Update register with inverted value of internal CRC
   LOAD_CHECKSUM = ~load_checksum_crc;

   return true;
}

/**
 * @brief Handle read from DMEM register
 * @param value Reference to return value
 * @param index DMEM word index (0-767 for host-accessible region)
 * @return true (callback handled the read)
 * 
 * DMEM can only be read when STATUS is IDLE or BUSY_SEC_WIPE_INT (boot-time).
 * Only first 3KB (768 words) are host-accessible via register interface.
 * Reads from protected region (index >= 768) return 0.
 * Reads during BUSY states return 0 and set ILLEGAL_BUS_ACCESS error.
 * Reads during BUSY_EXECUTE trigger fatal alert and LOCKED state.
 * LOCKED state returns 0 for security (no error).
 */
bool otbn_ip::dmem_read_callback(uint32_t& value, uint32_t index) {
   REG_FUNC_TRACE(logger);
   // Per spec Section 4.5: "reads return zero" when not IDLE
   // LOCKED state: Return 0 for security (no error, silent read)
   if (current_state == OTBN_STATE_LOCKED) {
       value = 0;
       return true;  // Silent return 0, no error
   }

   // DMEM can ONLY be read in IDLE or BUSY_SEC_WIPE_INT (boot-time)
   // (per spec Section 4.5, 5.2)
   if (current_state != OTBN_STATE_IDLE &&
       current_state != OTBN_STATE_BUSY_SEC_WIPE_INT) {
       value = 0;  // Return 0 on blocked read
       err_bits_accumulator |= otbn_err_bits::ILLEGAL_BUS_ACCESS;

       // ILLEGAL_BUS_ACCESS during BUSY_EXECUTE is FATAL
       if (current_state == OTBN_STATE_BUSY_EXECUTE) {
           current_state = OTBN_STATE_LOCKED;
           internal_secure_wipe();
           secure_wipe_dmem();
           secure_wipe_imem();
           load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
           FATAL_ALERT_CAUSE = otbn_fatal_cause::ILLEGAL_BUS_ACCESS;
           pending_alert_fatal = true;
           request_alert_update();
       }
       // Return true to ensure callback's value (0) is used by regmodel framework
       // Returning false would cause regmodel to read from actual memory instead
       return true;
   }

   // Protected DMEM region check 
   // Host can only access first 3 KiB (768 words) through register interface
   // Last 1 KiB reserved for algorithm-only sensitive data
   if (index >= 768) {
       value = 0;  // Return 0 for protected region
       return true;  // Silent block, no error (per spec)
   }

   // Read stored value
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   // DMEM base offset is 0x8000 bytes = 8192 words, index is 0-767
   unsigned int word_offset = (0x8000 / sizeof(unsigned int)) + index;
   value = memory.memory_block[word_offset];
   return true;
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Select and instantiate algorithm based on name
 * @param algo_name Algorithm name ("rsa_2048", "summation", "otbn_loop", "rnd_test", etc.)
 * 
 * Creates a new algorithm instance based on the provided name.
 * Supported algorithms:
 * - "rsa_2048": RSA-2048 modular exponentiation
 * - "rsa-2048-key-enabled": RSA-2048 with key validation
 * - "summation": Simple summation reference algorithm
 * - "rnd_test": RND register blocking test algorithm
 * 
 * Defaults to RSA-2048 if algorithm name is unknown.
 */
void otbn_ip::select_algorithm(const std::string& algo_name) {
   REG_FUNC_TRACE(logger);
   // DMEM size is 4 KiB (1024 words * 4 bytes = 4096 bytes)
   const size_t dmem_size = 4096;

   // Select algorithm based on name parameter
   if (algo_name == "rsa_2048" || algo_name == "RSA-2048" || algo_name == "RSA_2048") {
       current_algorithm = new otbn_algorithm_rsa_2048(dmem_size);
   }
   else if (algo_name == "rsa-2048-key-enabled" || algo_name == "RSA-2048-KEY-ENABLED" || algo_name == "RSA_2048_KEY_ENABLED" || algo_name == "rsa_2048_key_enabled") {
       current_algorithm = new otbn_algorithm_rsa_2048_key_enabled(dmem_size);

   }
   else if (algo_name == "summation" || algo_name == "SUMMATION" || algo_name == "sum") {
       current_algorithm = new otbn_algorithm_summation(dmem_size);

   }
   else if (algo_name == "otbn_loop" || algo_name == "OTBN_LOOP" || algo_name == "loops") {
       // Simple nested loop / loopi test algorithm, mirroring the OpenTitan loops.s test.
       current_algorithm = new otbn_algorithm_otbn_loop(dmem_size);

   }
   else if (algo_name == "rnd_test") {
       current_algorithm = new otbn_algorithm_rnd_test(dmem_size);

   }
   else if (algo_name == "p256_ecdsa" || algo_name == "P256_ECDSA" || algo_name == "ecdsa_p256") {
       // P256 ECDSA signature verification algorithm
       current_algorithm = new otbn_algorithm_p256_ecdsa(dmem_size);

   }
   else if (algo_name == "smoke" || algo_name == "SMOKE" || algo_name == "sep_integration") {
       // Smoke/integration program: reads outer_inc/inner_count/inner_inc from DMEM[0..2],
       // writes result to DMEM[3], INSN_CNT=39.
       // Used by otbn_smoke_test, otbn_sep_integration_test, otbn_fw_control_test.
       current_algorithm = new otbn_algorithm_smoke(dmem_size);

   }
   else if (algo_name == "rsa_3072" || algo_name == "RSA-3072" || algo_name == "RSA_3072") {
       current_algorithm = new otbn_algorithm_rsa_3072(dmem_size);

   }
   else if (algo_name == "callback_cov" || algo_name == "CALLBACK_COV") {
       current_algorithm = new otbn_algorithm_callback_cov(dmem_size);

   }
   // Add more algorithms here as they are implemented:
   // else if (algo_name == "x25519") {
   //     current_algorithm = new otbn_algorithm_x25519(dmem_size);
   // }
   else {
       // Default to RSA-2048 if unknown
       current_algorithm = new otbn_algorithm_rsa_2048(dmem_size);

   }
}

/**
 * @brief Handle RND data read request from algorithm
 * @param data Pointer to output buffer (8 words = 256 bits)
 * @return SUCCESS always
 *
 * Fills the buffer with 256 bits (8 × 32-bit words) of random data using
 * C rand(). The RTL sources this from the EDN subsystem; the VP does not
 * model EDN and uses rand() instead.
 */
otbn_algorithm::status_t otbn_ip::rnd_read_handler(uint32_t* data) {
    REG_FUNC_TRACE(logger);
    // Generate 256 bits (8 words) of random data using C rand().
    // Entropy is provided by OpenSSL/platform rather than the EDN subsystem,
    // which is not modelled in the VP.
    for (int i = 0; i < 8; ++i) {
        data[i] = static_cast<uint32_t>(rand());
    }
    return otbn_algorithm::SUCCESS;
}



/**
 * @brief Execute the current algorithm
 * 
 * SystemC thread that executes the selected algorithm. This function:
 * - Resets algorithm state before execution
 * - Copies DMEM data to algorithm buffer (full 4KB access)
 * - Calls algorithm execute() method
 * - Writes results back to DMEM (host-visible region only)
 * - Updates INSN_CNT register with instruction count
 * - Performs internal secure wipe (required per spec)
 * - Handles algorithm errors and state transitions
 * - Sets completion interrupt if successful
 */
void otbn_ip::execute_algorithm() {
   REG_FUNC_TRACE(logger);
   // TLM: Add wait to allow tests to observe BUSY_EXECUTE state before completion
   uint64_t cycle_count = current_algorithm->get_cycle_count();
   REG_DEBUG(2, logger) << "[OTBN] execute_algorithm: waiting for cycle count: " << cycle_count << " ns";
   wait(cycle_count, SC_NS);


   // Reset algorithm state before execution (per spec Section 9.4: Algorithm Lifecycle)
   if (current_algorithm) {
       current_algorithm->reset();
   }

   // Get DMEM pointer (full 4 KiB access for algorithm)
   uint8_t dmem_buffer[4096];

   // Read all 1024 words (4 KiB) from DMEM
   // Note: otbn_base has 768 words accessible via register interface,
   // but we need to access full DMEM for algorithm
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   const unsigned int DMEM_BASE_WORD_OFFSET = (0x8000 / sizeof(unsigned int));
   for (int i = 0; i < 768; i++) {  // Host-visible region
       uint32_t word = memory.memory_block[DMEM_BASE_WORD_OFFSET + i];
       dmem_buffer[i*4 + 0] = (word >> 0) & 0xFF;
       dmem_buffer[i*4 + 1] = (word >> 8) & 0xFF;
       dmem_buffer[i*4 + 2] = (word >> 16) & 0xFF;
       dmem_buffer[i*4 + 3] = (word >> 24) & 0xFF;
   }
   // Protected region (last 1 KiB) initialized to zero for algorithm use
   for (int i = 3072; i < 4096; i++) {
       dmem_buffer[i] = 0;
   }

   // Execute algorithm and capture status
   // Cast to char* per otbn_plan.md specification
   otbn_algorithm::status_t algo_status = current_algorithm->execute(reinterpret_cast<char*>(dmem_buffer));

   // Write back DMEM (host-visible region only)
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   for (int i = 0; i < 768; i++) {
       uint32_t word = (dmem_buffer[i*4 + 3] << 24) |
                      (dmem_buffer[i*4 + 2] << 16) |
                      (dmem_buffer[i*4 + 1] << 8) |
                      (dmem_buffer[i*4 + 0]);
       memory.memory_block[DMEM_BASE_WORD_OFFSET + i] = word;
   }

   // Update instruction count
   insn_count_value = current_algorithm->get_instruction_count();

   // Perform internal secure wipe (REQUIRED per spec Section 4.3.2)
   // This must happen before any state transition to prevent data leakage
   internal_secure_wipe();

   // Check algorithm status and handle errors
   if (algo_status == otbn_algorithm::ERROR) {
       // Check if software errors should be fatal
       uint32_t ctrl_val = CTRL;
       bool software_errs_fatal = (ctrl_val & 0x1) != 0;

       if (software_errs_fatal) {
           // Bug #5 fix: FATAL_SOFTWARE in ERR_BITS is only set when software_errs_fatal=1
           // Per spec section 3.7.3: "If CTRL.software_errs_fatal is set, the software error
           // also sets the fatal_software error in ERR_BITS"
           err_bits_accumulator |= otbn_err_bits::FATAL_SOFTWARE;
           // Transition to LOCKED state
           current_state = OTBN_STATE_LOCKED;
           internal_secure_wipe();
           secure_wipe_dmem();
           secure_wipe_imem();
           load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
           // Set FATAL_ALERT_CAUSE
           FATAL_ALERT_CAUSE = otbn_fatal_cause::FATAL_SOFTWARE;

           // Assert fatal alert (stays until reset) - deferred
           pending_alert_fatal = true;
           request_alert_update();
       } else {
           // Recoverable error - transition to IDLE (unless already LOCKED from bus error)
           if (current_state != OTBN_STATE_LOCKED) {
               current_state = OTBN_STATE_IDLE;

               // Pulse recoverable alert - deferred
               pending_alert_recov = true;
               request_alert_update();
               sc_spawn([this]() {
                   wait(1, SC_NS);
                   pending_alert_recov = false;
                   request_alert_update();
               });
           }
       }
   } else {
       // Success - transition to IDLE (unless already LOCKED from bus error)
       if (current_state != OTBN_STATE_LOCKED) {
           current_state = OTBN_STATE_IDLE;
       }
   }

   // Set INTR_STATE.done (unless LOCKED - terminal state doesn't signal completion)
   if (current_state != OTBN_STATE_LOCKED) {
       uint32_t intr_state_val = INTR_STATE;
       intr_state_val |= 0x1;
       INTR_STATE = intr_state_val;

       // Assert interrupt if enabled - deferred
       uint32_t enable_val = INTR_ENABLE;
       if (enable_val & 0x1) {
           pending_intr_done = true;
           request_interrupt_update();
       }
   }
   
}

/**
 * @brief Secure wipe DMEM with key rotation
 * 
 * SystemC thread that performs secure wipe of data memory:
 * 1. Transitions to IDLE state (unless LOCKED)
 * 2. Sets completion interrupt if enabled
 *
 * 
 * Per spec Section 5.4, 3.2.4: Key rotation makes old data unreadable.
 * 
 * The real design wipes using an OTP-derived scrambling key, 
 * but this model simply wipes the data by clearing it to zero.
 */
void otbn_ip::secure_wipe_dmem() {
   REG_FUNC_TRACE(logger);
   // Secure Wipe DMEM with Key Rotation (per spec Section 5.4, 3.2.4)
      REG_DEBUG(2, logger) << "[OTBN] inside secure_wipe_dmem " << std::dec<< std::endl;

   otp_key_req->request_scramble_key();

   // 4. Apply new scrambling parameters (key rotation makes old data unreadable)
   // In TLM model, we simulate this effect by overwriting DMEM with zeros
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   const unsigned int DMEM_BASE_WORD_OFFSET_WIPE = (0x8000 / sizeof(unsigned int));
   for (int i = 0; i < 768; i++) {
       memory.memory_block[DMEM_BASE_WORD_OFFSET_WIPE + i] = 0x00000000;
   }
    

   // 5. Transition to IDLE (unless LOCKED - terminal state cannot be exited except by reset)
   if (current_state != OTBN_STATE_LOCKED) {
       current_state = OTBN_STATE_IDLE;

       // Set INTR_STATE.done (only if not LOCKED)
       uint32_t intr_state_val = INTR_STATE;
       intr_state_val |= 0x1;
       INTR_STATE = intr_state_val;
      REG_DEBUG(2, logger) << "[OTBN]  intr_state_val " << std::dec << intr_state_val<< std::endl;
       // Assert interrupt if enabled - deferred
       uint32_t enable_val = INTR_ENABLE;
       if (enable_val & 0x1) {
           pending_intr_done = true;
           request_interrupt_update();
           REG_DEBUG(2, logger) << "[OTBN]  pending_intr_done set to true" << std::endl;
       }
   }
}

/**
 * @brief Secure wipe IMEM with key rotation
 * 
 * SystemC thread that performs secure wipe of instruction memory:
 * 1. Clears LOAD_CHECKSUM register and CRC accumulator
 * 2. Transitions to IDLE state (unless LOCKED)
 * 3. Sets completion interrupt if enabled
 * 
 * Per spec Section 5.4, 3.2.4: Key rotation makes old data unreadable.
 * 
 * The real design wipes using an OTP-derived scrambling key, 
 * but this model simply wipes the data by clearing it to zero.
 * 
 */
void otbn_ip::secure_wipe_imem() {
   REG_FUNC_TRACE(logger);
   // Secure Wipe IMEM with Key Rotation (per spec Section 5.4, 3.2.4)
   // Same process as DMEM wipe but for instruction memory

      REG_DEBUG(2, logger) << "[OTBN] inside secure_wipe_imem " << std::dec<< std::endl;

   otp_key_req->request_scramble_key();

   // 4. Apply new scrambling parameters (key rotation makes old data unreadable)
   // In TLM model, we simulate this effect by overwriting IMEM with zeros
   // Access underlying memory directly to avoid recursive operator[] with ASAN
   const unsigned int IMEM_BASE_WORD_OFFSET_WIPE = (0x4000 / sizeof(unsigned int));
   for (int i = 0; i < 2048; i++) {
       memory.memory_block[IMEM_BASE_WORD_OFFSET_WIPE + i] = 0x00000000;
   }

   // 4a. Clear LOAD_CHECKSUM register and CRC accumulator (per spec: IMEM wipe resets checksum)
   // Reset internal CRC to initial value, then update register
   load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
   LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value (0x00000000)

   // 5. Transition to IDLE (unless LOCKED - terminal state cannot be exited except by reset)
   if (current_state != OTBN_STATE_LOCKED) {
       current_state = OTBN_STATE_IDLE;
    
       // Set INTR_STATE.done (only if not LOCKED)
       uint32_t intr_state_val = INTR_STATE;
       intr_state_val |= 0x1;
       INTR_STATE = intr_state_val;

       REG_DEBUG(2, logger) << "[OTBN]  intr_state_val " << std::dec << intr_state_val<< std::endl;
       // Assert interrupt if enabled - deferred
       uint32_t enable_val = INTR_ENABLE;
       if (enable_val & 0x1) {
           pending_intr_done = true;
           request_interrupt_update();
              REG_DEBUG(2, logger) << "[OTBN]  pending_intr_done set to true" << std::endl;
       }
   }
}

/**
 * @brief Perform internal secure wipe sequence
 * 
 * Performs two-pass randomization of internal state (GPRs, WDRs, ACC, MOD, flags)
 * to prevent data leakage. This function:
 * 1. Requests first URND reseed from EDN
 * 2. First pass: Overwrites all WDR registers with URND entropy
 * 3. Requests second URND reseed
 * 4. Second pass: Overwrites all WDR registers again with new URND entropy
 * 
 * Per spec Section 4.3.5, 3.2.2: Required after algorithm execution to prevent
 * data leakage. In TLM model, operation completes atomically.
 */
void otbn_ip::internal_secure_wipe() {
   REG_FUNC_TRACE(logger);
   // Two-pass randomisation of WDRs using rand() (per spec Section 4.3.5, 3.2.2).
   // The RTL uses URND seeded from EDN; the VP substitutes C rand() since EDN
   // is not modelled — the effect (unpredictable wipe) is identical for VP purposes.
   for (int pass = 0; pass < 2; ++pass) {
       for (unsigned int i = 0; i < 32; i++) {
           for (unsigned int j = 0; j < 4; j++) {
               wdr_registers[i][j] = static_cast<uint64_t>(rand()) |
                                     (static_cast<uint64_t>(rand()) << 32);
           }
       }
   }

   // Bug #2 fix: key WDRs have been randomised — force re-load before next execution
   key_registered = false;
}
/** @} */

/**
 * @name CRC-32-IEEE Calculation for LOAD_CHECKSUM
 * @{
 */

/**
 * @brief Update CRC-32-IEEE checksum for memory write
 * @param crc Current CRC value
 * @param data 32-bit data word written to memory
 * @param is_imem true if IMEM write, false if DMEM write
 * @param word_index Word index within memory (0-2047 for IMEM, 0-767 for DMEM)
 * @return Updated CRC value
 * 
 * Computes CRC-32-IEEE checksum for LOAD_CHECKSUM register.
 * Input format: {imem_flag[1], word_index[15], data[32]} = 48 bits.
 * Uses polynomial 0x04C11DB7 (IEEE 802.3 standard).
 */
uint32_t otbn_ip::crc32_ieee_update(uint32_t crc, uint32_t data, bool is_imem, uint16_t word_index) {
   REG_FUNC_TRACE(logger);
   // CRC-32-IEEE polynomial: 0x04C11DB7
   // Input format: {imem_flag[1], word_index[15], data[32]} = 48 bits
   // Matches Python binascii.crc32() algorithm which processes bytes

   // Build 48-bit input value
   uint64_t input = 0;
   input |= (is_imem ? 1ULL : 0ULL) << 47;  // imem_flag bit
   input |= ((uint64_t)word_index & 0x7FFF) << 32;  // word_index (15 bits)
   input |= (uint64_t)data;  // data (32 bits)

   // Convert 48-bit value to 6 bytes in little-endian format (matching Python to_bytes(6, 'little'))
   // Python's to_bytes(6, 'little') gives bytes in order: [bits 0-7, bits 8-15, ..., bits 40-47]
   // The hex string "370100000080" shows these bytes in the order they appear: 0x37, 0x01, 0x00, 0x00, 0x00, 0x80
   uint8_t bytes[6];
   bytes[0] = (input >> 0) & 0xFF;   // bits 0-7 (LSB byte)
   bytes[1] = (input >> 8) & 0xFF;   // bits 8-15
   bytes[2] = (input >> 16) & 0xFF;  // bits 16-23
   bytes[3] = (input >> 24) & 0xFF;  // bits 24-31
   bytes[4] = (input >> 32) & 0xFF;  // bits 32-39
   bytes[5] = (input >> 40) & 0xFF;  // bits 40-47 (MSB byte)

   // Process bytes using reflected CRC-32-IEEE (matching Python binascii.crc32)
   // binascii.crc32 uses reflected polynomial 0xEDB88320 and processes bits LSB-first
   const uint32_t poly_reflected = 0xEDB88320;  // Reflected polynomial
   for (int byte_idx = 0; byte_idx < 6; byte_idx++) {
       crc ^= bytes[byte_idx];
       // Process 8 bits, LSB-first (reflected algorithm)
       for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
           if (crc & 1) {
               crc = (crc >> 1) ^ poly_reflected;
           } else {
               crc >>= 1;
           }
       }
   }

   return crc;
}
/** @} */

/**
 * @name Life Cycle Controller Monitoring Thread
 * @{
 */

/**
 * @brief Monitor lc_escalate_req and trigger lc_escalate_event
 * 
 * SystemC method that monitors lc_escalate_req signal and triggers
 * lc_escalate_event when the signal is asserted.
 */
void otbn_ip::lc_escalate_monitor_method() {
   REG_FUNC_TRACE(logger);
   if (lc_escalate_req.read() || lc_rma_req.read()) {
       // Trigger lc_escalate_event when lc_escalate_req is asserted
       lc_escalate_event.notify(SC_ZERO_TIME);
   }
}

/**
 * @brief Life Cycle Controller monitoring thread
 * 
 * SystemC thread that continuously monitors Life Cycle Controller signals:
 * - Escalation signal: Triggers immediate fatal error and LOCKED state
 * - RMA (Return Merchandise Authorization) signal: Performs full secure wipe
 *   (DMEM, IMEM, internal state) before transitioning to LOCKED state
 * 
 * Per spec Section 8.3, 4.3.6: Life Cycle signals override normal operation
 * and trigger security responses.
 */
void otbn_ip::lc_monitor_thread() {
   REG_FUNC_TRACE(logger);
   // Monitor Life Cycle Controller signals (per spec Section 8.3, 4.3.6)
   while (true) {
       // Wait for either lc_escalate_event or lc_rma_req
       wait(lc_escalate_event);

       // Check if escalation event was triggered
       if (lc_escalate_req.read()) {
            // Escalation triggers immediate fatal error and LOCKED state
            err_bits_accumulator |= otbn_err_bits::LIFECYCLE_ESCALATION;
            FATAL_ALERT_CAUSE = otbn_fatal_cause::LIFECYCLE_ESCALATION;

            // Transition to LOCKED state
            current_state = OTBN_STATE_LOCKED;
            internal_secure_wipe();
            secure_wipe_dmem();
            secure_wipe_imem();
            load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
            // Assert fatal alert (continuous until reset)
            pending_alert_fatal = true;
            request_alert_update();

            // Set done interrupt
            uint32_t intr_state_val = INTR_STATE;
            intr_state_val |= 0x1;
            INTR_STATE = intr_state_val;

            uint32_t enable_val = INTR_ENABLE;
            if (enable_val & 0x1) {
                pending_intr_done = true;
                request_interrupt_update();
            }

            // Acknowledge escalation
            // if (lc_escalate_rsp.get_interface() != nullptr) {
            lc_escalate_rsp.write(true);
        // }
        }


       // Check RMA signal
    //    if (lc_rma_req.get_interface() != nullptr) {
        if (lc_rma_req.read()) {
            // RMA requires full secure wipe before LOCKED (per spec Section 4.3.6)

            // Request OTP key rotation for DMEM (same as secure_wipe_dmem)

            otp_key_req->request_scramble_key();
        

            // Perform internal secure wipe
            internal_secure_wipe();

            // Transition to LOCKED state
            current_state = OTBN_STATE_LOCKED;

            secure_wipe_dmem();
            secure_wipe_imem();
            load_checksum_crc = 0xFFFFFFFF;  // Internal CRC initial value
           LOAD_CHECKSUM = ~load_checksum_crc;  // Update register with inverted value
            // Set done interrupt
            uint32_t intr_state_val = INTR_STATE;
            intr_state_val |= 0x1;
            INTR_STATE = intr_state_val;

            uint32_t enable_val = INTR_ENABLE;
            if (enable_val & 0x1) {
                pending_intr_done = true;
                request_interrupt_update();
            }

            // Acknowledge RMA
            lc_rma_rsp.write(true);

        }
    //    }
   }
}

// ============================================================================
// Reset Handler and Deferred Update Methods
// ============================================================================

/**
 * @brief Reset handler process
 * 
 * SystemC method process that handles reset signal changes. When reset is
 * asserted (rst_n is false), this function:
 * - Resets state machine to BUSY_SEC_WIPE_INT (post-reset state)
 * - Clears instruction count and error bits
 * - Clears pending interrupt and alert states
 * - Resets all registers to default values
 * - Resets WDR registers to zero
 * - Resets algorithm state
 * - Performs internal secure wipe
 * - Schedules transition to IDLE after wipe completes
 */
void otbn_ip::reset_handler() {
   REG_FUNC_TRACE(logger);
   // Check if reset is asserted (active-low)
   if (!rst_n.read()) {
      // Reset asserted - initialize all state immediately
      current_state = OTBN_STATE_BUSY_SEC_WIPE_INT;  // Post-reset starts in SEC_WIPE_INT
      insn_count_value = 0;
      err_bits_accumulator = 0;
      operation_done = false;
      load_checksum_crc = 0xFFFFFFFF;

      // Clear pending output states
      pending_intr_done = false;
      pending_alert_fatal = false;
      pending_alert_recov = false;

      // Request deferred updates (avoids multiple driver conflicts)
      request_interrupt_update();
      request_alert_update();

      // Reset all registers to default values
      reset_all_registers();

      // Reset WDR registers to zero
      for (int i = 0; i < NUM_WDR_REGISTERS; i++) {
          for (int j = 0; j < 4; j++) {
              wdr_registers[i][j] = 0;
          }
      }
      key_registered = false;  // Bug #1 fix: key validity must reset with WDRs

      // Reset algorithm state (per spec Section 9.4: Algorithm Lifecycle)
      if (current_algorithm) {
          current_algorithm->reset();
      }

      // Perform internal secure wipe immediately (TLM abstraction)
      // In TLM, we complete the wipe atomically rather than spanning multiple delta cycles
      internal_secure_wipe();

      // Schedule transition to IDLE after secure wipe completes
      // Use sc_spawn to defer the transition, allowing BUSY_SEC_WIPE_INT state to be observable
      sc_spawn([this]() {
          // Wait a small delay to ensure BUSY_SEC_WIPE_INT state is observable
          // This allows the test to detect the intermediate state
          wait(1, SC_NS);
          
          // After wipe completes, transition to IDLE
          current_state = OTBN_STATE_IDLE;
          
      });

      // Note: Done interrupt is NOT set during reset (only during normal operation completion)
   }
   // Reset deasserted - normal operation continues
}

/**
 * @brief Request deferred interrupt output update
 * 
 * Notifies interrupt_update_event to trigger interrupt_update_method().
 * Used to avoid multiple driver conflicts when updating interrupt outputs.
 */
void otbn_ip::request_interrupt_update() {
   REG_FUNC_TRACE(logger);
   interrupt_update_event.notify(SC_ZERO_TIME);
}

/**
 * @brief Update interrupt output ports
 * 
 * SystemC method that updates interrupt output ports based on pending state.
 * Called when interrupt_update_event is triggered.
 */
void otbn_ip::interrupt_update_method() {
   REG_FUNC_TRACE(logger);
   intr_done.write(pending_intr_done);
}

/**
 * @brief Request deferred alert output update
 * 
 * Notifies alert_update_event to trigger alert_update_method().
 * Used to avoid multiple driver conflicts when updating alert outputs.
 */
void otbn_ip::request_alert_update() {
   REG_FUNC_TRACE(logger);
   alert_update_event.notify(SC_ZERO_TIME);
}

/**
 * @brief Update alert output ports
 * 
 * SystemC method that updates alert output ports based on pending state.
 * Called when alert_update_event is triggered.
 */
void otbn_ip::alert_update_method() {
   REG_FUNC_TRACE(logger);
   alert_fatal.write(pending_alert_fatal);
   alert_recov.write(pending_alert_recov);
}

/**
 * @name CSR/WDR Callback Handlers for Algorithm Access
 * @{
 * 
 * Per otbn_plan.md: Algorithms can access CSR/WDR registers via callbacks.
 */

/**
 * @brief Handle CSR register read from algorithm
 * @param address Byte offset into CSR space
 * @param data Pointer to return value
 * @return ERROR (not implemented in TLM)
 * 
 * Allows algorithm to read 32-bit CSR registers during execution.
 * Currently not implemented in TLM model - returns error.
 */
otbn_algorithm::status_t otbn_ip::csr_read_handler(uint32_t address, uint32_t* data) {
    REG_FUNC_TRACE(logger);
    // CSR (Control/Status Register) read handler
    // Address is byte offset into CSR space
    // For now, return error - algorithms don't typically need CSR access in TLM
    // Can be enhanced if specific algorithms need status register reads

    REG_ERROR(0, logger) << "[OTBN] CSR read from algorithm at address 0x" << std::hex << address << " - not implemented in TLM" << std::dec;
    (void)data;  // Suppress unused parameter warning
    return otbn_algorithm::ERROR;
}

/**
 * @brief Handle CSR register write from algorithm
 * @param address Byte offset into CSR space
 * @param data Write value
 * @return ERROR (not implemented in TLM)
 * 
 * Allows algorithm to write 32-bit CSR registers during execution.
 * Currently not implemented in TLM model - returns error.
 */
otbn_algorithm::status_t otbn_ip::csr_write_handler(uint32_t address, uint32_t data) {
    REG_FUNC_TRACE(logger);
    // CSR (Control/Status Register) write handler
    // Address is byte offset into CSR space

    REG_ERROR(0, logger) << "[OTBN] CSR write from algorithm at address 0x" << std::hex << address << " data 0x" << data << " - not implemented in TLM" << std::dec;
    return otbn_algorithm::ERROR;
}

/**
 * @brief Handle WDR register read from algorithm
 * @param address WDR register index (0-31 for w0-w31)
 * @param data Pointer to output buffer (4 x 64-bit words = 256 bits)
 * @return SUCCESS if read was allowed, ERROR on failure
 * 
 * Allows algorithm to read 256-bit WDR registers during execution.
 * Validates address range and checks if key is registered before allowing read.
 * Sets KEY_INVALID error bit if key is not registered.
 */
otbn_algorithm::status_t otbn_ip::wdr_read_handler(uint32_t address, uint64_t* data) {
    REG_FUNC_TRACE(logger);
    // WDR (Wide Data Register) read handler
    // Address is register index (0-31 for w0-w31)

    // Validate address range
    if (address >= NUM_WDR_REGISTERS) {
        REG_ERROR(0, logger) << "[OTBN] WDR read: Invalid register index " << address << " (max " << (NUM_WDR_REGISTERS-1) << ")";
        return otbn_algorithm::ERROR;
    }

    // Bug #3 fix: KEY_INVALID only applies to key WSRs (WDR20-23), not general-purpose WDRs
    // Per OTBN spec: KEY_INVALID fires only when an OTBN program accesses WsrKeyS0L/H or WsrKeyS1L/H
    // without a valid sideloaded key. w0-w19 and w24-w31 are always readable.
    if (address >= static_cast<uint32_t>(WDR_KEY_S0_L) &&
        address <= static_cast<uint32_t>(WDR_KEY_S1_H) &&
        !key_registered) {
        err_bits_accumulator |= otbn_err_bits::KEY_INVALID;
        REG_ERROR(0, logger) << "[OTBN] WDR read blocked: No valid key registered (KEY_INVALID set)";
        return otbn_algorithm::ERROR;
    }

    // Copy 256-bit register (4 x 64-bit words) to output
    data[0] = wdr_registers[address][0];
    data[1] = wdr_registers[address][1];
    data[2] = wdr_registers[address][2];
    data[3] = wdr_registers[address][3];

    REG_DEBUG(2, logger) << "[OTBN] WDR read: w" << address << " = 0x" << std::hex << data[3] << data[2] << data[1] << data[0] << std::dec;

    return otbn_algorithm::SUCCESS;
}

/**
 * @brief Handle WDR register write from algorithm
 * @param address WDR register index (0-31 for w0-w31)
 * @param data Pointer to input buffer (4 x 64-bit words = 256 bits)
 * @return SUCCESS if write was allowed, ERROR on failure
 * 
 * Allows algorithm to write 256-bit WDR registers during execution.
 * Validates address range before allowing write.
 */
otbn_algorithm::status_t otbn_ip::wdr_write_handler(uint32_t address, uint64_t* data) {
    REG_FUNC_TRACE(logger);
    // WDR (Wide Data Register) write handler
    // Address is register index (0-31 for w0-w31)

    if (address >= NUM_WDR_REGISTERS) {
        REG_ERROR(0, logger) << "[OTBN] WDR write: Invalid register index " << address << " (max " << (NUM_WDR_REGISTERS-1) << ")";
        return otbn_algorithm::ERROR;
    }

    // Copy 256-bit data (4 x 64-bit words) to register
    wdr_registers[address][0] = data[0];
    wdr_registers[address][1] = data[1];
    wdr_registers[address][2] = data[2];
    wdr_registers[address][3] = data[3];

    REG_DEBUG(2, logger) << "[OTBN] WDR write: w" << address << " = 0x" << std::hex << data[3] << data[2] << data[1] << data[0] << std::dec;

    return otbn_algorithm::SUCCESS;
}

// ============================================================================
// Key Manager TLM Target Socket Handler (per otbn_plan.md line 21)
// ============================================================================
/**
 * @brief TLM b_transport handler for Key Manager socket
 *
 * Per otbn_plan.md specification: "The model has a simple target socket that is
 * connected to an external key manager. The key manager can program the keys in
 * the WDR registers - KEY_S0_L/KEY_S0_H and KEY_S1_L/KEY_S1_H wide registers."
 *
 * Address Map:
 *   0x00-0x1F: KEY_S0_L (256 bits = 32 bytes) → WDR20
 *   0x20-0x3F: KEY_S0_H (256 bits = 32 bytes) → WDR21
 *   0x40-0x5F: KEY_S1_L (256 bits = 32 bytes) → WDR22
 *   0x60-0x7F: KEY_S1_H (256 bits = 32 bytes) → WDR23
 *
 * @param trans TLM generic payload
 * @param delay TLM timing annotation
 */
void otbn_ip::keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    REG_FUNC_TRACE(logger);
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64 addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    // Only support writes (Key Manager programs keys)
    if (cmd != tlm::TLM_WRITE_COMMAND) {
        REG_ERROR(0, logger) << "[OTBN KeyMgr TLM] Error: Only WRITE commands supported, got " << (cmd == tlm::TLM_READ_COMMAND ? "READ" : "UNKNOWN");
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    // Address map per otbn_wrapper_key.rdl spec:
    //   0x000-0x01F → WDR20 (KEY_S0_L):  share0 bits [255:0]
    //   0x020-0x02F → WDR21 (KEY_S0_H):  share0 bits [383:256] in lower 128b
    //   0x030-0x04F → WDR22 (KEY_S1_L):  share1 bits [255:0]
    //   0x050-0x05F → WDR23 (KEY_S1_H):  share1 bits [383:256] in lower 128b
    //   0x060       → KEY_CTRL: commits key (set key_registered = true)
    if (addr == 0x60) {
        // Bug #4 fix: honour the KEY_VALID bit value — writing 0 must invalidate the key
        uint32_t ctrl_val = *reinterpret_cast<uint32_t*>(ptr);
        key_registered = (ctrl_val & 0x1u) != 0;
        REG_INFO(1, logger) << "[OTBN KeyMgr TLM] KEY_CTRL written — key_registered = " << key_registered;
        delay += sc_time(10, SC_NS);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    int wdr_index = -1;
    unsigned int byte_offset = 0;

    if (addr < 0x020) {
        wdr_index   = WDR_KEY_S0_L;
        byte_offset = static_cast<unsigned int>(addr);
    } else if (addr < 0x030) {
        wdr_index   = WDR_KEY_S0_H;
        byte_offset = static_cast<unsigned int>(addr - 0x020);
    } else if (addr < 0x050) {
        wdr_index   = WDR_KEY_S1_L;
        byte_offset = static_cast<unsigned int>(addr - 0x030);
    } else if (addr < 0x060) {
        wdr_index   = WDR_KEY_S1_H;
        byte_offset = static_cast<unsigned int>(addr - 0x050);
    } else {
        REG_ERROR(0, logger) << "[OTBN KeyMgr TLM] Error: Invalid address 0x" << std::hex << addr << " (valid: 0x000-0x02F share0, 0x030-0x05F share1, 0x060 KEY_CTRL)" << std::dec;
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Write data to WDR register (up to 32 bytes per KEY register)
    for (unsigned int i = 0; i < len && (byte_offset + i) < 32; i++) {
        unsigned int byte_pos = byte_offset + i;
        unsigned int word_idx = byte_pos / 8;  // Which 64-bit word (0-3)
        unsigned int byte_in_word = byte_pos % 8;  // Byte within word (0-7)

        // Clear byte at position, then set new value
        uint64_t mask = ~(0xFFULL << (byte_in_word * 8));
        wdr_registers[wdr_index][word_idx] = (wdr_registers[wdr_index][word_idx] & mask) |
                                              ((uint64_t)ptr[i] << (byte_in_word * 8));
    }

    // Bug #6 fix: RTL assigns IsprKeyS0H/S1H as {128'b0, share[383:256]} — upper 128 bits are
    // always zero-padded. Enforce that here so WDR21[255:128] and WDR23[255:128] are never stale.
    if (wdr_index == WDR_KEY_S0_H || wdr_index == WDR_KEY_S1_H) {
        wdr_registers[wdr_index][2] = 0;
        wdr_registers[wdr_index][3] = 0;
    }

    REG_INFO(1, logger) << "[OTBN KeyMgr TLM] Wrote " << len << " bytes to address 0x" << std::hex << addr << " (WDR" << std::dec << wdr_index << ")";

    // Accept transaction with timing delay
    delay += sc_time(10, SC_NS);  // Abstract TLM timing
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
