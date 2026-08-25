// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_scratch_warm_base.h"

class sep_scratch_warm_ip : public sep_scratch_warm_base
{
public:
    SC_HAS_PROCESS(sep_scratch_warm_ip);

    explicit sep_scratch_warm_ip(sc_module_name n);

    /// Active-low asynchronous reset — clears all scratch registers when asserted
    sc_core::sc_in<bool> rst_ni;

private:
    void reset_handler();
};
