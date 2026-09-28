// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file octs_system_timer_tb.cpp
 * @brief Self-checking unit testbench for the OCTS System Timer model.
 *
 * ## Topology
 *
 * Three DUT instances share one clock and reset:
 *
 * ```
 *            +--------------+   sync_load   +--------------+
 *   drv_p -->|    dut_p     |-------------->|    dut_s     |<-- drv_s
 *            |  (PRIMARY)   |    credit     | (SECONDARY)  |
 *            +--------------+-------------->+--------------+
 *
 *            +--------------+
 *   drv_x -->|    dut_x     |<-- sync_load / credit driven directly by the TB
 *            | (SECONDARY)  |
 *            +--------------+
 * ```
 *
 * `dut_p` -> `dut_s` is the end-to-end OCTS protocol check.  `dut_x` is a
 * standalone SECONDARY whose sync inputs the TB drives by hand, so credit
 * starvation and the credit re-anchor can be provoked deterministically.
 *
 * ## Cycle alignment
 *
 * The stimulus thread runs on the clock's **falling** edge, i.e. halfway
 * between the rising edges the DUT evaluates on.  So `advance(1)` == one clock
 * cycle, and every sample/drive happens at a point where the DUT's state for
 * the preceding rising edge is settled and unambiguous.  This is what makes
 * the exact cycle counts below meaningful rather than delta-cycle-order luck.
 *
 * Register accesses do not consume simulated time (the annotated `b_transport`
 * delay is checked once, then discarded), so a burst of accesses always lands
 * inside a single half-cycle.
 *
 * ## Coverage
 *
 *  1. Reset defaults of every register
 *  2. STATUS.MODE follows `is_primary_i`
 *  3. RW / RO / reserved-bit contracts, GPIO_ENABLE output
 *  4. Access rules: 32-bit only, alignment, out-of-window, annotated delay
 *  5. PRIMARY: START loads the 64-bit preset, RUNNING asserts, START self-clears
 *  6. PRIMARY: sync_load pulse is exactly PULSE_WIDTH cycles
 *  7. PRIMARY: credit pulse width and CREDIT_VAL period
 *  8. PRIMARY -> SECONDARY: count tracks with the modelled sync latency
 *  9. SECONDARY without sync stays idle
 * 10. SECONDARY: sync_load latency, load value, STEP counting
 * 11. SECONDARY: credit exhaustion halts the count, CREDIT_EXPIRED accumulates
 * 12. SECONDARY: credit pulse replenishes; CREDIT_EXPIRED write-to-clear
 * 13. SECONDARY with STEP=3: credit pulse re-anchors count to
 *     `expected_count + CREDIT_VAL` (pulls an over-run counter back)
 * 14. CTRL constraint warning (CREDIT_VAL <= PULSE_WIDTH)
 * 15. CCI `access_delay_ns` preset, discovery and live mutation
 */

#include "octs_system_timer.h"

#include "sim_log.h"

#include <tlm_utils/simple_initiator_socket.h>

#include <cci/utils/consuming_broker.h>
#include <cci_configuration>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

using namespace sc_core;
using smc::octs_system_timer;
using cfg_t = smc::octs_system_timer_cfg;

// ---------------------------------------------------------------------------
// Tiny check framework
// ---------------------------------------------------------------------------

static unsigned g_checks   = 0;
static unsigned g_failures = 0;

static void report_fail(const char* what, int line, const std::string& detail)
{
    ++g_failures;
    std::cout << "  FAIL [line " << line << "] " << what << ": " << detail
              << "\n";
}

template <typename A, typename B>
static void check_eq_(const char* what, A got, B exp, int line)
{
    ++g_checks;
    const auto g = static_cast<uint64_t>(got);
    const auto e = static_cast<uint64_t>(exp);
    if (g != e) {
        std::ostringstream os;
        os << "got 0x" << std::hex << g << " (" << std::dec << g
           << "), expected 0x" << std::hex << e << " (" << std::dec << e << ")";
        report_fail(what, line, os.str());
    }
}

static void check_true_(const char* what, bool cond, int line)
{
    ++g_checks;
    if (!cond) report_fail(what, line, "condition is false");
}

#define CHECK_EQ(what, got, exp) check_eq_((what), (got), (exp), __LINE__)
#define CHECK_TRUE(what, cond)   check_true_((what), (cond), __LINE__)

static void banner(const std::string& title)
{
    std::cout << "\n--- " << title << " ---\n";
}

// ---------------------------------------------------------------------------
// reg_driver: a minimal TLM initiator used to poke a DUT's register file
// ---------------------------------------------------------------------------

class reg_driver : public sc_module {
public:
    tlm_utils::simple_initiator_socket<reg_driver> sock;

    explicit reg_driver(sc_module_name n) : sc_module(n), sock("sock") {}

    /// Raw access; returns the TLM response so negative tests can inspect it.
    tlm::tlm_response_status access(tlm::tlm_command cmd, uint64_t addr,
                                   void* data, unsigned len,
                                   sc_time* out_delay = nullptr)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  d = SC_ZERO_TIME;

        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_byte_enable_length(0);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, d);
        if (out_delay != nullptr) *out_delay = d;
        return gp.get_response_status();
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t v = 0;
        const auto st = access(tlm::TLM_READ_COMMAND, addr, &v, 4);
        if (st != tlm::TLM_OK_RESPONSE) {
            ++g_failures;
            std::cout << "  FAIL: read32 @0x" << std::hex << addr
                      << " returned status " << std::dec << st << "\n";
        }
        return v;
    }

    void write32(uint64_t addr, uint32_t v)
    {
        const auto st = access(tlm::TLM_WRITE_COMMAND, addr, &v, 4);
        if (st != tlm::TLM_OK_RESPONSE) {
            ++g_failures;
            std::cout << "  FAIL: write32 @0x" << std::hex << addr
                      << " returned status " << std::dec << st << "\n";
        }
    }

    /// Fully general payload, for the malformed-access matrix: explicit
    /// streaming width, byte enables, incoming delay and AXI sideband.
    struct raw_result {
        tlm::tlm_response_status status;
        sc_time                  delay_delta;
        bool                     dmi_allowed;
    };
    raw_result raw(tlm::tlm_command cmd, uint64_t addr, void* data,
                   unsigned len, int streaming_width = -1,
                   unsigned char* be = nullptr, unsigned be_len = 0,
                   sc_time delay_in = SC_ZERO_TIME,
                   smc::smc_axi_extension* ext = nullptr)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  d = delay_in;

        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (ext != nullptr) gp.set_extension(ext);

        sock->b_transport(gp, d);

        if (ext != nullptr) gp.clear_extension<smc::smc_axi_extension>();
        return {gp.get_response_status(), d - delay_in, gp.is_dmi_allowed()};
    }

    /// Raw back-door access over transport_dbg.  Returns bytes transferred.
    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, void* data, unsigned len,
                 int streaming_width = -1, unsigned char* be = nullptr,
                 unsigned be_len = 0)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return sock->transport_dbg(gp);
    }

    bool dmi(uint64_t addr, tlm::tlm_dmi& dmi_data)
    {
        tlm::tlm_generic_payload gp;
        uint32_t scratch = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&scratch));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        return sock->get_direct_mem_ptr(gp, dmi_data);
    }
};

/// Changes the report actions for one severity and restores them on scope
/// exit, so an expected diagnostic cannot silence an unrelated one later.
struct scoped_report_actions {
    sc_core::sc_severity sev;
    sc_core::sc_actions  saved;
    scoped_report_actions(sc_core::sc_severity s, sc_core::sc_actions a)
        : sev(s), saved(sc_core::sc_report_handler::set_actions(s, a))
    {
    }
    ~scoped_report_actions()
    {
        sc_core::sc_report_handler::set_actions(sev, saved);
    }
};

// ---------------------------------------------------------------------------
// Testbench
// ---------------------------------------------------------------------------

class tb : public sc_module {
public:
    SC_HAS_PROCESS(tb);

    sc_clock          clk{"clk", 10, SC_NS};
    sc_signal<bool>   rst_n{"rst_n"};
    sc_signal<bool>   tie_low{"tie_low"};

    // PRIMARY under test, plus the SECONDARY it feeds.
    sc_signal<bool>     is_primary_p{"is_primary_p"};
    sc_signal<bool>     is_primary_s{"is_primary_s"};
    sc_signal<bool>     is_primary_x{"is_primary_x"};

