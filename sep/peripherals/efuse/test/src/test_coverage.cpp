// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "efuse_basetest.h"
#include <fstream>
#include <iomanip>
#include <sstream>

// Fuse-array word indices for the registers the .preload images below populate.
// Derived from PeakRDL register offsets (LOCKS at 0x0, LC_STATE at 0x8 = word 2).
namespace {
constexpr unsigned int word_of(unsigned int byte_offset) { return byte_offset / 4u; }

constexpr unsigned int LC_STATE_WORD         = word_of(sep_efuse::LC_STATE_OFFSET);
constexpr unsigned int TRANSIENT_RMA_EN_WORD = word_of(sep_efuse::TRANSIENT_RMA_EN_OFFSET);
constexpr unsigned int RMA_SIP_TOKEN_WORD0   = word_of(sep_efuse::RMA_SIP_TOKEN_OFFSET);
constexpr unsigned int RMA_CHIPLET_TOKEN_WORD0 = word_of(sep_efuse::RMA_CHIPLET_TOKEN_OFFSET);
constexpr unsigned int TOKEN_WORDS = 8u;   // both digests are 256-bit

// OTP-interface bit addresses. The raw interface is addressed by fuse-array BIT,
// so a register's base bit is its word index times 32. These were literals too
// (LC_STATE at 64, BL1_VERSION at 34*32) and moved with everything else.
constexpr unsigned int LC_STATE_BIT    = LC_STATE_WORD * 32u;
constexpr unsigned int BL1_VERSION_BIT = word_of(sep_efuse::BL1_VERSION_OFFSET) * 32u;
constexpr unsigned int CHIPLET_UID_BIT = word_of(sep_efuse::CHIPLET_UID_OFFSET) * 32u;
constexpr unsigned int SPARE_TEST_BIT  = word_of(sep_efuse::SYS_PUBK_PQC_HASH_OFFSET) * 32u;
}  // namespace

// ============================================================================
// Coverage tests — exercise WOSET handlers and interface-ctrl pulse paths
// in src/efuse.cpp that are not hit by Tests 1–8.
// ============================================================================

// std::to_string on a register value prints decimal, which reads as a different
// number entirely next to a "0x".
static std::string hex32(uint32_t v)
{
    std::ostringstream os;
    os << std::hex << v;
    return os.str();
}

void testbench::test_woset_locks_hi()
{
    report_test_start("Test 9: WOSET — LOCKS_HI");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::LOCKS_HI_OFFSET, 0x00000005);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_HI_OFFSET, val);
    if (val == 0x00000005)
        report_test_pass("LOCKS_HI: first write sets bits 0x5");
    else
        report_test_fail("LOCKS_HI first write", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::LOCKS_HI_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_HI_OFFSET, val);
    if (val == 0x00000005)
        report_test_pass("LOCKS_HI: bit 0 stays set after write-0 attempt (WOSET)");
    else
        report_test_fail("LOCKS_HI WOSET", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::LOCKS_HI_OFFSET, 0x000000F0);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_HI_OFFSET, val);
    if (val == 0x000000F5)
        report_test_pass("LOCKS_HI: additional bits ORed correctly");
    else
        report_test_fail("LOCKS_HI OR accumulation", "expected 0xF5 got 0x" + std::to_string(val));
}

/*
 * LC_STATE is not a woset like its neighbours: the write goes through the transition
 * machine and the register always ends up holding a legal differential code. The
 * config puts the part in PROD, which is where the interesting refusals live -- from
 * PROD the only way out is an RMA token, and there is no matching token yet.
 */
void testbench::test_woset_lc_state()
{
    report_test_start("Test 10: LC_STATE transition machine — refusals");

    uint32_t val = 0;
    const uint32_t prod = efuse_model::lc_state_encode(efuse_model::LC_RAW_PROD);

    auto expect_state = [&](uint32_t write, uint32_t want, const std::string &what) {
        m_test->register_write_32(sep_efuse::LC_STATE_OFFSET, write);
        wait(1, SC_NS);
        m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
        if (val == want)
            report_test_pass(what);
        else
            report_test_fail(what, "expected 0x" + hex32(want) + " got 0x" + hex32(val));
    };

    expect_state(efuse_model::LC_RAW_PROD, prod,
                 "LC_STATE: rewriting the current state is a no-op");
    expect_state(efuse_model::LC_RAW_RMA_SIP_0, prod,
                 "LC_STATE: RMA_SIP refused without a SiP token match");
    expect_state(efuse_model::LC_RAW_PROD_END, prod,
                 "LC_STATE: PROD_END is unreachable from PROD");
    expect_state(0x4u, prod,
                 "LC_STATE: a write whose destination is not a state is refused");
    expect_state(0xFFFFFFFFu, prod,
                 "LC_STATE: an all-ones write cannot force an illegal code");

    if (m_dut->get_lc_state() == efuse_model::LC_RAW_PROD)
        report_test_pass("LC_STATE: get_lc_state() still decodes PROD");
    else
        report_test_fail("LC_STATE get_lc_state()", "expected raw 0x1 got 0x" +
            hex32(m_dut->get_lc_state()));
}

void testbench::test_woset_sip_dis_hi()
{
    report_test_start("Test 11: WOSET — SIP_DIS_HI");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::SIP_DIS_HI_OFFSET, 0x3);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SIP_DIS_HI_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SIP_DIS_HI: bits 0,1 set");
    else
        report_test_fail("SIP_DIS_HI set", "expected 0x3 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::SIP_DIS_HI_OFFSET, 0x0);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SIP_DIS_HI_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SIP_DIS_HI: WOSET holds after write-0");
    else
        report_test_fail("SIP_DIS_HI WOSET", "bits cleared, expected 0x3 got 0x" + std::to_string(val));
}

