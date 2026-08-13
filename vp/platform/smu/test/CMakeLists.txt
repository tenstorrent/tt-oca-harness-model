# =============================================================================
# vp/platform/smu/test/CMakeLists.txt
#
# Standalone unit tests for the SMU on-die interconnect models
# (smu_axi_xbar.h, axi_window_remap.h — header-only, SystemC/TLM + CCI only;
# no Whisper/VeeR linkage, so this project builds without WHISPER_HOME).
#
# Driven by ../run_tests.sh (Release / --asan / --coverage, isolated build
# dirs), following the smc/peripherals convention.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(smu_interconnect_tests CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation (mutually exclusive; see run_tests.sh).
# ---------------------------------------------------------------------------
option(ENABLE_ASAN     "Build with AddressSanitizer"      OFF)
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
# SystemC / CCI discovery (same conventions as smc/peripherals/*).
# ---------------------------------------------------------------------------
find_package(SystemCLanguage QUIET CONFIG)

if(DEFINED ENV{CCI_HOME})
    set(CCI_HOME "$ENV{CCI_HOME}" CACHE PATH "Path to SystemC CCI installation")
endif()
if(NOT DEFINED CCI_HOME)
    set(CCI_HOME "/Users/pdroy/cci" CACHE PATH "Path to SystemC CCI installation")
endif()
if(NOT EXISTS "${CCI_HOME}/include/cci_configuration")
    message(FATAL_ERROR
        "CCI header not found at ${CCI_HOME}/include/cci_configuration. "
        "Set -DCCI_HOME=<path> or export CCI_HOME.")
endif()
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

# Match the C++ standard SystemC was built with (C++20 in this repo).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../smc/cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# ---------------------------------------------------------------------------
# Test bench
# ---------------------------------------------------------------------------
add_executable(smu_interconnect_tb smu_interconnect_tb.cpp)

target_include_directories(smu_interconnect_tb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc          # smu_axi_xbar.h, axi_window_remap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/include)  # sim_log.h

target_link_libraries(smu_interconnect_tb PRIVATE SystemC::systemc SystemC::cci)

target_compile_options(smu_interconnect_tb PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

# CCI / SystemC dylibs live outside the standard loader path.
set(_rpath "${CCI_HOME}/lib;${CCI_HOME}/lib64")
if(DEFINED ENV{SYSTEMC_HOME})
    list(APPEND _rpath "$ENV{SYSTEMC_HOME}/lib" "$ENV{SYSTEMC_HOME}/lib64")
endif()
set_target_properties(smu_interconnect_tb PROPERTIES
    BUILD_RPATH   "${_rpath}"
    INSTALL_RPATH "${_rpath}")

enable_testing()
add_test(NAME smu_interconnect_tb COMMAND smu_interconnect_tb)
