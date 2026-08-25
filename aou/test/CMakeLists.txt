# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(aou_core_tb aou_core_tb.cpp)
target_link_libraries(aou_core_tb PRIVATE aou_core SystemC::cci)
target_compile_options(aou_core_tb PRIVATE -Wall -Wextra -Wpedantic
                                           -Wno-deprecated-declarations)

add_test(NAME aou_core_tb COMMAND aou_core_tb)
set_tests_properties(aou_core_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
