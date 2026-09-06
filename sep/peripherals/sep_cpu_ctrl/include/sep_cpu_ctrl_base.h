// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_cpu_ctrl_register.h"
#include <string.h>

class sep_cpu_ctrl_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<64>::DT DT;
    sep_cpu_ctrl_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned long long)),
       CLOCK_GATE_CTRL(std::string(name) + ".CLOCK_GATE_CTRL", memory, (0x8 + 0x00)/sizeof(unsigned long long)), 
       REFERENCE_COUNTER(std::string(name) + ".REFERENCE_COUNTER", memory, (0x10 + 0x00)/sizeof(unsigned long long)), 
       TIMEOUT_INTERRUPT(std::string(name) + ".TIMEOUT_INTERRUPT", memory, (0x18 + 0x00)/sizeof(unsigned long long)), 
       PKA_CTRL(std::string(name) + ".PKA_CTRL", memory, (0x20 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_DMA(std::string(name) + ".TIMEOUT_COUNT_DMA", memory, (0x28 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_SYS_IN(std::string(name) + ".TIMEOUT_COUNT_SYS_IN", memory, (0x30 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_MAILBOX_INBOUND(std::string(name) + ".TIMEOUT_COUNT_MAILBOX_INBOUND", memory, (0x38 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_MAILBOX_OUTBOUND(std::string(name) + ".TIMEOUT_COUNT_MAILBOX_OUTBOUND", memory, (0x40 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_ENTROPY_WRITE(std::string(name) + ".TIMEOUT_COUNT_ENTROPY_WRITE", memory, (0x48 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_ENTROPY_READ(std::string(name) + ".TIMEOUT_COUNT_ENTROPY_READ", memory, (0x50 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_FILTER_OUT(std::string(name) + ".TIMEOUT_COUNT_FILTER_OUT", memory, (0x58 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_COUNT_ALIAS_REMAP(std::string(name) + ".TIMEOUT_COUNT_ALIAS_REMAP", memory, (0x60 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_ENABLE(std::string(name) + ".TIMEOUT_ENABLE", memory, (0x68 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_CLEAR(std::string(name) + ".TIMEOUT_CLEAR", memory, (0x70 + 0x00)/sizeof(unsigned long long)),
       TIMEOUT_MODE(std::string(name) + ".TIMEOUT_MODE", memory, (0x78 + 0x00)/sizeof(unsigned long long)),
       SEP_TEST_CTRL(std::string(name) + ".SEP_TEST_CTRL", memory, (0xB0 + 0x00)/sizeof(unsigned long long)),
       SEP_GLOBAL_BASE_ADDR(std::string(name) + ".SEP_GLOBAL_BASE_ADDR", memory, (0xC0 + 0x00)/sizeof(unsigned long long)), 
       SEP_LOCAL_BASE_ADDR(std::string(name) + ".SEP_LOCAL_BASE_ADDR", memory, (0xC8 + 0x00)/sizeof(unsigned long long)), 
       SEP_REGION_SIZE(std::string(name) + ".SEP_REGION_SIZE", memory, (0xD0 + 0x00)/sizeof(unsigned long long)), 
       SMU_GLOBAL_BASE_ADDR(std::string(name) + ".SMU_GLOBAL_BASE_ADDR", memory, (0x100 + 0x00)/sizeof(unsigned long long)), 
       SMU_REGION_SIZE(std::string(name) + ".SMU_REGION_SIZE", memory, (0x110 + 0x00)/sizeof(unsigned long long)), 
       SMC_FUSE_SENSE_STATUS(std::string(name) + ".SMC_FUSE_SENSE_STATUS", memory, (0x140 + 0x00)/sizeof(unsigned long long)), 
       SEP_FUSE_SENSE_STATUS(std::string(name) + ".SEP_FUSE_SENSE_STATUS", memory, (0x150 + 0x00)/sizeof(unsigned long long)), 
       SEP_STRAPS(std::string(name) + ".SEP_STRAPS", memory, (0x160 + 0x00)/sizeof(unsigned long long)), 
       RAS_BANK_INFO(std::string(name) + ".RAS_BANK_INFO", memory, (0x170 + 0x00)/sizeof(unsigned long long)), 
       SEP_SW_DEBUG(std::string(name) + ".SEP_SW_DEBUG", memory, (0x178 + 0x00)/sizeof(unsigned long long)), 
       SEP_NMI_VEC(std::string(name) + ".SEP_NMI_VEC", memory, (0x180 + 0x00)/sizeof(unsigned long long)), 
       SEP_NMI_VEC_LOCK(std::string(name) + ".SEP_NMI_VEC_LOCK", memory, (0x188 + 0x00)/sizeof(unsigned long long)), 
       EXT_TRNG_SRC_SEL(std::string(name) + ".EXT_TRNG_SRC_SEL", memory, (0x190 + 0x00)/sizeof(unsigned long long)), 
       EXT_TRNG_SRC_SEL_LOCK(std::string(name) + ".EXT_TRNG_SRC_SEL_LOCK", memory, (0x198 + 0x00)/sizeof(unsigned long long)),
       KM_WIPE_CTRL(std::string(name) + ".KM_WIPE_CTRL", memory, (0x1A0 + 0x00)/sizeof(unsigned long long)),
       SEP_VERSION_ID(std::string(name) + ".SEP_VERSION_ID", memory, (0x1000 + 0x00)/sizeof(unsigned long long))
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;
      regmodel::Memory<64> memory;
      tlm_utils::simple_target_socket<regmodel::Memory<64>, 32> target_socket;

      
      sep_cpu_ctrl::CLOCK_GATE_CTRL_type<64> CLOCK_GATE_CTRL;
      
      sep_cpu_ctrl::REFERENCE_COUNTER_type<64> REFERENCE_COUNTER;
      
      sep_cpu_ctrl::TIMEOUT_INTERRUPT_type<64> TIMEOUT_INTERRUPT;
      
      sep_cpu_ctrl::PKA_CTRL_type<64> PKA_CTRL;

      sep_cpu_ctrl::TIMEOUT_COUNT_DMA_type<64> TIMEOUT_COUNT_DMA;

      sep_cpu_ctrl::TIMEOUT_COUNT_SYS_IN_type<64> TIMEOUT_COUNT_SYS_IN;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_MAILBOX_INBOUND_type<64> TIMEOUT_COUNT_MAILBOX_INBOUND;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_MAILBOX_OUTBOUND_type<64> TIMEOUT_COUNT_MAILBOX_OUTBOUND;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_ENTROPY_WRITE_type<64> TIMEOUT_COUNT_ENTROPY_WRITE;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_ENTROPY_READ_type<64> TIMEOUT_COUNT_ENTROPY_READ;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_FILTER_OUT_type<64> TIMEOUT_COUNT_FILTER_OUT;
      
      sep_cpu_ctrl::TIMEOUT_COUNT_ALIAS_REMAP_type<64> TIMEOUT_COUNT_ALIAS_REMAP;
      
      sep_cpu_ctrl::TIMEOUT_ENABLE_type<64> TIMEOUT_ENABLE;
      
      sep_cpu_ctrl::TIMEOUT_CLEAR_type<64> TIMEOUT_CLEAR;
      
      sep_cpu_ctrl::TIMEOUT_MODE_type<64> TIMEOUT_MODE;
      
      sep_cpu_ctrl::SEP_TEST_CTRL_type<64> SEP_TEST_CTRL;
      
      sep_cpu_ctrl::SEP_GLOBAL_BASE_ADDR_type<64> SEP_GLOBAL_BASE_ADDR;
      
      sep_cpu_ctrl::SEP_LOCAL_BASE_ADDR_type<64> SEP_LOCAL_BASE_ADDR;
      
      sep_cpu_ctrl::SEP_REGION_SIZE_type<64> SEP_REGION_SIZE;
      
      sep_cpu_ctrl::SMU_GLOBAL_BASE_ADDR_type<64> SMU_GLOBAL_BASE_ADDR;
      
      sep_cpu_ctrl::SMU_REGION_SIZE_type<64> SMU_REGION_SIZE;
      
      sep_cpu_ctrl::SMC_FUSE_SENSE_STATUS_type<64> SMC_FUSE_SENSE_STATUS;
      
      sep_cpu_ctrl::SEP_FUSE_SENSE_STATUS_type<64> SEP_FUSE_SENSE_STATUS;
      
      sep_cpu_ctrl::SEP_STRAPS_type<64> SEP_STRAPS;
      
      sep_cpu_ctrl::RAS_BANK_INFO_type<64> RAS_BANK_INFO;
      
      sep_cpu_ctrl::SEP_SW_DEBUG_type<64> SEP_SW_DEBUG;
      
      sep_cpu_ctrl::SEP_NMI_VEC_type<64> SEP_NMI_VEC;
      
      sep_cpu_ctrl::SEP_NMI_VEC_LOCK_type<64> SEP_NMI_VEC_LOCK;
      
      sep_cpu_ctrl::EXT_TRNG_SRC_SEL_type<64> EXT_TRNG_SRC_SEL;
      
      sep_cpu_ctrl::EXT_TRNG_SRC_SEL_LOCK_type<64> EXT_TRNG_SRC_SEL_LOCK;

      sep_cpu_ctrl::KM_WIPE_CTRL_type<64> KM_WIPE_CTRL;

      sep_cpu_ctrl::SEP_VERSION_ID_type<64> SEP_VERSION_ID;
      
      void reset_all_registers();
};
