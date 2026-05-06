/**
 * @file edn.h
 * @brief EDN (Entropy Distribution Network) main model class
 *
 * This file defines the main implementation class for the EDN IP module.
 * The edn class extends edn_base to provide the complete behavioral model
 * including register callbacks, state machine logic, and CSRNG interface.
 *
 * Operating Modes:
 * - Boot-Time Request Mode: Automatic instantiate and generate for fast boot
 * - Auto Request Mode: Hardware-managed continuous distribution with automatic reseed
 * - Software Port Mode: Firmware-controlled command-by-command operation
 *
 * Key Interfaces:
 * - TLM-2.0 register target socket: Configuration and status access
 * - CSRNG command interface: Issue instantiate, generate, reseed, uninstantiate commands
 * - CSRNG entropy interface: Receive 128-bit entropy blocks from CSRNG
 * - Endpoint interfaces: Distribute 32-bit entropy to up to 8 peripherals
 * - Interrupt signals: Command completion and fatal error notifications
 * - Alert signals: Recoverable and fatal security/error alerts
 *
 * @note This class should implement register callbacks and behavioral logic.
 * @note Default memory size is 0x48 bytes (covers all 17 registers).
 */

#pragma once
#include "edn_base.h"
#include "edn_csrng_interface.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <csml_logger.h>
#include <csml_parameter.h>
#include <vector>
#include <queue>
#include <cstring>
#include <openssl/rand.h>

/**
 * @enum EdnMainSmState
 * @brief EDN main state machine sparse-encoded state values
 *
 * Represents the 10 states of the EDN_MAIN_SM state machine with
 * sparse encoding for security hardening (similar to multi-bit encoding).
 * These values match the hardware implementation and are used to update
 * the MAIN_SM_STATE register during state transitions.
 */
enum class EdnMainSmState : uint32_t
{
    Idle = 0xC1,               ///< Idle state (reset state)
    BootInsAckWait = 0x36,     ///< Boot mode: waiting for instantiate ack
    BootGenAckWait = 0x9C,     ///< Boot mode: waiting for generate ack
    AutoLoadIns = 0x63,        ///< Auto mode: loading instantiate command
    AutoFirstAckWait = 0x5A,   ///< Auto mode: waiting for first ack
    AutoDispatch = 0x3C,       ///< Auto mode: dispatching generate commands
    AutoGenAckWait = 0x59,     ///< Auto mode: waiting for generate ack
    AutoReseedAckWait = 0xA5,  ///< Auto mode: waiting for reseed ack
    SWPortMode = 0x96,         ///< Software port mode active
    Error = 0x47               ///< Error state (fatal condition)
};

/**
 * @class edn_ip
 * @brief Complete EDN module implementation with behavioral logic
 *
 * Extends edn_base to provide full EDN functionality including:
 * - Register write/read callbacks for 14 registers requiring side effects
 * - State machine management (EDN_MAIN_SM, EDN_ACK_SM)
 * - CSRNG command interface and entropy distribution
 * - Multi-bit encoding validation (0x6=enable, 0x9=disable)
 * - FIFO management (RESEED_CMD, GENERATE_CMD with 13-word depth)
 * - Error detection and alert generation
 *
 * Register Callbacks Required:
 * - Write: INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST, REGWEN, CTRL
 *          SW_CMD_REQ, RESEED_CMD, GENERATE_CMD, RECOV_ALERT_STS, ERR_CODE_TEST
 * - Read: INTR_STATE, SW_CMD_STS, HW_CMD_STS, ERR_CODE, MAIN_SM_STATE
 *
 * @note Implement behavioral logic in the constructor and callback methods.
 */
class edn_ip : public edn_base
{
   SC_HAS_PROCESS(edn_ip);

  public:
   // =========================================================================
   // =========================================================================
   // Interrupt Signals
   // =========================================================================

   /// Software command request completion interrupt
   sc_out<bool> intr_edn_cmd_req_done;

   /// Fatal error interrupt (FIFO errors)
   sc_out<bool> intr_edn_fatal_err;

   // =========================================================================
   // Alert Signals
   // =========================================================================

