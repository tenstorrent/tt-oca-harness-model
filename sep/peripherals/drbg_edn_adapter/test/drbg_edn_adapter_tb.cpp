// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// Frontdoor checks for drbg_axis_edn_adapter:
//   * only a fully strobed 32-bit beat is accepted
//   * clear_i drops tready and discards staged entropy
//   * one endpoint's cancel does not steal or corrupt a sibling
//   * a cancelled endpoint stays out of arbitration on the cycle cancel
//     drops, then rejoins
//   * round-robin deals one staged word per requesting endpoint
//   * FIPS (tuser) is forwarded with the beat

#include <systemc>
#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "drbg_edn_adapter.h"

namespace {

int g_failures = 0;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " #cond "\n";                                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected=" << +_e << " actual=" << +_a << "\n";    \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

struct tb : sc_core::sc_module {
    static constexpr unsigned N = 4;

    sc_core::sc_clock clk{"clk", 10, sc_core::SC_NS};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> clear{"clear"};
    sc_core::sc_signal<bool> cancel[N];
    sc_core::sc_signal<bool> tvalid{"tvalid"};
    sc_core::sc_signal<uint32_t> tdata{"tdata"};
    sc_core::sc_signal<uint32_t> tstrb{"tstrb"};
    sc_core::sc_signal<bool> tuser{"tuser"};
    sc_core::sc_signal<bool> tready{"tready"};
    sc_core::sc_signal<bool> edn_req[N];
    sc_core::sc_signal<bool> edn_ack[N];
    sc_core::sc_signal<uint32_t> edn_bus[N];
    sc_core::sc_signal<bool> edn_fips[N];

    sep::drbg_edn_adapter dut{"dut"};

    // Second instance sized by the CCI preset tb.dut2.num_endpoints = 2.
    sep::drbg_edn_adapter dut2{"dut2"};
    sc_core::sc_signal<bool> clr2{"clr2"};
    sc_core::sc_signal<bool> tv2{"tv2"};
    sc_core::sc_signal<uint32_t> td2{"td2"};
    sc_core::sc_signal<uint32_t> ts2{"ts2"};
    sc_core::sc_signal<bool> tu2{"tu2"};
    sc_core::sc_signal<bool> tr2{"tr2"};
    sc_core::sc_signal<bool> cancel2[2];
    sc_core::sc_signal<bool> req2[2];
    sc_core::sc_signal<bool> ack2_o[2];
    sc_core::sc_signal<uint32_t> bus2[2];
    sc_core::sc_signal<bool> fips2[2];

    SC_HAS_PROCESS(tb);
    explicit tb(sc_core::sc_module_name name) : sc_core::sc_module(name)
    {
        dut2.clk_i(clk);
        dut2.rst_ni(rst_n);
        dut2.clear_i(clr2);
        dut2.tvalid_i(tv2);
        dut2.tdata_i(td2);
        dut2.tstrb_i(ts2);
        dut2.tuser_i(tu2);
        dut2.tready_o(tr2);
        ts2.write(0xFu);
        for (unsigned i = 0; i < 2; ++i) {
            dut2.endpoint_cancel_i[i](cancel2[i]);
            dut2.edn_req_i[i](req2[i]);
            dut2.edn_ack_o[i](ack2_o[i]);
            dut2.edn_bus_o[i](bus2[i]);
            dut2.edn_fips_o[i](fips2[i]);
        }

        dut.clk_i(clk);
        dut.rst_ni(rst_n);
        dut.clear_i(clear);
        dut.tvalid_i(tvalid);
        dut.tdata_i(tdata);
        dut.tstrb_i(tstrb);
        dut.tuser_i(tuser);
        dut.tready_o(tready);
        for (unsigned i = 0; i < N; ++i) {
            dut.endpoint_cancel_i[i](cancel[i]);
            dut.edn_req_i[i](edn_req[i]);
            dut.edn_ack_o[i](edn_ack[i]);
            dut.edn_bus_o[i](edn_bus[i]);
            dut.edn_fips_o[i](edn_fips[i]);
        }
        SC_THREAD(run);
    }

