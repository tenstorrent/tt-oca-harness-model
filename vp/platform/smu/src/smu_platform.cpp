// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smu/src/smu_platform.cpp
//
// Topology (RTL: hw/smu/rtl/smu.sv):
//
//   SEP (och_sep_ss1)                       SMC (dut)
//   sep_ext_to_smc_axi --[axi_window_remap]--> sep_axi_in      (dedicated)
//   sep_smn_inbound_axi <---[smu_axi_xbar]----- output_axi     (crossbar)
//   sep_smn_outbound_axi -->[smu_axi_xbar]----> sys_axi_in / ext_out
//
//   ext_out / ext_in bind through the local AOU (AXI-over-UCIe LT stub).
//
//   Interrupts: sep.smc_mailbox_interrupt_o -> dut.sep_mailbox_interrupts_i.
// ===========================================================================

#include "smu_platform.hpp"

namespace smu {

smu_platform::smu_platform(const char* smc_name, const char* sep_name)
    : dut(smc_name)
    , sep(sep_name)
    , xbar("smu_xbar")
    , sep2smc_remap("sep2smc_remap",
                    SEP_SMC_REGION_BASE, SEP_SMC_REGION_SIZE, 0x0ULL)
    , idle_jtag("idle_jtag")
{
    // Dedicated SEP -> SMC path.
    sep.sep_ext_to_smc_axi.bind(sep2smc_remap.tgt);
    sep2smc_remap.init.bind(dut.sep_axi_in);

    // Crossbar paths.
    dut.output_axi.bind(xbar.smc_out);          // SMC outbound -> xbar
    xbar.smc_in.bind(dut.sys_axi_in);           // xbar -> SMC inbound
    xbar.sep_in.bind(sep.sep_smn_inbound_axi);  // xbar -> SEP inbound

    // The crossbar sizes its SEP aperture from the SEP's own window CSRs
    // (sep.sv: sep_global_base_addr_o / sep_region_size_o).
    xbar.sep_global_base_addr_i(sep.sep_global_base_addr_signal);
    xbar.sep_region_size_i(sep.sep_region_size_signal);

    // SEP outbound (RTL smn_outbound_axi).
    sep.sep_smn_outbound_axi.bind(xbar.sep_out);

    // Unused SMC inbound ports.
    idle_jtag.sock.bind(dut.jtag_axi_in);

    // Chiplet-facing boundary (RTL smu_axi_in/out) through local AOU.
    xbar.ext_out.bind(dut.aou_axi_s);
    dut.aou_axi_m.bind(xbar.ext_in);

    // SEP inbound mailbox interrupts -> SMC (RTL smu.sv: sep.smc_mailbox_interrupt_o
    // -> dut.sep_mailbox_interrupts_i).  The SMC lands them on peripheral bits
    // 7:0, i.e. PLIC source IDs 257..264.
    for (unsigned m = 0; m < smc::smc_platform::NUM_SEP_MAILBOX; ++m)
        dut.sep_mailbox_irq_i[m](sep.mbox_inbound_irq_signal[m]);
}

}  // namespace smu
