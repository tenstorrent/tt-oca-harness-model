// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "csrng_basetest.h"
#include "reg_logger.h"
#include "tlm_probe.h"

#include <string>

class csrng_test : public csrng_basetest
{
public:
   RegLogger logger;

   csrng_test(sc_module_name name);

   // Register access methods
   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   void register_read_32(unsigned int offset, uint32_t &read_value);
   void register_write_32(unsigned int offset, uint32_t write_value);

   // =============================================================================
   // Note: Port interfaces are declared but not used with csrng_base
   // These will be utilized when the full CRNG class with ports is implemented
   // =============================================================================

   /// Drive a deliberately malformed payload; the status is returned rather
   /// than recorded, because a rejection is the expected outcome.
   simtlm::access_result probe(simtlm::defect d, const simtlm::target_geometry &geo,
                               tlm::tlm_command cmd);

   /// Transport-failure bookkeeping. A refused register access is recorded here
   /// and fails the enclosing test via testbench's reporting helpers.
   unsigned           transport_failures() const { return m_transport_failures; }
   const std::string &last_transport_error() const { return m_last_transport_error; }
   void               clear_transport_failures();

   ~csrng_test() {}

private:
   void initialize_signals();
   void note_transport(const simtlm::access_result &r, const char *op,
                       unsigned int offset);

   unsigned    m_transport_failures = 0;
   std::string m_last_transport_error;
};