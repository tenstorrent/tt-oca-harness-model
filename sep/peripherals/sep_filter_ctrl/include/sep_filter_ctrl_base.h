// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_filter_ctrl_register.h"
#include <string.h>

// MAX_INSTANCES=32 is the compile-time template parameter for csml_reg_vector.
// Memory is always allocated for MAX_INSTANCES entries so that csml_reg_vector<T,32>
// never accesses out-of-bounds words — even when num_instances_=16 (inbound).
// The runtime num_instances_ gates which entries are reset and iterated by the IP.
static constexpr uint32_t SEP_FILTER_CTRL_MAX_INSTANCES = 32;
static constexpr uint32_t SEP_FILTER_CTRL_MAX_MEM_WORDS =
    SEP_FILTER_CTRL_MAX_INSTANCES * 0x20 / sizeof(unsigned long long);

class sep_filter_ctrl_base : public sc_module
{
  public:
    typedef typename csml_reg<64>::DT DT;

    sep_filter_ctrl_base(sc_module_name name, std::string type, uint32_t num_instances)
        : sc_module(name)
        , type(type)
        , num_instances_(num_instances)
        // Always allocate MAX_INSTANCES words so csml_reg_vector<T,32> stays in-bounds
        , memory(std::string(name) + ".Memory", SEP_FILTER_CTRL_MAX_MEM_WORDS)
        , FILTER_CONFIG(std::string(name) + ".FILTER_CONFIG", memory,
                        0x0 / sizeof(unsigned long long),
                        0x20 / sizeof(unsigned long long))
        , START_ADDR(std::string(name) + ".START_ADDR", memory,
                     0x8 / sizeof(unsigned long long),
                     0x20 / sizeof(unsigned long long))
        , END_ADDR(std::string(name) + ".END_ADDR", memory,
                   0x10 / sizeof(unsigned long long),
                   0x20 / sizeof(unsigned long long))
    {
        memory.bind_to_socket(target_socket);
    }

    std::string type;
    uint32_t    num_instances_;

    csml_memory<64> memory;
    tlm_utils::simple_target_socket<csml_memory<64>, 32> target_socket;

    // Template param MAX_INSTANCES=32 covers both outbound (32) and inbound (16).
    csml_reg_vector<sep_filter_ctrl::FILTER_CONFIG_type<64>, 32> FILTER_CONFIG;
    csml_reg_vector<sep_filter_ctrl::START_ADDR_type<64>,    32> START_ADDR;
    csml_reg_vector<sep_filter_ctrl::END_ADDR_type<64>,      32> END_ADDR;

    void reset_all_registers();
};
