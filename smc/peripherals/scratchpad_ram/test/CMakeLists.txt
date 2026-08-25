# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# ---------------------------------------------------------------------------
# scratchpad_ram_tb — primary self-checking bench: read-write semantics,
# sub-word + byte-enable accesses, preload, reset retention, CCI, ECC scrub.
# ---------------------------------------------------------------------------
add_executable(scratchpad_ram_tb scratchpad_ram_tb.cpp)
target_link_libraries(scratchpad_ram_tb PRIVATE smc_scratchpad_ram SystemC::cci)
target_compile_options(scratchpad_ram_tb PRIVATE -Wall -Wextra -Wpedantic
                                                 -Wno-deprecated-declarations)

# Provide the fixture directory so the bench can locate the preload image
# regardless of the working directory at test time.
target_compile_definitions(scratchpad_ram_tb PRIVATE
    SCRATCHPAD_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME scratchpad_ram_tb COMMAND scratchpad_ram_tb)

# FAIL_REGULAR_EXPRESSION must NOT match the CCI library's intentional
# "SET_PARAM_FAILED" message that the immutable-write test deliberately
# triggers.  Restrict to our own EXPECT_* output patterns.
set_tests_properties(scratchpad_ram_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# scratchpad_ram_neg_tb — negative-path / edge-case bench.  Exercises the
# SC_REPORT_FATAL constructor branches (bad size / alignment / delay,
# unreadable / malformed / empty preload), the uncorrectable-ECC read path,
# and the dbg_read32 / format-fallback / 0x-prefix parser branches.
# ---------------------------------------------------------------------------
add_executable(scratchpad_ram_neg_tb scratchpad_ram_neg_tb.cpp)
target_link_libraries(scratchpad_ram_neg_tb PRIVATE smc_scratchpad_ram SystemC::cci)
target_compile_options(scratchpad_ram_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                     -Wno-deprecated-declarations)
add_test(NAME scratchpad_ram_neg_tb COMMAND scratchpad_ram_neg_tb)
set_tests_properties(scratchpad_ram_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
