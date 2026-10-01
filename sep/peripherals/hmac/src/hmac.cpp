// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file hmac.cpp
 * @brief Implementation of HMAC IP SystemC model
 * 
 * This file contains the complete implementation of the HMAC IP model,
 * including constructor, destructor, state machine, register callbacks,
 * and cryptographic operations using OpenSSL.
 */

#include "hmac.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <tlm.h>
#include <systemc.h>

/**
 * @brief Constructor for HMAC IP model
 * @param n SystemC module name
 * @param log_verbosity Logger verbosity level (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG)
 * @param memory_size Size of memory-mapped space in bytes
 *
 * Initializes the HMAC IP model with the following steps:
 * - Initialize base class with memory size
 * - Initialize all SystemC ports
 * - Initialize state variables and OpenSSL contexts
 * - Register SystemC processes for reset and interrupt handling
 * - Register all register read/write callbacks
 * - Initialize registers to reset values
 */
hmac_ip::hmac_ip(sc_module_name n, unsigned int memory_size)
   : hmac_base(n, memory_size),
     intr_hmac_done("intr_hmac_done"),
     intr_fifo_empty("intr_fifo_empty"),
     intr_hmac_err("intr_hmac_err"),
     alert_fatal_fault("alert_fatal_fault"),
     clk_i("clk_i"),
     rst_ni("rst_ni"),
     keymgr_tl_socket("keymgr_tl_socket"),
     verbosity("verbosity", REG_DEFAULT_VERBOSITY),
     current_state(State::IDLE),
     process_stop_issued(false),
     msg_allowed(false),
     fifo_full_seen(false),
     packer_buffer(0),
     packer_bytes_count(0),
     message_length_bits(0),
     key_storage(32, 0),
     m_keymgr_key_valid(false)
{
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Initializing HMAC IP with memory_size=" << memory_size << std::endl;

   // Initialize key manager sideload buffers
   std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
   std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));

   // Register key manager private bus TLM handler
   keymgr_tl_socket.register_b_transport(this, &hmac_ip::keymgr_b_transport);

   // Register reset handler process - triggered on rst_ni changes
   SC_METHOD(reset_handler);
   sensitive << rst_ni;
   dont_initialize();  // Will be triggered when rst_ni signal changes

   // Register separate process to update interrupt outputs
   // It's sensitive to both reset changes and an event triggered by register writes
   SC_METHOD(update_interrupt_outputs_on_reset);
   sensitive << rst_ni << interrupt_update_event;
   dont_initialize();

   // Register block processing thread
   // This thread waits for block_processing_event and processes message blocks
   // with timing waits, allowing register writes to complete immediately.
   SC_THREAD(block_processing_thread);
   sensitive << block_processing_event;

   // Register digest computation thread
   // This thread waits for digest_computation_event and computes and writes 
   // digest to registers with timing waits, allowing register writes to complete immediately.
   SC_THREAD(digest_computation_thread);
   sensitive << digest_computation_event;

   // Register Callback Registration
   
   // Interrupt control callbacks
   std::function<bool(uint32_t)> intr_state_write = [this](uint32_t value) {
      return this->handle_write_INTR_STATE(value, INTR_STATE.write_bit_mask);
   };
   memory.register_write_callback(intr_state_write, INTR_STATE.offset);

   std::function<bool(uint32_t)> intr_enable_write = [this](uint32_t value) {
      return this->handle_write_INTR_ENABLE(value, INTR_ENABLE.write_bit_mask);
   };
   memory.register_write_callback(intr_enable_write, INTR_ENABLE.offset);

   std::function<bool(uint32_t)> intr_test_write = [this](uint32_t value) {
      return this->handle_write_INTR_TEST(value, INTR_TEST.write_bit_mask);
   };
   memory.register_write_callback(intr_test_write, INTR_TEST.offset);

   // Alert test callbacks
   std::function<bool(uint32_t)> alert_test_write = [this](uint32_t value) {
      return this->handle_write_ALERT_TEST(value, ALERT_TEST.write_bit_mask);
   };
   memory.register_write_callback(alert_test_write, ALERT_TEST.offset);

   // Configuration callbacks
   std::function<bool(uint32_t&)> cfg_read = [this](uint32_t& value) {
      return this->handle_read_CFG(value, CFG.read_bit_mask);
   };
   memory.register_read_callback(cfg_read, CFG.offset);

   std::function<bool(uint32_t)> cfg_write = [this](uint32_t value) {
      return this->handle_write_CFG(value, CFG.write_bit_mask);
   };
   memory.register_write_callback(cfg_write, CFG.offset);

   // Command read callback
   std::function<bool(uint32_t&)> cmd_read = [this](uint32_t& value) {
      return this->handle_read_CMD(value, CMD.read_bit_mask);
   };
   memory.register_read_callback(cmd_read, CMD.offset);

   // Command write callback
   std::function<bool(uint32_t)> cmd_write = [this](uint32_t value) {
      return this->handle_write_CMD(value, CMD.write_bit_mask);
   };
   memory.register_write_callback(cmd_write, CMD.offset);

   // Security callback
   std::function<bool(uint32_t)> wipe_secret_write = [this](uint32_t value) {
      return this->handle_write_WIPE_SECRET(value, WIPE_SECRET.write_bit_mask);
   };
   memory.register_write_callback(wipe_secret_write, WIPE_SECRET.offset);

   // KEY register callbacks (32 registers)
   for (unsigned int i = 0; i < 32; i++) {
      std::function<bool(uint32_t)> key_write = [this, i](uint32_t value) {
         return this->handle_write_KEY(i, value, KEY[i].write_bit_mask);
      };
      memory.register_write_callback(key_write, KEY[i].offset);
   }

   // DIGEST register callbacks (16 read + 16 write)
   for (unsigned int i = 0; i < 16; i++) {
      std::function<bool(uint32_t&)> digest_read = [this, i](uint32_t& value) {
         return this->handle_read_DIGEST(i, value, DIGEST[i].read_bit_mask);
      };
      memory.register_read_callback(digest_read, DIGEST[i].offset);

      std::function<bool(uint32_t)> digest_write = [this, i](uint32_t value) {
         return this->handle_write_DIGEST(i, value, DIGEST[i].write_bit_mask);
      };
      memory.register_write_callback(digest_write, DIGEST[i].offset);
   }

   // Message length callbacks
   std::function<bool(uint32_t)> msg_len_lower_write = [this](uint32_t value) {
      return this->handle_write_MSG_LENGTH_LOWER(value, MSG_LENGTH_LOWER.write_bit_mask);
   };
   memory.register_write_callback(msg_len_lower_write, MSG_LENGTH_LOWER.offset);

   std::function<bool(uint32_t)> msg_len_upper_write = [this](uint32_t value) {
      return this->handle_write_MSG_LENGTH_UPPER(value, MSG_LENGTH_UPPER.write_bit_mask);
   };
   memory.register_write_callback(msg_len_upper_write, MSG_LENGTH_UPPER.offset);

   // MSG_FIFO callback - use byte enable to determine write size and position
   // Register callbacks for all MSG_FIFO offsets using new callback with byte enable
   std::function<bool(uint32_t, uint8_t)> msg_fifo_write_be = [this](uint32_t value, uint8_t byte_enable) {
      return this->handle_write_MSG_FIFO_with_be(value, byte_enable);
   };
   for (unsigned int i = 0; i < 1024; i++) {
      memory.register_write_callback_with_be(msg_fifo_write_be, MSG_FIFO[i].offset);
   }

   // STATUS read callback
   std::function<bool(uint32_t&)> status_read = [this](uint32_t& value) {
      return this->handle_read_STATUS(value, STATUS.read_bit_mask);
   };
   memory.register_read_callback(status_read, STATUS.offset);

   // Initialize all registers to their reset values
   reset_all_registers();
   REG_INFO(1, logger) << "Registers initialized to reset values" << std::endl;

   // STATUS is a computed register - explicitly initialize to reset value
   // Initial state: IDLE (hmac_idle=1) + FIFO empty (fifo_empty=1) = 0x3
   STATUS = 0x3;

   // Initialize interrupt and alert output ports to inactive state
   // Note: These are sc_out ports and need to have initial values set
   intr_hmac_done.initialize(false);
   intr_fifo_empty.initialize(false);
   intr_hmac_err.initialize(false);
   alert_fatal_fault.initialize(false);

   // Initialize logger with verbosity from constructor parameter
   logger.setMaxVerbosity(verbosity.get_param_value());
   logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
   logger.setFunctionTrace(false);

   REG_INFO(1, logger) << "Interrupt and alert ports initialized" << std::endl;
   REG_INFO(1, logger) << "HMAC IP initialization complete (log_verbosity=" << verbosity.get_param_value() << ")" << std::endl;
}

/**
 * @brief Reset handler process
 * 
 * SystemC method process that handles reset signal changes. When reset is
 * asserted (rst_ni is false), this function:
 * - Resets state machine to IDLE
 * - Clears FIFO and packer state
 * - Clears key storage
 * - Frees and resets OpenSSL contexts
 * - Resets all registers
 */