    void settle()
    {
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
    }

    void tick()
    {
        wait(clk.posedge_event());
        settle();
    }

    void reset()
    {
        rst_n.write(false);
        clear.write(false);
        tvalid.write(false);
        tdata.write(0);
        tstrb.write(0xFu);
        tuser.write(false);
        for (unsigned i = 0; i < N; ++i) {
            cancel[i].write(false);
            edn_req[i].write(false);
        }
        tick();
        tick();
        rst_n.write(true);
        // One cycle of under_rst, then the ack state machine reaches idle.
        tick();
        tick();
        tick();
    }

    // Hold a beat until the adapter takes it, or report that it was refused.
    bool offer(uint32_t data, bool fips, unsigned budget = 6)
    {
        tdata.write(data);
        tuser.write(fips);
        tstrb.write(0xFu);
        tvalid.write(true);
        for (unsigned i = 0; i < budget; ++i) {
            settle();
            if (tready.read()) {
                tick();
                tvalid.write(false);
                return true;
            }
            tick();
        }
        tvalid.write(false);
        return false;
    }

    // Sample the first ack on `ep`. Returns false on timeout.
    bool wait_ack(unsigned ep, uint32_t& bus, bool& fips, unsigned budget = 12)
    {
        edn_req[ep].write(true);
        for (unsigned i = 0; i < budget; ++i) {
            tick();
            if (edn_ack[ep].read()) {
                bus = edn_bus[ep].read();
                fips = edn_fips[ep].read();
                edn_req[ep].write(false);
                return true;
            }
        }
        edn_req[ep].write(false);
        return false;
    }

