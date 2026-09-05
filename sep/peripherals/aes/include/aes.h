// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aes.h
 * @brief Main AES TLM model class definition and behavioral logic
 * 
 * This header defines the complete AES TLM model including:
 * - State machine enumerations for cipher control and tracking
 * - Port interfaces for clock, reset and sideload key
 * - Hardware registers access and callback handlers
 * - Behavioral methods for AES encryption/decryption using OpenSSL
 */

#pragma once
#include "aes_base.h"
#include "reg_param.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <tlm_utils/simple_target_socket.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>
#include <vector>
#include "reg_logger.h"
#include <openssl/rand.h>

/**
 * @class aes_model
 * @brief SystemC TLM model for the AES hardware peripheral
 * 
 * This class implements the complete behavior of the AES module, including
 * various block cipher modes (ECB, CBC, CFB, OFB, CTR), masking support
 * via key shares, and security hardening features like PRNG reseeding.
 * 
 * It inherits from aes_base which provides the register infrastructure and
 * memory-mapped access via a TLM target socket.
 */
class aes_model : public aes_base
{
public:
   SC_HAS_PROCESS(aes_model);

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
   regmodel::Param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
   /**
    * @brief Constructor for the AES model
    * @param n SystemC module name
    */
   aes_model(sc_module_name n);

   /** @brief Destructor - cleans up OpenSSL contexts and resources */
   ~aes_model();

   // =============================================================================
   // Port Interfaces
   // =============================================================================

   /// @name Clock and Reset
   /// @{
   sc_in<bool> clk_i;             ///< Clock input signal
   sc_in<bool> rst_ni;            ///< Active-low asynchronous reset input
   /// @}

   /// @name Hardware-only Interfaces
   /// @{
   tlm_utils::simple_target_socket<aes_model, 32> keymgr_tl_socket;  ///< TLM target socket for KeyMgr push sideload
   /// @}

   /// @name Alert Signaling
   /// @{
   sc_out<bool> alert_recov_ctrl_update_err;  ///< Recoverable alert: signaled on shadowed register errors
   sc_out<bool> alert_fatal_fault;             ///< Fatal alert: signaled on critical hardware faults
   /// @}

   /// @name Status Outputs
   /// @{
   sc_out<bool> idle_o;           ///< Idle status output (true when AES is not processing)
   sc_in<bool> lc_escalate_en;    ///< Life cycle escalation enable input (triggers fatal alert)
   /// @}

   /// @name regmodel Logger
   /// @{
   RegLogger logger;  ///< Logger instance for diagnostic output
   /// @}

protected:
   // =============================================================================
   // Internal Enumerations
   // =============================================================================

   /**
    * @enum CipherState
    * @brief Internal states for the AES cipher processing state machine
    */
   enum class CipherState {
      IDLE,       ///< Idle - waiting for start condition or register writes
      INIT,       ///< Initializing OpenSSL context and buffers
      ROUND,      ///< Processing cipher rounds (functional delay)
      FINISH,     ///< Finalizing output and updating registers
      CLEARING,   ///< Busy clearing sensitive data (PRNG or zero)
      ERROR       ///< Terminal error state following a fatal fault
   };

   /**
    * @enum AESMode
    * @brief Supported block cipher modes (mapped from CTRL_SHADOWED.MODE)
    */
   enum class AESMode {
      AES_ECB = 0x01, ///< Electronic Codebook Mode
      AES_CBC = 0x02, ///< Cipher Block Chaining Mode
      AES_CFB = 0x04, ///< Cipher Feedback Mode
      AES_OFB = 0x08, ///< Output Feedback Mode
      AES_CTR = 0x10, ///< Counter Mode
      AES_GCM = 0x20, ///< Galois/Counter Mode
      AES_NONE = 0x3F ///< No mode selected/Invalid configuration
   };

