// SPDX-License-Identifier: Apache-2.0
/**
 * @file smc_tlm_extensions.h
 * @brief Common TLM-2.0 generic-payload extension shared by every SMC IP.
 *
 * This header defines the `smc_axi_extension` struct that carries AXI4
 * sideband signals (source ID, protection bits, IDs) alongside every
 * `tlm_generic_payload` transaction issued on the SMC fabric.
 *
 * ### Design note
 * The extension is kept intentionally minimal so individual IP sub-packages
 * (e.g. `beu`) can be built stand-alone for early bring-up without pulling in
 * the full SMC model tree.  When the top-level SMC model is assembled, the
 * project-wide version of this header supersedes this file.
 *
 * ### References
 * - `01_SMC_Architecture.md §6.1` — canonical definition of sideband fields
 * - `02_SMC_IP_LowLevel_Design.md §2` — AXI filter that inspects these fields
 *   before forwarding transactions to the IP targets
 */

#ifndef SMC_TLM_EXTENSIONS_H_
#define SMC_TLM_EXTENSIONS_H_

#include <tlm>
#include <cstdint>

namespace smc {

/**
 * @brief AXI source-ID enumeration understood by the SMC fabric and filters.
 *
 * Each master on the SMC internal bus is assigned a fixed source ID.  The
 * `axi_filter` module (sitting between the NoC and each IP target) uses this
 * field to enforce access-control policies described in
 * `02_SMC_IP_LowLevel_Design.md §2`.
 */
enum source_id_t : uint32_t {
    SMC_ID   = 0x1, ///< CPU cluster and internal masters
    MMODE_ID = 0x2, ///< M-mode remapped traffic
    OTHER_ID = 0x3, ///< Xvisor remapped traffic
    JTAG_ID  = 0x4, ///< Debug Module / jtag2axi
    SEP_ID   = 0x5, ///< SEP chiplet inbound
    SYS_ID   = 0x6  ///< System NoC inbound
};

/**
 * @brief TLM-2.0 generic-payload extension carrying AXI4 sideband signals.
 *
 * Attach an instance of this extension to any `tlm_generic_payload` to
 * convey AXI sideband information through the SMC fabric.  IPs downstream
 * of the `axi_filter` may inspect (but not modify) these fields.
 *
 * All fields default to "trusted CPU cluster" values so that test-bench
 * code that omits the extension sees well-defined, passing behaviour.
 */
struct smc_axi_extension : public tlm::tlm_extension<smc_axi_extension> {
    uint32_t source_id  = SMC_ID; ///< Originating master; see @ref source_id_t
    uint8_t  prot       = 0;      ///< AXI4 PROT[2:0]: [0]=privileged, [1]=non-secure, [2]=instruction
    bool     cacheable  = false;  ///< AXI4 CACHE modifiable bit
    bool     non_secure = false;  ///< Mirrors prot[1]; provided for readability
    uint16_t axi_id     = 0;      ///< AXI transaction ID (ARID / AWID)
    uint8_t  axi_user   = 0;      ///< AXI user-defined sideband (ARUSER / AWUSER)

    /// @brief Deep-copy this extension (required by TLM-2.0 extension protocol).
    tlm_extension_base* clone() const override {
        return new smc_axi_extension(*this);
    }

    /// @brief Copy all fields from @p ext (required by TLM-2.0 extension protocol).
    void copy_from(const tlm_extension_base& ext) override {
        const auto& other = static_cast<const smc_axi_extension&>(ext);
        source_id  = other.source_id;
        prot       = other.prot;
        cacheable  = other.cacheable;
        non_secure = other.non_secure;
        axi_id     = other.axi_id;
        axi_user   = other.axi_user;
    }
};

} // namespace smc

#endif // SMC_TLM_EXTENSIONS_H_
