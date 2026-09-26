// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file key_manager_func006_test.cpp
 * @brief CMD_KEY_LOAD — host-supplied keys into the KPV over the mailbox
 *
 * CMD_KEY_LOAD (0x26) is the provisioning path that replaced the KPVLP window:
 * SEP hands the plaintext key over the mailbox and the KM places it in a vault
 * slot, returning an opaque handle. The handle's low byte is what the firmware
 * driver feeds straight into CMD_KEY_TRANSFER.
 *
 * Rejections all report RET_INVALID_ARG and carry the index of the offending
 * payload word, checked in the same order the firmware validates them.
 *
 * Tests covered:
 *   006a - CMD_KEY_LOAD with an 8-word key → RET_SUCCESS
 *   006b - ret_arg low byte = key handle in [1..255]
 *   006c - Payload shorter than the minimum three words → RET_INVALID_ARG, arg 0
 *   006d - Declared key size longer than the frame carries → RET_INVALID_ARG, arg 0
 *   006e - KEY_SIZE reserved bits [31:7] set → RET_INVALID_ARG, arg 0
 *   006f - DEST_VALID of zero → RET_INVALID_ARG, arg 1
 *   006g - DEST_VALID reserved bits [31:8] set → RET_INVALID_ARG, arg 1
 *   006h - A one-word key is accepted (minimum legal size)
 *   006i - Successive loads return distinct handles
 *   006j - A 16-word key fills exactly one slot
 *   006k - A 17-word key spans two slots. OTBN's sideload file is 12 words,
 *          the widest target, so words 0..11 are reconstructed from the
 *          transfer. Word 16 sits in the second slot and no engine register
 *          file is wide enough to observe it.
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>
#include <string>

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

