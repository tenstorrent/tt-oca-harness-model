// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "el2_pic_basetest.h"
#include "reg_param.h"
#include <sstream>
#include <iomanip>

// gcov coverage data flushing (GCC 11+)
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
testbench::testbench(sc_module_name name)
    : sc_module(name)
    , rst_n_sig("rst_n_sig")
    , clk_sig("clk_sig")
    , irq_sigs("irq_sigs", el2_pic::NUM_INTERRUPTS)
    , unbound_isock("unbound_isock")
{
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    m_dut  = std::make_unique<el2_pic::el2_pic_model>("el2_pic_dut");
    m_dut->bind_hart(&mock_hart);

    // Sync testbench logger verbosity with DUT (CCI ini may override build default)
    logger.setMaxVerbosity(m_dut->verbosity.get_param_value());
    m_test = std::make_unique<el2_pic_test>("el2_pic_test", el2_pic::NUM_INTERRUPTS);

    // TLM socket
    m_test->initiator_socket.bind(m_dut->target_socket);

    // Reset
    m_test->rst_no.bind(rst_n_sig);
    m_dut->rst_ni.bind(rst_n_sig);

    // Clock — el2_pic_model declares clk_i but no process is sensitive to it;
    // a static (never-toggled) signal is enough to satisfy port binding.
    m_dut->clk_i.bind(clk_sig);

    // IRQ lines
    for (unsigned i = 0; i < el2_pic::NUM_INTERRUPTS; ++i) {
        m_test->irq_o[i].bind(irq_sigs[i]);
        m_dut->irq_in[i].bind(irq_sigs[i]);
    }

    // Companion instance for FUNC-EL2PIC-012: clk and rst bound, every irq_in
    // left open, no hart. Left without a hart on purpose — arbitration returns
    // early on a null hart_, so it stays inert while still elaborating.
    m_dut_unbound = std::make_unique<el2_pic::el2_pic_model>("el2_pic_unbound");
    m_dut_unbound->rst_ni.bind(rst_n_sig);
    m_dut_unbound->clk_i.bind(clk_sig);
    unbound_isock.bind(m_dut_unbound->target_socket);

    SC_THREAD(run_tests);
}

// ---------------------------------------------------------------------------
// CSR read against m_dut_unbound (mirrors el2_pic_test::reg_read_32)
// ---------------------------------------------------------------------------
uint32_t testbench::unbound_read_32(unsigned byte_offset)
{
    uint32_t data = 0;
    const auto r = simtlm::read_word<uint32_t>(unbound_isock, byte_offset, data);
    note_unbound_transport(r, "unbound_read_32", byte_offset);
    return data;
}

void testbench::unbound_write_32(unsigned byte_offset, uint32_t value)
{
    const auto r = simtlm::write_word<uint32_t>(unbound_isock, byte_offset, value);
    note_unbound_transport(r, "unbound_write_32", byte_offset);
}

void testbench::note_unbound_transport(const simtlm::access_result& r, const char* op,
                                       unsigned byte_offset)
{
    if (r.ok())
        return;

    ++m_unbound_transport_failures;
    std::stringstream ss;
    ss << op << " at byte offset 0x" << std::hex << byte_offset
       << " returned " << simtlm::response_name(r.status);
    REG_WARN(1, logger) << "TRANSPORT: " << ss.str() << std::endl;
}

// ---------------------------------------------------------------------------
// Reporting helpers
// ---------------------------------------------------------------------------
void testbench::report_test_start(const std::string& test_name)
{
    // Transport errors are attributed to the test that caused them.
    m_test->clear_transport_failures();
    m_unbound_transport_failures = 0;

    REG_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================" << std::endl;
}

void testbench::report_test_pass(const std::string& test_name)
{
    // A scenario cannot pass on the strength of transactions the PIC refused.
    // Checking here rather than at each call site makes every existing test
    // sensitive to transport failures without restating the condition in any of
    // them. This is the single gate that finding #1 of the audit asks for.
    const unsigned tf = m_test->transport_failures() + m_unbound_transport_failures;
    if (tf != 0) {
        std::stringstream ss;
        ss << tf << " transport error(s) during the test; last: "
           << (m_test->last_transport_error().empty() ? "see TRANSPORT log above"
                                                      : m_test->last_transport_error());
        report_test_fail(test_name, ss.str());
        return;
    }

    m_tests_passed++;
    m_tests_run++;
    REG_INFO(1, logger) << test_name << ": PASS" << std::endl;
}

void testbench::report_test_fail(const std::string& test_name, const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    REG_WARN(1, logger) << test_name << ": FAIL - " << reason << std::endl;
}

void testbench::report_test_summary()
{
    std::stringstream ss;
    ss << "\n========================================\n"
       << "EL2 PIC Test Summary\n"
       << "========================================\n"
       << "Total:  " << m_tests_run    << "\n"
       << "Passed: " << m_tests_passed << "\n"
       << "Failed: " << m_tests_failed << "\n"
       << "========================================";
    REG_INFO(1, logger) << ss.str() << std::endl;
}

// ---------------------------------------------------------------------------
// Convenience: hard reset the DUT and all irq lines
// ---------------------------------------------------------------------------
static void do_reset(el2_pic_test& t, unsigned num_irqs)
{
    for (unsigned i = 0; i < num_irqs; ++i)
        t.drive_irq(i, false);
    t.assert_reset();
    wait(10, SC_NS);
    t.deassert_reset();
    wait(10, SC_NS);
}

// ---------------------------------------------------------------------------
// run_tests (SC_THREAD)
// ---------------------------------------------------------------------------
void testbench::run_tests()
{
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));

    // Initialise outputs
    m_test->rst_no.write(true);
    for (unsigned i = 0; i < el2_pic::NUM_INTERRUPTS; ++i)
        m_test->irq_o[i].write(false);
    wait(5, SC_NS);

    REG_INFO(1, logger) << "\n========================================"
                         << " EL2 PIC TESTBENCH START "
                         << "========================================" << std::endl;

    // FUNC-EL2PIC-001: Register Reset Values
    report_test_start("FUNC-EL2PIC-001: Register Reset Values");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_reset_values();

    // FUNC-EL2PIC-002: Register Read / Write with Masks
    report_test_start("FUNC-EL2PIC-002: Register RW Masks");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_mpiccfg_rw();
    test_meipl_rw();
    test_meie_rw();
    test_meigwctrl_rw();
    test_meip_readonly();
    test_meigwclr_writeonly();

    // FUNC-EL2PIC-003: Level-mode Gateway (active-high)
    report_test_start("FUNC-EL2PIC-003: Level Gateway Active-High");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_level_gateway_active_high();

    // FUNC-EL2PIC-004: Level-mode Gateway (active-low)
    report_test_start("FUNC-EL2PIC-004: Level Gateway Active-Low");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_level_gateway_active_low();

    // FUNC-EL2PIC-005: Edge-mode Gateway and MEIGWCLR
    report_test_start("FUNC-EL2PIC-005: Edge Gateway and MEIGWCLR");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_edge_gateway_and_meigwclr();

    // FUNC-EL2PIC-006: Priority Arbitration
    report_test_start("FUNC-EL2PIC-006: Priority Arbitration");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_arbitration_highest_priority_wins();

    // FUNC-EL2PIC-007: Interrupt Enable Gate
    report_test_start("FUNC-EL2PIC-007: Interrupt Enable Gate");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_enable_gate();

    // FUNC-EL2PIC-008: Priority Order Inversion
    report_test_start("FUNC-EL2PIC-008: Priority Order Inversion (priord)");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_priord_inversion();

    // FUNC-EL2PIC-009: Reset Clears All State
    report_test_start("FUNC-EL2PIC-009: Reset Clears All State");
    test_reset_clears_state();

    // FUNC-EL2PIC-010: Highest source ID is fully functional
    report_test_start("FUNC-EL2PIC-010: Highest Source ID (255)");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_high_source_end_to_end();

    // FUNC-EL2PIC-011: meip word mapping across all 8 pending words
    report_test_start("FUNC-EL2PIC-011: meip Word Mapping");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_meip_word_mapping();

    // FUNC-EL2PIC-013: Edge-mode clear against a still-asserted source
    report_test_start("FUNC-EL2PIC-013: Edge Clear While Asserted");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_edge_clear_while_asserted();

    // FUNC-EL2PIC-014: priord=1 makes raw priority 0 the highest
    report_test_start("FUNC-EL2PIC-014: priord=1 Raw Priority 0 Is Highest");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_priord_raw_zero_is_highest();

    // FUNC-EL2PIC-015: meipt / meicurpl threshold + notify_threshold_changed
    report_test_start("FUNC-EL2PIC-015: Threshold Blocks and Notify");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_threshold_blocks_and_notify();

    // FUNC-EL2PIC-016: Winner change while EIP already asserted
    report_test_start("FUNC-EL2PIC-016: Winner Change While Asserted");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_winner_change_while_asserted();

    // FUNC-EL2PIC-017: Source 0 ignored; unbound instance has no hart
    report_test_start("FUNC-EL2PIC-017: Source 0 and Null Hart");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    test_source0_and_null_hart();

    // FUNC-EL2PIC-012: Unbound sources tied low
    report_test_start("FUNC-EL2PIC-012: Unbound Sources Tied Low");
    test_unbound_sources_tied_low();

    // FUNC-EL2PIC-018: Malformed generic payloads
    report_test_start("FUNC-EL2PIC-018: Malformed Generic Payloads");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_malformed_payloads();

    // FUNC-EL2PIC-019: Threshold CSRs, reserved source 0, null-hart arbiter
    report_test_start("FUNC-EL2PIC-019: Threshold / Reserved Source / Null Hart");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_threshold_and_reserved_source();

    // FUNC-EL2PIC-020: Equal-priority tie break in both assertion orders
    report_test_start("FUNC-EL2PIC-020: Equal-Priority Tie Break");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_equal_priority_tie_break();

    // FUNC-EL2PIC-021: Winner deasserts, claim falls back, EIP never drops
    report_test_start("FUNC-EL2PIC-021: Winner Fallback Without EIP Glitch");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_winner_fallback_no_eip_glitch();

    // FUNC-EL2PIC-022: Active-low edge gateway
    report_test_start("FUNC-EL2PIC-022: Edge Gateway Active-Low");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_edge_gateway_active_low();

    // FUNC-EL2PIC-023: MEIGWCTRL polarity/type changed under a live input
    report_test_start("FUNC-EL2PIC-023: Gateway Reconfig While Asserted");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_gateway_reconfig_while_asserted();

    // FUNC-EL2PIC-024: Threshold boundaries, both priord modes, both CSRs
    report_test_start("FUNC-EL2PIC-024: Threshold Boundaries Both Modes");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_threshold_boundaries_both_modes();

    // FUNC-EL2PIC-025: MEIP writes ignored on every word; reserved source 0
    report_test_start("FUNC-EL2PIC-025: MEIP Writes / Reserved Source 0");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_meip_write_ignored_and_reserved_source0();

    // FUNC-EL2PIC-026: Reset asserted with several edge latches pending
    report_test_start("FUNC-EL2PIC-026: Reset With Pending Edge Latches");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_reset_with_pending_edge_latches();

    // FUNC-EL2PIC-027: transport_dbg and DMI policy
    report_test_start("FUNC-EL2PIC-027: Debug Transport and DMI Policy");
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);
    mock_hart.reset();
    test_debug_transport_and_dmi();

    // ------------------------------------------------------------------
    // Final summary
    // ------------------------------------------------------------------
    REG_INFO(1, logger) << "\n========================================"
                         << "       TEST SUITE SUMMARY"
                         << "========================================" << std::endl;

    std::stringstream ss;
    ss << "Total Tests:  " << m_tests_run;
    REG_INFO(1, logger) << ss.str() << std::endl;
    ss.str(""); ss << "Passed:       " << m_tests_passed << " (PASS)";
    REG_INFO(1, logger) << ss.str() << std::endl;
    ss.str(""); ss << "Failed:       " << m_tests_failed << " (FAIL)";
    REG_INFO(1, logger) << ss.str() << std::endl;
    if (m_tests_run > 0) {
        double rate = 100.0 * m_tests_passed / m_tests_run;
        ss.str(""); ss << "Success Rate: " << std::fixed << std::setprecision(1) << rate << "%";
        REG_INFO(1, logger) << ss.str() << std::endl;
    }
    if (m_tests_failed > 0) {
        REG_ERROR(0, logger) << "\nFailed Tests:" << std::endl;
        for (const auto& t : m_failed_tests) {
            REG_ERROR(0, logger) << "  - " << t << std::endl;
        }
        REG_ERROR(0, logger) << "[OVERALL RESULT: FAILED]" << std::endl;
    } else if (m_tests_passed > 0) {
        REG_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]" << std::endl;
    } else {
        REG_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]" << std::endl;
    }

    wait(100, SC_NS);
    sc_stop();
}