   /// Recoverable alert (entropy bus consistency, field configuration errors)
   sc_out<bool> alert_recov_alert;

   /// Fatal alert (state machine errors, FIFO errors, counter errors)
   sc_out<bool> alert_fatal_alert;

   // =========================================================================
   // Clock and Reset
   // =========================================================================

   /// Abstract clock input (frequency in Hz, not cycle-accurate)
   sc_in<double> clk_i;

   /// Active-low asynchronous reset
   sc_in<bool> rst_ni;

   /// @name CSML Logger
   /// @{
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
   CsmlLogger logger;  ///< Logger instance for diagnostic output
   csml_param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
   /// @}

   uint32_t m_forced_csrng_ack_status;
   void force_csrng_ack_status(uint32_t status) { m_forced_csrng_ack_status = status; }

   /**
    * @brief Constructor for EDN module
    * @param n SystemC module name
    *
    * Initializes the EDN module with:
    * - Register callback registration for all 14 registers requiring side effects
    * - State machine initialization (MAIN_SM_STATE starts in Idle=0xC1)
    * - FIFO initialization for command FIFOs
    * - Default configuration (all modes disabled, REGWEN unlocked)
    * - TLM quantumkeeper for temporal decoupling
    * - Port array initialization
    *
    * @note User should implement register callbacks and behavioral processes here.
    * @note Default memory size (0x48 bytes) is used from base class.
    */
   edn_ip(sc_module_name n);

  private:
   // =========================================================================
   // SystemC Processes
   // =========================================================================

   /**
    * @brief Asynchronous reset process
    *
    * Triggered on negative edge of rst_ni (active-low reset).
    * Resets all registers to default values and clears internal state.
    */
   void reset_process();

   // =========================================================================
   // Register Callback Methods
   // =========================================================================

   /**
    * @brief Write callback for REGWEN register
    * @param value Value being written to REGWEN register
    * @return true if write is accepted, false if rejected
    *
    * Implements Write-0-to-Clear (W0C) semantics for REGWEN bit [0]:
    * - When bit [0] is written to 0, it locks permanently until reset
    * - Once locked (REGWEN=0), subsequent writes have no effect
    * - Writing 1 has no effect (W0C mechanism)
    */
   bool regwen_write_callback(uint32_t value);

   /**
    * @brief Write callback for CTRL register
    * @param value Value being written to CTRL register
    * @return true if write is accepted, false if rejected
    *
    * Implements three-stage validation:
    * 1. REGWEN Protection: Reject write if REGWEN=0 (locked)
    * 2. Multi-bit Encoding Validation: Check all 4 fields for valid values (0x6 or 0x9)
    *    - If invalid, set corresponding alert bit in RECOV_ALERT_STS and assert alert
    * 3. State Machine Transitions: Update MAIN_SM_STATE when EDN_ENABLE changes
    */
   bool ctrl_write_callback(uint32_t value);

   /**
    * @brief Helper function to validate multi-bit encoding
    * @param value 4-bit field value to validate
    * @return true if value is 0x6 (enable) or 0x9 (disable), false otherwise
    *
    * Used to validate the 4 multi-bit encoded fields in CTRL register:
    * - EDN_ENABLE [3:0]
    * - BOOT_REQ_MODE [7:4]
    * - AUTO_REQ_MODE [11:8]
    * - CMD_FIFO_RST [15:12]
    */
   bool is_multibit_valid(uint32_t value);

   bool main_sm_state_read_callback(uint32_t& value);

   /**
    * @brief Write callback for INTR_STATE register (W1C)
    * @param value Write value (bits set to 1 will clear corresponding status bits)
    * @return true to accept write
    *
    * Implements Write-1-to-Clear (W1C) semantics for interrupt status bits:
    * - INTR_STATE.edn_cmd_req_done [0]: Software command completion
    * - INTR_STATE.edn_fatal_err [1]: Fatal error condition
    *
    * Side Effects:
    * - Writing 1 to a bit clears that interrupt status bit
    * - Writing 0 has no effect (W1C mechanism)
    * - Notifies interrupt_driver() to update interrupt output signals
    * - Interrupt signal deasserts if status cleared and no other conditions active
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["INTR_STATE"].access: "RW1C"
    * - architecture_map.registers["INTR_STATE"].write_effects: Clear status bits
    * - architecture_map.side_effects["w1c_clearing"]: W1C semantics
    */
   bool handle_write_INTR_STATE(uint32_t value);

