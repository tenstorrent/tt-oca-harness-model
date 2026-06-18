/******************************************************************************
 * @file kmac.cpp
 * @brief KMAC SystemC TLM model implementation
 *
 * This file implements the KMAC model constructor, port initialization, and
 * main processing thread for register callbacks and FSM operations.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "kmac.h"
#include "csml_logger.h"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/sha.h>

/******************************************************************************
 * @class kmac_ip::kmac_app_handler
 * @brief Internal handler class implementing kmac_app_if for application
 * interfaces
 *
 * This class provides the implementation of kmac_app_if methods that are
 * bound to the model's app_export[] sc_exports. External applications (KeyMgr,
 * LC_CTRL, ROM_CTRL) call these methods to interact with KMAC.
 ******************************************************************************/
class kmac_ip::kmac_app_handler : public kmac_app_if {
public:
  /**
   * @brief Constructor
   * @param parent Pointer to parent kmac_ip model instance
   * @param index Application interface index (0=KeyMgr, 1=LC_CTRL, 2=ROM_CTRL)
   */
  kmac_app_handler(kmac_ip *parent, unsigned int index)
      : parent_model(parent), app_index(index) {}

  /**
   * @brief Application initiates hash operation with data beat
   * @param data 64-bit data word
   * @param strobe Byte enable mask (bit 0 = byte 0 valid, etc.)
   * @param last True if this is the final data beat
   */
  virtual void app_request(uint64_t data, uint8_t strobe, bool last) override {
    if (parent_model) {
      parent_model->handle_app_request(app_index, data, strobe, last);
    }
  }

  /**
   * @brief Check if hash operation is complete
   * @return true if digest is ready, false if still processing
   */
  virtual bool is_done() const override {
    if (parent_model) {
      return parent_model->app_operation_done;
    }
    return false;
  }

  /**
   * @brief Get digest result in two-share form
   * @param[out] share0 Pointer to buffer for digest share 0 (256 bits = 8
   * words)
   * @param[out] share1 Pointer to buffer for digest share 1 (256 bits = 8
   * words)
   *
   * @note This method clears application interface state after retrieving
   * digest, allowing software MMIO access to resume.
   */
  virtual void get_digest(uint32_t *share0, uint32_t *share1) const override {
    if (parent_model && share0 && share1) {
      std::memcpy(share0, parent_model->app_digest_share0,
                  sizeof(parent_model->app_digest_share0));
      std::memcpy(share1, parent_model->app_digest_share1,
                  sizeof(parent_model->app_digest_share1));
      // Clear application interface state - operation complete
      parent_model->clear_app_state();
    }
  }

  /**
   * @brief Check if error occurred during operation
   * @return true if error detected, false otherwise
   */
  virtual bool has_error() const override {
    if (parent_model) {
      return parent_model->app_operation_error;
    }
    return false;
  }

private:
  mutable kmac_ip
      *parent_model; ///< Parent KMAC model instance (mutable for state cleanup)
  unsigned int app_index; ///< Application interface index
};

/******************************************************************************
 * @brief KMAC model constructor
 *
 * Initializes all port interfaces, allocates dynamic application interface
 * array, and registers SystemC processes for functional modeling.
 ******************************************************************************/
kmac_ip::kmac_ip(sc_module_name n,
                 unsigned int memory_size,
                 unsigned int num_app_intf, bool en_masking)
    : kmac_base(n, memory_size),
      verbosity("verbosity", CSML_DEFAULT_VERBOSITY),
      keymgr_tl_socket("keymgr_tl_socket"),
      app_export(nullptr), idle_o("idle_o"),
      lc_escalate_en_i("lc_escalate_en_i"), rst_ni("rst_ni"), clk_i("clk_i"),
      NumAppIntf(num_app_intf), EnMasking(en_masking),
      fsm_state(KmacState::IDLE), idle_update_event("idle_update_event"),
      status_update_event("status_update_event"), evp_md_ctx(nullptr),
      evp_mac(nullptr), evp_mac_ctx(nullptr), packer_position(0), fifo_depth(0),
      max_fifo_depth(10), digest_size(0), xof_output_offset(0),
      xof_total_length(0), app_handlers(nullptr),
      app_interface_active(false), active_app_index(0),
      app_operation_done(false), app_operation_error(false),
      sw_seed_write_count(0), entropy_sw_mode_ready(false),
      logger(),  // logger declared after app_handlers in class, so initialize after
      m_qk() // FUNC-KMAC-024: Initialize quantum keeper for temporal decoupling
{
  // FUNC-KMAC-024: Reset quantum keeper and set to default global quantum
  m_qk.reset();

  // FUNC-KMAC-014: Initialize software mode entropy seed buffer
  std::memset(sw_seed_buffer, 0, sizeof(sw_seed_buffer));

  // FUNC-KMAC-006: Initialize KeyMgr push sideload buffers
  std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
  std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));
  m_keymgr_key_len_bytes = 0;
  m_keymgr_key_valid = false;

  // Register KeyMgr push sideload handler
  keymgr_tl_socket.register_b_transport(this, &kmac_ip::keymgr_b_transport);

  // Initialize packer buffer to zero
  std::memset(packer_buffer, 0, sizeof(packer_buffer));

  // Initialize digest buffers to zero
  std::memset(digest_buffer, 0, sizeof(digest_buffer));
  std::memset(digest_share0, 0, sizeof(digest_share0));
  std::memset(digest_share1, 0, sizeof(digest_share1));
  std::memset(xof_full_output, 0, sizeof(xof_full_output));
  // Configure logger
  logger.setMaxVerbosity(verbosity.get_param_value());
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  // Initialize OpenSSL EVP context
  evp_md_ctx = EVP_MD_CTX_new();
  if (evp_md_ctx == nullptr) {
    CSML_ERROR(1, logger) << "Failed to allocate OpenSSL EVP_MD_CTX";
  }

  // Initialize application interface state
  std::memset(app_digest_share0, 0, sizeof(app_digest_share0));
  std::memset(app_digest_share1, 0, sizeof(app_digest_share1));
  app_algorithm[0] = 0; // KMAC (KeyMgr)
  app_algorithm[1] = 1; // cSHAKE128 (LC_CTRL)
  app_algorithm[2] = 2; // cSHAKE256 (ROM_CTRL)

  // Allocate application interface array based on NumAppIntf parameter
  if (NumAppIntf > 0) {
    app_export = new sc_export<kmac_app_if>[NumAppIntf];

    // Allocate and bind application interface handlers
    app_handlers = new kmac_app_handler *[NumAppIntf];
    for (unsigned int i = 0; i < NumAppIntf; i++) {
      app_handlers[i] = new kmac_app_handler(this, i);
      app_export[i](*app_handlers[i]);
      CSML_INFO(2, logger) << "Application interface [" << i
                           << "] handler created and bound (algorithm="
                           << app_algorithm[i] << ")";
    }
  }

  // Initialize output ports to default values
  idle_o.initialize(true); // Start in IDLE state

  // Register main processing thread with reset sensitivity
  SC_THREAD(reset_and_escalation_process);
  sensitive << rst_ni.neg(); // Sensitive to falling edge of active-low reset

  // Register idle_o driver SC_METHOD (single-writer pattern)
  SC_METHOD(idle_o_driver);
  sensitive << idle_update_event;
  dont_initialize();

  // Register intr_o driver SC_METHOD (single-writer pattern for interrupt output)
  SC_METHOD(intr_o_driver);
  sensitive << intr_update_event;
  dont_initialize();

  // Initialize all registers to their reset values
  reset_all_registers();

  // FUNC-KMAC-005: Register KEY_SHARE0 and KEY_SHARE1 write callbacks
  // (protected by CFG_REGWEN)
  for (unsigned int i = 0; i < 16; i++) {
    // KEY_SHARE0 callback
    std::function<bool(uint32_t, uint8_t)> key_share0_write =
        [this, i](uint32_t value, uint8_t be) {
          return this->handle_write_KEY_SHARE0(i, value, be);
        };
    memory.register_write_callback_with_be(
        key_share0_write,
        (0x30 / sizeof(uint32_t)) + i // Offset 0x30 + word index
    );

    // KEY_SHARE1 callback
    std::function<bool(uint32_t, uint8_t)> key_share1_write =
        [this, i](uint32_t value, uint8_t be) {
          return this->handle_write_KEY_SHARE1(i, value, be);
        };
    memory.register_write_callback_with_be(
        key_share1_write,
        (0x70 / sizeof(uint32_t)) + i // Offset 0x70 + word index
    );

    // KEY_SHARE0 read callback (write-only, return 0)
    std::function<bool(uint32_t &)> key_share0_read = [this,
                                                       i](uint32_t &value) {
      value = 0; // KEY_SHARE registers are write-only, return 0 per spec
      return true;
    };
    memory.register_read_callback(key_share0_read,
                                  (0x30 / sizeof(uint32_t)) + i);

    // KEY_SHARE1 read callback (write-only, return 0)
    std::function<bool(uint32_t &)> key_share1_read = [this,
                                                       i](uint32_t &value) {
      value = 0; // KEY_SHARE registers are write-only, return 0 per spec
      return true;
    };
    memory.register_read_callback(key_share1_read,
                                  (0x70 / sizeof(uint32_t)) + i);
  }

  // Register KEY_LEN write callback
  std::function<bool(uint32_t, uint8_t)> key_len_write = [this](uint32_t value,
                                                                uint8_t be) {
    return this->handle_write_KEY_LEN(value, be);
  };
  memory.register_write_callback_with_be(key_len_write, KEY_LEN.offset);

  // Register CFG_SHADOWED write callback
  std::function<bool(uint32_t, uint8_t)> cfg_shadowed_write =
      [this](uint32_t value, uint8_t be) {
        return this->handle_write_CFG_SHADOWED(value, be);
      };
  memory.register_write_callback_with_be(cfg_shadowed_write,
                                         CFG_SHADOWED.offset);

  CSML_INFO(2, logger)
      << "KEY_SHARE0/1, KEY_LEN, CFG_SHADOWED callbacks registered";
  CSML_INFO(2, logger) << "Key protection enabled:";
  CSML_INFO(2, logger)
      << "  - CFG_REGWEN auto-clear on START, auto-set on DONE";
  CSML_INFO(2, logger) << "  - Key zeroization on reset/error";
  CSML_INFO(2, logger) << "  - Shadow register validation for CFG_SHADOWED";

  // FUNC-KMAC-014: Register ENTROPY_SEED write callback
  std::function<bool(uint32_t, uint32_t)> entropy_seed_write =
      [this](uint32_t value, uint32_t write_mask) {
        return this->handle_write_ENTROPY_SEED(value, write_mask);
      };
  memory.register_write_callback_with_be(
      [this, entropy_seed_write](uint32_t value, uint8_t be) {
        uint32_t write_mask = 0xFFFFFFFF; // Full 32-bit access for ENTROPY_SEED
        return entropy_seed_write(value, write_mask);
      },
      ENTROPY_SEED.offset);

  CSML_INFO(2, logger) << "ENTROPY_SEED callback registered for FUNC-KMAC-014 "
                          "(Software Mode entropy seeding)";

  // FUNC-KMAC-017: Register ENTROPY_PERIOD write callback (CFG_REGWEN protected)
  std::function<bool(uint32_t, uint8_t)> entropy_period_write =
      [this](uint32_t value, uint8_t be) {
        return this->handle_write_ENTROPY_PERIOD(value, be);
      };
  memory.register_write_callback_with_be(entropy_period_write,
                                         ENTROPY_PERIOD.offset);

  CSML_INFO(2, logger) << "ENTROPY_PERIOD callback registered for FUNC-KMAC-017 "
                          "(CFG_REGWEN protection)";

  // FUNC-KMAC-017: Register PREFIX write callbacks (CFG_REGWEN protected)
  // PREFIX is an array of 11 registers at offset 0xB4
  for (unsigned int i = 0; i < 11; i++) {
    std::function<bool(uint32_t, uint8_t)> prefix_write =
        [this, i](uint32_t value, uint8_t be) {
          return this->handle_write_PREFIX(i, value, be);
        };
    memory.register_write_callback_with_be(
        prefix_write,
        (0xB4 / sizeof(uint32_t)) + i // Offset 0xB4 + word index
    );
  }

  CSML_INFO(2, logger) << "PREFIX[0..10] callbacks registered for FUNC-KMAC-017 "
                          "(CFG_REGWEN protection)";

  // Register CMD write callback
  std::function<bool(uint32_t, uint32_t)> cmd_write =
      [this](uint32_t value, uint32_t write_mask) {
        return this->handle_write_CMD(value, write_mask);
      };
  memory.register_write_callback_with_be(
      [this, cmd_write](uint32_t value, uint8_t be) {
        uint32_t write_mask = 0xFFFFFFFF; // Full 32-bit access for CMD
        return cmd_write(value, write_mask);
      },
      CMD.offset);

  // Register STATUS read callback
  std::function<bool(uint32_t &, uint32_t)> status_read =
      [this](uint32_t &value, uint32_t read_mask) {
        return this->handle_read_STATUS(value, read_mask);
      };
  memory.register_read_callback(
      [this, status_read](uint32_t &value) {
        uint32_t read_mask = 0xFFFFFFFF;
        return status_read(value, read_mask);
      },
      STATUS.offset);

  // Register MSG_FIFO window write callback (0x800-0xFFC)
  // Memory window covers 2048 bytes (512 32-bit words)
  for (unsigned int i = 0; i < 512; i++) {
    std::function<bool(uint32_t, uint8_t)> msg_fifo_write =
        [this, i](uint32_t value, uint8_t be) {
          return this->handle_write_MSG_FIFO(i, value, be);
        };
    memory.register_write_callback_with_be(
        msg_fifo_write,
        (0x800 / sizeof(uint32_t)) + i // Offset 0x800 + word index
    );
  }

  // Register STATE window read callback (0x400-0x5FC)
  // Memory window covers 512 bytes (128 32-bit words)
  for (unsigned int i = 0; i < 128; i++) {
    std::function<bool(uint32_t &)> state_read = [this, i](uint32_t &value) {
      return this->handle_read_STATE(i, value);
    };
    memory.register_read_callback(
        state_read,
        (0x400 / sizeof(uint32_t)) + i // Offset 0x400 + word index
    );
  }

  // Register INTR_STATE write callback for W1C behavior and interrupt output update
  // INTR_STATE is W1C (write-1-to-clear): writing 1 clears the corresponding bit
  memory.register_write_callback_with_be(
      [this](uint32_t value, uint8_t be) {
        // Implement W1C: clear bits where written value has 1s
        uint32_t w1c_mask = value & 0x7; // Only bits [2:0] are W1C
        if (w1c_mask & 0x1) INTR_STATE.kmac_done = 0;
        if (w1c_mask & 0x2) INTR_STATE.fifo_empty = 0;
        if (w1c_mask & 0x4) INTR_STATE.kmac_err = 0;
        // Trigger interrupt evaluation after clearing
        evaluate_interrupt();
        return false; // Don't let CSML do default write (we handled it)
      },
      INTR_STATE.offset);

  // Register INTR_TEST write callback - forces interrupt assertion for testing
  // Writing 1 to a bit forces the corresponding interrupt in INTR_STATE
  memory.register_write_callback(
      [this](uint32_t value) {
        if (value & 0x1) {
          INTR_STATE.kmac_done = 1;
          CSML_INFO(2, logger) << "INTR_TEST: Forcing kmac_done interrupt";
        }
        if (value & 0x2) {
          INTR_STATE.fifo_empty = 1;
          CSML_INFO(2, logger) << "INTR_TEST: Forcing fifo_empty interrupt";
        }
        if (value & 0x4) {
          INTR_STATE.kmac_err = 1;
          CSML_INFO(2, logger) << "INTR_TEST: Forcing kmac_err interrupt";
        }
        evaluate_interrupt();
        return false; // Write does not store in INTR_TEST register itself
      },
      INTR_TEST.offset);

  // Register INTR_ENABLE write callback for interrupt output update
  // When INTR_ENABLE changes, the interrupt output may need to change
  memory.register_write_callback_with_be(
      [this](uint32_t value, uint8_t be) {
        // Update the register value
        if (be & 0x1) INTR_ENABLE.kmac_done = value & 0x1;
        if (be & 0x1) INTR_ENABLE.fifo_empty = (value >> 1) & 0x1;
        if (be & 0x1) INTR_ENABLE.kmac_err = (value >> 2) & 0x1;
        // Trigger interrupt evaluation
        evaluate_interrupt();
        CSML_INFO(2, logger) << "INTR_ENABLE written: 0x" << std::hex << value
                             << std::dec << " - interrupt re-evaluated";
        return true;
      },
      INTR_ENABLE.offset);

  CSML_INFO(2, logger) << "KMAC model instantiated with:"
                       << " NumAppIntf=" << NumAppIntf
                       << " EnMasking=" << (EnMasking ? "true" : "false")
                       << " MemorySize=0x" << std::hex << memory_size
                       << std::dec;
  CSML_INFO(2, logger) << "Registers initialized to reset values";
  CSML_INFO(2, logger) << "MSG_FIFO window (0x800-0xFFC) and STATE window "
                          "(0x400-0x5FC) callbacks registered";
  CSML_INFO(2, logger) << "INTR_STATE/INTR_ENABLE callbacks registered for "
                          "interrupt output signaling";
}