int key_manager_func006_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* tb)
{
    int failures = 0;
    std::cout << "\n--- FUNC006: CMD_KEY_LOAD ---\n";

    test->trigger_reset();

    using fw = keymgr_tt::km_firmware_handler;

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;
    uint8_t seq = 0;

    // ------------------------------------------------------------------
    // 006a / 006b: load an 8-word key and collect the handle
    // ------------------------------------------------------------------
    std::vector<uint32_t> key_words;
    for (uint32_t w = 0; w < 8; w++)
        key_words.push_back(0xA0000000u | w);

    uint8_t handle_a = 0;
    ret_code = test->mb_load_key(seq, key_words, fw::DEST_HMAC, handle_a);
    CHECK_EQ(ret_code, 0, "006a: CMD_KEY_LOAD (8 words) ret_code = RET_SUCCESS");
    CHECK(handle_a >= 1u && handle_a <= 255u,
          "006b: ret_arg low byte = key handle in [1..255]");

    // A rejected CMD_KEY_LOAD reports INVALID_ARG (-7) and returns the index of
    // the payload word at fault.
    auto expect_invalid_arg = [&](const std::vector<uint32_t>& payload,
                                  uint32_t bad_word, const char* what) {
        test->mb_send_command(seq++, fw::CMD_KEY_LOAD, payload);
        bool ok = test->mb_receive_frame(frame);
        CHECK(ok, std::string(what) + ": response received");
        if (!ok) return;
        bool parsed_ok = key_manager_test::parse_resp_cmd(frame, resp_id, src_seq,
                                                         echoed_cmd, ret_code, ret_arg);
        CHECK(parsed_ok, std::string(what) + ": response parses as RESP_CMD");
        CHECK_EQ(ret_code, -7, std::string(what) + ": RET_INVALID_ARG (-7)");
        CHECK_EQ(ret_arg, bad_word, std::string(what) + ": ret_arg names the bad word");
    };

    // 006c: two words carry a size and a destination but no key at all.
    expect_invalid_arg({0u, fw::DEST_HMAC}, 0u, "006c: payload below minimum");

    // 006d: KEY_SIZE claims four words but only two follow.
    expect_invalid_arg({3u, fw::DEST_HMAC, 0x11111111u, 0x22222222u}, 0u,
                       "006d: declared size exceeds frame");

    // 006e: KEY_SIZE reserved bits above [6:0] must be zero.
    expect_invalid_arg({0x80u, fw::DEST_HMAC, 0x33333333u}, 0u,
                       "006e: KEY_SIZE reserved bits set");

    // 006f: a key with no permitted destination could never be used.
    expect_invalid_arg({0u, 0u, 0x44444444u}, 1u, "006f: DEST_VALID is zero");

    // 006g: DEST_VALID reserved bits above [7:0] must be zero.
    expect_invalid_arg({0u, 0x100u, 0x55555555u}, 1u,
                       "006g: DEST_VALID reserved bits set");

    // ------------------------------------------------------------------
    // 006h: one-word key is the minimum legal size
    // ------------------------------------------------------------------
    uint8_t handle_b = 0;
    ret_code = test->mb_load_key(seq, {0xDEADBEEFu}, fw::DEST_AES, handle_b);
    CHECK_EQ(ret_code, 0, "006h: CMD_KEY_LOAD (1 word) ret_code = RET_SUCCESS");
    CHECK(handle_b >= 1u, "006h: one-word key returns a usable handle");

    // ------------------------------------------------------------------
    // 006i: handles are distinct across loads
    // ------------------------------------------------------------------
    CHECK(handle_a != handle_b, "006i: successive loads return distinct handles");

    // ------------------------------------------------------------------
    // 006j / 006k: slot geometry at and just past the 16-word slot boundary.
    //   A 17-word key has to span two slots, so this is the case that would
    //   silently truncate if the loader ignored EXTEND.
    // ------------------------------------------------------------------
    {
        std::vector<uint32_t> key16;
        for (uint32_t w = 0; w < 16u; w++) key16.push_back(0xC0000000u | w);
        uint8_t h16 = 0;
        ret_code = test->mb_load_key(seq, key16, fw::DEST_HMAC, h16);
        CHECK_EQ(ret_code, 0, "006j: CMD_KEY_LOAD (16 words, one full slot) → RET_SUCCESS");
        CHECK(h16 >= 1u,      "006j: 16-word key returns a usable handle");

        std::vector<uint32_t> key17;
        for (uint32_t w = 0; w < 17u; w++) key17.push_back(0xD0000000u | w);
        uint8_t h17 = 0;
        ret_code = test->mb_load_key(seq, key17, fw::DEST_OTBN, h17);
        CHECK_EQ(ret_code, 0, "006k: CMD_KEY_LOAD (17 words, spans two slots) → RET_SUCCESS");
        CHECK(h17 >= 1u,      "006k: 17-word key returns a usable handle");

        // OTBN takes 12 words per share (SHARE1 base 0x30, KEY_CTRL at 0x60):
        // 12 SHARE0 + 12 SHARE1 + KEY_CTRL = 25 writes. Words 12..16 are past
        // that width, so this proves the prefix the firmware read back out of
        // the vault, including words that a one-slot truncation to 8 would drop.
        tb->otbn_stub.clear();
        test->mb_send_command(seq++, fw::CMD_KEY_TRANSFER,
                              {h17, static_cast<uint32_t>(fw::DEST_OTBN)});
        std::vector<uint32_t> xfer;
        const bool got = test->mb_receive_frame(xfer);
        CHECK(got, "006k: CMD_KEY_TRANSFER response received");
        int32_t xfer_rc = -1;
        uint32_t xfer_arg = 0;
        const bool parsed = key_manager_test::parse_resp_cmd(
            xfer, resp_id, src_seq, echoed_cmd, xfer_rc, xfer_arg);
        CHECK(parsed, "006k: transfer parses as RESP_CMD");
        CHECK_EQ(xfer_rc, 0, "006k: transfer of the 17-word key → RET_SUCCESS");

        const auto& wr = tb->otbn_stub.writes;
        bool rebuilt = parsed && xfer_rc == 0 && wr.size() == 25u
                    && wr[24].addr == 0x60u && wr[24].data == 1u;
        if (rebuilt) {
            for (int w = 0; w < 12; w++) {
                if (wr[w * 2].addr != static_cast<uint64_t>(w) * 4u ||
                    wr[w * 2 + 1].addr != 0x30u + static_cast<uint64_t>(w) * 4u ||
                    (wr[w * 2].data ^ wr[w * 2 + 1].data) != key17[w]) {
                    rebuilt = false;
                    break;
                }
            }
        }
        CHECK(rebuilt, "006k: OTBN SHARE0 XOR SHARE1 reconstructs key words 0..11");
    }

    std::cout << "\n--- FUNC006 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
