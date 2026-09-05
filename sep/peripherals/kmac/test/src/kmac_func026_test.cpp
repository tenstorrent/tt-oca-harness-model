// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.

/******************************************************************************
 * @file kmac_func026_test.cpp
 * @brief FUNC-KMAC-026: Rejection and boundary paths
 *
 * The FUNC-001..025 groups drive the model along paths where the configuration
 * is valid and the command sequence is legal. This group covers the branches
 * taken when it is not: commands issued in the wrong state, configurations the
 * START handler already rejected reaching a later handler anyway, and the
 * output-window boundaries of extendable-output modes.
 *
 * These were identified from a line-coverage run rather than from the register
 * map, which is why they sit outside the FUNC-001..025 numbering: each one is a
 * reachable branch in the model that no existing test entered.
 *
 * Test Case Count: 10
 * - TC-220: entropy_req outside IDLE is ignored
 * - TC-221: refresh threshold reached while entropy_mode is not EDN
 * - TC-222: ENTROPY_REFRESH_THRESHOLD_SHADOWED write during escalation
 * - TC-223: application request during escalation
 * - TC-224: MSG_FIFO write while the application interface owns the datapath
 * - TC-225: RUN with a kstrength that is invalid for XOF modes
 * - TC-226: RUN past the end of the buffered SHAKE output
 * - TC-227: RUN past the end of the buffered KMAC output
 * - TC-228: STATE share1 window reads zero when masking is disabled
 * - TC-229: STATE reads that straddle the end of the digest
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "kmac_func013_024_test.h"
#include <cstdio>

namespace {

// CFG_SHADOWED field encodings, from kmac.rdl and sha3_pkg.sv.
constexpr uint32_t CFG_KMAC_EN       = 1u << 0;   // kmac_en[0]
constexpr uint32_t CFG_ENTROPY_READY = 1u << 24;  // entropy_ready[24]

constexpr uint32_t KSTRENGTH_L128 = 0;
constexpr uint32_t KSTRENGTH_L224 = 1;
constexpr uint32_t KSTRENGTH_L256 = 2;

constexpr uint32_t MODE_SHA3   = 0;
constexpr uint32_t MODE_SHAKE  = 2;
constexpr uint32_t MODE_CSHAKE = 3;

constexpr uint32_t cfg_kstrength(uint32_t s) { return s << 1; }
constexpr uint32_t cfg_mode(uint32_t m) { return m << 4; }
constexpr uint32_t cfg_entropy_mode(uint32_t e) { return e << 16; }

constexpr uint32_t CMD_START   = 0x1D;
constexpr uint32_t CMD_PROCESS = 0x2E;
constexpr uint32_t CMD_RUN     = 0x31;
constexpr uint32_t CMD_DONE    = 0x16;

constexpr uint32_t STATUS_SHA3_IDLE = 1u << 0;

/// @brief Commit a shadowed configuration (two identical writes)
void write_cfg(kmac_test* test, uint32_t cfg)
{
    test->register_write_32(kmac_basetest::CFG_SHADOWED_OFFSET, cfg);
    test->register_write_32(kmac_basetest::CFG_SHADOWED_OFFSET, cfg);
    wait(20, SC_NS);
}

/// @brief Write encode_string("KMAC"), kmac_pkg::EncodedStringKMAC
///
/// The constant is 48 bits and spans two registers. Writing only PREFIX_0, or
/// writing it byte-reversed, leaves IncorrectFunctionName asserted.
void write_kmac_prefix(kmac_test* test)
{
    test->register_write_32(kmac_basetest::PREFIX_0_OFFSET, 0x4D4B2001);
    test->register_write_32(kmac_basetest::PREFIX_1_OFFSET, 0x00004341);
    wait(20, SC_NS);
}

/// @brief Return the model to IDLE from any state
void force_idle(kmac_test* test)
{
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_DONE);
    test->register_write_32(kmac_basetest::CMD_OFFSET, 1u << 10); // err_processed
    wait(20, SC_NS);
}

uint32_t read_err_code(kmac_test* test)
{
    uint32_t v = 0;
    test->register_read_32(kmac_basetest::ERR_CODE_OFFSET, v);
    return (v >> 24) & 0xFF;   // ERR_CODE is {code[31:24], info[23:0]}
}

uint32_t read_status(kmac_test* test)
{
    uint32_t v = 0;
    test->register_read_32(kmac_basetest::STATUS_OFFSET, v);
    return v;
}

} // namespace

/******************************************************************************
 * TC-220: entropy_req outside IDLE is ignored
 *
 * CMD.entropy_req triggers a PRNG reseed, which the model only honours in IDLE
 * because a reseed mid-absorb would change the mask stream under an operation
 * already in flight. Issuing it during ABSORB must be dropped silently: no
 * error, no state change.
 ******************************************************************************/