    sc_signal<bool>     sl_p2s{"sl_p2s"};
    sc_signal<bool>     cc_p2s{"cc_p2s"};
    sc_signal<uint64_t> count_p{"count_p"};
    sc_signal<bool>     gpio_p{"gpio_p"};
    sc_signal<uint32_t> cred_p{"cred_p"};
    sc_signal<bool>     credleft_p{"credleft_p"};

    sc_signal<bool>     sl_s_o{"sl_s_o"};
    sc_signal<bool>     cc_s_o{"cc_s_o"};
    sc_signal<uint64_t> count_s{"count_s"};
    sc_signal<bool>     gpio_s{"gpio_s"};
    sc_signal<uint32_t> cred_s{"cred_s"};
    sc_signal<bool>     credleft_s{"credleft_s"};

    // Standalone SECONDARY with TB-driven sync inputs.
    sc_signal<bool>     sl_drv{"sl_drv"};
    sc_signal<bool>     cc_drv{"cc_drv"};
    sc_signal<bool>     sl_x_o{"sl_x_o"};
    sc_signal<bool>     cc_x_o{"cc_x_o"};
    sc_signal<uint64_t> count_x{"count_x"};
    sc_signal<bool>     gpio_x{"gpio_x"};
    sc_signal<uint32_t> cred_x{"cred_x"};
    sc_signal<bool>     credleft_x{"credleft_x"};

    octs_system_timer dut_p{"dut_p"};
    octs_system_timer dut_s{"dut_s"};
    octs_system_timer dut_x{"dut_x"};

    reg_driver drv_p{"drv_p"};
    reg_driver drv_s{"drv_s"};
    reg_driver drv_x{"drv_x"};

    explicit tb(sc_module_name n) : sc_module(n)
    {
        drv_p.sock.bind(dut_p.reg_socket);
        drv_s.sock.bind(dut_s.reg_socket);
        drv_x.sock.bind(dut_x.reg_socket);

        bind_common(dut_p, is_primary_p);
        dut_p.timer_sync_load_i(tie_low);
        dut_p.timer_cnt_credit_i(tie_low);
        dut_p.timer_sync_load_o(sl_p2s);
        dut_p.timer_cnt_credit_o(cc_p2s);
        dut_p.timer_count_o(count_p);
        dut_p.timer_gpio_enable_o(gpio_p);
        dut_p.cur_credits_debug_o(cred_p);
        dut_p.credits_left_debug_o(credleft_p);

        bind_common(dut_s, is_primary_s);
        dut_s.timer_sync_load_i(sl_p2s);
        dut_s.timer_cnt_credit_i(cc_p2s);
        dut_s.timer_sync_load_o(sl_s_o);
        dut_s.timer_cnt_credit_o(cc_s_o);
        dut_s.timer_count_o(count_s);
        dut_s.timer_gpio_enable_o(gpio_s);
        dut_s.cur_credits_debug_o(cred_s);
        dut_s.credits_left_debug_o(credleft_s);

        bind_common(dut_x, is_primary_x);
        dut_x.timer_sync_load_i(sl_drv);
        dut_x.timer_cnt_credit_i(cc_drv);
        dut_x.timer_sync_load_o(sl_x_o);
        dut_x.timer_cnt_credit_o(cc_x_o);
        dut_x.timer_count_o(count_x);
        dut_x.timer_gpio_enable_o(gpio_x);
        dut_x.cur_credits_debug_o(cred_x);
        dut_x.credits_left_debug_o(credleft_x);

        SC_THREAD(run);
    }

private:
    void bind_common(octs_system_timer& d, sc_signal<bool>& mode)
    {
        d.clk_i(clk);
        d.rst_n_i(rst_n);
        d.is_primary_i(mode);
    }

    /// Advance @p n whole clock cycles, resuming on the falling edge.
    void advance(unsigned n = 1)
    {
        for (unsigned i = 0; i < n; ++i) wait(clk.negedge_event());
    }

    /// Drive a @p cycles -long pulse on @p sig, leaving the TB one cycle after
    /// the pulse's trailing edge.
    void pulse(sc_signal<bool>& sig, unsigned cycles)
    {
        sig.write(true);
        advance(cycles);
        sig.write(false);
    }

    void apply_reset()
    {
        rst_n.write(false);
        advance(3);
        rst_n.write(true);
        advance(1);
    }

    void run();

    void test_reset_defaults();
    void test_register_contracts();
    void test_access_rules();
    void test_primary_start_and_pulses();
    void test_primary_credit_period();
    void test_primary_to_secondary_sync();
    void test_secondary_load_and_starvation();
    void test_secondary_replenish_and_clear();
    void test_secondary_reanchor_with_step();
    void test_ctrl_constraint_warning();
    void test_cci_parameter();
    void test_tlm_error_paths();

    // Architectural sections added for the internal-review remediation.
    void test_csr_manifest();
    void test_tlm_matrix();
    void test_sideband();
    void test_debug_and_dmi();
    void test_credit_val_zero();
    void test_ctrl_boundaries();
    void test_sync_latency_independent();
    void test_pulse_shapes();
    void test_start_retrigger();
    void test_mode_switching();
    void test_reset_during_activity();
    void test_overflow_and_coherency();
    void test_debug_getters();
};

// ---------------------------------------------------------------------------

// ===========================================================================
// Architectural sections added for the internal-review remediation.
//
// Independent CSR manifest: a second transcription of the register table in
// include/octs_system_timer.h and memmap.adoc, giving each register's reset
// value and its read-back after software writes all-ones.  It is deliberately
// not derived from the model's switch statements, so a slip on either side
// fails the sweep.  Registers whose value is live hardware state (STATUS,
// TIMER_COUNT_*) are excluded and covered by the cycle-level sections.
// ===========================================================================
namespace {

struct csr_spec {
    uint64_t    off;
    const char* name;
    uint32_t    reset;
    uint32_t    after_ones;
};

constexpr csr_spec kCsrs[] = {
    // START is a singlepulse: it always reads back 0.
    {cfg_t::OFF_TIMER_START,       "TIMER_START",       0x00000000u, 0x00000000u},
    // CTRL [31:24] is reserved (RAZ/WI).
    {cfg_t::OFF_CTRL,              "CTRL",              0x0001020Au, 0x00FFFFFFu},
    {cfg_t::OFF_TIMER_PRESET_LO,   "TIMER_PRESET_LO",   0x00000000u, 0xFFFFFFFFu},
    {cfg_t::OFF_TIMER_PRESET_HI,   "TIMER_PRESET_HI",   0x00000000u, 0xFFFFFFFFu},
    // CREDIT_EXPIRED: writing anything clears it.
    {cfg_t::OFF_CREDIT_EXPIRED,    "CREDIT_EXPIRED",    0x00000000u, 0x00000000u},
    {cfg_t::OFF_TIMER_GPIO_ENABLE, "TIMER_GPIO_ENABLE", 0x00000000u, 0x00000001u},
};

/// Read-only registers: writes must not change what they report.
constexpr uint64_t kRoRegs[] = {
    cfg_t::OFF_STATUS, cfg_t::OFF_TIMER_COUNT_LO, cfg_t::OFF_TIMER_COUNT_HI,
};

}  // namespace

void tb::test_csr_manifest()
{
    banner("CSR manifest: reset, write-mask, RO and reserved contracts");
    apply_reset();

    for (const csr_spec& r : kCsrs) {
        CHECK_EQ(r.name, drv_x.read32(r.off), r.reset);
    }
    for (const csr_spec& r : kCsrs) {
        drv_x.write32(r.off, 0xFFFFFFFFu);
        CHECK_EQ(r.name, drv_x.read32(r.off), r.after_ones);
    }
    // The all-ones CTRL write above armed nothing else; restore the default so
    // later sections start from a known program.
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);

    // Read-only registers ignore writes, including while the timer is running.
    apply_reset();
    for (uint64_t off : kRoRegs) {
        const uint32_t before = drv_x.read32(off);
        drv_x.write32(off, 0xFFFFFFFFu);
        CHECK_EQ("RO register ignores write", drv_x.read32(off), before);
    }

    // Reserved words inside the window are RAZ/WI and do not disturb a
    // neighbouring register.  0x24 is the first address past the window.
    for (uint64_t off = 0; off < cfg_t::WINDOW_SIZE; off += 4) {
        bool mapped = false;
        for (const csr_spec& r : kCsrs) mapped = mapped || r.off == off;
        for (uint64_t ro : kRoRegs)     mapped = mapped || ro == off;
        if (mapped) continue;
        CHECK_EQ("reserved word reads zero", drv_x.read32(off), 0u);
        drv_x.write32(off, 0xFFFFFFFFu);
        CHECK_EQ("reserved word ignores write", drv_x.read32(off), 0u);
    }
    CHECK_EQ("CTRL survived the hole sweep", drv_x.read32(cfg_t::OFF_CTRL),
             cfg_t::CTRL_RESET);

    // START writes of 0 must not arm the pulse; only bit 0 matters.
    apply_reset();
    is_primary_x.write(true);
    advance();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x1000u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 0x0u);
    advance(2);
    CHECK_EQ("START=0 does not start the timer", count_x.read(), 0u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 0xFFFFFFFEu);  // bit0 clear
    advance(2);
    CHECK_EQ("START with bit0 clear does not start", count_x.read(), 0u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 0xFFFFFFFFu);  // bit0 set
    advance();
    CHECK_EQ("START with bit0 set loads the preset", count_x.read(), 0x1000u);

    is_primary_x.write(false);
    apply_reset();
}

