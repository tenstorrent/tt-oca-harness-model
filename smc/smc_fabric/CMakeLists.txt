# SMC Fabric — SystemC/TLM-2.0 LT model
#
# Builds a static library libsmc_fabric.a.
# Follows the same conventions as smc_plic / smc_clint.
#
# Usage:
#   cmake -S . -B build \
#         -DSYSTEMC_HOME=<path> \
#         [-DSMC_CXX_STANDARD=20]   # auto-detected from SystemC ABI
#
# If building as a sub-directory of the top-level SMC tree, the parent's
# find_package(SystemCLanguage) result is reused automatically.

cmake_minimum_required(VERSION 3.16)
project(smc_fabric CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation
# ---------------------------------------------------------------------------
option(ENABLE_ASAN     "Build with AddressSanitizer"     OFF)
option(ENABLE_COVERAGE "Build with source-level coverage" OFF)

if(ENABLE_ASAN AND ENABLE_COVERAGE)
    message(FATAL_ERROR
        "ENABLE_ASAN and ENABLE_COVERAGE cannot both be ON.")
endif()
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options   (-fsanitize=address)
endif()
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -g)
        add_link_options   (-fprofile-instr-generate)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "llvm")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage -g -O0)
        add_link_options   (--coverage)
        file(WRITE "${CMAKE_BINARY_DIR}/coverage_tool.txt" "gcov")
    else()
        message(FATAL_ERROR "ENABLE_COVERAGE requires Clang or GCC.")
    endif()
endif()

# ---------------------------------------------------------------------------
# SystemC discovery
# ---------------------------------------------------------------------------
find_package(SystemCLanguage QUIET CONFIG)

if(NOT TARGET SystemC::systemc)
    if(DEFINED ENV{SYSTEMC_HOME})
        message(STATUS "Using SystemC from SYSTEMC_HOME=$ENV{SYSTEMC_HOME}")
        add_library(SystemC::systemc UNKNOWN IMPORTED)
        find_library(SC_LIB NAMES systemc
            HINTS $ENV{SYSTEMC_HOME}/lib
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
            "SystemC not found. Set SYSTEMC_HOME or install SystemC with CMake.")
    endif()
endif()

# Match the C++ standard the linked SystemC library was built with.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# ---------------------------------------------------------------------------
# smc_axi_extension.h — shared header from cpu_cluster
# ---------------------------------------------------------------------------
set(SMC_AXI_EXTENSION_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../cpu_cluster/include"
    CACHE PATH "Directory containing smc_axi_extension.h")

# ---------------------------------------------------------------------------
# libsmc_fabric
# ---------------------------------------------------------------------------
add_library(smc_fabric STATIC
    src/smc_fabric.cpp
)

target_include_directories(smc_fabric
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
        ${SMC_AXI_EXTENSION_DIR}
)

target_link_libraries(smc_fabric
    PUBLIC SystemC::systemc
)

target_compile_options(smc_fabric PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-deprecated-declarations
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
endif()
