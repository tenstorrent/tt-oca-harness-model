// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// smc/platform/test/platform_tb.cpp
//
// Self-checking test bench for smc_platform (Phase 1):
//   (a) elaboration with CCI presets and every socket/port bound,
//   (b) MMIO round-trip through the fabric to a modeled peripheral (PLIC
//       priority + UART SCR), confirming the platform wiring routes
//       transactions end-to-end.
//
// Phase 2 (firmware-on-cluster) is added under SMC_PLATFORM_WITH_CLUSTER in a
// later step.
// ===========================================================================

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "smc_platform.h"

namespace {

// ---------------------------------------------------------------------------
// Minimal self-checking framework (mirrors the per-IP TBs).
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Initiator driver: issues single 4-byte b_transport accesses.
// ---------------------------------------------------------------------------
class driver : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<driver, 64> sock{"sock"};

    explicit driver(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

    // Raw access: returns the response status so callers can assert either
    // TLM_OK_RESPONSE (permitted path) or an error (filtered / decode miss).
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

    void write32(uint64_t addr, uint32_t data)
    {
        CHECK(access(tlm::TLM_WRITE_COMMAND, addr, data) == tlm::TLM_OK_RESPONSE,
              "write32 OK response");
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t data = 0xDEAD;
        CHECK(access(tlm::TLM_READ_COMMAND, addr, data) == tlm::TLM_OK_RESPONSE,
              "read32 OK response");
        return data;
    }
};

// Absolute local addresses (see integration guide).  Both are inside the
// fabric's 16 MB local alias aperture ([0xC000_0000, 0xC100_0000)) and so are
// reachable from the internal `mmio_in` master.  (PLIC/CLINT lie above the
// aperture and are only reachable from sys/sep inbound ports, which are
// inbound-filter-gated; they are exercised in Phase 2 via firmware.)
constexpr uint64_t A_SCRATCH0      = 0xC006'0000ULL; // scratchpad RAM word 0
constexpr uint64_t A_UART0_SCR      = 0xC000'A01CULL;  // UART[0] SCR

// ---------------------------------------------------------------------------
// CCI presets — must be set before constructing the platform.
// ---------------------------------------------------------------------------
void apply_presets(const std::string& top_name)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    cci::cci_originator o("platform_cfg");
    auto broker = cci::cci_get_global_broker(o);

    auto set = [&](const std::string& leaf, cci::cci_value v) {
        broker.set_preset_cci_value((top_name + "." + leaf), std::move(v));
    };

    set("bootrom.size_bytes",       cci::cci_value(uint64_t(0x20000)));
    set("scratchpad_ram.size_bytes", cci::cci_value(uint64_t(0x100000)));
    set("plic.num_sources",         cci::cci_value(336u));
    set("plic.num_contexts",        cci::cci_value(8u));
    set("clint.num_harts",          cci::cci_value(4u));
    set("clint.tick_period_ns",     cci::cci_value(0.0)); // freeze MTIME for determinism
    set("reset_unit.num_subsystems", cci::cci_value(32u));

