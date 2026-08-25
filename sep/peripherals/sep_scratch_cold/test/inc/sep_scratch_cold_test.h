// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "sep_scratch_cold_basetest.h"

class sep_scratch_cold_test : public sep_scratch_cold_basetest
{
public:
   sep_scratch_cold_test(sc_module_name name) : sep_scratch_cold_basetest(name)
   {
   }

   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   ~sep_scratch_cold_test() {}
};