/******************************************************************************
 * @brief KMAC model destructor
 *
 * Cleans up dynamically allocated application interface array.
 ******************************************************************************/
kmac_ip::~kmac_ip() {
  // Cleanup OpenSSL EVP context
  if (evp_md_ctx != nullptr) {
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(evp_md_ctx));
    evp_md_ctx = nullptr;
  }

  // Cleanup OpenSSL EVP_MAC contexts
  if (evp_mac_ctx != nullptr) {
    EVP_MAC_CTX_free(static_cast<EVP_MAC_CTX *>(evp_mac_ctx));
    evp_mac_ctx = nullptr;
  }
  if (evp_mac != nullptr) {
    EVP_MAC_free(static_cast<EVP_MAC *>(evp_mac));
    evp_mac = nullptr;
  }

  // Cleanup application interface handlers
  if (app_handlers != nullptr) {
    for (unsigned int i = 0; i < NumAppIntf; i++) {
      delete app_handlers[i];
      app_handlers[i] = nullptr;
    }
    delete[] app_handlers;
    app_handlers = nullptr;
  }

  // Cleanup application interface array
  if (app_export != nullptr) {
    delete[] app_export;
    app_export = nullptr;
  }
}

/******************************************************************************
 * @brief TLM b_transport handler for KeyMgr push sideload socket
 *
 * KeyMgr writes 8 key words at offsets 0x00–0x1C (KEY_SHARE0), 8 words at
 * 0x20–0x3C (KEY_SHARE1), then KEY_CTRL=1 at 0x40 to commit the key.
 ******************************************************************************/
void kmac_ip::keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
  if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    return;
  }

  uint64_t offset = trans.get_address();
  uint32_t* data  = reinterpret_cast<uint32_t*>(trans.get_data_ptr());

  if (offset <= 0x1C && (offset % 4) == 0) {
    // KEY_SHARE0 word: track how many bytes of share0 have been written
    unsigned int word_idx = offset / 4;
    m_keymgr_share0[word_idx] = *data;
    size_t written_bytes = (word_idx + 1) * 4;
    if (written_bytes > m_keymgr_key_len_bytes) {
      m_keymgr_key_len_bytes = written_bytes;
    }
  } else if (offset >= 0x20 && offset <= 0x3C && (offset % 4) == 0) {
    // KEY_SHARE1 words
    m_keymgr_share1[(offset - 0x20) / 4] = *data;
  } else if (offset == 0x40) {
    // KEY_CTRL: bit[0]=1 commits key valid, bit[0]=0 clears it
    m_keymgr_key_valid = (*data & 0x1u) != 0;
    if (m_keymgr_key_valid) {
      CSML_INFO(2, logger) << "[KMAC] KeyMgr sideload key committed ("
                           << m_keymgr_key_len_bytes << " bytes)";
    } else {
      CSML_INFO(2, logger) << "[KMAC] KeyMgr sideload key invalidated";
    }
  }

  trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

/******************************************************************************
 * @brief Reset and escalation handling process
 *
 * Monitors reset and lifecycle escalation signals. Handles:
 * - Hardware reset (rst_ni low): Resets all registers and internal state
 * - Lifecycle escalation (lc_escalate_en_i): Zeroizes keys and locks FSM
 *
 * Note: FSM state transitions (IDLE → ABSORB → SQUEEZE) are handled by
 * register callbacks (handle_write_CMD), not this process.
 ******************************************************************************/
void kmac_ip::reset_and_escalation_process() {
  while (true) {
    // Check for reset (active-low)
    if (!rst_ni.read()) {
      CSML_INFO(2, logger) << "Reset asserted - reinitializing KMAC";

      // Reset all registers to default values
      reset_all_registers();

      // Reset internal FSM state
      fsm_state = KmacState::IDLE;
      app_interface_active = false; // Clear application interface active flag

      // FUNC-KMAC-001: Reset SHA3 internal state variables
      packer_position = 0;
      std::memset(packer_buffer, 0, sizeof(packer_buffer));

      // Clear FIFO and FIFO Depth
      msg_fifo = {};
      fifo_depth = 0;

      // Clear digest buffers
      digest_size = 0;
      std::memset(digest_buffer, 0, sizeof(digest_buffer));
      std::memset(digest_share0, 0, sizeof(digest_share0));
      std::memset(digest_share1, 0, sizeof(digest_share1));

      // FUNC-KMAC-014: Reset software mode entropy state
      sw_seed_write_count = 0;
      entropy_sw_mode_ready = false;
      std::memset(sw_seed_buffer, 0, sizeof(sw_seed_buffer));
      CSML_INFO(2, logger)
          << "FUNC-KMAC-014: Software mode entropy state reset";

      // FUNC-KMAC-006: Clear KeyMgr sideload buffers on reset
      // KEY_CTRL.key_valid resets to 0 per RDL, so keymgr_key_i.valid = 0 after reset
      std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
      std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));
      m_keymgr_key_valid = false;
      m_keymgr_key_len_bytes = 0;
      CSML_INFO(2, logger) << "FUNC-KMAC-006: KeyMgr sideload buffers cleared";

      // Reset OpenSSL context
      if (evp_md_ctx != nullptr) {
        EVP_MD_CTX_reset(static_cast<EVP_MD_CTX *>(evp_md_ctx));
      }

      // Notify driver to update idle_o port
      idle_update_event.notify(SC_ZERO_TIME);

      // Wait for reset deassertion
      wait(rst_ni.posedge_event());
      CSML_INFO(2, logger) << "Reset deasserted - KMAC ready";
      continue;
    }

    // Wait for register activity, escalation, or timeout
    // Use wait() with event sensitivity to detect escalation immediately
    wait(10, SC_NS, lc_escalate_en_i.posedge_event());

    // FUNC-KMAC-021: Monitor escalation input (non-blocking check)
    // Escalation can occur at any time and is irreversible until reset
    if (lc_escalate_en_i.read() && fsm_state != KmacState::ESCALATION_LOCKED) {
      CSML_ERROR(1, logger) << "FUNC-KMAC-021: Life cycle escalation detected "
                               "- initiating security response";

      // Immediate transition to ESCALATION_LOCKED state (blocks all operations)
      update_fsm_state(KmacState::ESCALATION_LOCKED);

      // Set fatal error in STATUS.ALERT_FATAL_FAULT (bit 17)
      STATUS.ALERT_FATAL_FAULT = 1;

      // Set ERR_CODE to indicate escalation (fatal error)
      // Use KeyNotValid (0x01) as fatal escalation indicator per architecture
      // map
      ERR_CODE = 0x01000000; // Fatal error - escalation

      // FUNC-KMAC-021: Immediate key zeroization - KEY_SHARE0 and KEY_SHARE1
      CSML_INFO(2, logger)
          << "FUNC-KMAC-021: Zeroizing KEY_SHARE0 and KEY_SHARE1 registers";
      zeroize_key_registers();

      // FUNC-KMAC-021: Clear PREFIX registers (prevent function name leakage)
      for (size_t i = 0; i < 11; i++) {
        PREFIX[i].prefix = 0;
      }

      // FUNC-KMAC-021: Clear internal Keccak state (1600-bit state)
      if (evp_md_ctx != nullptr) {
        EVP_MD_CTX_reset(static_cast<EVP_MD_CTX *>(evp_md_ctx));
      }
      if (evp_mac_ctx != nullptr) {
        EVP_MAC_CTX_free(static_cast<EVP_MAC_CTX *>(evp_mac_ctx));
        evp_mac_ctx = nullptr;
      }

      // FUNC-KMAC-021: Clear internal buffers - packer, FIFO, digest
      std::memset(packer_buffer, 0, sizeof(packer_buffer));
      packer_position = 0;

      // Clear FIFO contents
      while (!msg_fifo.empty()) {
        msg_fifo.pop();
      }
      fifo_depth = 0;

      // Clear digest buffers
      std::memset(digest_buffer, 0, sizeof(digest_buffer));
      std::memset(digest_share0, 0, sizeof(digest_share0));
      std::memset(digest_share1, 0, sizeof(digest_share1));
      digest_size = 0;

      // Clear XOF output state
      std::memset(xof_full_output, 0, sizeof(xof_full_output));
      xof_output_offset = 0;
      xof_total_length = 0;

      // FUNC-KMAC-021: Clear application interface buffers
      std::memset(app_digest_share0, 0, sizeof(app_digest_share0));
      std::memset(app_digest_share1, 0, sizeof(app_digest_share1));

      // FUNC-KMAC-014: Clear entropy state
      std::memset(sw_seed_buffer, 0, sizeof(sw_seed_buffer));
      sw_seed_write_count = 0;
      entropy_sw_mode_ready = false;

      CSML_ERROR(1, logger) << "FUNC-KMAC-021: Escalation response complete";
      CSML_ERROR(1, logger) << "  - FSM locked in ESCALATION_LOCKED state";
      CSML_ERROR(1, logger)
          << "  - All keys, buffers, and internal state zeroized";
      CSML_ERROR(1, logger) << "  - All operations blocked until reset";
      CSML_ERROR(1, logger) << "  - STATUS.ALERT_FATAL_FAULT set (bit 17)";
      CSML_ERROR(1, logger)
          << "  - Only rst_ni assertion can restore functionality";
    }
  }
}

/******************************************************************************
 * @brief idle_o port driver (single-writer pattern)
 *
 * Updates idle_o signal based on FSM state.
 * This is the ONLY method that writes to idle_o.
 *
 * FUNC-KMAC-021: ESCALATION_LOCKED state reports idle_o=0 (not operational)
 ******************************************************************************/
void kmac_ip::idle_o_driver() {
  // FUNC-KMAC-021: ESCALATION_LOCKED is not idle (system locked down)
  bool idle_value = (fsm_state == KmacState::IDLE);
  idle_o.write(idle_value);

  CSML_INFO(2, logger) << "idle_o updated to " << idle_value
                       << " (fsm_state=" << static_cast<int>(fsm_state) << ")";
}

/******************************************************************************
 * @brief intr_o port driver (single-writer pattern)
 *
 * Updates intr_o signal based on (INTR_STATE & INTR_ENABLE) != 0.
 * This is the ONLY method that writes to intr_o.
 ******************************************************************************/
void kmac_ip::intr_o_driver() {
  // Compute combined interrupt: (INTR_STATE & INTR_ENABLE) != 0
  uint32_t intr_state_val = (INTR_STATE.kmac_done & 0x1) |
                            ((INTR_STATE.fifo_empty & 0x1) << 1) |
                            ((INTR_STATE.kmac_err & 0x1) << 2);
  uint32_t intr_enable_val = (INTR_ENABLE.kmac_done & 0x1) |
                             ((INTR_ENABLE.fifo_empty & 0x1) << 1) |
                             ((INTR_ENABLE.kmac_err & 0x1) << 2);

  bool intr_active = (intr_state_val & intr_enable_val) != 0;
  intr_o.write(intr_active);

  CSML_INFO(2, logger) << "intr_o updated to " << intr_active
                       << " (INTR_STATE=0x" << std::hex << intr_state_val
                       << " INTR_ENABLE=0x" << intr_enable_val << std::dec << ")";
}

/******************************************************************************
 * @brief Evaluate and update interrupt output
 *
 * Call this after any change to INTR_STATE or INTR_ENABLE registers.
 * Notifies intr_update_event to trigger intr_o_driver.
 ******************************************************************************/
void kmac_ip::evaluate_interrupt() {
  intr_update_event.notify(SC_ZERO_TIME);
}

/******************************************************************************
 * @brief Helper function to update FSM state and notify drivers
 * @param new_state New FSM state to transition to
 ******************************************************************************/
void kmac_ip::update_fsm_state(KmacState new_state) {
  if (fsm_state != new_state) {
    KmacState old_state = fsm_state;
    fsm_state = new_state;

    CSML_INFO(2, logger) << "FSM state transition: "
                         << static_cast<int>(old_state) << " -> "
                         << static_cast<int>(new_state);

    // Notify driver events (SC_ZERO_TIME for immediate update)
    idle_update_event.notify(SC_ZERO_TIME);
    status_update_event.notify(SC_ZERO_TIME);
  }
}

/******************************************************************************
 * @brief CFG_SHADOWED register write callback handler (FUNC-KMAC-005)
 ******************************************************************************/
bool kmac_ip::handle_write_CFG_SHADOWED(uint32_t value, uint8_t byte_enable) {
  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger) << "CFG_SHADOWED write rejected: CFG_REGWEN.en=0 "
                            "(configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger)
        << "CFG_SHADOWED write rejected: not in IDLE state (fsm_state="
        << static_cast<int>(fsm_state) << ")";
    return false;
  }

  static uint32_t shadow_pending_value = 0;
  static bool shadow_pending = false;

  if (!shadow_pending) {
    shadow_pending_value = value;
    shadow_pending = true;
    CSML_INFO(2, logger) << "CFG_SHADOWED first write: value=0x" << std::hex
                         << value << std::dec
                         << " (awaiting second write for shadow validation)";
    return true;
  } else {
    if (value == shadow_pending_value) {
      CFG_SHADOWED.kmac_en = (value >> 0) & 0x1;
      CFG_SHADOWED.kstrength = (value >> 1) & 0x7;
      CFG_SHADOWED.mode = (value >> 4) & 0x3;
      CFG_SHADOWED.msg_endianness = (value >> 8) & 0x1;
      CFG_SHADOWED.state_endianness = (value >> 9) & 0x1;
      CFG_SHADOWED.sideload = (value >> 12) & 0x1;
      CFG_SHADOWED.entropy_mode = (value >> 16) & 0x3;
      CFG_SHADOWED.entropy_fast_process = (value >> 19) & 0x1;
      CFG_SHADOWED.msg_mask = (value >> 20) & 0x1;
      CFG_SHADOWED.entropy_ready = (value >> 24) & 0x1;
      CFG_SHADOWED.en_unsupported_modestrength = (value >> 26) & 0x1;

      shadow_pending = false;
      CSML_INFO(2, logger)
          << "CFG_SHADOWED second write matched: configuration committed";
      CSML_INFO(2, logger) << "  kmac_en=" << CFG_SHADOWED.kmac_en << " mode=0x"
                           << std::hex << CFG_SHADOWED.mode << std::dec
                           << " kstrength=0x" << std::hex
                           << CFG_SHADOWED.kstrength << std::dec
                           << " sideload=" << CFG_SHADOWED.sideload;
      
      // FUNC-KMAC-014: Reset entropy state when entropy_mode is configured
      // This allows fresh seeding when switching to SW mode or reconfiguring
      if (CFG_SHADOWED.entropy_mode == 0x2) {
        // SW entropy mode - reset seed counter to allow new 6-write sequence
        sw_seed_write_count = 0;
        entropy_sw_mode_ready = false;
        std::memset(sw_seed_buffer, 0, sizeof(sw_seed_buffer));
        CSML_INFO(2, logger) << "  FUNC-KMAC-014: SW entropy mode configured, "
                             << "seed counter reset (ready for 6-write sequence)";
      }
      
      return true;
    } else {
      STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = 1;
      shadow_pending = false;
      CSML_ERROR(1, logger) << "CFG_SHADOWED shadow mismatch detected:";
      CSML_ERROR(1, logger) << "  First write:  0x" << std::hex
                            << shadow_pending_value << std::dec;
      CSML_ERROR(1, logger)
          << "  Second write: 0x" << std::hex << value << std::dec;
      CSML_ERROR(1, logger)
          << "  STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=1 (recoverable)";
      return false;
    }
  }
}

