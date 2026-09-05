# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.20)
project(smc_cpu_cluster
        VERSION   0.1.0
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
# CMAKE_CXX_STANDARD is set by smc_detect_systemc_cxx_std() further down,
# AFTER the SystemC target is defined, so the cluster always matches the C++
# language standard the linked SystemC library was built with.  This avoids
# the `sc_api_version_*_cxxNNNNNNL` link-time mismatch.  Override with
# -DSMC_CXX_STANDARD=<17|20|23> on the cmake line if you really need to pin.
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Treat all SMC code as warning-clean.
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# External dependencies (paths from the developer's environment)
# ---------------------------------------------------------------------------
# Whisper ISS (pre-built static archive: build-${CMAKE_HOST_SYSTEM_NAME}/librvcore.a)
# Whisper's GNUmakefile picks its build directory from `uname -s`, so on
# Linux you get build-Linux/, on macOS build-Darwin/.  CMake's
# CMAKE_HOST_SYSTEM_NAME tracks the same value.  Allow an explicit override
# via WHISPER_BUILD_DIR for unusual setups.
set(WHISPER_HOME "$ENV{WHISPER_HOME}" CACHE PATH "Path to Whisper source/build tree")
if(NOT DEFINED WHISPER_BUILD_DIR)
    if(DEFINED ENV{WHISPER_BUILD_DIR})
        set(WHISPER_BUILD_DIR "$ENV{WHISPER_BUILD_DIR}")
    else()
        set(WHISPER_BUILD_DIR "build-${CMAKE_HOST_SYSTEM_NAME}")
    endif()
endif()
if(NOT WHISPER_HOME OR NOT EXISTS "${WHISPER_HOME}/${WHISPER_BUILD_DIR}/librvcore.a")
    message(FATAL_ERROR
        "WHISPER_HOME must point to a built Whisper tree containing "
        "${WHISPER_BUILD_DIR}/librvcore.a (got: '${WHISPER_HOME}').\n"
        "Build Whisper first:\n"
        "    cd \$WHISPER_HOME && make MEM_CALLBACKS=1 EXTRA_CXXFLAGS=-std=gnu++20\n"
        "Override the build subdir with -DWHISPER_BUILD_DIR=<name> if your "
        "Whisper layout differs.")
endif()

# SystemC -- the cluster's C++ standard is auto-detected from this library's
# guard symbol (see smc_detect_systemc_cxx_std()), so any of C++17/20/23 are
# accepted as long as SystemC and Whisper agree.
set(SYSTEMC_HOME "$ENV{SYSTEMC_HOME}" CACHE PATH "Path to SystemC install root")
if(NOT SYSTEMC_HOME OR NOT EXISTS "${SYSTEMC_HOME}/include/systemc.h")
    message(FATAL_ERROR
        "SYSTEMC_HOME must point to a SystemC install containing "
        "include/systemc.h (got: '${SYSTEMC_HOME}')")
endif()

# Boost (required by Whisper headers and link)
set(BOOST_DIR "$ENV{BOOST_DIR}" CACHE PATH "Path to Boost install root")
if(NOT BOOST_DIR OR NOT EXISTS "${BOOST_DIR}/include/boost/version.hpp")
    message(FATAL_ERROR
        "BOOST_DIR must point to a Boost install containing "
        "include/boost/version.hpp (got: '${BOOST_DIR}')")
endif()

message(STATUS "WHISPER_HOME = ${WHISPER_HOME}")
message(STATUS "SYSTEMC_HOME = ${SYSTEMC_HOME}")
message(STATUS "BOOST_DIR    = ${BOOST_DIR}")

# ---------------------------------------------------------------------------
# SystemC imported target (SystemC::systemc)
#
# Cross-platform discovery: SystemC ships as `libsystemc.{so,dylib,a}` under
# either `lib/` (macOS / generic Unix layout / CMake config install) or
# `lib64/` (most Linux distros / 64-bit ARCH layout).  We let find_library
# pick the right file regardless of host.
#
# We use the Accellera-canonical name `SystemC::systemc` (CamelCase) so that
# (a) the SmcSystemCStd helper below can probe its IMPORTED_LOCATION, and
# (b) the peripherals' CMakeLists.txt (which also create SystemC::systemc)
#     and this cluster's top-level integration build can share one target
#     namespace.
# ---------------------------------------------------------------------------
if(NOT TARGET SystemC::systemc)
find_library(SC_LIB
    NAMES systemc
    HINTS "${SYSTEMC_HOME}/lib"
          "${SYSTEMC_HOME}/lib64"
          "${SYSTEMC_HOME}/lib-linux64"
          "${SYSTEMC_HOME}/lib-macosx"
          "${SYSTEMC_HOME}/lib-macosarm64"
    NO_DEFAULT_PATH
    REQUIRED)

add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
set_target_properties(SystemC::systemc PROPERTIES
    IMPORTED_LOCATION             "${SC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_HOME}/include"
)
message(STATUS "SystemC library = ${SC_LIB}")
endif()

# Match the C++ standard SystemC was built with (probes guard symbol in
# libsystemc.{so,dylib,a} -- see smc/cmake/SmcSystemCStd.cmake).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# ---------------------------------------------------------------------------
# Boost imported target (boost::headers) — header-only is sufficient for SMC
# build; Whisper's own .a already contains the Boost program_options symbols
# it needs from when it was linked into librvcore.a.
# ---------------------------------------------------------------------------
add_library(boost::headers INTERFACE IMPORTED GLOBAL)
set_target_properties(boost::headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BOOST_DIR}/include"
)

