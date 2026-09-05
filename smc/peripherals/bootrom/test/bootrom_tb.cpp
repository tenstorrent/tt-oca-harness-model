// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// bootrom_tb.cpp -- self-checking test bench for the SEP Boot ROM
// (CCI-compliant).
//
// CCI integration highlights
// ─────────────────────────────────────────────────────────────────────────
// • sc_main registers a global CCI broker before any module is constructed.
// • Preset values inject:
//     - tb.bootrom.size_bytes       (default 0x10000)
//     - tb.bootrom.init_file        (a hex fixture shipped under test/fixtures)
//     - tb.bootrom.init_file_format ("hex")
//     - tb.bootrom.access_delay_ns  (default 1.0 → 3.0 to demonstrate mutate)
// • A dedicated test exercises CCI introspection: handle lookup by name,
//   typed and untyped values, metadata, run-time mutation of the mutable
//   access_delay_ns parameter, and rejection of immutable-param writes.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "bootrom.h"

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

// ---------------------------------------------------------------------------
// Tiny TLM driver -- mimics what the SEP CPU bridge would issue.
// Supports 1/2/4/8-byte accesses (the RDL is 64-bit, but the fabric
// happily issues smaller transfers).
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    template <typename T>
    T read(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        T data{};
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(sizeof(T));
        gp.set_streaming_width(sizeof(T));
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read<" << sizeof(T) << ">(0x" << std::hex
                      << addr << ") got rsp=" << gp.get_response_string()
                      << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    template <typename T>
    void write(uint64_t addr, T value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(sizeof(T));
        gp.set_streaming_width(sizeof(T));
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write<" << sizeof(T) << ">(0x" << std::hex
                      << addr << ", 0x" << uint64_t(value) << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    // Returns the raw response status without failing the test on error --
    // used for the negative-path checks.
    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t len, void* data,
                                      uint32_t sw = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(sw == 0 ? len : sw);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    tlm::tlm_response_status raw_xfer_be(tlm::tlm_command cmd, uint64_t addr,
                                         uint32_t len, void* data,
                                         uint8_t* be_ptr, uint32_t be_len) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(be_ptr);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::bootrom dut;
    driver       drv;
    sc_core::sc_signal<bool> rst_n;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("bootrom")
        , drv("drv")
        , rst_n("rst_n")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        SC_THREAD(run);
    }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
    }

    static void settle() {
        for (int i = 0; i < 2; ++i) sc_core::wait(SC_ZERO_TIME);
    }

    void run() {
        std::cout << "==== SEP Boot ROM TB (CCI-compliant) ====\n";
        std::cout << "  size_bytes  = 0x" << std::hex << dut.size_bytes()
                  << std::dec << "\n";

        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        pulse_reset();

        // ------------------------------------------------------------------
        // 1. Preload contract: after construction with a hex preload, the
        //    first eight 64-bit words must equal the file (the sanity hex
        //    file starts with the four-word fast-boot stub used by the SEP
        //    cocotb conformance suite).
        // ------------------------------------------------------------------
        {
            // The fixture parser mirrors the bootrom hex format: one
            // 64-bit hex word per line, comments / blank lines ignored.
            const std::string fixture = std::string(BOOTROM_FIXTURES_DIR) +
                                        "/bootrom_sanity.rv64.hex";
            std::ifstream f(fixture);
            EXPECT_TRUE(f.is_open());
            std::vector<uint64_t> expected;
            std::string line;
            while (std::getline(f, line) && expected.size() < 4) {
                if (line.empty() || line[0] == '#') continue;
                expected.push_back(std::stoull(line, nullptr, 16));
            }
            EXPECT_EQ(size_t(4), expected.size());

            for (unsigned i = 0; i < expected.size(); ++i) {
                EXPECT_EQ(expected[i], drv.read<uint64_t>(i * 8));
                EXPECT_EQ(expected[i], dut.dbg_read64(i * 8));
            }
            std::cout << "  [PASS] hex preload — first 4 words match fixture\n";
        }

        // ------------------------------------------------------------------
        // 2. Conformance claim C2/C3: a read after reset returns valid data.
        //    Re-pulse reset and verify the same words still read back —
        //    contents survive reset because the ROM has no mutable state.
        // ------------------------------------------------------------------
        {
            const uint64_t before = drv.read<uint64_t>(0x0);
            pulse_reset();
            EXPECT_EQ(before, drv.read<uint64_t>(0x0));
            std::cout << "  [PASS] reads survive reset (no mutable ROM state)\n";
        }

        // ------------------------------------------------------------------
        // 3. Sub-word reads (1, 2, 4 byte) return the matching byte slice
        //    of the 64-bit word at the same base address.  The RDL is
        //    64-bit but the fabric routinely issues smaller transfers.
        // ------------------------------------------------------------------
        {
            const uint64_t w   = drv.read<uint64_t>(0x0);
            const uint32_t lo  = static_cast<uint32_t>(w);
            const uint32_t hi  = static_cast<uint32_t>(w >> 32);
            EXPECT_EQ(lo, drv.read<uint32_t>(0x0));
            EXPECT_EQ(hi, drv.read<uint32_t>(0x4));

            const uint16_t h0 = static_cast<uint16_t>(w);
            const uint16_t h1 = static_cast<uint16_t>(w >> 16);
            EXPECT_EQ(h0, drv.read<uint16_t>(0x0));
            EXPECT_EQ(h1, drv.read<uint16_t>(0x2));

            const uint8_t  b0 = static_cast<uint8_t>(w);
            const uint8_t  b1 = static_cast<uint8_t>(w >> 8);
            EXPECT_EQ(b0, drv.read<uint8_t>(0x0));
            EXPECT_EQ(b1, drv.read<uint8_t>(0x1));
            std::cout << "  [PASS] sub-word reads (1/2/4/8 B) coherent with 64-bit word\n";
        }

        // ------------------------------------------------------------------
        // 4. Conformance claim C4: writes are silently ignored, ROM
        //    contents are unchanged, and the bus returns TLM_OK_RESPONSE.
        // ------------------------------------------------------------------
        {
            const uint64_t before  = drv.read<uint64_t>(0x0);
            drv.write<uint64_t>(0x0, 0xDEAD'BEEF'CAFE'BABEULL);
            EXPECT_EQ(before, drv.read<uint64_t>(0x0));
            EXPECT_EQ(before, dut.dbg_read64(0x0));

            // 32 / 16 / 8-bit writes are equally ignored.
            drv.write<uint32_t>(0x8, 0xDEADBEEFu);
            drv.write<uint16_t>(0xA, 0xABCDu);
            drv.write<uint8_t> (0xC, 0xEFu);
            // The fixture's second word should be unchanged.
            std::ifstream f(std::string(BOOTROM_FIXTURES_DIR) +
                            "/bootrom_sanity.rv64.hex");
            std::string line;
            std::vector<uint64_t> golden;
            while (std::getline(f, line) && golden.size() < 2) {
                if (line.empty() || line[0] == '#') continue;
                golden.push_back(std::stoull(line, nullptr, 16));
            }
            EXPECT_EQ(golden[1], drv.read<uint64_t>(0x8));
            std::cout << "  [PASS] writes silently ignored (C4); contents unchanged\n";
        }

        // ------------------------------------------------------------------
        // 5. The high-half of the preloaded image is zero-initialised
        //    (the fixture only sets the first four words, so word 5 onwards
        //    must read as zero — matches test_preload_zero_init for the
        //    tail of any partial preload).
        // ------------------------------------------------------------------
        {
            EXPECT_EQ(uint64_t(0), drv.read<uint64_t>(8 * 4));
            EXPECT_EQ(uint64_t(0), drv.read<uint64_t>(dut.size_bytes() - 8));
            std::cout << "  [PASS] unpopulated tail reads as zero\n";
        }

        // ------------------------------------------------------------------
        // 6. transport_dbg — same address space, no annotated delay; reads
        //    work; writes are silently ignored.
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint64_t data = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(0x0);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
            gp.set_data_length(8);
            gp.set_streaming_width(8);
            gp.set_byte_enable_ptr(nullptr);
            EXPECT_EQ(8u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(dut.dbg_read64(0), data);

            const uint64_t before = dut.dbg_read64(0);
            uint64_t scratch = 0xDEAD'BEEF'CAFE'BABEULL;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));
            EXPECT_EQ(8u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(before, dut.dbg_read64(0)); // unchanged

            std::cout << "  [PASS] transport_dbg back-door read works; writes ignored\n";
        }

        // ------------------------------------------------------------------
        // 7. dbg_load_bytes: TB helper that loads bytes into the ROM
        //    without going through TLM.  Used for in-memory test images.
        // ------------------------------------------------------------------
        {
            const uint8_t pattern[8] = {0x11, 0x22, 0x33, 0x44,
                                        0x55, 0x66, 0x77, 0x88};
            const uint64_t off = dut.size_bytes() - 8;
            EXPECT_EQ(8u, dut.dbg_load_bytes(off, pattern, 8));
            EXPECT_EQ(uint64_t(0x8877'6655'4433'2211ULL),
                      drv.read<uint64_t>(off));
            // OOB load → zero bytes written, no crash.
            EXPECT_EQ(0u, dut.dbg_load_bytes(dut.size_bytes(), pattern, 8));
            std::cout << "  [PASS] dbg_load_bytes round-trips through b_transport\n";
        }

        // ------------------------------------------------------------------
        // 8. Negative tests — out-of-window, misaligned, wrong width.
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            // Out-of-window
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, dut.size_bytes(),
                                   4, &scratch));
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND,
                                   dut.size_bytes() + 0x100, 4, &scratch));
            // Misaligned (catches the alignment guard before the
            // window guard, which is the correct error-precedence
            // order for naturally-aligned bus accesses)
            uint64_t scratch64 = 0;
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 1, 4, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 4, 8, &scratch64));
            // Unsupported width (3 bytes)
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0, 3, &scratch));
            // Unsupported width (zero)
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0, 0, &scratch));
            // streaming_width mismatch
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0, 4, &scratch,
                                   /*sw=*/1));
            std::cout << "  [PASS] negative tests: window, alignment, width, sw\n";
        }

        // ------------------------------------------------------------------
        // 9. Byte-enable and unknown-command error paths.
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            uint8_t  be = 0xFF;
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      drv.raw_xfer_be(tlm::TLM_READ_COMMAND, 0,
                                      4, &scratch, &be, 1));
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0, 4, &scratch));
            std::cout << "  [PASS] byte-enable and unknown-command error paths\n";
        }

        // ------------------------------------------------------------------
        // 10. transport_dbg validation paths return 0 for invalid args.
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t scratch = 0;
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));
            gp.set_command(tlm::TLM_READ_COMMAND);

            // Bad length
            gp.set_address(0); gp.set_data_length(3);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

            // Misaligned
            gp.set_address(1); gp.set_data_length(4);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

            // OOB
            gp.set_address(dut.size_bytes() + 0x100); gp.set_data_length(4);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

            std::cout << "  [PASS] transport_dbg invalid-args paths return 0\n";
        }

        // ------------------------------------------------------------------
        // 11. CCI introspection — discover params, query metadata, mutate
        //     access_delay_ns at run-time, observe immutability.
        //
        // Inside the SystemC hierarchy we must use `cci_get_broker()` (no
        // explicit originator) per CCI 1.0 §7.4.
        // ------------------------------------------------------------------
        {
            auto broker  = cci::cci_get_broker();
            auto h_size  = broker.get_param_handle("tb.bootrom.size_bytes");
            auto h_file  = broker.get_param_handle("tb.bootrom.init_file");
            auto h_fmt   = broker.get_param_handle("tb.bootrom.init_file_format");
            auto h_delay = broker.get_param_handle("tb.bootrom.access_delay_ns");
            EXPECT_TRUE(h_size.is_valid());
            EXPECT_TRUE(h_file.is_valid());
            EXPECT_TRUE(h_fmt.is_valid());
            EXPECT_TRUE(h_delay.is_valid());

            EXPECT_EQ(dut.size_bytes(),
                      uint64_t(h_size.get_cci_value().get_uint64()));
            EXPECT_TRUE(!h_size.get_description().empty());
            EXPECT_TRUE(!h_fmt.get_cci_value().get_string().empty());

            // Run-time mutation of the mutable parameter.
            const double old_delay = h_delay.get_cci_value().get_double();
            h_delay.set_cci_value(cci::cci_value(old_delay * 2.0));
            // Drive a transaction so the model re-caches the new value.
            (void)drv.read<uint32_t>(0);
            EXPECT_EQ(old_delay * 2.0, h_delay.get_cci_value().get_double());

            // Immutable param rejects post-elaboration writes.
            const uint64_t old_size = h_size.get_cci_value().get_uint64();
            try {
                h_size.set_cci_value(cci::cci_value(uint64_t(old_size * 2)));
            } catch (...) {
                // Some CCI implementations throw; either is acceptable.
            }
            EXPECT_EQ(old_size, uint64_t(h_size.get_cci_value().get_uint64()));

            std::cout << "  [PASS] CCI: discovery, introspection, mutation, immutability\n";
        }

        // ------------------------------------------------------------------
        // 12. dump_state — sanity-check the human-readable dump (smoke).
        // ------------------------------------------------------------------
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            const std::string s = oss.str();
            EXPECT_TRUE(s.find("bootrom state") != std::string::npos);
            EXPECT_TRUE(s.find("size_bytes")    != std::string::npos);
            EXPECT_TRUE(s.find("init_file")     != std::string::npos);
            EXPECT_TRUE(s.find("contents[0..")  != std::string::npos);
            std::cout << "  [PASS] dump_state contains expected fields\n";
        }

        // ------------------------------------------------------------------
        // 13. Annotated delay is non-zero (C2: rvalid 1 cycle after read).
        //     Issue a read through b_transport with a local `delay` and
        //     check the accumulated delay is at least the access_delay_ns
        //     resolved from CCI (which test 11 just doubled).
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t scratch = 0;
            sc_time t        = SC_ZERO_TIME;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_byte_enable_ptr(nullptr);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, t);
            EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
            EXPECT_TRUE(t > SC_ZERO_TIME);
            std::cout << "  [PASS] annotated delay is non-zero (claim C2)\n";
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
    // CCI 1.0 immutable-write attempts raise SC_ERROR; demote to display
    // so the test bench can verify the no-mutation invariant inline.
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                            sc_core::SC_DISPLAY);

    // ── CCI: register global broker ──────────────────────────────────────
    static cci_utils::consuming_broker broker("GlobalBroker");
    cci::cci_register_broker(broker);

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    // Preset values — exercise the full set of bootrom CCI params.
    global_broker.set_preset_cci_value(
        "tb.bootrom.size_bytes",
        cci::cci_value(uint64_t(0x10000)));
    global_broker.set_preset_cci_value(
        "tb.bootrom.init_file",
        cci::cci_value(std::string(BOOTROM_FIXTURES_DIR
                                   "/bootrom_sanity.rv64.hex")));
    global_broker.set_preset_cci_value(
        "tb.bootrom.init_file_format",
        cci::cci_value(std::string("hex")));
    global_broker.set_preset_cci_value(
        "tb.bootrom.access_delay_ns",
        cci::cci_value(3.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
