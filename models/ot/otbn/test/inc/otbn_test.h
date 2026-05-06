/**
 * @file otbn_test.h
 * @brief OTBN test infrastructure with TLM helpers and interface stubs
 * 
 * Provides complete test infrastructure including:
 * - Hardware constant definitions (states, commands, error bits)
 * - TLM register access helpers
 * - Interface stub implementations (EDN, OTP, Key Manager, Life Cycle)
 * - Test utility functions for OTBN verification
 * 
 * The otbn_test class extends otbn_basetest with TLM transaction methods
 * and interface stub instances for comprehensive functional testing.
 */

#pragma once
#include "otbn_basetest.h"
#include "otbn_interfaces.h"
#include "csml_logger.h"
#include <iostream>
#include <vector>

// ============================================================================
// OTBN Hardware Constants
// These constants match the hardware specification and are used for
// self-documenting test code instead of magic numbers.
// ============================================================================

/**
 * @brief OTBN hardware constants namespace
 * 
 * Centralizes all hardware-defined values for states, commands, error bits,
 * and control bits. Using named constants improves test readability.
 */
namespace otbn_constants {
   // =========================================================================
   // State Machine States (STATUS register values)
   // =========================================================================
   constexpr uint32_t STATE_IDLE = 0x00;
   constexpr uint32_t STATE_BUSY_EXECUTE = 0x01;
   constexpr uint32_t STATE_BUSY_SEC_WIPE_DMEM = 0x02;
   constexpr uint32_t STATE_BUSY_SEC_WIPE_IMEM = 0x03;
   constexpr uint32_t STATE_BUSY_SEC_WIPE_INT = 0x04;
   constexpr uint32_t STATE_LOCKED = 0xFF;

   // =========================================================================
   // Command Codes (CMD register values)
   // =========================================================================
   constexpr uint8_t CMD_EXECUTE = 0xD8;
   constexpr uint8_t CMD_SEC_WIPE_DMEM = 0xC3;
   constexpr uint8_t CMD_SEC_WIPE_IMEM = 0x1E;

   // =========================================================================
   // Error Bits (ERR_BITS register bit positions)
   // =========================================================================
   constexpr uint32_t ERR_BAD_DATA_ADDR = (1 << 0);
   constexpr uint32_t ERR_BAD_INSN_ADDR = (1 << 1);
   constexpr uint32_t ERR_CALL_STACK = (1 << 2);
   constexpr uint32_t ERR_ILLEGAL_INSN = (1 << 3);
   constexpr uint32_t ERR_LOOP = (1 << 4);
   constexpr uint32_t ERR_KEY_INVALID = (1 << 5);
   constexpr uint32_t ERR_RND_REP_CHK_FAIL = (1 << 6);
   constexpr uint32_t ERR_RND_FIPS_CHK_FAIL = (1 << 7);
   constexpr uint32_t ERR_IMEM_INTG_VIOLATION = (1 << 16);
   constexpr uint32_t ERR_DMEM_INTG_VIOLATION = (1 << 17);
   constexpr uint32_t ERR_REG_INTG_VIOLATION = (1 << 18);
   constexpr uint32_t ERR_BUS_INTG_VIOLATION = (1 << 19);
   constexpr uint32_t ERR_BAD_INTERNAL_STATE = (1 << 20);
   constexpr uint32_t ERR_ILLEGAL_BUS_ACCESS = (1 << 21);
   constexpr uint32_t ERR_LIFECYCLE_ESCALATION = (1 << 22);
   constexpr uint32_t ERR_FATAL_SOFTWARE = (1 << 23);

   // =========================================================================
   // Fatal Alert Cause Bits (FATAL_ALERT_CAUSE register bit positions)
   // Note: Different bit positions from ERR_BITS!
   // =========================================================================
   constexpr uint32_t FATAL_ILLEGAL_BUS_ACCESS = (1 << 5);     // Bit 5
   constexpr uint32_t FATAL_LIFECYCLE_ESCALATION = (1 << 6);   // Bit 6
   constexpr uint32_t FATAL_SOFTWARE = (1 << 7);               // Bit 7