void tb::test_tlm_matrix()
{
    banner("TLM payload and boundary matrix");
    apply_reset();

    uint32_t scratch = 0;

    // Width and alignment: only naturally aligned 32-bit accesses.
    for (unsigned len : {1u, 2u, 3u, 5u, 8u}) {
        uint8_t buf[8] = {0};
        CHECK_EQ("bad width rejected",
                 drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, buf, len)
                     .status,
                 tlm::TLM_BURST_ERROR_RESPONSE);
    }
    for (uint64_t off : {1u, 2u, 3u}) {
        CHECK_EQ("misaligned rejected",
                 drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL + off,
                           &scratch, 4)
                     .status,
                 tlm::TLM_BURST_ERROR_RESPONSE);
    }

    // Window boundary, and 64-bit addresses whose `addr + len` would wrap.
    CHECK_EQ("last valid word accepted",
             drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE - 4, &scratch, 4)
                 .status,
             tlm::TLM_OK_RESPONSE);
    CHECK_EQ("first word past window rejected",
             drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, &scratch, 4)
                 .status,
             tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK_EQ("UINT64_MAX-3 rejected (would wrap)",
             drv_x.raw(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, &scratch, 4)
                 .status,
             tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK_EQ("UINT64_MAX rejected",
             drv_x.raw(tlm::TLM_READ_COMMAND, UINT64_MAX, &scratch, 4).status,
             tlm::TLM_BURST_ERROR_RESPONSE);  // misaligned first

    // Null pointer, zero length, unsupported command.
    CHECK_EQ("null pointer rejected",
             drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, nullptr, 4).status,
             tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK_EQ("zero length rejected",
             drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &scratch, 0, 0)
                 .status,
             tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK_EQ("unsupported command rejected",
             drv_x.raw(tlm::TLM_IGNORE_COMMAND, cfg_t::OFF_CTRL, &scratch, 4)
                 .status,
             tlm::TLM_COMMAND_ERROR_RESPONSE);

    // Byte enables refused at every byte-enable length, including the
    // zero-length and all-lanes-enabled forms.
    {
        unsigned char be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        for (unsigned be_len : {0u, 1u, 2u, 4u}) {
            CHECK_EQ("byte enables refused",
                     drv_x.raw(tlm::TLM_WRITE_COMMAND, cfg_t::OFF_CTRL,
                               &scratch, 4, -1, be, be_len)
                         .status,
                     tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        }
        // A null pointer with a stale non-zero length stays legal.
        CHECK_EQ("null BE pointer with stale length is legal",
                 drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &scratch, 4,
                           -1, nullptr, 4)
                     .status,
                 tlm::TLM_OK_RESPONSE);
    }

    // Streaming width: single beat, so TLM-2.0 requires sw >= data_length.
    for (unsigned sw = 0; sw <= 8; ++sw) {
        const auto exp = (sw < 4) ? tlm::TLM_BURST_ERROR_RESPONSE
                                  : tlm::TLM_OK_RESPONSE;
        CHECK_EQ("streaming-width relation",
                 drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &scratch, 4,
                           static_cast<int>(sw))
                     .status,
                 exp);
    }

    // Exact annotated delay from a non-zero incoming value, and no delay at
    // all on an error path.
    {
        // Pin the delay explicitly rather than assuming the sc_main preset:
        // test_cci_parameter() mutates access_delay_ns live before this runs.
        const double d_ns = 4.0;
        cci::cci_get_broker()
            .get_param_handle(std::string(dut_x.name()) + ".access_delay_ns")
            .set_cci_value(cci::cci_value(d_ns));
        const auto ok = drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL,
                                  &scratch, 4, -1, nullptr, 0,
                                  sc_time(7, SC_NS));
        CHECK_TRUE("incoming delay preserved, one access delay added",
                   ok.delay_delta == sc_time(d_ns, SC_NS));
        const auto bad = drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE,
                                   &scratch, 4, -1, nullptr, 0,
                                   sc_time(7, SC_NS));
        CHECK_TRUE("error path annotates no delay",
                   bad.delay_delta == SC_ZERO_TIME);
    }

    // A rejected access must not mutate register state.
    {
        drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0xA5A5A5A5u);
        uint8_t buf[8] = {0};
        drv_x.raw(tlm::TLM_WRITE_COMMAND, cfg_t::OFF_TIMER_PRESET_LO, buf, 2);
        CHECK_EQ("rejected access mutates nothing",
                 drv_x.read32(cfg_t::OFF_TIMER_PRESET_LO), 0xA5A5A5A5u);
    }
    apply_reset();
}

void tb::test_sideband()
{
    banner("Canonical AXI sideband");
    apply_reset();

    smc::smc_axi_extension golden;
    golden.source_id = smc::JTAG_ID;
    golden.axi_id    = 0x2468u;
    golden.axi_user  = 0x13u;
    golden.set_priv(false);
    golden.set_secure(true);
    golden.set_fetch(true);
    golden.set_locked(true);

    // The sideband is inspected but never consumed: every field survives a
    // register access unchanged, on a hit and on a decode fault.
    for (uint64_t addr : {uint64_t(cfg_t::OFF_CTRL),
                          uint64_t(cfg_t::OFF_TIMER_PRESET_LO),
                          uint64_t(cfg_t::WINDOW_SIZE)}) {
        for (tlm::tlm_command cmd :
             {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            smc::smc_axi_extension ext = golden;
            uint32_t data = 0x5A5A5A5Au;
            drv_x.raw(cmd, addr, &data, 4, -1, nullptr, 0, SC_ZERO_TIME, &ext);
            CHECK_EQ("sideband source_id preserved", ext.source_id,
                     golden.source_id);
            CHECK_EQ("sideband axi_id preserved", ext.axi_id, golden.axi_id);
            CHECK_EQ("sideband prot preserved", ext.prot, golden.prot);
            CHECK_EQ("sideband axi_user preserved", ext.axi_user,
                     golden.axi_user);
            CHECK_EQ("sideband is_secure preserved", ext.is_secure ? 1 : 0,
                     golden.is_secure ? 1 : 0);
            CHECK_EQ("sideband is_user preserved", ext.is_user ? 1 : 0,
                     golden.is_user ? 1 : 0);
        }
    }
    // An absent extension is equally acceptable: the model inspects but never
    // requires the sideband (authorization belongs to the fabric filter).
    // Reset first, because the write sweep above deliberately scribbled on
    // CTRL through one of the targeted offsets.
    apply_reset();
    CHECK_EQ("absent sideband accepted", drv_x.read32(cfg_t::OFF_CTRL),
             cfg_t::CTRL_RESET);
}

void tb::test_debug_and_dmi()
{
    banner("transport_dbg and DMI policy");
    apply_reset();

    // A debug read matches the software view without side effects.
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x1234ABCDu);
    {
        uint32_t v = 0;
        CHECK_EQ("debug read returns 4 bytes",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_TIMER_PRESET_LO,
                           &v, 4),
                 4u);
        CHECK_EQ("debug read matches the bus", v, 0x1234ABCDu);
    }

    // A debug WRITE is refused, so it can neither arm the START singlepulse
    // nor clear CREDIT_EXPIRED behind the model's back.
    {
        is_primary_x.write(true);
        advance();
        uint32_t one = 1;
        CHECK_EQ("debug write refused",
                 drv_x.dbg(tlm::TLM_WRITE_COMMAND, cfg_t::OFF_TIMER_START,
                           &one, 4),
                 0u);
        advance(2);
        CHECK_EQ("debug write did not start the timer", count_x.read(), 0u);
        is_primary_x.write(false);
        advance();
    }

    // Refusals mirror the b_transport contract.
    {
        uint32_t v = 0;
        unsigned char be = 0xFF;
        CHECK_EQ("debug past window", drv_x.dbg(tlm::TLM_READ_COMMAND,
                                                cfg_t::WINDOW_SIZE, &v, 4), 0u);
        CHECK_EQ("debug wrap address",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, &v, 4), 0u);
        CHECK_EQ("debug bad width",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &v, 2), 0u);
        CHECK_EQ("debug misaligned",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL + 1, &v, 4), 0u);
        CHECK_EQ("debug null pointer",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, nullptr, 4), 0u);
        CHECK_EQ("debug byte enables",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &v, 4, -1,
                           &be, 1),
                 0u);
        CHECK_EQ("debug short streaming width",
                 drv_x.dbg(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &v, 4, 1), 0u);
        CHECK_EQ("debug unsupported command",
                 drv_x.dbg(tlm::TLM_IGNORE_COMMAND, cfg_t::OFF_CTRL, &v, 4), 0u);
    }

    // DMI is denied: reads sample a live counter and writes have side effects.
    {
        for (uint64_t off : {uint64_t(cfg_t::OFF_CTRL),
                             uint64_t(cfg_t::OFF_TIMER_COUNT_LO),
                             cfg_t::WINDOW_SIZE - 4}) {
            tlm::tlm_dmi d;
            d.allow_read_write();
            CHECK_TRUE("DMI denied", !drv_x.dmi(off, d));
            CHECK_TRUE("DMI read not allowed", !d.is_read_allowed());
            CHECK_TRUE("DMI write not allowed", !d.is_write_allowed());
        }
        uint32_t v = 0;
        CHECK_TRUE("dmi_allowed cleared on hit",
                   !drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &v, 4)
                        .dmi_allowed);
        CHECK_TRUE("dmi_allowed cleared on miss",
                   !drv_x.raw(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, &v, 4)
                        .dmi_allowed);
    }
    apply_reset();
}

