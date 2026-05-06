
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class aes_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<aes_basetest, 32> initiator_socket;
    enum Register_offset
    {
      ALERT_TEST_OFFSET = (0x0 + 0x00), 
      KEY_SHARE0_OFFSET = (0x4 + 0x00), 
      KEY_SHARE1_OFFSET = (0x24 + 0x00), 
      IV_OFFSET = (0x44 + 0x00), 
      DATA_IN_OFFSET = (0x54 + 0x00), 
      DATA_OUT_OFFSET = (0x64 + 0x00), 
      CTRL_SHADOWED_OFFSET = (0x74 + 0x00), 
      CTRL_AUX_SHADOWED_OFFSET = (0x78 + 0x00), 
      CTRL_AUX_REGWEN_OFFSET = (0x7C + 0x00), 
      TRIGGER_OFFSET = (0x80 + 0x00), 
      STATUS_OFFSET = (0x84 + 0x00)  
    };

    enum Register_Read_Access
    {
      ALERT_TEST_READ = (0x0), 
      KEY_SHARE0_READ = (0x0), 
      KEY_SHARE1_READ = (0x0), 
      IV_READ = (0xffffffff), 
      DATA_IN_READ = (0x0), 
      DATA_OUT_READ = (0xffffffff), 
      CTRL_SHADOWED_READ = (0xffffffff), 
      CTRL_AUX_SHADOWED_READ = (0xffffffff), 
      CTRL_AUX_REGWEN_READ = (0xffffffff), 
      TRIGGER_READ = (0x0), 
      STATUS_READ = (0xffffffff)  
    };

    enum Register_Write_Access
    {
      ALERT_TEST_WRITE = (0xffffffff), 
      KEY_SHARE0_WRITE = (0xffffffff), 
      KEY_SHARE1_WRITE = (0xffffffff), 
      IV_WRITE = (0xffffffff), 
      DATA_IN_WRITE = (0xffffffff), 
      DATA_OUT_WRITE = (0x0), 
      CTRL_SHADOWED_WRITE = (0xffffffff), 
      CTRL_AUX_SHADOWED_WRITE = (0xffffffff), 
      CTRL_AUX_REGWEN_WRITE = (0xffffffff), 
      TRIGGER_WRITE = (0xffffffff), 
      STATUS_WRITE = (0x0)
    };

    enum Register_Reset_Val
    {
      ALERT_TEST_RESET = 0x0,
      KEY_SHARE0_RESET = 0x0,
      KEY_SHARE1_RESET = 0x0,
      IV_RESET = 0x0,
      DATA_IN_RESET = 0x0,
      DATA_OUT_RESET = 0x0,
      CTRL_SHADOWED_RESET = 4481,        // 0x1181 in hex
      CTRL_AUX_SHADOWED_RESET = 0x1,
      CTRL_AUX_REGWEN_RESET = 0x1,
      TRIGGER_RESET = 14,                 // 0xe in hex
      STATUS_RESET = 0x0
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    aes_basetest(sc_module_name name) : sc_module(name)
    {

    }

};