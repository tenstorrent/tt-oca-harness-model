
#pragma once
#include "csrng_basetest.h"
#include "csml_logger.h"

class csrng_test : public csrng_basetest
{
public:
   CsmlLogger logger;

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

   ~csrng_test() {}

private:
   void initialize_signals();
};