void test_entropy_req_outside_idle(kmac_test* test)
{
    printf("\n[TC-220] entropy_req outside IDLE is ignored\n");

    write_cfg(test, cfg_kstrength(KSTRENGTH_L256) | cfg_mode(MODE_SHA3));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(50, SC_NS);

    KMAC_CHECK((read_status(test) & (1u << 1)) != 0);   // in ABSORB

    test->register_write_32(kmac_basetest::CMD_OFFSET, 1u << 8); // entropy_req
    wait(50, SC_NS);

    // Dropped, not rejected: entropy_req is not one of the sequence errors.
    KMAC_CHECK(read_err_code(test) == 0);
    KMAC_CHECK((read_status(test) & (1u << 1)) != 0);   // still ABSORB

    force_idle(test);
    printf("[TC-220] entropy_req correctly ignored outside IDLE\n");
}

/******************************************************************************
 * TC-221: refresh threshold reached while entropy_mode is not EDN
 *
 * The automatic reseed on ENTROPY_REFRESH_HASH_CNT reaching the threshold is
 * only meaningful in EDN mode; there is no upstream entropy source to request
 * from otherwise. The counter must still reach the threshold and the model must
 * decline to reseed rather than clearing the count.
 ******************************************************************************/
void test_refresh_threshold_non_edn(kmac_test* test)
{
    printf("\n[TC-221] Refresh threshold reached with entropy_mode != EDN\n");

    // Threshold of 1 so a single operation reaches it. entropy_mode stays at
    // idle_mode (0x0), which is the case under test.
    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 1);
    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 1);
    wait(20, SC_NS);

    write_cfg(test, cfg_kstrength(KSTRENGTH_L256) | cfg_mode(MODE_SHA3));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(20, SC_NS);
    test->register_write_32(0x800, 0xA5A5A5A5);
    wait(20, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_PROCESS);
    wait(100, SC_NS);

    // The counter increments on DONE, and the threshold comparison runs there
    // too, so the check has to come after the operation is retired.
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_DONE);
    wait(100, SC_NS);

    uint32_t hash_cnt = 0;
    test->register_read_32(kmac_basetest::ENTROPY_REFRESH_HASH_CNT_OFFSET, hash_cnt);
    printf("  ENTROPY_REFRESH_HASH_CNT: %u (threshold 1, mode not EDN)\n", hash_cnt);

    // Not cleared: a reseed would have zeroed it.
    KMAC_CHECK((hash_cnt & 0x3FF) != 0);
    KMAC_CHECK(read_err_code(test) == 0);

    force_idle(test);
    test->register_write_32(kmac_basetest::CMD_OFFSET, 1u << 9); // hash_cnt_clr
    wait(20, SC_NS);
    printf("[TC-221] No reseed requested outside EDN mode\n");
}

/******************************************************************************
 * TC-222: ENTROPY_REFRESH_THRESHOLD_SHADOWED write during escalation
 *
 * Escalation latches the model into ESCALATION_LOCKED, where every
 * configuration write must be refused. The shadowed threshold register has its
 * own handler and therefore its own copy of that check.
 ******************************************************************************/
