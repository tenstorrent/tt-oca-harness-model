// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "lifecycle_ctrl_basetest.h"

class lifecycle_ctrl_test : public lifecycle_ctrl_basetest
{
public:
    lifecycle_ctrl_test(sc_module_name name) : lifecycle_ctrl_basetest(name) {}

    void register_read_32(unsigned int offset, uint32_t &read_value);
    void register_write_32(unsigned int offset, uint32_t write_value);

    ~lifecycle_ctrl_test() {}
};
