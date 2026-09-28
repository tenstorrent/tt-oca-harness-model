// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// uart_tb.cpp -- self-checking test bench for the SMC UART 16550 (CCI-compliant).
//
// CCI integration highlights
// --------------------------------------------------------------------------
// * sc_main registers a global CCI broker before any module is constructed.
// * Preset values are injected for tb.uart.rx_fifo_depth (32 -> 8) and
//   tb.uart.access_delay_ns (2 ns -> 5 ns) to demonstrate pre-construction
//   override.  tx_fifo_depth is left at its default (32).
// * The LT path is exercised through a tlm_quantumkeeper in the driver.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include "uart.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;
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

// Register offsets (DLAB=0 view).
constexpr uint64_t RBR = 0x00, THR = 0x00, DLL = 0x00;
constexpr uint64_t IER = 0x04, DLM = 0x04;
constexpr uint64_t IIR = 0x08, FCR = 0x08;
constexpr uint64_t LCR = 0x0C, MCR = 0x10, LSR = 0x14, MSR = 0x18;
constexpr uint64_t SCR = 0x1C, ECR = 0x20, ITR = 0x24;

// LSR bits.
constexpr uint32_t LSR_DR = 0x01, LSR_OE = 0x02, LSR_PE = 0x04, LSR_FE = 0x08,
                   LSR_BI = 0x10, LSR_THRE = 0x20, LSR_TEMT = 0x40, LSR_ERRF = 0x80;

/// Changes the report actions for one message ID and restores them on scope
/// exit.  Scoping by ID (not by severity) keeps every unrelated diagnostic of
/// the same severity live.
struct scoped_report_actions_id {
    const char*         id;
    sc_core::sc_actions saved;
    scoped_report_actions_id(const char* msg_id, sc_core::sc_actions a)
        : id(msg_id), saved(sc_core::sc_report_handler::set_actions(msg_id, a)) {}
    ~scoped_report_actions_id() {
        sc_core::sc_report_handler::set_actions(id, saved);
    }
};

/// Changes the report actions for one severity and restores them on scope
/// exit, so an expected diagnostic cannot silence an unrelated one later.
struct scoped_report_actions {
    sc_core::sc_severity sev;
    sc_core::sc_actions  saved;
    scoped_report_actions(sc_core::sc_severity s, sc_core::sc_actions a)
        : sev(s), saved(sc_core::sc_report_handler::set_actions(s, a)) {}
    ~scoped_report_actions() {
        sc_core::sc_report_handler::set_actions(sev, saved);
    }
};

/// Result of a fully general payload sent by driver::raw().
struct raw_result {
    tlm::tlm_response_status status;
    sc_time                  delay_delta;
    bool                     dmi_allowed;
};

// Tiny TLM driver exercising the LT path through a quantum keeper.
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    tlm_utils::tlm_quantumkeeper qk;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEF;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    /// Fully general payload: explicit streaming width, byte enables,
    /// incoming delay and AXI sideband.  Bypasses the quantum keeper so the
    /// annotated delay can be measured exactly.
    raw_result raw(tlm::tlm_command cmd, uint64_t addr, void* data,
                   unsigned len, int streaming_width = -1,
                   uint8_t* be = nullptr, unsigned be_len = 0,
                   sc_time delay_in = SC_ZERO_TIME,
                   smc::smc_axi_extension* ext = nullptr) {
        tlm::tlm_generic_payload gp;
        sc_time t = delay_in;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (ext != nullptr) gp.set_extension(ext);
        sock->b_transport(gp, t);
        if (ext != nullptr) gp.clear_extension<smc::smc_axi_extension>();
        return {gp.get_response_status(), t - delay_in, gp.is_dmi_allowed()};
    }

    /// Raw back-door access over transport_dbg.  Returns bytes transferred.
    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, void* data, unsigned len,
                 int streaming_width = -1, uint8_t* be = nullptr,
                 unsigned be_len = 0) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return sock->transport_dbg(gp);
    }

    bool dmi(uint64_t addr, tlm::tlm_dmi& dmi_data) {
        tlm::tlm_generic_payload gp;
        uint32_t scratch = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        return sock->get_direct_mem_ptr(gp, dmi_data);
    }

    void write32(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << addr << ", 0x" << value
                      << ") rsp=" << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    // Raw transfer: returns the response status without failing the test.
    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t len, uint32_t* data) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    unsigned dbg_read(uint64_t addr, uint32_t& out) {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&out));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }

    unsigned dbg_write(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::uart      dut;
    smc::uart_wrap wrap;
    driver         drv;
    driver         wrap_drv;

    sc_core::sc_signal<bool> rst_n;
    sc_core::sc_signal<bool> tx_sig, rx_sig;
    sc_core::sc_signal<bool> cts_n, dsr_n, ri_n, dcd_n;
    sc_core::sc_signal<bool> rts_n, dtr_n, out1_n, out2_n;
    sc_core::sc_signal<bool> rxrdy, txrdy, err, irq;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("uart")
        , wrap("uart_wrap")
        , drv("drv")
        , wrap_drv("wrap_drv")
        , rst_n("rst_n")
        , tx_sig("tx_sig"), rx_sig("rx_sig")
        , cts_n("cts_n"), dsr_n("dsr_n"), ri_n("ri_n"), dcd_n("dcd_n")
        , rts_n("rts_n"), dtr_n("dtr_n"), out1_n("out1_n"), out2_n("out2_n")
        , rxrdy("rxrdy"), txrdy("txrdy"), err("err"), irq("irq")
    {
        drv.sock.bind(dut.reg_socket);
        wrap_drv.sock.bind(wrap.reg_socket);
        dut.rst_n_i(rst_n);
        wrap.rst_n_i(rst_n);
        dut.tx_o(tx_sig);   dut.rx_i(rx_sig);
        dut.cts_ni(cts_n);  dut.dsr_ni(dsr_n);
        dut.ri_ni(ri_n);    dut.dcd_ni(dcd_n);
        dut.rts_no(rts_n);  dut.dtr_no(dtr_n);
        dut.out1_no(out1_n);dut.out2_no(out2_n);
        dut.rxrdy_o(rxrdy); dut.txrdy_o(txrdy);
        dut.err_o(err);     dut.irq_o(irq);

        SC_THREAD(run);
    }

    static void settle() {
        for (int i = 0; i < 3; ++i) sc_core::wait(SC_ZERO_TIME);
    }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
        settle();
    }

    /// Architectural interrupt-id oracle: IIR[3:1], or 0xFF when IIR reports
    /// no interrupt pending (INT_PEND, bit 0, is active low).  Note that
    /// reading IIR is itself a 16550 side effect -- it clears THRE -- which is
    /// exactly the stepwise-clearing behaviour the priority case exercises.
    uint8_t iir_id() {
        const uint32_t iir = drv.read32(IIR);
        if ((iir & 0x1u) != 0) return 0xFF;      // no interrupt pending
        return static_cast<uint8_t>((iir >> 1) & 0x7u);
    }

    // Program divisor = 1 and 8N1 so TX/RX are enabled.
    void setup_basic() {
        drv.write32(LCR, 0x80);  // DLAB
        drv.write32(DLL, 0x01);
        drv.write32(DLM, 0x00);
        drv.write32(LCR, 0x03);  // 8N1
        settle();
    }

    void run();
};