void testbench::test_woset_sys_dis_hi()
{
    report_test_start("Test 12: WOSET — SYS_DIS_HI");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::SYS_DIS_HI_OFFSET, 0x6);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SYS_DIS_HI_OFFSET, val);
    if (val == 0x6)
        report_test_pass("SYS_DIS_HI: bits 1,2 set");
    else
        report_test_fail("SYS_DIS_HI set", "expected 0x6 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::SYS_DIS_HI_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SYS_DIS_HI_OFFSET, val);
    if (val == 0x7)
        report_test_pass("SYS_DIS_HI: additional bit ORed");
    else
        report_test_fail("SYS_DIS_HI OR", "expected 0x7 got 0x" + std::to_string(val));
}

void testbench::test_woset_chiplet_pubk_revoke()
{
    report_test_start("Test 13: WOSET — CHIPLET_PUBK_REVOKE");

    uint32_t val = 0;
    const uint32_t loaded = m_dut->chiplet_pubk_revoke.get_param_value();

    m_test->register_write_32(sep_efuse::CHIPLET_PUBK_REVOKE_OFFSET, 0x5);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::CHIPLET_PUBK_REVOKE_OFFSET, val);
    if (val == (loaded | 0x5u))
        report_test_pass("CHIPLET_PUBK_REVOKE: runtime WOSET ORs with fuse value");
    else
        report_test_fail("CHIPLET_PUBK_REVOKE WOSET", "expected 0x" + std::to_string(loaded | 0x5u) +
            " got 0x" + std::to_string(val));
}

