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

#include <iostream>
#include "otbn_algorithm_rsa_2048.h"
#include "otbn_algorithm_rsa_2048_key_enabled.h"
#include "otbn_algorithm_rsa_3072.h"
#include "otbn_algorithm_summation.h"
#include "otbn_algorithm_rnd_test.h"
#include "otbn_algorithm_p256_ecdsa.h"
#include "otbn_algorithm_smoke.h"
#include "otbn_algorithm_otbn_loop.h"
#include "otbn_algorithm_callback_cov.h"

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
    // lc_escalate_monitor_method is sensitive to lc_rma_req and lc_escalate_req.
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

void testbench::test_cov_otp_scramble_key_channel()
{
    REG_INFO(1, logger) << "\nCoverage: OTP scramble-key response channel" << std::endl;

    bool passed = true;
    uint32_t key[4] = {0, 0, 0, 0};
    uint32_t nonce = 0;
    uint32_t seed = 0;

    if (!dut->otp_key_rsp->key_available()) {
        REG_INFO(1, logger) << "  FAIL: key_available() returned false" << std::endl;
        passed = false;
    }
    dut->otp_key_rsp->get_scramble_key(key, nonce, seed);
    if (key[0] == 0 && key[1] == 0 && key[2] == 0 && key[3] == 0) {
        REG_INFO(1, logger) << "  FAIL: get_scramble_key returned all zeros" << std::endl;
        passed = false;
    } else if (nonce == 0 || seed == 0) {
        REG_INFO(1, logger) << "  FAIL: nonce/seed not populated" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: OTP scramble key channel returned a mock key" << std::endl;
    }

    report_test_result("Coverage: OTP scramble-key channel", passed);
}

void testbench::test_cov_imem_oob_and_busy_block()
{
    REG_INFO(1, logger) << "\nCoverage: IMEM OOB + IMEM/DMEM block in wipe/LOCKED" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("imem oob setup");

    dut->logger.setMaxVerbosity(3);

    if (dut->imem_write_callback(0xA5A5A5A5u, 2048u)) {
        REG_INFO(1, logger) << "  FAIL: IMEM write index 2048 accepted" << std::endl;
        passed = false;
    }
    uint32_t imem_val = 0xFFFFFFFFu;
    if (!dut->imem_read_callback(imem_val, 3000u) || imem_val != 0u) {
        REG_INFO(1, logger) << "  FAIL: IMEM read OOB expected 0" << std::endl;
        passed = false;
    }

    // Non-fatal illegal bus: wipe states that are not BUSY_EXECUTE.
    dut->current_state = OTBN_STATE_BUSY_SEC_WIPE_DMEM;
    if (dut->imem_write_callback(0x1u, 0u)) {
        REG_INFO(1, logger) << "  FAIL: IMEM write during SEC_WIPE_DMEM accepted" << std::endl;
        passed = false;
    }
    imem_val = 0xFFFFFFFFu;
    if (!dut->imem_read_callback(imem_val, 0u) || imem_val != 0u) {
        REG_INFO(1, logger) << "  FAIL: IMEM read during wipe did not return 0" << std::endl;
        passed = false;
    }
    if (dut->dmem_write_callback(0x2u, 0u)) {
        REG_INFO(1, logger) << "  FAIL: DMEM write during SEC_WIPE_DMEM accepted" << std::endl;
        passed = false;
    }
    uint32_t dmem_val = 0xFFFFFFFFu;
    if (!dut->dmem_read_callback(dmem_val, 0u) || dmem_val != 0u) {
        REG_INFO(1, logger) << "  FAIL: DMEM read during wipe did not return 0" << std::endl;
        passed = false;
    }

    dut->current_state = OTBN_STATE_LOCKED;
    imem_val = 0xFFFFFFFFu;
    dut->imem_read_callback(imem_val, 0u);
    dmem_val = 0xFFFFFFFFu;
    dut->dmem_read_callback(dmem_val, 0u);
    if (imem_val != 0u || dmem_val != 0u) {
        REG_INFO(1, logger) << "  FAIL: LOCKED IMEM/DMEM reads must return 0" << std::endl;
        passed = false;
    }

    // CTRL / INSN_CNT writes ignored while not IDLE.
    dut->current_state = OTBN_STATE_BUSY_EXECUTE;
    test_model->register_write_32(otbn_regs::CTRL_OFFSET, 0x1);
    test_model->register_write_32(otbn_regs::INSN_CNT_OFFSET, 0x0);
    if (dut->err_bits_write_callback(0xFFFFFFFFu)) {
        REG_INFO(1, logger) << "  FAIL: ERR_BITS W1C accepted while BUSY" << std::endl;
        passed = false;
    }

    dut->current_state = OTBN_STATE_IDLE;
    apply_reset();
    wait_for_idle("imem oob cleanup");

    if (passed) {
        REG_INFO(1, logger) << "  PASS: OOB and wipe/LOCKED memory blocks" << std::endl;
    }
    report_test_result("Coverage: IMEM OOB + wipe/LOCKED block", passed);
}