void hmac_ip::reset_handler() {
   REG_FUNC_TRACE(logger);

   if (!rst_ni.read()) {
      REG_INFO(1, logger) << "Reset asserted - resetting HMAC IP" << std::endl;

      // Active-low reset handling
      current_state = State::IDLE;
      process_stop_issued = false;
      msg_allowed = false;
      fifo_full_seen = false;
      packer_buffer = 0;
      packer_bytes_count = 0;
      message_length_bits = 0;
      REG_DEBUG(2, logger) << "State machine and packer reset to initial values" << std::endl;

      // Clear FIFO
      size_t fifo_size = msg_fifo.size();
      while (!msg_fifo.empty()) {
         msg_fifo.pop();
      }
      REG_DEBUG(2, logger) << "Cleared FIFO (had " << fifo_size << " entries)" << std::endl;

      // Clear key storage (SW path and sideload path)
      std::fill(key_storage.begin(), key_storage.end(), 0);
      std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
      std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));
      m_keymgr_key_valid = false;
      REG_DEBUG(2, logger) << "Key storage and sideload buffers cleared" << std::endl;

      // Reset the hash engine and any in-flight message state
      m_sha.init(sha2_mode::sha256);
      m_hmac_key.clear();
      m_final_tail.clear();
      std::fill(std::begin(m_digest_restore_valid),
                std::end(m_digest_restore_valid), false);
      REG_DEBUG(2, logger) << "Hash engine reset" << std::endl;

      // Reset all registers
      // Note: Interrupts handled via INTR_STATE register reset
      reset_all_registers();
      REG_INFO(1, logger) << "All registers reset complete" << std::endl;

      // Note: Interrupt outputs will be updated by update_interrupt_outputs_on_reset()
   }
}

/**
 * @brief Update interrupt outputs on reset
 * 
 * SystemC method process that updates interrupt outputs when reset changes
 * or when interrupt update event is triggered.
 */
void hmac_ip::update_interrupt_outputs_on_reset() {
   REG_FUNC_TRACE(logger);
   // Update interrupt outputs based on current register values and rst_ni
   update_interrupt_outputs();
}

/**
 * @brief Block processing thread
 * 
 * SystemC thread that waits for block_processing_event and processes message
 * blocks from the FIFO. This thread handles the long operation with timing
 * waits, allowing register writes to complete immediately.
 * 
 * The thread processes blocks in two scenarios:
 * 1. When FIFO is full: processes blocks until FIFO is below the limit
 * 2. When a block is ready: processes one complete block
 */
void hmac_ip::block_processing_thread() {
   REG_FUNC_TRACE(logger);

   while (true) {
      // Wait for event to trigger block processing
      wait(block_processing_event);
      REG_DEBUG(2, logger) << "Block processing event triggered" << std::endl;

      // Process blocks while there are complete blocks ready
      while (current_state == State::PROCESSING &&
             msg_fifo.size() >= get_block_size_words()) {
         REG_DEBUG(2, logger) << "Processing message block (FIFO size=" << msg_fifo.size() << ")" << std::endl;
         process_message_block();
      }
   }
}

/**
 * @brief Digest computation thread
 * 
 * SystemC thread that waits for digest_computation_event and computes and writes
 * the digest to registers. This thread handles the long operation with timing
 * waits, allowing register writes to complete immediately.
 */
void hmac_ip::digest_computation_thread() {
   REG_FUNC_TRACE(logger);

   while (true) {
      // Wait for event to trigger digest computation
      wait(digest_computation_event);
      REG_INFO(1, logger) << "Digest computation event triggered" << std::endl;
      compute_and_write_digest();
   }

}


/**
 * @name Helper Functions
 * @{
 */

/**
 * @brief Get FIFO depth limit based on digest size configuration
 * @return FIFO depth limit in words
 * 
 * SHA-256 uses a smaller FIFO depth (16 words) compared to
 * SHA-384/512 (32 words).
 */
unsigned int hmac_ip::get_fifo_depth_limit() const {
   REG_FUNC_TRACE(logger);
   uint32_t digest_size = CFG.digest_size;
   unsigned int depth = (digest_size == 0x1) ? MSG_FIFO_DEPTH_SHA256 : MSG_FIFO_DEPTH_SHA384_512;
   REG_DEBUG(2, logger) << "FIFO depth limit=" << depth << " for digest_size=" << digest_size << std::endl;
   return depth;
}

/**
 * @brief Get block size in words based on digest size configuration
 * @return Block size in 32-bit words
 * 
 * SHA-256 uses 512-bit blocks (16 words), while SHA-384/512 use
 * 1024-bit blocks (32 words).
 */
unsigned int hmac_ip::get_block_size_words() const {
   REG_FUNC_TRACE(logger);
   uint32_t digest_size = CFG.digest_size;
   unsigned int block_size = (digest_size == 0x1) ? 16 : 32;
   REG_DEBUG(2, logger) << "Block size=" << block_size << " words for digest_size=" << digest_size << std::endl;
   return block_size;
}

/**
 * @brief Get block size in bytes
 * @return Block size in bytes
 */
unsigned int hmac_ip::get_block_size_bytes() const {
   REG_FUNC_TRACE(logger);
   return get_block_size_words() * 4;
}

/**
 * @brief Get digest size in words based on digest size configuration
 * @return Digest size in 32-bit words (8 for SHA-256, 12 for SHA-384, 16 for SHA-512)
 */
unsigned int hmac_ip::get_digest_size_words() const {
   REG_FUNC_TRACE(logger);
   uint32_t digest_size = CFG.digest_size;
   unsigned int words = 0;
   if (digest_size == 0x1) {
      words = 8;   // SHA-256
      REG_DEBUG(2, logger) << "Digest size=8 words (SHA-256)" << std::endl;
   } else if (digest_size == 0x2) {
      words = 12;  // SHA-384
      REG_DEBUG(2, logger) << "Digest size=12 words (SHA-384)" << std::endl;
   } else if (digest_size == 0x4) {
      words = 16;  // SHA-512
      REG_DEBUG(2, logger) << "Digest size=16 words (SHA-512)" << std::endl;
   } else {
      REG_DEBUG(2, logger) << "Invalid digest size configuration: " << digest_size << std::endl;
   }
   return words;
}

/**
 * @brief Get hash algorithm name string
 * @return Algorithm name ("SHA-256", "SHA-384", or "SHA-512")
 */
std::string hmac_ip::get_hash_algorithm() const {
   REG_FUNC_TRACE(logger);
   uint32_t digest_size = CFG.digest_size;
   std::string algorithm;
   if (digest_size == 0x1) {
      algorithm = "SHA-256";
      REG_DEBUG(2, logger) << "Hash algorithm: SHA-256" << std::endl;
   } else if (digest_size == 0x2) {
      algorithm = "SHA-384";
      REG_DEBUG(2, logger) << "Hash algorithm: SHA-384" << std::endl;
   } else if (digest_size == 0x4) {
      algorithm = "SHA-512";
      REG_DEBUG(2, logger) << "Hash algorithm: SHA-512" << std::endl;
   } else {
      algorithm = "";
      REG_DEBUG(2, logger) << "Invalid digest size configuration: " << digest_size << std::endl;
   }
   return algorithm;
}

/**
 * @brief Update interrupt output ports based on register state
 * 
 * Computes interrupt outputs as: INTR_STATE & INTR_ENABLE.
 * Only enabled interrupts that are set in INTR_STATE will assert
 * the corresponding output port.
 */
void hmac_ip::update_interrupt_outputs() {
   REG_FUNC_TRACE(logger);

   // Output = INTR_STATE & INTR_ENABLE
   bool hmac_done = (INTR_STATE.hmac_done & INTR_ENABLE.hmac_done);
   bool fifo_empty = (INTR_STATE.fifo_empty & INTR_ENABLE.fifo_empty);
   bool hmac_err = (INTR_STATE.hmac_err & INTR_ENABLE.hmac_err);

   REG_DEBUG(2, logger) << "Updating interrupts: done=" << hmac_done << " fifo_empty=" << fifo_empty << " err=" << hmac_err << std::endl;

   intr_hmac_done.write(hmac_done);
   intr_fifo_empty.write(fifo_empty);
   intr_hmac_err.write(hmac_err);
}

/**
 * @brief Update STATUS register based on current engine state
 * 
 * Computes status bits for:
 * - hmac_idle: Engine is in IDLE state
 * - fifo_empty: Message FIFO is empty
 * - fifo_full: Message FIFO is at capacity
 * - fifo_depth: Current FIFO depth in words (6 bits)
 */
void hmac_ip::update_status_register() {
   REG_FUNC_TRACE(logger);

   // Compute STATUS register value based on current state
   uint32_t status_value = 0;

   // Bit 0: hmac_idle
   if (current_state == State::IDLE) {
      status_value |= (1 << 0);
      REG_DEBUG(2, logger) << "HMAC is IDLE" << std::endl;
   }

   // Bit 1: fifo_empty
   if (msg_fifo.empty()) {
      status_value |= (1 << 1);
      REG_DEBUG(2, logger) << "FIFO is empty" << std::endl;
   }

   // Bit 2: fifo_full
   if (msg_fifo.size() >= get_fifo_depth_limit()) {
      status_value |= (1 << 2);
      REG_DEBUG(2, logger) << "FIFO is full" << std::endl;
   }

   // Bits 4-9: fifo_depth (6 bits)
   status_value |= ((static_cast<uint32_t>(msg_fifo.size()) & 0x3F) << 4);

   REG_DEBUG(2, logger) << "STATUS register updated: 0x" << std::hex << status_value << std::dec << " (FIFO depth=" << msg_fifo.size() << ")" << std::endl;

   // Directly assign computed value to STATUS register
   STATUS = status_value;
}

