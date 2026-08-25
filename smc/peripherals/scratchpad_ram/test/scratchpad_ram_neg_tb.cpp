// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// scratchpad_ram_neg_tb.cpp -- negative-path and edge-case coverage for the
// SMC Scratchpad RAM model.
//
// Focuses on paths the primary bench cannot reach without being destructive
// (SC_REPORT_FATAL during construction):
//
//   - size_bytes == 0
//   - size_bytes % 8 != 0
//   - access_delay_ns < 0.0
//   - init_file does not exist (hex / bin)
//   - binary preload is empty / unreadable
//   - hex preload is malformed (non-hex chars, trailing junk)
//   - hex preload has only comments / blank lines (zero data lines)
//   - hex preload overflows size_bytes with a non-zero word
//
// Plus positive-path edges not covered elsewhere:
//
//   - dbg_read32 happy / OOB / misaligned
//   - init_file_format = "garbage" --> falls back to auto-detection
//   - hex preload with `0x` / `0X` prefix
//   - hex preload that overflows size_bytes with zero-only words (tolerated)
//   - ecc_enabled = false disables injection (read never errors)
//   - dbg_clear_ecc_errors clears an injected uncorrectable error
//
// Each fatal-path test installs a SC_THROW handler for SC_FATAL so the
// constructor throws sc_core::sc_report instead of aborting.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "scratchpad_ram.h"