/******************************************************************************
 * @brief KEY_SHARE0 register write callback handler (FUNC-KMAC-005)
 ******************************************************************************/
bool kmac_ip::handle_write_KEY_SHARE0(unsigned int index, uint32_t value,
                                      uint8_t byte_enable) {
  if (index >= 16) {
    CSML_ERROR(1, logger) << "KEY_SHARE0 write rejected: invalid index "
                          << index;
    return false;
  }

  // FUNC-KMAC-021: Block all register writes when in ESCALATION_LOCKED state
  if (fsm_state == KmacState::ESCALATION_LOCKED) {
    CSML_ERROR(1, logger) << "KEY_SHARE0[" << index
                          << "] write rejected: FSM in ESCALATION_LOCKED state "
                             "(lc_escalate_en_i active)";
    return false;
  }

  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger)
        << "KEY_SHARE0[" << index
        << "] write rejected: CFG_REGWEN.en=0 (configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger) << "KEY_SHARE0[" << index
                         << "] write rejected: not in IDLE state (fsm_state="
                         << static_cast<int>(fsm_state) << ")";
    return false;
  }

  uint32_t current_value = KEY_SHARE0[index].key;
  uint32_t masked_value = current_value;

  for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
    if (byte_enable & (1 << byte_idx)) {
      uint32_t byte_mask = 0xFF << (byte_idx * 8);
      masked_value = (masked_value & ~byte_mask) | (value & byte_mask);
    }
  }

  KEY_SHARE0[index].key = masked_value;
  CSML_INFO(2, logger) << "KEY_SHARE0[" << index << "] written: 0x" << std::hex
                       << masked_value << std::dec << " (be=0x" << std::hex
                       << (int)byte_enable << std::dec << ")";
  return true;
}

/******************************************************************************
 * @brief KEY_SHARE1 register write callback handler (FUNC-KMAC-005)
 ******************************************************************************/
bool kmac_ip::handle_write_KEY_SHARE1(unsigned int index, uint32_t value,
                                      uint8_t byte_enable) {
  if (index >= 16) {
    CSML_ERROR(1, logger) << "KEY_SHARE1 write rejected: invalid index "
                          << index;
    return false;
  }

  // FUNC-KMAC-021: Block all register writes when in ESCALATION_LOCKED state
  if (fsm_state == KmacState::ESCALATION_LOCKED) {
    CSML_ERROR(1, logger) << "KEY_SHARE1[" << index
                          << "] write rejected: FSM in ESCALATION_LOCKED state "
                             "(lc_escalate_en_i active)";
    return false;
  }

  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger)
        << "KEY_SHARE1[" << index
        << "] write rejected: CFG_REGWEN.en=0 (configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger) << "KEY_SHARE1[" << index
                         << "] write rejected: not in IDLE state (fsm_state="
                         << static_cast<int>(fsm_state) << ")";
    return false;
  }

  uint32_t current_value = KEY_SHARE1[index].key;
  uint32_t masked_value = current_value;

  for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
    if (byte_enable & (1 << byte_idx)) {
      uint32_t byte_mask = 0xFF << (byte_idx * 8);
      masked_value = (masked_value & ~byte_mask) | (value & byte_mask);
    }
  }

  KEY_SHARE1[index].key = masked_value;
  CSML_INFO(2, logger) << "KEY_SHARE1[" << index << "] written: 0x" << std::hex
                       << masked_value << std::dec << " (be=0x" << std::hex
                       << (int)byte_enable << std::dec << ")";
  return true;
}

/******************************************************************************
 * @brief KEY_LEN register write callback handler (FUNC-KMAC-005)
 ******************************************************************************/
bool kmac_ip::handle_write_KEY_LEN(uint32_t value, uint8_t byte_enable) {
  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger)
        << "KEY_LEN write rejected: CFG_REGWEN.en=0 (configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger)
        << "KEY_LEN write rejected: not in IDLE state (fsm_state="
        << static_cast<int>(fsm_state) << ")";
    return false;
  }

  uint32_t len_value = value & 0x7;

  if (len_value > 0x4) {
    CSML_ERROR(1, logger) << "KEY_LEN write rejected: invalid len value 0x"
                          << std::hex << len_value << std::dec
                          << " (valid range: 0x0-0x4)";
    return false;
  }

  KEY_LEN.len = len_value;

  static const uint32_t key_bits_table[] = {128, 192, 256, 384, 512};
  static const uint32_t key_bytes_table[] = {16, 24, 32, 48, 64};
  uint32_t key_bits = key_bits_table[len_value];
  uint32_t key_bytes = key_bytes_table[len_value];

  CSML_INFO(2, logger) << "KEY_LEN written: len=0x" << std::hex << len_value
                       << std::dec << " (" << key_bits << " bits / "
                       << key_bytes << " bytes)";
  return true;
}

/******************************************************************************
 * @brief ENTROPY_PERIOD register write callback handler (FUNC-KMAC-017)
 *
 * ENTROPY_PERIOD is protected by CFG_REGWEN. Writes are rejected when
 * CFG_REGWEN.en = 0 (configuration locked during active operations).
 ******************************************************************************/
bool kmac_ip::handle_write_ENTROPY_PERIOD(uint32_t value, uint8_t byte_enable) {
  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger)
        << "ENTROPY_PERIOD write rejected: CFG_REGWEN.en=0 (configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger)
        << "ENTROPY_PERIOD write rejected: not in IDLE state (fsm_state="
        << static_cast<int>(fsm_state) << ")";
    return false;
  }

  // Apply byte enables to write value
  uint32_t current_value = ENTROPY_PERIOD;
  uint32_t masked_value = current_value;
  for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
    if (byte_enable & (1 << byte_idx)) {
      uint32_t byte_mask = 0xFF << (byte_idx * 8);
      masked_value = (masked_value & ~byte_mask) | (value & byte_mask);
    }
  }

  // Apply valid bits mask (prescaler[9:0] and wait_timer[31:16])
  uint32_t valid_mask = 0xFFFF03FF;
  ENTROPY_PERIOD = masked_value & valid_mask;

  CSML_INFO(2, logger) << "ENTROPY_PERIOD written: 0x" << std::hex << (masked_value & valid_mask)
                       << std::dec << " (prescaler=" << (masked_value & 0x3FF)
                       << ", wait_timer=" << ((masked_value >> 16) & 0xFFFF) << ")";
  return true;
}

/******************************************************************************
 * @brief PREFIX register write callback handler (FUNC-KMAC-017)
 *
 * PREFIX registers are protected by CFG_REGWEN. Writes are rejected when
 * CFG_REGWEN.en = 0 (configuration locked during active operations).
 ******************************************************************************/
bool kmac_ip::handle_write_PREFIX(unsigned int index, uint32_t value, uint8_t byte_enable) {
  if (CFG_REGWEN.en == 0) {
    CSML_INFO(2, logger)
        << "PREFIX[" << index << "] write rejected: CFG_REGWEN.en=0 (configuration locked)";
    return false;
  }

  if (fsm_state != KmacState::IDLE) {
    CSML_INFO(2, logger)
        << "PREFIX[" << index << "] write rejected: not in IDLE state (fsm_state="
        << static_cast<int>(fsm_state) << ")";
    return false;
  }

  // Apply byte enables to write value
  uint32_t current_value = PREFIX[index].prefix;
  uint32_t masked_value = current_value;
  for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
    if (byte_enable & (1 << byte_idx)) {
      uint32_t byte_mask = 0xFF << (byte_idx * 8);
      masked_value = (masked_value & ~byte_mask) | (value & byte_mask);
    }
  }

  PREFIX[index].prefix = masked_value;
  CSML_INFO(2, logger) << "PREFIX[" << index << "] written: 0x" << std::hex << masked_value
                       << std::dec << " (be=0x" << std::hex << (int)byte_enable << std::dec << ")";
  return true;
}

/******************************************************************************
 * @brief Helper function to zeroize key registers (FUNC-KMAC-005)
 ******************************************************************************/
void kmac_ip::zeroize_key_registers() {
  CSML_INFO(2, logger) << "Zeroizing key registers (security mechanism)";

  for (size_t i = 0; i < 16; i++) {
    KEY_SHARE0[i].key = 0;
    KEY_SHARE1[i].key = 0;
  }

  KEY_LEN.len = 0;

  CSML_INFO(2, logger) << "Key registers zeroized: KEY_SHARE0[0-15]=0, "
                          "KEY_SHARE1[0-15]=0, KEY_LEN=0";
}

/******************************************************************************
 * @brief ENTROPY_SEED register write callback handler (FUNC-KMAC-014)
 *
 * Implements software mode entropy seeding sequence for PRNG initialization.
 * Requires exactly 6 sequential writes to fully seed 192-bit entropy state.
 * Each write loads into corresponding 32-bit chunk of Bivium stream cipher
 * (functionally abstracted in TLM - just tracks seeding state).
 *
 * Prerequisites:
 * - CFG_SHADOWED.entropy_mode = 0x2 (sw_mode)
 * - CFG_SHADOWED.entropy_ready = 1
 *
 * Behavior:
 * - Writes 1-6: Load seed value into sw_seed_buffer[count], increment count
 * - After 6th write: Set entropy_sw_mode_ready flag, activate PRNG
 * - Writes after 6th: Ignored (PRNG already seeded and running)
 *
 * Architecture Map Reference:
 * - FSM: Entropy_Generator_FSM (RESET → SW_SEED_WAIT → READY)
 * - Register: ENTROPY_SEED at offset 0x2C
 * - Side Effects: Sequential chunk loading, PRNG activation after 6 writes
 *
 * @param value Written 32-bit seed chunk value
 * @param write_mask Write enable mask (always 0xFFFFFFFF for 32-bit register)
 * @return true if write processed, false if ignored
 ******************************************************************************/
bool kmac_ip::handle_write_ENTROPY_SEED(uint32_t value, uint32_t write_mask) {
  // Extract entropy_mode and entropy_ready from CFG_SHADOWED
  uint32_t entropy_mode = CFG_SHADOWED.entropy_mode;
  uint32_t entropy_ready = CFG_SHADOWED.entropy_ready;

  CSML_INFO(2, logger) << "ENTROPY_SEED write: value=0x" << std::hex << value
                       << std::dec << " entropy_mode=" << entropy_mode
                       << " entropy_ready=" << entropy_ready
                       << " sw_seed_write_count=" << sw_seed_write_count;

  // Validate entropy_mode = sw_mode (0x2) and entropy_ready = 1
  if (entropy_mode != 0x2) {
    CSML_INFO(2, logger)
        << "ENTROPY_SEED write ignored: entropy_mode != sw_mode (0x2), current="
        << entropy_mode;
    return false;
  }

  if (entropy_ready != 1) {
    CSML_INFO(2, logger) << "ENTROPY_SEED write ignored: entropy_ready != 1";
    return false;
  }

  // Check if seeding already complete (6 writes already received)
  if (sw_seed_write_count >= 6) {
    CSML_INFO(2, logger) << "ENTROPY_SEED write ignored: PRNG already seeded "
                            "(6 writes complete)";
    return false; // PRNG already activated, further writes have no effect
  }

  // Load seed value into buffer at current write count index
  sw_seed_buffer[sw_seed_write_count] = value;
  CSML_INFO(2, logger) << "ENTROPY_SEED chunk[" << sw_seed_write_count
                       << "] = 0x" << std::hex << value << std::dec
                       << " loaded into sw_seed_buffer";

  // Increment write count
  sw_seed_write_count++;

  // Check if this was the 6th write (seeding complete)
  if (sw_seed_write_count == 6) {
    // Activate PRNG (functionally abstracted - no actual Bivium implementation)
    entropy_sw_mode_ready = true;

    CSML_INFO(1, logger)
        << "FUNC-KMAC-014: Software mode entropy seeding COMPLETE";
    CSML_INFO(1, logger) << "  - 6 seed chunks loaded (192 bits total)";
    CSML_INFO(1, logger) << "  - PRNG activated: entropy_sw_mode_ready = true";
    CSML_INFO(1, logger) << "  - KMAC operations can now proceed";
    CSML_INFO(2, logger) << "  - Seed buffer: [" << std::hex << "0x"
                         << sw_seed_buffer[0] << ", "
                         << "0x" << sw_seed_buffer[1] << ", "
                         << "0x" << sw_seed_buffer[2] << ", "
                         << "0x" << sw_seed_buffer[3] << ", "
                         << "0x" << sw_seed_buffer[4] << ", "
                         << "0x" << sw_seed_buffer[5] << std::dec << "]";

    // Architecture Map: FSM transition SW_SEED_WAIT → READY
    CSML_INFO(2, logger)
        << "Entropy FSM: SW_SEED_WAIT → READY (functional abstraction)";
  } else {
    CSML_INFO(2, logger) << "ENTROPY_SEED chunk " << sw_seed_write_count
                         << "/6 received, " << (6 - sw_seed_write_count)
                         << " more required";
  }

  return true;
}

/******************************************************************************
 * @brief CMD register write callback handler
 *
 * Implements FSM state transitions based on sparse-encoded commands and
 * auxiliary command bit processing. START command initializes OpenSSL EVP
 * context for SHA3, SHAKE, cSHAKE, and KMAC modes based on CFG_SHADOWED
 * configuration (mode, kstrength, kmac_en).
 ******************************************************************************/
