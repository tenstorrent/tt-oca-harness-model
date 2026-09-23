// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file beu_tb.cpp
 * @brief Primary self-checking test bench for the SMC Bus Error Unit model.
 *
 * Exercises: reset values, 64-bit register RW/RO/masking semantics, error
 * injection (accrued status, CAUSE/PHYS_ADDR first-error latching + priority,
 * recording-enable gating), local vs. PLIC interrupt aggregation + per-source
 * masking, software ack (clear accrued) and re-arm (clear CAUSE), reset, and
 * CCI introspection.
 */

#include "beu.h"

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {

int g_failures = 0;

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        auto _e = (expected);                                                  \
        auto _a = (actual);                                                    \
        if (_e != _a) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " #actual " expected=0x" << std::hex               \
                      << static_cast<uint64_t>(_e) << " got=0x"                \
                      << static_cast<uint64_t>(_a) << std::dec << "\n";        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Register offsets (bus_error_unit.rdl).
constexpr uint64_t CAUSE = 0x00, PHYS_ADDR = 0x08, ENABLE = 0x10;
constexpr uint64_t PLIC_ENABLE = 0x18, ACCRUED_ENABLE = 0x20, LOCAL_ENABLE = 0x28;

// Source bit masks.
constexpr uint64_t M_ITL = 1u << 1; // ICache TileLink bus
constexpr uint64_t M_IEC = 1u << 2; // ICache correctable
constexpr uint64_t M_DTL = 1u << 5; // DCache TileLink bus
constexpr uint64_t M_DEC = 1u << 6; // DCache correctable
constexpr uint64_t M_DEU = 1u << 7; // DCache uncorrectable
constexpr uint64_t VALID_MASK = M_ITL | M_IEC | M_DTL | M_DEC | M_DEU; // 0xE6

// Tiny TLM driver exercising the LT path through a quantum keeper (64-bit).
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver, 64> sock;
    tlm_utils::tlm_quantumkeeper qk;

    explicit driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    uint64_t read64(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0xDEADBEEFCAFEF00Dull;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read64(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write64(uint64_t addr, uint64_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write64(0x" << std::hex << addr << ", 0x" << value
                      << ") rsp=" << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    unsigned dbg_read(uint64_t addr, uint64_t& out) {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&out));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

struct tb : sc_core::sc_module {
    driver drv;
    smc::beu dut;

    sc_core::sc_signal<bool> rstn{"rstn"};
    sc_core::sc_signal<bool> irq_local{"irq_local"};
    sc_core::sc_signal<bool> irq_plic{"irq_plic"};

    SC_HAS_PROCESS(tb);

    explicit tb(sc_core::sc_module_name n)
        : sc_module(n), drv("drv"), dut("beu")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rstn);
        dut.irq_local_o(irq_local);
        dut.irq_plic_o(irq_plic);
        SC_THREAD(run);
    }

    // Let SC_METHODs (reset / recompute) run and signal updates propagate.
    void settle() { sc_core::wait(sc_time(1, SC_NS)); }

    void do_reset() {
        rstn.write(false);
        settle();
        rstn.write(true);
        settle();
    }

    void run();
};

void tb::run()
{
    // ----------------------------------------------------------------------
    // 1. Reset values
    // ----------------------------------------------------------------------
    do_reset();
    EXPECT_EQ(0u,          drv.read64(CAUSE));
    EXPECT_EQ(0u,          drv.read64(PHYS_ADDR));
    EXPECT_EQ(VALID_MASK,  drv.read64(ENABLE));       // all sources enabled
    EXPECT_EQ(0u,          drv.read64(PLIC_ENABLE));
    EXPECT_EQ(0u,          drv.read64(ACCRUED_ENABLE));
    EXPECT_EQ(0u,          drv.read64(LOCAL_ENABLE));
    EXPECT_TRUE(!irq_local.read());
    EXPECT_TRUE(!irq_plic.read());
    std::cout << "  [PASS] reset values\n";

    // ----------------------------------------------------------------------
    // 2. Register RW / RO / masking (64-bit)
    // ----------------------------------------------------------------------
    drv.write64(ENABLE, 0xFFFFFFFFull);
    EXPECT_EQ(VALID_MASK, drv.read64(ENABLE));        // reserved bits masked off
    drv.write64(PLIC_ENABLE, 0xFFull);
    EXPECT_EQ(VALID_MASK, drv.read64(PLIC_ENABLE));
    drv.write64(LOCAL_ENABLE, 0xFFull);
    EXPECT_EQ(VALID_MASK, drv.read64(LOCAL_ENABLE));

    drv.write64(CAUSE, 0x5);                          // SW may set CAUSE
    EXPECT_EQ(0x5u, drv.read64(CAUSE));
    drv.write64(CAUSE, 0xF);                          // only [2:0] kept
    EXPECT_EQ(0x7u, drv.read64(CAUSE));

    drv.write64(PHYS_ADDR, 0x1234);                   // read-only -> ignored
    EXPECT_EQ(0u, drv.read64(PHYS_ADDR));
    std::cout << "  [PASS] register RW/RO/masking\n";

    // ----------------------------------------------------------------------
    // 3. Basic error injection: accrued status + CAUSE/PHYS_ADDR latch
    // ----------------------------------------------------------------------
    do_reset();
    dut.inject_error(smc::beu_src::DCACHE_UNCORRECTABLE, 0xA5A5A5A5ull);
    settle();
    EXPECT_EQ(M_DEU, drv.read64(ACCRUED_ENABLE));
    EXPECT_EQ(0x7u,  drv.read64(CAUSE));              // deu_error == 7
    EXPECT_EQ(0xA5A5A5A5ull, drv.read64(PHYS_ADDR));
    std::cout << "  [PASS] basic error injection\n";

    // ----------------------------------------------------------------------
    // 4. First-error latching: CAUSE/PHYS_ADDR hold until SW clears CAUSE
    // ----------------------------------------------------------------------
    dut.inject_error(smc::beu_src::ICACHE_TLBUS, 0xBEEF);
    settle();
    EXPECT_EQ(M_DEU | M_ITL, drv.read64(ACCRUED_ENABLE)); // accrued accumulates
    EXPECT_EQ(0x7u, drv.read64(CAUSE));               // CAUSE still first error
    EXPECT_EQ(0xA5A5A5A5ull, drv.read64(PHYS_ADDR));  // address unchanged

    drv.write64(CAUSE, 0);                            // re-arm recording
    drv.write64(ACCRUED_ENABLE, 0);                   // clear accrued status
    dut.inject_error(smc::beu_src::ICACHE_TLBUS, 0xC0DE);
    settle();
    EXPECT_EQ(0x1u,   drv.read64(CAUSE));             // itl_error == 1
    EXPECT_EQ(0xC0DEull, drv.read64(PHYS_ADDR));
    std::cout << "  [PASS] first-error latching / re-arm\n";

    // ----------------------------------------------------------------------
    // 5. Recording-enable gating: accrued still sets, CAUSE does not
    // ----------------------------------------------------------------------
    do_reset();
    drv.write64(ENABLE, 0);                           // disable all recording
    dut.inject_error(smc::beu_src::DCACHE_CORRECTABLE, 0xDEAD);
    settle();
    EXPECT_EQ(M_DEC, drv.read64(ACCRUED_ENABLE));     // raw accrued: always set
    EXPECT_EQ(0u,    drv.read64(CAUSE));              // recording gated off
    EXPECT_EQ(0u,    drv.read64(PHYS_ADDR));
    std::cout << "  [PASS] recording-enable gating\n";

    // ----------------------------------------------------------------------
    // 6. Local (NMI-like) interrupt + PLIC interrupt aggregation
    // ----------------------------------------------------------------------
    do_reset();
    drv.write64(LOCAL_ENABLE, M_DEU);                 // local mask: dcache uncorr
    dut.inject_error(smc::beu_src::DCACHE_UNCORRECTABLE, 0x10);
    settle();
    EXPECT_TRUE(irq_local.read());                    // local asserted
    EXPECT_TRUE(!irq_plic.read());                    // PLIC still masked

    drv.write64(PLIC_ENABLE, M_DEU);                  // now also PLIC
    settle();
    EXPECT_TRUE(irq_plic.read());

    drv.write64(ACCRUED_ENABLE, 0);                   // SW ack clears status
    settle();
    EXPECT_TRUE(!irq_local.read());
    EXPECT_TRUE(!irq_plic.read());

    // Software can replace accrued bits, including setting a previously clear bit.
    drv.write64(ACCRUED_ENABLE, M_DEU);
    settle();
    EXPECT_EQ(M_DEU, drv.read64(ACCRUED_ENABLE));
    EXPECT_TRUE(irq_local.read());
    EXPECT_TRUE(irq_plic.read());
    std::cout << "  [PASS] local + PLIC interrupt aggregation\n";

    // ----------------------------------------------------------------------
    // 7. Per-source interrupt mask selectivity
    // ----------------------------------------------------------------------
    do_reset();
    drv.write64(PLIC_ENABLE, M_ITL);                  // only ICache TileLink
    dut.inject_error(smc::beu_src::DCACHE_CORRECTABLE, 0x20);
    settle();
    EXPECT_EQ(M_DEC, drv.read64(ACCRUED_ENABLE));
    EXPECT_TRUE(!irq_plic.read());                    // masked source
    dut.inject_error(smc::beu_src::ICACHE_TLBUS, 0x24);
    settle();
    EXPECT_TRUE(irq_plic.read());                     // unmasked source raises
    std::cout << "  [PASS] per-source mask selectivity\n";

    // ----------------------------------------------------------------------
    // 8. transport_dbg peek is side-effect-free
    // ----------------------------------------------------------------------
    do_reset();
    dut.inject_error(smc::beu_src::DCACHE_TLBUS, 0x777);
    settle();
    {
        uint64_t v = 0;
        EXPECT_EQ(8u, drv.dbg_read(CAUSE, v));
        EXPECT_EQ(0x5u, v);                           // dtl_error == 5
        EXPECT_EQ(8u, drv.dbg_read(ACCRUED_ENABLE, v));
        EXPECT_EQ(M_DTL, v);
        // Peeking must not disturb state.
        EXPECT_EQ(0x5u, drv.read64(CAUSE));
        EXPECT_EQ(M_DTL, drv.read64(ACCRUED_ENABLE));
        EXPECT_EQ(0x777ull, drv.read64(PHYS_ADDR));
        EXPECT_EQ(M_DTL, dut.dbg_reg(ACCRUED_ENABLE));
        EXPECT_EQ(0u, dut.dbg_reg(0x30)); // unmapped offset -> 0
    }
    std::cout << "  [PASS] transport_dbg / dbg_reg peek\n";

    // ----------------------------------------------------------------------
    // 9. CCI introspection
    // ----------------------------------------------------------------------
    {
        auto broker = cci::cci_get_broker();
        auto h_d = broker.get_param_handle("tb.beu.access_delay_ns");
        EXPECT_TRUE(h_d.is_valid());
        EXPECT_TRUE(h_d.get_cci_value().get_double() == 7.0); // preset
        EXPECT_TRUE(h_d.is_preset_value());
        h_d.set_cci_value(cci::cci_value(3.0));
        EXPECT_TRUE(h_d.get_cci_value().get_double() == 3.0);

        std::cout << "  CCI parameters:\n";
        for (auto& h : broker.get_param_handles()) {
            std::cout << "    " << std::left << std::setw(30) << h.name()
                      << " = " << std::setw(8) << h.get_cci_value().to_json()
                      << (h.is_preset_value() ? " [preset]" : " [default]") << "\n";
        }
        std::cout << "  [PASS] CCI introspection\n";
    }

    dut.dump_state(std::cout);

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.beu.access_delay_ns", cci::cci_value(7.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
