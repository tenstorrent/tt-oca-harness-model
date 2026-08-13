
#pragma once
#include "sep_scratch_warm_basetest.h"

class sep_scratch_warm_test : public sep_scratch_warm_basetest
{
public:
   sep_scratch_warm_test(sc_module_name name) : sep_scratch_warm_basetest(name)
   {
   }

   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   ~sep_scratch_warm_test() {}
};