bool kmac_ip::handle_write_CMD(uint32_t value, uint32_t write_mask) {
  // FUNC-KMAC-021: Block all CMD writes when in ESCALATION_LOCKED state
  // Escalation requires hardware reset (rst_ni) for recovery - no software
  // recovery allowed
  if (fsm_state == KmacState::ESCALATION_LOCKED) {
    CSML_ERROR(1, logger) << "CMD write rejected: FSM in ESCALATION_LOCKED "
                             "state (lc_escalate_en_i active)";
    CSML_ERROR(1, logger)
        << "  - All operations blocked until hardware reset (rst_ni assertion)";
    CSML_ERROR(1, logger)
        << "  - err_processed cannot recover from ESCALATION_LOCKED";
    return false;
  }

  // Extract command field bits [5:0]
  uint32_t cmd_field = (value & 0x3F);

  // Extract auxiliary command bits
  bool entropy_req = (value & (1 << 8)) != 0;
  bool hash_cnt_clr = (value & (1 << 9)) != 0;
  bool err_processed = (value & (1 << 10)) != 0;

  CSML_INFO(2, logger) << "CMD write: value=0x" << std::hex << value << std::dec
                       << " cmd_field=0x" << std::hex << cmd_field << std::dec
                       << " entropy_req=" << entropy_req
                       << " hash_cnt_clr=" << hash_cnt_clr
                       << " err_processed=" << err_processed;

  // =========================================================================
  // FUNC-KMAC-019: Process err_processed bit first (error recovery)
  // =========================================================================
  if (err_processed) {
    // FUNC-KMAC-021: err_processed cannot recover from ESCALATION_LOCKED
    // (already checked above)
    if (fsm_state == KmacState::ERROR) {
      CSML_INFO(2, logger) << "FUNC-KMAC-019: Processing err_processed - "
                              "recovering from ERROR state to IDLE";

      // Reset FSM to IDLE
      update_fsm_state(KmacState::IDLE);

      // Restore CFG_REGWEN.en to allow configuration
      CFG_REGWEN.en = 1;

      // FUNC-KMAC-019: Clear ERR_CODE after error recovery
      // Per specification: err_processed clears ERR_CODE and allows FSM to
      // return to IDLE
      ERR_CODE = 0;
      CSML_INFO(2, logger)
          << "FUNC-KMAC-019: ERR_CODE cleared after error recovery";

      // FUNC-KMAC-019: Clear kmac_err interrupt bit (error recovery complete)
      // Note: INTR_STATE.kmac_err should already be cleared by software W1C
      // before err_processed but we ensure clean state here
      INTR_STATE.kmac_err = 0;

      CSML_INFO(2, logger) << "FUNC-KMAC-019: Error recovery complete - FSM "
                              "returned to IDLE, CFG_REGWEN.en=1, ERR_CODE=0";
    } else {
      CSML_INFO(2, logger) << "FUNC-KMAC-019: err_processed ignored - FSM not "
                              "in ERROR state (current state="
                           << static_cast<int>(fsm_state) << ")";
    }
  }

  // =========================================================================
  // Process hash_cnt_clr bit
  // =========================================================================
  if (hash_cnt_clr) {
    CSML_INFO(2, logger)
        << "Processing hash_cnt_clr: clearing ENTROPY_REFRESH_HASH_CNT to 0";
    ENTROPY_REFRESH_HASH_CNT = 0;
  }

  // =========================================================================
  // FUNC-KMAC-019: Process entropy_req bit with timeout detection
  // =========================================================================
  if (entropy_req) {
    if (fsm_state == KmacState::IDLE) {
      CSML_INFO(2, logger)
          << "Processing entropy_req: triggering manual PRNG reseed";

      // Clear entropy refresh hash counter
      ENTROPY_REFRESH_HASH_CNT = 0;

      // Read ENTROPY_PERIOD for timeout configuration
      // Bits [31:16]: wait_timer (16-bit timeout value in timer pulses)
      // Bits [9:0]: prescaler (10-bit prescaler for timer pulse generation)
      uint32_t entropy_period_reg = ENTROPY_PERIOD;
      uint32_t wait_timer = (entropy_period_reg >> 16) & 0xFFFF;
      uint32_t prescaler = entropy_period_reg & 0x3FF;

      CSML_INFO(2, logger)
          << "FUNC-KMAC-019: Entropy timeout config - wait_timer=" << wait_timer
          << ", prescaler=" << prescaler;

      // FUNC-KMAC-019: Simulate WaitTimerExpired error (0x04) detection
      // In real hardware: If EDN does not respond within timeout, error is
      // raised For TLM functional model: We simulate successful entropy
      // delivery To trigger timeout error, wait_timer would need to be very
      // small or entropy delivery would fail

      // EDN interface removed - entropy is always assumed available internally
      bool entropy_available = true;
      CSML_INFO(2, logger) << "FUNC-KMAC-019: Entropy delivery simulated "
                              "internally (no external EDN interface)";

      // FUNC-KMAC-019: If wait_timer=0, wait indefinitely (not recommended per
      // spec) If wait_timer>0 and entropy not available, trigger
      // WaitTimerExpired error
      if (wait_timer > 0 && !entropy_available) {
        CSML_ERROR(1, logger)
            << "FUNC-KMAC-019: WaitTimerExpired error (0x04) - "
            << "EDN did not respond within timeout (wait_timer=" << wait_timer
            << " timer pulses)";

        // Set ERR_CODE: 0x04 (WaitTimerExpired)
        // Debug bits [23:0] reserved (set to 0)
        ERR_CODE = 0x04000000;
        INTR_STATE.kmac_err = 1;
        update_fsm_state(KmacState::ERROR);

        CSML_INFO(2, logger) << "FUNC-KMAC-019: Entropy FSM moved to Wait "
                                "state, uses pre-generated entropy";
        return false;
      }

      // In real hardware, would send request to EDN interface
      // TLM abstraction: entropy port interaction would go here
      CSML_INFO(2, logger) << "Manual entropy reseed requested (EDN "
                              "interaction abstracted in TLM)";

      // FUNC-KMAC-024: Apply temporal decoupling for entropy request latency
      // Architecture map timing constraint: entropy-wait-timer configured via
      // ENTROPY_PERIOD register
      const double clk_freq_hz = 100e6; // Default 100 MHz
      uint32_t entropy_cycles =
          ((wait_timer > 0) ? wait_timer : 100) * (prescaler + 1);
      sc_time entropy_delay = sc_time(entropy_cycles / clk_freq_hz, SC_SEC);

      // Accumulate local time in quantum keeper
      m_qk.inc(entropy_delay);

      // Synchronize with global quantum if needed
      if (m_qk.need_sync()) {
        m_qk.sync();
      }

      CSML_INFO(2, logger) << "FUNC-KMAC-024: Entropy request delay="
                           << entropy_delay << " (wait_timer=" << wait_timer
                           << ", prescaler=" << prescaler
                           << ", local_time=" << m_qk.get_local_time() << ")";
    } else {
      CSML_INFO(2, logger) << "entropy_req ignored: only valid in IDLE state";
    }
  }

  // =========================================================================
  // Early return for auxiliary-bit-only writes
  // =========================================================================
  /// If auxiliary bits were processed and cmd_field is 0, skip command
  /// validation
  if ((err_processed || hash_cnt_clr || entropy_req) && cmd_field == 0x0) {
    return true; // Auxiliary-bit-only write, no command to validate
  }

  // =========================================================================
  // Process sparse-encoded command field
  // =========================================================================
  // Validate sparse command encoding
  bool valid_cmd = false;
  std::string cmd_name = "INVALID";

  switch (cmd_field) {
  case 0x1D: // START
    valid_cmd = true;
    cmd_name = "START";
    break;
  case 0x2E: // PROCESS
    valid_cmd = true;
    cmd_name = "PROCESS";
    break;
  case 0x31: // RUN
    valid_cmd = true;
    cmd_name = "RUN";
    break;
  case 0x16: // DONE
    valid_cmd = true;
    cmd_name = "DONE";
    break;
  default:
    valid_cmd = false;
    cmd_name = "INVALID";
    break;
  }

  if (!valid_cmd) {
    CSML_ERROR(1, logger) << "Invalid command encoding: 0x" << std::hex
                          << cmd_field << std::dec << " - rejecting command";

    // FUNC-KMAC-019: Set SwCmdSequence error (0x08)
    // Debug bits [2:0] contain received command value
    ERR_CODE = 0x08000000 | (cmd_field & 0xFF); // Error code with debug info

    // FUNC-KMAC-019: Generate kmac_err interrupt (set INTR_STATE.kmac_err)
    INTR_STATE.kmac_err = 1;

    // FUNC-KMAC-019: Note - Sha3Control error (0x80) may appear with
    // SwCmdSequence Internal SHA3 FSM control error indicating command issued
    // in wrong SHA3 state Software can ignore 0x80 if SwCmdSequence (0x08) also
    // present - focus on correcting command sequence
    CSML_INFO(2, logger) << "FUNC-KMAC-019: Invalid command may also trigger "
                            "Sha3Control error (0x80) internally";

    return false; // Reject invalid command
  }

  CSML_INFO(2, logger) << "Processing " << cmd_name << " command (0x"
                       << std::hex << cmd_field << std::dec << ")";

  // =====================================================================
  // FUNC-KMAC-007: Block software commands during application interface
  // operation
  // =====================================================================
  if (app_interface_active && cmd_field != 0x0) {
    CSML_ERROR(1, logger)
        << "Software CMD rejected: application interface active";

    // FUNC-KMAC-019: Set SwIssuedCmdInAppActive error (0x03)
    // Debug bits [2:0] contain received command value from software
    ERR_CODE = 0x03000000 | (cmd_field & 0xFF);
    INTR_STATE.kmac_err = 1;
    update_fsm_state(KmacState::ERROR);

    return false;
  }

  // =====================================================================
  // START Command (0x1D): IDLE → ABSORB
  // =====================================================================
  if (cmd_field == 0x1D) {
    if (fsm_state != KmacState::IDLE) {
      CSML_ERROR(1, logger) << "START command rejected: not in IDLE state";

      // FUNC-KMAC-019: Set SwCmdSequence error (0x08)
      ERR_CODE =
          0x08000000 | 0x1D; // SwCmdSequence with START cmd in debug bits
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    // =====================================================================
    // FUNC-KMAC-001 Phase 1: SHA3 OpenSSL Initialization
    // =====================================================================

    // Read CFG_SHADOWED fields for mode and kstrength
    uint32_t mode = CFG_SHADOWED.mode;
    uint32_t kstrength = CFG_SHADOWED.kstrength;
    uint32_t kmac_en = CFG_SHADOWED.kmac_en;
    uint32_t entropy_ready = CFG_SHADOWED.entropy_ready;
    uint32_t entropy_mode = CFG_SHADOWED.entropy_mode;

    CSML_INFO(2, logger) << "Configuration: mode=0x" << std::hex << mode
                         << " kstrength=0x" << kstrength
                         << " kmac_en=" << kmac_en
                         << " entropy_ready=" << entropy_ready
                         << " entropy_mode=0x" << entropy_mode << std::dec;

    // =====================================================================
    // FUNC-KMAC-019: Error Detection on START Command
    // =====================================================================

    // FUNC-KMAC-019: Check IncorrectEntropyMode error (0x05)
    // Raised when entropy_ready=1 but entropy_mode is invalid (not 0x0/idle,
    // 0x1/edn, 0x2/sw)
    if (entropy_ready == 1 &&
        (entropy_mode != 0x0 && entropy_mode != 0x1 && entropy_mode != 0x2)) {
      CSML_ERROR(1, logger)
          << "FUNC-KMAC-019: IncorrectEntropyMode error (0x05) - "
             "entropy_ready=1 but entropy_mode=0x"
          << std::hex << entropy_mode << std::dec << " is invalid";

      // Set ERR_CODE: 0x05 (IncorrectEntropyMode)
      // Debug bits [23:0] reserved (set to 0)
      ERR_CODE = 0x05000000;
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    // FUNC-KMAC-019: Check SwHashingWithoutEntropyReady error (0x09)
    // Raised when KMAC operation attempted with masking enabled but entropy not
    // ready. Note: SHA3/SHAKE operations (kmac_en=0) do NOT require entropy even
    // if EnMasking=1, since masking is only applied to KMAC operations.
    // Conditions: kmac_en=1 AND (EnMasking=1 OR msg_mask=1) AND entropy_ready=0
    uint32_t msg_mask = CFG_SHADOWED.msg_mask;

    if (kmac_en && (EnMasking || msg_mask == 1) && entropy_ready == 0) {
      CSML_ERROR(1, logger)
          << "FUNC-KMAC-019: SwHashingWithoutEntropyReady error (0x09) - "
          << "KMAC mode with masking enabled (EnMasking=" << EnMasking
          << ", msg_mask=" << msg_mask << ") but entropy_ready=0";

      // Set ERR_CODE: 0x09 (SwHashingWithoutEntropyReady)
      // Debug bits [23:0] reserved (set to 0)
      ERR_CODE = 0x09000000;
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    // FUNC-KMAC-019: Check entropy seeding completion for SW mode
    // If entropy_mode=0x2 (sw_mode), verify 6 ENTROPY_SEED writes completed
    if (entropy_mode == 0x2 && entropy_ready == 1 && !entropy_sw_mode_ready) {
      CSML_ERROR(1, logger)
          << "FUNC-KMAC-019: SwHashingWithoutEntropyReady error (0x09) - "
          << "SW entropy mode but PRNG not fully seeded (only "
          << sw_seed_write_count << "/6 writes completed)";

      // Set ERR_CODE: 0x09 (SwHashingWithoutEntropyReady)
      ERR_CODE = 0x09000000;
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    // SHA3 Mode (mode == 0x0)
    if (mode == 0x0) {
      const EVP_MD *md = nullptr;

      // Validate kstrength for SHA3 mode
      // Valid: 0x1 (L224), 0x2 (L256), 0x3 (L384), 0x4 (L512)
      // Invalid: 0x0 (L128) - not supported in SHA3 mode
      switch (kstrength) {
      case 0x1: // L224 (SHA3-224)
        md = EVP_sha3_224();
        CSML_INFO(2, logger) << "Selected SHA3-224 algorithm (EVP_sha3_224)";
        break;

      case 0x2: // L256 (SHA3-256)
        md = EVP_sha3_256();
        CSML_INFO(2, logger) << "Selected SHA3-256 algorithm (EVP_sha3_256)";
        break;

      case 0x3: // L384 (SHA3-384)
        md = EVP_sha3_384();
        CSML_INFO(2, logger) << "Selected SHA3-384 algorithm (EVP_sha3_384)";
        break;

      case 0x4: // L512 (SHA3-512)
        md = EVP_sha3_512();
        CSML_INFO(2, logger) << "Selected SHA3-512 algorithm (EVP_sha3_512)";
        break;

      default:
        // Invalid kstrength for SHA3 mode (including 0x0=L128)
        CSML_ERROR(1, logger) << "UnexpectedModeStrength error: SHA3 mode with "
                                 "invalid kstrength=0x"
                              << std::hex << kstrength << std::dec;

        // Set ERR_CODE: 0x06 (UnexpectedModeStrength) with debug info
        // Bits [31:24] = error code 0x06
        // Bits [23:16] = reserved/additional context
        // Bits [15:8]  = mode value
        // Bits [7:0]   = kstrength value
        uint32_t err_code_value =
            0x06000000 | ((mode & 0xFF) << 8) | (kstrength & 0xFF);
        ERR_CODE = err_code_value;
        INTR_STATE.kmac_err = 1;
        update_fsm_state(KmacState::ERROR);

        CSML_INFO(2, logger)
            << "ERR_CODE set to 0x" << std::hex << err_code_value << std::dec;

        // Note: Per specification, operation proceeds with undefined behavior
        // For Phase 1, we stop here and do not initialize OpenSSL
        return false;
      }

      // Initialize OpenSSL EVP context with selected SHA3 algorithm
      if (md != nullptr && evp_md_ctx != nullptr) {
        EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);

        // Reset context if previously used
        EVP_MD_CTX_reset(ctx);

        // Initialize digest operation
        if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
          CSML_ERROR(1, logger) << "OpenSSL EVP_DigestInit_ex failed for SHA3";
          return false;
        }

        CSML_INFO(2, logger)
            << "OpenSSL EVP context initialized for SHA3 operation";
      } else {
        CSML_ERROR(1, logger)
            << "Cannot initialize OpenSSL: EVP context not allocated";
        return false;
      }
    }
    // SHAKE Mode (mode == 0x2)
    else if (mode == 0x2) {
      const EVP_MD *md = nullptr;

      // Validate kstrength for SHAKE mode
      // Valid: 0x0 (SHAKE128), 0x2 (SHAKE256)
      switch (kstrength) {
      case 0x0: // L128 (SHAKE128)
        md = EVP_shake128();
        CSML_INFO(2, logger) << "Selected SHAKE128 algorithm (EVP_shake128)";
        break;

      case 0x2: // L256 (SHAKE256)
        md = EVP_shake256();
        CSML_INFO(2, logger) << "Selected SHAKE256 algorithm (EVP_shake256)";
        break;

      default:
        // Invalid kstrength for SHAKE mode
        CSML_ERROR(1, logger) << "UnexpectedModeStrength error: SHAKE mode "
                                 "with invalid kstrength=0x"
                              << std::hex << kstrength << std::dec;

        // Set ERR_CODE: 0x06 (UnexpectedModeStrength) with debug info
        uint32_t err_code_value =
            0x06000000 | ((mode & 0xFF) << 8) | (kstrength & 0xFF);
        ERR_CODE = err_code_value;
        INTR_STATE.kmac_err = 1;
        update_fsm_state(KmacState::ERROR);

        CSML_INFO(2, logger)
            << "ERR_CODE set to 0x" << std::hex << err_code_value << std::dec;
        return false;
      }

      // Initialize OpenSSL EVP context with selected SHAKE algorithm
      if (md != nullptr && evp_md_ctx != nullptr) {
        EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);

        // Reset context if previously used
        EVP_MD_CTX_reset(ctx);

        // Initialize digest operation
        if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
          CSML_ERROR(1, logger) << "OpenSSL EVP_DigestInit_ex failed for SHAKE";
          return false;
        }

        CSML_INFO(2, logger)
            << "OpenSSL EVP context initialized for SHAKE operation";
      } else {
        CSML_ERROR(1, logger)
            << "Cannot initialize OpenSSL: EVP context not allocated";
        return false;
      }
    }
    // cSHAKE Mode (mode == 0x3, kmac_en == 0) or KMAC Mode (mode == 0x3,
    // kmac_en == 1)
    else if (mode == 0x3) {
      // Validate kstrength for cSHAKE/KMAC mode
      // Valid: 0x0 (cSHAKE128/KMAC128), 0x2 (cSHAKE256/KMAC256)
      if (kstrength != 0x0 && kstrength != 0x2) {
        // Invalid kstrength for cSHAKE/KMAC mode
        CSML_ERROR(1, logger)
            << "UnexpectedModeStrength error: " << (kmac_en ? "KMAC" : "cSHAKE")
            << " mode with invalid kstrength=0x" << std::hex << kstrength
            << std::dec;

        // Set ERR_CODE: 0x06 (UnexpectedModeStrength) with debug info
        uint32_t err_code_value =
            0x06000000 | ((mode & 0xFF) << 8) | (kstrength & 0xFF);
        ERR_CODE = err_code_value;
        INTR_STATE.kmac_err = 1;
        update_fsm_state(KmacState::ERROR);

        CSML_INFO(2, logger)
            << "ERR_CODE set to 0x" << std::hex << err_code_value << std::dec;
        return false;
      }

      // =====================================================================
      // FUNC-KMAC-004: KMAC Mode - Manual construction using SHAKE
      // =====================================================================
      if (kmac_en) {
        // ==================================================================
        // FUNC-KMAC-004: KMAC Mode using manual construction with SHAKE
        // ==================================================================
        // OpenSSL's EVP_MAC KMAC API does not match NIST SP 800-185
        // implementation Instead, manually construct: KMAC(K,X,L,S) =
        // cSHAKE(bytepad(encode_string(K), rate) || X || right_encode(L), rate,
        // "KMAC", S) This matches the hardware specification and reference
        // implementation

        CSML_INFO(2, logger)
            << "Initializing KMAC mode using manual construction with SHAKE";

        // Step 1: Read and validate PREFIX for IncorrectFunctionName error
        // check PREFIX must start with encode_string("KMAC") = 0x01 0x20 0x4B
        // 0x4D 0x41 0x43
        uint8_t prefix_data[44];
        for (int i = 0; i < 11; i++) {
          uint32_t prefix_word = PREFIX[i].prefix;
          prefix_data[i * 4 + 0] = (prefix_word >> 0) & 0xFF;
          prefix_data[i * 4 + 1] = (prefix_word >> 8) & 0xFF;
          prefix_data[i * 4 + 2] = (prefix_word >> 16) & 0xFF;
          prefix_data[i * 4 + 3] = (prefix_word >> 24) & 0xFF;
        }

        // Validate PREFIX starts with encode_string("KMAC")
        const uint8_t expected_prefix_kmac[] = {0x01, 0x20, 0x4B,
                                                0x4D, 0x41, 0x43};
        bool prefix_valid = true;
        for (size_t i = 0; i < sizeof(expected_prefix_kmac); i++) {
          if (prefix_data[i] != expected_prefix_kmac[i]) {
            prefix_valid = false;
            break;
          }
        }

        if (!prefix_valid) {
          CSML_ERROR(1, logger) << "IncorrectFunctionName error: PREFIX does "
                                   "not start with encode_string(\"KMAC\")";
          // Set ERR_CODE: 0x07 (IncorrectFunctionName)
          ERR_CODE = 0x07000000;
          INTR_STATE.kmac_err = 1;
        } else {
          CSML_INFO(2, logger)
              << "PREFIX validation passed: encode_string(\"KMAC\") detected";
        }

        // Step 2: Extract customization string S from PREFIX
        // PREFIX format: encode_string("KMAC") || encode_string(S)
        // encode_string("KMAC") = 6 bytes, then encode_string(S) =
        // left_encode(len(S)*8) || S
        uint8_t customization_string[256];
        size_t customization_len = 0;

        // Parse encode_string(S) starting at byte 6
        if (prefix_data[6] == 0x01) {
          // left_encode format: 0x01 <length_byte>
          uint8_t s_bits = prefix_data[7];
          size_t s_bytes = (s_bits + 7) / 8; // Convert bits to bytes
          customization_len = s_bytes;
          if (customization_len > 0 && customization_len <= 256) {
            std::memcpy(customization_string, &prefix_data[8],
                        customization_len);
          }
        } else if (prefix_data[6] == 0x02) {
          // left_encode format: 0x02 <length_hi> <length_lo>
          uint16_t s_bits = (prefix_data[7] << 8) | prefix_data[8];
          size_t s_bytes = (s_bits + 7) / 8;
          customization_len = s_bytes;
          if (customization_len > 0 && customization_len <= 256) {
            std::memcpy(customization_string, &prefix_data[9],
                        customization_len);
          }
        }

        CSML_INFO(2, logger)
            << "Extracted customization string: " << customization_len
            << " bytes";

        // Debug: Log customization string in hex
        if (customization_len > 0) {
          std::stringstream ss_cust;
          for (size_t i = 0; i < customization_len; i++) {
            ss_cust << std::hex << std::setfill('0') << std::setw(2)
                    << (int)customization_string[i];
            if (i < customization_len - 1)
              ss_cust << " ";
          }
          CSML_INFO(2, logger)
              << "MODEL Customization string (hex): " << ss_cust.str();
        }

        // =================================================================
        // FUNC-KMAC-006: Step 3: Read key from KEY_SHARE registers or KeyMgr
        // sideload
        // =================================================================
        size_t key_bytes = 0;
        uint8_t key_material[64];
        std::memset(key_material, 0, sizeof(key_material));

        // Check CFG_SHADOWED.sideload flag for SW-initiated KMAC operations
        if (CFG_SHADOWED.sideload == 1) {
          // =================================================================
          // FUNC-KMAC-006: KeyMgr Sideloaded Key Path
          // =================================================================
          CSML_INFO(2, logger)
              << "KMAC using KeyMgr sideloaded key (CFG_SHADOWED.sideload=1)";

          // Read key from KeyMgr push sideload buffers
          // KeyMgr commits key by writing to keymgr_tl_socket then setting valid
          const uint32_t* sideload_share0 = m_keymgr_share0;
          const uint32_t* sideload_share1 = m_keymgr_share1;

          // Sideloaded key is always Key256 (256 bits = 32 bytes).
          // RTL: SideloadedKey = KeyLengths[SelKeySize] where
          //   KeyMgrKeyW = $bits(keymgr_key_i.key[0]) = keymgr_pkg::KeyWidth = 256
          //   SelKeySize = 2 -> Key256 always.
          key_bytes = 32;

          CSML_INFO(2, logger) << "Sideloaded key length: 256 bits (32 bytes, fixed Key256)";

          // Handle two-share key format from KeyMgr
          // Always XOR shares (KeyMgr provides masked format)
          // If EnMasking=0, internally unmask by XORing shares
          for (size_t i = 0; i < key_bytes; i++) {
            size_t word_idx = i / 4;
            size_t byte_idx = i % 4;
            uint8_t share0_byte =
                (sideload_share0[word_idx] >> (byte_idx * 8)) & 0xFF;
            uint8_t share1_byte =
                (sideload_share1[word_idx] >> (byte_idx * 8)) & 0xFF;
            key_material[i] = share0_byte ^ share1_byte;
          }

          if (EnMasking) {
            CSML_INFO(2, logger)
                << "Sideloaded key used in masked form (EnMasking=1)";
          } else {
            CSML_INFO(2, logger) << "Sideloaded key automatically unmasked "
                                    "(EnMasking=0, XOR applied)";
          }

          // Note: For SW-initiated operations with sideload=1, no KeyNotValid
          // error check per specification: "No error for SW-initiated ops with
          // invalid sideloaded key" (Application interface operations handle
          // KeyNotValid separately)

        } else {
          // =================================================================
          // FUNC-KMAC-005: Software KEY_SHARE Register Path
          // =================================================================
          CSML_INFO(2, logger) << "KMAC using software KEY_SHARE registers "
                                  "(CFG_SHADOWED.sideload=0)";

          // Read KEY_LEN register for key length
          uint32_t key_len_val = KEY_LEN.len;
          switch (key_len_val) {
          case 0x0:
            key_bytes = 16;
            break; // 128-bit
          case 0x1:
            key_bytes = 24;
            break; // 192-bit
          case 0x2:
            key_bytes = 32;
            break; // 256-bit
          case 0x3:
            key_bytes = 48;
            break; // 384-bit
          case 0x4:
            key_bytes = 64;
            break; // 512-bit
          default:
            CSML_ERROR(1, logger) << "Invalid KEY_LEN value: 0x" << std::hex
                                  << key_len_val << std::dec;
            return false;
          }

          CSML_INFO(2, logger)
              << "KMAC key length from KEY_LEN: " << (key_bytes * 8)
              << " bits (" << key_bytes << " bytes)";

          // Extract key material from KEY_SHARE registers
          if (EnMasking) {
            // Masked mode: XOR KEY_SHARE0 and KEY_SHARE1
            for (size_t i = 0; i < key_bytes; i++) {
              size_t word_idx = i / 4;
              size_t byte_idx = i % 4;
              uint32_t share0_word = KEY_SHARE0[word_idx].key;
              uint32_t share1_word = KEY_SHARE1[word_idx].key;
              uint8_t share0_byte = (share0_word >> (byte_idx * 8)) & 0xFF;
              uint8_t share1_byte = (share1_word >> (byte_idx * 8)) & 0xFF;
              key_material[i] = share0_byte ^ share1_byte;
            }
            CSML_INFO(2, logger) << "KMAC key extracted from KEY_SHARE0 XOR "
                                    "KEY_SHARE1 (EnMasking=1)";
          } else {
            // Unmasked mode: Use only KEY_SHARE0
            for (size_t i = 0; i < key_bytes; i++) {
              size_t word_idx = i / 4;
              size_t byte_idx = i % 4;
              uint32_t share0_word = KEY_SHARE0[word_idx].key;
              uint8_t share0_byte = (share0_word >> (byte_idx * 8)) & 0xFF;
              key_material[i] = share0_byte;
            }
            CSML_INFO(2, logger)
                << "KMAC key extracted from KEY_SHARE0 (EnMasking=0)";
          }
        }

        // Debug: Log key material in hex
        std::stringstream ss_key;
        for (size_t i = 0; i < key_bytes; i++) {
          ss_key << std::hex << std::setfill('0') << std::setw(2)
                 << (int)key_material[i];
          if (i < key_bytes - 1)
            ss_key << " ";
        }
        CSML_INFO(2, logger) << "MODEL Key material (" << (key_bytes * 8)
                             << " bits): " << ss_key.str();

        // Step 4: Initialize SHAKE context (SHAKE128 for KMAC128, SHAKE256 for
        // KMAC256)
        const EVP_MD *md = nullptr;
        size_t rate = 0;
        if (kstrength == 0x0) {
          md = EVP_shake128();
          rate = 168; // SHAKE128 rate
          CSML_INFO(2, logger)
              << "Using SHAKE128 for KMAC128 construction (rate=168)";
        } else {
          md = EVP_shake256();
          rate = 136; // SHAKE256 rate
          CSML_INFO(2, logger)
              << "Using SHAKE256 for KMAC256 construction (rate=136)";
        }

        // Reset existing EVP_MD context (don't create new one to avoid memory
        // leak)
        if (evp_md_ctx == nullptr) {
          CSML_ERROR(1, logger) << "EVP_MD_CTX not allocated";
          return false;
        }

        EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
        EVP_MD_CTX_reset(ctx); // Reset context to clear any previous state

        if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
          CSML_ERROR(1, logger)
              << "Failed to initialize SHAKE context for KMAC";
          return false;
        }

        // Step 5: Construct and absorb PREFIX block:
        // bytepad(encode_string("KMAC") || encode_string(S), rate)
        uint8_t prefix_block[256];
        size_t prefix_block_len = 0;

        // encode_string("KMAC")
        prefix_block[prefix_block_len++] = 0x01;
        prefix_block[prefix_block_len++] = 0x20; // 32 bits
        prefix_block[prefix_block_len++] = 0x4B; // 'K'
        prefix_block[prefix_block_len++] = 0x4D; // 'M'
        prefix_block[prefix_block_len++] = 0x41; // 'A'
        prefix_block[prefix_block_len++] = 0x43; // 'C'

        // encode_string(S) - customization string
        if (customization_len > 0) {
          size_t cust_bits = customization_len * 8;
          if (cust_bits <= 255) {
            prefix_block[prefix_block_len++] = 0x01;
            prefix_block[prefix_block_len++] = (uint8_t)(cust_bits & 0xFF);
          } else {
            prefix_block[prefix_block_len++] = 0x02;
            prefix_block[prefix_block_len++] =
                (uint8_t)((cust_bits >> 8) & 0xFF);
            prefix_block[prefix_block_len++] = (uint8_t)(cust_bits & 0xFF);
          }
          std::memcpy(prefix_block + prefix_block_len, customization_string,
                      customization_len);
          prefix_block_len += customization_len;
        } else {
          prefix_block[prefix_block_len++] = 0x01;
          prefix_block[prefix_block_len++] = 0x00;
        }

        // bytepad(prefix_block, rate)
        uint8_t bytepadded_prefix[168];
        size_t bytepad_len = 0;

        bytepadded_prefix[bytepad_len++] = 0x01;
        bytepadded_prefix[bytepad_len++] = (uint8_t)(rate & 0xFF);

        std::memcpy(bytepadded_prefix + bytepad_len, prefix_block,
                    prefix_block_len);
        bytepad_len += prefix_block_len;

        while (bytepad_len < rate) {
          bytepadded_prefix[bytepad_len++] = 0x00;
        }

        // Absorb PREFIX block
        if (EVP_DigestUpdate(ctx, bytepadded_prefix, rate) != 1) {
          CSML_ERROR(1, logger) << "Failed to absorb PREFIX block for KMAC";
          return false;
        }

        // Debug: Log first 40 bytes of PREFIX block
        std::stringstream ss_prefix;
        for (size_t i = 0; i < 40 && i < rate; i++) {
          ss_prefix << std::hex << std::setfill('0') << std::setw(2)
                    << (int)bytepadded_prefix[i];
          if (i < 39)
            ss_prefix << " ";
        }
        CSML_INFO(2, logger)
            << "MODEL PREFIX block (first 40 bytes): " << ss_prefix.str();

        CSML_INFO(2, logger) << "Absorbed PREFIX block (" << rate << " bytes)";

        // Step 6: Construct and absorb KEY block: bytepad(encode_string(K),
        // rate)
        size_t key_bits = key_bytes * 8;
        uint8_t encoded_key[256];
        size_t encoded_key_len = 0;

        // left_encode(key_bits)
        if (key_bits <= 255) {
          encoded_key[encoded_key_len++] = 0x01;
          encoded_key[encoded_key_len++] = (uint8_t)(key_bits & 0xFF);
        } else {
          encoded_key[encoded_key_len++] = 0x02;
          encoded_key[encoded_key_len++] = (uint8_t)((key_bits >> 8) & 0xFF);
          encoded_key[encoded_key_len++] = (uint8_t)(key_bits & 0xFF);
        }

        // Append key bytes
        std::memcpy(encoded_key + encoded_key_len, key_material, key_bytes);
        encoded_key_len += key_bytes;

        // bytepad(encode_string(K), rate)
        uint8_t key_block[168];
        size_t key_block_len = 0;

        key_block[key_block_len++] = 0x01;
        key_block[key_block_len++] = (uint8_t)(rate & 0xFF);

        std::memcpy(key_block + key_block_len, encoded_key, encoded_key_len);
        key_block_len += encoded_key_len;

        while (key_block_len < rate) {
          key_block[key_block_len++] = 0x00;
        }

        // Absorb KEY block
        if (EVP_DigestUpdate(ctx, key_block, rate) != 1) {
          CSML_ERROR(1, logger) << "Failed to absorb KEY block for KMAC";
          return false;
        }

        // Debug: Log first 40 bytes of KEY block
        std::stringstream ss_keyblock;
        for (size_t i = 0; i < 40 && i < rate; i++) {
          ss_keyblock << std::hex << std::setfill('0') << std::setw(2)
                      << (int)key_block[i];
          if (i < 39)
            ss_keyblock << " ";
        }
        CSML_INFO(2, logger)
            << "MODEL KEY block (first 40 bytes): " << ss_keyblock.str();

        CSML_INFO(2, logger) << "Absorbed KEY block (" << rate << " bytes)";

        CSML_INFO(2, logger)
            << "KMAC initialization complete - ready to absorb message data";
      }
      // =====================================================================
      // cSHAKE Mode - Use OpenSSL EVP_MD API (SHAKE128/256)
      // =====================================================================
      else {
        // Select SHAKE algorithm based on kstrength
        const EVP_MD *md = nullptr;
        if (kstrength == 0x0) {
          md = EVP_shake128();
          CSML_INFO(2, logger)
              << "Selected cSHAKE128 algorithm (EVP_shake128 base)";
        } else { // kstrength == 0x2
          md = EVP_shake256();
          CSML_INFO(2, logger)
              << "Selected cSHAKE256 algorithm (EVP_shake256 base)";
        }

        // Initialize OpenSSL EVP context with selected cSHAKE algorithm
        if (md != nullptr && evp_md_ctx != nullptr) {
          EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);

          // Reset context if previously used
          EVP_MD_CTX_reset(ctx);

          // Initialize digest operation
          if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
            CSML_ERROR(1, logger)
                << "OpenSSL EVP_DigestInit_ex failed for cSHAKE";
            return false;
          }

          CSML_INFO(2, logger)
              << "OpenSSL EVP context initialized for cSHAKE operation";

          // cSHAKE mode (kmac_en=0): Absorb PREFIX registers before message
          // data Per NIST SP 800-185, cSHAKE uses bytepad(encode_string(N) ||
          // encode_string(S), rate) PREFIX_0 through PREFIX_10 (11 registers x
          // 4 bytes = 44 bytes) store encode_string(N) || encode_string(S) Read
          // PREFIX registers (44 bytes total)
          uint8_t prefix_data[44];
          for (int i = 0; i < 11; i++) {
            uint32_t prefix_word = PREFIX[i].prefix;
            // Unpack little-endian (register storage format)
            prefix_data[i * 4 + 0] = (prefix_word >> 0) & 0xFF;
            prefix_data[i * 4 + 1] = (prefix_word >> 8) & 0xFF;
            prefix_data[i * 4 + 2] = (prefix_word >> 16) & 0xFF;
            prefix_data[i * 4 + 3] = (prefix_word >> 24) & 0xFF;
          }

          // Check if PREFIX is non-empty (any non-zero byte)
          bool prefix_nonempty = false;
          for (int i = 0; i < 44; i++) {
            if (prefix_data[i] != 0) {
              prefix_nonempty = true;
              break;
            }
          }

          if (prefix_nonempty) {
            // Apply bytepad encoding per NIST SP 800-185
            // bytepad(X, w) = left_encode(w) || X || 0x00... (padded to w
            // bytes) Determine rate based on kstrength
            size_t rate =
                (kstrength == 0x0) ? 168 : 136; // 168 for L128, 136 for L256

            // Allocate bytepad buffer
            uint8_t bytepadded[168]; // Max rate is 168
            size_t offset = 0;

            // Add left_encode(rate) at the beginning
            // For rate=168: left_encode(168) = 0x01 0xA8
            // For rate=136: left_encode(136) = 0x01 0x88
            bytepadded[offset++] = 0x01; // Length of encoding = 1 byte
            bytepadded[offset++] = (uint8_t)(rate & 0xFF);

            // Append PREFIX data
            for (int i = 0; i < 44; i++) {
              bytepadded[offset++] = prefix_data[i];
            }

            // Zero-pad to rate bytes
            while (offset < rate) {
              bytepadded[offset++] = 0x00;
            }

            // Absorb bytepadded PREFIX into Keccak state
            if (EVP_DigestUpdate(ctx, bytepadded, rate) != 1) {
              CSML_ERROR(1, logger) << "OpenSSL EVP_DigestUpdate failed for "
                                       "cSHAKE bytepad absorption";
              return false;
            }
            CSML_INFO(2, logger)
                << "cSHAKE bytepad absorbed: " << rate << " bytes (left_encode("
                << rate << ") || PREFIX || zero_padding)";
          } else {
            // Empty PREFIX: cSHAKE with N="" and S="" is functionally
            // equivalent to SHAKE
            CSML_INFO(2, logger)
                << "cSHAKE PREFIX empty: functionally equivalent to SHAKE";
          }
        } else {
          CSML_ERROR(1, logger)
              << "Cannot initialize OpenSSL: EVP context not allocated";
          return false;
        }
      } // End of cSHAKE mode (else branch)
    } // End of mode == 0x3 (cSHAKE/KMAC mode)
    else {
      // Unknown mode - should not reach here
      CSML_ERROR(1, logger)
          << "Unknown mode value: 0x" << std::hex << mode << std::dec;
      return false;
    }

    // Configuration validation passed - transition to ABSORB state
    CSML_INFO(2, logger) << "START command: transitioning IDLE → ABSORB";
    update_fsm_state(KmacState::ABSORB);

    // Auto-clear CFG_REGWEN.en (lock configuration)
    CFG_REGWEN.en = 0;
    CSML_INFO(2, logger)
        << "CFG_REGWEN.en auto-cleared to 0 (configuration locked)";

    // FUNC-KMAC-024: Apply temporal decoupling for START command processing
    // Architecture map timing constraint: register-access-latency = 5 cycles
    // Additional delay for prefix expansion timing in cSHAKE/KMAC modes
    const double clk_freq_hz = 100e6;           // Default 100 MHz
    const uint32_t start_processing_cycles = 5; // Register access latency
    sc_time start_delay =
        sc_time(start_processing_cycles / clk_freq_hz, SC_SEC);

    // Accumulate local time in quantum keeper
    m_qk.inc(start_delay);

    // Synchronize with global quantum if needed
    if (m_qk.need_sync()) {
      m_qk.sync();
    }

    CSML_INFO(2, logger) << "FUNC-KMAC-024: START command delay=" << start_delay
                         << " (local_time=" << m_qk.get_local_time() << ")";

    return true;
  }

  // =====================================================================
  // PROCESS Command (0x2E): ABSORB → SQUEEZE
  // =====================================================================
  else if (cmd_field == 0x2E) {
    if (fsm_state != KmacState::ABSORB) {
      CSML_ERROR(1, logger) << "PROCESS command rejected: not in ABSORB state";

      // FUNC-KMAC-019: Set SwCmdSequence error (0x08)
      ERR_CODE =
          0x08000000 | 0x2E; // SwCmdSequence with PROCESS cmd in debug bits
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    CSML_INFO(2, logger)
        << "PROCESS command: finalizing digest with OpenSSL EVP_DigestFinal_ex";

    // =====================================================================
    // FUNC-KMAC-001 Phase 2: SHA3 Digest Finalization
    // =====================================================================

    // =====================================================================
    // FUNC-KMAC-004: Parse right_encode(output_length) from message end
    // =====================================================================
    // For KMAC mode, software appends right_encode(output_bits) to the message.
    // Per NIST SP 800-185 and hardware spec:
    // Format: <value_bytes_big_endian> <length_byte>
    // Example: right_encode(256) = 0x01 0x00 0x02
    //   - value = 0x0100 (256 in big-endian, 2 bytes)
    //   - length = 0x02 (number of value bytes)
    //
    // We need to:
    // 1. Parse right_encode(L) from packer buffer end
    // 2. Extract output length value (in bits)
    // 3. Remove right_encode bytes from message before EVP_MAC_update
    // 4. Use extracted length (in bytes) in EVP_MAC_final
    //
    // Note: EVP_MAC KMAC will internally append right_encode(L),
    // so we must remove it from the message to avoid double encoding.

    size_t kmac_output_bytes = 0;   // KMAC output length in bytes
    uint32_t kmac_en = CFG_SHADOWED.kmac_en;
    uint32_t kstrength = CFG_SHADOWED.kstrength;

    // KMAC mode: Determine output length based on kstrength
    // Since right_encode(L) is already absorbed as part of the message data
    // during MSG_FIFO writes, we use the rate as the output buffer size.
    // KMAC is an XOF - the test reads only the bytes it needs.
    // Per NIST SP 800-185: KMAC128 uses rate=168, KMAC256 uses rate=136
    if (kmac_en) {
      // Use rate as output length (XOF can output up to rate bytes per block)
      // kstrength=0 => 128-bit security => SHAKE128 rate = 168 bytes
      // kstrength=1 => 256-bit security => SHAKE256 rate = 136 bytes
      if (kstrength == 0) {
        kmac_output_bytes = 168;  // KMAC128: SHAKE128 rate
      } else {
        kmac_output_bytes = 136;  // KMAC256: SHAKE256 rate
      }
      CSML_INFO(2, logger) << "KMAC mode: using output length "
                           << kmac_output_bytes * 8 << " bits ("
                           << kmac_output_bytes << " bytes) based on kstrength="
                           << kstrength;
    }

    // Flush partial packer entry (only actual bytes, no zero-padding)
    if (packer_position > 0) {
      CSML_INFO(2, logger) << "Flushing partial packer entry: "
                           << packer_position << " bytes";

      // Debug: Log partial packer buffer in hex
      std::stringstream ss_partial;
      for (size_t i = 0; i < packer_position; i++) {
        ss_partial << std::hex << std::setfill('0') << std::setw(2)
                   << (int)packer_buffer[i];
        if (i < packer_position - 1)
          ss_partial << " ";
      }
      CSML_INFO(2, logger) << "MODEL Partial packer data (hex): "
                           << ss_partial.str();

      // Update OpenSSL with only the actual bytes in packer buffer
      // Note: Do NOT zero-pad to 64-bit boundary - OpenSSL handles padding
      // internally All modes (SHA3/SHAKE/cSHAKE/KMAC) use EVP_DigestUpdate
      EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
      if (EVP_DigestUpdate(ctx, packer_buffer, packer_position) != 1) {
        CSML_ERROR(1, logger)
            << "OpenSSL EVP_DigestUpdate failed for final packer flush";
        return false;
      }

      packer_position = 0;
      std::memset(packer_buffer, 0, sizeof(packer_buffer));
    }

    // Finalize digest computation
    // Read mode to determine finalization method
    uint32_t mode = CFG_SHADOWED.mode;
    // kstrength already declared above for output length determination
    // kmac_en already declared above for right_encode parsing
    unsigned int md_len = 0;

    // KMAC mode: Finalize with XOF
    if (kmac_en) {
      EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);

      if (kmac_output_bytes == 0) {
        CSML_ERROR(1, logger) << "KMAC output length not set";
        return false;
      }

      // Note: right_encode(L) is already absorbed as part of the message
      // during MSG_FIFO writes, so we just finalize here.

      // For KMAC: Always extract FULL output in one call for correct XOF
      // semantics Buffer it and slice for STATE window access
      if (kmac_output_bytes > sizeof(xof_full_output)) {
        CSML_ERROR(1, logger)
            << "KMAC output length " << kmac_output_bytes
            << " exceeds buffer size " << sizeof(xof_full_output);
        return false;
      }

      if (EVP_DigestFinalXOF(ctx, xof_full_output, kmac_output_bytes) != 1) {
        CSML_ERROR(1, logger) << "OpenSSL EVP_DigestFinalXOF failed for KMAC";
        return false;
      }

      // Copy first chunk to digest_buffer for STATE window
      // STATE window shows chunks of output; RUN command advances the window
      // Use a reasonable chunk size that accommodates common MAC lengths
      const size_t state_window_size =
          64; // 512 bits - accommodates typical MAC lengths
      size_t initial_chunk_size =
          std::min(kmac_output_bytes, state_window_size);

      std::memcpy(digest_buffer, xof_full_output, initial_chunk_size);
      digest_size = initial_chunk_size;
      xof_output_offset = 0; // Start at beginning of generated output
      xof_total_length = kmac_output_bytes;

      CSML_INFO(2, logger) << "KMAC output: extracted " << kmac_output_bytes
                           << " bytes total, STATE window shows first "
                           << initial_chunk_size << " bytes";
    }
    // SHA3 mode (0x0): Use EVP_DigestFinal_ex for fixed-length output
    // SHAKE/cSHAKE modes (0x2, 0x3): Use EVP_DigestFinalXOF for XOF support
    else if (mode == 0x0) {
      // SHA3 mode: Fixed-length output
      EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
      if (EVP_DigestFinal_ex(ctx, digest_buffer, &md_len) != 1) {
        CSML_ERROR(1, logger) << "OpenSSL EVP_DigestFinal_ex failed for SHA3";
        return false;
      }

      digest_size = md_len;
      CSML_INFO(2, logger) << "SHA3 digest finalized: " << digest_size
                           << " bytes";
    } else {
      // SHAKE/cSHAKE mode: Extendable output (XOF)
      // For initial PROCESS, extract up to rate size
      // Calculate rate based on kstrength
      unsigned int rate_bytes = 0;

      switch (kstrength) {
      case 0x0:           // L128 (SHAKE128/cSHAKE128)
        rate_bytes = 168; // 1344 bits / 8
        break;

      case 0x2:           // L256 (SHAKE256/cSHAKE256)
        rate_bytes = 136; // 1088 bits / 8
        break;

      default:
        // Invalid kstrength (should have been caught in START)
        CSML_ERROR(1, logger) << "PROCESS: invalid kstrength for XOF mode";
        return false;
      }

      // EVP_DigestFinalXOF may only be called ONCE per context, and SHAKE/cSHAKE
      // output is a single continuous, deterministic byte stream.  Generate a
      // generous buffered output here in one call, expose the first rate-sized
      // block through the STATE window, and let successive RUN commands advance
      // the window through this buffer (matching hardware "squeeze more"
      // semantics where each RUN reveals the next sequential output block).
      EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
      if (EVP_DigestFinalXOF(ctx, xof_full_output, sizeof(xof_full_output)) !=
          1) {
        CSML_ERROR(1, logger)
            << "OpenSSL EVP_DigestFinalXOF failed for SHAKE/cSHAKE";
        return false;
      }

      xof_total_length = sizeof(xof_full_output);
      xof_output_offset = 0;

      std::memcpy(digest_buffer, xof_full_output, rate_bytes);
      digest_size = rate_bytes;
      CSML_INFO(2, logger) << "SHAKE/cSHAKE initial output finalized: "
                           << digest_size << " bytes (rate), "
                           << xof_total_length << " bytes buffered for RUN";
    }

    // If EnMasking=1, generate two random shares such that share0 XOR share1 =
    // digest
    if (EnMasking) {
      CSML_INFO(2, logger) << "EnMasking=1: Generating masked shares";

      // Generate random share0
      for (unsigned int i = 0; i < digest_size; i++) {
        // Simple pseudo-random generation (TLM functional model)
        // Real hardware would use EDN entropy
        digest_share0[i] = static_cast<uint8_t>(rand() & 0xFF);

        // Compute share1 = digest XOR share0
        digest_share1[i] = digest_buffer[i] ^ digest_share0[i];
      }

      CSML_INFO(2, logger)
          << "Masked shares generated (share0 XOR share1 = digest)";
    } else {
      // EnMasking=0: Copy digest to share0, zero share1
      std::memcpy(digest_share0, digest_buffer, digest_size);
      std::memset(digest_share1, 0, digest_size);

      CSML_INFO(2, logger)
          << "EnMasking=0: Digest stored in share0, share1 zeroed";
    }

    // Clear FIFO (all entries already absorbed during MSG_FIFO writes)
    while (!msg_fifo.empty()) {
      msg_fifo.pop();
    }
    fifo_depth = 0;
    CSML_INFO(2, logger) << "MSG_FIFO cleared (fifo_depth reset to 0)";

    // Transition to SQUEEZE state
    update_fsm_state(KmacState::SQUEEZE);

    // Generate kmac_done interrupt
    INTR_STATE.kmac_done = 1;
    CSML_INFO(2, logger)
        << "kmac_done interrupt generated (INTR_STATE.kmac_done=1)";
    evaluate_interrupt();  // Update intr_o output

    // FUNC-KMAC-024: Apply temporal decoupling for PROCESS command (Keccak
    // processing) Architecture map timing constraint: keccak-processing-latency
    // = 96 cycles (24 rounds x 4 cycles/round with masking) This is a
    // functional delay approximation - actual hardware timing is cycle-accurate
    const double clk_freq_hz = 100e6;             // Default 100 MHz
    const uint32_t keccak_processing_cycles = 96; // 24 rounds x 4 cycles/round
    sc_time process_delay =
        sc_time(keccak_processing_cycles / clk_freq_hz, SC_SEC);

    // Accumulate local time in quantum keeper
    m_qk.inc(process_delay);

    // Synchronize with global quantum if needed
    if (m_qk.need_sync()) {
      m_qk.sync();
    }

    CSML_INFO(2, logger) << "FUNC-KMAC-024: PROCESS command delay="
                         << process_delay
                         << " (local_time=" << m_qk.get_local_time() << ")";

    return true;
  }

  // =====================================================================
  // RUN Command (0x31): SHAKE/cSHAKE/KMAC Extended Output in SQUEEZE
  // =====================================================================
  else if (cmd_field == 0x31) {
    if (fsm_state != KmacState::SQUEEZE) {
      CSML_ERROR(1, logger) << "RUN command rejected: not in SQUEEZE state";

      // FUNC-KMAC-019: Set SwCmdSequence error (0x08)
      ERR_CODE = 0x08000000 | 0x31; // SwCmdSequence with RUN cmd in debug bits
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    CSML_INFO(2, logger) << "RUN command: executing additional Keccak rounds "
                            "for extended output";

    // =====================================================================
    // FUNC-KMAC-002: SHAKE XOF Extended Output via RUN Command
    // =====================================================================

    // Read configuration to determine mode and rate
    uint32_t mode = CFG_SHADOWED.mode;
    uint32_t kstrength = CFG_SHADOWED.kstrength;

    // Only SHAKE (mode=0x2), cSHAKE (mode=0x3), and KMAC (mode=0x3 with
    // kmac_en=1) support extended output via RUN command SHA3 (mode=0x0)
    // produces fixed-length output only

    if (mode == 0x0) {
      // SHA3 mode: RUN command not applicable (fixed-length output)
      CSML_ERROR(1, logger) << "RUN command rejected: SHA3 mode produces "
                               "fixed-length output only";

      // FUNC-KMAC-019: Set SwCmdSequence error (0x08)
      ERR_CODE = 0x08000000 | 0x31;
      INTR_STATE.kmac_err = 1;
      update_fsm_state(KmacState::ERROR);

      return false;
    }

    // Calculate rate size based on kstrength (XOF output block size)
    // kstrength=0x0 (L128): 1344-bit rate = 168 bytes
    // kstrength=0x2 (L256): 1088-bit rate = 136 bytes
    unsigned int rate_bytes = 0;

    switch (kstrength) {
    case 0x0:           // L128 (SHAKE128/cSHAKE128/KMAC128)
      rate_bytes = 168; // 1344 bits / 8
      CSML_INFO(2, logger) << "SHAKE/cSHAKE/KMAC128: rate = " << rate_bytes
                           << " bytes";
      break;

    case 0x2:           // L256 (SHAKE256/cSHAKE256/KMAC256)
      rate_bytes = 136; // 1088 bits / 8
      CSML_INFO(2, logger) << "SHAKE/cSHAKE/KMAC256: rate = " << rate_bytes
                           << " bytes";
      break;

    default:
      // Invalid kstrength for XOF modes (should have been caught in START)
      CSML_ERROR(1, logger) << "RUN command: invalid kstrength=0x" << std::hex
                            << kstrength << std::dec;

      ERR_CODE = 0x06000000 | ((mode & 0xFF) << 8) | (kstrength & 0xFF);
      INTR_STATE.kmac_err = 1;

      return false;
    }

    // Read kmac_en to determine if this is KMAC mode
    uint32_t kmac_en = CFG_SHADOWED.kmac_en;

    // For KMAC extended output: advance STATE window to show next chunk
    if (kmac_en && xof_total_length > 0) {
      // RUN advances by standard read chunk size (256 bits = 32 bytes)
      const size_t run_advance_size = 32;

      // Advance window position by typical software read size
      xof_output_offset += run_advance_size;

      // Check if more output is available
      if (xof_output_offset >= xof_total_length) {
        // No more output available - reset to show last available chunk
        xof_output_offset = (xof_total_length > run_advance_size)
                                ? (xof_total_length - run_advance_size)
                                : 0;
        CSML_INFO(2, logger)
            << "RUN command: no more output beyond offset " << xof_output_offset
            << " (total=" << xof_total_length << ")";
        // Keep STATE window at last position
        return true;
      }

      // Update digest_buffer to show next chunk from xof_full_output
      size_t bytes_remaining = xof_total_length - xof_output_offset;
      size_t bytes_to_copy = std::min(run_advance_size, bytes_remaining);

      // Shift digest_buffer: move data from xof_full_output at new offset
      std::memmove(digest_buffer, xof_full_output + xof_output_offset,
                   bytes_to_copy);
      // Clear remaining bytes if chunk is smaller than window
      if (bytes_to_copy < sizeof(digest_buffer)) {
        std::memset(digest_buffer + bytes_to_copy, 0,
                    sizeof(digest_buffer) - bytes_to_copy);
      }
      digest_size = bytes_to_copy;

      CSML_INFO(2, logger) << "RUN: advanced STATE window to offset "
                           << xof_output_offset << ", showing " << bytes_to_copy
                           << " bytes (total=" << xof_total_length << ")";
    } else {
      // Non-KMAC XOF (SHAKE/cSHAKE): advance the STATE window through the
      // buffered XOF output generated during PROCESS.  EVP_DigestFinalXOF cannot
      // be called a second time on the same context, so we slice the next
      // rate-sized block out of xof_full_output (each RUN exposes the next
      // sequential output block, matching hardware squeeze semantics).
      xof_output_offset += rate_bytes;

      if (xof_output_offset >= xof_total_length) {
        CSML_ERROR(1, logger)
            << "RUN command: extended output exhausted at offset "
            << xof_output_offset << " (buffered " << xof_total_length
            << " bytes)";
        ERR_CODE = 0x08000000 | 0x31; // SwCmdSequence: no more output
        INTR_STATE.kmac_err = 1;
        update_fsm_state(KmacState::ERROR);
        return false;
      }

      size_t bytes_remaining = xof_total_length - xof_output_offset;
      size_t bytes_to_copy =
          std::min(static_cast<size_t>(rate_bytes), bytes_remaining);
      std::memcpy(digest_buffer, xof_full_output + xof_output_offset,
                  bytes_to_copy);
      digest_size = static_cast<unsigned int>(bytes_to_copy);
      CSML_INFO(2, logger) << "RUN: advanced SHAKE/cSHAKE STATE window to offset "
                           << xof_output_offset << ", showing " << bytes_to_copy
                           << " bytes (buffered " << xof_total_length << ")";
    }

    // If EnMasking=1, regenerate two random shares for the new output block
    if (EnMasking) {
      CSML_INFO(2, logger)
          << "EnMasking=1: Generating masked shares for extended output";

      // Generate random share0
      for (unsigned int i = 0; i < digest_size; i++) {
        // Simple pseudo-random generation (TLM functional model)
        // Real hardware would use EDN entropy
        digest_share0[i] = static_cast<uint8_t>(rand() & 0xFF);

        // Compute share1 = digest XOR share0
        digest_share1[i] = digest_buffer[i] ^ digest_share0[i];
      }

      CSML_INFO(2, logger)
          << "Masked shares generated (share0 XOR share1 = digest)";
    } else {
      // EnMasking=0: Copy digest to share0, zero share1
      std::memcpy(digest_share0, digest_buffer, digest_size);
      std::memset(digest_share1, 0, digest_size);

      CSML_INFO(2, logger)
          << "EnMasking=0: Digest stored in share0, share1 zeroed";
    }

    // FSM remains in SQUEEZE state (no transition)
    // STATUS.sha3_squeeze briefly clears then reasserts during hardware
    // execution For functional TLM model, we keep it asserted (STATE window
    // immediately readable)

    CSML_INFO(2, logger) << "RUN command complete: FSM remains in SQUEEZE, "
                            "STATE window updated with new output block";

    return true;
  }

  // =====================================================================
  // DONE Command (0x16): Any state → IDLE
  // =====================================================================
  else if (cmd_field == 0x16) {
    CSML_INFO(2, logger)
        << "DONE command: returning to IDLE state with cleanup";

    // =====================================================================
    // FUNC-KMAC-001 Phase 3: SHA3 Cleanup
    // =====================================================================

    // Reset OpenSSL context (zeroize internal state)
    if (evp_md_ctx != nullptr) {
      EVP_MD_CTX_reset(static_cast<EVP_MD_CTX *>(evp_md_ctx));
      CSML_INFO(2, logger)
          << "OpenSSL EVP context reset (internal state zeroized)";
    }

    // Clean up KMAC EVP_MAC contexts if allocated
    if (evp_mac_ctx != nullptr) {
      EVP_MAC_CTX_free(static_cast<EVP_MAC_CTX *>(evp_mac_ctx));
      evp_mac_ctx = nullptr;
      CSML_INFO(2, logger) << "OpenSSL EVP_MAC context freed";
    }
    if (evp_mac != nullptr) {
      EVP_MAC_free(static_cast<EVP_MAC *>(evp_mac));
      evp_mac = nullptr;
      CSML_INFO(2, logger) << "OpenSSL EVP_MAC algorithm freed";
    }

    // Clear packer state
    packer_position = 0;
    std::memset(packer_buffer, 0, sizeof(packer_buffer));

    // Clear FIFO
    while (!msg_fifo.empty()) {
      msg_fifo.pop();
    }
    fifo_depth = 0;

    // Clear digest buffers (security: zeroize output)
    digest_size = 0;
    std::memset(digest_buffer, 0, sizeof(digest_buffer));
    std::memset(digest_share0, 0, sizeof(digest_share0));
    std::memset(digest_share1, 0, sizeof(digest_share1));

    CSML_INFO(2, logger)
        << "All internal state cleared (packer, FIFO, digest buffers)";

    // Transition to IDLE (allowed from any state)
    update_fsm_state(KmacState::IDLE);

    // FUNC-KMAC-013: Increment ENTROPY_REFRESH_HASH_CNT after successful operation
    // This counter tracks the number of hash operations since last PRNG reseed
    ENTROPY_REFRESH_HASH_CNT = (ENTROPY_REFRESH_HASH_CNT + 1) & 0x3FF; // 10-bit counter
    CSML_INFO(2, logger) << "FUNC-KMAC-013: ENTROPY_REFRESH_HASH_CNT incremented to "
                         << ENTROPY_REFRESH_HASH_CNT;

    // Auto-set CFG_REGWEN.en (unlock configuration)
    CFG_REGWEN.en = 1;
    CSML_INFO(2, logger)
        << "CFG_REGWEN.en auto-set to 1 (configuration unlocked)";

    // FUNC-KMAC-019: Clear ERR_CODE on successful DONE command
    // Per specification: ERR_CODE cleared only by Done command (on success) or
    // reset ERR_CODE persists across INTR_STATE interrupt clear
    if (static_cast<uint32_t>(ERR_CODE) != 0) {
      CSML_INFO(2, logger) << "FUNC-KMAC-019: Clearing ERR_CODE after "
                              "successful DONE (ERR_CODE was 0x"
                           << std::hex << static_cast<uint32_t>(ERR_CODE)
                           << std::dec << ")";
      ERR_CODE = 0;
    }

    // FUNC-KMAC-024: Apply temporal decoupling for DONE command cleanup
    // Architecture map timing constraint: register-access-latency = 5 cycles
    const double clk_freq_hz = 100e6;          // Default 100 MHz
    const uint32_t done_processing_cycles = 5; // Register access latency
    sc_time done_delay = sc_time(done_processing_cycles / clk_freq_hz, SC_SEC);

    // Accumulate local time in quantum keeper
    m_qk.inc(done_delay);

    // Synchronize with global quantum if needed
    if (m_qk.need_sync()) {
      m_qk.sync();
    }

    CSML_INFO(2, logger) << "FUNC-KMAC-024: DONE command delay=" << done_delay
                         << " (local_time=" << m_qk.get_local_time() << ")";

    return true;
  }

  // CMD register bits are R0W1C - they self-clear after action
  // Hardware automatically clears all written bits
  // No need to update CMD register storage

  return true;
}

