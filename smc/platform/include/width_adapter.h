// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// smc/platform/include/width_adapter.h
//
// Header-only TLM-2.0 LT width adapter.  Bridges a target socket of width
// `InBus` to an initiator socket of width `OutBus` by forwarding the generic
// payload verbatim (no byte re-packing).  This is sufficient for the SMC
// platform's register-class accesses, where every transaction's data_length
// is <= min(InBus, OutBus) / 8 (firmware uses 1/2/4-byte loads and stores).
//
// It exists because Accellera `simple_target_socket` / `simple_initiator_socket`
// refuse to bind across mismatched bus widths, so the fabric's 64-bit
// `to_front_port` cannot bind directly to a 32-bit peripheral reg_socket.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "sim_log.h"

namespace smc {

template <unsigned InBus = 64, unsigned OutBus = 32>
class width_adapter : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<width_adapter, InBus>   tgt{"tgt"};
    tlm_utils::simple_initiator_socket<width_adapter, OutBus> init{"init"};

    SC_HAS_PROCESS(width_adapter);

    explicit width_adapter(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        tgt.register_b_transport(this, &width_adapter::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        init->b_transport(trans, delay);
    }
};

}  // namespace smc