   /**
    * @brief Write callback for INTR_ENABLE register
    * @param value New interrupt enable mask value
    * @return true to accept write
    *
    * Controls which interrupt sources can generate interrupt signals:
    * - INTR_ENABLE.edn_cmd_req_done [0]: Enable command completion interrupt
    * - INTR_ENABLE.edn_fatal_err [1]: Enable fatal error interrupt
    *
    * Side Effects:
    * - Updates INTR_ENABLE register fields
    * - Notifies interrupt_driver() to re-evaluate interrupt outputs
    * - If corresponding INTR_STATE bit is set, interrupt may assert/deassert
    * - Interrupt signal = INTR_STATE AND INTR_ENABLE (per-bit logic)
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["INTR_ENABLE"].write_effects: Update enable mask
    * - Interrupt assertion logic: (INTR_STATE[i] == 1) && (INTR_ENABLE[i] == 1)
    */
   bool handle_write_INTR_ENABLE(uint32_t value);

   /**
    * @brief Write callback for INTR_TEST register
    * @param value Test bits to force interrupt status (write-only)
    * @return true (does not store value, write-only register)
    *
    * Forces interrupt status bits for testing interrupt handler paths:
    * - INTR_TEST.edn_cmd_req_done [0]: Force command completion interrupt
    * - INTR_TEST.edn_fatal_err [1]: Force fatal error interrupt
    *
    * Side Effects:
    * - Writing 1 to bit[i] sets corresponding INTR_STATE bit[i]
    * - Does NOT store value (write-only test register)
    * - If INTR_ENABLE bit is set, interrupt signal will assert
    * - Allows testing interrupt paths without triggering actual conditions
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["INTR_TEST"].access: "WO"
    * - architecture_map.registers["INTR_TEST"].write_effects: Set INTR_STATE bits
    * - Test mechanism per standard interrupt architecture pattern
    */
   bool handle_write_INTR_TEST(uint32_t value);

   /**
    * @brief Read callback for INTR_TEST register
    * @param value Reference to read value output
    * @return true to allow read
    *
    * Forces read value to 0x0 for write-only test register.
    * INTR_TEST is a write-only register used to force interrupt status bits.
    * Reads always return 0 per write-only register semantics.
    */
   bool handle_read_INTR_TEST(uint32_t& value);

   /**
    * @brief Read callback for INTR_STATE register
    * @param value Reference to read value output
    * @return true to allow read
    *
    * Returns current interrupt status reflecting hardware events:
    * - INTR_STATE.edn_cmd_req_done [0]: Set on SW command completion
    * - INTR_STATE.edn_fatal_err [1]: Set on FIFO/state machine/CSRNG errors
    *
    * Interrupt Status Sources:
    * - edn_cmd_req_done: Set by process_sw_command_async() on success
    * - edn_fatal_err: Set by trigger_fifo_overflow_error(), handle_csrng_error()
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["INTR_STATE"].read_effects: Return status bits
    * - Status bits are sticky until cleared via W1C write
    */
   bool handle_read_INTR_STATE(uint32_t& value);

   /**
    * @brief Write callback for RECOV_ALERT_STS register (W0C)
    */
   bool handle_write_RECOV_ALERT_STS(uint32_t value);

   /**
    * @brief Write callback for RESEED_CMD register
    * @param value Command word being written to FIFO
    * @return true if write accepted, false if rejected
    *
    * Implements auto mode reseed command FIFO functionality (EDN_FUNC_007):
    * - Pushes command words to reseed FIFO (13-word depth maximum)
    * - Detects FIFO overflow condition (write to full FIFO)
    * - On overflow:
    *   - Sets ERR_CODE.SFIFO_RESCMD_ERR (bit 0)
    *   - Sets ERR_CODE.FIFO_WRITE_ERR (bit 28)
    *   - Triggers edn_fatal_err interrupt
    *   - Asserts fatal alert
    *   - Rejects write (overflow protection)
    * - Command words include header and optional additional data (clen 0-12)
    * - FIFO cleared on mode exit or reset
    */
   bool handle_write_RESEED_CMD(uint32_t value);