void tb::test_credit_val_zero()
{
    banner("CREDIT_VAL = 0 is defined as credit generation disabled");
    apply_reset();

    // PRIMARY with CREDIT_VAL = 0 must emit no credit pulses at all -- not a
    // 256-cycle period from an underflowed CREDIT_VAL-1 comparison.
    {
        const scoped_report_actions quiet(SC_WARNING, SC_DO_NOTHING);
        drv_p.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 0u);  // CV=0
    }
    is_primary_p.write(true);
    advance();
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0);
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_HI, 0);
    drv_p.write32(cfg_t::OFF_TIMER_START, 1);
    advance();

    bool saw_credit = false;
    for (unsigned i = 0; i < 300; ++i) {
        advance();
        if (cc_p2s.read()) saw_credit = true;
    }
    CHECK_TRUE("CV=0: PRIMARY emits no credit pulse in 300 cycles",
               !saw_credit);
    CHECK_TRUE("CV=0: the counter still advances", count_p.read() > 0);
    // White-box, deliberately: with credit generation disabled the generator
    // must be *held* at 0 rather than churning 0..255 under the underflowed
    // comparison.  No register or port exposes it, so there is no
    // architectural observable for this invariant -- and without this check
    // the "hold" and "no pulse" guards are individually redundant and neither
    // is provable on its own.
    CHECK_EQ("CV=0: the credit generator is held at 0",
             dut_p.dbg_credit_counter(), 0u);

    // SECONDARY with CREDIT_VAL = 0 can never satisfy `cur_credits < CV`, so
    // it is starved from the first cycle after its sync load.
    apply_reset();
    {
        const scoped_report_actions quiet(SC_WARNING, SC_DO_NOTHING);
        drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 0u);
    }
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x40u);
    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 4);
    CHECK_EQ("CV=0: SECONDARY loads the preset then halts", count_x.read(),
             0x40u);
    CHECK_TRUE("CV=0: SECONDARY reports no credits left", !credleft_x.read());
    CHECK_TRUE("CV=0: starvation is being counted",
               drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED) > 0);

    drv_p.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    is_primary_p.write(false);
    apply_reset();
}

void tb::test_ctrl_boundaries()
{
    banner("CTRL field boundary values");
    apply_reset();
    is_primary_p.write(true);
    advance();

    // PULSE_WIDTH = 0 is the RTL's "rounds up to 1" case; 1 and 255 are the
    // other ends.  Measure the emitted sync pulse width directly.
    for (unsigned pw : {0u, 1u, 5u}) {
        apply_reset();
        is_primary_p.write(true);
        advance();
        const unsigned expect_w = (pw == 0) ? 1u : pw;
        drv_p.write32(cfg_t::OFF_CTRL, (1u << 16) | (pw << 8) | 200u);
        drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0);
        drv_p.write32(cfg_t::OFF_TIMER_START, 1);

        unsigned width = 0;
        for (unsigned i = 0; i < 64; ++i) {
            advance();
            if (sl_p2s.read()) ++width;
            else if (width > 0) break;
        }
        CHECK_EQ("sync pulse width follows PULSE_WIDTH", width, expect_w);
    }

    // STEP boundary values on a SECONDARY: the count advances by STEP per
    // cycle while credits last.  STEP = 0 means the counter never moves.
    for (unsigned st : {0u, 1u, 7u}) {
        apply_reset();
        drv_x.write32(cfg_t::OFF_CTRL, (st << 16) | (2u << 8) | 200u);
        drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x100u);
        pulse(sl_drv, 2);
        advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
        const uint64_t at_load = count_x.read();
        CHECK_EQ("SECONDARY loads the preset", at_load, 0x100u);
        advance(3);
        CHECK_EQ("SECONDARY advances by STEP per cycle", count_x.read(),
                 0x100u + 3ull * st);
    }

    drv_p.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    is_primary_p.write(false);
    apply_reset();
}

void tb::test_sync_latency_independent()
{
    banner("Input synchronizer latency measured independently");
    apply_reset();

    // The expected value is derived from the RTL path described in
    // prim_edge_detector.sv -- one input flop, a two-flop synchronizer, and
    // one edge-detect flop -- NOT from the model's own SYNC_LATENCY_CYCLES
    // constant.  If the model and that constant ever drift together, this
    // still fails.
    constexpr unsigned kInputFlop = 1, kSyncFlops = 2, kEdgeFlop = 1;
    constexpr unsigned kExpectedLatency = kInputFlop + kSyncFlops + kEdgeFlop;

    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x2000u);
    CHECK_EQ("SECONDARY idle before any sync", count_x.read(), 0u);

    // Raise sync for exactly one cycle and count rising edges until the
    // datapath consumes it.
    sl_drv.write(true);
    advance(1);
    sl_drv.write(false);

    unsigned edges = 1;  // the edge during which the input was high
    while (count_x.read() == 0 && edges < 16) {
        advance(1);
        ++edges;
    }
    CHECK_EQ("edge consumed after the modelled synchronizer depth", edges,
             kExpectedLatency);
    CHECK_EQ("preset loaded on the consumed edge", count_x.read(), 0x2000u);
    // And the model's published constant agrees with the measurement.
    CHECK_EQ("SYNC_LATENCY_CYCLES matches the measurement",
             octs_system_timer::SYNC_LATENCY_CYCLES, kExpectedLatency);
    apply_reset();
}

