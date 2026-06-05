// SPDX-License-Identifier: Apache-2.0
/**
 * @file smc_tlm_extensions.h
 * @brief CLINT-facing include for shared SMC TLM sideband types.
 *
 * `smc::smc_axi_extension` is defined in the CPU cluster tree at
 * `cpu_cluster/include/smc_axi_extension.h`.  CLINT CMake adds that
 * directory to the include path via `SMC_AXI_EXTENSION_DIR`.
 *
 * This header only adds fabric-wide `source_id_t` values used by filters;
 * see `01_SMC_Architecture.md` and `02_SMC_IP_LowLevel_Design.md`.
 *
 * Kept aligned with `peripherals/plic/include/smc_tlm_extensions.h` so both
 * IPs share identical fabric source-ID semantics.
 */

#ifndef SMC_TLM_EXTENSIONS_H_
#define SMC_TLM_EXTENSIONS_H_

#include "smc_axi_extension.h"

#include <cstdint>

namespace smc {

enum source_id_t : uint16_t {
    SMC_ID   = 0x10, ///< CPU cluster (= smc_axi_extension::SMC_CPU_SOURCE_ID)
    MMODE_ID = 0x20,
    OTHER_ID = 0x30,
    JTAG_ID  = 0x40,
    SEP_ID   = 0x50,
    SYS_ID   = 0x60
};

}  // namespace smc

#endif  // SMC_TLM_EXTENSIONS_H_