// ===========================================================================
// FUNC-EL2PIC-001: Register Reset Values
// ===========================================================================
void testbench::test_reset_values()
{
    bool pass = true;
    std::string fail_reason;

    auto check = [&](unsigned offset, uint32_t expected, const char* name) {
        uint32_t got = m_test->reg_read_32(offset);
        if (got != expected) {
            pass = false;
            std::ostringstream oss;
            oss << name << " reset val 0x" << std::hex << got
                << " != 0x" << expected << " ";
            fail_reason += oss.str();
        }
    };

    check(el2_pic_basetest::mpiccfg_offset(), 0, "MPICCFG");
    for (unsigned s = 1; s < el2_pic::NUM_INTERRUPTS; ++s) {
        check(el2_pic_basetest::meipl_offset(s),     0, "MEIPL");
        check(el2_pic_basetest::meie_offset(s),      0, "MEIE");
        check(el2_pic_basetest::meigwctrl_offset(s), 0, "MEIGWCTRL");
    }
    check(el2_pic_basetest::meip_offset(0), 0, "MEIP[0]");

    if (pass)
        report_test_pass("FUNC-EL2PIC-001: test_reset_values");
    else
        report_test_fail("FUNC-EL2PIC-001: test_reset_values", fail_reason);
}

// ===========================================================================
// FUNC-EL2PIC-002: Register RW with Masks
// ===========================================================================
void testbench::test_mpiccfg_rw()
{
    bool pass = true;
    std::string reason;

    // Write only the valid bit
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x1u);
    uint32_t v = m_test->reg_read_32(el2_pic_basetest::mpiccfg_offset());
    if (v != 0x1u) { pass = false; reason += "write 0x1 read back 0x" + std::to_string(v) + " "; }

    // Write all bits — only bit 0 should survive
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0xFFFFFFFFu);
    v = m_test->reg_read_32(el2_pic_basetest::mpiccfg_offset());
    if ((v & ~el2_pic_basetest::MPICCFG_READ_MASK) != 0) {
        pass = false; reason += "reserved bits leaked: 0x" + std::to_string(v);
    }

    // Clean up
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0);

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_mpiccfg_rw");
    else
        report_test_fail("FUNC-EL2PIC-002: test_mpiccfg_rw", reason);
}

void testbench::test_meipl_rw()
{
    bool pass = true;
    std::string reason;
    const unsigned s = 1;

    m_test->reg_write_32(el2_pic_basetest::meipl_offset(s), 0xFu);
    uint32_t v = m_test->reg_read_32(el2_pic_basetest::meipl_offset(s));
    if ((v & el2_pic_basetest::MEIPL_READ_MASK) != 0xFu) {
        pass = false; reason += "write 0xF read 0x" + std::to_string(v) + " ";
    }

    m_test->reg_write_32(el2_pic_basetest::meipl_offset(s), 0xFFFFFFFFu);
    v = m_test->reg_read_32(el2_pic_basetest::meipl_offset(s));
    if ((v & ~el2_pic_basetest::MEIPL_READ_MASK) != 0) {
        pass = false; reason += "reserved bits leaked: 0x" + std::to_string(v);
    }

    m_test->reg_write_32(el2_pic_basetest::meipl_offset(s), 0);

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_meipl_rw");
    else
        report_test_fail("FUNC-EL2PIC-002: test_meipl_rw", reason);
}

void testbench::test_meie_rw()
{
    bool pass = true;
    std::string reason;
    const unsigned s = 1;

    m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    uint32_t v = m_test->reg_read_32(el2_pic_basetest::meie_offset(s));
    if (v != 0x1u) { pass = false; reason += "write 0x1 read 0x" + std::to_string(v) + " "; }

    m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0xFFFFFFFFu);
    v = m_test->reg_read_32(el2_pic_basetest::meie_offset(s));
    if ((v & ~el2_pic_basetest::MEIE_READ_MASK) != 0) {
        pass = false; reason += "reserved bits leaked";
    }

    m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0);

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_meie_rw");
    else
        report_test_fail("FUNC-EL2PIC-002: test_meie_rw", reason);
}

void testbench::test_meigwctrl_rw()
{
    bool pass = true;
    std::string reason;
    const unsigned s = 2;

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x3u);
    uint32_t v = m_test->reg_read_32(el2_pic_basetest::meigwctrl_offset(s));
    if ((v & el2_pic_basetest::MEIGWCTRL_READ_MASK) != 0x3u) {
        pass = false; reason += "write 0x3 read 0x" + std::to_string(v) + " ";
    }

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0xFFFFFFFFu);
    v = m_test->reg_read_32(el2_pic_basetest::meigwctrl_offset(s));
    if ((v & ~el2_pic_basetest::MEIGWCTRL_READ_MASK) != 0) {
        pass = false; reason += "reserved bits leaked";
    }

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0);

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_meigwctrl_rw");
    else
        report_test_fail("FUNC-EL2PIC-002: test_meigwctrl_rw", reason);
}

void testbench::test_meip_readonly()
{
    bool pass = true;
    std::string reason;

    // MEIP should be 0 after reset
    uint32_t before = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
    // Attempt write — should be silently ignored
    m_test->reg_write_32(el2_pic_basetest::meip_offset(0), 0xFFFFFFFFu);
    uint32_t after = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
    if (after != before) {
        pass = false;
        reason += "MEIP changed after write: before=0x" + std::to_string(before) +
                  " after=0x" + std::to_string(after);
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_meip_readonly");
    else
        report_test_fail("FUNC-EL2PIC-002: test_meip_readonly", reason);
}

void testbench::test_meigwclr_writeonly()
{
    bool pass = true;
    std::string reason;
    const unsigned s = 3;

    // Reads from MEIGWCLR should always return 0
    uint32_t v = m_test->reg_read_32(el2_pic_basetest::meigwclr_offset(s));
    if (v != 0) {
        pass = false;
        reason += "MEIGWCLR read returned 0x" + std::to_string(v) + " (expected 0)";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-002: test_meigwclr_writeonly");
    else
        report_test_fail("FUNC-EL2PIC-002: test_meigwclr_writeonly", reason);
}

// ===========================================================================
// FUNC-EL2PIC-003: Level-mode Gateway (active-high)
// ===========================================================================
void testbench::test_level_gateway_active_high()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 1;

    // Configure: level mode (bit1=0), active-high (bit0=0 polarity), prio=5, enable=1
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u); // level, active-high
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 5u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    // Drive IRQ high
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted after irq_in high; ";
    }
    if (mock_hart.claim_id != src) {
        pass = false; reason += "claim_id=" + std::to_string(mock_hart.claim_id) +
                                " expected " + std::to_string(src) + "; ";
    }

    // Drive IRQ low — EIP should de-assert
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after irq_in de-asserted";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-003: test_level_gateway_active_high");
    else
        report_test_fail("FUNC-EL2PIC-003: test_level_gateway_active_high", reason);
}

// ===========================================================================
// FUNC-EL2PIC-004: Level-mode Gateway (active-low)
// ===========================================================================
void testbench::test_level_gateway_active_low()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 2;

    // Configure: level mode, active-low
    // MEIGWCTRL bits: [0]=polarity(0=active-high,1=active-low), [1]=irq_type(0=level,1=edge)
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x1u); // level, active-low
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 3u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    // irq_in LOW → effective high (active-low polarity) → EIP asserted
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted with active-low irq=0; ";
    }

    // irq_in HIGH → effective low → EIP de-asserted
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted with active-low irq=1";
    }

    // Restore
    m_test->drive_irq(src, false);

    if (pass)
        report_test_pass("FUNC-EL2PIC-004: test_level_gateway_active_low");
    else
        report_test_fail("FUNC-EL2PIC-004: test_level_gateway_active_low", reason);
}

