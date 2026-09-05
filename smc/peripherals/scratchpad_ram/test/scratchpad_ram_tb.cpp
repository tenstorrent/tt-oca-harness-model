// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// scratchpad_ram_tb.cpp -- self-checking test bench for the SMC Scratchpad
// RAM (CCI-compliant).
//
// Coverage:
//   • preload contract (hex) + read-back
//   • read-write semantics: write then read returns the written value
//   • sub-word reads/writes (1/2/4/8 B) coherent with the 64-bit word
//   • byte-enable (partial-word) writes commit only enabled bytes
//   • reset retention: SRAM contents survive a reset pulse
//   • zero-init tail
//   • transport_dbg back-door read + write
//   • dbg_load_bytes / dbg_read64
//   • SECDED ECC: correctable scrub (read succeeds) and uncorrectable read
//     (TLM_GENERIC_ERROR_RESPONSE), and write clears an injected error
//   • negative paths: out-of-window, misaligned, bad width, streaming mismatch
//   • CCI introspection / mutation / immutability
//   • annotated delay is non-zero
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
#include <vector>

#include "scratchpad_ram.h"

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
// Tiny TLM driver -- mimics what the SMC CPU bridge would issue.
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

    // Byte-enabled write: be_mask[i] != 0 means "write byte i".
    void write_be(uint64_t addr, const uint8_t* data, const uint8_t* be,
                  uint32_t len) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(const_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(const_cast<uint8_t*>(be));
        gp.set_byte_enable_length(len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write_be(0x" << std::hex << addr << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    // Returns the raw response status without failing the test on error.
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
};

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::scratchpad_ram dut;
    driver              drv;
    sc_core::sc_signal<bool> rst_n;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("scratchpad_ram")
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

    void run() {
        std::cout << "==== SMC Scratchpad RAM TB (CCI-compliant) ====\n";
        std::cout << "  size_bytes  = 0x" << std::hex << dut.size_bytes()
                  << std::dec << "\n";

        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        pulse_reset();

        // ------------------------------------------------------------------
        // 1. Preload contract: the first four 64-bit words match the fixture.
        // ------------------------------------------------------------------
        {
            const std::string fixture = std::string(SCRATCHPAD_FIXTURES_DIR) +
                                        "/scratchpad_sanity.rv64.hex";
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
        // 2. Read-write semantics: a write is observable on the next read
        //    (the defining difference from the read-only Boot ROM).
        // ------------------------------------------------------------------
        {
            const uint64_t off = 0x40;
            drv.write<uint64_t>(off, 0xDEAD'BEEF'CAFE'BABEULL);
            EXPECT_EQ(uint64_t(0xDEAD'BEEF'CAFE'BABEULL), drv.read<uint64_t>(off));
            EXPECT_EQ(uint64_t(0xDEAD'BEEF'CAFE'BABEULL), dut.dbg_read64(off));

            // 32 / 16 / 8-bit writes commit and read back.
            drv.write<uint32_t>(off + 8, 0x12345678u);
            EXPECT_EQ(uint32_t(0x12345678u), drv.read<uint32_t>(off + 8));
            drv.write<uint16_t>(off + 12, 0xABCDu);
            EXPECT_EQ(uint16_t(0xABCDu), drv.read<uint16_t>(off + 12));
            drv.write<uint8_t>(off + 14, 0xEFu);
            EXPECT_EQ(uint8_t(0xEFu), drv.read<uint8_t>(off + 14));
            std::cout << "  [PASS] read-after-write (1/2/4/8 B) returns written value\n";
        }

        // ------------------------------------------------------------------
        // 3. Sub-word reads are coherent with the 64-bit word.
        // ------------------------------------------------------------------
        {
            const uint64_t off = 0x80;
            drv.write<uint64_t>(off, 0x1122'3344'5566'7788ULL);
            const uint64_t w = drv.read<uint64_t>(off);
            EXPECT_EQ(static_cast<uint32_t>(w),        drv.read<uint32_t>(off));
            EXPECT_EQ(static_cast<uint32_t>(w >> 32),  drv.read<uint32_t>(off + 4));
            EXPECT_EQ(static_cast<uint16_t>(w),        drv.read<uint16_t>(off));
            EXPECT_EQ(static_cast<uint8_t>(w),         drv.read<uint8_t>(off));
            std::cout << "  [PASS] sub-word reads coherent with 64-bit word\n";
        }

        // ------------------------------------------------------------------
        // 4. Byte-enable (partial-word) write: only enabled bytes change.
        // ------------------------------------------------------------------
        {
            const uint64_t off = 0xC0;
            drv.write<uint64_t>(off, 0x0000'0000'0000'0000ULL);
            // Write 0xFF to bytes 0,2,4,6 (even bytes) only.
            const uint8_t data[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            const uint8_t be[8]   = {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00};
            drv.write_be(off, data, be, 8);
            EXPECT_EQ(uint64_t(0x00FF'00FF'00FF'00FFULL), drv.read<uint64_t>(off));
            std::cout << "  [PASS] byte-enabled write commits only enabled bytes\n";
        }

        // ------------------------------------------------------------------
        // 5. Reset retention: contents survive a reset pulse (SRAM, not ROM).
        // ------------------------------------------------------------------
        {
            const uint64_t off = 0x100;
            drv.write<uint64_t>(off, 0xA5A5'5A5A'A5A5'5A5AULL);
            pulse_reset();
            EXPECT_EQ(uint64_t(0xA5A5'5A5A'A5A5'5A5AULL), drv.read<uint64_t>(off));
            std::cout << "  [PASS] contents survive reset (SRAM retains state)\n";
        }

        // ------------------------------------------------------------------
        // 6. Zero-init tail (no preload word covers the high half).
        // ------------------------------------------------------------------
        {
            EXPECT_EQ(uint64_t(0), drv.read<uint64_t>(dut.size_bytes() - 8));
            std::cout << "  [PASS] unpopulated region reads as zero\n";
        }

        // ------------------------------------------------------------------
        // 7. transport_dbg — back-door read + write, no annotated delay.
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint64_t data = 0xFACE'FEED'1234'5678ULL;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(0x200);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
            gp.set_data_length(8);
            gp.set_streaming_width(8);
            gp.set_byte_enable_ptr(nullptr);
            EXPECT_EQ(8u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(uint64_t(0xFACE'FEED'1234'5678ULL), dut.dbg_read64(0x200));

            uint64_t rb = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&rb));
            EXPECT_EQ(8u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(uint64_t(0xFACE'FEED'1234'5678ULL), rb);
            std::cout << "  [PASS] transport_dbg back-door read + write\n";
        }

        // ------------------------------------------------------------------
        // 8. dbg_load_bytes round-trips through b_transport.
        // ------------------------------------------------------------------
        {
            const uint8_t pattern[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
            const uint64_t off = dut.size_bytes() - 8;
            EXPECT_EQ(8u, dut.dbg_load_bytes(off, pattern, 8));
            EXPECT_EQ(uint64_t(0x8877'6655'4433'2211ULL), drv.read<uint64_t>(off));
            EXPECT_EQ(0u, dut.dbg_load_bytes(dut.size_bytes(), pattern, 8));
            std::cout << "  [PASS] dbg_load_bytes round-trips through b_transport\n";
        }

        // ------------------------------------------------------------------
        // 9. SECDED ECC: correctable error is scrubbed (read succeeds),
        //    uncorrectable error fails the read, and a write clears it.
        // ------------------------------------------------------------------
        {
            EXPECT_TRUE(dut.ecc_enabled());
            const uint64_t off = 0x300;
            drv.write<uint64_t>(off, 0xCAFE'F00D'1357'9BDFULL);

            // Correctable: read succeeds, value intact, flag scrubbed.
            dut.dbg_inject_ecc_error(off, /*correctable=*/true);
            EXPECT_EQ(uint64_t(0xCAFE'F00D'1357'9BDFULL), drv.read<uint64_t>(off));

            // Uncorrectable: read returns TLM_GENERIC_ERROR_RESPONSE.
            uint64_t scratch = 0;
            dut.dbg_inject_ecc_error(off, /*correctable=*/false);
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, off, 8, &scratch));

            // Writing the word re-encodes ECC and clears the error.
            drv.write<uint64_t>(off, 0x0F0F'0F0F'0F0F'0F0FULL);
            EXPECT_EQ(uint64_t(0x0F0F'0F0F'0F0F'0F0FULL), drv.read<uint64_t>(off));
            std::cout << "  [PASS] SECDED: correctable scrub, uncorrectable read fails, write clears\n";
        }

        // ------------------------------------------------------------------
        // 10. Negative paths — out-of-window, misaligned, width, streaming.
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            uint64_t scratch64 = 0;
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, dut.size_bytes(), 4, &scratch));
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_WRITE_COMMAND,
                                   dut.size_bytes() + 0x100, 4, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 1, 4, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 4, 8, &scratch64));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0, 3, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0, 4, &scratch, /*sw=*/1));
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0, 4, &scratch));
            std::cout << "  [PASS] negative tests: window, alignment, width, sw, command\n";
        }

        // ------------------------------------------------------------------
        // 11. CCI introspection — discover params, mutate access_delay_ns,
        //     observe immutability of size_bytes.
        // ------------------------------------------------------------------
        {
            auto broker  = cci::cci_get_broker();
            auto h_size  = broker.get_param_handle("tb.scratchpad_ram.size_bytes");
            auto h_delay = broker.get_param_handle("tb.scratchpad_ram.access_delay_ns");
            auto h_ecc   = broker.get_param_handle("tb.scratchpad_ram.ecc_enabled");
            EXPECT_TRUE(h_size.is_valid());
            EXPECT_TRUE(h_delay.is_valid());
            EXPECT_TRUE(h_ecc.is_valid());
            EXPECT_EQ(dut.size_bytes(),
                      uint64_t(h_size.get_cci_value().get_uint64()));

            const double old_delay = h_delay.get_cci_value().get_double();
            h_delay.set_cci_value(cci::cci_value(old_delay * 2.0));
            (void)drv.read<uint32_t>(0);
            EXPECT_EQ(old_delay * 2.0, h_delay.get_cci_value().get_double());

            const uint64_t old_size = h_size.get_cci_value().get_uint64();
            try {
                h_size.set_cci_value(cci::cci_value(uint64_t(old_size * 2)));
            } catch (...) { /* CCI may throw on immutable write; acceptable */ }
            EXPECT_EQ(old_size, uint64_t(h_size.get_cci_value().get_uint64()));
            std::cout << "  [PASS] CCI: discovery, mutation, immutability\n";
        }

        // ------------------------------------------------------------------
        // 12. dump_state smoke test.
        // ------------------------------------------------------------------
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            const std::string s = oss.str();
            EXPECT_TRUE(s.find("scratchpad_ram state") != std::string::npos);
            EXPECT_TRUE(s.find("ecc_enabled")          != std::string::npos);
            EXPECT_TRUE(s.find("contents[0..")         != std::string::npos);
            std::cout << "  [PASS] dump_state contains expected fields\n";
        }

        // ------------------------------------------------------------------
        // 13. Annotated delay is non-zero (pipelined read latency).
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t scratch = 0;
            sc_time t = SC_ZERO_TIME;
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
            std::cout << "  [PASS] annotated delay is non-zero\n";
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
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                            sc_core::SC_DISPLAY);

    static cci_utils::consuming_broker broker("GlobalBroker");

    cci::cci_register_broker(broker);
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    global_broker.set_preset_cci_value(
        "tb.scratchpad_ram.size_bytes",
        cci::cci_value(uint64_t(0x10000)));
    global_broker.set_preset_cci_value(
        "tb.scratchpad_ram.init_file",
        cci::cci_value(std::string(SCRATCHPAD_FIXTURES_DIR
                                   "/scratchpad_sanity.rv64.hex")));
    global_broker.set_preset_cci_value(
        "tb.scratchpad_ram.init_file_format",
        cci::cci_value(std::string("hex")));
    global_broker.set_preset_cci_value(
        "tb.scratchpad_ram.access_delay_ns",
        cci::cci_value(2.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
