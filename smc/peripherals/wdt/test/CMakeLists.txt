add_executable(wdt_tb wdt_tb.cpp)
target_link_libraries(wdt_tb PRIVATE smc_wdt SystemC::cci)
target_compile_options(wdt_tb PRIVATE -Wall -Wextra -Wpedantic
                                       -Wno-deprecated-declarations)

add_test(NAME wdt_tb COMMAND wdt_tb)
set_tests_properties(wdt_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
