#pragma once
#include "csrng_register.h"
#include <string.h>

class csrng_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    csrng_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xc + 0x00)/sizeof(unsigned int)), 
       REGWEN(std::string(name) + ".REGWEN", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       CTRL(std::string(name) + ".CTRL", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       CMD_REQ(std::string(name) + ".CMD_REQ", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       RESEED_INTERVAL(std::string(name) + ".RESEED_INTERVAL", memory, (0x1c + 0x00)/sizeof(unsigned int)), 
       RESEED_COUNTER_0(std::string(name) + ".RESEED_COUNTER_0", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       RESEED_COUNTER_1(std::string(name) + ".RESEED_COUNTER_1", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       RESEED_COUNTER_2(std::string(name) + ".RESEED_COUNTER_2", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       SW_CMD_STS(std::string(name) + ".SW_CMD_STS", memory, (0x2c + 0x00)/sizeof(unsigned int)), 
       GENBITS_VLD(std::string(name) + ".GENBITS_VLD", memory, (0x30 + 0x00)/sizeof(unsigned int)), 
       GENBITS(std::string(name) + ".GENBITS", memory, (0x34 + 0x00)/sizeof(unsigned int)), 
       INT_STATE_READ_ENABLE(std::string(name) + ".INT_STATE_READ_ENABLE", memory, (0x38 + 0x00)/sizeof(unsigned int)), 
       INT_STATE_READ_ENABLE_REGWEN(std::string(name) + ".INT_STATE_READ_ENABLE_REGWEN", memory, (0x3c + 0x00)/sizeof(unsigned int)), 
       INT_STATE_NUM(std::string(name) + ".INT_STATE_NUM", memory, (0x40 + 0x00)/sizeof(unsigned int)), 
       INT_STATE_VAL(std::string(name) + ".INT_STATE_VAL", memory, (0x44 + 0x00)/sizeof(unsigned int)), 
       FIPS_FORCE(std::string(name) + ".FIPS_FORCE", memory, (0x48 + 0x00)/sizeof(unsigned int)), 
       HW_EXC_STS(std::string(name) + ".HW_EXC_STS", memory, (0x4c + 0x00)/sizeof(unsigned int)), 
       RECOV_ALERT_STS(std::string(name) + ".RECOV_ALERT_STS", memory, (0x50 + 0x00)/sizeof(unsigned int)), 
       ERR_CODE(std::string(name) + ".ERR_CODE", memory, (0x54 + 0x00)/sizeof(unsigned int)), 
       ERR_CODE_TEST(std::string(name) + ".ERR_CODE_TEST", memory, (0x58 + 0x00)/sizeof(unsigned int)), 
       MAIN_SM_STATE(std::string(name) + ".MAIN_SM_STATE", memory, (0x5c + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      crng::INTR_STATE_type<32> INTR_STATE;

      crng::INTR_ENABLE_type<32> INTR_ENABLE;

      crng::INTR_TEST_type<32> INTR_TEST;

      crng::ALERT_TEST_type<32> ALERT_TEST;

      crng::REGWEN_type<32> REGWEN;

      crng::CTRL_type<32> CTRL;

      crng::CMD_REQ_type<32> CMD_REQ;

      crng::RESEED_INTERVAL_type<32> RESEED_INTERVAL;

      crng::RESEED_COUNTER_0_type<32> RESEED_COUNTER_0;

      crng::RESEED_COUNTER_1_type<32> RESEED_COUNTER_1;

      crng::RESEED_COUNTER_2_type<32> RESEED_COUNTER_2;

      crng::SW_CMD_STS_type<32> SW_CMD_STS;

      crng::GENBITS_VLD_type<32> GENBITS_VLD;

      crng::GENBITS_type<32> GENBITS;

      crng::INT_STATE_READ_ENABLE_type<32> INT_STATE_READ_ENABLE;

      crng::INT_STATE_READ_ENABLE_REGWEN_type<32> INT_STATE_READ_ENABLE_REGWEN;

      crng::INT_STATE_NUM_type<32> INT_STATE_NUM;

      crng::INT_STATE_VAL_type<32> INT_STATE_VAL;

      crng::FIPS_FORCE_type<32> FIPS_FORCE;

      crng::HW_EXC_STS_type<32> HW_EXC_STS;

      crng::RECOV_ALERT_STS_type<32> RECOV_ALERT_STS;

      crng::ERR_CODE_type<32> ERR_CODE;

      crng::ERR_CODE_TEST_type<32> ERR_CODE_TEST;

      crng::MAIN_SM_STATE_type<32> MAIN_SM_STATE;
      
      void reset_all_registers();
};
