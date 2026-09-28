# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# SMC I3C Controller -- SystemC/TLM-2.0 LT model with SystemC CCI support.
#
# Builds a static library libsmc_i3c_controller.a plus self-checking test
# benches.  Drop-in for the eventual top-level libsmc.a target, consistent
# with the bootrom / plic / clint / scratchpad_ram / reset_unit sub-packages
# in smc/peripherals/.
#
# CCI support: set CCI_HOME (env or -DCCI_HOME=) to an Accellera SystemC CCI
# installation (default: /usr/local/cci).

cmake_minimum_required(VERSION 3.16)
project(smc_i3c_controller CXX)

# CMAKE_CXX_STANDARD is set by smc_detect_systemc_cxx_std() below, after
# SystemC discovery, so we always match the standard the linked SystemC
# library was built with (avoids the sc_api_version_*_cxxNNNNNNL link-time
# mismatch). Override with -DSMC_CXX_STANDARD=<17|20|23> on the cmake line.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation (mutually exclusive).
#   -DENABLE_ASAN=ON      AddressSanitizer
#   -DENABLE_COVERAGE=ON  source-level coverage (LLVM on Clang, gcov on GCC)
# ---------------------------------------------------------------------------
option(ENABLE_ASAN      "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(ENABLE_COVERAGE  "Build with source-level coverage" OFF)
option(ENABLE_UB_CANARY "Compile a deliberate UB statement into the test bench"  OFF)

if(ENABLE_ASAN AND ENABLE_COVERAGE)
    message(FATAL_ERROR
        "ENABLE_ASAN and ENABLE_COVERAGE cannot both be ON "
        "(incompatible compiler instrumentation).")
endif()

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer + UndefinedBehaviorSanitizer enabled")
    # UBSan stays recoverable so a single finding does not abort the run; the
    # benches keep going and enforce_asan_clean.sh fails the build from the log.
    add_compile_options(-fsanitize=address,undefined
                        -fsanitize=float-divide-by-zero
                        -fno-omit-frame-pointer -g)
    add_link_options   (-fsanitize=address,undefined
                        -fsanitize=float-divide-by-zero)
endif()

if(ENABLE_UB_CANARY AND NOT ENABLE_ASAN)
    message(FATAL_ERROR
        "ENABLE_UB_CANARY only makes sense with ENABLE_ASAN=ON (it exists to "
        "prove the UBSan instrumentation actually reports).")
endif()

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(STATUS "LLVM source-based coverage enabled (Clang)")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -g)
        add_link_options   (-fprofile-instr-generate)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "llvm")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "gcov coverage enabled (GCC)")
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
# SystemC discovery (CMake config or SYSTEMC_HOME).
# ---------------------------------------------------------------------------
find_package(SystemCLanguage QUIET CONFIG)

# ---------------------------------------------------------------------------
# CCI discovery: -DCCI_HOME > $CCI_HOME > /usr/local/cci.
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
if(NOT TARGET SystemC::cci)
add_library(SystemC::cci INTERFACE IMPORTED)
endif()
# find_library handles CCI >= 1.0 (single libcci) and CCI < 1.0
# (split libcci-config + libcci-inspection), on macOS and Linux.
find_library(CCI_CONFIG_LIB
    NAMES cci cci-config
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" "${CCI_HOME}/lib-linux64"
          "${CCI_HOME}/lib-macosx" "${CCI_HOME}/lib-macosarm64"
    NO_DEFAULT_PATH REQUIRED)
find_library(CCI_INSPECTION_LIB
    NAMES cci cci-inspection
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" "${CCI_HOME}/lib-linux64"
          "${CCI_HOME}/lib-macosx" "${CCI_HOME}/lib-macosarm64"
    NO_DEFAULT_PATH REQUIRED)
set_target_properties(SystemC::cci PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CCI_HOME}/include"
    INTERFACE_LINK_LIBRARIES       "${CCI_CONFIG_LIB};${CCI_INSPECTION_LIB}")

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
            "SystemC not found. Either install Accellera SystemC with "
            "CMake config, or set SYSTEMC_HOME.")
    endif()
endif()

# Match the C++ standard SystemC was built with (smc/cmake/SmcSystemCStd.cmake).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# ---------------------------------------------------------------------------
# libsmc_i3c_controller
# ---------------------------------------------------------------------------
add_library(smc_i3c_controller STATIC
    src/i3c_controller.cpp)

target_include_directories(smc_i3c_controller
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include)

target_link_libraries(smc_i3c_controller PUBLIC SystemC::systemc SystemC::cci)

target_compile_options(smc_i3c_controller PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

# ---------------------------------------------------------------------------
# Self-checking test benches
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
