add_executable(bootrom_tb bootrom_tb.cpp)
target_link_libraries(bootrom_tb PRIVATE smc_bootrom SystemC::cci)
target_compile_options(bootrom_tb PRIVATE -Wall -Wextra -Wpedantic
                                          -Wno-deprecated-declarations)

# Provide the test fixture directory to the test binary so it can locate
# `bootrom_sanity.rv64.hex` and `bootrom.rv64.img` regardless of the
# current working directory at test time.
target_compile_definitions(bootrom_tb PRIVATE
    BOOTROM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME bootrom_tb COMMAND bootrom_tb)

# FAIL_REGULAR_EXPRESSION must NOT match the CCI library's intentional
# "SET_PARAM_FAILED" message that the immutable-write test deliberately
# triggers to verify immutability semantics.  Restrict to our own EXPECT_*
# output patterns ("FAIL <file>:<line> ..." and "<n> FAILURE(S)").
set_tests_properties(bootrom_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# bootrom_bin_tb — separate binary that exercises the binary-preload path
# (init_file_format = "auto" with a `.img` filename).  Keeps the primary
# test bench focused on the hex path while covering the binary code paths.
# ---------------------------------------------------------------------------
add_executable(bootrom_bin_tb bootrom_bin_tb.cpp)
target_link_libraries(bootrom_bin_tb PRIVATE smc_bootrom SystemC::cci)
target_compile_options(bootrom_bin_tb PRIVATE -Wall -Wextra -Wpedantic
                                              -Wno-deprecated-declarations)
target_compile_definitions(bootrom_bin_tb PRIVATE
    BOOTROM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME bootrom_bin_tb COMMAND bootrom_bin_tb)
set_tests_properties(bootrom_bin_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
