#pragma once
#include "AVBBus_base.h"

class avbbus_ip : public AVBBus_base
{
public:
   SC_HAS_PROCESS(avbbus_ip);
   avbbus_ip(sc_module_name n, unsigned int memory_size = 0xD0) : AVBBus_base(n, memory_size)
   {
      /* FIXME: Implement constructor */
   }
};
