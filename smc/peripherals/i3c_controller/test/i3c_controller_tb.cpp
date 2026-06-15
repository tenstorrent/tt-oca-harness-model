// SPDX-License-Identifier: Apache-2.0
//
// i3c_controller_tb.cpp -- self-checking test bench for the OCA I3C Controller
// SystemC/TLM-2.0 LT model (CCI-compliant).
//
// Coverage:
//   • reset values (HCI_VERSION, HC_CONTROL, threshold ctrls, QUEUE_SIZE, …)
//   • RW / RO / W1C register semantics; MODE_SELECTOR read-only=1
//   • multi-instance address decode (instance = offset / INSTANCE_SPACING)
//   • DAT / DCT direct-access windows
//   • HCI command/response/TX/RX/IBI FIFOs via port registers
//   • transaction engine: private write, private read, CCC write
//   • error paths: address NACK, TX underflow, RX overflow
//   • IBI injection + IBI_PORT readout
//   • interrupt aggregation (PIO_INTR_SIGNAL_ENABLE → irq_o)
//   • RESET_CONTROL self-clearing queue resets; HC_CONTROL.ABORT
//   • transport_dbg back-door (DAT/DCT)
//   • CCI introspection / mutation / immutability
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "i3c_controller.h"

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
                      << "  expected=" << +_e << " actual=" << +_a             \
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

using cfg_t = smc::i3c_controller_cfg;

// ---------------------------------------------------------------------------
// Tiny TLM driver -- 32-bit AXI-Lite-style register access.
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }
};