// ===========================================================================
// FUNC-EL2PIC-005: Edge-mode Gateway and MEIGWCLR
// ===========================================================================
void testbench::test_edge_gateway_and_meigwclr()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 3;

    // Configure: edge mode (bit1=1), active-high (bit0=0)
    // MEIGWCTRL: [0]=polarity, [1]=irq_type
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x2u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 7u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    // Start low
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP asserted before edge; ";
    }

    // Rising edge → latch sets, EIP asserted
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted after rising edge; ";
    }

    // De-assert irq — latch stays, EIP stays
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP de-asserted without MEIGWCLR write; ";
    }

    // Write MEIGWCLR — clears latch, de-asserts EIP
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after MEIGWCLR write";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-005: test_edge_gateway_and_meigwclr");
    else
        report_test_fail("FUNC-EL2PIC-005: test_edge_gateway_and_meigwclr", reason);
}

// ===========================================================================
// FUNC-EL2PIC-006: Priority Arbitration
// ===========================================================================
void testbench::test_arbitration_highest_priority_wins()
{
    bool pass = true;
    std::string reason;
    const unsigned src_lo = 4;  // priority 3 (lower)
    const unsigned src_hi = 5;  // priority 8 (higher)

    for (unsigned s : {src_lo, src_hi}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u); // level, active-high
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_lo), 3u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_hi), 8u);
    wait(SC_ZERO_TIME);

    // Assert both sources
    m_test->drive_irq(src_lo, true);
    m_test->drive_irq(src_hi, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted; ";
    }
    if (mock_hart.claim_id != src_hi) {
        pass = false;
        reason += "claim_id=" + std::to_string(mock_hart.claim_id) +
                  " expected " + std::to_string(src_hi);
    }

    // Clean up
    m_test->drive_irq(src_lo, false);
    m_test->drive_irq(src_hi, false);
    wait(SC_ZERO_TIME);

    if (pass)
        report_test_pass("FUNC-EL2PIC-006: test_arbitration_highest_priority_wins");
    else
        report_test_fail("FUNC-EL2PIC-006: test_arbitration_highest_priority_wins", reason);
}

// ===========================================================================
// FUNC-EL2PIC-007: Interrupt Enable Gate (MEIE)
// ===========================================================================
void testbench::test_enable_gate()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 6;

    // Setup: level, active-high, priority set, but MEIE disabled
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 5u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x0u); // disabled
    wait(SC_ZERO_TIME);

    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP asserted with MEIE disabled; ";
    }

    // Enable MEIE → EIP should now assert
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted after enabling MEIE; ";
    }

    // Disable again → EIP should de-assert
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x0u);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after disabling MEIE";
    }

    m_test->drive_irq(src, false);

    if (pass)
        report_test_pass("FUNC-EL2PIC-007: test_enable_gate");
    else
        report_test_fail("FUNC-EL2PIC-007: test_enable_gate", reason);
}

// ===========================================================================
// FUNC-EL2PIC-008: Priority Order Inversion (priord)
// ===========================================================================
void testbench::test_priord_inversion()
{
    bool pass = true;
    std::string reason;
    const unsigned src_a = 7;  // raw priority 1 (low in normal mode)
    const unsigned src_b = 8;  // raw priority 2

    for (unsigned s : {src_a, src_b}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_a), 1u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_b), 2u);

    // priord=0: higher raw = higher effective → src_b wins
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x0u);
    wait(SC_ZERO_TIME);

    m_test->drive_irq(src_a, true);
    m_test->drive_irq(src_b, true);
    wait(SC_ZERO_TIME);

    if (mock_hart.claim_id != src_b) {
        pass = false;
        reason += "priord=0: expected claim " + std::to_string(src_b) +
                  " got " + std::to_string(mock_hart.claim_id) + "; ";
    }

    // priord=1: ~raw & 0xF → raw=1 → eff=14; raw=2 → eff=13 → src_a wins
    m_test->drive_irq(src_a, false);
    m_test->drive_irq(src_b, false);
    wait(SC_ZERO_TIME);

    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x1u);
    wait(SC_ZERO_TIME);

    // meipt/meicurpl are not auto-rescaled by hardware when priord changes
    // (el2_pic_ctrl.sv inverts them unconditionally, same as intpriority).
    // Raw 0 means "no threshold" only under priord=0; under priord=1 it
    // inverts to 0xF (maximum), which would block every source. Real
    // firmware switching to priord=1 must reprogram these for the new
    // encoding — mirror that here so this test measures arbitration
    // ordering, not an unconfigured-threshold false negative.
    mock_hart.meipt        = 0xFu;
    mock_hart.meicurpl_csr = 0xFu;

    m_test->drive_irq(src_a, true);
    m_test->drive_irq(src_b, true);
    wait(SC_ZERO_TIME);

    if (mock_hart.claim_id != src_a) {
        pass = false;
        reason += "priord=1: expected claim " + std::to_string(src_a) +
                  " got " + std::to_string(mock_hart.claim_id);
    }

    m_test->drive_irq(src_a, false);
    m_test->drive_irq(src_b, false);
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x0u);
    mock_hart.meipt        = 0u;
    mock_hart.meicurpl_csr = 0u;
    // Let the de-assert's gateway_changed(src_a)/gateway_changed(src_b) callbacks
    // (scheduled for the next delta cycle, not yet run) fully settle before the
    // next test begins — otherwise their pending re-arbitration races against
    // whatever the next test does immediately after this function returns.
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-008: test_priord_inversion");
    else
        report_test_fail("FUNC-EL2PIC-008: test_priord_inversion", reason);
}

// ===========================================================================
// FUNC-EL2PIC-009: Reset Clears All State
// ===========================================================================
void testbench::test_reset_clears_state()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 9;

    // Load state
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 0xFu);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u);
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted before reset (pre-condition); ";
    }

    // Assert reset
    do_reset(*m_test, el2_pic::NUM_INTERRUPTS);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after reset; ";
    }

    uint32_t meipl = m_test->reg_read_32(el2_pic_basetest::meipl_offset(src));
    if (meipl != 0) {
        pass = false; reason += "MEIPL[" + std::to_string(src) + "]=" +
                                std::to_string(meipl) + " after reset; ";
    }

    uint32_t meip = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
    if (meip != 0) {
        pass = false; reason += "MEIP[0]=0x" + std::to_string(meip) + " after reset";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-009: test_reset_clears_state");
    else
        report_test_fail("FUNC-EL2PIC-009: test_reset_clears_state", reason);
}

// ===========================================================================
// FUNC-EL2PIC-010: Highest source ID (255) is fully functional
//
// The SEP VeeR build sets RV_PIC_TOTAL_INT=255, so source 255 is the last legal
// ID. Exercising it proves the register arrays, the gateway spawn loop, the
// arbitration scan and the top pending word all reach the end of the range —
// none of which the low-numbered sources used by tests 003-009 would catch.
// ===========================================================================
void testbench::test_high_source_end_to_end()
{
    bool pass = true;
    std::string reason;
    const unsigned src = el2_pic::NUM_INTERRUPTS - 1;   // 255

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u); // level, active-high
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 7u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    // Configuration must survive a round trip this far up the array.
    uint32_t prio = m_test->reg_read_32(el2_pic_basetest::meipl_offset(src));
    if ((prio & el2_pic_basetest::MEIPL_READ_MASK) != 7u) {
        pass = false; reason += "MEIPL[255] read back 0x" + std::to_string(prio) + "; ";
    }

    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted for source 255; ";
    }
    if (mock_hart.claim_id != src) {
        pass = false; reason += "claim_id=" + std::to_string(mock_hart.claim_id) +
                                " expected 255; ";
    }

    // Source 255 lives in the top pending word, bit 31 (255 = 7*32 + 31).
    const unsigned top_word = el2_pic::NUM_PEND_WORDS - 1;
    uint32_t meip_top = m_test->reg_read_32(el2_pic_basetest::meip_offset(top_word));
    if (meip_top != 0x80000000u) {
        pass = false; reason += "meip[7]=0x" + std::to_string(meip_top) +
                                " expected 0x80000000; ";
    }
    uint32_t meip_low = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
    if (meip_low != 0) {
        pass = false; reason += "meip[0]=0x" + std::to_string(meip_low) +
                                " polluted by source 255; ";
    }

    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after source 255 de-asserted; ";
    }
    meip_top = m_test->reg_read_32(el2_pic_basetest::meip_offset(top_word));
    if (meip_top != 0) {
        pass = false; reason += "meip[7]=0x" + std::to_string(meip_top) + " not cleared";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-010: test_high_source_end_to_end");
    else
        report_test_fail("FUNC-EL2PIC-010: test_high_source_end_to_end", reason);
}

// ===========================================================================
// FUNC-EL2PIC-011: meip pending words map sources across all 8 words
//
// Bit Y of meip[X] is source X*32+Y (el2_pic_ctrl.sv:488-491). One source per
// word, at a different bit position each time, so a word-index or bit-position
// error cannot pass by coincidence.
// ===========================================================================
void testbench::test_meip_word_mapping()
{
    bool pass = true;
    std::string reason;

    // source, expected word, expected bit
    struct Case { unsigned src, word, bit; };
    const Case cases[] = {
        {  1, 0,  1 }, {  40, 1,  8 }, {  66, 2,  2 }, { 127, 3, 31 },
        {128, 4,  0 }, { 175, 5, 15 }, { 200, 6,  8 }, { 250, 7, 26 },
    };

    for (const Case& c : cases) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(c.src), 0x0u);
        m_test->reg_write_32(el2_pic_basetest::meipl_offset(c.src), 1u);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(c.src), 0x1u);
        m_test->drive_irq(c.src, true);
    }
    wait(SC_ZERO_TIME);

    // Each word must hold exactly the one bit its case named.
    for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w) {
        uint32_t expected = 0;
        for (const Case& c : cases)
            if (c.word == w) expected |= (1u << c.bit);

        uint32_t got = m_test->reg_read_32(el2_pic_basetest::meip_offset(w));
        if (got != expected) {
            pass = false;
            std::ostringstream oss;
            oss << "meip[" << w << "]=0x" << std::hex << got
                << " expected 0x" << expected << "; ";
            reason += oss.str();
        }
    }

    for (const Case& c : cases)
        m_test->drive_irq(c.src, false);
    wait(SC_ZERO_TIME);

    for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w) {
        uint32_t got = m_test->reg_read_32(el2_pic_basetest::meip_offset(w));
        if (got != 0) {
            pass = false;
            reason += "meip[" + std::to_string(w) + "] not cleared; ";
        }
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-011: test_meip_word_mapping");
    else
        report_test_fail("FUNC-EL2PIC-011: test_meip_word_mapping", reason);
}

