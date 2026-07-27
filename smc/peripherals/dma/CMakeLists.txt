# SMC DMA -- SystemC/TLM-2.0 LT model with SystemC CCI support.
#
# Builds a static library libsmc_dma.a plus a self-checking test bench.
# Mirrors the build conventions of peripherals/uart.
#
# CCI support: set CCI_HOME or the environment variable CCI_HOME to point
# at an Accellera SystemC CCI installation.

cmake_minimum_required(VERSION 3.16)
project(smc_dma CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

set(SMC_AXI_EXTENSION_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include"
    CACHE PATH "Directory containing smc_axi_extension.h")

add_library(smc_dma STATIC
    src/dma.cpp)

target_include_directories(smc_dma
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
        ${SMC_AXI_EXTENSION_DIR})

target_link_libraries(smc_dma PUBLIC SystemC::systemc SystemC::cci)

target_compile_options(smc_dma PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

enable_testing()
add_subdirectory(test)
