#pragma once
#include "AVBBus_register.h"
#include <string.h>

class AVBBus_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    AVBBus_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       AVS_FSM_RESET(std::string(name) + ".AVS_FSM_RESET", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       AVS_CTRL(std::string(name) + ".AVS_CTRL", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       AVS_CMD(std::string(name) + ".AVS_CMD", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       AVS_READBACK(std::string(name) + ".AVS_READBACK", memory, (0xc + 0x00)/sizeof(unsigned int), 1), 
       AVS_TARGET_ACKS(std::string(name) + ".AVS_TARGET_ACKS", memory, (0x48 + 0x00)/sizeof(unsigned int)), 
       AVS_LATEST_TARGET_SUBFRAME(std::string(name) + ".AVS_LATEST_TARGET_SUBFRAME", memory, (0x4c + 0x00)/sizeof(unsigned int), 1), 
       AVS_LATEST_TARGET_ACKS(std::string(name) + ".AVS_LATEST_TARGET_ACKS", memory, (0x88 + 0x00)/sizeof(unsigned int)), 
       AVS_NORMAL_STATUS(std::string(name) + ".AVS_NORMAL_STATUS", memory, (0x8c + 0x00)/sizeof(unsigned int)), 
       AVS_CONTROLLER_STATUS(std::string(name) + ".AVS_CONTROLLER_STATUS", memory, (0x90 + 0x00)/sizeof(unsigned int)), 
       AVS_TOTAL_RETRIES(std::string(name) + ".AVS_TOTAL_RETRIES", memory, (0x94 + 0x00)/sizeof(unsigned int)), 
       AVS_FIFOS_STATUS(std::string(name) + ".AVS_FIFOS_STATUS", memory, (0x98 + 0x00)/sizeof(unsigned int)), 
       AVS_INTERRUPT(std::string(name) + ".AVS_INTERRUPT", memory, (0x9c + 0x00)/sizeof(unsigned int)), 
       AVS_INTERRUPT_MASK(std::string(name) + ".AVS_INTERRUPT_MASK", memory, (0xa0 + 0x00)/sizeof(unsigned int)), 
       AVS_INTERRUPT_TEST(std::string(name) + ".AVS_INTERRUPT_TEST", memory, (0xa4 + 0x00)/sizeof(unsigned int)), 
       AVS_TARGET_ISSUED_INTERRUPT_IDS(std::string(name) + ".AVS_TARGET_ISSUED_INTERRUPT_IDS", memory, (0xa8 + 0x00)/sizeof(unsigned int)), 
       AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS(std::string(name) + ".AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS", memory, (0xac + 0x00)/sizeof(unsigned int)), 
       AVS_TARGET_BAD_CRC_INT_IDS(std::string(name) + ".AVS_TARGET_BAD_CRC_INT_IDS", memory, (0xb0 + 0x00)/sizeof(unsigned int)), 
       AVS_RETRY_CFG(std::string(name) + ".AVS_RETRY_CFG", memory, (0xc0 + 0x00)/sizeof(unsigned int)), 
       AVS_CLK_CFG(std::string(name) + ".AVS_CLK_CFG", memory, (0xc4 + 0x00)/sizeof(unsigned int)), 
       AVS_THROTTLE_CFG(std::string(name) + ".AVS_THROTTLE_CFG", memory, (0xc8 + 0x00)/sizeof(unsigned int)), 
       AVS_CONFIG(std::string(name) + ".AVS_CONFIG", memory, (0xcc + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      AVBBus::AVS_FSM_RESET_type<32> AVS_FSM_RESET;
      
      AVBBus::AVS_CTRL_type<32> AVS_CTRL;
      
      AVBBus::AVS_CMD_type<32> AVS_CMD;
      
      csml_reg_vector<AVBBus::AVS_READBACK_type<32>, 15> AVS_READBACK;
      
      AVBBus::AVS_TARGET_ACKS_type<32> AVS_TARGET_ACKS;
      
      csml_reg_vector<AVBBus::AVS_LATEST_TARGET_SUBFRAME_type<32>, 15> AVS_LATEST_TARGET_SUBFRAME;
      
      AVBBus::AVS_LATEST_TARGET_ACKS_type<32> AVS_LATEST_TARGET_ACKS;
      
      AVBBus::AVS_NORMAL_STATUS_type<32> AVS_NORMAL_STATUS;
      
      AVBBus::AVS_CONTROLLER_STATUS_type<32> AVS_CONTROLLER_STATUS;
      
      AVBBus::AVS_TOTAL_RETRIES_type<32> AVS_TOTAL_RETRIES;
      
      AVBBus::AVS_FIFOS_STATUS_type<32> AVS_FIFOS_STATUS;
      
      AVBBus::AVS_INTERRUPT_type<32> AVS_INTERRUPT;
      
      AVBBus::AVS_INTERRUPT_MASK_type<32> AVS_INTERRUPT_MASK;
      
      AVBBus::AVS_INTERRUPT_TEST_type<32> AVS_INTERRUPT_TEST;
      
      AVBBus::AVS_TARGET_ISSUED_INTERRUPT_IDS_type<32> AVS_TARGET_ISSUED_INTERRUPT_IDS;
      
      AVBBus::AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_type<32> AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS;
      
      AVBBus::AVS_TARGET_BAD_CRC_INT_IDS_type<32> AVS_TARGET_BAD_CRC_INT_IDS;
      
      AVBBus::AVS_RETRY_CFG_type<32> AVS_RETRY_CFG;
      
      AVBBus::AVS_CLK_CFG_type<32> AVS_CLK_CFG;
      
      AVBBus::AVS_THROTTLE_CFG_type<32> AVS_THROTTLE_CFG;
      
      AVBBus::AVS_CONFIG_type<32> AVS_CONFIG;
      
      void reset_all_registers();
};
