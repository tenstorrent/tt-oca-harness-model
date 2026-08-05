// SPDX-License-Identifier: Apache-2.0
/**
 * @file aou_core.cpp
 * @brief AOU_CORE LT implementation.
 */

#include "aou_core.h"

#include <cstring>

namespace aou {

namespace {

constexpr uint32_t IP_VERSION = 0x00010000u;

constexpr uint32_t INIT_ACTIVATE_START   = 1u << 0;
constexpr uint32_t INIT_DEACTIVATE_START = 1u << 1;
constexpr uint32_t INIT_STATE_ENABLED    = 1u << 2;
constexpr uint32_t INIT_STATE_DISABLED   = 1u << 3;
constexpr uint32_t INIT_TIMEOUT_MASK     = 0x7u << 4;
constexpr uint32_t INIT_INT_DEACTIVATE   = 1u << 7;
constexpr uint32_t INIT_INT_ACTIVATE     = 1u << 8;
constexpr uint32_t INIT_TR_COMPLETE      = (1u << 9) | (1u << 10);

/**
 * @brief RTL reset value for one register in a per-RP bank (offset 0x20+).
 * @param local_idx Index within the 6-register bank (0..5), i.e. off%6.
 * Only register 0 (`axi_split_tr`) resets non-zero (0x0F0F); the rest of
 * the bank (error-info, credit, timer regs) resets to 0.
 */
constexpr uint32_t rp_reset(unsigned local_idx)
{
    return local_idx == 0 ? 0x00000F0Fu : 0u;
}

} // namespace

/**
 * @brief Construct the model: apply CCI overrides, validate rp_count, seed
 * the per-RP register bank array with its RTL reset/mask values, size the
 * AXI socket vectors to rp_count, and register the APB/AXI b_transport
 * callbacks.
 */
aou_core::aou_core(sc_core::sc_module_name name, aou_cfg cfg)
    : sc_core::sc_module(name)
    , rp_count_p_("rp_count", cfg.rp_count, "AXI resource planes (1..4).")
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
                         "APB annotated delay (ns).")
    , bridge_delay_ns_p_("bridge_delay_ns", cfg.bridge_delay_ns,
                         "Extra delay on bridged AXI (ns).")
    , apb_socket("apb_socket")
    , axi_s("axi_s")
    , axi_m("axi_m")
    , fdi_active_i("fdi_active_i")
    , irq_o("irq_o")
    , cfg_(cfg)
{
    cfg_.rp_count        = rp_count_p_.get_value();
    cfg_.access_delay_ns = access_delay_ns_p_.get_value();
    cfg_.bridge_delay_ns = bridge_delay_ns_p_.get_value();

    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    bridge_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    if (cfg_.rp_count == 0 || cfg_.rp_count > aou_cfg::kMaxRp) {
        SC_REPORT_FATAL(this->name(), "aou_core rp_count must be in 1..4"); // GCOV_EXCL_LINE
    }

    for (unsigned i = 0; i < rp_csr_.size(); ++i) {
        const unsigned local = i % 6;
        const uint32_t rmask = (local == 0) ? 0x0000FFFFu
                             : (local == 1 || local == 5) ? 0x00000003u
                             : (local == 2) ? 0x00002001u
                             : 0xFFFFFFFFu;
        rp_csr_[i] = regmodel::Register32(rmask, rmask, rp_reset(local));
    }

    axi_s.init(cfg_.rp_count);
    axi_m.init(cfg_.rp_count);
    apb_socket.register_b_transport(this, &aou_core::b_transport_apb);
    for (unsigned i = 0; i < cfg_.rp_count; ++i) {
        axi_s[i].register_b_transport(this, &aou_core::b_transport_axi_s,
                                      static_cast<int>(i));
    }
    irq_o.initialize(false);

    SIM_LOG_INFO(this, "aou_core: rp_count=" << cfg_.rp_count);
}

/**
 * @brief Record the remote-die model this core bridges AXI traffic to/from.
 * Used by try_activate() (peer FDI check + auto-ack) and b_transport_axi_s()
 * (forwards to peer_->axi_m[dest]). Not required for CSR-only use.
 */
void aou_core::connect_peer(aou_core* peer) { peer_ = peer; }

/**
 * @brief Whether the FDI (UCIe die-to-die) link is currently up.
 * fdi_active_i must be bound by the integrator (e.g. tied high, or driven
 * by a link-bringup model); there is no unbound-port fallback here.
 */
bool aou_core::fdi_active() const
{
    return fdi_active_i.read();
}

/**
 * @brief Restore every CSR to its RTL reset value and drop back to
 * DISABLED, mirroring a write to aou_con0.aou_sw_reset (bit 4).
 */
