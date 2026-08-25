// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// sep/utils/tlm_extensions/sep_axi_extension.h
//
// Canonical SEP TLM-2.0 generic-payload extension. This is the SINGLE
// definition of `sep::sep_axi_extension` and `sep::source_id_t` for every
// SEP IP. Do not duplicate this type in peripheral trees -- include this
// header instead. Mirrors smc/common/include/smc_axi_extension.h's role on
// the SMC side.
//
// PURPOSE
// -------
// TLM-2.0 ignorable extension carried on transactions issued by SEP's local
// masters (VeeR CPU, secure_dma) and by the AP/STEE output-remap stages.
// Encodes the AXI sideband metadata that two distinct SEP models act on:
// sep_filter_ctrl (outbound_filter / inbound_filter) for its src-id /
// non-secure access-control checks, and local_master_alias_remap_ctrl for
// its per-region cacheable override:
//
//   source_id -- the SEP-wide initiator ID. RTL: hw/sep/sep_pkg.sv's
//                4-bit SEP_SOURCE_ID/MMODE_SOURCE_ID/SMC_SOURCE_ID/
//                OTHERS_SOURCE_ID constants, carried on AXI AWUSER/ARUSER
//                (a 12-bit field in RTL -- SEP_SYSTEM_PERIPHERALS_*_AXI_USER_WIDTH
//                -- but only the low nibble is ever driven; see
//                hw/comp/axi_filter/rtl/axi_filter_wrap.sv). Stamped once at
//                each master's own boundary (hw/sep/sep_cpu.sv:439-449 for
//                the CPU; hw/sep/sep.sv:895 for secure_dma, hardcoded to 0
//                via `.TlUserRsvd('0)`) and then carried unchanged by every
//                downstream remap stage EXCEPT hw/ip/output_remap/rtl/
//                output_remap.sv:99, which unconditionally overwrites it to
//                OTHERS_SOURCE_ID for traffic crossing into the AP/STEE
//                domain -- regardless of the original master.
//
//                NOTE: vp/platform/sep/docs/SEP_AXI4 Architecture.md labels
//                both the SMNU-bound and local-CSR/SRAM-bound branches as
//                "srcid = SEP_ID", but the RTL only stamps SEP_SOURCE_ID at
//                the CPU's own boundary -- secure_dma traffic reaching those
//                same branches still carries OTHERS_SOURCE_ID (0), since
//                nothing re-stamps it there. This header follows the RTL
//                constant, not the diagram's branch-level label; flag to the
//                RTL/diagram owner if that's not the intended behavior.
//
//   group_id  -- present in the RTL register/filter parameterization
//                (filter_ctrl.rdl's FILTER_CONFIG.group_id, axi_filter_wrap's
//                GroupIdWidth=4/GroupIdUserBitStart=4) but currently inert:
//                both outbound_filter and inbound_filter instances set
//                EnGroupIdFilter(1'b0) in hw/sep/sep_system_peripherals/rtl/
//                sep_system_peripherals.sv:345,392. Modeled here for
//                register-map fidelity; do not gate any filter decision on
//                it until EnGroupIdFilter is actually enabled in RTL.
//
//   is_ns     -- non-secure attribute, AWPROT[1]/ARPROT[1] in RTL, matching
//                sep_filter_ctrl's FILTER_CONFIG.allow_ns field exactly.
//                Both filter instances set EnNsFilter(1'b1), so this bit is
//                actively enforced (unlike group_id).
//
//                The `false` default is RTL-exact for every SEP-local master,
//                not a placeholder: VeeR EL2 hardwires the ports sep_cpu.sv
//                feeds to aw/ar.prot -- lsu and sb to 3'b001
//                (el2_lsu_bus_buffer.sv:881,898, el2_dbg.sv:740,763) and ifu to
//                3'b101 (el2_ifu_mem_ctl.sv:1075) -- so bit 1 is 0 on all three
//                regardless of privilege mode, and no SEP block sets it later
//                (axi_alias_remap, axi_window_remap and the SPI/UART wrappers
//                all pass prot through unchanged). Do NOT derive this from the
//                ISS's privilegeMode(): that diverges from RTL, and because
//                pass_ns is an exact match under BlockByDefault=1, a CPU access
//                claiming non-secure would be denied by every allow_ns=0 entry.
//
//   cacheable -- AWCACHE/ARCACHE in RTL. hw/ip/axi_alias_remap/rtl/
//                axi_alias_remap.sv:122,147 overwrites the outgoing cache
//                field with {CacheWidth{REGION_ATTRS[idx].cacheable}} on a
//                region hit, and passes the incoming value through
//                unchanged on a miss -- the same "compute on hit, passthrough
//                on miss" shape as source_id's handling in output_remap.sv.
//                This is the local_master_alias_remap_ctrl SystemC model's
//                own REGION_ATTRS.cacheable bit; that model applies it on both
//                its functional and debug paths, and restores the caller's
//                value afterwards (see its docs/03_*_Test_Plan.md T12-T17).
//                RTL replicates the single region bit across all four AxCACHE
//                bits, so one bool is faithful for this IP -- it cannot express
//                the finer-grained AXI encodings a different master might drive.
//
// Everything else in the standard AXI4 channel (id, len, size, burst, lock,
// qos, region) was checked and excluded deliberately, not by omission: every
// assignment of these fields in axi_alias_remap.sv and output_remap.sv is a
// plain, unconditional passthrough (`= axi_in_req_i.X`) -- no SEP remap or
// filter module ever computes or branches on them. len/size are already
// covered by tlm_generic_payload's own data_length/streaming_width, and
// id/burst/lock/qos/region have no consumer anywhere in this chain to feed.
//
// This is a `tlm_extension` (ignorable): targets that do not understand it
// just see a standard generic_payload. Filter/remap modules retrieve it via
// `trans.get_extension<sep_axi_extension>()`.
//
// Defaults to SEP_SOURCE_ID (the CPU) so that test-bench code which omits
// the extension sees well-defined, "trusted local master" behaviour --
// same philosophy as smc_axi_extension's SMC_CPU_SOURCE_ID default.
//
// Current wiring, for orientation when changing any of it:
//
//   stamped by  VeeR-ISSTlm (SEP_SOURCE_ID, member extension per transaction),
//               secure_dma (OTHERS_SOURCE_ID), and adapters.h (SMC_SOURCE_ID
//               for external traffic that arrives without an extension)
//   re-stamped  sep_output_remap_ctrl, to OTHERS_SOURCE_ID for the AP/STEE
//               crossing, restoring the caller's value afterwards
//   read by     sep_filter_ctrl (source_id vs FILTER_CONFIG.src_id, is_ns vs
//               allow_ns) and local_master_alias_remap_ctrl (cacheable override)
//
// Models that read the extension fall back to the field defaults below when a
// transaction arrives without one, so an un-stamped initiator looks like a
// trusted local master rather than failing closed.
//
// References
// - hw/comp/axi_filter/rtl/traffic_filter.sv, axi_filter_wrap.sv
// - hw/comp/axi_filter/data/registers/rdl/filter_ctrl.rdl
// - hw/ip/axi_alias_remap/rtl/axi_alias_remap.sv
// - hw/ip/output_remap/rtl/output_remap.sv
// - hw/sep/sep_pkg.sv, hw/sep/sep_cpu.sv, hw/sep/sep.sv
// - hw/sep/sep_system_peripherals/rtl/sep_system_peripherals.sv
// - vp/platform/sep/docs/SEP_AXI4 Architecture.md
// ===========================================================================