   // =========================================================================
   // Control Register Bits (CTRL register)
   // =========================================================================
   constexpr uint32_t CTRL_SOFTWARE_ERRS_FATAL = (1 << 0);     // Bit 0
}

// ============================================================================
// Test-side Interface Stub Implementations
// These provide mock/stub behavior for external interfaces during testing
// ============================================================================

// EDN Request Stub - Test side implements request interfaces
class edn_req_stub : public edn_req_if, public sc_module {
public:
    SC_HAS_PROCESS(edn_req_stub);
    edn_req_stub(sc_module_name name) : sc_module(name), request_count(0) {}

    void request_entropy() override {
        request_count++;
        // Mock: just track that a request was made
    }

    uint32_t get_request_count() const { return request_count; }
    void reset_request_count() { request_count = 0; }

private:
    uint32_t request_count;
};

// OTP Key Request Stub
class otp_key_req_stub : public otp_key_req_if, public sc_module {
public:
    SC_HAS_PROCESS(otp_key_req_stub);
    otp_key_req_stub(sc_module_name name) : sc_module(name), request_count(0) {}

    void request_scramble_key() override {
        request_count++;
        // Mock: just track that a scramble key request was made
    }

    uint32_t get_request_count() const { return request_count; }
    void reset_request_count() { request_count = 0; }

private:
    uint32_t request_count;
};

// Life Cycle Controller Request Stub
class lc_ctrl_req_stub : public lc_ctrl_req_if, public sc_module {
public:
    SC_HAS_PROCESS(lc_ctrl_req_stub);
    lc_ctrl_req_stub(sc_module_name name) : sc_module(name), asserted(false) {}

    bool is_asserted() override {
        return asserted;
    }

    void set_asserted(bool value) { asserted = value; }

private:
    bool asserted;
};

// EDN Response Stub - Test side implements response interfaces
class edn_rsp_stub : public edn_rsp_if, public sc_module {
public:
    SC_HAS_PROCESS(edn_rsp_stub);
    edn_rsp_stub(sc_module_name name) : sc_module(name), entropy_ready(false) {}

    bool entropy_available() override {
        return entropy_ready;
    }

    uint32_t get_entropy() override {
        if (!entropy_ready) return 0;
        
        uint32_t val = entropy_queue.front();
        entropy_queue.erase(entropy_queue.begin());
        
        if (entropy_queue.empty()) {
            entropy_ready = false;
        }
        return val;
    }

    // Test control methods
    void set_entropy_available(bool ready) {
        entropy_ready = ready;
    }

    void push_entropy(uint32_t val) {
        entropy_queue.push_back(val);
        entropy_ready = true;
    }

    void clear_entropy() {
        entropy_queue.clear();
        entropy_ready = false;
    }

private:
    bool entropy_ready;
    std::vector<uint32_t> entropy_queue;
};

// ============================================================================
// Key Manager TLM Stub (Specification-Compliant)
// ============================================================================
/**
 * @brief TLM-based Key Manager stub for programming OTBN WDR registers
 *
 * Per otbn_plan.md line 21: "The model has a simple target socket that is
 * connected to an external key manager."
 *
 * This stub implements a TLM initiator that sends transactions to program
 * the KEY_S0_L/H and KEY_S1_L/H WDR registers via the Key Manager TLM socket.
 */
class keymgr_tl_stub : public sc_module {
public:
    tlm_utils::simple_initiator_socket<keymgr_tl_stub> initiator_socket;

    SC_HAS_PROCESS(keymgr_tl_stub);
    keymgr_tl_stub(sc_module_name name) : sc_module(name), initiator_socket("initiator_socket") {}

