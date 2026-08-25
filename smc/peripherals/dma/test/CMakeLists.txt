# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(dma_tb dma_tb.cpp)
target_link_libraries(dma_tb PRIVATE smc_dma SystemC::cci)
target_compile_options(dma_tb PRIVATE -Wall -Wextra -Wpedantic
                                      -Wno-deprecated-declarations)

add_executable(dma_neg_tb dma_neg_tb.cpp)
target_link_libraries(dma_neg_tb PRIVATE smc_dma SystemC::cci)
target_compile_options(dma_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                          -Wno-deprecated-declarations)

add_test(NAME dma_tb COMMAND dma_tb)
set_tests_properties(dma_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")

add_test(NAME dma_neg_tb COMMAND dma_neg_tb)
set_tests_properties(dma_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")
