// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_reset_ctrl_register.h"
#include <string>

class sep_reset_ctrl_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<64>::DT DT;
    sep_reset_ctrl_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned long long)),
       SW_RESET_N(std::string(name) + ".SW_RESET_N", memory, (0x0 + 0x00)/sizeof(unsigned long long))
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;
      regmodel::Memory<64> memory;
      tlm_utils::simple_target_socket<regmodel::Memory<64>, 32> target_socket;

      
      sep_reset_ctrl::SW_RESET_N_type<64> SW_RESET_N;
      
      void reset_all_registers();
};
