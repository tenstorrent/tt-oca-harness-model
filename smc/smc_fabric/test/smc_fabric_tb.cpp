// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// smc_fabric_tb.cpp -- self-checking test bench for the SMC Fabric LT model.
//
// Exercises the routing/decode/remap/filter behaviour against the authoritative
// address map (smc_local_xbar_pkg.sv / smc_internal_axi_lite_xbar_pkg.sv) and
// the source-ID / remap constants (smc_pkg.sv):
//
//   1.  Reset / power-on defaults
//   2.  Local decode routing to each downstream initiator socket
//   3.  smc_base_config CSRs (GLOBAL_BASE) vs cpu_ctrl front-port routing
//   4.  Alias remap (programmable window → local target)
//   5.  Outbound routing (default allow) → output_axi
//   6.  Inbound filter: default-deny, allow entry, NS filtering
//   7.  sep_axi_in bypasses the inbound filter
//   8.  Outbound filter: program deny, default-allow elsewhere
//   9.  Unmapped local address → TLM_ADDRESS_ERROR
//   10. DMI is always denied
//   11. Reset clears programmed tables
//   12. Output remap (M-mode/Xvisor): bit-exact REGION_ATTRS.offset[55:0] +
//       valid[63] gate (reset = passthrough), src-ID retag independent of valid
//   13. Filter CSR image: bit-exact FILTER_CONFIG/START_ADDR/END_ADDR + locked
//   14. Alternate internal masters (jtag / data_accel / log)
//   15. Testbench API + LOCAL_BASE / REGION_SIZE global CSRs
//   16. M-mode output remap via local-base aperture
//   17. PERIPH_EXT local decode (sys / sep direct)
//   18. Deny-read poison fill with trailing partial bytes
//   19. Alias-remap CSR image (alias_remap.rdl: START/END/ATTRS with
//       cacheable[59:56] + valid[63]), AxCACHE replacement, 44-bit modular add
//   20. Output-remap 64-bit write (valid[63], reserved RAZ/WI) + IGNORE_COMMAND
//   21. Filter CSR edge cases (src_id, R/W-only, burst, OOB entries)
//   22. Outbound-filter NS matching
//   23. DMI denied on jtag / data_accel / log
//   24. no_addr_remap config bypasses output remap
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include "smc_gpio_irq.h"
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <cci/utils/consuming_broker.h>

#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>

#include "smc_fabric.h"
#include "smc_axi_extension.h"

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
                      << std::hex << "  expected=0x" << (uint64_t)_e           \
                      << " actual=0x" << (uint64_t)_a << std::dec              \
                      << "  (" #expected " == " #actual ")\n";                 \
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

// ---------------------------------------------------------------------------
// Downstream probe target — records hits and acts as a tiny memory.
// ---------------------------------------------------------------------------
struct probe : sc_core::sc_module {
    tlm_utils::simple_target_socket<probe, 64> sock;

    unsigned hits      = 0;
    uint64_t last_addr = 0;
    uint32_t last_data = 0;
    // Sideband snapshot of the smc_axi_extension on the last transaction
    // (had_ext=false when the payload carried no extension).
    bool     had_ext      = false;
    uint16_t last_src_id  = 0xFFFFu;
    uint8_t  last_cache   = 0xFFu;
    std::map<uint64_t, uint32_t> mem;
    sc_time block_for = SC_ZERO_TIME;
    std::deque<sc_time> block_sequence;

    explicit probe(sc_module_name n) : sc_core::sc_module(n), sock("sock") {
        sock.register_b_transport(this, &probe::b_tr);
    }

    void b_tr(tlm::tlm_generic_payload& gp, sc_time& /*delay*/) {
        ++hits;
        sc_time delay = block_for;
        if (!block_sequence.empty()) {
            delay = block_sequence.front();
            block_sequence.pop_front();
        }
        if (delay != SC_ZERO_TIME)
            sc_core::wait(delay);
        last_addr = gp.get_address();
        if (auto* ext = gp.get_extension<smc::smc_axi_extension>()) {
            had_ext     = true;
            last_src_id = ext->source_id;
            last_cache  = ext->axi_cache;
        } else {
            had_ext     = false;
            last_src_id = 0xFFFFu;
            last_cache  = 0xFFu;
        }
        const unsigned len = std::min(gp.get_data_length(), 4u);
        if (gp.is_write()) {
            uint32_t v = 0;
            std::memcpy(&v, gp.get_data_ptr(), len);
            last_data = v;
            mem[gp.get_address()] = v;
        } else {
            auto it = mem.find(gp.get_address());
            uint32_t v = (it != mem.end()) ? it->second : 0u;
            std::memcpy(gp.get_data_ptr(), &v, len);
            last_data = v;
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ---------------------------------------------------------------------------
// Initiator driver — issues b_transport into one fabric target socket.
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver, 64> sock;

    explicit driver(sc_module_name n) : sc_core::sc_module(n), sock("sock") {}

    // AxCACHE value attached to the extension by raw()/xfer() when with_ext.
    uint8_t axi_cache_in = 0;

    tlm::tlm_response_status raw(tlm::tlm_command cmd, uint64_t addr,
                                 unsigned len, uint8_t* buf,
                                 bool with_ext = false, uint16_t src_id = 0,
                                 bool ns = true) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(buf);
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        smc::smc_axi_extension ext;
        if (with_ext) {
            ext.source_id = src_id;
            ext.set_secure(!ns);   // ns=true → non-secure
            ext.axi_cache = axi_cache_in;
            gp.set_extension(&ext);
        }
        sock->b_transport(gp, t);
        if (with_ext) gp.clear_extension<smc::smc_axi_extension>();
        return gp.get_response_status();
    }

    tlm::tlm_response_status xfer(tlm::tlm_command cmd, uint64_t addr,
                                  uint32_t* data,
                                  bool with_ext = false,
                                  uint16_t src_id = 0, bool ns = true) {
        return raw(cmd, addr, 4, reinterpret_cast<uint8_t*>(data),
                   with_ext, src_id, ns);
    }

    void write32(uint64_t a, uint32_t v) {
        auto r = xfer(tlm::TLM_WRITE_COMMAND, a, &v);
        if (r != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << a << ") rsp="
                      << r << std::dec << "\n";
            ++g_failures;
        }
    }
    uint32_t read32(uint64_t a) {
        uint32_t v = 0xDEADBEEF;
        auto r = xfer(tlm::TLM_READ_COMMAND, a, &v);
        if (r != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << a << ") rsp="
                      << r << std::dec << "\n";
            ++g_failures;
        }
        return v;
    }
    void write64(uint64_t a, uint64_t v) {
        auto r = raw(tlm::TLM_WRITE_COMMAND, a, 8,
                     reinterpret_cast<uint8_t*>(&v));
        if (r != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write64(0x" << std::hex << a << ") rsp="
                      << r << std::dec << "\n";
            ++g_failures;
        }
    }
    bool dmi_query(uint64_t a) {
        tlm::tlm_generic_payload gp;
        tlm::tlm_dmi dmi;
        gp.set_address(a);
        gp.set_command(tlm::TLM_READ_COMMAND);
        return sock->get_direct_mem_ptr(gp, dmi);
    }
};

// ---------------------------------------------------------------------------
// Absolute address map (base = LOCAL_BASE = 0xC000_0000)
// ---------------------------------------------------------------------------
constexpr uint64_t A_PERIPH_MAIN = 0xC000'2000ULL;
constexpr uint64_t A_SPM         = 0xC004'0000ULL;
constexpr uint64_t A_DMA         = 0xC003'8000ULL;
constexpr uint64_t A_DFD         = 0xC016'0000ULL;
constexpr uint64_t A_DFT         = 0xC000'B800ULL;   // RTL DFX_CSR base
constexpr uint64_t A_AOU_PARK    = 0xC000'C000ULL;   // VP-only AOU park
constexpr uint64_t A_PERIPH_GONE = 0xC000'E000ULL;   // above RTL periph_main end
constexpr uint64_t A_CPUCTRL     = 0xC003'9000ULL;
constexpr uint64_t A_MAILBOX     = 0xC001'8000ULL;
constexpr uint64_t A_AR          = 0xC001'2000ULL;
constexpr uint64_t A_IBF         = 0xC001'5000ULL;
constexpr uint64_t A_OBF         = 0xC001'6000ULL;
constexpr uint64_t A_GBASE       = 0xC001'0000ULL;
constexpr uint64_t A_LBASE       = 0xC001'0008ULL;
constexpr uint64_t A_RSIZE       = 0xC001'0010ULL;
constexpr uint64_t A_CLOCK_GATE  = 0xC001'0018ULL;
constexpr uint64_t A_HANG_SYS_CTRL = 0xC001'0020ULL;
constexpr uint64_t A_HANG_SYS_THR  = 0xC001'0028ULL;
constexpr uint64_t A_HANG_SEP_CTRL = 0xC001'0030ULL;
constexpr uint64_t A_HANG_SEP_THR  = 0xC001'0038ULL;
constexpr uint64_t A_HANG_DATA_CTRL = 0xC001'0040ULL;
constexpr uint64_t A_HANG_DATA_THR  = 0xC001'0048ULL;
constexpr uint64_t A_PERIPH_EXT  = 0xC040'0000ULL;
constexpr uint64_t A_UNMAPPED    = 0xC000'1000ULL;  // gap between WDT and periph
constexpr uint64_t A_OUTBOUND    = 0x8000'0000ULL;  // in neither aperture
constexpr uint64_t A_MM_LOCAL    = 0xC100'0000ULL;  // M-mode window (local base)

// FILTER_CONFIG[31:0] field encodings (filter_ctrl.rdl).
constexpr uint32_t CFG_READ_EN  = 1u << 0;
constexpr uint32_t CFG_WRITE_EN = 1u << 1;
constexpr uint32_t CFG_EN       = 1u << 4;   // addr_mode (entry enable)
constexpr uint32_t CFG_ALLOW_NS = 1u << 8;   // 1 ⇒ matches non-secure traffic
constexpr uint32_t CFG_LOCKED   = 1u << 31;  // locked[63] lands in high word
// "Allow a range for non-secure read+write" — the common case for the drivers,
// which issue non-secure (ns=true) transactions by default.
constexpr uint32_t CFG_RW_NS      = CFG_EN | CFG_READ_EN | CFG_WRITE_EN | CFG_ALLOW_NS;
constexpr uint32_t CFG_RW_SECURE  = CFG_EN | CFG_READ_EN | CFG_WRITE_EN; // allow_ns=0
constexpr uint32_t CFG_READ_NS    = CFG_EN | CFG_READ_EN | CFG_ALLOW_NS;
constexpr uint32_t CFG_WRITE_NS   = CFG_EN | CFG_WRITE_EN | CFG_ALLOW_NS;
constexpr uint32_t CFG_ALLOW_BURST = 1u << 24;
// Enabled secure-matching entry with no read/write permission → hits then blocks
// (used to deny a range on the otherwise allow-by-default outbound path).
constexpr uint32_t CFG_DENY     = CFG_EN;                              // allow_ns=0, no r/w

// Output-remap CSR window bases (smc_fabric.h MR_CTRL_BASE / XR_CTRL_BASE).
constexpr uint64_t A_MR = 0xC001'3000ULL;  // M-mode remap CSRs
constexpr uint64_t A_XR = 0xC001'4000ULL;  // Xvisor remap CSRs

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::smc_fabric dut;
    smc::smc_fabric dut_noremap;

