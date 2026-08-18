#include "testbench.h"
#include "lifecycle_ctrl_basetest.h"
#include "csml_parameter.h"
#include <cstdio>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// =============================================================================
// Constructor
// =============================================================================

testbench::testbench(sc_module_name name)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    m_dut  = std::make_unique<lifecycle_ctrl_model>("lifecycle_ctrl_dut");
    m_test = std::make_unique<lifecycle_ctrl_test>("lifecycle_ctrl_test");

    logger.setMaxVerbosity(m_dut->verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    m_test->initiator_socket.bind(m_dut->target_socket);

    SC_THREAD(run_tests);
}

// =============================================================================
// Test Reporting
// =============================================================================

void testbench::report_test_start(const std::string &name)
{
    CSML_INFO(1, logger) << "\n========================================"
                         << "\n" << name
                         << "\n========================================" << std::endl;
}

void testbench::report_test_pass(const std::string &name)
{
    m_tests_run++;
    m_tests_passed++;
    CSML_INFO(1, logger) << "[PASS] " << name << std::endl;
}

void testbench::report_test_fail(const std::string &name, const std::string &reason)
{
    m_tests_run++;
    m_tests_failed++;
    m_failed_tests.push_back(name);
    CSML_ERROR(0, logger) << "[FAIL] " << name << " — " << reason << std::endl;
}

void testbench::report_test_summary()
{
    CSML_INFO(1, logger) << "\n========================================"
                         << "\nlc_ctrl Test Summary"
                         << "\n========================================"
                         << "\nTotal : " << m_tests_run
                         << "\nPassed: " << m_tests_passed
                         << "\nFailed: " << m_tests_failed << std::endl;

    for (const auto &t : m_failed_tests)
        CSML_ERROR(0, logger) << "  FAILED: " << t << std::endl;
}

// =============================================================================
// Test 1: Register reset values
// =============================================================================

void testbench::test_reset_values()
{
    report_test_start("Test 1: Register Reset Values");

    uint32_t val = 0xDEADBEEF;

    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if (val == lifecycle_ctrl_basetest::DEMOTE_1_RESET)
        report_test_pass("DEMOTE_1 reset=0x0");
    else
        report_test_fail("DEMOTE_1 reset",
            "expected 0x0 got 0x" + std::to_string(val));

    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if (val == lifecycle_ctrl_basetest::DEMOTE_2_RESET)
        report_test_pass("DEMOTE_2 reset=0x0");
    else
        report_test_fail("DEMOTE_2 reset",
            "expected 0x0 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 2: FEAT_CTRL_LO/HI are read-only
// =============================================================================

void testbench::test_feat_ctrl_ro()
{
    report_test_start("Test 2: FEAT_CTRL_LO/HI are RO");

    uint32_t before = 0, after = 0;

    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, before);
    m_test->register_write_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, 0xDEADBEEF);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, after);

    if (after == before)
        report_test_pass("FEAT_CTRL_LO is RO (write ignored)");
    else
        report_test_fail("FEAT_CTRL_LO RO protection", "value changed after write");

    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, before);
    m_test->register_write_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, 0xCAFEBABE);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, after);

    if (after == before)
        report_test_pass("FEAT_CTRL_HI is RO (write ignored)");
    else
        report_test_fail("FEAT_CTRL_HI RO protection", "value changed after write");
}

// =============================================================================
// Test 3: TEST_DEV state — FEAT_CTRL = ~(sip_dis | sys_dis)
// =============================================================================