/**
 * @brief Apply endian swap to data if configured
 * @param data Reference to 32-bit data word to swap
 * 
 * If CFG.endian_swap is set, swaps byte order of the data word.
 */
void hmac_ip::apply_endian_swap(uint32_t& data) {
   REG_FUNC_TRACE(logger);

   if (CFG.endian_swap) {
      uint32_t original = data;
      data = ((data & 0xFF) << 24) | ((data & 0xFF00) << 8) |
             ((data & 0xFF0000) >> 8) | ((data & 0xFF000000) >> 24);
      REG_DEBUG(2, logger) << "Endian swap: 0x" << std::hex << original << " -> 0x" << data << std::dec << std::endl;
   }
}

/**
 * @brief Map the CFG digest_size encoding onto a hash engine mode
 */
bool hmac_ip::sha_mode_from_cfg(uint32_t digest_size, sha2_mode& mode) const {
   switch (digest_size) {
      case 0x1: mode = sha2_mode::sha256; return true;
      case 0x2: mode = sha2_mode::sha384; return true;
      case 0x4: mode = sha2_mode::sha512; return true;
      default:  return false;
   }
}

/**
 * @brief Build one HMAC key block, XORed with the given pad byte
 *
 * Follows FIPS 198-1: a key shorter than the block is zero-extended, and a key
 * longer than the block is replaced by its own hash. The hardware only accepts
 * key lengths up to the block size for a given digest -- longer combinations are
 * rejected as SwInvalidConfig -- so the pre-hash path is defensive only.
 */
std::vector<uint8_t> hmac_ip::key_block(uint8_t pad_byte) const {
   const size_t block = m_sha.block_bytes();
   std::vector<uint8_t> out(block, pad_byte);

   std::vector<uint8_t> key = m_hmac_key;
   if (key.size() > block) {
      sha2_engine pre;
      pre.init(m_sha.mode());
      std::vector<uint8_t> hashed(pre.digest_bytes());
      size_t full = (key.size() / block) * block;
      for (size_t o = 0; o < full; o += block) pre.compress(key.data() + o);
      pre.finalize(key.data() + full, key.size() - full, hashed.data());
      key = hashed;
   }

   for (size_t i = 0; i < key.size(); i++) {
      out[i] = static_cast<uint8_t>(key[i] ^ pad_byte);
   }
   return out;
}

/**
 * @brief Start a fresh hash operation
 * @param algorithm Hash algorithm name ("SHA-256", "SHA-384", or "SHA-512")
 * @param hmac_en Whether HMAC mode is enabled
 * @param key_length Key length configuration value
 * @return true on success, false on error
 *
 * Resets the hash engine to the initial chaining values for the selected
 * digest. In HMAC mode the K^ipad block is absorbed here, so on return the
 * engine sits exactly at the start of the message, which is also what makes the
 * absorbed bit count differ from MSG_LENGTH by one block.
 */
bool hmac_ip::initialize_hash_context(const std::string& algorithm, bool hmac_en, uint32_t key_length) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Initializing hash context: algorithm=" << algorithm
                        << " hmac_en=" << hmac_en
                        << " key_length=0x" << std::hex << key_length << std::dec << std::endl;

   m_final_tail.clear();
   m_hmac_key.clear();

   sha2_mode mode;
   if (algorithm == "SHA-256") {
      mode = sha2_mode::sha256;
   } else if (algorithm == "SHA-384") {
      mode = sha2_mode::sha384;
   } else if (algorithm == "SHA-512") {
      mode = sha2_mode::sha512;
   } else {
      REG_ERROR(0, logger) << "Error: Unsupported hash algorithm" << std::endl;
      report_error(0x6);
      return false;
   }
   REG_DEBUG(2, logger) << "Selected " << algorithm << " algorithm" << std::endl;

   m_sha.init(mode);

   if (hmac_en) {
      REG_INFO(1, logger) << "Initializing HMAC mode" << std::endl;

      unsigned int key_words = 0;
      switch (key_length) {
         case 0x01: key_words = 4; REG_DEBUG(2, logger) << "Key length: 128-bit" << std::endl; break;
         case 0x02: key_words = 8; REG_DEBUG(2, logger) << "Key length: 256-bit" << std::endl; break;
         case 0x04: key_words = 12; REG_DEBUG(2, logger) << "Key length: 384-bit" << std::endl; break;
         case 0x08: key_words = 16; REG_DEBUG(2, logger) << "Key length: 512-bit" << std::endl; break;
         case 0x10: key_words = 32; REG_DEBUG(2, logger) << "Key length: 1024-bit" << std::endl; break;
      }

      m_hmac_key.reserve(static_cast<size_t>(key_words) * 4u);
      for (unsigned int i = 0; i < key_words; i++) {
         uint32_t word = effective_key_word(i);
         m_hmac_key.push_back((word >> 24) & 0xFF);
         m_hmac_key.push_back((word >> 16) & 0xFF);
         m_hmac_key.push_back((word >> 8) & 0xFF);
         m_hmac_key.push_back(word & 0xFF);
      }

      // Absorb K^ipad. Hardware does the same, which is why its internal SHA
      // length runs one block ahead of the MSG_LENGTH software sees.
      const std::vector<uint8_t> ipad = key_block(0x36);
      m_sha.compress(ipad.data());
      REG_INFO(1, logger) << "HMAC initialization successful (K^ipad absorbed)" << std::endl;
   } else {
      REG_INFO(1, logger) << "Initializing SHA-2 hash-only mode" << std::endl;
   }

   REG_INFO(1, logger) << "Hash context initialization complete" << std::endl;
   return true;
}

/**
 * @brief Write the engine's chaining state into the DIGEST registers
 *
 * Uses the same word mapping hardware applies to a completed digest: SHA-256
 * puts one 32-bit chaining word per register and mirrors the result into the
 * upper half, while SHA-384/512 split each 64-bit word high-then-low across a
 * register pair.
 */
void hmac_ip::export_state_to_digest() {
   REG_FUNC_TRACE(logger);

   if (m_sha.mode() == sha2_mode::sha256) {
      for (unsigned int i = 0; i < 8; i++) {
         DIGEST[i] = static_cast<uint32_t>(m_sha.chain(i));
         DIGEST[i + 8] = DIGEST[i];
      }
   } else {
      for (unsigned int i = 0; i < 8; i++) {
         const uint64_t word = m_sha.chain(i);
         DIGEST[i * 2]     = static_cast<uint32_t>(word >> 32);
         DIGEST[i * 2 + 1] = static_cast<uint32_t>(word & 0xFFFFFFFFull);
      }
   }

   REG_DEBUG(2, logger) << "Exported chaining state to DIGEST registers" << std::endl;
}

/**
 * @brief Reload the engine's chaining state from the DIGEST registers
 *
 * The inverse of export_state_to_digest(). The absorbed bit count is rebuilt
 * from MSG_LENGTH, which counts only the message bits software has written; in
 * HMAC mode the K^ipad block is added back, since it is absorbed by the engine
 * but never reflected in MSG_LENGTH.
 */
void hmac_ip::import_state_from_digest() {
   REG_FUNC_TRACE(logger);

   sha2_mode mode;
   if (!sha_mode_from_cfg(CFG.digest_size, mode)) {
      REG_ERROR(0, logger) << "Cannot restore context: unsupported digest_size" << std::endl;
      return;
   }

   auto digest_word = [this](unsigned int i) -> uint32_t {
      return m_digest_restore_valid[i] ? m_digest_restore[i]
                                       : static_cast<uint32_t>(DIGEST[i]);
   };

   m_sha.init(mode);

   if (mode == sha2_mode::sha256) {
      for (unsigned int i = 0; i < 8; i++) {
         m_sha.set_chain(i, digest_word(i));
      }
   } else {
      for (unsigned int i = 0; i < 8; i++) {
         const uint64_t hi = digest_word(i * 2);
         const uint64_t lo = digest_word(i * 2 + 1);
         m_sha.set_chain(i, (hi << 32) | lo);
      }
   }

   std::fill(std::begin(m_digest_restore_valid),
             std::end(m_digest_restore_valid), false);

   uint64_t bits = (static_cast<uint64_t>(MSG_LENGTH_UPPER) << 32) | MSG_LENGTH_LOWER;
   if (CFG.hmac_en) {
      bits += static_cast<uint64_t>(m_sha.block_bytes()) * 8u;
   }
   m_sha.set_absorbed_bits(bits);
   message_length_bits = (static_cast<uint64_t>(MSG_LENGTH_UPPER) << 32) | MSG_LENGTH_LOWER;

   // The key is needed again to close out the HMAC at finalization.
   m_hmac_key.clear();
   if (CFG.hmac_en) {
      unsigned int key_words = 0;
      switch (CFG.key_length) {
         case 0x01: key_words = 4; break;
         case 0x02: key_words = 8; break;
         case 0x04: key_words = 12; break;
         case 0x08: key_words = 16; break;
         case 0x10: key_words = 32; break;
      }
      for (unsigned int i = 0; i < key_words; i++) {
         uint32_t word = effective_key_word(i);
         m_hmac_key.push_back((word >> 24) & 0xFF);
         m_hmac_key.push_back((word >> 16) & 0xFF);
         m_hmac_key.push_back((word >> 8) & 0xFF);
         m_hmac_key.push_back(word & 0xFF);
      }
   }

   m_final_tail.clear();

   REG_INFO(1, logger) << "Restored chaining state from DIGEST, absorbed_bits=" << bits << std::endl;
}

