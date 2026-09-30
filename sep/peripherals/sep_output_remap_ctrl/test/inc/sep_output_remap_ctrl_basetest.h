// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class sep_output_remap_ctrl_basetest : public sc_module
{
public:
    // CSR initiator: 32-bit bus width matches target_socket<regmodel::Memory<64>, 32>
    tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_basetest, 32> initiator_socket;

    enum Register_offset
    {
        REGION_ATTRS_OFFSET = 0x0
    };

    enum Register_Read_Access
    {
        REGION_ATTRS_READ = 0x80ffffffffffffffULL
    };

    enum Register_Write_Access
    {
        REGION_ATTRS_WRITE = 0x80ffffffffffffffULL
    };

    enum Register_Reset_Val
    {
        REGION_ATTRS_RESET = 0x0
    };

    struct Register_Property_t
    {
        unsigned int reg_offset;
        uint64_t     read_mask;
        uint64_t     write_mask;
        uint64_t     reg_reset;
        std::string  reg_name;
    };

    sep_output_remap_ctrl_basetest(sc_module_name name) : sc_module(name) {}
};
