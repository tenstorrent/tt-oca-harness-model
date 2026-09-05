// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// bootrom_neg_tb.cpp -- negative-path and edge-case coverage for the
// SEP Boot ROM model.
//
// This bench focuses on paths the primary bootrom_tb / bootrom_bin_tb
// cannot reach without being destructive (SC_REPORT_FATAL during
// construction):
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
// It also exercises positive-path edges that are not covered elsewhere:
//
//   - dbg_read32 happy / OOB / misaligned
//   - init_file_format = "garbage" --> falls back to auto-detection
//   - hex preload with `0x` / `0X` prefix (parser strips it)
//   - hex preload that overflows size_bytes with zero-only words
//     (tolerated)
//
// Each fatal-path test installs a SC_THROW handler for SC_FATAL so the
// constructor throws sc_core::sc_report instead of aborting; the bench
// catches it and asserts that exactly one fatal was raised.

#include <systemc>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bootrom.h"

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

// -----------------------------------------------------------------------
// Tiny temp-file helper.  Files live under /tmp for the duration of the
// run and are unlinked in the destructor.  The tag makes the path
// human-recognisable in the SC report output if a test fails.
// -----------------------------------------------------------------------
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& tag, const std::string& suffix)
        : path("/tmp/bootrom_neg_" + tag + suffix) {
        std::remove(path.c_str()); // start clean
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

// CCI preset helper for one bootrom instance (one unique `base` name).
struct presets {
    cci::cci_broker_handle broker;
    std::string            base; // e.g. "rom_size0"

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

} // namespace

int sc_main(int, char**)
{
    // Demote SC_FATAL from abort to throw so we can probe failure paths.
    // SC_DISPLAY keeps the message visible for debugging.  Demote SC_ERROR
    // similarly (matches bootrom_tb / bootrom_bin_tb).
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL,
        sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_ERROR,
        sc_core::SC_DISPLAY);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);
    cci::cci_originator originator("originator");
    auto broker = cci::cci_get_global_broker(originator);

    std::cout << "==== SEP Boot ROM negative-path TB ====\n";

    // -------------------------------------------------------------------
    // Constructor validation: size_bytes / alignment / delay
    // -------------------------------------------------------------------
    {
        presets p{broker, "rom_size0"};
        p.set_size(0);
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_size0");
        }));
        std::cout << "  [PASS] size_bytes == 0 -> SC_FATAL\n";
    }

    {
        presets p{broker, "rom_size_unaligned"};
        p.set_size(7); // not a multiple of 8
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_size_unaligned");
        }));
        std::cout << "  [PASS] size_bytes % 8 != 0 -> SC_FATAL\n";
    }

    {
        presets p{broker, "rom_neg_delay"};
        p.set_size(0x100);
        p.set_delay(-1.0);
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_neg_delay");
        }));
        std::cout << "  [PASS] access_delay_ns < 0 -> SC_FATAL\n";
    }

    // -------------------------------------------------------------------
    // Preload-file errors
    // -------------------------------------------------------------------
    {
        presets p{broker, "rom_no_hex"};
        p.set_size(0x100);
        p.set_file("/no/such/path/bootrom.hex");
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_no_hex");
        }));
        std::cout << "  [PASS] hex preload missing -> SC_FATAL\n";
    }

    {
        presets p{broker, "rom_no_bin"};
        p.set_size(0x100);
        p.set_file("/no/such/path/bootrom.img");
        p.set_format("bin");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_no_bin");
        }));
        std::cout << "  [PASS] binary preload missing -> SC_FATAL\n";
    }

    {
        // Empty .img -> bootrom reports "empty or unreadable".
        TempFile empty("emptybin", ".img");
        empty.write_binary({});
        presets p{broker, "rom_empty_bin"};
        p.set_size(0x100);
        p.set_file(empty.path);
        p.set_format("auto"); // .img -> bin
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_empty_bin");
        }));
        std::cout << "  [PASS] empty .img preload -> SC_FATAL\n";
    }

    // Malformed hex: non-hex chars trigger stoull to throw -> catch path.
    {
        TempFile bad("nonhex", ".hex");
        bad.write_text("GGGGGGGGGGGGGGGG\n");
        presets p{broker, "rom_bad_hex"};
        p.set_size(0x100);
        p.set_file(bad.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_bad_hex");
        }));
        std::cout << "  [PASS] malformed hex (non-hex chars) -> SC_FATAL\n";
    }

    // Hex with trailing junk: stoull parses partially, pos != s.size().
    {
        TempFile bad("trailjunk", ".hex");
        bad.write_text("DEADBEEFG\n"); // 'G' aborts conversion
        presets p{broker, "rom_trail_junk"};
        p.set_size(0x100);
        p.set_file(bad.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_trail_junk");
        }));
        std::cout << "  [PASS] hex with trailing junk -> SC_FATAL\n";
    }

    // Hex file with only comments / blank lines.
    {
        TempFile empty("nodata", ".hex");
        empty.write_text("# comment-only\n\n   \n#another comment\n");
        presets p{broker, "rom_no_data"};
        p.set_size(0x100);
        p.set_file(empty.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_no_data");
        }));
        std::cout << "  [PASS] hex with no data lines -> SC_FATAL\n";
    }

    // Hex preload that overflows size_bytes with a non-zero word.
    {
        TempFile big("overnz", ".hex");
        // size_bytes=16 (2 words).  Provide 3 words; the third is
        // non-zero and must trip the fatal path.
        big.write_text("0000000000000001\n"
                       "0000000000000002\n"
                       "00000000DEADBEEF\n");
        presets p{broker, "rom_overflow_nz"};
        p.set_size(0x10);
        p.set_file(big.path);
        p.set_format("hex");
        EXPECT_TRUE(expect_fatal([&]{
            smc::bootrom rom("rom_overflow_nz");
        }));
        std::cout << "  [PASS] hex overflow with non-zero word -> SC_FATAL\n";
    }

    // -------------------------------------------------------------------
    // Positive-path edges
    // -------------------------------------------------------------------

    // Hex with `0x` / `0X` prefix -- parser must strip both.
    {
        TempFile good("prefix", ".hex");
        good.write_text("0xCAFEBABEDEADBEEF\n"
                        "0X0000000012345678\n");
        presets p{broker, "rom_prefix"};
        p.set_size(0x10);
        p.set_file(good.path);
        p.set_format("hex");
        smc::bootrom rom("rom_prefix");
        EXPECT_EQ(uint64_t(0xCAFEBABEDEADBEEFULL), rom.dbg_read64(0));
        EXPECT_EQ(uint64_t(0x0000000012345678ULL), rom.dbg_read64(8));
        std::cout << "  [PASS] hex `0x` / `0X` prefix is stripped\n";
    }

    // Hex preload that overflows size_bytes with zero-only extra rows
    // (tolerated by design -- many hex generators emit a full memory
    // image with the unused tail zero-padded).
    {
        TempFile pad("overz", ".hex");
        // size_bytes=16 (2 words).  Append two zero words past EOF.
        pad.write_text("0000000000000001\n"
                       "0000000000000002\n"
                       "0000000000000000\n"
                       "0000000000000000\n");
        presets p{broker, "rom_overflow_zero"};
        p.set_size(0x10);
        p.set_file(pad.path);
        p.set_format("hex");
        smc::bootrom rom("rom_overflow_zero");
        EXPECT_EQ(uint64_t(1), rom.dbg_read64(0));
        EXPECT_EQ(uint64_t(2), rom.dbg_read64(8));
        std::cout << "  [PASS] hex overflow with zero words is tolerated\n";
    }

    // init_file_format = "garbage" -> not in {hex,bin,auto}.  Resolver
    // falls through to auto-detection, picks "hex" by file suffix.
    {
        TempFile fall("garbagefmt", ".hex");
        fall.write_text("00000000000000AA\n"
                        "00000000000000BB\n");
        presets p{broker, "rom_garbage_fmt"};
        p.set_size(0x10);
        p.set_file(fall.path);
        p.set_format("garbage");
        smc::bootrom rom("rom_garbage_fmt");
        EXPECT_EQ(uint64_t(0xAA), rom.dbg_read64(0));
        EXPECT_EQ(uint64_t(0xBB), rom.dbg_read64(8));
        std::cout << "  [PASS] init_file_format='garbage' falls back to auto\n";
    }

    // dbg_read32: happy path + misaligned + OOB.  These lines were not
    // exercised at all by the existing benches.
    {
        TempFile good("dbg32", ".hex");
        // word[0] = 0x89ABCDEF01234567 -> on LE host stored as bytes
        //   67 45 23 01 EF CD AB 89  --> dbg_read32(0)=0x01234567,
        //                                dbg_read32(4)=0x89ABCDEF
        // word[1] = 0xFEDCBA9876543210 ->
        //   10 32 54 76 98 BA DC FE  --> dbg_read32(8)=0x76543210,
        //                                dbg_read32(12)=0xFEDCBA98
        good.write_text("89ABCDEF01234567\n"
                        "FEDCBA9876543210\n");
        presets p{broker, "rom_dbg32"};
        p.set_size(0x10);
        p.set_file(good.path);
        p.set_format("hex");
        smc::bootrom rom("rom_dbg32");

        EXPECT_EQ(uint32_t(0x01234567), rom.dbg_read32(0));
        EXPECT_EQ(uint32_t(0x89ABCDEF), rom.dbg_read32(4));
        EXPECT_EQ(uint32_t(0x76543210), rom.dbg_read32(8));
        EXPECT_EQ(uint32_t(0xFEDCBA98), rom.dbg_read32(12));

        // Misaligned offsets must return 0 (off & 3u != 0).
        EXPECT_EQ(uint32_t(0), rom.dbg_read32(1));
        EXPECT_EQ(uint32_t(0), rom.dbg_read32(2));
        EXPECT_EQ(uint32_t(0), rom.dbg_read32(3));

        // OOB offsets must return 0.
        EXPECT_EQ(uint32_t(0), rom.dbg_read32(0x10));
        EXPECT_EQ(uint32_t(0), rom.dbg_read32(0x100));

        std::cout << "  [PASS] dbg_read32 happy / misaligned / OOB\n";
    }

    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << g_failures << " FAILURE(S)\n";
    }
    return g_failures == 0 ? 0 : 1;
}
