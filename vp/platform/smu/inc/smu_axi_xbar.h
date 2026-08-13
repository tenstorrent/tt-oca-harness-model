// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smu/inc/smu_axi_xbar.h
//
// TLM-2.0 LT model of the OCAH SMU AXI crossbar (`smu_axi_xbar`,
// tt-oca-hw hw/smu/rtl/smu_axi_xbar.sv; see doc/architecture.adoc
// "SMU AXI Crossbar" and doc/integration_guide.adoc "SMU AXI Crossbar
// Address Map").
//
// 3x3 non-reflexive AXI4 crossbar connecting the SEP, the SMC, and the
// chiplet-facing external boundary (the SMN / future AXI-over-D2D leg):
//
//   masters (target sockets)     subordinates (initiator sockets)
//   ------------------------     ------------------------------
//   sep_out  (SEP outbound)  ->  smc_in | ext_out   (never sep_in)
//   smc_out  (SMC output_axi)->  sep_in | ext_out   (never smc_in)
//   ext_in   (SMN/D2D inbound)->  sep_in | smc_in    (never ext_out)
//
// Routing uses a runtime address map with two programmable apertures and a
// static catch-all, mirroring the RTL:
//   * SEP aperture [sep_global_base, +sep_region_size)
//   * SMC aperture [smc_global_base, +smc_region_size)
//   * ext_out is the catch-all for SEP/SMC outbound traffic that misses both
//     apertures; ext_in traffic missing both apertures fails with
//     TLM_ADDRESS_ERROR_RESPONSE (the RTL has no fourth route for it).
//
// The apertures are CCI-mutable params so platform firmware/integrators
// reprogram them exactly like the RTL's SEP_GLOBAL_BASE_ADDR/SEP_REGION_SIZE
// and SMC GLOBAL_BASE/REGION_SIZE CSR pairs (re-read on every transaction).
// Addresses are forwarded unchanged (global addresses end-to-end; each
// subsystem performs its own inbound global->local remap, as in RTL).
//
// The RTL's axi_iw_converter ID-width stages (xbar 10-bit -> port 6-bit) are
// not functionally modeled: TLM-2.0 LT transports route responses on the
// return path of the same call, so no ID bookkeeping is required.
//
// Depends only on SystemC/TLM, CCI, and `sim_log.h`.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>

#include <cstdint>

#include "sim_log.h"

namespace smu {

class smu_axi_xbar : public sc_core::sc_module
{
public:
    // -----------------------------------------------------------------------
    // Aperture configuration (RTL CSR equivalents).  Mutable at run-time and
    // re-read per transaction, matching the RTL's quasi-static CSR inputs.
    // Defaults follow the RTL reset/VP conventions: SMC global window at
    // 0x4000_0000 (16 MiB), SEP global window at 0x5000_0000 (16 MiB) — the
    // two apertures must not overlap and must sit below the ext_out catch-all.
    // -----------------------------------------------------------------------
    cci::cci_param<uint64_t> sep_global_base_p_;
    cci::cci_param<uint64_t> sep_region_size_p_;
    cci::cci_param<uint64_t> smc_global_base_p_;
    cci::cci_param<uint64_t> smc_region_size_p_;

    // -----------------------------------------------------------------------
    // Master-side target sockets (RTL: sep_out_req_i / smc_out_req_i /
    // ext_in_req_i).
    // -----------------------------------------------------------------------
    tlm_utils::simple_target_socket<smu_axi_xbar, 64> sep_out{"sep_out"};
    tlm_utils::simple_target_socket<smu_axi_xbar, 64> smc_out{"smc_out"};
    tlm_utils::simple_target_socket<smu_axi_xbar, 64> ext_in{"ext_in"};

    // -----------------------------------------------------------------------
    // Subordinate-side initiator sockets (RTL: sep_in_req_o / smc_in_req_o /
    // ext_out_req_o).
    // -----------------------------------------------------------------------
    tlm_utils::simple_initiator_socket<smu_axi_xbar, 64> sep_in{"sep_in"};
    tlm_utils::simple_initiator_socket<smu_axi_xbar, 64> smc_in{"smc_in"};
    tlm_utils::simple_initiator_socket<smu_axi_xbar, 64> ext_out{"ext_out"};

    SC_HAS_PROCESS(smu_axi_xbar);