/******************************************************************************
 * @brief STATUS register read callback handler
 *
 * Dynamically constructs STATUS register value from current FSM state and
 * other hardware status signals.
 ******************************************************************************/
bool kmac_ip::handle_read_STATUS(uint32_t &value, uint32_t read_mask) {
  // Construct STATUS register value dynamically
  value = 0;

  // Bit 0 - sha3_idle
  if (fsm_state == KmacState::IDLE) {
    value |= (1 << 0);
  }

  // Bit 1 - sha3_absorb
  if (fsm_state == KmacState::ABSORB) {
    value |= (1 << 1);
  }

  // Bit 2 - sha3_squeeze
  if (fsm_state == KmacState::SQUEEZE) {
    value |= (1 << 2);
  }

  // Bits [12:8] - fifo_depth (actual FIFO depth tracking)
  value |= ((fifo_depth & 0x1F) << 8);

  // Bit 14 - fifo_empty (computed from fifo_depth)
  if (fifo_depth == 0) {
    value |= (1 << 14);
  }

  // Bit 15 - fifo_full (computed from fifo_depth vs max_fifo_depth)
  if (fifo_depth >= max_fifo_depth) {
    value |= (1 << 15);
  }

  // Bit 16 - ALERT_FATAL_FAULT (read from STATUS register storage)
  if (static_cast<uint32_t>(STATUS.ALERT_FATAL_FAULT) != 0) {
    value |= (1 << 16);
  }

  // Bit 17 - ALERT_RECOV_CTRL_UPDATE_ERR (read from STATUS register storage)
  if (static_cast<uint32_t>(STATUS.ALERT_RECOV_CTRL_UPDATE_ERR) != 0) {
    value |= (1 << 17);
  }

  CSML_INFO(2, logger) << "STATUS read: value=0x" << std::hex << value
                       << std::dec << " (idle=" << ((value & 0x1) != 0)
                       << ", absorb=" << ((value & 0x2) != 0)
                       << ", squeeze=" << ((value & 0x4) != 0)
                       << ", fifo_depth=" << fifo_depth << ")";

  return true;
}