void testbench::test_test_dev_state()
{
    report_test_start("Test 3: TEST_DEV state — initial FEAT_CTRL");

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    uint64_t sip_dis  = ((uint64_t)m_dut->sip_dis_hi.get_param_value() << 32) | m_dut->sip_dis_lo.get_param_value();
    uint64_t sys_dis  = ((uint64_t)m_dut->sys_dis_hi.get_param_value() << 32) | m_dut->sys_dis_lo.get_param_value();
    uint64_t expected = ~(sip_dis | sys_dis);
    if (!m_dut->secure_tm.get_param_value())
        expected &= ~0x0000FFFF00000000ULL;

    if (val_lo == (uint32_t)(expected & 0xFFFFFFFF))
        report_test_pass("FEAT_CTRL_LO: TEST_DEV initial value correct");
    else
        report_test_fail("FEAT_CTRL_LO TEST_DEV",
            "expected 0x" + std::to_string((uint32_t)(expected & 0xFFFFFFFF)) +
            " got 0x" + std::to_string(val_lo));

    if (val_hi == (uint32_t)(expected >> 32))
        report_test_pass("FEAT_CTRL_HI: TEST_DEV initial value correct");
    else
        report_test_fail("FEAT_CTRL_HI TEST_DEV",
            "expected 0x" + std::to_string((uint32_t)(expected >> 32)) +
            " got 0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 4: PROD state — func bits only, no demote
// =============================================================================

void testbench::test_prod_state_no_demote()
{
    report_test_start("Test 4: PROD state — FEAT_CTRL=func-only (no demote)");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if (lc != lifecycle_ctrl_model::LC_STATE_PROD) {
        report_test_pass("Test 4 skipped — lc_state not PROD (set lc_state=1 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    // PROD base with all dis=0: ~(0|0) & FUNC_BITS_MASK = 0xFFFF000000000000
    if (val_lo == 0x00000000 && val_hi == 0xFFFF0000)
        report_test_pass("PROD no-demote: FEAT_CTRL=func-only (0xFFFF000000000000)");
    else
        report_test_fail("PROD no-demote FEAT_CTRL",
            "expected lo=0x00000000 hi=0xFFFF0000 got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 5: DEMOTE_1 W1S — write 1 sets the bit, write 0 does not clear it
// =============================================================================

void testbench::test_demote_1_w1s()
{
    report_test_start("Test 5: DEMOTE_1 W1S semantics");

    uint32_t val = 0;

    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_1: bit 0 set after write-1");
    else
        report_test_fail("DEMOTE_1 W1S set",
            "expected bit 0 set, got 0x" + std::to_string(val));

    // Write-0 must not clear the bit (W1S)
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_1: bit 0 stays set after write-0 (W1S)");
    else
        report_test_fail("DEMOTE_1 W1S hold", "bit was cleared — expected bit 0 still set");

    // In TEST_DEV demote only re-enables debug bits; with all dis=0 the result is
    // all-ones, less the test section if secure_tm is deasserted.
    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);
    const uint32_t exp_hi = m_dut->secure_tm.get_param_value() ? 0xFFFFFFFFu : 0xFFFF0000u;
    if (val_lo == 0xFFFFFFFF && val_hi == exp_hi)
        report_test_pass("FEAT_CTRL: TEST_DEV+demote1 correct");
    else
        report_test_fail("FEAT_CTRL TEST_DEV+demote1",
            "expected 0xFFFFFFFF/0x" + std::to_string(exp_hi) + " got lo=0x" +
            std::to_string(val_lo) + " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 6: DEMOTE_1 lock — once bit 1 is set, further writes are ignored
// =============================================================================

void testbench::test_demote_1_lock()
{
    report_test_start("Test 6: DEMOTE_1 lock semantics");

    uint32_t val = 0;

    // Set lock bit
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000002);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x2) == 0x2)
        report_test_pass("DEMOTE_1: lock bit set");
    else
        report_test_fail("DEMOTE_1 lock set",
            "expected lock bit set, got 0x" + std::to_string(val));

    uint32_t before = val;

    // Write after lock — must be silently ignored
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if (val == before)
        report_test_pass("DEMOTE_1: write ignored after lock");
    else
        report_test_fail("DEMOTE_1 post-lock write",
            "DEMOTE_1 changed after lock, got 0x" + std::to_string(val));
}

// =============================================================================
// Test 7: DEMOTE_2 W1S — independent domain, same W1S semantics
// =============================================================================

void testbench::test_demote_2_w1s()
{
    report_test_start("Test 7: DEMOTE_2 W1S semantics");

    uint32_t val = 0;

    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_2: bit 0 set after write-1");
    else
        report_test_fail("DEMOTE_2 W1S set",
            "expected bit 0 set, got 0x" + std::to_string(val));

    // Write-0 must not clear the bit (W1S)
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_2: bit 0 stays set after write-0 (W1S)");
    else
        report_test_fail("DEMOTE_2 W1S hold", "bit was cleared — expected bit 0 still set");
}

// =============================================================================
// Test 11: DEMOTE_2 lock — once bit 1 is set, further writes are ignored
// =============================================================================

void testbench::test_demote_2_lock()
{
    report_test_start("Test 11: DEMOTE_2 lock semantics");

    uint32_t val = 0;

    // Set lock bit
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000002);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if ((val & 0x2) == 0x2)
        report_test_pass("DEMOTE_2: lock bit set");
    else
        report_test_fail("DEMOTE_2 lock set",
            "expected lock bit set, got 0x" + std::to_string(val));

    uint32_t before = val;

    // Write after lock — must be silently ignored and a CSML WARNING logged
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if (val == before)
        report_test_pass("DEMOTE_2: write ignored after lock");
    else
        report_test_fail("DEMOTE_2 post-lock write",
            "DEMOTE_2 changed after lock, got 0x" + std::to_string(val));
}

// =============================================================================
// Test 8: INVALID state — encodings 0x4 and 0x5 must produce FEAT_CTRL=0
// =============================================================================

void testbench::test_invalid_state()
{
    report_test_start("Test 8: INVALID state (0x4, 0x5) — FEAT_CTRL=0");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if (lc != 0x4u && lc != 0x5u) {
        report_test_pass("Test 8 skipped — lc_state not INVALID (set lc_state=4 or 5 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    std::string label = "lc_state=0x" + std::to_string(lc);
    if (val_lo == 0x0 && val_hi == 0x0)
        report_test_pass("INVALID " + label + ": FEAT_CTRL=0x0");
    else
        report_test_fail("INVALID FEAT_CTRL " + label,
            "expected 0x0/0x0 got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 9: RMA_CHIPLET state — encodings 4'b011x (0x6, 0x7) give all-ones
// =============================================================================

void testbench::test_rma_chiplet_state()
{
    report_test_start("Test 9: RMA_CHIPLET state — FEAT_CTRL=0xFFFFFFFF_FFFFFFFF");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if ((lc & lifecycle_ctrl_model::LC_STATE_RANGE_MASK) != lifecycle_ctrl_model::LC_STATE_RMA_CHIPLET_BASE) {
        report_test_pass("Test 9 skipped — lc_state not RMA_CHIPLET (set lc_state=6 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    if (val_lo == 0xFFFFFFFF && val_hi == 0xFFFFFFFF)
        report_test_pass("RMA_CHIPLET: FEAT_CTRL=0xFFFFFFFF_FFFFFFFF");
    else
        report_test_fail("RMA_CHIPLET FEAT_CTRL",
            "expected 0xFFFFFFFF/0xFFFFFFFF got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 10: secure_tm=false gates test bits [47:32]
// =============================================================================

void testbench::test_secure_tm()
{
    report_test_start("Test 10: secure_tm gates test bits [47:32]");

    bool stm = m_dut->secure_tm.get_param_value();

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    // TEST_DEV + all-dis=0: feat_ctrl = ~0; secure_tm=false clears bits [47:32]
    uint32_t exp_lo = 0xFFFFFFFF;
    uint32_t exp_hi = stm ? 0xFFFFFFFF : 0xFFFF0000;

    if (val_lo == exp_lo && val_hi == exp_hi)
        report_test_pass(std::string("secure_tm=") + (stm ? "true" : "false") +
                         ": FEAT_CTRL correct");
    else
        report_test_fail("secure_tm FEAT_CTRL",
            "expected lo=0x" + std::to_string(exp_lo) + " hi=0x" + std::to_string(exp_hi) +
            " got lo=0x" + std::to_string(val_lo) + " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 12: security_disable override
// =============================================================================

void testbench::test_security_disable()
{
    report_test_start("Test 12: security_disable override");

    bool sec_dis = m_dut->security_disable.get_param_value();
    if (!sec_dis) {
        report_test_pass("Test 12 skipped — security_disable not true (set security_disable=true in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    if (val_lo == 0xFFFFFFFF && val_hi == 0xFFFFFFFF)
        report_test_pass("security_disable: FEAT_CTRL=0xFFFFFFFF_FFFFFFFF");
    else
        report_test_fail("security_disable FEAT_CTRL",
            "expected all-ones, got lo=0x" + std::to_string(val_lo) + " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Helpers for the input-driven tests
//
// The tests above read FEAT_CTRL as the config happens to leave it, so each one covers
// a single state arm and skips otherwise. set_inputs() is how the eFuse model drives
// this block on a platform, so using it here covers every arm in one run.
// =============================================================================

void testbench::drive_inputs(uint32_t lc_code, uint64_t sip_dis, uint64_t sys_dis,
                             bool security_disable, bool secure_tm)
{
    lifecycle_ctrl_model::lc_inputs in;
    in.lc_state_code    = lc_code;
    in.sip_dis          = sip_dis;
    in.sys_dis          = sys_dis;
    in.security_disable = security_disable;
    in.secure_tm        = secure_tm;
    m_dut->set_inputs(in);
}

void testbench::clear_demote()
{
    m_dut->reset_all_registers();
}

uint64_t testbench::read_feat_ctrl()
{
    uint32_t lo = 0, hi = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, lo);
    m_test->register_read_32(lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, hi);
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

static std::string hex64(uint64_t v)
{
    char buf[19];
    std::snprintf(buf, sizeof(buf), "0x%016llx", static_cast<unsigned long long>(v));
    return buf;
}

static std::string hex32(uint32_t v)
{
    char buf[11];
    std::snprintf(buf, sizeof(buf), "0x%x", v);
    return buf;
}

void testbench::check_feat_ctrl(const std::string &name, uint64_t expected)
{
    const uint64_t got = read_feat_ctrl();
    if (got == expected)
        report_test_pass(name + " — FEAT_CTRL=" + hex64(got));
    else
        report_test_fail(name, "expected " + hex64(expected) + " got " + hex64(got));
}

// =============================================================================
// Test 13: every LC state arm, driven through the input bundle
// =============================================================================

void testbench::test_all_state_arms()
{
    report_test_start("Test 13: FEAT_CTRL for every LC state arm");

    clear_demote();

    // Distinct masks so the arms are told apart rather than all collapsing to all-ones:
    // one func bit and one debug bit in each, at different positions.
    constexpr uint64_t SIP = 0x0001'0000'0000'0001ULL;
    constexpr uint64_t SYS = 0x0002'0000'0000'0002ULL;
    constexpr uint64_t FUNC = 0xFFFF'0000'0000'0000ULL;
    constexpr uint64_t ALL  = 0xFFFF'FFFF'FFFF'FFFFULL;

    struct arm { uint32_t raw; const char *name; uint64_t expected; };
    const arm arms[] = {
        // TEST_DEV: everything not disabled by either mask.
        { 0x0, "TEST_DEV",       ~(SIP | SYS) },
        // PROD and PROD_END: functional section only.
        { 0x1, "PROD",           ~(SIP | SYS) & FUNC },
        { 0x8, "PROD_END",       ~(SIP | SYS) & FUNC },
        // RMA_SIP covers 4'b001x: SIP's mask applies, SYS's does not.
        { 0x2, "RMA_SIP (0x2)",  ~SIP },
        { 0x3, "RMA_SIP (0x3)",  ~SIP },
        // RMA_CHIPLET covers 4'b011x: everything on, both masks ignored.
        { 0x6, "RMA_CHIPLET (0x6)", ALL },
        { 0x7, "RMA_CHIPLET (0x7)", ALL },
        // Everything else is INVALID and fails closed.
        { 0x4, "INVALID (0x4)",  0 },
        { 0x5, "INVALID (0x5)",  0 },
        { 0x9, "INVALID (0x9)",  0 },
        { 0xF, "INVALID (0xF)",  0 },
    };

    for (const auto &a : arms) {
        drive_inputs(lifecycle_ctrl_model::lc_state_encode(a.raw), SIP, SYS);
        check_feat_ctrl(a.name, a.expected);
    }

    // The test section is gated last, after the state chain and after the
    // security_disable override, so it applies to any arm.
    constexpr uint64_t TEST_SECTION = 0x0000'FFFF'0000'0000ULL;
    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x0), SIP, SYS,
                 /*security_disable=*/false, /*secure_tm=*/false);
    check_feat_ctrl("TEST_DEV with secure_tm deasserted",
                    ~(SIP | SYS) & ~TEST_SECTION);

    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x0), SIP, SYS,
                 /*security_disable=*/true, /*secure_tm=*/false);
    check_feat_ctrl("security_disable with secure_tm deasserted",
                    ~TEST_SECTION);
}

// =============================================================================
// Test 14: an illegal differential code raises lc_sigint_err and zeroes the vector
// =============================================================================

void testbench::test_lc_sigint_fail_safe()
{
    report_test_start("Test 14: lc_sigint_err fail-safe on an illegal LC code");

    clear_demote();

    // 0x00 is what an erased part reads and 0xFF a fully burned one; the rest are
    // single-rail corruptions. Note 0x0F is *not* in this list: it is the legal encoding
    // of raw 0xF, which is an invalid state but not a rail mismatch.
    const uint32_t bad_codes[] = { 0x00u, 0xFFu, 0xF1u, 0x0Eu, 0xAAu };
    for (uint32_t code : bad_codes) {
        drive_inputs(code);
        const std::string label = "code " + hex32(code);
        if (m_dut->get_lc_sigint_err())
            report_test_pass("lc_sigint_err raised for " + label);
        else
            report_test_fail("lc_sigint_err for " + label, "expected the error to be raised");
        check_feat_ctrl("sigint fail-safe for " + label, 0);
    }

    // An invalid *state* is not the same thing as a corrupt code: 0x0F is a legal
    // encoding of raw 0xF, so the vector is zero but no integrity error is reported.
    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0xF));
    check_feat_ctrl("legal code for an invalid state", 0);
    if (!m_dut->get_lc_sigint_err())
        report_test_pass("invalid state alone does not raise lc_sigint_err");
    else
        report_test_fail("lc_sigint_err for a legal code", "raised for a well-formed pair");

    // A legal code clears it again — the flag tracks the input, it is not sticky.
    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x1));
    if (!m_dut->get_lc_sigint_err())
        report_test_pass("lc_sigint_err clears once the code is legal again");
    else
        report_test_fail("lc_sigint_err clear", "still raised for a legal code");

    // security_disable is applied after the state chain in the RTL, so it overrides even
    // this fail-safe. Worth pinning: it is the one case where a corrupt LC state does not
    // disable everything.
    drive_inputs(0x00u, 0, 0, /*security_disable=*/true);
    check_feat_ctrl("security_disable overrides the sigint zero",
                    0xFFFF'FFFF'FFFF'FFFFULL);
    if (m_dut->get_lc_sigint_err())
        report_test_pass("lc_sigint_err still reported while security_disable overrides");
    else
        report_test_fail("lc_sigint_err under security_disable", "expected it to stay raised");
}

// =============================================================================
// Test 15: a live SiP_DIS/SYS_DIS change moves FEAT_CTRL
// =============================================================================

void testbench::test_live_feature_disable()
{
    report_test_start("Test 15: FEAT_CTRL follows a live feature-disable change");

    clear_demote();

    // Both eFuse masks are woset and software-writable, so firmware can disable a
    // feature after boot. FEAT_CTRL has to move with it; sampling the masks once at
    // elaboration would leave it frozen at the boot value.
    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x0));
    check_feat_ctrl("no disables", 0xFFFF'FFFF'FFFF'FFFFULL);

    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x0), 0x0000'0000'0000'0001ULL);
    check_feat_ctrl("after SiP_DIS bit 0 burns", ~0x1ULL);

    drive_inputs(lifecycle_ctrl_model::lc_state_encode(0x0),
                 0x0000'0000'0000'0001ULL, 0x0000'0000'0000'0002ULL);
    check_feat_ctrl("after SYS_DIS bit 1 burns too", ~0x3ULL);
}

// =============================================================================
// Test 16: the demote state handed to the key manager is differentially encoded
// =============================================================================

void testbench::test_demote_diff_encoding()
{
    report_test_start("Test 16: get_demote_state() is differentially encoded");

    clear_demote();

    // Each rail pair is {~v, v}, so an undemoted domain reads 2'b10 and the register
    // resets to 0xA rather than 0x0. 2'b00 is not a legal code at all.
    struct step { uint32_t offset; const char *label; uint32_t expected; };
    const step steps[] = {
        { 0,                                        "reset (neither demoted)", 0xA },
        { lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, "demote_1 set",            0x9 },
        { lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, "demote_2 set as well",    0x5 },
    };

    for (const auto &s : steps) {
        if (s.offset != 0) {
            m_test->register_write_32(s.offset, 0x1);
            wait(1, SC_NS);
        }
        const uint32_t got = m_dut->get_demote_state();
        if (got == s.expected)
            report_test_pass(std::string("demote state at ") + s.label + " = " + hex32(got));
        else
            report_test_fail(std::string("demote encoding, ") + s.label,
                "expected " + hex32(s.expected) + " got " + hex32(got));
    }
}

// =============================================================================
// Test 17: the DEMOTE upper words at 0xC and 0x14 are backed and woset
// =============================================================================

void testbench::test_demote_upper_words()
{
    report_test_start("Test 17: DEMOTE_1/2 upper words (rsvd[63:32])");

    clear_demote();

    const unsigned int offsets[] = { lifecycle_ctrl_basetest::DEMOTE_1_HI_OFFSET,
                                     lifecycle_ctrl_basetest::DEMOTE_2_HI_OFFSET };
    const char *names[] = { "DEMOTE_1_HI", "DEMOTE_2_HI" };

    for (int i = 0; i < 2; i++) {
        uint32_t val = 0xDEADBEEF;
        m_test->register_read_32(offsets[i], val);
        if (val == 0)
            report_test_pass(std::string(names[i]) + " reads 0 at reset");
        else
            report_test_fail(std::string(names[i]) + " reset",
                "expected 0 got " + hex32(val));

        // rsvd is onwrite = woset, so two disjoint writes accumulate.
        m_test->register_write_32(offsets[i], 0xF0F0F0F0);
        wait(1, SC_NS);
        m_test->register_write_32(offsets[i], 0x0F0F0F0F);
        wait(1, SC_NS);
        m_test->register_read_32(offsets[i], val);
        if (val == 0xFFFFFFFF)
            report_test_pass(std::string(names[i]) + " accumulates set bits (woset)");
        else
            report_test_fail(std::string(names[i]) + " woset",
                "expected 0xFFFFFFFF got " + hex32(val));
    }
}

// =============================================================================
// Test 18: lock covers the demote bit only
// =============================================================================

void testbench::test_lock_scope()
{
    report_test_start("Test 18: lock write-protects demote but not rsvd");

    clear_demote();

    // swwe = ~lock is applied to the demote field alone in the RTL: lock and rsvd carry
    // no swwe, so they stay writable once locked.
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x2);
    wait(1, SC_NS);

    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x8000'0001u);
    wait(1, SC_NS);

    uint32_t val = 0;
    m_test->register_read_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, val);

    if ((val & 0x1u) == 0)
        report_test_pass("locked DEMOTE_1: demote bit refused");
    else
        report_test_fail("locked DEMOTE_1 demote", "demote bit was set while locked");

    if ((val & 0x8000'0000u) != 0)
        report_test_pass("locked DEMOTE_1: rsvd bit still accepted");
    else
        report_test_fail("locked DEMOTE_1 rsvd", "rsvd bit was blocked by the lock");
}

// =============================================================================
// Test 19: PROD_DBG_1 takes priority over PROD_DBG_2
// =============================================================================

void testbench::test_prod_dbg_priority()
{
    report_test_start("Test 19: demote_1 wins over demote_2 in PROD");

    constexpr uint64_t SIP  = 0x0001'0000'0000'0000ULL;
    constexpr uint64_t SYS  = 0x0002'0000'0000'0000ULL;
    constexpr uint64_t FUNC = 0xFFFF'0000'0000'0000ULL;
    constexpr uint64_t DBG  = 0x0000'0000'FFFF'FFFFULL;
    const uint32_t prod = lifecycle_ctrl_model::lc_state_encode(0x1);

    // demote_2 alone gives PROD_DBG_2, whose functional section comes from ~SYS_DIS.
    clear_demote();
    drive_inputs(prod, SIP, SYS);
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x1);
    wait(1, SC_NS);
    check_feat_ctrl("PROD + demote_2 (PROD_DBG_2, ~SYS_DIS)", (~SYS & FUNC) | DBG);

    // With both set, PROD_DBG_1 wins, so the functional section comes from ~SIP_DIS
    // instead. Masking on the wrong one is the failure this pins down.
    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_1_OFFSET, 0x1);
    wait(1, SC_NS);
    check_feat_ctrl("PROD + both demotes (PROD_DBG_1 wins, ~SIP_DIS)", (~SIP & FUNC) | DBG);
}

// =============================================================================
// Test 20: the two outputs the rest of SEP consumes
// =============================================================================

void testbench::test_outputs_to_sep()
{
    report_test_start("Test 20: sep_debug and prod_dbg_active");

    const uint32_t prod     = lifecycle_ctrl_model::lc_state_encode(0x1);
    const uint32_t test_dev = lifecycle_ctrl_model::lc_state_encode(0x0);

    // sep_debug is FEAT_CTRL[0], and the inbound filter is bypassed while it is set.
    // PROD without a demote is the case that matters: the filter must be live there.
    clear_demote();
    drive_inputs(prod, 0, 0);
    if (!m_dut->get_sep_debug() && m_dut->get_feat_ctrl() == read_feat_ctrl())
        report_test_pass("PROD: sep_debug clear, and get_feat_ctrl() agrees with the register");
    else
        report_test_fail("PROD sep_debug", "sep_debug=" + std::to_string(m_dut->get_sep_debug()) +
            " feat_ctrl=" + hex64(m_dut->get_feat_ctrl()));

    // TEST_DEV enables everything, which includes sep_debug — a part out of reset with
    // an unprogrammed fuse array has the filter bypassed, as it does on silicon.
    drive_inputs(test_dev, 0, 0);
    if (m_dut->get_sep_debug())
        report_test_pass("TEST_DEV: sep_debug set");
    else
        report_test_fail("TEST_DEV sep_debug", "expected set, feat_ctrl=" +
            hex64(m_dut->get_feat_ctrl()));

    // A demote back into PROD_DBG restores the debug section, sep_debug with it.
    drive_inputs(prod, 0, 0);
    if (m_dut->get_prod_dbg_active())
        report_test_fail("prod_dbg_active reset", "asserted with no demote written");
    else
        report_test_pass("prod_dbg_active: clear with no demote");

    m_test->register_write_32(lifecycle_ctrl_basetest::DEMOTE_2_OFFSET, 0x1);
    wait(1, SC_NS);
    if (m_dut->get_prod_dbg_active() && m_dut->get_sep_debug())
        report_test_pass("PROD + demote_2: prod_dbg_active and sep_debug both set");
    else
        report_test_fail("prod_dbg_active demote", "prod_dbg=" +
            std::to_string(m_dut->get_prod_dbg_active()) + " sep_debug=" +
            std::to_string(m_dut->get_sep_debug()));

    // The eFuse takes prod_dbg_active regardless of state, and qualifies it itself.
    drive_inputs(test_dev, 0, 0);
    if (m_dut->get_prod_dbg_active())
        report_test_pass("prod_dbg_active: follows the demote bits, not the state");
    else
        report_test_fail("prod_dbg_active state", "deasserted by a state change");
}

// =============================================================================
// Main test sequence
// =============================================================================

void testbench::run_tests()
{
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));
    wait(10, SC_NS);

    CSML_INFO(1, logger) << "\n========================================"
                         << "\nLC_CTRL IP TESTBENCH"
                         << "\n========================================" << std::endl;

    test_reset_values();
    test_feat_ctrl_ro();
    test_test_dev_state();
    test_prod_state_no_demote();
    test_demote_1_w1s();
    test_demote_1_lock();
    test_demote_2_w1s();
    test_demote_2_lock();
    test_invalid_state();
    test_rma_chiplet_state();
    test_secure_tm();
    test_security_disable();

    // These drive the inputs themselves, so they run last: they leave the DUT holding
    // whatever state the last case set rather than the configured one.
    test_all_state_arms();
    test_lc_sigint_fail_safe();
    test_live_feature_disable();
    test_demote_diff_encoding();
    test_demote_upper_words();
    test_lock_scope();
    test_prod_dbg_priority();
    test_outputs_to_sep();

    report_test_summary();

    wait(100, SC_NS);
    sc_stop();
}

// =============================================================================
// sc_main
// =============================================================================

int sc_main(int argc, char *argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);
    testbench tb("testbench");
    sc_start();
#ifdef __GNUC__
#ifdef __COVERAGE__
    __gcov_dump();
#endif
#endif
    int ret = tb.m_tests_failed;
    std::quick_exit(ret);
    return ret;
}