    explicit smu_axi_xbar(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , sep_global_base_p_("sep_global_base", 0x5000'0000ULL,
                             "SEP global aperture base (SEP_GLOBAL_BASE_ADDR CSR equivalent)")
        , sep_region_size_p_("sep_region_size", 0x0100'0000ULL,
                             "SEP global aperture size (SEP_REGION_SIZE CSR equivalent, 16 MiB reset)")
        , smc_global_base_p_("smc_global_base", 0x4000'0000ULL,
                             "SMC global aperture base (SMC GLOBAL_BASE CSR equivalent)")
        , smc_region_size_p_("smc_region_size", 0x0100'0000ULL,
                             "SMC global aperture size (SMC REGION_SIZE CSR equivalent, 16 MiB reset)")
    {
        sep_out.register_b_transport(this, &smu_axi_xbar::bt_sep_out);
        smc_out.register_b_transport(this, &smu_axi_xbar::bt_smc_out);
        ext_in .register_b_transport(this, &smu_axi_xbar::bt_ext_in);
        sep_out.register_transport_dbg(this, &smu_axi_xbar::dbg_sep_out);
        smc_out.register_transport_dbg(this, &smu_axi_xbar::dbg_smc_out);
        ext_in .register_transport_dbg(this, &smu_axi_xbar::dbg_ext_in);

        SIM_LOG_INFO(this, "smu_axi_xbar: sep_aperture=[0x" << std::hex
                     << sep_global_base_p_.get_value() << ",+0x" << sep_region_size_p_.get_value()
                     << ") smc_aperture=[0x" << smc_global_base_p_.get_value()
                     << ",+0x" << smc_region_size_p_.get_value() << ")");
    }

private:
    enum class master { sep, smc, ext };

    bool in_sep_aperture(uint64_t addr) const
    {
        const uint64_t base = sep_global_base_p_.get_value();
        return addr >= base && addr < base + sep_region_size_p_.get_value();
    }

    bool in_smc_aperture(uint64_t addr) const
    {
        const uint64_t base = smc_global_base_p_.get_value();
        return addr >= base && addr < base + smc_region_size_p_.get_value();
    }

    // Non-reflexive route resolution.  Returns the initiator socket to
    // forward on, or nullptr when the master has no legal route.
    tlm_utils::simple_initiator_socket<smu_axi_xbar, 64>*
    route(master m, uint64_t addr)
    {
        if (m == master::ext) {
            if (in_sep_aperture(addr)) return &sep_in;
            if (in_smc_aperture(addr)) return &smc_in;
            return nullptr;                        // no ext->ext route
        }
        if (m == master::sep) {
            if (in_smc_aperture(addr)) return &smc_in;
            return &ext_out;                       // static catch-all
        }
        // master::smc
        if (in_sep_aperture(addr)) return &sep_in;
        return &ext_out;                           // static catch-all
    }

    static const char* master_name(master m)
    {
        if (m == master::sep) return "sep_out";
        if (m == master::smc) return "smc_out";
        return "ext_in";
    }

    void forward(master m, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t addr = trans.get_address();
        auto* sock = route(m, addr);
        if (sock == nullptr) {
            SIM_LOG_DEBUG(this, master_name(m) << " decode miss addr=0x"
                          << std::hex << addr << " (no legal route)");
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        SIM_LOG_TRACE(this, master_name(m) << " addr=0x" << std::hex << addr
                      << " -> " << sock->name());
        (*sock)->b_transport(trans, delay);
    }

    unsigned forward_dbg(master m, tlm::tlm_generic_payload& trans)
    {
        auto* sock = route(m, trans.get_address());
        if (sock == nullptr) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }
        return (*sock)->transport_dbg(trans);
    }

    void bt_sep_out(tlm::tlm_generic_payload& t, sc_core::sc_time& d) { forward(master::sep, t, d); }
    void bt_smc_out(tlm::tlm_generic_payload& t, sc_core::sc_time& d) { forward(master::smc, t, d); }
    void bt_ext_in (tlm::tlm_generic_payload& t, sc_core::sc_time& d) { forward(master::ext, t, d); }

    unsigned dbg_sep_out(tlm::tlm_generic_payload& t) { return forward_dbg(master::sep, t); }
    unsigned dbg_smc_out(tlm::tlm_generic_payload& t) { return forward_dbg(master::smc, t); }
    unsigned dbg_ext_in (tlm::tlm_generic_payload& t) { return forward_dbg(master::ext, t); }
};

}  // namespace smu