    // Cluster sizing (used only when SMC_PLATFORM_WITH_CLUSTER).
    set("cluster.num_harts",    cci::cci_value(4u));
    set("cluster.reset_pc",     cci::cci_value(uint64_t(0xC004'0000ULL)));
    set("cluster.fast_mem_lo",  cci::cci_value(uint64_t(0)));
    set("cluster.fast_mem_hi",  cci::cci_value(uint64_t(0)));
    // The MMIO aperture must cover the bootrom (0xC004'0000) so cluster fetches
    // route via the mmio socket to the fabric; otherwise they fall through to
    // the data socket (a stub in this TB) and read back as illegal insns.
    set("cluster.mmio_lo",      cci::cci_value(uint64_t(0xC000'0000ULL)));
    set("cluster.mmio_hi",      cci::cci_value(uint64_t(0xC100'0000ULL)));
    set("cluster.source_id",    cci::cci_value(uint16_t(0x10)));

#ifdef SMC_PLATFORM_WITH_CLUSTER
    // When the CPU cluster is instantiated, its harts fetch from bootrom at
    // reset_pc.  Preload a tiny park firmware (a self-looping `jal x0,0`) so
    // the harts spin harmlessly instead of executing zero-filled bootrom and
    // trapping on illegal instructions.  The real firmware run is exercised
    // by platform_fw_tb.
    set("bootrom.init_file",        cci::cci_value(std::string(SMC_PLATFORM_PARK_HEX)));
    set("bootrom.init_file_format", cci::cci_value(std::string("hex")));
#endif
}

// ---------------------------------------------------------------------------
// Standalone addr_router unit test: exercise the decode-miss path that the
// fully-populated platform map never hits (every front_port window is sub-routed
// and periph has a catch-all).  The modules are created in sc_main before
// sc_start; the accesses run after the kernel has started.
// ---------------------------------------------------------------------------
struct addr_router_ut {
    smc::addr_router<64, 64> router{"router_ut", 1};
    smc::stub_target<64>     stub{"stub_ut", false};
    driver                   drv{"drv_ut"};
    sc_core::sc_signal<bool> irq_sink{"irq_sink"};

    addr_router_ut() {
        router.add_route(0, 0x0000, 0x1000, "hit");
        router.out[0].bind(stub.reg_socket);
        drv.sock.bind(router.tgt);
        stub.irq_o.bind(irq_sink);
    }

    void run() {
        uint32_t v = 0;
        CHECK(drv.access(tlm::TLM_WRITE_COMMAND, 0x0000, v) == tlm::TLM_OK_RESPONSE,
              "addr_router hit -> OK");
        CHECK(drv.access(tlm::TLM_READ_COMMAND,  0x1000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "addr_router miss -> ADDRESS_ERROR");
    }
};

} // namespace

int sc_main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    const std::string top_name = "top";
    apply_presets(top_name);

    smc::smc_platform dut{top_name.c_str()};

    // The platform's external inbound target sockets require bound initiators
    // (their internal BW ports).  These idle drivers never issue traffic to
    // sys/sep; drv_jtag doubles as the active round-trip master below (the
    // jtag_axi_in path bypasses the inbound filter and reaches local targets,
    // so it stands in for the cluster-driven mmio_in path when no firmware
    // run is being exercised).
    driver drv_sys{"drv_sys"}, drv_jtag{"drv_jtag"}, drv_sep{"drv_sep"};
    drv_sys .sock.bind(dut.sys_axi_in);
    drv_jtag.sock.bind(dut.jtag_axi_in);
    drv_sep .sock.bind(dut.sep_axi_in);

    // Standalone addr_router (created before sc_start; run after).  Exercises
    // the decode-miss path the fully-populated platform map never hits.
    addr_router_ut router_ut;

    // (a) Elaboration — a tiny sc_start exercises all bound sockets/processes.
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_US));
    std::cout << "elaboration + sc_start OK\n";

    // Standalone addr_router accesses (kernel now running).
    router_ut.run();

    // (b) MMIO round-trip through jtag_axi_in -> fabric -> front_port_router ->
    //     width adapter -> scratchpad RAM (exercises the 64->32 width adapter).
    const uint32_t scr_w = 0x1234'ABCDu;
    drv_jtag.write32(A_SCRATCH0, scr_w);
    const uint32_t scr_r = drv_jtag.read32(A_SCRATCH0);
    CHECK_EQ(scr_r, scr_w, "scratchpad RAM word0 readback");

    // (b2) MMIO round-trip -> periph_router -> UART[0] SCR.
    const uint32_t uscr_w = 0xAB;
    drv_jtag.write32(A_UART0_SCR, uscr_w);
    const uint32_t uscr_r = drv_jtag.read32(A_UART0_SCR);
    CHECK_EQ(uscr_r & 0xFFu, uscr_w, "UART[0] SCR readback");

    // (c) External inbound paths (forward callbacks into the fabric).
    //  - jtag_axi_in / sep_axi_in bypass the inbound filter and route local
    //    targets directly, so a round-trip to a local peripheral succeeds.
    //  - sys_axi_in is gated by the inbound filter (block_by_default=1 with no
    //    entries configured), so a local access is denied.
    {
        uint32_t v = 0;
        CHECK(drv_jtag.access(tlm::TLM_WRITE_COMMAND, A_SCRATCH0, v)
                  == tlm::TLM_OK_RESPONSE,
              "jtag_axi_in -> scratchpad write OK");
        v = 0x55AA'1234u;
        CHECK(drv_jtag.access(tlm::TLM_WRITE_COMMAND, A_SCRATCH0, v)
                  == tlm::TLM_OK_RESPONSE,
              "jtag_axi_in -> scratchpad write OK (2)");
        v = 0;
        CHECK(drv_jtag.access(tlm::TLM_READ_COMMAND, A_SCRATCH0, v)
                  == tlm::TLM_OK_RESPONSE,
              "jtag_axi_in -> scratchpad read OK");
        CHECK_EQ(v, 0x55AA'1234u, "jtag_axi_in scratchpad readback");

        v = 0xCD;
        CHECK(drv_sep.access(tlm::TLM_WRITE_COMMAND, A_UART0_SCR, v)
                  == tlm::TLM_OK_RESPONSE,
              "sep_axi_in -> UART SCR write OK");
        v = 0;
        CHECK(drv_sep.access(tlm::TLM_READ_COMMAND, A_UART0_SCR, v)
                  == tlm::TLM_OK_RESPONSE,
              "sep_axi_in -> UART SCR read OK");
        CHECK_EQ(v & 0xFFu, 0xCDu, "sep_axi_in UART SCR readback");

        v = 0;
        CHECK(drv_sys.access(tlm::TLM_WRITE_COMMAND, A_SCRATCH0, v)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "sys_axi_in inbound filter denies by default");
    }

    // (d) Stub targets (RAZ/WI) reached through the fabric + routers.
    //  - wdt_debug stub via front_port_router.out[0] (stub_target<64>).
    //  - periph_misc catch-all via periph_router.out[10] (stub_target<32>).
    {
        constexpr uint64_t A_WDT_DBG  = 0xC000'0000ULL; // front_port route 0
        constexpr uint64_t A_PERIPH_MISC = 0xC000'2200ULL; // periph catch-all
        drv_jtag.write32(A_WDT_DBG, 0xABCDEF12u);
        CHECK_EQ(drv_jtag.read32(A_WDT_DBG), 0u, "wdt_debug stub RAZ");
        drv_jtag.write32(A_PERIPH_MISC, 0x55AA55AAu);
        CHECK_EQ(drv_jtag.read32(A_PERIPH_MISC), 0u, "periph_misc stub RAZ");
    }

    // (e) addr_router decode miss is exercised by the standalone unit test
    //     test_addr_router_miss() below (the fully-populated platform map has
    //     no front_port gaps in the local alias aperture, so the miss path is
    //     unreachable through the fabric itself).

    // (f) Stub IRQ injection exercises stub_target::irq_drive_method (both the
    //     timed-hold and zero-duration branches).  The stubs are public platform
    //     members; pulse_irq() schedules the SC_METHOD that drives irq_o (bound
    //     to a dummy sink).
    dut.stub_wdt_debug.pulse_irq(sc_core::sc_time(1, sc_core::SC_NS));
    dut.stub_wdt_debug.pulse_irq(sc_core::sc_time(0, sc_core::SC_NS));
    dut.stub_data      .pulse_irq(sc_core::sc_time(1, sc_core::SC_NS));
    dut.stub_data      .pulse_irq(sc_core::sc_time(0, sc_core::SC_NS));
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));

    // (g) Peripheral IRQ -> interrupt_aggregator recompute assert path.
    //     Enabling UART0 ETBEI while THR is empty (THRE=1 at reset) asserts the
    //     THRE interrupt, which drives uart_irq[0] -> intagg.src[6] -> recompute
    //     sets plic_src[18].  We only need the path executed for coverage; the
    //     PLIC pending bit is read back as the assertion.
    constexpr uint64_t A_UART0_IER = 0xC000'A004ULL;
    drv_jtag.write32(A_UART0_IER, 0x02u); // ETBEI
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    // PLIC pending register (0xC400'1000) is outside the local alias aperture,
    // so confirm via the intagg's PLIC source output directly.
    CHECK_EQ(dut.intagg.plic_src[18].read(), true,
             "UART0 THRE interrupt propagated through interrupt_aggregator");

    // (h) multi_stub_target TLM fw interface defaults.  The platform is LT
    //     (b_transport only) so DMI / nb_transport / debug transport are never
    //     issued through the socket; call them directly on the public stub to
    //     exercise the contract stubs.  b_transport is also called directly so
    //     the stub body is covered in cluster-OFF builds (where cluster.data
    //     never drives stub_data).
    {
        tlm::tlm_generic_payload gp;
        unsigned char buf[4] = {0};
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(0);
        gp.set_data_ptr(buf);
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;
        dut.stub_data.b_transport(gp, t);   // RAZ/WI -> TLM_OK_RESPONSE
        CHECK_EQ(gp.get_response_status() == tlm::TLM_OK_RESPONSE, true,
                 "multi_stub b_transport -> OK");
        CHECK_EQ(dut.stub_data.transport_dbg(gp), 0u,
                 "multi_stub transport_dbg returns 0");
        tlm::tlm_dmi dmi;
        CHECK_EQ(dut.stub_data.get_direct_mem_ptr(gp, dmi), false,
                 "multi_stub get_direct_mem_ptr returns false");
        tlm::tlm_phase ph = tlm::BEGIN_REQ;
        CHECK_EQ(dut.stub_data.nb_transport_fw(gp, ph, t), tlm::TLM_ACCEPTED,
                 "multi_stub nb_transport_fw accepts");
    }

    if (g_fail == 0) {
        std::cout << "ALL TESTS PASSED (" << g_pass << " checks)\n";
        return 0;
    }
    std::cout << "TESTS FAILED: " << g_fail << " failure(s)\n";
    return 1;
}