    /**
     * @brief Program 384-bit key into OTBN WDR registers
     *
     * Key layout (per OpenTitan specification):
     *   - KEY_S0: 256 bits (key[0-7]) → WDR20 (lower) + WDR21 (upper)
     *   - KEY_S1: 128 bits (key[8-11]) → WDR22 (lower) + WDR23 (upper)
     *
     * TLM Address Map:
     *   - 0x00-0x1F: KEY_S0_L (32 bytes) → WDR20
     *   - 0x20-0x3F: KEY_S0_H (32 bytes) → WDR21
     *   - 0x40-0x5F: KEY_S1_L (32 bytes) → WDR22
     *   - 0x60-0x7F: KEY_S1_H (32 bytes) → WDR23
     *
     * @param key 384-bit key as array of 12 x 32-bit words
     */
    void program_key(uint32_t key[12]) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        unsigned char data[32];

        // Write KEY_S0_L (addresses 0x00-0x1F) - Lower 256 bits of KEY_S0
        // key[0-7] → 32 bytes
        for (int i = 0; i < 8; i++) {
            uint32_t word = key[i];
            data[i*4+0] = (word >> 0) & 0xFF;
            data[i*4+1] = (word >> 8) & 0xFF;
            data[i*4+2] = (word >> 16) & 0xFF;
            data[i*4+3] = (word >> 24) & 0xFF;
        }
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x00);  // KEY_S0_L
        trans.set_data_ptr(data);
        trans.set_data_length(32);
        trans.set_streaming_width(32);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
        if (trans.is_response_error()) {
            SC_REPORT_ERROR("KeyMgr TLM", "Failed to write KEY_S0_L");
        }

        // Write KEY_S0_H (addresses 0x20-0x3F) - Upper 256 bits of KEY_S0
        // key[8-9] → first 8 bytes, rest zeros
        for (int i = 0; i < 2; i++) {
            uint32_t word = key[8+i];
            data[i*4+0] = (word >> 0) & 0xFF;
            data[i*4+1] = (word >> 8) & 0xFF;
            data[i*4+2] = (word >> 16) & 0xFF;
            data[i*4+3] = (word >> 24) & 0xFF;
        }
        for (int i = 8; i < 32; i++) {
            data[i] = 0;  // Pad with zeros
        }
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x20);  // KEY_S0_H
        trans.set_data_ptr(data);
        trans.set_data_length(32);
        trans.set_streaming_width(32);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
        if (trans.is_response_error()) {
            SC_REPORT_ERROR("KeyMgr TLM", "Failed to write KEY_S0_H");
        }

        // Write KEY_S1_L (addresses 0x40-0x5F) - Lower 256 bits of KEY_S1
        // key[10-11] → first 8 bytes, rest zeros
        for (int i = 0; i < 2; i++) {
            uint32_t word = key[10+i];
            data[i*4+0] = (word >> 0) & 0xFF;
            data[i*4+1] = (word >> 8) & 0xFF;
            data[i*4+2] = (word >> 16) & 0xFF;
            data[i*4+3] = (word >> 24) & 0xFF;
        }
        for (int i = 8; i < 32; i++) {
            data[i] = 0;  // Pad with zeros
        }
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x40);  // KEY_S1_L
        trans.set_data_ptr(data);
        trans.set_data_length(32);
        trans.set_streaming_width(32);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
        if (trans.is_response_error()) {
            SC_REPORT_ERROR("KeyMgr TLM", "Failed to write KEY_S1_L");
        }

        // Write KEY_S1_H (addresses 0x60-0x7F) - Upper 256 bits of KEY_S1 (all zeros)
        for (int i = 0; i < 32; i++) {
            data[i] = 0;
        }
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0x60);  // KEY_S1_H
        trans.set_data_ptr(data);
        trans.set_data_length(32);
        trans.set_streaming_width(32);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
        if (trans.is_response_error()) {
            SC_REPORT_ERROR("KeyMgr TLM", "Failed to write KEY_S1_H");
        }
    }
};

