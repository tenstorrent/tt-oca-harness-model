
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class sep_cpu_ctrl_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<sep_cpu_ctrl_basetest, 32> initiator_socket;
    enum Register_offset
    {
      CLOCK_GATE_CTRL_OFFSET = (0x8 + 0x00), 
      REFERENCE_COUNTER_OFFSET = (0x10 + 0x00), 
      TIMEOUT_INTERRUPT_OFFSET = (0x18 + 0x00), 
      PKA_CTRL_OFFSET = (0x20 + 0x00),
      TIMEOUT_COUNT_DMA_OFFSET = (0x28 + 0x00),
      TIMEOUT_COUNT_SYS_IN_OFFSET = (0x30 + 0x00),
      TIMEOUT_COUNT_MAILBOX_INBOUND_OFFSET = (0x38 + 0x00),
      TIMEOUT_COUNT_MAILBOX_OUTBOUND_OFFSET = (0x40 + 0x00),
      TIMEOUT_COUNT_ENTROPY_WRITE_OFFSET = (0x48 + 0x00),
      TIMEOUT_COUNT_ENTROPY_READ_OFFSET = (0x50 + 0x00),
      TIMEOUT_COUNT_FILTER_OUT_OFFSET = (0x58 + 0x00),
      TIMEOUT_COUNT_ALIAS_REMAP_OFFSET = (0x60 + 0x00),
      TIMEOUT_ENABLE_OFFSET = (0x68 + 0x00),
      TIMEOUT_CLEAR_OFFSET = (0x70 + 0x00),
      TIMEOUT_MODE_OFFSET = (0x78 + 0x00),
      SEP_TEST_CTRL_OFFSET = (0xB0 + 0x00),
      SEP_GLOBAL_BASE_ADDR_OFFSET = (0xC0 + 0x00), 
      SEP_LOCAL_BASE_ADDR_OFFSET = (0xC8 + 0x00), 
      SEP_REGION_SIZE_OFFSET = (0xD0 + 0x00), 
      SMU_GLOBAL_BASE_ADDR_OFFSET = (0x100 + 0x00), 
      SMU_REGION_SIZE_OFFSET = (0x110 + 0x00), 
      SMC_FUSE_SENSE_STATUS_OFFSET = (0x140 + 0x00), 
      SEP_FUSE_SENSE_STATUS_OFFSET = (0x150 + 0x00), 
      SEP_STRAPS_OFFSET = (0x160 + 0x00), 
      RAS_BANK_INFO_OFFSET = (0x170 + 0x00), 
      SEP_SW_DEBUG_OFFSET = (0x178 + 0x00), 
      SEP_NMI_VEC_OFFSET = (0x180 + 0x00), 
      SEP_NMI_VEC_LOCK_OFFSET = (0x188 + 0x00), 
      EXT_TRNG_SRC_SEL_OFFSET = (0x190 + 0x00), 
      EXT_TRNG_SRC_SEL_LOCK_OFFSET = (0x198 + 0x00),
      KM_WIPE_CTRL_OFFSET = (0x1A0 + 0x00),
      SEP_VERSION_ID_OFFSET = (0x1000 + 0x00)
    };

    enum Register_Read_Access
    {
      CLOCK_GATE_CTRL_READ = (0x3F07FF),
      REFERENCE_COUNTER_READ = (0xffffffffffffffff),
      TIMEOUT_INTERRUPT_READ = (0xff),
      PKA_CTRL_READ = (0x7),
      TIMEOUT_COUNT_DMA_READ = (0xffffffffffff),
      TIMEOUT_COUNT_SYS_IN_READ = (0xffffffffffff),
      TIMEOUT_COUNT_MAILBOX_INBOUND_READ = (0xffffffffffff),
      TIMEOUT_COUNT_MAILBOX_OUTBOUND_READ = (0xffffffffffff),
      TIMEOUT_COUNT_ENTROPY_WRITE_READ = (0xffffffffffff),
      TIMEOUT_COUNT_ENTROPY_READ_READ = (0xffffffffffff),
      TIMEOUT_COUNT_FILTER_OUT_READ = (0xffffffffffff),
      TIMEOUT_COUNT_ALIAS_REMAP_READ = (0xffffffffffff),
      TIMEOUT_ENABLE_READ = (0xff),
      TIMEOUT_CLEAR_READ = (0x0),
      TIMEOUT_MODE_READ = (0x0),
      SEP_TEST_CTRL_READ = (0x0),
      SEP_GLOBAL_BASE_ADDR_READ = (0xffffffffffffff), 
      SEP_LOCAL_BASE_ADDR_READ = (0xffffffffffffff), 
      SEP_REGION_SIZE_READ = (0xffffffff), 
      SMU_GLOBAL_BASE_ADDR_READ = (0xffffffffffffff), 
      SMU_REGION_SIZE_READ = (0xffffffff), 
      SMC_FUSE_SENSE_STATUS_READ = (0x0), 
      SEP_FUSE_SENSE_STATUS_READ = (0x0), 
      SEP_STRAPS_READ = (0x0), 
      RAS_BANK_INFO_READ = (0xff), 
      SEP_SW_DEBUG_READ = (0xffffffff), 
      SEP_NMI_VEC_READ = (0xfffffffe), 
      SEP_NMI_VEC_LOCK_READ = (0x1), 
      EXT_TRNG_SRC_SEL_READ = (0x7), 
      EXT_TRNG_SRC_SEL_LOCK_READ = (0x1),
      KM_WIPE_CTRL_READ = (0x1),
      SEP_VERSION_ID_READ = (0x0)
    };

    enum Register_Write_Access
    {
      CLOCK_GATE_CTRL_WRITE = (0x3F07FF),
      REFERENCE_COUNTER_WRITE = (0xffffffffffffffff),
      TIMEOUT_INTERRUPT_WRITE = (0x0),
      PKA_CTRL_WRITE = (0x7),
      TIMEOUT_COUNT_DMA_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_SYS_IN_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_MAILBOX_INBOUND_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_MAILBOX_OUTBOUND_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_ENTROPY_WRITE_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_ENTROPY_READ_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_FILTER_OUT_WRITE = (0xffffffffffff),
      TIMEOUT_COUNT_ALIAS_REMAP_WRITE = (0xffffffffffff),
      TIMEOUT_ENABLE_WRITE = (0xff),
      TIMEOUT_CLEAR_WRITE = (0xff),
      TIMEOUT_MODE_WRITE = (0xffff),
      SEP_TEST_CTRL_WRITE = (0x0),
      SEP_GLOBAL_BASE_ADDR_WRITE = (0xffffffffffffff), 
      SEP_LOCAL_BASE_ADDR_WRITE = (0xffffffffffffff), 
      SEP_REGION_SIZE_WRITE = (0xffffffff), 
      SMU_GLOBAL_BASE_ADDR_WRITE = (0xffffffffffffff), 
      SMU_REGION_SIZE_WRITE = (0xffffffff), 
      SMC_FUSE_SENSE_STATUS_WRITE = (0x0), 
      SEP_FUSE_SENSE_STATUS_WRITE = (0x0), 
      SEP_STRAPS_WRITE = (0x0), 
      RAS_BANK_INFO_WRITE = (0xff), 
      SEP_SW_DEBUG_WRITE = (0xffffffff), 
      SEP_NMI_VEC_WRITE = (0xfffffffe), 
      SEP_NMI_VEC_LOCK_WRITE = (0x1), 
      EXT_TRNG_SRC_SEL_WRITE = (0x7), 
      EXT_TRNG_SRC_SEL_LOCK_WRITE = (0x1),
      KM_WIPE_CTRL_WRITE = (0x1),
      SEP_VERSION_ID_WRITE = (0x0)
    };

    enum Register_Reset_Val
    {
      CLOCK_GATE_CTRL_RESET = (0x1F0021),
      REFERENCE_COUNTER_RESET = (0),
      TIMEOUT_INTERRUPT_RESET = (0),
      PKA_CTRL_RESET = (0x0),
      TIMEOUT_COUNT_DMA_RESET = (0),
      TIMEOUT_COUNT_SYS_IN_RESET = (0),
      TIMEOUT_COUNT_MAILBOX_INBOUND_RESET = (0), 
      TIMEOUT_COUNT_MAILBOX_OUTBOUND_RESET = (0), 
      TIMEOUT_COUNT_ENTROPY_WRITE_RESET = (0), 
      TIMEOUT_COUNT_ENTROPY_READ_RESET = (0), 
      TIMEOUT_COUNT_FILTER_OUT_RESET = (0), 
      TIMEOUT_COUNT_ALIAS_REMAP_RESET = (0), 
      TIMEOUT_ENABLE_RESET = (0), 
      TIMEOUT_CLEAR_RESET = (0), 
      TIMEOUT_MODE_RESET = (0), 
      SEP_TEST_CTRL_RESET = (0), 
      SEP_GLOBAL_BASE_ADDR_RESET = (0), 
      SEP_LOCAL_BASE_ADDR_RESET = (0xD0000000), 
      SEP_REGION_SIZE_RESET = (0x01000000), 
      SMU_GLOBAL_BASE_ADDR_RESET = (0x80000000), 
      SMU_REGION_SIZE_RESET = (0x40000000), 
      SMC_FUSE_SENSE_STATUS_RESET = (0x0), 
      SEP_FUSE_SENSE_STATUS_RESET = (0x0), 
      SEP_STRAPS_RESET = (0x0), 
      RAS_BANK_INFO_RESET = (0x0), 
      SEP_SW_DEBUG_RESET = (0x0), 
      SEP_NMI_VEC_RESET = (0x0000000060000080), 
      SEP_NMI_VEC_LOCK_RESET = (0x0), 
      EXT_TRNG_SRC_SEL_RESET = (0x7), 
      EXT_TRNG_SRC_SEL_LOCK_RESET = (0x0),
      KM_WIPE_CTRL_RESET = (0x0),
      SEP_VERSION_ID_RESET = (0xdeadbeef)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  uint64_t read_mask;
		  uint64_t write_mask;
		  uint64_t reg_reset;
		  std::string reg_name;
    };

    sep_cpu_ctrl_basetest(sc_module_name name) : sc_module(name)
    {

    }

};