// ===========================================================================
// FUNC-EL2PIC-013: Edge-mode meigwclr against a still-asserted source
//
// The "edge" gateway latches level rather than detecting a transition:
// el2_pic_ctrl.sv:588 sets gw_int_pending from effective unconditionally, and
// :592 ORs the live effective level on top of the latch. So clearing while the
// source is still asserted re-pends immediately, and only returning the input
// to its inactive level makes a clear stick. A transition-detector
// implementation passes FUNC-EL2PIC-005 but drops the interrupt here.
// ===========================================================================
void testbench::test_edge_clear_while_asserted()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 9;

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x2u);  // edge, active-high
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 7u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);

    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted on assert; ";
    }

    // Clear with the source still high: must re-pend rather than go quiet.
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP dropped by meigwclr while source still asserted; ";
    }
    if ((m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & (1u << src)) == 0) {
        pass = false; reason += "meip bit cleared while source still asserted; ";
    }

    // Drop the source. The latch is still set from before, so it stays pending
    // until cleared — this is what distinguishes edge from level mode.
    m_test->drive_irq(src, false);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "latch lost when source de-asserted; ";
    }

    // Now the clear sticks.
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after clearing an inactive source; ";
    }
    if (m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) != 0) {
        pass = false; reason += "meip[0] not clear at end";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-013: test_edge_clear_while_asserted");
    else
        report_test_fail("FUNC-EL2PIC-013: test_edge_clear_while_asserted", reason);
}

// ===========================================================================
// FUNC-EL2PIC-014: Under priord=1, raw priority 0 is the highest
//
// "Priority 0 never interrupts" holds only for priord=0. el2_pic_ctrl.sv:329
// inverts intpriority before arbitration and applies no raw==0 exception, so
// under priord=1 raw 0 becomes effective 15 (the winner) and raw 15 becomes the
// value that can never clear the threshold. FUNC-EL2PIC-008 uses raw 1 and 2,
// which straddle neither end, so it cannot catch a raw==0 skip.
// ===========================================================================
void testbench::test_priord_raw_zero_is_highest()
{
    bool pass = true;
    std::string reason;
    const unsigned src_zero = 11;   // raw 0  → eff 15 under priord=1 (highest)
    const unsigned src_mid  = 12;   // raw 8  → eff 7
    const unsigned src_max  = 13;   // raw 15 → eff 0  (can never win)

    for (unsigned s : {src_zero, src_mid, src_max}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u);  // level, active-high
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_zero), 0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_mid),  8u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_max),  15u);

    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x1u);   // priord=1
    // Thresholds invert too, so leaving them at raw 0 would present an
    // effective 15 and block everything. Same reprogramming FUNC-EL2PIC-008 does.
    mock_hart.meipt        = 0xFu;
    mock_hart.meicurpl_csr = 0xFu;
    wait(SC_ZERO_TIME);

    m_test->drive_irq(src_mid, true);
    m_test->drive_irq(src_max, true);
    m_test->drive_irq(src_zero, true);
    wait(SC_ZERO_TIME);

    if (!mock_hart.eip_asserted) {
        pass = false; reason += "no EIP with raw-0 source pending under priord=1; ";
    }
    if (mock_hart.claim_id != src_zero) {
        pass = false;
        reason += "expected claim " + std::to_string(src_zero) + " (raw 0 → eff 15) got " +
                  std::to_string(mock_hart.claim_id) + "; ";
    }

    // Drop the raw-0 source: src_mid (eff 7) should take over, and src_max
    // (eff 0) must never win once src_mid also goes away.
    m_test->drive_irq(src_zero, false);
    wait(SC_ZERO_TIME);
    if (mock_hart.claim_id != src_mid) {
        pass = false;
        reason += "expected claim " + std::to_string(src_mid) + " got " +
                  std::to_string(mock_hart.claim_id) + "; ";
    }

    m_test->drive_irq(src_mid, false);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "raw 15 (eff 0) won under priord=1; ";
    }

    m_test->drive_irq(src_max, false);
    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x0u);
    mock_hart.meipt        = 0u;
    mock_hart.meicurpl_csr = 0u;
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-014: test_priord_raw_zero_is_highest");
    else
        report_test_fail("FUNC-EL2PIC-014: test_priord_raw_zero_is_highest", reason);
}

// ===========================================================================
// FUNC-EL2PIC-012: Unbound irq_in sources are tied low
//
// The PIC has 256 slots but an integration drives only the sources it has. This
// checks the companion m_dut_unbound instance, constructed with no irq_in
// bindings at all: reaching this test already proves elaboration survived (the
// gateway spawn would otherwise fail on an unbound port, and sc_start would
// abort with E109), and the port check below confirms the tie-off is what
// carried it rather than some accident of ordering.
// ===========================================================================
void testbench::test_unbound_sources_tied_low()
{
    bool pass = true;
    std::string reason;

    unsigned unbound = 0;
    unsigned high    = 0;
    for (unsigned i = 0; i < el2_pic::NUM_INTERRUPTS; ++i) {
        if (!m_dut_unbound->irq_in[i].get_interface()) {
            ++unbound;
        } else if (m_dut_unbound->irq_in[i].read()) {
            ++high;   // a tie-off must read low, or every source looks pending
        }
    }

    if (unbound != 0) {
        pass = false;
        reason += std::to_string(unbound) + " irq_in ports left unbound; ";
    }
    if (high != 0) {
        pass = false;
        reason += std::to_string(high) + " tied-off ports read high; ";
    }

    // With every gateway at its reset config (level mode, active-high), a tied
    // low input must leave all pending words clear. Had the tie-off driven high,
    // all 255 sources would read pending here.
    for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w) {
        uint32_t got = unbound_read_32(el2_pic_basetest::meip_offset(w));
        if (got != 0) {
            pass = false;
            std::ostringstream oss;
            oss << "unbound instance meip[" << w << "]=0x" << std::hex << got << "; ";
            reason += oss.str();
        }
    }

    // The bound DUT must be unaffected: its own bindings take precedence over
    // any tie-off, which is what lets a parent drive just the sources it has.
    if (m_dut->irq_in[1].get_interface() != &irq_sigs[1]) {
        pass = false;
        reason += "bound DUT's irq_in[1] no longer points at the testbench signal";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-012: test_unbound_sources_tied_low");
    else
        report_test_fail("FUNC-EL2PIC-012: test_unbound_sources_tied_low", reason);
}

// ===========================================================================
// FUNC-EL2PIC-015: meipt / meicurpl threshold drops an asserted EIP and
// notify_threshold_changed() reapplies the new threshold immediately.
//
// Firmware writes meipt (0xBC9) / meicurpl (0xBCC) and the ISS wrapper calls
// notify_threshold_changed(). Without that hook the PIC would keep EIP high
// until the next gateway event. Also covers the "threshold already blocking
// before the source pends" path (eip_asserted_ is false, just return).
// ===========================================================================
void testbench::test_threshold_blocks_and_notify()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 14;

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 5u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    mock_hart.meipt        = 0u;
    mock_hart.meicurpl_csr = 0u;
    wait(SC_ZERO_TIME);

    // Source pends with no threshold: EIP must assert.
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not asserted before threshold; ";
    }

    // Raise meipt to 5 (eff 5); prio 5 is not strictly greater → drop EIP.
    mock_hart.meipt = 5u;
    m_dut->notify_threshold_changed();
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after meipt=5 blocked prio 5; ";
    }

    // Lower meipt so the still-pending source wins again.
    mock_hart.meipt = 4u;
    m_dut->notify_threshold_changed();
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP not re-asserted after meipt lowered; ";
    }

    // meicurpl at 5 also blocks (same compare as meipt).
    mock_hart.meicurpl_csr = 5u;
    m_dut->notify_threshold_changed();
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted after meicurpl=5; ";
    }

    // Fresh source with threshold already blocking: no EIP, no clear path.
    m_test->drive_irq(src, false);
    mock_hart.meipt        = 15u;
    mock_hart.meicurpl_csr = 0u;
    wait(SC_ZERO_TIME);
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP asserted while meipt=15 blocks every source";
    }

    m_test->drive_irq(src, false);
    mock_hart.meipt        = 0u;
    mock_hart.meicurpl_csr = 0u;
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-015: test_threshold_blocks_and_notify");
    else
        report_test_fail("FUNC-EL2PIC-015: test_threshold_blocks_and_notify", reason);
}

// ===========================================================================
// FUNC-EL2PIC-016: A higher-priority source arriving while EIP is already
// asserted must retarget claim_id (best_id != current_claim_id_).
// FUNC-EL2PIC-006 asserts both sources in the same delta, so it never takes
// the "already asserted, different winner" branch.
// ===========================================================================
void testbench::test_winner_change_while_asserted()
{
    bool pass = true;
    std::string reason;
    const unsigned src_lo = 15;
    const unsigned src_hi = 16;

    for (unsigned s : {src_lo, src_hi}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_lo), 3u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src_hi), 9u);
    wait(SC_ZERO_TIME);

    m_test->drive_irq(src_lo, true);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src_lo) {
        pass = false;
        reason += "low-prio source did not claim first (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }

    m_test->drive_irq(src_hi, true);
    wait(SC_ZERO_TIME);
    if (mock_hart.claim_id != src_hi) {
        pass = false;
        reason += "high-prio source did not steal claim (claim=" +
                  std::to_string(mock_hart.claim_id) + ")";
    }

    m_test->drive_irq(src_lo, false);
    m_test->drive_irq(src_hi, false);
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-016: test_winner_change_while_asserted");
    else
        report_test_fail("FUNC-EL2PIC-016: test_winner_change_while_asserted", reason);
}

