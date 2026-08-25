# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(cpu_ctrl_tb cpu_ctrl_tb.cpp)
target_link_libraries(cpu_ctrl_tb PRIVATE smc_cpu_ctrl SystemC::cci)
target_compile_options(cpu_ctrl_tb PRIVATE -Wall -Wextra -Wpedantic
                                           -Wno-deprecated-declarations)

add_test(NAME cpu_ctrl_tb COMMAND cpu_ctrl_tb)
set_tests_properties(cpu_ctrl_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
