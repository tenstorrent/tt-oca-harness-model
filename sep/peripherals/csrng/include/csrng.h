// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file crng.h
 * @brief Main CRNG TLM model class definition and behavioral logic
 *
 * This header defines the complete CRNG TLM model including:
 * - CTR_DRBG (Counter mode Deterministic Random Bit Generator) implementation
 * - Command processing state machine
 * - Port interfaces for clock, reset, and interrupts
 * - Hardware registers access and callback handlers
 * - Security features: multi-bit encoding, access control, FIPS compliance
 */

#pragma once
#include "csrng_base.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <queue>
#include "reg_logger.h"
#include "reg_param.h"

/**
 * @class csrng_model
 * @brief SystemC TLM model for the CRNG (Cryptographic Random Number Generator)
 *
 * This class implements the complete behavior of the CRNG module, including
 * CTR_DRBG random number generation, entropy source integration, seed life
 * management, command processing, and security countermeasures.
 *
 * It inherits from csrng_base which provides the register infrastructure and
 * memory-mapped access via a TLM target socket.
 */
class csrng_model : public csrng_base
{
   friend class testbench;
public:
   SC_HAS_PROCESS(csrng_model);

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
   regmodel::Param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
   /**
    * @brief Constructor for the CRNG model
    * @param n SystemC module name
    */
   csrng_model(sc_module_name n);

   /** @brief Destructor - cleans up OpenSSL contexts and resources */
   ~csrng_model();

   // =============================================================================
   // Port Interfaces
   // =============================================================================

   /// @name Clock and Reset
   /// @{
   sc_in<bool> clk_i;             ///< Clock input signal
   sc_in<bool> rst_ni;            ///< Active-low asynchronous reset input
   /// @}
  //Fix:Otp en signal added
   sc_in<uint8_t> otp_en_csrng_sw_app_read;  ///< OTP enable signal 
//Fix:4interrupt added
   /// @name Interrupt Outputs
   /// @{
   sc_out<bool> cs_cmd_req_done;     ///< Command completion interrupt
   sc_out<bool> cs_entropy_req;      ///< Entropy request interrupt
   sc_out<bool> cs_hw_inst_exc;       ///< Hardware instance exception interrupt
   sc_out<bool> cs_fatal_err;        ///< Fatal error interrupt
   /// @}

   //Fix:Alert signal added
   /// @name Alert Outputs (FUNC_ALERT_TEST)
   /// @{
   sc_out<bool> recov_alert_o;       ///< Recoverable alert output
   sc_out<bool> fatal_alert_o;       ///< Fatal alert output
   /// @}
   /// @name RegLogger
   /// @{
   RegLogger logger;  ///< Logger instance for diagnostic output
   /// @}

protected:
   // =============================================================================
   // Internal Enumerations
   // =============================================================================

   /**
    * @enum CommandFSMState
    * @brief Internal states for the command processing state machine
    */
   enum class CommandFSMState {
      IDLE,              ///< Idle - ready to accept commands
      COMMAND_DISPATCH,  ///< Dispatching command to handler
      ENTROPY_REQUEST,   ///< Waiting for entropy from entropy source
      CTR_DRBG_GENERATE, ///< Generating random blocks
      STATE_UPDATE,      ///< Updating instance state
      ERROR              ///< Fatal error state
   };

   /**
    * @enum MainSMStateValue
    * @brief Hardware-defined state encoding values for the MAIN_SM_STATE register
    *
    * These values are written to the MAIN_SM_STATE register by the command FSM
    * to provide debug visibility into the current state machine state.
    */
   enum MainSMStateValue : uint32_t {
      MAIN_SM_IDLE            = 0x4E,  ///< Idle state (module disabled or no commands)
      MAIN_SM_SW_CMD_RDY      = 0x99,  ///< Software command ready (module enabled, awaiting commands)
      MAIN_SM_CMD_DISPATCH    = 0xC3,  ///< Command dispatch state
      MAIN_SM_ENTROPY_REQUEST = 0x73,  ///< Entropy request / boot load state
      MAIN_SM_CTR_DRBG_GEN    = 0xE1,  ///< CTR_DRBG generate processing state
      MAIN_SM_STATE_UPDATE    = 0x3C,  ///< State update / command ACK state
      MAIN_SM_ERROR           = 0xFF   ///< Error state
   };