// ===========================================================================
// FUNC-EL2PIC-017: Source 0 is reserved (gateway_changed returns immediately)
// and the unbound companion instance has no hart, so arbitration must return
// on hart_ == nullptr rather than touching a claim_id.
// ===========================================================================
void testbench::test_source0_and_null_hart()
{
    bool pass = true;
    std::string reason;

    // Source 0: configure as if it were a real source, then toggle the pin.
    // MEIE[0] / MEIPL[0] have no post-write callbacks (registration skips 0),
    // so this only exercises gateway_changed(0).
    m_test->drive_irq(0, true);
    wait(SC_ZERO_TIME);
    uint32_t meip0 = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
    if (meip0 & 1u) {
        pass = false; reason += "source 0 set meip bit 0; ";
    }
    if (mock_hart.claim_id == 0 && mock_hart.eip_asserted) {
        pass = false; reason += "source 0 asserted EIP; ";
    }
    m_test->drive_irq(0, false);
    wait(SC_ZERO_TIME);

    // Unbound instance: write MEIE[1] / MEIPL[1] so post_write re-evaluates
    // with hart_ == nullptr. Must not crash; pending stays 0 (inputs tied low).
    unbound_write_32(el2_pic_basetest::meipl_offset(1), 0xFu);
    unbound_write_32(el2_pic_basetest::meie_offset(1), 0x1u);
    unbound_write_32(el2_pic_basetest::meigwctrl_offset(1), 0x0u);
    wait(SC_ZERO_TIME);
    uint32_t unbound_meip = unbound_read_32(el2_pic_basetest::meip_offset(0));
    if (unbound_meip != 0) {
        pass = false;
        reason += "unbound instance meip[0]=0x" + std::to_string(unbound_meip);
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-017: test_source0_and_null_hart");
    else
        report_test_fail("FUNC-EL2PIC-017: test_source0_and_null_hart", reason);
}

// ===========================================================================
// FUNC-EL2PIC-019: meipt/meicurpl threshold, reserved source 0, null hart
//
// Distinct from FUNC-EL2PIC-015, which this used to share an ID with. 015
// sweeps the threshold against a fixed winner; this one covers the
// same-winner-reevaluated and null-hart-instance paths.
// ===========================================================================
void testbench::test_threshold_and_reserved_source()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 4;

    // Source 0 is reserved. Toggling it must hit gateway_changed's early
    // return and leave the hart idle.
    m_test->drive_irq(0, true);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false;
        reason += "source 0 asserted EIP; ";
    }
    m_test->drive_irq(0, false);
    wait(SC_ZERO_TIME);

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 5u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(SC_ZERO_TIME);

    // meipt effective 7 >= prio 5 → winner is suppressed.
    mock_hart.meipt = 7u;
    m_test->drive_irq(src, true);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false;
        reason += "EIP asserted while meipt blocked prio 5; ";
    }

    // Drop the threshold and re-evaluate without another gateway edge.
    mock_hart.meipt = 0u;
    m_dut->notify_threshold_changed();
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src) {
        pass = false;
        reason += "notify_threshold_changed did not assert EIP; ";
    }

    // Same winner re-evaluated: MEIPL write must keep EIP/claim_id stable.
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 6u);
    wait(SC_ZERO_TIME);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src) {
        pass = false;
        reason += "same-winner MEIPL write dropped EIP; ";
    }

    // Raise meicurpl above the winner so the already-asserted IRQ is cleared.
    mock_hart.meicurpl_csr = 0xFu;
    m_dut->notify_threshold_changed();
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false;
        reason += "meicurpl did not clear EIP; ";
    }

    m_test->drive_irq(src, false);
    mock_hart.reset();

    // Companion instance has no hart. A pending+enabled write must take the
    // hart_ == nullptr early return rather than touching the bound mock.
    unbound_write_32(el2_pic_basetest::meipl_offset(1), 5u);
    unbound_write_32(el2_pic_basetest::meie_offset(1), 0x1u);
    wait(SC_ZERO_TIME);
    if (mock_hart.eip_asserted) {
        pass = false;
        reason += "null-hart instance drove the bound mock hart; ";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-019: test_threshold_and_reserved_source");
    else
        report_test_fail("FUNC-EL2PIC-019: test_threshold_and_reserved_source", reason);
}

// ===========================================================================
// sc_main
// ---------------------------------------------------------------------------
// FUNC-EL2PIC-018: Malformed generic payloads
//
// The PIC inherits its blocking transport from regmodel::Memory, so this is as
// much a check on the shared register file as on the PIC. What it asserts is
// the part that holds regardless of decode policy: every defect gets a decided
// response, none of them crash the model, and none of them disturb a register
// they were not aimed at.
// ---------------------------------------------------------------------------
void testbench::test_malformed_payloads()
{
    const std::string test_name = "FUNC-EL2PIC-018: Malformed Generic Payloads";

    // MEIPL[1] is writable (4-bit priority) and is the target of the matrix.
    // MEIE[2] is the untouched neighbour used as a corruption witness.
    const unsigned meipl1 = el2_pic::OFFS_MEIPL_BASE + 1 * sizeof(uint32_t);
    const unsigned meie2  = el2_pic::OFFS_MEIE_BASE  + 2 * sizeof(uint32_t);

    m_test->reg_write_32(meipl1, 0xF);
    m_test->reg_write_32(meie2, 0x1);

    simtlm::target_geometry geo;
    geo.valid_address  = meipl1;
    geo.word_bytes     = sizeof(uint32_t);
    geo.aperture_bytes = el2_pic::MEM_SIZE_BYTES;

    for (simtlm::defect d : simtlm::all_defects()) {
        for (tlm::tlm_command cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            const auto r = m_test->probe(d, geo, cmd);
            if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                std::stringstream ss;
                ss << simtlm::defect_name(d) << " ("
                   << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                   << ") left the payload INCOMPLETE";
                report_test_fail(test_name, ss.str());
                return;
            }
        }
    }

    // The witness must be untouched: none of the defects addressed it.
    const uint32_t witness = m_test->reg_read_32(meie2);
    if (witness != 0x1) {
        std::stringstream ss;
        ss << "MEIE[2] corrupted by malformed traffic: expected 0x1, read 0x"
           << std::hex << witness;
        report_test_fail(test_name, ss.str());
        return;
    }

    // Well-formed access must still work afterwards: a rejected payload must not
    // leave the register file wedged.
    m_test->reg_write_32(meipl1, 0x7);
    const uint32_t after = m_test->reg_read_32(meipl1);
    if (after != 0x7) {
        std::stringstream ss;
        ss << "MEIPL[1] unusable after malformed traffic: expected 0x7, read 0x"
           << std::hex << after;
        report_test_fail(test_name, ss.str());
        return;
    }

    report_test_pass(test_name);
}

// ===========================================================================
// FUNC-EL2PIC-020 (PIC-F-01): Equal-priority tie break
//
// reevaluate_arbitration() scans sources 1..255 ascending and keeps the
// incumbent on a tie (`eff_prio > best_eff_prio`, strictly), so at equal
// nonzero priority the lower source ID wins no matter which pin moved first.
// FUNC-EL2PIC-006 and -016 both use unequal priorities, so a descending scan
// or a `>=` comparison passes them and only shows up here.
// ===========================================================================
void testbench::test_equal_priority_tie_break()
{
    bool pass = true;
    std::string reason;
    const unsigned lo   = 20;
    const unsigned hi   = 21;
    const unsigned prio = 5;

    for (unsigned s : {lo, hi}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u);  // level, active-high
        m_test->reg_write_32(el2_pic_basetest::meipl_offset(s), prio);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    wait(1, sc_core::SC_NS);

    auto check_winner = [&](const char* order) {
        if (!mock_hart.eip_asserted) {
            pass = false; reason += std::string(order) + ": no EIP; ";
            return;
        }
        if (mock_hart.claim_id != lo) {
            pass = false;
            reason += std::string(order) + ": claim=" + std::to_string(mock_hart.claim_id) +
                      " expected " + std::to_string(lo) + "; ";
        }
        if (mock_hart.meicidpl != prio) {
            pass = false;
            reason += std::string(order) + ": meicidpl=" + std::to_string(mock_hart.meicidpl) +
                      " expected " + std::to_string(prio) + "; ";
        }
        const uint32_t meip = m_test->reg_read_32(el2_pic_basetest::meip_offset(0));
        if (meip != ((1u << lo) | (1u << hi))) {
            std::ostringstream oss;
            oss << order << ": meip[0]=0x" << std::hex << meip << " both sources not pending; ";
            pass = false; reason += oss.str();
        }
    };

    auto both_low = [&]() {
        m_test->drive_irq(lo, false);
        m_test->drive_irq(hi, false);
        wait(1, sc_core::SC_NS);
    };

    m_test->drive_irq(lo, true);
    wait(1, sc_core::SC_NS);
    m_test->drive_irq(hi, true);
    wait(1, sc_core::SC_NS);
    check_winner("lo-then-hi");

    both_low();

    // The interesting order: hi already owns the claim when lo arrives, so the
    // tie has to be re-decided in lo's favour rather than left alone.
    m_test->drive_irq(hi, true);
    wait(1, sc_core::SC_NS);
    if (mock_hart.claim_id != hi) {
        pass = false;
        reason += "hi did not claim while alone (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }
    m_test->drive_irq(lo, true);
    wait(1, sc_core::SC_NS);
    check_winner("hi-then-lo");

    both_low();

    m_test->drive_irq(lo, true);
    m_test->drive_irq(hi, true);
    wait(1, sc_core::SC_NS);
    check_winner("same-delta");

    both_low();

    if (pass)
        report_test_pass("FUNC-EL2PIC-020: test_equal_priority_tie_break");
    else
        report_test_fail("FUNC-EL2PIC-020: test_equal_priority_tie_break", reason);
}

// ===========================================================================
// FUNC-EL2PIC-021 (PIC-F-02): Winner deasserts, claim falls back, EIP holds
//
// The retarget branch only calls set_pic_claim_id()/trigger_external_interrupt()
// and leaves eip_asserted_ alone, so the hart must never see the line drop
// between two back-to-back interrupts. Final state cannot show that, hence the
// clear_count tally: a clear-then-retrigger implementation ends in the same
// place but glitches the line.
// ===========================================================================
void testbench::test_winner_fallback_no_eip_glitch()
{
    bool pass = true;
    std::string reason;
    const unsigned lo = 22;   // priority 3
    const unsigned hi = 23;   // priority 9

    for (unsigned s : {lo, hi}) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(s), 0x0u);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(s), 0x1u);
    }
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(lo), 3u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(hi), 9u);
    wait(1, sc_core::SC_NS);

    m_test->drive_irq(lo, true);
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != lo) {
        pass = false;
        reason += "low-priority source did not claim first (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }

    m_test->drive_irq(hi, true);
    wait(1, sc_core::SC_NS);
    if (mock_hart.claim_id != hi || mock_hart.meicidpl != 9u) {
        pass = false;
        reason += "high-priority source did not take the claim (claim=" +
                  std::to_string(mock_hart.claim_id) + " meicidpl=" +
                  std::to_string(mock_hart.meicidpl) + "); ";
    }

    const unsigned clears_before = mock_hart.clear_count;

    // Drop the winner. lo is still pending and enabled, so the claim retargets
    // downwards and the line stays up throughout.
    m_test->drive_irq(hi, false);
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "EIP dropped on fallback to the remaining source; ";
    }
    if (mock_hart.claim_id != lo) {
        pass = false;
        reason += "claim did not fall back to " + std::to_string(lo) + " (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }
    if (mock_hart.meicidpl != 3u) {
        pass = false;
        reason += "meicidpl=" + std::to_string(mock_hart.meicidpl) +
                  " did not follow the fallback winner; ";
    }
    if (mock_hart.clear_count != clears_before) {
        pass = false;
        reason += "EIP was cleared " + std::to_string(mock_hart.clear_count - clears_before) +
                  " time(s) during the retarget; ";
    }
    if ((m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & (1u << hi)) != 0) {
        pass = false; reason += "deasserted winner still pending; ";
    }

    // Only when the last source goes away may the line drop, exactly once.
    m_test->drive_irq(lo, false);
    wait(1, sc_core::SC_NS);
    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP still asserted with nothing pending; ";
    }
    if (mock_hart.clear_count != clears_before + 1) {
        pass = false;
        reason += "expected exactly one clear at the end, saw " +
                  std::to_string(mock_hart.clear_count - clears_before) + "; ";
    }
    if (mock_hart.claim_id != 0) {
        pass = false; reason += "claim_id not released; ";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-021: test_winner_fallback_no_eip_glitch");
    else
        report_test_fail("FUNC-EL2PIC-021: test_winner_fallback_no_eip_glitch", reason);
}

