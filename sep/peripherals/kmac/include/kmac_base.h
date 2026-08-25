// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "kmac_register.h"
#include <string.h>

class kmac_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    kmac_base(sc_module_name name, unsigned int memory_size = 0x1000) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xC + 0x00)/sizeof(unsigned int)), 
       CFG_REGWEN(std::string(name) + ".CFG_REGWEN", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       CFG_SHADOWED(std::string(name) + ".CFG_SHADOWED", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       CMD(std::string(name) + ".CMD", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       STATUS(std::string(name) + ".STATUS", memory, (0x1C + 0x00)/sizeof(unsigned int)), 
       ENTROPY_PERIOD(std::string(name) + ".ENTROPY_PERIOD", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       ENTROPY_REFRESH_HASH_CNT(std::string(name) + ".ENTROPY_REFRESH_HASH_CNT", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       ENTROPY_REFRESH_THRESHOLD_SHADOWED(std::string(name) + ".ENTROPY_REFRESH_THRESHOLD_SHADOWED", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       ENTROPY_SEED(std::string(name) + ".ENTROPY_SEED", memory, (0x2C + 0x00)/sizeof(unsigned int)), 
       KEY_SHARE0(std::string(name) + ".KEY_SHARE0", memory, (0x30 + 0x00)/sizeof(unsigned int), 1), 
       KEY_SHARE1(std::string(name) + ".KEY_SHARE1", memory, (0x70 + 0x00)/sizeof(unsigned int), 1), 
       KEY_LEN(std::string(name) + ".KEY_LEN", memory, (0xB0 + 0x00)/sizeof(unsigned int)), 
       PREFIX(std::string(name) + ".PREFIX", memory, (0xB4 + 0x00)/sizeof(unsigned int), 1), 
       ERR_CODE(std::string(name) + ".ERR_CODE", memory, (0xE0 + 0x00)/sizeof(unsigned int)),
       STATE(std::string(name) + ".STATE", memory, (0x400 + 0x00)/sizeof(unsigned int), 1),
       MSG_FIFO(std::string(name) + ".MSG_FIFO", memory, (0x800 + 0x00)/sizeof(unsigned int), 1)
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      kmac::INTR_STATE_type<32> INTR_STATE;
      
      kmac::INTR_ENABLE_type<32> INTR_ENABLE;
      
      kmac::INTR_TEST_type<32> INTR_TEST;
      
      kmac::ALERT_TEST_type<32> ALERT_TEST;
      
      kmac::CFG_REGWEN_type<32> CFG_REGWEN;
      
      kmac::CFG_SHADOWED_type<32> CFG_SHADOWED;
      
      kmac::CMD_type<32> CMD;
      
      kmac::STATUS_type<32> STATUS;
      
      kmac::ENTROPY_PERIOD_type<32> ENTROPY_PERIOD;
      
      kmac::ENTROPY_REFRESH_HASH_CNT_type<32> ENTROPY_REFRESH_HASH_CNT;
      
      kmac::ENTROPY_REFRESH_THRESHOLD_SHADOWED_type<32> ENTROPY_REFRESH_THRESHOLD_SHADOWED;
      
      kmac::ENTROPY_SEED_type<32> ENTROPY_SEED;
      
      csml_reg_vector<kmac::KEY_SHARE0_type<32>, 16> KEY_SHARE0;

      csml_reg_vector<kmac::KEY_SHARE1_type<32>, 16> KEY_SHARE1;
      
      kmac::KEY_LEN_type<32> KEY_LEN;
      
      csml_reg_vector<kmac::PREFIX_type<32>, 11> PREFIX;
      
      kmac::ERR_CODE_type<32> ERR_CODE;

      csml_reg_vector<kmac::STATE_type<32>, 128> STATE;

      csml_reg_vector<kmac::MSG_FIFO_type<32>, 512> MSG_FIFO;
      
      void reset_all_registers();
};
