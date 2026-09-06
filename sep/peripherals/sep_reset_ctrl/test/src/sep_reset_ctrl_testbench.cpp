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
    // DUT
    sep_reset_ctrl_ip dut;
    
    // Test harness for CSR access
    sep_reset_ctrl_test test_harness;
    
    // Global reset signal
    sc_core::sc_signal<bool> global_rst_sig;
    
    // Reset output signals from DUT
    sc_core::sc_signal<bool> km_rst_sig;
    sc_core::sc_signal<bool> otbn_rst_sig;
    sc_core::sc_signal<bool> aes_rst_sig;
    sc_core::sc_signal<bool> hmac_rst_sig;
    sc_core::sc_signal<bool> kmac_rst_sig;

    SC_CTOR(sep_reset_ctrl_testbench) 
        : dut("dut")
        , test_harness("test_harness")
        , global_rst_sig("global_rst_sig")
    {
        // Connect CSR access path
        test_harness.initiator_socket.bind(dut.target_socket);
        
        // Connect reset signals
        dut.global_rst_ni(global_rst_sig);
        dut.km_rst_ni(km_rst_sig);
        dut.otbn_rst_n(otbn_rst_sig);
        dut.aes_rst_ni(aes_rst_sig);
        dut.hmac_rst_ni(hmac_rst_sig);
        dut.kmac_rst_ni(kmac_rst_sig);
        
        // Initialize global reset to released (true)
        global_rst_sig.write(true);

        SC_THREAD(run_all_tests);
    }

    void run_all_tests()
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "SEP RESET CONTROLLER COMPREHENSIVE TESTBENCH" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        // Basic register tests
        test_harness.test_reset_values();
        test_harness.test_register_access();
        test_harness.test_reset_output_updates();
        test_harness.test_global_reset_behavior();

        // Signal-level tests
        test_reset_signal_behavior();
        test_global_reset_override();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🎉 ALL TESTS PASSED! ✅" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        sc_core::sc_stop();
    }

private:
    void test_reset_signal_behavior()
    {
        std::cout << "\n=== Testing Reset Signal Behavior ===" << std::endl;
        
        // Allow some time for signals to settle
        wait(1, sc_core::SC_NS);

        // Test case 1: Check reset state (0x1E)
        // km=0, otbn=1, aes=1, hmac=1, kmac=1
        test_harness.csr_write_64(0x0, 0x1E);
        wait(1, sc_core::SC_NS); // Allow signal propagation
        
        std::cout << "  Reset state (0x1E): ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";  
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << std::endl;
        
        assert(km_rst_sig.read() == false && "KM should be in reset");
        assert(otbn_rst_sig.read() == true && "OTBN should be released");
        assert(aes_rst_sig.read() == true && "AES should be released");
        assert(hmac_rst_sig.read() == true && "HMAC should be released");
        assert(kmac_rst_sig.read() == true && "KMAC should be released");
        
        // Test case 2: All peripherals in reset (0x00)
        test_harness.csr_write_64(0x0, 0x00);
        wait(1, sc_core::SC_NS);
        
        std::cout << "  All in reset (0x00): ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << std::endl;
        
        assert(km_rst_sig.read() == false && "All should be in reset");
        assert(otbn_rst_sig.read() == false && "All should be in reset");
        assert(aes_rst_sig.read() == false && "All should be in reset");
        assert(hmac_rst_sig.read() == false && "All should be in reset");
        assert(kmac_rst_sig.read() == false && "All should be in reset");
        
        // Test case 3: All peripherals released (0x1F)
        test_harness.csr_write_64(0x0, 0x1F);
        wait(1, sc_core::SC_NS);
        
        std::cout << "  All released (0x1F): ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << std::endl;
        
        assert(km_rst_sig.read() == true && "All should be released");
        assert(otbn_rst_sig.read() == true && "All should be released");
        assert(aes_rst_sig.read() == true && "All should be released");
        assert(hmac_rst_sig.read() == true && "All should be released");
        assert(kmac_rst_sig.read() == true && "All should be released");
        
        std::cout << "Reset signal behavior test PASSED" << std::endl;
    }
    
    void test_global_reset_override()
    {
        std::cout << "\n=== Testing Global Reset Override ===" << std::endl;
        
        // Set all SW resets to released
        test_harness.csr_write_64(0x0, 0x1F);
        wait(1, sc_core::SC_NS);

        // Assert global reset
        global_rst_sig.write(false);
        wait(1, sc_core::SC_NS);
        
        std::cout << "  Global reset asserted: ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << std::endl;
        
        // When global_rst_ni is false, all outputs should be false
        assert(km_rst_sig.read() == false && "Global reset should override all");
        assert(otbn_rst_sig.read() == false && "Global reset should override all");
        assert(aes_rst_sig.read() == false && "Global reset should override all");
        assert(hmac_rst_sig.read() == false && "Global reset should override all");
        assert(kmac_rst_sig.read() == false && "Global reset should override all");
        
        // Release global reset
        global_rst_sig.write(true);
        wait(1, sc_core::SC_NS);

        std::cout << "  Global reset released: ";
        std::cout << "km=" << km_rst_sig.read() << " ";
        std::cout << "otbn=" << otbn_rst_sig.read() << " ";
        std::cout << "aes=" << aes_rst_sig.read() << " ";
        std::cout << "hmac=" << hmac_rst_sig.read() << " ";
        std::cout << "kmac=" << kmac_rst_sig.read() << std::endl;

        // A global reset must clear SW_RESET_N back to its documented default
        // (0x1E: km held in reset, everything else released) — NOT preserve
        // whatever value firmware had programmed (0x1F) before the reset.
        assert(km_rst_sig.read() == false && "km_sw_rst_n defaults to 0 (held in reset) after global reset");
        assert(otbn_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(aes_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(hmac_rst_sig.read() == true && "Should return to SW_RESET_N default");
        assert(kmac_rst_sig.read() == true && "Should return to SW_RESET_N default");

        uint64_t sw_reset_n = test_harness.csr_read_64(0x0);
        assert((sw_reset_n & 0x1F) == 0x1E && "SW_RESET_N CSR itself must read back its default (0x1E) after global reset");

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