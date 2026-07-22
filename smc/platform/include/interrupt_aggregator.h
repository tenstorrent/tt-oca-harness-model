// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// smc/platform/include/interrupt_aggregator.h
//
// Composes the SMC peripheral interrupt vector and drives the PLIC source
// lines, mirroring the `peripheral_interrupts_o` composition in
// `smc_peripherals.sv` (see `systemc_tlm2_integration_guide.adoc` §Interrupt
// Composition).
//
// Each `src[i]` input is a level-active peripheral IRQ; the constructor maps
// it to a PLIC source bit `plic_bits[i]` (0-based; PLIC source ID = bit + 1).
// The SC_METHOD drives `plic_src[b]` = the OR of all inputs mapped to bit `b`,
// and drives every other `plic_src[b]` low.  CLINT MSIP/MTIP bypass the PLIC
// and are wired directly to the CPU cluster — they are NOT handled here.
//
// The platform binds `plic_src[i] -> plic.src_in[i]` for every PLIC source so
// every PLIC input is driven (no unbound ports).
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>

#include <cstdint>
#include <vector>

#include "sim_log.h"

namespace smc {

class interrupt_aggregator : public sc_core::sc_module
{
public:
    sc_core::sc_vector<sc_core::sc_in<bool>>  src{"src"};
    sc_core::sc_vector<sc_core::sc_out<bool>> plic_src{"plic_src"};

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    SC_HAS_PROCESS(interrupt_aggregator);

    interrupt_aggregator(sc_core::sc_module_name name,
                         unsigned num_inputs,
                         unsigned num_plic_src,
                         std::vector<unsigned> plic_bits)
        : sc_core::sc_module(name)
        , plic_bits_(std::move(plic_bits))
    {
        src.init(num_inputs);
        plic_src.init(num_plic_src);
        for (unsigned i = 0; i < plic_src.size(); ++i)
            plic_src[i].initialize(false);

        SC_METHOD(recompute);
        for (unsigned i = 0; i < src.size(); ++i)
            sensitive << src[i];
    }

private:
    void recompute()
    {
        // Default every PLIC source low; raise the ones driven by active inputs.
        for (unsigned b = 0; b < plic_src.size(); ++b)
            plic_src[b].write(false);

        for (unsigned i = 0; i < src.size(); ++i) {
            if (!src[i].read())
                continue;
            if (i < plic_bits_.size() && plic_bits_[i] < plic_src.size())
                plic_src[plic_bits_[i]].write(true);
        }
    }

    std::vector<unsigned> plic_bits_;
};

}  // namespace smc
