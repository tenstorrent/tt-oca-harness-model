// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// i3c_controller_cov_tb.cpp — coverage-completion testbench for the OCA I3C
// Controller model.  Targets the ~15 % of branches left uncovered by the
// primary bench, bringing overall line coverage above 95 %.
//
// New sections (continuing the numbering from i3c_controller_tb.cpp):
//
//   17  CCC read transaction (i3c_xfer_kind::CccRead)
//   18  Bus model NACK with non-Success error code (ack=false, error=CrcError)
//   19  Bus model ACK with non-Success error   (ack=true, error=ParityError)
//   20  RX FIFO overflow — bus returns more DWORDs than rx_fifo_depth
//   21  Command queue full → HC_WARN_CMD_SEQ_STALL_STAT
//   22  Response queue full → HC_INTERNAL_ERR_STAT
//   23  HC_CONTROL.ABORT → cmd_q flushed + TRANSFER_ABORT_STAT latched
//   24  SOFT_RST → all queues + latched interrupt status cleared
//   25  Individual RESET_CONTROL bits: RESP_QUEUE_RST, RX_FIFO_RST, IBI_QUEUE_RST
//   26  TX FIFO overflow (XFER_DATA_PORT write when tx_q is full)
//   27  Re-enable BUS_ENABLE with queued commands → xfer_pending_ backlog drained
//   28  HC-level interrupt: INTR_SIGNAL_ENABLE + INTR_FORCE → irq high/low
//   29  PIO level-threshold interrupts: TX_THLD_STAT, CMD_QUEUE_READY_STAT,
//       RX_THLD_STAT, IBI_STATUS_THLD_STAT; zero-threshold default (→ 1)
//   30  DCT_SECTION_OFFSET RW (ENTDAA index bits [31:23])
//   31  HC_CAPABILITIES, ALT_QUEUE_SIZE, STBY_CR_CAPABILITIES reads (RAZ/RO)
//   32  STBY_CR_CONTROL (CR_REQUEST_SEND self-clear) + STBY_CR_DEVICE_ADDR RW
//   33  PIO_CONTROL write; PIO_INTR_STATUS_ENABLE write/read
//   34  inject_ibi: OOR instance → false; IBI queue full → false
//   35  set_bus_model OOR (SC_REPORT_WARNING path, must not crash)
//   36  dump_state: valid instance and OOR instance
//   37  transport_dbg DCT write/read round-trip + CSR write rejection
//   38  dbg_read: OOR → 0; multiple CSR snapshot paths
//   39  xfer_method chaining: three back-to-back commands processed in series
//   40  Partial read: bus returns fewer bytes than cmd DATA_LENGTH
//
// DUT uses small FIFO depths (cmd/rx=4, tx=8, ibi=8) to keep overflow tests
// to a handful of operations.
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
#include <sstream>
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
// Tiny TLM driver — 32-bit AXI-Lite-style register access.
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

    // Small FIFO depths: cmd/resp queue=4, RX=4 DWORDs (16 bytes),
    // TX=8 DWORDs, IBI=8 DWORDs.  Overflow paths are exercisable with
    // just a handful of operations.
    static constexpr unsigned N         = 2;
    static constexpr unsigned CMD_DEPTH = 4;
    static constexpr unsigned RX_DEPTH  = 4;
    static constexpr unsigned TX_DEPTH  = 8;
    static constexpr unsigned IBI_DEPTH = 8;

    smc::i3c_controller dut;
    driver              drv;

    sc_core::sc_vector<sc_core::sc_signal<bool>> irq{"irq", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> scl{"scl", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sda{"sda", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> scl_oe{"scl_oe", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sda_oe{"sda_oe", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> od_pp{"od_pp", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> rpa{"rpa", N};
    sc_core::sc_vector<sc_core::sc_signal<bool>> ria{"ria", N};

    // Bus-model knobs for instance 0 (set before issuing a command).
    bool                 bm_ack     = true;
    smc::i3c_err         bm_error   = smc::i3c_err::Success;
    std::vector<uint8_t> bm_payload;    // read payload returned by the bus model
    smc::i3c_xfer_kind   last_kind  = smc::i3c_xfer_kind::PrivateWrite;
    uint8_t              last_ccc   = 0;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("dut", [] {
              cfg_t c;
              c.num_instances  = N;
              c.cmd_fifo_depth = CMD_DEPTH;
              c.rx_fifo_depth  = RX_DEPTH;
              c.tx_fifo_depth  = TX_DEPTH;
              c.ibi_fifo_depth = IBI_DEPTH;
              return c;
          }())
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

    void settle()   { sc_core::wait(  1, SC_NS); }
    void run_xfer() { sc_core::wait(200, SC_NS); } // > xfer_delay_ns (100 ns)

    void push_command(unsigned inst, uint64_t desc) {
        drv.write(A(inst, cfg_t::COMMAND_PORT), uint32_t(desc & 0xFFFFFFFFu));
        drv.write(A(inst, cfg_t::COMMAND_PORT), uint32_t(desc >> 32));
    }

    static uint64_t make_cmd(uint8_t tid, bool rnw, uint8_t devidx,
                             bool cp, uint8_t ccc, uint16_t len) {
        return (uint64_t(tid  & 0xFu) <<  3) |
               (uint64_t(rnw ? 1u : 0u) << 7) |
               (uint64_t(devidx & 0x7Fu) << 8) |
               (uint64_t(cp  ? 1u : 0u) << 15) |
               (uint64_t(ccc)           << 32) |
               (uint64_t(len)           << 48);
    }

    void enable_bus (unsigned inst = 0) {
        drv.write(A(inst, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));
    }
    void disable_bus(unsigned inst = 0) {
        drv.write(A(inst, cfg_t::HC_CONTROL), 0u);
    }

    void run();
};

void tb::run()
{
    // ------------------------------------------------------------------
    // Bus model for instance 0.
    // Configurable via the tb member variables bm_ack / bm_error /
    // bm_payload.  Always ACKs address 0x42 unless bm_ack == false.
    // ------------------------------------------------------------------
    dut.set_bus_model(0, [this](smc::i3c_xfer& x) {
        last_kind = x.kind;
        last_ccc  = x.ccc_code;
        x.ack     = (x.dynamic_addr == 0x42) && bm_ack;
        x.error   = bm_error;
        if (x.ack && (x.kind == smc::i3c_xfer_kind::PrivateRead ||
                      x.kind == smc::i3c_xfer_kind::CccRead)) {
            x.read_data = bm_payload;
        }
    });

    // DAT entry 0 → dynamic address 0x42.
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    enable_bus();
    settle();

    // ===== 17. CCC read transaction =====
    {
        bm_payload = {0x10, 0x20};
        push_command(0, make_cmd(/*tid*/1, /*rnw*/true, /*dev*/0,
                                 /*cp*/true, /*ccc*/0x91, /*len*/2));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(1u, resp & 0xFu);                                  // TID
        EXPECT_EQ(0u, (resp >> 28) & 0xFu);                          // Success
        EXPECT_EQ(2u, (resp >> 16) & 0xFFFu);                        // actual_len
        EXPECT_EQ((unsigned)smc::i3c_xfer_kind::CccRead, (unsigned)last_kind);
        EXPECT_EQ(0x91u, (unsigned)last_ccc);
        // 0x10, 0x20 packed little-endian into one DWORD.
        EXPECT_EQ(0x00002010u, drv.read(A(0, cfg_t::XFER_DATA_PORT)));
        bm_payload.clear();
    }

    // ===== 18. Bus model NACK with non-Success error code =====
    // ack=false, error=CrcError → response error == CrcError (not AddressNack
    // override: the source code returns x.error when x.error != Success).
    {
        bm_ack   = false;
        bm_error = smc::i3c_err::CrcError;
        push_command(0, make_cmd(/*tid*/2, /*rnw*/true, /*dev*/0, false, 0, /*len*/4));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(2u, resp & 0xFu);
        EXPECT_EQ((unsigned)smc::i3c_err::CrcError, (resp >> 28) & 0xFu);
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT)); // W1C
        bm_ack   = true;
        bm_error = smc::i3c_err::Success;
    }

    // ===== 19. Bus model ACK with non-Success error (ack=true, error≠Success) =====
    {
        bm_error = smc::i3c_err::ParityError;
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x12345678u);
        push_command(0, make_cmd(/*tid*/3, /*rnw*/false, /*dev*/0, false, 0, /*len*/4));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(3u, resp & 0xFu);
        EXPECT_EQ((unsigned)smc::i3c_err::ParityError, (resp >> 28) & 0xFu);
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT)); // W1C
        bm_error = smc::i3c_err::Success;
    }

    // ===== 20. RX FIFO overflow =====
    // rx_fifo_depth = 4 DWORDs (16 bytes).  Request 20 bytes → bus model
    // returns 20 bytes = 5 DWORDs, which exceeds the 4-DWORD RX capacity.
    {
        bm_payload.assign(20, 0xAAu);
        push_command(0, make_cmd(/*tid*/4, /*rnw*/true, /*dev*/0, false, 0, /*len*/20));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(4u, resp & 0xFu);
        EXPECT_EQ((unsigned)smc::i3c_err::OverflowUnder, (resp >> 28) & 0xFu);
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT));
        bm_payload.clear();
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::RX_FIFO_RST));
    }

    // ===== 21. Command queue full → HC_WARN_CMD_SEQ_STALL_STAT =====
    // cmd_fifo_depth = 4.  Push 5 commands while bus disabled; the 5th is
    // dropped and HC_WARN_CMD_SEQ_STALL_STAT is latched.
    {
        disable_bus();
        for (unsigned i = 0; i < CMD_DEPTH; ++i)
            push_command(0, make_cmd(uint8_t(i), false, 0, false, 0, 0));
        // One more: queue full path.
        push_command(0, make_cmd(9u, false, 0, false, 0, 0));
        EXPECT_TRUE((drv.read(A(0, cfg_t::INTR_STATUS)) &
                     (1u << smc::hc_intr::HC_WARN_CMD_SEQ_STALL_STAT)) != 0);
        drv.write(A(0, cfg_t::INTR_STATUS),
                  (1u << smc::hc_intr::HC_WARN_CMD_SEQ_STALL_STAT)); // W1C
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::CMD_QUEUE_RST));
        enable_bus();
    }

    // ===== 22. Response queue full → HC_INTERNAL_ERR_STAT =====
    // Issue CMD_DEPTH+1 zero-length reads without draining RESPONSE_PORT.
    // The first CMD_DEPTH fill resp_q to the limit; the extra one finds
    // resp_q.size() == cmd_fifo_depth and sets HC_INTERNAL_ERR_STAT instead.
    {
        for (unsigned i = 0; i <= CMD_DEPTH; ++i) {
            push_command(0, make_cmd(uint8_t(i), /*rnw*/true, 0, false, 0, /*len*/0));
            run_xfer();
        }
        EXPECT_TRUE((drv.read(A(0, cfg_t::INTR_STATUS)) &
                     (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::INTR_STATUS),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::RESP_QUEUE_RST));
    }

    // ===== 23. HC_CONTROL.ABORT → cmd_q flushed + TRANSFER_ABORT_STAT =====
    {
        disable_bus();
        // Queue a command (stays in cmd_q because bus is disabled).
        push_command(0, make_cmd(0xAu, false, 0, false, 0, 4));
        // Write HC_CONTROL with ABORT|BUS_ENABLE.
        drv.write(A(0, cfg_t::HC_CONTROL),
                  (1u << smc::hc_control::ABORT) | (1u << smc::hc_control::BUS_ENABLE));
        settle();
        // ABORT self-clears; BUS_ENABLE stays.
        EXPECT_TRUE((drv.read(A(0, cfg_t::HC_CONTROL)) &
                     (1u << smc::hc_control::ABORT)) == 0);
        EXPECT_TRUE((drv.read(A(0, cfg_t::HC_CONTROL)) &
                     (1u << smc::hc_control::BUS_ENABLE)) != 0);
        // TRANSFER_ABORT_STAT latched in PIO_INTR_STATUS.
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ABORT_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ABORT_STAT));
        // cmd_q was flushed → no response produced.
        run_xfer();
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::RESPONSE_PORT)));
    }

    // ===== 24. SOFT_RST clears all queues + latched interrupt status =====
    {
        // Populate TX and latch HC_INTERNAL_ERR via INTR_FORCE.
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0xDEAD0001u);
        drv.write(A(0, cfg_t::INTR_FORCE),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
        EXPECT_TRUE((drv.read(A(0, cfg_t::INTR_STATUS)) &
                     (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT)) != 0);
        // SOFT_RST: clears cmd_q, resp_q, tx_q, rx_q, ibi_q,
        //           pio_intr_status, intr_status.
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::SOFT_RST));
        settle();
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::INTR_STATUS)));
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::RESPONSE_PORT)));  // resp_q empty → 0
    }

    // ===== 25. Individual RESET_CONTROL bits: RESP_QUEUE_RST, RX_FIFO_RST,
    //           IBI_QUEUE_RST =====
    {
        // Plant data in resp_q and rx_q via a successful read.
        bm_payload = {0x01, 0x02, 0x03, 0x04};
        push_command(0, make_cmd(0xBu, /*rnw*/true, 0, false, 0, 4));
        run_xfer();
        // resp_q has 1 entry; rx_q has 1 DWORD.

        // RX_FIFO_RST: clears rx_q.
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::RX_FIFO_RST));
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::XFER_DATA_PORT)));  // empty → 0

        // RESP_QUEUE_RST: clears resp_q.
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::RESP_QUEUE_RST));
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::RESPONSE_PORT)));   // empty → 0

        // IBI_QUEUE_RST: inject an IBI, then clear.
        EXPECT_TRUE(dut.inject_ibi(0, 0x10, {0xDEu, 0xADu}));
        settle();
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::IBI_PORT)));        // empty → 0

        bm_payload.clear();
    }

    // ===== 26. TX FIFO overflow (XFER_DATA_PORT write when tx_q is full) =====
    {
        disable_bus();
        // Fill TX FIFO to capacity (TX_DEPTH = 8).
        for (unsigned i = 0; i < TX_DEPTH; ++i)
            drv.write(A(0, cfg_t::XFER_DATA_PORT), uint32_t(i));
        // One more → overflow → TRANSFER_ERR_STAT latched.
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0xDEADu);
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0);
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT));
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::TX_FIFO_RST));
        enable_bus();
    }

    // ===== 27. Re-enable BUS_ENABLE with queued commands → backlog drains =====
    // Queue 2 commands while bus is disabled.  On enable the controller
    // pushes both to xfer_pending_ (the !was_enabled && now_enabled path).
    {
        disable_bus();
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x11111111u);
        push_command(0, make_cmd(0xCu, false, 0, false, 0, 4));
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x22222222u);
        push_command(0, make_cmd(0xDu, false, 0, false, 0, 4));
        enable_bus();      // triggers backlog drain
        run_xfer();        // wait for cmd 0xC
        run_xfer();        // wait for cmd 0xD
        const uint32_t r1 = drv.read(A(0, cfg_t::RESPONSE_PORT));
        const uint32_t r2 = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(0xCu, r1 & 0xFu);
        EXPECT_EQ(0xDu, r2 & 0xFu);
        EXPECT_EQ(0u, (r1 >> 28) & 0xFu); // Success
        EXPECT_EQ(0u, (r2 >> 28) & 0xFu); // Success
    }

    // ===== 28. HC-level interrupt: INTR_SIGNAL_ENABLE + INTR_FORCE → irq =====
    {
        drv.write(A(0, cfg_t::INTR_STATUS_ENABLE),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
        drv.write(A(0, cfg_t::INTR_SIGNAL_ENABLE),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
        drv.write(A(0, cfg_t::INTR_FORCE),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT));
        settle();
        EXPECT_TRUE(irq[0].read());  // HC irq path asserted
        drv.write(A(0, cfg_t::INTR_STATUS),
                  (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT)); // W1C
        settle();
        EXPECT_TRUE(!irq[0].read()); // cleared
        drv.write(A(0, cfg_t::INTR_STATUS_ENABLE),  0u);
        drv.write(A(0, cfg_t::INTR_SIGNAL_ENABLE),  0u);
    }

    // ===== 29. PIO level-threshold interrupts =====
    {
        // -- TX_THLD_STAT: tx_buf_thld = 1<<(bits[2:0]+1) = 1<<(1+1) = 4.
        //    tx_free = TX_DEPTH = 8 >= 4 → bit set in PIO_INTR_STATUS.
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::TX_THLD_STAT)) != 0);

        // -- CMD_QUEUE_READY_STAT: cmd_empty_thld = 1 (reset bits[7:0]=0x01).
        //    cmd_free = CMD_DEPTH = 4 >= 1 → bit set.
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::CMD_QUEUE_READY_STAT)) != 0);

        // -- RX_THLD_STAT: rx_buf_thld = 1<<(bits[10:8]+1).
        //    DATA_BUFFER_THLD_CTRL reset = 0x01010101 → bits[10:8]=0x01 → threshold=4.
        //    Fill rx_q with 4 DWORDs (16 bytes = rx_fifo_depth) so size >= 4.
        bm_payload.assign(16, 0x55u);
        push_command(0, make_cmd(0xEu, /*rnw*/true, 0, false, 0, 16));
        run_xfer();
        drv.read(A(0, cfg_t::RESPONSE_PORT)); // drain response
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::RX_THLD_STAT)) != 0);
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::RX_FIFO_RST));
        bm_payload.clear();

        // -- IBI_STATUS_THLD_STAT: ibi_status_thld = queue_thld_ctrl[31:24] = 1.
        //    Inject an IBI with no payload → ibi_q.size()=1 >= 1 → bit set.
        EXPECT_TRUE(dut.inject_ibi(0, 0x20, {}));
        settle();
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::IBI_STATUS_THLD_STAT)) != 0);
        drv.read(A(0, cfg_t::IBI_PORT)); // drain

        // -- Zero-threshold defaults to 1: write QUEUE_THLD_CTRL with
        //    cmd_empty_thld=0 and resp_buf_thld=0.  Both clamp to 1 in the
        //    model, so CMD_QUEUE_READY_STAT still fires.
        drv.write(A(0, cfg_t::QUEUE_THLD_CTRL), 0x01000000u); // [7:0]=0, [15:8]=0
        settle();
        EXPECT_TRUE((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                     (1u << smc::pio_intr::CMD_QUEUE_READY_STAT)) != 0);
        drv.write(A(0, cfg_t::QUEUE_THLD_CTRL), cfg_t::THLD_CTRL_RESET); // restore
    }

    // ===== 30. DCT_SECTION_OFFSET RW (ENTDAA index bits [31:23]) =====
    {
        const uint32_t entdaa = (5u << 23);
        drv.write(A(0, cfg_t::DCT_SECTION_OFFSET), entdaa | 0x0000FFFFu);
        const uint32_t rdback = drv.read(A(0, cfg_t::DCT_SECTION_OFFSET));
        // Only the ENTDAA bits [31:23] are RW; the fixed offset/size fields
        // are re-computed from constants (not stored).
        EXPECT_EQ(entdaa, rdback & 0xFF800000u);
        // dbg_read must return the same composite value.
        EXPECT_EQ(rdback, dut.dbg_read(A(0, cfg_t::DCT_SECTION_OFFSET)));
    }

    // ===== 31. HC_CAPABILITIES, ALT_QUEUE_SIZE, STBY_CR_CAPABILITIES =====
    {
        EXPECT_EQ(0x00000007u, drv.read(A(0, cfg_t::HC_CAPABILITIES)));
        EXPECT_EQ(0u,          drv.read(A(0, cfg_t::ALT_QUEUE_SIZE)));
        EXPECT_EQ(0u,          drv.read(A(0, cfg_t::STBY_CR_CAPABILITIES)));
        // Writes to these RO registers are silently ignored.
        drv.write(A(0, cfg_t::HC_CAPABILITIES),      0xDEADBEEFu);
        drv.write(A(0, cfg_t::ALT_QUEUE_SIZE),        0xDEADBEEFu);
        drv.write(A(0, cfg_t::STBY_CR_CAPABILITIES),  0xDEADBEEFu);
        EXPECT_EQ(0x00000007u, drv.read(A(0, cfg_t::HC_CAPABILITIES)));
        EXPECT_EQ(0u,          drv.read(A(0, cfg_t::ALT_QUEUE_SIZE)));
        EXPECT_EQ(0u,          drv.read(A(0, cfg_t::STBY_CR_CAPABILITIES)));
        // Also exercise the INTR_FORCE read (returns 0, write-only).
        EXPECT_EQ(0u, drv.read(A(0, cfg_t::INTR_FORCE)));
    }

    // ===== 32. STBY_CR registers =====
    {
        drv.write(A(0, cfg_t::STBY_CR_DEVICE_ADDR), 0x00420001u);
        EXPECT_EQ(0x00420001u, drv.read(A(0, cfg_t::STBY_CR_DEVICE_ADDR)));
        EXPECT_EQ(0x00420001u, dut.dbg_read(A(0, cfg_t::STBY_CR_DEVICE_ADDR)));

        // STBY_CR_CONTROL: bit 5 (CR_REQUEST_SEND) is self-clearing.
        drv.write(A(0, cfg_t::STBY_CR_CONTROL), 0xFFFFFFFFu);
        const uint32_t sc = drv.read(A(0, cfg_t::STBY_CR_CONTROL));
        EXPECT_EQ(0u, sc & (1u << 5)); // self-cleared
        EXPECT_EQ(sc, dut.dbg_read(A(0, cfg_t::STBY_CR_CONTROL)));
    }

    // ===== 33. PIO_CONTROL write; PIO_INTR_STATUS_ENABLE write/read =====
    {
        // PIO_CONTROL stores only bits [2:0].
        drv.write(A(0, cfg_t::PIO_CONTROL), 0x5u);
        EXPECT_EQ(0x5u, drv.read(A(0, cfg_t::PIO_CONTROL)));
        EXPECT_EQ(0x5u, dut.dbg_read(A(0, cfg_t::PIO_CONTROL)));
        drv.write(A(0, cfg_t::PIO_CONTROL), cfg_t::PIO_CONTROL_RESET); // restore

        drv.write(A(0, cfg_t::PIO_INTR_STATUS_ENABLE), 0x3FFu);
        EXPECT_EQ(0x3FFu, drv.read(A(0, cfg_t::PIO_INTR_STATUS_ENABLE)));
        EXPECT_EQ(0x3FFu, dut.dbg_read(A(0, cfg_t::PIO_INTR_STATUS_ENABLE)));
        drv.write(A(0, cfg_t::PIO_INTR_STATUS_ENABLE), 0u); // restore
    }

    // ===== 34. inject_ibi: OOR instance → false; IBI queue full → false =====
    {
        // Ensure the IBI queue is clean.
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));

        // OOR instance (>= N=2) → always false.
        EXPECT_TRUE(!dut.inject_ibi(N, 0x10, {}));

        // Fill the IBI queue (ibi_fifo_depth=8 DWORDs).
        // Each call with an 8-byte payload uses 1 (status) + 2 (data) = 3 DWORDs.
        //   call 1: size 0→3  (0+1+2=3 ≤ 8 → ok)
        //   call 2: size 3→6  (3+1+2=6 ≤ 8 → ok)
        //   call 3: size 6+? (6+1+2=9 > 8 → fail)
        EXPECT_TRUE( dut.inject_ibi(0, 0x10, {1, 2, 3, 4, 5, 6, 7, 8}));
        settle();
        EXPECT_TRUE( dut.inject_ibi(0, 0x11, {1, 2, 3, 4, 5, 6, 7, 8}));
        settle();
        EXPECT_TRUE(!dut.inject_ibi(0, 0x12, {1, 2, 3, 4, 5, 6, 7, 8}));

        // Drain.
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));
    }

    // ===== 35. set_bus_model OOR: SC_REPORT_WARNING — must not crash =====
    {
        dut.set_bus_model(N, [](smc::i3c_xfer&) {}); // N >= num_instances
    }

    // ===== 36. dump_state: valid instance and OOR instance =====
    {
        std::ostringstream oss;
        dut.dump_state(0, oss);
        EXPECT_TRUE(oss.str().find("i3c_controller[0]") != std::string::npos);
        oss.str(""); oss.clear();
        dut.dump_state(99u, oss);
        EXPECT_TRUE(oss.str().find("bad instance") != std::string::npos);
    }

    // ===== 37. transport_dbg DCT write/read round-trip + CSR rejection =====
    {
        uint32_t v = 0xFEEDBEEFu;
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(A(0, cfg_t::DCT_BASE + 0));
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&v));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
        v = 0u;
        gp.set_command(tlm::TLM_READ_COMMAND);
        EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
        EXPECT_EQ(0xFEEDBEEFu, v);

        // Verify round-trip is visible via b_transport as well.
        EXPECT_EQ(0xFEEDBEEFu, drv.read(A(0, cfg_t::DCT_BASE + 0)));

        // transport_dbg write to a non-table offset → rejected (returns 0).
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(A(0, cfg_t::HC_CONTROL));
        EXPECT_EQ(0u, drv.sock->transport_dbg(gp));
    }

    // ===== 38. dbg_read: OOR → 0; multiple CSR snapshot paths =====
    {
        // Out-of-aperture address.
        EXPECT_EQ(0u, dut.dbg_read(uint64_t(N) * cfg_t::INSTANCE_SPACING + 4));

        // Sample every dbg_read switch branch not hit by prior tests.
        EXPECT_EQ(cfg_t::HCI_VERSION_VALUE,
                  dut.dbg_read(A(0, cfg_t::HCI_VERSION)));
        EXPECT_EQ(0x00000007u,
                  dut.dbg_read(A(0, cfg_t::HC_CAPABILITIES)));
        EXPECT_EQ(uint32_t(cfg_t::DAT_BASE |
                           (cfg_t::DAT_ENTRIES << 12) |
                           (cfg_t::DAT_DWORDS  << 19)),
                  dut.dbg_read(A(0, cfg_t::DAT_SECTION_OFFSET)));
        EXPECT_EQ(uint32_t(cfg_t::COMMAND_PORT),
                  dut.dbg_read(A(0, cfg_t::PIO_SECTION_OFFSET)));
        EXPECT_EQ(0u, dut.dbg_read(A(0, cfg_t::INTR_STATUS)));
        EXPECT_EQ(0u, dut.dbg_read(A(0, cfg_t::INTR_STATUS_ENABLE)));
        EXPECT_EQ(0u, dut.dbg_read(A(0, cfg_t::INTR_SIGNAL_ENABLE)));
        EXPECT_EQ(cfg_t::STBY_CR_EXTCAP_HDR,
                  dut.dbg_read(A(0, cfg_t::STBY_CR_EXTCAP_HEADER)));
        // Unmapped offset inside the window → default: return 0.
        EXPECT_EQ(0u, dut.dbg_read(A(0, 0x018u)));
    }

    // ===== 39. xfer_method chaining: three back-to-back commands =====
    // All three are enqueued into xfer_pending_ before xfer_method runs.
    // The method processes one per tick, re-notifying xfer_event_ until
    // the pending list is empty.
    {
        drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::TX_FIFO_RST));
        for (unsigned i = 0; i < 3u; ++i) {
            drv.write(A(0, cfg_t::XFER_DATA_PORT), uint32_t(i + 0xAA00u));
            push_command(0, make_cmd(uint8_t(i), false, 0, false, 0, 4));
        }
        // Wait long enough for all three xfer ticks (3 × 100 ns + margin).
        sc_core::wait(700, SC_NS);
        for (unsigned i = 0; i < 3u; ++i) {
            const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
            EXPECT_EQ(i, resp & 0xFu);
            EXPECT_EQ(0u, (resp >> 28) & 0xFu); // Success
        }
    }

    // ===== 40. Partial read: bus returns fewer bytes than DATA_LENGTH =====
    // Request 8 bytes; bus model returns 3 bytes.  actual_len = 3 (got<want).
    {
        bm_payload = {0xAAu, 0xBBu, 0xCCu};
        push_command(0, make_cmd(0xFu, /*rnw*/true, 0, false, 0, /*len*/8));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ(0xFu, resp & 0xFu);
        EXPECT_EQ(3u,   (resp >> 16) & 0xFFFu); // actual_len = got = 3
        EXPECT_EQ(0u,   (resp >> 28) & 0xFu);   // Success (no overflow)
        // RX contains 1 DWORD: bytes {0xAA,0xBB,0xCC,0x00} little-endian.
        EXPECT_EQ(0x00CCBBAAu, drv.read(A(0, cfg_t::XFER_DATA_PORT)));
        bm_payload.clear();
    }

    if (g_failures == 0) std::cout << "ALL TESTS PASSED\n";
    else                 std::cout << "FAILURE(S): " << g_failures << " check(s) failed\n";

    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char*[])
{
    // Suppress SC_REPORT_WARNING output to keep test logs clean while still
    // executing the warning path (set_bus_model OOR, section 35).
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_WARNING, sc_core::SC_DO_NOTHING);

    static cci_utils::consuming_broker broker("global_broker");

    cci::cci_register_broker(broker);
    tb t("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
