// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// i2c_controller_tb.cpp -- primary self-checking test bench for the OCA I2C
// Controller LT model (CCI-compliant).
//
// CCI integration highlights
// --------------------------------------------------------------------------
// * sc_main registers a global CCI broker before any module is constructed.
// * Preset values are injected for tb.i2c.rx_fifo_depth (64 -> 8),
//   tb.i2c.access_delay_ns (2 -> 5) and tb.i2c.xfer_delay_ns (100 -> 20) to
//   demonstrate pre-construction override and to keep transaction latency short.
// * The LT path is exercised through a tlm_quantumkeeper in the driver.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <algorithm>
#include <functional>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

#include "i2c_controller.h"

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

// Register offsets.
constexpr uint64_t INTR_STATE = 0x00, INTR_ENABLE = 0x04, INTR_TEST = 0x08;
constexpr uint64_t SMBUS_CTRL = 0x0C, CTRL = 0x10, STATUS = 0x14;
constexpr uint64_t RDATA = 0x18, FDATA = 0x1C, FIFO_CTRL = 0x20;
constexpr uint64_t HOST_FIFO_CONFIG = 0x24, TARGET_FIFO_CONFIG = 0x28;
constexpr uint64_t HOST_FIFO_STATUS = 0x2C, TARGET_FIFO_STATUS = 0x30;
constexpr uint64_t OVRD = 0x34, VAL = 0x38, TIMING0 = 0x3C;
constexpr uint64_t TIMEOUT_CTRL = 0x50, TARGET_ID = 0x54;
constexpr uint64_t ACQDATA = 0x58, TXDATA = 0x5C;
constexpr uint64_t TARGET_NACK_COUNT = 0x68, TARGET_ACK_CTRL = 0x6C;
constexpr uint64_t ACQ_FIFO_NEXT_DATA = 0x70, HOST_NACK_HANDLER_TIMEOUT = 0x74;
constexpr uint64_t CONTROLLER_EVENTS = 0x78, TARGET_EVENTS = 0x7C, SMBUS_STATUS = 0x80;

// FDATA flags.
constexpr uint32_t F_START = 1u << 8, F_STOP = 1u << 9, F_READB = 1u << 10,
                   F_NAKOK = 1u << 12;
// CTRL bits.
constexpr uint32_t C_ENABLEHOST = 1u << 0, C_ENABLETARGET = 1u << 1;
// INTR bits.
constexpr uint32_t I_FMT_THRESHOLD = 1u << 0, I_RX_THRESHOLD = 1u << 1,
                   I_ACQ_THRESHOLD = 1u << 2, I_CONTROLLER_HALT = 1u << 4,
                   I_CMD_COMPLETE = 1u << 9, I_TX_THRESHOLD = 1u << 11,
                   I_SMBALERT = 1u << 15;

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

    smc::i2c_controller dut;
    smc::i2c_wrap_ctrl  wrap;
    driver              drv;
    driver              wrap_drv;

    sc_core::sc_signal<bool> rst_n;
    sc_core::sc_signal<bool> irq;

    // Fake remote slave for Controller Mode (no real SCL/SDA in this LT model).
    // Write stores payload in slave_mem[addr]; Read returns up to read_len bytes
    // from that same address (empty if never written). seed_slave() can preload.
    bool                                   slave_will_ack = true;
    std::map<uint8_t, std::vector<uint8_t>> slave_mem;
    unsigned                               bus_calls = 0;     // how many times DUT called us
    uint8_t                                bus_last_addr = 0; // last request (for EXPECT_*)
    smc::i2c_dir                           bus_last_dir  = smc::i2c_dir::Write;
    std::vector<uint8_t>                   bus_last_write;
    unsigned                               bus_last_rlen = 0;

    void seed_slave(uint8_t addr, std::vector<uint8_t> data)
    {
        slave_mem[addr] = std::move(data);
    }

    // Called by the DUT from drain_fmt() when it wants to talk to the bus.
    void emulate_remote_slave(smc::i2c_xfer& req)
    {
        ++bus_calls;
        bus_last_addr  = req.addr;
        bus_last_dir   = req.dir;
        bus_last_write = req.write_data;
        bus_last_rlen  = req.read_len;

        req.ack = slave_will_ack;
        if (!req.ack) return;

        if (req.dir == smc::i2c_dir::Write) {
            slave_mem[req.addr] = req.write_data; // store what master wrote
            return;
        }

        // Read: return bytes previously written/seeded for this address.
        req.read_data.clear();
        const auto it = slave_mem.find(req.addr);
        if (it == slave_mem.end()) return;
        const unsigned n =
            std::min(req.read_len, static_cast<unsigned>(it->second.size()));
        req.read_data.assign(it->second.begin(), it->second.begin() +
                             static_cast<std::ptrdiff_t>(n));
    }

    explicit tb(sc_module_name n)
        : sc_module(n), dut("i2c"), wrap("i2c_ctrl"), drv("drv"),
          wrap_drv("wrap_drv"), rst_n("rst_n"), irq("irq")
    {
        drv.sock.bind(dut.reg_socket);
        wrap_drv.sock.bind(wrap.reg_socket);
        dut.rst_n_i(rst_n);
        wrap.rst_n_i(rst_n);
        dut.irq_o(irq);

        dut.set_bus_model(std::bind(&tb::emulate_remote_slave, this,
                                    std::placeholders::_1));

        SC_THREAD(run);
    }

    static void settle() {
        for (int i = 0; i < 3; ++i) sc_core::wait(SC_ZERO_TIME);
    }

    // Advance real sim time so timed xfer events fire, then settle deltas.
    void step(double ns = 60.0) {
        sc_core::wait(ns, SC_NS);
        settle();
    }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
        settle();
    }

    void run();
};

