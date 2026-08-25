// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "csml_parameter.h"

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


void testbench::run_tests()
{
    int pass = 0, fail = 0;
    key_manager_test*  test = m_test.get();
    key_manager_model* dut  = m_dut.get();

    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "  KeyMgr TT Testbench";
    CSML_INFO(1, logger) << "========================================";

    // Apply reset before first test; each subsequent FUNC test resets itself.
    test->trigger_reset();

    (key_manager_func001_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func002_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func003_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func004_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func005_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func006_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func007_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func008_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func009_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func010_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func011_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func012_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func013_test(test, dut, this) == 0) ? pass++ : fail++;
    (key_manager_func014_test(test, dut, this) == 0) ? pass++ : fail++;

    // Summary
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "  RESULTS: " << pass << " PASSED  " << fail << " FAILED";
    CSML_INFO(1, logger) << "========================================";

    m_tests_failed = fail;
    sc_stop();
}

int sc_main(int argc, char* argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);
    testbench tb("testbench");
    sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);
    return 0;
}
