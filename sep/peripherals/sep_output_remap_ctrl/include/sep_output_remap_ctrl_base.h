// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_output_remap_ctrl_register.h"
#include <string>

class sep_output_remap_ctrl_base : public sc_module
{
public:
    typedef typename regmodel::Reg<64>::DT DT;

    sep_output_remap_ctrl_base(sc_module_name name, std::string type, unsigned int memory_size)
        : sc_module(name)
        , type(type)
        , memory(std::string(name) + ".Memory", memory_size / sizeof(unsigned long long))
        , REGION_ATTRS(std::string(name) + ".REGION_ATTRS", memory,
                       (0x0 + 0x00) / sizeof(unsigned long long), 1)
    {
        memory.bind_to_socket(target_socket);
    }

    std::string type;
    regmodel::Memory<64> memory;
    tlm_utils::simple_target_socket<regmodel::Memory<64>, 32> target_socket;

    regmodel::RegVector<sep_output_remap_ctrl::REGION_ATTRS_type<64>, 16> REGION_ATTRS;

    void reset_all_registers();
};
