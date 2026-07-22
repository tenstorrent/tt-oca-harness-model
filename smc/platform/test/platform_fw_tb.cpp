// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// smc/platform/test/platform_fw_tb.cpp
//
// Phase-2 firmware-on-cluster test bench for smc_platform.  Built only when the
// CPU cluster (Whisper + Boost) is enabled at configure time.
//
// Loads a tiny bare-metal RV64 firmware into the boot ROM via the
// `bootrom.init_file` CCI preset, sets the cluster's reset PC to the boot ROM
// base, and runs the simulation.  The firmware writes 0x1234'ABCD to
// scratchpad RAM word 0 and emits "UI" on UART[0]; this bench asserts both.
// ===========================================================================

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "smc_platform.h"
#include "sim_log.h"

#ifndef SMC_PLATFORM_FIRMWARE_HEX
#error "SMC_PLATFORM_FIRMWARE_HEX must be defined to the firmware.hex path"
#endif

namespace {

unsigned g_fail = 0;
unsigned g_pass = 0;

#define CHECK(cond, msg)                                          \
    do {                                                          \
        if (cond) { ++g_pass; }                                   \
        else {                                                    \
            ++g_fail;                                            \
            std::cout << "FAIL: " << msg << " (line "              \
                      << __LINE__ << ")\n";                      \
        }                                                        \
    } while (false)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

class driver : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<driver, 64> sock{"sock"};

    explicit driver(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

    tlm::tlm_response_status access(tlm::tlm_command cmd, uint64_t addr,
                                    uint32_t& data)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_byte_enable_length(0);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t data = 0xDEAD;
        CHECK(access(tlm::TLM_READ_COMMAND, addr, data) == tlm::TLM_OK_RESPONSE,
              "read32 OK response");
        return data;
    }
};

constexpr uint64_t A_SCRATCH0 = 0xC006'0000ULL;

void apply_presets(const std::string& top_name)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    cci::cci_originator o("platform_cfg");
    auto broker = cci::cci_get_global_broker(o);

    auto set = [&](const std::string& leaf, cci::cci_value v) {
        broker.set_preset_cci_value((top_name + "." + leaf), std::move(v));
    };

    set("bootrom.size_bytes",       cci::cci_value(uint64_t(0x20000)));
    set("bootrom.init_file",        cci::cci_value(std::string(SMC_PLATFORM_FIRMWARE_HEX)));
    set("bootrom.init_file_format", cci::cci_value(std::string("hex")));
    set("scratchpad_ram.size_bytes", cci::cci_value(uint64_t(0x100000)));
    set("plic.num_sources",         cci::cci_value(336u));
    set("plic.num_contexts",        cci::cci_value(8u));
    set("clint.num_harts",          cci::cci_value(4u));
    set("clint.tick_period_ns",     cci::cci_value(0.0));
    set("reset_unit.num_subsystems", cci::cci_value(32u));

    // Cluster: fetch from bootrom, no fast-mem, full local aperture on mmio.
    set("cluster.num_harts",    cci::cci_value(4u));
    set("cluster.reset_pc",     cci::cci_value(uint64_t(0xC004'0000ULL)));
    set("cluster.fast_mem_lo",  cci::cci_value(uint64_t(0)));
    set("cluster.fast_mem_hi",  cci::cci_value(uint64_t(0)));
    set("cluster.mmio_lo",      cci::cci_value(uint64_t(0xC000'0000ULL)));
    set("cluster.mmio_hi",      cci::cci_value(uint64_t(0xC100'0000ULL)));
    set("cluster.source_id",    cci::cci_value(uint16_t(0x10)));
}

} // namespace

int sc_main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    simlog::set_level(simlog::level::trace);

    const std::string top_name = "top";
    apply_presets(top_name);

    smc::smc_platform dut{top_name.c_str()};

    // Verification driver on the unfiltered jtag_axi_in external port; idle
    // drivers on sys/sep satisfy their BW ports (sys is filter-gated anyway).
    driver drv_jtag{"drv_jtag"}, drv_sys{"drv_sys"}, drv_sep{"drv_sep"};
    drv_jtag.sock.bind(dut.jtag_axi_in);
    drv_sys .sock.bind(dut.sys_axi_in);
    drv_sep .sock.bind(dut.sep_axi_in);

    // Run the CPU long enough to execute the firmware (~10 insns) and reach
    // the busy loop.  1 ms is far more than the LT quantum needs here.
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));

    // (a) Scratchpad word 0 written by firmware.
    const uint32_t scr = drv_jtag.read32(A_SCRATCH0);
    CHECK_EQ(scr, 0x1234'ABCDu, "firmware scratchpad write readback");

    // (b) UART[0] TX history starts with "UI".
    unsigned ui_count = 0;
    uint8_t  ch = 0;
    const uint8_t expect[2] = {'U', 'I'};
    while (dut.uart[0].dbg_tx_pop(ch)) {
        if (ui_count < 2) {
            CHECK_EQ(ch, expect[ui_count], "UART TX byte matches firmware");
        }
        ++ui_count;
        if (ui_count > 64) break;  // safety: firmware busy-loops, TX stops at 2
    }
    CHECK(ui_count >= 2, "UART emitted at least 2 bytes (UI)");

    if (g_fail == 0) {
        std::cout << "ALL TESTS PASSED (" << g_pass << " checks)\n";
        return 0;
    }
    std::cout << "TESTS FAILED: " << g_fail << " failure(s)\n";
    return 1;
}
