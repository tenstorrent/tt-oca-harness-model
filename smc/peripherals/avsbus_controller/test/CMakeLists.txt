# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# ---------------------------------------------------------------------------
# avsbus_controller_tb -- primary self-checking bench
# ---------------------------------------------------------------------------
add_executable(avsbus_controller_tb avsbus_controller_tb.cpp)
target_link_libraries(avsbus_controller_tb PRIVATE smc_avsbus_controller SystemC::cci)
target_compile_options(avsbus_controller_tb PRIVATE -Wall -Wextra -Wpedantic
                                                    -Wno-deprecated-declarations)
add_test(NAME avsbus_controller_tb COMMAND avsbus_controller_tb)
set_tests_properties(avsbus_controller_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# avsbus_controller_neg_tb -- negative-path / edge-case bench
# ---------------------------------------------------------------------------
add_executable(avsbus_controller_neg_tb avsbus_controller_neg_tb.cpp)
target_link_libraries(avsbus_controller_neg_tb PRIVATE smc_avsbus_controller SystemC::cci)
target_compile_options(avsbus_controller_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                        -Wno-deprecated-declarations)
add_test(NAME avsbus_controller_neg_tb COMMAND avsbus_controller_neg_tb)
set_tests_properties(avsbus_controller_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