/**
 * @brief Process one message block from FIFO
 * 
 * Extracts a complete block from the FIFO, converts to bytes (big-endian),
 * updates the OpenSSL context (HMAC or hash), applies timing annotation,
 * and updates status. Also checks for FIFO empty interrupt condition.
 */
void hmac_ip::process_message_block() {
   REG_FUNC_TRACE(logger);

   unsigned int block_size = get_block_size_words();
   REG_DEBUG(2, logger) << "Processing message block: block_size=" << block_size << " words, FIFO size=" << msg_fifo.size() << std::endl;

   if (msg_fifo.size() >= block_size) {
      std::vector<uint8_t> block_data;
      block_data.reserve(get_block_size_bytes());

      // Extract one block from FIFO
      for (unsigned int i = 0; i < block_size; i++) {
         uint32_t word = msg_fifo.front();
         msg_fifo.pop();

         // Convert to bytes (big-endian for SHA-2)
         block_data.push_back((word >> 24) & 0xFF);
         block_data.push_back((word >> 16) & 0xFF);
         block_data.push_back((word >> 8) & 0xFF);
         block_data.push_back(word & 0xFF);
      }
      REG_DEBUG(3, logger) << "Extracted " << block_data.size() << " bytes from FIFO" << std::endl;

      m_sha.compress(block_data.data());
      REG_DEBUG(2, logger) << "Absorbed block into hash engine" << std::endl;

      // Apply timing annotation
      double clock_freq = clk_i.read();
      unsigned int cycles = (CFG.digest_size == 0x1) ? SHA256_BLOCK_CYCLES : SHA384_512_BLOCK_CYCLES;
      double delay_ns = (cycles / clock_freq) * 1e9;
      REG_DEBUG(2, logger) << "Applying timing delay: " << cycles << " cycles (" << delay_ns << " ns)" << std::endl;
      wait(delay_ns, SC_NS);

      // Update status
      update_status_register();

      // Raise fifo_empty only once per fill cycle. Hardware gates the interrupt
      // on software being allowed to write the FIFO and on the FIFO having been
      // full beforehand; without the latter the engine, which drains faster
      // than software can fill, would assert it on essentially every block.
      // STATUS.fifo_empty is the ungated value and is not affected by this.
      if (msg_fifo.empty() && msg_allowed && fifo_full_seen &&
          current_state == State::PROCESSING && !process_stop_issued) {
         REG_INFO(1, logger) << "FIFO empty after being full - setting fifo_empty interrupt" << std::endl;
         INTR_STATE.fifo_empty = 1;
         interrupt_update_event.notify(SC_ZERO_TIME);
      }
      REG_INFO(1, logger) << "Message block processed successfully" << std::endl;
   }
}

/**
 * @brief Push one word into the message FIFO
 * @param word Word to enqueue, already endian-swapped if configured
 *
 * Hardware stalls the bus while the FIFO is full and releases the write once
 * the engine consumes an entry. Hashing is untimed here, so the equivalent is
 * to drain a block before enqueuing rather than to block the transaction.
 *
 * Also maintains fifo_full_seen, which gates the fifo_empty interrupt: it is
 * set when the FIFO fills and cleared as soon as software starts refilling an
 * empty FIFO, so the interrupt fires once per fill cycle instead of on every
 * drain.
 */
void hmac_ip::push_fifo_word(uint32_t word) {
   REG_FUNC_TRACE(logger);

   const size_t depth_limit = get_fifo_depth_limit();

   if (msg_fifo.size() >= depth_limit) {
      REG_DEBUG(2, logger) << "FIFO full (" << msg_fifo.size()
                            << ") - draining a block before accepting write" << std::endl;
      process_message_block();
   }

   const bool was_empty = msg_fifo.empty();

   msg_fifo.push(word);

   if (was_empty) {
      // Software is refilling an empty FIFO, so the previous full episode has
      // been serviced and must not re-arm the interrupt.
      fifo_full_seen = false;
   }

   if (msg_fifo.size() >= depth_limit) {
      fifo_full_seen = true;
      REG_DEBUG(2, logger) << "FIFO reached full depth - arming fifo_empty interrupt" << std::endl;
   }

   REG_DEBUG(3, logger) << "Pushed word to FIFO: 0x" << std::hex << word << std::dec
                         << " (FIFO size now=" << msg_fifo.size() << ")" << std::endl;
}

/**
 * @brief Flush packer buffer to FIFO if needed
 * @return Number of valid bytes in the last word (0 if packer was empty)
 * 
 * If packer_buffer contains accumulated bytes, flushes them to msg_fifo
 * with endian swap applied if configured. Returns the number of valid bytes
 * that were in the packer (used for partial word handling during finalization).
 */
unsigned int hmac_ip::flush_packer_if_needed() {
   REG_FUNC_TRACE(logger);

   unsigned int last_word_valid_bytes = 0;  // Track valid bytes in last word from packer flush
   if (packer_bytes_count > 0) {
      REG_DEBUG(2, logger) << "Flushing packer buffer: " << static_cast<int>(packer_bytes_count) << " bytes, buffer=0x" << std::hex << packer_buffer << std::dec << std::endl;

      // Apply endian swap if configured (before pushing, same as normal accumulation)
      uint32_t word_to_push = packer_buffer;
      apply_endian_swap(word_to_push);
      push_fifo_word(word_to_push);
      last_word_valid_bytes = packer_bytes_count;  // Remember how many bytes are valid
      packer_bytes_count = 0;
      packer_buffer = 0;

      REG_INFO(1, logger) << "Packer flushed: " << last_word_valid_bytes << " valid bytes pushed to FIFO" << std::endl;
   } else {
      REG_DEBUG(2, logger) << "Packer buffer is empty, nothing to flush" << std::endl;
   }
   return last_word_valid_bytes;
}

/**
 * @brief Process remaining data from FIFO and update OpenSSL context
 * @param last_word_valid_bytes Number of valid bytes in the last word from packer flush
 * 
 * Extracts all remaining words from the FIFO, handles partial bytes from packer flush,
 * and updates the OpenSSL context (HMAC or hash) with the remaining data.
 */
void hmac_ip::process_remaining_data(unsigned int last_word_valid_bytes) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Processing remaining data: FIFO size=" << msg_fifo.size()
                        << " last_word_valid_bytes=" << last_word_valid_bytes << std::endl;

   // Process remaining data

      // For final block, extract all remaining words
      std::vector<uint8_t> remaining_data;
      size_t word_count = msg_fifo.size();
      size_t word_index = 0;
      REG_DEBUG(2, logger) << "Extracting " << word_count << " words from FIFO" << std::endl;

      while (!msg_fifo.empty()) {
         uint32_t word = msg_fifo.front();
         msg_fifo.pop();
         word_index++;

         // For the last word, only extract valid bytes if it came from packer flush
         if (word_index == word_count && last_word_valid_bytes > 0 && last_word_valid_bytes < 4) {
            // Only extract the valid bytes from the last word
            REG_DEBUG(3, logger) << "Last word (partial): extracting " << last_word_valid_bytes << " bytes" << std::endl;
            for (unsigned int i = 0; i < last_word_valid_bytes; i++) {
               remaining_data.push_back((word >> (24 - i*8)) & 0xFF);
            }
         } else {
            // Extract all 4 bytes from the word
            remaining_data.push_back((word >> 24) & 0xFF);
            remaining_data.push_back((word >> 16) & 0xFF);
            remaining_data.push_back((word >> 8) & 0xFF);
            remaining_data.push_back(word & 0xFF);
         }
      }

      REG_DEBUG(3, logger) << "Extracted " << remaining_data.size() << " bytes total" << std::endl;

      // The tail cannot be absorbed yet: padding depends on the total message
      // length, so it is held until the digest is finalized.
      m_final_tail = remaining_data;
      REG_DEBUG(2, logger) << "Held " << m_final_tail.size() << " tail byte(s) for finalization" << std::endl;

   REG_INFO(1, logger) << "Remaining data processed successfully" << std::endl;
}

/**
 * @brief Finalize hash/HMAC and write digest to registers
 * @return true on success, false on error
 * 
 * Finalizes the hash/HMAC operation, writes the digest to registers,
 * applies timing annotation, and sets the completion interrupt.
 */
