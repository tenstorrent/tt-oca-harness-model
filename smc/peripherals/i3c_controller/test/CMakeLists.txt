# ---------------------------------------------------------------------------
# i3c_controller_tb -- primary self-checking bench: register access (reset
# values, RW/RO/W1C semantics), multi-instance address decode, DAT/DCT
# windows, HCI command/response/TX/RX/IBI FIFOs, transaction engine (private
# write/read, CCC, NACK/underflow/overflow errors), interrupt aggregation,
# RESET_CONTROL self-clearing queue resets, CCI introspection.
# ---------------------------------------------------------------------------
add_executable(i3c_controller_tb i3c_controller_tb.cpp)
target_link_libraries(i3c_controller_tb PRIVATE smc_i3c_controller SystemC::cci)
target_compile_options(i3c_controller_tb PRIVATE -Wall -Wextra -Wpedantic
                                                 -Wno-deprecated-declarations)
add_test(NAME i3c_controller_tb COMMAND i3c_controller_tb)
set_tests_properties(i3c_controller_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# i3c_controller_neg_tb -- negative-path / edge-case bench: bad config
# (SC_REPORT_FATAL constructor branches), TLM error responses, transport_dbg,
# CCI immutability, queue overflow / full conditions.
# ---------------------------------------------------------------------------
add_executable(i3c_controller_neg_tb i3c_controller_neg_tb.cpp)
target_link_libraries(i3c_controller_neg_tb PRIVATE smc_i3c_controller SystemC::cci)
target_compile_options(i3c_controller_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                     -Wno-deprecated-declarations)
add_test(NAME i3c_controller_neg_tb COMMAND i3c_controller_neg_tb)
set_tests_properties(i3c_controller_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