   /**
    * @brief Write callback for GENERATE_CMD register
    * @param value Command word being written to FIFO
    * @return true if write accepted, false if rejected
    *
    * Implements auto mode generate command FIFO functionality (EDN_FUNC_007):
    * - Pushes command words to generate FIFO (13-word depth maximum)
    * - Detects FIFO overflow condition (write to full FIFO)
    * - On overflow:
    *   - Sets ERR_CODE.SFIFO_GENCMD_ERR (bit 1)
    *   - Sets ERR_CODE.FIFO_WRITE_ERR (bit 28)
    *   - Triggers edn_fatal_err interrupt
    *   - Asserts fatal alert
    *   - Rejects write (overflow protection)
    * - Command words include header and optional additional data (clen 0-12)
    * - FIFO cleared on mode exit or reset
    */
   bool handle_write_GENERATE_CMD(uint32_t value);

   /**
    * @brief Trigger FIFO overflow fatal error
    * @param fifo_type Type of FIFO that overflowed ("RESEED_CMD" or "GENERATE_CMD")
    *
    * Common fatal error handler for FIFO overflow conditions:
    * - Sets appropriate ERR_CODE.SFIFO_*_ERR bit (0=reseed, 1=generate)
    * - Sets ERR_CODE.FIFO_WRITE_ERR (bit 28) aggregate indicator
    * - Sets INTR_STATE.edn_fatal_err (bit 1)
    * - Asserts alert_fatal_alert output
    * - Transitions state machine to Error state
    * - Errors are sticky until reset
    */
   void trigger_fifo_overflow_error(const char* fifo_type);

   // =========================================================================
   // EDN_FUNC_010: Alert Generation and Error Reporting Callbacks
   // =========================================================================

   /**
    * @brief Write callback for ALERT_TEST register
    * @param value Alert test bits (write-only)
    * @return true (does not store value, write-only register)
    *
    * Forces alert output signals for testing alert wiring and system-level
    * alert aggregation logic without modifying status registers:
    * - ALERT_TEST.recov_alert [0]: Force alert_recov_alert signal assertion
    * - ALERT_TEST.fatal_alert [1]: Force alert_fatal_alert signal assertion
    *
    * Side Effects:
    * - Temporarily pulses alert output signals
    * - Does NOT modify RECOV_ALERT_STS or ERR_CODE registers
    * - Does NOT store value (write-only test register)
    * - Allows verification of alert paths without creating errors
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["ALERT_TEST"].access: "WO"
    * - architecture_map.registers["ALERT_TEST"].write_effects: Pulse alerts
    * - architecture_map.side_effects["alert_test_mechanism"]
    */
   bool handle_write_ALERT_TEST(uint32_t value);

   /**
    * @brief Read callback for ALERT_TEST register
    * @param value Reference to read value output
    * @return true to allow read
    *
    * Forces read value to 0x0 for write-only test register.
    * ALERT_TEST is a write-only register used to force alert signals.
    * Reads always return 0 per write-only register semantics.
    */
   bool handle_read_ALERT_TEST(uint32_t& value);

   /**
    * @brief Write callback for ERR_CODE_TEST register
    * @param value Error code test value [4:0] specifies ERR_CODE bit to force
    * @return true (does not store value, write-only register)
    *
    * Forces specific ERR_CODE bits for testing error handling paths:
    * - ERR_CODE_TEST [4:0]: Bit position (0-30) in ERR_CODE to force
    * - Sets corresponding ERR_CODE bit (sticky until reset)
    * - Sets INTR_STATE.edn_fatal_err
    * - Asserts alert_fatal_alert signal
    * - Transitions state machine to Error state
    *
    * Side Effects:
    * - Forces ERR_CODE bit at specified position
    * - Triggers fatal error interrupt and alert
    * - Does NOT store value (write-only test register)
    * - Allows testing error paths without actual hardware faults
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["ERR_CODE_TEST"].access: "WO"
    * - architecture_map.registers["ERR_CODE_TEST"].write_effects: Set ERR_CODE bit
    * - architecture_map.side_effects["error_injection_test"]
    */
   bool handle_write_ERR_CODE_TEST(uint32_t value);