    void run()
    {
        std::cout << "==== DRBG AXIS-to-EDN adapter ====\n";
        EXPECT_EQ(N, dut.num_endpoints());

        // Partial strobe is refused. A following full beat is the one delivered.
        reset();
        tdata.write(0x11111111u);
        tuser.write(true);
        tstrb.write(0x7u);
        tvalid.write(true);
        settle();
        EXPECT_EQ(false, tready.read());
        tick();
        tvalid.write(false);
        EXPECT(offer(0xA5A5A5A5u, true));
        uint32_t bus = 0;
        bool fips = false;
        EXPECT(wait_ack(0, bus, fips));
        EXPECT_EQ(0xA5A5A5A5u, bus);
        EXPECT_EQ(true, fips);
        for (unsigned i = 1; i < N; ++i)
            EXPECT_EQ(false, edn_ack[i].read());
        std::cout << "  [PASS] full strobe only; FIPS forwarded; siblings quiet\n";

        // The first cycle out of reset is under_rst: tready may be high but
        // the staging FIFO does not accept the beat.
        reset();
        rst_n.write(false);
        tick();
        rst_n.write(true);
        tdata.write(0xD0D0D0D0u);
        tuser.write(false);
        tstrb.write(0xFu);
        tvalid.write(true);
        settle();
        EXPECT_EQ(true, tready.read());
        tick();  // under_rst consumes the handshake and drops the beat
        tvalid.write(false);
        tick();
        tick();
        edn_req[0].write(true);
        bool any_ack = false;
        for (unsigned i = 0; i < 8; ++i) {
            tick();
            any_ack = any_ack || edn_ack[0].read() || edn_bus[0].read() != 0u;
        }
        EXPECT(!any_ack);  // the stage is empty: nothing to deliver at all
        edn_req[0].write(false);
        tick();
        // Positive control: the next beat after under_rst is delivered.
        EXPECT(offer(0xD1D1D1D1u, false));
        EXPECT(wait_ack(0, bus, fips));
        EXPECT_EQ(0xD1D1D1D1u, bus);
        std::cout << "  [PASS] reset-release cycle does not stage a beat\n";

        // clear_i drops tready immediately and the staged word is gone.
        reset();
        EXPECT(offer(0x22222222u, false));
        clear.write(true);
        settle();
        EXPECT_EQ(false, tready.read());
        tick();
        clear.write(false);
        tick();
        EXPECT(offer(0x33333333u, false));
        EXPECT(wait_ack(1, bus, fips));
        EXPECT_EQ(0x33333333u, bus);
        EXPECT_EQ(false, fips);
        EXPECT_EQ(false, edn_ack[0].read());
        std::cout << "  [PASS] clear flushes the stage and drops tready\n";

        // Four beats fill the stage. The fifth is refused until one is consumed.
        reset();
        for (uint32_t w = 1; w <= 4; ++w)
            EXPECT(offer(0x1000u + w, false));
        tdata.write(0x1005u);
        tstrb.write(0xFu);
        tvalid.write(true);
        settle();
        EXPECT_EQ(false, tready.read());
        tick();
        tvalid.write(false);
        EXPECT(wait_ack(0, bus, fips));
        EXPECT_EQ(0x1001u, bus);
        settle();
        tvalid.write(true);
        tdata.write(0x1005u);
        settle();
        EXPECT_EQ(true, tready.read());
        tick();
        tvalid.write(false);
        std::cout << "  [PASS] stage depth 4 back-pressures, then accepts again\n";

        // Round-robin over three requesters and three staged words. Each gets
        // exactly one word, in index order, and nobody is served twice.
        reset();
        EXPECT(offer(0xAAu, true));
        EXPECT(offer(0xBBu, false));
        EXPECT(offer(0xEEu, true));
        {
            const uint32_t want[3] = {0xAAu, 0xBBu, 0xEEu};
            const bool want_fips[3] = {true, false, true};
            unsigned acks[3] = {0, 0, 0};
            uint32_t got[3] = {0, 0, 0};
            bool got_fips[3] = {false, false, false};
            unsigned order[3] = {9, 9, 9};
            unsigned n_done = 0;
            for (unsigned e = 0; e < 3; ++e) edn_req[e].write(true);
            for (unsigned i = 0; i < 24 && n_done < 3; ++i) {
                tick();
                for (unsigned e = 0; e < 3; ++e) {
                    if (!edn_ack[e].read()) continue;
                    ++acks[e];
                    if (acks[e] == 1) {
                        got[e] = edn_bus[e].read();
                        got_fips[e] = edn_fips[e].read();
                        order[n_done++] = e;
                    }
                    edn_req[e].write(false);
                }
            }
            for (unsigned e = 0; e < 3; ++e) edn_req[e].write(false);
            tick();
            tick();
            for (unsigned e = 0; e < 3; ++e) {
                EXPECT_EQ(1u, acks[e]);
                EXPECT_EQ(want[e], got[e]);
                EXPECT_EQ(want_fips[e], got_fips[e]);
                EXPECT_EQ(e, order[e]);
            }
            EXPECT_EQ(false, edn_ack[3].read());
        }
        std::cout << "  [PASS] round-robin gives each requester its own beat\n";

        // Fairness under continuous demand: endpoints 0 and 1 hold their
        // requests while words arrive one at a time. prim_arbiter_ppc
        // alternates 0,1,0,1; a fixed-priority arbiter would starve 1.
        reset();
        edn_req[0].write(true);
        edn_req[1].write(true);
        {
            unsigned who[4] = {9, 9, 9, 9};
            uint32_t what[4] = {0, 0, 0, 0};
            for (unsigned w = 0; w < 4; ++w) {
                EXPECT(offer(0x3000u + w, false));
                for (unsigned i = 0; i < 10 && who[w] == 9; ++i) {
                    tick();
                    for (unsigned e = 0; e < 2; ++e) {
                        if (edn_ack[e].read()) {
                            who[w] = e;
                            what[w] = edn_bus[e].read();
                        }
                    }
                }
                tick();
                tick();
            }
            for (unsigned w = 0; w < 4; ++w) {
                EXPECT_EQ(w % 2u, who[w]);
                EXPECT_EQ(0x3000u + w, what[w]);
            }
        }
        edn_req[0].write(false);
        edn_req[1].write(false);
        std::cout << "  [PASS] round-robin alternates under continuous demand\n";

        // Cancel endpoint 0 while both 0 and 1 request. Endpoint 1 still gets
        // the staged beat; endpoint 0 never acks.
        reset();
        EXPECT(offer(0xCCu, true));
        edn_req[0].write(true);
        edn_req[1].write(true);
        cancel[0].write(true);
        bool ep0_acked = false;
        bool ep1_acked = false;
        uint32_t sib = 0;
        bool sib_fips = false;
        for (unsigned i = 0; i < 8; ++i) {
            tick();
            if (edn_ack[0].read()) ep0_acked = true;
            if (edn_ack[1].read()) {
                ep1_acked = true;
                sib = edn_bus[1].read();
                sib_fips = edn_fips[1].read();
            }
        }
        EXPECT(!ep0_acked);
        EXPECT(ep1_acked);
        EXPECT_EQ(0xCCu, sib);
        EXPECT_EQ(true, sib_fips);
        edn_req[0].write(false);
        edn_req[1].write(false);
        cancel[0].write(false);
        std::cout << "  [PASS] cancel keeps one endpoint out; sibling served\n";

        // A cancelled endpoint does not drain the stage, including the first
        // cycle after cancel drops (ep_flush_q). The stage is full, so tready
        // shows exactly when the endpoint takes a word.
        //   baseline: a live requester drains a word on the first edge.
        reset();
        for (uint32_t w = 1; w <= 4; ++w) EXPECT(offer(0x2000u + w, false));
        settle();
        EXPECT_EQ(false, tready.read());
        edn_req[0].write(true);
        tick();
        EXPECT_EQ(true, tready.read());
        edn_req[0].write(false);
        //   cancelled: held out while cancel is high and for one more edge.
        reset();
        for (uint32_t w = 1; w <= 4; ++w) EXPECT(offer(0x2000u + w, false));
        cancel[0].write(true);
        edn_req[0].write(true);
        for (unsigned i = 0; i < 3; ++i) {
            tick();
            EXPECT_EQ(false, tready.read());
            EXPECT_EQ(false, edn_ack[0].read());
        }
        cancel[0].write(false);
        tick();  // ep_flush_q is still 1 on this edge
        EXPECT_EQ(false, tready.read());
        tick();  // rejoined: the word is granted
        EXPECT_EQ(true, tready.read());
        edn_req[0].write(false);
        std::cout << "  [PASS] cancelled endpoint stays out until the cycle after cancel\n";

        // A word already delivered to endpoint 0 (it stays in the holding FIFO
        // until the next request) is cleared by cancel, not just masked.
        reset();
        EXPECT(offer(0x5A5A0001u, true));
        EXPECT(wait_ack(0, bus, fips));
        EXPECT_EQ(0x5A5A0001u, bus);
        tick();
        EXPECT_EQ(0x5A5A0001u, edn_bus[0].read());  // held, not yet popped
        EXPECT_EQ(true, edn_fips[0].read());
        cancel[0].write(true);
        settle();
        EXPECT_EQ(0u, edn_bus[0].read());           // forced low during cancel
        EXPECT_EQ(false, edn_fips[0].read());
        EXPECT_EQ(false, edn_ack[0].read());
        tick();
        cancel[0].write(false);
        tick();
        tick();
        EXPECT_EQ(0u, edn_bus[0].read());           // the held word is gone
        EXPECT_EQ(false, edn_fips[0].read());
        std::cout << "  [PASS] cancel flushes the endpoint's held word\n";

        // Cancelling endpoint 0 does not disturb endpoint 1's in-flight word:
        // the word was granted to endpoint 1 before the cancel arrives.
        reset();
        EXPECT(offer(0x5A5A0002u, true));
        EXPECT(offer(0x5A5A0003u, false));
        EXPECT(wait_ack(0, bus, fips));       // endpoint 0 holds word 2
        EXPECT_EQ(0x5A5A0002u, bus);
        edn_req[1].write(true);
        tick();                               // word 3 granted to endpoint 1
        cancel[0].write(true);
        bool sib_ack = false;
        for (unsigned i = 0; i < 4 && !sib_ack; ++i) {
            tick();
            EXPECT_EQ(0u, edn_bus[0].read());
            if (edn_ack[1].read()) {
                sib_ack = true;
                EXPECT_EQ(0x5A5A0003u, edn_bus[1].read());
                EXPECT_EQ(false, edn_fips[1].read());
            }
        }
        EXPECT(sib_ack);
        edn_req[1].write(false);
        cancel[0].write(false);
        std::cout << "  [PASS] cancel leaves a sibling's in-flight word intact\n";

        // clear_i flushes every endpoint's held word and the stage together.
        reset();
        EXPECT(offer(0x5A5A0004u, false));
        EXPECT(offer(0x5A5A0005u, false));
        EXPECT(wait_ack(2, bus, fips));
        EXPECT_EQ(0x5A5A0004u, bus);
        tick();
        EXPECT_EQ(0x5A5A0004u, edn_bus[2].read());
        clear.write(true);
        settle();
        EXPECT_EQ(0u, edn_bus[2].read());
        EXPECT_EQ(false, tready.read());
        tick();
        clear.write(false);
        tick();
        tick();
        EXPECT_EQ(0u, edn_bus[2].read());
        edn_req[3].write(true);
        bool stale = false;
        for (unsigned i = 0; i < 8; ++i) {
            tick();
            stale = stale || edn_ack[3].read();
        }
        edn_req[3].write(false);
        EXPECT(!stale);  // word 5 was discarded with the stage
        std::cout << "  [PASS] clear flushes held words and the stage\n";

        // CCI num_endpoints sizes the ports; a 2-endpoint instance works end to end.
        EXPECT_EQ(2u, dut2.num_endpoints());
        EXPECT_EQ(2u, static_cast<unsigned>(dut2.edn_req_i.size()));
        EXPECT_EQ(2u, static_cast<unsigned>(dut2.edn_ack_o.size()));
        EXPECT_EQ(2u, static_cast<unsigned>(dut2.endpoint_cancel_i.size()));
        tv2.write(true);
        td2.write(0x77777777u);
        tick();
        tv2.write(false);
        req2[1].write(true);
        bool ack2 = false;
        for (unsigned i = 0; i < 8 && !ack2; ++i) {
            tick();
            if (ack2_o[1].read()) {
                ack2 = true;
                EXPECT_EQ(0x77777777u, bus2[1].read());
            }
        }
        req2[1].write(false);
        EXPECT(ack2);
        EXPECT_EQ(false, ack2_o[0].read());
        std::cout << "  [PASS] CCI num_endpoints=2 instance\n";

        if (g_failures == 0)
            std::cout << "\nALL TESTS PASSED\n";
        else
            std::cout << "\n" << g_failures << " FAILURE(S)\n";
        sc_core::sc_stop();
    }
};

}  // namespace

