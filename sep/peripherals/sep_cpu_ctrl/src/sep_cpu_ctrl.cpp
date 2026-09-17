// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_cpu_ctrl.h"
#include <functional>

sep_cpu_ctrl_ip::sep_cpu_ctrl_ip(sc_core::sc_module_name n)
    : sep_cpu_ctrl_base(n, "sep_cpu_ctrl", 0x1008)
    , smc_fuse_sense_done("smc_fuse_sense_done", 0u)
    , sep_fuse_sense_done("sep_fuse_sense_done", 0u)
    , sep_standalone("sep_standalone", 0u)
    , fast_spi_en("fast_spi_en", 0u)
    , fast_iccm_en("fast_iccm_en", 0u)
    , fast_dccm_en("fast_dccm_en", 0u)
    , fast_sram_en("fast_sram_en", 0u)
    , fast_pka_en("fast_pka_en", 0u)
    , test_en("test_en", 0u)
    , bypass_mem_repair("bypass_mem_repair", 0u)
{
    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    // Runs at time 0 so the exports carry the CSR reset values before any
    // traffic, then on every republish request.
    SC_METHOD(publish_window_process);
    sensitive << window_changed_;

    register_callbacks();
}

void sep_cpu_ctrl_ip::end_of_elaboration()
{
    hwif_in.smc_fuse_sense_done = bool(smc_fuse_sense_done.get_param_value());
    hwif_in.sep_fuse_sense_done = bool(sep_fuse_sense_done.get_param_value());
    hwif_in.sep_standalone      = bool(sep_standalone.get_param_value());
    hwif_in.fast_spi_en         = bool(fast_spi_en.get_param_value());
    hwif_in.fast_iccm_en        = bool(fast_iccm_en.get_param_value());
    hwif_in.fast_dccm_en        = bool(fast_dccm_en.get_param_value());
    hwif_in.fast_sram_en        = bool(fast_sram_en.get_param_value());
    hwif_in.fast_pka_en         = bool(fast_pka_en.get_param_value());
    hwif_in.test_en             = bool(test_en.get_param_value());
    hwif_in.bypass_mem_repair   = bool(bypass_mem_repair.get_param_value());
}

void sep_cpu_ctrl_ip::reset_handler()
{
    if (!rst_ni.read()) {
        reset_all_registers();
        hwif_in.reference_counter_rc = 0;  // free-running counter restarts at 0 on cold reset
        fs_nmi_vec_               = 0x60000080;
        fs_nmi_vec_lock_          = false;
        fs_ext_trng_src_sel_      = 0x7;
        fs_ext_trng_src_sel_lock_ = false;
        nmi_vec_o.write(static_cast<uint32_t>(fs_nmi_vec_) << 1);
        publish_inbound_window();
    }
}

// =============================================================================
// Inbound-window exports — mirror sep_system_csr.sv's continuous assigns
//
// RTL drives sep_global_base_addr_o/sep_region_size_o combinationally off the
// CSR fields. Here one process owns both ports and everything else asks it to
// re-sample, which keeps the LT model to a single driver per signal while still
// tracking whatever the registers hold.
// =============================================================================
void sep_cpu_ctrl_ip::publish_inbound_window()
{
    window_changed_.notify(sc_core::SC_ZERO_TIME);
}

