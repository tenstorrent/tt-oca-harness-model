# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mmio_poke)

target_sources(app PRIVATE src/main.c)
# Canonical SMC address map used by the bare-metal suite.
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../../smc-vp-tests/common)
