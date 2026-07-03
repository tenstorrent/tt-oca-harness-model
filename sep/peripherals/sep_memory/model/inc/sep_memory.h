#pragma once

#include <systemc.h>
#include <tlm_utils/simple_target_socket.h>

#include <boost/iostreams/device/mapped_file.hpp>
#include <fstream>
#include <functional>
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

    // Optional, observation-only write tap. If set, it is invoked on every accepted
    // write to this instance (after the backing-store update) with the local offset,
    // data pointer, and length. The SIM_OUT virtual console uses this to watch
    // SEP_SCRATCH_COLD_SCRATCH_2 without changing the register's R/W semantics.
    using WriteTap = std::function<void(uint64_t offset, const uint8_t* data, unsigned len)>;
    void setWriteTap(WriteTap tap) { m_write_tap = std::move(tap); }

  private:
    bool m_read_only;
    WriteTap m_write_tap;  // empty by default — no overhead when unset

};
