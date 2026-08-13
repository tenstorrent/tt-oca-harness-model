
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class local_alias_remap_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<local_alias_remap_basetest, 32> initiator_socket;
    enum Register_offset
    {
      REGION_START_OFFSET = (0x0 + 0x00), 
      REGION_END_OFFSET = (0x8 + 0x00), 
      REGION_ATTRS_OFFSET = (0x10 + 0x00)  
    };

    enum Register_Read_Access
    {
      REGION_START_READ = (0xfffffffffff000), 
      REGION_END_READ = (0xfffffffffff000), 
      REGION_ATTRS_READ = (0xc0fffffffffff000)  
    };

    enum Register_Write_Access
    {
      REGION_START_WRITE = (0xfffffffffff000), 
      REGION_END_WRITE = (0xfffffffffff000), 
      REGION_ATTRS_WRITE = (0xc0fffffffffff000)
    };

    enum Register_Reset_Val
    {
      REGION_START_RESET = (0x0), 
      REGION_END_RESET = (0x0), 
      REGION_ATTRS_RESET = (0x0)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  uint64_t read_mask;
		  uint64_t write_mask;
		  uint64_t reg_reset;
		  std::string reg_name;
    };

    local_alias_remap_basetest(sc_module_name name) : sc_module(name)
    {

    }

};