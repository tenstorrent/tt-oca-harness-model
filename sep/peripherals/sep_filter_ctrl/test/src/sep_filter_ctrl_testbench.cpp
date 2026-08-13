/**
 * @file sep_filter_ctrl_testbench.cpp
 * @brief Testbench for the generic sep_filter_ctrl peripheral.
 *
 * Suite A — outbound instance (num_instances=32):
 *   A1  reset values
 *   A2  basic register read/write
 *   A3  WOSET locked bit
 *   A4  hw-readonly data_bus_width
 *   A5  CSR clears to entry_enabled=0 when unconfigured
 *   A6  comprehensive CSR scenarios
 *   A7  data path: BlockByDefault denies all when unconfigured
 *   A8  data path: default deny when entry active but no match
 *   A9  data path: read within allowed range
 *   A10 data_transport_dbg: same filter logic as b_transport, on the debug path
 *
 * Suite B — inbound instance (num_instances=16):
 *   B1  reset state independent from outbound
 *   B2  inbound filter enforces its own entries
 *   B3  write to outbound CSR does not affect inbound
 */

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../../include/sep_filter_ctrl.h"
#include "sep_filter_ctrl_test.h"
#include "csml_parameter.h"

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

// =============================================================================
// FilteredStub — records last transaction seen on the filtered output
// =============================================================================
struct FilteredStub : sc_core::sc_module
{
    tlm_utils::simple_target_socket<FilteredStub> socket;
    uint64_t            last_addr = 0;
    uint64_t            last_data = 0;
    tlm::tlm_command    last_cmd  = tlm::TLM_IGNORE_COMMAND;
    bool                received  = false;

    SC_CTOR(FilteredStub) : socket("socket")
    {
        socket.register_b_transport(this,   &FilteredStub::b_transport);
        socket.register_transport_dbg(this, &FilteredStub::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        last_addr = trans.get_address();
        last_cmd  = trans.get_command();
        received  = true;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND && trans.get_data_length() >= 8)
            std::memcpy(&last_data, trans.get_data_ptr(), 8);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay = sc_core::SC_ZERO_TIME;
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        last_addr = trans.get_address();
        received  = true;
        return trans.get_data_length();
    }

    void reset() { last_addr = 0; last_data = 0; last_cmd = tlm::TLM_IGNORE_COMMAND; received = false; }
};

// =============================================================================
// DataInitiator — generates data-path transactions
// =============================================================================
struct DataInitiator : sc_core::sc_module
{
    tlm_utils::simple_initiator_socket<DataInitiator> socket;
    SC_CTOR(DataInitiator) : socket("socket") {}

    bool write(uint64_t addr, uint64_t data)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }

    bool read(uint64_t addr, uint64_t& data)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }

    // Debug (GDB/memory-inspector) path — exercises transport_dbg() instead of
    // b_transport(). Returns the byte count forwarded (0 means denied).
    unsigned int dbg_read(uint64_t addr, uint64_t& data)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        return socket->transport_dbg(trans);
    }
};

// =============================================================================
// Top-level testbench
// =============================================================================
struct SepFilterCtrlTestbench : sc_core::sc_module
{
    // Drive rst_ni high (deasserted) throughout the testbench
    sc_core::sc_signal<bool> rst_n_sig;

    // Outbound: 32 instances
    sep_filter_ctrl_ip  outbound_dut;
    sep_filter_ctrl_test outbound_harness;
    DataInitiator        outbound_data_init;
    FilteredStub         outbound_stub;

    // Inbound: 16 instances
    sep_filter_ctrl_ip  inbound_dut;
    sep_filter_ctrl_test inbound_harness;
    DataInitiator        inbound_data_init;
    FilteredStub         inbound_stub;

    // Suite C: an inbound instance with filter_skip_i strapped high, standing in
    // for feat_ctrl_o.sep_debug (sep.sv:952). The signal carries its value from
    // construction because these tests run without sc_start(), so there is no
    // delta cycle in which a later write() could take effect.
    sc_core::sc_signal<bool> skip_hi_sig;
    sep_filter_ctrl_ip   skip_dut;
    sep_filter_ctrl_test skip_harness;
    DataInitiator        skip_data_init;
    FilteredStub         skip_stub;