// Per-instance register address helper.
constexpr uint64_t A(unsigned inst, uint64_t reg) {
    return uint64_t(inst) * cfg_t::INSTANCE_SPACING + reg;
}

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::i3c_controller dut;
    driver              drv;

    static constexpr unsigned N = 3; // instances under test

    sc_core::sc_vector<sc_core::sc_signal<bool>> irq{"irq", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> scl{"scl", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sda{"sda", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> scl_oe{"scl_oe", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sda_oe{"sda_oe", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> od_pp{"od_pp", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> rpa{"rpa", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> ria{"ria", N};

    // Storage captured by the instance-0 bus model.
    std::vector<uint8_t> last_write;
    uint8_t              last_addr   = 0;
    bool                 read_mode_ack = true;
    std::vector<uint8_t> read_payload;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("dut", [] { cfg_t c; c.num_instances = N; return c; }())
        , drv("drv")
    {
        drv.sock.bind(dut.reg_socket);
        for (unsigned i = 0; i < N; ++i) {
            dut.irq_o[i](irq[i]);
            dut.scl_o[i](scl[i]);
            dut.sda_o[i](sda[i]);
            dut.scl_oe_o[i](scl_oe[i]);
            dut.sda_oe_o[i](sda_oe[i]);
            dut.sel_od_pp_o[i](od_pp[i]);
            dut.recovery_payload_available_o[i](rpa[i]);
            dut.recovery_image_activated_o[i](ria[i]);
        }
        SC_THREAD(run);
    }

    void settle() { sc_core::wait(1, SC_NS); }
    void run_xfer() { sc_core::wait(200, SC_NS); }

    void push_command(unsigned inst, uint64_t desc) {
        drv.write(A(inst, cfg_t::COMMAND_PORT), uint32_t(desc & 0xFFFFFFFF));
        drv.write(A(inst, cfg_t::COMMAND_PORT), uint32_t(desc >> 32));
    }

    // Build a model command descriptor (see spec §Command descriptor).
    static uint64_t make_cmd(uint8_t tid, bool rnw, uint8_t devidx,
                             bool cp, uint8_t ccc, uint16_t len) {
        return (uint64_t(tid & 0xF) << 3) |
               (uint64_t(rnw ? 1 : 0) << 7) |
               (uint64_t(devidx & 0x7F) << 8) |
               (uint64_t(cp ? 1 : 0) << 15) |
               (uint64_t(ccc) << 32) |
               (uint64_t(len) << 48);
    }

    void run();
};

void tb::run()
{
    // ------------------------------------------------------------------
    // Bus model for instance 0: a single target at dynamic address 0x42.
    // ------------------------------------------------------------------
    dut.set_bus_model(0, [this](smc::i3c_xfer& x) {
        last_addr = x.dynamic_addr;
        if (x.dynamic_addr != 0x42) { x.ack = false; x.error = smc::i3c_err::AddressNack; return; }
        x.ack = true;
        x.error = smc::i3c_err::Success;
        if (x.kind == smc::i3c_xfer_kind::PrivateWrite ||
            x.kind == smc::i3c_xfer_kind::CccWrite) {
            last_write = x.write_data;
        } else {
            x.read_data = read_payload;
        }
    });

    settle();

    // ===== 1. Reset values =====
    EXPECT_EQ(0x00000120u, drv.read(A(0, cfg_t::HCI_VERSION)));
    EXPECT_EQ(0x00000040u, drv.read(A(0, cfg_t::HC_CONTROL)));      // MODE_SELECTOR=1
    EXPECT_EQ(0x01010101u, drv.read(A(0, cfg_t::QUEUE_THLD_CTRL)));
    EXPECT_EQ(0x01010101u, drv.read(A(0, cfg_t::DATA_BUFFER_THLD_CTRL)));
    EXPECT_EQ(0x00000003u, drv.read(A(0, cfg_t::PIO_CONTROL)));
    EXPECT_EQ(0x00000080u, drv.read(A(0, cfg_t::PIO_SECTION_OFFSET)));
    EXPECT_EQ(0x00000012u, drv.read(A(0, cfg_t::STBY_CR_EXTCAP_HEADER)));
    // DAT_SECTION_OFFSET: offset 0x300, 32 entries, 2 dwords/entry.
    EXPECT_EQ(uint32_t(0x300u | (32u << 12) | (2u << 19)),
              drv.read(A(0, cfg_t::DAT_SECTION_OFFSET)));

    // ===== 2. RW + RO + W1C semantics =====
    drv.write(A(0, cfg_t::CONTROLLER_DEVICE_ADDR), 0x8042'0000u);
    EXPECT_EQ(0x8042'0000u, drv.read(A(0, cfg_t::CONTROLLER_DEVICE_ADDR)));
    drv.write(A(0, cfg_t::HCI_VERSION), 0xDEADBEEF);          // RO: ignored
    EXPECT_EQ(0x00000120u, drv.read(A(0, cfg_t::HCI_VERSION)));
    // INTR_STATUS W1C via INTR_FORCE.
    drv.write(A(0, cfg_t::INTR_FORCE), (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
    EXPECT_TRUE((drv.read(A(0, cfg_t::INTR_STATUS)) &
                 (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT)) != 0);
    drv.write(A(0, cfg_t::INTR_STATUS), (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
    EXPECT_TRUE((drv.read(A(0, cfg_t::INTR_STATUS)) &
                 (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT)) == 0);

    // ===== 3. DAT / DCT windows =====
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16)); // entry0 dynamic addr 0x42
    drv.write(A(0, cfg_t::DAT_BASE + 4), 0xCAFEF00Du);
    EXPECT_EQ((0x42u << 16), drv.read(A(0, cfg_t::DAT_BASE + 0)));
    EXPECT_EQ(0xCAFEF00Du, drv.read(A(0, cfg_t::DAT_BASE + 4)));
    drv.write(A(0, cfg_t::DCT_BASE + 0), 0x12345678u);
    EXPECT_EQ(0x12345678u, drv.read(A(0, cfg_t::DCT_BASE + 0)));

    // ===== 4. Enable bus =====
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));
    EXPECT_TRUE((drv.read(A(0, cfg_t::HC_CONTROL)) &
                 (1u << smc::hc_control::BUS_ENABLE)) != 0);
    // PRESENT_STATE.AC_CURRENT_OWN now set.
    EXPECT_TRUE((drv.read(A(0, cfg_t::PRESENT_STATE)) & (1u << 2)) != 0);

    // ===== 5. Private write transaction =====
    drv.write(A(0, cfg_t::XFER_DATA_PORT), 0xDEADBEEFu); // TX data
    push_command(0, make_cmd(/*tid*/5, /*rnw*/false, /*dev*/0, false, 0, /*len*/4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(5u, resp & 0xF);                 // TID
        EXPECT_EQ(4u, (resp >> 16) & 0xFFF);       // data length
        EXPECT_EQ(0u, (resp >> 28) & 0xF);         // error = SUCCESS
        EXPECT_EQ(0x42u, last_addr);
        EXPECT_EQ(4u, (unsigned)last_write.size());
        EXPECT_EQ(0xEFu, last_write[0]);
        EXPECT_EQ(0xBEu, last_write[1]);
        EXPECT_EQ(0xADu, last_write[2]);
        EXPECT_EQ(0xDEu, last_write[3]);
    }

    // ===== 6. Private read transaction =====
    read_payload = {0x11, 0x22, 0x33, 0x44};
    push_command(0, make_cmd(/*tid*/6, /*rnw*/true, /*dev*/0, false, 0, /*len*/4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(6u, resp & 0xF);
        EXPECT_EQ(4u, (resp >> 16) & 0xFFF);
        EXPECT_EQ(0u, (resp >> 28) & 0xF);
        EXPECT_EQ(0x44332211u, drv.read(A(0, cfg_t::XFER_DATA_PORT)));
    }

    // ===== 7. CCC write transaction =====
    drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x0000ABCDu);
    push_command(0, make_cmd(/*tid*/7, /*rnw*/false, /*dev*/0, /*cp*/true, /*ccc*/0x80, /*len*/2));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(7u, resp & 0xF);
        EXPECT_EQ(0u, (resp >> 28) & 0xF);
        EXPECT_EQ(2u, (unsigned)last_write.size());
        EXPECT_EQ(0xCDu, last_write[0]);
        EXPECT_EQ(0xABu, last_write[1]);
    }

    // ===== 8. Address NACK (unknown device) =====
    push_command(0, make_cmd(/*tid*/8, /*rnw*/true, /*dev*/1, false, 0, /*len*/4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(8u, resp & 0xF);
        EXPECT_EQ((unsigned)smc::i3c_err::AddressNack, (resp >> 28) & 0xF);
    }

    // ===== 9. TX underflow =====
    drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x000000AAu); // only 1 dword for len 8
    push_command(0, make_cmd(/*tid*/9, /*rnw*/false, /*dev*/0, false, 0, /*len*/8));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(9u, resp & 0xF);
        EXPECT_EQ((unsigned)smc::i3c_err::OverflowUnder, (resp >> 28) & 0xF);
        // TRANSFER_ERR_STAT latched.
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT)); // W1C
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) == 0);
    }
    // Drain the stray TX dword for a clean state.
    drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::TX_FIFO_RST));

    // ===== 10. IBI injection =====
    EXPECT_TRUE(dut.inject_ibi(0, 0x42, {0xAA, 0xBB}));
    settle();
    {
        const uint32_t status = drv.read(A(0, cfg_t::IBI_PORT));
        EXPECT_EQ(uint32_t((0x42u << 1) | (2u << 8)), status);
        EXPECT_EQ(0x0000BBAAu, drv.read(A(0, cfg_t::IBI_PORT)));
    }

    // ===== 11. Interrupt aggregation (RESP_READY → irq) =====
    drv.write(A(0, cfg_t::PIO_INTR_SIGNAL_ENABLE),
              (1u << smc::pio_intr::RESP_READY_STAT));
    read_payload = {0x55, 0x66, 0x77, 0x88};
    push_command(0, make_cmd(/*tid*/3, /*rnw*/true, /*dev*/0, false, 0, /*len*/4));
    run_xfer();
    settle();
    EXPECT_TRUE(irq[0].read());                 // response pending → irq high
    drv.read(A(0, cfg_t::RESPONSE_PORT));       // drain response
    drv.read(A(0, cfg_t::XFER_DATA_PORT));      // drain rx
    settle();
    EXPECT_TRUE(!irq[0].read());                // cleared
    drv.write(A(0, cfg_t::PIO_INTR_SIGNAL_ENABLE), 0);

    // ===== 12. RESET_CONTROL self-clearing + queue reset =====
    // Disable bus, queue a command, CMD_QUEUE_RST clears it.
    drv.write(A(0, cfg_t::HC_CONTROL), 0); // bus disabled
    push_command(0, make_cmd(1, false, 0, false, 0, 4));
    drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::CMD_QUEUE_RST));
    EXPECT_EQ(0u, drv.read(A(0, cfg_t::RESET_CONTROL))); // self-clearing
    // Re-enable; nothing should process (queue empty) → no response.
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));
    run_xfer();
    EXPECT_EQ(0u, drv.read(A(0, cfg_t::RESPONSE_PORT))); // empty → 0

    // ===== 13. Multi-instance isolation =====
    drv.write(A(1, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE) |
                                       (1u << smc::hc_control::I2C_DEV_PRES));
    EXPECT_TRUE((drv.read(A(1, cfg_t::HC_CONTROL)) &
                 (1u << smc::hc_control::I2C_DEV_PRES)) != 0);
    // Instance 2 untouched → reset value.
    EXPECT_EQ(0x00000040u, drv.read(A(2, cfg_t::HC_CONTROL)));
    // Instance 0 DAT not visible from instance 1.
    EXPECT_EQ(0u, drv.read(A(1, cfg_t::DAT_BASE + 0)));

    // ===== 14. transport_dbg back-door (DAT) =====
    {
        uint32_t v = 0xA5A5A5A5u;
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(A(2, cfg_t::DAT_BASE + 8));
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&v));
        gp.set_data_length(4);
        EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
        EXPECT_EQ(0xA5A5A5A5u, drv.read(A(2, cfg_t::DAT_BASE + 8)));
        EXPECT_EQ(0xA5A5A5A5u, dut.dbg_read(A(2, cfg_t::DAT_BASE + 8)));
    }

    // ===== 15. QUEUE_SIZE reflects configured depths =====
    {
        const uint32_t qs = drv.read(A(0, cfg_t::QUEUE_SIZE));
        EXPECT_EQ(8u, qs & 0xFF);          // CR queue depth
        EXPECT_EQ(8u, (qs >> 8) & 0xFF);   // IBI status depth
    }

    // ===== 16. CCI introspection =====
    {
        cci::cci_broker_handle broker = cci::cci_get_broker();
        auto h = broker.get_param_handle(std::string(dut.name()) + ".access_delay_ns");
        EXPECT_TRUE(h.is_valid());
        if (h.is_valid()) {
            h.set_cci_value(cci::cci_value(5.0));
            EXPECT_EQ(5.0, h.get_cci_value().get_double());
        }
        auto hi = broker.get_param_handle(std::string(dut.name()) + ".num_instances");
        EXPECT_TRUE(hi.is_valid());
        if (hi.is_valid())
            EXPECT_EQ((unsigned)tb::N, hi.get_cci_value().get_uint());
    }

    if (g_failures == 0) std::cout << "ALL TESTS PASSED\n";
    else                 std::cout << "FAILURE(S): " << g_failures << " check(s) failed\n";

    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char*[])
{
    cci::cci_register_broker(new cci_utils::consuming_broker("global_broker"));
    tb t("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
