# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# ---------------------------------------------------------------------------
# reset_unit_tb -- primary self-checking bench: register access, reset
# derivation, JTAG overrides, subsystem reset-control bundles, locks, FLR
# cool-reset flow, isolate-request logic, CCI introspection.
# ---------------------------------------------------------------------------
add_executable(reset_unit_tb reset_unit_tb.cpp)
target_link_libraries(reset_unit_tb PRIVATE smc_reset_unit SystemC::cci)
target_compile_options(reset_unit_tb PRIVATE -Wall -Wextra -Wpedantic
                                             -Wno-deprecated-declarations)
add_test(NAME reset_unit_tb COMMAND reset_unit_tb)
set_tests_properties(reset_unit_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# reset_unit_neg_tb -- negative-path / edge-case bench: bad config
# (SC_REPORT_FATAL constructor branches), TLM error responses, transport_dbg,
# CCI immutability.
# ---------------------------------------------------------------------------
add_executable(reset_unit_neg_tb reset_unit_neg_tb.cpp)
target_link_libraries(reset_unit_neg_tb PRIVATE smc_reset_unit SystemC::cci)
target_compile_options(reset_unit_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                 -Wno-deprecated-declarations)
add_test(NAME reset_unit_neg_tb COMMAND reset_unit_neg_tb)
set_tests_properties(reset_unit_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