    SC_CTOR(SepFilterCtrlTestbench)
        : outbound_dut("outbound_dut", sep_filter_ctrl_ip::InstanceType::OUTBOUND)
        , outbound_harness("outbound_harness")
        , outbound_data_init("outbound_data_init")
        , outbound_stub("outbound_stub")
        , inbound_dut("inbound_dut", sep_filter_ctrl_ip::InstanceType::INBOUND)
        , inbound_harness("inbound_harness")
        , inbound_data_init("inbound_data_init")
        , inbound_stub("inbound_stub")
        , skip_hi_sig("skip_hi_sig", true)
        , skip_dut("skip_dut", sep_filter_ctrl_ip::InstanceType::INBOUND)
        , skip_harness("skip_harness")
        , skip_data_init("skip_data_init")
        , skip_stub("skip_stub")
    {
        rst_n_sig.write(true);  // deassert reset for entire testbench run
        outbound_dut.rst_ni(rst_n_sig);
        inbound_dut.rst_ni(rst_n_sig);
        skip_dut.rst_ni(rst_n_sig);
        skip_dut.filter_skip_i(skip_hi_sig);

        outbound_harness.initiator_socket.bind(outbound_dut.target_socket);
        outbound_data_init.socket.bind(outbound_dut.data_socket);
        outbound_dut.filtered_socket.bind(outbound_stub.socket);

        inbound_harness.initiator_socket.bind(inbound_dut.target_socket);
        inbound_data_init.socket.bind(inbound_dut.data_socket);
        inbound_dut.filtered_socket.bind(inbound_stub.socket);

        skip_harness.initiator_socket.bind(skip_dut.target_socket);
        skip_data_init.socket.bind(skip_dut.data_socket);
        skip_dut.filtered_socket.bind(skip_stub.socket);
    }

    void run_all_tests()
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "SEP FILTER CTRL COMPREHENSIVE TESTBENCH" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        run_suite_a();
        run_suite_b();
        run_suite_c();
        run_suite_d();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "ALL TESTS PASSED!" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
    }

