// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file edn_test.h
 * @brief EDN test harness with complementary port interfaces
 *
 * Provides test infrastructure for EDN verification including:
 * - CSRNG interface simulation (command handler and entropy provider)
 * - Peripheral endpoint simulation (8 endpoints with request generators)
 * - Interrupt and alert monitoring
 * - Register access utilities
 * - Clock and reset control
 */

#pragma once
#include "edn_basetest.h"
#include "csml_logger.h"
#include <queue>

class edn_ip;

/**
 * @class edn_test
 * @brief EDN test harness with CSRNG and endpoint simulation
 *
 * Extends edn_basetest to provide complete test environment with:
 * - Complementary ports to bind with EDN model
 * - CSRNG interface implementation for command/data simulation
 * - Peripheral endpoint request generators
 * - Register access helper functions
 */
class edn_test : public edn_basetest
{
  public:
   edn_ip* m_dut = nullptr;
   void set_dut(edn_ip* dut) { m_dut = dut; }

   // =========================================================================
   // Interrupt Monitoring (Inputs from Model)
   // =========================================================================

   /// Software command completion interrupt monitor
   sc_in<bool> intr_edn_cmd_req_done;

   /// Fatal error interrupt monitor
   sc_in<bool> intr_edn_fatal_err;

   // =========================================================================
   // Alert Monitoring (Inputs from Model)
   // =========================================================================

   /// Recoverable alert monitor
   sc_in<bool> alert_recov_alert;

   /// Fatal alert monitor
   sc_in<bool> alert_fatal_alert;

   // =========================================================================
   // Clock and Reset Control (Outputs to Model)
   // =========================================================================

   /// Clock frequency output (Hz)
   sc_out<double> clk_o;

   /// Reset output (active-low)
   sc_out<bool> rst_no;

   /**
    * @brief Constructor
    * @param name SystemC module name
    *
    * Initializes test harness with port arrays, channels, and logger.
    */
   SC_HAS_PROCESS(edn_test);
   edn_test(sc_module_name name);

   /**
    * @brief Destructor
    */
   ~edn_test();

   /**
    * @brief Read 8-bit value from register
    * @param offset Register byte offset
    * @param read_value Output parameter for read data
    */
   void register_read_8(unsigned int offset, uint8_t &read_value);

   /**
    * @brief Write 8-bit value to register
    * @param offset Register byte offset
    * @param write_value Data to write
    */
   void register_write_8(unsigned int offset, uint8_t write_value);

   /**
    * @brief Read 32-bit value from register
    * @param offset Register byte offset
    * @param read_value Output parameter for read data
    */
   void register_read_32(unsigned int offset, uint32_t &read_value);

   /**
    * @brief Write 32-bit value to register
    * @param offset Register byte offset
    * @param write_value Data to write
    */
   void register_write_32(unsigned int offset, uint32_t write_value);

   /**
    * @brief Apply reset sequence
    * @param duration Reset duration in nanoseconds
    */
   void apply_reset(double duration_ns = 100.0);

   /**
    * @brief Set clock frequency
    * @param freq_hz Clock frequency in Hz
    */
   void set_clock_frequency(double freq_hz);

   /// CSML logger instance
   CsmlLogger logger;

   // Dummy methods to satisfy legacy test calls after decoupling
   void request_entropy(unsigned int endpoint_id) { (void)endpoint_id; }
   void provide_csrng_entropy(const uint32_t genbits[4], bool fips_compliance);
   void set_forced_csrng_ack_status(uint32_t status);
   
   // Dummy signals to satisfy legacy test calls
   sc_signal<bool> edn_req[8];
   sc_signal<bool> edn_ack[8];
   sc_signal<sc_uint<32>> edn_bus[8];
   sc_signal<bool> edn_fips[8];

   std::queue<uint32_t> m_mock_buffer;
   bool m_mock_fips = false;
   unsigned int m_rr_index = 0;
   bool m_clear_mock_outputs = false;

   bool edn_is_enabled() const;
   void clear_mock_endpoint_state();
   void mock_endpoint_process();
};