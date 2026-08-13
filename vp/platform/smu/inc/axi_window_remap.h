// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smu/inc/axi_window_remap.h
//
// TLM-2.0 LT model of the OCAH `axi_window_remap` RTL block
// (tt-oca-hw hw/ip/axi_window_remap/rtl/axi_window_remap.sv).
//
// Generic single-window address translation: transactions whose address falls
// in [local_alias_base, local_alias_base + region_size) are forwarded with
// the address adjusted by (local_alias_base - target_base); addresses outside
// the window pass through unchanged — exactly the RTL mux behavior.
//
// In the SMU platform this models `sep_ext_to_smc_axi_local_alias_remap`
// (hw/smu/rtl/smu.sv): the SEP's SMC window [0x4000_0000, +1 GiB) is rebased
// to alias base 0x0 before entering the SMC's dedicated `sep_axi_in` port.
//
// Width-generic, blocking-transport only (LT), depends on SystemC/TLM and
// `sim_log.h`.  Window parameters are plain constructor arguments (the SMU
// instance is structural; the RTL inputs are quasi-static straps/CSRs of the
// integrating platform, not of this block).
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include "sim_log.h"

namespace smu {

template <unsigned InBus = 64, unsigned OutBus = InBus>
class axi_window_remap : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<axi_window_remap, InBus>   tgt{"tgt"};
    tlm_utils::simple_initiator_socket<axi_window_remap, OutBus> init{"init"};

    SC_HAS_PROCESS(axi_window_remap);

    axi_window_remap(sc_core::sc_module_name name,
                     uint64_t local_alias_base,
                     uint64_t region_size,
                     uint64_t target_base)
        : sc_core::sc_module(name)
        , local_alias_base_(local_alias_base)
        , region_size_(region_size)
        , target_base_(target_base)
    {
        tgt.register_b_transport(this, &axi_window_remap::b_transport);
        tgt.register_transport_dbg(this, &axi_window_remap::transport_dbg);
    }

private:
    // RTL: in-window -> addr - (local_alias_base - target_base); else passthru.
    uint64_t map(uint64_t addr) const
    {
        if (addr >= local_alias_base_ && addr < local_alias_base_ + region_size_)
            return addr - (local_alias_base_ - target_base_);
        return addr;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t orig = trans.get_address();
        const uint64_t remapped = map(orig);
        SIM_LOG_TRACE(this, "remap 0x" << std::hex << orig << " -> 0x" << remapped);
        trans.set_address(remapped);
        init->b_transport(trans, delay);
        trans.set_address(orig);  // restore for the caller
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const uint64_t orig = trans.get_address();
        trans.set_address(map(orig));
        const unsigned n = init->transport_dbg(trans);
        trans.set_address(orig);
        return n;
    }

    const uint64_t local_alias_base_;
    const uint64_t region_size_;
    const uint64_t target_base_;
};

}  // namespace smu