   /**
    * @enum CommandStatus
    * @brief Command completion status codes
    */
   enum CommandStatus {
      CMD_SUCCESS = 0x0,              ///< Command completed successfully
      CMD_INVALID_ACMD = 0x1,         ///< Invalid command type
      CMD_INVALID_GEN_CMD = 0x2,      ///< Invalid GENERATE command parameters
      CMD_INVALID_CMD_SEQ = 0x3,      ///< Invalid command sequence
      CMD_RESEED_CNT_EXCEEDED = 0x4   ///< Reseed counter exceeded
   };

   /**
    * @struct DRBGInstance
    * @brief State for a single DRBG instance
    */
   struct DRBGInstance {
      int instance_num;                  ///< Instance number (0-2)
      uint8_t status;                    ///< Instantiation status (0=uninstantiated, 1=instantiated)
      uint8_t compliance_flag;           ///< FIPS compliance flag
      uint32_t reseed_counter;           ///< Reseed counter
      std::array<uint32_t, 4> V;         ///< V value (128 bits)
      std::array<uint32_t, 8> Key;       ///< Key value (256 bits)
      EVP_CIPHER_CTX* drbg_ctx;          ///< OpenSSL context for DRBG operations
   };

   // =============================================================================
   // Internal State Variables
   // =============================================================================

   /// @name Temporal Decoupling
   tlm_utils::tlm_quantumkeeper m_qk; ///< Quantum keeper for Loosely-Timed (LT) synchronization

   /// @name Command FSM State
   CommandFSMState m_cmd_fsm_state;   ///< Current state of command FSM
   bool m_cmd_ready;                  ///< Command interface ready flag
   bool m_cmd_ack;                    ///< Command acknowledgment flag
   CommandStatus m_cmd_status;        ///< Command status code
   int m_current_instance;            ///< Currently selected instance
   bool m_command_in_progress;        ///< Command collection in progress
   uint32_t m_current_command;        ///< Current command header
   std::array<uint32_t, 12> m_additional_data; ///< Additional data buffer
   uint8_t m_additional_data_count;   ///< Count of additional data words received
   uint8_t m_expected_additional_data; ///< Expected additional data words

   /// @name DRBG Instance State
   std::array<DRBGInstance, 3> m_drbg_instances; ///< DRBG instances (0=SW, 1-2=HW)

   /// @name GENBITS Output State
   bool m_genbits_valid;              ///< GENBITS data valid flag
   bool m_genbits_fips;               ///< GENBITS FIPS compliance flag
   int m_genbits_read_index;          ///< Sequential read pointer for GENBITS (0-3)
   std::array<uint32_t, 4> m_genbits_buffer; ///< 128-bit GENBITS buffer
   std::queue<std::array<uint32_t, 4>> m_genbits_block_queue; ///< Queue for multi-block generation (glen > 1)
   uint64_t m_previous_genbits_64;    ///< Previous 64-bit value for repetition check

   /// @name Internal State Read Control
   int m_int_state_num;               ///< Selected instance for internal state read
   int m_int_state_read_index;        ///< Sequential read pointer for INT_STATE_VAL (0-13)

   /// @name Error and Alert State
   bool m_fatal_error;                ///< Fatal error occurred flag
   bool m_recoverable_alert;          ///< Recoverable alert occurred flag

   /// @name Interrupt State
   bool m_intr_cmd_req_done;          ///< Command completion interrupt
   bool m_intr_entropy_req;           ///< Entropy request interrupt
   bool m_intr_hw_inst_exc;           ///< Hardware instance exception interrupt
   bool m_intr_fatal_err;             ///< Fatal error interrupt

