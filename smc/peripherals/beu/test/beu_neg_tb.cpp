// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file beu_neg_tb.cpp
 * @brief Negative-path / edge-case test bench for the SMC Bus Error Unit.
 *
 * Exercises the error and rarely-hit branches to complement beu_tb.cpp and
 * push line coverage above 95 %:
 *   - TLM response codes: COMMAND / BURST / ADDRESS errors, in-window decode
 *     miss (unmapped 8-aligned offset), misalignment.
 *   - transport_dbg: bad length, misalignment, out-of-window, ignore command,
 *     valid read/write, unmapped-offset write rejection.
 *   - Read-only PHYS_ADDR write-ignore through the normal path.
 */

#include "beu.h"

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <cstring>
#include <iostream>

using sc_core::sc_time;
using sc_core::SC_NS;

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

constexpr uint64_t CAUSE = 0x00, PHYS_ADDR = 0x08, ENABLE = 0x10;

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver, 64> sock;
    explicit driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {}

    // Raw b_transport returning the response status (no auto-fail).
    tlm::tlm_response_status xact(tlm::tlm_command cmd, uint64_t addr,
                                  unsigned len, uint64_t data = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t(sc_core::SC_ZERO_TIME);
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(len);
        gp.set_streaming_width(len ? len : 1);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, unsigned len,
                 uint64_t* data) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len ? len : 1);
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

    void run();
};

void tb::run()
{
    // Reset once.
    rstn.write(false); sc_core::wait(sc_time(1, SC_NS));
    rstn.write(true);  sc_core::wait(sc_time(1, SC_NS));

    // ---- b_transport response codes -------------------------------------
    EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
              drv.xact(tlm::TLM_IGNORE_COMMAND, CAUSE, 8));
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              drv.xact(tlm::TLM_READ_COMMAND, CAUSE, 4));  // wrong width
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.xact(tlm::TLM_READ_COMMAND, 0x1000, 8)); // out of window
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.xact(tlm::TLM_READ_COMMAND, 0x4, 8));    // misaligned
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.xact(tlm::TLM_READ_COMMAND, 0x30, 8));   // in-window decode miss
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.xact(tlm::TLM_WRITE_COMMAND, 0x38, 8, 0xF)); // decode miss (write)
    EXPECT_EQ(tlm::TLM_OK_RESPONSE,
              drv.xact(tlm::TLM_READ_COMMAND, ENABLE, 8));
    std::cout << "  [PASS] b_transport response codes\n";

    // ---- Read-only PHYS_ADDR write is accepted but ignored --------------
    EXPECT_EQ(tlm::TLM_OK_RESPONSE,
              drv.xact(tlm::TLM_WRITE_COMMAND, PHYS_ADDR, 8, 0xDEAD));
    {
        uint64_t v = 0xFF;
        drv.dbg(tlm::TLM_READ_COMMAND, PHYS_ADDR, 8, &v);
        EXPECT_EQ(0u, v);
    }
    std::cout << "  [PASS] RO PHYS_ADDR write ignored\n";

    // ---- transport_dbg edge cases ---------------------------------------
    uint64_t v = 0;
    EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, CAUSE, 4, &v));    // bad length
    EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0x4, 8, &v));      // misaligned
    EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0x1000, 8, &v));   // out of window
    EXPECT_EQ(0u, drv.dbg(tlm::TLM_IGNORE_COMMAND, CAUSE, 8, &v));  // ignore cmd
    EXPECT_EQ(0u, drv.dbg(tlm::TLM_WRITE_COMMAND, 0x30, 8, &v));    // unmapped write

    // Valid dbg read and write of a real register.
    EXPECT_EQ(8u, drv.dbg(tlm::TLM_READ_COMMAND, ENABLE, 8, &v));
    EXPECT_EQ(0xE6u, v);                                            // reset ENABLE
    v = 0x24;
    EXPECT_EQ(8u, drv.dbg(tlm::TLM_WRITE_COMMAND, ENABLE, 8, &v));
    v = 0;
    drv.dbg(tlm::TLM_READ_COMMAND, ENABLE, 8, &v);
    EXPECT_EQ(0x24u, v);
    std::cout << "  [PASS] transport_dbg edge cases\n";

    // ---- Injection with a masked-off source keeps CAUSE clear -----------
    drv.xact(tlm::TLM_WRITE_COMMAND, ENABLE, 8, 0); // disable recording
    dut.inject_error(smc::beu_src::ICACHE_CORRECTABLE, 0x99);
    sc_core::wait(sc_time(1, SC_NS));
    EXPECT_EQ(0u, dut.dbg_reg(CAUSE));
    EXPECT_EQ(uint64_t{1} << 2, dut.dbg_reg(0x20)); // ACCRUED still set
    std::cout << "  [PASS] gated injection\n";

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
    static cci_utils::consuming_broker broker("GlobalBroker");
    cci::cci_register_broker(broker);
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
