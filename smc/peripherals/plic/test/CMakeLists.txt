# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(plic_tb plic_tb.cpp)
target_link_libraries(plic_tb PRIVATE smc_plic SystemC::cci)
target_compile_options(plic_tb PRIVATE -Wall -Wextra -Wpedantic
                                       -Wno-deprecated-declarations)

add_test(NAME plic_tb COMMAND plic_tb)
set_tests_properties(plic_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
