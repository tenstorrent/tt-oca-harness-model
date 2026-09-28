# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
add_executable(memory_zeroer_tb memory_zeroer_tb.cpp)
target_link_libraries(memory_zeroer_tb PRIVATE smc_memory_zeroer SystemC::cci)
target_compile_options(memory_zeroer_tb PRIVATE -Wall -Wextra -Wpedantic
                                                -Wno-deprecated-declarations)

add_test(NAME memory_zeroer_tb COMMAND memory_zeroer_tb)
# Finding 14: rely on process exit (sc_main returns non-zero on g_failures)
# plus the TB's exact chunk_size JSON check after the scoped immutable write.
# FAIL_REGULAR_EXPRESSION stays narrow so a displayed CCI SC_ERROR cannot
# false-fail CTest; assertion failures still print lines starting with "FAIL ".
set_tests_properties(memory_zeroer_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "^FAIL ")
