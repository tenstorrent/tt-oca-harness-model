
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class AVBBus_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<AVBBus_basetest, 32> initiator_socket;
    enum Register_offset
    {
      AVS_FSM_RESET_OFFSET = (0x0 + 0x00), 
      AVS_CTRL_OFFSET = (0x4 + 0x00), 
      AVS_CMD_OFFSET = (0x8 + 0x00), 
      AVS_READBACK_OFFSET = (0xc + 0x00), 
      AVS_TARGET_ACKS_OFFSET = (0x48 + 0x00), 
      AVS_LATEST_TARGET_SUBFRAME_OFFSET = (0x4c + 0x00), 
      AVS_LATEST_TARGET_ACKS_OFFSET = (0x88 + 0x00), 
      AVS_NORMAL_STATUS_OFFSET = (0x8c + 0x00), 
      AVS_CONTROLLER_STATUS_OFFSET = (0x90 + 0x00), 
      AVS_TOTAL_RETRIES_OFFSET = (0x94 + 0x00), 
      AVS_FIFOS_STATUS_OFFSET = (0x98 + 0x00), 
      AVS_INTERRUPT_OFFSET = (0x9c + 0x00), 
      AVS_INTERRUPT_MASK_OFFSET = (0xa0 + 0x00), 
      AVS_INTERRUPT_TEST_OFFSET = (0xa4 + 0x00), 
      AVS_TARGET_ISSUED_INTERRUPT_IDS_OFFSET = (0xa8 + 0x00), 
      AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_OFFSET = (0xac + 0x00), 
      AVS_TARGET_BAD_CRC_INT_IDS_OFFSET = (0xb0 + 0x00), 
      AVS_RETRY_CFG_OFFSET = (0xc0 + 0x00), 
      AVS_CLK_CFG_OFFSET = (0xc4 + 0x00), 
      AVS_THROTTLE_CFG_OFFSET = (0xc8 + 0x00), 
      AVS_CONFIG_OFFSET = (0xcc + 0x00)  
    };

    enum Register_Read_Access
    {
      AVS_FSM_RESET_READ = (0x1), 
      AVS_CTRL_READ = (0x0), 
      AVS_CMD_READ = (0x0), 
      AVS_READBACK_READ = (0x31fffff), 
      AVS_TARGET_ACKS_READ = (0x3fffffff), 
      AVS_LATEST_TARGET_SUBFRAME_READ = (0x31fffff), 
      AVS_LATEST_TARGET_ACKS_READ = (0x3fffffff), 
      AVS_NORMAL_STATUS_READ = (0x11111), 
      AVS_CONTROLLER_STATUS_READ = (0x11), 
      AVS_TOTAL_RETRIES_READ = (0xffffffff), 
      AVS_FIFOS_STATUS_READ = (0xf0f0f0f), 
      AVS_INTERRUPT_READ = (0xff), 
      AVS_INTERRUPT_MASK_READ = (0xff), 
      AVS_INTERRUPT_TEST_READ = (0xff), 
      AVS_TARGET_ISSUED_INTERRUPT_IDS_READ = (0x7fff), 
      AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_READ = (0x7fff), 
      AVS_TARGET_BAD_CRC_INT_IDS_READ = (0x7fff), 
      AVS_RETRY_CFG_READ = (0xffff), 
      AVS_CLK_CFG_READ = (0xffff1103), 
      AVS_THROTTLE_CFG_READ = (0xff), 
      AVS_CONFIG_READ = (0x1)  
    };

    enum Register_Write_Access
    {
      AVS_FSM_RESET_WRITE = (0x1), 
      AVS_CTRL_WRITE = (0x111), 
      AVS_CMD_WRITE = (0xfdffffff), 
      AVS_READBACK_WRITE = (0x0), 
      AVS_TARGET_ACKS_WRITE = (0x0), 
      AVS_LATEST_TARGET_SUBFRAME_WRITE = (0x0), 
      AVS_LATEST_TARGET_ACKS_WRITE = (0x0), 
      AVS_NORMAL_STATUS_WRITE = (0x0), 
      AVS_CONTROLLER_STATUS_WRITE = (0x0), 
      AVS_TOTAL_RETRIES_WRITE = (0x0), 
      AVS_FIFOS_STATUS_WRITE = (0x0), 
      AVS_INTERRUPT_WRITE = (0xff), 
      AVS_INTERRUPT_MASK_WRITE = (0xff), 
      AVS_INTERRUPT_TEST_WRITE = (0xff), 
      AVS_TARGET_ISSUED_INTERRUPT_IDS_WRITE = (0x0), 
      AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_WRITE = (0x0), 
      AVS_TARGET_BAD_CRC_INT_IDS_WRITE = (0x0), 
      AVS_RETRY_CFG_WRITE = (0xffff), 
      AVS_CLK_CFG_WRITE = (0xffff1103), 
      AVS_THROTTLE_CFG_WRITE = (0xff), 
      AVS_CONFIG_WRITE = (0x1)
    };

    enum Register_Reset_Val
    {
      AVS_FSM_RESET_RESET = (0), 
      AVS_CTRL_RESET = (0), 
      AVS_CMD_RESET = (0), 
      AVS_READBACK_RESET = (0), 
      AVS_TARGET_ACKS_RESET = (0), 
      AVS_LATEST_TARGET_SUBFRAME_RESET = (0), 
      AVS_LATEST_TARGET_ACKS_RESET = (0), 
      AVS_NORMAL_STATUS_RESET = (0), 
      AVS_CONTROLLER_STATUS_RESET = (0), 
      AVS_TOTAL_RETRIES_RESET = (0), 
      AVS_FIFOS_STATUS_RESET = (0), 
      AVS_INTERRUPT_RESET = (0), 
      AVS_INTERRUPT_MASK_RESET = (0), 
      AVS_INTERRUPT_TEST_RESET = (0), 
      AVS_TARGET_ISSUED_INTERRUPT_IDS_RESET = (0), 
      AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_RESET = (0), 
      AVS_TARGET_BAD_CRC_INT_IDS_RESET = (0), 
      AVS_RETRY_CFG_RESET = (0), 
      AVS_CLK_CFG_RESET = (2147745795), 
      AVS_THROTTLE_CFG_RESET = (0), 
      AVS_CONFIG_RESET = (1)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    AVBBus_basetest(sc_module_name name) : sc_module(name)
    {

    }

};