   /**
    * @brief Read callback for ERR_CODE_TEST register
    * @param value Reference to read value output
    * @return true to allow read
    *
    * Forces read value to 0x0 for write-only test register.
    * ERR_CODE_TEST is a write-only register used to force ERR_CODE bits.
    * Reads always return 0 per write-only register semantics.
    */
   bool handle_read_ERR_CODE_TEST(uint32_t& value);

   /**
    * @brief Read callback for ERR_CODE register
    * @param value Reference to read value output
    * @return true to allow read
    *
    * Returns current fatal error code register reflecting hardware errors:
    * - ERR_CODE.SFIFO_RESCMD_ERR [0]: RESEED_CMD FIFO overflow
    * - ERR_CODE.SFIFO_GENCMD_ERR [1]: GENERATE_CMD FIFO overflow
    * - ERR_CODE.EDN_ACK_SM_ERR [20]: ACK state machine illegal state
    * - ERR_CODE.EDN_MAIN_SM_ERR [21]: Main state machine illegal state
    * - ERR_CODE.EDN_CNTR_ERR [22]: Hardened counter error
    * - ERR_CODE.FIFO_WRITE_ERR [28]: Internal FIFO write error
    * - ERR_CODE.FIFO_READ_ERR [29]: Internal FIFO read error
    * - ERR_CODE.FIFO_STATE_ERR [30]: Internal FIFO state error
    *
    * Error Status Sources:
    * - Set by trigger_fifo_overflow_error(), handle_write_ERR_CODE_TEST()
    * - Set by state machine error detection (future implementation)
    *
    * All bits sticky until system reset (read-only register).
    *
    * Architecture Map Alignment:
    * - architecture_map.registers["ERR_CODE"].access: "RO"
    * - architecture_map.registers["ERR_CODE"].read_effects: Return sticky errors
    * - Sticky semantics: bits remain set until reset
    */
   bool handle_read_ERR_CODE(uint32_t& value);

   // =========================================================================
   // Internal State
   // =========================================================================

   /// TLM quantumkeeper for temporal decoupling
   tlm_utils::tlm_quantumkeeper m_qk;

   /// REGWEN lock status (true = unlocked, false = locked)
   bool m_regwen_locked;

   /// EDN main state machine current state
   EdnMainSmState m_main_sm_state;

   // =========================================================================
   // CSRNG Command Management (EDN_FUNC_004)
   // =========================================================================

   /// SW command buffer for multi-word accumulation (header + 0-12 data words)
   std::vector<uint32_t> m_sw_cmd_buffer;

   /// Expected number of words for current SW command (parsed from header clen field)
   uint32_t m_expected_sw_cmd_words;

   /// SW command processing flag (true when command in progress)
   bool m_sw_cmd_processing;

   /// SW_CMD_STS initialization flag (true after first module enable since reset)
   /// Used to differentiate reset value (0x0) from operational dynamic values
   bool m_sw_cmd_sts_initialized;

   /// HW command buffer (for boot-time and auto-request mode commands)
   std::vector<uint32_t> m_hw_cmd_buffer;

   /// Entropy buffer (32-bit chunks from 128-bit CSRNG blocks)
   std::queue<uint32_t> m_entropy_buffer;

   /// FIPS compliance status for buffered entropy
   bool m_entropy_fips;

   // === Interrupt and Alert State (Single-Writer Pattern) ===
   sc_event m_intr_update_event;
   sc_event m_alert_update_event;
   void interrupt_driver();
   void alert_driver();

   /**
    * @brief Write callback for SW_CMD_REQ register
    * @param value Command word being written
    * @return true if write accepted, false if rejected
    *
    * Implements SW command FIFO functionality:
    * - Accumulates multi-word commands (header + 0-12 data words)
    * - Parses header clen field to determine command length
    * - Forwards complete commands to CSRNG via send_command()
    * - Updates SW_CMD_STS on completion
    * - Generates interrupt on command completion
    * - Handles CSRNG errors with dual alert mechanism
    */
   bool handle_write_SW_CMD_REQ(uint32_t value);