   /**
    * @enum GCMPhase
    * @brief GCM phase selector (CTRL_GCM_SHADOWED.PHASE)
    *
    * Sparse one-hot encoding taken from gcm_phase_e in aes_pkg.sv.
    */
   enum class GCMPhase {
      GCM_INIT    = 0x01, ///< Compute the hash subkey and encrypt J0
      GCM_RESTORE = 0x02, ///< Reload a previously saved GHASH state
      GCM_AAD     = 0x04, ///< Absorb additional authenticated data
      GCM_TEXT    = 0x08, ///< Encrypt/decrypt payload and absorb ciphertext
      GCM_SAVE    = 0x10, ///< Export the running GHASH state
      GCM_TAG     = 0x20  ///< Absorb the length block and emit the tag
   };

   /**
    * @enum AESOperation
    * @brief AES operation types
    */
   enum class AESOperation {
      AES_ENC = 0x1,  ///< Encryption operation
      AES_DEC = 0x2   ///< Decryption operation
   };

   /**
    * @enum AESKeyLen
    * @brief AES key length options
    */
   enum class AESKeyLen {
      AES_128 = 0x1,  ///< 128-bit key
      AES_192 = 0x2,  ///< 192-bit key
      AES_256 = 0x4   ///< 256-bit key
   };

   // =============================================================================
   // Internal State Variables
   // =============================================================================

   /// @name Temporal Decoupling
   tlm_utils::tlm_quantumkeeper m_qk; ///< Quantum keeper for Loosely-Timed (LT) synchronization

   /// @name CCI Configuration Parameters
   /// @{
   //cci::cci_param<double> m_clk_freq_hz; ///< AES clock frequency in Hz (default: 100 MHz)
   double m_clk_freq_hz; ///< AES clock frequency in Hz (default: 100 MHz)
   /// @}

   /// @name FSM and Alert Status
   CipherState m_cipher_state;  ///< Current state of the behavioral FSM
   bool m_is_idle;              ///< Tracks whether the model is in a functional idle state
   bool m_in_error_state;       ///< true if a fatal fault has locked the module
   bool m_alert_fatal;          ///< Master fatal alert signal state
   bool m_alert_recoverable;    ///< Master recoverable alert signal state

   /// @name Internal Synchronization Events
   sc_event alert_update_event; ///< Triggers the update_alert_outputs process
   sc_event idle_update_event;  ///< Triggers the update_idle_status process

   /// @name Functional Configuration
   AESMode m_current_mode;           ///< Currently configured block cipher mode
   AESOperation m_current_operation; ///< Currently configured encryption/decryption
   AESKeyLen m_current_key_len;      ///< Currently configured key length
   bool m_sideload_enabled;          ///< true if sideload key path is active
   bool m_manual_operation;          ///< true if software triggers cipher start

   /// @name Data Storage
   std::array<uint32_t, 8> m_key_share0; ///< Internal buffer for Key Share 0
   std::array<uint32_t, 8> m_key_share1; ///< Internal buffer for Key Share 1
   std::array<uint32_t, 4> m_iv;         ///< Internal buffer for Initialization Vector
   std::array<uint32_t, 4> m_data_in;    ///< Internal buffer for Input Plaintext/Ciphertext
   std::array<uint32_t, 4> m_data_out;   ///< Internal buffer for Output Plaintext/Ciphertext
   std::array<uint32_t, 4> m_saved_input_block; ///< Ciphertext buffer for CBC decryption IV updates

   /// @name Configuration Tracking
   uint8_t m_data_in_written_mask;     ///< Bitmask tracking which DATA_IN words were written
   uint8_t m_data_out_read_mask;       ///< Bitmask tracking which DATA_OUT words were read
   uint8_t m_iv_written_mask;          ///< Bitmask tracking which IV words were written
   uint8_t m_key_share0_written_mask;  ///< Bitmask tracking which KEY_SHARE0 words were written
   uint8_t m_key_share1_written_mask;  ///< Bitmask tracking which KEY_SHARE1 words were written
   bool m_iv_configured;               ///< true if IV is valid for the current operation

   /// @name Hardware Handshaking Flags
   bool m_input_ready;   ///< Module is ready to accept new DATA_IN data
   bool m_output_valid;  ///< Valid result data is available in DATA_OUT registers
   bool m_stall;         ///< Back-pressure signal: Output valid but not yet read
   bool m_output_lost;   ///< Error flag: New output generated before previous read

