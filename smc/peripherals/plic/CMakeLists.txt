# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# SMC PLIC -- SystemC/TLM-2.0 LT model with SystemC CCI support.
#
# Builds a static library libsmc_plic.a plus a self-checking test bench.
# Drop-in replacement for the eventual top-level libsmc.a target as
# documented in 02_SMC_IP_LowLevel_Design.md "Build & packaging".
#
# CCI support: set CCI_HOME or the environment variable CCI_HOME to point
# at an Accellera SystemC CCI installation (default: /usr/local/cci).

cmake_minimum_required(VERSION 3.16)
project(smc_plic CXX)

# CMAKE_CXX_STANDARD is set by smc_detect_systemc_cxx_std() below, after
# SystemC discovery, so we always match the standard the linked SystemC
# library was built with (avoids the `sc_api_version_*_cxxNNNNNNL` link-time
# mismatch).  Override with -DSMC_CXX_STANDARD=<17|20|23> on the cmake line.

# Emit compile_commands.json so Cursor / VS Code (clangd or the MS C/C++
# extension) can resolve SystemC / CCI headers and do "Go to Definition",
# "Find References", hover, and autocomplete on every symbol used here.
# A symlink at peripherals/<ip>/compile_commands.json points clangd at this.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation.
#   cmake ... -DENABLE_ASAN=ON      → AddressSanitizer (memory-error detection)
#   cmake ... -DENABLE_COVERAGE=ON  → source-level line coverage
#                                     Clang/AppleClang: LLVM instrumented coverage
#                                     GCC:              gcov (--coverage)
# The two are mutually exclusive (incompatible runtime instrumentation).
# ---------------------------------------------------------------------------
option(ENABLE_ASAN     "Build with AddressSanitizer"    OFF)
option(ENABLE_COVERAGE "Build with source-level coverage" OFF)

if(ENABLE_ASAN AND ENABLE_COVERAGE)
    message(FATAL_ERROR
        "ENABLE_ASAN and ENABLE_COVERAGE cannot both be ON "
        "(incompatible compiler instrumentation).")
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
        # -O0 prevents the optimiser from merging basic blocks, which keeps
        # per-line counters accurate.
        add_compile_options(--coverage -g -O0)
        add_link_options   (--coverage)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "gcov")
    else()
        message(FATAL_ERROR
            "ENABLE_COVERAGE requires Clang or GCC; "
            "compiler '${CMAKE_CXX_COMPILER_ID}' is not supported.")
    endif()
endif()