void testbench::test_woset_bl_version()
{
    report_test_start("Test 14: WOSET — BL1/BL2_VERSION arrays");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::BL1_VERSION_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    if (val == 0x1)
        report_test_pass("BL1_VERSION[0]: WOSET sets bit 0");
    else
        report_test_fail("BL1_VERSION[0]", "expected 0x1 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::BL1_VERSION_OFFSET + 4, 0x00000002);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET + 4, val);
    if (val == 0x2)
        report_test_pass("BL1_VERSION[1]: per-word WOSET");
    else
        report_test_fail("BL1_VERSION[1]", "expected 0x2 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::BL2_VERSION_OFFSET, 0x00000004);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL2_VERSION_OFFSET, val);
    if (val == 0x4)
        report_test_pass("BL2_VERSION[0]: WOSET sets bit 2");
    else
        report_test_fail("BL2_VERSION[0]", "expected 0x4 got 0x" + std::to_string(val));
}

void testbench::test_efuse_write_ctrl_go()
{
    report_test_start("Test 15: EFUSE_PROGRAM_CTRL — program_go without program_enable");

    uint32_t val = 0;

    // Pulse bit 17 (program_go) with bit 24 (program_busy) set, but leave
    // program_enable (bit 27) clear. efuse_program_interface.sv answers
    // done-with-error rather than stalling or programming anything.
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, (1u << 17) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25) | (1u << 26);  // done=1, status=error, go/busy cleared
    if (val == expected)
        report_test_pass("EFUSE_PROGRAM_CTRL: gated program reports done + error");
    else
        report_test_fail("EFUSE_PROGRAM_CTRL write_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));

    // The refusal is sticky in STATUS.efuse_req_error until explicitly cleared.
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val & (1u << 4))
        report_test_pass("STATUS: efuse_req_error latched by the refused program");
    else
        report_test_fail("STATUS efuse_req_error", "expected bit 4 set, got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("STATUS: efuse_req_err_clear clears it, sense_done still set");
    else
        report_test_fail("STATUS req_err_clear", "expected 0x1 got 0x" + std::to_string(val));
}

void testbench::test_efuse_read_ctrl_go()
{
    report_test_start("Test 16: EFUSE_READ_CTRL — read_go without read_enable");

    uint32_t val = 0;

    // Same shape as Test 15, on the read side: read_enable is bit 28.
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, (1u << 16) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25) | (1u << 26);
    if (val == expected)
        report_test_pass("EFUSE_READ_CTRL: gated read reports done + error");
    else
        report_test_fail("EFUSE_READ_CTRL read_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));

    // A refused read must not leave data behind for a caller that skips the status.
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0u)
        report_test_pass("EFUSE_READ_INTERFACE_READ_DATA: zero after a refused read");
    else
        report_test_fail("read data after refusal", "expected 0x0 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
}

void testbench::test_fuse_array_program_read()
{
    report_test_start("Test 19: Fuse array — program, read back, and program-once");

    uint32_t val = 0;

    const uint32_t enable_read = (1u << 28);

    // The array and the shadow map are the same 8192 bits, so a fuse-backed field
    // must read identically through both paths. CHIPLET_UID is read-only to
    // software, so no earlier test has disturbed it: byte offset 0xC8 is array word
    // its word index, hence CHIPLET_UID_BIT.
    uint32_t shadow_uid = 0;
    m_test->register_read_32(sep_efuse::CHIPLET_UID_OFFSET, shadow_uid);

    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | CHIPLET_UID_BIT);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("enabled read completes without error");
    else
        report_test_fail("enabled read", "expected done + no error, got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == shadow_uid)
        report_test_pass("OTP read agrees with the shadow register over the same bits");
    else
        report_test_fail("array/shadow agreement", "shadow 0x" + std::to_string(shadow_uid) +
            " vs OTP 0x" + std::to_string(val));

    // A shadow write must not reach the array. Silicon latches shadow writes in
    // flops and only EFUSE_PROGRAM_CTRL burns fuses, so LOCKS accumulating WOSET
    // bits at the register interface leaves the corresponding fuses erased -- the
    // two deliberately diverge after any shadow write.
    uint32_t shadow_locks = 0;
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, shadow_locks);
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | 0u);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (shadow_locks != 0u && val != shadow_locks)
        report_test_pass("shadow writes do not burn fuses (LOCKS diverges from the array)");
    else
        report_test_fail("shadow write isolation", "shadow 0x" + std::to_string(shadow_locks) +
            " unexpectedly equals OTP 0x" + std::to_string(val));

    // Program a bit in a word nothing else uses, then read it back. Any
    // unlocked word will do; SYS_PUBK_PQC_HASH is one nothing in this suite
    // touches. (This used to name RESERVED_2, a block the map no longer has.)
    const uint32_t test_bit = SPARE_TEST_BIT + 3u;
    const uint32_t enable_prog = (1u << 27) | (1u << 16);  // program_enable, data=1
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (1u << 18) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("enabled program completes without error");
    else
        report_test_fail("enabled program", "expected done + no error, got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_INTERFACE_RD_DATA_OFFSET, val);
    if (val == (1u << 3))
        report_test_pass("program read-back returns the word with the new bit set");
    else
        report_test_fail("program read-back", "expected 0x8 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val & (1u << 3))
        report_test_pass("programmed bit reads back as 1");
    else
        report_test_fail("programmed bit read-back", "expected bit 3 set, got 0x" + std::to_string(val));

    // Programming data=0 is rejected outright rather than clearing the bit: the
    // array is one-time-programmable, so nothing can unburn bit 3.
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              (1u << 27) | (1u << 17) | test_bit);   // data=0
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("program with data=0 is refused");
    else
        report_test_fail("program data=0", "expected error status, got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val & (1u << 3))
        report_test_pass("burned bit survives the attempt to clear it");
    else
        report_test_fail("program-once", "bit 3 was cleared, got 0x" + std::to_string(val));

    // Out-of-range addresses latch their own sticky error, distinct from req_error.
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | 8192u);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val & (1u << 6))
        report_test_pass("STATUS: read_addr_error latched by an out-of-range read");
    else
        report_test_fail("read_addr_error", "expected bit 6 set, got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, (1u << 8) | (1u << 10));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("STATUS: addr-error clear returns the register to sense_done only");
    else
        report_test_fail("addr-error clear", "expected 0x1 got 0x" + std::to_string(val));
}

void testbench::test_otp_accessors()
{
    report_test_start("Test 17: OTP accessors — get_lc_state / get_chiplet_uid");

    // get_lc_state() returns the raw state, get_lc_state_code() the fuse contents;
    // the latter is what software reads from the register.
    const uint32_t lc = m_dut->get_lc_state();
    uint32_t reg = 0;
    m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, reg);
    if (m_dut->get_lc_state_code() == reg)
        report_test_pass("get_lc_state_code() matches LC_STATE register");
    else
        report_test_fail("get_lc_state_code()", "mismatch with register read");

    const uint32_t expect_raw = efuse_model::lc_state_code_valid(reg)
                                  ? (reg & 0xFu) : efuse_model::LC_STATE_RAW_INVALID;
    if (lc == expect_raw)
        report_test_pass("get_lc_state() returns the decoded raw state");
    else
        report_test_fail("get_lc_state()", "expected raw 0x" + std::to_string(expect_raw) +
            " got 0x" + std::to_string(lc));

    const uint32_t *uid = m_dut->get_chiplet_uid();
    bool match = true;
    for (int i = 0; i < 8; ++i) {
        uint32_t word = 0;
        m_test->register_read_32(sep_efuse::CHIPLET_UID_OFFSET + static_cast<unsigned>(i * 4), word);
        if (word != uid[i]) {
            match = false;
            break;
        }
    }
    if (match)
        report_test_pass("get_chiplet_uid() matches CHIPLET_UID array");
    else
        report_test_fail("get_chiplet_uid()", "mismatch with register reads");
}

void testbench::test_shim_ctrl_window()
{
    report_test_start("Test 18: EFUSE_SHIM_CTRL — separate window R/W");

    // The shim answers on its own socket, so an offset here must not reach the
    // register the same offset names in the sep_efuse window. LOCKS_LO sits at
    // 0x000 there and is WOSET, which would make a clean read-back impossible if
    // the two windows were ever collapsed back into one.
    uint32_t val = 0;

    m_test->shim_read_32(sep_efuse::EFUSE_BANK_INIT_TIME_OFFSET, val);
    if (val == 0x00000020)
        report_test_pass("EFUSE_BANK_INIT_TIME: reset value 0x20");
    else
        report_test_fail("EFUSE_BANK_INIT_TIME reset", "expected 0x20 got 0x" + std::to_string(val));

    m_test->shim_write_32(sep_efuse::EFUSE_BANK_INIT_TIME_OFFSET, 0x0000ABCD);
    wait(1, SC_NS);
    m_test->shim_read_32(sep_efuse::EFUSE_BANK_INIT_TIME_OFFSET, val);
    if (val == 0x0000ABCD)
        report_test_pass("EFUSE_BANK_INIT_TIME: read-back at shim offset 0x000");
    else
        report_test_fail("EFUSE_BANK_INIT_TIME", "expected 0xABCD got 0x" + std::to_string(val));

    // The windows are disjoint: writing the shim must leave LOCKS_LO alone.
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val != 0x0000ABCD)
        report_test_pass("shim writes do not alias into the sep_efuse window");
    else
        report_test_fail("window isolation", "LOCKS_LO took the shim's value");
}

