// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include <systemc.h>
#include "../../include/hmac.h"
#include "hmac_test.h"
#include "csml_logger.h"

// Expected CFG read-back for a given written value.
//
// CFG is an external register: reads return the sanitised digest_size and
// key_length rather than the raw value software wrote. Unsupported encodings
// read back as SHA2_None (0x8) and Key_None (0x20) respectively, and the two
// fields are sanitised independently of each other and of hmac_en. Plain SHA-2
// tests that leave key_length at 0 therefore read back Key_None.
static inline uint32_t expected_cfg_readback(uint32_t written)
{
    const uint32_t digest_size = (written >> 5) & 0xF;
    const uint32_t key_length  = (written >> 9) & 0x3F;

    uint32_t expected = written;

    if (digest_size != 0x1 && digest_size != 0x2 && digest_size != 0x4) {
        expected &= ~(0xFu << 5);
        expected |= (0x8u << 5);
    }

    if (key_length != 0x1 && key_length != 0x2 && key_length != 0x4 &&
        key_length != 0x8 && key_length != 0x10) {
        expected &= ~(0x3Fu << 9);
        expected |= (0x20u << 9);
    }

    return expected;
}

class testbench : public sc_module
{
public:
    // Module instances
    hmac_ip* dut;        // Device Under Test (HMAC model)
    hmac_test* test;     // Test module

    // Clock signal
    sc_clock* clk;       // SystemC clock generator (50 MHz, 20ns period)

    // Intermediate signals for port connections
    sc_signal<bool> sig_intr_hmac_done;
    sc_signal<bool> sig_intr_fifo_empty;
    sc_signal<bool> sig_intr_hmac_err;
    sc_signal<bool> sig_alert_fatal_fault;
    sc_signal<double> sig_clk_i;
    sc_signal<bool> sig_rst_ni;

    SC_HAS_PROCESS(testbench);

    testbench(sc_module_name name) : sc_module(name)
    {
        // Initialize logger
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);

        // Create 50 MHz clock (20ns period, 50% duty cycle, 0ns start time, true initial value)
        clk = new sc_clock("clk", 20, SC_NS, 0.5, 0, SC_NS, true);

        // Instantiate HMAC model (DUT) with memory size
        // Memory size calculation: 0x2000 (8KB to cover MSG_FIFO window at 0x1000-0x1FFF)
        dut = new hmac_ip("hmac_dut", 0x2000);

        // Sync testbench logger verbosity with DUT (CCI ini may override build default)
        logger.setMaxVerbosity(dut->verbosity.get_param_value());

        // Instantiate test module
        test = new hmac_test("hmac_test");

        // Bind TLM ports: test initiator -> DUT target
        test->initiator_socket.bind(dut->target_socket);

        // Bind keymgr sideload socket: test initiator -> DUT target
        test->keymgr_initiator_socket.bind(dut->keymgr_tl_socket);

        // Bind interrupt ports: DUT outputs -> signals -> test inputs
        dut->intr_hmac_done.bind(sig_intr_hmac_done);
        test->intr_hmac_done.bind(sig_intr_hmac_done);

        dut->intr_fifo_empty.bind(sig_intr_fifo_empty);
        test->intr_fifo_empty.bind(sig_intr_fifo_empty);

        dut->intr_hmac_err.bind(sig_intr_hmac_err);
        test->intr_hmac_err.bind(sig_intr_hmac_err);

        // Bind alert port: DUT output -> signal -> test input
        dut->alert_fatal_fault.bind(sig_alert_fatal_fault);
        test->alert_fatal_fault.bind(sig_alert_fatal_fault);

        // Bind clock port: test output -> signal -> DUT input
        test->clk_i.bind(sig_clk_i);
        dut->clk_i.bind(sig_clk_i);

        // Bind reset port: test output -> signal -> DUT input
        test->rst_ni.bind(sig_rst_ni);
        dut->rst_ni.bind(sig_rst_ni);

        // Register test process
        SC_THREAD(run_tests);
    }

    ~testbench()
    {
        delete clk;
        delete dut;
        delete test;
    }

    void run_tests();

private:
    // Test case functions
    void test_read_write_registers();
    void test_read_only_registers();
    void test_port_binding_verification();
    void test_reset_functionality();

    //Smoke Tests
    void test_reset_mechanisms();
    void test_readonly_registers();
    void test_writeonly_registers();
    void test_readwrite_registers();
    void test_sha256_hash();
    void test_sha256_endian_swap();
    void test_sha256_digest_swap();
    void test_hmac_done_interrupt();
    void test_interrupt_masking();
    void test_interrupt_injection();

    //Functional Tests
    void test_sha256_hash_multiblock_message();
    void test_sha384_hash();
    void test_sha512_hash();
    void test_empty_message_hash();

    // Core HMAC Tests
    void test_hmac_sha256_key128();
    void test_hmac_sha256_key256();
    void test_hmac_sha256_key512();
    void test_hmac_sha384_key384();
    void test_hmac_sha512_key1024();

    // Error Condition Tests
    void test_hmac_err_interrupt();
    void test_error_hash_start_sha_disabled();
    void test_error_hash_start_when_active();
    void test_error_key_write_during_processing();
    void test_error_msg_fifo_before_start();
    void test_error_invalid_digest_size();
	void test_error_invalid_key_length_hmac();
    void test_error_key1024_sha256();
    void test_error_recovery();
    void test_error_msg_fifo_after_process();

    // Message Length Tracking Test
    void test_message_length_tracking();

    // FIFO Tests
    void test_fifo_status_updates();
    void test_fifo_empty_interrupt();
    void test_fifo_back_pressure();
    void test_subword_writes_byte();
    void test_subword_writes_halfword();

    void test_error_conditions();
    void test_key_swap();
    void test_msg_fifo_address_window();
    void test_reset_during_processing();

    // Key Manager Sideload Tests
    void test_keymgr_sideload_hmac_sha256();
    void test_keymgr_sideload_ignores_sw_key();
    void test_keymgr_sideload_xor_shares();
    void test_keymgr_sideload_cleared_on_reset();
    void test_keymgr_sideload_preserves_sw_key();

    // FIFO interrupt gating
    void test_fifo_empty_interrupt_gating();

    // hash_stop/hash_continue must reproduce a single-shot digest
    void test_context_save_restore_digest(uint32_t cfg, bool hmac_mode, const char *label);

    // Security Tests
    void test_wipe_secret();
    void test_key_register_writeonly();
    void test_cfg_write_protection();
    void test_digest_write_protection();
    void test_reserved_fields();
    void test_minimum_length_transfer();
    void test_status_hmac_idle_transitions();
    void test_command_self_clearing();
    void test_block_boundary_message();
    void test_maximum_length_transfer();
    void test_context_save_basic();
    void test_hash_stop_sync_fifo_drain();
    void test_context_sha_en_disable_clear();

    // Command rejects, leftover-FIFO start, SHA-384 stop/continue, keymgr read
    void test_coverage_gap_paths();

    // Helper functions
    void wait_for_hmac_idle();
    void wait_for_hmac_done();

    // Logger instance for structured logging
    CsmlLogger logger;

public:
    uint32_t m_tests_failed = 0;
};
