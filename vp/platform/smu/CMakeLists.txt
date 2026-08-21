# =============================================================================
# vp/platform/smu/CMakeLists.txt
#
# SMU platform VP target — same directory shape as vp/platform/smc and
# vp/platform/sep:
#   main.cpp + smu_platform.hpp + src/smu_platform.cpp + inc/ + config/ + docs/
# Plus test/ + run_tests.sh for the interconnect unit suite.
#
# On-die integration of the SMC and SEP virtual platforms through the SMU
# AXI crossbar model (see doc/smc-sep-d2d-interconnect.adoc). Builds
# `smu-vp`:
#
#   smu-vp <smc-cci-ini> <smc-elf> <sep-cci-ini> [sim_time_ms]
#
# This directory reuses the SEP/SMC model library targets created by the
# sibling platform directories (vp/platform/sep and vp/platform/smc, which
# CMake processes first) — it deliberately does NOT add_subdirectory() those
# source trees again (duplicate target definitions).  It is only included
# from vp/platform/CMakeLists.txt when the SMC platform is built (WHISPER_HOME
# available), since the SMC CPU cluster is mandatory here.
# =============================================================================

if(NOT TARGET sep_memory_model)
    message(FATAL_ERROR
        "smu platform requires the SEP platform targets; configure via the vp "
        "top-level (vp/CMakeLists.txt adds platform/sep before platform/smu).")
endif()
if(NOT TARGET smc_cpu_cluster)
    message(FATAL_ERROR
        "smu platform requires the SMC platform targets (WHISPER_HOME); "
        "configure via the vp top-level with WHISPER_HOME set.")
endif()

