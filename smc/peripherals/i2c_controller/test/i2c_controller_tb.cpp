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
#include "smc_axi_extension.h"
#include "tlm_probe.h"

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
                   I_ACQ_THRESHOLD = 1u << 2, I_RX_OVERFLOW = 1u << 3,
                   I_CONTROLLER_HALT = 1u << 4, I_SCL_INTERFERENCE = 1u << 5,
                   I_SDA_INTERFERENCE = 1u << 6, I_STRETCH_TIMEOUT = 1u << 7,
                   I_SDA_UNSTABLE = 1u << 8, I_CMD_COMPLETE = 1u << 9,
                   I_TX_THRESHOLD = 1u << 11,
                   I_UNEXP_STOP = 1u << 13, I_HOST_TIMEOUT = 1u << 14,
                   I_SMBALERT = 1u << 15, I_CTRL_TX_FIFO_ERR = 1u << 16,
                   I_CTRL_RX_FIFO_ERR = 1u << 17, I_TGT_TX_FIFO_ERR = 1u << 18,
                   I_TGT_RX_FIFO_ERR = 1u << 19;

// TARGET_ID packing: ADDRESS0[6:0] | MASK0[13:7] | ADDRESS1[20:14] | MASK1[27:21].
constexpr uint32_t pack_target_id(uint8_t a0, uint8_t m0, uint8_t a1, uint8_t m1)
{
    return (uint32_t(a0) & 0x7Fu) | ((uint32_t(m0) & 0x7Fu) << 7) |
           ((uint32_t(a1) & 0x7Fu) << 14) | ((uint32_t(m1) & 0x7Fu) << 21);
}

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

    // Delay-aware access. read32/write32 above feed the annotated delay into
    // the quantum keeper, which is realistic but makes the value unobservable;
    // this one bypasses the keeper and hands back the final delay so a test can
    // assert it. Returning the final value rather than a delta means a target
    // that overwrites the caller's delay instead of accumulating onto it shows
    // up as a mismatch instead of underflowing sc_time.
    tlm::tlm_response_status timed(tlm::tlm_command cmd, uint64_t addr,
                                   unsigned len, uint8_t* ptr,
                                   unsigned streaming_width,
                                   sc_time incoming, sc_time& final_delay) {
        tlm::tlm_generic_payload gp;
        final_delay = incoming;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(ptr);
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, final_delay);
        return gp.get_response_status();
    }

    // Lets a caller bend data pointer, streaming width and byte enables
    // independently, to prove the target refuses malformed shapes.
    tlm::tlm_response_status shaped(tlm::tlm_command cmd, uint64_t addr,
                                    unsigned len, uint8_t* ptr,
                                    unsigned streaming_width,
                                    unsigned char* be = nullptr,
                                    unsigned be_len = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(ptr);
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width);
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
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

    // Full-control access for DMI-allowed / extension / response oracles.
    void xfer(tlm::tlm_command cmd, uint64_t addr, uint32_t& data,
              sc_time& delay, tlm::tlm_generic_payload& gp,
              smc::smc_axi_extension* ext = nullptr) {
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (ext) gp.set_extension(ext);
        sock->b_transport(gp, delay);
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
    // When set, replaces slave_mem for Controller-Mode reads (short/exact/overlong).
    std::function<std::vector<uint8_t>(const smc::i2c_xfer&)> read_override;

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

        if (read_override) {
            req.read_data = read_override(req);
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
    // Finding 13: irq_o starts as X until the first recompute; suppress only
    // the expected SC_ID_LOGIC_X_TO_BOOL_ around that first settle, then restore.
    // ----------------------------------------------------------------------
    {
        const sc_core::sc_actions prev_x =
            sc_core::sc_report_handler::set_actions(
                sc_core::SC_ID_LOGIC_X_TO_BOOL_, sc_core::SC_DO_NOTHING);
        pulse_reset();
        sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                                prev_x);
    }
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
    EXPECT_EQ(0u, drv.read32(HOST_FIFO_STATUS) & 0xFFFu); // FMTLVL architectural
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
    EXPECT_EQ(3u, (drv.read32(HOST_FIFO_STATUS) >> 16) & 0xFFFu); // RXLVL
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
    EXPECT_EQ(8u, (drv.read32(HOST_FIFO_STATUS) >> 16) & 0xFFFu);
    EXPECT_TRUE((drv.read32(INTR_STATE) & I_RX_OVERFLOW) != 0);
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
    EXPECT_EQ(2u, drv.read32(HOST_FIFO_STATUS) & 0xFFFu);
    drv.write32(FIFO_CTRL, 1u << 1);                // FMTRST
    EXPECT_EQ(0u, dut.dbg_fmt_count());
    EXPECT_EQ(0u, drv.read32(HOST_FIFO_STATUS) & 0xFFFu);
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
    // Finding 3: target_write/target_read are external-I2C stimulus adapters;
    // they bypass TLM ingress and do not prove bus-protocol timing/sideband.
    // Kept as adapter coverage — not an integrated bus-model path.
    // ----------------------------------------------------------------------
    pulse_reset();
    drv.write32(CTRL, C_ENABLETARGET);
    drv.write32(TARGET_ID, 0x50u | (0x7Fu << 7));   // ADDRESS0=0x50, MASK0=0x7F
    EXPECT_TRUE(dut.target_write(0x50, {0xDE, 0xAD}));
    settle();
    EXPECT_EQ(4u, dut.dbg_acq_count());             // Start + 2 data + Stop
    EXPECT_EQ(4u, (drv.read32(TARGET_FIFO_STATUS) >> 16) & 0xFFFu); // ACQLVL
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
    std::cout << "  [PASS] target write / ACQ FIFO (adapter)\n";

    // ----------------------------------------------------------------------
    // 12. Target-Mode READ: drains the Target TX FIFO. (Finding 3: adapter.)
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
    EXPECT_EQ(0u, drv.read32(TARGET_FIFO_STATUS) & 0xFFFu);
    std::cout << "  [PASS] target read / TX FIFO (adapter)\n";

    // ----------------------------------------------------------------------
    // 13. Target NACK count: unmatched address, saturating + read-clear.
    // Finding 3: adapter stimulus.
    // ----------------------------------------------------------------------
    EXPECT_TRUE(!dut.target_write(0x07, {0x01}));   // 0x07 != 0x50
    EXPECT_TRUE(!dut.target_read(0x07, 1, out));
    settle();
    EXPECT_EQ(2u, drv.read32(TARGET_NACK_COUNT));
    EXPECT_EQ(0u, drv.read32(TARGET_NACK_COUNT));   // rclr: cleared on read
    std::cout << "  [PASS] target NACK count (adapter)\n";

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
    // 17. transport_dbg: reads are side-effect-free; writes call reg_write.
    // A8 (INTERNAL_REVIEW_OPEN_QUESTIONS): whether debug writes should be
    // side-effect free is open — assert the current side-effecting policy.
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
        EXPECT_EQ(4u, drv.dbg_write(TARGET_ID, 0x1234)); // storage dbg write
        EXPECT_EQ(0x1234u & 0x0FFFFFFFu, dut.dbg_reg(TARGET_ID));
    }
    // A8: a debug write that clears TARGET_EVENTS must actually clear them.
    EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 3)) != 0);
    EXPECT_EQ(4u, drv.dbg_write(TARGET_EVENTS, 0x1Fu));
    settle();
    EXPECT_EQ(0u, drv.read32(TARGET_EVENTS));
    // A8: a debug write that pushes FDATA (host enabled) schedules a transfer.
    pulse_reset();
    drv.write32(CTRL, C_ENABLEHOST);
    slave_will_ack = true; bus_calls = 0;
    EXPECT_EQ(4u, drv.dbg_write(FDATA, (uint32_t(0x41 << 1)) | F_START));
    EXPECT_EQ(4u, drv.dbg_write(FDATA, 0x99u | F_STOP));
    step();
    EXPECT_EQ(1u, bus_calls);
    EXPECT_EQ(0x41u, bus_last_addr);
    // A null debug buffer must not reach memcpy.
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(TIMING0);
        gp.set_data_ptr(nullptr);
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        EXPECT_EQ(0u, drv.sock->transport_dbg(gp));
        gp.set_address(smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL);
        EXPECT_EQ(0u, wrap_drv.sock->transport_dbg(gp));
    }
    std::cout << "  [PASS] transport_dbg read peek + write side effects (A8)\n";

    // ----------------------------------------------------------------------
    // 17b. Annotated access delay, main and wrapper targets.
    //
    // Both models add access_delay_ns on the success path only and return
    // early on every error, so both halves are asserted. Starting from a
    // non-zero incoming delay is the point: it separates "accumulated onto"
    // from "overwrote". Runs before the CCI block below, which mutates the
    // main target's delay to 9 ns.
    // ----------------------------------------------------------------------
    {
        const sc_time base(123, SC_NS);
        const sc_time main_acc(5, SC_NS);  // tb.i2c.access_delay_ns preset
        const sc_time wrap_acc(2, SC_NS);  // tb.i2c_ctrl default, no preset
        uint32_t data = 0;
        auto*    p    = reinterpret_cast<uint8_t*>(&data);
        sc_time  got;

        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  drv.timed(tlm::TLM_WRITE_COMMAND, TIMING0, 4, p, 4, base, got));
        EXPECT_TRUE(got == base + main_acc);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  drv.timed(tlm::TLM_READ_COMMAND, TIMING0, 4, p, 4, base, got));
        EXPECT_TRUE(got == base + main_acc);

        EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                  wrap_drv.timed(tlm::TLM_READ_COMMAND,
                                 smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL, 4, p, 4,
                                 base, got));
        EXPECT_TRUE(got == base + wrap_acc);

        // Errors cost nothing.
        struct { const char* what; tlm::tlm_command cmd; uint64_t addr;
                 unsigned len; } bad[] = {
            {"ignore cmd",  tlm::TLM_IGNORE_COMMAND, TIMING0, 4},
            {"bad length",  tlm::TLM_READ_COMMAND,   TIMING0, 2},
            {"unaligned",   tlm::TLM_READ_COMMAND,   TIMING0 + 1, 4},
            {"past window", tlm::TLM_READ_COMMAND,
                            smc::i2c_controller_cfg::WINDOW_SIZE, 4},
        };
        for (const auto& b : bad) {
            const auto st = drv.timed(b.cmd, b.addr, b.len, p, b.len, base, got);
            EXPECT_TRUE(st != tlm::TLM_OK_RESPONSE);
            if (!(got == base))
                std::cerr << "FAIL  " << b.what
                          << " changed the annotated delay\n";
            EXPECT_TRUE(got == base);
        }
        std::cout << "  [PASS] annotated delay accumulates; errors add none\n";
    }

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

        // The handle reporting 9.0 only proves the broker stored it. Confirm
        // the model actually charges the new value (main + wrapper).
        {
            uint32_t data = 0;
            sc_time  got;
            const sc_time base(31, SC_NS);
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.timed(tlm::TLM_READ_COMMAND, TIMING0, 4,
                                reinterpret_cast<uint8_t*>(&data), 4, base, got));
            EXPECT_TRUE(got == base + sc_time(9, SC_NS));

            auto h_wd = broker.get_param_handle("tb.i2c_ctrl.access_delay_ns");
            EXPECT_TRUE(h_wd.is_valid());
            // Before mutation: wrapper still at its construction default (2 ns).
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      wrap_drv.timed(tlm::TLM_READ_COMMAND,
                                     smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL, 4,
                                     reinterpret_cast<uint8_t*>(&data), 4,
                                     base, got));
            EXPECT_TRUE(got == base + sc_time(2, SC_NS));
            h_wd.set_cci_value(cci::cci_value(7.0));
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      wrap_drv.timed(tlm::TLM_READ_COMMAND,
                                     smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL, 4,
                                     reinterpret_cast<uint8_t*>(&data), 4,
                                     base, got));
            EXPECT_TRUE(got == base + sc_time(7, SC_NS));
        }

        std::cout << "  CCI parameters:\n";
        for (auto& h : broker.get_param_handles()) {
            std::cout << "    " << std::left << std::setw(30) << h.name()
                      << " = " << std::setw(8) << h.get_cci_value().to_json()
                      << (h.is_preset_value() ? " [preset]" : " [default]") << "\n";
        }
        std::cout << "  [PASS] CCI introspection + delay after mutation\n";
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

    // ----------------------------------------------------------------------
    // 19b. Wrapper target payload shape.
    //
    // The wrapper shares the main target's validation, so it must refuse the
    // same malformed shapes. A null pointer with a legal length and address
    // previously reached the memcpy in b_transport and segfaulted.
    // ----------------------------------------------------------------------
    {
        using C = smc::i2c_wrap_ctrl_cfg;
        uint32_t data = 0;
        auto*    p    = reinterpret_cast<uint8_t*>(&data);

        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_READ_COMMAND,  C::OFF_I2C_CTRL, 4, nullptr, 4));
        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_WRITE_COMMAND, C::OFF_I2C_CTRL, 4, nullptr, 4));

        unsigned char be[4] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED,
                               TLM_BYTE_ENABLED, TLM_BYTE_DISABLED};
        EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_WRITE_COMMAND, C::OFF_I2C_CTRL, 4, p, 4,
                                  be, sizeof(be)));

        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_READ_COMMAND, C::OFF_I2C_CTRL, 4, p, 2));
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_READ_COMMAND, C::OFF_I2C_CTRL, 4, p, 0));
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                  wrap_drv.shaped(tlm::TLM_IGNORE_COMMAND, C::OFF_I2C_CTRL, 4, p, 4));

        // The register content must survive every refusal.
        EXPECT_EQ(0u, wrap_drv.read32(C::OFF_I2C_CTRL));
        std::cout << "  [PASS] i2c_wrap_ctrl payload shape (null/BE/width/cmd)\n";
    }

    // ----------------------------------------------------------------------
    // 20. Read-length matrix (finding 8). Over-long enqueue is current policy
    // (A9 — do not clamp/reject in the model).
    // ----------------------------------------------------------------------
    {
        const unsigned rlen_fails = g_failures;
        constexpr unsigned kRxDepth = 8; // tb.i2c.rx_fifo_depth preset
        auto run_read = [&](unsigned fbyte, unsigned expect_rlen,
                            std::vector<uint8_t> cb_bytes,
                            unsigned expect_rx, bool expect_ovf) {
            pulse_reset();
            drv.write32(CTRL, C_ENABLEHOST);
            slave_will_ack = true; bus_calls = 0;
            read_override = [cb_bytes](const smc::i2c_xfer&) { return cb_bytes; };
            drv.write32(FDATA, (uint32_t(0x10 << 1) | 1u) | F_START);
            drv.write32(FDATA, fbyte | F_READB | F_STOP);
            step();
            EXPECT_EQ(1u, bus_calls);
            EXPECT_EQ(expect_rlen, bus_last_rlen);
            EXPECT_EQ(expect_rx, (drv.read32(HOST_FIFO_STATUS) >> 16) & 0xFFFu);
            EXPECT_EQ(expect_rx, dut.dbg_rx_count());
            const bool ovf = (drv.read32(INTR_STATE) & I_RX_OVERFLOW) != 0;
            EXPECT_EQ(expect_ovf ? 1u : 0u, ovf ? 1u : 0u);
            for (unsigned i = 0; i < expect_rx; ++i)
                EXPECT_EQ(cb_bytes[i], static_cast<uint8_t>(drv.read32(RDATA)));
            read_override = nullptr;
        };

        // READB byte 0 → requested length 256; short callback.
        run_read(0, 256, {0xA1, 0xA2, 0xA3}, 3, false);
        // Length 1, exact callback.
        run_read(1, 1, {0xB1}, 1, false);
        // FIFO depth, exact.
        run_read(kRxDepth, kRxDepth,
                 std::vector<uint8_t>(kRxDepth, 0xC0), kRxDepth, false);
        // depth+1 exact → overflow, depth retained.
        {
            std::vector<uint8_t> nine(kRxDepth + 1, 0xD0);
            run_read(kRxDepth + 1, kRxDepth + 1, nine, kRxDepth, true);
        }
        // 256 exact into depth-8 → overflow.
        {
            std::vector<uint8_t> many(256, 0xE0);
            run_read(0, 256, many, kRxDepth, true);
        }
        // A9: over-long callback enqueues past requested length (no clamp).
        pulse_reset();
        drv.write32(CTRL, C_ENABLEHOST);
        slave_will_ack = true; bus_calls = 0;
        read_override = [](const smc::i2c_xfer&) {
            return std::vector<uint8_t>{0x11, 0x22, 0x33, 0x44}; // req was 2
        };
        drv.write32(FDATA, (uint32_t(0x10 << 1) | 1u) | F_START);
        drv.write32(FDATA, 2u | F_READB | F_STOP);
        step();
        EXPECT_EQ(2u, bus_last_rlen);
        EXPECT_EQ(4u, (drv.read32(HOST_FIFO_STATUS) >> 16) & 0xFFFu); // A9
        EXPECT_EQ(0x11u, drv.read32(RDATA));
        EXPECT_EQ(0x22u, drv.read32(RDATA));
        EXPECT_EQ(0x33u, drv.read32(RDATA));
        EXPECT_EQ(0x44u, drv.read32(RDATA));
        read_override = nullptr;
        if (g_failures == rlen_fails) {
            std::cout << "  [PASS] read-length matrix + A9 overlong enqueue\n";
        }
    }

    // ----------------------------------------------------------------------
    // 21. Target address match matrix (finding 9). Finding 3: adapter path.
    // ----------------------------------------------------------------------
    {
        auto acq_byte = [&]() -> uint32_t {
            return drv.read32(ACQDATA);
        };
        auto expect_match_write = [&](uint8_t addr, const std::vector<uint8_t>& d,
                                      bool stop) {
            drv.write32(FIFO_CTRL, (1u << 7) | (1u << 8)); // ACQRST|TXRST
            drv.write32(TARGET_EVENTS, 0x1F);
            settle();
            EXPECT_TRUE(dut.target_write(addr, d, stop));
            settle();
            const uint32_t start = acq_byte();
            EXPECT_EQ(uint32_t(addr << 1), start & 0xFFu);
            EXPECT_EQ(1u, (start >> 8) & 0x7u);
            for (uint8_t b : d) EXPECT_EQ(uint32_t(b), acq_byte() & 0xFFu);
            if (stop) {
                EXPECT_EQ(2u, (acq_byte() >> 8) & 0x7u);
                EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 4)) != 0);
            } else {
                EXPECT_EQ(0u, drv.read32(TARGET_EVENTS) & (1u << 4));
            }
            EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 3)) != 0);
        };
        auto expect_nack = [&](uint8_t addr) {
            (void)drv.read32(TARGET_NACK_COUNT); // clear
            EXPECT_TRUE(!dut.target_write(addr, {0x01}));
            settle();
            EXPECT_EQ(1u, drv.read32(TARGET_NACK_COUNT));
        };

        // Slot 0 exact.
        pulse_reset();
        drv.write32(CTRL, C_ENABLETARGET);
        drv.write32(TARGET_ID, pack_target_id(0x50, 0x7F, 0, 0));
        expect_match_write(0x50, {0xAA}, true);

        // Slot 1 alone.
        drv.write32(TARGET_ID, pack_target_id(0, 0, 0x22, 0x7F));
        expect_match_write(0x22, {0xBB}, true);
        expect_nack(0x50);

        // Masked alias (MASK0=0x70): 0x55 matches 0x50 under mask; 0x60 near-miss.
        drv.write32(TARGET_ID, pack_target_id(0x50, 0x70, 0, 0));
        expect_match_write(0x55, {0xCC}, true);
        expect_nack(0x60);

        // Both enabled with overlap on 0x33.
        drv.write32(TARGET_ID, pack_target_id(0x33, 0x7F, 0x33, 0x7F));
        expect_match_write(0x33, {0xDD}, true);

        // Both masks zero → matching disabled.
        drv.write32(TARGET_ID, pack_target_id(0x50, 0, 0x22, 0));
        expect_nack(0x50);
        expect_nack(0x22);

        // 7-bit extremes.
        drv.write32(TARGET_ID, pack_target_id(0x00, 0x7F, 0x7F, 0x7F));
        expect_match_write(0x00, {0x01}, true);
        expect_match_write(0x7F, {0x02}, true);

        // Target disabled.
        drv.write32(CTRL, 0);
        (void)drv.read32(TARGET_NACK_COUNT);
        EXPECT_TRUE(!dut.target_write(0x00, {0x01}));
        settle();
        EXPECT_EQ(1u, drv.read32(TARGET_NACK_COUNT));

        // No-STOP transfer: START_DETECT, no STOP_DETECT, no Stop ACQ entry.
        drv.write32(CTRL, C_ENABLETARGET);
        drv.write32(TARGET_ID, pack_target_id(0x40, 0x7F, 0, 0));
        drv.write32(FIFO_CTRL, (1u << 7) | (1u << 8));
        drv.write32(TARGET_EVENTS, 0x1F);
        settle();
        EXPECT_TRUE(dut.target_write(0x40, {0xEE, 0xFF}, false));
        settle();
        EXPECT_EQ(3u, (drv.read32(TARGET_FIFO_STATUS) >> 16) & 0xFFFu); // S+2data
        EXPECT_TRUE((drv.read32(TARGET_EVENTS) & (1u << 3)) != 0);
        EXPECT_EQ(0u, drv.read32(TARGET_EVENTS) & (1u << 4));
        EXPECT_EQ(uint32_t(0x40 << 1), drv.read32(ACQDATA) & 0xFFu);
        EXPECT_EQ(0xEEu, drv.read32(ACQDATA) & 0xFFu);
        EXPECT_EQ(0xFFu, drv.read32(ACQDATA) & 0xFFu);
        EXPECT_EQ(0u, (drv.read32(TARGET_FIFO_STATUS) >> 16) & 0xFFFu);
        std::cout << "  [PASS] target address match matrix (adapter)\n";
    }

    // ----------------------------------------------------------------------
    // 22. Interrupt branches (finding 10): W1C, levels, thresholds, enable.
    // ----------------------------------------------------------------------
    {
        // Each W1C source cleared independently via INTR_TEST + INTR_STATE.
        pulse_reset();
        const uint32_t w1c_bits[] = {
            I_RX_OVERFLOW, I_SCL_INTERFERENCE, I_SDA_INTERFERENCE,
            I_STRETCH_TIMEOUT, I_SDA_UNSTABLE, I_CMD_COMPLETE, I_UNEXP_STOP,
            I_HOST_TIMEOUT, I_SMBALERT, I_CTRL_TX_FIFO_ERR, I_CTRL_RX_FIFO_ERR,
            I_TGT_TX_FIFO_ERR, I_TGT_RX_FIFO_ERR};
        uint32_t all = 0;
        for (uint32_t b : w1c_bits) all |= b;
        drv.write32(INTR_TEST, all);
        settle();
        EXPECT_EQ(all, drv.read32(INTR_STATE) & all);
        for (uint32_t b : w1c_bits) {
            drv.write32(INTR_STATE, b); // W1C one bit
            settle();
            EXPECT_EQ(0u, drv.read32(INTR_STATE) & b);
            // Remaining previously-set bits that were not yet cleared stay set
            // only if still latched — clear-one leaves others until their turn.
        }
        // Re-force two bits; clear one; the other sticks.
        drv.write32(INTR_TEST, I_SMBALERT | I_UNEXP_STOP);
        settle();
        drv.write32(INTR_STATE, I_SMBALERT);
        settle();
        EXPECT_EQ(0u, drv.read32(INTR_STATE) & I_SMBALERT);
        EXPECT_TRUE((drv.read32(INTR_STATE) & I_UNEXP_STOP) != 0);
        drv.write32(INTR_STATE, I_UNEXP_STOP);

        // W1C write to a level bit does not stick / does not clear the level.
        pulse_reset();
        drv.write32(HOST_FIFO_CONFIG, (4u << 16)); // FMT_THRESH=4 → level on
        settle();
        EXPECT_TRUE((drv.read32(INTR_STATE) & I_FMT_THRESHOLD) != 0);
        drv.write32(INTR_STATE, I_FMT_THRESHOLD); // W1C on level bit: no-op
        settle();
        EXPECT_TRUE((drv.read32(INTR_STATE) & I_FMT_THRESHOLD) != 0);

        // FMT threshold: equality and one either side (assert when size < thresh).
        auto check_fmt = [&](unsigned thresh, unsigned queued, bool expect_on) {
            pulse_reset();
            drv.write32(HOST_FIFO_CONFIG, (thresh << 16));
            for (unsigned i = 0; i < queued; ++i) drv.write32(FDATA, 0x10 + i);
            settle();
            EXPECT_EQ(queued, drv.read32(HOST_FIFO_STATUS) & 0xFFFu);
            const bool on = (drv.read32(INTR_STATE) & I_FMT_THRESHOLD) != 0;
            EXPECT_EQ(expect_on ? 1u : 0u, on ? 1u : 0u);
        };
        check_fmt(2, 1, true);   // 1 < 2
        check_fmt(2, 2, false);  // 2 == 2
        check_fmt(2, 3, false);  // 3 > 2

        // RX threshold: on when size > thresh.
        auto check_rx = [&](unsigned thresh, unsigned n, bool expect_on) {
            pulse_reset();
            drv.write32(CTRL, C_ENABLEHOST);
            drv.write32(HOST_FIFO_CONFIG, thresh);
            slave_will_ack = true;
            read_override = [n](const smc::i2c_xfer&) {
                return std::vector<uint8_t>(n, 0x5A);
            };
            drv.write32(FDATA, (uint32_t(0x11 << 1) | 1u) | F_START);
            drv.write32(FDATA, n | F_READB | F_STOP);
            step();
            EXPECT_EQ(n, (drv.read32(HOST_FIFO_STATUS) >> 16) & 0xFFFu);
            const bool on = (drv.read32(INTR_STATE) & I_RX_THRESHOLD) != 0;
            EXPECT_EQ(expect_on ? 1u : 0u, on ? 1u : 0u);
            read_override = nullptr;
        };
        check_rx(2, 1, false); // 1 < 2
        check_rx(2, 2, false); // 2 == 2
        check_rx(2, 3, true);  // 3 > 2

        // ACQ threshold: on when size > thresh. (adapter)
        auto check_acq = [&](unsigned thresh, unsigned nbytes, bool expect_on) {
            pulse_reset();
            drv.write32(CTRL, C_ENABLETARGET);
            drv.write32(TARGET_ID, pack_target_id(0x50, 0x7F, 0, 0));
            drv.write32(TARGET_FIFO_CONFIG, (thresh << 16));
            std::vector<uint8_t> payload(nbytes, 0x01);
            EXPECT_TRUE(dut.target_write(0x50, payload)); // entries = 1+nbytes+1
            settle();
            const unsigned lvl = (drv.read32(TARGET_FIFO_STATUS) >> 16) & 0xFFFu;
            const bool on = (drv.read32(INTR_STATE) & I_ACQ_THRESHOLD) != 0;
            EXPECT_EQ(expect_on ? 1u : 0u, on ? 1u : 0u);
            (void)lvl;
        };
        // thresh=3: Start+1data+Stop = 3 → not on; +1 data = 4 → on.
        check_acq(3, 1, false);
        check_acq(3, 2, true);

        // TX threshold: on when size < thresh.
        auto check_tx = [&](unsigned thresh, unsigned queued, bool expect_on) {
            pulse_reset();
            drv.write32(TARGET_FIFO_CONFIG, thresh);
            for (unsigned i = 0; i < queued; ++i) drv.write32(TXDATA, i);
            settle();
            EXPECT_EQ(queued, drv.read32(TARGET_FIFO_STATUS) & 0xFFFu);
            const bool on = (drv.read32(INTR_STATE) & I_TX_THRESHOLD) != 0;
            EXPECT_EQ(expect_on ? 1u : 0u, on ? 1u : 0u);
        };
        check_tx(2, 1, true);
        check_tx(2, 2, false);
        check_tx(2, 3, false);

        // TARGET_NACK_COUNT saturation at 0xFF.
        pulse_reset();
        drv.write32(CTRL, C_ENABLETARGET);
        drv.write32(TARGET_ID, pack_target_id(0x50, 0x7F, 0, 0));
        drv.write32(TARGET_NACK_COUNT, 0xFEu);
        EXPECT_TRUE(!dut.target_write(0x01, {0x00})); // → 0xFF
        settle();
        EXPECT_TRUE(!dut.target_write(0x01, {0x00})); // stays 0xFF
        settle();
        EXPECT_EQ(0xFFu, drv.read32(TARGET_NACK_COUNT));

        // Enable while status already pending → irq_o; disable drops irq_o,
        // sticky status remains.
        pulse_reset();
        drv.write32(INTR_TEST, I_SMBALERT);
        settle();
        EXPECT_TRUE((drv.read32(INTR_STATE) & I_SMBALERT) != 0);
        EXPECT_EQ(false, irq.read());
        drv.write32(INTR_ENABLE, I_SMBALERT);
        settle();
        EXPECT_EQ(true, irq.read());
        drv.write32(INTR_ENABLE, 0);
        settle();
        EXPECT_EQ(false, irq.read());
        EXPECT_TRUE((drv.read32(INTR_STATE) & I_SMBALERT) != 0);

        std::cout << "  [PASS] interrupt W1C / thresholds / enable-pending\n";
    }

    // ----------------------------------------------------------------------
    // 23. Reset while transfer scheduled + while IRQ/FIFO non-empty (finding 7).
    // ----------------------------------------------------------------------
    {
        pulse_reset();
        drv.write32(CTRL, C_ENABLEHOST);
        drv.write32(INTR_ENABLE, I_CMD_COMPLETE);
        slave_will_ack = true; bus_calls = 0;
        // Queue a transfer; xfer_delay_ns preset = 20 ns.
        drv.write32(FDATA, (uint32_t(0x60 << 1)) | F_START);
        drv.write32(FDATA, 0x55 | F_STOP);
        sc_core::wait(1, SC_NS); // scheduled, not yet expired
        EXPECT_EQ(0u, bus_calls);
        // Also leave IRQ path and a non-empty RX path primed after a prior xfer:
        // fill FMT occupancy observable via HOST_FIFO_STATUS before reset.
        EXPECT_TRUE((drv.read32(HOST_FIFO_STATUS) & 0xFFFu) >= 1u);

        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
        settle();
        const unsigned calls_after_reset = bus_calls;
        sc_core::wait(100, SC_NS); // well past the cancelled xfer
        settle();
        EXPECT_EQ(calls_after_reset, bus_calls); // no callback after reset

        // Reset register image.
        EXPECT_EQ(0x00u, drv.read32(INTR_STATE));
        EXPECT_EQ(0x00u, drv.read32(INTR_ENABLE));
        EXPECT_EQ(0x00u, drv.read32(CTRL));
        EXPECT_EQ(0u, drv.read32(HOST_FIFO_STATUS));
        EXPECT_EQ(0u, drv.read32(TARGET_FIFO_STATUS));
        EXPECT_EQ(false, irq.read());
        {   // STATUS empty/idle bits
            const uint32_t st = drv.read32(STATUS);
            EXPECT_EQ((1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<8)|(1u<<9), st);
        }

        // Reset while IRQ asserted and FIFO non-empty.
        pulse_reset();
        drv.write32(CTRL, C_ENABLEHOST);
        drv.write32(INTR_ENABLE, I_CMD_COMPLETE);
        slave_will_ack = true; bus_calls = 0;
        read_override = [](const smc::i2c_xfer&) {
            return std::vector<uint8_t>{0x10, 0x20, 0x30};
        };
        drv.write32(FDATA, (uint32_t(0x61 << 1) | 1u) | F_START);
        drv.write32(FDATA, 3u | F_READB | F_STOP);
        step();
        EXPECT_EQ(true, irq.read());
        EXPECT_TRUE((drv.read32(HOST_FIFO_STATUS) >> 16) != 0);
        bus_calls = 0;
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
        settle();
        EXPECT_EQ(0u, bus_calls);
        EXPECT_EQ(false, irq.read());
        EXPECT_EQ(0u, drv.read32(HOST_FIFO_STATUS));
        EXPECT_EQ(0x00u, drv.read32(INTR_STATE));
        read_override = nullptr;
        std::cout << "  [PASS] reset cancels scheduled xfer + clears IRQ/FIFO\n";
    }

    // ----------------------------------------------------------------------
    // 24. DMI denied (finding 15) + stale dmi_allowed behaviour.
    // ----------------------------------------------------------------------
    {
        const auto dmi_main = simtlm::dmi_request(drv.sock, TIMING0);
        EXPECT_TRUE(!dmi_main.granted);
        const auto dmi_wrap =
            simtlm::dmi_request(wrap_drv.sock, smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL);
        EXPECT_TRUE(!dmi_wrap.granted);

        // Main success path clears a stale dmi_allowed=true.
        {
            tlm::tlm_generic_payload gp;
            uint32_t data = 0;
            sc_time t = SC_ZERO_TIME;
            gp.set_dmi_allowed(true);
            drv.xfer(tlm::TLM_READ_COMMAND, TIMING0, data, t, gp);
            EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
            EXPECT_TRUE(!gp.is_dmi_allowed());
        }
        // Error returns clear the hint as well. DMI is not granted.
        {
            tlm::tlm_generic_payload gp;
            uint32_t data = 0;
            sc_time t = SC_ZERO_TIME;
            gp.set_dmi_allowed(true);
            drv.xfer(tlm::TLM_READ_COMMAND, TIMING0 + 1, data, t, gp); // unaligned
            EXPECT_TRUE(gp.get_response_status() != tlm::TLM_OK_RESPONSE);
            EXPECT_TRUE(!gp.is_dmi_allowed());
        }
        {
            tlm::tlm_generic_payload gp;
            uint32_t data = 0;
            sc_time t = SC_ZERO_TIME;
            gp.set_dmi_allowed(true);
            wrap_drv.xfer(tlm::TLM_READ_COMMAND,
                          smc::i2c_wrap_ctrl_cfg::OFF_I2C_CTRL, data, t, gp);
            EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
            EXPECT_TRUE(!gp.is_dmi_allowed());
        }
        std::cout << "  [PASS] DMI denied; dmi_allowed policy as implemented\n";
    }

    // ----------------------------------------------------------------------
    // 25. B2: AXI extension attached; model leaves fields unchanged.
    // ----------------------------------------------------------------------
    {
        smc::smc_axi_extension ext;
        ext.source_id = smc::SEP_ID;
        ext.axi_id    = 0xAB;
        ext.set_priv(false);
        ext.set_secure(true);
        ext.set_fetch(true);
        ext.set_locked(true);
        ext.axi_user  = 0x5A;

        tlm::tlm_generic_payload gp;
        uint32_t data = 0x12345678u;
        sc_time t = SC_ZERO_TIME;
        gp.set_dmi_allowed(false);
        drv.xfer(tlm::TLM_WRITE_COMMAND, TIMING0, data, t, gp, &ext);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        smc::smc_axi_extension* got = nullptr;
        gp.get_extension(got);
        EXPECT_TRUE(got != nullptr);
        EXPECT_EQ(uint16_t(smc::SEP_ID), got->source_id);
        EXPECT_EQ(uint16_t(0xAB), got->axi_id);
        EXPECT_TRUE(got->is_user);
        EXPECT_TRUE(got->is_secure);
        EXPECT_TRUE(got->is_fetch);
        EXPECT_TRUE(got->is_locked);
        EXPECT_EQ(uint8_t(0x5A), got->axi_user);
        gp.clear_extension<smc::smc_axi_extension>();
        std::cout << "  [PASS] AXI extension preserved (B2: no auth)\n";
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
    // Finding 13: no global SC_ID_LOGIC_X_TO_BOOL_ suppress — see tb::run §1.
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