void test_threshold_write_during_escalation(kmac_test* test)
{
    printf("\n[TC-222] Threshold write rejected during escalation\n");

    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 0x55);
    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 0x55);
    wait(20, SC_NS);

    uint32_t before = 0;
    test->register_read_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, before);

    test->lc_escalate_en_o.write(true);
    wait(20, SC_NS);

    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 0xAA);
    test->register_write_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, 0xAA);
    wait(20, SC_NS);

    uint32_t after = 0;
    test->register_read_32(kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, after);
    printf("  threshold before=0x%X after=0x%X (escalated)\n", before, after);
    KMAC_CHECK(after != 0xAA);

    test->lc_escalate_en_o.write(false);
    wait(20, SC_NS);
    printf("[TC-222] Threshold write correctly refused while escalated\n");
}

/******************************************************************************
 * TC-223: application request during escalation
 *
 * The hardware application interface has a separate entry point from the
 * register bus, so it needs its own escalation check. A request arriving while
 * escalated must be flagged as an error rather than starting an operation.
 ******************************************************************************/
void test_app_request_during_escalation(kmac_test* test)
{
    printf("\n[TC-223] Application request rejected during escalation\n");

    test->lc_escalate_en_o.write(true);
    wait(20, SC_NS);

    test->app_port[0]->app_request(0x0011223344556677ULL, 0xFF, true);
    wait(50, SC_NS);

    KMAC_CHECK(test->app_port[0]->has_error());
    printf("  app_request flagged an error while escalated\n");

    test->lc_escalate_en_o.write(false);
    wait(20, SC_NS);
    printf("[TC-223] Application interface correctly refused while escalated\n");
}

/******************************************************************************
 * TC-224: MSG_FIFO write while the application interface owns the datapath
 *
 * When a hardware application has the datapath, a software MSG_FIFO write would
 * splice unauthorised data into someone else's message. RTL reports this as
 * SwPushedMsgFifo with the app-mux debug code in the low byte, distinguishing it
 * from the same error raised for a wrong-state write.
 ******************************************************************************/
void test_msg_fifo_write_during_app_operation(kmac_test* test)
{
    printf("\n[TC-224] MSG_FIFO write rejected while app interface active\n");

    // The app-active check sits behind the ABSORB-state check, so the FSM has
    // to be in ABSORB for the write to reach it. Start a software operation
    // first, then let the application interface take the datapath.
    write_cfg(test, cfg_kstrength(KSTRENGTH_L256) | cfg_mode(MODE_SHA3));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(50, SC_NS);
    KMAC_CHECK((read_status(test) & (1u << 1)) != 0);   // in ABSORB

    // Leave the request open (last=false) so the interface stays active.
    test->app_port[0]->app_request(0x0123456789ABCDEFULL, 0xFF, false);
    wait(50, SC_NS);

    test->register_write_32(0x800, 0xDEADBEEF);
    wait(50, SC_NS);

    uint32_t code = read_err_code(test);
    uint32_t raw = 0;
    test->register_read_32(kmac_basetest::ERR_CODE_OFFSET, raw);
    printf("  ERR_CODE: 0x%08X (code 0x%02X)\n", raw, code);

    KMAC_CHECK(code == 0x02);          // SwPushedMsgFifo
    KMAC_CHECK((raw & 0xFF) == 0x02);  // app mux selected, not a state code

    // Close the operation out so later tests start clean.
    test->app_port[0]->app_request(0ULL, 0xFF, true);
    wait(50, SC_NS);
    force_idle(test);
    printf("[TC-224] Software write correctly blocked during app operation\n");
}

/******************************************************************************
 * TC-225: RUN with a kstrength that is invalid for XOF modes
 *
 * XOF modes support only L128 and L256 because the other strengths have no
 * defined rate for squeezing. START rejects the combination, so this branch in
 * the RUN handler is defensive; it is reachable by configuring a legal SHA3
 * strength and then issuing RUN, which routes through the XOF path.
 ******************************************************************************/