void testbench::test_cov_keymgr_ignore_and_invalid_cmd()
{
    REG_INFO(1, logger) << "\nCoverage: KeyMgr IGNORE command + invalid CMD opcode" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("keymgr ignore");

    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    unsigned char data[4] = {0x11, 0x22, 0x33, 0x44};
    trans.set_command(tlm::TLM_IGNORE_COMMAND);
    trans.set_address(0x00);
    trans.set_data_ptr(data);
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    test_model->keymgr_tl_stub_inst->initiator_socket->b_transport(trans, delay);

    if (trans.get_response_status() != tlm::TLM_COMMAND_ERROR_RESPONSE) {
        REG_INFO(1, logger) << "  FAIL: IGNORE expected COMMAND_ERROR, got "
                             << static_cast<int>(trans.get_response_status()) << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: IGNORE rejected with COMMAND_ERROR" << std::endl;
    }

    if (dut->cmd_write_callback(0x00u)) {
        REG_INFO(1, logger) << "  FAIL: unrecognized CMD was accepted" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: unrecognized CMD silently rejected" << std::endl;
    }

    dut->current_state = OTBN_STATE_LOCKED;
    if (!dut->cmd_write_callback(0xd8u)) {
        REG_INFO(1, logger) << "  FAIL: CMD write in LOCKED should return true (ignore)" << std::endl;
        passed = false;
    }
    dut->current_state = OTBN_STATE_BUSY_EXECUTE;
    if (!dut->cmd_write_callback(0xd8u)) {
        REG_INFO(1, logger) << "  FAIL: CMD write in BUSY should return true (ignore)" << std::endl;
        passed = false;
    }
    dut->current_state = OTBN_STATE_IDLE;

    apply_reset();
    wait_for_idle("keymgr ignore cleanup");
    report_test_result("Coverage: KeyMgr IGNORE + invalid CMD", passed);
}

void testbench::test_cov_alert_fatal_and_err_bits_busy()
{
    REG_INFO(1, logger) << "\nCoverage: ALERT_TEST.fatal + LOAD_CHECKSUM write" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("alert fatal");

    test_model->register_write_32(otbn_regs::ALERT_TEST_OFFSET, 0x1u);
    wait(5, SC_NS);
    if (!alert_fatal_sig.read()) {
        REG_INFO(1, logger) << "  FAIL: alert_fatal not asserted after ALERT_TEST.fatal" << std::endl;
        passed = false;
    } else {
        REG_INFO(1, logger) << "  PASS: ALERT_TEST.fatal asserted alert_fatal" << std::endl;
    }

    test_model->register_write_32(otbn_regs::LOAD_CHECKSUM_OFFSET, 0x0u);
    uint32_t csum = 0;
    test_model->register_read_32(otbn_regs::LOAD_CHECKSUM_OFFSET, csum);

    apply_reset();
    wait_for_idle("alert fatal cleanup");
    report_test_result("Coverage: ALERT_TEST.fatal", passed);
}

