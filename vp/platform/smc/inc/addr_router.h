// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smc/inc/addr_router.h
//
// Header-only TLM-2.0 address-range demux used by `smc_platform` to split the
// fabric's `to_front_port` and `to_periph` initiator sockets across multiple
// downstream peripheral targets.
//
// The `tgt` target socket receives transactions; an `out[i]` initiator socket
// forwards each transaction whose address falls in route `i`'s [base, base+size)
// window, with `base` subtracted before forwarding (downstream targets are
// offset-relative).  Routes are matched longest-window-first so a small
// sub-window can override a larger enclosing one.
//
// `tgt` is a multi-bind socket because one device set can have more than one
// upstream master.  The front-port router is reached both by the fabric (for
// external masters coming through the local crossbar) and directly by the CPU
// cluster, which in RTL talks to its own PLIC/CLINT/BEU over the rocket-chip
// periphery bus rather than through the SMC fabric.  Every bound master is
// routed identically, so the bound-initiator index is ignored.
//
// Unmapped accesses complete with TLM_ADDRESS_ERROR_RESPONSE so unintended
// decode holes surface immediately rather than silently hanging firmware.
//
// The router is width-generic and depends only on SystemC/TLM + `sim_log.h`.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "sim_log.h"

namespace smc {

template <unsigned InBus = 64, unsigned OutBus = InBus>
class addr_router : public sc_core::sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<addr_router, InBus> tgt{"tgt"};

    sc_core::sc_vector<tlm_utils::simple_initiator_socket<addr_router, OutBus>> out{"out"};

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    SC_HAS_PROCESS(addr_router);

    addr_router(sc_core::sc_module_name name, unsigned num_outputs)
        : sc_core::sc_module(name)
    {
        out.init(num_outputs);
        tgt.register_b_transport(this, &addr_router::b_transport);
    }

    // -----------------------------------------------------------------------
    // Route table.  Routes may be added in any order; lookup checks the
    // smallest (most specific) window first so a narrow sub-window shadows a
    // wider enclosing catch-all.
    // -----------------------------------------------------------------------
    addr_router& add_route(unsigned idx, uint64_t base, uint64_t size,
                           const std::string& label = "")
    {
        routes_.push_back({idx, base, size, label});
        // Sort ascending by size so the smallest (most specific) window wins.
        std::sort(routes_.begin(), routes_.end(),
                  [](const route& a, const route& b) { return a.size < b.size; });
        return *this;
    }

private:
    struct route {
        unsigned    idx;
        uint64_t    base;
        uint64_t    size;
        std::string label;
    };

    // Leading int is the bound-initiator index from the multi-socket; all
    // upstream masters are routed identically, so it is unused.
    void b_transport(int, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        const uint64_t addr = trans.get_address();

        for (const auto& r : routes_) {
            if (addr >= r.base && addr < r.base + r.size) {
                const uint64_t orig = addr;
                trans.set_address(addr - r.base);
                out[r.idx]->b_transport(trans, delay);
                trans.set_address(orig);  // restore for the caller
                return;
            }
        }

        SIM_LOG_DEBUG(this, "decode miss off=0x" << std::hex << addr);
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    std::vector<route> routes_;
};

}  // namespace smc
