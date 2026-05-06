#pragma once

/**
 * @file hmac.h
 * @brief HMAC IP SystemC model implementation
 *
 * This file contains the main HMAC IP SystemC model class that implements
 * HMAC and SHA-2 hash operations using OpenSSL. The model supports SHA-256,
 * SHA-384, and SHA-512 algorithms with configurable key lengths.
 *
 * @note Requires linking with -lssl -lcrypto for OpenSSL support
 */


#include "hmac_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <queue>
#include <vector>
#include <memory>
#include <openssl/evp.h> // OpenSSL library requires linking with -lssl -lcrypto
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @brief HMAC IP SystemC model implementation
 * 
 * This class implements the HMAC IP block with the following features:
 * - SHA-2 hash algorithms (SHA-256, SHA-384, SHA-512)
 * - HMAC mode with configurable key lengths (128, 256, 384, 512, 1024 bits)
 * - Message FIFO for streaming data input
 * - Interrupt and alert signal generation
 * - Register-based configuration and control
 * 
 * The implementation uses OpenSSL for cryptographic operations and SystemC
 * for hardware modeling and timing.
 */
class hmac_ip : public hmac_base
{
public:
   // Interrupt Output Ports
   // HMAC operation completion interrupt output
   sc_out<bool> intr_hmac_done;
   // FIFO empty interrupt output
   sc_out<bool> intr_fifo_empty;
   // HMAC error interrupt output
   sc_out<bool> intr_hmac_err;
   
   // Fatal fault alert output
   sc_out<bool> alert_fatal_fault;
   
    // Functional clock input (frequency in Hz)
   sc_in<double> clk_i;

   // Active-low asynchronous reset input
   sc_in<bool> rst_ni;

      // Key Manager sideload socket (private key bus from key_manager)
   // KM writes KEY_SHARE0 (offsets 0x00-0x1C), KEY_SHARE1 (0x20-0x3C), KEY_CTRL (0x40)
   tlm_utils::simple_target_socket<hmac_ip, 32> keymgr_tl_socket;

   // State machine states for HMAC engine
   enum class State {
      IDLE,        // Engine is idle, ready for new operation
      PROCESSING,  // Engine is processing message data
      FINALIZED    // Engine has completed and finalized the hash
   };

   SC_HAS_PROCESS(hmac_ip);

   // Constructor for HMAC IP model
   // n: SystemC module name
   // log_verbosity: Logger verbosity level (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG, default: 2)
   // memory_size: Size of memory-mapped space in bytes (default: 0x2000)
   // Initializes the HMAC IP model, registers all SystemC processes,
   // sets up register callbacks, and initializes all state variables.
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
   csml_param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
   hmac_ip(sc_module_name n, unsigned int memory_size = 0x2000);

   // Reset handler process
   // SystemC method process that handles reset signal changes.
   // Clears all state, FIFO, and OpenSSL contexts when reset is asserted.
   void reset_handler();
   
   // Update interrupt outputs on reset
   // SystemC method process that updates interrupt outputs when reset
   // changes or when interrupt update event is triggered.
   void update_interrupt_outputs_on_reset();
   
   // Block processing thread
   // SystemC thread that waits for block_processing_event and processes
   // message blocks from the FIFO. This thread handles the long operation
   // with timing waits, allowing register writes to complete immediately.
   void block_processing_thread();
   
   // Digest computation thread
   // SystemC thread that waits for digest_computation_event and computes
   // and writes the digest to registers. This thread handles the long operation
   // with timing waits, allowing register writes to complete immediately.
   void digest_computation_thread();
   
   // Event to trigger block processing thread
   sc_event block_processing_event;
   // Event to trigger digest computation thread
   sc_event digest_computation_event;
   // Event to trigger interrupt output updates
   sc_event interrupt_update_event;

   // Destructor
   // Frees all OpenSSL contexts and resources.
   ~hmac_ip();

private:
   // State Machine Variables
   
   // Current state of the HMAC engine
   State current_state;
   // Flag indicating Process/Stop command has been issued (prevents interrupt during finalization)
   bool process_stop_issued;
   // Flag indicating digest computation is needed (used as backup to event)
   bool digest_computation_needed;
   

   // FIFO Management
   
   // Message FIFO queue storing 32-bit words
   std::queue<uint32_t> msg_fifo;
   // Buffer for accumulating partial words from byte-level writes
   uint32_t packer_buffer;
   // Number of valid bytes currently in packer_buffer
   uint8_t packer_bytes_count;
   // Number of valid bytes in last word from packer flush (used during hash_process)
   unsigned int last_word_valid_bytes;
   // Total message length in bits (64-bit value)
   uint64_t message_length_bits;
   

