# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# ---------------------------------------------------------------------------
# beu_tb -- primary self-checking bench: register access (reset values, RW/RO
# semantics, field masks, 64-bit access), error injection (accrued status,
# CAUSE/PHYS_ADDR first-error latching + priority, recording-enable gating),
# local vs. PLIC interrupt aggregation and masking, SW ack/re-arm paths,
# reset, CCI introspection.
# ---------------------------------------------------------------------------
add_executable(beu_tb beu_tb.cpp)
target_link_libraries(beu_tb PRIVATE smc_beu SystemC::cci)
target_compile_options(beu_tb PRIVATE -Wall -Wextra -Wpedantic
                                      -Wno-deprecated-declarations)
add_test(NAME beu_tb COMMAND beu_tb)
set_tests_properties(beu_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# beu_neg_tb -- negative-path / edge-case bench: TLM error responses (command/
# burst/address/decode), transport_dbg read+write, RO-register write-ignore,
# unmapped-in-window decode miss, back-door peeks.  Brings line coverage
# above 95 %.
# ---------------------------------------------------------------------------
add_executable(beu_neg_tb beu_neg_tb.cpp)
target_link_libraries(beu_neg_tb PRIVATE smc_beu SystemC::cci)
target_compile_options(beu_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                          -Wno-deprecated-declarations)
add_test(NAME beu_neg_tb COMMAND beu_neg_tb)
set_tests_properties(beu_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
