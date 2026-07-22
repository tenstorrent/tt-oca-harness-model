# SMC top-level SystemC/TLM-2.0 platform integration.
#
# Instantiates the fabric + every modeled peripheral (+ optionally the
# Whisper-backed CPU cluster) and wires them into `smc_platform`.  This is a
# standalone CMake project that add_subdirectory()'s each SMC IP so it can link
# the per-IP static libraries (smc_uart, smc_plic, ...).  Mirrors the build
# conventions of peripherals/uart.
#
# Build:
#   SYSTEMC_HOME=/Users/pdroy/local/systemc-3.0.2-cxx20 \
#   CCI_HOME=/Users/pdroy/local/cci-cxx20 \
#   cmake -S . -B build -DSMC_CXX_STANDARD=20
#   cmake --build build
#
# Cluster (firmware-on-CPU) support requires Whisper + Boost:
#   -DSMC_PLATFORM_WITH_CLUSTER=ON -DWHISPER_HOME=... -DBOOST_DIR=...

cmake_minimum_required(VERSION 3.16)
project(smc_platform CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation (mutually exclusive; see test-coverage-asan rule).
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
# SystemC discovery (same pattern as peripherals/uart).
# ---------------------------------------------------------------------------
find_package(SystemCLanguage QUIET CONFIG)

if(DEFINED ENV{CCI_HOME})
    set(CCI_HOME "$ENV{CCI_HOME}" CACHE PATH "Path to SystemC CCI installation")
endif()
if(NOT DEFINED CCI_HOME)
    set(CCI_HOME "/Users/pdroy/cci" CACHE PATH "Path to SystemC CCI installation")
endif()
message(STATUS "Using CCI from CCI_HOME=${CCI_HOME}")
if(NOT EXISTS "${CCI_HOME}/include/cci_configuration")
    message(FATAL_ERROR
        "CCI header not found at ${CCI_HOME}/include/cci_configuration. "
        "Set -DCCI_HOME=<path> or export CCI_HOME.")
endif()
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

# Canonical smc_axi_extension.h + repo common headers (reg_access.h, sim_log.h).
set(SMC_AXI_EXTENSION_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
    CACHE PATH "Directory containing smc_axi_extension.h")
set(SMC_REPO_COMMON_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include"
    CACHE PATH "Directory containing reg_access.h / sim_log.h")

# ---------------------------------------------------------------------------
# CPU cluster (Whisper + Boost) — optional.  Auto-disable when the deps are
# not available, mirroring run_all_smc_tests.sh's _cpu_cluster_ready gate.
# ---------------------------------------------------------------------------
option(SMC_PLATFORM_WITH_CLUSTER
        "Wire smc_cpu_cluster into the platform (requires Whisper + Boost)" ON)

if(SMC_PLATFORM_WITH_CLUSTER)
    set(WHISPER_HOME "$ENV{WHISPER_HOME}" CACHE PATH "Path to built Whisper tree")
    set(BOOST_DIR    "$ENV{BOOST_DIR}"    CACHE PATH "Path to Boost install root")
    if(NOT WHISPER_HOME OR NOT EXISTS "${WHISPER_HOME}"
        OR NOT BOOST_DIR OR NOT EXISTS "${BOOST_DIR}")
        message(STATUS
            "SMC_PLATFORM_WITH_CLUSTER=ON but WHISPER_HOME/BOOST_DIR not set or "
            "missing; building platform WITHOUT the CPU cluster.")
        set(SMC_PLATFORM_WITH_CLUSTER OFF)
    else()
        message(STATUS "SMC platform: CPU cluster ENABLED (Whisper + Boost)")
    endif()
endif()

# ---------------------------------------------------------------------------
# Bring in every SMC IP as a subdirectory so its static library target exists.
# Each IP is a standalone project; nesting them here is supported.  The IPs
# guard their own enable_testing()/add_subdirectory(test) on being the top
# project, so their per-IP test binaries are NOT built here.
# ---------------------------------------------------------------------------
set(SMC_IP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../peripherals")

add_subdirectory(${SMC_IP_DIR}/bootrom          ${CMAKE_BINARY_DIR}/ip_bootrom)
add_subdirectory(${SMC_IP_DIR}/scratchpad_ram   ${CMAKE_BINARY_DIR}/ip_scratchpad)
add_subdirectory(${SMC_IP_DIR}/plic             ${CMAKE_BINARY_DIR}/ip_plic)
add_subdirectory(${SMC_IP_DIR}/clint            ${CMAKE_BINARY_DIR}/ip_clint)
add_subdirectory(${SMC_IP_DIR}/reset_unit       ${CMAKE_BINARY_DIR}/ip_reset_unit)
add_subdirectory(${SMC_IP_DIR}/uart             ${CMAKE_BINARY_DIR}/ip_uart)
add_subdirectory(${SMC_IP_DIR}/i2c_controller   ${CMAKE_BINARY_DIR}/ip_i2c)
add_subdirectory(${SMC_IP_DIR}/i3c_controller  ${CMAKE_BINARY_DIR}/ip_i3c)
add_subdirectory(${SMC_IP_DIR}/cpu_ctrl         ${CMAKE_BINARY_DIR}/ip_cpu_ctrl)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../smc_fabric ${CMAKE_BINARY_DIR}/ip_smc_fabric)

if(SMC_PLATFORM_WITH_CLUSTER)
    # The cluster's own PLIC-integration test target (smc_plic_ip) is redundant
    # here; disable it to avoid double-compiling plic.cpp.
    set(SMC_BUILD_PLIC_INTEGRATION OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cpu_cluster ${CMAKE_BINARY_DIR}/ip_cpu_cluster)
endif()

# ---------------------------------------------------------------------------
# libsmc_platform
# ---------------------------------------------------------------------------
add_library(smc_platform STATIC
    src/smc_platform.cpp)

target_include_directories(smc_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SMC_AXI_EXTENSION_DIR}
        ${SMC_REPO_COMMON_INCLUDE})

target_link_libraries(smc_platform
    PUBLIC
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
        SystemC::systemc
        SystemC::cci)

if(SMC_PLATFORM_WITH_CLUSTER)
    target_link_libraries(smc_platform PUBLIC smc_cpu_cluster)
    target_compile_definitions(smc_platform PUBLIC SMC_PLATFORM_WITH_CLUSTER=1)
endif()

target_compile_options(smc_platform PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

# ---------------------------------------------------------------------------
# Self-checking test bench
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
