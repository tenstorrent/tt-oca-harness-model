#pragma once
#include <systemc.h>
#include <memory>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "key_manager.h"
#include "key_manager_test.h"
#include "csml_logger.h"

/**
 * @brief Top-level testbench — instantiates DUT and test harness,
 *        binds all TLM sockets, and wires reset / IRQ signals.
 *
 * Engine stubs record every key word written by the DUT so that
 * behavioral tests can verify correct key material delivery.
 */
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    CsmlLogger logger;
    uint32_t m_tests_failed = 0;

    // DUT and test harness
    std::unique_ptr<key_manager_model> m_dut;
    std::unique_ptr<key_manager_test>  m_test;

    // Shared signals
    sc_signal<bool> rst_n_signal;
    sc_signal<bool> wipe_n_signal;  ///< Active-low emergency wipe (driven high = de-asserted)
    sc_signal<bool> irq_signal;

    // Recording engine stubs — one per crypto engine.
    // Tests call clear() before verifying engine writes.
    recording_engine_stub hmac_stub;
    recording_engine_stub kmac_stub;
    recording_engine_stub aes_stub;
    recording_engine_stub otbn_stub;

    testbench(sc_module_name name);

private:
    void run_tests();  ///< SC_THREAD: sequentially runs all test functions
};
