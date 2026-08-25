// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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

    // Lock modelling: a REG_UPDATE strobe commits the shadow config and makes
    // the firmware-polled pll_cntl status report lock.  Firmware polls
    // pll_cntl.CGM_x_STATUS / AWM_x_STATUS (not the sub-block's own status),
    // so the coordination lives here in the composing wrapper.
    cgm0_.observe_write(cgm::OFF_REG_UPDATE,
                        [this](uint32_t w) { commit_cgm_lock(0, w); });
    cgm1_.observe_write(cgm::OFF_REG_UPDATE,
                        [this](uint32_t w) { commit_cgm_lock(1, w); });
    awm0_.observe_write(awm::GLOBAL_REG_UPDATE,
                        [this](uint32_t w) { commit_awm_lock(0, w); });
    awm1_.observe_write(awm::GLOBAL_REG_UPDATE,
                        [this](uint32_t w) { commit_awm_lock(1, w); });

    SIM_LOG_INFO(this,
                 "pll_wrapper instantiated: base=0x"
                     << std::hex << cfg_.base_addr << " window=0x"
                     << cfg_.WINDOW_SIZE << std::dec << " sub-blocks=5");
}

void pll_wrapper::commit_cgm_lock(unsigned idx, uint32_t written)
{
    // Only a reg_update strobe (bit 0) commits shadow config + re-locks.
    if ((written & 0x1u) == 0u) return;

    cgm& c = (idx == 0) ? cgm0_ : cgm1_;

    // Lock follows cgm_enable (ENABLES[0]); a disable + REG_UPDATE drops lock.
    uint32_t enables = 0;
    c.peek(cgm::OFF_ENABLES, enables);
    const bool locked = (enables & 0x1u) != 0u;

    // pll_cntl.CGM_x_STATUS.lock_detect[0] — the bit firmware polls.
    const uint64_t st_off =
        (idx == 0) ? pll_cntl::OFF_CGM_0_STATUS : pll_cntl::OFF_CGM_1_STATUS;
    uint32_t st = 0;
    cntl_.peek(st_off, st);
    st = locked ? (st | 0x1u) : (st & ~0x1u);
    cntl_.poke(st_off, st);

    // Mirror the sub-block's own CGM_STATUS.lock_detect[0] for read accuracy.
    uint32_t cst = 0;
    c.peek(cgm::OFF_CGM_STATUS, cst);
    cst = locked ? (cst | 0x1u) : (cst & ~0x1u);
    c.poke(cgm::OFF_CGM_STATUS, cst);

    SIM_LOG_INFO(this, "CGM" << idx << " REG_UPDATE: lock_detect="
                             << (locked ? 1 : 0));
}

void pll_wrapper::commit_awm_lock(unsigned idx, uint32_t written)
{
    if ((written & 0x1u) == 0u) return;  // reg_update strobe

    // Firmware exit conditions: AWM_0 lock_detect == 7 (all three CGMs used
    // and locked), AWM_1 lock_detect == 1.
    const uint32_t lockval = (idx == 0) ? 0x7u : 0x1u;

    const uint64_t st_off =
        (idx == 0) ? pll_cntl::OFF_AWM_0_STATUS : pll_cntl::OFF_AWM_1_STATUS;
    uint32_t st = 0;
    cntl_.peek(st_off, st);
    st = (st & ~0x7u) | lockval;  // lock_detect[2:0]
    cntl_.poke(st_off, st);

    // Mirror awm GLOBAL LOCK_STATUS.lock_detect[5:3] for read accuracy.
    awm& a = (idx == 0) ? awm0_ : awm1_;
    uint32_t gls = 0;
    a.peek(awm::GLOBAL_LOCK_STATUS, gls);
    gls = (gls & ~(0x7u << 3)) | (lockval << 3);
    a.poke(awm::GLOBAL_LOCK_STATUS, gls);

    SIM_LOG_INFO(this, "AWM" << idx << " REG_UPDATE: lock_detect=0x"
                             << std::hex << lockval << std::dec);
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
