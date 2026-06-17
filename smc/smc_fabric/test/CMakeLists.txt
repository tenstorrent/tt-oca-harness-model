add_executable(smc_fabric_tb smc_fabric_tb.cpp)
target_link_libraries(smc_fabric_tb PRIVATE smc_fabric)
target_compile_options(smc_fabric_tb PRIVATE -Wall -Wextra -Wpedantic
                                             -Wno-deprecated-declarations)

add_test(NAME smc_fabric_tb COMMAND smc_fabric_tb)
set_tests_properties(smc_fabric_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
