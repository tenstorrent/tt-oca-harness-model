// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// plic_wide_tb.cpp -- default 336-source PLIC sweep. Pins stay on sc_vector;
// only word/id boundaries are raised (not a flattened 336-pin unroll).

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <iostream>

#include "plic.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected=" << _e << " actual=" << _a << "\n";      \
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

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEF;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE)
            ++g_failures;
        return data;
    }

    void write32(uint64_t addr, uint32_t value)
    {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE)
            ++g_failures;
    }
};

constexpr uint64_t prio_addr(unsigned src)
{
    return smc::plic_cfg::PRIORITY_BASE + 4 * src;
}
constexpr uint64_t pending_addr(unsigned word)
{
    return smc::plic_cfg::PENDING_BASE + 4 * word;
}
constexpr uint64_t enable_addr(unsigned ctx, unsigned word)
{
    return smc::plic_cfg::ENABLE_BASE
         + ctx * smc::plic_cfg::ENABLE_STRIDE + 4 * word;
}
constexpr uint64_t thr_addr(unsigned ctx)
{
    return smc::plic_cfg::CONTEXT_BASE
         + ctx * smc::plic_cfg::CONTEXT_STRIDE
         + smc::plic_cfg::CONTEXT_THR_OFF;
}
constexpr uint64_t cc_addr(unsigned ctx)
{
    return smc::plic_cfg::CONTEXT_BASE
         + ctx * smc::plic_cfg::CONTEXT_STRIDE
         + smc::plic_cfg::CONTEXT_CC_OFF;
}

struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_sources_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_contexts_p_;

    smc::plic dut;
    driver    drv;

    sc_core::sc_signal<bool>                     rst_n;
    sc_core::sc_vector<sc_core::sc_signal<bool>> src_sig;
    sc_core::sc_vector<sc_core::sc_signal<bool>> ctx_sig;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , num_sources_p_("num_sources", 336u, "must match plic.num_sources")
        , num_contexts_p_("num_contexts", 8u, "must match plic.num_contexts")
        , dut("plic")
        , drv("drv")
        , rst_n("rst_n")
        , src_sig("src_sig", num_sources_p_.get_value())
        , ctx_sig("ctx_sig", num_contexts_p_.get_value())
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        for (unsigned i = 0; i < num_sources_p_.get_value(); ++i)
            dut.src_in[i](src_sig[i]);
        for (unsigned c = 0; c < num_contexts_p_.get_value(); ++c)
            dut.ctx_out[c](ctx_sig[c]);
        SC_THREAD(run);
    }

    static void settle()
    {
        for (int i = 0; i < 2; ++i)
            sc_core::wait(SC_ZERO_TIME);
    }

    void raise(unsigned src, bool level = true)
    {
        src_sig[src - 1].write(level);
        settle();
    }

    void run()
    {
        std::cout << "==== SMC PLIC wide TB (" << num_sources_p_.get_value()
                  << " sources) ====\n";
        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);

        EXPECT_EQ(336u, num_sources_p_.get_value());
        EXPECT_EQ(336u, dut.src_in.size());

        const unsigned sources[] = {1u, 31u, 32u, 33u, 63u, 64u, 65u,
                                    255u, 256u, 335u, 336u};
        for (unsigned src : sources) {
            drv.write32(prio_addr(src), 5);
            drv.write32(thr_addr(0), 0);
            drv.write32(enable_addr(0, src / 32), 1u << (src & 31));
            raise(src, true);
            EXPECT_TRUE(dut.dbg_pending(src));
            const uint32_t pword = drv.read32(pending_addr(src / 32));
            EXPECT_TRUE((pword & (1u << (src & 31))) != 0);
            settle();
            EXPECT_TRUE(ctx_sig[0].read());
            const uint32_t claimed = drv.read32(cc_addr(0));
            EXPECT_EQ(src, claimed);
            raise(src, false);
            drv.write32(cc_addr(0), src);
            EXPECT_TRUE(!dut.dbg_pending(src));
            drv.write32(enable_addr(0, src / 32), 0);
            drv.write32(prio_addr(src), 0);
            settle();
            if (g_failures)
                break;
        }
        EXPECT_TRUE(!dut.dbg_pending(337));
        std::cout << "  [PASS] 336-source boundary sweep\n";

        if (g_failures == 0)
            std::cout << "\nALL TESTS PASSED\n";
        else
            std::cout << "\n" << g_failures << " FAILURE(S)\n";
        sc_core::sc_stop();
    }
};

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