/**
 * @brief OTBN test class with TLM access and interface stubs
 * 
 * Extends otbn_basetest with:
 * - TLM register read/write methods
 * - Interface stub instances (EDN, OTP, Key Manager, Life Cycle)
 * - Helper methods for interface interaction verification
 * - Register value validation utilities
 * 
 * Used by testbench to interact with OTBN DUT via TLM and verify
 * external interface behavior.
 */
class otbn_test : public otbn_basetest
{
public:
   /// CSML Logger instance
   CsmlLogger logger;  ///< Logger for debug and tracing

   otbn_test(sc_module_name name) : otbn_basetest(name)
   {
      // Initialize logger
      logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
      logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
      logger.setFunctionTrace(false);

      // Instantiate interface stubs
      edn_rnd_req_stub = new edn_req_stub("edn_rnd_req_stub");
      edn_rnd_rsp_stub = new edn_rsp_stub("edn_rnd_rsp_stub");
      edn_urnd_req_stub = new edn_req_stub("edn_urnd_req_stub");
      keymgr_tl_stub_inst = new keymgr_tl_stub("keymgr_tl_stub");
      otp_key_req_stub_inst = new otp_key_req_stub("otp_key_req_stub");
      // lc_escalate_req and lc_rma_req are now sc_in<bool> ports, use sc_signal instead of stubs
      // Signals are declared as member variables, no need to create here
   }

   ~otbn_test() {
      // Clean up interface stubs
      delete edn_rnd_req_stub;
      delete edn_rnd_rsp_stub;
      delete edn_urnd_req_stub;
      delete keymgr_tl_stub_inst;
      delete otp_key_req_stub_inst;
      // lc_escalate_req and lc_rma_req are now sc_signal<bool>, no cleanup needed
   }

   // 32-bit register access functions
   void register_read_32(unsigned int offset, uint32_t &read_value);
   void register_write_32(unsigned int offset, uint32_t write_value);

   // Helper function to validate register values
   bool assert_register_value(unsigned int offset, uint32_t expected_value, const char* reg_name);

   // ============================================================================
   // Interface Stub Helper Methods
   // ============================================================================

   // EDN interface helpers
   uint32_t get_edn_rnd_request_count() const { return edn_rnd_req_stub->get_request_count(); }
   uint32_t get_edn_urnd_request_count() const { return edn_urnd_req_stub->get_request_count(); }
   void reset_edn_rnd_count() { edn_rnd_req_stub->reset_request_count(); }
   void reset_edn_urnd_count() { edn_urnd_req_stub->reset_request_count(); }
   void reset_all_edn_counts() {
      edn_rnd_req_stub->reset_request_count();
      edn_urnd_req_stub->reset_request_count();
   }

   // Key Manager TLM interface helpers
   void program_keymgr_key(uint32_t key[12]) { keymgr_tl_stub_inst->program_key(key); }

   // OTP interface helpers
   uint32_t get_otp_request_count() const { return otp_key_req_stub_inst->get_request_count(); }
   void reset_otp_count() { otp_key_req_stub_inst->reset_request_count(); }

   // Life Cycle Controller helpers
   void set_lc_escalate(bool asserted) { lc_escalate_req_sig.write(asserted); }
   void set_lc_rma(bool asserted) { lc_rma_req_sig.write(asserted); }
   bool get_lc_escalate() const { return lc_escalate_req_sig.read(); }
   bool get_lc_rma() const { return lc_rma_req_sig.read(); }

   // Combined interface counter reset
   void reset_all_interface_counters() {
      reset_all_edn_counts();
      reset_otp_count();
   }

   // Interface stubs - Public members accessible from testbench
   edn_req_stub *edn_rnd_req_stub;
   edn_rsp_stub *edn_rnd_rsp_stub;
   edn_req_stub *edn_urnd_req_stub;
   keymgr_tl_stub *keymgr_tl_stub_inst;
   otp_key_req_stub *otp_key_req_stub_inst;
   // lc_escalate_req and lc_rma_req are now sc_in<bool> ports, use sc_signal instead
   sc_signal<bool> lc_escalate_req_sig;
   sc_signal<bool> lc_rma_req_sig;
};