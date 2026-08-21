/**
 * @file key_manager_func014_test.cpp
 *
 * Covers:
 *   - Emergency wipe via wipe_ni & unrecoverable fault / KM flush flow
 *   - Inbound Mailbox Overflow & Overflow IRQ
 *   - Outbound Mailbox Overflow
 *   - Outbound Mailbox Underflow & Underflow IRQ
 *   - Sequence number error with prior valid RX
 *   - KPV slot resource exhaustion
 *   - Payload size/length validation error checks
 *   - Destination policy enforcement, including accepted-but-dropped ABR ports
 *   - kpv_ctrl_t serialization roundtrip
 *   - km_kpv direct methods (km_clear_key, km_is_valid, km_erase_key)
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>
#include <string>
#include <vector>

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
    // Load a key so the wipe has live vault state to destroy
    uint8_t wipe_handle = 0;
    ret_code = test->mb_load_key(seq, {0x11223344u, 0x55667788u},
                                 keymgr_tt::km_firmware_handler::DEST_HMAC, wipe_handle);
    CHECK(ret_code == 0 && wipe_handle != 0, "014a: CMD_KEY_LOAD success");

    bool got = false, parsed = false;

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
    // 014f: KPV Slot Resource Exhaustion
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    // Fill the vault with single-slot keys. KEY_SIZE is a word count, so asking
    // for 16 words takes exactly one 16-word slot; 32 of those exhaust the vault.
    // Some requests may fail early once the free slots are fragmented, so this
    // keeps going until a load is refused rather than assuming a fixed count.
    {
        bool exhausted = false;
        for (int i = 0; i < keymgr_tt::km_kpv::NUM_SLOTS + 4 && !exhausted; i++) {
            test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_GENERATE,
                                  {15u, 1u});
            got = test->mb_receive_frame(frame);
            parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                     ret_code, ret_arg);
            if (got && parsed && ret_code == -1) exhausted = true;
        }
        CHECK(exhausted, "014f: CMD_KEY_GENERATE eventually reports RET_FAILURE on a full KPV");

        // With no slot free, a host-supplied key has nowhere to go either.
        uint8_t full_handle = 0;
        ret_code = test->mb_load_key(seq, {0xAAAAAAAAu},
                                     keymgr_tt::km_firmware_handler::DEST_HMAC, full_handle);
        CHECK_EQ(ret_code, -1, "014f: CMD_KEY_LOAD on full KPV returns RET_FAILURE (-1)");
    }

    // ------------------------------------------------------------------
    // 014g: Payload Size & Length Validation Error Checks
    // ------------------------------------------------------------------
    // CMD_KEY_LOAD is variable-length, so it validates its own payload and
    // reports INVALID_ARG naming the offending word rather than INVALID_LEN.
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_KEY_LOAD, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -7, "014g: CMD_KEY_LOAD with empty payload -> RET_INVALID_ARG");

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
    CHECK(got && parsed && ret_code == -5, "014g: CMD_KEY_TRANSFER with short payload -> RET_INVALID_LEN");

    // CMD_ENGINE_SHRED with too short payload
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_ENGINE_SHRED, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == -5, "014g: CMD_ENGINE_SHRED with short payload -> RET_INVALID_LEN");

    // ------------------------------------------------------------------
    // 014h: CMD_RECOV_ACK Flow Check
    // ------------------------------------------------------------------
    test->mb_send_command(seq++, keymgr_tt::km_firmware_handler::CMD_RECOV_ACK, {});
    got = test->mb_receive_frame(frame);
    parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
    CHECK(got && parsed && ret_code == 0, "014h: CMD_RECOV_ACK processed successfully");

    // ------------------------------------------------------------------
    // 014i: Destination policy is enforced at transfer time
    //   The policy travels with the handle, so a key loaded for AES must be
    //   refused for HMAC. ABR destinations are accepted even though no engine
    //   model exists.
    // ------------------------------------------------------------------
    test->trigger_reset();
    seq = 0;

    {
        using fw = keymgr_tt::km_firmware_handler;

        uint8_t aes_handle = 0;
        ret_code = test->mb_load_key(seq, {0x12345678u, 0x9ABCDEF0u},
                                     fw::DEST_AES, aes_handle);
        CHECK(ret_code == 0, "014i: key loaded with DEST_AES policy");

        test->mb_send_command(seq++, fw::CMD_KEY_TRANSFER,
                              {static_cast<uint32_t>(aes_handle),
                               static_cast<uint32_t>(fw::DEST_HMAC)});
        got = test->mb_receive_frame(frame);
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
        CHECK(got && parsed && ret_code == -1,
              "014i: transfer to a destination outside the policy -> RET_FAILURE");

        // A key loaded for an ABR seed port transfers successfully onto the
        // ABR ML-DSA seed socket.
        uint8_t abr_handle = 0;
        ret_code = test->mb_load_key(seq, {0x0F0F0F0Fu},
                                     fw::DEST_ABR_MLDSA_SEED, abr_handle);
        CHECK(ret_code == 0, "014i: key loaded with an ABR seed destination");

        test->mb_send_command(seq++, fw::CMD_KEY_TRANSFER,
                              {static_cast<uint32_t>(abr_handle),
                               static_cast<uint32_t>(fw::DEST_ABR_MLDSA_SEED)});
        got = test->mb_receive_frame(frame);
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd, ret_code, ret_arg);
        CHECK(got && parsed && ret_code == 0,
              "014i: transfer to an ABR seed port is accepted");
    }

    // ------------------------------------------------------------------
    // 014j: kpv_ctrl_t serialization roundtrip (Helper code coverage)
    // ------------------------------------------------------------------
    keymgr_tt::kpv_ctrl_t ctrl_test;
    ctrl_test.lock_write = true;
    ctrl_test.lock_use = true;
    ctrl_test.extend = 3;
    ctrl_test.last_dword = 7;
    uint32_t ctrl_encoded = ctrl_test.to_uint32();

    keymgr_tt::kpv_ctrl_t ctrl_decoded;
    ctrl_decoded.from_uint32(ctrl_encoded);
    CHECK(ctrl_decoded.lock_write == ctrl_test.lock_write, "014j: kpv_ctrl_t lock_write roundtrip");
    CHECK(ctrl_decoded.lock_use == ctrl_test.lock_use, "014j: kpv_ctrl_t lock_use roundtrip");
    CHECK_EQ(ctrl_decoded.extend, ctrl_test.extend, "014j: kpv_ctrl_t extend roundtrip");
    CHECK_EQ(ctrl_decoded.last_dword, ctrl_test.last_dword, "014j: kpv_ctrl_t last_dword roundtrip");

    // ------------------------------------------------------------------
    // 014k: km_kpv direct methods (km_clear_key, km_is_valid, km_erase_key)
    // ------------------------------------------------------------------
    keymgr_tt::km_kpv local_kpv;
    local_kpv.reset();
    local_kpv.km_write_key_word(0, 0, 0x12345678);
    CHECK(local_kpv.km_is_valid(0) == false, "014k: km_is_valid returns false initially");
    local_kpv.km_set_valid(0, true);
    CHECK(local_kpv.km_is_valid(0) == true, "014k: km_is_valid returns true after set");
    local_kpv.km_clear_key(0);
    CHECK(local_kpv.km_is_valid(0) == false, "014k: km_is_valid returns false after clear");

    // ERASE must work through a write lock — that is the point of the hardware
    // path, since a locked slot would otherwise be unreclaimable.
    local_kpv.km_write_key_word(1, 0, 0xA5A5A5A5u);
    local_kpv.km_set_valid(1, true);
    local_kpv.km_lock_write(1);
    local_kpv.km_lock_use(1);
    CHECK(local_kpv.ctrl(1).lock_write, "014k: slot 1 is write-locked before erase");

    local_kpv.km_erase_key(1, []() { return 0xDEADBEEFu; });
    CHECK(!local_kpv.ctrl(1).lock_write, "014k: km_erase_key clears the control register");
    CHECK(!local_kpv.km_is_valid(1),     "014k: km_erase_key invalidates the slot");
    {
        uint32_t erased = 0;
        CHECK(local_kpv.km_read_key_word(1, 0, erased),
              "014k: erased slot is readable again (lock_use cleared)");
        CHECK_EQ(erased, 0xDEADBEEFu, "014k: km_erase_key overwrote the key word");
    }

    // ------------------------------------------------------------------
    // 014l: multi-slot key geometry round-trips through the vault
    //   A key wider than one 16-word slot is packed across consecutive slots,
    //   and its length is recovered from EXTEND plus the final slot's LAST_DWORD
    //   alone. These are the sizes where an off-by-one in that packing shows up.
    // ------------------------------------------------------------------
    {
        using keymgr_tt::km_kpv;

        const int sizes[] = {1, 15, 16, 17, 32, 33, km_kpv::MAX_KEY_WORDS};
        for (int len : sizes) {
            km_kpv vault;
            vault.reset();

            std::vector<uint32_t> key(static_cast<size_t>(len));
            for (int w = 0; w < len; w++)
                key[static_cast<size_t>(w)] = 0xE0000000u | static_cast<uint32_t>(w);

            const int expect_slots = (len - 1) / km_kpv::WORDS_PER_KEY + 1;
            CHECK_EQ(km_kpv::slots_for_words(len), expect_slots,
                     std::string("014l: slot count for ") + std::to_string(len) + " words");

            CHECK(vault.km_write_key(0, key.data(), len),
                  std::string("014l: wrote a ") + std::to_string(len) + "-word key");

            std::vector<uint32_t> read_back;
            CHECK(vault.km_read_key(0, read_back),
                  std::string("014l: read back a ") + std::to_string(len) + "-word key");
            CHECK(read_back == key,
                  std::string("014l: ") + std::to_string(len) + "-word key round-trips intact");
        }

        // A key too wide for the vault is refused rather than truncated.
        km_kpv vault;
        vault.reset();
        std::vector<uint32_t> too_wide(km_kpv::MAX_KEY_WORDS + 1, 0xFFFFFFFFu);
        CHECK_EQ(km_kpv::slots_for_words(km_kpv::MAX_KEY_WORDS + 1), 0,
                 "014l: a key beyond the maximum reports no slot count");
        CHECK(!vault.km_write_key(0, too_wide.data(),
                                  static_cast<int>(too_wide.size())),
              "014l: an over-wide key is refused");
    }

    // ------------------------------------------------------------------
    // 014m: handles are issued monotonically and never recycled, while the slots
    //   behind a revoked key do return to the pool. This is also the ordering that
    //   keeps a failed provisioning attempt from stranding a write-locked slot.
    // ------------------------------------------------------------------
    {
        using fw = keymgr_tt::km_firmware_handler;
        test->trigger_reset();
        seq = 0;

        uint8_t first = 0;
        ret_code = test->mb_load_key(seq, {0x11112222u}, fw::DEST_HMAC, first);
        CHECK(ret_code == 0 && first == 1u, "014m: the first key issued gets handle 1");

        test->mb_send_command(seq++, fw::CMD_KEY_REVOKE, {static_cast<uint32_t>(first)});
        got = test->mb_receive_frame(frame);
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                 ret_code, ret_arg);
        CHECK(got && parsed && ret_code == 0, "014m: the key is revoked");

        uint8_t second = 0;
        ret_code = test->mb_load_key(seq, {0x33334444u}, fw::DEST_HMAC, second);
        CHECK(ret_code == 0,      "014m: a key still loads after the revoke");
        CHECK(second != first,    "014m: the revoked handle is not handed out again");
        CHECK(second == 2u,       "014m: handles advance monotonically");

        // The revoked handle is gone for good, so using it fails.
        test->mb_send_command(seq++, fw::CMD_KEY_TRANSFER,
                              {static_cast<uint32_t>(first),
                               static_cast<uint32_t>(fw::DEST_HMAC)});
        got = test->mb_receive_frame(frame);
        parsed = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                 ret_code, ret_arg);
        CHECK(got && parsed && ret_code == -1,
              "014m: transferring a revoked handle -> RET_FAILURE");
    }

    std::cout << "\n--- FUNC014 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