bool hmac_ip::compute_and_write_digest() {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Computing and writing digest" << std::endl;

   // Close out the inner hash over the held tail.
   std::vector<uint8_t> digest_bytes(m_sha.digest_bytes());
   m_sha.finalize(m_final_tail.data(), m_final_tail.size(), digest_bytes.data());
   m_final_tail.clear();
   REG_INFO(1, logger) << "Hash finalized: digest_len=" << digest_bytes.size() << " bytes" << std::endl;

   if (CFG.hmac_en) {
      // HMAC closes with a second, independent hash over K^opad and the inner
      // digest. It holds no state across commands, so it is run here in full.
      REG_DEBUG(2, logger) << "Finalizing HMAC outer hash" << std::endl;

      const std::vector<uint8_t> opad = key_block(0x5C);

      sha2_engine outer;
      outer.init(m_sha.mode());
      outer.compress(opad.data());

      std::vector<uint8_t> mac(outer.digest_bytes());
      outer.finalize(digest_bytes.data(), digest_bytes.size(), mac.data());
      digest_bytes = mac;

      REG_INFO(1, logger) << "HMAC finalized: digest_len=" << digest_bytes.size() << " bytes" << std::endl;
   }

   // Write digest to registers
   unsigned int digest_words = get_digest_size_words();
   REG_DEBUG(2, logger) << "Writing digest to registers: " << digest_words << " words" << std::endl;

   // Store the digest unswapped. digest_swap is a read-path transformation in
   // hardware, so it is applied when software reads DIGEST rather than being
   // baked in here; that way toggling the bit after a hash completes changes
   // what subsequent reads return, as it does in RTL.
   for (unsigned int i = 0; i < digest_words && i < 16; i++) {
      uint32_t word = (digest_bytes[i*4] << 24) | (digest_bytes[i*4+1] << 16) |
                     (digest_bytes[i*4+2] << 8) | digest_bytes[i*4+3];
      DIGEST[i] = word;
   }

   // SHA-256 produces eight words but there are sixteen digest registers.
   // Hardware mirrors the result into the upper half so that every DIGEST CSR
   // holds defined data and WIPE_SECRET scrubs all of them uniformly.
   if (digest_words == 8) {
      for (unsigned int i = 0; i < 8; i++) {
         DIGEST[i + 8] = DIGEST[i];
      }
      REG_DEBUG(2, logger) << "Replicated DIGEST[0..7] into DIGEST[8..15] for SHA-256" << std::endl;
   }

   // Apply timing
   double clock_freq = clk_i.read();
   unsigned int cycles = (CFG.digest_size == 0x1) ? SHA256_BLOCK_CYCLES : SHA384_512_BLOCK_CYCLES;
   if (CFG.hmac_en) {
      cycles += HMAC_EXTRA_CYCLES;
   }

   double delay_ns = (cycles / clock_freq) * 1e9;
   REG_DEBUG(2, logger) << "Applying finalization timing: " << cycles << " cycles (" << delay_ns << " ns)" << std::endl;
   wait(delay_ns, SC_NS);

   // Set completion interrupt
   INTR_STATE.hmac_done = 1;
   current_state = State::IDLE;
   update_status_register();
   interrupt_update_event.notify(SC_ZERO_TIME);

   REG_INFO(1, logger) << "Digest computation complete - transitioning to IDLE" << std::endl;

   return true;
}

/**
 * @brief Test whether the current configuration blocks a hash from starting
 * @param digest_size Sanitised CFG.digest_size encoding
 * @param key_length Sanitised CFG.key_length encoding
 * @param hmac_en Whether keyed HMAC mode is selected
 * @return true if hash_start/hash_continue must be refused with SwInvalidConfig
 *
 * Key length is only meaningful in HMAC mode; plain SHA-2 ignores it, including
 * the Key_1024 restriction that otherwise applies to SHA-256.
 */
bool hmac_ip::is_invalid_config(uint32_t digest_size, uint32_t key_length, bool hmac_en) const {
   return (digest_size == 0x8) ||
          (hmac_en && (key_length == 0x20)) ||
          (hmac_en && (key_length == 0x10) && (digest_size == 0x1));
}

/**
 * @brief Report an error condition
 * @param error_code Error code to set in ERR_CODE register
 *
 * Sets the error code in ERR_CODE register and asserts the hmac_err
 * interrupt. Triggers interrupt output update.
 *
 * Only the first error of a series is captured: while hmac_err is still
 * pending, ERR_CODE holds the code software has not yet read. Software must
 * clear hmac_err (W1C) before a later error can be recorded.
 */
void hmac_ip::report_error(uint32_t error_code) {
   REG_FUNC_TRACE(logger);

   if (INTR_STATE.hmac_err) {
      REG_INFO(1, logger) << "Error 0x" << std::hex << error_code << std::dec
                           << " not recorded: hmac_err still pending with ERR_CODE=0x"
                           << std::hex << static_cast<uint32_t>(ERR_CODE) << std::dec << std::endl;
      return;
   }

   REG_ERROR(0, logger) << "Reporting error: error_code=0x" << std::hex << error_code << std::dec << std::endl;

   ERR_CODE = error_code;
   INTR_STATE.hmac_err = 1;
   interrupt_update_event.notify(SC_ZERO_TIME);

   REG_INFO(1, logger) << "Error reported - hmac_err interrupt asserted" << std::endl;
}

/**
 * @name Register Callback Functions
 * @{
 */

/**
 * @brief Handle write to INTR_STATE register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Implements write-1-to-clear (W1C) behavior for hmac_done and hmac_err bits.
 */
bool hmac_ip::handle_write_INTR_STATE(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to INTR_STATE: value=0x" << std::hex << value << std::dec << std::endl;

   // W1C for hmac_done and hmac_err
   if (value & 0x1) {
      INTR_STATE.hmac_done = 0;
      REG_DEBUG(2, logger) << "Cleared hmac_done interrupt" << std::endl;
   }
   if (value & 0x4) {
      INTR_STATE.hmac_err = 0;
      REG_DEBUG(2, logger) << "Cleared hmac_err interrupt" << std::endl;
   }

   interrupt_update_event.notify(SC_ZERO_TIME);
   return false; // Handled in callback
}

/**
 * @brief Handle write to INTR_ENABLE register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Updates interrupt enable bits with proper mask handling.
 */
bool hmac_ip::handle_write_INTR_ENABLE(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to INTR_ENABLE: value=0x" << std::hex << value << std::dec << std::endl;

   // Manually write the value to INTR_ENABLE register with proper mask handling
   uint32_t current = INTR_ENABLE;
   uint32_t new_value = (current & ~write_mask) | (value & write_mask);
   INTR_ENABLE = new_value;

   REG_INFO(1, logger) << "INTR_ENABLE updated: 0x" << std::hex << new_value << std::dec << std::endl;

   interrupt_update_event.notify(SC_ZERO_TIME);
   return false; // Callback handled the write
}

/**
 * @brief Handle write to INTR_TEST register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write does not store)
 * 
 * Forces interrupt assertion in INTR_STATE for testing purposes.
 * Writing 1 to a bit forces the corresponding interrupt.
 */
bool hmac_ip::handle_write_INTR_TEST(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Write to INTR_TEST: value=0x" << std::hex << value << std::dec << std::endl;

   // Force interrupt assertion
   if (value & 0x1) {
      INTR_STATE.hmac_done = 1;
      REG_DEBUG(2, logger) << "Forcing hmac_done interrupt" << std::endl;
   }
   if (value & 0x2) {
      INTR_STATE.fifo_empty = 1;
      REG_DEBUG(2, logger) << "Forcing fifo_empty interrupt" << std::endl;
   }
   if (value & 0x4) {
      INTR_STATE.hmac_err = 1;
      REG_DEBUG(2, logger) << "Forcing hmac_err interrupt" << std::endl;
   }

   interrupt_update_event.notify(SC_ZERO_TIME);
   return false; // Write does not store
}

/**
 * @brief Handle write to ALERT_TEST register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write does not store)
 * 
 * Triggers alert_fatal_fault output when bit 0 is set.
 * Alert stays asserted until reset.
 */
bool hmac_ip::handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to ALERT_TEST: value=0x" << std::hex << value << std::dec << std::endl;

   if (value & 0x1) {
      alert_fatal_fault.write(true);
      REG_INFO(1, logger) << "Fatal fault alert triggered (will stay asserted until reset)" << std::endl;
      // Note: Alert stays asserted until reset
   }
   return false; // Write does not store
}

/**
 * @brief Handle read from CFG register
 * @param value Reference to return value
 * @param read_mask Read mask
 * @return true (callback handled the read)
 */
bool hmac_ip::handle_read_CFG(uint32_t& value, uint32_t read_mask) {
   REG_FUNC_TRACE(logger);

   // Return current configuration
   value = CFG;
   REG_DEBUG(2, logger) << "Read from CFG: value=0x" << std::hex << value << std::dec << std::endl;

   return true; // Callback handled the read
}

/**
 * @brief Handle write to CFG register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Validates configuration and only allows writes when engine is idle.
 * Validates digest size, key length, and their combinations.
 * Clears DIGEST registers when sha_en transitions from 1 to 0.
 */
