// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include <tlm.h>

#include "local_alias_remap_basetest.h"

class local_alias_remap_test : public local_alias_remap_basetest
{
public:
   local_alias_remap_test(sc_module_name name) : local_alias_remap_basetest(name)
   {
   }

   /// CSR helpers return the TLM response status. Callers must treat a non-OK
   /// status as a failed check — these never invent a zeroed read on error.
   /// [[nodiscard]] because dropping the status is exactly how a rejected
   /// transaction turns into a silent pass on stale buffer contents.
   [[nodiscard]] tlm::tlm_response_status register_read_8(unsigned int offset, uint8_t& read_value);
   [[nodiscard]] tlm::tlm_response_status register_write_8(unsigned int offset, uint8_t write_value);
   [[nodiscard]] tlm::tlm_response_status register_read_64(unsigned int offset, uint64_t& read_value);
   [[nodiscard]] tlm::tlm_response_status register_write_64(unsigned int offset, uint64_t write_value);

   /// Generic CSR access for malformed-protocol cases (custom length / command /
   /// byte-enables / streaming width / null data pointer).
   [[nodiscard]] tlm::tlm_response_status csr_transport(tlm::tlm_command cmd,
                                                        unsigned int offset,
                                                        unsigned char* data,
                                                        unsigned int len,
                                                        unsigned int streaming_width,
                                                        unsigned char* be = nullptr,
                                                        unsigned int be_len = 0);

   ~local_alias_remap_test() {}
};
