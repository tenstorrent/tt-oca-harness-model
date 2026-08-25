// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// plic_tb.cpp -- self-checking test bench for the SMC PLIC (CCI-compliant).
//
// CCI integration highlights
// ─────────────────────────────────────────────────────────────────────────
// • sc_main registers a global CCI broker before any module is constructed.
// • Preset values are injected for tb.plic.num_sources (336 → 64) and
//   tb.plic.access_delay_ns (2 ns → 5 ns) to demonstrate pre-construction
//   override.  tb.plic.num_contexts is left at its default (8).
// • The tb struct declares matching cci_param members (num_sources_p_,
//   num_contexts_p_) that receive the same presets for signal-array sizing.
// • Test 21 exercises CCI introspection: param lookup by name, typed and
//   untyped handles, description, metadata, originator tracking, and
//   run-time mutation of the mutable access_delay_ns parameter.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

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
                      << "  expected=" << _e << " actual=" << _a               \
                      << "  (" #expected " == " #actual ")\n";                 \
            ++g_failures;                                                      \
        }                                                                     \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                     \
    } while (0)

// Tiny TLM driver -- mimics what the CPU cluster bus bridge would issue.
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEF;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write32(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << addr
                      << ", 0x" << value << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    // Returns the raw response status without failing the test on error --
    // used for the negative-path checks.
    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t  len, uint32_t* data) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