   /**
    * @brief Read callback for SW_CMD_STS register
    * @param value Reference to value being read
    * @return true to allow read
    *
    * Returns SW command status:
    * - CMD_REG_RDY [0]: Ready for next command word (1=ready)
    * - CMD_RDY [1]: Ready for new command (1=ready)
    * - CMD_ACK [2]: Command acknowledged by CSRNG (1=acked)
    * - CMD_STS [5:3]: CSRNG status code (0=success)
    */
   bool handle_read_SW_CMD_STS(uint32_t& value);

   /**
    * @brief Read callback for HW_CMD_STS register
    * @param value Reference to value being read
    * @return true to allow read
    *
    * Returns HW command status for boot-time and auto modes:
    * - BOOT_MODE [0]: Boot mode active
    * - AUTO_MODE [1]: Auto mode active
    * - CMD_TYPE [5:2]: Last HW command type
    * - CMD_ACK [6]: HW command acknowledged
    * - CMD_STS [9:7]: CSRNG status for HW commands
    */
   bool handle_read_HW_CMD_STS(uint32_t& value);

   /**
    * @brief Handle CSRNG error with dual alert mechanism
    * @param ack_status Non-zero CSRNG acknowledgment status
    *
    * Implements dual error response per architecture:
    * - Sets RECOV_ALERT_STS.CSRNG_ACK_ERR (bit 13) - recoverable
    * - Sets ERR_CODE.SFIFO_ESRNG_ERR (bit 2) - fatal
    * - Triggers edn_fatal_err interrupt
    * - Asserts fatal alert
    * - Transitions state machine to Error state
    */
   void handle_csrng_error(uint32_t ack_status);

   /**
    * @brief Asynchronous SW command processing thread
    *
    * Spawned via sc_spawn() when a complete SW command is received.
    * Follows AES EDN interface pattern: callbacks cannot contain wait(),
    * so this async thread handles CSRNG interface calls and can model
    * timing delays. Processes accumulated command buffer, forwards to
    * CSRNG, handles acknowledgment, and updates SW_CMD_STS.
    */
   void process_sw_command_async();

   /**
    * @brief Send hardware-generated CSRNG command (async thread wrapper)
    * @param cmd_header Command header word (cmd type, clen, flags)
    * @param cmd_data Pointer to additional command data words (may be nullptr if clen=0)
    * @param num_data_words Number of data words (must match clen in header)
    *
    * Used by boot-time and auto-request mode state machines to issue
    * hardware-controlled CSRNG commands. Updates HW_CMD_STS register
    * and handles acknowledgment/errors.
    */
   void send_hw_csrng_command(uint32_t cmd_header, const uint32_t* cmd_data, uint32_t num_data_words);

   /**
    * @brief Asynchronous HW command processing thread
    *
    * Spawned via sc_spawn() to process hardware commands asynchronously.
    * Uses m_hw_cmd_buffer member variable to access command data.
    * This allows modeling timing delays without blocking the caller.
    */
   void process_hw_command_async();

   /**
    * @brief Receive entropy block from CSRNG
    * @param genbits 128-bit entropy block (4x 32-bit words)
    * @param fips_compliance FIPS indicator for this block
    *
    * Converts 128-bit CSRNG block to 4x 32-bit chunks and stores in
    * entropy buffer for endpoint distribution. Chunk order:
    * [0]=bits[127:96], [1]=bits[95:64], [2]=bits[63:32], [3]=bits[31:0]
    */
   void receive_csrng_entropy(const uint32_t genbits[4], bool fips_compliance);



   // =========================================================================
   // Auto Request Mode FIFO Management (EDN_FUNC_007)
   // =========================================================================

   /// Maximum FIFO depth for RESEED_CMD and GENERATE_CMD (13 words per architecture)
   static const unsigned int FIFO_MAX_DEPTH = 13;

   /// Reseed command FIFO (stores up to 13 words: header + 0-12 data)
   std::queue<uint32_t> m_reseed_cmd_fifo;