void aou_core::soft_reset()
{
    aou_con0_.reset(0x00004008u);
    aou_interrupt_mask_.reset(0);
    lp_linkreset_.reset(0x00002400u);
    dest_rp_.reset(0x00003210u);
    prior_rp_axi_.reset(0x0A503210u);
    prior_timer_.reset(0x000F000Fu);
    for (unsigned i = 0; i < rp_csr_.size(); ++i)
        rp_csr_[i].reset(rp_reset(i % 6));
    enabled_ = activate_start_ = deactivate_start_ = false;
    int_activate_start_ = int_deactivate_start_ = false;
    deactivate_timeout_ = 0;
    update_irq();
}

/**
 * @brief Drive irq_o from the two sticky status bits it aggregates.
 * aou_interrupt_mask_ is intentionally not consulted: per aou-core.rdl,
 * neither int_activate_start nor int_deactivate_start is covered by any
 * mask bit in the real RTL either (the mask only gates linkreset/early-
 * resp/ID-mismatch sources, which this LT model doesn't implement).
 */
void aou_core::update_irq()
{
    irq_o.write(int_activate_start_ || int_deactivate_start_);
}

/**
 * @brief Attempt the DISABLED -> ENABLED transition after firmware writes
 * activate_start. Requires this core's and the peer's FDI link to be up.
 * LT simplification: rather than modeling the ACK message round trip, the
 * peer is auto-acked immediately (peer_activate_ack()) once its FDI is up.
 */
void aou_core::try_activate()
{
    if (!activate_start_ || !fdi_active() || enabled_) return;
    if (peer_ != nullptr) {
        if (!peer_->fdi_active()) return;
        // LT: peer auto-acks when its FDI is up (remote die ready).
        peer_->peer_activate_ack();
    }
    enabled_ = true;
    activate_start_ = false;
    int_activate_start_ = true;
    update_irq();
    SIM_LOG_INFO(this, "AOU ENABLED");
}

/**
 * @brief Called by the peer's try_activate() to bring this side to ENABLED
 * too, without this core needing its own activate_start written first —
 * models receiving the remote die's activation ACK/notification.
 */
void aou_core::peer_activate_ack()
{
    if (enabled_ || !fdi_active()) return;
    enabled_ = true;
    activate_start_ = false;
    int_activate_start_ = true;
    update_irq();
}

/**
 * @brief Move ENABLED -> DISABLED.
 * @param force True for aou_con0.deactivate_force (unconditional, bypasses
 * the enabled_/deactivate_start_ checks); false for the normal
 * aou_init.deactivate_start path (requires enabled_ and deactivate_start_).
 */
void aou_core::try_deactivate(bool force)
{
    if (!force && !deactivate_start_) return;
    if (!enabled_ && !force) return;
    enabled_ = false;
    activate_start_ = deactivate_start_ = false;
    int_deactivate_start_ = true;
    update_irq();
    SIM_LOG_INFO(this, "AOU DISABLED");
}

/**
 * @brief TLM target callback for the APB CSR window (apb_socket).
 * Validates a 4-byte, 4-byte-aligned access inside [0, kWindowSize), then
 * dispatches to reg_read()/reg_write(); annotates access_delay_ns.
 */