void testbench::test_fuse_preload_file()
{
    report_test_start("Test 20: Fuse array — .preload image backdoor");

    uint32_t val = 0;

    // Written here rather than shipped as a fixture so the test is independent of
    // where it runs from, and so the malformed cases below can be constructed too.
    // Format is the RTL's: one ASCII bit per line, LSB first.
    const std::string good = "/tmp/efuse_test_good.preload";
    {
        std::ofstream out(good);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            // Write 0xF0 into LC_STATE's low byte -- the differential encoding
            // of TEST_DEV that the RTL default image carries. The window is
            // computed from the register's offset so it follows the map.
            const bool set = (bit >= LC_STATE_BIT + 4u && bit <= LC_STATE_BIT + 7u);
            out << (set ? '1' : '0') << "\n";
        }
    }

    if (m_dut->preload_fuses_from_file(good))
        report_test_pass("preload image accepted");
    else
        report_test_fail("preload accept", "a well-formed image was rejected");

    m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("LC_STATE sensed from the image as 0xF0 (TEST_DEV)");
    else
        report_test_fail("preload sense", "expected 0xF0 got 0x" + std::to_string(val));

    // The image replaces the array wholesale, so previously programmed bits are gone.
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | LC_STATE_BIT);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("OTP read of the LC_STATE word matches the image");
    else
        report_test_fail("preload array", "expected 0xF0 got 0x" + std::to_string(val));

    // A malformed image must be refused outright and leave the array as it was,
    // rather than committing a half-parsed load.
    const std::string bad = "/tmp/efuse_test_bad.preload";
    {
        std::ofstream out(bad);
        out << "0\n1\n0\nX\n0\n";
    }
    if (!m_dut->preload_fuses_from_file(bad))
        report_test_pass("malformed image refused");
    else
        report_test_fail("preload reject", "a bad character was accepted");

    m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("refused image left the array untouched");
    else
        report_test_fail("preload atomicity", "array changed, LC_STATE now 0x" + std::to_string(val));

    if (!m_dut->preload_fuses_from_file("/tmp/efuse_no_such_file.preload"))
        report_test_pass("missing image refused");
    else
        report_test_fail("preload missing", "a nonexistent path was accepted");

    // Blank and whitespace-only lines are skipped rather than counted as bits.
    const std::string spaced = "/tmp/efuse_test_spaced.preload";
    {
        std::ofstream out(spaced);
        out << "1\n\n  \n1\n";   // two bits, at positions 0 and 1
    }
    if (m_dut->preload_fuses_from_file(spaced))
        report_test_pass("whitespace-tolerant image accepted");
    else
        report_test_fail("preload whitespace", "blank lines were treated as an error");

    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val == 0x3u)
        report_test_pass("blank lines skipped: bits landed at 0 and 1");
    else
        report_test_fail("preload whitespace bits", "expected 0x3 got 0x" + std::to_string(val));
}

void testbench::test_fuse_program_out_of_range()
{
    report_test_start("Test 21: Fuse array — out-of-range program");

    uint32_t val = 0;

    // 8192 is one past the last bit. The command completes with an error and
    // latches its own sticky flag, distinct from the read side's.
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              (1u << 27) | (1u << 16) | (1u << 17) | 8192u);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("out-of-range program reports done + error");
    else
        report_test_fail("oob program", "expected done + error, got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val & (1u << 5))
        report_test_pass("STATUS: program_addr_error latched");
    else
        report_test_fail("program_addr_error", "expected bit 5 set, got 0x" + std::to_string(val));

    // Clearing only the program flag must leave the others alone.
    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, (1u << 9));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (!(val & (1u << 5)) && (val & (1u << 4)))
        report_test_pass("program_addr_error_clear is selective (req_error survives)");
    else
        report_test_fail("selective clear", "expected bit 5 clear and bit 4 set, got 0x" +
            std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("all clears together return STATUS to sense_done only");
    else
        report_test_fail("clear all", "expected 0x1 got 0x" + std::to_string(val));

    // The refused program must not have burned anything at the wrapped address.
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | 0u);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if ((val & 0x1u) == 0x1u)
        report_test_pass("word 0 still holds only what the image put there");
    else
        report_test_fail("oob program side effect", "word 0 is 0x" + std::to_string(val));
}

