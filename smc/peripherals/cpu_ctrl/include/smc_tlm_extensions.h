// SPDX-License-Identifier: Apache-2.0
/**
 * @file smc_tlm_extensions.h
 * @brief CPU-ctrl-facing include for shared SMC TLM sideband types.
 *
 * `smc::smc_axi_extension` is defined in the CPU cluster tree at
 * `cpu_cluster/include/smc_axi_extension.h`.  CPU-ctrl CMake adds that
 * directory to the include path (standalone and integration builds).
 */

#ifndef SMC_CPU_CTRL_TLM_EXTENSIONS_H_
#define SMC_CPU_CTRL_TLM_EXTENSIONS_H_

#include "smc_axi_extension.h"

#include <cstdint>

namespace smc {

enum source_id_t : uint16_t {
    SMC_ID   = 0x10,
    MMODE_ID = 0x20,
    OTHER_ID = 0x30,
    JTAG_ID  = 0x40,
    SEP_ID   = 0x50,
    SYS_ID   = 0x60
};

}  // namespace smc

#endif  // SMC_CPU_CTRL_TLM_EXTENSIONS_H_