/******************************************************************************
 * @brief MSG_FIFO window write callback handler
 *
 * Implements message data absorption with byte packing and FIFO management.
 * This is the core data path for SHA3 message processing.
 ******************************************************************************/
bool kmac_ip::handle_write_MSG_FIFO(unsigned int index, uint32_t value,
                                    uint8_t byte_enable) {
  // =====================================================================
  // FUNC-KMAC-001 Phase 4: MSG_FIFO Write with Packer
  // =====================================================================

  // Validate index bounds (0-511)
  if (index >= 512) {
    CSML_ERROR(1, logger) << "MSG_FIFO write rejected: invalid index " << index;
    return false;
  }

  // FUNC-KMAC-021: Block MSG_FIFO writes when in ESCALATION_LOCKED state
  if (fsm_state == KmacState::ESCALATION_LOCKED) {
    CSML_ERROR(1, logger) << "MSG_FIFO[" << index
                          << "] write rejected: FSM in ESCALATION_LOCKED state "
                             "(lc_escalate_en_i active)";
    return false;
  }

  // Check FSM state: Only accept writes in ABSORB state
  if (fsm_state != KmacState::ABSORB) {
    CSML_ERROR(1, logger)
        << "MSG_FIFO write rejected: not in ABSORB state (current state="
        << static_cast<int>(fsm_state) << ")";

    // FUNC-KMAC-019: Set SwPushedMsgFifo error (0x02)
    // Debug bits [15:8] contain KMAC_APP FSM state, bits [7:0] contain mux
    // selection For TLM: Using address info in debug bits [15:8], mux=1 (SW) in
    // bits [7:0]
    ERR_CODE = 0x02000000 | (static_cast<uint8_t>(fsm_state) << 8) | 0x01;
    INTR_STATE.kmac_err = 1;
    update_fsm_state(KmacState::ERROR);

    return false;
  }

  // FUNC-KMAC-007: Block software MSG_FIFO writes during application interface
  // operation
  if (app_interface_active) {
    CSML_ERROR(1, logger)
        << "MSG_FIFO write rejected: application interface active";

    // FUNC-KMAC-019: Set SwPushedMsgFifo error (0x02)
    // Debug bits [15:8] contain KMAC_APP FSM state, bits [7:0] = 0x02 (App mux
    // selected)
    ERR_CODE = 0x02000000 | (static_cast<uint8_t>(fsm_state) << 8) | 0x02;
    INTR_STATE.kmac_err = 1;
    update_fsm_state(KmacState::ERROR);

    return false;
  }

  // Read endianness configuration
  uint32_t msg_endianness = CFG_SHADOWED.msg_endianness;

  // Apply byte-swap if msg_endianness=1 (swap on 32-bit word granularity)
  uint32_t write_value = value;
  if (msg_endianness == 1) {
    // Swap bytes within 32-bit word: ABCD -> DCBA
    write_value = ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
                  ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
  }

  // =====================================================================
  // IMPROVED: Extract bytes with length and offset tracking
  // This follows HMAC's approach for robust byte handling
  // =====================================================================
  unsigned int data_length = 0;
  unsigned int byte_offset = 0;
  uint8_t data_bytes[4] = {0, 0, 0, 0};

  // Find first set bit (LSB) to determine byte_offset
  // This identifies which byte lane the write starts at
  for (unsigned int i = 0; i < 4; i++) {
    if (byte_enable & (1 << i)) {
      byte_offset = i;
      break;
    }
  }

  // Extract all enabled bytes into clean array
  // byte_enable bits: [3:0] correspond to bytes [3:0]
  unsigned int src_byte_idx = 0;
  for (unsigned int i = 0; i < 4; i++) {
    if (byte_enable & (1 << i)) {
      data_bytes[src_byte_idx++] =
          static_cast<uint8_t>((write_value >> (i * 8)) & 0xFF);
      data_length++;
    }
  }

  CSML_DEBUG(3, logger) << "MSG_FIFO[" << index << "] write: " << data_length
                        << " bytes, byte_enable=0x" << std::hex
                        << (int)byte_enable << std::dec
                        << ", byte_offset=" << byte_offset;

  // =====================================================================
  // Sequential packing for MSG_FIFO
  // MSG_FIFO is a streaming FIFO - all bytes pack sequentially regardless
  // of their byte lane in the write transaction. Lane-based packing would
  // cause consecutive word writes to overwrite each other.
  // =====================================================================
  for (unsigned int i = 0; i < data_length; i++) {
    // Always use sequential packing for MSG_FIFO
    unsigned int byte_pos = packer_position % 8;
    CSML_DEBUG(4, logger)
        << "Sequential pack: byte at byte_pos=" << byte_pos
        << " (packer_position=" << packer_position << ")";

    // Store byte in packer buffer
    packer_buffer[byte_pos] = data_bytes[i];
    packer_position++;

    // Check if 64-bit entry is complete (8 bytes accumulated)
    if (packer_position >= 8) {
      // Push complete 64-bit entry to FIFO
      uint64_t fifo_entry = 0;
      for (int j = 0; j < 8; j++) {
        fifo_entry |= (static_cast<uint64_t>(packer_buffer[j]) << (j * 8));
      }

      msg_fifo.push(fifo_entry);
      fifo_depth++;

      CSML_INFO(2, logger) << "Pushed 64-bit entry to FIFO: 0x" << std::hex
                           << fifo_entry << std::dec
                           << " (fifo_depth=" << fifo_depth << ")";

      // Debug: Log packer buffer contents
      std::stringstream ss_packer;
      for (int j = 0; j < 8; j++) {
        ss_packer << std::hex << std::setfill('0') << std::setw(2)
                  << (int)packer_buffer[j];
        if (j < 7)
          ss_packer << " ";
      }
      CSML_INFO(2, logger) << "MSG absorbed (8 bytes): " << ss_packer.str();

      // Update OpenSSL digest with this 64-bit entry
      // All modes (SHA3/SHAKE/cSHAKE/KMAC) use EVP_DigestUpdate
      EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
      if (EVP_DigestUpdate(ctx, packer_buffer, 8) != 1) {
        CSML_ERROR(1, logger) << "OpenSSL EVP_DigestUpdate failed";
        return false;
      }

      // Reset packer for next entry
      packer_position = 0;
      std::memset(packer_buffer, 0, sizeof(packer_buffer));

      // FUNC-KMAC-024: Check for FIFO full (implement backpressure with
      // temporal decoupling)
      if (fifo_depth >= max_fifo_depth) {
        CSML_INFO(2, logger)
            << "FIFO full (" << fifo_depth << "/" << max_fifo_depth
            << ") - applying backpressure with temporal decoupling";

        // FUNC-KMAC-024: Calculate backpressure delay based on clock frequency
        // Architecture map timing constraint: 100 cycles for SHA3 processing
        // Use abstract clock frequency input (default 100 MHz if not driven)
        // delay = 100 cycles / clk_i_frequency_hz
        const double clk_freq_hz = 100e6; // Default 100 MHz
        const uint32_t processing_cycles = 100;
        sc_time backpressure_delay =
            sc_time(processing_cycles / clk_freq_hz, SC_SEC);

        // Accumulate local time in quantum keeper
        m_qk.inc(backpressure_delay);

        // Synchronize with global quantum if needed
        if (m_qk.need_sync()) {
          m_qk.sync();
        }

        CSML_INFO(2, logger)
            << "FUNC-KMAC-024: Backpressure delay=" << backpressure_delay
            << " (local_time=" << m_qk.get_local_time() << ")";
      }
    }
  }

  return true;
}

