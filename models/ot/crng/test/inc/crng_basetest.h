
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class crng_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<crng_basetest, 32> initiator_socket;
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xc + 0x00), 
      REGWEN_OFFSET = (0x10 + 0x00), 
      CTRL_OFFSET = (0x14 + 0x00), 
      CMD_REQ_OFFSET = (0x18 + 0x00), 
      RESEED_INTERVAL_OFFSET = (0x1c + 0x00), 
      RESEED_COUNTER_0_OFFSET = (0x20 + 0x00), 
      RESEED_COUNTER_1_OFFSET = (0x24 + 0x00), 
      RESEED_COUNTER_2_OFFSET = (0x28 + 0x00), 
      SW_CMD_STS_OFFSET = (0x2c + 0x00), 
      GENBITS_VLD_OFFSET = (0x30 + 0x00), 
      GENBITS_OFFSET = (0x34 + 0x00), 
      INT_STATE_READ_ENABLE_OFFSET = (0x38 + 0x00), 
      INT_STATE_READ_ENABLE_REGWEN_OFFSET = (0x3c + 0x00), 
      INT_STATE_NUM_OFFSET = (0x40 + 0x00), 
      INT_STATE_VAL_OFFSET = (0x44 + 0x00), 
      FIPS_FORCE_OFFSET = (0x48 + 0x00), 
      HW_EXC_STS_OFFSET = (0x4c + 0x00), 
      RECOV_ALERT_STS_OFFSET = (0x50 + 0x00), 
      ERR_CODE_OFFSET = (0x54 + 0x00), 
      ERR_CODE_TEST_OFFSET = (0x58 + 0x00), 
      MAIN_SM_STATE_OFFSET = (0x5c + 0x00)  
    };

    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x0), 
      INTR_ENABLE_READ = (0xf), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      REGWEN_READ = (0x0), 
      CTRL_READ = (0x0), 
      CMD_REQ_READ = (0x0), 
      RESEED_INTERVAL_READ = (0x0), 
      RESEED_COUNTER_0_READ = (0x0), 
      RESEED_COUNTER_1_READ = (0x0), 
      RESEED_COUNTER_2_READ = (0x0), 
      SW_CMD_STS_READ = (0x3e), 
      GENBITS_VLD_READ = (0x3), 
      GENBITS_READ = (0x0), 
      INT_STATE_READ_ENABLE_READ = (0x7), 
      INT_STATE_READ_ENABLE_REGWEN_READ = (0x0), 
      INT_STATE_NUM_READ = (0x0), 
      INT_STATE_VAL_READ = (0x0), 
      FIPS_FORCE_READ = (0x7), 
      HW_EXC_STS_READ = (0x0), 
      RECOV_ALERT_STS_READ = (0xf01f), 
      ERR_CODE_READ = (0x77f0ffff), 
      ERR_CODE_TEST_READ = (0x0), 
      MAIN_SM_STATE_READ = (0xff)  
    };

    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x0), 
      INTR_ENABLE_WRITE = (0xf), 
      INTR_TEST_WRITE = (0xf), 
      ALERT_TEST_WRITE = (0x3), 
      REGWEN_WRITE = (0x0), 
      CTRL_WRITE = (0x0), 
      CMD_REQ_WRITE = (0x0), 
      RESEED_INTERVAL_WRITE = (0x0), 
      RESEED_COUNTER_0_WRITE = (0x0), 
      RESEED_COUNTER_1_WRITE = (0x0), 
      RESEED_COUNTER_2_WRITE = (0x0), 
      SW_CMD_STS_WRITE = (0x0), 
      GENBITS_VLD_WRITE = (0x0), 
      GENBITS_WRITE = (0x0), 
      INT_STATE_READ_ENABLE_WRITE = (0x7), 
      INT_STATE_READ_ENABLE_REGWEN_WRITE = (0x0), 
      INT_STATE_NUM_WRITE = (0x0), 
      INT_STATE_VAL_WRITE = (0x0), 
      FIPS_FORCE_WRITE = (0x7), 
      HW_EXC_STS_WRITE = (0x0), 
      RECOV_ALERT_STS_WRITE = (0x0), 
      ERR_CODE_WRITE = (0x0), 
      ERR_CODE_TEST_WRITE = (0x0), 
      MAIN_SM_STATE_WRITE = (0x0)
    };

    enum Register_Reset_Val
    {
      INTR_STATE_RESET = 0x0, 
      INTR_ENABLE_RESET = 0x0, 
      INTR_TEST_RESET = 0x0, 
      ALERT_TEST_RESET = 0x0, 
      REGWEN_RESET = 0x1, 
      CTRL_RESET = 0x9999, 
      CMD_REQ_RESET = 0x0, 
      RESEED_INTERVAL_RESET = 0xFFFFFFFF, 
      RESEED_COUNTER_0_RESET = 0x0, 
      RESEED_COUNTER_1_RESET = 0x0, 
      RESEED_COUNTER_2_RESET = 0x0, 
      SW_CMD_STS_RESET = 0x0, 
      GENBITS_VLD_RESET = 0x0, 
      GENBITS_RESET = 0x0, 
      INT_STATE_READ_ENABLE_RESET = 0x7, 
      INT_STATE_READ_ENABLE_REGWEN_RESET = 0x1, 
      INT_STATE_NUM_RESET = 0x0, 
      INT_STATE_VAL_RESET = 0x0, 
      FIPS_FORCE_RESET = 0x0, 
      HW_EXC_STS_RESET = 0x0, 
      RECOV_ALERT_STS_RESET = 0x0, 
      ERR_CODE_RESET = 0x0, 
      ERR_CODE_TEST_RESET = 0x0, 
      MAIN_SM_STATE_RESET = 0x4E
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    crng_basetest(sc_module_name name) : sc_module(name)
    {

    }

};