    // Inbound drivers (one per fabric target socket).
    driver d_jtag, d_mmio, d_daccel, d_log, d_sys, d_sep;
    driver d_mmio_nr;
    driver d_nr_jtag, d_nr_daccel, d_nr_log, d_nr_sys, d_nr_sep;

    // Downstream probes (one per fabric initiator socket).
    probe p_front, p_daccel, p_periph, p_dfd, p_cpu, p_mbox, p_dft, p_out;
    probe p_aR, p_mR, p_xR, p_ibf, p_obf;  // internal-CSR sockets (unused but bound)
    probe p_out_nr;
    probe p_nr_front, p_nr_daccel, p_nr_periph, p_nr_dfd, p_nr_cpu, p_nr_mbox;
    probe p_nr_dft, p_nr_aR, p_nr_mR, p_nr_xR, p_nr_ibf, p_nr_obf;

    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> rst_n_nr{"rst_n_nr"};
    sc_core::sc_signal<bool> hang_irq{"hang_irq"};
    sc_core::sc_signal<bool> hang_irq_nr{"hang_irq_nr"};
    smc::gpio_irq_reduce gpio_irqs{"gpio_irqs"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> gpio_wrap{
        "gpio_wrap", smc::kNumGpioWraps};
    sc_core::sc_signal<bool> gpio_lower{"gpio_lower"};
    sc_core::sc_signal<bool> gpio_upper{"gpio_upper"};
    sc_core::sc_event start_sys, start_sep, start_sep2, start_daccel;
    bool sys_done = false;
    bool sep_done = false;
    bool daccel_done = false;

    static smc::smc_fabric::config no_remap_cfg() {
        smc::smc_fabric::config c{};
        c.no_addr_remap = true;
        return c;
    }

    explicit tb(sc_module_name n)
        : sc_core::sc_module(n)
        , dut("fabric")
        , dut_noremap("fabric_noremap", no_remap_cfg())
        , d_jtag("d_jtag"), d_mmio("d_mmio"), d_daccel("d_daccel")
        , d_log("d_log"), d_sys("d_sys"), d_sep("d_sep")
        , d_mmio_nr("d_mmio_nr")
        , d_nr_jtag("d_nr_jtag"), d_nr_daccel("d_nr_daccel")
        , d_nr_log("d_nr_log"), d_nr_sys("d_nr_sys"), d_nr_sep("d_nr_sep")
        , p_front("p_front"), p_daccel("p_daccel"), p_periph("p_periph")
        , p_dfd("p_dfd"), p_cpu("p_cpu"), p_mbox("p_mbox"), p_dft("p_dft")
        , p_out("p_out"), p_aR("p_aR"), p_mR("p_mR"), p_xR("p_xR")
        , p_ibf("p_ibf"), p_obf("p_obf")
        , p_out_nr("p_out_nr")
        , p_nr_front("p_nr_front"), p_nr_daccel("p_nr_daccel")
        , p_nr_periph("p_nr_periph"), p_nr_dfd("p_nr_dfd")
        , p_nr_cpu("p_nr_cpu"), p_nr_mbox("p_nr_mbox"), p_nr_dft("p_nr_dft")
        , p_nr_aR("p_nr_aR"), p_nr_mR("p_nr_mR"), p_nr_xR("p_nr_xR")
        , p_nr_ibf("p_nr_ibf"), p_nr_obf("p_nr_obf")
    {
        for (unsigned i = 0; i < smc::kNumGpioWraps; ++i)
            gpio_irqs.wrap_irq[i].bind(gpio_wrap[i]);
        gpio_irqs.lower_o.bind(gpio_lower);
        gpio_irqs.upper_o.bind(gpio_upper);

        // Inbound: driver → fabric target sockets.
        d_jtag  .sock.bind(dut.jtag_axi_in);
        d_mmio  .sock.bind(dut.mmio_in);
        d_daccel.sock.bind(dut.data_accel_in);
        d_log   .sock.bind(dut.log_in);
        d_sys   .sock.bind(dut.sys_axi_in);
        d_sep   .sock.bind(dut.sep_axi_in);

        // Outbound: fabric initiator sockets → probes.
        dut.to_front_port          .bind(p_front .sock);
        dut.to_data_accel_ctrl     .bind(p_daccel.sock);
        dut.to_periph              .bind(p_periph.sock);
        dut.to_dfd_apb             .bind(p_dfd   .sock);
        dut.to_cpu_ctrl            .bind(p_cpu   .sock);
        dut.to_mailbox             .bind(p_mbox  .sock);
        dut.to_dft_csr             .bind(p_dft   .sock);
        dut.output_axi             .bind(p_out   .sock);
        dut.to_aR_ctrl             .bind(p_aR    .sock);
        dut.to_mR_ctrl             .bind(p_mR    .sock);
        dut.to_xR_ctrl             .bind(p_xR    .sock);
        dut.to_inbound_filter_ctrl .bind(p_ibf   .sock);
        dut.to_outbound_filter_ctrl.bind(p_obf   .sock);

        dut.rst_n_i(rst_n);
        dut.axi_hang_irq_o(hang_irq);

        d_mmio_nr.sock.bind(dut_noremap.mmio_in);
        d_nr_jtag  .sock.bind(dut_noremap.jtag_axi_in);
        d_nr_daccel.sock.bind(dut_noremap.data_accel_in);
        d_nr_log   .sock.bind(dut_noremap.log_in);
        d_nr_sys   .sock.bind(dut_noremap.sys_axi_in);
        d_nr_sep   .sock.bind(dut_noremap.sep_axi_in);
        dut_noremap.output_axi.bind(p_out_nr.sock);
        dut_noremap.to_front_port          .bind(p_nr_front .sock);
        dut_noremap.to_data_accel_ctrl     .bind(p_nr_daccel.sock);
        dut_noremap.to_periph              .bind(p_nr_periph.sock);
        dut_noremap.to_dfd_apb             .bind(p_nr_dfd   .sock);
        dut_noremap.to_cpu_ctrl            .bind(p_nr_cpu   .sock);
        dut_noremap.to_mailbox             .bind(p_nr_mbox  .sock);
        dut_noremap.to_dft_csr             .bind(p_nr_dft   .sock);
        dut_noremap.to_aR_ctrl             .bind(p_nr_aR    .sock);
        dut_noremap.to_mR_ctrl             .bind(p_nr_mR    .sock);
        dut_noremap.to_xR_ctrl             .bind(p_nr_xR    .sock);
        dut_noremap.to_inbound_filter_ctrl .bind(p_nr_ibf   .sock);
        dut_noremap.to_outbound_filter_ctrl.bind(p_nr_obf   .sock);
        dut_noremap.rst_n_i(rst_n_nr);
        dut_noremap.axi_hang_irq_o(hang_irq_nr);

        SC_THREAD(run);
        SC_THREAD(sys_worker);
        SC_THREAD(sep_worker);
        SC_THREAD(sep_worker2);
        SC_THREAD(daccel_worker);
    }

    void sys_worker() {
        while (true) {
            sc_core::wait(start_sys);
            uint32_t value = 0;
            (void)d_sys.xfer(tlm::TLM_READ_COMMAND, A_SPM, &value,
                             true, 0, true);
            sys_done = true;
        }
    }

    void sep_worker() {
        while (true) {
            sc_core::wait(start_sep);
            uint32_t value = 0;
            (void)d_sep.xfer(tlm::TLM_READ_COMMAND, A_SPM, &value);
            sep_done = true;
        }
    }

    void sep_worker2() {
        while (true) {
            sc_core::wait(start_sep2);
            uint32_t value = 0;
            (void)d_sep.xfer(tlm::TLM_READ_COMMAND, A_SPM, &value);
        }
    }

    void daccel_worker() {
        while (true) {
            sc_core::wait(start_daccel);
            uint32_t value = 0;
            (void)d_daccel.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &value);
            daccel_done = true;
        }
    }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(1, SC_NS);
        rst_n.write(true);
        sc_core::wait(1, SC_NS);
    }

    // Program filter entry 0 to cover [base, base+size) using the bit-exact
    // FILTER_CONFIG / START_ADDR / END_ADDR layout (end_addr is inclusive).
    void program_filter(uint64_t window, uint64_t base, uint64_t size,
                        uint32_t cfg_lo) {
        const uint64_t end = base + size - 1;
        d_mmio.write32(window + 0x00, cfg_lo);                                 // FILTER_CONFIG[31:0]
        d_mmio.write32(window + 0x08, static_cast<uint32_t>(base));            // START_ADDR[31:0]
        d_mmio.write32(window + 0x0C, static_cast<uint32_t>(base >> 32) & 0x00FF'FFFFu);
        d_mmio.write32(window + 0x10, static_cast<uint32_t>(end));             // END_ADDR[31:0]
        d_mmio.write32(window + 0x14, static_cast<uint32_t>(end >> 32) & 0x00FF'FFFFu);
    }

    void test_gpio_irq_halves() {
        // #2950: all 65 wraps contribute. The split is NumGpioWraps/2, so
        // wraps [31:0] are peripheral bit 28 and wraps [64:32] are bit 29.
        // Wrap 64 used to sit past the bonded-only OR and was invisible.
        auto settle = [] {
            sc_core::wait(SC_ZERO_TIME);
            sc_core::wait(SC_ZERO_TIME);
        };
        auto clear = [&] {
            for (unsigned i = 0; i < smc::kNumGpioWraps; ++i)
                gpio_wrap[i].write(false);
            settle();
        };
        EXPECT_EQ(65u, smc::kNumGpioWraps);  // smc_pkg::NumGpioWraps
        clear();
        EXPECT_EQ(false, gpio_lower.read());
        EXPECT_EQ(false, gpio_upper.read());

        // One-hot sweep over every wrap. Oracle: smc_peripherals.sv
        // [28] = |gpio_interrupt[31:0], [29] = |gpio_interrupt[64:32].
        for (unsigned i = 0; i < 65u; ++i) {
            gpio_wrap[i].write(true);
            settle();
            EXPECT_EQ(i <= 31u, gpio_lower.read());
            EXPECT_EQ(i >= 32u, gpio_upper.read());
            gpio_wrap[i].write(false);
            settle();
            EXPECT_EQ(false, gpio_lower.read());
            EXPECT_EQ(false, gpio_upper.read());
        }

        // Level OR: the half stays high until its last source drops.
        gpio_wrap[0].write(true);
        gpio_wrap[64].write(true);
        settle();
        EXPECT_EQ(true, gpio_lower.read());
        EXPECT_EQ(true, gpio_upper.read());
        gpio_wrap[31].write(true);
        gpio_wrap[0].write(false);
        settle();
        EXPECT_EQ(true, gpio_lower.read());
        gpio_wrap[31].write(false);
        settle();
        EXPECT_EQ(false, gpio_lower.read());
        EXPECT_EQ(true, gpio_upper.read());
        clear();
        EXPECT_EQ(false, gpio_upper.read());
        std::cout << "  [PASS] GPIO irq halves over all 65 wraps\n";
    }

    void run() {
        std::cout << "==== SMC Fabric TB ====\n";
        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        test_gpio_irq_halves();

        // ----------------------------------------------------------------
        // 1. Reset / power-on defaults.
        // ----------------------------------------------------------------
        pulse_reset();
        EXPECT_EQ(0x4000'0000ULL, dut.read_global_base());
        EXPECT_EQ(0x0100'0000ULL, dut.read_region_size());
        EXPECT_EQ(false, dut.get_inbound_filter_entry(0).addr_mode);
        EXPECT_EQ(0x7ULL, dut.get_inbound_filter_entry(0).end_addr);  // RDL reset
        EXPECT_EQ(false, dut.get_alias_region(0).valid);
        std::cout << "  [PASS] reset / power-on defaults\n";

        // ----------------------------------------------------------------
        // 2. Local decode routing (internal MMIO master).
        //    Address is masked but stays identical inside the aperture.
        // ----------------------------------------------------------------
        d_mmio.write32(A_PERIPH_MAIN, 0x1111);
        EXPECT_EQ(1u, p_periph.hits);
        EXPECT_EQ(A_PERIPH_MAIN, p_periph.last_addr);

        d_mmio.write32(A_SPM, 0x2222);
        EXPECT_EQ(1u, p_front.hits);
        EXPECT_EQ(A_SPM, p_front.last_addr);

        d_mmio.write32(A_DMA, 0x3333);
        EXPECT_EQ(1u, p_daccel.hits);
        EXPECT_EQ(A_DMA, p_daccel.last_addr);

        d_mmio.write32(A_CPUCTRL, 0x3334);
        EXPECT_EQ(2u, p_front.hits);
        EXPECT_EQ(A_CPUCTRL, p_front.last_addr);

        // DFD (0xC016_0000) is above LOCAL_ALIAS_REGION_SIZE — use global alias.
        d_mmio.write32(0x4016'0000ULL, 0x4444);
        EXPECT_EQ(1u, p_dfd.hits);
        EXPECT_EQ(A_DFD, p_dfd.last_addr);

        d_mmio.write32(A_DFT, 0x5555);
        EXPECT_EQ(1u, p_dft.hits);

        d_mmio.write32(A_MAILBOX, 0x6666);
        EXPECT_EQ(1u, p_mbox.hits);
        std::cout << "  [PASS] local decode routing\n";

        // ----------------------------------------------------------------
        // 2b. Realignment Phase 2 window ends.
        //     periph_main now stops at the RTL 0xC000_B800, so the old VP
        //     tail (0xC000_C000..0xC000_E7FF) is no longer swallowed; only
        //     the VP-only AOU park inside it still reaches the periph port.
        // ----------------------------------------------------------------
        {
            const unsigned periph_before = p_periph.hits;
            d_mmio.write32(A_AOU_PARK, 0x7777);
            EXPECT_EQ(periph_before + 1u, p_periph.hits);
            EXPECT_EQ(A_AOU_PARK, p_periph.last_addr);

            uint32_t scratch = 0;
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_GONE,
                                  &scratch));
            EXPECT_EQ(periph_before + 1u, p_periph.hits);
            std::cout << "  [PASS] phase-2 periph_main / AOU park ends\n";
        }

        // ----------------------------------------------------------------
        // 3. smc_base_config CSR handled internally; cpu_ctrl goes to front_port.
        // ----------------------------------------------------------------
        d_mmio.write32(A_GBASE, 0x1234'0000u);
        EXPECT_EQ(0x1234'0000ULL, dut.read_global_base());
        EXPECT_EQ(0x1234'0000u, d_mmio.read32(A_GBASE));
        EXPECT_EQ(0u, p_cpu.hits);  // GLOBAL_BASE not forwarded

        const unsigned front_before = p_front.hits;
        d_mmio.write32(A_CPUCTRL, 0xABCD);
        EXPECT_EQ(front_before + 1u, p_front.hits);
        EXPECT_EQ(A_CPUCTRL, p_front.last_addr);
        EXPECT_EQ(0xABCDu, d_mmio.read32(A_CPUCTRL));  // round-trips via front-port probe mem
        std::cout << "  [PASS] smc_base_config CSR vs cpu_ctrl front-port routing\n";

        pulse_reset();  // restore global_base

        // ----------------------------------------------------------------
        // 4. Alias remap: window 0x9000_0000 → periph main.
        //    alias_remap.rdl layout: REGION_START @0x00, REGION_END @0x08,
        //    REGION_ATTRS @0x10 = offset[55:12] | cacheable[59:56] | valid[63].
        //    Programmed with 32-bit writes: low word at +0x00, high at +0x04.
        // ----------------------------------------------------------------
        const uint64_t src   = 0x9000'0000ULL;
        const uint32_t off   = static_cast<uint32_t>(A_PERIPH_MAIN - src);   // 0x3000_2000
        d_mmio.write32(A_AR + 0x00, static_cast<uint32_t>(src));          // start[31:0]
        d_mmio.write32(A_AR + 0x08, static_cast<uint32_t>(src) + 0x1000); // end[31:0]
        d_mmio.write32(A_AR + 0x10, off);                                 // offset[31:0]
        d_mmio.write32(A_AR + 0x14, 0x8000'0000u);                        // valid[63]
        {
            auto r = dut.get_alias_region(0);
            EXPECT_EQ(src,            r.start);
            EXPECT_EQ(src + 0x1000,   r.end);
            EXPECT_EQ(static_cast<int64_t>(off), r.offset);
            EXPECT_EQ(0u, static_cast<unsigned>(r.cacheable));
            EXPECT_TRUE(r.valid);
            EXPECT_EQ(0x8000'0000u, d_mmio.read32(A_AR + 0x14));
        }
        const unsigned periph_before = p_periph.hits;
        d_mmio.write32(src + 0x10, 0x7777);   // remapped → 0xC0002010
        EXPECT_EQ(periph_before + 1u, p_periph.hits);
        EXPECT_EQ(A_PERIPH_MAIN + 0x10, p_periph.last_addr);
        EXPECT_TRUE(dut.get_remap_debug().hit);
        EXPECT_EQ(0u, dut.get_remap_debug().region_index);

        // Valid clear (offset still programmed) → no hit, address unchanged
        // → 0x9000_0010 is outside the local aperture → outbound.
        d_mmio.write32(A_AR + 0x14, 0x0000'0000u);
        {
            const unsigned o = p_out.hits;
            d_mmio.write32(src + 0x10, 0x7778);
            EXPECT_EQ(o + 1u, p_out.hits);
            EXPECT_EQ(src + 0x10, p_out.last_addr);
            EXPECT_TRUE(!dut.get_remap_debug().hit);
        }
        std::cout << "  [PASS] alias remap\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 5. Outbound routing (default allow) → output_axi.
        // ----------------------------------------------------------------
        const unsigned out_before = p_out.hits;
        d_mmio.write32(A_OUTBOUND, 0x8888);
        EXPECT_EQ(out_before + 1u, p_out.hits);
        EXPECT_EQ(A_OUTBOUND, p_out.last_addr);  // plain path: addr unchanged

        // M-mode window: at reset valid==0 → address passes through unchanged.
        const unsigned out_mmode = p_out.hits;
        d_mmio.write32(0x4100'0055ULL, 0x9999);
        EXPECT_EQ(out_mmode + 1u, p_out.hits);
        EXPECT_EQ(0x4100'0055ULL, p_out.last_addr);
        std::cout << "  [PASS] outbound default-allow routing\n";

        // ----------------------------------------------------------------
        // 6. Inbound filter: default deny, then allow, then NS filtering.
        // ----------------------------------------------------------------
        uint32_t scratch = 0;
        // 6a. Default deny on sys path.
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch));

        // 6b. Allow [0xC0000000, 0xC1000000) for non-secure R/W → sys succeeds.
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL, CFG_RW_NS);
        EXPECT_TRUE(dut.get_inbound_filter_entry(0).addr_mode);
        EXPECT_TRUE(dut.get_inbound_filter_entry(0).read_en);
        EXPECT_TRUE(dut.get_inbound_filter_entry(0).allow_ns);
        EXPECT_EQ(0xC000'0000ULL, dut.get_inbound_filter_entry(0).start_addr);
        EXPECT_EQ(0xC0FF'FFFFULL, dut.get_inbound_filter_entry(0).end_addr);
        const unsigned periph_h = p_periph.hits;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch));
        EXPECT_EQ(periph_h + 1u, p_periph.hits);

        // 6c. Address outside the filter range is still denied.
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, 0xC1FF'0000ULL, &scratch));
        std::cout << "  [PASS] inbound filter allow/deny\n";

        // 6d. NS filtering: secure-only entry (allow_ns=0 ⇒ matches ns=false).
        pulse_reset();
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL, CFG_RW_SECURE);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,           // secure access passes
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, /*ns=*/false));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, // non-secure blocked
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, /*ns=*/true));
        std::cout << "  [PASS] inbound NS filtering\n";

        // ----------------------------------------------------------------
        // 7. sep_axi_in bypasses the inbound filter (still default-deny).
        // ----------------------------------------------------------------
        pulse_reset();
        const unsigned periph_sep = p_periph.hits;
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,   // sys denied (no filter)
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch));
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,              // sep bypasses filter
                  d_sep.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch));
        EXPECT_EQ(periph_sep + 1u, p_periph.hits);
        std::cout << "  [PASS] sep bypasses inbound filter\n";

        // ----------------------------------------------------------------
        // 8. Outbound filter: deny a range; elsewhere stays default-allow.
        // ----------------------------------------------------------------
        program_filter(A_OBF, A_OUTBOUND, 0x0100'0000ULL, CFG_DENY);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.xfer(tlm::TLM_WRITE_COMMAND, A_OUTBOUND, &scratch));
        const unsigned out_h = p_out.hits;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,              // outside deny range
                  d_mmio.xfer(tlm::TLM_WRITE_COMMAND, 0x8800'0000ULL, &scratch));
        EXPECT_EQ(out_h + 1u, p_out.hits);
        std::cout << "  [PASS] outbound filter deny\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 9. Unmapped local address → TLM_ADDRESS_ERROR.
        // ----------------------------------------------------------------
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.xfer(tlm::TLM_READ_COMMAND, A_UNMAPPED, &scratch));
        std::cout << "  [PASS] unmapped local address denied\n";

        // ----------------------------------------------------------------
        // 10. DMI is always denied.
        // ----------------------------------------------------------------
        EXPECT_EQ(false, d_mmio.dmi_query(A_SPM));
        EXPECT_EQ(false, d_sys.dmi_query(A_PERIPH_MAIN));
        std::cout << "  [PASS] DMI denied\n";

        // ----------------------------------------------------------------
        // 11. Reset clears programmed tables.
        // ----------------------------------------------------------------
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL, CFG_RW_NS);
        EXPECT_TRUE(dut.get_inbound_filter_entry(0).addr_mode);
        pulse_reset();
        EXPECT_EQ(false, dut.get_inbound_filter_entry(0).addr_mode);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,   // back to default-deny
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch));
        std::cout << "  [PASS] reset clears programmed tables\n";

        // ----------------------------------------------------------------
        // 12. Output remap — bit-exact to output_remap.sv (tt-oca-hw #2572):
        //       idx = (addr-window_base)[22:20];
        //       out = valid ? {offset[55:20], (addr-window_base)[19:0]} : addr.
        //     Window base (outbound) = global_base + REMAP_START.
        //     REGION_ATTRS = offset[55:0] | valid[63]; the high 32-bit word at
        //     +0x04 therefore carries valid in bit 31.
        // ----------------------------------------------------------------
        pulse_reset();
        const uint64_t mm_base = dut.read_global_base() + 0x0100'0000ULL; // 0x4100_0000
        const uint64_t xv_base = dut.read_global_base() + 0x0180'0000ULL; // 0x4180_0000
        constexpr uint32_t OR_VALID_HI = 0x8000'0000u;                  // valid[63] in +0x04

        // 12a. At reset valid==0: the address passes through UNCHANGED (an
        //      identity mapping), not a remap to {0, low20}.
        {
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, mm_base + 0x55, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(mm_base + 0x55, p_out.last_addr);
        }

        // 12b. Program M-mode region 3 (stride 0x08, 64-bit REGION_ATTRS at
        //      +0x00). offset = 0xABC0_0000 → offset[55:20]=0xABC.
        //      Without valid the offset is inert; with valid it remaps.
        d_mmio.write32(A_MR + 3 * 0x08 + 0x00, 0xABC0'0000u);  // offset[31:0]
        EXPECT_EQ(0xABC0'0000u, d_mmio.read32(A_MR + 3 * 0x08 + 0x00));
        EXPECT_EQ(0u,           d_mmio.read32(A_MR + 3 * 0x08 + 0x04));  // valid still 0
        {
            const uint64_t a = mm_base + (3ULL << 20) + 0x120;   // 0x4130_0120
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(a, p_out.last_addr);                       // offset programmed, valid=0
        }
        d_mmio.write32(A_MR + 3 * 0x08 + 0x04, OR_VALID_HI);    // set valid
        EXPECT_EQ(OR_VALID_HI, d_mmio.read32(A_MR + 3 * 0x08 + 0x04));
        {
            // addr in region 3: (addr-base)[22:20]==3, low20 preserved.
            const uint64_t a = mm_base + (3ULL << 20) + 0x120;   // 0x4130_0120
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0xABC0'0120ULL, p_out.last_addr);  // {0xABC, 0x00120}
        }
        {
            // Region 2 (adjacent, still valid=0) is unaffected by region 3.
            const uint64_t a = mm_base + (2ULL << 20) + 0x120;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(a, p_out.last_addr);
        }

        // 12c. 56-bit field: high word at +0x04 = {valid, 7'b0, offset[55:32]}.
        d_mmio.write32(A_MR + 5 * 0x08 + 0x00, 0x0010'0000u);               // offset[31:0]
        d_mmio.write32(A_MR + 5 * 0x08 + 0x04, OR_VALID_HI | 0x7F00'00DEu); // offset[55:32]+valid, rsvd set
        EXPECT_EQ(0x0010'0000u,             d_mmio.read32(A_MR + 5 * 0x08 + 0x00));
        EXPECT_EQ(OR_VALID_HI | 0x0000'00DEu, d_mmio.read32(A_MR + 5 * 0x08 + 0x04)); // [62:56] RAZ
        {
            // offset = 0x0000_00DE_0010_0000 → offset[55:20] = 0xDE00_1.
            const uint64_t a = mm_base + (5ULL << 20) + 0x004; // region 5
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x0000'00DE'0010'0004ULL, p_out.last_addr);
        }
        // Clearing valid (offset untouched) restores passthrough for region 5.
        d_mmio.write32(A_MR + 5 * 0x08 + 0x04, 0x0000'00DEu);
        {
            const uint64_t a = mm_base + (5ULL << 20) + 0x004;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(a, p_out.last_addr);
        }
        std::cout << "  [PASS] M-mode output remap (bit-exact offset[55:0] + valid[63])\n";

        // 12d. Xvisor path uses the same encoding on its own window.
        d_mmio.write32(A_XR + 1 * 0x08 + 0x00, 0x7770'0000u);  // region 1 offset
        {
            const uint64_t a = xv_base + (1ULL << 20) + 0x0AB;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(a, p_out.last_addr);                       // valid=0 → passthrough
        }
        d_mmio.write32(A_XR + 1 * 0x08 + 0x04, OR_VALID_HI);
        {
            const uint64_t a = xv_base + (1ULL << 20) + 0x0AB;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x7770'00ABULL, p_out.last_addr);
        }
        std::cout << "  [PASS] Xvisor output remap\n";

        // 12f. Source-ID re-tag (UserOverride) is independent of valid: an
        //      invalid entry still re-tags the outbound source_id while the
        //      address passes through.
        {
            EXPECT_TRUE(!dut.get_alias_region(0).valid);   // sanity: untouched slot
            const unsigned h = p_out.hits;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      d_mmio.xfer(tlm::TLM_WRITE_COMMAND, mm_base + 0x77, &scratch,
                                  /*with_ext=*/true, smc::SMC_ID));
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(mm_base + 0x77, p_out.last_addr);
            EXPECT_TRUE(p_out.had_ext);
            EXPECT_EQ(static_cast<uint16_t>(0xCu),   // smc_pkg.sv MMODE source ID
                      p_out.last_src_id);
        }
        std::cout << "  [PASS] output remap re-tags source_id even when valid=0\n";

        // 12e. Reset clears the registers (offset and valid) → identity again.
        pulse_reset();
        EXPECT_EQ(0u, d_mmio.read32(A_MR + 3 * 0x08 + 0x00));
        EXPECT_EQ(0u, d_mmio.read32(A_MR + 3 * 0x08 + 0x04));
        {
            const uint64_t a = mm_base + (3ULL << 20) + 0x120;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(a, p_out.last_addr);
        }
        std::cout << "  [PASS] reset clears output-remap registers\n";

        // ----------------------------------------------------------------
        // 13. Filter CSR image: bit-exact FILTER_CONFIG/START_ADDR/END_ADDR.
        // ----------------------------------------------------------------
        pulse_reset();
        // 13a. FILTER_CONFIG read-back of programmed fields (entry 1).
        const uint64_t e1 = A_IBF + 1 * 0x20;
        d_mmio.write32(e1 + 0x00, CFG_RW_NS | (5u << 16));  // +src_id=5
        d_mmio.write32(e1 + 0x08, 0xAB00'0000u);            // START_ADDR
        d_mmio.write32(e1 + 0x10, 0xAB10'0FFFu);            // END_ADDR (inclusive)
        {
            uint32_t cfg = d_mmio.read32(e1 + 0x00);
            EXPECT_TRUE((cfg & CFG_EN)       != 0u);  // addr_mode
            EXPECT_TRUE((cfg & CFG_READ_EN)  != 0u);
            EXPECT_TRUE((cfg & CFG_WRITE_EN) != 0u);
            EXPECT_TRUE((cfg & CFG_ALLOW_NS) != 0u);
            EXPECT_EQ(0x3u, (cfg >> 12) & 0x7u);      // data_bus_width (RO) = 3
            EXPECT_EQ(0x5u, (cfg >> 16) & 0xFu);      // src_id
            auto fe = dut.get_inbound_filter_entry(1);
            EXPECT_EQ(0xAB00'0000ULL, fe.start_addr);
            EXPECT_EQ(0xAB10'0FFFULL, fe.end_addr);
            EXPECT_EQ(5u, fe.src_id);
        }

        // 13b. START_ADDR 56-bit read-back, reconstructed purely from the CSR
        //      low/high words (no peek at the model struct).
        d_mmio.write32(e1 + 0x08, 0x1234'5678u);
        d_mmio.write32(e1 + 0x0C, 0x00AB'CDEFu);     // high 24 bits
        const uint32_t start_lo = d_mmio.read32(e1 + 0x08);
        const uint32_t start_hi = d_mmio.read32(e1 + 0x0C);
        EXPECT_EQ(0x1234'5678u, start_lo);
        EXPECT_EQ(0x00AB'CDEFu, start_hi);
        EXPECT_EQ(0x00AB'CDEF'1234'5678ULL,
                  (static_cast<uint64_t>(start_hi) << 32) | start_lo);

        // 13c. locked[63] is write-one-to-set. Once set, every write to the
        //      entry's FILTER_CONFIG / START_ADDR / END_ADDR is steered to the
        //      AXI error subordinate and returns DECERR (RTL #2480,
        //      axi_filter_wrap.sv); the stored values are untouched and reads
        //      still return the locked configuration.
        const uint32_t cfg_lo_before = d_mmio.read32(e1 + 0x00);
        const uint32_t end_lo_before = d_mmio.read32(e1 + 0x10);
        d_mmio.write32(e1 + 0x04, CFG_LOCKED);       // set locked (high word, bit31) — still OK
        EXPECT_TRUE((d_mmio.read32(e1 + 0x04) & CFG_LOCKED) != 0u);  // locked reads back
        {
            uint32_t junk = 0xDEAD'BEEFu;
            // Every 32-bit lane of the three locked registers → DECERR.
            for (uint32_t off : {0x00u, 0x04u, 0x08u, 0x0Cu, 0x10u, 0x14u}) {
                EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                          d_mmio.xfer(tlm::TLM_WRITE_COMMAND, e1 + off, &junk));
            }
            // A full 64-bit beat to each locked register → DECERR as well.
            uint64_t junk64 = 0xDEAD'BEEF'CAFE'F00DULL;
            for (uint32_t off : {0x00u, 0x08u, 0x10u}) {
                EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                          d_mmio.raw(tlm::TLM_WRITE_COMMAND, e1 + off, 8,
                                     reinterpret_cast<uint8_t*>(&junk64)));
            }
            // Re-asserting the lock is also a FILTER_CONFIG write → DECERR.
            uint32_t relock = CFG_LOCKED;
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.xfer(tlm::TLM_WRITE_COMMAND, e1 + 0x04, &relock));
            // The reserved tail of the stride is not one of the three
            // registers: still write-ignored with an OK response.
            d_mmio.write32(e1 + 0x18, 0xDEAD'BEEFu);
            d_mmio.write32(e1 + 0x1C, 0xDEAD'BEEFu);
        }
        // Nothing moved: CONFIG (low word + locked), START, END all intact.
        EXPECT_EQ(cfg_lo_before, d_mmio.read32(e1 + 0x00));
        EXPECT_TRUE((d_mmio.read32(e1 + 0x04) & CFG_LOCKED) != 0u);
        EXPECT_EQ(0x1234'5678u, d_mmio.read32(e1 + 0x08));  // START_ADDR unchanged
        EXPECT_EQ(0x00AB'CDEFu, d_mmio.read32(e1 + 0x0C));
        EXPECT_EQ(end_lo_before, d_mmio.read32(e1 + 0x10));
        EXPECT_EQ(5u, dut.get_inbound_filter_entry(1).src_id);
        EXPECT_TRUE(dut.get_inbound_filter_entry(1).locked);
        // The lock is per-entry: a neighbouring unlocked entry still accepts
        // writes with an OK response.
        d_mmio.write32(A_IBF + 2 * 0x20 + 0x08, 0x0BAD'0000u);
        EXPECT_EQ(0x0BAD'0000u, d_mmio.read32(A_IBF + 2 * 0x20 + 0x08));
        std::cout << "  [PASS] filter CSR image (CONFIG/START/END, locked → DECERR)\n";

        // ----------------------------------------------------------------
        // 14. Alternate internal masters (jtag / data_accel / log).
        // ----------------------------------------------------------------
        {
            const unsigned h = p_front.hits;
            d_jtag.write32(A_SPM, 0xA001);
            EXPECT_EQ(h + 1u, p_front.hits);
            EXPECT_EQ(A_SPM, p_front.last_addr);
        }
        {
            const unsigned h = p_daccel.hits;
            d_daccel.write32(A_DMA, 0xA002);
            EXPECT_EQ(h + 1u, p_daccel.hits);
        }
        {
            const unsigned h = p_periph.hits;
            d_log.write32(A_PERIPH_MAIN, 0xA003);
            EXPECT_EQ(h + 1u, p_periph.hits);
        }
        std::cout << "  [PASS] alternate internal masters\n";

        // ----------------------------------------------------------------
        // 15. Testbench API + LOCAL_BASE / REGION_SIZE CSRs.
        // ----------------------------------------------------------------
        dut.write_global_base(0x5000'0000ULL);
        EXPECT_EQ(0x5000'0000ULL, dut.read_global_base());
        EXPECT_EQ(0x5000'0000u, d_mmio.read32(A_GBASE));

        dut.write_region_size(0x0300'0000ULL);
        EXPECT_EQ(0x0300'0000ULL, dut.read_region_size());
        EXPECT_EQ(0x0300'0000u, d_mmio.read32(A_RSIZE));

        EXPECT_EQ(0xC000'0000u, d_mmio.read32(A_LBASE));  // RO LOCAL_BASE
        d_mmio.write32(A_LBASE, 0xDEAD'BEEFu);            // ignored write
        EXPECT_EQ(0xC000'0000u, d_mmio.read32(A_LBASE));

        uint32_t ignore = 0;
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_IGNORE_COMMAND, A_GBASE, 4,
                             reinterpret_cast<uint8_t*>(&ignore)));
        std::cout << "  [PASS] global CSR / testbench API\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 16. M-mode output remap via local-base aperture (0xC100_xxxx).
        // ----------------------------------------------------------------
        d_mmio.write32(A_MR + 2 * 0x08 + 0x00, 0x5550'0000u);
        d_mmio.write32(A_MR + 2 * 0x08 + 0x04, 0x8000'0000u);   // valid
        {
            const uint64_t a = A_MM_LOCAL + (2ULL << 20) + 0x33;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x5550'0033ULL, p_out.last_addr);
        }
        std::cout << "  [PASS] M-mode remap via local-base aperture\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 17. PERIPH_EXT local decode (sys / sep direct, above 16 MB demux).
        // ----------------------------------------------------------------
        program_filter(A_IBF, 0xC000'0000ULL, 0x1000'0000ULL, CFG_RW_NS);
        {
            const unsigned h = p_periph.hits;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_EXT, &scratch,
                                 true, 0, true));
            EXPECT_EQ(h + 1u, p_periph.hits);
            EXPECT_EQ(A_PERIPH_EXT, p_periph.last_addr);
        }
        {
            const unsigned h = p_periph.hits;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      d_sep.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_EXT + 0x100, &scratch));
            EXPECT_EQ(h + 1u, p_periph.hits);
            EXPECT_EQ(A_PERIPH_EXT + 0x100, p_periph.last_addr);
        }
        std::cout << "  [PASS] PERIPH_EXT decode (sys / sep)\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 18. Deny-read poison fill with trailing partial bytes (len=6).
        // ----------------------------------------------------------------
        {
            std::array<uint8_t, 6> poison_buf{};
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.raw(tlm::TLM_READ_COMMAND, A_UNMAPPED, 6,
                                 poison_buf.data()));
            EXPECT_EQ(0x1Eu, poison_buf[4]);
            EXPECT_EQ(0xABu, poison_buf[5]);
        }
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_READ_COMMAND, A_UNMAPPED, 4, nullptr));
        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_READ_COMMAND, A_GBASE + 0x18, 4, nullptr));
        std::cout << "  [PASS] deny-read poison trailing bytes\n";

        // ----------------------------------------------------------------
        // 19. Alias-remap CSR image (alias_remap.rdl, tt-oca-hw #2464):
        //     REGION_START @0x00 start_addr[55:12]
        //     REGION_END   @0x08 end_addr[55:12]   (exclusive)
        //     REGION_ATTRS @0x10 offset[55:12] | cacheable[59:56] | valid[63]
        //     +0x18/+0x1C  unmapped → RAZ/WI
        //     AxCACHE on a hit is REPLACED bit-for-bit by cacheable[3:0];
        //     on a miss the incoming AxCACHE is preserved.
        // ----------------------------------------------------------------
        {
            const uint64_t ar0 = A_AR;
            // 19a. 32-bit CSR image: low-12 bits of START/END/offset are WI.
            d_mmio.write32(ar0 + 0x00, 0xA000'0FFFu);
            d_mmio.write32(ar0 + 0x08, 0xA000'1ABCu);
            d_mmio.write32(ar0 + 0x10, 0x0001'0123u);                 // offset[31:0]
            d_mmio.write32(ar0 + 0x14, 0x8A00'0000u);                 // valid | cacheable=0xA
            EXPECT_EQ(0xA000'0000u, d_mmio.read32(ar0 + 0x00));
            EXPECT_EQ(0xA000'1000u, d_mmio.read32(ar0 + 0x08));
            EXPECT_EQ(0x0001'0000u, d_mmio.read32(ar0 + 0x10));
            EXPECT_EQ(0x8A00'0000u, d_mmio.read32(ar0 + 0x14));
            {
                auto r = dut.get_alias_region(0);
                EXPECT_EQ(0xA000'0000ULL, r.start);
                EXPECT_EQ(0xA000'1000ULL, r.end);
                EXPECT_EQ(static_cast<int64_t>(0x0001'0000), r.offset);
                EXPECT_EQ(0xAu, static_cast<unsigned>(r.cacheable));
                EXPECT_TRUE(r.valid);
            }
            // Reserved words inside the 0x20 stride: RAZ/WI, OK response.
            d_mmio.write32(ar0 + 0x18, 0x99u);
            d_mmio.write32(ar0 + 0x1C, 0x99u);
            EXPECT_EQ(0u, d_mmio.read32(ar0 + 0x18));
            EXPECT_EQ(0u, d_mmio.read32(ar0 + 0x1C));
            {
                auto r = dut.get_alias_region(0);      // untouched by reserved writes
                EXPECT_EQ(0xAu, static_cast<unsigned>(r.cacheable));
                EXPECT_TRUE(r.valid);
            }

            // 19b. 64-bit REGION_ATTRS write: bits [62:60] (incl. the legacy
            //      bit-62 "cacheable") and offset[11:0] are dropped; RW mask
            //      is 0x8FFF_FFFF_FFFF_F000.
            d_mmio.write64(ar0 + 0x10, 0xFFFF'FFFF'FFFF'FFFFULL);
            {
                uint64_t rd = 0;
                EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                          d_mmio.raw(tlm::TLM_READ_COMMAND, ar0 + 0x10, 8,
                                     reinterpret_cast<uint8_t*>(&rd)));
                EXPECT_EQ(0x8FFF'FFFF'FFFF'F000ULL, rd);
                auto r = dut.get_alias_region(0);
                EXPECT_EQ(0xFu, static_cast<unsigned>(r.cacheable));
                EXPECT_EQ(static_cast<int64_t>(0x00FF'FFFF'FFFF'F000ULL), r.offset);
            }
            d_mmio.write64(ar0 + 0x10, 0x4000'0000'0000'0000ULL);    // bit 62 only
            {
                auto r = dut.get_alias_region(0);
                EXPECT_EQ(0u, static_cast<unsigned>(r.cacheable));
                EXPECT_TRUE(!r.valid);
                EXPECT_EQ(0u, d_mmio.read32(ar0 + 0x14));
            }
            // 64-bit START/END writes are masked to [55:12] as well.
            d_mmio.write64(ar0 + 0x00, 0xFFFF'FFFF'FFFF'FFFFULL);
            EXPECT_EQ(0x00FF'FFFF'FFFF'F000ULL, dut.get_alias_region(0).start);
            d_mmio.write64(ar0 + 0x08, 0x0123'4567'89AB'CDEFULL);
            EXPECT_EQ(0x0023'4567'89AB'C000ULL, dut.get_alias_region(0).end);
            EXPECT_EQ(0x0023'4567u, d_mmio.read32(ar0 + 0x0C));

            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      d_mmio.raw(tlm::TLM_IGNORE_COMMAND, ar0, 4,
                                 reinterpret_cast<uint8_t*>(&ignore)));
        }
        std::cout << "  [PASS] alias-remap CSR image (RDL layout)\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 19c. Alias-remap AxCACHE replacement + low-12-bit preservation.
        //      Region 1: 0x9100_0000..0x9100_2000 → A_PERIPH_MAIN, cacheable=0x6.
        //      Region 2: 0x9300_0000..0x9300_1000 → A_SPM,         cacheable=0x0.
        // ----------------------------------------------------------------
        {
            const uint64_t win1 = 0x9100'0000ULL;
            const uint64_t win2 = 0x9300'0000ULL;
            const uint64_t ar1  = A_AR + 1 * 0x20;
            const uint64_t ar2  = A_AR + 2 * 0x20;
            d_mmio.write64(ar1 + 0x00, win1);
            d_mmio.write64(ar1 + 0x08, win1 + 0x2000);
            d_mmio.write64(ar1 + 0x10, (A_PERIPH_MAIN - win1) |
                                       (0x6ULL << 56) | (1ULL << 63));
            d_mmio.write64(ar2 + 0x00, win2);
            d_mmio.write64(ar2 + 0x08, win2 + 0x1000);
            d_mmio.write64(ar2 + 0x10, (A_SPM - win2) |
                                       (0x0ULL << 56) | (1ULL << 63));

            // Hit region 1 with AxCACHE=0x0 → 0x6 ; low 12 bits (0xABC) kept.
            d_mmio.axi_cache_in = 0x0;
            {
                const unsigned h = p_periph.hits;
                EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                          d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win1 + 0x1ABC, &scratch,
                                      /*with_ext=*/true, smc::SMC_ID));
                EXPECT_EQ(h + 1u, p_periph.hits);
                EXPECT_EQ(A_PERIPH_MAIN + 0x1ABC, p_periph.last_addr);
                EXPECT_TRUE(p_periph.had_ext);
                EXPECT_EQ(0x6u, static_cast<unsigned>(p_periph.last_cache));
                EXPECT_TRUE(dut.get_remap_debug().hit);
                EXPECT_EQ(1u, dut.get_remap_debug().region_index);
            }
            // Hit region 1 with AxCACHE=0xF → still 0x6 (replace, not OR).
            d_mmio.axi_cache_in = 0xF;
            {
                d_mmio.xfer(tlm::TLM_READ_COMMAND, win1 + 0x0004, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(A_PERIPH_MAIN + 0x0004, p_periph.last_addr);
                EXPECT_EQ(0x6u, static_cast<unsigned>(p_periph.last_cache));
            }
            // Hit region 2 (cacheable=0x0) with AxCACHE=0xF → 0x0 (not AND).
            {
                const unsigned h = p_front.hits;
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win2 + 0x0010, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(h + 1u, p_front.hits);
                EXPECT_EQ(A_SPM + 0x0010, p_front.last_addr);
                EXPECT_EQ(0x0u, static_cast<unsigned>(p_front.last_cache));
                EXPECT_EQ(2u, dut.get_remap_debug().region_index);
            }
            // Miss (one past region 1 end) → address & AxCACHE untouched.
            d_mmio.axi_cache_in = 0xB;
            {
                const unsigned h = p_out.hits;
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win1 + 0x2000, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(h + 1u, p_out.hits);
                EXPECT_EQ(win1 + 0x2000, p_out.last_addr);
                EXPECT_EQ(0xBu, static_cast<unsigned>(p_out.last_cache));
                EXPECT_TRUE(!dut.get_remap_debug().hit);
            }
            // Hit without an extension: address remaps, nothing to retag.
            {
                const unsigned h = p_periph.hits;
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win1 + 0x0100, &scratch);
                EXPECT_EQ(h + 1u, p_periph.hits);
                EXPECT_EQ(A_PERIPH_MAIN + 0x0100, p_periph.last_addr);
                EXPECT_TRUE(!p_periph.had_ext);
            }
            // Lowest-index priority: overlap region 0 onto region 1's window
            // with a different cacheable (0x9) → region 0 wins.
            d_mmio.write64(A_AR + 0x00, win1);
            d_mmio.write64(A_AR + 0x08, win1 + 0x1000);
            d_mmio.write64(A_AR + 0x10, (A_PERIPH_MAIN - win1) |
                                        (0x9ULL << 56) | (1ULL << 63));
            d_mmio.axi_cache_in = 0x0;
            {
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win1 + 0x0020, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(A_PERIPH_MAIN + 0x0020, p_periph.last_addr);
                EXPECT_EQ(0x9u, static_cast<unsigned>(p_periph.last_cache));
                EXPECT_EQ(0u, dut.get_remap_debug().region_index);
                // Beyond region 0's end but inside region 1 → region 1 (0x6).
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win1 + 0x1020, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(A_PERIPH_MAIN + 0x1020, p_periph.last_addr);
                EXPECT_EQ(0x6u, static_cast<unsigned>(p_periph.last_cache));
                EXPECT_EQ(1u, dut.get_remap_debug().region_index);
            }
            // start == end (after [55:12] masking) → empty window → miss
            // even with valid set.
            const uint64_t ar3  = A_AR + 3 * 0x20;
            const uint64_t win3 = 0x9700'0000ULL;
            d_mmio.write64(ar3 + 0x00, win3);
            d_mmio.write64(ar3 + 0x08, win3 + 0x0FFF);                // masks to win3
            d_mmio.write64(ar3 + 0x10, (1ULL << 63));
            EXPECT_EQ(win3, dut.get_alias_region(3).end);
            {
                const unsigned h = p_out.hits;
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win3 + 0x10, &scratch);
                EXPECT_EQ(h + 1u, p_out.hits);
                EXPECT_EQ(win3 + 0x10, p_out.last_addr);
                EXPECT_TRUE(!dut.get_remap_debug().hit);
            }
            // 44-bit modular add (RTL: addr[55:12] + offset[55:12], carry
            // dropped).  offset[55:12] = all-ones ≡ −1 page, so the window
            // 0x9500_0000 lands on 0x94FF_F000 with low 12 bits kept.
            const uint64_t ar4  = A_AR + 4 * 0x20;
            const uint64_t win4 = 0x9500'0000ULL;
            d_mmio.write64(ar4 + 0x00, win4);
            d_mmio.write64(ar4 + 0x08, win4 + 0x1000);
            d_mmio.write64(ar4 + 0x10, 0x00FF'FFFF'FFFF'F000ULL |
                                       (0x2ULL << 56) | (1ULL << 63));
            {
                const unsigned h = p_out.hits;
                d_mmio.xfer(tlm::TLM_WRITE_COMMAND, win4 + 0x0F04, &scratch,
                            /*with_ext=*/true, smc::SMC_ID);
                EXPECT_EQ(h + 1u, p_out.hits);
                EXPECT_EQ(0x94FF'FF04ULL, p_out.last_addr);
                EXPECT_EQ(0x2u, static_cast<unsigned>(p_out.last_cache));
                EXPECT_TRUE(dut.get_remap_debug().hit);
                EXPECT_EQ(4u, dut.get_remap_debug().region_index);
            }
            d_mmio.axi_cache_in = 0x0;
        }
        std::cout << "  [PASS] alias-remap AxCACHE replacement / modular add\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 20. Output-remap 64-bit write + IGNORE_COMMAND.
        // ----------------------------------------------------------------
        // Reserved [62:56] written as ones are dropped; valid[63] is kept.
        d_mmio.write64(A_MR + 4 * 0x08 + 0x00, 0xFF00'00AA'BBCC'DDEEULL);
        EXPECT_EQ(0xBBCC'DDEEu, d_mmio.read32(A_MR + 4 * 0x08 + 0x00));
        EXPECT_EQ(0x8000'00AAu, d_mmio.read32(A_MR + 4 * 0x08 + 0x04));
        {
            uint64_t rd = 0;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      d_mmio.raw(tlm::TLM_READ_COMMAND, A_MR + 4 * 0x08, 8,
                                 reinterpret_cast<uint8_t*>(&rd)));
            EXPECT_EQ(0x8000'00AA'BBCC'DDEEULL, rd);
        }
        d_mmio.write64(A_MR + 4 * 0x08 + 0x00, 0x0000'00AA'BBCC'DDEEULL);   // clear valid
        EXPECT_EQ(0x0000'00AAu, d_mmio.read32(A_MR + 4 * 0x08 + 0x04));
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_IGNORE_COMMAND, A_MR, 4,
                             reinterpret_cast<uint8_t*>(&ignore)));
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_IGNORE_COMMAND, A_XR, 4,
                             reinterpret_cast<uint8_t*>(&ignore)));
        std::cout << "  [PASS] output-remap 64-bit CSR write\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 21. Filter CSR edge cases.
        // ----------------------------------------------------------------
        {
            const uint64_t e2 = A_IBF + 2 * 0x20;
            d_mmio.write64(e2 + 0x00, CFG_RW_NS | CFG_ALLOW_BURST |
                                       (static_cast<uint64_t>(7u) << 16));
            EXPECT_EQ(0x3u, (d_mmio.read32(e2 + 0x00) >> 12) & 0x7u);
            EXPECT_EQ(7u, dut.get_inbound_filter_entry(2).src_id);
            EXPECT_TRUE(dut.get_inbound_filter_entry(2).allow_burst);

            d_mmio.write32(e2 + 0x10, 0xFFFF'FFFFu);
            d_mmio.write32(e2 + 0x14, 0x00AB'CDEFu);
            EXPECT_EQ(0x00AB'CDEFu, d_mmio.read32(e2 + 0x14));

            d_mmio.write32(e2 + 0x18, 0x1234'5678u);
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      d_mmio.raw(tlm::TLM_IGNORE_COMMAND, e2, 4,
                                 reinterpret_cast<uint8_t*>(&ignore)));

            // Outbound bank honours the same lock → DECERR contract.
            const uint64_t o3 = A_OBF + 3 * 0x20;
            d_mmio.write64(o3 + 0x08, 0x0000'00AB'0000'0000ULL);
            d_mmio.write64(o3 + 0x00, CFG_RW_NS | (static_cast<uint64_t>(1) << 63));
            EXPECT_TRUE(dut.get_outbound_filter_entry(3).locked);
            uint32_t junk = 0;
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.xfer(tlm::TLM_WRITE_COMMAND, o3 + 0x00, &junk));
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.xfer(tlm::TLM_WRITE_COMMAND, o3 + 0x0C, &junk));
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      d_mmio.xfer(tlm::TLM_WRITE_COMMAND, o3 + 0x14, &junk));
            EXPECT_EQ(0x0000'00ABu, d_mmio.read32(o3 + 0x0C));   // START high word intact
            EXPECT_TRUE((d_mmio.read32(o3 + 0x00) & CFG_EN) != 0u);
        }

        pulse_reset();
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL,
                       CFG_RW_NS | (9u << 16));
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 9, true));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 3, true));

        pulse_reset();
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL, CFG_READ_NS);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, true));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_sys.xfer(tlm::TLM_WRITE_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, true));

        pulse_reset();
        program_filter(A_IBF, 0xC000'0000ULL, 0x0100'0000ULL, CFG_WRITE_NS);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_sys.xfer(tlm::TLM_WRITE_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, true));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch,
                             true, 0, true));

        pulse_reset();
        program_filter(A_IBF, 0xC000'2010ULL, 0x10ULL,
                       CFG_RW_NS | CFG_ALLOW_BURST);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_sys.xfer(tlm::TLM_READ_COMMAND, 0xC000'2018ULL, &scratch,
                             true, 0, true));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_READ_COMMAND, A_IBF + 16 * 0x20, 4,
                             reinterpret_cast<uint8_t*>(&ignore)));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.raw(tlm::TLM_READ_COMMAND, A_AR + 8 * 0x20, 4,
                             reinterpret_cast<uint8_t*>(&ignore)));
        std::cout << "  [PASS] filter CSR edge cases\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 22. Outbound-filter NS matching (plain path forces SMC_SRC_ID=3).
        // ----------------------------------------------------------------
        program_filter(A_OBF, A_OUTBOUND, 0x1000ULL, CFG_EN);  // secure-only deny
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  d_mmio.xfer(tlm::TLM_WRITE_COMMAND, A_OUTBOUND, &scratch,
                              true, 0, false));
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  d_mmio.xfer(tlm::TLM_WRITE_COMMAND, A_OUTBOUND, &scratch,
                              true, 0, true));
        std::cout << "  [PASS] outbound-filter NS matching\n";

        // ----------------------------------------------------------------
        // 23. DMI denied on jtag / data_accel / log.
        // ----------------------------------------------------------------
        EXPECT_EQ(false, d_jtag.dmi_query(A_SPM));
        EXPECT_EQ(false, d_daccel.dmi_query(A_DMA));
        EXPECT_EQ(false, d_log.dmi_query(A_PERIPH_MAIN));
        std::cout << "  [PASS] DMI denied on all internal masters\n";

        // ----------------------------------------------------------------
        // 24. Dynamic REGION_SIZE controls membership and local mask.
        // ----------------------------------------------------------------
        pulse_reset();
        d_mmio.write32(A_RSIZE, 0x0400'0000u);
        {
            const unsigned h = p_periph.hits;
            d_mmio.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_EXT, &scratch);
            EXPECT_EQ(h + 1u, p_periph.hits);
            EXPECT_EQ(A_PERIPH_EXT, p_periph.last_addr);
        }
        d_mmio.write32(A_RSIZE, 0x1000u);
        {
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_READ_COMMAND, A_PERIPH_MAIN, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(A_PERIPH_MAIN, p_out.last_addr);
        }
        // A deliberately undersized aperture makes its own CSR unreachable;
        // use the labeled test API to restore it before the next frontdoor case.
        dut.write_region_size(0x0100'0000u);
        d_mmio.write32(A_RSIZE, 0u);
        {
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_READ_COMMAND, A_SPM, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
        }
        dut.write_region_size(0x0100'0000u);
        std::cout << "  [PASS] dynamic REGION_SIZE routing\n";

        // ----------------------------------------------------------------
        // 25. RDL-faithful base-config storage and irq_test gating.
        // ----------------------------------------------------------------
        pulse_reset();
        EXPECT_EQ(0x1F00'0000u, d_mmio.read32(A_CLOCK_GATE));
        d_mmio.write32(A_CLOCK_GATE, 0xFFFF'FFFFu);
        EXPECT_EQ(0x3F00'3FFFu, d_mmio.read32(A_CLOCK_GATE));
        EXPECT_EQ(0x1000u, d_mmio.read32(A_HANG_SYS_THR));
        EXPECT_EQ(0x1000u, d_mmio.read32(A_HANG_SEP_THR));
        EXPECT_EQ(0x1000u, d_mmio.read32(A_HANG_DATA_THR));
        d_mmio.write32(A_HANG_SYS_CTRL, 0x111u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        EXPECT_EQ(0x1111u, d_mmio.read32(A_HANG_SYS_CTRL));
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_SEP_CTRL) & 0x1000u);
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_DATA_CTRL) & 0x1000u);
        d_mmio.write32(A_HANG_SYS_CTRL, 0x1000u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_SYS_CTRL));
        EXPECT_EQ(false, hang_irq.read());
        d_mmio.write32(A_HANG_SYS_CTRL, 0x111u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        d_mmio.write32(A_HANG_SYS_CTRL, 0x101u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());
        EXPECT_EQ(0x101u, d_mmio.read32(A_HANG_SYS_CTRL));  // irq_en gates irq[12]
        d_mmio.write32(A_HANG_SYS_CTRL, 0x110u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());
        EXPECT_EQ(0x110u, d_mmio.read32(A_HANG_SYS_CTRL));  // enable gates irq[12]
        // Each detector's status is its own: SEP alone interrupting.
        d_mmio.write32(A_HANG_SYS_CTRL, 0u);
        d_mmio.write32(A_HANG_SEP_CTRL, 0x111u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_SYS_CTRL));
        EXPECT_EQ(0x1111u, d_mmio.read32(A_HANG_SEP_CTRL));
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_DATA_CTRL));
        d_mmio.write32(A_HANG_SEP_CTRL, 0u);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());
        std::cout << "  [PASS] hang CSR masks and irq_test gating\n";

        // ----------------------------------------------------------------
        // 26. Three event-driven detector legs, OR, and threshold edges.
        // ----------------------------------------------------------------
        pulse_reset();
        program_filter(A_IBF, A_SPM, 0x1000u, CFG_RW_NS);
        d_mmio.write32(A_HANG_SYS_THR, 2u);
        d_mmio.write32(A_HANG_SEP_THR, 2u);
        d_mmio.write32(A_HANG_DATA_THR, 2u);
        d_mmio.write32(A_HANG_SYS_CTRL, 0x11u);
        d_mmio.write32(A_HANG_SEP_CTRL, 0x11u);
        d_mmio.write32(A_HANG_DATA_CTRL, 0x11u);
        p_front.block_for = sc_time(5, SC_NS);
        p_periph.block_for = sc_time(8, SC_NS);
        sys_done = sep_done = daccel_done = false;
        start_sys.notify(SC_ZERO_TIME);
        start_sep.notify(SC_ZERO_TIME);
        start_daccel.notify(SC_ZERO_TIME);
        sc_core::wait(1, SC_NS);
        EXPECT_EQ(false, hang_irq.read()); // T-epsilon
        sc_core::wait(1, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());      // T
        EXPECT_EQ(0x1011u, d_mmio.read32(A_HANG_SYS_CTRL));
        EXPECT_EQ(0x1011u, d_mmio.read32(A_HANG_SEP_CTRL));
        EXPECT_EQ(0x1011u, d_mmio.read32(A_HANG_DATA_CTRL));
        sc_core::wait(3, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(sys_done && sep_done);
        EXPECT_TRUE(hang_irq.read());      // data-accelerator leg still hung
        EXPECT_EQ(0u, d_mmio.read32(A_HANG_SYS_CTRL) & 0x1000u);
        EXPECT_EQ(0x1011u, d_mmio.read32(A_HANG_DATA_CTRL));
        sc_core::wait(3, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(daccel_done);
        EXPECT_EQ(false, hang_irq.read()); // T+completion
        p_front.block_for = SC_ZERO_TIME;
        p_periph.block_for = SC_ZERO_TIME;
        std::cout << "  [PASS] three-leg timeout and combined OR\n";

        // ----------------------------------------------------------------
        // 27. Zero threshold, mid-window update, progress rearm, reset cancel.
        // ----------------------------------------------------------------
        d_mmio.write32(A_HANG_SEP_THR, 0u);
        p_front.block_for = sc_time(4, SC_NS);
        sep_done = false;
        start_sep.notify(SC_ZERO_TIME);
        sc_core::wait(4, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());
        EXPECT_TRUE(sep_done);

        d_mmio.write32(A_HANG_SEP_THR, 10u);
        p_front.block_for = sc_time(12, SC_NS);
        sep_done = false;
        start_sep.notify(SC_ZERO_TIME);
        sc_core::wait(1, SC_NS);
        d_mmio.write32(A_HANG_SEP_THR, 1u); // applies to next window
        sc_core::wait(8, SC_NS);
        EXPECT_EQ(false, hang_irq.read());
        sc_core::wait(1, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        sc_core::wait(2, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(sep_done);
        EXPECT_EQ(false, hang_irq.read());

        d_mmio.write32(A_HANG_SEP_THR, 2u);
        p_front.block_for = SC_ZERO_TIME;
        p_front.block_sequence = {sc_time(3, SC_NS), sc_time(7, SC_NS)};
        start_sep.notify(SC_ZERO_TIME);
        start_sep2.notify(SC_ZERO_TIME);
        sc_core::wait(2, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        sc_core::wait(1, SC_NS); // first completion is progress and rearms
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());
        sc_core::wait(2, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_TRUE(hang_irq.read());
        sc_core::wait(2, SC_NS);
        sc_core::wait(SC_ZERO_TIME);
        sc_core::wait(SC_ZERO_TIME);
        EXPECT_EQ(false, hang_irq.read());

        p_front.block_for = sc_time(5, SC_NS);
        start_sep.notify(SC_ZERO_TIME);
        sc_core::wait(1, SC_NS);
        rst_n.write(false);
        sc_core::wait(2, SC_NS);
        EXPECT_EQ(false, hang_irq.read());
        rst_n.write(true);
        sc_core::wait(2, SC_NS);
        p_front.block_for = SC_ZERO_TIME;
        std::cout << "  [PASS] hang zero/update/progress/reset semantics\n";

        // ----------------------------------------------------------------
        // 28. no_addr_remap config bypasses output remap.
        // ----------------------------------------------------------------
        rst_n_nr.write(true);
        sc_core::wait(1, SC_NS);
        rst_n_nr.write(false);
        sc_core::wait(1, SC_NS);
        rst_n_nr.write(true);
        sc_core::wait(1, SC_NS);
        {
            // Program a VALID M-mode region 0 so the bypass is discriminating
            // (an unprogrammed region is passthrough anyway since #2572).
            d_mmio_nr.write32(A_MR + 0 * 0x08 + 0x00, 0x1230'0000u);
            d_mmio_nr.write32(A_MR + 0 * 0x08 + 0x04, 0x8000'0000u);
            EXPECT_EQ(0x8000'0000u, d_mmio_nr.read32(A_MR + 0 * 0x08 + 0x04));
            const uint64_t mm_addr =
                dut_noremap.read_global_base() + 0x0100'0000ULL + 0x55;
            const unsigned h = p_out_nr.hits;
            d_mmio_nr.axi_cache_in = 0x3;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      d_mmio_nr.xfer(tlm::TLM_WRITE_COMMAND, mm_addr, &scratch,
                                     /*with_ext=*/true, smc::SMC_ID));
            EXPECT_EQ(h + 1u, p_out_nr.hits);
            EXPECT_EQ(mm_addr, p_out_nr.last_addr);                 // not 0x1230_0055
            EXPECT_EQ(static_cast<uint16_t>(smc::SMC_ID), p_out_nr.last_src_id); // no retag
            EXPECT_EQ(0x3u, static_cast<unsigned>(p_out_nr.last_cache));
        }
        std::cout << "  [PASS] no_addr_remap bypass\n";

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
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