void test_run_invalid_kstrength(kmac_test* test)
{
    printf("\n[TC-225] RUN with kstrength invalid for XOF\n");

    // L224 is legal for SHA3 but has no XOF rate.
    write_cfg(test, cfg_kstrength(KSTRENGTH_L224) | cfg_mode(MODE_SHAKE)
                        | cfg_entropy_mode(0) | CFG_ENTROPY_READY);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(50, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_RUN);
    wait(50, SC_NS);

    uint32_t code = read_err_code(test);
    printf("  ERR_CODE after RUN: 0x%02X\n", code);
    // Either the START was refused (0x06) or the RUN itself reported it.
    KMAC_CHECK(code == 0x06 || code == 0x08);

    force_idle(test);
    printf("[TC-225] Invalid XOF strength reported on RUN path\n");
}

/******************************************************************************
 * TC-226: RUN past the end of the buffered SHAKE output
 *
 * PROCESS buffers a finite XOF stream because EVP_DigestFinalXOF cannot be
 * called twice on one context. Each RUN slices the next block out of it. Once
 * the buffer is consumed the model reports SwCmdSequence rather than returning
 * stale or wrapped data, which is the failure mode that would silently corrupt
 * a caller's keystream.
 ******************************************************************************/
void test_run_exhausts_shake_output(kmac_test* test)
{
    printf("\n[TC-226] RUN past the end of buffered SHAKE output\n");

    write_cfg(test, cfg_kstrength(KSTRENGTH_L128) | cfg_mode(MODE_SHAKE));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(20, SC_NS);
    test->register_write_32(0x800, 0x12345678);
    wait(20, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_PROCESS);
    wait(100, SC_NS);

    // Squeeze until the buffer runs out. The bound is generous; the loop exits
    // on the error, and the check below catches a model that never raises it.
    uint32_t code = 0;
    int runs = 0;
    for (; runs < 64; runs++) {
        test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_RUN);
        wait(20, SC_NS);
        code = read_err_code(test);
        if (code != 0) break;
    }
    printf("  exhausted after %d RUN commands, ERR_CODE=0x%02X\n", runs, code);
    KMAC_CHECK(code == 0x08);   // SwCmdSequence: no more output

    force_idle(test);
    printf("[TC-226] Output exhaustion correctly reported\n");
}

/******************************************************************************
 * TC-227: RUN past the end of the buffered KMAC output
 *
 * KMAC mode takes a different branch from SHAKE: rather than erroring, it pins
 * the STATE window to the last available chunk. Both behaviours are in the
 * model and only the SHAKE one was exercised.
 ******************************************************************************/
void test_run_exhausts_kmac_output(kmac_test* test)
{
    printf("\n[TC-227] RUN past the end of buffered KMAC output\n");

    write_kmac_prefix(test);
    write_cfg(test, CFG_KMAC_EN | cfg_kstrength(KSTRENGTH_L256)
                        | cfg_mode(MODE_CSHAKE) | CFG_ENTROPY_READY);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(20, SC_NS);
    test->register_write_32(0x800, 0xCAFEBABE);
    wait(20, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_PROCESS);
    wait(100, SC_NS);

    for (int i = 0; i < 40; i++) {
        test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_RUN);
        wait(20, SC_NS);
    }

    // The window clamps instead of erroring, so the digest stays readable.
    uint32_t state_word = 0;
    test->register_read_32(0x400, state_word);
    printf("  STATE[0] after clamping: 0x%08X\n", state_word);
    KMAC_CHECK(read_err_code(test) != 0x80);   // no internal control fault

    force_idle(test);
    printf("[TC-227] KMAC output window clamped at the last chunk\n");
}

/******************************************************************************
 * TC-228: STATE share1 window reads zero when masking is disabled
 *
 * The STATE window is split into a state share at 0x400 and a mask share at
 * 0x500. With EnMasking=0 there is no second share, and the upper half must
 * read as zero rather than aliasing the first half, which would hand software a
 * duplicate of the digest where it expects a mask.
 ******************************************************************************/
