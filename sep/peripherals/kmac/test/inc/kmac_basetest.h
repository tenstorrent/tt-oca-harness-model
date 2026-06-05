
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class kmac_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<kmac_basetest, 32> initiator_socket;
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xC + 0x00), 
      CFG_REGWEN_OFFSET = (0x10 + 0x00), 
      CFG_SHADOWED_OFFSET = (0x14 + 0x00), 
      CMD_OFFSET = (0x18 + 0x00), 
      STATUS_OFFSET = (0x1C + 0x00), 
      ENTROPY_PERIOD_OFFSET = (0x20 + 0x00), 
      ENTROPY_REFRESH_HASH_CNT_OFFSET = (0x24 + 0x00), 
      ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET = (0x28 + 0x00), 
      ENTROPY_SEED_OFFSET = (0x2C + 0x00), 
      KEY_SHARE0_OFFSET = (0x30 + 0x00), 
      KEY_SHARE1_OFFSET = (0x70 + 0x00),
      KEY_LEN_OFFSET = (0xB0 + 0x00),
      PREFIX_0_OFFSET = (0xB4 + 0x00),
      PREFIX_1_OFFSET = (0xB4 + 0x04),
      PREFIX_2_OFFSET = (0xB4 + 0x08),
      PREFIX_3_OFFSET = (0xB4 + 0x0C),
      PREFIX_4_OFFSET = (0xB4 + 0x10),
      PREFIX_5_OFFSET = (0xB4 + 0x14),
      PREFIX_6_OFFSET = (0xB4 + 0x18),
      PREFIX_7_OFFSET = (0xB4 + 0x1C),
      PREFIX_8_OFFSET = (0xB4 + 0x20),
      PREFIX_9_OFFSET = (0xB4 + 0x24),
      PREFIX_10_OFFSET = (0xB4 + 0x28),
      PREFIX_OFFSET = (0xB4 + 0x00),  // Alias for PREFIX_0
      ERR_CODE_OFFSET = (0xE0 + 0x00),
      STATE_OFFSET = (0x400 + 0x00),    // Keccak State Memory Window (128 words)
      MSG_FIFO_OFFSET = (0x800 + 0x00)  // Message FIFO Memory Window (512 words)
    };

    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x7), 
      INTR_ENABLE_READ = (0x7), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      CFG_REGWEN_READ = (0x1), 
      CFG_SHADOWED_READ = (0x51b133f), 
      CMD_READ = (0x0), 
      STATUS_READ = (0x3df07), 
      ENTROPY_PERIOD_READ = (0xffff03ff), 
      ENTROPY_REFRESH_HASH_CNT_READ = (0x3ff), 
      ENTROPY_REFRESH_THRESHOLD_SHADOWED_READ = (0x3ff), 
      ENTROPY_SEED_READ = (0x0), 
      KEY_SHARE0_READ = (0x0), 
      KEY_SHARE1_READ = (0x0), 
      KEY_LEN_READ = (0x0), 
      PREFIX_READ = (0xffffffff), 
      ERR_CODE_READ = (0xffffffff),
      STATE_READ = (0xffffffff),      // RO - Read-Only
      MSG_FIFO_READ = (0x0)           // WO - Write-Only (no read access)
    };

    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x5), 
      INTR_ENABLE_WRITE = (0x7), 
      INTR_TEST_WRITE = (0x7), 
      ALERT_TEST_WRITE = (0x3), 
      CFG_REGWEN_WRITE = (0x0), 
      CFG_SHADOWED_WRITE = (0x51b133f), 
      CMD_WRITE = (0x73f), 
      STATUS_WRITE = (0x0), 
      ENTROPY_PERIOD_WRITE = (0xffff03ff), 
      ENTROPY_REFRESH_HASH_CNT_WRITE = (0x0), 
      ENTROPY_REFRESH_THRESHOLD_SHADOWED_WRITE = (0x3ff), 
      ENTROPY_SEED_WRITE = (0xffffffff), 
      KEY_SHARE0_WRITE = (0xffffffff), 
      KEY_SHARE1_WRITE = (0xffffffff), 
      KEY_LEN_WRITE = (0x7), 
      PREFIX_WRITE = (0xffffffff), 
      ERR_CODE_WRITE = (0x0),
      STATE_WRITE = (0x0),            // RO - Read-Only (no write access)
      MSG_FIFO_WRITE = (0xffffffff)   // WO - Write-Only
    };

    enum Register_Reset_Val
    {
      INTR_STATE_RESET = 0x00000000,
      INTR_ENABLE_RESET = 0x00000000,
      INTR_TEST_RESET = 0x00000000,
      ALERT_TEST_RESET = 0x00000000,
      CFG_REGWEN_RESET = 0x00000001,
      CFG_SHADOWED_RESET = 0x00001000,  // sideload[12]=1 (default per RDL)
      CMD_RESET = 0x00000000,
      STATUS_RESET = 0x00004001,
      ENTROPY_PERIOD_RESET = 0x00000000,
      ENTROPY_REFRESH_HASH_CNT_RESET = 0x00000000,
      ENTROPY_REFRESH_THRESHOLD_SHADOWED_RESET = 0x00000000,
      ENTROPY_SEED_RESET = 0x00000000,
      KEY_SHARE0_RESET = 0x00000000,
      KEY_SHARE1_RESET = 0x00000000,
      KEY_LEN_RESET = 0x00000000,
      PREFIX_RESET = 0x00000000,
      ERR_CODE_RESET = 0x00000000,
      STATE_RESET = 0x00000000,
      MSG_FIFO_RESET = 0x00000000
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    kmac_basetest(sc_module_name name) : sc_module(name)
    {

    }

};