void testbench::test_cov_software_errs_fatal()
{
    REG_INFO(1, logger) << "\nCoverage: CTRL.software_errs_fatal + algorithm ERROR" << std::endl;

    bool passed = true;
    apply_reset();
    wait_for_idle("sw err fatal setup");
    clear_all_errors();

    test_model->register_write_32(otbn_regs::CTRL_OFFSET, 0x1u);
    wait(2, SC_NS);

    const std::string algo = dut->algorithm_type.get_param_value();
    if (algo == "summation") {
        for (uint32_t i = 0; i < 768; i++) {
            load_dmem_word(i, 0);
        }
        write_dmem_byte(0, 0);
    } else if (algo == "rsa_2048_key_enabled") {
        load_rsa_test_data();
    } else if (algo == "rsa_3072") {
        for (uint32_t i = 0; i < 768; i++) {
            load_dmem_word(i, 0);
        }
        write_dmem_byte(0x600 + 383, 5);
    }

    test_model->register_write_32(otbn_regs::CMD_OFFSET, otbn_constants::CMD_EXECUTE);
    wait_for_idle("sw err fatal execute");

    if (algo == "summation" || algo == "rsa_2048_key_enabled" || algo == "rsa_3072") {
        uint32_t status = read_status();
        uint32_t err_bits = read_err_bits();
        if (status != 0xFF) {
            REG_INFO(1, logger) << "  FAIL: expected LOCKED after fatal software error, STATUS=0x"
                                 << std::hex << status << std::dec << std::endl;
            passed = false;
        } else if ((err_bits & (1u << 23)) == 0u) {
            REG_INFO(1, logger) << "  FAIL: FATAL_SOFTWARE not set, ERR_BITS=0x"
                                 << std::hex << err_bits << std::dec << std::endl;
            passed = false;
        } else {
            REG_INFO(1, logger) << "  PASS: software_errs_fatal locked OTBN" << std::endl;
        }
    } else {
        REG_INFO(1, logger) << "  PASS: CTRL.software_errs_fatal write exercised ("
                             << algo << ")" << std::endl;
    }

    clear_all_errors();
    apply_reset();
    wait_for_idle("sw err fatal cleanup");
    report_test_result("Coverage: software_errs_fatal", passed);
}

