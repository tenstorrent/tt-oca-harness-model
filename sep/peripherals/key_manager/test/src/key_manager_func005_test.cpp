/**
 * @file key_manager_func005_test.cpp
 * @brief Command surface — retired commands are rejected, new ones respond
 *
 * This test guards the interface change rather than a feature. The new RTL
 * removed SEP's direct access to the vault, which retired CMD_KPVLP_SLOT_REQ
 * (0x20) and CMD_KEY_REGISTER (0x21); both must now look like unknown commands.
 * It also checks the commands added alongside CMD_KEY_LOAD report definite
 * outcomes instead of being silently unknown.
 *
 * Tests covered:
 *   005a - Retired CMD_KPVLP_SLOT_REQ (0x20) → RET_INVALID_CMD
 *   005b - Retired CMD_KEY_REGISTER   (0x21) → RET_INVALID_CMD
 *   005c - CMD_EXEC_ROM (0x10) → RET_SUCCESS
 *   005d - CMD_SRAM_LOAD_EXEC (0x11) → RET_FAILURE (no mutable firmware modelled)
 *   005e - CMD_SRAM_EXEC (0x12) → RET_FAILURE
 *   005f - CMD_ABR_SK_TRANSFER (0x27) → RET_FAILURE (no Adams Bridge model)
 *   005g - CMD_OTP_READ_LOCK_COLD (0x28) sets lock bits and accumulates them
 *   005h - CMD_OTP_READ_LOCK_COLD with empty payload → RET_INVALID_LEN
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

namespace {

/// Send a command, read the response, and hand back the return code and argument.
/// Returns false when no parsable response arrived.
bool send_expect(key_manager_test* test, uint8_t& seq, uint8_t cmd_id,
                 const std::vector<uint32_t>& payload,
                 int32_t& ret_code, uint32_t& ret_arg)
{
    test->mb_send_command(seq++, cmd_id, payload);

    std::vector<uint32_t> frame;
    if (!test->mb_receive_frame(frame)) return false;

    uint8_t resp_id = 0, src_seq = 0, echoed_cmd = 0;
    return key_manager_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                           ret_code, ret_arg);
}

} // namespace

int key_manager_func005_test(key_manager_test* test, key_manager_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC005: command surface (retired and added commands) ---\n";

    test->trigger_reset();

    using fw = keymgr_tt::km_firmware_handler;

    uint8_t  seq      = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;

    // ------------------------------------------------------------------
    // 005a / 005b: the retired KPVLP provisioning commands
    //   0x20 and 0x21 were the SEP-visible vault path. They must now be
    //   indistinguishable from any other unimplemented command ID.
    // ------------------------------------------------------------------
    bool ok = send_expect(test, seq, 0x20u, {0u}, ret_code, ret_arg);
    CHECK(ok, "005a: retired 0x20 produced a response");
    CHECK_EQ(ret_code, -4, "005a: CMD_KPVLP_SLOT_REQ (0x20) → RET_INVALID_CMD (-4)");

    ok = send_expect(test, seq, 0x21u, {0u, 0u, 0u, 0u}, ret_code, ret_arg);
    CHECK(ok, "005b: retired 0x21 produced a response");
    CHECK_EQ(ret_code, -4, "005b: CMD_KEY_REGISTER (0x21) → RET_INVALID_CMD (-4)");

    // ------------------------------------------------------------------
    // 005c-005e: ROM-mode and mutable-firmware commands
    // ------------------------------------------------------------------
    ok = send_expect(test, seq, fw::CMD_EXEC_ROM, {}, ret_code, ret_arg);
    CHECK(ok, "005c: CMD_EXEC_ROM produced a response");
    CHECK_EQ(ret_code, 0, "005c: CMD_EXEC_ROM → RET_SUCCESS");

    ok = send_expect(test, seq, fw::CMD_SRAM_LOAD_EXEC, {4u}, ret_code, ret_arg);
    CHECK(ok, "005d: CMD_SRAM_LOAD_EXEC produced a response");
    CHECK_EQ(ret_code, -1, "005d: CMD_SRAM_LOAD_EXEC → RET_FAILURE (-1)");

    ok = send_expect(test, seq, fw::CMD_SRAM_EXEC, {}, ret_code, ret_arg);
    CHECK(ok, "005e: CMD_SRAM_EXEC produced a response");
    CHECK_EQ(ret_code, -1, "005e: CMD_SRAM_EXEC → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 005f: Adams Bridge shared-key ingest has no engine to read from
    // ------------------------------------------------------------------
    ok = send_expect(test, seq, fw::CMD_ABR_SK_TRANSFER, {0u}, ret_code, ret_arg);
    CHECK(ok, "005f: CMD_ABR_SK_TRANSFER produced a response");
    CHECK_EQ(ret_code, -1, "005f: CMD_ABR_SK_TRANSFER → RET_FAILURE (-1)");

    // ------------------------------------------------------------------
    // 005g: OTP cold read-lock is write-one-to-set, so bits accumulate
    // ------------------------------------------------------------------
    ok = send_expect(test, seq, fw::CMD_OTP_READ_LOCK_COLD, {0x03u}, ret_code, ret_arg);
    CHECK(ok, "005g: CMD_OTP_READ_LOCK_COLD produced a response");
    CHECK_EQ(ret_code, 0,     "005g: CMD_OTP_READ_LOCK_COLD → RET_SUCCESS");
    CHECK_EQ(ret_arg,  0x03u, "005g: lock bits [1:0] latched");

    ok = send_expect(test, seq, fw::CMD_OTP_READ_LOCK_COLD, {0x0Cu}, ret_code, ret_arg);
    CHECK(ok, "005g: second CMD_OTP_READ_LOCK_COLD produced a response");
    CHECK_EQ(ret_arg, 0x0Fu, "005g: lock bits accumulate rather than replace");

    // A bit outside [5:0] must not stick.
    ok = send_expect(test, seq, fw::CMD_OTP_READ_LOCK_COLD, {0xFFu}, ret_code, ret_arg);
    CHECK(ok, "005g: masked CMD_OTP_READ_LOCK_COLD produced a response");
    CHECK_EQ(ret_arg, 0x3Fu, "005g: writes are masked to the six defined lock bits");

    // ------------------------------------------------------------------
    // 005h: empty payload is a length error
    // ------------------------------------------------------------------
    ok = send_expect(test, seq, fw::CMD_OTP_READ_LOCK_COLD, {}, ret_code, ret_arg);
    CHECK(ok, "005h: empty-payload response received");
    CHECK_EQ(ret_code, -5, "005h: CMD_OTP_READ_LOCK_COLD with no payload → RET_INVALID_LEN (-5)");

    std::cout << "\n--- FUNC005 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