# Boost program_options shared lib — librvcore.a references it (via Whisper's
# Args.cpp), and it must land AFTER that archive on the link line, which is what
# the whisper::whisper append in external/whisper-cmake/CMakeLists.txt arranges.
#
# Search every lib layout a Boost install may use, because NO_DEFAULT_PATH keeps
# us to BOOST_DIR (so we cannot pick a system Boost that disagrees with
# BOOST_DIR's headers): <root>/lib for source/TT installs, <root>/lib64 on RHEL,
# <root>/lib/<arch> for Debian/Ubuntu multiarch, and <root>/stage/lib for an
# in-tree b2 build.  The multiarch case is not hypothetical: CI passes
# BOOST_DIR=/usr on Ubuntu, where the library is in
# /usr/lib/x86_64-linux-gnu — searching only <root>/lib found nothing, the
# target below was never created, and smc-vp failed to link with undefined
# boost::program_options symbols.
# CMAKE_LIBRARY_ARCHITECTURE only carries the triplet on Debian-family hosts
# (it is empty on RHEL), so glob the multiarch directories too rather than
# depending on it.
file(GLOB _boost_multiarch_libdirs "${BOOST_DIR}/lib/*-linux-gnu")
find_library(BOOST_PROGRAM_OPTIONS_LIB
    NAMES boost_program_options
    PATHS "${BOOST_DIR}/lib"
          "${BOOST_DIR}/lib64"
          "${BOOST_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
          ${_boost_multiarch_libdirs}
          "${BOOST_DIR}/stage/lib"
    NO_DEFAULT_PATH)
if(BOOST_PROGRAM_OPTIONS_LIB)
    # UNKNOWN, not SHARED: the located library may be a static archive -- the
    # RHEL CI runner builds Boost with link=static, so only
    # libboost_program_options.a exists there.
    add_library(boost::program_options UNKNOWN IMPORTED GLOBAL)
    set_target_properties(boost::program_options PROPERTIES
        IMPORTED_LOCATION             "${BOOST_PROGRAM_OPTIONS_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${BOOST_DIR}/include"
    )
    message(STATUS "BOOST_PO_LIB = ${BOOST_PROGRAM_OPTIONS_LIB}")
else()
    message(STATUS "BOOST_PO_LIB = not found under ${BOOST_DIR} "
                   "(whisper-cmake will fall back to Boost::program_options)")
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation (component-developer-guide § IP model conventions)
#   cmake ... -DENABLE_ASAN=ON      → AddressSanitizer
#   cmake ... -DENABLE_COVERAGE=ON  → gcov (GCC) or LLVM coverage (Clang)
# ---------------------------------------------------------------------------
option(ENABLE_ASAN     "Build with AddressSanitizer"     OFF)
option(ENABLE_COVERAGE "Build with source-level coverage" OFF)

if(ENABLE_ASAN AND ENABLE_COVERAGE)
    message(FATAL_ERROR
        "ENABLE_ASAN and ENABLE_COVERAGE cannot both be ON.")
endif()
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options   (-fsanitize=address)
endif()
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(STATUS "LLVM source-based coverage enabled (Clang)")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -g)
        add_link_options   (-fprofile-instr-generate)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "llvm")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "gcov coverage enabled (GCC)")
        add_compile_options(--coverage -g -O0 -fprofile-arcs -ftest-coverage)
        add_link_options   (--coverage)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "gcov")
    else()
        message(FATAL_ERROR "ENABLE_COVERAGE requires GCC or Clang.")
    endif()
endif()

# ---------------------------------------------------------------------------
# SystemC CCI (required — smc_cpu_cluster exposes cci_param configuration)
# ---------------------------------------------------------------------------
if(DEFINED ENV{CCI_HOME})
    set(CCI_HOME "$ENV{CCI_HOME}" CACHE PATH "Path to SystemC CCI install")
endif()
if(NOT CCI_HOME OR NOT EXISTS "${CCI_HOME}/include/cci_configuration")
    message(FATAL_ERROR
        "CCI not found at CCI_HOME=${CCI_HOME}. "
        "Set -DCCI_HOME=<path> or export CCI_HOME (must contain "
        "include/cci_configuration and lib[64]/libcci-config.{so,dylib}).")
