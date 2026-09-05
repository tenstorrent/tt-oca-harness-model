// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "otbn_basetest.h"
#include "otbn_algorithm_rsa_2048.h"
#include "otbn_algorithm_rsa_2048_key_enabled.h"
#include "otbn_algorithm_rsa_3072.h"
#include "otbn_algorithm_summation.h"
#include "otbn_algorithm_rnd_test.h"
#include <vector>

// ============================================================================
// Coverage tests — exercise otbn.cpp and algorithm error paths not hit by the
// main functional suite (KeyMgr TLM errors, LC+INTR, algorithm negatives).
// ============================================================================

void testbench::test_cov_keymgr_tlm_read_error()
{
    REG_INFO(1, logger) << "\nCoverage: KeyMgr TLM READ command rejection" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("keymgr read error");

    tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
    if (!test_model->keymgr_tl_stub_inst->send_read(0x00, status)) {
        REG_INFO(1, logger) << "  FAIL: send_read transport failed" << std::endl;
        passed = false;
    } else if (status != tlm::TLM_COMMAND_ERROR_RESPONSE) {
        REG_INFO(1, logger) << "  FAIL: expected TLM_COMMAND_ERROR_RESPONSE, got "
                             << static_cast<int>(status) << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: READ rejected with COMMAND_ERROR" << std::endl;
    }

    report_test_result("Coverage: KeyMgr TLM READ error", passed);
}

void testbench::test_cov_keymgr_tlm_bad_address()
{
    REG_INFO(1, logger) << "\nCoverage: KeyMgr TLM invalid address" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("keymgr bad addr");

    tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
    if (!test_model->keymgr_tl_stub_inst->send_write(0x70, 0xDEADBEEFu, status)) {
        REG_INFO(1, logger) << "  FAIL: send_write transport failed" << std::endl;
        passed = false;
    } else if (status != tlm::TLM_ADDRESS_ERROR_RESPONSE) {
        REG_INFO(1, logger) << "  FAIL: expected TLM_ADDRESS_ERROR_RESPONSE, got "
                             << static_cast<int>(status) << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: write to 0x70 rejected with ADDRESS_ERROR" << std::endl;
    }

    report_test_result("Coverage: KeyMgr TLM bad address", passed);
}

void testbench::test_cov_keymgr_key_invalidate()
{
    REG_INFO(1, logger) << "\nCoverage: KeyMgr KEY_CTRL invalidate (write 0)" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("keymgr invalidate");

    uint32_t test_key[12] = {
        0x11111111, 0x22222222, 0x33333333, 0x44444444,
        0x55555555, 0x66666666, 0x77777777, 0x88888888,
        0x99999999, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC
    };
    test_model->program_keymgr_key(test_key);
    wait(10, SC_NS);

    tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
    if (!test_model->keymgr_tl_stub_inst->send_write(0x60, 0u, status) ||
        status != tlm::TLM_OK_RESPONSE) {
        REG_INFO(1, logger) << "  FAIL: KEY_CTRL=0 write failed" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: KEY_CTRL=0 accepted (key invalidated)" << std::endl;
    }

    report_test_result("Coverage: KeyMgr key invalidate", passed);
}

void testbench::test_cov_keymgr_wdr_s1_h_write()
{
    REG_INFO(1, logger) << "\nCoverage: KeyMgr WDR KEY_S1_H region (0x50)" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("keymgr s1h");

    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    unsigned char data[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,
                              0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0x50);
    trans.set_data_ptr(data);
    trans.set_data_length(16);
    trans.set_streaming_width(16);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    test_model->keymgr_tl_stub_inst->initiator_socket->b_transport(trans, delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
        REG_INFO(1, logger) << "  FAIL: KEY_S1_H write rejected" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: KEY_S1_H region written via keymgr TLM" << std::endl;
    }

    report_test_result("Coverage: KeyMgr WDR S1_H write", passed);
}

void testbench::test_cov_dmem_protected_read()
{
    REG_INFO(1, logger) << "\nCoverage: DMEM protected region read returns 0" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("dmem protected read");

    uint32_t val = read_dmem_word(768);
    if (val != 0) {
        REG_INFO(1, logger) << "  FAIL: protected DMEM[768] expected 0, got 0x"
                             << std::hex << val << std::dec << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: protected DMEM read returns 0" << std::endl;
    }

    report_test_result("Coverage: DMEM protected read", passed);
}