/******************************************************************************
 * @brief STATE window read callback handler
 *
 * Implements conditional digest output access with key protection and
 * masking support.
 ******************************************************************************/
bool kmac_ip::handle_read_STATE(unsigned int index, uint32_t &value) {
  // =====================================================================
  // FUNC-KMAC-001 Phase 5: STATE Window Read with Conditional Access
  // =====================================================================

  // Validate index bounds (0-127)
  if (index >= 128) {
    CSML_ERROR(1, logger) << "STATE read rejected: invalid index " << index;
    value = 0;
    return false;
  }

  // FUNC-KMAC-007: Block STATE reads during application interface operation
  // (key protection)
  if (app_interface_active) {
    value = 0;
    CSML_INFO(2, logger)
        << "STATE[" << index
        << "] read: returning 0 (application interface active, key protection)";
    return true;
  }

  // Key protection: Return 0 if not in SQUEEZE state
  if (fsm_state != KmacState::SQUEEZE) {
    value = 0;
    CSML_INFO(2, logger) << "STATE[" << index
                         << "] read: returning 0 (key protection, fsm_state="
                         << static_cast<int>(fsm_state) << ")";
    return true;
  }

  // Read endianness configuration
  uint32_t state_endianness = CFG_SHADOWED.state_endianness;

  // Convert index to byte offset within STATE window (512 bytes total)
  // Index 0-127 maps to bytes 0-511
  // Share0 region: indices 0-63 (bytes 0-255)
  // Share1 region: indices 64-127 (bytes 256-511)
  uint32_t byte_offset = index * 4;

  const uint8_t *source_buffer = nullptr;
  unsigned int buffer_offset = 0;

  // Determine which share region based on byte offset
  if (byte_offset < 256) {
    // Share0 region (indices 0-63, bytes 0-255)
    source_buffer = (EnMasking) ? digest_share0 : digest_buffer;
    buffer_offset = byte_offset;
  } else {
    // Share1 region (indices 64-127, bytes 256-511)
    source_buffer = (EnMasking) ? digest_share1 : nullptr;
    buffer_offset = byte_offset - 256;
  }

  // If source_buffer is null (share1 with EnMasking=0), return 0
  if (source_buffer == nullptr) {
    value = 0;
    return true;
  }

  // Bounds checking: Ensure buffer_offset + 4 <= digest_size
  if (buffer_offset + 4 > digest_size) {
    // Partial or out-of-bounds read: Return 0 for bytes beyond digest_size
    value = 0;

    // Fill valid bytes only
    for (unsigned int i = 0; i < 4 && (buffer_offset + i) < digest_size; i++) {
      value |=
          (static_cast<uint32_t>(source_buffer[buffer_offset + i]) << (i * 8));
    }
  } else {
    // Full 32-bit read within digest bounds
    value = 0;
    for (unsigned int i = 0; i < 4; i++) {
      value |=
          (static_cast<uint32_t>(source_buffer[buffer_offset + i]) << (i * 8));
    }
  }

  // Apply byte-swap if state_endianness=1 (swap on 32-bit word granularity)
  if (state_endianness == 1) {
    // Swap bytes within 32-bit word: ABCD -> DCBA
    value = ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
            ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
  }

  CSML_INFO(2, logger) << "STATE[" << index << "] read:"
                       << " byte_offset=" << byte_offset << " value=0x"
                       << std::hex << value << std::dec
                       << " (digest_size=" << digest_size << ")";

  return true;
}

