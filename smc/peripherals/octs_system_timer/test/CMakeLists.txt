# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(octs_system_timer_tb octs_system_timer_tb.cpp)
target_link_libraries(octs_system_timer_tb
    PRIVATE smc_octs_system_timer SystemC::cci)
target_compile_options(octs_system_timer_tb PRIVATE -Wall -Wextra -Wpedantic
                                                   -Wno-deprecated-declarations)

if(ENABLE_UB_CANARY)
    target_compile_definitions(octs_system_timer_tb PRIVATE OCTS_UB_CANARY=1)
endif()

add_test(NAME octs_system_timer_tb COMMAND octs_system_timer_tb)
# Process exit status is authoritative.  The bench scopes the one expected
# SC_WARNING (CREDIT_VAL <= PULSE_WIDTH) around the statement that provokes it,
# so the failure regex no longer has to be loosened to accommodate it.
set_tests_properties(octs_system_timer_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL \\[line|FAILURE")