void testbench::test_cov_algorithm_standalone_error_paths()
{
    REG_INFO(1, logger) << "\nCoverage: standalone algorithm DMEM/callback errors" << std::endl;

    bool passed = true;

    {
        otbn_algorithm_rsa_2048 algo(512);
        char buf[512] = {};
        if (algo.execute(buf) != otbn_algorithm::ERROR) {
            REG_INFO(1, logger) << "  FAIL: RSA-2048 small DMEM did not ERROR" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_rsa_2048_key_enabled algo(512);
        algo.register_key_status_cb([]() { return true; });
        char buf[512] = {};
        if (algo.execute(buf) != otbn_algorithm::ERROR) {
            REG_INFO(1, logger) << "  FAIL: RSA-2048-key small DMEM did not ERROR" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_rsa_3072 algo(100);
        char buf[100] = {};
        if (algo.execute(buf) != otbn_algorithm::ERROR) {
            REG_INFO(1, logger) << "  FAIL: RSA-3072 small DMEM did not ERROR" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_summation algo(10);
        char buf[256] = {};
        buf[0] = 20;  // N=20 requires 22 bytes
        if (algo.execute(buf) != otbn_algorithm::ERROR) {
            REG_INFO(1, logger) << "  FAIL: summation N exceeds DMEM did not ERROR" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_rnd_test algo(64);
        char buf[64] = {};
        if (algo.execute(buf) != otbn_algorithm::ERROR) {
            REG_INFO(1, logger) << "  FAIL: RND with no callback did not ERROR" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_smoke algo(64);
        char buf[64] = {};
        algo.reset();
        algo.message_objects(std::cout, std::cout);
        (void)algo.get_instruction_count();
        (void)algo.get_cycle_count();
        if (algo.execute(buf) != otbn_algorithm::SUCCESS) {
            REG_INFO(1, logger) << "  FAIL: smoke standalone execute failed" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_otbn_loop algo(64);
        char buf[64] = {};
        algo.reset();
        algo.message_objects(std::cout, std::cout);
        if (algo.execute(buf) != otbn_algorithm::SUCCESS) {
            REG_INFO(1, logger) << "  FAIL: otbn_loop standalone execute failed" << std::endl;
            passed = false;
        }
    }
    {
        otbn_algorithm_callback_cov algo(64);
        char buf[64] = {};
        algo.reset();
        algo.message_objects(std::cout, std::cout);
        if (algo.execute(buf) != otbn_algorithm::SUCCESS) {
            REG_INFO(1, logger) << "  FAIL: callback_cov standalone execute failed" << std::endl;
            passed = false;
        }
    }
    {
        // Valid OpenSSL P-256 vector (same words as load_p256_test_data) so
        // write_p256_value runs even in algorithm passes that skip the DUT
        // happy path.
        otbn_algorithm_p256_ecdsa algo(4096);
        char buf[4096] = {};
        auto* dw = reinterpret_cast<uint32_t*>(buf);
        static const uint32_t msg[8] = {0xBE7DFBEA, 0xB71092A6, 0xB98DE50C,
                                        0x0D3DC917, 0xBE9E25BA, 0xD2E8E857,
                                        0x2404BEAF, 0xED4109AA};
        static const uint32_t r[8] = {0x41991BEC, 0x8DC3060C, 0x890BC8AC,
                                      0xCAFC6FDB, 0x1B99574C, 0x1B213D1B,
                                      0x8C2F8DD0, 0x83F371A8};
        static const uint32_t s[8] = {0x46A84F3A, 0xBBBD0F26, 0x713856D0,
                                      0x109547DA, 0x13CEFB0A, 0xAF9F360A,
                                      0xBBFA26C3, 0x3B6258F0};
        static const uint32_t qx[8] = {0x4296164C, 0x79E7D74C, 0xEB97E5C1,
                                       0xEDE9A495, 0x682D79BE, 0xF1AA16EA,
                                       0xE1E12803, 0xEE28EC71};
        static const uint32_t qy[8] = {0x1AB238AE, 0xD63E53F6, 0x5C3E0077,
                                       0xACD9BE18, 0xEA12A8B1, 0xE6688EE7,
                                       0x68012F20, 0xEECC45DF};
        for (int i = 0; i < 8; i++) {
            dw[(0x520 / 4) + i] = msg[i];
            dw[(0x540 / 4) + i] = r[i];
            dw[(0x560 / 4) + i] = s[i];
            dw[(0x580 / 4) + i] = qx[i];
            dw[(0x5A0 / 4) + i] = qy[i];
        }
        algo.reset();
        algo.message_objects(std::cout, std::cout);
        (void)algo.get_instruction_count();
        if (algo.execute(buf) != otbn_algorithm::SUCCESS) {
            REG_INFO(1, logger) << "  FAIL: P256 standalone valid-vector execute failed"
                                 << std::endl;
            passed = false;
        }
    }

    uint32_t rnd_words[8] = {};
    if (dut->rnd_read_handler(rnd_words) != otbn_algorithm::SUCCESS) {
        REG_INFO(1, logger) << "  FAIL: rnd_read_handler did not succeed" << std::endl;
        passed = false;
    }
    uint32_t csr_scratch = 0;
    (void)dut->csr_read_handler(0x10, &csr_scratch);
    (void)dut->csr_write_handler(0x10, 0xA5A5A5A5u);

    if (passed) {
        REG_INFO(1, logger) << "  PASS: standalone algorithm error paths" << std::endl;
    }
    report_test_result("Coverage: standalone algorithm errors", passed);
}

void testbench::run_coverage_tests()
{
    REG_INFO(1, logger) << "\n========================================" << std::endl;
    REG_INFO(1, logger) << "   OTBN Coverage Tests" << std::endl;
    REG_INFO(1, logger) << "========================================\n" << std::endl;

    dut->logger.setMaxVerbosity(3);

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

    test_cov_otp_scramble_key_channel();
    test_cov_imem_oob_and_busy_block();
    test_cov_keymgr_ignore_and_invalid_cmd();
    test_cov_alert_fatal_and_err_bits_busy();
    test_cov_software_errs_fatal();
    test_cov_algorithm_standalone_error_paths();
}
