// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// ===========================================================================
// vp/platform/sep/inc/smc_global_port.h
//
// SEP boundary port for the SMC global window (SEP CPU addresses
// [0x4000_0000, +1 GiB) per hw/sep/doc/memory_map.adoc; the VP maps the
// first 8 MiB, see Args.hpp smc_global_start/end_addr).
//
// Models the SEP-side end of the dedicated SEP->SMC AXI path
// (`sep_ext_to_smc_axi` in hw/sep/sep.sv).  Two operating modes:
//
//   * forward_en = false (default, standalone sep-vp): transactions land in
//     an internal paged-memory fallback, byte-identical to the historical
//     `smc_global` SEPMemory stub (RW backing store, no SMC behavior,
//     including the construction-time boot-handshake seeding and the
//     SEP_STATUS write tap).
//   * forward_en = true (SMU platform): transactions are re-based with
//     `window_base` (the SEP bus strips it before delivery; the boundary
//     speaks global addresses like the RTL port) and forwarded to `init64`,
//     which the SMU platform routes through the `axi_window_remap` model to
//     the SMC's `sep_axi_in`.  The fallback store is bypassed; the
//     construction-time seeding is skipped (the SMC firmware owns the
//     SMC->SEP handshake contract in an integrated system).
//
// The fallback is implemented on PagedMemory directly (not an embedded
// SEPMemory) so the module carries no extra socket that would need binding.
// ===========================================================================

#include <systemc.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <functional>

#include "paged_mem.h"
#include "reg_param.h"
#include "reg_logger.h"

class sep_smc_global_port : public sc_module
{
public:
#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif
    RegLogger logger;
    regmodel::Param<int> verbosity;

    /// Runtime mode select: false = internal RW fallback (standalone sep-vp),
    /// true = forward to the SMU platform via init64.
    regmodel::Param<bool> forward_en;
    /// Global base of the SMC window on the SEP CPU bus (re-added on forward;
    /// the internal SimpleBus strips it before delivery).
    regmodel::Param<uint64_t> window_base;

    /// Bus side (32-bit SimpleBus initiator socket binds here; addresses are
    /// window-local offsets).
    tlm_utils::simple_target_socket<sep_smc_global_port, 32> tgt32{"tgt32"};
    /// Boundary side (SMU platform binds here; addresses are global).
    tlm_utils::simple_initiator_socket<sep_smc_global_port, 64> init64{"init64"};

    /// Observation-only write tap (fallback mode only) — same contract as
    /// SEPMemory::setWriteTap.
    using WriteTap = std::function<void(uint64_t offset, const uint8_t* data, unsigned len)>;

    SC_HAS_PROCESS(sep_smc_global_port);

    explicit sep_smc_global_port(sc_module_name name)
        : sc_module(name)
        , verbosity("verbosity", REG_DEFAULT_VERBOSITY)
        , forward_en("forward_en", false)
        , window_base("window_base", 0x40000000ULL)
    {
        logger.setMaxVerbosity(verbosity.get_param_value());
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);

        tgt32.register_b_transport(this, &sep_smc_global_port::b_transport);
        tgt32.register_transport_dbg(this, &sep_smc_global_port::transport_dbg);
    }

    // regmodel::Param::get_param_value() is non-const, so this is too.
    bool forwarding() { return forward_en.get_param_value(); }

    // -- Fallback backdoor interface (used by the boot-handshake seed block;
    //    mirrors SEPMemory::load_data / load_zero) --------------------------
    void load_data(const char* src, uint64_t dst_addr, size_t n)
    {
        m_mem.writeBytes(dst_addr, reinterpret_cast<const uint8_t*>(src), n);
    }

    void load_zero(uint64_t dst_addr, size_t n)
    {
        m_mem.writeBytes(dst_addr, std::vector<uint8_t>(n, 0));
    }

    void setWriteTap(WriteTap tap) { m_write_tap = std::move(tap); }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        if (forwarding()) {
            const uint64_t local = trans.get_address();
            trans.set_address(window_base.get_param_value() + local);
            REG_DEBUG(3, logger) << name() << " FWD "
                << (trans.is_write() ? "WRITE" : "READ")
                << " local=0x" << std::hex << local
                << " global=0x" << trans.get_address() << std::dec << std::endl;
            init64->b_transport(trans, delay);
            trans.set_address(local);  // restore for the caller
            return;
        }

        // Fallback: plain RW backing store (historical smc_global stub).
        const uint64_t addr = trans.get_address();
        auto* ptr = trans.get_data_ptr();
        const auto len = trans.get_data_length();
        if (trans.is_write()) {
            m_mem.writeBytes(addr, ptr, len);
            if (m_write_tap)
                m_write_tap(addr, ptr, len);
        } else {
            m_mem.readBytes(addr, ptr, len);
        }
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& trans)
    {
        if (forwarding()) {
            const uint64_t local = trans.get_address();
            trans.set_address(window_base.get_param_value() + local);
            const unsigned n = init64->transport_dbg(trans);
            trans.set_address(local);
            return n;
        }
        const uint64_t addr = trans.get_address();
        auto* ptr = trans.get_data_ptr();
        const auto len = trans.get_data_length();
        if (trans.is_write())
            m_mem.writeBytes(addr, ptr, len);
        else
            m_mem.readBytes(addr, ptr, len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return len;
    }

    PagedMemory m_mem;
    WriteTap    m_write_tap;  // empty by default
};