void aou_core::b_transport_apb(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    auto* ptr = gp.get_data_ptr();
    const unsigned len = gp.get_data_length();
    if (ptr == nullptr || len != 4 || (gp.get_address() & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const uint64_t off = gp.get_address();
    if (off >= aou_cfg::kWindowSize) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    uint32_t data = 0;
    bool ok = false;
    if (gp.is_read()) {
        ok = reg_read(off, data);
        if (ok) std::memcpy(ptr, &data, 4);
    } else if (gp.is_write()) {
        std::memcpy(&data, ptr, 4);
        ok = reg_write(off, data);
    }
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
}

/**
 * @brief TLM target callback for RP @p id's inbound AXI traffic (axi_s[id]).
 * Rejects the transaction (TLM_COMMAND_ERROR_RESPONSE) unless both this
 * core and the connected peer are ENABLED. Otherwise looks up the
 * destination RP from dest_rp_ and forwards the same payload straight to
 * peer_->axi_m[dest] — this is a routing hop, not FDI flit packing; the
 * actual UCIe transport is elided per this model's LT scope.
 */
void aou_core::b_transport_axi_s(int id, tlm::tlm_generic_payload& gp,
                                 sc_core::sc_time& delay)
{
    const unsigned rp = static_cast<unsigned>(id);
    if (!enabled_ || peer_ == nullptr || !peer_->enabled_) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    const unsigned dest = (dest_rp_.raw() >> (rp * 4)) & 0x3u;
    if (dest >= peer_->cfg_.rp_count) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    delay += sc_core::sc_time(bridge_delay_ns_p_.get_value(), sc_core::SC_NS);
    peer_->axi_m[dest]->b_transport(gp, delay);
}

/**
 * @brief Read-side CSR decode.
 * Global registers (ip_version, aou_con0, aou_interrupt_mask, lp_linkreset,
 * dest_rp, prior_rp_axi, prior_timer) read straight from their Register32
 * storage; aou_init is synthesized from internal activation/interrupt
 * state rather than stored; offsets in [OFF_RP0_BASE, kWindowSize) index
 * into the per-RP bank array. Returns false (decode miss) for anything
 * else.
 */
bool aou_core::reg_read(uint64_t off, uint32_t& data) const
{
    switch (off) {
    case aou_cfg::OFF_IP_VERSION:         data = IP_VERSION; return true;
    case aou_cfg::OFF_AOU_CON0:           data = aou_con0_.read(); return true;
    case aou_cfg::OFF_AOU_INTERRUPT_MASK: data = aou_interrupt_mask_.read(); return true;
    case aou_cfg::OFF_LP_LINKRESET:       data = lp_linkreset_.read(); return true;
    case aou_cfg::OFF_DEST_RP:            data = dest_rp_.read(); return true;
    case aou_cfg::OFF_PRIOR_RP_AXI:       data = prior_rp_axi_.read(); return true;
    case aou_cfg::OFF_PRIOR_TIMER:        data = prior_timer_.read(); return true;
    case aou_cfg::OFF_AOU_INIT: {
        data = INIT_TR_COMPLETE;
        if (activate_start_) data |= INIT_ACTIVATE_START;
        if (deactivate_start_) data |= INIT_DEACTIVATE_START;
        if (enabled_) data |= INIT_STATE_ENABLED;
        else data |= INIT_STATE_DISABLED;
        data |= (uint32_t(deactivate_timeout_) << 4) & INIT_TIMEOUT_MASK;
        if (int_deactivate_start_) data |= INIT_INT_DEACTIVATE;
        if (int_activate_start_) data |= INIT_INT_ACTIVATE;
        return true;
    }
    default:
        break;
    }
    if (off >= aou_cfg::OFF_RP0_BASE && off < aou_cfg::kWindowSize) {
        const unsigned idx =
            static_cast<unsigned>((off - aou_cfg::OFF_RP0_BASE) / 4);
        if (idx < rp_csr_.size()) {
            data = rp_csr_[idx].read();
            return true;
        }
    }
    return false; // GCOV_EXCL_LINE -- unreachable: [0, kWindowSize) is fully
                  // covered by the named cases above plus the RP-bank range,
                  // given OFF_RP0_BASE/kWindowSize/rp_csr_.size() agree.
}

/**
 * @brief Write-side CSR decode, mirroring reg_read()'s offset map.
 * aou_con0 triggers soft_reset() (bit4) and/or try_deactivate(true) (bit0,
 * deactivate_force) as side effects; aou_init drives the activation FSM
 * (activate_start/deactivate_start set, int_* W1C via the two INT_INT_*
 * bits) through try_activate()/try_deactivate(false); every other
 * register (including the per-RP bank) is plain masked storage via
 * Register32::write(). Returns false (decode miss) for anything else.
 */
bool aou_core::reg_write(uint64_t off, uint32_t data)
{
    switch (off) {
    case aou_cfg::OFF_IP_VERSION:
        return true;
    case aou_cfg::OFF_AOU_CON0: {
        const bool force = (data & 1u) != 0;
        aou_con0_.write(data);
        if (aou_con0_.raw() & (1u << 4)) soft_reset();
        if (force) try_deactivate(true);
        return true;
    }
    case aou_cfg::OFF_AOU_INIT:
        if (data & INIT_ACTIVATE_START) activate_start_ = true;
        if (data & INIT_DEACTIVATE_START) deactivate_start_ = true;
        if (data & INIT_INT_ACTIVATE) int_activate_start_ = false;
        if (data & INIT_INT_DEACTIVATE) int_deactivate_start_ = false;
        deactivate_timeout_ =
            static_cast<uint8_t>((data & INIT_TIMEOUT_MASK) >> 4);
        try_activate();
        try_deactivate(false);
        update_irq();
        return true;
    case aou_cfg::OFF_AOU_INTERRUPT_MASK:
        aou_interrupt_mask_.write(data);
        return true;
    case aou_cfg::OFF_LP_LINKRESET:
        lp_linkreset_.write(data);
        return true;
    case aou_cfg::OFF_DEST_RP:
        dest_rp_.write(data);
        return true;
    case aou_cfg::OFF_PRIOR_RP_AXI:
        prior_rp_axi_.write(data);
        return true;
    case aou_cfg::OFF_PRIOR_TIMER:
        prior_timer_.write(data);
        return true;
    default:
        break;
    }
    if (off >= aou_cfg::OFF_RP0_BASE && off < aou_cfg::kWindowSize) {
        const unsigned idx =
            static_cast<unsigned>((off - aou_cfg::OFF_RP0_BASE) / 4);
        if (idx < rp_csr_.size()) {
            rp_csr_[idx].write(data);
            return true;
        }
    }
    return false; // GCOV_EXCL_LINE -- unreachable, see reg_read()'s mirror-image note.
}

} // namespace aou
