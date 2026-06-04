
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class hmac_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<hmac_basetest, 32> initiator_socket;
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xc + 0x00), 
      CFG_OFFSET = (0x10 + 0x00), 
      CMD_OFFSET = (0x14 + 0x00), 
      STATUS_OFFSET = (0x18 + 0x00), 
      ERR_CODE_OFFSET = (0x1c + 0x00), 
      WIPE_SECRET_OFFSET = (0x20 + 0x00), 
      KEY_OFFSET = (0x24 + 0x00),
      KEY_SPACING = 4, 
      DIGEST_OFFSET = (0xa4 + 0x00),
      DIGEST_SPACING = 4, 
      MSG_LENGTH_LOWER_OFFSET = (0xe4 + 0x00), 
      MSG_LENGTH_UPPER_OFFSET = (0xe8 + 0x00), 
      MSG_FIFO_OFFSET = (0x1000 + 0x00),
      MSG_FIFO_SPACING = 4
    };

    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x7), 
      INTR_ENABLE_READ = (0x7), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      CFG_READ = (0x7fff), 
      CMD_READ = (0x0), 
      STATUS_READ = (0x3f7), 
      ERR_CODE_READ = (0xffffffff), 
      WIPE_SECRET_READ = (0x0), 
      KEY_READ = (0x0), 
      DIGEST_READ = (0xffffffff), 
      MSG_LENGTH_LOWER_READ = (0xffffffff), 
      MSG_LENGTH_UPPER_READ = (0xffffffff), 
      MSG_FIFO_READ = (0x0)
    };

    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x5), 
      INTR_ENABLE_WRITE = (0x7), 
      INTR_TEST_WRITE = (0x7), 
      ALERT_TEST_WRITE = (0x1), 
      CFG_WRITE = (0x7fff), 
      CMD_WRITE = (0xf), 
      STATUS_WRITE = (0x0), 
      ERR_CODE_WRITE = (0x0), 
      WIPE_SECRET_WRITE = (0xffffffff), 
      KEY_WRITE = (0xffffffff), 
      DIGEST_WRITE = (0xffffffff), 
      MSG_LENGTH_LOWER_WRITE = (0xffffffff), 
      MSG_LENGTH_UPPER_WRITE = (0xffffffff), 
      MSG_FIFO_WRITE = (0xffffffff)
    };

    enum Register_Reset_Val
    {
      INTR_STATE_RESET = (0), 
      INTR_ENABLE_RESET = (0), 
      INTR_TEST_RESET = (0), 
      ALERT_TEST_RESET = (0), 
      CFG_RESET = (16640), 
      CMD_RESET = (0), 
      STATUS_RESET = (3), 
      ERR_CODE_RESET = (0), 
      WIPE_SECRET_RESET = (0), 
      KEY_RESET = (0), 
      DIGEST_RESET = (0), 
      MSG_LENGTH_LOWER_RESET = (0), 
      MSG_LENGTH_UPPER_RESET = (0), 
      MSG_FIFO_RESET = (0)
    };

    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    hmac_basetest(sc_module_name name) : sc_module(name)
    {

    }

};