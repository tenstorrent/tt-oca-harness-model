# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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

# §A.4 coverage: gcovr over src/ when built with GCC.  Clang uses LLVM
# instrumentation (see top-level CMakeLists.txt) and llvm-cov via run_tests.sh.
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(GCOVR_EXECUTABLE gcovr)
        if(GCOVR_EXECUTABLE)
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage
                COMMAND ${GCOVR_EXECUTABLE}
                        --root "${CMAKE_SOURCE_DIR}"
                        --filter "${CMAKE_SOURCE_DIR}/src/.*"
                        --html-details "coverage/index.html"
                        --print-summary
                        --exclude-throw-branches
                        --exclude-unreachable-branches
                        .
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating gcovr coverage report (build/coverage/index.html)"
                VERBATIM)
        else()
            message(WARNING "ENABLE_COVERAGE=ON with GCC but gcovr was not found; "
                            "creating stub `coverage` target.")
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E echo
                    "gcovr not found; install gcovr or run ./run_tests.sh --coverage"
                COMMENT "gcovr unavailable — stub coverage target"
                VERBATIM)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E echo
                "Clang LLVM coverage: use ./run_tests.sh --coverage (llvm-cov, not gcovr)"
            COMMENT "LLVM coverage — use run_tests.sh --coverage"
            VERBATIM)
    endif()
endif()