/******************************************************************************
 * FUNC-KMAC-007: Application Interface Methods
 ******************************************************************************/

void kmac_ip::handle_app_request(unsigned int app_index, uint64_t data,
                                 uint8_t strobe, bool last) {
  CSML_INFO(2, logger) << "Application interface [" << app_index
                       << "] request: data=0x" << std::hex << data << std::dec
                       << " strobe=0x" << std::hex << (int)strobe << std::dec
                       << " last=" << last;

  // FUNC-KMAC-021: Block application interface requests when in
  // ESCALATION_LOCKED state
  if (fsm_state == KmacState::ESCALATION_LOCKED) {
    CSML_ERROR(1, logger) << "Application interface [" << app_index
                          << "] request rejected: FSM in ESCALATION_LOCKED "
                             "state (lc_escalate_en_i active)";
    app_operation_error = true;
    return;
  }

  if (!app_interface_active) {
    app_interface_active = true;
    active_app_index = app_index;
    app_operation_done = false;
    app_operation_error = false;
    // Clear shared FIFO for new app operation
    msg_fifo = {};
    fifo_depth = 0;
    CSML_INFO(2, logger) << "Application interface [" << app_index
                         << "] granted access - software MMIO now blocked";
  } else if (active_app_index != app_index) {
    CSML_INFO(2, logger) << "Application interface [" << app_index
                         << "] blocked: app [" << active_app_index
                         << "] active";
    return;
  }

  // Only accumulate data if at least one byte is valid (strobe != 0x00)
  if (strobe != 0x00) {
    msg_fifo.push(data);
    fifo_depth++;
    CSML_INFO(2, logger) << "Application data accumulated in msg_fifo: fifo_depth="
                         << fifo_depth;
  } else {
    CSML_INFO(2, logger)
        << "Application data beat ignored: strobe=0x00 (no valid bytes)";
  }

  if (last) {
    CSML_INFO(2, logger) << "Application last beat - executing operation";
    execute_app_operation(app_index);
  }
}

void kmac_ip::execute_app_operation(unsigned int app_index) {
  CSML_INFO(2, logger) << "Executing application operation [" << app_index
                       << "] message_entries=" << fifo_depth;

  // FUNC-KMAC-019: Check KeyNotValid error (0x01) for KeyMgr application
  // interface (index 0) Raised when application interface requests KMAC
  // operation but sideloaded key not valid
  if (app_index == 0 &&
      app_algorithm[0] == 0) { // KeyMgr interface using KMAC mode
    // Check KeyMgr push sideload key validity
    if (!m_keymgr_key_valid) {
        CSML_ERROR(1, logger) << "FUNC-KMAC-019: KeyNotValid error (0x01) - "
                              << "KeyMgr application interface requested KMAC "
                                 "operation but sideloaded key not valid";

        // Set ERR_CODE: 0x01 (KeyNotValid)
        // Debug bits [23:0] reserved (set to 0)
        // Fatal error for application interface - requires system reset, no
        // recovery via err_processed
        ERR_CODE = 0x01000000;
        INTR_STATE.kmac_err = 1;

        // Set fatal error status
        STATUS.ALERT_FATAL_FAULT = 1;

        // Application operation error flag
        app_operation_error = true;
        app_operation_done = true;

        CSML_INFO(2, logger)
            << "FUNC-KMAC-019: KeyNotValid is FATAL - requires system reset, "
               "no err_processed recovery";
        return;
    }
  }

  // Validate non-empty message
  if (msg_fifo.empty()) {
    CSML_ERROR(1, logger)
        << "Application operation failed: Empty message not supported";
    app_operation_error = true;
    app_operation_done = true;
    return;
  }

  EVP_MD_CTX *ctx = static_cast<EVP_MD_CTX *>(evp_md_ctx);
  if (!ctx) {
    CSML_ERROR(1, logger)
        << "Application operation failed: EVP_MD_CTX not allocated";
    app_operation_error = true;
    return;
  }

  EVP_MD_CTX_reset(ctx);
  const EVP_MD *md =
      (app_algorithm[app_index] == 1) ? EVP_shake128() : EVP_shake256();

  if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
    CSML_ERROR(1, logger) << "Application operation failed: EVP_DigestInit_ex";
    app_operation_error = true;
    return;
  }

  // For KMAC mode: absorb key and prefix
  if (app_algorithm[app_index] == 0) {
    uint8_t key_block[32] = {0};
    EVP_DigestUpdate(ctx, key_block, 32);
    uint8_t kmac_prefix[] = {0x01, 0x20, 'K', 'M', 'A', 'C'};
    EVP_DigestUpdate(ctx, kmac_prefix, sizeof(kmac_prefix));
    CSML_INFO(2, logger) << "KMAC key and prefix absorbed";
  } else {
    // cSHAKE: bytepad(encode_string(N) || encode_string(S), rate) per NIST SP
    // 800-185
    const char *customization = (app_index == 1) ? "LC_CTRL" : "ROM_CTRL";
    size_t custom_len = strlen(customization);
    size_t rate = (app_algorithm[app_index] == 1)
                      ? 168
                      : 136; // SHAKE128=168, SHAKE256=136

    // Construct: encode_string("") || encode_string(customization)
    uint8_t prefix_block[64];
    size_t prefix_len = 0;

    // encode_string("") = left_encode(0) || "" = 0x01 0x00
    prefix_block[prefix_len++] = 0x01;
    prefix_block[prefix_len++] = 0x00;

    // encode_string(customization) = left_encode(len*8) || customization
    prefix_block[prefix_len++] = 0x01; // left_encode length = 1 byte
    prefix_block[prefix_len++] = (uint8_t)(custom_len * 8); // bits
    std::memcpy(prefix_block + prefix_len, customization, custom_len);
    prefix_len += custom_len;

    // bytepad(prefix_block, rate) = left_encode(rate) || prefix_block ||
    // 0x00...
    uint8_t bytepadded[168];
    size_t offset = 0;
    bytepadded[offset++] = 0x01;
    bytepadded[offset++] = (uint8_t)(rate & 0xFF);
    std::memcpy(bytepadded + offset, prefix_block, prefix_len);
    offset += prefix_len;
    while (offset < rate)
      bytepadded[offset++] = 0x00;

    EVP_DigestUpdate(ctx, bytepadded, rate);
    CSML_INFO(2, logger) << "cSHAKE bytepad absorbed: " << customization
                         << " (rate=" << rate << ")";
  }

  // Absorb message
  // Absorb message from shared msg_fifo
  while (!msg_fifo.empty()) {
    uint64_t word = msg_fifo.front();
    msg_fifo.pop();
    fifo_depth--;
    uint8_t bytes[8];
    for (int j = 0; j < 8; j++)
      bytes[j] = (word >> (j * 8)) & 0xFF;
    EVP_DigestUpdate(ctx, bytes, 8);
  }
  CSML_INFO(2, logger) << "Application message absorbed";

  // For KMAC: append output length
  if (app_algorithm[app_index] == 0) {
    uint8_t len_enc[] = {0x02, 0x01, 0x00};
    EVP_DigestUpdate(ctx, len_enc, 3);
    CSML_INFO(2, logger) << "KMAC output length encoded";
  }

  // Finalize
  uint8_t digest[32];
  EVP_DigestFinalXOF(ctx, digest, 32);

  // Split into shares
  for (int i = 0; i < 8; i++) {
    uint32_t word = 0;
    for (int j = 0; j < 4; j++)
      word |= (digest[i * 4 + j] << (j * 8));
    if (EnMasking) {
      app_digest_share1[i] = 0xDEADBEEF ^ (i * 0x12345678);
      app_digest_share0[i] = word ^ app_digest_share1[i];
    } else {
      app_digest_share0[i] = word;
      app_digest_share1[i] = 0;
    }
  }

  app_operation_done = true;
  CSML_INFO(2, logger) << "Application operation completed";

  // FUNC-KMAC-024: Apply temporal decoupling for application interface response
  // timing Architecture map timing constraint: application interface operations
  // take Keccak processing time plus additional cycles for key absorption and
  // prefix processing
  const double clk_freq_hz = 100e6;           // Default 100 MHz
  const uint32_t app_processing_cycles = 150; // 96 (Keccak) + 54 (key/prefix)
  sc_time app_delay = sc_time(app_processing_cycles / clk_freq_hz, SC_SEC);

  // Accumulate local time in quantum keeper
  m_qk.inc(app_delay);

  // Synchronize with global quantum if needed
  if (m_qk.need_sync()) {
    m_qk.sync();
  }

  CSML_INFO(2, logger) << "FUNC-KMAC-024: Application interface [" << app_index
                       << "] response delay=" << app_delay
                       << " (local_time=" << m_qk.get_local_time() << ")";
}

void kmac_ip::clear_app_state() {
  CSML_INFO(2, logger) << "Clearing application interface state";
  app_interface_active = false;
  active_app_index = 0;
  app_operation_done = false;
  app_operation_error = false;
  // msg_fifo already consumed by execute_app_operation()
  std::memset(app_digest_share0, 0, sizeof(app_digest_share0));
  std::memset(app_digest_share1, 0, sizeof(app_digest_share1));
}