// ===========================================================================
// FUNC-EL2PIC-022 (PIC-F-03): Active-low edge gateway
//
// Polarity and latching interact: the latch must set on the polarity-adjusted
// level, not on the raw pin, and a clear must stick only while that adjusted
// level is inactive. FUNC-EL2PIC-005/013 cover edge mode active-high only, so
// a gateway that latches the raw pin passes both of them and fails here.
// ===========================================================================
void testbench::test_edge_gateway_active_low()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 24;
    const uint32_t bit = 1u << src;

    auto meip0 = [&]() { return m_test->reg_read_32(el2_pic_basetest::meip_offset(0)); };

    auto check = [&](const char* step, bool want_eip, bool want_pending) {
        if (mock_hart.eip_asserted != want_eip) {
            pass = false;
            reason += std::string(step) + ": EIP=" + (mock_hart.eip_asserted ? "1" : "0") +
                      " expected " + (want_eip ? "1" : "0") + "; ";
        }
        const bool pending = (meip0() & bit) != 0;
        if (pending != want_pending) {
            pass = false;
            reason += std::string(step) + ": pending=" + (pending ? "1" : "0") +
                      " expected " + (want_pending ? "1" : "0") + "; ";
        }
    };

    // Under active-low polarity the inactive level is a high pin. Reaching that
    // state through level mode first leaves the latch provably clear, so the
    // sequence below starts from a known-empty gateway.
    m_test->drive_irq(src, true);
    wait(1, sc_core::SC_NS);
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x1u);  // level, active-low
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x3u);  // edge,  active-low
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 6u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);
    check("armed", false, false);

    // Pin low is the asserted level here: the latch sets.
    m_test->drive_irq(src, false);
    wait(1, sc_core::SC_NS);
    check("asserted", true, true);
    if (mock_hart.claim_id != src) {
        pass = false;
        reason += "claim=" + std::to_string(mock_hart.claim_id) + " expected " +
                  std::to_string(src) + "; ";
    }

    // Pin back high: inactive, but the latch is sticky.
    m_test->drive_irq(src, true);
    wait(1, sc_core::SC_NS);
    check("deasserted-sticky", true, true);

    // Any write clears; the value carries no meaning (write_MEIGWCLR ignores it).
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x0u);
    wait(1, sc_core::SC_NS);
    check("cleared-with-zero", false, false);

    // Reassert, then clear while still active: the clear cannot stick.
    m_test->drive_irq(src, false);
    wait(1, sc_core::SC_NS);
    check("reasserted", true, true);

    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0xFFFFFFFFu);
    wait(1, sc_core::SC_NS);
    check("clear-while-active", true, true);

    // Return the source to its inactive level; the latch survives, as in 013.
    m_test->drive_irq(src, true);
    wait(1, sc_core::SC_NS);
    check("inactive-latch-held", true, true);

    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x2u);
    wait(1, sc_core::SC_NS);
    check("cleared-with-two", false, false);

    // Repeated clears of an already-clear gateway are idempotent.
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);
    check("repeated-clear", false, false);

    if (pass)
        report_test_pass("FUNC-EL2PIC-022: test_edge_gateway_active_low");
    else
        report_test_fail("FUNC-EL2PIC-022: test_edge_gateway_active_low", reason);
}

// ===========================================================================
// FUNC-EL2PIC-023 (PIC-F-04): MEIGWCTRL polarity/type changed under a live pin
//
// post_write_MEIGWCTRL recomputes the source against the current input rather
// than waiting for the next pin event, so every write below has an exact
// MEIP/EIP consequence in the same delta. The level→edge transitions also pin
// down that a reconfigure does not clear an already-set latch.
// ===========================================================================
void testbench::test_gateway_reconfig_while_asserted()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 25;
    const uint32_t bit = 1u << src;

    auto reconfigure = [&](uint32_t ctrl, const char* step, bool want_eip, bool want_pending) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), ctrl);
        wait(1, sc_core::SC_NS);

        const uint32_t readback = m_test->reg_read_32(el2_pic_basetest::meigwctrl_offset(src));
        if ((readback & el2_pic_basetest::MEIGWCTRL_READ_MASK) != ctrl) {
            std::ostringstream oss;
            oss << step << ": MEIGWCTRL read back 0x" << std::hex << readback
                << " after writing 0x" << ctrl << "; ";
            pass = false; reason += oss.str();
        }
        if (mock_hart.eip_asserted != want_eip) {
            pass = false;
            reason += std::string(step) + ": EIP=" + (mock_hart.eip_asserted ? "1" : "0") +
                      " expected " + (want_eip ? "1" : "0") + "; ";
        }
        const bool pending =
            (m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & bit) != 0;
        if (pending != want_pending) {
            pass = false;
            reason += std::string(step) + ": pending=" + (pending ? "1" : "0") +
                      " expected " + (want_pending ? "1" : "0") + "; ";
        }
    };

    m_test->drive_irq(src, true);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 9u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src || mock_hart.meicidpl != 9u) {
        pass = false;
        reason += "precondition: level active-high source did not claim (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }

    // Polarity flip against a high pin makes the effective level inactive, so
    // level mode must drop the interrupt on the spot.
    reconfigure(0x1u, "level-active-low", false, false);
    if (mock_hart.claim_id != 0) {
        pass = false; reason += "claim not released when polarity masked the source; ";
    }

    reconfigure(0x0u, "level-active-high", true, true);
    if (mock_hart.claim_id != src || mock_hart.meicidpl != 9u) {
        pass = false;
        reason += "claim/meicidpl not restored on the polarity flip back; ";
    }

    // Level → edge with the pin still asserted latches immediately.
    reconfigure(0x2u, "edge-active-high", true, true);

    // Edge + polarity flip: the effective level goes inactive, but a set latch
    // is cleared only by MEIGWCLR, so the source stays pending.
    reconfigure(0x3u, "edge-active-low", true, true);

    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);
    if (mock_hart.eip_asserted ||
        (m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & bit) != 0) {
        pass = false; reason += "MEIGWCLR did not clear the inactive edge latch; ";
    }

    // Back to edge active-high with the pin unchanged: the latch resets.
    reconfigure(0x2u, "edge-active-high-relatch", true, true);

    // Edge → level while asserted keeps pending; dropping the pin now clears it
    // without a MEIGWCLR, which is the whole difference between the two modes.
    reconfigure(0x0u, "edge-to-level", true, true);
    m_test->drive_irq(src, false);
    wait(1, sc_core::SC_NS);
    if (mock_hart.eip_asserted ||
        (m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & bit) != 0) {
        pass = false; reason += "level mode did not follow the pin down; ";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-023: test_gateway_reconfig_while_asserted");
    else
        report_test_fail("FUNC-EL2PIC-023: test_gateway_reconfig_while_asserted", reason);
}

