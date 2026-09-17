// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl_testbench.cpp
 * @brief Comprehensive testbench for SEP Reset Controller
 */

#include <systemc.h>
#include <iostream>
#include <cassert>

#include "../../include/sep_reset_ctrl.h"
#include "sep_reset_ctrl_test.h"
#include "reg_param.h"

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// Top-level testbench module
struct sep_reset_ctrl_testbench : sc_core::sc_module
{
    SC_HAS_PROCESS(sep_reset_ctrl_testbench);
    sep_reset_ctrl_ip dut;

    sep_reset_ctrl_test test_harness;

    sc_core::sc_signal<bool> global_rst_sig;

    sc_core::sc_signal<bool> km_rst_sig;
    sc_core::sc_signal<bool> otbn_rst_sig;
    sc_core::sc_signal<bool> aes_rst_sig;
    sc_core::sc_signal<bool> hmac_rst_sig;
    sc_core::sc_signal<bool> kmac_rst_sig;
    sc_core::sc_signal<bool> trng_rst_sig;
    sc_core::sc_signal<bool> abr_rst_sig;

    SC_CTOR(sep_reset_ctrl_testbench)
        : dut("dut")
        , test_harness("test_harness")
        , global_rst_sig("global_rst_sig")
    {
        test_harness.initiator_socket.bind(dut.target_socket);

        dut.global_rst_ni(global_rst_sig);
        dut.km_rst_ni(km_rst_sig);
        dut.otbn_rst_n(otbn_rst_sig);
        dut.aes_rst_ni(aes_rst_sig);
        dut.hmac_rst_ni(hmac_rst_sig);
        dut.kmac_rst_ni(kmac_rst_sig);
        dut.trng_rst_ni(trng_rst_sig);
        dut.abr_rst_ni(abr_rst_sig);

        global_rst_sig.write(true);

        SC_THREAD(run_all_tests);
    }

    void run_all_tests()
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "SEP RESET CONTROLLER COMPREHENSIVE TESTBENCH" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        test_harness.test_reset_values();
        test_harness.test_register_access();
        test_harness.test_reset_output_updates();
        test_harness.test_global_reset_behavior();

        test_reset_signal_behavior();
        test_global_reset_override();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "ALL TESTS PASSED" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        sc_core::sc_stop();
    }

private:
    void dump_outputs(const char* label)
    {
        std::cout << "  " << label << ": ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << " ";
        std::cout << "trng=" << trng_rst_sig.read() << " ";
        std::cout << "abr=" << abr_rst_sig.read() << std::endl;
    }

    void expect_all(bool v, const char* msg)
    {
        assert(km_rst_sig.read() == v && msg);
        assert(otbn_rst_sig.read() == v && msg);
        assert(aes_rst_sig.read() == v && msg);
        assert(hmac_rst_sig.read() == v && msg);
        assert(kmac_rst_sig.read() == v && msg);
        assert(trng_rst_sig.read() == v && msg);
        assert(abr_rst_sig.read() == v && msg);
    }

    void test_reset_signal_behavior()
    {
        std::cout << "\n=== Testing Reset Signal Behavior ===" << std::endl;

        wait(1, sc_core::SC_NS);

        test_harness.csr_write_64(0x0, sep_reset_ctrl_ip::SW_RESET_N_RESET);
        wait(1, sc_core::SC_NS);

        dump_outputs("Reset state (0x7E)");
        assert(km_rst_sig.read() == false && "KM should be in reset");
        assert(otbn_rst_sig.read() == true && "OTBN should be released");
        assert(aes_rst_sig.read() == true && "AES should be released");
        assert(hmac_rst_sig.read() == true && "HMAC should be released");
        assert(kmac_rst_sig.read() == true && "KMAC should be released");
        assert(trng_rst_sig.read() == true && "TRNG should be released");
        assert(abr_rst_sig.read() == true && "ABR should be released");

        test_harness.csr_write_64(0x0, 0x00);
        wait(1, sc_core::SC_NS);
        dump_outputs("All in reset (0x00)");
        expect_all(false, "All should be in reset");

        test_harness.csr_write_64(0x0, sep_reset_ctrl_ip::SW_RESET_N_MASK);
        wait(1, sc_core::SC_NS);
        dump_outputs("All released (0x7F)");
        expect_all(true, "All should be released");

        std::cout << "Reset signal behavior test PASSED" << std::endl;
    }

    void test_global_reset_override()
    {
        std::cout << "\n=== Testing Global Reset Override ===" << std::endl;

        test_harness.csr_write_64(0x0, sep_reset_ctrl_ip::SW_RESET_N_MASK);
        wait(1, sc_core::SC_NS);

        global_rst_sig.write(false);
        wait(1, sc_core::SC_NS);

        dump_outputs("Global reset asserted");
        expect_all(false, "Global reset should override all");

        global_rst_sig.write(true);
        wait(1, sc_core::SC_NS);

        dump_outputs("Global reset released");
        assert(km_rst_sig.read() == false && "km_sw_rst_n defaults to 0 (held in reset) after global reset");
        assert(otbn_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(aes_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(hmac_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(kmac_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(trng_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(abr_rst_sig.read() == true && "Should return to SW_RESET_N default");

        uint64_t sw_reset_n = test_harness.csr_read_64(0x0);
        assert((sw_reset_n & sep_reset_ctrl_ip::SW_RESET_N_MASK) == sep_reset_ctrl_ip::SW_RESET_N_RESET &&
               "SW_RESET_N CSR itself must read back its default (0x7E) after global reset");

        std::cout << "Global reset override test PASSED" << std::endl;
    }
};

int sc_main(int argc, char* argv[])
{
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);
    sep_reset_ctrl_testbench testbench("testbench");
    sc_core::sc_start();
    std::cout << "\nSEP Reset Controller testbench completed successfully!" << std::endl;
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(0);
    return 0;
}
