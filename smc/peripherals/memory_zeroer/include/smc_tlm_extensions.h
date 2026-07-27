// SPDX-License-Identifier: Apache-2.0
/**
 * @file smc_tlm_extensions.h
 * @brief Memory-zeroer-facing include for shared SMC TLM sideband types.
 *
 * `smc::smc_axi_extension` is defined in the shared SMC common tree at
 * `smc/common/include/smc_axi_extension.h`.  This peripheral's CMake adds
 * that directory to the include path (standalone and integration builds).
 */

#ifndef SMC_MEMORY_ZEROER_TLM_EXTENSIONS_H_
#define SMC_MEMORY_ZEROER_TLM_EXTENSIONS_H_

// `smc::source_id_t` and `smc::smc_axi_extension` come from the canonical
// header included above; this peripheral must not redefine them.
#include "smc_axi_extension.h"

#endif  // SMC_MEMORY_ZEROER_TLM_EXTENSIONS_H_
