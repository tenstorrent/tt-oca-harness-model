// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "../inc/testbench.h"
#include <iostream>

namespace {

void reset_dut(hmac_test* test)
{
    test->rst_ni.write(false);
    wait(20, SC_NS);
    test->rst_ni.write(true);
    wait(20, SC_NS);
}

void keymgr_read_word(hmac_test* test, uint64_t offset, uint32_t& value,
                      tlm::tlm_response_status& status)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;
    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    test->keymgr_initiator_socket->b_transport(payload, delay);
    status = payload.get_response_status();
}

} // namespace

// Command rejects, idle-only writes, leftover-FIFO start, SHA-384 stop/continue,
// and a key-manager read. These are the src/hmac.cpp branches the functional
// suite never entered.
void testbench::test_coverage_gap_paths()
{
    CSML_INFO(1, logger) << "\n========================================" << std::endl;
    CSML_INFO(1, logger) << "  Coverage: command rejects and context paths" << std::endl;
    CSML_INFO(1, logger) << "========================================\n" << std::endl;

    uint32_t write_val = 0;
    uint32_t read_val = 0;

    reset_dut(test);
    wait_for_hmac_idle();

    // hash_process is only legal in PROCESSING. Issuing it from IDLE must be
    // dropped without leaving the engine or raising a stored error.
    CSML_INFO(1, logger) << "--- hash_process while IDLE ---" << std::endl;
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x2);
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::STATUS_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(1u, read_val & 0x1u, "hash_process in IDLE leaves hmac_idle set");

    // hash_continue uses the same priority as hash_start: SwInvalidConfig,
    // then SwHashStartWhenShaDisabled, then SwHashStartWhenActive.
    CSML_INFO(1, logger) << "--- hash_continue rejected: invalid digest_size ---" << std::endl;
    write_val = (1u << 1) | (0x8u << 5); // sha_en=1, SHA2_None
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x8);
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::ERR_CODE_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0x6u, read_val, "hash_continue SwInvalidConfig (0x6)");
    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x4);
    wait(5, SC_NS);

    CSML_INFO(1, logger) << "--- hash_continue rejected: sha_en=0 ---" << std::endl;
    write_val = (0x1u << 5); // sha_en=0, SHA2_256
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x8);
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::ERR_CODE_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0x2u, read_val, "hash_continue SwHashStartWhenShaDisabled (0x2)");
    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x4);
    wait(5, SC_NS);

    CSML_INFO(1, logger) << "--- hash_continue rejected: engine active ---" << std::endl;
    write_val = (1u << 1) | (0x1u << 5);
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x1); // hash_start
    wait(10, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x8); // hash_continue
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::ERR_CODE_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0x4u, read_val, "hash_continue SwHashStartWhenActive (0x4)");
    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x4);
    wait(5, SC_NS);

    // MSG_LENGTH is the context-restore port; hardware ignores it once hashing.
    CSML_INFO(1, logger) << "--- MSG_LENGTH write rejected while PROCESSING ---" << std::endl;
    test->write_register_32(hmac_basetest::MSG_LENGTH_LOWER_OFFSET, 0xA5A5A5A5u);
    test->write_register_32(hmac_basetest::MSG_LENGTH_UPPER_OFFSET, 0x5A5A5A5Au);
    wait(5, SC_NS);
    test->read_register_32(hmac_basetest::MSG_LENGTH_LOWER_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0u, read_val, "MSG_LENGTH_LOWER unchanged while PROCESSING");
    test->read_register_32(hmac_basetest::MSG_LENGTH_UPPER_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0u, read_val, "MSG_LENGTH_UPPER unchanged while PROCESSING");

    // Leave leftover FIFO words via an off-boundary hash_stop, then hash_start
    // must drain them so the next operation does not absorb stale data.
    CSML_INFO(1, logger) << "--- hash_start clears leftover FIFO ---" << std::endl;
    test->write_register_32(hmac_basetest::MSG_FIFO_OFFSET, 0x11111111u);
    test->write_register_32(hmac_basetest::MSG_FIFO_OFFSET, 0x22222222u);
    wait(10, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x4); // hash_stop
    wait(20, SC_NS);
    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x5);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x1); // hash_start
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::STATUS_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(1u, (read_val >> 1) & 0x1u,
                       "hash_start drained leftover FIFO (fifo_empty=1)");
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x4);
    wait(10, SC_NS);
    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x1);
    wait(5, SC_NS);

    // Burst-write past the FIFO depth without yielding so the model has to
    // drain a block before accepting the write, matching hardware back-pressure.
    CSML_INFO(1, logger) << "--- FIFO-full write drains a block ---" << std::endl;
    reset_dut(test);
    wait_for_hmac_idle();
    write_val = (1u << 1) | (0x1u << 5);
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x1);
    wait(10, SC_NS);
    for (int i = 0; i < 33; i++) {
        test->write_register_32(hmac_basetest::MSG_FIFO_OFFSET,
                               0x10000000u + static_cast<uint32_t>(i));
    }
    wait(50, SC_NS);
    test->read_register_32(hmac_basetest::MSG_LENGTH_LOWER_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(33u * 32u, read_val, "33 FIFO words accepted (one drained under back-pressure)");
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x2);
    wait(200, SC_NS);
    wait_for_hmac_done();

    // SHA-384 hash_stop on a 1024-bit block publishes the 64-bit chaining
    // words as high/low DIGEST pairs; hash_continue must reload that layout.
    CSML_INFO(1, logger) << "--- SHA-384 hash_stop / hash_continue ---" << std::endl;
    reset_dut(test);
    wait_for_hmac_idle();
    write_val = (1u << 1) | (0x2u << 5); // sha_en=1, SHA2_384
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x1);
    wait(10, SC_NS);
    for (int i = 0; i < 32; i++) {
        test->write_register_32(hmac_basetest::MSG_FIFO_OFFSET,
                               0xC0FFEE00u + static_cast<uint32_t>(i));
    }
    wait(200, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x4);
    wait(100, SC_NS);
    test->read_register_32(hmac_basetest::INTR_STATE_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0x1u, read_val & 0x1u,
                       "SHA-384 hash_stop on a block boundary sets hmac_done");

    uint32_t saved[16] = {};
    for (int i = 0; i < 16; i++) {
        test->read_register_32(hmac_basetest::DIGEST_OFFSET + (i * 4), saved[i]);
        wait(5, SC_NS);
    }
    uint32_t saved_len = 0;
    test->read_register_32(hmac_basetest::MSG_LENGTH_LOWER_OFFSET, saved_len);
    wait(5, SC_NS);
    test->assert_equal(1024u, saved_len, "SHA-384 MSG_LENGTH is one 128-byte block");
    if (saved[0] == 0 && saved[1] == 0) {
        m_tests_failed++;
        CSML_ERROR(0, logger) << "FAIL: SHA-384 hash_stop left DIGEST empty" << std::endl;
    }

    test->write_register_32(hmac_basetest::INTR_STATE_OFFSET, 0x1);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CFG_OFFSET, write_val);
    wait(5, SC_NS);
    for (int i = 0; i < 16; i++) {
        test->write_register_32(hmac_basetest::DIGEST_OFFSET + (i * 4), saved[i]);
        wait(5, SC_NS);
    }
    test->write_register_32(hmac_basetest::MSG_LENGTH_LOWER_OFFSET, saved_len);
    wait(5, SC_NS);
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x8);
    wait(10, SC_NS);
    test->read_register_32(hmac_basetest::STATUS_OFFSET, read_val);
    wait(5, SC_NS);
    test->assert_equal(0u, read_val & 0x1u, "SHA-384 hash_continue leaves IDLE");
    test->write_register_32(hmac_basetest::CMD_OFFSET, 0x2);
    wait(200, SC_NS);
    wait_for_hmac_done();

    // Sideload is write-only; a read must be refused, not return key material.
    CSML_INFO(1, logger) << "--- keymgr sideload read is rejected ---" << std::endl;
    uint32_t keymgr_val = 0xFFFFFFFFu;
    tlm::tlm_response_status keymgr_status = tlm::TLM_OK_RESPONSE;
    keymgr_read_word(test, 0x00, keymgr_val, keymgr_status);
    wait(5, SC_NS);
    if (keymgr_status != tlm::TLM_COMMAND_ERROR_RESPONSE) {
        m_tests_failed++;
        CSML_ERROR(0, logger) << "FAIL: keymgr read returned "
                              << keymgr_status << " (expected COMMAND_ERROR)"
                              << std::endl;
    } else {
        CSML_INFO(1, logger) << "PASS: keymgr read rejected with COMMAND_ERROR" << std::endl;
    }

    CSML_INFO(1, logger) << "\n--- Test Complete: coverage gap paths ---" << std::endl;
}
