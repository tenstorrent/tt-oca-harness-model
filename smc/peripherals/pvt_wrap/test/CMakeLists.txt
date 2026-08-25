# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# pvt_wrap test bench

cmake_minimum_required(VERSION 3.16)

add_executable(pvt_wrap_tb pvt_wrap_tb.cpp)

target_include_directories(pvt_wrap_tb
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include)

target_link_libraries(pvt_wrap_tb
    PRIVATE smc_pvt_wrap SystemC::systemc SystemC::cci)

target_compile_options(pvt_wrap_tb PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

add_test(NAME pvt_wrap_tb COMMAND pvt_wrap_tb)