   /// @name Alert State (FUNC_ALERT_TEST)
   bool m_alert_recov;                ///< Recoverable alert state
   bool m_alert_fatal;                ///< Fatal alert state

   /// @name Synchronization Events
   sc_event cmd_fsm_event;            ///< Triggers command FSM process
   sc_event intr_update_event;        ///< Triggers interrupt output update
   sc_event alert_update_event;       ///< Triggers alert output update (FUNC_ALERT_TEST)

   // =============================================================================
   // SystemC Processes
   // =============================================================================

   /**
    * @brief Reset process - handles asynchronous reset
    *
    * Triggered on rst_ni negative edge. Resets all internal state,
    * uninstantiates all DRBG instances, and resets registers.
    */
   void reset_process();

   /**
    * @brief Interrupt output update process
    *
    * Dedicated process to update interrupt outputs safely from a single thread
    */
   void interrupt_update_process();

   /**
    * @brief Alert output update process (FUNC_ALERT_TEST)
    *
    * Dedicated process to update alert outputs safely from a single thread.
    * Handles ALERT_TEST register writes that trigger recov_alert and fatal_alert.
    */
   void alert_update_process();
   
   /**
    * @brief Command FSM process
    *
    * Processes commands from the command interface. Handles command
    * dispatching, entropy requests, DRBG operations, and state updates.
    */
   void command_fsm_process();

   // =============================================================================
   // FUNC_001-003: Core Random Number Generation
   // =============================================================================

   /**
    * @brief Generates random blocks using CTR_DRBG
    * @param instance_num Instance number (0-2)
    * @param num_blocks Number of 128-bit blocks to generate
    * @return true if generation successful
    */
   bool generate_random_blocks(int instance_num, uint32_t num_blocks);

   /**
    * @brief Requests entropy from entropy source
    * @param seed_buffer Output buffer for 384-bit entropy
    * @param fips_compliant Output FIPS compliance flag
    * @return true if entropy request successful
    */
   bool request_entropy(unsigned char* seed_buffer, bool& fips_compliant);

   /**
    * @brief Checks if reseed is required
    * @param instance_num Instance number (0-2)
    * @return true if reseed required
    */
   bool reseed_required(int instance_num);

   /**
    * @brief Resets reseed counter for instance
    * @param instance_num Instance number (0-2)
    */
   void reset_reseed_counter(int instance_num);

   // =============================================================================
   // FUNC_004-008: Command Processing
   // =============================================================================

   /**
    * @brief Processes INSTANTIATE command
    */
   bool cmd_instantiate(int instance_num, uint8_t flag0, uint8_t clen,
                       const std::array<uint32_t, 12>& additional_data);

   /**
    * @brief Processes GENERATE command
    */
   bool cmd_generate(int instance_num, uint16_t glen);

   /**
    * @brief Processes RESEED command
    */
   bool cmd_reseed(int instance_num, uint8_t flag0, uint8_t clen,
                  const std::array<uint32_t, 12>& additional_data);

   /**
    * @brief Processes UPDATE command
    */
   bool cmd_update(int instance_num, uint8_t clen,
                  const std::array<uint32_t, 12>& additional_data);

   /**
    * @brief Processes UNINSTANTIATE command
    */
   bool cmd_uninstantiate(int instance_num);

   /**
    * @brief Helper to uninstantiate an instance
    */
   void uninstantiate_instance(int instance_num);

   /**
    * @brief Derives V and Key for a DRBG instance from seed material using OpenSSL
    *
    * Builds instance-specific seed material by combining the seed buffer,
    * instance number (for uniqueness), and optional additional data (personalization).
    * Then seeds OpenSSL PRNG and generates V (128-bit) and Key (256-bit) values.
    *
    * @param inst Reference to the DRBG instance to populate V and Key
    * @param instance_num Instance number (0-2), used for uniqueness and error logging
    * @param seed_buffer Pointer to 48-byte (384-bit) seed buffer
    * @param clen Length of additional data in words (0-12)
    * @param additional_data Additional data buffer (personalization/additional input)
    * @return true if V and Key were successfully derived, false on OpenSSL failure
    */
   bool derive_v_and_key(DRBGInstance& inst, int instance_num,
                         const unsigned char* seed_buffer, uint8_t clen,
                         const std::array<uint32_t, 12>& additional_data);

