// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "sep_scratch_warm_basetest.h"
#include "tlm_probe.h"

class sep_scratch_warm_test : public sep_scratch_warm_basetest
{
public:
   sep_scratch_warm_test(sc_module_name name) : sep_scratch_warm_basetest(name)
   {
   }

   // Return the transport outcome so callers can assert on it; `read_value` is
   // only written when the access succeeded.
   simtlm::access_result register_read_8(unsigned int offset, uint8_t &read_value);
   simtlm::access_result register_write_8(unsigned int offset, uint8_t write_value);
   ~sep_scratch_warm_test() {}
};