set(_vp_root  "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(_sep_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../sep")
set(_smc_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../smc")

# -----------------------------------------------------------------------------
# smc_cluster_smu — SMC CPU cluster repackaged as a shared library with all
# Whisper (librvcore) symbols hidden.
#
# Why: smu-vp links TWO descendants of the WdRiscv ISS codebase — the SMC
# cluster's Whisper (librvcore.a) and the SEP's VeeR-ISS (libveeriss_model.a).
# Both define the same `WdRiscv::*` symbols (plus `forceUserStop`), so a
# single static link fails with ~67 duplicate-symbol errors, and any
# order-based "first archive wins" trick would silently bind one side's
# references to the other fork's ABI-incompatible objects.
#
# The cluster already encapsulates Whisper completely: consumers
# (smc_platform.cpp, smu main.cpp) reference only
#   smc::smc_cpu_cluster::{smc_cpu_cluster,~smc_cpu_cluster,load_elf}
# and nothing from WdRiscv.  So we link the cluster + Whisper archives into a
# dylib whose export table hides every Whisper-archive symbol; the SEP side
# then statically links its own VeeR-ISS fork with no competition.  macOS
# two-level namespacing keeps the dylib's internal Whisper bindings private
# at run time.
#
# smc-vp is unaffected: it keeps linking the static smc_cpu_cluster +
# whisper::whisper exactly as before.
#
# Linux note: this requires Whisper objects to be position-independent —
# build Whisper with `make MEM_CALLBACKS=1 EXTRA_CXXFLAGS="-std=gnu++20 -fPIC"`
# (macOS objects are always PIC, so no special build is needed there).
# -----------------------------------------------------------------------------

# Reuse the cluster's own source list so the dylib never goes stale when the
# cluster adds a file.
get_target_property(_cluster_srcs smc_cpu_cluster SOURCES)
get_target_property(_cluster_dir  smc_cpu_cluster SOURCE_DIR)
set(_cluster_srcs_abs "")
foreach(_s IN LISTS _cluster_srcs)
    if(NOT IS_ABSOLUTE "${_s}")
        set(_s "${_cluster_dir}/${_s}")
    endif()
    list(APPEND _cluster_srcs_abs "${_s}")
endforeach()

# Whisper usage requirements (includes, MEM_CALLBACKS/LZ4 defines, companion
# archives + system libs, force_load/whole-archive link options) come straight
# from the whisper::whisper composite target so the LZ4 probe and per-platform
# link mechanics in smc/cpu_cluster/external/whisper-cmake stay authoritative.
get_target_property(_w_incs  whisper::whisper INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_w_defs  whisper::whisper INTERFACE_COMPILE_DEFINITIONS)
get_target_property(_w_links whisper::whisper INTERFACE_LINK_LIBRARIES)
get_target_property(_w_opts  whisper::whisper INTERFACE_LINK_OPTIONS)
foreach(_p IN ITEMS _w_incs _w_defs _w_links _w_opts)
    if(NOT ${_p})
        set(${_p} "")
    endif()
endforeach()

# Return the static archive paths an imported/INTERFACE target would pull in.
function(smu_static_archives_from out_var tgt)
    set(_archs "")
    if(TARGET ${tgt})
        get_target_property(_type ${tgt} TYPE)
        get_target_property(_loc ${tgt} IMPORTED_LOCATION)
        if(NOT _loc)
            get_target_property(_loc ${tgt} IMPORTED_LOCATION_RELEASE)
        endif()
        if(_type STREQUAL "STATIC_LIBRARY" OR _loc MATCHES "\\.(a|lib)$")
            list(APPEND _archs "${_loc}")
        endif()
        get_target_property(_libs ${tgt} INTERFACE_LINK_LIBRARIES)
        if(_libs)
            foreach(_l IN LISTS _libs)
                if(_l MATCHES "\\.(a|lib)$")
                    list(APPEND _archs "${_l}")
                endif()
            endforeach()
        endif()
        list(REMOVE_DUPLICATES _archs)
    endif()
    set(${out_var} "${_archs}" PARENT_SCOPE)
endfunction()

add_library(smc_cluster_smu SHARED ${_cluster_srcs_abs})

target_include_directories(smc_cluster_smu
    PUBLIC
        ${_smc_root}/cpu_cluster/include
        ${_smc_root}/common/include
    PRIVATE
        ${_w_incs})
target_compile_definitions(smc_cluster_smu PRIVATE ${_w_defs})

# Boost.program_options lives IN this DSO (Whisper's Args.cpp).  A static Boost
# must be PIC (RHEL CI rebuilds it with cxxflags=-fPIC); Ubuntu/macOS use a
# shared Boost.  Do not leave those refs for smu-vp to export: Boost is built
# with hidden visibility, so "hidden symbol ... referenced by DSO" fails.
#
# Static CCI must NOT be copied into this DSO.  cci_param registration is a
# process-wide singleton; two copies (DSO + executable) split the broker.
# Headers stay here; the archive is whole-archived into smu-vp instead.
set(_cci_archs "")
if(NOT APPLE)
    smu_static_archives_from(_cci_archs SystemC::cci)
endif()

target_link_libraries(smc_cluster_smu
    PRIVATE
        SystemC::systemc
        boost::headers
        ${_w_links})

if(_cci_archs)
    get_target_property(_cci_incs SystemC::cci INTERFACE_INCLUDE_DIRECTORIES)
    if(_cci_incs)
        target_include_directories(smc_cluster_smu PRIVATE ${_cci_incs})
    endif()
    target_link_options(smc_cluster_smu PRIVATE "-Wl,--allow-shlib-undefined")
else()
    target_link_libraries(smc_cluster_smu PRIVATE SystemC::cci)
endif()

if(APPLE)
    # Hide every symbol defined in the four Whisper archives from the dylib's
    # export table.  The archives are pre-built, so the list can be generated
    # at configure time with nm (whisper-cmake already requires nm).
    if(DEFINED ENV{WHISPER_BUILD_DIR})
        set(_whisper_build_dir "$ENV{WHISPER_BUILD_DIR}")
    else()
        set(_whisper_build_dir "build-${CMAKE_HOST_SYSTEM_NAME}")
    endif()
    set(_whisper_archives
        "${WHISPER_HOME}/${_whisper_build_dir}/librvcore.a"
        "${WHISPER_HOME}/virtual_memory/libvirtual_memory.a"
        "${WHISPER_HOME}/pci/libpci.a"
        "${WHISPER_HOME}/third_party/softfloat/build/RISCV-GCC/softfloat.a")
    set(_hide_syms "")
    foreach(_arch IN LISTS _whisper_archives)
        if(EXISTS "${_arch}")
            execute_process(COMMAND nm -g --defined-only "${_arch}"
                            OUTPUT_VARIABLE _nmout ERROR_QUIET)
            string(REPLACE "\n" ";" _nm_lines "${_nmout}")
            foreach(_line IN LISTS _nm_lines)
                if(_line MATCHES "^ *[0-9a-fA-F]* +[A-Za-z] +([^ ]+)$")
                    list(APPEND _hide_syms "${CMAKE_MATCH_1}")
                endif()
            endforeach()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _hide_syms)
    string(REPLACE ";" "\n" _hide_syms "${_hide_syms}")
    set(_hide_file "${CMAKE_CURRENT_BINARY_DIR}/whisper_hidden_symbols.txt")
    file(WRITE "${_hide_file}" "${_hide_syms}\n")
    # ${_w_opts} carries -Wl,-force_load,<librvcore> (weak template symbols).
    target_link_options(smc_cluster_smu PRIVATE
        ${_w_opts} "-Wl,-unexported_symbols_list,${_hide_file}")