void sep_cpu_ctrl_ip::publish_window_process()
{
    sep_global_base_addr_o.write(
        static_cast<uint64_t>(SEP_GLOBAL_BASE_ADDR) & 0x00FF'FFFF'FFFF'FFFFULL);
    sep_region_size_o.write(
        static_cast<uint64_t>(SEP_REGION_SIZE) & 0xFFFF'FFFFULL);
}

// =============================================================================
// register_callbacks — wire handler methods to memory offsets (registration only)
// =============================================================================
void sep_cpu_ctrl_ip::register_callbacks()
{
    using std::placeholders::_1;

    // --- Write callbacks ---

    std::function<bool(DT)> nmi_vec_write = std::bind(&sep_cpu_ctrl_ip::handle_write_SEP_NMI_VEC, this, _1, SEP_NMI_VEC.write_bit_mask);
    memory.register_write_callback(nmi_vec_write, SEP_NMI_VEC.offset);

    std::function<bool(DT)> nmi_lock_write = std::bind(&sep_cpu_ctrl_ip::handle_write_SEP_NMI_VEC_LOCK, this, _1, SEP_NMI_VEC_LOCK.write_bit_mask);
    memory.register_write_callback(nmi_lock_write, SEP_NMI_VEC_LOCK.offset);

    std::function<bool(DT)> trng_sel_write = std::bind(&sep_cpu_ctrl_ip::handle_write_EXT_TRNG_SRC_SEL, this, _1, EXT_TRNG_SRC_SEL.write_bit_mask);
    memory.register_write_callback(trng_sel_write, EXT_TRNG_SRC_SEL.offset);

    std::function<bool(DT)> trng_lock_write = std::bind(&sep_cpu_ctrl_ip::handle_write_EXT_TRNG_SRC_SEL_LOCK, this, _1, EXT_TRNG_SRC_SEL_LOCK.write_bit_mask);
    memory.register_write_callback(trng_lock_write, EXT_TRNG_SRC_SEL_LOCK.offset);

    std::function<bool(DT)> timeout_clear_write = std::bind(&sep_cpu_ctrl_ip::handle_write_TIMEOUT_CLEAR, this, _1, TIMEOUT_CLEAR.write_bit_mask);
    memory.register_write_callback(timeout_clear_write, TIMEOUT_CLEAR.offset);

    std::function<bool(DT)> dma_bus_err_clear_write = std::bind(&sep_cpu_ctrl_ip::handle_write_DMA_BUS_ERR_CLEAR, this, _1, DMA_BUS_ERR_CLEAR.write_bit_mask);
    memory.register_write_callback(dma_bus_err_clear_write, DMA_BUS_ERR_CLEAR.offset);

    std::function<bool(DT)> periph_bus_err_clear_write = std::bind(&sep_cpu_ctrl_ip::handle_write_PERIPH_BUS_ERR_CLEAR, this, _1, PERIPH_BUS_ERR_CLEAR.write_bit_mask);
    memory.register_write_callback(periph_bus_err_clear_write, PERIPH_BUS_ERR_CLEAR.offset);

    std::function<bool(DT)> ref_counter_write = std::bind(&sep_cpu_ctrl_ip::handle_write_REFERENCE_COUNTER, this, _1, REFERENCE_COUNTER.write_bit_mask);
    memory.register_write_callback(ref_counter_write, REFERENCE_COUNTER.offset);

    // --- Post-write callbacks ---
    // Both inbound-window registers are plain storage; these only republish the
    // stored value on sep_global_base_addr_o / sep_region_size_o afterwards.
    std::function<bool()> window_published = std::bind(&sep_cpu_ctrl_ip::post_write_inbound_window, this);
    memory.register_post_write_callback(window_published, SEP_GLOBAL_BASE_ADDR.offset);
    memory.register_post_write_callback(window_published, SEP_REGION_SIZE.offset);

    // --- Read callbacks ---

    std::function<bool(DT&)> rc_read = std::bind(&sep_cpu_ctrl_ip::handle_read_REFERENCE_COUNTER, this, _1, REFERENCE_COUNTER.read_bit_mask);
    memory.register_read_callback(rc_read, REFERENCE_COUNTER.offset);

    std::function<bool(DT&)> toint_read = std::bind(&sep_cpu_ctrl_ip::handle_read_TIMEOUT_INTERRUPT, this, _1, TIMEOUT_INTERRUPT.read_bit_mask);
    memory.register_read_callback(toint_read, TIMEOUT_INTERRUPT.offset);

    std::function<bool(DT&)> testctrl_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_TEST_CTRL, this, _1, SEP_TEST_CTRL.read_bit_mask);
    memory.register_read_callback(testctrl_read, SEP_TEST_CTRL.offset);

    std::function<bool(DT&)> smc_fuse_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SMC_FUSE_SENSE_STATUS, this, _1, SMC_FUSE_SENSE_STATUS.read_bit_mask);
    memory.register_read_callback(smc_fuse_read, SMC_FUSE_SENSE_STATUS.offset);

    std::function<bool(DT&)> sep_fuse_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_FUSE_SENSE_STATUS, this, _1, SEP_FUSE_SENSE_STATUS.read_bit_mask);
    memory.register_read_callback(sep_fuse_read, SEP_FUSE_SENSE_STATUS.offset);

    std::function<bool(DT&)> straps_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_STRAPS, this, _1, SEP_STRAPS.read_bit_mask);
    memory.register_read_callback(straps_read, SEP_STRAPS.offset);

    std::function<bool(DT&)> nmi_vec_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_NMI_VEC, this, _1, SEP_NMI_VEC.read_bit_mask);
    memory.register_read_callback(nmi_vec_read, SEP_NMI_VEC.offset);

    std::function<bool(DT&)> nmi_lock_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_NMI_VEC_LOCK, this, _1, SEP_NMI_VEC_LOCK.read_bit_mask);
    memory.register_read_callback(nmi_lock_read, SEP_NMI_VEC_LOCK.offset);

    std::function<bool(DT&)> trng_sel_read = std::bind(&sep_cpu_ctrl_ip::handle_read_EXT_TRNG_SRC_SEL, this, _1, EXT_TRNG_SRC_SEL.read_bit_mask);
    memory.register_read_callback(trng_sel_read, EXT_TRNG_SRC_SEL.offset);

    std::function<bool(DT&)> trng_lock_read = std::bind(&sep_cpu_ctrl_ip::handle_read_EXT_TRNG_SRC_SEL_LOCK, this, _1, EXT_TRNG_SRC_SEL_LOCK.read_bit_mask);
    memory.register_read_callback(trng_lock_read, EXT_TRNG_SRC_SEL_LOCK.offset);

    std::function<bool(DT&)> version_read = std::bind(&sep_cpu_ctrl_ip::handle_read_SEP_VERSION_ID, this, _1, SEP_VERSION_ID.read_bit_mask);
    memory.register_read_callback(version_read, SEP_VERSION_ID.offset);
}