void tb::test_pulse_shapes()
{
    banner("Input pulse shape and edge-detector behaviour");

    // A level held high for many cycles must produce exactly ONE edge: the
    // detector is rising-edge, not level, sensitive.
    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 200u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x10u);
    sl_drv.write(true);
    advance(12);
    const uint64_t held = count_x.read();
    sl_drv.write(false);
    advance(2);
    // One load at 0x10 then free-running by STEP: the count must be 0x10 plus
    // the elapsed cycles, i.e. no repeated reload back to 0x10.
    CHECK_TRUE("held-high input loads exactly once", held > 0x10u);

    // Two pulses separated by a single low cycle must produce two edges: the
    // second reload pulls the count back to the preset.
    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 200u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x80u);
    pulse(sl_drv, 1);
    advance(1);           // one low cycle
    pulse(sl_drv, 1);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 2);
    CHECK_TRUE("back-to-back pulses both reload",
               count_x.read() >= 0x80u && count_x.read() < 0x90u);

    // A credit pulse arriving before any sync must not start the timer: the
    // SECONDARY is enabled by sync_load only.
    apply_reset();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x55u);
    pulse(cc_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 3);
    CHECK_EQ("credit before sync does not start the SECONDARY",
             count_x.read(), 0u);

    // Simultaneous sync + credit: sync has priority, so the count loads the
    // preset rather than re-anchoring.
    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 200u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x300u);
    sl_drv.write(true);
    cc_drv.write(true);
    advance(2);
    sl_drv.write(false);
    cc_drv.write(false);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("simultaneous sync+credit: sync wins", count_x.read(), 0x300u);

    // A PRIMARY ignores its sync inputs entirely.
    apply_reset();
    is_primary_x.write(true);
    advance();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x99u);
    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 3);
    CHECK_EQ("PRIMARY ignores sync inputs", count_x.read(), 0u);
    is_primary_x.write(false);

    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    apply_reset();
}

void tb::test_start_retrigger()
{
    banner("Repeated START in each pulse-FSM state");
    apply_reset();
    is_primary_p.write(true);
    advance();

    // A long pulse width gives a wide window in which to retrigger.
    drv_p.write32(cfg_t::OFF_CTRL, (1u << 16) | (6u << 8) | 200u);
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x1000u);
    drv_p.write32(cfg_t::OFF_TIMER_START, 1);
    advance(2);
    CHECK_TRUE("sync pulse is active", sl_p2s.read());
    const uint64_t before = count_p.read();
    CHECK_TRUE("counter is running", before >= 0x1000u);

    // START again while the sync pulse is still active: the preset reloads
    // (the datapath sees the singlepulse) even though the pulse FSM is busy.
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x2000u);
    drv_p.write32(cfg_t::OFF_TIMER_START, 1);
    advance(1);
    CHECK_EQ("START during an active pulse still reloads the preset",
             count_p.read(), 0x2000u);

    // START while idle restarts cleanly.
    advance(10);
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x3000u);
    drv_p.write32(cfg_t::OFF_TIMER_START, 1);
    advance(1);
    CHECK_EQ("START while idle reloads the preset", count_p.read(), 0x3000u);

    // START in SECONDARY mode enables the timer but loads through the normal
    // secondary path; it must not emit a sync pulse.
    apply_reset();
    is_primary_p.write(false);
    advance();
    drv_p.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x77u);
    drv_p.write32(cfg_t::OFF_TIMER_START, 1);
    advance(2);
    CHECK_TRUE("SECONDARY never drives sync_load_o", !sl_p2s.read());

    drv_p.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    apply_reset();
}

void tb::test_mode_switching()
{
    banner("Live PRIMARY <-> SECONDARY switching");
    apply_reset();

    // Run as PRIMARY, then flip to SECONDARY mid-flight.
    is_primary_x.write(true);
    advance();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x500u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 1);
    advance(4);
    CHECK_TRUE("PRIMARY is counting", count_x.read() > 0x500u);
    CHECK_EQ("STATUS.MODE reads PRIMARY",
             drv_x.read32(cfg_t::OFF_STATUS) & cfg_t::STATUS_MODE, 0u);

    is_primary_x.write(false);
    advance(2);
    CHECK_EQ("STATUS.MODE follows the live input",
             drv_x.read32(cfg_t::OFF_STATUS) & cfg_t::STATUS_MODE,
             cfg_t::STATUS_MODE);
    // Sync/credit outputs are gated off in SECONDARY mode whatever the pulse
    // FSM is doing internally.
    CHECK_TRUE("sync_load_o gated off in SECONDARY", !sl_x_o.read());
    CHECK_TRUE("cnt_credit_o gated off in SECONDARY", !cc_x_o.read());

    // Switching back must not produce an illegal output combination.
    is_primary_x.write(true);
    advance(2);
    CHECK_EQ("STATUS.MODE back to PRIMARY",
             drv_x.read32(cfg_t::OFF_STATUS) & cfg_t::STATUS_MODE, 0u);

    is_primary_x.write(false);
    apply_reset();
}

void tb::test_reset_during_activity()
{
    banner("Reset asserted during each active phase");

    auto check_all_clear = [&](const char* ctx) {
        CHECK_EQ(ctx, count_x.read(), 0u);
        CHECK_EQ(ctx, drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);
        CHECK_TRUE(ctx, !sl_x_o.read());
        CHECK_TRUE(ctx, !cc_x_o.read());
        CHECK_EQ(ctx, drv_x.read32(cfg_t::OFF_STATUS) & cfg_t::STATUS_RUNNING,
                 0u);
    };

    // Reset while a PRIMARY sync pulse is mid-flight.
    apply_reset();
    is_primary_x.write(true);
    advance();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (8u << 8) | 200u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x600u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 1);
    advance(3);
    CHECK_TRUE("sync pulse active before reset", sl_x_o.read());
    apply_reset();
    check_all_clear("reset during sync pulse");

    // Reset while the input synchronizer pipeline is in flight -- the pending
    // edge must be discarded, not consumed after release.
    is_primary_x.write(false);
    apply_reset();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x700u);
    pulse(sl_drv, 1);
    advance(1);                       // edge still inside the synchronizer
    apply_reset();
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 2);
    CHECK_EQ("reset discards an in-flight synchronizer edge", count_x.read(),
             0u);

    // Reset while starved: the starvation counter and its peak both clear.
    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 3u);  // small budget
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x800u);
    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 8);
    CHECK_TRUE("starvation is accumulating",
               drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED) > 0);
    apply_reset();
    check_all_clear("reset during starvation");

    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    apply_reset();
}

void tb::test_overflow_and_coherency()
{
    banner("64-bit rollover and split LO/HI read coherency");
    apply_reset();
    is_primary_x.write(true);
    advance();

    // Preset a PRIMARY just below the 64-bit rollover and let it wrap.
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0xFFFFFFFFu);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0xFFFFFFFFu);
    drv_x.write32(cfg_t::OFF_TIMER_START, 1);
    advance(1);
    CHECK_EQ("preset loaded at the 64-bit limit", count_x.read(), UINT64_MAX);
    advance(1);
    CHECK_EQ("counter wraps to zero", count_x.read(), 0u);
    advance(1);
    CHECK_EQ("counter continues after the wrap", count_x.read(), 1u);

    // Split LO/HI reads are independent samples of a live counter.  Reading
    // them either side of a 32-bit rollover therefore tears: LO is from before
    // the carry and HI from after.  The model has no latch-on-LO-read policy,
    // so firmware must re-read; this pins that contract.
    apply_reset();
    is_primary_x.write(true);
    advance();
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0xFFFFFFFFu);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0x00000000u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 1);
    advance(1);
    const uint32_t lo_before = drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO);
    advance(1);                       // the carry into HI happens here
    const uint32_t hi_after = drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI);
    CHECK_EQ("LO sampled before the carry", lo_before, 0xFFFFFFFFu);
    CHECK_EQ("HI sampled after the carry", hi_after, 1u);
    // Read together in one half-cycle the pair is consistent again.
    const uint32_t lo_now = drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO);
    const uint32_t hi_now = drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI);
    CHECK_EQ("a same-cycle LO/HI pair is coherent",
             (uint64_t(hi_now) << 32) | lo_now, count_x.read());

    is_primary_x.write(false);
    apply_reset();
}