   /// @name Shadow Register Tracking
   bool m_ctrl_shadowed_first_write_pending;     ///< Tracks two-write protocol for CTRL_SHADOWED
   uint32_t m_ctrl_shadowed_shadow_value;        ///< Shadow storage for CTRL_SHADOWED
   bool m_ctrl_aux_shadowed_first_write_pending; ///< Tracks two-write protocol for CTRL_AUX_SHADOWED
   uint32_t m_ctrl_aux_shadowed_shadow_value;    ///< Shadow storage for CTRL_AUX_SHADOWED
   bool m_ctrl_gcm_shadowed_first_write_pending; ///< Tracks two-write protocol for CTRL_GCM_SHADOWED
   uint32_t m_ctrl_gcm_shadowed_shadow_value;    ///< Shadow storage for CTRL_GCM_SHADOWED

   /// @name GCM State
   ///
   /// The hardware keeps no length counters: aes_ghash.sv feeds GCM_TAG from
   /// DATA_IN, so software supplies the length block itself.
   /// @{
   GCMPhase m_gcm_phase;           ///< Committed CTRL_GCM_SHADOWED.PHASE
   uint32_t m_gcm_num_valid_bytes; ///< Committed CTRL_GCM_SHADOWED.NUM_VALID_BYTES (1..16)
   bool m_gcm_init_done;           ///< GCM_INIT has produced the hash subkey and S
   bool m_gcm_first_block;         ///< No block absorbed yet since the last INIT/RESTORE
   std::array<uint8_t, 16> m_gcm_hash_subkey; ///< H = E(K, 0^128)
   std::array<uint8_t, 16> m_gcm_s;           ///< S = E(K, J0); added to form the tag
   std::array<uint8_t, 16> m_gcm_ghash;       ///< Running GHASH accumulator
   /// @}

   /// @name Security Hardening tracking
   uint32_t m_prng_reseed_rate;      ///< Rate at which automatically reseed
   uint32_t m_block_counter;         ///< Number of blocks processed since last reseed
   bool m_key_touch_forces_reseed;   ///< Automatically reseed when key is updated
   bool m_prng_reseed_needed;        ///< Deferred reseed request flag

   /// @name OpenSSL Integration
   EVP_CIPHER_CTX* m_cipher_ctx;     ///< OpenSSL EVP cipher context for cryptographic functions

   /// @name KeyMgr Push Sideload Buffers
   uint32_t m_keymgr_share0[8];  ///< Share0 words pushed by KM (KEY_SHARE0 offsets 0x00-0x1C)
   uint32_t m_keymgr_share1[8];  ///< Share1 words pushed by KM (KEY_SHARE1 offsets 0x20-0x3C)
   bool m_keymgr_key_valid;      ///< Set true when KM writes KEY_CTRL (offset 0x40)

   // =============================================================================
   // SystemC Processes (SC_METHOD/SC_THREAD)
   // =============================================================================
   void reset_process();       ///< Handles asynchronous reset (active-low rst_ni)
   void escalation_monitor(); ///< Monitors life cycle escalation signals
   void update_idle_status();  ///< Updates the idle_o output port status
   void update_alert_outputs(); ///< Updates alert_fatal/recoverable output ports

   // =============================================================================
   // Cipher Behavioral Operations
   // =============================================================================
   /**
    * @brief Marks the block busy at the point an operation is accepted
    *
    * Operations run in spawned processes, which do not start until the calling
    * process yields. Software that writes a trigger and then polls STATUS.IDLE
    * would otherwise still see the block idle and race ahead of the operation,
    * so the status has to drop inside the transaction that accepts it.
    */
   void enter_busy(CipherState state);

   /** @brief Main cipher processing thread; manages FSM and functional delays */
   void perform_cipher_operation();

   /** @brief Evaluates whether auto-start conditions (all data written) are met */
   bool check_auto_start_conditions();

   /** @brief Core encryption/decryption logic calling OpenSSL EVP functions */
   void execute_encryption_decryption();

   /** @brief Calculates simulation delay based on key length and mode */
   sc_time calculate_cipher_delay();

   /** @brief Calculates delay for register clearing operations */
   sc_time calculate_clearing_delay(bool is_key_iv_clear);

   /** @brief Worker method for clearing sensitive key/IV registers */
   void perform_key_iv_data_in_clear();

