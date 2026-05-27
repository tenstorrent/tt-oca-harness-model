// SPDX-License-Identifier: Apache-2.0
//
// bootrom_bin_tb.cpp -- separate test binary that exercises the **binary**
// preload path (init_file_format = "auto" with a `.img` filename → "bin").
// The primary test bench (bootrom_tb.cpp) covers only the hex path; this
// binary fills in the binary-preload code paths so total coverage stays
// high.
//
// The fixture used here is the SEP production boot stub
// (`hw/smc/data/scripts/bootrom.rv64.img`) copied verbatim into
// `test/fixtures/bootrom.rv64.img` by the build.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint64_t read64(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read64 rsp=" << gp.get_response_string() << "\n";
            ++g_failures;
        }
        return data;
    }
};

struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);
    smc::bootrom dut;
    driver       drv;
    sc_core::sc_signal<bool> rst_n;

    explicit tb(sc_module_name n)
        : sc_module(n), dut("bootrom"), drv("drv"), rst_n("rst_n")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        SC_THREAD(run);
    }

    void run() {
        std::cout << "==== SEP Boot ROM TB — binary preload ====\n";
        rst_n.write(true);
        sc_core::wait(1, SC_NS);

        // ------------------------------------------------------------------
        // 1. Binary preload contract: the `.img` file contains raw
        //    little-endian 8-byte words.  Read the file ourselves and
        //    confirm the first few words match what the ROM exposes.
        // ------------------------------------------------------------------
        const std::string fixture =
            std::string(BOOTROM_FIXTURES_DIR) + "/bootrom.rv64.img";
        std::ifstream f(fixture, std::ios::binary);
        EXPECT_TRUE(f.is_open());

        std::vector<uint64_t> golden;
        while (golden.size() < 4) {
            uint64_t w = 0;
            if (!f.read(reinterpret_cast<char*>(&w), 8)) break;
            golden.push_back(w);
        }
        EXPECT_TRUE(!golden.empty());

        for (size_t i = 0; i < golden.size(); ++i) {
            EXPECT_EQ(golden[i], drv.read64(i * 8));
            EXPECT_EQ(golden[i], dut.dbg_read64(i * 8));
        }
        std::cout << "  [PASS] binary preload — words match .img file ("
                  << golden.size() << " words verified)\n";

        // ------------------------------------------------------------------
        // 2. Format auto-detection: the .img extension must have triggered
        //    binary mode.  Confirm via dump_state.
        // ------------------------------------------------------------------
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            EXPECT_TRUE(oss.str().find("init_file_format = bin") != std::string::npos);
            std::cout << "  [PASS] init_file_format auto-resolved to 'bin' for .img\n";
        }

        // ------------------------------------------------------------------
        // 3. Tail of ROM is zero (the .img file is smaller than size_bytes).
        // ------------------------------------------------------------------
        EXPECT_EQ(uint64_t(0), drv.read64(dut.size_bytes() - 8));
        std::cout << "  [PASS] ROM tail past end-of-image is zero-padded\n";

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
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    // Auto format detection: pass an `.img` path with the default "auto"
    // format and let the model pick "bin" by extension.
    global_broker.set_preset_cci_value(
        "tb.bootrom.init_file",
        cci::cci_value(std::string(BOOTROM_FIXTURES_DIR "/bootrom.rv64.img")));
    // init_file_format intentionally NOT set → defaults to "auto".

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