// =============================================================================
// Write handlers
// =============================================================================

// REFERENCE_COUNTER: sw write reloads the free-running counter (real RTL: the
// write value crosses into the refclk domain via prim_refclk_count_w_cdc's
// i_cnt_update/i_cnt_update_value, and the counter resumes counting from it).
bool sep_cpu_ctrl_ip::handle_write_REFERENCE_COUNTER(DT value, DT write_bit_mask)
{
    hwif_in.reference_counter_rc = static_cast<uint64_t>(value & write_bit_mask);
    return true;
}

// SEP_GLOBAL_BASE_ADDR / SEP_REGION_SIZE: plain storage whose stored value
// leaves the subsystem on sep_global_base_addr_o / sep_region_size_o. Runs after
// the store, so the republish a delta later re-samples the new contents.
bool sep_cpu_ctrl_ip::post_write_inbound_window()
{
    publish_inbound_window();
    return true;
}

// SEP_NMI_VEC: lock-gated write; drives nmi_vec_o.
// Field convention: fs_nmi_vec_ = reg[31:1] right-shifted; address = fs_nmi_vec_ << 1.
bool sep_cpu_ctrl_ip::handle_write_SEP_NMI_VEC(DT value, DT /*write_bit_mask*/)
{
    if (!fs_nmi_vec_lock_) {
        fs_nmi_vec_ = static_cast<uint32_t>((value >> 1) & 0x7FFFFFFFu);
        nmi_vec_o.write(static_cast<uint32_t>(fs_nmi_vec_) << 1);
    }
    return true;
}

// SEP_NMI_VEC_LOCK: woset — set bits can never be cleared.
bool sep_cpu_ctrl_ip::handle_write_SEP_NMI_VEC_LOCK(DT value, DT write_bit_mask)
{
    fs_nmi_vec_lock_ |= bool(value & write_bit_mask);
    return false;
}

// EXT_TRNG_SRC_SEL: lock-gated write.
bool sep_cpu_ctrl_ip::handle_write_EXT_TRNG_SRC_SEL(DT value, DT write_bit_mask)
{
    if (!fs_ext_trng_src_sel_lock_)
        fs_ext_trng_src_sel_ = static_cast<uint8_t>(value & write_bit_mask);
    return false;
}

// EXT_TRNG_SRC_SEL_LOCK: woset.
bool sep_cpu_ctrl_ip::handle_write_EXT_TRNG_SRC_SEL_LOCK(DT value, DT write_bit_mask)
{
    fs_ext_trng_src_sel_lock_ |= bool(value & write_bit_mask);
    return false;
}