void testbench::test_lock_enforcement()
{
    report_test_start("Test 22: LOCKS — write and read enforcement");

    uint32_t val = 0;

    // Start from an erased image so the lock state is known: LOCKS is sticky with no
    // software clear, and earlier tests have set bits in it. Sensing an all-zero array
    // reloads the lock flops along with everything else, which is the only way back to
    // an unlocked model short of a reset.
    const std::string erased = "/tmp/efuse_test_erased.preload";
    {
        std::ofstream out(erased);
        for (unsigned int bit = 0; bit < 8192; bit++)
            out << "0\n";
    }
    if (m_dut->preload_fuses_from_file(erased))
        report_test_pass("erased image sensed: locks back to zero");
    else
        report_test_fail("lock test setup", "erased image was rejected");

    // BL1_VERSION is the field under test: eight words at 0x088 (array word 34, bit
    // 1088), WOSET from software, and governed by LOCKS_LO bits 18 (write) and 19
    // (read). BL2_VERSION next door has its own pair and stays unlocked throughout,
    // so it witnesses that the policy is per field rather than global.
    const uint32_t bl1_bit  = BL1_VERSION_BIT;
    const uint32_t enable_read = (1u << 28);
    const uint32_t enable_prog = (1u << 27) | (1u << 16);   // program_enable, data=1

    m_test->register_write_32(sep_efuse::BL1_VERSION_OFFSET, 0x5);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("unlocked: shadow write accepted");
    else
        report_test_fail("unlocked shadow write", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (bl1_bit + 2u));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("unlocked: OTP program accepted");
    else
        report_test_fail("unlocked program", "expected done + no error, got 0x" + std::to_string(val));

    // Write lock only. Reads must keep working, which is what makes the two bits
    // independent rather than one coarse "locked" state.
    m_test->register_write_32(sep_efuse::LOCKS_LO_OFFSET, (1u << 18));
    wait(1, SC_NS);

    m_test->register_write_32(sep_efuse::BL1_VERSION_OFFSET, 0x8);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("write-locked: shadow write dropped, old value intact");
    else
        report_test_fail("write lock", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (bl1_bit + 3u));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("write-locked: OTP program refused with done + error");
    else
        report_test_fail("write lock OTP", "expected done + error, got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val & (1u << 4))
        report_test_pass("write-locked: req_error latched");
    else
        report_test_fail("write lock req_error", "expected bit 4 set, got 0x" + std::to_string(val));

    // Nothing was burned: bit 2 from the unlocked program is there, bit 3 is not.
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | bl1_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0x4)
        report_test_pass("write-locked: refused program burned nothing");
    else
        report_test_fail("write lock no-burn", "expected 0x4 got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("write-locked: shadow still readable");
    else
        report_test_fail("write lock read path", "expected 0x5 got 0x" + std::to_string(val));

    // Read lock as well.
    m_test->register_write_32(sep_efuse::LOCKS_LO_OFFSET, (1u << 19));
    wait(1, SC_NS);

    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    if (val == 0xBADCAB1Eu)
        report_test_pass("read-locked: shadow read returns 0xBADCAB1E");
    else
        report_test_fail("read lock shadow", "expected 0xBADCAB1E got 0x" + std::to_string(val));

    // One lock pair covers all eight words of the field, not just the first.
    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET + 7 * 4, val);
    if (val == 0xBADCAB1Eu)
        report_test_pass("read-locked: last word of the field denied too");
    else
        report_test_fail("read lock span", "expected 0xBADCAB1E got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | bl1_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("read-locked: OTP read refused with done + error");
    else
        report_test_fail("read lock OTP", "expected done + error, got 0x" + std::to_string(val));

    // The OTP path reports the refusal in status and returns zero, rather than
    // returning the sentinel the shadow path uses.
    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0u)
        report_test_pass("read-locked: OTP read data is zero, not the sentinel");
    else
        report_test_fail("read lock OTP data", "expected 0x0 got 0x" + std::to_string(val));

    // BL2_VERSION shares no lock bits with BL1_VERSION and is still fully accessible.
    m_test->register_write_32(sep_efuse::BL2_VERSION_OFFSET, 0x9);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::BL2_VERSION_OFFSET, val);
    if (val == 0x9)
        report_test_pass("neighbouring field unaffected by BL1_VERSION's locks");
    else
        report_test_fail("lock isolation", "expected 0x9 got 0x" + std::to_string(val));

    // LOCKS_LO governs other fields and is never itself lockable, so it has to stay
    // readable -- otherwise software could not tell what it had locked.
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val == ((1u << 18) | (1u << 19)))
        report_test_pass("LOCKS_LO itself remains readable");
    else
        report_test_fail("LOCKS_LO readable", "expected 0xC0000 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
}