void tb::run() {
    std::cout << "==== SMC UART 16550 TB (CCI-compliant) ====\n";

    // Modem inputs idle (active-low deasserted = high).
    cts_n.write(true); dsr_n.write(true); ri_n.write(true); dcd_n.write(true);
    rx_sig.write(true);
    rst_n.write(true);
    sc_core::wait(1, SC_NS);

    // ----------------------------------------------------------------------
    // 1. Reset clears state.
    // ----------------------------------------------------------------------
    pulse_reset();
    EXPECT_EQ(0x60u, drv.read32(LSR));           // THRE | TEMT
    EXPECT_EQ(0x00u, drv.read32(IER));
    EXPECT_EQ(0x01u, drv.read32(IIR) & 0x01u);   // INTERRUPT_PENDING = 1
    EXPECT_EQ(0x00u, drv.read32(LCR));
    EXPECT_EQ(false, irq.read());
    std::cout << "  [PASS] reset clears state\n";

    // ----------------------------------------------------------------------
    // 2. Scratch register R/W.
    // ----------------------------------------------------------------------
    drv.write32(SCR, 0xA5);
    EXPECT_EQ(0xA5u, drv.read32(SCR));
    std::cout << "  [PASS] scratch register R/W\n";

    // ----------------------------------------------------------------------
    // 3. DLAB muxing: 0x00/0x04 access DLL/DLM when DLAB=1.
    // ----------------------------------------------------------------------
    drv.write32(LCR, 0x80);            // DLAB=1
    drv.write32(DLL, 0x1B);
    drv.write32(DLM, 0x02);
    EXPECT_EQ(0x1Bu, drv.read32(DLL));
    EXPECT_EQ(0x02u, drv.read32(DLM));
    drv.write32(LCR, 0x03);            // DLAB=0 -> 0x00/0x04 are RBR/THR/IER
    drv.write32(IER, 0x00);
    EXPECT_EQ(0x00u, drv.read32(IER));
    EXPECT_EQ(0x03u, drv.read32(LCR));
    std::cout << "  [PASS] DLAB muxing\n";

    // ----------------------------------------------------------------------
    // 4. Non-FIFO transmit: THR write -> dbg_tx_pop.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(THR, 'H');
    drv.write32(THR, 'i');
    settle();
    uint8_t c = 0;
    EXPECT_TRUE(dut.dbg_tx_pop(c)); EXPECT_EQ(uint8_t('H'), c);
    EXPECT_TRUE(dut.dbg_tx_pop(c)); EXPECT_EQ(uint8_t('i'), c);
    EXPECT_TRUE(!dut.dbg_tx_pop(c));
    EXPECT_TRUE((drv.read32(LSR) & LSR_THRE) != 0);
    std::cout << "  [PASS] non-FIFO transmit\n";

    // ----------------------------------------------------------------------
    // 5. THRE interrupt (ETBEI): asserts, cleared by reading IIR.
    // ----------------------------------------------------------------------
    drv.write32(IER, 0x02);            // ETBEI
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x02u, drv.read32(IIR) & 0x0Fu); // id 0x1 -> byte 0x02
    settle();
    EXPECT_EQ(false, irq.read());      // IIR read cleared THRE int
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] THRE interrupt + IIR clear\n";

    // ----------------------------------------------------------------------
    // 6. Non-FIFO receive: inject -> DR, RBR read returns char.
    // ----------------------------------------------------------------------
    dut.inject_rx_char('Z');
    settle();
    EXPECT_TRUE((drv.read32(LSR) & LSR_DR) != 0);
    EXPECT_EQ(uint32_t('Z'), drv.read32(RBR));
    settle();
    EXPECT_EQ(0u, drv.read32(LSR) & LSR_DR);   // DR clears after read
    EXPECT_EQ(0u, drv.read32(RBR));            // empty -> 0
    std::cout << "  [PASS] non-FIFO receive\n";

    // ----------------------------------------------------------------------
    // 7. Data-ready interrupt (ERBFI), non-FIFO.
    // ----------------------------------------------------------------------
    drv.write32(IER, 0x01);            // ERBFI
    dut.inject_rx_char('q');
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x04u, drv.read32(IIR) & 0x0Fu); // id 0x2 -> byte 0x04
    (void)drv.read32(RBR);             // drain
    settle();
    EXPECT_EQ(false, irq.read());
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] data-ready interrupt\n";

    // ----------------------------------------------------------------------
    // 8. FIFO mode + RX trigger level.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);            // FIFO enable
    settle();
    EXPECT_EQ(0xC0u, drv.read32(IIR) & 0xC0u); // FIFOS_ENABLED = 11
    drv.write32(ECR, 0x00);
    drv.write32(FCR, 0x01 | (0x1 << 6)); // trigger level index 1 -> 4 chars
    drv.write32(IER, 0x01);              // ERBFI
    dut.inject_rx_char('a');
    dut.inject_rx_char('b');
    dut.inject_rx_char('c');
    settle();
    EXPECT_EQ(false, irq.read());        // below trigger (3 < 4)
    dut.inject_rx_char('d');
    settle();
    EXPECT_EQ(true, irq.read());         // reached trigger
    EXPECT_EQ(4u, dut.dbg_rx_count());
    // Drain via RBR; FIFO order preserved.
    EXPECT_EQ(uint32_t('a'), drv.read32(RBR));
    EXPECT_EQ(uint32_t('b'), drv.read32(RBR));
    EXPECT_EQ(uint32_t('c'), drv.read32(RBR));
    EXPECT_EQ(uint32_t('d'), drv.read32(RBR));
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] FIFO mode + RX trigger\n";

    // ----------------------------------------------------------------------
    // 9. FIFO transmit of several bytes.
    // ----------------------------------------------------------------------
    const char* msg = "FIFO!";
    for (const char* p = msg; *p; ++p) drv.write32(THR, uint8_t(*p));
    settle();
    for (const char* p = msg; *p; ++p) {
        EXPECT_TRUE(dut.dbg_tx_pop(c));
        EXPECT_EQ(uint8_t(*p), c);
    }
    std::cout << "  [PASS] FIFO transmit\n";

    // ----------------------------------------------------------------------
    // 10. RX errors -> LSR + RLS interrupt + err_o.
    // ----------------------------------------------------------------------
    drv.write32(FCR, 0x01 | 0x02);     // FIFO enable + RX reset (clear)
    drv.write32(IER, 0x04);            // ELSI
    dut.inject_rx_char('e', /*parity*/true, /*framing*/true, /*break*/false);
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x06u, drv.read32(IIR) & 0x0Fu); // id 0x3 -> byte 0x06
    EXPECT_EQ(true, err.read());
    {
        uint32_t lsr = drv.read32(LSR);
        EXPECT_TRUE((lsr & LSR_PE) != 0);
        EXPECT_TRUE((lsr & LSR_FE) != 0);
    }
    settle();
    // After LSR read the sticky PE/FE clear -> RLS deasserts.
    EXPECT_EQ(0u, drv.read32(LSR) & (LSR_PE | LSR_FE));
    (void)drv.read32(RBR);
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] RX errors + RLS interrupt\n";

    // ----------------------------------------------------------------------
    // 11. FIFO-error interrupt (EFEI).
    // ----------------------------------------------------------------------
    drv.write32(FCR, 0x01 | 0x02);     // clear RX FIFO
    drv.write32(IER, 0x10);            // EFEI
    dut.inject_rx_char('x', false, false, /*break*/true);
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x0Eu, drv.read32(IIR) & 0x0Fu); // id 0x7 -> byte 0x0E
    EXPECT_TRUE((drv.read32(LSR) & LSR_ERRF) != 0);
    (void)drv.read32(RBR);             // remove erroneous char
    settle();
    EXPECT_EQ(false, irq.read());
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] FIFO-error interrupt\n";

    // ----------------------------------------------------------------------
    // 12. Overrun (FIFO full -> drop, OE set, cleared on LSR read).
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();                     // non-FIFO (depth 1 RBR)
    dut.inject_rx_char('1');
    dut.inject_rx_char('2');           // overrun: overwrites, sets OE
    settle();
    {
        uint32_t lsr = drv.read32(LSR);
        EXPECT_TRUE((lsr & LSR_OE) != 0);
    }
    settle();
    EXPECT_EQ(0u, drv.read32(LSR) & LSR_OE);   // OE cleared on read
    EXPECT_EQ(uint32_t('2'), drv.read32(RBR)); // held byte overwritten
    std::cout << "  [PASS] overrun handling\n";

    // ----------------------------------------------------------------------
    // 13. Modem status: drive inputs -> MSR levels + deltas + interrupt.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(IER, 0x08);            // EDSSI
    cts_n.write(false);                // assert CTS (active-low)
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x00u, drv.read32(IIR) & 0x0Fu); // id 0x0 -> byte 0x00
    {
        uint32_t msr = drv.read32(MSR);
        EXPECT_TRUE((msr & 0x10) != 0);  // CTS level
        EXPECT_TRUE((msr & 0x01) != 0);  // DCTS delta
    }
    settle();
    EXPECT_EQ(false, irq.read());        // MSR read cleared deltas
    // RI trailing edge: assert then deassert -> TERI.
    ri_n.write(false); settle();
    ri_n.write(true);  settle();
    EXPECT_TRUE((drv.read32(MSR) & 0x04) != 0); // TERI
    drv.write32(IER, 0x00);
    cts_n.write(true);
    std::cout << "  [PASS] modem status + deltas\n";

    // ----------------------------------------------------------------------
    // 14. Modem control outputs (active-low).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(MCR, 0x03);            // DTR + RTS asserted
    settle();
    EXPECT_EQ(false, rts_n.read());    // active-low asserted
    EXPECT_EQ(false, dtr_n.read());
    drv.write32(MCR, 0x00);
    settle();
    EXPECT_EQ(true, rts_n.read());
    EXPECT_EQ(true, dtr_n.read());
    std::cout << "  [PASS] modem control outputs\n";

    // ----------------------------------------------------------------------
    // 15. System loopback: TX feeds RX; MSR reflects MCR.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(MCR, 0x10 | 0x03);     // LOOP + RTS + DTR
    settle();
    drv.write32(THR, 'L');
    settle();
    EXPECT_TRUE((drv.read32(LSR) & LSR_DR) != 0);
    EXPECT_EQ(uint32_t('L'), drv.read32(RBR));
    {
        uint32_t msr = drv.read32(MSR);
        EXPECT_TRUE((msr & 0x10) != 0);  // CTS reflects RTS
        EXPECT_TRUE((msr & 0x20) != 0);  // DSR reflects DTR
    }
    drv.write32(MCR, 0x00);
    std::cout << "  [PASS] system loopback\n";

    // ----------------------------------------------------------------------
    // 16. Line loopback: rx_i feeds tx_o; MSR inputs = 0.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    cts_n.write(false);                // would assert CTS in normal mode
    drv.write32(MCR, 0x20);            // LINE_LOOPBACK
    rx_sig.write(false);
    settle();
    EXPECT_EQ(false, tx_sig.read());   // tx_o follows rx_i
    EXPECT_EQ(0u, drv.read32(MSR) & 0xF0u); // modem inputs forced 0
    rx_sig.write(true); settle();
    EXPECT_EQ(true, tx_sig.read());
    drv.write32(MCR, 0x00);
    cts_n.write(true);
    std::cout << "  [PASS] line loopback\n";

    // ----------------------------------------------------------------------
    // 17. Break generation forces tx_o low.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(LCR, 0x40);            // SET_BREAK
    settle();
    EXPECT_EQ(false, tx_sig.read());
    drv.write32(LCR, 0x00);
    settle();
    EXPECT_EQ(true, tx_sig.read());
    std::cout << "  [PASS] break generation\n";

    // ----------------------------------------------------------------------
    // 18. DMA Mode 0 handshake.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);            // FIFO enable, DMA mode 0
    settle();
    EXPECT_EQ(true,  txrdy.read());    // TX not full
    EXPECT_EQ(false, rxrdy.read());    // RX empty
    dut.inject_rx_char('m');
    settle();
    EXPECT_EQ(true, rxrdy.read());     // RX non-empty
    (void)drv.read32(RBR);
    settle();
    EXPECT_EQ(false, rxrdy.read());
    std::cout << "  [PASS] DMA mode 0\n";

    // ----------------------------------------------------------------------
    // 19. DMA Mode 1 RX FSM (watermark -> assert until empty).
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(ECR, 0x00);
    drv.write32(FCR, 0x01 | 0x08 | (0x1 << 6)); // FIFO + DMA mode 1 + trig 4
    settle();
    dut.inject_rx_char('1');
    dut.inject_rx_char('2');
    dut.inject_rx_char('3');
    settle();
    EXPECT_EQ(false, rxrdy.read());    // below watermark
    dut.inject_rx_char('4');
    settle();
    EXPECT_EQ(true, rxrdy.read());     // watermark reached
    (void)drv.read32(RBR); (void)drv.read32(RBR);
    settle();
    EXPECT_EQ(true, rxrdy.read());     // stays asserted until empty
    (void)drv.read32(RBR); (void)drv.read32(RBR);
    settle();
    EXPECT_EQ(false, rxrdy.read());    // drained -> deassert
    std::cout << "  [PASS] DMA mode 1 RX FSM\n";

    // ----------------------------------------------------------------------
    // 20. RX timeout interrupt.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01 | (0x1 << 6)); // FIFO, trigger 4 (so DR int won't fire on 1 char)
    drv.write32(IER, 0x01);            // ERBFI (also enables timeout)
    dut.inject_rx_char('t');
    settle();
    EXPECT_EQ(false, irq.read());      // below trigger, no timeout yet
    sc_core::wait(2, SC_US);           // exceed the 1 us timeout
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x0Cu, drv.read32(IIR) & 0x0Fu); // id 0x6 -> byte 0x0C
    (void)drv.read32(RBR);             // drain clears timeout
    settle();
    EXPECT_EQ(false, irq.read());
    drv.write32(IER, 0x00);
    std::cout << "  [PASS] RX timeout interrupt\n";

    // ----------------------------------------------------------------------
    // 21. ITR force-interrupt bits.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(ITR, 0x08);            // force modem-status int
    settle();
    EXPECT_EQ(true, irq.read());
    EXPECT_EQ(0x00u, drv.read32(IIR) & 0x0Fu);
    drv.write32(ITR, 0x10);            // force FIFO-error int (highest)
    settle();
    EXPECT_EQ(0x0Eu, drv.read32(IIR) & 0x0Fu);
    drv.write32(ITR, 0x00);
    settle();
    EXPECT_EQ(false, irq.read());
    std::cout << "  [PASS] ITR force bits\n";

    // ----------------------------------------------------------------------
    // 22. Divisor = 0 disables TX (byte queued, not transmitted).
    // ----------------------------------------------------------------------
    pulse_reset();                     // divisor 0 after reset
    drv.write32(THR, 'N');
    settle();
    EXPECT_EQ(1u, dut.dbg_tx_count()); // queued, not drained
    EXPECT_TRUE(!dut.dbg_tx_pop(c));   // nothing transmitted
    // Program divisor -> queued byte now transmits.
    drv.write32(LCR, 0x80);
    drv.write32(DLL, 0x01);
    drv.write32(LCR, 0x03);
    settle();
    EXPECT_EQ(0u, dut.dbg_tx_count());
    EXPECT_TRUE(dut.dbg_tx_pop(c)); EXPECT_EQ(uint8_t('N'), c);
    std::cout << "  [PASS] divisor=0 disables TX\n";

    // ----------------------------------------------------------------------
    // 23. Clearing BREAK resumes queued serial TX.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(MCR, 0x00);
    rx_sig.write(true);
    sc_core::wait(2, SC_US);             // drain leftover serial + quantum
    EXPECT_EQ(4u, drv.dbg_write(THR, static_cast<uint32_t>('B')));
    EXPECT_EQ(4u, drv.dbg_write(LCR, 0x43u)); // 8N1 + BREAK before TX thread runs
    settle();
    EXPECT_EQ(false, tx_sig.read());     // break drives the line low
    EXPECT_EQ(4u, drv.dbg_write(LCR, 0x03u)); // clearing BREAK should restart TX
    sc_core::wait(SC_ZERO_TIME);
    sc_core::wait(8, SC_NS);             // mid start bit
    EXPECT_EQ(false, tx_sig.read());
    sc_core::wait(12 * 16 - 8, SC_NS);   // through stop + idle
    settle();
    EXPECT_EQ(true, tx_sig.read());
    std::cout << "  [PASS] clearing BREAK resumes queued TX\n";

    // ----------------------------------------------------------------------
    // 24. Bus error responses.
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x00, 2, &d));   // len != 4
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x02, 4, &d));   // unaligned
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x200, 4, &d));  // out of window
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x28, 4, &d));   // decode miss
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_WRITE_COMMAND, 0x28, 4, &d));  // write decode miss
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0x00, 4, &d)); // bad command
    }
    std::cout << "  [PASS] bus error responses\n";

    // ----------------------------------------------------------------------
    // 25. transport_dbg + dbg_reg (side-effect-free).
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    dut.inject_rx_char('D');
    settle();
    {
        uint32_t v = 0;
        EXPECT_EQ(4u, drv.dbg_read(RBR, v)); // debug read does NOT pop
        EXPECT_EQ(uint32_t('D'), v);
        EXPECT_EQ(1u, dut.dbg_rx_count());   // still present
        EXPECT_EQ(uint32_t('D'), dut.dbg_reg(RBR));
        EXPECT_EQ(0x03u, dut.dbg_reg(LCR));
    }
    EXPECT_EQ(uint32_t('D'), drv.read32(RBR)); // real read pops
    std::cout << "  [PASS] transport_dbg + dbg_reg\n";

    // ----------------------------------------------------------------------
    // 26. Register-access + dbg_reg + transport_dbg-write coverage.
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    // Reads of the remaining registers (decode coverage).
    (void)drv.read32(MCR);
    (void)drv.read32(ECR);
    (void)drv.read32(ITR);
    // Writes to the read-only LSR/MSR are silently ignored (no bus error).
    drv.write32(LSR, 0xFF);
    drv.write32(MSR, 0xFF);
    EXPECT_EQ(0x60u, drv.read32(LSR));
    // FCR XMIT_FIFO_RESET clears a queued (un-transmitted) TX byte.
    pulse_reset();                         // divisor 0 -> TX disabled
    drv.write32(FCR, 0x01);                // FIFO enable
    drv.write32(THR, 'q');                 // queued, not drained
    EXPECT_EQ(1u, dut.dbg_tx_count());
    drv.write32(FCR, 0x01 | 0x04);         // XMIT_FIFO_RESET
    EXPECT_EQ(0u, dut.dbg_tx_count());
    // Side-effect-free peeks across every register (DLAB=0 then DLAB=1).
    (void)dut.dbg_reg(IER); (void)dut.dbg_reg(IIR); (void)dut.dbg_reg(MCR);
    (void)dut.dbg_reg(LSR); (void)dut.dbg_reg(MSR); (void)dut.dbg_reg(SCR);
    (void)dut.dbg_reg(ECR); (void)dut.dbg_reg(ITR); (void)dut.dbg_reg(0x28);
    drv.write32(LCR, 0x80);
    (void)dut.dbg_reg(DLL); (void)dut.dbg_reg(DLM);
    drv.write32(LCR, 0x00);
    // transport_dbg write path.
    EXPECT_EQ(4u, drv.dbg_write(SCR, 0x3C));
    EXPECT_EQ(0x3Cu, drv.read32(SCR));
    std::cout << "  [PASS] register-access + dbg coverage\n";

    // ----------------------------------------------------------------------
    // 27. DMA Mode 1 TX FSM (full -> deassert until empty).
    // ----------------------------------------------------------------------
    pulse_reset();                         // divisor 0 -> TX cannot drain
    drv.write32(ECR, 0x00);
    drv.write32(FCR, 0x01 | 0x08);         // FIFO + DMA mode 1
    settle();
    EXPECT_EQ(true, txrdy.read());         // empty -> ready
    for (int i = 0; i < 32; ++i) drv.write32(THR, 'a'); // fill TX FIFO (depth 32)
    settle();
    EXPECT_EQ(false, txrdy.read());        // full -> not ready
    // Program divisor: queued bytes drain, FIFO empties, TXRDY re-asserts.
    drv.write32(LCR, 0x80);
    drv.write32(DLL, 0x01);
    drv.write32(LCR, 0x03);
    settle();
    EXPECT_EQ(true, txrdy.read());
    std::cout << "  [PASS] DMA mode 1 TX FSM\n";

    // ----------------------------------------------------------------------
    // 28. CCI parameter introspection.
    // ----------------------------------------------------------------------
    {
        auto broker = cci::cci_get_broker();

        auto h_rx = broker.get_param_handle("tb.uart.rx_fifo_depth");
        EXPECT_TRUE(h_rx.is_valid());
        EXPECT_TRUE(h_rx.get_cci_value().to_json() == std::string("8")); // preset
        EXPECT_TRUE(h_rx.is_preset_value());

        auto h_tx = broker.get_param_handle("tb.uart.tx_fifo_depth");
        EXPECT_TRUE(h_tx.is_valid());
        EXPECT_TRUE(h_tx.get_cci_value().to_json() == std::string("32")); // default

        auto h_d = broker.get_param_handle("tb.uart.access_delay_ns");
        EXPECT_TRUE(h_d.is_valid());
        h_d.set_cci_value(cci::cci_value(7.0)); // mutate at run-time
        EXPECT_TRUE(h_d.get_cci_value().get_double() == 7.0);

        std::cout << "  CCI parameters:\n";
        for (auto& h : broker.get_param_handles()) {
            std::cout << "    " << std::left << std::setw(34) << h.name()
                      << " = " << std::setw(8) << h.get_cci_value().to_json()
                      << (h.is_preset_value() ? " [preset]" : " [default]") << "\n";
        }
        std::cout << "  [PASS] CCI introspection\n";
    }

    // ----------------------------------------------------------------------
    // uart_wrap: log-engine-ctrl @0x0 and log_engine @0x200
    // ----------------------------------------------------------------------
    {
        using C = smc::uart_wrap_cfg;
        wrap_drv.write32(C::OFF_WRAP_CTRL, 0x1);
        EXPECT_EQ(0x1u, wrap_drv.read32(C::OFF_WRAP_CTRL));
        wrap_drv.write32(C::OFF_LOG_CTRL, 0x1);
        wrap_drv.write32(C::OFF_LOG_REGION_SIZE, 0x12345);
        wrap_drv.write32(C::OFF_LOG_REGION_LO, 0xAABBCCDD);
        wrap_drv.write32(C::OFF_LOG_REGION_HI, 0xFF123456);
        wrap_drv.write32(C::OFF_LOG_WRITE_ADDR, 0x1000);
        wrap_drv.write32(C::OFF_LOG_ENTRY, 0x80);
        EXPECT_EQ(0x1u, wrap_drv.read32(C::OFF_LOG_CTRL));
        EXPECT_EQ(0x12345u, wrap_drv.read32(C::OFF_LOG_REGION_SIZE));
        EXPECT_EQ(0xAABBCCDDu, wrap_drv.read32(C::OFF_LOG_REGION_LO));
        EXPECT_EQ(0x00123456u, wrap_drv.read32(C::OFF_LOG_REGION_HI));
        EXPECT_EQ(0x80u, wrap_drv.read32(C::OFF_LOG_ENTRY));
        wrap_drv.write32(C::OFF_LOG_INTR_TEST, 0x11);
        EXPECT_EQ(0x11u, wrap_drv.read32(C::OFF_LOG_INTR_STATUS));
        wrap_drv.write32(C::OFF_LOG_INTR_STATUS, 0x01);
        EXPECT_EQ(0x10u, wrap_drv.read32(C::OFF_LOG_INTR_STATUS));
        wrap_drv.write32(C::OFF_LOG_CTRL, 0x0); // disable resets log engine
        EXPECT_EQ(0u, wrap_drv.read32(C::OFF_LOG_REGION_SIZE));
        EXPECT_EQ(0x1u, wrap_drv.read32(C::OFF_WRAP_CTRL)); // wrap ctrl survives
        uint32_t dbg = 0;
        EXPECT_EQ(4u, wrap_drv.dbg_read(C::OFF_WRAP_CTRL, dbg));
        EXPECT_EQ(0x1u, dbg);
        EXPECT_EQ(4u, wrap_drv.dbg_write(C::OFF_WRAP_CTRL, 0));
        EXPECT_EQ(0u, wrap_drv.read32(C::OFF_WRAP_CTRL));
        std::cout << "  [PASS] uart_wrap CSRs\n";
    }

    // ----------------------------------------------------------------------
    // Bit-serial TX on tx_o and RX from rx_i (divisor=1 → 16 ns/bit).
    // ----------------------------------------------------------------------
    pulse_reset();
    setup_basic();
    drv.write32(MCR, 0); // no loopback
    rx_sig.write(true);
    sc_core::wait(2, SC_US); // drain leftover serial + quantum
    // Driving a bit pattern onto an sc_signal<bool> transiently converts an X,
    // which SystemC reports under SC_ID_LOGIC_X_TO_BOOL_.  Scope that one
    // report ID to this block: suppressing by severity instead would silence
    // every unrelated diagnostic of the same severity for the whole run.
    const scoped_report_actions_id quiet_logic_x(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                                 sc_core::SC_DO_NOTHING);

    // dbg_write avoids the TLM quantum keeper advancing through the frame.
    EXPECT_EQ(4u, drv.dbg_write(THR, static_cast<uint32_t>('K')));
    sc_core::wait(SC_ZERO_TIME);
    sc_core::wait(8, SC_NS); // mid start bit
    EXPECT_EQ(false, tx_sig.read());
    sc_core::wait(12 * 16 - 8, SC_NS); // through stop + idle
    settle();
    EXPECT_EQ(true, tx_sig.read()); // idle high after stop
    {
        uint8_t ch = 0;
        EXPECT_TRUE(dut.dbg_tx_pop(ch));
        EXPECT_EQ(static_cast<uint8_t>('K'), ch);
    }
    // Drive 0xA5 LSB-first onto rx_i: start, 1,0,1,0,0,1,0,1, stop
    rx_sig.write(false);
    sc_core::wait(16, SC_NS);
    for (int i = 0; i < 8; ++i) {
        rx_sig.write(((0xA5 >> i) & 1) != 0);
        sc_core::wait(16, SC_NS);
    }
    rx_sig.write(true);
    sc_core::wait(16, SC_NS);
    settle();
    EXPECT_TRUE((drv.read32(LSR) & LSR_DR) != 0);
    EXPECT_EQ(0xA5u, drv.read32(RBR) & 0xFFu);
    std::cout << "  [PASS] bit-serial TX/RX timing\n";

    // dump_state for visual inspection / coverage of the dump path.
    dut.dump_state(std::cout);


    // ==================================================================
    // Architectural sections added for the internal-review remediation.
    // ==================================================================

    // ---- RX character timeout restarts on an RBR read (audit finding 2) --
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);                       // FIFO enable
    drv.write32(IER, 0x01);                       // ERBFI: data-ready + timeout
    settle();
    {
        // Two characters queued: the timeout was armed by the second arrival.
        dut.inject_rx_char('A');
        dut.inject_rx_char('B');
        settle();
        // Sit just short of one idle period, then read one character.  A
        // correct 16550 restarts the timeout from that read, so no timeout may
        // fire for another full period.
        sc_core::wait(900, SC_NS);
        EXPECT_EQ(uint32_t('A'), drv.read32(RBR) & 0xFFu);
        settle();
        sc_core::wait(700, SC_NS);               // 1.6 us since the arrival
        settle();
        EXPECT_TRUE(iir_id() != uint8_t(smc::uart_intr_id::RECEPTION_TIMEOUT));
        // ... and one full idle period after the read it does fire.
        sc_core::wait(500, SC_NS);
        settle();
        EXPECT_EQ(uint8_t(smc::uart_intr_id::RECEPTION_TIMEOUT), iir_id());
        // Draining the last character disarms the timeout entirely.
        EXPECT_EQ(uint32_t('B'), drv.read32(RBR) & 0xFFu);
        settle();
        sc_core::wait(2, SC_US);
        settle();
        EXPECT_TRUE(iir_id() != uint8_t(smc::uart_intr_id::RECEPTION_TIMEOUT));
    }
    std::cout << "  [PASS] RX timeout restarts on RBR read, disarms when empty\n";

    // ---- TLM payload matrix, both targets (audit findings 3 and 11) ------
    pulse_reset();
    {
        struct target { driver* d; uint64_t window; const char* name; };
        const target targets[] = {
            {&drv,      smc::uart_cfg::WINDOW_SIZE,      "uart"},
            {&wrap_drv, smc::uart_wrap_cfg::WINDOW_SIZE, "uart_wrap"},
        };
        for (const target& t : targets) {
            uint32_t scratch = 0;
            uint8_t  buf8[8] = {0};

            // A null data pointer used to be dereferenced straight into memcpy.
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_READ_COMMAND, 0, nullptr, 4).status);
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_WRITE_COMMAND, 0, nullptr, 4).status);
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_READ_COMMAND, 0, &scratch, 0, 0).status);
            // Width and alignment.
            for (unsigned len : {1u, 2u, 3u, 5u, 8u})
                EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                          t.d->raw(tlm::TLM_READ_COMMAND, 0, buf8, len).status);
            for (uint64_t off : {1u, 2u, 3u})
                EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                          t.d->raw(tlm::TLM_READ_COMMAND, off, &scratch, 4).status);
            // Window edge and 64-bit addresses whose addr+len would wrap.
            // The two targets deliberately differ on an unmapped offset that
            // is still inside the window: `uart` reports a decode miss, while
            // `uart_wrap` is RAZ/WI.
            EXPECT_EQ(t.d == &drv ? tlm::TLM_ADDRESS_ERROR_RESPONSE
                                  : tlm::TLM_OK_RESPONSE,
                      t.d->raw(tlm::TLM_READ_COMMAND, t.window - 4, &scratch, 4).status);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_READ_COMMAND, t.window, &scratch, 4).status);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, &scratch, 4).status);
            // Byte enables refused at every byte-enable length.
            {
                uint8_t be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
                for (unsigned be_len : {0u, 1u, 2u, 4u})
                    EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                              t.d->raw(tlm::TLM_WRITE_COMMAND, 0, &scratch, 4,
                                       -1, be, be_len).status);
            }
            // Streaming width: single beat, so sw >= len is required.
            for (unsigned sw = 0; sw <= 8; ++sw)
                EXPECT_EQ(sw < 4 ? tlm::TLM_BURST_ERROR_RESPONSE
                                 : tlm::TLM_OK_RESPONSE,
                          t.d->raw(tlm::TLM_READ_COMMAND, 0, &scratch, 4,
                                   static_cast<int>(sw)).status);
            // Unsupported command.
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      t.d->raw(tlm::TLM_IGNORE_COMMAND, 0, &scratch, 4).status);

            // DMI is denied and dmi_allowed cleared on hit and miss.
            tlm::tlm_dmi d;
            d.allow_read_write();
            EXPECT_TRUE(!t.d->dmi(0, d));
            EXPECT_TRUE(!d.is_read_allowed());
            EXPECT_TRUE(!d.is_write_allowed());
            EXPECT_TRUE(!t.d->raw(tlm::TLM_READ_COMMAND, 0, &scratch, 4).dmi_allowed);
            EXPECT_TRUE(!t.d->raw(tlm::TLM_READ_COMMAND, t.window, &scratch, 4)
                             .dmi_allowed);

            // The canonical sideband is inspected but never consumed.
            smc::smc_axi_extension ext;
            ext.source_id = smc::JTAG_ID;
            ext.axi_id    = 0x1234u;
            ext.axi_user  = 0x0Fu;
            ext.set_priv(false);
            ext.set_secure(true);
            const smc::smc_axi_extension golden = ext;
            t.d->raw(tlm::TLM_READ_COMMAND, 0, &scratch, 4, -1, nullptr, 0,
                     SC_ZERO_TIME, &ext);
            EXPECT_EQ(golden.source_id, ext.source_id);
            EXPECT_EQ(golden.axi_id, ext.axi_id);
            EXPECT_EQ(golden.prot, ext.prot);
            EXPECT_EQ(golden.axi_user, ext.axi_user);
        }
    }
    std::cout << "  [PASS] TLM matrix on both targets: null/BE/SW/wrap/DMI/sideband\n";

    // ---- Exact annotated delay, before and after CCI mutation (finding 9) -
    {
        auto broker = cci::cci_get_broker();
        auto h = broker.get_param_handle("tb.uart.access_delay_ns");
        EXPECT_TRUE(h.is_valid());
        uint32_t scratch = 0;
        for (double d_ns : {5.0, 11.0, 2.0}) {
            h.set_cci_value(cci::cci_value(d_ns));
            const auto r = drv.raw(tlm::TLM_READ_COMMAND, SCR, &scratch, 4, -1,
                                   nullptr, 0, sc_time(7, SC_NS));
            EXPECT_EQ(tlm::TLM_OK_RESPONSE, r.status);
            EXPECT_TRUE(r.delay_delta == sc_time(d_ns, SC_NS));
        }
        // An error path annotates nothing at all.
        const auto bad = drv.raw(tlm::TLM_READ_COMMAND,
                                 smc::uart_cfg::WINDOW_SIZE, &scratch, 4, -1,
                                 nullptr, 0, sc_time(7, SC_NS));
        EXPECT_TRUE(bad.delay_delta == SC_ZERO_TIME);
        h.set_cci_value(cci::cci_value(5.0));
    }
    std::cout << "  [PASS] exact annotated delay tracks live CCI mutation\n";

    // ---- Debug transport contract (audit finding 8) ----------------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);
    settle();
    {
        // A debug READ must be side-effect-free: it may not pop RBR.
        dut.inject_rx_char('Z');
        settle();
        uint32_t v = 0;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_READ_COMMAND, RBR, &v, 4));
        EXPECT_EQ(uint32_t('Z'), v & 0xFFu);
        EXPECT_TRUE((drv.read32(LSR) & LSR_DR) != 0);   // still pending
        EXPECT_EQ(uint32_t('Z'), drv.read32(RBR) & 0xFFu);  // the real pop
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_DR) == 0);

        // A debug WRITE deliberately uses the software path, so a debugger can
        // program the device.  SCR is the harmless witness.
        uint32_t w = 0x5Au;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND, SCR, &w, 4));
        EXPECT_EQ(0x5Au, drv.read32(SCR) & 0xFFu);

        // Malformed debug requests are refused on both targets.
        uint8_t be = 0xFF;
        for (driver* d : {&drv, &wrap_drv}) {
            EXPECT_EQ(0u, d->dbg(tlm::TLM_READ_COMMAND, 0, nullptr, 4));
            EXPECT_EQ(0u, d->dbg(tlm::TLM_READ_COMMAND, 0, &v, 2));
            EXPECT_EQ(0u, d->dbg(tlm::TLM_READ_COMMAND, 1, &v, 4));
            EXPECT_EQ(0u, d->dbg(tlm::TLM_READ_COMMAND, 0, &v, 4, -1, &be, 1));
            EXPECT_EQ(0u, d->dbg(tlm::TLM_READ_COMMAND, 0, &v, 4, 1));
            // TLM_IGNORE_COMMAND used to fall through the wrapper's `else`
            // and write a CSR.
            EXPECT_EQ(0u, d->dbg(tlm::TLM_IGNORE_COMMAND, 0, &v, 4));
        }
        // Prove the ignored debug command really did not write the wrapper.
        wrap_drv.write32(smc::uart_wrap_cfg::OFF_LOG_REGION_SIZE, 0x1234u);
        uint32_t poison = 0xFFFFFFFFu;
        EXPECT_EQ(0u, wrap_drv.dbg(tlm::TLM_IGNORE_COMMAND,
                                   smc::uart_wrap_cfg::OFF_LOG_REGION_SIZE,
                                   &poison, 4));
        EXPECT_EQ(0x1234u,
                  wrap_drv.read32(smc::uart_wrap_cfg::OFF_LOG_REGION_SIZE));
    }
    std::cout << "  [PASS] debug contract: read side-effect-free, write is the SW path\n";

    // ---- Interrupt priority with real sources (audit finding 5) ----------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);
    drv.write32(IER, 0x1F);            // every real source enabled, EFEI included
    settle();
    {
        using ID = smc::uart_intr_id;
        // THRE is already active out of reset (the transmitter is empty) and
        // outranks modem status, so clear it first: reading IIR is the 16550's
        // documented THRE acknowledge.
        EXPECT_EQ(uint8_t(ID::TRANSMITTER_HOLDING_REGISTER_EMPTY), iir_id());
        settle();

        // Raise the modem source, then stack higher-priority sources on top;
        // IIR must always report the highest active one.
        dcd_n.write(false); settle();
        EXPECT_EQ(uint8_t(ID::MODEM_STATUS), iir_id());

        dut.inject_rx_char('a');       // data ready outranks modem status
        settle();
        EXPECT_EQ(uint8_t(ID::RECEIVED_DATA_READY), iir_id());

        dut.inject_rx_char('b', false, true, false);  // framing error
        settle();
        // In FIFO mode a queued error raises the FIFO-error source, which
        // outranks both data-ready and modem status.
        EXPECT_EQ(uint8_t(ID::FIFO_ERROR), iir_id());
        EXPECT_TRUE(irq.read());

        // Clear from the top down and watch the id fall through the chain.
        drv.read32(RBR); drv.read32(RBR);   // drain both characters
        drv.read32(LSR);                    // clear sticky line status
        settle();
        EXPECT_TRUE(iir_id() != uint8_t(ID::FIFO_ERROR));
        // Modem status is the last source standing.
        EXPECT_EQ(uint8_t(ID::MODEM_STATUS), iir_id());
        drv.read32(MSR);                    // clear modem deltas
        settle();
        EXPECT_EQ(uint8_t(0xFF), iir_id());  // nothing pending
        EXPECT_TRUE(!irq.read());
        dcd_n.write(true); settle();
        drv.read32(MSR); settle();
    }
    std::cout << "  [PASS] interrupt priority chain with simultaneous real sources\n";

    // ---- RX FIFO capacity and drop policy (audit finding 6) --------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);
    settle();
    {
        constexpr unsigned kDepth = 8;   // rx_fifo_depth preset in sc_main
        for (unsigned i = 0; i < kDepth; ++i)
            dut.inject_rx_char(static_cast<uint8_t>('0' + i));
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_OE) == 0);
        // One extra character overruns: OE sets and the new byte is dropped,
        // leaving the original contents intact.
        dut.inject_rx_char('X');
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_OE) != 0);
        for (unsigned i = 0; i < kDepth; ++i)
            EXPECT_EQ(uint32_t('0' + i), drv.read32(RBR) & 0xFFu);
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_DR) == 0);

        // Non-FIFO mode holds exactly one byte and overwrites on overrun.
        drv.write32(FCR, 0x00);
        settle();
        dut.inject_rx_char('m');
        dut.inject_rx_char('n');
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_OE) != 0);
        EXPECT_EQ(uint32_t('n'), drv.read32(RBR) & 0xFFu);

        // Every FIFO trigger selector programs a level, and FIFO reset flushes.
        drv.write32(FCR, 0x01);
        for (uint32_t sel = 0; sel < 4; ++sel) {
            drv.write32(FCR, 0x01 | (sel << 6));
            settle();
            dut.inject_rx_char('t');
            settle();
            drv.write32(FCR, 0x01 | 0x02);     // RX FIFO reset
            settle();
            EXPECT_TRUE((drv.read32(LSR) & LSR_DR) == 0);
        }
    }
    std::cout << "  [PASS] RX FIFO capacity, drop policy, trigger selectors, reset\n";

    // ---- Persistent RX error semantics (audit finding 7) -----------------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);
    drv.write32(IER, 0x0F);
    settle();
    {
        // Error on the MIDDLE entry: the front is clean, so LSR shows no error
        // yet, but the FIFO-level error output is already asserted.
        dut.inject_rx_char('a');
        dut.inject_rx_char('b', true, false, false);   // parity error
        dut.inject_rx_char('c');
        settle();
        EXPECT_TRUE(err.read());
        EXPECT_TRUE((drv.read32(LSR) & LSR_PE) == 0);  // front is clean

        EXPECT_EQ(uint32_t('a'), drv.read32(RBR) & 0xFFu);
        settle();
        // Now the errored character is at the front: its status is revealed.
        EXPECT_TRUE((drv.read32(LSR) & LSR_PE) != 0);
        // Reading LSR clears the sticky bits, but the error output stays up
        // while the errored entry is still queued.
        EXPECT_TRUE((drv.read32(LSR) & LSR_PE) == 0);
        EXPECT_TRUE(err.read());
        EXPECT_EQ(uint32_t('b'), drv.read32(RBR) & 0xFFu);
        settle();
        // Popped: no errored entry remains, so the error output drops.
        EXPECT_TRUE(!err.read());
        EXPECT_EQ(uint32_t('c'), drv.read32(RBR) & 0xFFu);
        settle();

        // A break on the last entry behaves the same way.
        dut.inject_rx_char('d');
        dut.inject_rx_char('e', false, false, true);   // break
        settle();
        EXPECT_TRUE(err.read());
        drv.read32(RBR);
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_BI) != 0);
        drv.read32(RBR);
        settle();
        EXPECT_TRUE(!err.read());
    }
    std::cout << "  [PASS] RX error persistence across LSR and RBR sequences\n";

    // ---- uart_wrap CSR contract (UART-WRAP-001) --------------------------
    pulse_reset();
    {
        using wcfg = smc::uart_wrap_cfg;
        // Reset image.
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_WRAP_CTRL));
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_CTRL));
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_WRITE_ADDR));

        // Field masks: each register keeps only its documented bits.
        wrap_drv.write32(wcfg::OFF_WRAP_CTRL, 0xFFFFFFFFu);
        EXPECT_EQ(wcfg::WRAP_CTRL_MASK, wrap_drv.read32(wcfg::OFF_WRAP_CTRL));
        wrap_drv.write32(wcfg::OFF_LOG_REGION_SIZE, 0xFFFFFFFFu);
        EXPECT_EQ(wcfg::LOG_REGION_SIZE_MASK,
                  wrap_drv.read32(wcfg::OFF_LOG_REGION_SIZE));
        wrap_drv.write32(wcfg::OFF_LOG_REGION_HI, 0xFFFFFFFFu);
        EXPECT_EQ(wcfg::LOG_REGION_HI_MASK,
                  wrap_drv.read32(wcfg::OFF_LOG_REGION_HI));

        // All 16 log entries are independent storage, first and last included.
        for (unsigned i = 0; i < wcfg::NUM_LOG_ENTRIES; ++i)
            wrap_drv.write32(wcfg::OFF_LOG_ENTRY + 4 * i, 0xFFFF0000u | (i + 1));
        for (unsigned i = 0; i < wcfg::NUM_LOG_ENTRIES; ++i)
            EXPECT_EQ(wcfg::LOG_LEN_MASK & (0xFFFF0000u | (i + 1)),
                      wrap_drv.read32(wcfg::OFF_LOG_ENTRY + 4 * i));

        // W1C interrupt status, and the test register that sets it.
        wrap_drv.write32(wcfg::OFF_LOG_INTR_TEST, wcfg::LOG_INTR_MASK);
        EXPECT_EQ(wcfg::LOG_INTR_MASK, wrap_drv.read32(wcfg::OFF_LOG_INTR_STATUS));
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_INTR_TEST));  // WO reads 0
        wrap_drv.write32(wcfg::OFF_LOG_INTR_STATUS, 0x01u);
        EXPECT_EQ(wcfg::LOG_INTR_MASK & ~0x01u,
                  wrap_drv.read32(wcfg::OFF_LOG_INTR_STATUS));
        wrap_drv.write32(wcfg::OFF_LOG_INTR_STATUS, wcfg::LOG_INTR_MASK);
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_INTR_STATUS));

        // Holes inside the window are RAZ/WI and do not disturb a neighbour.
        for (uint64_t off : {uint64_t(0x04), uint64_t(0x100), uint64_t(0x1FC),
                             uint64_t(0x20), uint64_t(0x3FC)}) {
            EXPECT_EQ(0u, wrap_drv.read32(off));
            wrap_drv.write32(off, 0xFFFFFFFFu);
            EXPECT_EQ(0u, wrap_drv.read32(off));
        }
        EXPECT_EQ(wcfg::WRAP_CTRL_MASK, wrap_drv.read32(wcfg::OFF_WRAP_CTRL));

        // Disabling the log engine resets its CSRs but not WRAP_CTRL.
        wrap_drv.write32(wcfg::OFF_LOG_CTRL, 0x1u);
        wrap_drv.write32(wcfg::OFF_LOG_REGION_SIZE, 0x2222u);
        wrap_drv.write32(wcfg::OFF_LOG_CTRL, 0x0u);
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_REGION_SIZE));
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_LOG_ENTRY));
        EXPECT_EQ(wcfg::WRAP_CTRL_MASK, wrap_drv.read32(wcfg::OFF_WRAP_CTRL));

        // Architectural reset clears everything.
        wrap_drv.write32(wcfg::OFF_WRAP_CTRL, 0x1u);
        pulse_reset();
        EXPECT_EQ(0u, wrap_drv.read32(wcfg::OFF_WRAP_CTRL));
    }
    std::cout << "  [PASS] uart_wrap CSR contract: masks, 16 entries, holes, reset\n";

    // ---- THRE / TEMT transitions around divisor and FIFO reset -----------
    pulse_reset();
    setup_basic();
    drv.write32(FCR, 0x01);
    settle();
    {
        // Out of reset the transmitter is empty: both THRE and TEMT are set.
        EXPECT_TRUE((drv.read32(LSR) & LSR_THRE) != 0);
        EXPECT_TRUE((drv.read32(LSR) & LSR_TEMT) != 0);

        // With the divisor cleared the transmitter is disabled, so a queued
        // character cannot drain and the holding register stops reporting
        // empty.
        drv.write32(LCR, 0x80);
        drv.write32(DLL, 0x00);
        drv.write32(DLM, 0x00);
        drv.write32(LCR, 0x03);
        settle();
        drv.write32(THR, 'p');
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_THRE) == 0);
        EXPECT_TRUE((drv.read32(LSR) & LSR_TEMT) == 0);
        EXPECT_EQ(1u, dut.dbg_tx_count());

        // A TX FIFO reset discards it and restores the empty indication.
        drv.write32(FCR, 0x01 | 0x04);
        settle();
        EXPECT_EQ(0u, dut.dbg_tx_count());
        EXPECT_TRUE((drv.read32(LSR) & LSR_THRE) != 0);
        EXPECT_TRUE((drv.read32(LSR) & LSR_TEMT) != 0);

        // Re-enabling the divisor lets traffic flow again.
        setup_basic();
        drv.write32(THR, 'r');
        settle();
        EXPECT_TRUE((drv.read32(LSR) & LSR_THRE) != 0);
    }
    std::cout << "  [PASS] THRE/TEMT across divisor gating and TX FIFO reset\n";


