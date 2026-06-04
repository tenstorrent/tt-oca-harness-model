
#pragma once
#include "AVBBus_basetest.h"

class AVBBus_test : public AVBBus_basetest
{
public:
   AVBBus_test(sc_module_name name) : AVBBus_basetest(name)
   {
   }

   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   ~AVBBus_test() {}
};