   /// Generate command FIFO (stores up to 13 words: header + 0-12 data)
   std::queue<uint32_t> m_generate_cmd_fifo;

   // =========================================================================
   // EDN_FUNC_013: Auto Request Mode State
   // =========================================================================

   /// Generate command counter (tracks generates issued since last reseed)
   uint32_t m_auto_generate_counter;

   /// Flag indicating instantiate has been issued in auto mode
   bool m_auto_instantiated;



   // =========================================================================
   // Entropy Bus Consistency Checking (EDN_FUNC_008)
   // =========================================================================

   /// Previous genbits value for consecutive comparison (128-bit as 4x 32-bit)
   uint32_t m_prev_genbits[4];

   /// Flag indicating if previous genbits value is valid for comparison
   bool m_prev_genbits_valid;

   /**
    * @brief Check entropy bus consistency between consecutive genbits
    * @param current_genbits Current 128-bit entropy block (4x 32-bit words)
    *
    * Compares current genbits value against previously received value.
    * If all 4 words match consecutively, sets RECOV_ALERT_STS.EDN_BUS_CMP_ALERT
    * and asserts recoverable alert signal.
    *
    * Architecture Map Alignment:
    * - architecture_map.side_effects["entropy_bus_consistency_check"]
    * - architecture_map.registers["RECOV_ALERT_STS"].fields["EDN_BUS_CMP_ALERT"]
    * - Section 1.10 of detailed design: Entropy Bus Consistency Checking
    *
    * Detection Logic:
    * - Consecutive 128-bit blocks are compared word-by-word
    * - Match on all 4 words indicates potential bus fault or data corruption
    * - First genbits after reset/enablement has no previous value (no check)
    *
    * Error Response:
    * - Set RECOV_ALERT_STS.EDN_BUS_CMP_ALERT (bit 12)
    * - Assert alert_recov_alert output (level-sensitive)
    * - Firmware clears via W0C write to RECOV_ALERT_STS
    * - Does NOT prevent entropy distribution (recoverable condition)
    *
    * Security Implications:
    * - Duplicate entropy indicates potential entropy source failure
    * - May signal CSRNG malfunction or security attack
    * - Firmware should log occurrence and consider CSRNG reseed
    */
   void check_entropy_bus_consistency(const uint32_t current_genbits[4]);

   // =========================================================================
   // EDN_FUNC_012: Boot-Time Request Mode Operation
   // =========================================================================

   /**
    * @brief Boot-time mode Instantiate command sequence (async thread)
    *
    * Automatically sends Instantiate command to CSRNG using BOOT_INS_CMD
    * register configuration when entering BootInsAckWait state.
    *
    * Architecture Map Alignment:
    * - architecture_map.operations["boot_time_mode"].steps[1]: Issue instantiate
    * - architecture_map.state_machines["EDN_MAIN_SM"].transitions: BootInsAckWait→BootGenAckWait
    * - architecture_map.registers["BOOT_INS_CMD"]: Command configuration source
    *
    * State Transitions:
    * - On CSRNG success: BootInsAckWait → BootGenAckWait, spawn boot_mode_generate()
    * - On CSRNG error: Transition to Error state, set ERR_CODE.SFIFO_ESRNG_ERR
    *
    * Command Format:
    * - Uses BOOT_INS_CMD register value as header word
    * - Hardware enforces clen=0 (no personalization string in boot mode)
    * - Default value 0x901: Instantiate command, clen=0, no flags
    *
    * Timing Considerations:
    * - Instantiate typically takes ~5ms (physical entropy gathering)
    * - Pre-FIPS seed for first EDN (reduced latency)
    * - Subsequent EDNs wait for FIPS-approved seeds
    *
    * Detailed Design Reference: Section 1.2.1 Boot-Time Request Mode
    */
   void boot_mode_instantiate();

