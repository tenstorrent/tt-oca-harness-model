
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class sep_reset_ctrl_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<sep_reset_ctrl_basetest, 32> initiator_socket;
    enum Register_offset
    {
      SW_RESET_N_OFFSET = (0x0 + 0x00)  
    };

    enum Register_Read_Access
    {
      SW_RESET_N_READ = (0x1f)  
    };

    enum Register_Write_Access
    {
      SW_RESET_N_WRITE = (0x1f)
    };

    enum Register_Reset_Val
    {
      // km_n resets asserted (0), the other four released — sep_reset_ctrl.rdl
      // and sep_reset_ctrl_reg.sv:318-410.
      SW_RESET_N_RESET = (0x000000000000001e)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  uint64_t read_mask;
		  uint64_t write_mask;
		  uint64_t reg_reset;
		  std::string reg_name;
    };

    sep_reset_ctrl_basetest(sc_module_name name) : sc_module(name)
    {

    }

};