bool hmac_ip::handle_write_CFG(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Write to CFG: value=0x" << std::hex << value << std::dec << std::endl;

   // Only writable when idle
   if (current_state != State::IDLE) {
      REG_DEBUG(2, logger) << "CFG write rejected - not in IDLE state" << std::endl;
      return false; // Discard write silently
   }

   // Validate configuration
   uint32_t digest_size = (value >> 5) & 0xF;
   uint32_t key_length = (value >> 9) & 0x3F;
   uint32_t hmac_en = value & 0x1;

   REG_DEBUG(2, logger) << "CFG parameters: digest_size=0x" << std::hex << digest_size << " key_length=0x" << key_length << " hmac_en=" << hmac_en << std::dec << std::endl;

   // Unsupported encodings read back as the "none" encoding. CFG is an external
   // register in RTL: prim_subreg_ext has no storage, so reads return the
   // sanitised digest_size/key_length rather than the raw value software wrote.
   // The two fields are sanitised independently, and neither raises an error
   // here — an illegal configuration is only reported at hash_start/hash_continue.
   if ((digest_size != 0x1) && (digest_size != 0x2) && (digest_size != 0x4)) {
      REG_INFO(1, logger) << "Unsupported digest_size 0x" << std::hex << digest_size
                           << std::dec << " reads back as SHA2_None" << std::endl;
      value &= ~(0xF << 5);
      value |= (0x8 << 5);  // SHA2_None
   }

   if ((key_length != 0x1) && (key_length != 0x2) && (key_length != 0x4) &&
       (key_length != 0x8) && (key_length != 0x10)) {
      REG_INFO(1, logger) << "Unsupported key_length 0x" << std::hex << key_length
                           << std::dec << " reads back as Key_None" << std::endl;
      value &= ~(0x3F << 9);
      value |= (0x20 << 9);  // Key_None
   }

   // Manually write the value to CFG register with proper mask handling
   uint32_t current = CFG;
   uint32_t new_value = (current & ~write_mask) | (value & write_mask);

   // Check if sha_en (bit 1) transitions from 1 to 0
   // This triggers clearing of all DIGEST registers to prevent context leakage
   bool sha_en_was_set = (current & (1 << 1)) != 0;
   bool sha_en_will_be_cleared = ((new_value & (1 << 1)) == 0) &&
                                  ((write_mask & (1 << 1)) != 0);

   if (sha_en_was_set && sha_en_will_be_cleared) {
      REG_INFO(1, logger) << "sha_en transitioned from 1 to 0 - clearing DIGEST registers" << std::endl;
      // Clear all DIGEST registers when sha_en transitions from 1 to 0
      for (unsigned int i = 0; i < 16; i++) {
         DIGEST[i] = 0x00000000;
      }
   }

   CFG = new_value;
   REG_INFO(1, logger) << "CFG updated: 0x" << std::hex << new_value << std::dec << std::endl;

   return false; // Callback handled the write
}

/**
 * @brief Handle read from CMD register
 * @param value Reference to return value (always 0)
 * @param read_mask Read mask
 * @return true (callback handled the read)
 * 
 * CMD register is read-zero (r0w1c). Always returns 0 regardless
 * of what was written, as command bits self-clear.
 */
bool hmac_ip::handle_read_CMD(uint32_t& value, uint32_t read_mask) {
   REG_FUNC_TRACE(logger);

   // CMD register is r0w1c (read-zero, write-one-to-clear)
   // Always returns 0 regardless of what was written
   value = 0;
   REG_DEBUG(2, logger) << "Read from CMD: always returns 0 (r0w1c)" << std::endl;

   return true; // Callback handled the read
}

/**
 * @brief Handle write to CMD register
 * @param value Write value (command bits)
 * @param write_mask Write mask
 * @return false (command bits self-clear)
 * 
 * Processes command bits:
 * - hash_start (bit 0): Start new hash operation
 * - hash_process (bit 1): Process remaining data and finalize
 * - hash_stop (bit 2): Stop operation and save context
 * - hash_continue (bit 3): Continue from saved context
 * 
 * Commands are write-1-to-trigger and self-clear.
 */
bool hmac_ip::handle_write_CMD(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Write to CMD: value=0x" << std::hex << value << std::dec << std::endl;

   // hash_start command (bit 0)
   if (value & 0x1) {
      REG_INFO(1, logger) << "Processing hash_start command" << std::endl;

      uint32_t digest_size = CFG.digest_size;
      uint32_t key_length = CFG.key_length;
      uint32_t hmac_en = CFG.hmac_en;

      REG_DEBUG(2, logger) << "Configuration: digest_size=0x" << std::hex << digest_size << " key_length=0x" << key_length << " hmac_en=" << hmac_en << std::dec << std::endl;

      // Checked in RTL's priority order: SwInvalidConfig outranks
      // SwHashStartWhenShaDisabled, which outranks SwHashStartWhenActive.
      // key_length only constrains HMAC mode; it is irrelevant for plain SHA-2.
      if (is_invalid_config(digest_size, key_length, hmac_en)) {
         REG_ERROR(0, logger) << "hash_start rejected: invalid configuration" << std::endl;
         report_error(0x6); // SwInvalidConfig
         return false;
      }

      if (!CFG.sha_en) {
         REG_ERROR(0, logger) << "hash_start rejected: sha_en is not set" << std::endl;
         report_error(0x2); // SwHashStartWhenShaDisabled
         return false;
      }

      if (current_state != State::IDLE) {
         REG_ERROR(0, logger) << "hash_start rejected: engine is not IDLE" << std::endl;
         report_error(0x4); // SwHashStartWhenActive
         return false;
      }

      // Clear FIFO to ensure no stale data from previous operations
      size_t fifo_size = msg_fifo.size();
      while (!msg_fifo.empty()) {
         msg_fifo.pop();
      }
      if (fifo_size > 0) {
         REG_DEBUG(2, logger) << "Cleared " << fifo_size << " stale words from FIFO" << std::endl;
      }

      // The key source is selected when the core consumes the key, not here, so
      // the software key register is left untouched by a sideload operation.
      if (hmac_en && m_keymgr_key_valid) {
         REG_INFO(1, logger) << "Using sideload key from key manager" << std::endl;
      }

      // Initialize OpenSSL context
      std::string algorithm = get_hash_algorithm();
      if (!initialize_hash_context(algorithm, hmac_en, key_length)) {
         return false;
      }

      // Transition to PROCESSING
      current_state = State::PROCESSING;
      message_length_bits = 0;
      process_stop_issued = false;
      msg_allowed = true;
      fifo_full_seen = false;
      packer_bytes_count = 0;
      packer_buffer = 0;

      REG_INFO(1, logger) << "Transitioning to PROCESSING state" << std::endl;

      // Clear digest registers
      for (unsigned int i = 0; i < 16; i++) {
         DIGEST[i] = 0;
      }
      REG_DEBUG(2, logger) << "DIGEST registers cleared" << std::endl;

      // Clear message length
      MSG_LENGTH_LOWER = 0;
      MSG_LENGTH_UPPER = 0;

      update_status_register();
      REG_INFO(1, logger) << "hash_start command complete" << std::endl;
   }

   // hash_process command (bit 1)
   if (value & 0x2) {
      REG_INFO(1, logger) << "Processing hash_process command" << std::endl;

      if (current_state != State::PROCESSING) {
         REG_ERROR(0, logger) << "hash_process rejected: not in PROCESSING state" << std::endl;
         return false;
      }

      // Flush packer to FIFO if needed
      unsigned int last_word_valid_bytes = flush_packer_if_needed();

      // Mark that Process command has been issued (prevents fifo_empty interrupt during finalization)
      process_stop_issued = true;
      // The packer has been flushed, so software may no longer write MSG_FIFO.
      msg_allowed = false;
      fifo_full_seen = false;
      REG_DEBUG(2, logger) << "process_stop_issued flag set" << std::endl;

      // Convert the remaining data to bytes and update OpenSSL context
      process_remaining_data(last_word_valid_bytes);

      // No fifo_empty interrupt here: hash_process closes the FIFO to software
      // and clears the was-full tracking, so hardware gates the interrupt off
      // for the rest of the operation.

      REG_INFO(1, logger) << "Triggering digest computation" << std::endl;
      digest_computation_event.notify(SC_ZERO_TIME);


   }

   // hash_stop command (bit 2)
   if (value & 0x4) {
      REG_INFO(1, logger) << "Processing hash_stop command" << std::endl;

      if (current_state != State::PROCESSING) {
         REG_ERROR(0, logger) << "hash_stop rejected: not in PROCESSING state" << std::endl;
         return false;
      }

      process_stop_issued = true;
      msg_allowed = false;
      fifo_full_seen = false;

      // Flush any complete blocks still in the FIFO into the hash engine
      // synchronously before transitioning to IDLE. The block_processing_thread
      // cannot be relied on here because it checks current_state==PROCESSING
      // before draining — if hash_stop and the last MSG_FIFO write land in the
      // same simulation time step the thread will see IDLE and skip the drain.
      unsigned int block_size = get_block_size_words();
      while (msg_fifo.size() >= block_size) {
         std::vector<uint8_t> block_data;
         block_data.reserve(get_block_size_bytes());
         for (unsigned int i = 0; i < block_size; i++) {
            uint32_t word = msg_fifo.front();
            msg_fifo.pop();
            block_data.push_back((word >> 24) & 0xFF);
            block_data.push_back((word >> 16) & 0xFF);
            block_data.push_back((word >> 8)  & 0xFF);
            block_data.push_back( word        & 0xFF);
         }
         m_sha.compress(block_data.data());
         REG_INFO(1, logger) << "hash_stop: flushed one block into the hash engine" << std::endl;
      }
      // Any sub-block remainder stays where it is. The hardware FIFO has its
      // clear input tied off and is never flushed, so leftover words and the
      // packer's partial word survive the pause and are consumed when hashing
      // resumes on hash_continue.
      const bool block_aligned = msg_fifo.empty() && (packer_bytes_count == 0);

      current_state = State::IDLE;
      process_stop_issued = false;

      if (block_aligned) {
         // The chaining state is only defined between blocks, which is exactly
         // where a well-formed hash_stop lands. Publish it so software can read
         // it out of DIGEST, save it, and hand it back at hash_continue.
         export_state_to_digest();

         INTR_STATE.hmac_done = 1;
         REG_INFO(1, logger) << "Transitioning to IDLE state - setting hmac_done interrupt" << std::endl;
      } else {
         // The engine only ever completes whole blocks, so a hash_stop issued
         // part-way through one leaves hmac_done deasserted. Software that
         // waits on hmac_done alone will wait forever; this is a misuse of the
         // command rather than a modelling shortcut, so make it visible.
         REG_ERROR(0, logger) << "hash_stop issued off a block boundary ("
                               << msg_fifo.size() << " word(s) and "
                               << static_cast<unsigned int>(packer_bytes_count)
                               << " byte(s) pending): hmac_done is not asserted, "
                               << "matching hardware, which completes whole blocks only"
                               << std::endl;
      }

      update_status_register();
      interrupt_update_event.notify(SC_ZERO_TIME);
   }

   // hash_continue command (bit 3)
   if (value & 0x8) {
      REG_INFO(1, logger) << "Processing hash_continue command" << std::endl;

      // RTL gates hash_continue on the same conditions as hash_start, and
      // reports the same errors in the same priority order.
      if (is_invalid_config(CFG.digest_size, CFG.key_length, CFG.hmac_en)) {
         REG_ERROR(0, logger) << "hash_continue rejected: invalid configuration" << std::endl;
         report_error(0x6); // SwInvalidConfig
         return false;
      }

      if (!CFG.sha_en) {
         REG_ERROR(0, logger) << "hash_continue rejected: sha_en is not set" << std::endl;
         report_error(0x2); // SwHashStartWhenShaDisabled
         return false;
      }

      if (current_state != State::IDLE) {
         REG_ERROR(0, logger) << "hash_continue rejected: engine is not IDLE" << std::endl;
         report_error(0x4); // SwHashStartWhenActive
         return false;
      }

      // Reload the chaining state software placed in DIGEST and the bit count it
      // placed in MSG_LENGTH. This is the point of the whole stop/continue
      // mechanism: the engine resumes mid-message from a state it never held.
      import_state_from_digest();

      current_state = State::PROCESSING;
      process_stop_issued = false;
      msg_allowed = true;
      fifo_full_seen = false;
      REG_INFO(1, logger) << "Transitioning to PROCESSING state (context restored)" << std::endl;
      update_status_register();
   }

   return false; // Command bits self-clear
}