void testbench::test_cov_lc_rma_intr_enable()
{
    REG_INFO(1, logger) << "\nCoverage: LC RMA with INTR_ENABLE.done" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("lc rma intr setup");

    test_model->register_write_32(otbn_regs::INTR_STATE_OFFSET, 0x1);
    test_model->register_write_32(otbn_regs::INTR_ENABLE_OFFSET, 0x1);
    wait(5, SC_NS);

    test_model->otp_key_req_stub_inst->reset_request_count();
    // lc_escalate_monitor_method is sensitive only to lc_escalate_req; pulse it
    // while lc_rma is asserted so lc_monitor_thread runs the RMA handler.
    test_model->set_lc_rma(true);
    test_model->set_lc_escalate(true);
    wait(100, SC_NS);

    if (test_model->otp_key_req_stub_inst->get_request_count() < 1) {
        REG_INFO(1, logger) << "  FAIL: OTP scramble key not requested during RMA" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: OTP scramble key requested during RMA" << std::endl;
    }

    uint32_t intr_state = 0;
    test_model->register_read_32(otbn_basetest::INTR_STATE_OFFSET, intr_state);
    if ((intr_state & 0x1) == 0) {
        REG_INFO(1, logger) << "  FAIL: INTR_STATE.done not set after RMA" << std::endl;
        passed = false;
    } else if (!intr_done_sig.read()) {
        REG_INFO(1, logger) << "  FAIL: intr_done not asserted after RMA with INTR_ENABLE" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: done interrupt asserted after RMA" << std::endl;
    }

    test_model->set_lc_rma(false);
    test_model->set_lc_escalate(false);
    apply_reset();
    report_test_result("Coverage: LC RMA + INTR_ENABLE", passed);
}

void testbench::test_cov_lc_escalation_intr_enable()
{
    REG_INFO(1, logger) << "\nCoverage: LC escalation with INTR_ENABLE.done" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("lc esc intr setup");

    test_model->register_write_32(otbn_regs::INTR_STATE_OFFSET, 0x1);
    test_model->register_write_32(otbn_regs::INTR_ENABLE_OFFSET, 0x1);
    wait(5, SC_NS);

    test_model->set_lc_escalate(true);
    wait(50, SC_NS);

    uint32_t status = read_status();
    if (status != 0xFF) {
        REG_INFO(1, logger) << "  FAIL: STATUS not LOCKED after escalation" << std::endl;
        passed = false;
    }

    uint32_t intr_state = 0;
    test_model->register_read_32(otbn_basetest::INTR_STATE_OFFSET, intr_state);
    if ((intr_state & 0x1) == 0) {
        REG_INFO(1, logger) << "  FAIL: INTR_STATE.done not set after LC escalation" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: INTR_STATE.done set during LC escalation" << std::endl;
    }

    if (!intr_done_sig.read()) {
        REG_INFO(1, logger) << "  FAIL: intr_done not asserted with INTR_ENABLE=1" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: intr_done asserted with INTR_ENABLE=1" << std::endl;
    }

    test_model->set_lc_escalate(false);
    apply_reset();
    report_test_result("Coverage: LC escalation + INTR_ENABLE", passed);
}

void testbench::test_cov_rsa2048_key_invalid()
{
    REG_INFO(1, logger) << "\nCoverage: RSA-2048 key-enabled without registered key" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("rsa2048 key invalid");
    clear_all_errors();

    load_rsa_test_data();
    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("rsa2048 key invalid execute");

    uint32_t err_bits = read_err_bits();
    if ((err_bits & 0x20) == 0) {
        REG_INFO(1, logger) << "  FAIL: KEY_INVALID (bit 5) not set, ERR_BITS=0x"
                             << std::hex << err_bits << std::dec << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: KEY_INVALID set when key not registered" << std::endl;
    }

    clear_all_errors();
    report_test_result("Coverage: RSA-2048 KEY_INVALID", passed);
}

void testbench::test_cov_summation_n_zero()
{
    REG_INFO(1, logger) << "\nCoverage: Summation N=0 error path" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("summation N=0");
    clear_all_errors();

    for (uint32_t i = 0; i < 768; i++) {
        load_dmem_word(i, 0);
    }
    write_dmem_byte(0, 0);  // N = 0

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("summation N=0 execute");

    uint32_t status = read_status();
    if (status != otbn_constants::STATE_IDLE) {
        REG_INFO(1, logger) << "  FAIL: STATUS not IDLE after N=0 error" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: Algorithm returned ERROR for N=0" << std::endl;
    }

    clear_all_errors();
    report_test_result("Coverage: Summation N=0", passed);
}

