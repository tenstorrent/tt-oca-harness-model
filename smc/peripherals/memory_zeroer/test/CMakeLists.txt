# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(memory_zeroer_tb memory_zeroer_tb.cpp)
target_link_libraries(memory_zeroer_tb PRIVATE smc_memory_zeroer SystemC::cci)
target_compile_options(memory_zeroer_tb PRIVATE -Wall -Wextra -Wpedantic
                                                -Wno-deprecated-declarations)

add_test(NAME memory_zeroer_tb COMMAND memory_zeroer_tb)
set_tests_properties(memory_zeroer_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    # Intentional CCI immutable-write errors must not fail the test.
    FAIL_REGULAR_EXPRESSION "^FAIL ")
