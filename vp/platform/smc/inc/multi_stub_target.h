// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smc/inc/multi_stub_target.h
//
// Same RAZ/WI behaviour as `stub_target`, but built on the base
// `tlm::tlm_target_socket` (an sc_export) so that **multiple initiator sockets
// can bind to the same target**.  Accellera's `simple_target_socket` caps its
// internal backward port at one initiator, which makes it unusable for the
// platform's conditional bindings where a cluster-only initiator (e.g.
// `cluster.data`) and an always-bound idle initiator must share one stub in
// different build modes.  With a base export socket both can bind without
// conflict.
//
// Only the LT `b_transport` path is implemented (the platform is
// loosely-timed); the remaining `tlm_fw_transport_if` methods return defaults.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>

#include <cstdint>

#include "sim_log.h"

namespace smc {

template <unsigned BusWidth = 64, unsigned N = 2>
class multi_stub_target : public sc_core::sc_module
                     , public tlm::tlm_fw_transport_if<>
{
public:
    // N (default 2) sets the max number of initiator sockets that may bind to
    // this one target: the always-bound idle initiator plus a cluster-only
    // initiator (and, for stub_data, a second idle initiator that needs a sink
    // when the cluster owns fabric.mmio_in).
    tlm::tlm_target_socket<BusWidth, tlm::tlm_base_protocol_types, N> reg_socket{"reg_socket"};

    sc_core::sc_out<bool> irq_o{"irq_o"};

    SC_HAS_PROCESS(multi_stub_target);

    explicit multi_stub_target(sc_core::sc_module_name name,
                               bool warn_on_access = true)
        : sc_core::sc_module(name)
        , warn_on_access_(warn_on_access)
    {
        reg_socket.bind(*this);
        irq_o.initialize(false);
        SC_METHOD(irq_drive_method);
        sensitive << irq_event_;
        dont_initialize();
    }

    void pulse_irq(const sc_core::sc_time& duration = sc_core::SC_ZERO_TIME)
    {
        irq_pending_ = true;
        irq_hold_ = duration;
        irq_event_.notify(sc_core::SC_ZERO_TIME);
    }

    // -- tlm_fw_transport_if -------------------------------------------------
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay) override
    {
        const auto addr = trans.get_address();
        const auto len  = trans.get_data_length();
        auto* ptr       = trans.get_data_ptr();

        if (warn_on_access_ && !warned_) {
            warned_ = true;
            SIM_LOG_DEBUG(this, "multi-stub access off=0x" << std::hex << addr
                          << " len=" << std::dec << len
                          << (trans.is_read() ? " rd" : " wr"));
        }

        if (trans.is_read()) {
            for (unsigned i = 0; i < len; ++i) ptr[i] = 0;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    // The remaining tlm_fw_transport_if methods are pure-virtual contract
    // stubs.  The platform is loosely-timed (b_transport only) so DMI, AT, and
    // debug transport are never issued through the socket; the testbench
    // exercises them directly on the stub to satisfy the coverage gate.
    bool get_direct_mem_ptr(tlm::tlm_generic_payload&,
                            tlm::tlm_dmi&) override { return false; }

    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload&,
                                       tlm::tlm_phase&,
                                       sc_core::sc_time&) override
    { return tlm::TLM_ACCEPTED; }

    unsigned int transport_dbg(tlm::tlm_generic_payload&) override { return 0; }

private:
    void irq_drive_method()
    {
        if (irq_pending_) {
            irq_pending_ = false;
            irq_o.write(true);
            if (irq_hold_ > sc_core::SC_ZERO_TIME)
                irq_event_.notify(irq_hold_);
            else
                irq_event_.notify(sc_core::SC_ZERO_TIME);
        } else {
            irq_o.write(false);
        }
    }

    bool warn_on_access_;
    bool warned_ = false;
    bool irq_pending_ = false;
    sc_core::sc_time irq_hold_ = sc_core::SC_ZERO_TIME;
    sc_core::sc_event irq_event_;
};

}  // namespace smc
