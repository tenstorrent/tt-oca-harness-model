// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class sep_filter_ctrl_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<sep_filter_ctrl_basetest, 32> initiator_socket;

    enum Register_offset
    {
        FILTER_CONFIG_OFFSET = (0x0 + 0x00),
        START_ADDR_OFFSET    = (0x8 + 0x00),
        END_ADDR_OFFSET      = (0x10 + 0x00)
    };

    enum Register_Read_Access
    {
        FILTER_CONFIG_READ = (0x8000000001ff0113),
        START_ADDR_READ    = (0xffffffffffffff),
        END_ADDR_READ      = (0xffffffffffffff)
    };

    enum Register_Write_Access
    {
        FILTER_CONFIG_WRITE = (0x8000000001ff0113),
        START_ADDR_WRITE    = (0xffffffffffffff),
        END_ADDR_WRITE      = (0xffffffffffffff)
    };

    enum Register_Reset_Val
    {
        FILTER_CONFIG_RESET = (0x0),
        START_ADDR_RESET    = (0x0),
        END_ADDR_RESET      = (0x7)
    };

    struct Register_Property_t
    {
        unsigned int reg_offset;
        uint64_t     read_mask;
        uint64_t     write_mask;
        uint64_t     reg_reset;
        std::string  reg_name;
    };

    sep_filter_ctrl_basetest(sc_module_name name) : sc_module(name) {}
};
