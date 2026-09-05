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

    smc::uart dut;
    driver    drv;

    sc_core::sc_signal<bool> rst_n;
    sc_core::sc_signal<bool> tx_sig, rx_sig;
    sc_core::sc_signal<bool> cts_n, dsr_n, ri_n, dcd_n;
    sc_core::sc_signal<bool> rts_n, dtr_n, out1_n, out2_n;
    sc_core::sc_signal<bool> rxrdy, txrdy, err, irq;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("uart")
        , drv("drv")
        , rst_n("rst_n")
        , tx_sig("tx_sig"), rx_sig("rx_sig")
        , cts_n("cts_n"), dsr_n("dsr_n"), ri_n("ri_n"), dcd_n("dcd_n")
        , rts_n("rts_n"), dtr_n("dtr_n"), out1_n("out1_n"), out2_n("out2_n")
        , rxrdy("rxrdy"), txrdy("txrdy"), err("err"), irq("irq")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
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
    // 23. Bus error responses.
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
    // 24. transport_dbg + dbg_reg (side-effect-free).
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
    // 25. Register-access + dbg_reg + transport_dbg-write coverage.
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
    // 26. DMA Mode 1 TX FSM (full -> deassert until empty).
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
    // 27. CCI parameter introspection.
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

    // dump_state for visual inspection / coverage of the dump path.
    dut.dump_state(std::cout);

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

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

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