namespace {

unsigned g_failures = 0;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__               \
                      << "  expected TRUE: " #cond "\n";                      \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

#define EXPECT_EQ(expected, actual)                                           \
    do {                                                                      \
        const auto _e = (expected);                                           \
        const auto _a = (actual);                                             \
        if (!(_e == _a)) {                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__               \
                      << "  expected=" << _e << " actual=" << _a << "\n";     \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

// Tiny temp-file helper; files live under /tmp and are unlinked on destruct.
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& tag, const std::string& suffix)
        : path("/tmp/scratchpad_neg_" + tag + suffix) {
        std::remove(path.c_str());
    }
    ~TempFile() { std::remove(path.c_str()); }

    void write_text(const std::string& body) const {
        std::ofstream f(path);
        f.write(body.data(), static_cast<std::streamsize>(body.size()));
    }
    void write_binary(const std::vector<uint8_t>& bytes) const {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
};

// CCI preset helper for one scratchpad_ram instance (one unique `base` name).
struct presets {
    cci::cci_broker_handle broker;
    std::string            base;

    void set_size(uint64_t v) {
        broker.set_preset_cci_value(base + ".size_bytes",
                                    cci::cci_value(uint64_t(v)));
    }
    void set_file(const std::string& p) {
        broker.set_preset_cci_value(base + ".init_file",
                                    cci::cci_value(std::string(p)));
    }
    void set_format(const std::string& fmt) {
        broker.set_preset_cci_value(base + ".init_file_format",
                                    cci::cci_value(std::string(fmt)));
    }
    void set_delay(double d) {
        broker.set_preset_cci_value(base + ".access_delay_ns",
                                    cci::cci_value(d));
    }
    void set_ecc(bool e) {
        broker.set_preset_cci_value(base + ".ecc_enabled",
                                    cci::cci_value(e));
    }
};

// Run `body`; return true iff it raised any exception (typically
// sc_core::sc_report from SC_REPORT_FATAL).
template <typename F>
bool expect_fatal(F&& body)
{
    try {
        body();
    } catch (const sc_core::sc_report&) { return true; }
      catch (const std::exception&)     { return true; }
      catch (...)                       { return true; }
    return false;
}

// Small initiator used for the ECC read-path checks (needs a bound socket).
struct probe : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<probe> sock;
    explicit probe(sc_core::sc_module_name n) : sc_module(n), sock("sock") {}

    tlm::tlm_response_status read64(uint64_t addr, uint64_t& out) {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&out));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL,
        sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_ERROR,
        sc_core::SC_DISPLAY);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    cci::cci_originator originator("originator");
    auto broker = cci::cci_get_global_broker(originator);

    std::cout << "==== SMC Scratchpad RAM negative-path TB ====\n";

    // -------------------------------------------------------------------
    // Constructor validation: size / alignment / delay
    // -------------------------------------------------------------------
    {
        presets p{broker, "spm_size0"};
        p.set_size(0);
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_size0"); }));
        std::cout << "  [PASS] size_bytes == 0 -> SC_FATAL\n";
    }
    {
        presets p{broker, "spm_unaligned"};
        p.set_size(7);
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_unaligned"); }));
        std::cout << "  [PASS] size_bytes % 8 != 0 -> SC_FATAL\n";
    }
    {
        presets p{broker, "spm_neg_delay"};
        p.set_size(0x100);
        p.set_delay(-1.0);
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_neg_delay"); }));
        std::cout << "  [PASS] access_delay_ns < 0 -> SC_FATAL\n";
    }

    // -------------------------------------------------------------------
    // Preload-file errors
    // -------------------------------------------------------------------
    {
        presets p{broker, "spm_no_hex"};
        p.set_size(0x100);
        p.set_file("/no/such/path/spm.hex");
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_no_hex"); }));
        std::cout << "  [PASS] hex preload missing -> SC_FATAL\n";
    }
    {
        presets p{broker, "spm_no_bin"};
        p.set_size(0x100);
        p.set_file("/no/such/path/spm.img");
        p.set_format("bin");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_no_bin"); }));
        std::cout << "  [PASS] binary preload missing -> SC_FATAL\n";
    }
    {
        TempFile empty("emptybin", ".img");
        empty.write_binary({});
        presets p{broker, "spm_empty_bin"};
        p.set_size(0x100);
        p.set_file(empty.path);
        p.set_format("auto");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_empty_bin"); }));
        std::cout << "  [PASS] empty .img preload -> SC_FATAL\n";
    }
    {
        TempFile bad("nonhex", ".hex");
        bad.write_text("GGGGGGGGGGGGGGGG\n");
        presets p{broker, "spm_bad_hex"};
        p.set_size(0x100);
        p.set_file(bad.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_bad_hex"); }));
        std::cout << "  [PASS] malformed hex (non-hex chars) -> SC_FATAL\n";
    }
    {
        TempFile bad("trailjunk", ".hex");
        bad.write_text("DEADBEEFG\n");
        presets p{broker, "spm_trail_junk"};
        p.set_size(0x100);
        p.set_file(bad.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_trail_junk"); }));
        std::cout << "  [PASS] hex with trailing junk -> SC_FATAL\n";
    }
    {
        TempFile nodata("nodata", ".hex");
        nodata.write_text("# comment-only\n\n   \n#another comment\n");
        presets p{broker, "spm_no_data"};
        p.set_size(0x100);
        p.set_file(nodata.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_no_data"); }));
        std::cout << "  [PASS] hex with no data lines -> SC_FATAL\n";
    }
    {
        TempFile big("overnz", ".hex");
        big.write_text("0000000000000001\n"
                       "0000000000000002\n"
                       "00000000DEADBEEF\n");
        presets p{broker, "spm_overflow_nz"};
        p.set_size(0x10);
        p.set_file(big.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{ smc::scratchpad_ram r("spm_overflow_nz"); }));
        std::cout << "  [PASS] hex overflow with non-zero word -> SC_FATAL\n";
    }

    // -------------------------------------------------------------------
    // Positive-path edges
    // -------------------------------------------------------------------
    {
        TempFile good("prefix", ".hex");
        good.write_text("0xCAFEBABEDEADBEEF\n"
                        "0X0000000012345678\n");
        presets p{broker, "spm_prefix"};
        p.set_size(0x10);
        p.set_file(good.path);
        p.set_format("hex");
        smc::scratchpad_ram r("spm_prefix");
        EXPECT_EQ(uint64_t(0xCAFEBABEDEADBEEFULL), r.dbg_read64(0));
        EXPECT_EQ(uint64_t(0x0000000012345678ULL), r.dbg_read64(8));
        std::cout << "  [PASS] hex `0x` / `0X` prefix is stripped\n";
    }
    {
        TempFile pad("overz", ".hex");
        pad.write_text("0000000000000001\n"
                       "0000000000000002\n"
                       "0000000000000000\n"
                       "0000000000000000\n");
        presets p{broker, "spm_overflow_zero"};
        p.set_size(0x10);
        p.set_file(pad.path);
        p.set_format("hex");
        smc::scratchpad_ram r("spm_overflow_zero");
        EXPECT_EQ(uint64_t(1), r.dbg_read64(0));
        EXPECT_EQ(uint64_t(2), r.dbg_read64(8));
        std::cout << "  [PASS] hex overflow with zero words is tolerated\n";
    }
    {
        TempFile fall("garbagefmt", ".hex");
        fall.write_text("00000000000000AA\n"
                        "00000000000000BB\n");
        presets p{broker, "spm_garbage_fmt"};
        p.set_size(0x10);
        p.set_file(fall.path);
        p.set_format("garbage");
        smc::scratchpad_ram r("spm_garbage_fmt");
        EXPECT_EQ(uint64_t(0xAA), r.dbg_read64(0));
        EXPECT_EQ(uint64_t(0xBB), r.dbg_read64(8));
        std::cout << "  [PASS] init_file_format='garbage' falls back to auto\n";
    }
    {
        TempFile good("dbg32", ".hex");
        good.write_text("89ABCDEF01234567\n"
                        "FEDCBA9876543210\n");
        presets p{broker, "spm_dbg32"};
        p.set_size(0x10);
        p.set_file(good.path);
        p.set_format("hex");
        smc::scratchpad_ram r("spm_dbg32");
        EXPECT_EQ(uint32_t(0x01234567), r.dbg_read32(0));
        EXPECT_EQ(uint32_t(0x89ABCDEF), r.dbg_read32(4));
        EXPECT_EQ(uint32_t(0x76543210), r.dbg_read32(8));
        EXPECT_EQ(uint32_t(0xFEDCBA98), r.dbg_read32(12));
        EXPECT_EQ(uint32_t(0), r.dbg_read32(1));   // misaligned
        EXPECT_EQ(uint32_t(0), r.dbg_read32(0x10)); // OOB
        std::cout << "  [PASS] dbg_read32 happy / misaligned / OOB\n";
    }

    // -------------------------------------------------------------------
    // ECC behaviour edges (need a bound socket for the read path)
    // -------------------------------------------------------------------
    {
        // ecc_enabled = false: injection is a no-op, reads never error.
        presets p{broker, "spm_noecc"};
        p.set_size(0x40);
        p.set_ecc(false);
        smc::scratchpad_ram r("spm_noecc");
        probe pr("pr_noecc");
        pr.sock.bind(r.reg_socket);

        EXPECT_TRUE(!r.ecc_enabled());
        r.dbg_inject_ecc_error(0, /*correctable=*/false); // ignored
        uint64_t v = 0;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, pr.read64(0, v));
        std::cout << "  [PASS] ecc_enabled=false disables injection\n";
    }
    {
        // dbg_clear_ecc_errors clears an injected uncorrectable error.
        presets p{broker, "spm_eccclr"};
        p.set_size(0x40);
        smc::scratchpad_ram r("spm_eccclr");
        probe pr("pr_eccclr");
        pr.sock.bind(r.reg_socket);

        uint64_t v = 0;
        r.dbg_inject_ecc_error(0x10, /*correctable=*/false);
        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE, pr.read64(0x10, v));
        r.dbg_clear_ecc_errors();
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, pr.read64(0x10, v));
        std::cout << "  [PASS] dbg_clear_ecc_errors recovers the read\n";
    }

    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << g_failures << " FAILURE(S)\n";
    }
    return g_failures == 0 ? 0 : 1;
}
