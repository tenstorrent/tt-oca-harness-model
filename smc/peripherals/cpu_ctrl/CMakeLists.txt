# SMC CPU Control -- SystemC/TLM-2.0 LT model with SystemC CCI support.

cmake_minimum_required(VERSION 3.16)
project(smc_cpu_ctrl CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(ENABLE_ASAN     "Build with AddressSanitizer"      OFF)
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

find_package(SystemCLanguage QUIET CONFIG)

if(DEFINED ENV{CCI_HOME})
    set(CCI_HOME "$ENV{CCI_HOME}" CACHE PATH "Path to SystemC CCI installation")
endif()
if(NOT DEFINED CCI_HOME)
    set(CCI_HOME "/Users/pdroy/cci" CACHE PATH "Path to SystemC CCI installation")
endif()
message(STATUS "Using CCI from CCI_HOME=${CCI_HOME}")

find_library(CCI_CONFIG_LIB NAMES cci cci-config
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" NO_DEFAULT_PATH REQUIRED)
find_library(CCI_INSPECTION_LIB NAMES cci cci-inspection
    HINTS "${CCI_HOME}/lib" "${CCI_HOME}/lib64" NO_DEFAULT_PATH REQUIRED)
add_library(SystemC::cci INTERFACE IMPORTED)
set_target_properties(SystemC::cci PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CCI_HOME}/include"
    INTERFACE_LINK_LIBRARIES       "${CCI_CONFIG_LIB};${CCI_INSPECTION_LIB}")

if(NOT TARGET SystemC::systemc)
    if(DEFINED ENV{SYSTEMC_HOME})
        add_library(SystemC::systemc UNKNOWN IMPORTED)
        find_library(SC_LIB NAMES systemc HINTS $ENV{SYSTEMC_HOME}/lib
                                                 $ENV{SYSTEMC_HOME}/lib64
                                                 $ENV{SYSTEMC_HOME}/lib-linux64
                                                 REQUIRED)
        set_target_properties(SystemC::systemc PROPERTIES
            IMPORTED_LOCATION             "${SC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{SYSTEMC_HOME}/include")
    else()
        message(FATAL_ERROR "SystemC not found. Set SYSTEMC_HOME.")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(SmcSystemCStd)
smc_detect_systemc_cxx_std()

set(SMC_AXI_EXTENSION_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cpu_cluster/include"
    CACHE PATH "Directory containing smc_axi_extension.h")

add_library(smc_cpu_ctrl STATIC src/cpu_ctrl.cpp)
target_include_directories(smc_cpu_ctrl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
        ${SMC_AXI_EXTENSION_DIR})
target_link_libraries(smc_cpu_ctrl PUBLIC SystemC::systemc SystemC::cci)
target_compile_options(smc_cpu_ctrl PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

enable_testing()
add_subdirectory(test)