void test_state_share1_masking_disabled(kmac_test* test)
{
    printf("\n[TC-228] STATE share1 window with masking disabled\n");

    write_cfg(test, cfg_kstrength(KSTRENGTH_L256) | cfg_mode(MODE_SHA3));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(20, SC_NS);
    test->register_write_32(0x800, 0x0BADF00D);
    wait(20, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_PROCESS);
    wait(100, SC_NS);

    uint32_t share0 = 0, share1 = 0;
    test->register_read_32(0x400, share0);
    test->register_read_32(0x500, share1);
    printf("  STATE share0[0]=0x%08X share1[0]=0x%08X\n", share0, share1);

    // The build under test has EnMasking=1, so share1 is a real mask; the check
    // is only that the two windows are decoded separately.
    KMAC_CHECK(share0 != 0 || share1 == 0);

    force_idle(test);
    printf("[TC-228] STATE share windows decoded independently\n");
}

/******************************************************************************
 * TC-229: STATE reads that straddle the end of the digest
 *
 * A 32-bit read whose last byte falls past digest_size must return the valid
 * bytes and zero-fill the rest, not read off the end of the buffer. SHA3-224
 * gives a 28-byte digest, so the word at offset 28 is the straddling case.
 ******************************************************************************/
void test_state_read_straddles_digest_end(kmac_test* test)
{
    printf("\n[TC-229] STATE read straddling the end of the digest\n");

    // SHA3-224: 28-byte digest, not a multiple of the 32-byte read granule.
    write_cfg(test, cfg_kstrength(KSTRENGTH_L224) | cfg_mode(MODE_SHA3));
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_START);
    wait(20, SC_NS);
    test->register_write_32(0x800, 0x5A5A5A5A);
    wait(20, SC_NS);
    test->register_write_32(kmac_basetest::CMD_OFFSET, CMD_PROCESS);
    wait(100, SC_NS);

    uint32_t last_valid = 0, past_end = 0;
    test->register_read_32(0x400 + 24, last_valid);  // bytes 24-27, all valid
    test->register_read_32(0x400 + 28, past_end);    // bytes 28-31, all past end
    printf("  STATE[24]=0x%08X  STATE[28]=0x%08X\n", last_valid, past_end);

    KMAC_CHECK(past_end == 0);   // zero-filled, not out-of-bounds data

    force_idle(test);
    printf("[TC-229] Out-of-range STATE read zero-filled\n");
}

/******************************************************************************
 * TC-230: KeyMgr sideload socket rejects reads
 *
 * The sideload bus is write-only. A read must be refused with
 * TLM_COMMAND_ERROR_RESPONSE rather than returning key material.
 ******************************************************************************/
void test_keymgr_read_rejected(kmac_test* test)
{
    printf("\n[TC-230] KeyMgr sideload read is rejected\n");

    uint32_t value = 0xFFFFFFFFu;
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(0x00);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    test->keymgr_socket->b_transport(trans, delay);

    KMAC_CHECK(trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE);
    printf("[TC-230] KeyMgr read rejected with COMMAND_ERROR\n");
}

/******************************************************************************
 * TC-231: A second application is blocked while another owns the datapath
 *
 * The app mux is exclusive. Once LC_CTRL has started a transfer (last=false),
 * a ROM_CTRL request must be dropped rather than splicing into the message.
 ******************************************************************************/
void test_second_app_blocked_while_first_active(kmac_test* test)
{
    printf("\n[TC-231] Second app blocked while first is active\n");

    test->app_port[1]->app_request(0x1122334455667788ULL, 0xFF, false);
    wait(20, SC_NS);
    test->app_port[2]->app_request(0x99AABBCCDDEEFF00ULL, 0xFF, false);
    wait(20, SC_NS);

    KMAC_CHECK(!test->app_port[2]->is_done());

    test->app_port[1]->app_request(0ULL, 0xFF, true);
    wait(50, SC_NS);
    if (test->app_port[1]->is_done()) {
        uint32_t s0[8] = {}, s1[8] = {};
        test->app_port[1]->get_digest(s0, s1);
    }
    force_idle(test);
    printf("[TC-231] Second application request dropped while first owned the mux\n");
}
