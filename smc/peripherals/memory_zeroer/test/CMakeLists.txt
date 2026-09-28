# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(memory_zeroer_tb memory_zeroer_tb.cpp)
target_link_libraries(memory_zeroer_tb PRIVATE smc_memory_zeroer SystemC::cci)
target_compile_options(memory_zeroer_tb PRIVATE -Wall -Wextra -Wpedantic
                                                -Wno-deprecated-declarations)

if(ENABLE_UB_CANARY)
    target_compile_definitions(memory_zeroer_tb PRIVATE MZ_UB_CANARY=1)
endif()

add_test(NAME memory_zeroer_tb COMMAND memory_zeroer_tb)
# Process exit status is authoritative; these regexes must agree with every
# failure line the bench can print (`FAIL <file>:<line>` from EXPECT_*, and the
# `N FAILURE(S)` summary).  The bench scopes its expected-report handling around
# the single CCI immutable-write probe, so no diagnostic has to be tolerated
# here any more.
set_tests_properties(memory_zeroer_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL |FAILURE\\(S\\)")
