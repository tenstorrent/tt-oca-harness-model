// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_output_remap_ctrl_basetest.h"

class sep_output_remap_ctrl_test : public sep_output_remap_ctrl_basetest
{
public:
    sep_output_remap_ctrl_test(sc_module_name name)
        : sep_output_remap_ctrl_basetest(name) {}

    void register_read_8 (unsigned int offset, uint8_t&  read_value);
    void register_write_8(unsigned int offset, uint8_t   write_value);

    void register_read_64 (unsigned int offset, uint64_t& read_value);
    void register_write_64(unsigned int offset, uint64_t  write_value);

    ~sep_output_remap_ctrl_test() {}
};