private:
    // =========================================================================
    // Suite A — outbound instance (32 entries)
    // =========================================================================
    void run_suite_a()
    {
        std::cout << "\n--- Suite A: outbound_filter (32 instances) ---" << std::endl;

        // A1: reset values
        outbound_harness.test_reset_values(32);

        // A2: basic register read/write
        outbound_harness.test_register_access_basic();

        // A3: WOSET locked bit
        outbound_harness.test_woset_locked_field();

        // A4: hw-readonly data_bus_width
        outbound_harness.test_hw_readonly_data_bus_width();

        // A5: passthrough when unconfigured
        outbound_harness.test_passthrough_when_unconfigured(32);

        // A6: comprehensive CSR scenarios
        outbound_harness.test_comprehensive_filter_scenarios();

        // A7: data path — BlockByDefault (RTL: axi_filter_wrap sets
        // BlockByDefault=1'b1 for both SEP filter instances). With no active
        // entries, every transaction is denied, not passed through.
        // Clear all entries first
        for (uint32_t i = 0; i < 32; ++i)
            outbound_harness.csr_write_64(i, 0x00, 0x0ULL);

        outbound_stub.reset();
        bool ok = outbound_data_init.write(0xABCD1000ULL, 0xDEADBEEFULL);
        assert(!ok && "A7: write should be denied when filter unconfigured (BlockByDefault)");
        assert(!outbound_stub.received && "A7: denied transaction must not reach filtered_socket");
        std::cout << "A7: data path BlockByDefault PASSED" << std::endl;

        // A8: default deny — configure one entry then access outside it
        outbound_harness.csr_write_64(0, 0x08, 0x10000ULL);
        outbound_harness.csr_write_64(0, 0x10, 0x20000ULL);
        outbound_harness.csr_write_64(0, 0x00, 0x00000013ULL); // entry_enabled=1

        outbound_stub.reset();
        ok = outbound_data_init.write(0x30000ULL, 0x12345678ULL);
        assert(!ok && "A8: write outside range should be denied");
        assert(!outbound_stub.received && "A8: denied transaction must not reach filtered_socket");
        std::cout << "A8: default deny PASSED" << std::endl;

        // A9: permitted read within range
        outbound_stub.reset();
        uint64_t rd = 0;
        ok = outbound_data_init.read(0x15000ULL, rd);
        assert(ok && "A9: read within range should succeed");
        assert(outbound_stub.received && "A9: transaction should reach filtered_socket");
        std::cout << "A9: data path read permitted PASSED" << std::endl;

        // A10: data_transport_dbg — same filter logic as data_b_transport,
        // exercised via transport_dbg() instead of b_transport(). Region 0
        // ([0x10000, 0x20000)) is still configured from A8/A9 above.
        outbound_stub.reset();
        uint64_t dbg_rd = 0;
        unsigned int dbg_ret = outbound_data_init.dbg_read(0x15000ULL, dbg_rd);
        assert(dbg_ret != 0 && "A10: dbg read within range should succeed (nonzero return)");
        assert(outbound_stub.received && "A10: permitted dbg transaction should reach filtered_socket");

        outbound_stub.reset();
        dbg_ret = outbound_data_init.dbg_read(0x30000ULL, dbg_rd); // outside any configured region
        assert(dbg_ret == 0 && "A10: dbg read outside range should be denied (0 return)");
        assert(!outbound_stub.received && "A10: denied dbg transaction must not reach filtered_socket");
        std::cout << "A10: data_transport_dbg PASSED" << std::endl;

        // A11: the write half of command gating. B2 covers a read denied by a
        // write-only entry; this covers a write denied by a read-only one, so
        // both arms of the permission check are driven through the data path.
        // Entry 3 rather than 1: A3 sets entry 1's WOSET lock, which silently
        // discards every later write to it (including A6's).
        outbound_harness.csr_write_64(3, 0x08, 0x40000ULL);
        outbound_harness.csr_write_64(3, 0x10, 0x50000ULL);
        outbound_harness.csr_write_64(3, 0x00, 0x00000011ULL); // read_allowed=1, entry_enabled=1

        outbound_stub.reset();
        ok = outbound_data_init.write(0x45000ULL, 0x5A5A5A5AULL);
        assert(!ok && "A11: write to a read-only entry should be denied");
        assert(!outbound_stub.received && "A11: denied write must not reach filtered_socket");

        outbound_stub.reset();
        ok = outbound_data_init.read(0x45000ULL, rd);
        assert(ok && "A11: read from a read-only entry should be permitted");
        assert(outbound_stub.received && "A11: permitted read should reach filtered_socket");
        std::cout << "A11: read-only entry denies writes PASSED" << std::endl;
    }

    // =========================================================================
    // Suite B — inbound instance (16 entries)
    // =========================================================================
    void run_suite_b()
    {
        std::cout << "\n--- Suite B: inbound_filter (16 instances) ---" << std::endl;

        // B1: reset state independent from outbound
        inbound_harness.test_reset_values(16);

        // B2: inbound filter enforces its own entries
        // Configure inbound entry 0, [0x80000, 0x90000), write-only
        inbound_harness.csr_write_64(0, 0x08, 0x80000ULL);
        inbound_harness.csr_write_64(0, 0x10, 0x90000ULL);
        inbound_harness.csr_write_64(0, 0x00, 0x00000012ULL); // write_allowed=1, entry_enabled=1

        inbound_stub.reset();
        uint64_t rd = 0;
        bool ok = inbound_data_init.read(0x85000ULL, rd);
        assert(!ok && "B2: read from write-only entry should be denied");
        assert(!inbound_stub.received && "B2: denied read must not reach filtered_socket");

        inbound_stub.reset();
        ok = inbound_data_init.write(0x85000ULL, 0xCAFEBABEULL);
        assert(ok && "B2: write within write-only entry should be permitted");
        assert(inbound_stub.received && "B2: permitted write should reach filtered_socket");
        std::cout << "B2: inbound per-entry enforcement PASSED" << std::endl;

        // B3: outbound CSR write does not affect inbound
        outbound_harness.csr_write_64(5, 0x08, 0x99999000ULL);
        outbound_harness.csr_write_64(5, 0x10, 0xAAAA0000ULL);
        outbound_harness.csr_write_64(5, 0x00, 0x00000013ULL);

        // Inbound entry 5 should still be unconfigured
        uint64_t inbound_cfg5 = inbound_harness.csr_read_64(5, 0x00);
        assert(((inbound_cfg5 & (1ULL << 4)) == 0) && "B3: inbound entry 5 entry_enabled should be 0");
        std::cout << "B3: outbound write does not affect inbound PASSED" << std::endl;
    }

    // =========================================================================
    // Suite C — filter_skip_i (axi_filter_wrap.sv forces isolate_write and
    // isolate_read low, so neither the match nor the permission check applies)
    // =========================================================================
    void run_suite_c()
    {
        std::cout << "\n--- Suite C: filter_skip_i bypass ---" << std::endl;

        // C1: no entries enabled at all. Suite A7 proved this denies when skip is
        // low, so forwarding here is attributable to the skip input.
        skip_stub.reset();
        bool ok = skip_data_init.write(0xABCD1000ULL, 0xDEADBEEFULL);
        assert(ok && "C1: skip must bypass BlockByDefault");
        assert(skip_stub.received && "C1: bypassed transaction should reach filtered_socket");
        std::cout << "C1: skip bypasses BlockByDefault PASSED" << std::endl;

        // C2: a matching entry that forbids the direction. read_allowed=0 makes
        // this a permission denial rather than a miss, which skip also defeats.
        skip_harness.csr_write_64(0, 0x08, 0x80000ULL);
        skip_harness.csr_write_64(0, 0x10, 0x90000ULL);
        skip_harness.csr_write_64(0, 0x00, 0x00000012ULL); // write_allowed=1, entry_enabled=1

        skip_stub.reset();
        uint64_t rd = 0;
        ok = skip_data_init.read(0x85000ULL, rd);
        assert(ok && "C2: skip must bypass the read_allowed permission check");
        assert(skip_stub.received && "C2: bypassed read should reach filtered_socket");
        std::cout << "C2: skip bypasses permission denial PASSED" << std::endl;

        // C3: the debug path honours skip identically.
        skip_stub.reset();
        uint64_t dbg_rd = 0;
        unsigned int dbg_ret = skip_data_init.dbg_read(0x30000ULL, dbg_rd); // outside every entry
        assert(dbg_ret != 0 && "C3: skip must bypass filtering on the dbg path too");
        assert(skip_stub.received && "C3: bypassed dbg transaction should reach filtered_socket");
        std::cout << "C3: skip bypasses dbg path PASSED" << std::endl;
    }

    // =========================================================================
    // Suite D — reset. Runs last because it is the only suite that starts the
    // scheduler: reset_handler is an SC_METHOD, so it cannot fire until a delta
    // cycle exists, and every preceding suite drives transactions directly from
    // sc_main with the scheduler stopped.
    // =========================================================================
    void run_suite_d()
    {
        std::cout << "\n--- Suite D: reset ---" << std::endl;

        // Entry 0 is still configured and enabled from Suites A8–A10.
        uint64_t cfg = outbound_harness.csr_read_64(0, 0x00);
        assert((cfg & (1ULL << 4)) && "D1: entry 0 should still be enabled before reset");

        // Two starts, and each must advance simulated time. A write issued from
        // sc_main is only committed in a following update phase, and the value
        // change is what triggers reset_handler; sc_start(SC_ZERO_TIME) was not
        // enough to deliver either write here. The first start commits the
        // constructor's rst_n_sig.write(true), so that reset is observably
        // deasserted before the second start drives the falling edge under test.
        sc_core::sc_start(1, sc_core::SC_NS);
        assert((outbound_harness.csr_read_64(0, 0x00) & (1ULL << 4)) &&
               "D1: deasserted reset must not clear configuration");

        rst_n_sig.write(false);
        sc_core::sc_start(1, sc_core::SC_NS);

        // 0x3000, not 0: data_bus_width is hw=w and always reads back 3 (A1
        // records the same value for a freshly reset entry).
        cfg = outbound_harness.csr_read_64(0, 0x00);
        assert(cfg == 0x3000 && "D1: reset must clear FILTER_CONFIG except data_bus_width");
        assert(outbound_harness.csr_read_64(0, 0x08) == 0 && "D1: reset must clear START_ADDR");
        assert(outbound_harness.csr_read_64(0, 0x10) == 0x7 && "D1: END_ADDR resets to 0x7");
        std::cout << "D1: rst_ni clears configuration PASSED" << std::endl;

        // Deny-by-default is restored, since no entry survives the reset.
        outbound_stub.reset();
        bool ok = outbound_data_init.read(0x15000ULL, cfg);
        assert(!ok && "D2: previously permitted range must be denied after reset");
        assert(!outbound_stub.received && "D2: denied transaction must not reach filtered_socket");
        std::cout << "D2: filtering denies after reset PASSED" << std::endl;
    }
};

// =============================================================================
// sc_main
// =============================================================================
int sc_main(int argc, char* argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);
    SepFilterCtrlTestbench testbench("testbench");
    int result = 0;
    try {
        testbench.run_all_tests();
        std::cout << "\nSep Filter Ctrl testbench completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        result = 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception" << std::endl;
        result = 1;
    }
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(result);
    return 0;
}