// ===========================================================================
// FUNC-EL2PIC-024: Threshold boundaries in both priority-order modes
//
// The compare is strict on both CSRs (`prio > meipt_eff && prio > meicurpl_eff`),
// and meipt/meicurpl are inverted by the same ~x&0xF as intpriority when
// priord=1. Sweeping effective thresholds 0 / W-1 / W / W+1 / 15 against a
// fixed effective winner W in both modes and on both CSRs pins down the
// off-by-one and the inversion together: a `>=` compare flips the W case, and a
// missing threshold inversion flips every priord=1 case.
//
// Driven through notify_threshold_changed() and the mock hart's CSR fields.
// PIC-I-01 — the same sweep through the production VeeR post-CSR callback —
// stays open: this build substitutes test/inc/VeeR-ISSTlm.hpp for the real ISS
// wrapper (CMakeLists.txt:144-193), so there is no CSR write path to drive.
// ===========================================================================
void testbench::test_threshold_boundaries_both_modes()
{
    bool pass = true;
    std::string reason;
    const unsigned src = 26;
    const unsigned W   = 8;   // effective winner priority, same in both modes

    struct Boundary { unsigned eff_thresh; bool expect_eip; };
    const Boundary boundaries[] = {
        { 0,     true  },
        { W - 1, true  },
        { W,     false },   // strict compare: equal does not interrupt
        { W + 1, false },
        { 15,    false },
    };

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);

    for (unsigned priord = 0; priord <= 1; ++priord) {
        // Hardware inverts priorities and thresholds with the same operation,
        // so a raw value is derived from the effective one the sweep names.
        auto raw_of = [priord](unsigned eff) { return priord ? ((~eff) & 0xFu) : eff; };

        m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), priord);
        m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), raw_of(W));
        mock_hart.meipt        = raw_of(0);
        mock_hart.meicurpl_csr = raw_of(0);
        m_test->drive_irq(src, true);
        wait(1, sc_core::SC_NS);

        for (unsigned csr = 0; csr < 2; ++csr) {
            const char* csr_name = (csr == 0) ? "meipt" : "meicurpl";

            for (const Boundary& b : boundaries) {
                // The CSR not under test is parked at effective 0, the lowest
                // threshold in either mode, so it never masks the result.
                mock_hart.meipt        = raw_of(csr == 0 ? b.eff_thresh : 0);
                mock_hart.meicurpl_csr = raw_of(csr == 1 ? b.eff_thresh : 0);
                m_dut->notify_threshold_changed();
                wait(1, sc_core::SC_NS);

                std::ostringstream tag;
                tag << "priord=" << priord << " " << csr_name
                    << " eff=" << b.eff_thresh;

                if (mock_hart.eip_asserted != b.expect_eip) {
                    pass = false;
                    reason += tag.str() + ": EIP=" +
                              (mock_hart.eip_asserted ? "1" : "0") + " expected " +
                              (b.expect_eip ? "1" : "0") + "; ";
                    continue;
                }
                if (b.expect_eip) {
                    if (mock_hart.claim_id != src) {
                        pass = false;
                        reason += tag.str() + ": claim=" +
                                  std::to_string(mock_hart.claim_id) + "; ";
                    }
                    if (mock_hart.meicidpl != W) {
                        pass = false;
                        reason += tag.str() + ": meicidpl=" +
                                  std::to_string(mock_hart.meicidpl) + " expected " +
                                  std::to_string(W) + "; ";
                    }
                } else if (mock_hart.claim_id != 0) {
                    pass = false;
                    reason += tag.str() + ": claim_id held at " +
                              std::to_string(mock_hart.claim_id) + " while blocked; ";
                }

                // Thresholding gates delivery, never the pending bit itself.
                if ((m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & (1u << src)) == 0) {
                    pass = false;
                    reason += tag.str() + ": threshold cleared the pending bit; ";
                }
            }
        }

        m_test->drive_irq(src, false);
        wait(1, sc_core::SC_NS);
    }

    m_test->reg_write_32(el2_pic_basetest::mpiccfg_offset(), 0x0u);
    mock_hart.meipt        = 0u;
    mock_hart.meicurpl_csr = 0u;
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-024: test_threshold_boundaries_both_modes");
    else
        report_test_fail("FUNC-EL2PIC-024: test_threshold_boundaries_both_modes", reason);
}

// ===========================================================================
// FUNC-EL2PIC-025: MEIP writes ignored on every word; reserved source 0
//
// FUNC-EL2PIC-002 attempts one MEIP write, at word 0, against an all-zero
// bitmap — a write that landed in storage would be indistinguishable from the
// value that was already there. Here every word carries a live pending bit
// first, so both an all-ones and an all-zeros write have something to corrupt.
//
// Source 0 is reserved: its registers are inside the aperture and store
// field-masked values, but register_all_callbacks() skips index 0 and
// gateway_changed() returns early for it, so it can never pend or claim.
// ===========================================================================
void testbench::test_meip_write_ignored_and_reserved_source0()
{
    bool pass = true;
    std::string reason;

    // One source per pending word at a different bit each time, so a stored
    // write cannot coincidentally match the live bitmap.
    struct Case { unsigned src, word, bit; };
    const Case cases[] = {
        {  2, 0,  2 }, {  33, 1,  1 }, {  70, 2,  6 }, { 100, 3,  4 },
        { 130, 4,  2 }, { 170, 5, 10 }, { 210, 6, 18 }, { 255, 7, 31 },
    };

    for (const Case& c : cases) {
        m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(c.src), 0x0u);
        m_test->reg_write_32(el2_pic_basetest::meipl_offset(c.src), 1u);
        m_test->reg_write_32(el2_pic_basetest::meie_offset(c.src), 0x1u);
        m_test->drive_irq(c.src, true);
    }
    wait(1, sc_core::SC_NS);

    auto expected_word = [&](unsigned w) {
        uint32_t e = 0;
        for (const Case& c : cases)
            if (c.word == w) e |= (1u << c.bit);
        return e;
    };

    for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w) {
        if (m_test->reg_read_32(el2_pic_basetest::meip_offset(w)) != expected_word(w)) {
            pass = false;
            reason += "precondition: meip[" + std::to_string(w) + "] not primed; ";
        }
    }

    // All-ones would show up as extra set bits; all-zeros would erase live ones.
    for (uint32_t payload : {0xFFFFFFFFu, 0x00000000u, 0xA5A5A5A5u}) {
        for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w) {
            m_test->reg_write_32(el2_pic_basetest::meip_offset(w), payload);
            wait(SC_ZERO_TIME);

            const uint32_t got = m_test->reg_read_32(el2_pic_basetest::meip_offset(w));
            if (got != expected_word(w)) {
                std::ostringstream oss;
                oss << "meip[" << w << "] became 0x" << std::hex << got
                    << " after writing 0x" << payload
                    << " (expected 0x" << expected_word(w) << "); ";
                pass = false; reason += oss.str();
            }
        }
    }

    // All eight sources share priority 1, so the ascending scan hands the claim
    // to the lowest ID; an MEIP write must not have disturbed that either.
    if (!mock_hart.eip_asserted || mock_hart.claim_id != cases[0].src) {
        pass = false;
        reason += "MEIP writes disturbed the claim (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }

    for (const Case& c : cases)
        m_test->drive_irq(c.src, false);
    wait(1, sc_core::SC_NS);

    // Reserved source 0: writes are accepted and field-masked on readback.
    struct Reserved { unsigned offset; uint32_t expect; const char* name; };
    const Reserved reserved[] = {
        { el2_pic_basetest::meipl_offset(0),     0x0000000Fu, "MEIPL[0]"     },
        { el2_pic_basetest::meie_offset(0),      0x00000001u, "MEIE[0]"      },
        { el2_pic_basetest::meigwctrl_offset(0), 0x00000003u, "MEIGWCTRL[0]" },
    };
    for (const Reserved& r : reserved) {
        m_test->reg_write_32(r.offset, 0xFFFFFFFFu);
        const uint32_t got = m_test->reg_read_32(r.offset);
        if (got != r.expect) {
            std::ostringstream oss;
            oss << r.name << " read back 0x" << std::hex << got
                << " expected 0x" << r.expect << "; ";
            pass = false; reason += oss.str();
        }
    }

    // MEIGWCLR[0] keeps the write-only contract of the rest of the array: the
    // write is accepted, the readback is zero.
    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(0), 0x1u);
    const uint32_t clr0 = m_test->reg_read_32(el2_pic_basetest::meigwclr_offset(0));
    if (clr0 != 0) {
        pass = false;
        reason += "MEIGWCLR[0] read back 0x" + std::to_string(clr0) + "; ";
    }

    // Now give source 0 the configuration that would make it the top
    // arbitration candidate if it were real: level, active-high, priority 15,
    // enabled. The mask writes above left it edge/active-low, which is inert on
    // a rising pin for reasons that have nothing to do with source 0 being
    // reserved, so the negative result would not have meant anything.
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(0), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(0), 0xFu);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(0), 0x1u);
    m_test->drive_irq(0, true);
    wait(1, sc_core::SC_NS);
    if ((m_test->reg_read_32(el2_pic_basetest::meip_offset(0)) & 1u) != 0) {
        pass = false; reason += "reserved source 0 set meip[0] bit 0; ";
    }
    // Source 0 is guarded twice over, and only the pending check above can
    // actually fail today: reevaluate_arbitration() uses best_id == 0 as its
    // "no winner" sentinel, so source 0 winning is already indistinguishable
    // from nothing winning. Kept as a guard for the day that sentinel is
    // replaced by an explicit flag.
    if (mock_hart.eip_asserted) {
        pass = false; reason += "reserved source 0 asserted EIP; ";
    }
    m_test->drive_irq(0, false);

    m_test->reg_write_32(el2_pic_basetest::meipl_offset(0), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(0), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(0), 0x0u);
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-025: test_meip_write_ignored_and_reserved_source0");
    else
        report_test_fail("FUNC-EL2PIC-025: test_meip_write_ignored_and_reserved_source0", reason);
}

