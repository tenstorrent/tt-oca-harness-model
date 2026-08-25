// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "local_alias_remap_basetest.h"

class local_alias_remap_test : public local_alias_remap_basetest
{
public:
   local_alias_remap_test(sc_module_name name) : local_alias_remap_basetest(name)
   {
   }

   void register_read_8(unsigned int offset, uint8_t &read_value);
   void register_write_8(unsigned int offset, uint8_t write_value);
   void register_read_64(unsigned int offset, uint64_t &read_value);
   void register_write_64(unsigned int offset, uint64_t write_value);
   ~local_alias_remap_test() {}
};