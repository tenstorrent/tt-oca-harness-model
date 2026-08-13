#pragma once
#include "sep_scratch_cold_register.h"
#include <string.h>

class sep_scratch_cold_base : public sc_module
{
  public:
    typedef typename csml_reg<64>::DT DT;
    sep_scratch_cold_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned long long)),
       SCRATCH(std::string(name) + ".SCRATCH", memory, (0x0 + 0x00)/sizeof(unsigned long long), 1)
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;
      csml_memory<64> memory;
      tlm_utils::simple_target_socket<csml_memory<64>, 32> target_socket;

      
      csml_reg_vector<sep_scratch_cold::SCRATCH_type<64>, 8> SCRATCH;
      
      void reset_all_registers();
};