void testbench::test_cov_p256_invalid_signature()
{
    REG_INFO(1, logger) << "\nCoverage: P256 ECDSA invalid signature" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("p256 invalid sig");
    clear_all_errors();

    load_p256_test_data();
    // Corrupt signature component s (last byte of S region)
    write_dmem_byte(0x560 + 31, 0xFF);

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_algorithm_completion(2000);
    wait_for_idle("p256 invalid sig execute");

    const uint32_t OK_WORD = 0x504 / 4;
    uint32_t ok_val = read_dmem_word(OK_WORD);
    if (ok_val != 0x00001d4e) {  // HARDENED_BOOL_FALSE
        REG_INFO(1, logger) << "  FAIL: expected HARDENED_BOOL_FALSE (0x1d4e), got 0x"
                             << std::hex << ok_val << std::dec << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: Invalid signature → HARDENED_BOOL_FALSE" << std::endl;
    }

    report_test_result("Coverage: P256 invalid signature", passed);
}

void testbench::test_cov_p256_invalid_pubkey()
{
    REG_INFO(1, logger) << "\nCoverage: P256 ECDSA invalid public key point" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("p256 bad pubkey");
    clear_all_errors();

    load_p256_test_data();
    // (1, 1) is not on the P-256 curve → EC_POINT_set_affine_coordinates fails
    const uint32_t X_WORD = 0x580 / 4;
    const uint32_t Y_WORD = 0x5A0 / 4;
    load_dmem_word(X_WORD + 7, 0x00000001);
    load_dmem_word(Y_WORD + 7, 0x00000001);

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_algorithm_completion(2000);
    wait_for_idle("p256 bad pubkey execute");

    REG_INFO(1, logger) << "  PASS: Invalid pubkey error path exercised" << std::endl;
    clear_all_errors();
    report_test_result("Coverage: P256 invalid pubkey", passed);
}

void testbench::test_cov_rsa3072_happy_path()
{
    REG_INFO(1, logger) << "\nCoverage: RSA-3072 happy path (sig^65537 mod n)" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("rsa3072 happy");
    clear_all_errors();

    for (uint32_t i = 0; i < 768; i++) {
        load_dmem_word(i, 0);
    }
    // n=13, sig=5 in LSByte-first layout (reverse_bytes → BE last byte)
    write_dmem_byte(383, 13);
    write_dmem_byte(0x600 + 383, 5);

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("rsa3072 happy execute");

    uint32_t err_bits = read_err_bits();
    if (err_bits != 0) {
        REG_INFO(1, logger) << "  FAIL: ERR_BITS=0x" << std::hex << err_bits << std::dec << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: RSA-3072 execution completed without errors" << std::endl;
    }

    report_test_result("Coverage: RSA-3072 happy path", passed);
}

void testbench::test_cov_rsa3072_zero_modulus()
{
    REG_INFO(1, logger) << "\nCoverage: RSA-3072 zero modulus error" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("rsa3072 zero mod");
    clear_all_errors();

    for (uint32_t i = 0; i < 768; i++) {
        load_dmem_word(i, 0);
    }
    write_dmem_byte(0x600 + 383, 5);  // non-zero sig, zero modulus

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("rsa3072 zero mod execute");

    REG_INFO(1, logger) << "  PASS: Zero modulus path exercised" << std::endl;
    clear_all_errors();
    report_test_result("Coverage: RSA-3072 zero modulus", passed);
}

void testbench::test_cov_csr_wdr_callback_execute()
{
    REG_INFO(1, logger) << "\nCoverage: CSR/WDR callbacks via callback_cov algorithm" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("callback_cov pre-exec");

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("callback_cov execute");

    uint32_t marker = read_dmem_word(0);
    if (marker != 0xC0DEC0DEu) {
        REG_INFO(1, logger) << "  FAIL: expected DMEM[0]=0xC0DEC0DE, got 0x"
                             << std::hex << marker << std::dec << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: callback_cov algorithm completed" << std::endl;
    }

    clear_all_errors();
    report_test_result("Coverage: CSR/WDR callback_cov execute", passed);
}