void testbench::test_token_matching()
{
    report_test_start("Test 23: Token matching — SHA-256 compare against the digest fuse");

    uint32_t val = 0;

    // SHA-256 of a 32-byte all-zero token. This is the constant the standalone SEP
    // testbench uses (top_define.sv SEP_SEC_DIS_TEST_TOKEN_HASH), which makes it a
    // useful independent check: if the model's message byte order or digest word order
    // were wrong, the digest of the all-zero token would not be this value.
    //   66687aad f862bd77 6c8fc18b 8e9f8e20 08971485 6ee233b3 902a591d 0d5f2925
    // Word 0 is digest bits [31:0], so the words run opposite to that byte order.
    const uint32_t zero_token_digest[8] = {
        0x0d5f2925u, 0x902a591du, 0x6ee233b3u, 0x08971485u,
        0x8e9f8e20u, 0x6c8fc18bu, 0xf862bd77u, 0x66687aadu,
    };

    // The RMA SIP reference is a fuse field, so it is set the way hardware sets it:
    // through the array. RMA_SIP_TOKEN_DIGEST sits at shadow byte 0x24, i.e. array
    // word 9, i.e. bit 288.
    const std::string image = "/tmp/efuse_test_token.preload";
    {
        std::ofstream out(image);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            const unsigned int word = bit / 32;
            bool set = false;
            if (word >= RMA_SIP_TOKEN_WORD0
                && word < RMA_SIP_TOKEN_WORD0 + TOKEN_WORDS)
                set = (zero_token_digest[word - RMA_SIP_TOKEN_WORD0] >> (bit % 32)) & 1u;
            out << (set ? '1' : '0') << "\n";
        }
    }
    if (m_dut->preload_fuses_from_file(image))
        report_test_pass("digest image sensed into RMA_SIP_TOKEN_DIGEST");
    else
        report_test_fail("token test setup", "digest image was rejected");

    // Token inputs reset to zero, so the staged token is already the all-zero one.
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x15u)
        report_test_pass("zero token matches the digest fuse (status 0x15)");
    else
        report_test_fail("token match", "expected 0x15 got 0x" + std::to_string(val));

    // Flip one bit of the token: the digest changes completely, so this is also a
    // check that the comparison looks at the whole 256 bits.
    m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("altered token mismatches (status 0x2A)");
    else
        report_test_fail("token mismatch", "expected 0x2A got 0x" + std::to_string(val));

    // The word order matters: the same eight words in the opposite order hash to a
    // different digest, so a model that streamed them the other way would fail here
    // while still passing the all-zero case (which is order-independent).
    const uint32_t ordered_token[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    for (int i = 0; i < 8; i++)
        m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET + i * 4, ordered_token[i]);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("ordered token mismatches a digest it was not built from");
    else
        report_test_fail("token order", "expected 0x2A got 0x" + std::to_string(val));

    // Each token type has its own go bit, reference and result. Triggering SIP must
    // leave the other two results alone.
    m_test->register_read_32(sep_efuse::RMA_CHIPLET_TOKEN_MATCH_OFFSET, val);
    if (val == 0x0u)
        report_test_pass("untriggered chiplet result still reads 0x00");
    else
        report_test_fail("token independence", "expected 0x0 got 0x" + std::to_string(val));

    // The chiplet reference is all zeros in this image, so no token can match it --
    // exactly the situation an unprogrammed digest fuse leaves in silicon.
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x100);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::RMA_CHIPLET_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("zero reference cannot be matched (chiplet mismatch)");
    else
        report_test_fail("zero reference", "expected 0x2A got 0x" + std::to_string(val));

    // Same for secure disable, whose reference is the parameter rather than a fuse and
    // defaults to zero as the RTL parameter does.
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x10000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SEC_DISABLE_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("sec-disable mismatches with the default zero reference");
    else
        report_test_fail("sec disable default", "expected 0x2A got 0x" + std::to_string(val));

    // Results are hardware-written; software cannot forge a match.
    m_test->register_write_32(sep_efuse::SEC_DISABLE_TOKEN_MATCH_OFFSET, 0x15);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SEC_DISABLE_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("match result is read-only to software");
    else
        report_test_fail("match RO", "software wrote the result, now 0x" + std::to_string(val));

    // TOKEN_EOP go bits are singlepulse and the register is write-only.
    m_test->register_read_32(sep_efuse::TOKEN_EOP_OFFSET, val);
    if (val == 0x0u)
        report_test_pass("TOKEN_EOP reads back zero (write-only singlepulse)");
    else
        report_test_fail("TOKEN_EOP readback", "expected 0x0 got 0x" + std::to_string(val));

    // What the match authorises: LC_STATE bit 1 (array bit 65) can only be burned
    // while the SIP token matches. Restore the matching token first.
    const uint32_t enable_prog = (1u << 27) | (1u << 16);
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (LC_STATE_BIT + 1u));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("LC_STATE RMA bit refused while the token mismatches");
    else
        report_test_fail("LC gating", "expected error status, got 0x" + std::to_string(val));

    for (int i = 0; i < 8; i++)
        m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET + i * 4, 0);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (LC_STATE_BIT + 1u));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("LC_STATE RMA bit accepted once the token matches");
    else
        report_test_fail("LC gating match", "expected done + no error, got 0x" + std::to_string(val));

    // The chiplet bit has its own gate, and its token has not matched.
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (LC_STATE_BIT + 2u));
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("LC_STATE chiplet bit still gated by its own token");
    else
        report_test_fail("LC gating chiplet", "expected error status, got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
}

/*
 * The half of the transition machine that only opens with a matching token: the walk
 * TEST_DEV -> RMA_SIP -> RMA_CHIPLET, and the stop at the far end. Both RMA digest
 * fuses are planted with the digest of the all-zero token, which is what the token
 * input registers already hold, so pulsing each go bit is enough to open its gate --
 * see test_token_matching for where that digest comes from.
 */
