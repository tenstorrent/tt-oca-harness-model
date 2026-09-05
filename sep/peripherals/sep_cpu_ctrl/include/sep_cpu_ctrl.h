// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_cpu_ctrl_base.h"
#include "reg_logger.h"
#include "reg_param.h"
#include <systemc.h>

// -----------------------------------------------------------------------------
// hwif_in  — hardware-driven inputs; platform populates before sc_start()
// -----------------------------------------------------------------------------
struct SepCpuCtrlHwifIn {
    // REFERENCE_COUNTER
    uint64_t reference_counter_rc          = 0;

    // TIMEOUT_INTERRUPT
    bool sys_in_timeout_int                = false;
    bool dma_data_timeout_int              = false;
    bool alias_remap_timeout_int           = false;
    bool filter_out_timeout_int            = false;
    bool entropy_read_timeout_int          = false;
    bool entropy_write_timeout_int         = false;
    bool inbound_mailbox_timeout_int       = false;
    bool outbound_mailbox_timeout_int      = false;

    // SEP_TEST_CTRL
    bool fast_spi_en                       = false;
    bool fast_iccm_en                      = false;
    bool fast_dccm_en                      = false;
    bool fast_sram_en                      = false;
    bool fast_pka_en                       = false;
    bool sep_standalone                    = false;

    // Fuse sense — set to true before sc_start() to unblock boot
    bool smc_fuse_sense_done               = false;
    bool sep_fuse_sense_done               = false;

    // SEP_STRAPS
    bool test_en                           = false;
    bool bypass_mem_repair                 = false;
};

// -----------------------------------------------------------------------------
// sep_cpu_ctrl_ip  — regmodel _ip layer
// -----------------------------------------------------------------------------
class sep_cpu_ctrl_ip : public sep_cpu_ctrl_base {
public:
    SC_HAS_PROCESS(sep_cpu_ctrl_ip);
    typedef typename regmodel::Reg<64>::DT DT;

    sc_core::sc_in<bool>      rst_ni{"rst_ni"};
    sc_core::sc_out<uint32_t> nmi_vec_o{"nmi_vec_o"};

    // Inbound-window exports. sep_system_csr.sv drives these as continuous
    // assigns off the cpu_ctrl hwif (sep_global_base_addr_o / sep_region_size_o)
    // and sep.sv forwards them to the SMU interconnect, which sizes its SEP
    // aperture from them. The CSR pair is therefore the single source of truth
    // for the window on both sides of the boundary: the SEP-side inbound remap
    // reads the registers directly, and everything upstream follows these ports.
    // addr is [55:0] and size is [31:0]; both are published zero-extended.
    sc_core::sc_out<uint64_t> sep_global_base_addr_o{"sep_global_base_addr_o"};
    sc_core::sc_out<uint64_t> sep_region_size_o{"sep_region_size_o"};

    // Request a republish of both exports from the current CSR contents. The
    // RTL assign is continuous, so anything that changes either register out of
    // band — a platform seeding the window in place of firmware, for instance —
    // must call this; ordinary firmware writes request it from their write
    // callbacks. The write itself happens one delta later in
    // publish_window_process, which is the only driver of the two ports: having
    // the callers write directly would make every one of them a driver and
    // SystemC rejects two processes writing one signal in the same delta.
    void publish_inbound_window();

    SepCpuCtrlHwifIn hwif_in;

    // -------------------------------------------------------------------------
    // CCI parameters — static straps/fuse flags; set via .ini, no recompile
    // -------------------------------------------------------------------------
    regmodel::Param<uint32_t> smc_fuse_sense_done;
    regmodel::Param<uint32_t> sep_fuse_sense_done;
    regmodel::Param<uint32_t> sep_standalone;
    regmodel::Param<uint32_t> fast_spi_en;
    regmodel::Param<uint32_t> fast_iccm_en;
    regmodel::Param<uint32_t> fast_dccm_en;
    regmodel::Param<uint32_t> fast_sram_en;
    regmodel::Param<uint32_t> fast_pka_en;
    regmodel::Param<uint32_t> test_en;
    regmodel::Param<uint32_t> bypass_mem_repair;

    explicit sep_cpu_ctrl_ip(sc_core::sc_module_name n);
    void end_of_elaboration() override;

private:
    // Internal state for registers the base cannot express
    uint32_t fs_nmi_vec_               = 0x60000080; // addr = fs_nmi_vec_ << 1
    bool     fs_nmi_vec_lock_          = false;
    uint8_t  fs_ext_trng_src_sel_      = 0x7;
    bool     fs_ext_trng_src_sel_lock_ = false;

    void register_callbacks();
    void reset_handler();

    // Sole driver of sep_global_base_addr_o / sep_region_size_o.
    void publish_window_process();
    sc_core::sc_event window_changed_;

    // -------------------------------------------------------------------------
    // Write callback handlers  (DT = regmodel::Memory<64> word type = unsigned long long)
    // -------------------------------------------------------------------------
    bool handle_write_SEP_NMI_VEC(DT value, DT write_bit_mask);
    bool handle_write_SEP_NMI_VEC_LOCK(DT value, DT write_bit_mask);
    bool handle_write_EXT_TRNG_SRC_SEL(DT value, DT write_bit_mask);
    bool handle_write_EXT_TRNG_SRC_SEL_LOCK(DT value, DT write_bit_mask);
    bool handle_write_TIMEOUT_CLEAR(DT value, DT write_bit_mask);
    bool handle_write_REFERENCE_COUNTER(DT value, DT write_bit_mask);

    // Post-write hook, not a write callback: registering a write callback for an
    // offset *replaces* regmodel::Reg's own handle_write, which is what performs the
    // masked store, so a plain-storage register would stop storing. The
    // post-write hook runs after that store instead of in place of it.
    bool post_write_inbound_window();

    // -------------------------------------------------------------------------
    // Read callback handlers
    // -------------------------------------------------------------------------
    bool handle_read_REFERENCE_COUNTER(DT& value, DT read_bit_mask);
    bool handle_read_TIMEOUT_INTERRUPT(DT& value, DT read_bit_mask);
    bool handle_read_SEP_TEST_CTRL(DT& value, DT read_bit_mask);
    bool handle_read_SMC_FUSE_SENSE_STATUS(DT& value, DT read_bit_mask);
    bool handle_read_SEP_FUSE_SENSE_STATUS(DT& value, DT read_bit_mask);
    bool handle_read_SEP_STRAPS(DT& value, DT read_bit_mask);
    bool handle_read_SEP_NMI_VEC(DT& value, DT read_bit_mask);
    bool handle_read_SEP_NMI_VEC_LOCK(DT& value, DT read_bit_mask);
    bool handle_read_EXT_TRNG_SRC_SEL(DT& value, DT read_bit_mask);
    bool handle_read_EXT_TRNG_SRC_SEL_LOCK(DT& value, DT read_bit_mask);
    bool handle_read_SEP_VERSION_ID(DT& value, DT read_bit_mask);
};
