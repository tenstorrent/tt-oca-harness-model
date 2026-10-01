// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_filter_ctrl_register.h"
#include <string.h>

// MAX_INSTANCES=32 is the compile-time template parameter for regmodel::RegVector.
// Memory is always allocated for MAX_INSTANCES entries so that regmodel::RegVector<T,32>
// never accesses out-of-bounds words — even when num_instances_=16 (inbound).
// The runtime num_instances_ gates which entries are reset and iterated by the IP.
static constexpr uint32_t SEP_FILTER_CTRL_MAX_INSTANCES = 32;
static constexpr uint32_t SEP_FILTER_CTRL_MAX_MEM_WORDS =
    SEP_FILTER_CTRL_MAX_INSTANCES * 0x20 / sizeof(unsigned long long);

class sep_filter_ctrl_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<64>::DT DT;

    sep_filter_ctrl_base(sc_module_name name, std::string type, uint32_t num_instances)
        : sc_module(name)
        , type(type)
        , num_instances_(num_instances)
        // Always allocate MAX_INSTANCES words so regmodel::RegVector<T,32> stays in-bounds
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
        // The CSR socket is owned here rather than bound straight to
        // regmodel::Memory: axi_filter_wrap.sv steers writes to a locked entry
        // to the AXI error subordinate (DECERR), which the register file's
        // write-callback contract cannot express (a callback result never
        // changes the TLM response). The derived IP decides via
        // csr_write_steered_to_err_slv(); everything else is delegated to
        // the register file unchanged.
        target_socket.register_b_transport(this, &sep_filter_ctrl_base::csr_b_transport);
        target_socket.register_transport_dbg(this, &sep_filter_ctrl_base::csr_transport_dbg);
    }

    std::string type;
    uint32_t    num_instances_;

    regmodel::Memory<64> memory;
    tlm_utils::simple_target_socket<sep_filter_ctrl_base, 32> target_socket;

    /// True when a write covering [addr, addr+len) must be answered with
    /// DECERR instead of being applied (locked entry, RTL #2480). The IP
    /// supplies the locked-bit decision; the base (an sc_module, so already
    /// polymorphic) only hosts the socket plumbing and is never instantiated
    /// on its own.
    virtual bool csr_write_steered_to_err_slv(sc_dt::uint64 addr,
                                              unsigned int  len) const = 0;

    void csr_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        if (trans.is_write()) {
            // Malformed payloads keep the register file's own verdict so the
            // TLM well-formedness contract is unchanged; only a well-formed
            // write can reach the error subordinate.
            const tlm::tlm_response_status vs = regmodel::Memory<64>::validate(trans);
            if (vs == tlm::TLM_OK_RESPONSE &&
                csr_write_steered_to_err_slv(trans.get_address(), trans.get_data_length())) {
                trans.set_dmi_allowed(false);
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
        }
        memory.b_transport(trans, delay);
    }

    unsigned int csr_transport_dbg(tlm::tlm_generic_payload& trans)
    {
        if (trans.is_write()) {
            const tlm::tlm_response_status vs = regmodel::Memory<64>::validate(trans);
            if (vs == tlm::TLM_OK_RESPONSE &&
                csr_write_steered_to_err_slv(trans.get_address(), trans.get_data_length())) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return 0;  // nothing transferred — the error subordinate absorbed it
            }
        }
        return memory.transport_dbg(trans);
    }

    // Template param MAX_INSTANCES=32 covers both outbound (32) and inbound (16).
    regmodel::RegVector<sep_filter_ctrl::FILTER_CONFIG_type<64>, 32> FILTER_CONFIG;
    regmodel::RegVector<sep_filter_ctrl::START_ADDR_type<64>,    32> START_ADDR;
    regmodel::RegVector<sep_filter_ctrl::END_ADDR_type<64>,      32> END_ADDR;

    void reset_all_registers();
};
