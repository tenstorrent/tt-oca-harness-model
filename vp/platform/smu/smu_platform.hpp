// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smu/smu_platform.hpp
//
// SMU on-die composition of the SMC and SEP virtual platforms plus the
// SMU AXI interconnect (RTL: hw/smu/rtl/smu.sv). Implementation lives in
// src/smu_platform.cpp — same split as smc_platform / sep_platform.
//
// This type is not an sc_module. The two subsystem platforms stay
// top-level (`dut`, `och_sep_ss1`) so standalone smc-vp / sep-vp CCI
// inis apply unchanged.
// ===========================================================================

#pragma once

#include "smc_platform.hpp"
#include "sep_platform.hpp"

#include "inc/smu_axi_xbar.h"
#include "inc/axi_window_remap.h"

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

namespace smu {

class idle_initiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<idle_initiator, 64> sock{"sock"};
    explicit idle_initiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {}
};

class smu_platform
{
public:
    // RTL SEP_SMC_REGION_BASE / _SIZE: the SEP's dedicated view of the SMC.
    static constexpr uint64_t SEP_SMC_REGION_BASE = 0x4000'0000ULL;
    static constexpr uint64_t SEP_SMC_REGION_SIZE = 0x4000'0000ULL;  // 1 GiB

    smc::smc_platform         dut;
    och_sep_ss                sep;
    smu_axi_xbar              xbar;
    axi_window_remap<64, 64>  sep2smc_remap;
    idle_initiator            idle_jtag;

    explicit smu_platform(const char* smc_name = "dut",
                          const char* sep_name = "och_sep_ss1");
};

}  // namespace smu
