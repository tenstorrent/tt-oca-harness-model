// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "hmac_basetest.h"
#include "reg_logger.h"

class hmac_test : public hmac_basetest
{
public:
   // Interrupt inputs (received from model)
   sc_in<bool> intr_hmac_done;
   sc_in<bool> intr_fifo_empty;
   sc_in<bool> intr_hmac_err;

   // Alert input (received from model)
   sc_in<bool> alert_fatal_fault;

   // Clock output (driven by test)
   sc_out<double> clk_i;

   // Reset output (driven by test)
   sc_out<bool> rst_ni;

   // Initiator socket for key manager sideload bus
   tlm_utils::simple_initiator_socket<hmac_test, 32> keymgr_initiator_socket;

   // Key manager sideload register offsets (from hmac_wrapper_key.rdl)
   enum Keymgr_Offset {
      KEYMGR_SHARE0_OFFSET = 0x00,  // KEY_SHARE0[0..7] at 0x00-0x1C
      KEYMGR_SHARE1_OFFSET = 0x20,  // KEY_SHARE1[0..7] at 0x20-0x3C
      KEYMGR_CTRL_OFFSET   = 0x40   // KEY_CTRL (bit 0 = key_valid)
   };

   hmac_test(sc_module_name name) : hmac_basetest(name),
                                     intr_hmac_done("intr_hmac_done"),
                                     intr_fifo_empty("intr_fifo_empty"),
                                     intr_hmac_err("intr_hmac_err"),
                                     alert_fatal_fault("alert_fatal_fault"),
                                     clk_i("clk_i"),
                                     rst_ni("rst_ni"),
                                     keymgr_initiator_socket("keymgr_initiator_socket")
   {
      // Initialize logger
      logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
      logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
      logger.setFunctionTrace(false);
      // Port initialization deferred to after elaboration (in testbench)
   }

   // 32-bit register access functions
   void read_register_32(unsigned int offset, uint32_t &read_value);
   void write_register_32(unsigned int offset, uint32_t write_value);
   void write_register_8(unsigned int offset, uint8_t write_value);
   void write_register_16(unsigned int offset, uint16_t write_value);

   // Write one 32-bit word to the key manager sideload private bus
   void keymgr_write_word(uint64_t offset, uint32_t value);
   // Read is rejected by the model (write-only sideload bus).
   void keymgr_read_word(uint64_t offset, uint32_t &value);
   
   // Assert functions for validation
   void assert_equal(uint32_t expected, uint32_t actual, const char* message);
   void assert_not_equal(uint32_t write_value, uint32_t read_value, const char* message);

   // Assertion failures seen so far. These stop the simulation, which means the
   // testbench's own per-test counter never gets a chance to record them, so
   // sc_main has to consult this separately to return a non-zero exit code.
   uint32_t m_assert_failures = 0;

   // Register helpers that see a non-OK response. The helper used to log and
   // return, so a failed read kept the caller's old value and the suite still
   // passed. sc_main adds this into the process exit code.
   uint32_t m_transport_failures = 0;

   // Status from the most recent key-manager read. The sideload bus is
   // write-only, so a read is expected to be rejected and must not be counted
   // as a surprise transport failure.
   tlm::tlm_response_status m_last_response = tlm::TLM_INCOMPLETE_RESPONSE;

   // Logger instance for structured logging
   RegLogger logger;

   ~hmac_test() {}
};