#ifdef UART_UB_CANARY
    // Built only by `run_tests.sh --ubsan-canary`.  Proves the UBSan build
    // really does report undefined behaviour, so a clean --asan run means
    // something.  volatile keeps the shift out of the optimiser's hands.
    {
        volatile int shift = 33;
        volatile int value = 1;
        std::cout << "  [UB CANARY] " << (value << shift) << "\n";
    }
#endif

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char**)
{
    // SC_REPORT_FATAL must throw so the constructor guard rails can be probed
    // below without aborting the run.
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);

    // CCI: register global broker before any cci_param is constructed.
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);

    // Inject presets before tb / uart are constructed.
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.uart.rx_fifo_depth",
                                       cci::cci_value(8u));
    global_broker.set_preset_cci_value("tb.uart.access_delay_ns",
                                       cci::cci_value(5.0));

    // Constructor guard rails: FIFO depths outside 1..4096 are fatal.  Probed
    // before the real DUT is elaborated, as in the sister IPs' negative benches.
    {
        auto expect_fatal = [](auto&& body) {
            try { body(); } catch (...) { return true; }
            return false;
        };
        smc::uart_cfg bad;
        bad.rx_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&] { smc::uart u("bad_rx0", bad); }));
        bad = smc::uart_cfg{};
        bad.rx_fifo_depth = 4097;
        EXPECT_TRUE(expect_fatal([&] { smc::uart u("bad_rx4097", bad); }));
        bad = smc::uart_cfg{};
        bad.tx_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&] { smc::uart u("bad_tx0", bad); }));
        bad = smc::uart_cfg{};
        bad.tx_fifo_depth = 4097;
        EXPECT_TRUE(expect_fatal([&] { smc::uart u("bad_tx4097", bad); }));
        std::cout << "  [PASS] constructor guard rails (FIFO depth 0 / 4097)\n";
    }

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
