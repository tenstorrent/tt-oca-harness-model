// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc.h>
#include <tlm_utils/simple_target_socket.h>

#include <boost/iostreams/device/mapped_file.hpp>
#include <fstream>
#include <iostream>
#include <utility>

#include "paged_mem.h"
#include "load_if.h"
#include "csml_parameter.h"
#include "csml_logger.h"

class SEPMemory : public sc_module, public load_if
{
  public:

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
    // CSML Logger
    CsmlLogger logger;
    csml_param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug

    typedef tlm::tlm_generic_payload TRANS;

    tlm_utils::simple_target_socket<SEPMemory> tsock;

    PagedMemory m_mem;

    ~SEPMemory() = default;
    SEPMemory(sc_module_name name, bool read_only = false);

    void load_data(const char *src, uint64_t dst_addr, size_t n) override;
    void load_zero(uint64_t dst_addr, size_t n) override;
    void load_binary_file(const std::string &filename, uint64_t addr);

    void b_transport(TRANS& trans, sc_core::sc_time& delay);
    bool get_direct_mem_ptr(TRANS& trans, tlm::tlm_dmi& dmi);
    unsigned transport_dbg(TRANS& trans);

  private:
    bool m_read_only;

};