// TIMEOUT_CLEAR: singlepulse — each set bit clears the matching TIMEOUT_INTERRUPT
// bit, then the register itself self-clears back to 0 (sw=w, no storage).
bool sep_cpu_ctrl_ip::handle_write_TIMEOUT_CLEAR(DT value, DT write_bit_mask)
{
    const DT clear = value & write_bit_mask;
    if (clear & (DT(1) << 0)) hwif_in.sys_in_timeout_int            = false;
    if (clear & (DT(1) << 1)) hwif_in.dma_data_timeout_int          = false;
    if (clear & (DT(1) << 2)) hwif_in.alias_remap_timeout_int       = false;
    if (clear & (DT(1) << 3)) hwif_in.filter_out_timeout_int        = false;
    if (clear & (DT(1) << 4)) hwif_in.entropy_read_timeout_int      = false;
    if (clear & (DT(1) << 5)) hwif_in.entropy_write_timeout_int     = false;
    if (clear & (DT(1) << 6)) hwif_in.inbound_mailbox_timeout_int   = false;
    if (clear & (DT(1) << 7)) hwif_in.outbound_mailbox_timeout_int  = false;

    TIMEOUT_CLEAR = 0;  // singlepulse — never retains state
    return true;
}

bool sep_cpu_ctrl_ip::handle_write_DMA_BUS_ERR_CLEAR(DT value, DT write_bit_mask)
{
    if ((value & write_bit_mask) != 0) {
        DMA_BUS_ERR_STATUS = 0;
    }
    DMA_BUS_ERR_CLEAR = 0;
    return true;
}

bool sep_cpu_ctrl_ip::handle_write_PERIPH_BUS_ERR_CLEAR(DT value, DT write_bit_mask)
{
    const DT pulse = value & write_bit_mask;
    PERIPH_BUS_ERR_STATUS = static_cast<DT>(PERIPH_BUS_ERR_STATUS) & ~pulse;
    PERIPH_BUS_ERR_CLEAR = 0;
    return true;
}

// =============================================================================
// Read handlers
// =============================================================================

bool sep_cpu_ctrl_ip::handle_read_REFERENCE_COUNTER(DT& value, DT /*read_bit_mask*/)
{
    value = ++hwif_in.reference_counter_rc;
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_TIMEOUT_INTERRUPT(DT& value, DT /*read_bit_mask*/)
{
    value = 0;
    value |= DT(hwif_in.sys_in_timeout_int)           << 0;
    value |= DT(hwif_in.dma_data_timeout_int)         << 1;
    value |= DT(hwif_in.alias_remap_timeout_int)      << 2;
    value |= DT(hwif_in.filter_out_timeout_int)       << 3;
    value |= DT(hwif_in.entropy_read_timeout_int)     << 4;
    value |= DT(hwif_in.entropy_write_timeout_int)    << 5;
    value |= DT(hwif_in.inbound_mailbox_timeout_int)  << 6;
    value |= DT(hwif_in.outbound_mailbox_timeout_int) << 7;
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_TEST_CTRL(DT& value, DT /*read_bit_mask*/)
{
    value = 0;
    value |= DT(hwif_in.fast_spi_en)    << 31;
    value |= DT(hwif_in.fast_iccm_en)   << 30;
    value |= DT(hwif_in.fast_dccm_en)   << 29;
    value |= DT(hwif_in.fast_sram_en)   << 28;
    value |= DT(hwif_in.fast_pka_en)    << 27;
    value |= DT(hwif_in.sep_standalone) << 26;
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SMC_FUSE_SENSE_STATUS(DT& value, DT /*read_bit_mask*/)
{
    value = DT(hwif_in.smc_fuse_sense_done);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_FUSE_SENSE_STATUS(DT& value, DT /*read_bit_mask*/)
{
    value = DT(hwif_in.sep_fuse_sense_done);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_STRAPS(DT& value, DT /*read_bit_mask*/)
{
    value = DT(hwif_in.test_en) | (DT(hwif_in.bypass_mem_repair) << 1);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_NMI_VEC(DT& value, DT /*read_bit_mask*/)
{
    value = static_cast<DT>(fs_nmi_vec_) << 1;
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_NMI_VEC_LOCK(DT& value, DT /*read_bit_mask*/)
{
    value = DT(fs_nmi_vec_lock_);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_EXT_TRNG_SRC_SEL(DT& value, DT /*read_bit_mask*/)
{
    value = DT(fs_ext_trng_src_sel_);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_EXT_TRNG_SRC_SEL_LOCK(DT& value, DT /*read_bit_mask*/)
{
    value = DT(fs_ext_trng_src_sel_lock_);
    return true;
}

bool sep_cpu_ctrl_ip::handle_read_SEP_VERSION_ID(DT& value, DT /*read_bit_mask*/)
{
    value = 0x00000000DEADBEEFull;
    return true;
}
