
#pragma once
#include "crng_basetest.h"
#include "csml_logger.h"

class crng_test : public crng_basetest
{
public:
   CsmlLogger logger;

   crng_test(sc_module_name name);

   // Register access methods
   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   void register_read_32(unsigned int offset, uint32_t &read_value);
   void register_write_32(unsigned int offset, uint32_t write_value);

   // =============================================================================
   // Note: Port interfaces are declared but not used with crng_base
   // These will be utilized when the full CRNG class with ports is implemented
   // =============================================================================

   ~crng_test() {}

private:
   void initialize_signals();
};