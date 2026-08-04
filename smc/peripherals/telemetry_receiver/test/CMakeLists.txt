# ---------------------------------------------------------------------------
# telemetry_receiver_tb -- primary self-checking bench: reset values, register
# RW/RO/masking semantics, ATB beat assembly and message decode (hand-built
# beat stream cross-checking the reference encoder), per-counter valid bits,
# multi-packet messages, message-queue FIFO ordering and drop-oldest overflow,
# BUFFER_POP, threshold and missing-last interrupts, TX/RX flush, CCI
# introspection.
# ---------------------------------------------------------------------------
add_executable(telemetry_receiver_tb telemetry_receiver_tb.cpp)
target_link_libraries(telemetry_receiver_tb PRIVATE smc_telemetry_receiver SystemC::cci)
target_compile_options(telemetry_receiver_tb PRIVATE -Wall -Wextra -Wpedantic
                                                    -Wno-deprecated-declarations)
add_test(NAME telemetry_receiver_tb COMMAND telemetry_receiver_tb)
set_tests_properties(telemetry_receiver_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# telemetry_receiver_neg_tb -- negative-path / edge-case bench: TLM error
# responses (command/burst/address/decode), transport_dbg read+write,
# RO-register write-ignore, unmapped-in-window decode miss, beats rejected
# while in reset, pop-when-empty, threshold-field truncation, non-default CCI
# geometry (buffer_depth / max_counters_per_message presets).  Brings line
# coverage above 95 %.
# ---------------------------------------------------------------------------
add_executable(telemetry_receiver_neg_tb telemetry_receiver_neg_tb.cpp)
target_link_libraries(telemetry_receiver_neg_tb PRIVATE smc_telemetry_receiver SystemC::cci)
target_compile_options(telemetry_receiver_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                        -Wno-deprecated-declarations)
add_test(NAME telemetry_receiver_neg_tb COMMAND telemetry_receiver_neg_tb)
set_tests_properties(telemetry_receiver_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