/**
 * @brief Handle write to WIPE_SECRET register
 * @param value Wipe value to write to sensitive data
 * @param write_mask Write mask
 * @return false (write does not store to register)
 * 
 * Wipes sensitive data (key storage and digest registers) with the
 * provided value. Also frees and resets OpenSSL contexts.
 */
bool hmac_ip::handle_write_WIPE_SECRET(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "Write to WIPE_SECRET: wiping sensitive data with value=0x"
                        << std::hex << value << std::dec << std::endl;

   // Wipe key storage (SW path and sideload path)
   std::fill(key_storage.begin(), key_storage.end(), value);
   std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
   std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));
   m_keymgr_key_valid = false;
   REG_DEBUG(2, logger) << "Key storage and sideload buffers wiped" << std::endl;

   // Wipe digest registers and any pending software context-restore writes
   for (unsigned int i = 0; i < 16; i++) {
      DIGEST[i] = value;
      m_digest_restore[i] = 0;
      m_digest_restore_valid[i] = false;
   }
   REG_DEBUG(2, logger) << "DIGEST registers wiped" << std::endl;

   // Scrub the hash engine too: the chaining state of an in-flight hash and the
   // retained key are as sensitive as the registers just wiped.
   m_sha.init(sha2_mode::sha256);
   std::fill(m_hmac_key.begin(), m_hmac_key.end(), 0);
   m_hmac_key.clear();
   std::fill(m_final_tail.begin(), m_final_tail.end(), 0);
   m_final_tail.clear();
   REG_DEBUG(2, logger) << "Hash engine state wiped" << std::endl;

   REG_INFO(1, logger) << "Sensitive data wiped successfully" << std::endl;

   return false; // Write does not store to WIPE_SECRET register
}

/**
 * @brief Handle write to KEY register
 * @param index KEY register index (0-31)
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write does not store to register)
 * 
 * Stores key data internally. Only writable when engine is idle.
 * Applies key_swap if configured. Reports error if written during processing.
 */
bool hmac_ip::handle_write_KEY(unsigned int index, uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to KEY[" << index << "]: value=0x" << std::hex << value << std::dec << std::endl;

   // Only writable when idle
   if (current_state != State::IDLE) {
      REG_ERROR(0, logger) << "KEY write rejected: not in IDLE state" << std::endl;
      report_error(0x3); // SwUpdateSecretKeyInProcess
      return false;
   }

   // When key manager sideload is active, SW KEY writes are ignored
   // (mirrors RTL: keymgr_key_i.valid overrides secret_key from registers)
   if (m_keymgr_key_valid) {
      REG_INFO(1, logger) << "KEY[" << index << "] write ignored: keymgr sideload active" << std::endl;
      return false;
   }
   else {
      REG_INFO(1, logger) << "SW Written KEYS used when Keymanager key is not valid" << std::endl;
   }

   // Store key internally (write-only)
   uint32_t key_value = value;
   if (CFG.key_swap) {
      // Note: apply_endian_swap(key_value) - FIX: Currently using inline swap
      key_value = ((key_value & 0xFF) << 24) | ((key_value & 0xFF00) << 8) |
                  ((key_value & 0xFF0000) >> 8) | ((key_value & 0xFF000000) >> 24);
      REG_DEBUG(2, logger) << "Key swap applied: 0x" << std::hex << value << " -> 0x" << key_value << std::dec << std::endl;
   }
   key_storage[index] = key_value;
   REG_DEBUG(2, logger) << "KEY[" << index << "] stored in internal storage" << std::endl;

   return false; // Write does not store to register (write-only)
}

/**
 * @brief Handle read from DIGEST register
 * @param index DIGEST register index (0-15)
 * @param value Reference to return value
 * @param read_mask Read mask
 * @return true (callback handled the read)
 */
bool hmac_ip::handle_read_DIGEST(unsigned int index, uint32_t& value, uint32_t read_mask) {
   REG_FUNC_TRACE(logger);

   value = DIGEST[index];

   // digest_swap byte-reverses each 32-bit word on the way out, using the
   // configuration in force at the time of the read.
   if (CFG.digest_swap) {
      const uint32_t raw = value;
      value = byte_reverse32(value);
      REG_DEBUG(2, logger) << "Digest swap on read: DIGEST[" << index << "] 0x" << std::hex
                            << raw << " -> 0x" << value << std::dec << std::endl;
   }

   REG_DEBUG(2, logger) << "Read from DIGEST[" << index << "]: value=0x" << std::hex << value << std::dec << std::endl;

   return true; // Callback handled the read
}

/**
 * @brief Handle write to DIGEST register
 * @param index DIGEST register index (0-15)
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Only writable when engine is idle. Used for context save/restore.
 */
bool hmac_ip::handle_write_DIGEST(unsigned int index, uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to DIGEST[" << index << "]: value=0x" << std::hex << value << std::dec << std::endl;

   // Only writable when idle
   if (current_state != State::IDLE) {
      REG_DEBUG(2, logger) << "DIGEST write rejected: not in IDLE state" << std::endl;
      return false; // Reject write
   }

   // Undo digest_swap on the way in so that storage always holds the raw word,
   // mirroring the read path. Byte reversal is its own inverse.
   if (CFG.digest_swap) {
      value = byte_reverse32(value);
   }

   // DIGEST is hwext on silicon: a software write is held for hash_continue
   // and does not update the value a subsequent read returns. hmac_p2_sensreg
   // checks that idle writes do not echo.
   const uint32_t current =
       m_digest_restore_valid[index] ? m_digest_restore[index] : 0u;
   const uint32_t new_value = (current & ~write_mask) | (value & write_mask);
   m_digest_restore[index] = new_value;
   m_digest_restore_valid[index] = true;

   REG_DEBUG(2, logger) << "DIGEST[" << index << "] SW write captured for context restore" << std::endl;

   return false; // Callback handled the write
}

/**
 * @brief Handle write to MSG_LENGTH_LOWER register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Only writable when engine is idle. Used for context save/restore.
 */
