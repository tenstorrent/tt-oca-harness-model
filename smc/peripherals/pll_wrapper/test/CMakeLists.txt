add_executable(pll_wrapper_tb pll_wrapper_tb.cpp)
target_link_libraries(pll_wrapper_tb PRIVATE smc_pll_wrapper SystemC::cci)
target_compile_options(pll_wrapper_tb PRIVATE -Wall -Wextra -Wpedantic
                                              -Wno-deprecated-declarations)

add_test(NAME pll_wrapper_tb COMMAND pll_wrapper_tb)
set_tests_properties(pll_wrapper_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "^FAIL ")
