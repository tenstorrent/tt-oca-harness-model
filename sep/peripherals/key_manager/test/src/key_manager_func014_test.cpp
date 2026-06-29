/**
 * @file key_manager_func014_test.cpp
 *
 * Covers:
 *   - Emergency wipe via wipe_ni & unrecoverable fault / KM flush flow
 *   - Inbound Mailbox Overflow & Overflow IRQ
 *   - Outbound Mailbox Overflow
 *   - Outbound Mailbox Underflow & Underflow IRQ
 *   - Sequence number error with prior valid RX
 *   - KPVLP slot request resource exhaustion
 *   - Payload size/length validation error checks
 *   - kpv_ctrl_t serialization roundtrip
 *   - km_kpv unused methods (km_clear_key, km_is_valid)
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { std::cout << "[FAIL] " << msg << std::endl; failures++; } \
        else          { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

#define CHECK_EQ(got, expected, msg) \
    do { \
        if ((got) != (expected)) { \
            std::cout << "[FAIL] " << msg \
                      << "  got=0x" << std::hex << (uint32_t)(got) \
                      << "  expected=0x" << (uint32_t)(expected) << std::dec << std::endl; \
            failures++; \
        } else { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

int key_manager_func014_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC014: Additional Functional Coverage Tests ---\n";

    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // ------------------------------------------------------------------
    // 014a: Emergency Wipe via wipe_ni & Unrecoverable Fault / KM Flush Flow
    // ------------------------------------------------------------------
    // Request 2 slots, write key words, and register a key to setup state
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {1u});
    bool got = test->mb_receive_frame(frame);
    bool parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "014a: CMD_KPVLP_SLOT_REQ success");
    uint8_t base_slot = ret_arg & 0x1Fu;

    // Write key words via KPVLP
    test->register_write_32(key_manager_basetest::kpvlp_key_offset(base_slot, 0), 0x11223344);
    test->register_write_32(key_manager_basetest::kpvlp_key_offset(base_slot, 1), 0x55667788);

    // Set control register
    uint32_t ctrl_val = (1u << 9) | (7u << 17); // dest_valid=HMAC, last_dword=7
    test->register_write_32(key_manager_basetest::kpvlp_ctrl_offset(base_slot), ctrl_val);

    std::vector<uint32_t> reg_words = {0x11223344, 0x55667788, 0, 0, 0, 0, 0, 0};
    uint32_t key_crc = test->crc32_payload(reg_words.data(), reg_words.size());
    std::vector<uint32_t> reg_payload = {
        base_slot,
        7u, // KEY_SIZE_M1
        1u, // DEST_VALID
        key_crc
    };

    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER, reg_payload);
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "014a: CMD_KEY_REGISTER success");

    // Trigger active-low emergency wipe
    tb->wipe_n_signal.write(false);
    wait(10, SC_NS);

    // Verify no response can be read because mailbox is flushed during wipe
    got = test->mb_receive_frame(frame, 10);
    CHECK(!got, "014a: no response received during wipe because mailbox is flushed");

    // Check that IRQ FLUSHED_BY_KM is set (bit 4 of MB_IRQS)
    uint32_t mb_irqs = 0;
    test->register_read_32(key_manager_basetest::MB_IRQS_OFFSET, mb_irqs);
    CHECK(mb_irqs & (1u << 4), "014a: FLUSHED_BY_KM IRQ set in MB_IRQS");

    // Check KPVLP_STATUS is cleared
    uint32_t kpvlp_status = 0xFFFFFFFFu;
    test->register_read_32(key_manager_basetest::KPVLP_STATUS_OFFSET, kpvlp_status);
    CHECK_EQ(kpvlp_status, 0u, "014a: KPVLP_STATUS is 0 after wipe");

    // Verify model is halted (subsequent commands ignored)
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame, 20);
    CHECK(!got, "014a: no response received after halt");

    // Restore wipe signal
    tb->wipe_n_signal.write(true);
    wait(10, SC_NS);

    // ------------------------------------------------------------------
    // 014b: Inbound Mailbox Overflow & Overflow IRQ
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Write 17 words directly to MB_WDATA without setting separator
    for (int i = 0; i < 17; i++) {
        test->register_write_32(key_manager_basetest::MB_WDATA_OFFSET, i);
    }

    // Read MB_IRQS to check if INBOUND_OVERFLOW IRQ (bit 2) is raised
    uint32_t irqs_val = 0;
    test->register_read_32(key_manager_basetest::MB_IRQS_OFFSET, irqs_val);
    CHECK(irqs_val & (1u << 2), "014b: INBOUND_OVERFLOW IRQ set");

    // Read MB_STATUS to verify overflow flag is set (bit 20)
    uint32_t status_val = 0;
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(status_val & (1u << 20), "014b: MB_STATUS.inbound_overflow set");

    // Clear the overflow flag in MB_STATUS via W1C (bit 20)
    test->register_write_32(key_manager_basetest::MB_STATUS_OFFSET, 1u << 20);
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(!(status_val & (1u << 20)), "014b: MB_STATUS.inbound_overflow cleared after W1C");

    // Clear IRQ via W1C (bit 2)
    test->register_write_32(key_manager_basetest::MB_IRQS_OFFSET, 1u << 2);
    test->register_read_32(key_manager_basetest::MB_IRQS_OFFSET, irqs_val);
    CHECK(!(irqs_val & (1u << 2)), "014b: INBOUND_OVERFLOW IRQ cleared");

    // ------------------------------------------------------------------
    // 014c: Outbound Mailbox Overflow
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Send 5 HW_VER commands in a row without reading responses (5 * 4 = 20 words, exceeds FIFO_DEPTH=16)
    for (int i = 0; i < 5; i++) {
        test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    }
    wait(100, SC_NS); // wait for firmware to process all

    // Check MB_STATUS to verify outbound overflow flag (bit 21) is set
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(status_val & (1u << 21), "014c: MB_STATUS.outbound_overflow set");

    // Clear outbound overflow flag (bit 21)
    test->register_write_32(key_manager_basetest::MB_STATUS_OFFSET, 1u << 21);
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(!(status_val & (1u << 21)), "014c: MB_STATUS.outbound_overflow cleared");

    // ------------------------------------------------------------------
    // 014d: Outbound Mailbox Underflow & Underflow IRQ
    // ------------------------------------------------------------------
    // Flush both mailbox FIFOs
    test->register_write_32(key_manager_basetest::MB_CTRL_OFFSET, 0x4u);

    // Read MB_RDATA when empty
    uint32_t dummy_read = 0;
    test->register_read_32(key_manager_basetest::MB_RDATA_OFFSET, dummy_read);

    // Verify underflow IRQ (bit 3) is raised
    test->register_read_32(key_manager_basetest::MB_IRQS_OFFSET, irqs_val);
    CHECK(irqs_val & (1u << 3), "014d: OUTBOUND_UNDERFLOW IRQ set");

    // Verify underflow status (bit 23) is set
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(status_val & (1u << 23), "014d: MB_STATUS.outbound_underflow set");

    // Clear underflow status via W1C
    test->register_write_32(key_manager_basetest::MB_STATUS_OFFSET, 1u << 23);
    test->register_read_32(key_manager_basetest::MB_STATUS_OFFSET, status_val);
    CHECK(!(status_val & (1u << 23)), "014d: MB_STATUS.outbound_underflow cleared");

    // Clear IRQ via W1C
    test->register_write_32(key_manager_basetest::MB_IRQS_OFFSET, 1u << 3);
    test->register_read_32(key_manager_basetest::MB_IRQS_OFFSET, irqs_val);
    CHECK(!(irqs_val & (1u << 3)), "014d: OUTBOUND_UNDERFLOW IRQ cleared");

    // ------------------------------------------------------------------
    // 014e: Sequence Number Error with Prior Valid RX
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Send valid command (seq = 0)
    test->mb_send_command(seq, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "014e: Valid first command with seq=0");

    // Send sequence mismatch (expected seq = 1, send 5)
    test->mb_send_command(5, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -3, "014e: Sequence mismatch command rejected with RET_CMD_NOSEQ");
    CHECK_EQ(ret_arg, 0, "014e: Last valid sequence number returned is 0");

    // ------------------------------------------------------------------
    // 014f: KPVLP Slot Request Resource Exhaustion
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Request 8 slots 4 times via CMD_KEY_GENERATE (total 32 slots) to occupy KPV
    for (int i = 0; i < 4; i++) {
        test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE, {7u, 1u});
        got = test->mb_receive_frame(frame);
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
        CHECK(got && parsed && ret_code == 0, "014f: Allocated 8 slots successfully");
    }

    // Now request 1 more slot via CMD_KPVLP_SLOT_REQ (should fail)
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {0u});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -1, "014f: Slot request on full KPV returns RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 014g: Payload Size & Length Validation Error Checks
    // ------------------------------------------------------------------
    // CMD_KPVLP_SLOT_REQ with empty payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -5, "014g: CMD_KPVLP_SLOT_REQ with empty payload -> RET_INVALID_LEN");

    // CMD_KEY_REGISTER with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER, {0u, 0u});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -5, "014g: CMD_KEY_REGISTER with short payload -> RET_INVALID_LEN");

    // CMD_KEY_GENERATE with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -5, "014g: CMD_KEY_GENERATE with short payload -> RET_INVALID_LEN");

    // CMD_KEY_REVOKE with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REVOKE, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -5, "014g: CMD_KEY_REVOKE with short payload -> RET_INVALID_LEN");

    // CMD_KEY_TRANSFER with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_TRANSFER, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -1, "014g: CMD_KEY_TRANSFER with short payload -> RET_FAILURE");

    // CMD_ENGINE_SHRED with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -1, "014g: CMD_ENGINE_SHRED with short payload -> RET_FAILURE");

    // ------------------------------------------------------------------
    // 014h: CMD_RECOV_ACK Flow Check
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_RECOV_ACK, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "014h: CMD_RECOV_ACK processed successfully");

    // ------------------------------------------------------------------
    // 014i: Key Register Policy Verification Failures (dest_valid / last_dword mismatch)
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Request 1 slot (base_slot = 0)
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KPVLP_SLOT_REQ, {0u});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    base_slot = ret_arg & 0x1Fu;

    // Write a key word
    test->register_write_32(key_manager_basetest::kpvlp_key_offset(base_slot, 0), 0x12345678);

    // Set control register (dest_valid=AES(4), last_dword=7)
    ctrl_val = (4u << 9) | (7u << 17);
    test->register_write_32(key_manager_basetest::kpvlp_ctrl_offset(base_slot), ctrl_val);

    // Mismatched dest_valid: try to register with DEST_VALID = HMAC(1) instead of AES(4)
    std::vector<uint32_t> short_words = {0x12345678, 0, 0, 0, 0, 0, 0, 0};
    uint32_t short_crc = test->crc32_payload(short_words.data(), short_words.size());
    std::vector<uint32_t> fail_payload1 = { base_slot, 7u, 1u, short_crc };
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER, fail_payload1);
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -7, "014i: Mismatched dest_valid registration fails with RET_INVALID_ARG");
    CHECK_EQ(ret_arg, 2u, "014i: Error argument field index is 2 (DEST_VALID mismatch)");

    // Mismatched last_dword: try to register with KEY_SIZE_M1 = 3 (last_dword=3) instead of 7
    std::vector<uint32_t> fail_payload2 = { base_slot, 3u, 4u, short_crc };
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_REGISTER, fail_payload2);
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -7, "014i: Mismatched last_dword registration fails with RET_INVALID_ARG");
    CHECK_EQ(ret_arg, 1u, "014i: Error argument field index is 1 (KEY_SIZE_M1 mismatch)");

    // ------------------------------------------------------------------
    // 014j: kpv_ctrl_t serialization roundtrip (Helper code coverage)
    // ------------------------------------------------------------------
    keymgr_tt::kpv_ctrl_t ctrl_test;
    ctrl_test.lock_write = true;
    ctrl_test.lock_use = true;
    ctrl_test.unlock_sep = true;
    ctrl_test.extend = 3;
    ctrl_test.dest_valid = 5;
    ctrl_test.last_dword = 7;
    uint32_t ctrl_encoded = ctrl_test.to_uint32();

    keymgr_tt::kpv_ctrl_t ctrl_decoded;
    ctrl_decoded.from_uint32(ctrl_encoded);
    CHECK(ctrl_decoded.lock_write == ctrl_test.lock_write, "014j: kpv_ctrl_t lock_write roundtrip");
    CHECK(ctrl_decoded.lock_use == ctrl_test.lock_use, "014j: kpv_ctrl_t lock_use roundtrip");
    CHECK(ctrl_decoded.unlock_sep == ctrl_test.unlock_sep, "014j: kpv_ctrl_t unlock_sep roundtrip");
    CHECK_EQ(ctrl_decoded.extend, ctrl_test.extend, "014j: kpv_ctrl_t extend roundtrip");
    CHECK_EQ(ctrl_decoded.dest_valid, ctrl_test.dest_valid, "014j: kpv_ctrl_t dest_valid roundtrip");
    CHECK_EQ(ctrl_decoded.last_dword, ctrl_test.last_dword, "014j: kpv_ctrl_t last_dword roundtrip");

    // ------------------------------------------------------------------
    // 014k: km_kpv unused methods (km_clear_key, km_is_valid)
    // ------------------------------------------------------------------
    keymgr_tt::km_kpv local_kpv;
    local_kpv.reset();
    local_kpv.km_write_key_word(0, 0, 0x12345678);
    CHECK(local_kpv.km_is_valid(0) == false, "014k: km_is_valid returns false initially");
    local_kpv.km_set_valid(0, true);
    CHECK(local_kpv.km_is_valid(0) == true, "014k: km_is_valid returns true after set");
    local_kpv.km_clear_key(0);
    CHECK(local_kpv.km_is_valid(0) == false, "014k: km_is_valid returns false after clear");

    std::cout << "\n--- FUNC014 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
