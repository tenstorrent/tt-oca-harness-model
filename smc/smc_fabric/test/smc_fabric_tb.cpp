// SPDX-License-Identifier: Apache-2.0
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
//   12. Output remap (M-mode/Xvisor): bit-exact REGION_ATTRS.offset[55:0]
//   13. Filter CSR image: bit-exact FILTER_CONFIG/START_ADDR/END_ADDR + locked
//   14. Alternate internal masters (jtag / data_accel / log)
//   15. Testbench API + LOCAL_BASE / REGION_SIZE global CSRs
//   16. M-mode output remap via local-base aperture
//   17. PERIPH_EXT local decode (sys / sep direct)
//   18. Deny-read poison fill with trailing partial bytes
//   19. Alias-remap CSR read-back, cacheable bit, IGNORE_COMMAND
//   20. Output-remap 64-bit write + IGNORE_COMMAND
//   21. Filter CSR edge cases (src_id, R/W-only, burst, OOB entries)
//   22. Outbound-filter NS matching
//   23. DMI denied on jtag / data_accel / log
//   24. no_addr_remap config bypasses output remap
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstring>
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
    std::map<uint64_t, uint32_t> mem;

    explicit probe(sc_module_name n) : sc_core::sc_module(n), sock("sock") {
        sock.register_b_transport(this, &probe::b_tr);
    }

    void b_tr(tlm::tlm_generic_payload& gp, sc_time& /*delay*/) {
        ++hits;
        last_addr = gp.get_address();
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

        SC_THREAD(run);
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

    void run() {
        std::cout << "==== SMC Fabric TB ====\n";
        rst_n.write(true);
        sc_core::wait(1, SC_NS);

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
        // ----------------------------------------------------------------
        const uint64_t src   = 0x9000'0000ULL;
        const uint32_t off   = static_cast<uint32_t>(A_PERIPH_MAIN - src);
        d_mmio.write32(A_AR + 0x00, static_cast<uint32_t>(src));      // start
        d_mmio.write32(A_AR + 0x08, static_cast<uint32_t>(src) + 0x1000); // end
        d_mmio.write32(A_AR + 0x10, off);                            // offset
        d_mmio.write32(A_AR + 0x18, 0x1);                            // valid
        {
            auto r = dut.get_alias_region(0);
            EXPECT_EQ(src,            r.start);
            EXPECT_EQ(src + 0x1000,   r.end);
            EXPECT_TRUE(r.valid);
        }
        const unsigned periph_before = p_periph.hits;
        d_mmio.write32(src + 0x10, 0x7777);   // remapped → 0xC0002010
        EXPECT_EQ(periph_before + 1u, p_periph.hits);
        EXPECT_EQ(A_PERIPH_MAIN + 0x10, p_periph.last_addr);
        EXPECT_TRUE(dut.get_remap_debug().hit);
        EXPECT_EQ(0u, dut.get_remap_debug().region_index);
        std::cout << "  [PASS] alias remap\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 5. Outbound routing (default allow) → output_axi.
        // ----------------------------------------------------------------
        const unsigned out_before = p_out.hits;
        d_mmio.write32(A_OUTBOUND, 0x8888);
        EXPECT_EQ(out_before + 1u, p_out.hits);
        EXPECT_EQ(A_OUTBOUND, p_out.last_addr);  // plain path: addr unchanged

        // M-mode window: at reset offset==0 → {0, low20}.
        const unsigned out_mmode = p_out.hits;
        d_mmio.write32(0x4100'0055ULL, 0x9999);
        EXPECT_EQ(out_mmode + 1u, p_out.hits);
        EXPECT_EQ(0x0000'0055ULL, p_out.last_addr);
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
        // 12. Output remap — bit-exact to output_remap.sv:
        //       idx = (addr-window_base)[22:20];
        //       out = {offset[55:20], (addr-window_base)[19:0]}.
        //     Window base (outbound) = global_base + REMAP_START.
        // ----------------------------------------------------------------
        pulse_reset();
        const uint64_t mm_base = dut.read_global_base() + 0x0100'0000ULL; // 0x4100_0000
        const uint64_t xv_base = dut.read_global_base() + 0x0180'0000ULL; // 0x4180_0000

        // 12a. At reset offset==0, region 0 maps to {0, low20}.
        {
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, mm_base + 0x55, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x0000'0055ULL, p_out.last_addr);
        }

        // 12b. Program M-mode region 3 (stride 0x08, 64-bit REGION_ATTRS at
        //      +0x00). offset = 0xABC0_0000 → offset[55:20]=0xABC.
        d_mmio.write32(A_MR + 3 * 0x08 + 0x00, 0xABC0'0000u);  // offset[31:0]
        EXPECT_EQ(0xABC0'0000u, d_mmio.read32(A_MR + 3 * 0x08 + 0x00));
        {
            // addr in region 3: (addr-base)[22:20]==3, low20 preserved.
            const uint64_t a = mm_base + (3ULL << 20) + 0x120;   // 0x4130_0120
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0xABC0'0120ULL, p_out.last_addr);  // {0xABC, 0x00120}
        }

        // 12c. 56-bit field: high word at +0x04 = offset[55:32].
        d_mmio.write32(A_MR + 5 * 0x08 + 0x00, 0x0010'0000u);  // offset[31:0]
        d_mmio.write32(A_MR + 5 * 0x08 + 0x04, 0x0000'00DEu);  // offset[55:32]
        EXPECT_EQ(0x0010'0000u, d_mmio.read32(A_MR + 5 * 0x08 + 0x00));
        EXPECT_EQ(0x0000'00DEu, d_mmio.read32(A_MR + 5 * 0x08 + 0x04));
        {
            // offset = 0x0000_00DE_0010_0000 → offset[55:20] = 0xDE00_1.
            const uint64_t a = mm_base + (5ULL << 20) + 0x004; // region 5
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x0000'00DE'0010'0004ULL, p_out.last_addr);
        }
        std::cout << "  [PASS] M-mode output remap (bit-exact offset[55:0])\n";

        // 12d. Xvisor path uses the same encoding on its own window.
        d_mmio.write32(A_XR + 1 * 0x08 + 0x00, 0x7770'0000u);  // region 1 offset
        {
            const uint64_t a = xv_base + (1ULL << 20) + 0x0AB;
            const unsigned h = p_out.hits;
            d_mmio.xfer(tlm::TLM_WRITE_COMMAND, a, &scratch);
            EXPECT_EQ(h + 1u, p_out.hits);
            EXPECT_EQ(0x7770'00ABULL, p_out.last_addr);
        }
        std::cout << "  [PASS] Xvisor output remap\n";

        // 12e. Reset clears the offset registers (back to identity-to-low20).
        pulse_reset();
        EXPECT_EQ(0u, d_mmio.read32(A_MR + 3 * 0x08 + 0x00));
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

        // 13c. locked[63] is write-one-to-set and freezes the entry.
        d_mmio.write32(e1 + 0x04, CFG_LOCKED);       // set locked (high word, bit31)
        EXPECT_TRUE((d_mmio.read32(e1 + 0x04) & CFG_LOCKED) != 0u);  // locked reads back
        d_mmio.write32(e1 + 0x08, 0xDEAD'BEEFu);     // ignored — entry is locked
        EXPECT_EQ(0x1234'5678u, d_mmio.read32(e1 + 0x08));  // START_ADDR unchanged
        std::cout << "  [PASS] filter CSR image (CONFIG/START/END, locked)\n";

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
        std::cout << "  [PASS] deny-read poison trailing bytes\n";

        // ----------------------------------------------------------------
        // 19. Alias-remap CSR read-back + cacheable + IGNORE_COMMAND.
        // ----------------------------------------------------------------
        {
            const uint64_t ar0 = A_AR;
            d_mmio.write32(ar0 + 0x00, 0xA000'0000u);
            d_mmio.write32(ar0 + 0x08, 0xA000'1000u);
            d_mmio.write32(ar0 + 0x10, 0x0001'0000u);
            d_mmio.write32(ar0 + 0x18, 0x3u);  // valid | cacheable
            EXPECT_EQ(0xA000'0000u, d_mmio.read32(ar0 + 0x00));
            EXPECT_EQ(0xA000'1000u, d_mmio.read32(ar0 + 0x08));
            EXPECT_EQ(0x0001'0000u, d_mmio.read32(ar0 + 0x10));
            EXPECT_EQ(0x3u, d_mmio.read32(ar0 + 0x18));
            auto r = dut.get_alias_region(0);
            EXPECT_TRUE(r.valid);
            EXPECT_TRUE(r.cacheable);
            d_mmio.write32(ar0 + 0x1C, 0x99u);  // reserved field — ignored
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      d_mmio.raw(tlm::TLM_IGNORE_COMMAND, ar0, 4,
                                 reinterpret_cast<uint8_t*>(&ignore)));
        }
        std::cout << "  [PASS] alias-remap CSR read-back\n";

        pulse_reset();

        // ----------------------------------------------------------------
        // 20. Output-remap 64-bit write + IGNORE_COMMAND.
        // ----------------------------------------------------------------
        d_mmio.write64(A_MR + 4 * 0x08 + 0x00, 0x0000'00AA'BBCC'DDEEULL);
        EXPECT_EQ(0xBBCC'DDEEu, d_mmio.read32(A_MR + 4 * 0x08 + 0x00));
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
        // 24. no_addr_remap config bypasses output remap.
        // ----------------------------------------------------------------
        rst_n_nr.write(true);
        sc_core::wait(1, SC_NS);
        rst_n_nr.write(false);
        sc_core::wait(1, SC_NS);
        rst_n_nr.write(true);
        sc_core::wait(1, SC_NS);
        {
            const uint64_t mm_addr =
                dut_noremap.read_global_base() + 0x0100'0000ULL + 0x55;
            const unsigned h = p_out_nr.hits;
            d_mmio_nr.xfer(tlm::TLM_WRITE_COMMAND, mm_addr, &scratch);
            EXPECT_EQ(h + 1u, p_out_nr.hits);
            EXPECT_EQ(mm_addr, p_out_nr.last_addr);
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
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
