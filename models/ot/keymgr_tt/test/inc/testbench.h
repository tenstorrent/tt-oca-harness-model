#pragma once
#include <systemc.h>
#include <memory>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "keymgr_tt.h"
#include "keymgr_tt_test.h"
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

    // DUT and test harness
    std::unique_ptr<keymgr_tt_model> m_dut;
    std::unique_ptr<keymgr_tt_test>  m_test;

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
