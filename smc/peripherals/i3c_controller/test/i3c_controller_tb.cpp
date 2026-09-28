// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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

// EXPECT_EQ with a caller-supplied context string, so a failure inside a sweep
// names the register / shape it came from.
#define EXPECT_EQ_CTX(expected, actual, ctx)                                   \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " << (ctx) << ": expected=" << +_e                 \
                      << " actual=" << +_a << "\n";                            \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE_CTX(cond, ctx)                                             \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " << (ctx) << ": expected TRUE: " #cond "\n";      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Independent CSR manifest
//
// A second transcription of the I3CCSR map from the register table in
// include/i3c_controller.h and memmap.adoc: what each register reads after
// reset, and what it reads back after software writes all-ones.  It is
// deliberately not derived from the model's switch statements, so a slip on
// either side fails the sweep.
//
// Registers whose read value depends on live queue levels (PIO_INTR_STATUS)
// or that pop a FIFO (COMMAND/RESPONSE/XFER_DATA/IBI ports) are excluded here
// and covered by the dedicated threshold and transaction sections.
// ---------------------------------------------------------------------------
struct csr_spec {
    uint64_t    off;
    const char* name;
    uint32_t    reset;       ///< value read after reset
    uint32_t    after_ones;  ///< value read after writing 0xFFFFFFFF
};

// HC_CONTROL: WMASK = BUS_ENABLE|RESUME|ABORT|HALT_ON_TO|HOT_JOIN|I2C_DEV|IBA.
// RESUME and ABORT are self-clearing, and MODE_SELECTOR always reads 1.
constexpr uint32_t kHcCtrlWmask   = (1u << 31) | (1u << 30) | (1u << 29) |
                                    (1u << 12) | (1u << 8) | (1u << 7) | 1u;
constexpr uint32_t kHcCtrlAfterOnes =
    (kHcCtrlWmask & ~((1u << 30) | (1u << 29))) | (1u << 6);

