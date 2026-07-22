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
 * (e.g. `i2c_controller`) can be built stand-alone for early bring-up without
 * pulling in the full SMC model tree.  When the top-level SMC model is
 * assembled, the project-wide version of this header supersedes this file.
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
 *
 * | Value       | Master                        |
 * |-------------|-------------------------------|
 * | `SMC_ID`    | CPU cluster / internal masters |
 * | `MMODE_ID`  | M-mode remapped traffic       |
 * | `OTHER_ID`  | Xvisor remapped traffic       |
 * | `JTAG_ID`   | Debug Module / jtag2axi       |
 * | `SEP_ID`    | SEP chiplet inbound           |
 * | `SYS_ID`    | System NoC inbound            |
 *
 * See `01_SMC_Architecture.md §4` for the full fabric behavior specification.
 */
enum source_id_t : uint16_t {
    SMC_ID   = 0x10, ///< CPU cluster (= smc_axi_extension::SMC_CPU_SOURCE_ID)
    MMODE_ID = 0x20,
    OTHER_ID = 0x30,
    JTAG_ID  = 0x40,
    SEP_ID   = 0x50,
    SYS_ID   = 0x60
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
    static constexpr uint16_t SMC_CPU_SOURCE_ID = 0x10;

    uint16_t source_id = SMC_CPU_SOURCE_ID;
    uint16_t axi_id    = 0;
    uint8_t  prot      = 0b0111; // [3]=lock, [2]=priv, [1]=ns, [0]=data

    bool     is_locked = false;
    bool     is_fetch  = false;
    bool     is_secure = false;
    bool     is_user   = false;

    uint8_t  axi_user  = 0;

    tlm_extension_base* clone() const override { return new smc_axi_extension(*this); }
    void copy_from(const tlm_extension_base& ext) override {
        *this = static_cast<const smc_axi_extension&>(ext);
    }
};

} // namespace smc

#endif // SMC_TLM_EXTENSIONS_H_