# ---------------------------------------------------------------------------
# SystemC discovery.  We support either:
#   (a) a CMake-installed SystemC providing SystemC::systemc (>= 2.3.4)
#   (b) the SYSTEMC_HOME environment variable pointing at a manual install
# ---------------------------------------------------------------------------
# Prefer SYSTEMC_HOME so a host SystemC 2.3.4 CMake package cannot pair
# with CCI 1.0.2 (needs sc_time::from_string).
if(DEFINED ENV{SYSTEMC_HOME} AND EXISTS "$ENV{SYSTEMC_HOME}/include/systemc.h")
    if(NOT TARGET SystemC::systemc)
        message(STATUS "Using SystemC from SYSTEMC_HOME=$ENV{SYSTEMC_HOME}")
        add_library(SystemC::systemc UNKNOWN IMPORTED)
        find_library(SC_LIB NAMES systemc
            HINTS $ENV{SYSTEMC_HOME}/lib
                  $ENV{SYSTEMC_HOME}/lib64
                  $ENV{SYSTEMC_HOME}/lib-linux64
                  $ENV{SYSTEMC_HOME}/lib-macosx
                  $ENV{SYSTEMC_HOME}/lib-macosarm64
            NO_DEFAULT_PATH REQUIRED
        set_target_properties(SystemC::systemc PROPERTIES
            IMPORTED_LOCATION             "${SC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{SYSTEMC_HOME}/include")
    endif()
else()
    find_package(SystemCLanguage QUIET CONFIG)
endif()

# ---------------------------------------------------------------------------
# CCI (SystemC Configuration, Control and Inspection) discovery.
#
# Priority:
#   1. -DCCI_HOME=<path> on the cmake command line
#   2. CCI_HOME environment variable
#   3. Hard-coded default (/usr/local/cci)
#
# Avoids chaining through find_dependency(SystemCLanguage) inside the CCI
# package by creating the imported target manually.
# ---------------------------------------------------------------------------
if(DEFINED ENV{CCI_HOME})
    set(CCI_HOME "$ENV{CCI_HOME}" CACHE PATH "Path to SystemC CCI installation")
endif()
if(NOT DEFINED CCI_HOME)
    set(CCI_HOME "/usr/local/cci" CACHE PATH "Path to SystemC CCI installation")
endif()
message(STATUS "Using CCI from CCI_HOME=${CCI_HOME}")
if(NOT EXISTS "${CCI_HOME}/include/cci_configuration")
    message(FATAL_ERROR
        "CCI header not found at ${CCI_HOME}/include/cci_configuration. "
        "Set -DCCI_HOME=<path> or export CCI_HOME.")
endif()
# Portable CCI lib discovery: search both `lib/` (macOS / generic) and
# `lib64/` (most Linux distros), and accept either `.so` (Linux) or
# `.dylib` (macOS) by letting find_library pick the right extension.
find_library(CCI_CONFIG_LIB
    NAMES cci cci-config
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" "${CCI_HOME}/lib-linux64"
          "${CCI_HOME}/lib-macosx" "${CCI_HOME}/lib-macosarm64"
    NO_DEFAULT_PATH
    REQUIRED)
find_library(CCI_INSPECTION_LIB
    NAMES cci cci-inspection
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" "${CCI_HOME}/lib-linux64"
          "${CCI_HOME}/lib-macosx" "${CCI_HOME}/lib-macosarm64"
    NO_DEFAULT_PATH
    REQUIRED)
if(NOT TARGET SystemC::cci)
add_library(SystemC::cci INTERFACE IMPORTED)
set_target_properties(SystemC::cci PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CCI_HOME}/include"
    INTERFACE_LINK_LIBRARIES       "${CCI_CONFIG_LIB};${CCI_INSPECTION_LIB}")
endif()

if(NOT TARGET SystemC::systemc)
    if(DEFINED ENV{SYSTEMC_HOME})
        message(STATUS "Using SystemC from SYSTEMC_HOME=$ENV{SYSTEMC_HOME}")
        add_library(SystemC::systemc UNKNOWN IMPORTED)
        find_library(SC_LIB NAMES systemc HINTS $ENV{SYSTEMC_HOME}/lib
                                                 $ENV{SYSTEMC_HOME}/lib64
                                                 $ENV{SYSTEMC_HOME}/lib-linux64
                                                 $ENV{SYSTEMC_HOME}/lib-macosx
                                                 $ENV{SYSTEMC_HOME}/lib-macosarm64
                                                 REQUIRED)
        set_target_properties(SystemC::systemc PROPERTIES
            IMPORTED_LOCATION             "${SC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{SYSTEMC_HOME}/include")
    else()
        message(FATAL_ERROR
            "SystemC not found.  Either install Accellera SystemC with "
            "CMake config, or set SYSTEMC_HOME.")
    endif()
endif()

# Match the C++ standard SystemC was built with (see smc/cmake/SmcSystemCStd.cmake).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# Canonical smc_axi_extension.h (shared across the SMC tree).
# Not cached: a stale CACHE entry from before the header moved leaves
# incremental build_asan/ / build_cov/ compiling against a missing -I.
include(SmcAxiExtension)

# ---------------------------------------------------------------------------
# libsmc_plic
# ---------------------------------------------------------------------------
add_library(smc_plic STATIC
    src/plic.cpp)

target_include_directories(smc_plic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
        ${SMC_AXI_EXTENSION_DIR})

target_link_libraries(smc_plic PUBLIC SystemC::systemc SystemC::cci)

target_compile_options(smc_plic PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

# ---------------------------------------------------------------------------
# Self-checking test bench
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