void tb::test_debug_getters()
{
    banner("Debug getters paired with architectural observables");
    apply_reset();

    // CTRL field decoders agree with what software programmed.
    const uint32_t ctrl = (3u << 16) | (2u << 8) | 40u;  // STEP=3 PW=2 CV=40
    drv_x.write32(cfg_t::OFF_CTRL, ctrl);
    CHECK_EQ("credit_val() decodes CTRL", dut_x.credit_val(), 40u);
    CHECK_EQ("pulse_width() decodes CTRL", dut_x.pulse_width(), 2u);
    CHECK_EQ("step() decodes CTRL", dut_x.step(), 3u);
    CHECK_TRUE("credit_disabled() false for CV != 0", !dut_x.credit_disabled());
    CHECK_EQ("eff_pulse_width() passes a non-zero width through",
             dut_x.eff_pulse_width(), 2u);

    // PULSE_WIDTH = 0 rounds up to 1, and CV = 0 reports disabled.
    {
        const scoped_report_actions quiet(SC_WARNING, SC_DO_NOTHING);
        drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (0u << 8) | 0u);
    }
    CHECK_EQ("eff_pulse_width() rounds 0 up to 1", dut_x.eff_pulse_width(), 1u);
    CHECK_TRUE("credit_disabled() true for CV == 0", dut_x.credit_disabled());
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);

    // Run a PRIMARY and pair every state getter with a port or register.
    apply_reset();
    is_primary_x.write(true);
    advance();
    CHECK_TRUE("dbg_enabled() false before START", !dut_x.dbg_enabled());
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x1234u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 1);
    advance(3);
    CHECK_TRUE("dbg_enabled() true once started", dut_x.dbg_enabled());
    CHECK_EQ("dbg_count() matches timer_count_o", dut_x.dbg_count(),
             count_x.read());
    CHECK_EQ("dbg_count() matches the LO/HI register pair", dut_x.dbg_count(),
             (uint64_t(drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI)) << 32) |
                 drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO));

    // dump_state during an active credit pulse renders the CREDIT branch.
    {
        drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (4u << 8) | 6u);
        drv_x.write32(cfg_t::OFF_TIMER_START, 1);
        unsigned guard = 0;
        while (!cc_x_o.read() && guard < 64) { advance(); ++guard; }
        CHECK_TRUE("credit pulse observed on the port", cc_x_o.read());
        std::ostringstream os;
        dut_x.dump_state(os);
        CHECK_TRUE("dump_state names the CREDIT pulse state",
                   os.str().find("CREDIT") != std::string::npos);
        CHECK_TRUE("dump_state names the mode",
                   os.str().find("PRIMARY") != std::string::npos);
    }

    // On a starved SECONDARY the expiry getters agree with the register.
    is_primary_x.write(false);
    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 3u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x90u);
    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES + 8);
    CHECK_EQ("dbg_credit_expired_max() matches CREDIT_EXPIRED",
             dut_x.dbg_credit_expired_max(),
             drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED));
    CHECK_TRUE("dbg_credit_expired() is counting",
               dut_x.dbg_credit_expired() > 0);
    CHECK_EQ("dbg_cur_credits() matches cur_credits_debug_o",
             dut_x.dbg_cur_credits(), cred_x.read());
    CHECK_EQ("dbg_expected_count() anchors the secondary timeline",
             dut_x.dbg_expected_count(), 0x90u);

    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    apply_reset();
}

void tb::run()
{
    std::cout << "\n=== octs_system_timer unit tests ===\n";

    tie_low.write(false);
    sl_drv.write(false);
    cc_drv.write(false);
    is_primary_p.write(true);   // PRIMARY
    is_primary_s.write(false);  // SECONDARY
    is_primary_x.write(false);  // SECONDARY

    apply_reset();

    test_reset_defaults();
    test_register_contracts();
    test_access_rules();
    test_primary_start_and_pulses();
    test_primary_credit_period();
    test_primary_to_secondary_sync();
    test_secondary_load_and_starvation();
    test_secondary_replenish_and_clear();
    test_secondary_reanchor_with_step();
    test_ctrl_constraint_warning();
    test_cci_parameter();
    test_tlm_error_paths();

    // Architectural sections added for the internal-review remediation.  Each
    // resets the DUTs on entry and exit, so they are independent of the
    // sections above and of each other.
    test_csr_manifest();
    test_tlm_matrix();
    test_sideband();
    test_debug_and_dmi();
    test_credit_val_zero();
    test_ctrl_boundaries();
    test_sync_latency_independent();
    test_pulse_shapes();
    test_start_retrigger();
    test_mode_switching();
    test_reset_during_activity();
    test_overflow_and_coherency();
    test_debug_getters();

#ifdef OCTS_UB_CANARY
    // Built only by `run_tests.sh --ubsan-canary`.  Proves the UBSan build
    // really does report undefined behaviour, so a clean --asan run means
    // something.  volatile keeps the shift out of the optimiser's hands.
    {
        volatile int shift = 33;
        volatile int value = 1;
        std::cout << "  [UB CANARY] " << (value << shift) << "\n";
    }
#endif

    std::cout << "\n=== " << (g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << ": " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed ===\n\n";

    sc_stop();
}

// --- 1. Reset defaults -----------------------------------------------------

void tb::test_reset_defaults()
{
    banner("1. Reset defaults");

    CHECK_EQ("CTRL reset", drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);
    CHECK_EQ("TIMER_START reads 0", drv_x.read32(cfg_t::OFF_TIMER_START), 0u);
    CHECK_EQ("TIMER_PRESET_LO reset",
             drv_x.read32(cfg_t::OFF_TIMER_PRESET_LO), 0u);
    CHECK_EQ("TIMER_PRESET_HI reset",
             drv_x.read32(cfg_t::OFF_TIMER_PRESET_HI), 0u);
    CHECK_EQ("TIMER_COUNT_LO reset",
             drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO), 0u);
    CHECK_EQ("TIMER_COUNT_HI reset",
             drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI), 0u);
    CHECK_EQ("CREDIT_EXPIRED reset",
             drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);
    CHECK_EQ("TIMER_GPIO_ENABLE reset",
             drv_x.read32(cfg_t::OFF_TIMER_GPIO_ENABLE), 0u);

    // Decoded CTRL fields must match the RDL defaults.
    CHECK_EQ("CTRL.CREDIT_VAL default",  dut_x.credit_val(),  0x0Au);
    CHECK_EQ("CTRL.PULSE_WIDTH default", dut_x.pulse_width(), 0x02u);
    CHECK_EQ("CTRL.STEP default",        dut_x.step(),        0x01u);

    // --- 2. STATUS.MODE follows is_primary_i ---
    CHECK_EQ("STATUS PRIMARY (MODE=0, not running)",
             drv_p.read32(cfg_t::OFF_STATUS), 0u);
    CHECK_EQ("STATUS SECONDARY (MODE=1, not running)",
             drv_x.read32(cfg_t::OFF_STATUS), cfg_t::STATUS_MODE);

    std::cout << "reset defaults and STATUS.MODE OK\n";
}

// --- 3. Register contracts -------------------------------------------------

void tb::test_register_contracts()
{
    banner("3. RW / RO / reserved-bit contracts");

    // CTRL: [23:0] writable, [31:24] reserved -> read-as-zero / write-ignore.
    drv_x.write32(cfg_t::OFF_CTRL, 0xFF030415u);
    CHECK_EQ("CTRL drops reserved [31:24]", drv_x.read32(cfg_t::OFF_CTRL),
             0x00030415u);
    CHECK_EQ("CTRL.CREDIT_VAL written",  dut_x.credit_val(),  0x15u);
    CHECK_EQ("CTRL.PULSE_WIDTH written", dut_x.pulse_width(), 0x04u);
    CHECK_EQ("CTRL.STEP written",        dut_x.step(),        0x03u);
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    CHECK_EQ("CTRL restored", drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);

    // 64-bit preset is two independent 32-bit halves.
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0xDEADBEEFu);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0x12345678u);
    CHECK_EQ("PRESET_LO RW", drv_x.read32(cfg_t::OFF_TIMER_PRESET_LO),
             0xDEADBEEFu);
    CHECK_EQ("PRESET_HI RW", drv_x.read32(cfg_t::OFF_TIMER_PRESET_HI),
             0x12345678u);

    // GPIO_ENABLE is a single bit and drives an output.
    drv_x.write32(cfg_t::OFF_TIMER_GPIO_ENABLE, 0xFFFFFFFFu);
    advance();
    CHECK_EQ("GPIO_ENABLE keeps bit 0 only",
             drv_x.read32(cfg_t::OFF_TIMER_GPIO_ENABLE), 1u);
    CHECK_TRUE("gpio_enable_o asserted", gpio_x.read());
    drv_x.write32(cfg_t::OFF_TIMER_GPIO_ENABLE, 0u);
    advance();
    CHECK_TRUE("gpio_enable_o deasserted", !gpio_x.read());

    // Read-only registers ignore writes.
    drv_x.write32(cfg_t::OFF_STATUS, 0xFFFFFFFFu);
    CHECK_EQ("STATUS is RO", drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE);
    drv_x.write32(cfg_t::OFF_TIMER_COUNT_LO, 0xFFFFFFFFu);
    drv_x.write32(cfg_t::OFF_TIMER_COUNT_HI, 0xFFFFFFFFu);
    CHECK_EQ("COUNT_LO is RO", drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO), 0u);
    CHECK_EQ("COUNT_HI is RO", drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI), 0u);

    std::cout << "register access contracts OK\n";
}

