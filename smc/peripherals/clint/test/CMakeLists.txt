# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(clint_tb clint_tb.cpp)
target_link_libraries(clint_tb PRIVATE smc_clint SystemC::cci)
target_compile_options(clint_tb PRIVATE -Wall -Wextra -Wpedantic
                                        -Wno-deprecated-declarations)

add_test(NAME clint_tb COMMAND clint_tb)
# FAIL_REGULAR_EXPRESSION must NOT match the CCI library's intentional
# "SET_PARAM_FAILED" message that test 16 deliberately triggers to verify
# immutability semantics.  Restrict to our own EXPECT_* output patterns
# ("FAIL <file>:<line> ..." and "<n> FAILURE(S)").
set_tests_properties(clint_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# clint_tick_tb — separate binary that enables auto-tick (tick_period_ns > 0)
# to cover tick_method, constructor tick-arm, and reset_proc cancel/re-arm.
# ---------------------------------------------------------------------------
add_executable(clint_tick_tb clint_tick_tb.cpp)
target_link_libraries(clint_tick_tb PRIVATE smc_clint SystemC::cci)
target_compile_options(clint_tick_tb PRIVATE -Wall -Wextra -Wpedantic
                                             -Wno-deprecated-declarations)

add_test(NAME clint_tick_tb COMMAND clint_tick_tb)
set_tests_properties(clint_tick_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
