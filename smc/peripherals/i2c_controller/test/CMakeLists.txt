# ---------------------------------------------------------------------------
# i2c_controller_tb -- primary self-checking bench: register access (reset
# values, RW/RO/W1C semantics, field masks), Controller-Mode FMT/RX FIFO
# transactions via a bus model (write/read/NACK/halt/resume, CMD_COMPLETE),
# Target-Mode ACQ/TX FIFOs (target_write/target_read, address match, NACK
# count), FIFO thresholds + interrupt aggregation, FIFO_CTRL resets, INTR_TEST
# force paths, CCI introspection.
# ---------------------------------------------------------------------------
add_executable(i2c_controller_tb i2c_controller_tb.cpp)
target_link_libraries(i2c_controller_tb PRIVATE smc_i2c_controller SystemC::cci)
target_compile_options(i2c_controller_tb PRIVATE -Wall -Wextra -Wpedantic
                                                 -Wno-deprecated-declarations)
add_test(NAME i2c_controller_tb COMMAND i2c_controller_tb)
set_tests_properties(i2c_controller_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")

# ---------------------------------------------------------------------------
# i2c_controller_neg_tb -- negative-path / edge-case bench: bad config
# (SC_REPORT_FATAL constructor branch), TLM error responses (command/burst/
# address/decode), transport_dbg read+write, RO-register write-ignore, WO
# read-as-zero, FIFO overflow error interrupts, back-door on a disabled
# target, CCI immutability.  Brings line coverage above 95 %.
# ---------------------------------------------------------------------------
add_executable(i2c_controller_neg_tb i2c_controller_neg_tb.cpp)
target_link_libraries(i2c_controller_neg_tb PRIVATE smc_i2c_controller SystemC::cci)
target_compile_options(i2c_controller_neg_tb PRIVATE -Wall -Wextra -Wpedantic
                                                     -Wno-deprecated-declarations)
add_test(NAME i2c_controller_neg_tb COMMAND i2c_controller_neg_tb)
set_tests_properties(i2c_controller_neg_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "(^FAIL [^_]|FAILURE\\(S\\))")
