/**
 * @file keymgr_tt_func004_test.cpp
 * @brief Message framing validation
 *
 * Tests covered:
 *   004a - Bad CRC-8 in header → RET_HEADER_CRC (seq counter NOT incremented)
 *   004b - Frame size mismatch (pay_len>0 but missing payload/CRC words) → RET_INVALID_LEN
 *   004c - Bad CRC-32 payload → RET_PAYLOAD_CRC
 *   004d - Out-of-order sequence number → RET_CMD_NOSEQ
 *   004e - Unknown cmd_id → RET_INVALID_CMD
 *   004f - After RET_CMD_NOSEQ, send correct seq → success (seq counter recovered)
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

// Build a raw header word with deliberate bad CRC-8.
static uint32_t make_bad_crc_header(uint8_t seq, uint8_t cmd_id, uint8_t pay_len)
{
    uint32_t h24 = static_cast<uint32_t>(seq)
                 | (static_cast<uint32_t>(cmd_id)  << 8)
                 | (static_cast<uint32_t>(pay_len)  << 16);
    uint8_t good_crc = keymgr_tt_test::crc8_header(h24);
    uint8_t bad_crc  = static_cast<uint8_t>(good_crc ^ 0xFFu);  // flip all bits
    return h24 | (static_cast<uint32_t>(bad_crc) << 24);
}

int keymgr_tt_func004_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC004: Message Framing Validation ---\n";

    // Clean state — firmware seq counter starts at 0.
    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t fw_seq = 0;  // tracks firmware's expected m_next_rx_seq

    // ------------------------------------------------------------------
    // 004a: Bad CRC-8 header with seq=0 → RET_HEADER_CRC
    //   Firmware seq counter stays at 0 (CRC8 check fails before seq check).
    // ------------------------------------------------------------------
    uint32_t bad_hdr = make_bad_crc_header(fw_seq, keymgr_tt::km_firmware_handler::CMD_HW_VER, 0);
    test->mb_send_raw_frame({bad_hdr});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "004a: response received for bad CRC-8 frame");
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,                 "004a: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -2,        "004a: ret_code = RET_HEADER_CRC (-2)");
    // fw_seq stays 0 — do NOT increment.

    // ------------------------------------------------------------------
    // 004b: Frame size mismatch — claim pay_len=1 but send only the header word.
    //   Firmware: CRC8 OK → seq check passes (seq=0, m_next_rx_seq increments
    //   to 1) → frame size check fails → RET_INVALID_LEN.
    // ------------------------------------------------------------------
    {
        uint32_t h24 = static_cast<uint32_t>(fw_seq)
                     | (static_cast<uint32_t>(keymgr_tt::km_firmware_handler::CMD_HW_VER) << 8)
                     | (1u << 16);  // pay_len=1
        uint8_t  crc = keymgr_tt_test::crc8_header(h24);
        uint32_t hdr = h24 | (static_cast<uint32_t>(crc) << 24);
        // Send only the header (no payload, no CRC32 word) — size mismatch
        test->mb_send_raw_frame({hdr});
    }
    got = test->mb_receive_frame(frame);
    CHECK(got, "004b: response received for size-mismatch frame");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,              "004b: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -5,     "004b: ret_code = RET_INVALID_LEN (-5)");
    fw_seq++;  // seq check passed → counter incremented to 1

    // ------------------------------------------------------------------
    // 004c: Bad CRC-32 payload — send CMD_HW_VER with 1 payload word + corrupted CRC32
    //   Firmware: CRC8 OK → seq OK (seq=1→2) → size OK → CRC32 mismatch → RET_PAYLOAD_CRC
    // ------------------------------------------------------------------
    {
        uint32_t payload_word = 0xDEADBEEFu;
        uint32_t good_crc32   = keymgr_tt_test::crc32_payload(&payload_word, 1);
        uint32_t bad_crc32    = good_crc32 ^ 0xFFFFFFFFu;  // corrupt CRC32

        uint32_t h24 = static_cast<uint32_t>(fw_seq)
                     | (static_cast<uint32_t>(keymgr_tt::km_firmware_handler::CMD_HW_VER) << 8)
                     | (1u << 16);  // pay_len=1
        uint8_t  crc8 = keymgr_tt_test::crc8_header(h24);
        uint32_t hdr  = h24 | (static_cast<uint32_t>(crc8) << 24);

        test->mb_send_raw_frame({hdr, payload_word, bad_crc32});
    }
    got = test->mb_receive_frame(frame);
    CHECK(got, "004c: response received for bad CRC-32 frame");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "004c: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -6, "004c: ret_code = RET_PAYLOAD_CRC (-6)");
    fw_seq++;  // seq check passed → counter incremented to 2

    // ------------------------------------------------------------------
    // 004d: Out-of-order sequence — send seq=99 when firmware expects fw_seq=2
    //   Firmware: CRC8 OK → seq mismatch → RET_CMD_NOSEQ
    //   Since m_has_valid_rx is still false (no fully-valid cmd yet), no ret_arg.
    // ------------------------------------------------------------------
    test->mb_send_command(99, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "004d: response received for out-of-order seq");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "004d: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -3, "004d: ret_code = RET_CMD_NOSEQ (-3)");
    // fw_seq stays 2 — seq check failed, no increment

    // ------------------------------------------------------------------
    // 004e: Unknown cmd_id (0xAA) with correct seq → RET_INVALID_CMD
    // ------------------------------------------------------------------
    test->mb_send_command(fw_seq, 0xAAu, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "004e: response received for unknown cmd_id");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,          "004e: response parses as RESP_CMD");
    CHECK_EQ(ret_code, -4, "004e: ret_code = RET_INVALID_CMD (-4)");
    fw_seq++;  // passes seq check → incremented to 3

    // ------------------------------------------------------------------
    // 004f: After RET_CMD_NOSEQ, correct seq succeeds
    //   (fw_seq=3 now, after 004e increment)
    //   Verify RET_SUCCESS to confirm seq resynchronised.
    // ------------------------------------------------------------------
    test->mb_send_command(fw_seq, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    got = test->mb_receive_frame(frame);
    CHECK(got, "004f: response received for correctly-sequenced cmd after NOSEQ");
    parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                             ret_code, ret_arg);
    CHECK(parsed,         "004f: response parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "004f: ret_code = RET_SUCCESS after seq recovery");

    std::cout << "\n--- FUNC004 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