// ---------------------------------------------------------------------------
// Address arithmetic helpers
// ---------------------------------------------------------------------------
constexpr uint64_t prio_addr(unsigned src) {
    return smc::plic_cfg::PRIORITY_BASE + 4 * src;
}
constexpr uint64_t pending_addr(unsigned word) {
    return smc::plic_cfg::PENDING_BASE + 4 * word;
}
constexpr uint64_t enable_addr(unsigned ctx, unsigned word) {
    return smc::plic_cfg::ENABLE_BASE
         + ctx * smc::plic_cfg::ENABLE_STRIDE + 4 * word;
}
constexpr uint64_t thr_addr(unsigned ctx) {
    return smc::plic_cfg::CONTEXT_BASE
         + ctx * smc::plic_cfg::CONTEXT_STRIDE
         + smc::plic_cfg::CONTEXT_THR_OFF;
}
constexpr uint64_t cc_addr(unsigned ctx) {
    return smc::plic_cfg::CONTEXT_BASE
         + ctx * smc::plic_cfg::CONTEXT_STRIDE
         + smc::plic_cfg::CONTEXT_CC_OFF;
}

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    // ------------------------------------------------------------------
    // CCI sizing parameters — declared FIRST so they are initialised
    // before dut and the sc_vector signal arrays below.
    //
    // These mirror tb.plic.num_sources / tb.plic.num_contexts.  sc_main
    // sets presets for BOTH so that the testbench signal arrays and the
    // DUT port arrays are always consistent.
    // ------------------------------------------------------------------
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_sources_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_contexts_p_;

    smc::plic dut;
    driver    drv;

    sc_core::sc_signal<bool>                     rst_n;
    sc_core::sc_vector<sc_core::sc_signal<bool>> src_sig;
    sc_core::sc_vector<sc_core::sc_signal<bool>> ctx_sig;

    explicit tb(sc_module_name n)
        : sc_module(n)
        // TB sizing params pick up presets set in sc_main (tb.num_sources etc.)
        , num_sources_p_("num_sources", 336u,
                         "TB signal-array size — must match plic.num_sources preset.")
        , num_contexts_p_("num_contexts", 8u,
                          "TB signal-array size — must match plic.num_contexts preset.")
        // DUT is constructed after CCI params; its own presets are resolved here.
        , dut("plic")
        , drv("drv")
        , rst_n  ("rst_n")
        , src_sig("src_sig", num_sources_p_.get_value())
        , ctx_sig("ctx_sig", num_contexts_p_.get_value())
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        for (unsigned i = 0; i < num_sources_p_.get_value();  ++i)
            dut.src_in[i] (src_sig[i]);
        for (unsigned c = 0; c < num_contexts_p_.get_value(); ++c)
            dut.ctx_out[c](ctx_sig[c]);

        SC_THREAD(run);
    }

    // Convenience accessors avoid verbose casts in the test body.
    unsigned num_sources()  const { return num_sources_p_.get_value();  }
    unsigned num_contexts() const { return num_contexts_p_.get_value(); }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
    }

    void raise(unsigned src, bool level = true) {
        src_sig[src - 1].write(level);
        settle();
    }

    // Wait long enough for any pending recompute_event_ notifications to
    // fire and for output_method to drive ctx_out.  Two delta cycles:
    // one for src_method / b_transport to call schedule_recompute(), one
    // for output_method to drive ctx_out.
    static void settle() {
        for (int i = 0; i < 2; ++i) sc_core::wait(sc_core::SC_ZERO_TIME);
    }

    void run() {
        std::cout << "==== SMC PLIC TB (CCI-compliant) ====\n";
        std::cout << "  DUT topology: "
                  << num_sources()  << " sources / "
                  << num_contexts() << " contexts\n";

        // ------------------------------------------------------------------
        // 1. Reset clears state.
        // ------------------------------------------------------------------
        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        pulse_reset();
        for (unsigned s = 1; s <= num_sources(); ++s) {
            EXPECT_EQ(0u, drv.read32(prio_addr(s)));
            if (g_failures) break;
        }
        for (unsigned c = 0; c < num_contexts(); ++c) {
            EXPECT_EQ(0u, drv.read32(thr_addr(c)));
            EXPECT_EQ(false, ctx_sig[c].read());
        }
        std::cout << "  [PASS] reset clears state\n";

        // ------------------------------------------------------------------
        // 2. Priority R/W with 3-bit truncation.
        // ------------------------------------------------------------------
        drv.write32(prio_addr(7), 0x12345678); // only [2:0] survive
        EXPECT_EQ(0u, drv.read32(prio_addr(7)) & ~0x7u);
        EXPECT_EQ(0x0u, drv.read32(prio_addr(7))); // 0x8 & 0x7 == 0
        drv.write32(prio_addr(7), 5);
        EXPECT_EQ(5u, drv.read32(prio_addr(7)));
        std::cout << "  [PASS] priority R/W + 3-bit truncation\n";

        // ------------------------------------------------------------------
        // 3. Source 0 is reserved -- writes are ignored, reads as 0.
        // ------------------------------------------------------------------
        drv.write32(prio_addr(0), 7);
        EXPECT_EQ(0u, drv.read32(prio_addr(0)));
        std::cout << "  [PASS] source 0 reserved\n";

        // ------------------------------------------------------------------
        // 4. Pending bit is set on rising edge of source line.
        // ------------------------------------------------------------------
        EXPECT_EQ(false, dut.dbg_pending(7));
        raise(7, true);
        EXPECT_EQ(true,  dut.dbg_pending(7));
        // PENDING register layout: 32 sources per word
        const uint32_t pword = drv.read32(pending_addr(7 / 32));
        EXPECT_TRUE((pword & (1u << (7 & 31))) != 0);
        std::cout << "  [PASS] pending latched on rising edge\n";

        // ------------------------------------------------------------------
        // 5. Without enable, no context output asserts.
        // ------------------------------------------------------------------
        settle();
        for (unsigned c = 0; c < num_contexts(); ++c)
            EXPECT_EQ(false, ctx_sig[c].read());
        std::cout << "  [PASS] disabled source does not drive ctx_out\n";

        // ------------------------------------------------------------------
        // 6. Enable source 7 in context 0; ctx_out[0] must rise; others stay low.
        // ------------------------------------------------------------------
        drv.write32(enable_addr(/*ctx*/0, /*word*/0), 1u << 7);
        settle();
        EXPECT_EQ(true,  ctx_sig[0].read());
        for (unsigned c = 1; c < num_contexts(); ++c)
            EXPECT_EQ(false, ctx_sig[c].read());
        std::cout << "  [PASS] per-context enable independence\n";

        // ------------------------------------------------------------------
        // 7. Threshold gating -- raise threshold above source priority.
        // ------------------------------------------------------------------
        drv.write32(thr_addr(0), 6); // priority 5 <= threshold 6 -> masked
        settle();
        EXPECT_EQ(false, ctx_sig[0].read());
        drv.write32(thr_addr(0), 4); // priority 5 > threshold 4 -> visible
        settle();
        EXPECT_EQ(true, ctx_sig[0].read());
        std::cout << "  [PASS] threshold gating\n";

        // ------------------------------------------------------------------
        // 8. Best-pending arbitration: priority then source-id tie-break.
        // ------------------------------------------------------------------
        drv.write32(prio_addr(11), 6); // higher prio than #7
        drv.write32(prio_addr(13), 5); // tie with #7, but higher id
        drv.write32(enable_addr(0, 0), (1u << 7) | (1u << 11) | (1u << 13));
        raise(11, true);
        raise(13, true);
        EXPECT_EQ(11u, dut.dbg_claim_top(0)); // prio 6 wins
        // Now drop #11 to expose tie between #7 and #13 at prio 5.
        drv.write32(prio_addr(11), 0); // disable via priority
        EXPECT_EQ(7u, dut.dbg_claim_top(0));  // lower id wins tie
        std::cout << "  [PASS] best-pending arbitration\n";

        // ------------------------------------------------------------------
        // 9. Claim / complete cycle.
        // ------------------------------------------------------------------
        drv.write32(prio_addr(13), 0);
        settle();
        EXPECT_EQ(7u, dut.dbg_claim_top(0));

        const uint32_t claimed = drv.read32(cc_addr(0));
        EXPECT_EQ(7u,    claimed);
        EXPECT_EQ(false, dut.dbg_pending(7));
        settle();
        EXPECT_EQ(false, ctx_sig[0].read());

        // Source line is still high -- complete must re-arm it.
        drv.write32(cc_addr(0), 7);
        EXPECT_EQ(true, dut.dbg_pending(7));
        settle();
        EXPECT_EQ(true, ctx_sig[0].read());
        std::cout << "  [PASS] claim/complete with line still high\n";

        // ------------------------------------------------------------------
        // 10. Claim again, drop the line, then complete -- must NOT re-pend.
        // ------------------------------------------------------------------
        EXPECT_EQ(7u, drv.read32(cc_addr(0)));
        raise(7, false); // line goes low
        drv.write32(cc_addr(0), 7);
        EXPECT_EQ(false, dut.dbg_pending(7));
        settle();
        EXPECT_EQ(false, ctx_sig[0].read());
        std::cout << "  [PASS] complete after line de-asserted\n";

        // ------------------------------------------------------------------
        // 11. Misaligned / oversize accesses produce error response.
        // ------------------------------------------------------------------
        uint32_t scratch = 0;
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x2, 4, &scratch));
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 8, &scratch));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND,
                               smc::plic_cfg::WINDOW_SIZE + 4, 4, &scratch));
        std::cout << "  [PASS] TLM error responses\n";

        // ------------------------------------------------------------------
        // 12. transport_dbg back-door read of claim/complete must NOT claim.
        // ------------------------------------------------------------------
        raise(7, true);                       // pending again
        settle();
        EXPECT_EQ(true,  dut.dbg_pending(7));

        tlm::tlm_generic_payload gp_dbg;
        uint32_t dbg_data = 0;
        gp_dbg.set_command(tlm::TLM_READ_COMMAND);
        gp_dbg.set_address(cc_addr(0));
        gp_dbg.set_data_ptr(reinterpret_cast<uint8_t*>(&dbg_data));
        gp_dbg.set_data_length(4);
        unsigned n = drv.sock->transport_dbg(gp_dbg);
        EXPECT_EQ(4u, n);
        EXPECT_EQ(7u, dbg_data);
        EXPECT_EQ(true, dut.dbg_pending(7)); // still pending: dbg did not claim
        std::cout << "  [PASS] transport_dbg has no side effects\n";

        // ------------------------------------------------------------------
        // 13. Cross-context isolation: enable #7 in ctx 4 (S-mode of core 2),
        //     ensure it fires there too with its own threshold.
        // ------------------------------------------------------------------
        drv.write32(enable_addr(4, 0), 1u << 7);
        drv.write32(thr_addr(4), 0);
        settle();
        EXPECT_EQ(true, ctx_sig[4].read());
        // Raising ctx-4 threshold above prio must mute ctx 4 only.
        drv.write32(thr_addr(4), 7);
        settle();
        EXPECT_EQ(false, ctx_sig[4].read());
        EXPECT_EQ(true,  ctx_sig[0].read()); // ctx 0 still fires
        std::cout << "  [PASS] cross-context isolation\n";

        // ------------------------------------------------------------------
        // 14. Reset returns to all-zero state.
        // ------------------------------------------------------------------
        pulse_reset();
        for (unsigned c = 0; c < num_contexts(); ++c)
            EXPECT_EQ(false, ctx_sig[c].read());
        EXPECT_EQ(0u, drv.read32(prio_addr(7)));
        EXPECT_EQ(0u, drv.read32(enable_addr(0, 0)));
        std::cout << "  [PASS] reset returns to clean state\n";

        // ------------------------------------------------------------------
        // 15. Debug back-door API.
        // ------------------------------------------------------------------
        drv.write32(prio_addr(3), 5);
        drv.write32(enable_addr(/*ctx*/0, /*word*/0), 1u << 3);
        drv.write32(thr_addr(0), 2);

        EXPECT_EQ(5u,   dut.dbg_priority(3));
        EXPECT_EQ(true, dut.dbg_enable(0, 3));
        EXPECT_EQ(2u,   dut.dbg_threshold(0));

        EXPECT_EQ(0u,    dut.dbg_priority(0));
        EXPECT_EQ(0u,    dut.dbg_priority(num_sources() + 1));
        EXPECT_EQ(false, dut.dbg_enable(num_contexts(), 3));
        EXPECT_EQ(false, dut.dbg_enable(0, 0));
        EXPECT_EQ(false, dut.dbg_enable(0, num_sources() + 1));
        EXPECT_EQ(0u,    dut.dbg_threshold(num_contexts()));

        {
            std::ostringstream oss;
            dut.dump_state(oss);
            EXPECT_TRUE(!oss.str().empty());
        }
        std::cout << "  [PASS] debug back-door API\n";

        // ------------------------------------------------------------------
        // 16. Write to PENDING region is silently discarded (SW=r per rdl).
        // ------------------------------------------------------------------
        drv.write32(pending_addr(0), 0xFFFFFFFF);
        EXPECT_EQ(0u, drv.read32(pending_addr(0)));
        std::cout << "  [PASS] PENDING write silently discarded\n";

        // ------------------------------------------------------------------
        // 17. Context-block reserved sub-offset: RAZ on read, WI on write.
        // ------------------------------------------------------------------
        {
            const uint64_t reserved_ctx =
                smc::plic_cfg::CONTEXT_BASE + 0x8; // suboff = 8
            EXPECT_EQ(0u, drv.read32(reserved_ctx));
            drv.write32(reserved_ctx, 0xDEAD);
            EXPECT_EQ(0u, drv.read32(reserved_ctx));
        }
        std::cout << "  [PASS] context-block reserved sub-offset (RAZ/WI)\n";

        // ------------------------------------------------------------------
        // 18. b_transport: TLM_COMMAND_ERROR_RESPONSE for unknown command.
        // ------------------------------------------------------------------
        {
            uint32_t s2 = 0;
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0, 4, &s2));
        }
        std::cout << "  [PASS] TLM_COMMAND_ERROR_RESPONSE\n";

        // ------------------------------------------------------------------
        // 19. transport_dbg: general read and write paths.
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t v = 0;

            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(prio_addr(3));
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&v));
            gp.set_data_length(4);
            EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(5u, v);

            v = 6;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(prio_addr(5));
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&v));
            EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(6u, dut.dbg_priority(5));
        }
        std::cout << "  [PASS] transport_dbg general read and write\n";

        // ------------------------------------------------------------------
        // 20. Enable register last-word bit masking.
        // ------------------------------------------------------------------
        {
            const unsigned W         = (num_sources() + 1u + 31u) / 32u;
            const unsigned last_word = W - 1u;
            drv.write32(enable_addr(/*ctx*/0, last_word), 0xFFFFFFFFu);
            const uint32_t valid_bits = (num_sources() + 1u) - last_word * 32u;
            const uint32_t expected   = (valid_bits < 32u)
                                            ? ((1u << valid_bits) - 1u)
                                            : 0xFFFFFFFFu;
            EXPECT_EQ(expected, drv.read32(enable_addr(0, last_word)));
        }
        std::cout << "  [PASS] enable last-word bit masking\n";

        // ------------------------------------------------------------------
        // 21. CCI parameter introspection.
        //
        // Verifies that:
        //   • All three DUT CCI params are findable by hierarchical name.
        //   • Typed handles deliver correct values and preset/default flags.
        //   • Descriptions are non-empty (documentation embedded in params).
        //   • Metadata entries injected by the DUT constructor are readable.
        //   • Originator tracking records who set the value.
        //   • The mutable access_delay_ns param can be changed at run-time.
        //   • Broker enumeration lists all CCI params in this simulation.
        // ------------------------------------------------------------------
        {
            auto broker = cci::cci_get_broker();

            // ── Typed handle for num_sources (IMMUTABLE, PRESET) ──────────
            auto h_src = broker.get_param_handle<unsigned>("tb.plic.num_sources");
            EXPECT_TRUE(h_src.is_valid());
            EXPECT_EQ(64u, h_src.get_value());          // preset was 64
            EXPECT_TRUE(h_src.is_preset_value());
            EXPECT_TRUE(!h_src.get_description().empty());

            // Metadata injected by plic constructor
            cci::cci_value_map meta_src = h_src.get_metadata();
            EXPECT_TRUE(meta_src.has_entry("rdl_field"));
            EXPECT_TRUE(meta_src.has_entry("fw_define"));
            EXPECT_TRUE(meta_src.has_entry("valid_range"));

            // ── num_contexts (IMMUTABLE, DEFAULT — no preset was set) ─────
            auto h_ctx = broker.get_param_handle<unsigned>("tb.plic.num_contexts");
            EXPECT_TRUE(h_ctx.is_valid());
            EXPECT_EQ(8u, h_ctx.get_value());           // default unchanged
            EXPECT_TRUE(h_ctx.is_default_value());
            EXPECT_TRUE(!h_ctx.get_description().empty());

            // ── access_delay_ns (MUTABLE, PRESET) ────────────────────────
            auto h_delay = broker.get_param_handle<double>("tb.plic.access_delay_ns");
            EXPECT_TRUE(h_delay.is_valid());
            EXPECT_EQ(5.0, h_delay.get_value());        // preset was 5.0
            EXPECT_TRUE(h_delay.is_preset_value());
            EXPECT_TRUE(!h_delay.get_description().empty());

            // Metadata for access_delay_ns
            cci::cci_value_map meta_delay = h_delay.get_metadata();
            EXPECT_TRUE(meta_delay.has_entry("unit"));
            EXPECT_TRUE(meta_delay.has_entry("tlm_phase"));

            // ── Originator tracking ───────────────────────────────────────
            std::string origin = h_src.get_value_origin().name();
            EXPECT_TRUE(!origin.empty());

            // ── Run-time mutation of mutable param ───────────────────────
            h_delay.set_value(10.0);
            EXPECT_EQ(10.0, h_delay.get_value());
            h_delay.set_value(5.0);  // restore

            // ── TB sizing params also visible via broker ──────────────────
            auto h_tb_src = broker.get_param_handle<unsigned>("tb.num_sources");
            EXPECT_TRUE(h_tb_src.is_valid());
            EXPECT_EQ(64u, h_tb_src.get_value());
            EXPECT_TRUE(h_tb_src.is_preset_value());

            // ── Enumerate and print all CCI params in this simulation ─────
            std::cout << "\n  CCI Parameter Snapshot:\n";
            std::cout << "  " << std::string(72, '-') << "\n";
            std::cout << "  " << std::left
                      << std::setw(38) << "Name"
                      << std::setw(12) << "Value"
                      << "Origin\n";
            std::cout << "  " << std::string(72, '-') << "\n";
            for (auto& h : broker.get_param_handles()) {
                std::string pname = h.name();
                std::cout << "  " << std::left << std::setw(38) << pname
                          << std::setw(12) << h.get_cci_value().to_json()
                          << (h.is_preset_value()  ? "[preset]"  : "")
                          << (h.is_default_value() ? "[default]" : "")
                          << "\n";
            }
            std::cout << "  " << std::string(72, '-') << "\n";
            std::cout << "  Value origin of tb.plic.num_sources: "
                      << origin << "\n\n";

            std::cout << "  [PASS] CCI parameter introspection\n";
        }

        if (g_failures == 0) {
            std::cout << "\nALL TESTS PASSED\n";
        } else {
            std::cout << "\n" << g_failures << " FAILURE(S)\n";
        }
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    // ── CCI: register global broker ──────────────────────────────────────
    // Must happen before ANY cci_param is constructed.
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    // ── CCI: inject preset values before tb / plic are constructed ───────
    // Presets override the default values hard-coded in the DUT and TB.
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    // DUT: override num_sources (336 → 64) and access_delay_ns (2 ns → 5 ns).
    // num_contexts is intentionally left at the default (8) to show mixed use.
    global_broker.set_preset_cci_value(
        "tb.plic.num_sources",   cci::cci_value(64u));
    global_broker.set_preset_cci_value(
        "tb.plic.access_delay_ns", cci::cci_value(5.0));

    // TB sizing params must match the DUT overrides.
    global_broker.set_preset_cci_value(
        "tb.num_sources", cci::cci_value(64u));
    // tb.num_contexts: no preset → default 8, consistent with DUT default.

    // ── Instantiate and run ───────────────────────────────────────────────
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