constexpr csr_spec kCsrs[] = {
    {cfg_t::HCI_VERSION,            "HCI_VERSION",            0x0000'0120u, 0x0000'0120u},
    {cfg_t::HC_CONTROL,             "HC_CONTROL",             0x0000'0040u, kHcCtrlAfterOnes},
    {cfg_t::CONTROLLER_DEVICE_ADDR, "CONTROLLER_DEVICE_ADDR", 0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::RESET_CONTROL,          "RESET_CONTROL",          0x0000'0000u, 0x0000'0000u},
    {cfg_t::INTR_STATUS_ENABLE,     "INTR_STATUS_ENABLE",     0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::INTR_SIGNAL_ENABLE,     "INTR_SIGNAL_ENABLE",     0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::INTR_FORCE,             "INTR_FORCE",             0x0000'0000u, 0x0000'0000u},
    {cfg_t::PIO_SECTION_OFFSET,     "PIO_SECTION_OFFSET",     0x0000'0080u, 0x0000'0080u},
    {cfg_t::QUEUE_THLD_CTRL,        "QUEUE_THLD_CTRL",        0x0101'0101u, 0xFFFF'FFFFu},
    {cfg_t::DATA_BUFFER_THLD_CTRL,  "DATA_BUFFER_THLD_CTRL",  0x0101'0101u, 0xFFFF'FFFFu},
    {cfg_t::ALT_QUEUE_SIZE,         "ALT_QUEUE_SIZE",         0x0000'0000u, 0x0000'0000u},
    {cfg_t::PIO_INTR_STATUS_ENABLE, "PIO_INTR_STATUS_ENABLE", 0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::PIO_INTR_SIGNAL_ENABLE, "PIO_INTR_SIGNAL_ENABLE", 0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::PIO_CONTROL,            "PIO_CONTROL",            0x0000'0003u, 0x0000'0007u},
    {cfg_t::STBY_CR_EXTCAP_HEADER,  "STBY_CR_EXTCAP_HEADER",  0x0000'0012u, 0x0000'0012u},
    // CR_REQUEST_SEND (bit 5) is self-clearing.
    {cfg_t::STBY_CR_CONTROL,        "STBY_CR_CONTROL",        0x0000'0000u, 0xFFFF'FFDFu},
    {cfg_t::STBY_CR_DEVICE_ADDR,    "STBY_CR_DEVICE_ADDR",    0x0000'0000u, 0xFFFF'FFFFu},
    {cfg_t::STBY_CR_CAPABILITIES,   "STBY_CR_CAPABILITIES",   0x0000'0000u, 0x0000'0000u},
};

/// Every 4-byte offset in an instance window that the model implements.
/// Everything else inside 0x000..0xFFF must be RAZ/WI.
bool is_mapped_offset(uint64_t loff)
{
    if (loff >= cfg_t::DAT_BASE && loff < cfg_t::DAT_END) return true;
    if (loff >= cfg_t::DCT_BASE && loff < cfg_t::DCT_END) return true;
    switch (loff) {
    case cfg_t::HCI_VERSION:            case cfg_t::HC_CONTROL:
    case cfg_t::CONTROLLER_DEVICE_ADDR: case cfg_t::HC_CAPABILITIES:
    case cfg_t::RESET_CONTROL:          case cfg_t::PRESENT_STATE:
    case cfg_t::INTR_STATUS:            case cfg_t::INTR_STATUS_ENABLE:
    case cfg_t::INTR_SIGNAL_ENABLE:     case cfg_t::INTR_FORCE:
    case cfg_t::DAT_SECTION_OFFSET:     case cfg_t::DCT_SECTION_OFFSET:
    case cfg_t::PIO_SECTION_OFFSET:     case cfg_t::COMMAND_PORT:
    case cfg_t::RESPONSE_PORT:          case cfg_t::XFER_DATA_PORT:
    case cfg_t::IBI_PORT:               case cfg_t::QUEUE_THLD_CTRL:
    case cfg_t::DATA_BUFFER_THLD_CTRL:  case cfg_t::QUEUE_SIZE:
    case cfg_t::ALT_QUEUE_SIZE:         case cfg_t::PIO_INTR_STATUS:
    case cfg_t::PIO_INTR_STATUS_ENABLE: case cfg_t::PIO_INTR_SIGNAL_ENABLE:
    case cfg_t::PIO_CONTROL:            case cfg_t::STBY_CR_EXTCAP_HEADER:
    case cfg_t::STBY_CR_CONTROL:        case cfg_t::STBY_CR_DEVICE_ADDR:
    case cfg_t::STBY_CR_CAPABILITIES:
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::i3c_controller dut;
    driver              drv;
    sc_core::sc_signal<bool> rst_n{"rst_n"};

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
        dut.rst_n_i(rst_n);
        rst_n.write(true);
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

    // Build an HCI regular-transfer DAT descriptor (TCRI 7.1.2.2).
    static uint64_t make_cmd(uint8_t tid, bool rnw, uint8_t devidx,
                             bool cp, uint8_t ccc, uint16_t len) {
        return (uint64_t(tid & 0xF) << 3) |
               (uint64_t(ccc) << 7) |
               (uint64_t(cp ? 1 : 0) << 15) |
               (uint64_t(devidx & 0x1F) << 16) |
               (uint64_t(rnw ? 1 : 0) << 29) |
               (uint64_t(len) << 48);
    }

    void run();

    // Architectural sections added for the internal-review remediation.
    void t_csr_manifest();
    void t_window_map();
    void t_cmd_attr();
    void t_timing();
    void t_intr_enable_matrix();
    void t_outputs_idle();
    void t_sideband();
    void t_length_matrix();
    void t_ibi_matrix();
    void t_read_data_policy();

    /// Reset the whole DUT and let outputs settle.
    void hard_reset() {
        rst_n.write(false);
        settle();
        rst_n.write(true);
        settle();
    }

    /// Non-destructive "is a response waiting?" oracle: the RESP_READY level
    /// bit follows resp_q depth, so it can be polled without popping.
    bool resp_pending(unsigned inst) {
        return (drv.read(A(inst, cfg_t::PIO_INTR_STATUS)) &
                (1u << smc::pio_intr::RESP_READY_STAT)) != 0;
    }
};

// ---------------------------------------------------------------------------
// Every CSR against the independent manifest
// ---------------------------------------------------------------------------
void tb::t_csr_manifest()
{
    // Instance 2 is otherwise unused, so the all-ones sweep cannot disturb the
    // transaction tests that run on instances 0 and 1.
    constexpr unsigned I = 2;
    hard_reset();

    for (const csr_spec& r : kCsrs) {
        EXPECT_EQ_CTX(r.reset, drv.read(A(I, r.off)),
                      std::string(r.name) + " reset");
    }
    // Computed read-only registers carry their configured values.
    EXPECT_EQ_CTX(uint32_t(0x300u | (32u << 12) | (2u << 19)),
                  drv.read(A(I, cfg_t::DAT_SECTION_OFFSET)),
                  "DAT_SECTION_OFFSET reset");
    EXPECT_EQ_CTX(uint32_t(0x400u | (16u << 12) | (4u << 19)),
                  drv.read(A(I, cfg_t::DCT_SECTION_OFFSET)) & 0x007F'FFFFu,
                  "DCT_SECTION_OFFSET reset");
    EXPECT_EQ_CTX(0u, drv.read(A(I, cfg_t::INTR_STATUS)), "INTR_STATUS reset");

    for (const csr_spec& r : kCsrs) {
        drv.write(A(I, r.off), 0xFFFF'FFFFu);
        EXPECT_EQ_CTX(r.after_ones, drv.read(A(I, r.off)),
                      std::string(r.name) + " after write-ones");
    }
    // Read-only registers ignore writes and keep their computed value.
    const uint32_t caps  = drv.read(A(I, cfg_t::HC_CAPABILITIES));
    const uint32_t qsize = drv.read(A(I, cfg_t::QUEUE_SIZE));
    drv.write(A(I, cfg_t::HC_CAPABILITIES), 0xFFFF'FFFFu);
    drv.write(A(I, cfg_t::QUEUE_SIZE), 0xFFFF'FFFFu);
    drv.write(A(I, cfg_t::PRESENT_STATE), 0xFFFF'FFFFu);
    drv.write(A(I, cfg_t::DAT_SECTION_OFFSET), 0xFFFF'FFFFu);
    EXPECT_EQ_CTX(caps, drv.read(A(I, cfg_t::HC_CAPABILITIES)), "HC_CAPABILITIES RO");
    EXPECT_EQ_CTX(qsize, drv.read(A(I, cfg_t::QUEUE_SIZE)), "QUEUE_SIZE RO");
    EXPECT_EQ_CTX(uint32_t(0x300u | (32u << 12) | (2u << 19)),
                  drv.read(A(I, cfg_t::DAT_SECTION_OFFSET)), "DAT_SECTION_OFFSET RO");
    // DCT_SECTION_OFFSET: only the ENTDAA index field [31:23] is writable; the
    // geometry fields below it stay at their configured values.
    const uint32_t dct_geom = drv.read(A(I, cfg_t::DCT_SECTION_OFFSET)) & 0x007F'FFFFu;
    drv.write(A(I, cfg_t::DCT_SECTION_OFFSET), 0xFFFF'FFFFu);
    EXPECT_EQ_CTX(0xFF80'0000u,
                  drv.read(A(I, cfg_t::DCT_SECTION_OFFSET)) & 0xFF80'0000u,
                  "DCT_SECTION_OFFSET ENTDAA index RW");
    EXPECT_EQ_CTX(dct_geom,
                  drv.read(A(I, cfg_t::DCT_SECTION_OFFSET)) & 0x007F'FFFFu,
                  "DCT_SECTION_OFFSET geometry RO");

    // INTR_STATUS is W1C and INTR_FORCE is write-1-to-set, for every bit of
    // the valid set and no others.
    hard_reset();
    for (unsigned b : {10u, 11u, 12u, 13u, 14u}) {
        std::string ctx = "INTR bit " + std::to_string(b);
        drv.write(A(I, cfg_t::INTR_FORCE), (1u << b));
        EXPECT_EQ_CTX((1u << b), drv.read(A(I, cfg_t::INTR_STATUS)), ctx + " force");
        drv.write(A(I, cfg_t::INTR_STATUS), ~(1u << b));   // W1C elsewhere
        EXPECT_EQ_CTX((1u << b), drv.read(A(I, cfg_t::INTR_STATUS)), ctx + " W1C other");
        drv.write(A(I, cfg_t::INTR_STATUS), (1u << b));
        EXPECT_EQ_CTX(0u, drv.read(A(I, cfg_t::INTR_STATUS)), ctx + " W1C self");
    }
    // Bits outside the valid set can never be forced.
    drv.write(A(I, cfg_t::INTR_FORCE), 0xFFFF'FFFFu);
    EXPECT_EQ_CTX(uint32_t(smc::hc_intr::W1C_MASK),
                  drv.read(A(I, cfg_t::INTR_STATUS)), "INTR_FORCE masks reserved");
    drv.write(A(I, cfg_t::INTR_STATUS), 0xFFFF'FFFFu);
    EXPECT_EQ_CTX(0u, drv.read(A(I, cfg_t::INTR_STATUS)), "INTR_STATUS W1C all");

    // The public snapshot back door must agree with the bus for every CSR it
    // claims to serve, without popping a FIFO or annotating delay.
    drv.write(A(I, cfg_t::CONTROLLER_DEVICE_ADDR), 0x1234'5678u);
    drv.write(A(I, cfg_t::QUEUE_THLD_CTRL), 0x0203'0405u);
    drv.write(A(I, cfg_t::DATA_BUFFER_THLD_CTRL), 0x0102'0304u);
    drv.write(A(I, cfg_t::DAT_BASE + 4), 0x5A5A'0000u);
    drv.write(A(I, cfg_t::DCT_BASE + 4), 0xA5A5'0000u);
    for (uint64_t off : {uint64_t(cfg_t::HCI_VERSION), uint64_t(cfg_t::HC_CONTROL),
                         uint64_t(cfg_t::CONTROLLER_DEVICE_ADDR),
                         uint64_t(cfg_t::HC_CAPABILITIES),
                         uint64_t(cfg_t::PRESENT_STATE),
                         uint64_t(cfg_t::INTR_STATUS_ENABLE),
                         uint64_t(cfg_t::INTR_SIGNAL_ENABLE),
                         uint64_t(cfg_t::DAT_SECTION_OFFSET),
                         uint64_t(cfg_t::DCT_SECTION_OFFSET),
                         uint64_t(cfg_t::PIO_SECTION_OFFSET),
                         uint64_t(cfg_t::QUEUE_THLD_CTRL),
                         uint64_t(cfg_t::DATA_BUFFER_THLD_CTRL),
                         uint64_t(cfg_t::QUEUE_SIZE),
                         uint64_t(cfg_t::PIO_INTR_STATUS_ENABLE),
                         uint64_t(cfg_t::PIO_INTR_SIGNAL_ENABLE),
                         uint64_t(cfg_t::PIO_CONTROL),
                         uint64_t(cfg_t::STBY_CR_EXTCAP_HEADER),
                         uint64_t(cfg_t::STBY_CR_CONTROL),
                         uint64_t(cfg_t::STBY_CR_DEVICE_ADDR),
                         uint64_t(cfg_t::DAT_BASE + 4),
                         uint64_t(cfg_t::DCT_BASE + 4)}) {
        EXPECT_EQ_CTX(drv.read(A(I, off)), dut.dbg_read(A(I, off)),
                      "dbg_read agrees with bus @0x" + std::to_string(off));
    }
    // Out of aperture and an unmapped hole both snapshot as zero.
    EXPECT_EQ_CTX(0u, dut.dbg_read(uint64_t(N) * cfg_t::INSTANCE_SPACING),
                  "dbg_read out of aperture");
    EXPECT_EQ_CTX(0u, dut.dbg_read(A(I, 0x18)), "dbg_read hole");
    // The CCI-resolved instance count is what the bench configured.
    EXPECT_EQ_CTX(unsigned(N), dut.num_instances(), "num_instances()");

    hard_reset();
    std::cout << "  [PASS] every CSR: reset, write-mask, RO and W1C contract\n";
}

// ---------------------------------------------------------------------------
// Instance window map: holes, table bounds, instance boundaries
// ---------------------------------------------------------------------------
void tb::t_window_map()
{
    constexpr unsigned I = 2;
    hard_reset();

    // Every unmapped word in the instance window is RAZ/WI and does not
    // disturb a neighbouring register.
    unsigned holes = 0;
    for (uint64_t loff = 0; loff < cfg_t::INSTANCE_SPACING; loff += 4) {
        if (is_mapped_offset(loff)) continue;
        ++holes;
        std::string ctx = "hole @0x" + std::to_string(loff);
        EXPECT_EQ_CTX(0u, drv.read(A(I, loff)), ctx + " RAZ");
        drv.write(A(I, loff), 0xFFFF'FFFFu);
        EXPECT_EQ_CTX(0u, drv.read(A(I, loff)), ctx + " WI");
    }
    EXPECT_TRUE(holes > 0);
    // The sweep must not have perturbed any real register.
    for (const csr_spec& r : kCsrs) {
        EXPECT_EQ_CTX(r.reset, drv.read(A(I, r.off)),
                      std::string(r.name) + " after hole sweep");
    }
    std::cout << "  [PASS] " << holes
              << " reserved words are RAZ/WI and non-disturbing\n";

    // DAT/DCT first and last words, and the word just past each table.
    drv.write(A(I, cfg_t::DAT_BASE), 0x1111'1111u);
    drv.write(A(I, cfg_t::DAT_END - 4), 0x2222'2222u);
    drv.write(A(I, cfg_t::DCT_BASE), 0x3333'3333u);
    drv.write(A(I, cfg_t::DCT_END - 4), 0x4444'4444u);
    EXPECT_EQ_CTX(0x1111'1111u, drv.read(A(I, cfg_t::DAT_BASE)), "DAT first");
    EXPECT_EQ_CTX(0x2222'2222u, drv.read(A(I, cfg_t::DAT_END - 4)), "DAT last");
    EXPECT_EQ_CTX(0x3333'3333u, drv.read(A(I, cfg_t::DCT_BASE)), "DCT first");
    EXPECT_EQ_CTX(0x4444'4444u, drv.read(A(I, cfg_t::DCT_END - 4)), "DCT last");
    // 0x500 is the first word past the DCT window: a hole, so RAZ/WI.
    EXPECT_EQ_CTX(0u, drv.read(A(I, cfg_t::DCT_END)), "first word past DCT");
    // The two tables do not alias each other.
    EXPECT_EQ_CTX(0x1111'1111u, drv.read(A(I, cfg_t::DAT_BASE)), "DAT/DCT no alias");

    // Instance decode: the last word of instance I and the first word of
    // instance I+1 are different storage.
    drv.write(A(0, cfg_t::DAT_BASE), 0xAAAA'0000u);
    drv.write(A(1, cfg_t::DAT_BASE), 0xBBBB'0000u);
    EXPECT_EQ_CTX(0xAAAA'0000u, drv.read(A(0, cfg_t::DAT_BASE)), "inst0 DAT");
    EXPECT_EQ_CTX(0xBBBB'0000u, drv.read(A(1, cfg_t::DAT_BASE)), "inst1 DAT");
    // The last addressable word of the aperture resolves; the next faults.
    // (The out-of-aperture response itself is asserted in the negative bench.)
    EXPECT_EQ_CTX(0u, drv.read(A(N - 1, cfg_t::INSTANCE_SPACING - 4)),
                  "last aperture word is a hole");

    hard_reset();
    std::cout << "  [PASS] DAT/DCT bounds, table aliasing, instance decode\n";
}

// ---------------------------------------------------------------------------
// Command attribute legality
// ---------------------------------------------------------------------------
void tb::t_cmd_attr()
{
    hard_reset();
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));

    // Attribute 0 is the regular transfer the model implements.
    drv.write(A(0, cfg_t::XFER_DATA_PORT), 0xDEADBEEFu);
    push_command(0, make_cmd(1, false, 0, false, 0, 4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (resp >> 28) & 0xF, "attr 0 succeeds");
    }

    // Every other attribute is reported NotSupported, consumes no TX payload,
    // and latches TRANSFER_ERR_STAT.
    for (uint8_t attr = 1; attr < 8; ++attr) {
        std::string ctx = "CMD_ATTR " + std::to_string(attr);
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::TX_FIFO_RST) |
                  (1u << smc::reset_control::RESP_QUEUE_RST));
        drv.write(A(0, cfg_t::PIO_INTR_STATUS),
                  (1u << smc::pio_intr::TRANSFER_ERR_STAT));
        drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x1234'5678u);

        push_command(0, make_cmd(2, false, 0, false, 0, 4) | attr);
        run_xfer();

        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(2u, resp & 0xF, ctx + " TID echoed");
        EXPECT_EQ_CTX(uint32_t(smc::i3c_err::NotSupported), (resp >> 28) & 0xF,
                      ctx + " NotSupported");
        EXPECT_EQ_CTX(0u, (resp >> 16) & 0xFFF, ctx + " zero length");
        EXPECT_TRUE_CTX((drv.read(A(0, cfg_t::PIO_INTR_STATUS)) &
                         (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0,
                        ctx + " TRANSFER_ERR latched");

        // The rejected descriptor must not have eaten the TX payload.  The TX
        // FIFO is write-only, so the oracle is a following valid command: it
        // has to find the DWORD still queued and present it to the bus.
        last_write.clear();
        push_command(0, make_cmd(3, false, 0, false, 0, 4));
        run_xfer();
        const uint32_t ok_resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (ok_resp >> 28) & 0xF, ctx + " follow-up succeeds");
        EXPECT_EQ_CTX(size_t(4), last_write.size(), ctx + " TX payload survived");
        if (last_write.size() == 4) {
            EXPECT_EQ_CTX(uint8_t(0x78), last_write[0], ctx + " TX byte 0");
            EXPECT_EQ_CTX(uint8_t(0x56), last_write[1], ctx + " TX byte 1");
            EXPECT_EQ_CTX(uint8_t(0x34), last_write[2], ctx + " TX byte 2");
            EXPECT_EQ_CTX(uint8_t(0x12), last_write[3], ctx + " TX byte 3");
        }
    }

    hard_reset();
    std::cout << "  [PASS] CMD_ATTR: 0 runs, 1..7 report NotSupported\n";
}

// ---------------------------------------------------------------------------
// Exact annotated delay and mutable transfer delay
// ---------------------------------------------------------------------------
void tb::t_timing()
{
    hard_reset();
    auto broker = cci::cci_get_broker();
    auto hx = broker.get_param_handle(std::string(dut.name()) + ".xfer_delay_ns");
    auto ha = broker.get_param_handle(std::string(dut.name()) + ".access_delay_ns");
    EXPECT_TRUE(hx.is_valid());
    EXPECT_TRUE(ha.is_valid());

    // Exact b_transport annotation, from a non-zero incoming delay.
    auto access_delay = [&](uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
        sc_time t(7, SC_NS);
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        drv.sock->b_transport(gp, t);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return t - sc_time(7, SC_NS);
    };
    ha.set_cci_value(cci::cci_value(3.0));
    EXPECT_TRUE(access_delay(A(0, cfg_t::HC_CONTROL)) == sc_time(3, SC_NS));
    ha.set_cci_value(cci::cci_value(9.0));
    EXPECT_TRUE(access_delay(A(0, cfg_t::HC_CONTROL)) == sc_time(9, SC_NS));
    ha.set_cci_value(cci::cci_value(2.0));

    // xfer_delay_ns is documented mutable: a command enqueued after the change
    // must complete at the NEW delay, not the construction-time one.
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));

    for (double d : {100.0, 40.0, 250.0}) {
        hx.set_cci_value(cci::cci_value(d));
        std::string ctx = "xfer_delay_ns " + std::to_string(int(d));

        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::RESP_QUEUE_RST));
        read_payload = {0x01, 0x02, 0x03, 0x04};
        push_command(0, make_cmd(4, true, 0, false, 0, 4));

        // Just before the delay expires there is still no response ...
        sc_core::wait(sc_time(d - 1.0, SC_NS));
        EXPECT_TRUE_CTX(!resp_pending(0), ctx + " not early");
        // ... and just after it, there is.
        sc_core::wait(sc_time(2.0, SC_NS));
        EXPECT_TRUE_CTX(resp_pending(0), ctx + " on time");
        drv.read(A(0, cfg_t::RESPONSE_PORT));
        drv.read(A(0, cfg_t::XFER_DATA_PORT));
    }
    hx.set_cci_value(cci::cci_value(100.0));

    // A command already scheduled keeps the due time it was enqueued with, even
    // if xfer_delay_ns is shortened afterwards.  One shared sc_event serialises
    // the engine and a timed notify() keeps whichever notification lands
    // earliest, so re-notifying on a later enqueue would pull the queued
    // command forward and complete it sooner than its own delay allowed.
    {
        drv.write(A(0, cfg_t::RESET_CONTROL),
                    (1u << smc::reset_control::RESP_QUEUE_RST));
        hx.set_cci_value(cci::cci_value(200.0));
        read_payload = {0x09};
        push_command(0, make_cmd(5, true, 0, false, 0, 1));   // due at +200 ns

        sc_core::wait(20, SC_NS);
        hx.set_cci_value(cci::cci_value(20.0));               // shorten mid-flight
        push_command(0, make_cmd(6, true, 0, false, 0, 1));   // second command

        // At +60 ns the shortened delay would already have fired the first
        // command had the pending notification been moved.
        sc_core::wait(40, SC_NS);
        EXPECT_TRUE_CTX(!resp_pending(0),
                        "queued command keeps its original due time");

        // It completes on its own schedule, and the next one picks up the new
        // delay from the chaining tick.
        sc_core::wait(150, SC_NS);
        EXPECT_TRUE_CTX(resp_pending(0), "queued command completes at +200 ns");
        drv.read(A(0, cfg_t::RESPONSE_PORT));
        drv.read(A(0, cfg_t::XFER_DATA_PORT));
        sc_core::wait(40, SC_NS);
        EXPECT_TRUE_CTX(resp_pending(0), "next command uses the new delay");
        drv.read(A(0, cfg_t::RESPONSE_PORT));
        drv.read(A(0, cfg_t::XFER_DATA_PORT));
        hx.set_cci_value(cci::cci_value(100.0));
    }

    hard_reset();
    std::cout << "  [PASS] exact access delay; xfer_delay_ns is really mutable\n";
}

// ---------------------------------------------------------------------------
// Interrupt status-enable versus signal-enable
// ---------------------------------------------------------------------------
void tb::t_intr_enable_matrix()
{
    hard_reset();
    const uint32_t bitmask = (1u << smc::hc_intr::HC_INTERNAL_ERR_STAT);

    // All four enable combinations against a forced HC status bit.  The model
    // drives irq_o from signal-enable alone; status-enable does not gate it.
    //
    // NOTE (open spec question, audit finding 5): HCI implementations differ on
    // whether INTR_STATUS_ENABLE gates latching of the status bit.  The bench
    // pins today's behaviour so a change is deliberate and reviewed.
    for (unsigned se = 0; se < 2; ++se) {
        for (unsigned ge = 0; ge < 2; ++ge) {
            std::string ctx = "status_en=" + std::to_string(se) +
                              " signal_en=" + std::to_string(ge);
            hard_reset();
            drv.write(A(0, cfg_t::INTR_STATUS_ENABLE), se ? bitmask : 0u);
            drv.write(A(0, cfg_t::INTR_SIGNAL_ENABLE), ge ? bitmask : 0u);
            drv.write(A(0, cfg_t::INTR_FORCE), bitmask);
            settle();

            // The status bit latches regardless of either enable.
            EXPECT_EQ_CTX(bitmask, drv.read(A(0, cfg_t::INTR_STATUS)) & bitmask,
                          ctx + " status latches");
            // irq_o follows signal-enable only.
            EXPECT_EQ_CTX(ge ? 1 : 0, irq[0].read() ? 1 : 0, ctx + " irq_o");

            // Clearing the status drops the interrupt.
            drv.write(A(0, cfg_t::INTR_STATUS), bitmask);
            settle();
            EXPECT_TRUE_CTX(!irq[0].read(), ctx + " irq clears with status");
        }
    }

    // Enabling the signal after the status is already pending raises irq_o.
    hard_reset();
    drv.write(A(0, cfg_t::INTR_FORCE), bitmask);
    settle();
    EXPECT_TRUE(!irq[0].read());
    drv.write(A(0, cfg_t::INTR_SIGNAL_ENABLE), bitmask);
    settle();
    EXPECT_TRUE(irq[0].read());

    // Interrupts are per-instance: instance 1 stays quiet throughout.
    EXPECT_TRUE(!irq[1].read());

    hard_reset();
    std::cout << "  [PASS] interrupt enable matrix and per-instance isolation\n";
}

// ---------------------------------------------------------------------------
// Abstract bus outputs hold their documented idle contract
// ---------------------------------------------------------------------------
void tb::t_outputs_idle()
{
    auto check_idle = [&](const std::string& ctx) {
        for (unsigned i = 0; i < N; ++i) {
            const std::string c = ctx + " inst " + std::to_string(i);
            EXPECT_TRUE_CTX(scl[i].read(), c + " scl idle high");
            EXPECT_TRUE_CTX(sda[i].read(), c + " sda idle high");
            EXPECT_TRUE_CTX(!scl_oe[i].read(), c + " scl_oe low");
            EXPECT_TRUE_CTX(!sda_oe[i].read(), c + " sda_oe low");
            EXPECT_TRUE_CTX(!od_pp[i].read(), c + " sel_od_pp low");
            EXPECT_TRUE_CTX(!rpa[i].read(), c + " recovery payload low");
            EXPECT_TRUE_CTX(!ria[i].read(), c + " recovery image low");
        }
    };

    hard_reset();
    check_idle("after reset");

    // After real traffic.
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));
    drv.write(A(0, cfg_t::XFER_DATA_PORT), 0xDEADBEEFu);
    push_command(0, make_cmd(1, false, 0, false, 0, 4));
    run_xfer();
    drv.read(A(0, cfg_t::RESPONSE_PORT));
    check_idle("after a transfer");

    // After an abort.
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE) |
                                       (1u << smc::hc_control::ABORT));
    settle();
    check_idle("after abort");

    // After a mode-control write that sets every writable HC_CONTROL bit.
    drv.write(A(0, cfg_t::HC_CONTROL), 0xFFFF'FFFFu);
    settle();
    check_idle("after mode-control write");

    hard_reset();
    std::cout << "  [PASS] bus and recovery outputs hold their idle contract\n";
}

// ---------------------------------------------------------------------------
// Canonical AXI sideband
// ---------------------------------------------------------------------------
void tb::t_sideband()
{
    hard_reset();

    auto make_ext = [] {
        smc::smc_axi_extension e;
        e.source_id = smc::JTAG_ID;
        e.axi_id    = 0x1357u;
        e.axi_user  = 0x2Au;
        e.set_priv(false);
        e.set_secure(true);
        e.set_fetch(true);
        e.set_locked(true);
        return e;
    };
    const smc::smc_axi_extension golden = make_ext();

    // The sideband is inspected but never consumed: every field must survive a
    // register access unchanged, on each instance and on a decode fault.
    const uint64_t targets[] = {
        A(0, cfg_t::HC_CONTROL), A(1, cfg_t::DAT_BASE),
        A(N - 1, cfg_t::QUEUE_THLD_CTRL),
        uint64_t(N) * cfg_t::INSTANCE_SPACING,  // out of aperture
    };
    for (uint64_t addr : targets) {
        for (tlm::tlm_command cmd :
             {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            smc::smc_axi_extension ext = make_ext();
            uint32_t data = 0x9ABC'DEF0u;
            tlm::tlm_generic_payload gp;
            sc_time t = SC_ZERO_TIME;
            gp.set_command(cmd);
            gp.set_address(addr);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            gp.set_extension(&ext);
            drv.sock->b_transport(gp, t);
            gp.clear_extension<smc::smc_axi_extension>();

            const std::string ctx = "sideband @0x" + std::to_string(addr);
            EXPECT_EQ_CTX(golden.source_id, ext.source_id, ctx + " source_id");
            EXPECT_EQ_CTX(golden.axi_id, ext.axi_id, ctx + " axi_id");
            EXPECT_EQ_CTX(golden.prot, ext.prot, ctx + " prot");
            EXPECT_EQ_CTX(golden.axi_user, ext.axi_user, ctx + " axi_user");
            EXPECT_EQ_CTX(golden.is_secure ? 1 : 0, ext.is_secure ? 1 : 0,
                          ctx + " is_secure");
            EXPECT_EQ_CTX(golden.is_user ? 1 : 0, ext.is_user ? 1 : 0,
                          ctx + " is_user");
        }
    }
    // An absent extension is equally acceptable: the model inspects but never
    // requires the sideband (authorization belongs to the fabric filter).
    // Reset first, because the write sweep above deliberately scribbled on
    // HC_CONTROL through one of the routed targets.
    hard_reset();
    EXPECT_EQ(0x0000'0040u, drv.read(A(0, cfg_t::HC_CONTROL)));

    hard_reset();
    std::cout << "  [PASS] AXI sideband preserved on every route and a fault\n";
}

// ---------------------------------------------------------------------------
// Transfer length / DWORD packing boundaries
// ---------------------------------------------------------------------------
void tb::t_length_matrix()
{
    hard_reset();
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));

    // Writes: non-DWORD lengths must present exactly `len` bytes to the bus,
    // little-endian within each pushed DWORD.
    for (uint16_t len : {uint16_t(1), uint16_t(3), uint16_t(4), uint16_t(5),
                         uint16_t(8)}) {
        const std::string ctx = "write len " + std::to_string(len);
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::TX_FIFO_RST) |
                  (1u << smc::reset_control::RESP_QUEUE_RST));
        last_write.clear();

        const unsigned dwords = (len + 3u) / 4u;
        for (unsigned d = 0; d < dwords; ++d)
            drv.write(A(0, cfg_t::XFER_DATA_PORT), 0x04030201u + d * 0x04040404u);

        push_command(0, make_cmd(1, false, 0, false, 0, len));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (resp >> 28) & 0xF, ctx + " success");
        EXPECT_EQ_CTX(uint32_t(len), (resp >> 16) & 0xFFF, ctx + " response length");
        EXPECT_EQ_CTX(size_t(len), last_write.size(), ctx + " bytes presented");
        if (last_write.size() == len) {
            for (unsigned b = 0; b < len; ++b) {
                const unsigned d = b / 4;
                const uint32_t w = 0x04030201u + d * 0x04040404u;
                EXPECT_EQ_CTX(uint8_t((w >> (8 * (b % 4))) & 0xFF), last_write[b],
                              ctx + " byte " + std::to_string(b));
            }
        }
    }

    // Reads: the RX FIFO receives ceil(len/4) DWORDs and the response reports
    // the actual byte count.
    for (uint16_t len : {uint16_t(1), uint16_t(3), uint16_t(4), uint16_t(5)}) {
        const std::string ctx = "read len " + std::to_string(len);
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::RX_FIFO_RST) |
                  (1u << smc::reset_control::RESP_QUEUE_RST));
        read_payload.assign(len, 0);
        for (unsigned b = 0; b < len; ++b) read_payload[b] = uint8_t(0xA0 + b);

        push_command(0, make_cmd(2, true, 0, false, 0, len));
        run_xfer();
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (resp >> 28) & 0xF, ctx + " success");
        EXPECT_EQ_CTX(uint32_t(len), (resp >> 16) & 0xFFF, ctx + " response length");
        const unsigned dwords = (len + 3u) / 4u;
        for (unsigned d = 0; d < dwords; ++d) {
            uint32_t expect_w = 0;
            for (unsigned b = 0; b < 4; ++b) {
                const unsigned idx = d * 4 + b;
                if (idx < len) expect_w |= uint32_t(0xA0 + idx) << (8 * b);
            }
            EXPECT_EQ_CTX(expect_w, drv.read(A(0, cfg_t::XFER_DATA_PORT)),
                          ctx + " dword " + std::to_string(d));
        }
    }

    // A zero-length command still produces a response and touches no FIFO.
    drv.write(A(0, cfg_t::RESET_CONTROL),
              (1u << smc::reset_control::TX_FIFO_RST) |
              (1u << smc::reset_control::RESP_QUEUE_RST));
    last_write.clear();
    push_command(0, make_cmd(3, false, 0, false, 0, 0));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(3u, resp & 0xF, "len 0 TID");
        EXPECT_EQ_CTX(0u, (resp >> 16) & 0xFFF, "len 0 response length");
        EXPECT_EQ_CTX(size_t(0), last_write.size(), "len 0 no bytes");
    }

    hard_reset();
    std::cout << "  [PASS] length/packing matrix for reads and writes\n";
}

// ---------------------------------------------------------------------------
// IBI descriptor / payload / queue matrix
// ---------------------------------------------------------------------------
void tb::t_ibi_matrix()
{
    hard_reset();

    // Payload sizes around the DWORD boundary; the status descriptor carries
    // the 7-bit address shifted left and the byte count.
    for (size_t n : {size_t(0), size_t(1), size_t(3), size_t(4), size_t(5)}) {
        const std::string ctx = "IBI payload " + std::to_string(n);
        drv.write(A(0, cfg_t::RESET_CONTROL),
                  (1u << smc::reset_control::IBI_QUEUE_RST));
        std::vector<uint8_t> payload(n);
        for (size_t b = 0; b < n; ++b) payload[b] = uint8_t(0x10 + b);

        EXPECT_TRUE_CTX(dut.inject_ibi(0, 0x42, payload), ctx + " accepted");
        settle();
        EXPECT_EQ_CTX(uint32_t((0x42u << 1) | (uint32_t(n) << 8)),
                      drv.read(A(0, cfg_t::IBI_PORT)), ctx + " status descriptor");
        for (size_t d = 0; d < (n + 3) / 4; ++d) {
            uint32_t expect_w = 0;
            for (unsigned b = 0; b < 4; ++b) {
                const size_t idx = d * 4 + b;
                if (idx < n) expect_w |= uint32_t(0x10 + idx) << (8 * b);
            }
            EXPECT_EQ_CTX(expect_w, drv.read(A(0, cfg_t::IBI_PORT)),
                          ctx + " payload dword " + std::to_string(d));
        }
        // Queue drained: a further read returns 0.
        EXPECT_EQ_CTX(0u, drv.read(A(0, cfg_t::IBI_PORT)), ctx + " drained");
    }

    // The address is masked to 7 bits.
    drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));
    EXPECT_TRUE(dut.inject_ibi(0, 0xFF, {}));
    settle();
    EXPECT_EQ_CTX(uint32_t(0x7Fu << 1), drv.read(A(0, cfg_t::IBI_PORT)),
                  "IBI address masked to 7 bits");

    // A payload that cannot fit the queue is refused outright, leaving the
    // queue untouched rather than half-written.
    drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));
    EXPECT_TRUE(!dut.inject_ibi(0, 0x42, std::vector<uint8_t>(4096, 0xAA)));
    settle();
    EXPECT_EQ_CTX(0u, drv.read(A(0, cfg_t::IBI_PORT)), "oversized IBI not enqueued");
    // Out-of-range instance is refused.
    EXPECT_TRUE(!dut.inject_ibi(N, 0x42, {}));

    // IBI is per-instance.
    drv.write(A(0, cfg_t::RESET_CONTROL), (1u << smc::reset_control::IBI_QUEUE_RST));
    EXPECT_TRUE(dut.inject_ibi(1, 0x21, {0x99}));
    settle();
    EXPECT_EQ_CTX(0u, drv.read(A(0, cfg_t::IBI_PORT)), "IBI isolated from inst 0");
    EXPECT_EQ_CTX(uint32_t((0x21u << 1) | (1u << 8)),
                  drv.read(A(1, cfg_t::IBI_PORT)), "IBI on inst 1");

    hard_reset();
    std::cout << "  [PASS] IBI address/payload/queue/isolation matrix\n";
}

// ---------------------------------------------------------------------------
// Read-data contract: short and overlong bus-model replies
// ---------------------------------------------------------------------------
void tb::t_read_data_policy()
{
    hard_reset();
    drv.write(A(0, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(0, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));

    // A target that returns fewer bytes than requested completes with Success
    // and the ACTUAL length in the response.
    //
    // NOTE (open spec question, audit finding 12): i3c_err::I3cShortReadErr
    // exists for "short read not permitted", but the command descriptor has no
    // modelled flag to select that policy, so the model always treats a short
    // read as success.  Pinned here so a policy change is deliberate.
    drv.write(A(0, cfg_t::RESET_CONTROL),
              (1u << smc::reset_control::RX_FIFO_RST) |
              (1u << smc::reset_control::RESP_QUEUE_RST));
    read_payload = {0x11, 0x22, 0x33};          // 3 of 8 requested
    push_command(0, make_cmd(1, true, 0, false, 0, 8));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (resp >> 28) & 0xF, "short read success");
        EXPECT_EQ_CTX(3u, (resp >> 16) & 0xFFF, "short read actual length");
        EXPECT_EQ_CTX(0x0033'2211u, drv.read(A(0, cfg_t::XFER_DATA_PORT)),
                      "short read data");
    }

    // A target that returns MORE bytes than requested is truncated to the
    // requested length; the surplus never reaches the RX FIFO.
    drv.write(A(0, cfg_t::RESET_CONTROL),
              (1u << smc::reset_control::RX_FIFO_RST) |
              (1u << smc::reset_control::RESP_QUEUE_RST));
    read_payload = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
    push_command(0, make_cmd(2, true, 0, false, 0, 4));   // only 4 wanted
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(0u, (resp >> 28) & 0xF, "overlong read success");
        EXPECT_EQ_CTX(4u, (resp >> 16) & 0xFFF, "overlong read truncated length");
        EXPECT_EQ_CTX(0xA4A3'A2A1u, drv.read(A(0, cfg_t::XFER_DATA_PORT)),
                      "overlong read data");
        EXPECT_EQ_CTX(0u, drv.read(A(0, cfg_t::XFER_DATA_PORT)),
                      "no surplus dword in RX FIFO");
    }

    // With no bus model attached at all, the address is NACKed: nothing on the
    // modelled bus can answer.  Instance 1 never gets a bus model.
    drv.write(A(1, cfg_t::DAT_BASE + 0), (0x42u << 16));
    drv.write(A(1, cfg_t::HC_CONTROL), (1u << smc::hc_control::BUS_ENABLE));
    push_command(1, make_cmd(4, true, 0, false, 0, 4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(1, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(uint32_t(smc::i3c_err::AddressNack), (resp >> 28) & 0xF,
                      "no bus model NACKs");
        EXPECT_TRUE_CTX((drv.read(A(1, cfg_t::PIO_INTR_STATUS)) &
                         (1u << smc::pio_intr::TRANSFER_ERR_STAT)) != 0,
                        "no bus model latches TRANSFER_ERR");
    }

    // A bus model that ACKs but reports an error propagates that error code.
    drv.write(A(0, cfg_t::RESET_CONTROL),
              (1u << smc::reset_control::RESP_QUEUE_RST));
    dut.set_bus_model(0, [](smc::i3c_xfer& x) {
        x.ack = true;
        x.error = smc::i3c_err::ParityError;
    });
    push_command(0, make_cmd(3, true, 0, false, 0, 4));
    run_xfer();
    {
        const uint32_t resp = drv.read(A(0, cfg_t::RESPONSE_PORT));
        EXPECT_EQ_CTX(uint32_t(smc::i3c_err::ParityError), (resp >> 28) & 0xF,
                      "ACK with error propagates");
    }

    hard_reset();
    std::cout << "  [PASS] short / overlong / errored read-data contract\n";
}

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

    // ===== 14. Architectural reset input =====
    drv.write(A(1, cfg_t::HC_CONTROL), 0xFFFFFFFFu);
    drv.write(A(1, cfg_t::DAT_BASE), 0xA5A55A5Au);
    rst_n.write(false);
    settle();
    rst_n.write(true);
    settle();
    EXPECT_EQ(cfg_t::HC_CONTROL_RESET, drv.read(A(1, cfg_t::HC_CONTROL)));
    EXPECT_EQ(0u, drv.read(A(1, cfg_t::DAT_BASE)));

    // ===== 15. transport_dbg back-door (DAT) =====
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

    // ===== 16. QUEUE_SIZE reflects configured depths =====
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

    // ==================================================================
    // Architectural sections added for the internal-review remediation.
    // Each restores the DUT to reset on entry and exit, so they are
    // independent of the numbered sections above and of each other.
    // ==================================================================
    std::cout << "\n--- Architectural: register map ---\n";
    t_csr_manifest();
    t_window_map();

    std::cout << "\n--- Architectural: HCI command engine ---\n";
    t_cmd_attr();
    t_length_matrix();
    t_read_data_policy();
    t_ibi_matrix();

    std::cout << "\n--- Architectural: TLM, timing, interrupts, outputs ---\n";
    t_sideband();
    t_timing();
    t_intr_enable_matrix();
    t_outputs_idle();

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "FAILURE(S): " << g_failures << " check(s) failed\n";

    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char*[])
{
    static cci_utils::consuming_broker cci_global_broker("global_broker");
    cci::cci_register_broker(cci_global_broker);
    tb t("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