void tb::run()
{
    std::cout << "==== OCA I2C Controller TB (CCI-compliant) ====\n";
    rst_n.write(true);
    sc_core::wait(1, SC_NS);

    // ----------------------------------------------------------------------
    // 1. Reset values.
    // ----------------------------------------------------------------------
    pulse_reset();
    EXPECT_EQ(0x00u, drv.read32(INTR_STATE));
    EXPECT_EQ(0x00u, drv.read32(INTR_ENABLE));
    EXPECT_EQ(0x00u, drv.read32(CTRL));
    EXPECT_EQ(false, irq.read());
    {   // STATUS reset: FMTEMPTY|HOSTIDLE|TARGETIDLE|RXEMPTY|TXEMPTY|ACQEMPTY
        const uint32_t st = drv.read32(STATUS);
        EXPECT_EQ((1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<8)|(1u<<9), st);
    }
    std::cout << "  [PASS] reset values\n";

    // ----------------------------------------------------------------------
    // 2. Plain register R/W with field masking.
    // ----------------------------------------------------------------------
    drv.write32(CTRL, 0xFFFFFFFF);
    EXPECT_EQ(0x000000FFu, drv.read32(CTRL));       // CTRL[7:0]
    drv.write32(OVRD, 0xFFFFFFFF);
    EXPECT_EQ(0x00000007u, drv.read32(OVRD));       // OVRD[2:0]
    drv.write32(TIMING0, 0xFFFFFFFF);
    EXPECT_EQ(0x1FFF1FFFu, drv.read32(TIMING0));    // THIGH|TLOW
    drv.write32(TIMEOUT_CTRL, 0xFFFFFFFF);
    EXPECT_EQ(0xFFFFFFFFu, drv.read32(TIMEOUT_CTRL));
    drv.write32(HOST_NACK_HANDLER_TIMEOUT, 0xFFFFFFFF);
    EXPECT_EQ(0xFFFFFFFFu, drv.read32(HOST_NACK_HANDLER_TIMEOUT));
    drv.write32(SMBUS_CTRL, 0xFFFFFFFF);
    EXPECT_EQ(0x00000011u, drv.read32(SMBUS_CTRL)); // SMBSUS|SMBALERT
    EXPECT_EQ(0x00000000u, drv.read32(VAL));        // RO -> 0
    EXPECT_EQ(0x00000000u, drv.read32(SMBUS_STATUS));// RO -> 0
    drv.write32(VAL, 0xFFFFFFFF);                   // RO write ignored
    EXPECT_EQ(0x00000000u, drv.read32(VAL));
    drv.write32(CTRL, 0x0);
    std::cout << "  [PASS] register R/W + masking\n";

    // ----------------------------------------------------------------------
    // 3. Controller-Mode WRITE transaction via the bus model.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = true; bus_calls = 0;
    drv.write32(FDATA, (uint32_t(0x50 << 1) | 0u) | F_START); // addr 0x50 WRITE
    drv.write32(FDATA, 0xAA);
    drv.write32(FDATA, 0xBB | F_STOP);
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_EQ(0x50u, bus_last_addr);
    EXPECT_TRUE(bus_last_dir == smc::i2c_dir::Write);
    EXPECT_EQ(2u, static_cast<uint32_t>(bus_last_write.size()));
    if (bus_last_write.size() == 2) {
        EXPECT_EQ(0xAAu, bus_last_write[0]);
        EXPECT_EQ(0xBBu, bus_last_write[1]);
    }
    EXPECT_EQ(0u, dut.dbg_fmt_count());             // FMT drained
    std::cout << "  [PASS] controller write transaction\n";

    // ----------------------------------------------------------------------
    // 4. Controller-Mode WRITE then READ (slave_mem stores, then returns).
    // ----------------------------------------------------------------------
    slave_will_ack = true; bus_calls = 0;
    // Write 3 bytes into fake slave at 0x51
    drv.write32(FDATA, (uint32_t(0x51 << 1) | 0u) | F_START);
    drv.write32(FDATA, 0x11);
    drv.write32(FDATA, 0x22);
    drv.write32(FDATA, 0x33 | F_STOP);
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_EQ(3u, static_cast<uint32_t>(slave_mem[0x51].size()));
    // Read them back
    bus_calls = 0;
    drv.write32(FDATA, (uint32_t(0x51 << 1) | 1u) | F_START); // addr 0x51 READ
    drv.write32(FDATA, 3u | F_READB | F_STOP);                // read 3 bytes
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_TRUE(bus_last_dir == smc::i2c_dir::Read);
    EXPECT_EQ(3u, bus_last_rlen);
    EXPECT_EQ(3u, dut.dbg_rx_count());
    EXPECT_EQ(0x11u, drv.read32(RDATA));
    EXPECT_EQ(0x22u, drv.read32(RDATA));
    EXPECT_EQ(0x33u, drv.read32(RDATA));
    EXPECT_EQ(0x00u, drv.read32(RDATA));            // empty -> 0
    std::cout << "  [PASS] controller write-then-read (slave_mem)\n";

    // ----------------------------------------------------------------------
    // 4b. RX FIFO overflow (read more bytes than the 8-deep RX FIFO holds).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = true;
    seed_slave(0x20, std::vector<uint8_t>(10, 0x5A)); // 10 bytes in slave memory
    drv.write32(FDATA, (uint32_t(0x20 << 1) | 1u) | F_START);
    drv.write32(FDATA, 10u | F_READB | F_STOP);
    step();
    EXPECT_EQ(8u, dut.dbg_rx_count());              // clamped to depth
    EXPECT_TRUE((drv.read32(INTR_STATE) & (1u << 3)) != 0); // RX_OVERFLOW
    std::cout << "  [PASS] RX FIFO overflow\n";

    // 4c. Trailing open segment (no STOP) still executes in this LT model.
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = true; bus_calls = 0;
    drv.write32(FDATA, (uint32_t(0x21 << 1)) | F_START); // write, no STOP
    drv.write32(FDATA, 0xC5);
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_EQ(0x21u, bus_last_addr);
    EXPECT_EQ(1u, static_cast<uint32_t>(bus_last_write.size()));
    EXPECT_EQ(1u, static_cast<uint32_t>(slave_mem[0x21].size()));
    EXPECT_EQ(0xC5u, slave_mem[0x21][0]);
    std::cout << "  [PASS] trailing open segment\n";

    // ----------------------------------------------------------------------
    // 5. CMD_COMPLETE interrupt (raised on STOP), cleared by W1C.
    // ----------------------------------------------------------------------
    drv.write32(INTR_ENABLE, I_CMD_COMPLETE);
    slave_will_ack = true;
    drv.write32(FDATA, (uint32_t(0x40 << 1)) | F_START);
    drv.write32(FDATA, 0x01 | F_STOP);
    step();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_CMD_COMPLETE) != 0);
    EXPECT_EQ(true, irq.read());
    drv.write32(INTR_STATE, I_CMD_COMPLETE);        // W1C
    settle();
    EXPECT_EQ(0u, drv.read32(INTR_STATE) & I_CMD_COMPLETE);
    EXPECT_EQ(false, irq.read());
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] CMD_COMPLETE interrupt + W1C\n";

    // ----------------------------------------------------------------------
    // 6. Address NACK halts the controller; clearing events resumes.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    drv.write32(INTR_ENABLE, I_CONTROLLER_HALT);
    slave_will_ack = false;
    drv.write32(FDATA, (uint32_t(0x60 << 1)) | F_START);
    drv.write32(FDATA, 0x01 | F_STOP);
    step();
    EXPECT_TRUE((drv.read32(CONTROLLER_EVENTS) & 0x1u) != 0); // NACK
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_CONTROLLER_HALT) != 0);
    EXPECT_EQ(true, irq.read());
    drv.write32(CONTROLLER_EVENTS, 0xF);            // W1C -> resume
    settle();
    EXPECT_EQ(0u, drv.read32(CONTROLLER_EVENTS));
    EXPECT_EQ(0u, drv.read32(INTR_STATE) & I_CONTROLLER_HALT);
    EXPECT_EQ(false, irq.read());
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] NACK halt + resume\n";

    // ----------------------------------------------------------------------
    // 7. NAKOK: a NACK with NAKOK does not halt; STOP still completes.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = false;
    drv.write32(FDATA, (uint32_t(0x60 << 1)) | F_START | F_NAKOK);
    drv.write32(FDATA, 0x01 | F_STOP | F_NAKOK);
    step();
    EXPECT_EQ(0u, drv.read32(CONTROLLER_EVENTS));   // not halted
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_CMD_COMPLETE) != 0);
    drv.write32(INTR_STATE, I_CMD_COMPLETE);
    std::cout << "  [PASS] NAKOK suppresses halt\n";

    // ----------------------------------------------------------------------
    // 8. FMT threshold interrupt (FMT level below FMT_THRESH).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(HOST_FIFO_CONFIG, (4u << 16));      // FMT_THRESH = 4
    drv.write32(INTR_ENABLE, I_FMT_THRESHOLD);
    settle();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_FMT_THRESHOLD) != 0); // 0 < 4
    EXPECT_EQ(true, irq.read());
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] FMT threshold interrupt\n";

    // ----------------------------------------------------------------------
    // 9. RX threshold interrupt (RX level above RX_THRESH).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    drv.write32(HOST_FIFO_CONFIG, 1u);              // RX_THRESH = 1
    drv.write32(INTR_ENABLE, I_RX_THRESHOLD);
    slave_will_ack = true;
    seed_slave(0x30, {0xA0, 0xA1, 0xA2, 0xA3});
    drv.write32(FDATA, (uint32_t(0x30 << 1) | 1u) | F_START);
    drv.write32(FDATA, 4u | F_READB | F_STOP);
    step();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_RX_THRESHOLD) != 0); // 4 > 1
    EXPECT_EQ(true, irq.read());
    {   // HOST_FIFO_STATUS reflects levels.
        const uint32_t hfs = drv.read32(HOST_FIFO_STATUS);
        EXPECT_EQ(0u, hfs & 0xFFFu);                // FMTLVL = 0
        EXPECT_EQ(4u, (hfs >> 16) & 0xFFFu);        // RXLVL  = 4
    }
    drv.write32(FIFO_CTRL, 1u << 0);                // RXRST
    settle();
    EXPECT_EQ(0u, dut.dbg_rx_count());
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] RX threshold interrupt + RXRST\n";

    // ----------------------------------------------------------------------
    // 10. FMT reset via FIFO_CTRL (host disabled so FMT is retained).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(FDATA, 0x11);
    drv.write32(FDATA, 0x22);
    EXPECT_EQ(2u, dut.dbg_fmt_count());
    drv.write32(FIFO_CTRL, 1u << 1);                // FMTRST
    EXPECT_EQ(0u, dut.dbg_fmt_count());
    std::cout << "  [PASS] FMT reset\n";

    // ----------------------------------------------------------------------
    // 10b. Enable host AFTER queueing FMT: the CTRL write kicks the engine.
    // ----------------------------------------------------------------------
    pulse_reset();
    slave_will_ack = true; bus_calls = 0;
    drv.write32(FDATA, (uint32_t(0x22 << 1)) | F_START); // queued (host off)
    drv.write32(FDATA, 0x5A | F_STOP);
    EXPECT_EQ(2u, dut.dbg_fmt_count());
    drv.write32(CTRL, C_ENABLEHOST);                // enabling host drains it
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_EQ(0u, dut.dbg_fmt_count());
    std::cout << "  [PASS] enable-host-after-queue kick\n";

    // ----------------------------------------------------------------------
    // 10c. Repeated START within one FMT stream => two bus segments.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = true; bus_calls = 0;
    drv.write32(FDATA, (uint32_t(0x33 << 1)) | F_START);        // segment A (write)
    drv.write32(FDATA, 0x01);
    drv.write32(FDATA, (uint32_t(0x34 << 1) | 1u) | F_START);   // repeated START -> B (read)
    drv.write32(FDATA, 1u | F_READB | F_STOP);
    step();
    EXPECT_EQ(2u, bus_calls);                        // A flushed at repeated START, then B
    EXPECT_EQ(0x34u, bus_last_addr);
    std::cout << "  [PASS] repeated START -> two segments\n";

    // ----------------------------------------------------------------------
    // 10d. Resume a halted controller that still has queued FMT entries.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = false;                            // segment A will NACK+halt
    drv.write32(FDATA, (uint32_t(0x35 << 1)) | F_START);
    drv.write32(FDATA, 0x01 | F_STOP);
    drv.write32(FDATA, (uint32_t(0x36 << 1)) | F_START); // B stays queued while halted
    drv.write32(FDATA, 0x02 | F_STOP);
    step();
    EXPECT_EQ(2u, dut.dbg_fmt_count());              // A halted, B still queued
    EXPECT_TRUE((drv.read32(CONTROLLER_EVENTS) & 0x1u) != 0);
    slave_will_ack = true; bus_calls = 0;
    drv.write32(CONTROLLER_EVENTS, 0xF);            // clear events -> reschedule drain
    step();
    EXPECT_EQ(1u, bus_calls);                        // B now completes
    EXPECT_EQ(0x36u, bus_last_addr);
    EXPECT_EQ(0u, dut.dbg_fmt_count());
    std::cout << "  [PASS] resume with pending FMT\n";

    // ----------------------------------------------------------------------
    // 11. Target-Mode WRITE: ACQ FIFO fills; START/STOP detect latch.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLETARGET);
    drv.write32(TARGET_ID, 0x50u | (0x7Fu << 7));   // ADDRESS0=0x50, MASK0=0x7F
    EXPECT_TRUE(dut.target_write(0x50, {0xDE, 0xAD}));
    settle();
    EXPECT_EQ(4u, dut.dbg_acq_count());             // Start + 2 data + Stop
    {   // First ACQ entry: Start signal, byte = addr<<1 (write).
        const uint32_t e0 = drv.read32(ACQDATA);
        EXPECT_EQ(uint32_t(0x50 << 1), e0 & 0xFFu);
        EXPECT_EQ(1u, (e0 >> 8) & 0x7u);            // SIGNAL = Start
    }
    EXPECT_EQ(0xDEu, drv.read32(ACQDATA) & 0xFFu);
    EXPECT_EQ(0xADu, drv.read32(ACQDATA) & 0xFFu);
    EXPECT_EQ(2u, (drv.read32(ACQDATA) >> 8) & 0x7u); // SIGNAL = Stop
    EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 3)) != 0); // START_DETECT
    EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 4)) != 0); // STOP_DETECT
    std::cout << "  [PASS] target write / ACQ FIFO\n";

    // ----------------------------------------------------------------------
    // 12. Target-Mode READ: drains the Target TX FIFO.
    // ----------------------------------------------------------------------
    drv.write32(FIFO_CTRL, 1u << 7);                // ACQRST
    drv.write32(TXDATA, 0x77);
    drv.write32(TXDATA, 0x88);
    EXPECT_EQ(2u, dut.dbg_tx_count());
    {   // TARGET_FIFO_STATUS reflects TX level.
        const uint32_t tfs = drv.read32(TARGET_FIFO_STATUS);
        EXPECT_EQ(2u, tfs & 0xFFFu);
    }
    std::vector<uint8_t> out;
    EXPECT_TRUE(dut.target_read(0x50, 2, out));
    EXPECT_EQ(2u, static_cast<uint32_t>(out.size()));
    if (out.size() == 2) { EXPECT_EQ(0x77u, out[0]); EXPECT_EQ(0x88u, out[1]); }
    EXPECT_EQ(0u, dut.dbg_tx_count());
    std::cout << "  [PASS] target read / TX FIFO\n";

    // ----------------------------------------------------------------------
    // 13. Target NACK count: unmatched address, saturating + read-clear.
    // ----------------------------------------------------------------------
    EXPECT_TRUE(!dut.target_write(0x07, {0x01}));   // 0x07 != 0x50
    EXPECT_TRUE(!dut.target_read(0x07, 1, out));
    settle();
    EXPECT_EQ(2u, drv.read32(TARGET_NACK_COUNT));
    EXPECT_EQ(0u, drv.read32(TARGET_NACK_COUNT));   // rclr: cleared on read
    std::cout << "  [PASS] target NACK count\n";

    // ----------------------------------------------------------------------
    // 14. ACQ threshold + TX threshold interrupts.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLETARGET);
    drv.write32(TARGET_ID, 0x50u | (0x7Fu << 7));
    drv.write32(TARGET_FIFO_CONFIG, (0u << 16) | 4u); // ACQ_THRESH=0, TX_THRESH=4
    drv.write32(INTR_ENABLE, I_ACQ_THRESHOLD | I_TX_THRESHOLD);
    settle();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_TX_THRESHOLD) != 0); // 0 < 4
    EXPECT_TRUE(dut.target_write(0x50, {0x01, 0x02}));
    settle();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_ACQ_THRESHOLD) != 0); // acq > 0
    EXPECT_EQ(true, irq.read());
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] ACQ/TX threshold interrupts\n";

    // ----------------------------------------------------------------------
    // 15. INTR_TEST force paths (latched W1C bit + held level bit).
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(INTR_ENABLE, I_SMBALERT);
    drv.write32(INTR_TEST, I_SMBALERT);             // pulse-force SMBALERT
    settle();
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_SMBALERT) != 0);
    EXPECT_EQ(true, irq.read());
    drv.write32(INTR_STATE, I_SMBALERT);            // W1C clears latched
    settle();
    EXPECT_EQ(0u, drv.read32(INTR_STATE) & I_SMBALERT);
    drv.write32(INTR_TEST, I_FMT_THRESHOLD);        // held level force
    EXPECT_EQ(I_FMT_THRESHOLD, drv.read32(INTR_TEST));
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_FMT_THRESHOLD) != 0);
    drv.write32(INTR_TEST, 0);
    drv.write32(INTR_ENABLE, 0);
    std::cout << "  [PASS] INTR_TEST force\n";

    // ----------------------------------------------------------------------
    // 16. TARGET_ACK_CTRL + ACQ_FIFO_NEXT_DATA + misc read-only regs.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(TARGET_ACK_CTRL, 0x80000005u);      // NBYTES=5, NACK pulse
    EXPECT_EQ(5u, drv.read32(TARGET_ACK_CTRL) & 0x1FFu);
    EXPECT_EQ(1u, drv.read32(TARGET_NACK_COUNT));   // NACK pulse bumped it
    (void)drv.read32(TARGET_NACK_COUNT);
    EXPECT_EQ(0u, drv.read32(ACQ_FIFO_NEXT_DATA));  // ACQ empty
    std::cout << "  [PASS] TARGET_ACK_CTRL / ACQ_FIFO_NEXT_DATA\n";

    // ----------------------------------------------------------------------
    // 17. transport_dbg (side-effect-free read + write path) & dbg_reg.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLETARGET);
    drv.write32(TARGET_ID, 0x50u | (0x7Fu << 7));
    dut.target_write(0x50, {0x5A});                 // ACQ: start + data + stop
    settle();
    {
        uint32_t v = 0;
        EXPECT_EQ(4u, drv.dbg_read(ACQDATA, v));    // debug read does NOT pop
        EXPECT_EQ(uint32_t(0x50 << 1), v & 0xFFu);
        EXPECT_EQ(3u, dut.dbg_acq_count());         // still 3 entries
        EXPECT_EQ(uint32_t(0x50 << 1), dut.dbg_reg(ACQDATA) & 0xFFu);
        EXPECT_EQ(4u, drv.dbg_write(TARGET_ID, 0x1234)); // dbg write path
        EXPECT_EQ(0x1234u & 0x0FFFFFFFu, dut.dbg_reg(TARGET_ID));
    }
    std::cout << "  [PASS] transport_dbg + dbg_reg\n";

    // ----------------------------------------------------------------------
    // 18. CCI parameter introspection.
    // ----------------------------------------------------------------------
    {
        auto broker = cci::cci_get_broker();

        auto h_rx = broker.get_param_handle("tb.i2c.rx_fifo_depth");
        EXPECT_TRUE(h_rx.is_valid());
        EXPECT_TRUE(h_rx.get_cci_value().to_json() == std::string("8")); // preset
        EXPECT_TRUE(h_rx.is_preset_value());

        auto h_fmt = broker.get_param_handle("tb.i2c.fmt_fifo_depth");
        EXPECT_TRUE(h_fmt.is_valid());
        EXPECT_TRUE(h_fmt.get_cci_value().to_json() == std::string("64")); // default

        auto h_d = broker.get_param_handle("tb.i2c.access_delay_ns");
        EXPECT_TRUE(h_d.is_valid());
        h_d.set_cci_value(cci::cci_value(9.0));
        EXPECT_TRUE(h_d.get_cci_value().get_double() == 9.0);

        std::cout << "  CCI parameters:\n";
        for (auto& h : broker.get_param_handles()) {
            std::cout << "    " << std::left << std::setw(30) << h.name()
                      << " = " << std::setw(8) << h.get_cci_value().to_json()
                      << (h.is_preset_value() ? " [preset]" : " [default]") << "\n";
        }
        std::cout << "  [PASS] CCI introspection\n";
    }

    {
        using C = smc::i2c_wrap_ctrl_cfg;
        wrap_drv.write32(C::OFF_I2C_CTRL, 0x111);
        EXPECT_EQ(0x111u, wrap_drv.read32(C::OFF_I2C_CTRL));
        wrap_drv.write32(C::OFF_I2C_CTRL + 4, 0x001);
        EXPECT_EQ(0x001u, wrap_drv.read32(C::OFF_I2C_CTRL + 4));
        wrap_drv.write32(C::OFF_I2C_CTRL + 8, 0xFFF);
        EXPECT_EQ(0x111u, wrap_drv.read32(C::OFF_I2C_CTRL + 8));
        uint32_t dbg = 0;
        EXPECT_EQ(4u, wrap_drv.dbg_read(C::OFF_I2C_CTRL, dbg));
        EXPECT_EQ(0x111u, dbg);
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
        EXPECT_EQ(0u, wrap_drv.read32(C::OFF_I2C_CTRL));
        std::cout << "  [PASS] i2c_wrap_ctrl CSRs\n";
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

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.i2c.rx_fifo_depth",  cci::cci_value(8u));
    global_broker.set_preset_cci_value("tb.i2c.access_delay_ns", cci::cci_value(5.0));
    global_broker.set_preset_cci_value("tb.i2c.xfer_delay_ns",   cci::cci_value(20.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
