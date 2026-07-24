// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smc/inc/stub_target.h
//
// Minimal TLM-2.0 target used by `smc_platform` to stand in for RTL-connected
// blocks that do not yet have functional SystemC models (WDT/debug, BEU, DMA,
// zeroer, DFD, mailbox, DFT, GPIO, PVT, AVS, eFuse, telemetry, OCTS, DTP, misc,
// system-memory output, and the fabric's internal-CSR initiator sockets).
//
// Behaviour:
//   * Reads return a per-offset reset value (default 0) and complete with
//     TLM_OK_RESPONSE.  Out-of-window offsets are RAZ (read-as-zero).
//   * Writes are accepted and ignored (WI); TLM_OK_RESPONSE is returned.
//   * Optionally logs a one-shot DEBUG message on the first access so decode
//     holes show up in traces without spamming the log.
//   * Optionally exposes an `irq_o` line a testbench can pulse to inject an
//     interrupt into the PLIC for blocks modelled only as stubs.
//
// The stub is intentionally dependency-free beyond SystemC/TLM and `sim_log.h`.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <unordered_map>

#include "sim_log.h"

namespace smc {

template <unsigned BusWidth = 32>
class stub_target : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<stub_target, BusWidth> reg_socket{"reg_socket"};

    sc_core::sc_out<bool> irq_o{"irq_o"};

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    SC_HAS_PROCESS(stub_target);

    explicit stub_target(sc_core::sc_module_name name,
                         bool warn_on_access = true)
        : sc_core::sc_module(name)
        , warn_on_access_(warn_on_access)
    {
        reg_socket.register_b_transport(this, &stub_target::b_transport);

        // irq_o idles low; a testbench may drive it to inject an interrupt.
        irq_o.initialize(false);

        SC_METHOD(irq_drive_method);
        sensitive << irq_event_;
        dont_initialize();
    }

    // -----------------------------------------------------------------------
    // Reset-value map (optional).  Offsets not in the map read as zero.
    // -----------------------------------------------------------------------
    void set_reset_value(uint64_t offset, uint32_t value)
    {
        reset_values_[offset] = value;
    }

    // Testbench hook: pulse irq_o for `duration` to inject a PLIC source.
    void pulse_irq(const sc_core::sc_time& duration = sc_core::SC_ZERO_TIME)
    {
        irq_pending_ = true;
        irq_hold_ = duration;
        irq_event_.notify(sc_core::SC_ZERO_TIME);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const auto addr = trans.get_address();
        const auto len  = trans.get_data_length();
        auto* ptr       = trans.get_data_ptr();

        if (warn_on_access_ && !warned_) {
            warned_ = true;
            SIM_LOG_DEBUG(this, "stub access off=0x" << std::hex << addr
                          << " len=" << std::dec << len
                          << (trans.is_read() ? " rd" : " wr"));
        }

        if (trans.is_read()) {
            const uint32_t v = lookup(addr);
            for (unsigned i = 0; i < len; ++i)
                ptr[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
        }
        // Writes are accepted and ignored (WI).

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    uint32_t lookup(uint64_t offset) const
    {
        const auto it = reset_values_.find(offset);
        return it == reset_values_.end() ? 0u : it->second;
    }

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
    std::unordered_map<uint64_t, uint32_t> reset_values_;
};

}  // namespace smc
