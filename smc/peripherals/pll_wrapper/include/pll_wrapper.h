// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file pll_wrapper.h
 * @brief SystemC/TLM-2.0 model of the SMC PLL wrapper (pll_wrap.rdl).
 *
 * Composes the three sub-models exactly as pll_wrap.rdl instantiates them:
 *
 *   pll_cntl   pll_cntl   @0x000
 *   cgm        cgm_0      @0x100
 *   cgm        cgm_1      @0x200
 *   awm_wrap   awm_0      @0x400   (awm_wrap == awm @0x0)
 *   awm_wrap   awm_1      @0xA00
 *
 * The wrapper presents a single 32-bit register target and decodes each
 * incoming access to the owning sub-block's window, rebasing the address to
 * the block-local offset and forwarding over an internal initiator socket
 * (the same pattern the SMC platform fabric uses for its peripherals).
 */

#ifndef SMC_PLL_WRAPPER_H_
#define SMC_PLL_WRAPPER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <iostream>

#include "awm.h"
#include "cgm.h"
#include "pll_cntl.h"

namespace smc {
namespace pll {

struct pll_wrapper_cfg {
    /// Total window: awm_1 @0xA00 + awm window (0x500) => 0xF00, rounded 0x1000.
    static constexpr uint64_t WINDOW_SIZE = 0x1000ULL;

    uint64_t base_addr       = 0x0ULL;
    double   access_delay_ns = 1.0;
};

/// Composed PLL wrapper register model (from pll_wrap.rdl).
class pll_wrapper : public sc_core::sc_module {
public:
    // Sub-block base offsets (pll_wrap.rdl).
    static constexpr uint64_t OFF_PLL_CNTL = 0x000ULL;
    static constexpr uint64_t OFF_CGM_0    = 0x100ULL;
    static constexpr uint64_t OFF_CGM_1    = 0x200ULL;
    static constexpr uint64_t OFF_AWM_0    = 0x400ULL;
    static constexpr uint64_t OFF_AWM_1    = 0xA00ULL;

    tlm_utils::simple_target_socket<pll_wrapper> reg_socket;
    sc_core::sc_in<bool>                          rst_n_i;

    SC_HAS_PROCESS(pll_wrapper);

    explicit pll_wrapper(sc_core::sc_module_name name,
                         pll_wrapper_cfg cfg = pll_wrapper_cfg{});

    // Sub-model accessors (test / debug back door).
    pll_cntl& cntl()  { return cntl_; }
    cgm&      cgm_0() { return cgm0_; }
    cgm&      cgm_1() { return cgm1_; }
    awm&      awm_0() { return awm0_; }
    awm&      awm_1() { return awm1_; }

    uint64_t window_size() const { return cfg_.WINDOW_SIZE; }
    uint64_t base_addr()   const { return cfg_.base_addr; }

    void dump_state(std::ostream& os = std::cout) const;

private:
    struct sub {
        uint64_t                                       base;
        uint64_t                                       size;
        tlm_utils::simple_initiator_socket<pll_wrapper>* init;
        const char*                                    name;
    };

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);

    // Lock modelling (driven by REG_UPDATE writes; see pll_wrapper.cpp).
    // A CGM/AWM shadow-commit (REG_UPDATE strobe) makes the firmware-polled
    // pll_cntl status register report lock, so programming sequences that
    // `while (lock_detect != N)` make forward progress.
    void commit_cgm_lock(unsigned idx, uint32_t written);
    void commit_awm_lock(unsigned idx, uint32_t written);

    pll_wrapper_cfg cfg_;

    pll_cntl cntl_;
    cgm      cgm0_;
    cgm      cgm1_;
    awm      awm0_;
    awm      awm1_;

    tlm_utils::simple_initiator_socket<pll_wrapper> init_cntl_;
    tlm_utils::simple_initiator_socket<pll_wrapper> init_cgm0_;
    tlm_utils::simple_initiator_socket<pll_wrapper> init_cgm1_;
    tlm_utils::simple_initiator_socket<pll_wrapper> init_awm0_;
    tlm_utils::simple_initiator_socket<pll_wrapper> init_awm1_;

    std::array<sub, 5> subs_;
};

}  // namespace pll
}  // namespace smc

#endif  // SMC_PLL_WRAPPER_H_
