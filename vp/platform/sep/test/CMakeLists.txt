# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# vp/platform/sep/test/CMakeLists.txt
#
# Unit tests for the SEP platform:
#   xbar_policy_tb   the local-crossbar connectivity policy (inc/xbar_policy.h).
#                    Plain C++ — no SystemC, CCI, VeeR or OpenSSL.
#   abr_aperture_tb  the crossbar's Adams Bridge window against the extent the
#                    model decodes. Builds the Adams Bridge model, so it needs
#                    SystemC, CCI and OpenSSL like the peripheral benches.
#
# Driven by ../run_tests.sh (Release / --asan / --coverage, isolated build
# dirs), following the vp/platform/smu convention.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(sep_platform_tests C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options   (-fsanitize=address,undefined)
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
# Test bench
# ---------------------------------------------------------------------------
add_executable(xbar_policy_tb xbar_policy_tb.cpp)

target_include_directories(xbar_policy_tb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc)          # xbar_policy.h

target_compile_options(xbar_policy_tb PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME xbar_policy_tb COMMAND xbar_policy_tb)

# ---------------------------------------------------------------------------
# Adams Bridge aperture bench
# ---------------------------------------------------------------------------
set(_repo_root ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
list(APPEND CMAKE_MODULE_PATH ${_repo_root}/cmake)
include(FindCCI)
include(FindSystemC)
add_subdirectory(${_repo_root}/sep/peripherals/adams_bridge adams_bridge EXCLUDE_FROM_ALL)

add_executable(abr_aperture_tb abr_aperture_tb.cpp)
target_include_directories(abr_aperture_tb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../infra)     # bus.h
target_include_directories(abr_aperture_tb SYSTEM PRIVATE ${CCI_INCLUDE_DIR})
target_compile_definitions(abr_aperture_tb PRIVATE SC_ALLOW_DEPRECATED_IEEE_API)
target_link_libraries(abr_aperture_tb PRIVATE adams_bridge_model)
add_test(NAME abr_aperture_tb COMMAND abr_aperture_tb)