bool hmac_ip::handle_write_MSG_LENGTH_LOWER(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to MSG_LENGTH_LOWER: value=0x" << std::hex << value << std::dec << std::endl;

   if (current_state != State::IDLE) {
      REG_DEBUG(2, logger) << "MSG_LENGTH_LOWER write rejected: not in IDLE state" << std::endl;
      return false; // Reject write
   }

   // Manually write the value with proper mask handling
   uint32_t current = MSG_LENGTH_LOWER;
   uint32_t new_value = (current & ~write_mask) | (value & write_mask);
   MSG_LENGTH_LOWER = new_value;

   REG_DEBUG(2, logger) << "MSG_LENGTH_LOWER updated for context restore" << std::endl;

   return false; // Callback handled the write
}

/**
 * @brief Handle write to MSG_LENGTH_UPPER register
 * @param value Write value
 * @param write_mask Write mask
 * @return false (write handled in callback)
 * 
 * Only writable when engine is idle. Used for context save/restore.
 */
bool hmac_ip::handle_write_MSG_LENGTH_UPPER(uint32_t value, uint32_t write_mask) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to MSG_LENGTH_UPPER: value=0x" << std::hex << value << std::dec << std::endl;

   if (current_state != State::IDLE) {
      REG_DEBUG(2, logger) << "MSG_LENGTH_UPPER write rejected: not in IDLE state" << std::endl;
      return false; // Reject write
   }

   // Manually write the value with proper mask handling
   uint32_t current = MSG_LENGTH_UPPER;
   uint32_t new_value = (current & ~write_mask) | (value & write_mask);
   MSG_LENGTH_UPPER = new_value;

   REG_DEBUG(2, logger) << "MSG_LENGTH_UPPER updated for context restore" << std::endl;

   return false; // Callback handled the write
}

/**
 * @brief Handle write to MSG_FIFO register with byte enable
 * @param value Write value
 * @param byte_enable Byte enable mask (4 bits, one per byte)
 * @return false (write does not store to FIFO register)
 * 
 * Supports byte-level writes via byte_enable. Accumulates bytes into
 * packer_buffer and pushes complete words to msg_fifo. Handles FIFO
 * back-pressure and automatic block processing. Updates message length.
 * 
 * @note Only allowed when sha_en is set and engine is in PROCESSING state.
 */
bool hmac_ip::handle_write_MSG_FIFO_with_be(uint32_t value, uint8_t byte_enable) {
   REG_FUNC_TRACE(logger);
   REG_DEBUG(2, logger) << "Write to MSG_FIFO: value=0x" << std::hex << value
                         << " byte_enable=0x" << static_cast<int>(byte_enable) << std::dec << std::endl;

   // Validate conditions
   if (!CFG.sha_en || current_state != State::PROCESSING) {
      REG_ERROR(0, logger) << "MSG_FIFO write rejected: sha_en=" << CFG.sha_en << " state=" << (current_state == State::PROCESSING ? "PROCESSING" : "not PROCESSING") << std::endl;
      report_error(0x5); // SwPushMsgWhenDisallowed
      return false;
   }

   // Count set bits in byte_enable to get data_length
   unsigned int data_length = 0;

   // Count number of set bits to determine data_length
   for (unsigned int i = 0; i < 4; i++) {
      if (byte_enable & (1 << i)) {
         data_length++;
      }
   }

   // Extract actual data bytes based on byte_enable
   // Note: regmodel provides the value with bytes positioned according to the write
   uint8_t data_bytes[4] = {0, 0, 0, 0};

   // Extract bytes LSByte-first (byte_enable bit i covers bits[i*8+7:i*8]).
   // This matches the OpenTitan prim_packer LSByte-first behaviour: with endian_swap=0
   // the byte at the lowest bus address (LSByte) becomes the first SHA message byte.
   unsigned int src_byte_idx = 0;
   for (unsigned int i = 0; i < 4; i++) {
      if (byte_enable & (1 << i)) {
         data_bytes[src_byte_idx++] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
      }
   }

   // Packer logic: accumulate bytes into packer_buffer
   // For single-byte writes, pack sequentially regardless of address/byte_enable
   // For multi-byte writes (halfword/word), use lane-based packing
   REG_DEBUG(3, logger) << "Packing " << data_length << " bytes into packer buffer" << std::endl;

   for (unsigned int i = 0; i < data_length; i++) {
      // Sequential big-endian packing: first arriving byte goes to bits[31:24].
      // Combined with LSByte-first extraction, the LSByte of the written word is placed
      // at bits[31:24] and becomes the first SHA byte (endian_swap=0 / no swap).
      // Setting endian_swap=1 reverses the assembled word so MSByte goes to SHA first.
      unsigned int byte_pos = 3 - (packer_bytes_count % 4);
      
      // Clear the byte position first, then set it
      packer_buffer &= ~(0xFF << (byte_pos * 8));
      packer_buffer |= (static_cast<uint32_t>(data_bytes[i]) << (byte_pos * 8));
      packer_bytes_count++;

      // When we've accumulated 4 bytes, push to FIFO
      if (packer_bytes_count >= 4) {
         // Apply endian swap if configured (before pushing)
         uint32_t word_to_push = packer_buffer;
         apply_endian_swap(word_to_push);

         push_fifo_word(word_to_push);

         packer_buffer = 0;
         packer_bytes_count = 0;

         // Update status after pushing word
         update_status_register();

         // Process block if ready
         if (msg_fifo.size() >= get_block_size_words()) {
            REG_INFO(1, logger) << "Block ready - triggering block processing" << std::endl;
            block_processing_event.notify(SC_ZERO_TIME);
         }
      }
   }

   // Update message length in bytes (convert to bits)
   message_length_bits += (data_length * 8);
   MSG_LENGTH_LOWER = static_cast<uint32_t>(message_length_bits & 0xFFFFFFFF);
   MSG_LENGTH_UPPER = static_cast<uint32_t>((message_length_bits >> 32) & 0xFFFFFFFF);

   REG_DEBUG(3, logger) << "Message length updated: " << message_length_bits << " bits" << std::endl;

   return false; // Write does not store to FIFO register
}

/**
 * @brief Handle read from STATUS register
 * @param value Reference to return value
 * @param read_mask Read mask
 * @return true (callback handled the read completely)
 * 
 * Updates STATUS register before reading to ensure current values.
 */
bool hmac_ip::handle_read_STATUS(uint32_t& value, uint32_t read_mask) {
   REG_FUNC_TRACE(logger);

   update_status_register();
   value = STATUS;
   REG_DEBUG(2, logger) << "Read from STATUS: value=0x" << std::hex << value << std::dec << std::endl;

   return true; // Callback handled the read completely
}

/**
 * @brief TLM b_transport handler for key manager private key bus socket
 *
 * Accepts write transactions from key_manager carrying sideload key material.
 * Offset map (from hmac_wrapper_key.rdl):
 *   0x00-0x1C  KEY_SHARE0[0..7]
 *   0x20-0x3C  KEY_SHARE1[0..7]
 *   0x40       KEY_CTRL  (bit 0 = key_valid)
 * Read transactions are rejected.
 */
void hmac_ip::keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
   if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
   }
   const unsigned len = trans.get_data_length();
   unsigned char* ptr = trans.get_data_ptr();
   if (ptr == nullptr) {
      trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
   }
   if (len < 4) {
      trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
      return;
   }
   uint64_t offset = trans.get_address();
   uint32_t* data  = reinterpret_cast<uint32_t*>(ptr);
   if (offset <= 0x1C && (offset % 4) == 0) {
      m_keymgr_share0[offset / 4] = *data;
      REG_DEBUG(2, logger) << "keymgr: share0[" << (offset/4) << "]=0x"  << std::hex << *data << std::dec << std::endl;
   } else if (offset >= 0x20 && offset <= 0x3C && (offset % 4) == 0) {
      m_keymgr_share1[(offset - 0x20) / 4] = *data;
      REG_DEBUG(2, logger) << "keymgr: share1[" << ((offset-0x20)/4) << "]=0x" << std::hex << *data << std::dec << std::endl;
   } else if (offset == 0x40) {
      m_keymgr_key_valid = (*data & 0x1u) != 0;
      REG_INFO(1, logger) << "keymgr: key_valid=" << m_keymgr_key_valid << std::endl;
   }
   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

/**
 * @brief Select the key word actually fed to the MAC
 * @param index Key word index (0-31)
 * @return Sideload key word when the key manager key is valid, else the
 *         software-written key word
 *
 * Hardware muxes between the software key register and the key-manager key at
 * the point the core consumes it, rather than copying one over the other. The
 * software key therefore survives a sideload operation and becomes visible
 * again as soon as the key manager deasserts validity.
 *
 * The sideload key is the XOR of the two shares and carries only 256 bits of
 * material, MSB-justified in the 1024-bit key vector.
 */
uint32_t hmac_ip::effective_key_word(unsigned int index) const {
   if (index >= 32) {
      return 0;
   }

   if (!m_keymgr_key_valid) {
      return key_storage[index];
   }

   return (index < 8) ? (m_keymgr_share0[index] ^ m_keymgr_share1[index]) : 0u;
}

/**
 * @brief Destructor
 *
 * The hash engine and its buffers are self-managing, so there is nothing to
 * release here.
 */
hmac_ip::~hmac_ip() {
   REG_FUNC_TRACE(logger);
   REG_INFO(1, logger) << "HMAC IP destroyed successfully" << std::endl;
}