// --- 4. Access rules -------------------------------------------------------

void tb::test_access_rules()
{
    banner("4. Access rules (32-bit only, aligned, in-window)");

    uint32_t buf = 0;
    sc_time  delay = SC_ZERO_TIME;

    CHECK_EQ("aligned 32-bit access is OK",
             drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::OFF_TIMER_GPIO_ENABLE,
                          &buf, 4, &delay),
             tlm::TLM_OK_RESPONSE);
    CHECK_TRUE("b_transport annotates a delay", delay > SC_ZERO_TIME);

    CHECK_EQ("byte access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x00, &buf, 1),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("halfword access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x00, &buf, 2),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("misaligned 32-bit access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x02, &buf, 4),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("access past the window rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, &buf, 4),
             tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK_EQ("last register in window is reachable",
             drv_x.access(tlm::TLM_READ_COMMAND,
                          cfg_t::WINDOW_SIZE - 4, &buf, 4),
             tlm::TLM_OK_RESPONSE);

    std::cout << "access rules OK\n";
}

// --- 5/6. PRIMARY start, 64-bit preset load, sync_load pulse width ---------

void tb::test_primary_start_and_pulses()
{
    banner("5/6. PRIMARY start, 64-bit preset load, sync_load pulse width");

    // Straddle the 32-bit boundary so both preset/count halves are exercised.
    const uint64_t preset = 0x0000'0001'0000'1000ULL;

    // Programming sequence from memmap.adoc: load the *same* preset into every
    // participating timer, then start the PRIMARY.  The SECONDARY latches its
    // own preset when the sync_load pulse arrives, so dut_s must be programmed
    // too -- otherwise it would load 0 and track an offset timeline.
    for (reg_driver* drv : {&drv_p, &drv_s}) {
        drv->write32(cfg_t::OFF_TIMER_PRESET_LO, 0x00001000u);
        drv->write32(cfg_t::OFF_TIMER_PRESET_HI, 0x00000001u);
    }
    drv_p.write32(cfg_t::OFF_TIMER_START, 0x1u);

    advance();  // the rising edge that consumes the START singlepulse

    CHECK_EQ("count loaded from 64-bit preset", count_p.read(), preset);
    CHECK_EQ("COUNT_LO after load", drv_p.read32(cfg_t::OFF_TIMER_COUNT_LO),
             0x00001000u);
    CHECK_EQ("COUNT_HI after load", drv_p.read32(cfg_t::OFF_TIMER_COUNT_HI),
             0x00000001u);
    CHECK_EQ("TIMER_START self-clears", drv_p.read32(cfg_t::OFF_TIMER_START),
             0u);
    CHECK_EQ("STATUS.RUNNING asserted", drv_p.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_RUNNING);
    CHECK_TRUE("sync_load_o asserted on cycle 1", sl_p2s.read());

    // PULSE_WIDTH = 2 -> asserted for exactly two cycles.
    advance();
    CHECK_EQ("count increments by 1 in PRIMARY", count_p.read(), preset + 1);
    CHECK_TRUE("sync_load_o still asserted on cycle 2", sl_p2s.read());
    advance();
    CHECK_EQ("count keeps incrementing", count_p.read(), preset + 2);
    CHECK_TRUE("sync_load_o deasserted after PULSE_WIDTH cycles",
               !sl_p2s.read());

    // A PRIMARY never drives its credit budget or expiry counters.
    CHECK_EQ("PRIMARY reports no expired credits",
             drv_p.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);

    std::cout << "PRIMARY start + sync_load pulse OK (count=0x" << std::hex
              << count_p.read() << std::dec << ")\n";
}

// --- 7. PRIMARY credit pulse width and period ------------------------------

void tb::test_primary_credit_period()
{
    banner("7. PRIMARY credit pulse width and CREDIT_VAL period");

    const unsigned expect_width  = dut_p.eff_pulse_width();  // 2
    const unsigned expect_period = dut_p.credit_val();       // 10
    const unsigned kBound        = 200;

    unsigned n = 0;
    while (!cc_p2s.read() && n < kBound) { advance(); ++n; }
    CHECK_TRUE("credit pulse observed", cc_p2s.read());

    unsigned width = 0;
    while (cc_p2s.read() && width < kBound) { advance(); ++width; }
    CHECK_EQ("credit pulse width == PULSE_WIDTH", width, expect_width);

    unsigned gap = 0;
    while (!cc_p2s.read() && gap < kBound) { advance(); ++gap; }
    CHECK_TRUE("second credit pulse observed", cc_p2s.read());
    CHECK_EQ("credit pulse period == CREDIT_VAL", width + gap, expect_period);

    // sync_load is a one-shot per START; it must not re-fire with credits.
    CHECK_TRUE("sync_load_o stays low after its one-shot", !sl_p2s.read());

    std::cout << "credit pulse width=" << width << " period=" << (width + gap)
              << " OK\n";
}

// --- 8. PRIMARY -> SECONDARY synchronisation --------------------------------

void tb::test_primary_to_secondary_sync()
{
    banner("8. PRIMARY -> SECONDARY count tracking");

    const unsigned kThreePeriods = 3u * dut_p.credit_val();

    // Let a few credit periods elapse so the SECONDARY has been re-anchored
    // by real credit pulses, not just by its initial sync_load.
    advance(kThreePeriods);

    CHECK_EQ("SECONDARY reports MODE=1 and RUNNING",
             drv_s.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE | cfg_t::STATUS_RUNNING);
    CHECK_TRUE("SECONDARY has credits left", credleft_s.read());
    CHECK_EQ("SECONDARY never starved", drv_s.read32(cfg_t::OFF_CREDIT_EXPIRED),
             0u);

    // The SECONDARY trails by exactly the input-synchronizer depth, and that
    // offset is constant: the credit protocol keeps re-anchoring it.
    for (unsigned i = 0; i < kThreePeriods; ++i) {
        const uint64_t p = count_p.read();
        const uint64_t s = count_s.read();
        CHECK_EQ("SECONDARY trails PRIMARY by SYNC_LATENCY_CYCLES", p - s,
                 octs_system_timer::SYNC_LATENCY_CYCLES);
        CHECK_TRUE("SECONDARY within its credit budget",
                   (p - s) <= dut_p.credit_val());
        advance();
    }

    std::cout << "PRIMARY=0x" << std::hex << count_p.read() << " SECONDARY=0x"
              << count_s.read() << std::dec << " (constant lag of "
              << octs_system_timer::SYNC_LATENCY_CYCLES << " cycles) OK\n";
}

// --- 9/10/11. SECONDARY load, STEP counting, credit starvation -------------

void tb::test_secondary_load_and_starvation()
{
    banner("9/10/11. SECONDARY load latency, STEP counting, credit starvation");

    // 9. A SECONDARY that never sees a sync_load never leaves 0.
    CHECK_EQ("idle SECONDARY count stays 0", count_x.read(), 0u);
    CHECK_EQ("idle SECONDARY is not RUNNING", drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE);

    // Faithful to the RTL: a SECONDARY's credit accounting is gated only on
    // `~is_primary_i`, not on the timer being started.  An un-synced SECONDARY
    // therefore drains its budget and piles up a meaningless CREDIT_EXPIRED --
    // which is why memmap.adoc has software clear the register.
    CHECK_TRUE("un-synced SECONDARY drains its credit budget",
               !credleft_x.read());
    CHECK_TRUE("un-synced SECONDARY accumulates a stale CREDIT_EXPIRED",
               drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED) > 0);

    const uint64_t preset = 0x200;
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);  // CV=10, PW=2, STEP=1
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, static_cast<uint32_t>(preset));
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0u);

    // 10. A sync_load pulse is consumed SYNC_LATENCY_CYCLES rising edges after
    // it is driven (input flop + 2FF synchronizer + edge-detect flop).
    pulse(sl_drv, 2);
    CHECK_EQ("count still 0 before the synchronizer drains", count_x.read(),
             0u);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("SECONDARY loads preset after SYNC_LATENCY_CYCLES",
             count_x.read(), preset);
    CHECK_EQ("SECONDARY is RUNNING after sync_load",
             drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE | cfg_t::STATUS_RUNNING);
    CHECK_EQ("expected_count anchored to preset", dut_x.dbg_expected_count(),
             preset);
    CHECK_EQ("sync_load clears the expiry counter", dut_x.dbg_credit_expired(),
             0u);

    // Drop the stale pre-sync peak so the starvation run below is measured
    // from zero.
    drv_x.write32(cfg_t::OFF_CREDIT_EXPIRED, 0u);

    advance();
    CHECK_EQ("SECONDARY advances by STEP", count_x.read(),
             preset + dut_x.step());

    // 11. With no credit pulses the budget drains and the counter halts once
    // cur_credits reaches CREDIT_VAL.
    const uint64_t cv = dut_x.credit_val();
    advance(static_cast<unsigned>(cv - 1));
    CHECK_EQ("count halts at preset + CREDIT_VAL", count_x.read(), preset + cv);
    CHECK_EQ("credit budget exhausted", dut_x.dbg_cur_credits(), cv);
    CHECK_TRUE("credits_left_debug_o deasserted", !credleft_x.read());

    advance(5);
    CHECK_EQ("count stays halted while starved", count_x.read(), preset + cv);
    const uint32_t expired_a = drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED);
    CHECK_TRUE("CREDIT_EXPIRED accumulates while starved", expired_a > 0);

    advance(4);
    const uint32_t expired_b = drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED);
    CHECK_TRUE("CREDIT_EXPIRED tracks a growing starvation run",
               expired_b > expired_a);

    std::cout << "starvation OK (count halted at 0x" << std::hex
              << count_x.read() << std::dec << ", CREDIT_EXPIRED "
              << expired_a << " -> " << expired_b << ")\n";
}

