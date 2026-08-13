#include "testbench.h"
#include "el2_pic_basetest.h"
#include "csml_parameter.h"
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
    tlm::tlm_generic_payload trans;
    uint32_t data = 0;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(byte_offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    unbound_isock->b_transport(trans, delay);
    return data;
}

// ---------------------------------------------------------------------------
// Reporting helpers
// ---------------------------------------------------------------------------
void testbench::report_test_start(const std::string& test_name)
{
    CSML_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================" << std::endl;
}

void testbench::report_test_pass(const std::string& test_name)
{
    m_tests_passed++;
    m_tests_run++;
    CSML_INFO(1, logger) << test_name << ": PASS" << std::endl;
}

void testbench::report_test_fail(const std::string& test_name, const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    CSML_WARN(1, logger) << test_name << ": FAIL - " << reason << std::endl;
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
    CSML_INFO(1, logger) << ss.str() << std::endl;
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

    CSML_INFO(1, logger) << "\n========================================"
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

    // FUNC-EL2PIC-012: Unbound sources tied low
    report_test_start("FUNC-EL2PIC-012: Unbound Sources Tied Low");
    test_unbound_sources_tied_low();

    // ------------------------------------------------------------------
    // Final summary
    // ------------------------------------------------------------------
    CSML_INFO(1, logger) << "\n========================================"
                         << "       TEST SUITE SUMMARY"
                         << "========================================" << std::endl;

    std::stringstream ss;
    ss << "Total Tests:  " << m_tests_run;
    CSML_INFO(1, logger) << ss.str() << std::endl;
    ss.str(""); ss << "Passed:       " << m_tests_passed << " (PASS)";
    CSML_INFO(1, logger) << ss.str() << std::endl;
    ss.str(""); ss << "Failed:       " << m_tests_failed << " (FAIL)";
    CSML_INFO(1, logger) << ss.str() << std::endl;
    if (m_tests_run > 0) {
        double rate = 100.0 * m_tests_passed / m_tests_run;
        ss.str(""); ss << "Success Rate: " << std::fixed << std::setprecision(1) << rate << "%";
        CSML_INFO(1, logger) << ss.str() << std::endl;
    }
    if (m_tests_failed > 0) {
        CSML_ERROR(0, logger) << "\nFailed Tests:" << std::endl;
        for (const auto& t : m_failed_tests) {
            CSML_ERROR(0, logger) << "  - " << t << std::endl;
        }
        CSML_ERROR(0, logger) << "[OVERALL RESULT: FAILED]" << std::endl;
    } else if (m_tests_passed > 0) {
        CSML_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]" << std::endl;
    } else {
        CSML_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]" << std::endl;
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
// sc_main
// ===========================================================================
int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");

    CSML_INFO(1, tb.logger) << "Starting EL2 PIC Testbench" << std::endl;
    sc_start();
    CSML_INFO(1, tb.logger) << "Simulation completed" << std::endl;

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    // Use quick_exit to avoid CCI broker destructor crash
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);

    return 0;
}
