#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class lifecycle_ctrl_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<lifecycle_ctrl_basetest, 32> initiator_socket;

    enum Register_offset
    {
        FEAT_CTRL_LO_OFFSET = 0x0000,
        FEAT_CTRL_HI_OFFSET = 0x0004,
        DEMOTE_1_OFFSET     = 0x0008,
        DEMOTE_1_HI_OFFSET  = 0x000C,
        DEMOTE_2_OFFSET     = 0x0010,
        DEMOTE_2_HI_OFFSET  = 0x0014
    };

    enum Register_Read_Access
    {
        FEAT_CTRL_LO_READ = 0xffffffff,
        FEAT_CTRL_HI_READ = 0xffffffff,
        DEMOTE_1_READ     = 0xffffffff,
        DEMOTE_2_READ     = 0xffffffff
    };

    enum Register_Write_Access
    {
        FEAT_CTRL_LO_WRITE = 0x00000000,
        FEAT_CTRL_HI_WRITE = 0x00000000,
        DEMOTE_1_WRITE     = 0xffffffff,
        DEMOTE_2_WRITE     = 0xffffffff
    };

    enum Register_Reset_Val
    {
        FEAT_CTRL_LO_RESET = 0x00000000,
        FEAT_CTRL_HI_RESET = 0x00000000,
        DEMOTE_1_RESET     = 0x00000000,
        DEMOTE_2_RESET     = 0x00000000
    };

    struct Register_Property_t
    {
        unsigned int reg_offset;
        unsigned int read_mask;
        unsigned int write_mask;
        unsigned int reg_reset;
        std::string  reg_name;
    };

    lifecycle_ctrl_basetest(sc_module_name name) : sc_module(name) {}
};