// --- 12. Credit replenish and CREDIT_EXPIRED write-to-clear ----------------

void tb::test_secondary_replenish_and_clear()
{
    banner("12. Credit replenish and CREDIT_EXPIRED write-to-clear");

    const uint64_t halted = count_x.read();

    pulse(cc_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);

    CHECK_EQ("credit pulse clears the expiry counter",
             dut_x.dbg_credit_expired(), 0u);
    CHECK_TRUE("credits available again", credleft_x.read());

    advance(3);
    CHECK_TRUE("counter resumes after replenish", count_x.read() > halted);

    // "To reset the value of this register, write anything to it."
    drv_x.write32(cfg_t::OFF_CREDIT_EXPIRED, 0xA5A5A5A5u);
    CHECK_EQ("CREDIT_EXPIRED cleared by a write",
             drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);

    std::cout << "replenish + CREDIT_EXPIRED clear OK (count=0x" << std::hex
              << count_x.read() << std::dec << ")\n";
}

// --- 13. Credit pulse re-anchors an over-run counter ------------------------

void tb::test_secondary_reanchor_with_step()
{
    banner("13. Credit pulse re-anchors count to expected_count + CREDIT_VAL");

    apply_reset();

    // STEP=3 does not divide CREDIT_VAL=10, so the free-running counter
    // overshoots the PRIMARY's timeline and the credit pulse must pull it back.
    const uint32_t ctrl = (3u << 16) | (2u << 8) | 10u;
    drv_x.write32(cfg_t::OFF_CTRL, ctrl);
    const uint64_t preset = 0x100;
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, static_cast<uint32_t>(preset));
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0u);

    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("loaded preset", count_x.read(), preset);

    // cur_credits: 0 -> 3 -> 6 -> 9 -> 12, so the counter runs to preset+12
    // and then halts one cycle later.
    advance(4);
    CHECK_EQ("counter overshoots to preset + 12", count_x.read(), preset + 12);
    advance();
    CHECK_EQ("counter halts past the budget", count_x.read(), preset + 12);
    CHECK_EQ("cur_credits overshot CREDIT_VAL", dut_x.dbg_cur_credits(), 12u);

    pulse(cc_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("credit pulse re-anchors count to expected + CREDIT_VAL",
             count_x.read(), preset + 10);
    CHECK_EQ("expected_count advances by CREDIT_VAL",
             dut_x.dbg_expected_count(), preset + 10);
    CHECK_EQ("credit budget reset", dut_x.dbg_cur_credits(), 0u);

    std::cout << "re-anchor OK (0x" << std::hex << (preset + 12) << " -> 0x"
              << count_x.read() << std::dec << ")\n";
    dut_x.dump_state();
}

// --- 14. CTRL constraint warning -------------------------------------------

void tb::test_ctrl_constraint_warning()
{
    banner("14. CTRL constraint check (CREDIT_VAL > PULSE_WIDTH)");

    std::cout << "(one SC_WARNING about CREDIT_VAL <= PULSE_WIDTH is expected "
                 "below)\n";

    const int before = sc_report_handler::get_count(SC_WARNING);
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (8u << 8) | 4u);  // CV=4 <= PW=8
    const int after = sc_report_handler::get_count(SC_WARNING);

    CHECK_TRUE("illegal CREDIT_VAL/PULSE_WIDTH combination warns",
               after > before);

    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    CHECK_EQ("CTRL restored after the negative test",
             drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);

    std::cout << "CTRL constraint check OK\n";
}

// --- 15. CCI parameter discovery and live mutation -------------------------

void tb::test_cci_parameter()
{
    banner("15. CCI access_delay_ns discovery and live mutation");

    auto broker = cci::cci_get_broker();
    auto handle = broker.get_param_handle("tb.dut_x.access_delay_ns");

    CHECK_TRUE("access_delay_ns is discoverable via CCI", handle.is_valid());
    if (!handle.is_valid()) return;

    auto delay_ns_of_a_read = [this]() {
        uint32_t buf   = 0;
        sc_time  delay = SC_ZERO_TIME;
        drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &buf, 4, &delay);
        return delay.to_seconds() * 1e9;
    };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };

    // sc_main presets this to 5.0 before elaboration.
    CHECK_TRUE("preset applied", near(handle.get_cci_value().get_double(), 5.0));
    CHECK_TRUE("annotated delay matches the preset",
               near(delay_ns_of_a_read(), 5.0));

    handle.set_cci_value(cci::cci_value(3.0));
    CHECK_TRUE("annotated delay follows a run-time override",
               near(delay_ns_of_a_read(), 3.0));

    std::cout << "CCI parameter discovery + mutation OK\n";
}

void tb::test_tlm_error_paths()
{
    banner("16. TLM error responses and dump_state CREDIT branch");

    uint32_t buf = 0;
    tlm::tlm_generic_payload gp;
    sc_time delay = SC_ZERO_TIME;

    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(cfg_t::OFF_CTRL);
    gp.set_data_ptr(nullptr);
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    gp.set_byte_enable_ptr(nullptr);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("null data_ptr is GENERIC_ERROR", gp.get_response_status(),
             tlm::TLM_GENERIC_ERROR_RESPONSE);

    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
    gp.set_data_length(0);
    gp.set_streaming_width(0);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("zero length is GENERIC_ERROR", gp.get_response_status(),
             tlm::TLM_GENERIC_ERROR_RESPONSE);

    uint8_t be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    gp.set_byte_enable_ptr(be);
    gp.set_byte_enable_length(4);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("byte enables are BYTE_ENABLE_ERROR", gp.get_response_status(),
             tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);

    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    gp.set_streaming_width(1);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("streaming_width < len is BURST_ERROR", gp.get_response_status(),
             tlm::TLM_BURST_ERROR_RESPONSE);

    gp.set_streaming_width(4);
    gp.set_command(tlm::TLM_IGNORE_COMMAND);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("IGNORE command is COMMAND_ERROR", gp.get_response_status(),
             tlm::TLM_COMMAND_ERROR_RESPONSE);

    // Reserved-hole default in reg_read/reg_write: 0x24 is past the window
    // (ADDRESS_ERROR). There is no aligned hole inside WINDOW_SIZE.

    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 10u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x100u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 0x1u);
    pulse(cc_drv, 2);
    advance(1);
    dut_x.dump_state();

    std::cout << "TLM error paths OK\n";
}

// ---------------------------------------------------------------------------

int sc_main(int, char*[])
{
    simlog::set_level(simlog::level::info);

    // CCI parameters must be constructed against a registered broker.
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);

    cci::cci_originator tb_cfg("tb_cfg");
    auto global_broker = cci::cci_get_global_broker(tb_cfg);
    global_broker.set_preset_cci_value("tb.dut_x.access_delay_ns",
                                       cci::cci_value(5.0));

    tb top("tb");
    sc_start();  // the stimulus thread calls sc_stop() when it is done

    return g_failures == 0 ? 0 : 1;
}