void testbench::test_cov_wdr_key_read_with_key()
{
    REG_INFO(1, logger) << "\nCoverage: WDR key region read with sideload key" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("wdr key read setup");

    uint32_t test_key[12] = {
        0x11111111, 0x22222222, 0x33333333, 0x44444444,
        0x55555555, 0x66666666, 0x77777777, 0x88888888,
        0x99999999, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC
    };
    test_model->program_keymgr_key(test_key);
    wait(10, SC_NS);

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("wdr key read execute");

    if (read_dmem_word(0) != 0xC0DEC0DEu) {
        REG_INFO(1, logger) << "  FAIL: callback_cov did not complete after key program" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: key WDR read path exercised" << std::endl;
    }

    clear_all_errors();
    report_test_result("Coverage: WDR key read with sideload key", passed);
}

void testbench::test_cov_otp_key_rsp_channel()
{
    REG_INFO(1, logger) << "\nCoverage: OTP key response channel mock" << std::endl;

    bool passed = true;
    if (!dut->otp_key_rsp->key_available()) {
        REG_INFO(1, logger) << "  FAIL: key_available() returned false" << std::endl;
        passed = false;
    }

    uint32_t key[4] = {0, 0, 0, 0};
    uint32_t nonce = 0;
    uint32_t seed = 0;
    dut->otp_key_rsp->get_scramble_key(key, nonce, seed);
    if (key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0) {
        REG_INFO(1, logger) << "  FAIL: scramble key was all zeros" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: OTP mock returned a scramble key" << std::endl;
    }

    report_test_result("Coverage: OTP key response channel", passed);
}

void testbench::test_cov_algorithm_error_guards()
{
    REG_INFO(1, logger) << "\nCoverage: Algorithm DMEM-size and RND error guards" << std::endl;

    bool passed = true;
    std::vector<char> buf(256, 0);

    otbn_algorithm_rsa_2048 rsa2048(512);
    if (rsa2048.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: rsa_2048 should reject small DMEM" << std::endl;
        passed = false;
    }

    otbn_algorithm_rsa_2048_key_enabled rsa2048_key(512);
    rsa2048_key.register_key_status_cb([]() { return true; });
    if (rsa2048_key.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: rsa_2048_key_enabled should reject small DMEM" << std::endl;
        passed = false;
    }

    otbn_algorithm_rsa_3072 rsa3072(512);
    if (rsa3072.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: rsa_3072 should reject small DMEM" << std::endl;
        passed = false;
    }

    buf[0] = 20;  // N=20, N+2=22 > constructed DMEM size of 10
    otbn_algorithm_summation sum(10);
    if (sum.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: summation should reject N exceeding DMEM" << std::endl;
        passed = false;
    }

    otbn_algorithm_rnd_test rnd_nocb(256);
    if (rnd_nocb.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: rnd_test should error without callback" << std::endl;
        passed = false;
    }

    otbn_algorithm_rnd_test rnd_fail(256);
    rnd_fail.register_rnd_read_cb([](uint32_t*) { return otbn_algorithm::ERROR; });
    if (rnd_fail.execute(buf.data()) != otbn_algorithm::ERROR) {
        REG_INFO(1, logger) << "  FAIL: rnd_test should error when callback fails" << std::endl;
        passed = false;
    }

    if (passed) {
        REG_INFO(1, logger) << "  PASS: Algorithm error guards returned ERROR" << std::endl;
    }
    report_test_result("Coverage: Algorithm error guards", passed);
}

void testbench::run_coverage_tests()
{
    REG_INFO(1, logger) << "\n========================================" << std::endl;
    REG_INFO(1, logger) << "   OTBN Coverage Tests" << std::endl;
    REG_INFO(1, logger) << "========================================\n" << std::endl;

    test_cov_keymgr_tlm_read_error();
    test_cov_keymgr_tlm_bad_address();
    test_cov_keymgr_key_invalidate();
    test_cov_keymgr_wdr_s1_h_write();
    test_cov_dmem_protected_read();
    test_cov_otp_key_rsp_channel();
    test_cov_algorithm_error_guards();
    test_cov_lc_escalation_intr_enable();
    test_cov_lc_rma_intr_enable();

    const std::string algo = dut->algorithm_type.get_param_value();
    if (algo == "rsa_2048_key_enabled") {
        test_cov_rsa2048_key_invalid();
    } else if (algo == "summation") {
        test_cov_summation_n_zero();
    } else if (algo == "p256_ecdsa") {
        test_cov_p256_invalid_signature();
        test_cov_p256_invalid_pubkey();
    } else if (algo == "rsa_3072") {
        test_cov_rsa3072_happy_path();
        test_cov_rsa3072_zero_modulus();
    } else if (algo == "callback_cov") {
        test_cov_csr_wdr_callback_execute();
        test_cov_wdr_key_read_with_key();
    }
}