// `--bad-endpoints N`: elaborate one adapter with num_endpoints = N in its own
// process. Exits 0 only when elaboration reports SC_FATAL with the range
// message. The fatal is cached instead of thrown: an SC_THROW from the
// uninstrumented SystemC library is not catchable in an ASan binary, and
// abort() then fails CTest as a signal.
int sc_main(int argc, char** argv)
{
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);
    cci::cci_originator cfg("tb_cfg");
    auto broker = cci::cci_get_global_broker(cfg);

    if (argc == 3 && std::string(argv[1]) == "--bad-endpoints") {
        const unsigned n = static_cast<unsigned>(std::stoul(argv[2]));
        broker.set_preset_cci_value("bad.num_endpoints", cci::cci_value(n));
        sc_core::sc_report_handler::set_actions(
            sc_core::SC_FATAL,
            sc_core::SC_DISPLAY | sc_core::SC_CACHE_REPORT);
        sep::drbg_edn_adapter bad("bad");
        const sc_core::sc_report* r = sc_core::sc_report_handler::get_cached_report();
        const std::string msg = r ? r->get_msg() : "(none)";
        sc_core::sc_report_handler::clear_cached_report();
        const bool ok = msg.find("num_endpoints must be in 1..32") != std::string::npos;
        std::cout << (ok ? "PASS" : "FAIL") << ": num_endpoints=" << n
                  << " fatal: " << msg << "\n";
        return ok ? 0 : 1;
    }

    broker.set_preset_cci_value("tb.dut2.num_endpoints", cci::cci_value(2u));
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