void testbench::test_lc_state_transitions()
{
    report_test_start("Test 24: LC_STATE transition machine — token-gated path");

    const uint32_t zero_token_digest[8] = {
        0x0d5f2925u, 0x902a591du, 0x6ee233b3u, 0x08971485u,
        0x8e9f8e20u, 0x6c8fc18bu, 0xf862bd77u, 0x66687aadu,
    };

    // RMA_SIP_TOKEN_DIGEST is shadow byte 0x24 (words 9-16), RMA_CHIPLET_TOKEN_DIGEST
    // 0x44 (words 17-24), LC_STATE 0x08 (word 2), starting at TEST_DEV.
    const std::string image = "/tmp/efuse_test_lc_walk.preload";
    {
        std::ofstream out(image);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            const unsigned int word = bit / 32;
            uint32_t word_val = 0;
            if (word == LC_STATE_WORD)
                word_val = efuse_model::lc_state_encode(efuse_model::LC_RAW_TEST_DEV);
            else if (word >= RMA_SIP_TOKEN_WORD0
                     && word < RMA_SIP_TOKEN_WORD0 + TOKEN_WORDS)
                word_val = zero_token_digest[word - RMA_SIP_TOKEN_WORD0];
            else if (word >= RMA_CHIPLET_TOKEN_WORD0
                     && word < RMA_CHIPLET_TOKEN_WORD0 + TOKEN_WORDS)
                word_val = zero_token_digest[word - RMA_CHIPLET_TOKEN_WORD0];
            out << ((word_val >> (bit % 32)) & 1u ? '1' : '0') << "\n";
        }
    }
    if (!m_dut->preload_fuses_from_file(image)) {
        report_test_fail("LC walk setup", "digest image was rejected");
        return;
    }

    uint32_t val = 0;
    auto write_lc = [&](uint32_t v) {
        m_test->register_write_32(sep_efuse::LC_STATE_OFFSET, v);
        wait(1, SC_NS);
    };
    auto check = [&](uint32_t want_raw, const std::string &what) {
        const uint32_t want = efuse_model::lc_state_encode(want_raw);
        m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
        if (val == want)
            report_test_pass(what);
        else
            report_test_fail(what, "expected 0x" + hex32(want) + " got 0x" + hex32(val));
    };

    check(efuse_model::LC_RAW_TEST_DEV, "LC_STATE: image sensed as TEST_DEV");

    // Bit 1 is gated on the SiP token. Stage a token that is not the all-zero one to
    // put the gate in a known-shut state first, since earlier tests leave it open.
    m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 1u << 0);
    wait(1, SC_NS);
    write_lc(efuse_model::LC_RAW_RMA_SIP_0);
    check(efuse_model::LC_RAW_TEST_DEV, "LC_STATE: RMA_SIP refused while the SiP token mismatches");

    m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET, 0x0);
    wait(1, SC_NS);
    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 1u << 0);
    wait(1, SC_NS);
    write_lc(efuse_model::LC_RAW_RMA_SIP_0);
    check(efuse_model::LC_RAW_RMA_SIP_0, "LC_STATE: SiP token match opens TEST_DEV -> RMA_SIP");

    // Bit 2 has its own gate, and needs RMA_SIP established first — which it now is.
    write_lc(0x4u);
    check(efuse_model::LC_RAW_RMA_SIP_0, "LC_STATE: RMA_CHIPLET refused without its own token");

    m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, 1u << 8);
    wait(1, SC_NS);
    write_lc(0x4u);
    check(efuse_model::LC_RAW_RMA_CHIP_0, "LC_STATE: chiplet token match opens RMA_CHIPLET");

    // RMA_CHIPLET is terminal: even a legal destination is refused.
    write_lc(0x1u);
    check(efuse_model::LC_RAW_RMA_CHIP_0, "LC_STATE: RMA_CHIPLET is terminal");

    if (m_dut->get_lc_state() == efuse_model::LC_RAW_RMA_CHIP_0)
        report_test_pass("LC_STATE: get_lc_state() follows the transitions");
    else
        report_test_fail("LC_STATE get_lc_state()", "expected raw 0x6 got 0x" +
            hex32(m_dut->get_lc_state()));
}

/*
 * locked_field_access_interrupt: raised by the shadow access controller, and only by
 * it. Runs straight after test_lock_enforcement so it inherits the locks that test
 * sets -- BL1_VERSION is both read- and write-locked by then, and LOCKS is sticky.
 */
void testbench::test_locked_field_interrupt()
{
    report_test_start("Test 25: locked_field_access_interrupt — shadow refusals only");

    uint32_t val = 0;
    take_locked_field_pulses();

    // An access that is not refused must not raise it, or the interrupt says nothing.
    m_test->register_read_32(sep_efuse::BL2_VERSION_OFFSET, val);
    wait(2, SC_NS);
    if (take_locked_field_pulses() == 0)
        report_test_pass("unlocked read is silent");
    else
        report_test_fail("unlocked read", "interrupt raised on a permitted access");

    m_test->register_read_32(sep_efuse::BL1_VERSION_OFFSET, val);
    wait(2, SC_NS);
    if (take_locked_field_pulses() == 1)
        report_test_pass("read-locked shadow read raises the interrupt");
    else
        report_test_fail("locked read interrupt", "no pulse on a denied read");

    m_test->register_write_32(sep_efuse::BL1_VERSION_OFFSET, 0x55);
    wait(2, SC_NS);
    if (take_locked_field_pulses() == 1)
        report_test_pass("write-locked shadow write raises the interrupt");
    else
        report_test_fail("locked write interrupt", "no pulse on a dropped write");

    // The OTP path is guarded by efuse_guard, which reports through req_error instead.
    // Sharing the interrupt would make it impossible to tell the two refusals apart.
    const uint32_t bl1_bit = (sep_efuse::BL1_VERSION_OFFSET / 4) * 32;
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | bl1_bit);
    wait(2, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    const bool otp_refused = (val & (1u << 26)) != 0;
    if (otp_refused && take_locked_field_pulses() == 0)
        report_test_pass("refused OTP read reports req_error without the interrupt");
    else
        report_test_fail("OTP refusal interrupt",
                         otp_refused ? "interrupt raised on the guard path"
                                     : "OTP read was not refused");

    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
}

/*
 * TRANSIENT_RMA_EN: the lifecycle advances on a token match alone, with no LC_STATE
 * write. Both images plant the digest of the all-zero token in both RMA digest fuses,
 * as test_token_matching does, so pulsing a go bit is what opens each gate.
 */
