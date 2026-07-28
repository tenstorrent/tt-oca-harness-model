// SPDX-License-Identifier: Apache-2.0
/**
 * @file pll_wrapper.cpp
 * @brief PLL wrapper composition + address decode (from pll_wrap.rdl).
 */

#include "pll_wrapper.h"

#include "sim_log.h"

namespace smc {
namespace pll {

namespace {

// Build a sub-model cfg carrying the composed (absolute) base for nicer logs.
template <typename Cfg>
Cfg child_cfg(uint64_t wrapper_base, uint64_t offset, double delay)
{
    Cfg c;
    c.base_addr       = wrapper_base + offset;
    c.access_delay_ns = delay;
    return c;
}

}  // namespace

pll_wrapper::pll_wrapper(sc_core::sc_module_name name, pll_wrapper_cfg cfg)
    : sc_core::sc_module(name)
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , cfg_(cfg)
    , cntl_("pll_cntl",
            child_cfg<pll_cntl_cfg>(cfg.base_addr, OFF_PLL_CNTL,
                                    cfg.access_delay_ns))
    , cgm0_("cgm_0",
            child_cfg<cgm_cfg>(cfg.base_addr, OFF_CGM_0, cfg.access_delay_ns))
    , cgm1_("cgm_1",
            child_cfg<cgm_cfg>(cfg.base_addr, OFF_CGM_1, cfg.access_delay_ns))
    , awm0_("awm_0",
            child_cfg<awm_cfg>(cfg.base_addr, OFF_AWM_0, cfg.access_delay_ns))
    , awm1_("awm_1",
            child_cfg<awm_cfg>(cfg.base_addr, OFF_AWM_1, cfg.access_delay_ns))
    , init_cntl_("init_cntl")
    , init_cgm0_("init_cgm0")
    , init_cgm1_("init_cgm1")
    , init_awm0_("init_awm0")
    , init_awm1_("init_awm1")
{
    reg_socket.register_b_transport(this, &pll_wrapper::b_transport);

    // Internal fabric: rebased forwards to each sub-block's register target.
    init_cntl_.bind(cntl_.reg_socket);
    init_cgm0_.bind(cgm0_.reg_socket);
    init_cgm1_.bind(cgm1_.reg_socket);
    init_awm0_.bind(awm0_.reg_socket);
    init_awm1_.bind(awm1_.reg_socket);

    // Fan the wrapper reset out to every sub-block (parent->child port bind).
    cntl_.rst_n_i(rst_n_i);
    cgm0_.rst_n_i(rst_n_i);
    cgm1_.rst_n_i(rst_n_i);
    awm0_.rst_n_i(rst_n_i);
    awm1_.rst_n_i(rst_n_i);

    subs_ = {{
        {OFF_PLL_CNTL, pll_cntl_cfg::WINDOW_SIZE, &init_cntl_, "pll_cntl"},
        {OFF_CGM_0,    cgm_cfg::WINDOW_SIZE,      &init_cgm0_, "cgm_0"},
        {OFF_CGM_1,    cgm_cfg::WINDOW_SIZE,      &init_cgm1_, "cgm_1"},
        {OFF_AWM_0,    awm_cfg::WINDOW_SIZE,      &init_awm0_, "awm_0"},
        {OFF_AWM_1,    awm_cfg::WINDOW_SIZE,      &init_awm1_, "awm_1"},
    }};

    SIM_LOG_INFO(this,
                 "pll_wrapper instantiated: base=0x"
                     << std::hex << cfg_.base_addr << " window=0x"
                     << cfg_.WINDOW_SIZE << std::dec << " sub-blocks=5");
}

void pll_wrapper::b_transport(tlm::tlm_generic_payload& gp,
                              sc_core::sc_time& delay)
{
    const uint64_t adr = gp.get_address();

    for (const sub& s : subs_) {
        if (adr >= s.base && adr < s.base + s.size) {
            // Rebase to the block-local offset, forward, then restore address.
            gp.set_address(adr - s.base);
            (*s.init)->b_transport(gp, delay);
            gp.set_address(adr);
            SIM_LOG_TRACE(this,
                          "route off=0x" << std::hex << adr << " -> " << s.name
                                         << " local=0x" << (adr - s.base)
                                         << std::dec);
            return;
        }
    }

    // Reserved gap between sub-blocks (or past the window).
    gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    SIM_LOG_DEBUG(this,
                  "decode miss at off=0x" << std::hex << adr << std::dec);
}

void pll_wrapper::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] pll_wrapper composed state:\n";
    cntl_.dump_state(os);
    cgm0_.dump_state(os);
    cgm1_.dump_state(os);
    awm0_.dump_state(os);
    awm1_.dump_state(os);
}

}  // namespace pll
}  // namespace smc