// ===========================================================================
// FUNC-EL2PIC-026 (PIC-F-05): Reset with several edge latches pending
//
// FUNC-EL2PIC-009 resets one source whose pin is still high, so the post-reset
// zero is also what recomputing from the pin would give. Edge latches are the
// harder case: they are pending with every pin already low, so only
// source_pending_.fill(false) in reset_process() can clear them, and a reset
// that only reset the register block would leave four sources pending across
// four different words.
// ===========================================================================
void testbench::test_reset_with_pending_edge_latches()
{
    bool pass = true;
    std::string reason;
    const unsigned srcs[]  = {  27,  60, 130, 240 };
    const unsigned prios[] = {   4,   9,   6,   2 };
    const unsigned winner  = 60;   // priority 9 is the highest of the four

    auto arm_edge_sources = [&]() {
        for (unsigned i = 0; i < 4; ++i) {
            m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(srcs[i]), 0x2u);
            m_test->reg_write_32(el2_pic_basetest::meipl_offset(srcs[i]), prios[i]);
            m_test->reg_write_32(el2_pic_basetest::meie_offset(srcs[i]), 0x1u);
        }
        wait(1, sc_core::SC_NS);
    };

    auto count_pending_words = [&]() {
        unsigned n = 0;
        for (unsigned w = 0; w < el2_pic::NUM_PEND_WORDS; ++w)
            if (m_test->reg_read_32(el2_pic_basetest::meip_offset(w)) != 0) ++n;
        return n;
    };

    arm_edge_sources();

    // Pulse every source, then release every pin: the latches hold the pending
    // state with nothing driving it.
    for (unsigned s : srcs) m_test->drive_irq(s, true);
    wait(1, sc_core::SC_NS);
    for (unsigned s : srcs) m_test->drive_irq(s, false);
    wait(1, sc_core::SC_NS);

    if (!mock_hart.eip_asserted || mock_hart.claim_id != winner) {
        pass = false;
        reason += "precondition: latched winner not claimed (claim=" +
                  std::to_string(mock_hart.claim_id) + "); ";
    }
    if (count_pending_words() != 4) {
        pass = false;
        reason += "precondition: expected 4 pending words, saw " +
                  std::to_string(count_pending_words()) + "; ";
    }

    const unsigned clears_before = mock_hart.clear_count;

    m_test->assert_reset();
    wait(10, SC_NS);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "EIP survived reset; ";
    }
    if (mock_hart.claim_id != 0) {
        pass = false; reason += "claim_id survived reset; ";
    }
    if (mock_hart.clear_count != clears_before + 1) {
        pass = false;
        reason += "expected exactly one clear from reset, saw " +
                  std::to_string(mock_hart.clear_count - clears_before) + "; ";
    }
    if (count_pending_words() != 0) {
        pass = false;
        reason += std::to_string(count_pending_words()) +
                  " pending word(s) survived reset; ";
    }
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned s = srcs[i];
        if (m_test->reg_read_32(el2_pic_basetest::meipl_offset(s)) != 0 ||
            m_test->reg_read_32(el2_pic_basetest::meie_offset(s)) != 0 ||
            m_test->reg_read_32(el2_pic_basetest::meigwctrl_offset(s)) != 0) {
            pass = false;
            reason += "source " + std::to_string(s) + " config survived reset; ";
        }
    }

    m_test->deassert_reset();
    wait(10, SC_NS);
    if (mock_hart.eip_asserted || count_pending_words() != 0) {
        pass = false; reason += "state came back on reset release; ";
    }

    // Second half: a pin edge and the reset land in the same delta, so
    // gateway_changed() and reset_process() are both runnable with no defined
    // order between them. MEIP is deliberately not asserted at that instant —
    // a gateway running after reset_process legitimately re-pends off a pin
    // that is still high, matching the RTL's combinational intpend. What must
    // hold either way is that nothing reaches the hart, because reset also
    // cleared MEIE.
    arm_edge_sources();
    for (unsigned s : srcs) m_test->drive_irq(s, true);
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted) {
        pass = false; reason += "precondition: no EIP before the same-delta reset; ";
    }

    const unsigned race_src = 28;   // arrives in the same delta as the reset
    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(race_src), 0x0u);
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(race_src), 15u);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(race_src), 0x1u);
    wait(1, sc_core::SC_NS);

    m_test->drive_irq(race_src, true);
    m_test->assert_reset();
    wait(10, SC_NS);

    if (mock_hart.eip_asserted) {
        pass = false; reason += "same-delta reset left EIP asserted; ";
    }
    if (mock_hart.claim_id != 0) {
        pass = false; reason += "same-delta reset left a claim_id; ";
    }
    if (m_test->reg_read_32(el2_pic_basetest::meipl_offset(race_src)) != 0 ||
        m_test->reg_read_32(el2_pic_basetest::meie_offset(race_src)) != 0) {
        pass = false; reason += "same-delta reset did not clear the racing source's config; ";
    }

    m_test->deassert_reset();
    wait(10, SC_NS);

    // With every pin back low the pending map must be empty regardless of how
    // the race resolved.
    for (unsigned s : srcs) m_test->drive_irq(s, false);
    m_test->drive_irq(race_src, false);
    wait(1, sc_core::SC_NS);
    if (count_pending_words() != 0 || mock_hart.eip_asserted) {
        pass = false; reason += "pending map not empty after the race settled; ";
    }

    if (pass)
        report_test_pass("FUNC-EL2PIC-026: test_reset_with_pending_edge_latches");
    else
        report_test_fail("FUNC-EL2PIC-026: test_reset_with_pending_edge_latches", reason);
}

// ===========================================================================
// FUNC-EL2PIC-027 (PIC-T-05): transport_dbg and DMI policy
//
// Both are asserted rather than assumed. Two policies are being recorded here,
// and they belong to the shared register file (common/include/reg_file.h),
// not to the PIC:
//
//   - transport_dbg() routes through the same read_registers()/write_registers()
//     as b_transport, so a debug *read* is side-effect free only because the
//     read callbacks are, while a debug *write* does run post-write side
//     effects. That is not the "no side effects" a strict reading of TLM-2.0
//     would expect from a debug transaction, so it is asserted explicitly and
//     flagged rather than left implied.
//   - The PIC registers no DMI callback, so get_direct_mem_ptr must refuse.
//     A register file backed by callbacks has no window it could hand out.
// ===========================================================================
void testbench::test_debug_transport_and_dmi()
{
    bool pass = true;
    std::string reason;
    const unsigned src  = 29;
    const unsigned word = src / 32;
    const uint32_t bit  = 1u << (src % 32);

    m_test->reg_write_32(el2_pic_basetest::meigwctrl_offset(src), 0x2u);  // edge, active-high
    m_test->reg_write_32(el2_pic_basetest::meipl_offset(src), 0xAu);
    m_test->reg_write_32(el2_pic_basetest::meie_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);

    // Pulse the source so the pending bit is held by the latch, not the pin.
    m_test->drive_irq(src, true);
    wait(1, sc_core::SC_NS);
    m_test->drive_irq(src, false);
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src) {
        pass = false; reason += "precondition: latched source did not claim; ";
    }

    auto dbg_read = [&](unsigned offset, uint32_t& out) {
        out = 0;
        return simtlm::debug_read(m_test->initiator_socket, offset,
                                  reinterpret_cast<unsigned char*>(&out), sizeof(out));
    };

    uint32_t v = 0;
    unsigned n = dbg_read(el2_pic_basetest::meipl_offset(src), v);
    if (n != sizeof(uint32_t)) {
        pass = false;
        reason += "debug read of MEIPL returned " + std::to_string(n) + " bytes; ";
    }
    if ((v & el2_pic_basetest::MEIPL_READ_MASK) != 0xAu) {
        std::ostringstream oss;
        oss << "debug read of MEIPL gave 0x" << std::hex << v << "; ";
        pass = false; reason += oss.str();
    }

    // MEIP has no backing storage — pending_word() computes it per read — so a
    // debug read that bypassed the read callbacks would return a stale zero.
    n = dbg_read(el2_pic_basetest::meip_offset(word), v);
    if (n != sizeof(uint32_t) || v != bit) {
        std::ostringstream oss;
        oss << "debug read of meip[" << word << "] returned " << n << " bytes, 0x"
            << std::hex << v << " (expected 0x" << bit << "); ";
        pass = false; reason += oss.str();
    }

    // Write-only register on the debug path too, and reading it must not clear
    // the latch it controls.
    n = dbg_read(el2_pic_basetest::meigwclr_offset(src), v);
    if (n != sizeof(uint32_t) || v != 0) {
        std::ostringstream oss;
        oss << "debug read of MEIGWCLR returned " << n << " bytes, 0x"
            << std::hex << v << " (expected 0); ";
        pass = false; reason += oss.str();
    }
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src) {
        pass = false; reason += "debug reads disturbed the interrupt state; ";
    }

    // A debug access the register file refuses must report zero bytes moved,
    // not the length it was handed.
    uint32_t sink = 0;
    n = simtlm::debug_read(m_test->initiator_socket, el2_pic_basetest::meipl_offset(src),
                           reinterpret_cast<unsigned char*>(&sink), 0);
    if (n != 0) {
        pass = false;
        reason += "zero-length debug read claimed " + std::to_string(n) + " bytes; ";
    }

    // Debug writes share the b_transport side-effect path: disabling the source
    // through transport_dbg has to drop the interrupt.
    uint32_t off = 0x0u;
    n = simtlm::debug_write(m_test->initiator_socket, el2_pic_basetest::meie_offset(src),
                            reinterpret_cast<unsigned char*>(&off), sizeof(off));
    wait(1, sc_core::SC_NS);
    if (n != sizeof(uint32_t)) {
        pass = false;
        reason += "debug write returned " + std::to_string(n) + " bytes; ";
    }
    if (mock_hart.eip_asserted) {
        pass = false; reason += "debug write of MEIE=0 did not run the post-write side effect; ";
    }

    uint32_t on = 0x1u;
    simtlm::debug_write(m_test->initiator_socket, el2_pic_basetest::meie_offset(src),
                        reinterpret_cast<unsigned char*>(&on), sizeof(on));
    wait(1, sc_core::SC_NS);
    if (!mock_hart.eip_asserted || mock_hart.claim_id != src) {
        pass = false; reason += "debug write of MEIE=1 did not restore the claim; ";
    }

    // DMI must be refused, for both commands and on both DUT instances.
    struct DmiProbe { const char* who; simtlm::dmi_result r; };
    const DmiProbe probes[] = {
        { "dut read",  simtlm::dmi_request(m_test->initiator_socket,
                                           el2_pic_basetest::meipl_offset(src),
                                           tlm::TLM_READ_COMMAND) },
        { "dut write", simtlm::dmi_request(m_test->initiator_socket,
                                           el2_pic_basetest::meipl_offset(src),
                                           tlm::TLM_WRITE_COMMAND) },
        { "unbound read", simtlm::dmi_request(unbound_isock,
                                              el2_pic_basetest::meipl_offset(1),
                                              tlm::TLM_READ_COMMAND) },
    };
    for (const DmiProbe& p : probes) {
        if (p.r.granted) {
            pass = false;
            reason += std::string("DMI granted to ") + p.who + "; ";
        }
    }

    // A normal blocking access must also clear any stale dmi_allowed hint it
    // was handed, which is what b_transport's set_dmi_allowed(false) is for.
    uint32_t probe_val = 0;
    const auto rd = simtlm::read_word<uint32_t>(m_test->initiator_socket,
                                                el2_pic_basetest::meipl_offset(src),
                                                probe_val);
    if (!rd.ok()) {
        pass = false;
        reason += std::string("post-DMI read failed: ") + simtlm::response_name(rd.status) + "; ";
    }
    if (rd.dmi_allowed) {
        pass = false; reason += "b_transport left dmi_allowed set; ";
    }

    m_test->reg_write_32(el2_pic_basetest::meigwclr_offset(src), 0x1u);
    wait(1, sc_core::SC_NS);

    if (pass)
        report_test_pass("FUNC-EL2PIC-027: test_debug_transport_and_dmi");
    else
        report_test_fail("FUNC-EL2PIC-027: test_debug_transport_and_dmi", reason);
}

// ===========================================================================
int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");

    REG_INFO(1, tb.logger) << "Starting EL2 PIC Testbench" << std::endl;
    sc_start();
    REG_INFO(1, tb.logger) << "Simulation completed" << std::endl;

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    // Use quick_exit to avoid CCI broker destructor crash
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);

    return 0;
}
