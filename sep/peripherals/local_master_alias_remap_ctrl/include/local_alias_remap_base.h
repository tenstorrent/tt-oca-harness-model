#pragma once
#include "local_alias_remap_register.h"
#include <string.h>

class local_alias_remap_base : public sc_module
{
  public:
    typedef typename csml_reg<64>::DT DT;
    local_alias_remap_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned long long)),
       REGION_START(std::string(name) + ".REGION_START", memory, 0, 4), 
       REGION_END(std::string(name) + ".REGION_END", memory, 1, 4), 
       REGION_ATTRS(std::string(name) + ".REGION_ATTRS", memory, 2, 4)
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;
      csml_memory<64> memory;
      tlm_utils::simple_target_socket<csml_memory<64>, 32> target_socket;

      
      csml_reg_vector<local_alias_remap::REGION_START_type<64> , 16> REGION_START;
      
      csml_reg_vector<local_alias_remap::REGION_END_type<64>, 16> REGION_END;
      
      csml_reg_vector<local_alias_remap::REGION_ATTRS_type<64>, 16> REGION_ATTRS;
      
      void reset_all_registers();
};
