/**
 * @file sep_output_remap_ctrl.cpp
 * @brief Implementation of sep_output_remap_ctrl_ip — generic output remap model.
 *
 * Instantiated twice in och_sep_ss.hpp:
 *   sep_output_remap_ctrl_ip ap_output_remap  ("ap_output_remap",   InstanceType::AP);
 *   sep_output_remap_ctrl_ip stee_output_remap("stee_output_remap", InstanceType::STEE);
 */

#include "sep_output_remap_ctrl.h"

#include "sep_axi_extension.h"

// =============================================================================
// InstanceType constructor — delegates to the parametric constructor
// =============================================================================
sep_output_remap_ctrl_ip::sep_output_remap_ctrl_ip(sc_module_name n, InstanceType type)
    : sep_output_remap_ctrl_ip(
        n,
        type == InstanceType::AP ? AP_REGION_BASE : STEE_REGION_BASE,
        type == InstanceType::AP ? AP_NUM_REGIONS : STEE_NUM_REGIONS,
        type == InstanceType::AP ? AP_IDX_START   : STEE_IDX_START)
{}

// =============================================================================
// Parametric constructor
// =============================================================================
sep_output_remap_ctrl_ip::sep_output_remap_ctrl_ip(
    sc_module_name n,
    uint64_t       region_base,
    uint32_t       num_regions,
    uint32_t       idx_start)
    : sep_output_remap_ctrl_base(n, "sep_output_remap_ctrl", MEMORY_SIZE)
    , REGION_BASE(region_base)
    , NUM_REGIONS(num_regions)
    , IDX_START(idx_start)
    , data_socket("data_socket")
    , remapped_socket("remapped_socket")
    , rst_ni("rst_ni")
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    if (num_regions == 0 || (num_regions & (num_regions - 1)) != 0) {
        SC_REPORT_FATAL(name(), "num_regions must be a non-zero power of 2");
    }
    if (num_regions > MAX_REGIONS) {
        SC_REPORT_FATAL(name(), "num_regions exceeds MAX_REGIONS (16)");
    }

    // Derive index width and masks
    idx_width_ = 0;
    uint32_t tmp = num_regions;
    while (tmp > 1) { idx_width_++; tmp >>= 1; }  // log2(num_regions)

    lower_mask_ = (idx_start > 0) ? ((1ULL << idx_start) - 1ULL) : 0ULL;
    idx_mask_   = ((1ULL << idx_width_) - 1ULL) << idx_start;

    // Register data-path socket callbacks
    data_socket.register_b_transport(this,
        &sep_output_remap_ctrl_ip::data_b_transport);
    data_socket.register_transport_dbg(this,
        &sep_output_remap_ctrl_ip::data_transport_dbg);
}

// =============================================================================
// reset()
// =============================================================================
void sep_output_remap_ctrl_ip::reset()
{
    reset_all_registers();
}

// =============================================================================
// remap_address()
//
// Implements the output_remap.sv algorithm exactly:
//
//   adjusted = addr - REGION_BASE
//   idx      = adjusted[IDX_START + log2(N) - 1 : IDX_START]
//   remapped = { REGION_ATTRS[idx][55:IDX_START],
//                adjusted[IDX_START-1:0] }
//
// For SEP: IDX_START=19, N=16  →  idx = adjusted[22:19], lower 19 bits kept.
// =============================================================================
uint64_t sep_output_remap_ctrl_ip::remap_address(uint64_t addr) const
{
    const uint64_t adjusted = addr - REGION_BASE;

    const uint32_t idx = static_cast<uint32_t>(
        (adjusted & idx_mask_) >> IDX_START
    ) % NUM_REGIONS;

    const uint64_t offset = static_cast<uint64_t>(REGION_ATTRS[idx])
                            & 0x00FFFFFFFFFFFFFFULL;

    const uint64_t upper_bits = offset   & ~lower_mask_;
    const uint64_t lower_bits = adjusted &  lower_mask_;

    return upper_bits | lower_bits;
}

// =============================================================================
// data_b_transport()
//
// output_remap.sv is instantiated for both the AP and STEE stages with
// UserOverrideEn=1'b1 and UserOverrideVal=OTHERS_SOURCE_ID
// (sep_system_peripherals.sv:255-256,282-283), so every transaction crossing
// into those domains is re-tagged regardless of which master issued it. The
// source ID is restored afterwards for the same reason the address is: the
// caller owns this payload and the override applies only downstream.
// =============================================================================
void sep_output_remap_ctrl_ip::data_b_transport(tlm::tlm_generic_payload& trans,
                                                  sc_core::sc_time& delay)
{
    const uint64_t orig_addr = trans.get_address();
    const uint64_t new_addr  = remap_address(orig_addr);

    sep::sep_axi_extension* ext = trans.get_extension<sep::sep_axi_extension>();
    const uint8_t orig_src_id = ext ? ext->source_id : 0;
    if (ext) ext->source_id = sep::OTHERS_SOURCE_ID;

    trans.set_address(new_addr);
    remapped_socket->b_transport(trans, delay);
    trans.set_address(orig_addr);   // restore

    if (ext) ext->source_id = orig_src_id;
}

// =============================================================================
// data_transport_dbg()
// =============================================================================
unsigned int sep_output_remap_ctrl_ip::data_transport_dbg(tlm::tlm_generic_payload& trans)
{
    const uint64_t orig_addr = trans.get_address();
    const uint64_t new_addr  = remap_address(orig_addr);

    trans.set_address(new_addr);
    unsigned int ret = remapped_socket->transport_dbg(trans);
    trans.set_address(orig_addr);   // restore

    return ret;
}

// =============================================================================
// reset_handler — fires on any rst_ni edge; clears all registers when low
// =============================================================================
void sep_output_remap_ctrl_ip::reset_handler()
{
    if (!rst_ni.read())
        reset();
}
