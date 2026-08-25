// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class sep_scratch_cold_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<sep_scratch_cold_basetest, 32> initiator_socket;
    enum Register_offset
    {
      SCRATCH_OFFSET = (0x0 + 0x00)  
    };

    enum Register_Read_Access
    {
      SCRATCH_READ = (0xffffffff)  
    };

    enum Register_Write_Access
    {
      SCRATCH_WRITE = (0xffffffff)
    };

    enum Register_Reset_Val
    {
      SCRATCH_RESET = (0x0)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  uint64_t read_mask;
		  uint64_t write_mask;
		  uint64_t reg_reset;
		  std::string reg_name;
    };

    sep_scratch_cold_basetest(sc_module_name name) : sc_module(name)
    {

    }

};