   // =============================================================================
   // FUNC_009: Command FSM and Arbitration
   // =============================================================================

   /**
    * @brief Processes current command
    */
   void process_command();

   /**
    * @brief Executes command asynchronously in spawned thread context
    *
    * This function runs in a dynamically spawned thread, allowing it to
    * call wait() and use quantum keeper synchronization. It is invoked
    * by process_command() using sc_spawn.
    */
   void execute_command_async();

   // =============================================================================
   // FUNC_014-018: Interrupt Management
   // =============================================================================

   /**
    * @brief Updates interrupt outputs based on state and enable
    */
   void update_interrupt_outputs();

   // =============================================================================
   // FUNC_ALERT_TEST: Alert Test Functionality
   // =============================================================================

   /**
    * @brief Updates alert outputs based on ALERT_TEST register writes
    *
    * When ALERT_TEST register is written:
    * - Bit 0 (recov_alert) = 1: Triggers one recoverable alert pulse
    * - Bit 1 (fatal_alert) = 1: Triggers one fatal alert pulse
    */
   void update_alert_outputs();

   /**
    * @brief Trigger recoverable alert when RECOV_ALERT_STS bit is set
    *
    * This method should be called whenever a recoverable alert condition occurs
    * (i.e., when any bit in RECOV_ALERT_STS register is set). It triggers a pulse
    * on the recov_alert_o output to notify the system alert handler.
    */
   void trigger_recov_alert();
   // =============================================================================
   // Helper Methods
   // =============================================================================

   /**
    * @brief Validates multi-bit encoding
    * @param value 4-bit encoded value
    * @param is_enable Output indicating enable-true (0x6) vs disable-true (0x9)
    * @return true if encoding is valid
    */
   bool validate_multi_bit_encoding(uint8_t value, bool& is_enable);

   // =============================================================================
   // Common Command Helpers (shared across cmd_instantiate/reseed/generate/update)
   // =============================================================================

   /**
    * @brief Validates instance number is in valid range (0-2)
    *
    * Sets CMD_INVALID_ACMD status and ERROR FSM state on failure.
    *
    * @param instance_num Instance number to validate
    * @return true if instance number is valid
    */
   bool validate_instance_number(int instance_num);

   /**
    * @brief Checks that a DRBG instance is currently instantiated
    *
    * Sets CMD_INVALID_CMD_SEQ status, raises recoverable alert, and sets
    * ERROR FSM state on failure.
    *
    * @param instance_num Instance number (must be pre-validated 0-2)
    * @return true if instance is instantiated
    */
   bool check_instance_instantiated(int instance_num);

   /**
    * @brief Validates flag0 encoding and corrects to deterministic if invalid
    *
    * Valid values: 0x6 (entropy source) or 0x9 (deterministic).
    * On invalid encoding, raises ACMD_FLAG0_FIELD_ALERT and forces 0x9.
    *
    * @param flag0 Flag0 value (modified in-place if invalid)
    */
   void validate_flag0(uint8_t& flag0);

   /**
    * @brief Prepares seed buffer based on flag0 mode
    *
    * If flag0 == 0x6 (entropy mode): requests entropy from entropy source.
    * If flag0 == 0x9 (deterministic): fills seed buffer with zeros.
    *
    * @param flag0 Entropy mode flag
    * @param seed_buffer Output 48-byte (384-bit) seed buffer
    * @param fips_compliant In/out FIPS compliance flag (updated by entropy source)
    * @return true if seed preparation succeeded
    */
   bool prepare_seed_buffer(uint8_t flag0, unsigned char* seed_buffer, bool& fips_compliant);