   /** @brief Worker method for clearing the output registers */
   void perform_data_out_clear();

   /** @brief Asynchronous thread for requesting and applying PRNG reseeds */
   void perform_prng_reseed_async();

   // =============================================================================
   // Helper Methods
   // =============================================================================
   void check_escalation();           ///< Checks LC escalation and transitions to ERROR if enabled
   void trigger_fatal_alert();        ///< Signals fatal fault and locks the module
   void trigger_recoverable_alert();  ///< Signals recoverable error (shadow mismatch)
   void update_status_register();     ///< Refreshes all STATUS register bitfields
   void request_alert_update();       ///< Triggers asynchronous alert port update
   void compute_full_key(std::array<uint8_t, 32>& full_key); ///< XORs key shares into one key
   const EVP_CIPHER* get_openssl_cipher(); ///< Maps internal mode to OpenSSL cipher type
   void increment_ctr_iv();           ///< Increments CTR mode counter (128-bit big-endian)
   void clear_registers_with_prng();  ///< Zeroes/randomizes internal sensitive data
   void request_prng_reseed();        ///< Sets the reseed pending flag
   void perform_prng_reseed();         ///< Blocking reseed operation with OpenSSL interaction
   void check_and_perform_automatic_prng_reseed(); ///< Evaluations auto-reseed logic
   bool load_sideload_key();          ///< Loads key manager sideload key from push buffers
   void keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay); ///< TLM handler for KeyMgr push writes
   uint32_t get_prng_reseed_threshold(); ///< Returns block count limit for current rate

   // =============================================================================
   // Register Write/Read Callback Handlers
   // =============================================================================
   /// @name Register Access Handlers
   /// @{
   bool handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask);
   bool handle_write_KEY_SHARE0(unsigned int index, uint32_t value, uint32_t write_mask);
   bool handle_write_KEY_SHARE1(unsigned int index, uint32_t value, uint32_t write_mask);
   bool handle_write_IV(unsigned int index, uint32_t value, uint32_t write_mask);
   bool handle_read_IV(unsigned int index, uint32_t& value, uint32_t read_mask);
   bool handle_write_DATA_IN(unsigned int index, uint32_t value, uint32_t write_mask);
   bool handle_read_DATA_OUT(unsigned int index, uint32_t& value, uint32_t read_mask);
   bool handle_write_CTRL_SHADOWED(uint32_t value, uint32_t write_mask);
   bool handle_read_CTRL_SHADOWED(uint32_t& value, uint32_t read_mask);
   bool handle_write_CTRL_AUX_SHADOWED(uint32_t value, uint32_t write_mask);
   bool handle_read_CTRL_AUX_SHADOWED(uint32_t& value, uint32_t read_mask);
   bool handle_write_CTRL_AUX_REGWEN(uint32_t value, uint32_t write_mask);
   bool handle_write_TRIGGER(uint32_t value, uint32_t write_mask);
   bool handle_read_STATUS(uint32_t& value, uint32_t read_mask);
   bool handle_write_CTRL_GCM_SHADOWED(uint32_t value, uint32_t write_mask);
   bool handle_read_CTRL_GCM_SHADOWED(uint32_t& value, uint32_t read_mask);
   /// @}

   // =============================================================================
   // GCM Datapath
   // =============================================================================
   /// @name GCM helpers
   /// @{
   /** @brief Multiplies the accumulator by the hash subkey in GF(2^128) */
   void ghash_mul(std::array<uint8_t, 16>& acc) const;

   /** @brief XORs a block into the accumulator and multiplies by H */
   void ghash_absorb(const uint8_t* block, size_t len);

   /** @brief Resolves an attempted PHASE write against the legal transitions */
   uint32_t resolve_gcm_phase(uint32_t requested) const;

   /** @brief Runs one GCM block according to the committed phase */
   void perform_gcm_block();

   /** @brief Derives H and S once the key and IV are available */
   bool ensure_gcm_init();

   /** @brief Encrypts one block with the raw block cipher (no mode chaining) */
   bool aes_encrypt_block(const uint8_t* in, uint8_t* out);
   /// @}

   /** @brief Registers all TL-UL register callbacks with the underlying regmodel model */
   void register_all_callbacks();
};
