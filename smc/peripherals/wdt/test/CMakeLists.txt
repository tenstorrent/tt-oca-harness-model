# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(wdt_tb wdt_tb.cpp)
target_link_libraries(wdt_tb PRIVATE smc_wdt SystemC::cci)
target_compile_options(wdt_tb PRIVATE -Wall -Wextra -Wpedantic
                                       -Wno-deprecated-declarations)

add_test(NAME wdt_tb COMMAND wdt_tb)
set_tests_properties(wdt_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# wdt_tick_tb — separate binary with tick_period_ns > 0 (auto-tick paths).
add_executable(wdt_tick_tb wdt_tick_tb.cpp)
target_link_libraries(wdt_tick_tb PRIVATE smc_wdt SystemC::cci)
target_compile_options(wdt_tick_tb PRIVATE -Wall -Wextra -Wpedantic
                                            -Wno-deprecated-declarations)

add_test(NAME wdt_tick_tb COMMAND wdt_tick_tb)
set_tests_properties(wdt_tick_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