   // Key Storage

   // Internal key storage (write-only, 32 words for up to 1024-bit keys)
   std::vector<uint32_t> key_storage;

   // Key Manager sideload buffers (populated via keymgr_tl_socket)
   uint32_t m_keymgr_share0[8];  // KEY_SHARE0 words pushed by KM (offsets 0x00-0x1C)
   uint32_t m_keymgr_share1[8];  // KEY_SHARE1 words pushed by KM (offsets 0x20-0x3C)
   bool m_keymgr_key_valid;       // True when KM has committed a valid key (KEY_CTRL bit0=1)
   

   // OpenSSL Cryptographic Contexts
   
   // Hash context for SHA-2 only mode
   EVP_MD_CTX* hash_context;
   // HMAC context for HMAC mode (EVP_MAC API, OpenSSL 3.0+)
   EVP_MAC_CTX* hmac_context;
   // Message digest algorithm selector (SHA-256/384/512)
   const EVP_MD* current_md;
   // HMAC MAC algorithm object
   EVP_MAC* hmac_mac;
   

   // Configuration Parameters
   
   // FIFO depth limit for SHA-256 (in words)
   static constexpr unsigned int MSG_FIFO_DEPTH_SHA256 = 16;
   // FIFO depth limit for SHA-384/512 (in words)
   static constexpr unsigned int MSG_FIFO_DEPTH_SHA384_512 = 32;
   // Number of cycles per SHA-256 block
   static constexpr unsigned int SHA256_BLOCK_CYCLES = 80;
   // Number of cycles per SHA-384/512 block
   static constexpr unsigned int SHA384_512_BLOCK_CYCLES = 96;
   // Extra cycles for HMAC finalization
   static constexpr unsigned int HMAC_EXTRA_CYCLES = 240;

   // Helper Functions
   
   // Get FIFO depth limit based on digest size
   // Returns: FIFO depth limit in words
   unsigned int get_fifo_depth_limit() const;
   
   // Get block size in words based on digest size
   // Returns: Block size in 32-bit words
   unsigned int get_block_size_words() const;
   
   // Get block size in bytes
   // Returns: Block size in bytes
   unsigned int get_block_size_bytes() const;
   
   // Get digest size in words based on digest size configuration
   // Returns: Digest size in 32-bit words
   unsigned int get_digest_size_words() const;
   
   // Get hash algorithm name string
   // Returns: Algorithm name ("SHA-256", "SHA-384", or "SHA-512")
   std::string get_hash_algorithm() const;
   
   // Update interrupt output ports based on register state
   // Computes interrupt outputs as: INTR_STATE & INTR_ENABLE
   void update_interrupt_outputs();
   
   // Update STATUS register based on current engine state
   // Computes status bits for idle, FIFO empty/full, and FIFO depth.
   void update_status_register();
   
   // Process one message block from FIFO
   // Extracts a complete block from the FIFO, updates OpenSSL context,
   // and applies timing annotation.
   void process_message_block();
   
   // Flush packer buffer to FIFO if needed
   // Returns: Number of valid bytes in the last word (0 if packer was empty)
   // If packer_buffer contains accumulated bytes, flushes them to msg_fifo
   // with endian swap applied if configured. Returns the number of valid bytes
   // that were in the packer (used for partial word handling during finalization).
   unsigned int flush_packer_if_needed();
   
   // Process remaining data from FIFO and update OpenSSL context
   // last_word_valid_bytes: Number of valid bytes in the last word from packer flush
   // Extracts all remaining words from the FIFO, handles partial bytes from packer flush,
   // and updates the OpenSSL context (HMAC or hash) with the remaining data.
   void process_remaining_data(unsigned int last_word_valid_bytes);
   
   // Finalize hash/HMAC and write digest to registers
   // Returns: true on success, false on error
   // Finalizes the hash/HMAC operation, writes the digest to registers,
   // applies timing annotation, and sets the completion interrupt.
   bool compute_and_write_digest();
   
   // Apply endian swap to data if configured
   // data: Reference to 32-bit data word to swap
   void apply_endian_swap(uint32_t& data);
   
