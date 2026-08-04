add_executable(octs_system_timer_tb octs_system_timer_tb.cpp)
target_link_libraries(octs_system_timer_tb
    PRIVATE smc_octs_system_timer SystemC::cci)
target_compile_options(octs_system_timer_tb PRIVATE -Wall -Wextra -Wpedantic
                                                   -Wno-deprecated-declarations)

add_test(NAME octs_system_timer_tb COMMAND octs_system_timer_tb)
set_tests_properties(octs_system_timer_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    # Test 14 deliberately provokes an SC_WARNING about CREDIT_VAL <=
    # PULSE_WIDTH; only the harness's own "FAIL" lines are real failures.
    FAIL_REGULAR_EXPRESSION "FAIL \\[line")