else()
    # GNU ld: version script exports only the cluster interface; everything
    # else (Whisper, std/boost instantiations) stays local to the .so.
    set(_map_file "${CMAKE_CURRENT_BINARY_DIR}/smc_cluster_smu.map")
    file(WRITE "${_map_file}"
         "{\n  global: *smc_cpu_cluster*;\n  local: *;\n};\n")
    target_link_options(smc_cluster_smu PRIVATE
        "-Wl,--version-script,${_map_file}")
endif()

# -----------------------------------------------------------------------------
# smu-vp executable: SMU sc_main + smu_platform + both subsystem modules.
# -----------------------------------------------------------------------------
add_executable(smu-vp
    main.cpp
    src/smu_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../smc/src/smc_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../sep/src/sep_platform.cpp)

target_include_directories(smu-vp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        # SMC platform + helpers + canonical extension/common headers
        ${CMAKE_CURRENT_SOURCE_DIR}/../smc
        ${CMAKE_CURRENT_SOURCE_DIR}/../smc/inc
        ${_smc_root}/common/include
        ${_smc_root}/peripherals
        ${_vp_root}/../common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../aou/include
        # SEP platform + infra
        ${CMAKE_CURRENT_SOURCE_DIR}/../sep/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../sep
        ${CMAKE_CURRENT_SOURCE_DIR}/../infra
        # SEP peripheral model headers (same list as vp/platform/sep)
        ${_sep_root}/peripherals/hmac/include
        ${_sep_root}/peripherals/otbn/include
        ${_sep_root}/peripherals/spi_controller/include
        ${_sep_root}/peripherals/spi_flash/include
        ${_sep_root}/peripherals/kmac/include
        ${_sep_root}/peripherals/adams_bridge/include
        ${_sep_root}/peripherals/csrng/include
        ${_sep_root}/peripherals/aes/include
        ${_sep_root}/peripherals/mailbox/include
        ${_sep_root}/peripherals/aon_timer/include
        ${_sep_root}/peripherals/secure_dma/include
        ${_sep_root}/peripherals/key_manager/include
        ${_sep_root}/peripherals/efuse/include
        ${_sep_root}/peripherals/entropy_src/include
        ${_sep_root}/peripherals/edn/include
        ${_sep_root}/peripherals/sep_scratch_cold/include
        ${_sep_root}/peripherals/sep_scratch_warm/include
        ${_sep_root}/peripherals/sep_output_remap_ctrl/include
        ${_sep_root}/peripherals/sep_filter_ctrl/include
        ${_sep_root}/peripherals/sep_reset_ctrl/include
        ${_sep_root}/peripherals/sep_cpu_ctrl/include
        ${_sep_root}/peripherals/local_master_alias_remap_ctrl/include
        ${_sep_root}/utils/tlm_extensions
        ${_sep_root}/cpu/VeeR-ISS/
        ${_sep_root}/cpu/VeeR-ISS/softfloat/source/include
        ${_sep_root}/cpu/VeeR-ISSTlm/model/inc)