endif()
message(STATUS "CCI_HOME = ${CCI_HOME}")

find_library(CCI_CONFIG_LIB
    NAMES cci cci-config
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64"
    NO_DEFAULT_PATH
    REQUIRED)
find_library(CCI_INSPECTION_LIB
    NAMES cci cci-inspection
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64"
    NO_DEFAULT_PATH
    REQUIRED)

if(NOT TARGET SystemC::cci)
    add_library(SystemC::cci INTERFACE IMPORTED GLOBAL)
    set_target_properties(SystemC::cci PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CCI_HOME}/include"
        INTERFACE_LINK_LIBRARIES      "${CCI_CONFIG_LIB};${CCI_INSPECTION_LIB}")
endif()

# ---------------------------------------------------------------------------
# Optional: PLIC integration target.
#
# When SMC_BUILD_PLIC_INTEGRATION is ON we build the real SMC PLIC IP
# (peripherals/plic/src/plic.cpp) as a sibling static library and expose a
# new GoogleTest binary that wires the cluster's `mmio` socket to the PLIC's
# `reg_socket`, and the PLIC's `ctx_out` to the cluster's `irq_ext` ports.
#
# This requires SystemC CCI (the PLIC declares cci::cci_param<...> for its
# configurable parameters).  Set CCI_HOME (env or -D) to point at an
# Accellera CCI install containing include/cci_configuration and the
# libcci-config / libcci-inspection shared libraries (either lib/ or lib64/).
# ---------------------------------------------------------------------------
option(SMC_BUILD_PLIC_INTEGRATION "Build the cluster<->PLIC integration test" ON)

set(SMC_PLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../peripherals/plic"
    CACHE PATH "Path to the SMC PLIC IP (peripherals/plic)")

if(SMC_BUILD_PLIC_INTEGRATION)
    if(NOT EXISTS "${SMC_PLIC_DIR}/src/plic.cpp")
        message(FATAL_ERROR
            "SMC_BUILD_PLIC_INTEGRATION=ON but PLIC sources not found at "
            "${SMC_PLIC_DIR}/src/plic.cpp.  Set -DSMC_PLIC_DIR=<path>.")
    endif()

    # Compile the PLIC IP directly into a sibling static library.  This
    # avoids pulling in the PLIC's own CMakeLists (which has its own
    # SystemC discovery + standalone test bench).  We deliberately use a
    # different target name (`smc_plic_ip`) to avoid colliding with the
    # PLIC project's `smc_plic` target if the user ever does
    # add_subdirectory(${SMC_PLIC_DIR}).
    add_library(smc_plic_ip STATIC "${SMC_PLIC_DIR}/src/plic.cpp")

    target_include_directories(smc_plic_ip
        PUBLIC
            "${SMC_PLIC_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            # smc_axi_extension.h (canonical) + sim_log.h from the shared
            # common/ trees (smc-scoped and top-level).
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include"
    )

    # CCI / SystemC dylib paths at run-time -- both live outside the standard
    # loader search list, so we bake an RPATH into every consumer.  We list
    # both `lib` (macOS / CMake-installed Unix) and `lib64` (typical Linux
    # distro) so the binary is portable across hosts.
    set_target_properties(smc_plic_ip PROPERTIES
        BUILD_RPATH   "${CCI_HOME}/lib;${CCI_HOME}/lib64;${SYSTEMC_HOME}/lib;${SYSTEMC_HOME}/lib64"
        INSTALL_RPATH "${CCI_HOME}/lib;${CCI_HOME}/lib64;${SYSTEMC_HOME}/lib;${SYSTEMC_HOME}/lib64")

    target_link_libraries(smc_plic_ip
        PUBLIC
            SystemC::systemc
            SystemC::cci)
endif()

# ---------------------------------------------------------------------------
# libsmc_cpu_cluster (same layout as peripherals/plic: include/ + src/)
# ---------------------------------------------------------------------------
add_subdirectory(external/whisper-cmake)

add_library(smc_cpu_cluster STATIC
    src/iss_backend_whisper.cpp
    src/smc_cpu_cluster.cpp
)

target_include_directories(smc_cpu_cluster
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)

target_link_libraries(smc_cpu_cluster
    PUBLIC
        whisper::whisper
        SystemC::systemc
        SystemC::cci
        boost::headers
)

target_sources(smc_cpu_cluster
    PRIVATE
        include/iss_hart.h
        include/iss_backend_whisper.h
        include/smc_cpu_cluster.h
)

# ---------------------------------------------------------------------------
# Tests (self-checking cluster_tb, per 03_SMC_Test_Plan.pdf §A.1, §A.6)
# Built only when this is the top-level project.
# ---------------------------------------------------------------------------
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(test)
endif()