   // Initialize OpenSSL context for hash or HMAC operation
   // algorithm: Hash algorithm name ("SHA-256", "SHA-384", or "SHA-512")
   // hmac_en: Whether HMAC mode is enabled
   // key_length: Key length configuration value
   // Returns: true on success, false on error
   // Initializes the appropriate OpenSSL context (HMAC or hash) based on
   // the configuration. Sets up the message digest algorithm and prepares
   // the key for HMAC mode.
   bool initialize_openssl_context(const std::string& algorithm, bool hmac_en, uint32_t key_length);
   

   // Register Callback Functions

   // Handle write to INTR_STATE register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_INTR_STATE(uint32_t value, uint32_t write_mask);
   
   // Handle write to INTR_ENABLE register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask);
   
   // Handle write to INTR_TEST register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write does not store)
   bool handle_write_INTR_TEST(uint32_t value, uint32_t write_mask);
   
   // Handle write to ALERT_TEST register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write does not store)
   bool handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask);
   

   // Configuration and Control Callbacks
   
   // Handle read from CFG register
   // value: Reference to return value
   // read_mask: Read mask
   // Returns: true (callback handled the read)
   bool handle_read_CFG(uint32_t& value, uint32_t read_mask);
   
   // Handle write to CFG register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_CFG(uint32_t value, uint32_t write_mask);
   
   // Handle read from CMD register
   // value: Reference to return value (always 0)
   // read_mask: Read mask
   // Returns: true (callback handled the read)
   bool handle_read_CMD(uint32_t& value, uint32_t read_mask);
   
   // Handle write to CMD register
   // value: Write value (command bits)
   // write_mask: Write mask
   // Returns: false (command bits self-clear)
   bool handle_write_CMD(uint32_t value, uint32_t write_mask);
   

   // Security Callback
   
   // Handle write to WIPE_SECRET register
   // value: Wipe value to write to sensitive data
   // write_mask: Write mask
   // Returns: false (write does not store to register)
   bool handle_write_WIPE_SECRET(uint32_t value, uint32_t write_mask);
   

   // Key Register Callback
   
   // Handle write to KEY register
   // index: KEY register index (0-31)
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write does not store to register)
   bool handle_write_KEY(unsigned int index, uint32_t value, uint32_t write_mask);

   // TLM b_transport handler for key manager private key bus socket
   // KM writes share0 (0x00-0x1C), share1 (0x20-0x3C), KEY_CTRL (0x40)
   void keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

   // XOR share0 and share1 into key_storage[0..7]; zeros key_storage[8..31]
   // Returns false if keymgr key is not valid
   bool load_sideload_key();
   

   // Digest Register Callbacks
   
   // Handle read from DIGEST register
   // index: DIGEST register index (0-15)
   // value: Reference to return value
   // read_mask: Read mask
   // Returns: true (callback handled the read)
   bool handle_read_DIGEST(unsigned int index, uint32_t& value, uint32_t read_mask);
   
   // Handle write to DIGEST register
   // index: DIGEST register index (0-15)
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_DIGEST(unsigned int index, uint32_t value, uint32_t write_mask);
   

   // Message Length Callbacks
   
   // Handle write to MSG_LENGTH_LOWER register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_MSG_LENGTH_LOWER(uint32_t value, uint32_t write_mask);
   
   // Handle write to MSG_LENGTH_UPPER register
   // value: Write value
   // write_mask: Write mask
   // Returns: false (write handled in callback)
   bool handle_write_MSG_LENGTH_UPPER(uint32_t value, uint32_t write_mask);
   

   // Message FIFO Callback
   
   // Handle write to MSG_FIFO register with byte enable
   // value: Write value
   // byte_enable: Byte enable mask (4 bits)
   // Returns: false (write does not store to FIFO register)
   // Supports byte-level writes via byte_enable. Accumulates bytes into
   // packer_buffer and pushes complete words to msg_fifo.
   bool handle_write_MSG_FIFO_with_be(uint32_t value, uint8_t byte_enable);
   

   // Status Callback
   
   // Handle read from STATUS register
   // value: Reference to return value
   // read_mask: Read mask
   // Returns: true (callback handled the read completely)
   bool handle_read_STATUS(uint32_t& value, uint32_t read_mask);
   

   // Error Reporting

   // Report an error condition
   // error_code: Error code to set in ERR_CODE register
   // Sets the error code and asserts the hmac_err interrupt.
   void report_error(uint32_t error_code);

   // Logger instance for structured logging
   // mutable allows logging in const member functions
   mutable CsmlLogger logger;
};

