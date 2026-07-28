# =============================================================================
# vp/platform/smc/CMakeLists.txt
#
# SMC platform VP target — mirrors vp/platform/sep/.
# Builds the `smc-vp` executable: a SystemC/TLM-2.0 platform that instantiates
# the SMC fabric + every modeled peripheral + the Whisper-backed CVA6 CPU
# cluster, and runs bare-metal RV64 firmware loaded from an ELF (the same
# invocation shape as `sep-vp <ini> <elf>`).
#
# This is the canonical SMC platform VP module re-structured to the
# vp/platform/<subsys>/ layout (main.cpp + <module>.hpp + inc/ + config/).
# The historical smc/platform/ integration blob has been removed; the platform
# is now built and tested only through this VP target.
# =============================================================================

# Export compile_commands.json so clangd / IDE tooling resolves the SystemC,
# CCI, and SMC-IP include paths for this target's sources.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Bring in every SMC IP + the fabric + the CPU cluster as subdirectories so
# their static-library targets exist.  Each IP is a standalone project; nesting
# them here is supported.  The IPs guard enable_testing()/add_subdirectory(test)
# on being the top-level project, so their per-IP test binaries are NOT built
# here.
# -----------------------------------------------------------------------------
set(_smc_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../smc")
set(SMC_IP_DIR "${_smc_root}/peripherals")

add_subdirectory(${SMC_IP_DIR}/bootrom          ${CMAKE_CURRENT_BINARY_DIR}/ip_bootrom)
add_subdirectory(${SMC_IP_DIR}/scratchpad_ram   ${CMAKE_CURRENT_BINARY_DIR}/ip_scratchpad)
add_subdirectory(${SMC_IP_DIR}/plic             ${CMAKE_CURRENT_BINARY_DIR}/ip_plic)
add_subdirectory(${SMC_IP_DIR}/clint            ${CMAKE_CURRENT_BINARY_DIR}/ip_clint)
add_subdirectory(${SMC_IP_DIR}/reset_unit       ${CMAKE_CURRENT_BINARY_DIR}/ip_reset_unit)
add_subdirectory(${SMC_IP_DIR}/uart             ${CMAKE_CURRENT_BINARY_DIR}/ip_uart)
add_subdirectory(${SMC_IP_DIR}/i2c_controller   ${CMAKE_CURRENT_BINARY_DIR}/ip_i2c)
add_subdirectory(${SMC_IP_DIR}/i3c_controller  ${CMAKE_CURRENT_BINARY_DIR}/ip_i3c)
add_subdirectory(${SMC_IP_DIR}/cpu_ctrl         ${CMAKE_CURRENT_BINARY_DIR}/ip_cpu_ctrl)
add_subdirectory(${SMC_IP_DIR}/wdt              ${CMAKE_CURRENT_BINARY_DIR}/ip_wdt)
add_subdirectory(${_smc_root}/smc_fabric        ${CMAKE_CURRENT_BINARY_DIR}/ip_smc_fabric)

# The cluster's own PLIC-integration test target (smc_plic_ip) is redundant
# here (we link the real smc_plic below); disable it to avoid double-compiling
# plic.cpp.
set(SMC_BUILD_PLIC_INTEGRATION OFF CACHE BOOL "" FORCE)
add_subdirectory(${_smc_root}/cpu_cluster       ${CMAKE_CURRENT_BINARY_DIR}/ip_cpu_cluster)

# -----------------------------------------------------------------------------
# Canonical smc_axi_extension.h + repo common headers (reg_access.h, sim_log.h).
# -----------------------------------------------------------------------------
set(SMC_AXI_EXTENSION_DIR   "${_smc_root}/common/include"
    CACHE PATH "Directory containing smc_axi_extension.h")
set(SMC_REPO_COMMON_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include"
    CACHE PATH "Directory containing reg_access.h / sim_log.h")

# -----------------------------------------------------------------------------
# smc-vp executable
# -----------------------------------------------------------------------------
add_executable(smc-vp
    main.cpp
    src/smc_platform.cpp)

# smc_platform.hpp lives at the subdir root; helpers live in inc/.
target_include_directories(smc-vp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${SMC_AXI_EXTENSION_DIR}
        ${SMC_REPO_COMMON_INCLUDE})

# The platform module is always built with the CPU cluster wired in — the VP
# exists to run firmware on the CVA6 cores, so SMC_PLATFORM_WITH_CLUSTER is
# mandatory here.
target_compile_definitions(smc-vp PRIVATE SMC_PLATFORM_WITH_CLUSTER=1)

target_link_libraries(smc-vp
    PRIVATE
        smc_fabric
        smc_bootrom
        smc_scratchpad_ram
        smc_plic
        smc_clint
        smc_reset_unit
        smc_uart
        smc_i2c_controller
        smc_i3c_controller
        smc_cpu_ctrl
        smc_wdt
        smc_cpu_cluster
        SystemC::systemc
        SystemC::cci
        Boost::iostreams
        Boost::program_options)

# CCI / SystemC dylibs live outside the standard loader path; bake an RPATH
# so the binary runs without LD_LIBRARY_PATH/DYLD_LIBRARY_PATH.
set(_rpath "${CCI_HOME}/lib;${CCI_HOME}/lib64;${SYSTEMC_HOME}/lib;${SYSTEMC_HOME}/lib64")
set_target_properties(smc-vp PROPERTIES
    BUILD_RPATH   "${_rpath}"
    INSTALL_RPATH "${_rpath}")

target_compile_options(smc-vp PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

install(TARGETS smc-vp RUNTIME DESTINATION ./)
