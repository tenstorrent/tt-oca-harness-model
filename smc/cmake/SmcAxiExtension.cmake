# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# SmcAxiExtension.cmake
#
# Resolves the canonical `smc_axi_extension.h` directory
# (`smc/common/include`) from this module's location so every IP — peripheral,
# fabric, or cpu_cluster — uses the same path.
#
# Do NOT make this a CACHE variable.  A cached path from before the header
# moved out of per-IP include/ leaves incremental `build_asan/` and
# `build_cov/` trees compiling against a stale -I that no longer contains
# the file.
#
# USAGE (after CMAKE_MODULE_PATH includes smc/cmake):
#   include(SmcAxiExtension)
#   # sets SMC_AXI_EXTENSION_DIR in the caller
# =============================================================================

set(SMC_AXI_EXTENSION_DIR "${CMAKE_CURRENT_LIST_DIR}/../common/include")
if(NOT EXISTS "${SMC_AXI_EXTENSION_DIR}/smc_axi_extension.h")
    message(FATAL_ERROR
        "smc_axi_extension.h not found at ${SMC_AXI_EXTENSION_DIR}. "
        "The canonical header lives in smc/common/include/ "
        "(see .cursor/rules/smc-axi-extension-canonical.mdc).")
endif()