#pragma once

#include <tlm.h>

#include <cstdint>

namespace sep {

/**
 * @brief AXI source-ID enumeration understood by the SEP filter/remap IPs.
 *
 * Each local master is assigned a fixed source ID, stamped once at its own
 * AXI boundary in RTL. `sep_filter_ctrl` (outbound_filter / inbound_filter)
 * uses this field, compared against its per-entry FILTER_CONFIG.src_id,
 * to enforce access-control policy.
 *
 * | Value              | Master                                          |
 * |--------------------|--------------------------------------------------|
 * | `SEP_SOURCE_ID`    | SEP VeeR CPU (IFU/LSU/DBG)                       |
 * | `MMODE_SOURCE_ID`  | M-mode remapped traffic                          |
 * | `SMC_SOURCE_ID`    | SMC-originated inbound traffic                   |
 * | `OTHERS_SOURCE_ID` | secure_dma; AP/STEE-bound traffic (re-stamped)   |
 */
enum source_id_t : uint8_t {
    SEP_SOURCE_ID    = 0xF, ///< SEP VeeR CPU (IFU/LSU/DBG) -- sep_cpu.sv
    MMODE_SOURCE_ID  = 0xC, ///< M-mode remapped traffic
    SMC_SOURCE_ID    = 0x3, ///< SMC-originated inbound traffic
    OTHERS_SOURCE_ID = 0x0  ///< secure_dma (hardcoded); AP/STEE output-remap re-stamp
};

class sep_axi_extension : public tlm::tlm_extension<sep_axi_extension>
{
public:
    uint8_t source_id = SEP_SOURCE_ID; ///< sep_pkg::*_SOURCE_ID; see source_id_t table above.
    uint8_t group_id  = 0;             ///< RTL-present, currently inert (EnGroupIdFilter=0).
    bool    is_ns     = false;         ///< AWPROT[1]/ARPROT[1]; matches FILTER_CONFIG.allow_ns.
    bool    cacheable = false;         ///< AWCACHE/ARCACHE; matches REGION_ATTRS.cacheable.

    sep_axi_extension() = default;

    // Required tlm_extension overrides ---------------------------------------
    tlm::tlm_extension_base* clone() const override
    {
        return new sep_axi_extension(*this);
    }
    void copy_from(const tlm::tlm_extension_base& other) override
    {
        *this = static_cast<const sep_axi_extension&>(other);
    }
};

}  // namespace sep