# The SMC platform module is always built with the CPU cluster wired in.
target_compile_definitions(smu-vp PRIVATE SMC_PLATFORM_WITH_CLUSTER=1)

# The SEP platform headers need a deep template instantiation budget.
target_compile_options(smu-vp PRIVATE
    -ftemplate-depth=8192
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -Wno-unused-parameter)

target_link_libraries(smu-vp
    PRIVATE
        # SMC platform
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
        smc_dma
        smc_beu
        smc_telemetry_receiver
        smc_avsbus_controller
        aou_core
        smc_memory_zeroer
        smc_octs_system_timer
        smc_pll_wrapper
        smc_pvt_wrap
        # Cluster + Whisper come in via the shared repackage above (static
        # smc_cpu_cluster would drag librvcore.a's WdRiscv symbols into this
        # link and collide with the SEP's VeeR-ISS).
        smc_cluster_smu
        # SEP platform (keep in lockstep with vp/platform/sep/CMakeLists.txt)
        platform-infra
        hmac_model
        otbn_model
        sep_memory_model
        sep_scratch_cold_model
        sep_scratch_warm_model
        sep_output_remap_ctrl_model
        sep_filter_ctrl_model
        sep_reset_ctrl_model
        sep_cpu_ctrl_model
        local_master_alias_remap_ctrl_model
        csml_logger
        spi_controller_model
        spi_flash_model
        kmac_model
        adams_bridge_model
        csrng_model
        aes_model
        mailbox_model
        aon_timer_model
        secure_dma_model
        key_manager_model
        efuse_model
        lifecycle_ctrl_model
        entropy_src_model
        edn_model
        veeriss_model
        # Common
        SystemC::systemc
        Boost::iostreams
        Boost::program_options
        OpenSSL::Crypto
        Threads::Threads
        z)

# Static CCI was left unresolved in libsmc_cluster_smu.so so the process has
# one broker.  Whole-archive it into the executable and export-dynamic so the
# .so binds at load.  Skip the regular CCI::cci link to avoid duplicate objs.
if(_cci_archs)
    target_link_options(smu-vp PRIVATE "-Wl,--export-dynamic")
    target_link_libraries(smu-vp PRIVATE
        "-Wl,--whole-archive" ${_cci_archs} "-Wl,--no-whole-archive")
elseif(CCI_FOUND)
    target_link_libraries(smu-vp PRIVATE CCI::cci)
endif()

# CCI / SystemC dylibs live outside the standard loader path; bake an RPATH
# so the binary runs without LD_LIBRARY_PATH/DYLD_LIBRARY_PATH.
set(_rpath "${CCI_HOME}/lib;${CCI_HOME}/lib64;${SYSTEMC_HOME}/lib;${SYSTEMC_HOME}/lib64")
set_target_properties(smu-vp smc_cluster_smu PROPERTIES
    BUILD_RPATH   "${_rpath}"
    INSTALL_RPATH "${_rpath}")

install(TARGETS smu-vp smc_cluster_smu
        RUNTIME DESTINATION ./
        LIBRARY DESTINATION ./)
