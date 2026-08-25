// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// clint_tb.cpp -- self-checking test bench for the SMC CLINT (CCI-compliant).
//
// CCI integration highlights
// ─────────────────────────────────────────────────────────────────────────
// • sc_main registers a global CCI broker before any module is constructed.
// • Preset values are injected for tb.clint.num_harts (4 → 2),
//   tb.clint.tick_period_ns (100 → 0, i.e. test-controlled time), and
//   tb.clint.access_delay_ns (2 → 5) to demonstrate pre-construction override.
// • The tb struct declares matching cci_param members (num_harts_p_,
//   tick_period_ns_p_) that receive the same presets for signal-array sizing
//   and so the TB chooses an explicit-time vs. auto-tick code path.
// • A dedicated test exercises CCI introspection: param lookup by name,
//   typed and untyped handles, descriptions, metadata, originator tracking,
//   and run-time mutation of the mutable access_delay_ns parameter.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "clint.h"

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

// ---------------------------------------------------------------------------
// Tiny TLM driver -- mimics what the CPU cluster bus bridge would issue.
// Supports both 32-bit and 64-bit accesses (the firmware driver in
// fw/smc/common/drivers/riscv_clint0.c always uses 32-bit; some 64-bit-host
// configurations issue 64-bit accesses).
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEF;
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
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write32(uint64_t addr, uint32_t value) {
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
            std::cerr << "FAIL write32(0x" << std::hex << addr
                      << ", 0x" << value << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    uint64_t read64(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0xDEAD'BEEF'DEAD'BEEFULL;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read64(0x" << std::hex << addr << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write64(uint64_t addr, uint64_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write64(0x" << std::hex << addr
                      << ", 0x" << value << ") got rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    // Returns the raw response status without failing the test on error --
    // used for the negative-path checks.
    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t len, void* data) {
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

    // Variant with a deliberately wrong streaming_width (TC-20).
    tlm::tlm_response_status raw_xfer_sw(tlm::tlm_command cmd, uint64_t addr,
                                         uint32_t len, void* data, uint32_t sw) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(sw);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    // Variant with byte-enable pointer set (TC-21).
    tlm::tlm_response_status raw_xfer_be(tlm::tlm_command cmd, uint64_t addr,
                                         uint32_t len, void* data,
                                         uint8_t* be_ptr, uint32_t be_len) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(be_ptr);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

// ---------------------------------------------------------------------------
// Address arithmetic helpers — match the canonical layout in clint.h /
// clint.rdl.  Use uint64_t addresses so 32-bit hosts compile cleanly.
// ---------------------------------------------------------------------------
constexpr uint64_t msip_addr(unsigned hart) {
    return smc::clint_cfg::MSIP_BASE + smc::clint_cfg::MSIP_STRIDE * hart;
}
constexpr uint64_t mtimecmp_addr(unsigned hart) {
    return smc::clint_cfg::MTIMECMP_BASE + smc::clint_cfg::MTIMECMP_STRIDE * hart;
}
constexpr uint64_t mtime_addr() { return smc::clint_cfg::MTIME_OFFSET; }

// Mirror of the firmware "high=FFFFFFFF, low, high" sequence for MTIMECMP.
// See fw/smc/common/drivers/riscv_clint0.c::__metal_driver_riscv_clint0_mtimecmp_set
// — this avoids spurious MTIP transitions while updating the comparator.
inline void fw_set_mtimecmp(driver& drv, unsigned hart, uint64_t value)
{
    drv.write32(mtimecmp_addr(hart) + 4, 0xFFFFFFFFu);
    drv.write32(mtimecmp_addr(hart) + 0, static_cast<uint32_t>(value));
    drv.write32(mtimecmp_addr(hart) + 4, static_cast<uint32_t>(value >> 32));
}

// Mirror of the firmware MTIME read that guards against rollover by re-reading
// the high word (riscv_clint0.c::__metal_clint0_mtime_get).
inline uint64_t fw_read_mtime(driver& drv)
{
    uint32_t hi, lo;
    do {
        hi = drv.read32(mtime_addr() + 4);
        lo = drv.read32(mtime_addr() + 0);
    } while (drv.read32(mtime_addr() + 4) != hi);
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    // ------------------------------------------------------------------
    // CCI sizing parameters — declared FIRST so they are initialised
    // before dut and the sc_vector signal arrays below.
    //
    // tb.num_harts mirrors tb.clint.num_harts so the TB signal arrays are
    // always consistent with the DUT port arrays.  tb.tick_period_ns
    // mirrors tb.clint.tick_period_ns so the TB knows whether to drive
    // MTIME via dbg_set_mtime or simply wait for ticks.
    // ------------------------------------------------------------------
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_harts_p_;
    cci::cci_param<double,   cci::CCI_IMMUTABLE_PARAM> tick_period_ns_p_;

    smc::clint dut;
    driver     drv;

    sc_core::sc_signal<bool>                     rst_n;
    sc_core::sc_vector<sc_core::sc_signal<bool>> msip_sig;
    sc_core::sc_vector<sc_core::sc_signal<bool>> mtip_sig;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , num_harts_p_("num_harts", 4u,
                       "TB signal-array size — must match clint.num_harts preset.")
        , tick_period_ns_p_("tick_period_ns", 100.0,
                            "TB-side mirror of clint.tick_period_ns. "
                            "Used to choose the test code path (auto-tick vs explicit).")
        , dut("clint")
        , drv("drv")
        , rst_n   ("rst_n")
        , msip_sig("msip_sig", num_harts_p_.get_value())
        , mtip_sig("mtip_sig", num_harts_p_.get_value())
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        for (unsigned h = 0; h < num_harts_p_.get_value(); ++h) {
            dut.msip_o[h](msip_sig[h]);
            dut.mtip_o[h](mtip_sig[h]);
        }

        SC_THREAD(run);
    }

    unsigned num_harts() const { return num_harts_p_.get_value(); }
    bool     tick_disabled() const { return tick_period_ns_p_.get_value() == 0.0; }

    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(20, SC_NS);
        rst_n.write(true);
        sc_core::wait(20, SC_NS);
    }

    // Two delta cycles is enough for: reg-write → schedule_recompute → output_method
    // to drive the sc_signal, and for the TB's check to read it.
    static void settle() {
        for (int i = 0; i < 2; ++i) sc_core::wait(SC_ZERO_TIME);
    }

    // Convenience: drive MTIME to a specific value, settle, and assert.
    void set_mtime_and_settle(uint64_t v) {
        dut.dbg_set_mtime(v);
        settle();
        EXPECT_EQ(v, dut.dbg_mtime());
    }

    void run() {
        std::cout << "==== SMC CLINT TB (CCI-compliant) ====\n";
        std::cout << "  DUT topology: "
                  << num_harts() << " harts"
                  << ", tick_period_ns=" << tick_period_ns_p_.get_value()
                  << "\n";

        // ------------------------------------------------------------------
        // 1. Reset clears state.
        //    Per the clint.h compliance notes:
        //      MTIME      = 0
        //      MSIP[h]    = 0
        //      MTIMECMP[h]= 0xFFFFFFFFFFFFFFFF (so MTIP=0 out of reset)
        // ------------------------------------------------------------------
        rst_n.write(true);
        sc_core::wait(1, SC_NS);
        pulse_reset();
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());
        for (unsigned h = 0; h < num_harts(); ++h) {
            EXPECT_EQ(0u, dut.dbg_msip(h));
            EXPECT_EQ(uint64_t(0xFFFFFFFFFFFFFFFFULL), dut.dbg_mtimecmp(h));
            EXPECT_EQ(false, dut.dbg_mtip(h));
            EXPECT_EQ(false, msip_sig[h].read());
            EXPECT_EQ(false, mtip_sig[h].read());
        }
        std::cout << "  [PASS] reset clears MTIME / MSIP / MTIP; MTIMECMP=max\n";

        // ------------------------------------------------------------------
        // 2. MSIP R/W — only bit[0] survives; bits[31:1] are RAZ/WI.
        // ------------------------------------------------------------------
        for (unsigned h = 0; h < num_harts(); ++h) {
            drv.write32(msip_addr(h), 0xFFFFFFFFu);
            EXPECT_EQ(1u, drv.read32(msip_addr(h)));   // 31:1 are RAZ
            EXPECT_EQ(1u, dut.dbg_msip(h));
            settle();
            EXPECT_EQ(true, msip_sig[h].read());        // msip_o[h] tracks bit0
            drv.write32(msip_addr(h), 0u);
            EXPECT_EQ(0u, drv.read32(msip_addr(h)));
            settle();
            EXPECT_EQ(false, msip_sig[h].read());
        }
        std::cout << "  [PASS] MSIP R/W truncates to bit[0]; msip_o tracks register\n";

        // ------------------------------------------------------------------
        // 3. MSIP isolation — writing one hart's MSIP must not perturb others.
        // ------------------------------------------------------------------
        if (num_harts() >= 2) {
            drv.write32(msip_addr(0), 1);
            settle();
            for (unsigned h = 0; h < num_harts(); ++h) {
                EXPECT_EQ(h == 0 ? true : false, msip_sig[h].read());
            }
            drv.write32(msip_addr(0), 0);
            settle();
            std::cout << "  [PASS] MSIP per-hart isolation\n";
        }

        // ------------------------------------------------------------------
        // 4. MTIME 64-bit single access (read/write via the canonical 8-byte
        //    transaction at 0xBFF8).
        // ------------------------------------------------------------------
        drv.write64(mtime_addr(), 0x0123'4567'89AB'CDEFULL);
        EXPECT_EQ(uint64_t(0x0123'4567'89AB'CDEFULL), drv.read64(mtime_addr()));
        EXPECT_EQ(uint64_t(0x0123'4567'89AB'CDEFULL), dut.dbg_mtime());
        std::cout << "  [PASS] MTIME 64-bit R/W\n";

        // ------------------------------------------------------------------
        // 5. MTIME 32-bit two-half access (matches the riscv_clint0.c driver).
        // ------------------------------------------------------------------
        drv.write32(mtime_addr() + 0, 0x11223344);
        drv.write32(mtime_addr() + 4, 0x55667788);
        EXPECT_EQ(uint32_t(0x11223344), drv.read32(mtime_addr() + 0));
        EXPECT_EQ(uint32_t(0x55667788), drv.read32(mtime_addr() + 4));
        EXPECT_EQ(uint64_t(0x5566'7788'1122'3344ULL), dut.dbg_mtime());
        std::cout << "  [PASS] MTIME 32-bit half-word R/W\n";

        // ------------------------------------------------------------------
        // 6. Firmware-style MTIME read that guards against rollover.
        // ------------------------------------------------------------------
        dut.dbg_set_mtime(0x4000'0000'1234'5678ULL);
        EXPECT_EQ(uint64_t(0x4000'0000'1234'5678ULL), fw_read_mtime(drv));
        std::cout << "  [PASS] firmware-style MTIME read sequence\n";

        // ------------------------------------------------------------------
        // 7. MTIMECMP 64-bit single access (one transaction).
        // ------------------------------------------------------------------
        for (unsigned h = 0; h < num_harts(); ++h) {
            drv.write64(mtimecmp_addr(h), 0xAAAA'BBBB'CCCC'DDDDULL);
            EXPECT_EQ(uint64_t(0xAAAA'BBBB'CCCC'DDDDULL), drv.read64(mtimecmp_addr(h)));
            EXPECT_EQ(uint64_t(0xAAAA'BBBB'CCCC'DDDDULL), dut.dbg_mtimecmp(h));
        }
        std::cout << "  [PASS] MTIMECMP 64-bit R/W per hart\n";

        // ------------------------------------------------------------------
        // 8. MTIMECMP 32-bit half-word access — firmware sequence.
        //    Verifies that the "high=FFFFFFFF, low, high" pattern produces
        //    the intended 64-bit value.
        // ------------------------------------------------------------------
        const uint64_t cmp_target = 0x0000'0001'0000'1000ULL;
        fw_set_mtimecmp(drv, 0, cmp_target);
        EXPECT_EQ(cmp_target, dut.dbg_mtimecmp(0));
        std::cout << "  [PASS] firmware-style MTIMECMP set sequence\n";

        // ------------------------------------------------------------------
        // 9. MTIP comparator — boundary behaviour at MTIME = MTIMECMP - 1,
        //    MTIME = MTIMECMP, MTIME = MTIMECMP + 1.
        // ------------------------------------------------------------------
        // Reset so all per-hart state is well known.
        pulse_reset();

        const uint64_t cmp = 100;
        for (unsigned h = 0; h < num_harts(); ++h) {
            drv.write64(mtimecmp_addr(h), cmp);
        }

        // MTIME = cmp - 1 → MTIP all 0
        set_mtime_and_settle(cmp - 1);
        for (unsigned h = 0; h < num_harts(); ++h) {
            EXPECT_EQ(false, mtip_sig[h].read());
        }
        // MTIME = cmp → MTIP all 1 (>= boundary)
        set_mtime_and_settle(cmp);
        for (unsigned h = 0; h < num_harts(); ++h) {
            EXPECT_EQ(true, mtip_sig[h].read());
        }
        // MTIME = cmp + 1 → MTIP still 1
        set_mtime_and_settle(cmp + 1);
        for (unsigned h = 0; h < num_harts(); ++h) {
            EXPECT_EQ(true, mtip_sig[h].read());
        }
        std::cout << "  [PASS] MTIP comparator at -1 / == / +1 boundary\n";

        // ------------------------------------------------------------------
        // 10. MTIP is cleared by writing MTIMECMP > MTIME.
        // ------------------------------------------------------------------
        for (unsigned h = 0; h < num_harts(); ++h) {
            drv.write64(mtimecmp_addr(h), 0xFFFF'FFFF'FFFF'FFFFULL);
            settle();
            EXPECT_EQ(false, mtip_sig[h].read());
        }
        std::cout << "  [PASS] MTIP cleared by raising MTIMECMP\n";

        // ------------------------------------------------------------------
        // 11. Per-hart MTIMECMP independence — different comparators yield
        //     different MTIP at the same MTIME.
        // ------------------------------------------------------------------
        if (num_harts() >= 2) {
            pulse_reset();
            drv.write64(mtimecmp_addr(0), 50);
            drv.write64(mtimecmp_addr(1), 200);
            set_mtime_and_settle(100);
            EXPECT_EQ(true,  mtip_sig[0].read());
            EXPECT_EQ(false, mtip_sig[1].read());
            std::cout << "  [PASS] per-hart MTIMECMP independence\n";
        }

        // ------------------------------------------------------------------
        // 12. MSIP is independent of MTIP and vice-versa.
        // ------------------------------------------------------------------
        pulse_reset();
        // Configure hart 0 so MTIP fires at time 10, MSIP unset.
        drv.write64(mtimecmp_addr(0), 10);
        set_mtime_and_settle(10);
        EXPECT_EQ(true,  mtip_sig[0].read());
        EXPECT_EQ(false, msip_sig[0].read());
        drv.write32(msip_addr(0), 1);
        settle();
        EXPECT_EQ(true, msip_sig[0].read());  // MSIP independently asserted
        EXPECT_EQ(true, mtip_sig[0].read());  // MTIP unchanged
        drv.write64(mtimecmp_addr(0), 0xFFFFFFFFFFFFFFFFULL);
        settle();
        EXPECT_EQ(false, mtip_sig[0].read()); // MTIP independently cleared
        EXPECT_EQ(true,  msip_sig[0].read()); // MSIP unchanged
        drv.write32(msip_addr(0), 0);
        settle();
        EXPECT_EQ(false, msip_sig[0].read());
        std::cout << "  [PASS] MSIP and MTIP are independent per hart\n";

        // ------------------------------------------------------------------
        // 13. MTIME auto-tick — only meaningful when tick_period_ns > 0.
        //     The TB chooses tick_period_ns = 0 by default for determinism;
        //     this test conditionally re-enables it via a separate path
        //     using dbg_set_mtime to model "ticks".
        // ------------------------------------------------------------------
        if (!tick_disabled()) {
            // Real auto-tick path (only taken when sc_main left tick > 0).
            const uint64_t t0 = dut.dbg_mtime();
            sc_core::wait(sc_time(tick_period_ns_p_.get_value() * 5, SC_NS));
            const uint64_t t1 = dut.dbg_mtime();
            EXPECT_TRUE(t1 > t0);
            std::cout << "  [PASS] MTIME auto-tick increments over time\n";
        } else {
            // Synthetic path: simulate 1024 ticks via dbg_set_mtime.
            pulse_reset();
            const uint64_t before = dut.dbg_mtime();
            for (int i = 1; i <= 1024; ++i) {
                dut.dbg_set_mtime(before + i);
            }
            settle();
            EXPECT_EQ(uint64_t(before + 1024), dut.dbg_mtime());
            std::cout << "  [PASS] synthetic MTIME advance (auto-tick disabled)\n";
        }

        // ------------------------------------------------------------------
        // 14. transport_dbg back-door — same address space, no annotated delay.
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint64_t data = 0xCAFE'BABE'F00D'BAADULL;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(mtime_addr());
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
            gp.set_data_length(8);
            gp.set_streaming_width(8);
            gp.set_byte_enable_ptr(nullptr);
            unsigned bytes = drv.sock->transport_dbg(gp);
            EXPECT_EQ(8u, bytes);
            EXPECT_EQ(uint64_t(0xCAFE'BABE'F00D'BAADULL), dut.dbg_mtime());

            uint64_t back = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&back));
            bytes = drv.sock->transport_dbg(gp);
            EXPECT_EQ(8u, bytes);
            EXPECT_EQ(uint64_t(0xCAFE'BABE'F00D'BAADULL), back);
        }
        std::cout << "  [PASS] transport_dbg back-door read/write\n";

        // ------------------------------------------------------------------
        // 15. Negative tests — out-of-window, misaligned, wrong size.
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            // Out-of-window
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x20000, 4, &scratch));
            // Misaligned 32-bit
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, mtime_addr() + 1, 4, &scratch));
            // Misaligned 64-bit (mtime address ends in 0xBFF8 ⇒ aligned;
            //   we deliberately try +4 to violate 8-byte alignment)
            uint64_t scratch64 = 0;
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, mtime_addr() + 4, 8, &scratch64));
            // Wrong width (3 bytes)
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, mtime_addr(), 3, &scratch));
            // 8-byte access on MSIP (which is 4-byte only) → reg_read returns false
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, msip_addr(0), 8, &scratch64));
            std::cout << "  [PASS] negative tests: window, alignment, width\n";
        }

        // ------------------------------------------------------------------
        // 16. CCI introspection — discover params, query metadata, mutate
        //     access_delay_ns at run-time, observe the change.
        //
        // Inside the SystemC hierarchy we must use `cci_get_broker()` (no
        // explicit originator) per CCI 1.0 §7.4.  A named originator and the
        // global accessor are only legal at sc_main scope.
        // ------------------------------------------------------------------
        {
            auto broker = cci::cci_get_broker();

            // Lookup by hierarchical name ----------------------------------
            auto h_num    = broker.get_param_handle("tb.clint.num_harts");
            auto h_tick   = broker.get_param_handle("tb.clint.tick_period_ns");
            auto h_delay  = broker.get_param_handle("tb.clint.access_delay_ns");
            EXPECT_TRUE(h_num.is_valid());
            EXPECT_TRUE(h_tick.is_valid());
            EXPECT_TRUE(h_delay.is_valid());

            // Type-erased value ---------------------------------------------
            EXPECT_EQ(num_harts(), h_num.get_cci_value().get_uint());

            // Description survived metadata round-trip ----------------------
            EXPECT_TRUE(!h_num.get_description().empty());

            // Run-time mutation of the *mutable* parameter ------------------
            const double old_delay = h_delay.get_cci_value().get_double();
            h_delay.set_cci_value(cci::cci_value(old_delay * 2.0));
            // Issue a transaction to make the model re-cache the value.
            (void)drv.read32(msip_addr(0));
            EXPECT_EQ(old_delay * 2.0, h_delay.get_cci_value().get_double());

            // Immutable parameter rejects post-elaboration writes.
            // Suppress just for this single set: we *want* the report so we
            // can verify the param value is unchanged afterwards.  (CCI logs
            // a single SC_ERROR; we filter it inline.)
            const unsigned old_num = h_num.get_cci_value().get_uint();
            const auto prev_actions = sc_core::sc_report_handler::get_log_file_name();
            (void)prev_actions; // unused — present for documentation
            try {
                h_num.set_cci_value(cci::cci_value(unsigned(old_num + 1)));
            } catch (...) {
                // Some CCI implementations throw; either is acceptable.
            }
            EXPECT_EQ(old_num, h_num.get_cci_value().get_uint());

            std::cout << "  [PASS] CCI: discovery, introspection, mutation, immutability\n";
        }

        // ------------------------------------------------------------------
        // 17. dump_state — sanity-check the human-readable dump (smoke).
        // ------------------------------------------------------------------
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            const std::string s = oss.str();
            EXPECT_TRUE(s.find("CLINT state") != std::string::npos);
            EXPECT_TRUE(s.find("mtime") != std::string::npos);
            EXPECT_TRUE(s.find("hart[0]") != std::string::npos);
            std::cout << "  [PASS] dump_state contains expected fields\n";
        }

        // ------------------------------------------------------------------
        // 18. MTIMECMP 32-bit half-word READ — both the low and high halves.
        //     TC-7/8 only test 64-bit reads or 32-bit writes of MTIMECMP.
        //     This test exercises the 32-bit read path (clint.cpp lines 253-255)
        //     for both half==0 and half==4.
        // ------------------------------------------------------------------
        pulse_reset();
        for (unsigned h = 0; h < num_harts(); ++h) {
            const uint64_t val = 0xDEAD'BEEF'CAFE'F00DULL;
            drv.write64(mtimecmp_addr(h), val);
            const uint32_t lo = drv.read32(mtimecmp_addr(h) + 0);
            const uint32_t hi = drv.read32(mtimecmp_addr(h) + 4);
            EXPECT_EQ(uint32_t(0xCAFE'F00DU), lo);
            EXPECT_EQ(uint32_t(0xDEAD'BEEFU), hi);
            EXPECT_EQ(val, (uint64_t(hi) << 32) | lo);
        }
        std::cout << "  [PASS] MTIMECMP 32-bit half-word READ (low + high halves)\n";

        // ------------------------------------------------------------------
        // 19. In-window hole: RAZ/WI.
        //     Address 0x0100 is inside the 48 KB window but belongs to no
        //     register (MSIP ends at MSIP_BASE + num_harts*4, MTIMECMP
        //     starts at 0x4000).  Writes silently succeed (WI) and reads
        //     return 0 (RAZ).  Covers clint.cpp lines 274-275 and 344-345.
        // ------------------------------------------------------------------
        {
            constexpr uint64_t hole = 0x0100;
            uint32_t scratch = 0xDEAD;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.raw_xfer(tlm::TLM_WRITE_COMMAND, hole, 4, &scratch));
            scratch = 0xDEAD;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, hole, 4, &scratch));
            EXPECT_EQ(uint32_t(0), scratch);
        }
        std::cout << "  [PASS] in-window hole is RAZ/WI\n";

        // ------------------------------------------------------------------
        // 20. streaming_width mismatch → TLM_BURST_ERROR_RESPONSE.
        //     The model rejects any transfer whose streaming_width != data_length
        //     (clint.cpp lines 372-374).
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer_sw(tlm::TLM_READ_COMMAND, mtime_addr(),
                                      4, &scratch, /*sw=*/1));
            uint64_t scratch64 = 0;
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer_sw(tlm::TLM_WRITE_COMMAND, mtime_addr(),
                                      8, &scratch64, /*sw=*/4));
        }
        std::cout << "  [PASS] streaming_width mismatch → TLM_BURST_ERROR_RESPONSE\n";

        // ------------------------------------------------------------------
        // 21. Byte-enable and unknown command error paths.
        //     • Non-null byte_enable_ptr → TLM_BYTE_ENABLE_ERROR_RESPONSE
        //       (clint.cpp lines 376-379).
        //     • TLM_IGNORE_COMMAND (neither read nor write) →
        //       TLM_COMMAND_ERROR_RESPONSE (clint.cpp lines 401-403).
        // ------------------------------------------------------------------
        {
            uint32_t scratch = 0;
            uint8_t  be = 0xFF;
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      drv.raw_xfer_be(tlm::TLM_READ_COMMAND, mtime_addr(),
                                      4, &scratch, &be, 1));
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, mtime_addr(),
                                   4, &scratch));
        }
        std::cout << "  [PASS] byte-enable error and unknown-command error\n";

        // ------------------------------------------------------------------
        // 22. transport_dbg error paths → return 0.
        //     Tests invalid data_length, misaligned address, and out-of-window
        //     address (clint.cpp lines 428-433).
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t scratch = 0;
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));

            // Wrong length (3 bytes)
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(mtime_addr());
            gp.set_data_length(3);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

            // Misaligned 32-bit access (addr + 1)
            gp.set_address(mtime_addr() + 1);
            gp.set_data_length(4);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

            // Out-of-window address
            gp.set_address(smc::clint_cfg::WINDOW_SIZE + 0x100);
            gp.set_data_length(4);
            EXPECT_EQ(0u, drv.sock->transport_dbg(gp));
        }
        std::cout << "  [PASS] transport_dbg invalid-args paths return 0\n";

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
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);
    // Suppress fatal-on-error for CCI immutable-write attempts; we use them
    // intentionally in test 16 to verify the immutability semantics.
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                            sc_core::SC_DISPLAY);

    // ── CCI: register global broker ──────────────────────────────────────
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    // ── CCI: inject preset values before tb / clint are constructed ──────
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    // DUT: 4-hart default → 2 harts; auto-tick disabled (tick_period_ns=0)
    // so the TB drives MTIME deterministically via dbg_set_mtime; double the
    // access delay (2 → 5 ns) to demonstrate mutable-param override.
    global_broker.set_preset_cci_value(
        "tb.clint.num_harts",       cci::cci_value(2u));
    global_broker.set_preset_cci_value(
        "tb.clint.tick_period_ns",  cci::cci_value(0.0));
    global_broker.set_preset_cci_value(
        "tb.clint.access_delay_ns", cci::cci_value(5.0));

    // TB sizing params must mirror the DUT presets.
    global_broker.set_preset_cci_value(
        "tb.num_harts",      cci::cci_value(2u));
    global_broker.set_preset_cci_value(
        "tb.tick_period_ns", cci::cci_value(0.0));

    // ── Instantiate and run ───────────────────────────────────────────────
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