   /**
    * @brief Boot-time mode Generate command sequence (async thread)
    *
    * Automatically sends Generate command to CSRNG using BOOT_GEN_CMD
    * register configuration after successful Instantiate acknowledgment.
    *
    * Architecture Map Alignment:
    * - architecture_map.operations["boot_time_mode"].steps[2]: Issue generate
    * - architecture_map.registers["BOOT_GEN_CMD"]: Command configuration source
    * - architecture_map.timing_constraints["generate_command_duration"]: ~0.7ms max
    *
    * State Behavior:
    * - Stays in BootGenAckWait state during CSRNG processing
    * - Entropy delivered to endpoints via endpoint distribution logic
    * - edn_fips signals de-asserted (pre-FIPS entropy)
    *
    * Command Format:
    * - Uses BOOT_GEN_CMD register value as header word
    * - Hardware enforces clen=0 (no additional data)
    * - Default glen=0xFFF (4096 blocks, maximum boot entropy)
    *
    * Boot Entropy Limitations:
    * - Fixed entropy quantity defined by BOOT_GEN_CMD.glen
    * - If glen limit reached while endpoints requesting: endpoint bus hangs
    * - Firmware must exit boot mode before exhaustion
    *
    * FIPS Considerations:
    * - Boot mode uses pre-FIPS seed (fast boot, ~2ms availability)
    * - edn_fips[0:7] remain de-asserted during boot operation
    * - Firmware must exit and re-instantiate for FIPS-approved entropy
    *
    * Detailed Design Reference: Section 1.2.1, Section 4.7.1 Corner Cases
    */
   void boot_mode_generate();

   /**
    * @brief Boot-time mode Uninstantiate command sequence (async thread)
    *
    * Automatically sends Uninstantiate command when exiting boot-time mode
    * by clearing CTRL.BOOT_REQ_MODE. Destroys the CSRNG instance associated
    * with this EDN to prevent synchronization errors.
    *
    * Architecture Map Alignment:
    * - architecture_map.operations["boot_time_mode"].exit_sequence: Auto uninstantiate
    * - architecture_map.state_machines["EDN_MAIN_SM"].transitions: Boot→SWPortMode
    * - Prevents CSRNG/EDN desynchronization per Section 4.7.3
    *
    * Exit Sequence:
    * 1. Firmware clears CTRL.BOOT_REQ_MODE (keeps EDN_ENABLE=0x6)
    * 2. State machine transitions to SWPortMode
    * 3. EDN automatically issues Uninstantiate command
    * 4. HW_CMD_STS.BOOT_MODE cleared
    * 5. Firmware can now issue SW commands via SW_CMD_REQ
    *
    * Alternative Exit (Not Recommended):
    * - Clearing CTRL.EDN_ENABLE disables EDN immediately
    * - Causes EDN/CSRNG desynchronization
    * - Requires full CSRNG disable/re-enable before EDN re-enable
    *
    * Command Format:
    * - Uninstantiate command header: 0x5 (cmd_type=5, clen=0)
    * - No additional data words (uninstantiate has no parameters)
    *
    * Synchronization Importance:
    * - Ensures CSRNG instance properly destroyed
    * - Allows clean transition to software port mode
    * - Prevents "orphaned" CSRNG instances
    *
    * Detailed Design Reference: Section 1.2.1, Section 4.7.3 Error Prevention
    */
   void boot_mode_uninstantiate();

   // =========================================================================
   // EDN_FUNC_013: Auto Request Mode Operation
   // =========================================================================

   /// Auto mode generate counter (tracks generates since last reseed)
   uint32_t m_auto_gen_counter;

   /**
    * @brief Auto mode initialization after manual Instantiate
    *
    * Architecture Map: architecture_map.operations["auto_request_mode"]
    * Detailed Design: Section 1.2.2 Auto Request Mode
    */
   void auto_mode_init();

   /**
    * @brief Auto mode dispatcher thread (continuous operation)
    *
    * Architecture Map: architecture_map.state_machines["EDN_MAIN_SM"].AutoDispatch
    * Detailed Design: Section 1.2.2, Section 4.8
    */
   void auto_mode_dispatch();

   /**
    * @brief Issue Generate command from GENERATE_CMD FIFO
    * @return CSRNG acknowledgment status (0=success)
    */
   uint32_t auto_mode_issue_generate();

   /**
    * @brief Issue Reseed command from RESEED_CMD FIFO
    * @return CSRNG acknowledgment status (0=success)
    */
   uint32_t auto_mode_issue_reseed();
};