   /**
    * @brief Applies FIPS_FORCE register override to compliance flag
    *
    * Reads FIPS_FORCE and CTRL registers to determine if FIPS compliance
    * should be forced for the given instance.
    *
    * @param instance_num Instance number (0-2)
    * @param fips_compliant In/out FIPS compliance flag (set to true if forced)
    */
   void apply_fips_force_override(int instance_num, bool& fips_compliant);

   /**
    * @brief Converts additional data words to byte buffer (little-endian)
    *
    * Zero-initializes the 48-byte output buffer, then converts up to
    * clen words (max 12) from the additional_data array.
    *
    * @param clen Number of additional data words (0-12)
    * @param additional_data Input word array
    * @param add_input Output 48-byte buffer
    */
   void convert_additional_data_to_bytes(uint8_t clen,
                                         const std::array<uint32_t, 12>& additional_data,
                                         unsigned char* add_input);

   /**
    * @brief Seeds OpenSSL PRNG based on flag0 mode, seed buffer, and additional data
    *
    * In entropy mode (0x6): XORs seed with additional data if provided.
    * In deterministic mode (0x9): uses additional data or zero seed.
    *
    * @param flag0 Entropy mode flag
    * @param clen Number of additional data words
    * @param seed_buffer 48-byte seed buffer
    * @param add_input 48-byte additional data buffer
    */
   void seed_prng(uint8_t flag0, uint8_t clen,
                  const unsigned char* seed_buffer, const unsigned char* add_input);

   // =============================================================================
   // FUNC_010-013: Register Callbacks
   // =============================================================================

   /// @name Command Request Interface
   bool handle_write_CMD_REQ(DT value, DT write_mask);

   /// @name Status and Data Output Registers
   // INTR_ENABLE read callback removed - regmodel handles RW registers automatically
   bool handle_read_INTR_TEST(DT& value, DT read_mask);
   bool handle_read_ALERT_TEST(DT& value, DT read_mask);
   bool handle_read_CMD_REQ(DT& value, DT read_mask);
   bool handle_read_SW_CMD_STS(DT& value, DT read_mask);
   bool handle_read_GENBITS_VLD(DT& value, DT read_mask);
   bool handle_read_GENBITS(DT& value, DT read_mask);
   bool handle_read_RESEED_COUNTER_0(DT& value, DT read_mask);
   bool handle_read_RESEED_COUNTER_1(DT& value, DT read_mask);
   bool handle_read_RESEED_COUNTER_2(DT& value, DT read_mask);
   bool handle_read_INT_STATE_VAL(DT& value, DT read_mask);
   bool handle_write_INT_STATE_NUM(DT value, DT write_mask);
   bool handle_read_ERR_CODE(DT& value, DT read_mask);
   bool handle_read_MAIN_SM_STATE(DT& value, DT read_mask);

   /// @name Error Test Register
   bool handle_write_ERR_CODE_TEST(DT value, DT write_mask);
   bool handle_read_ERR_CODE_TEST(DT& value, DT read_mask);

   /// @name Control and Configuration Registers
   bool handle_write_CTRL(DT value, DT write_mask);
   bool handle_write_REGWEN(DT value, DT write_mask);
   bool handle_write_RESEED_INTERVAL(DT value, DT write_mask);
   bool handle_write_FIPS_FORCE(DT value, DT write_mask);
   bool handle_write_INT_STATE_READ_ENABLE(DT value, DT write_mask);
   bool handle_write_INT_STATE_READ_ENABLE_REGWEN(DT value, DT write_mask);

   /// @name Interrupt Management
   bool handle_write_INTR_STATE(DT value, DT write_mask);
   bool handle_write_INTR_ENABLE(DT value, DT write_mask);
   bool handle_write_INTR_TEST(DT value, DT write_mask);
   bool handle_write_ALERT_TEST(DT value, DT write_mask);
   bool handle_write_HW_EXC_STS(DT value, DT write_mask);
   bool handle_write_RECOV_ALERT_STS(DT value, DT write_mask);

   /**
    * @brief Registers all callbacks with regmodel framework
    */
   void register_all_callbacks();
};

