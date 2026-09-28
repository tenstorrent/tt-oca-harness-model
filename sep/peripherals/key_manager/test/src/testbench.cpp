// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "reg_param.h"
#include "tlm_probe.h"

#include <iostream>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// Forward declarations of test functions
extern int key_manager_func001_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func002_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func003_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func004_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func005_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func006_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func007_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func008_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func009_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func010_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func011_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func012_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func013_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_func014_test(key_manager_test*, key_manager_model*, testbench*);
extern int key_manager_crc_anchor_test();

testbench::testbench(sc_module_name name)
  : sc_module(name),
    rst_n_signal  ("rst_n_signal",  true),
    wipe_n_signal ("wipe_n_signal", true),   // de-asserted (high) by default
    irq_signal    ("irq_signal",   false),
    hmac_stub     ("hmac_stub"),
    kmac_stub     ("kmac_stub"),
    aes_stub      ("aes_stub"),
    otbn_stub     ("otbn_stub"),
    abr_mldsa_seed_stub("abr_mldsa_seed_stub"),
    abr_mlkem_d_stub   ("abr_mlkem_d_stub"),
    abr_mlkem_z_stub   ("abr_mlkem_z_stub"),
    abr_mlkem_msg_stub ("abr_mlkem_msg_stub")
{
    // Instantiate DUT and test harness
    m_dut  = std::make_unique<key_manager_model>("keymgr_tt");
    m_test = std::make_unique<key_manager_test> ("key_manager_test");

    // Bind TLM sockets: test initiator → DUT mailbox, the only SEP-visible port
    m_test->initiator_socket.bind(m_dut->mailbox_socket);

    // Bind DUT crypto-engine initiator sockets → recording stubs
    m_dut->hmac_key_socket.bind(hmac_stub.socket);
    m_dut->kmac_key_socket.bind(kmac_stub.socket);
    m_dut->aes_key_socket .bind(aes_stub.socket);
    m_dut->otbn_key_socket.bind(otbn_stub.socket);
    m_dut->abr_mldsa_seed_socket.bind(abr_mldsa_seed_stub.socket);
    m_dut->abr_mlkem_d_socket.bind(abr_mlkem_d_stub.socket);
    m_dut->abr_mlkem_z_socket.bind(abr_mlkem_z_stub.socket);
    m_dut->abr_mlkem_msg_socket.bind(abr_mlkem_msg_stub.socket);

    // Wire reset, wipe and IRQ signals
    m_dut ->rst_ni .bind(rst_n_signal);
    m_dut ->wipe_ni.bind(wipe_n_signal);
    m_dut ->irq    .bind(irq_signal);
    m_test->rst_no .bind(rst_n_signal);
    m_test->irq_i  .bind(irq_signal);

    SC_THREAD(run_tests);
}


namespace {

// Malformed generic payloads against the key manager's mailbox CSR window.
//
// MB_IRQEN (0x14) is the target: it is plain read/write, unlike WDATA/WSEP/CTRL
// which push data or commands into the firmware handler. What each defect
// should return is decode policy, so this only asserts the invariant part —
// every defect gets a decided response, nothing crashes, and the window is
// still usable afterwards.
int run_malformed_payload_test(key_manager_test* test)
{
    constexpr unsigned APERTURE  = 0x1C;  // seven 32-bit mailbox registers
    constexpr unsigned OFF_IRQEN = key_manager_basetest::MB_IRQEN_OFFSET;

    int failures = 0;

    simtlm::target_geometry geo;
    geo.valid_address  = OFF_IRQEN;
    geo.word_bytes     = 4;
    geo.aperture_bytes = APERTURE;

    for (simtlm::defect d : simtlm::all_defects()) {
        for (tlm::tlm_command cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            const auto r = test->probe(d, geo, cmd);
            if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                std::cout << "[FAIL] MALFORMED: " << simtlm::defect_name(d) << " ("
                          << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                          << ") left the payload INCOMPLETE" << std::endl;
                ++failures;
            }
        }
    }

    // The mailbox window must still work after being fed bad payloads.
    test->clear_transport_failures();
    test->register_write_32(OFF_IRQEN, 0x3u);
    uint32_t back = 0xFFFFFFFFu;
    test->register_read_32(OFF_IRQEN, back);
    if (test->transport_failures() != 0) {
        std::cout << "[FAIL] MALFORMED: mailbox window unusable afterwards: "
                  << test->last_transport_error() << std::endl;
        ++failures;
    }

    if (failures == 0)
        std::cout << "[PASS] MALFORMED: all generic-payload defects handled" << std::endl;
    return failures;
}

}  // namespace

void testbench::run_tests()
{
    int pass = 0, fail = 0;
    key_manager_test*  test = m_test.get();
    key_manager_model* dut  = m_dut.get();

    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "  KeyMgr TT Testbench";
    REG_INFO(1, logger) << "========================================";

    // Apply reset before first test; each subsequent FUNC test resets itself.
    test->trigger_reset();
    test->clear_transport_failures();

    // A FUNC test only counts as a pass if it both returned 0 and completed
    // without a refused register access. Gating here means each existing test
    // becomes transport-sensitive without being edited.
    auto graded = [&](int rc, const char* which) {
        const unsigned tf = test->transport_failures();
        if (tf != 0) {
            REG_WARN(1, logger) << which << ": " << tf
                                 << " transport error(s); last: "
                                 << test->last_transport_error();
            test->clear_transport_failures();
            return false;
        }
        return rc == 0;
    };

    graded(key_manager_crc_anchor_test(), "CRC-ANCHOR") ? pass++ : fail++;
    graded(key_manager_func001_test(test, dut, this), "FUNC001") ? pass++ : fail++;
    graded(key_manager_func002_test(test, dut, this), "FUNC002") ? pass++ : fail++;
    graded(key_manager_func003_test(test, dut, this), "FUNC003") ? pass++ : fail++;
    graded(key_manager_func004_test(test, dut, this), "FUNC004") ? pass++ : fail++;
    graded(key_manager_func005_test(test, dut, this), "FUNC005") ? pass++ : fail++;
    graded(key_manager_func006_test(test, dut, this), "FUNC006") ? pass++ : fail++;
    graded(key_manager_func007_test(test, dut, this), "FUNC007") ? pass++ : fail++;
    graded(key_manager_func008_test(test, dut, this), "FUNC008") ? pass++ : fail++;
    graded(key_manager_func009_test(test, dut, this), "FUNC009") ? pass++ : fail++;
    graded(key_manager_func010_test(test, dut, this), "FUNC010") ? pass++ : fail++;
    graded(key_manager_func011_test(test, dut, this), "FUNC011") ? pass++ : fail++;
    graded(key_manager_func012_test(test, dut, this), "FUNC012") ? pass++ : fail++;
    graded(key_manager_func013_test(test, dut, this), "FUNC013") ? pass++ : fail++;
    graded(key_manager_func014_test(test, dut, this), "FUNC014") ? pass++ : fail++;

    // Malformed generic payloads on the mailbox CSR window.
    graded(run_malformed_payload_test(test), "MALFORMED") ? pass++ : fail++;

    // Summary
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "  RESULTS: " << pass << " PASSED  " << fail << " FAILED";
    REG_INFO(1, logger) << "========================================";

    m_tests_failed = fail;
    sc_stop();
}

int sc_main(int argc, char* argv[])
{
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);
    testbench tb("testbench");
    sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);
    return 0;
}
