# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(pll_wrapper_tb pll_wrapper_tb.cpp)
target_link_libraries(pll_wrapper_tb PRIVATE smc_pll_wrapper SystemC::cci)
target_compile_options(pll_wrapper_tb PRIVATE -Wall -Wextra -Wpedantic
                                              -Wno-deprecated-declarations)

if(ENABLE_UB_CANARY)
    target_compile_definitions(pll_wrapper_tb PRIVATE PLL_UB_CANARY=1)
endif()

add_test(NAME pll_wrapper_tb COMMAND pll_wrapper_tb)
# Process exit status is authoritative; these regexes must agree with every
# failure line the bench can print (`FAIL <file>:<line>` from EXPECT_*, and the
# `N FAILURE(S)` summary) so a CTest run cannot report PASS on a failing bench.
set_tests_properties(pll_wrapper_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL |FAILURE\\(S\\)")
