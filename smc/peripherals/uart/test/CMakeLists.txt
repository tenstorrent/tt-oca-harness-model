add_executable(uart_tb uart_tb.cpp)
target_link_libraries(uart_tb PRIVATE smc_uart SystemC::cci)
target_compile_options(uart_tb PRIVATE -Wall -Wextra -Wpedantic
                                       -Wno-deprecated-declarations)

add_test(NAME uart_tb COMMAND uart_tb)
set_tests_properties(uart_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