void testbench::test_transient_rma()
{
    report_test_start("Test 26: TRANSIENT_RMA_EN — token-driven auto-transition");

    const uint32_t zero_token_digest[8] = {
        0x0d5f2925u, 0x902a591du, 0x6ee233b3u, 0x08971485u,
        0x8e9f8e20u, 0x6c8fc18bu, 0xf862bd77u, 0x66687aadu,
    };

    // TRANSIENT_RMA_EN is shadow byte 0x010, i.e. word 4.
    auto write_image = [&](const std::string &path, bool transient) {
        std::ofstream out(path);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            const unsigned int word = bit / 32;
            uint32_t word_val = 0;
            if (word == LC_STATE_WORD)
                word_val = efuse_model::lc_state_encode(efuse_model::LC_RAW_TEST_DEV);
            else if (word == TRANSIENT_RMA_EN_WORD && transient)
                word_val = 0x1u;
            else if (word >= RMA_SIP_TOKEN_WORD0
                     && word < RMA_SIP_TOKEN_WORD0 + TOKEN_WORDS)
                word_val = zero_token_digest[word - RMA_SIP_TOKEN_WORD0];
            else if (word >= RMA_CHIPLET_TOKEN_WORD0
                     && word < RMA_CHIPLET_TOKEN_WORD0 + TOKEN_WORDS)
                word_val = zero_token_digest[word - RMA_CHIPLET_TOKEN_WORD0];
            out << ((word_val >> (bit % 32)) & 1u ? '1' : '0') << "\n";
        }
    };

    uint32_t val = 0;
    auto check = [&](uint32_t want_raw, const std::string &what) {
        const uint32_t want = efuse_model::lc_state_encode(want_raw);
        m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
        if (val == want)
            report_test_pass(what);
        else
            report_test_fail(what, "expected 0x" + hex32(want) + " got 0x" + hex32(val));
    };
    auto match_token = [&](uint32_t go_bit) {
        m_test->register_write_32(sep_efuse::TOKEN_EOP_OFFSET, go_bit);
        wait(1, SC_NS);
    };

    // Match results are sticky and earlier tests leave both matched, so each step
    // stages the token it wants and re-pulses rather than assuming a starting point.
    auto stage_sip     = [&](uint32_t w0) {
        m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET, w0);
        wait(1, SC_NS);
    };
    auto stage_chiplet = [&](uint32_t w0) {
        m_test->register_write_32(sep_efuse::RMA_CHIPLET_TOKEN_I_OFFSET, w0);
        wait(1, SC_NS);
    };
    for (int i = 1; i < 8; i++) {
        m_test->register_write_32(sep_efuse::RMA_SIP_TOKEN_I_OFFSET + i * 4, 0);
        m_test->register_write_32(sep_efuse::RMA_CHIPLET_TOKEN_I_OFFSET + i * 4, 0);
    }
    wait(1, SC_NS);

    // Without the fuse, a match authorises a transition but does not perform one.
    const std::string off_image = "/tmp/efuse_test_transient_off.preload";
    const std::string on_image  = "/tmp/efuse_test_transient_on.preload";
    write_image(off_image, false);
    write_image(on_image,  true);

    if (!m_dut->preload_fuses_from_file(off_image)) {
        report_test_fail("transient RMA setup", "image was rejected");
        return;
    }
    stage_sip(0);
    match_token(1u << 0);
    check(efuse_model::LC_RAW_TEST_DEV, "no transient fuse: SiP match leaves LC_STATE alone");

    if (!m_dut->preload_fuses_from_file(on_image)) {
        report_test_fail("transient RMA setup", "image was rejected");
        return;
    }
    check(efuse_model::LC_RAW_TEST_DEV, "transient image sensed as TEST_DEV");

    // Shut both gates first — every earlier test leaves them matched, and a latched
    // match is enough on its own here, with no pulse of its own required.
    // SiP first: while the chiplet match is still latched it is blocked anyway, so
    // this only clears its result. Clearing the chiplet one first would let the
    // latched SiP match advance the state before the test has begun.
    stage_sip(0x1);
    match_token(1u << 0);
    stage_chiplet(0x1);
    match_token(1u << 8);
    check(efuse_model::LC_RAW_TEST_DEV, "neither token matching leaves LC_STATE alone");

    stage_sip(0x0);
    match_token(1u << 0);
    check(efuse_model::LC_RAW_RMA_SIP_0, "SiP match advances TEST_DEV -> RMA_SIP with no write");

    stage_chiplet(0x0);
    match_token(1u << 8);
    check(efuse_model::LC_RAW_RMA_CHIP_0, "chiplet match advances RMA_SIP -> RMA_CHIPLET");

    // RMA_CHIPLET is terminal for the transient path too.
    match_token(1u << 0);
    check(efuse_model::LC_RAW_RMA_CHIP_0, "RMA_CHIPLET is terminal under transient RMA");

    // A matched chiplet token blocks the SiP advance instead of falling through to it
    // (efuse_shadow_regs.sv:342-343, whose empty branch says so in as many words). The
    // chiplet match is still latched from above, so a fresh image at TEST_DEV is stuck:
    // clearing the chiplet match is the only way out, which is worth pinning down since
    // an if/else chain written the obvious way would quietly do the SiP transition.
    if (!m_dut->preload_fuses_from_file(on_image)) {
        report_test_fail("transient RMA setup", "image was rejected");
        return;
    }
    stage_sip(0);
    match_token(1u << 0);
    check(efuse_model::LC_RAW_TEST_DEV, "a latched chiplet match blocks the SiP advance");
}

void testbench::run_coverage_tests()
{
    test_woset_locks_hi();
    test_woset_lc_state();
    test_woset_sip_dis_hi();
    test_woset_sys_dis_hi();
    test_woset_chiplet_pubk_revoke();
    test_woset_bl_version();
    test_efuse_write_ctrl_go();
    test_efuse_read_ctrl_go();
    test_otp_accessors();
    test_shim_ctrl_window();
    test_fuse_array_program_read();
    test_fuse_preload_file();
    test_fuse_program_out_of_range();
    // LOCKS is sticky, so anything this sets stays locked for whatever follows —
    // which the interrupt test relies on, and the preloads below then clear.
    test_lock_enforcement();
    test_locked_field_interrupt();
    test_token_matching();
    // Depends on the matching SiP token test_token_matching leaves behind.
    test_lc_state_transitions();
    test_transient_rma();
}
