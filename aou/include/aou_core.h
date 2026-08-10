// SPDX-License-Identifier: Apache-2.0
/**
 * @file aou_core.h
 * @brief SystemC/TLM-2.0 LT model of AOU_CORE (AXI-over-UCIe).
 *
 * LT scope: APB CSR map (aou-core.rdl), activate/deactivate, AXI slave→peer
 * master bridge. No FDI flit/credit/QoS timing.
 */

#ifndef AOU_CORE_H_
#define AOU_CORE_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>

#include <cci_configuration>

#include "reg_access.h"
#include "sim_log.h"

namespace aou {

struct aou_cfg {
    unsigned rp_count        = 1;
    double   access_delay_ns = 1.0;
    double   bridge_delay_ns = 0.0;

    static constexpr unsigned kMaxRp      = 4;
    static constexpr uint64_t kWindowSize = 0x80;

    static constexpr uint64_t OFF_IP_VERSION         = 0x00;
    static constexpr uint64_t OFF_AOU_CON0           = 0x04;
    static constexpr uint64_t OFF_AOU_INIT           = 0x08;
    static constexpr uint64_t OFF_AOU_INTERRUPT_MASK = 0x0C;
    static constexpr uint64_t OFF_LP_LINKRESET       = 0x10;
    static constexpr uint64_t OFF_DEST_RP            = 0x14;
    static constexpr uint64_t OFF_PRIOR_RP_AXI       = 0x18;
    static constexpr uint64_t OFF_PRIOR_TIMER        = 0x1C;
    static constexpr uint64_t OFF_RP0_BASE           = 0x20;
    static constexpr uint64_t RP_STRIDE              = 0x18;
};

class aou_core : public sc_core::sc_module {
public:
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> rp_count_p_;
    cci::cci_param<double> access_delay_ns_p_;
    cci::cci_param<double> bridge_delay_ns_p_;

    tlm_utils::simple_target_socket<aou_core> apb_socket;
    sc_core::sc_vector<tlm_utils::simple_target_socket_tagged<aou_core, 64>> axi_s;
    sc_core::sc_vector<tlm_utils::simple_initiator_socket<aou_core, 64>> axi_m;

    sc_core::sc_in<bool>  fdi_active_i;
    sc_core::sc_out<bool> irq_o;

    explicit aou_core(sc_core::sc_module_name name, aou_cfg cfg = {});

    void connect_peer(aou_core* peer);
    bool is_enabled() const { return enabled_; }

protected:
    void b_transport_apb(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    void b_transport_axi_s(int id, tlm::tlm_generic_payload& gp,
                           sc_core::sc_time& delay);

    bool reg_read(uint64_t off, uint32_t& data) const;
    bool reg_write(uint64_t off, uint32_t data);
    void soft_reset();
    void update_irq();
    void try_activate();
    void try_deactivate(bool force);
    void peer_activate_ack();

    bool fdi_active() const;

    aou_cfg   cfg_;
    aou_core* peer_ = nullptr;

    bool enabled_              = false;
    bool activate_start_       = false;
    bool deactivate_start_     = false;
    bool int_activate_start_   = false;
    bool int_deactivate_start_ = false;
    uint8_t deactivate_timeout_ = 0;

    // Plain CSR storage (reset values from aou-core.rdl). Soft features that
    // have no LT side-effect are still R/W-visible for firmware.
    regmodel::Register32 aou_con0_{0x0FFFF81Cu, 0x0FFFF81Du, 0x00004008u};
    regmodel::Register32 aou_interrupt_mask_{0x000001FCu, 0x000001FCu, 0};
    regmodel::Register32 lp_linkreset_{0x00003FFFu, 0x00003FFFu, 0x00002400u};
    regmodel::Register32 dest_rp_{0x00003333u, 0x00003333u, 0x00003210u};
    regmodel::Register32 prior_rp_axi_{0x0FF3FFF3u, 0x0FF3FFF3u, 0x0A503210u};
    regmodel::Register32 prior_timer_{0xFFFFFFFFu, 0xFFFFFFFFu, 0x000F000Fu};
    std::array<regmodel::Register32, 24> rp_csr_{}; // 4 RPs × 6 regs
};

} // namespace aou

#endif // AOU_CORE_H_
