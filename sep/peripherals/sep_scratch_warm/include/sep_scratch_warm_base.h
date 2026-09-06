// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_scratch_warm_register.h"
#include <string.h>

class sep_scratch_warm_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<64>::DT DT;
    sep_scratch_warm_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned long long)),
       SCRATCH(std::string(name) + ".SCRATCH", memory, (0x0 + 0x00)/sizeof(unsigned long long), 1)
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;
      regmodel::Memory<64> memory;
      tlm_utils::simple_target_socket<regmodel::Memory<64>, 32> target_socket;

      
      regmodel::RegVector<sep_scratch_warm::SCRATCH_type<64>, 8> SCRATCH;
      
      void reset_all_registers();
};
