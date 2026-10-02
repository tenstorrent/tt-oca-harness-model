// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "aes_basetest.h"
#include "reg_logger.h"

// =============================================================================
// Test-side type definition for keymgr sideload key
// =============================================================================
namespace aes_if {

/**
 * @struct keymgr_sideload_key_t
 * @brief Test helper structure representing a sideload key from the key manager
 *
 * Contains two key shares (for masking support) and a validity indicator.
 * Used by test infrastructure to conveniently group key data for TLM transactions.
 */
struct keymgr_sideload_key_t {
    uint32_t key_share0[8];  ///< Key share 0 (up to 256 bits for AES-256)
    uint32_t key_share1[8];  ///< Key share 1 (up to 256 bits for AES-256)
    bool valid;              ///< true = Key data is valid and can be used by AES

    /** @brief Initializes sideload key structure to a default invalid state */
    keymgr_sideload_key_t() : valid(false) {
        for (int i = 0; i < 8; i++) {
            key_share0[i] = 0;
            key_share1[i] = 0;
        }
    }
};

} // namespace aes_if

// =============================================================================
// Main Test Class
// =============================================================================

class aes_test : public aes_basetest
{
public:
   RegLogger logger;

   aes_test(sc_module_name name);

   // Register access methods
   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   void register_read_32(unsigned int offset, uint32_t &read_value);
   void register_write_32(unsigned int offset, uint32_t write_value);
   /// Partial-lane write via an explicit 4-byte TLM byte-enable mask.
   tlm::tlm_response_status register_write_32_with_be(unsigned int offset,
                                                      uint32_t write_value,
                                                      const unsigned char be[4]);
   /// Raw initiator b_transport for the register socket (TLM matrix).
   tlm::tlm_response_status register_b_transport(tlm::tlm_command cmd,
                                                 uint64_t addr,
                                                 unsigned char* data,
                                                 unsigned int len,
                                                 unsigned int streaming_width,
                                                 unsigned char* be = nullptr,
                                                 unsigned int be_len = 0);
   unsigned int register_transport_dbg(tlm::tlm_command cmd, uint64_t addr,
                                       unsigned char* data, unsigned int len);
   bool register_get_direct_mem_ptr(tlm::tlm_command cmd, uint64_t addr);

   // =============================================================================
   // Complementary Port Interfaces (Test side)
   // =============================================================================

   // Clock and Reset Interfaces (Test drives these)
   sc_out<bool> clk_o;            // Clock signal output to drive AES clk_i
   sc_out<bool> rst_no;           // Active-low reset output to drive AES rst_ni

   // Key Manager Sideload Interface (Test pushes key via TLM to model's keymgr_tl_socket)
   tlm_utils::simple_initiator_socket<aes_test> keymgr_socket;
   void set_keymgr_key(const aes_if::keymgr_sideload_key_t& key);
   void invalidate_keymgr_key();
   bool get_keymgr_key(aes_if::keymgr_sideload_key_t& key);
   /// Returns true when the keymgr socket rejects a read (write-only port).
   bool keymgr_read_rejected(uint64_t offset);

   // Alert Interfaces (Test monitors these)
   sc_in<bool> alert_recov_ctrl_update_err_i;  // Monitor recoverable alert
   sc_in<bool> alert_fatal_fault_i;             // Monitor fatal alert

   // Idle Status Interface (Test monitors this)
   sc_in<bool> idle_i;            // Monitor idle status from AES

   // Life Cycle Escalation Interface (Test drives this)
   sc_out<bool> lc_escalate_en_o; // Drive life cycle escalation

   // Test helper methods
   void wait_for_idle();
   void trigger_reset();
   void trigger_escalation();
   void check_alerts();
   bool is_idle();

   ~aes_test() {}

private:
   void initialize_signals();
   void keymgr_write_word(uint64_t offset, uint32_t value);
   aes_if::keymgr_sideload_key_t m_keymgr_key;
   bool m_keymgr_key_valid;
};
