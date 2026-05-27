# Self-checking test bench (same pattern as peripherals/plic/test).

add_executable(cluster_tb cluster_tb.cpp)

target_link_libraries(cluster_tb PRIVATE smc_cpu_cluster)

target_include_directories(cluster_tb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# SystemC + CCI dylibs are not in the standard loader search path on either
# macOS or Linux, so bake an RPATH that covers both `lib` (macOS / generic
# Unix layout) and `lib64` (typical Linux distro).
set(_cluster_tb_rpath
    "${SYSTEMC_HOME}/lib"
    "${SYSTEMC_HOME}/lib64")
if(TARGET smc_plic_ip)
    target_link_libraries(cluster_tb PRIVATE smc_plic_ip SystemC::cci)
    target_compile_definitions(cluster_tb PRIVATE SMC_HAVE_PLIC=1)
    list(APPEND _cluster_tb_rpath
        "${CCI_HOME}/lib" "${CCI_HOME}/lib64")
endif()

set_target_properties(cluster_tb PROPERTIES
    BUILD_RPATH   "${_cluster_tb_rpath}"
    INSTALL_RPATH "${_cluster_tb_rpath}")

target_compile_options(cluster_tb PRIVATE
    -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)

add_test(NAME cluster_tb COMMAND cluster_tb)
set_tests_properties(cluster_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL"
    TIMEOUT 120)

# §A.4 coverage (gcovr over include/ + src/)
if(SMC_ENABLE_COVERAGE)
    find_program(GCOVR_EXECUTABLE gcovr)
    if(GCOVR_EXECUTABLE)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory coverage
            COMMAND ${GCOVR_EXECUTABLE}
                    --root "${CMAKE_SOURCE_DIR}"
                    --filter "${CMAKE_SOURCE_DIR}/(include|src)/.*"
                    --html-details "coverage/index.html"
                    --print-summary
                    --exclude-throw-branches
                    --exclude-unreachable-branches
                    .
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report (build/coverage/index.html)"
            VERBATIM)
    else()
        message(WARNING "SMC_ENABLE_COVERAGE=ON but gcovr was not found in PATH; "
                        "the `coverage` target will not be created.")
    endif()
endif()
