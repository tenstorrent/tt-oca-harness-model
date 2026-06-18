// SPDX-License-Identifier: Apache-2.0
//
// cluster_tb.cpp -- self-checking test bench for the SMC CPU cluster.
// Same convention as peripherals/plic/test/plic_tb.cpp: one binary, macro
// checks, prints "ALL TESTS PASSED" on success.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#ifdef SMC_HAVE_PLIC
#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include "plic.h"
#endif

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

#include "System.hpp"
#include "Hart.hpp"
#include "smc_test_utils.h"
#include "smc_cpu_cluster.h"
#include "smc_axi_extension.h"

unsigned g_failures = 0;

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected=" << _e << " actual=" << _a               \
                      << "  (" #expected " == " #actual ")\n";                   \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_NE(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (_e == _a) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected !=" << _e << " actual=" << _a              \
                      << "  (" #expected " != " #actual ")\n";                   \
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

#define EXPECT_FALSE(cond)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected FALSE: " #cond "\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  assert TRUE: " #cond "\n";                         \
            ++g_failures;                                                      \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_FALSE(cond)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  assert FALSE: " #cond "\n";                        \
            ++g_failures;                                                      \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  assert " << _e << " == " << _a << "\n";            \
            ++g_failures;                                                      \
            return;                                                            \
        }                                                                      \
    } while (0)

void pass(const char* name) { std::cout << "  [PASS] " << name << '\n'; }

// ---------------------------------------------------------------------------
// MMIO inspector for smc_axi_extension tests
// ---------------------------------------------------------------------------
class InspectingRamStub : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<InspectingRamStub, 64> socket{"socket"};

    bool         saw_extension = false;
    uint16_t     last_source_id = 0;
    uint16_t     last_axi_id    = 0;
    uint8_t      last_prot      = 0;
    bool         last_locked    = false;
    unsigned     write_calls    = 0;
    std::map<uint64_t, uint8_t> mem;

    SC_HAS_PROCESS(InspectingRamStub);
    explicit InspectingRamStub(sc_core::sc_module_name n) : sc_module(n)
    {
        socket.register_b_transport(this, &InspectingRamStub::b_transport);
    }

    void seed(uint64_t addr, uint64_t value, unsigned size)
    {
        for (unsigned i = 0; i < size; ++i) {
            mem[addr + i] = uint8_t((value >> (8 * i)) & 0xFF);
        }
    }

    uint64_t read_le(uint64_t addr, unsigned size) const
    {
        uint64_t v = 0;
        for (unsigned i = 0; i < size; ++i) {
            auto it = mem.find(addr + i);
            uint8_t b = (it != mem.end()) ? it->second : 0;
            v |= (uint64_t(b) << (8 * i));
        }
        return v;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        if (auto* ext = trans.get_extension<smc::smc_axi_extension>()) {
            saw_extension  = true;
            last_source_id = ext->source_id;
            last_axi_id    = ext->axi_id;
            last_prot      = ext->prot;
            last_locked    = ext->is_locked;
        }
        const uint64_t addr = trans.get_address();
        const unsigned len  = trans.get_data_length();
        uint8_t* ptr = trans.get_data_ptr();
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            ++write_calls;
            for (unsigned i = 0; i < len; ++i) mem[addr + i] = ptr[i];
        } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            for (unsigned i = 0; i < len; ++i) {
                auto it = mem.find(addr + i);
                ptr[i] = (it != mem.end()) ? it->second : 0;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

#ifdef SMC_HAVE_PLIC
// PLIC lives high in the cluster MMIO carve-out so low MMIO addresses (used by
// other tests at 0x80000000+) stay on the InspectingRamStub.
static constexpr uint64_t PLIC_MMIO_BASE  = 0x88000000ULL;
static constexpr uint64_t PLIC_MMIO_SIZE  = smc::plic_cfg::WINDOW_SIZE;
static constexpr uint64_t PLIC_FLAG_DONE  = 0x0;
static constexpr uint64_t PLIC_FLAG_CLAIM = 0x38;

// ---------------------------------------------------------------------------
// MMIO demux: subtract PLIC base for the PLIC reg_socket (offset map).
// ---------------------------------------------------------------------------
class MmioPlicRouter : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<MmioPlicRouter, 64>       tgt{"tgt"};
    tlm_utils::simple_initiator_socket<MmioPlicRouter, 32>    plic_init{"plic_init"};
    tlm_utils::simple_initiator_socket<MmioPlicRouter, 64>    stub_init{"stub_init"};

    SC_HAS_PROCESS(MmioPlicRouter);
    explicit MmioPlicRouter(sc_core::sc_module_name n) : sc_module(n)
    {
        tgt.register_b_transport(this, &MmioPlicRouter::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        const uint64_t addr = gp.get_address();
        if (addr >= PLIC_MMIO_BASE && addr < PLIC_MMIO_BASE + PLIC_MMIO_SIZE) {
            if (gp.get_data_length() != 4) {
                gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
                return;
            }
            gp.set_address(addr - PLIC_MMIO_BASE);
            plic_init->b_transport(gp, delay);
            return;
        }
        stub_init->b_transport(gp, delay);
    }
};

// Hand-encoded RV64 firmware: CPU programs PLIC, WFI, ISR claim/complete.
static void load_plic_cpu_firmware(smc::iss_hart& h)
{
    constexpr uint64_t MAIN_PC  = 0x80;
    constexpr uint64_t TRAP_PC  = 0x200;
    constexpr uint64_t LIT_BASE = 0x400;

    const uint64_t reg_prio1 = PLIC_MMIO_BASE + 4u;               // source 1
    const uint64_t reg_en0   = PLIC_MMIO_BASE + 0x2000u;
    const uint64_t reg_thr0  = PLIC_MMIO_BASE + 0x200000u;
    const uint64_t reg_claim = PLIC_MMIO_BASE + 0x200004u;

    auto w64 = [&](uint64_t addr, uint64_t v) {
        ASSERT_TRUE(h.mem_write(addr, 8, v));
    };
    auto w32 = [&](uint64_t addr, uint32_t insn) {
        ASSERT_TRUE(h.mem_write(addr, 4, insn));
    };

    w64(LIT_BASE + 0, reg_prio1);
    w64(LIT_BASE + 8, reg_en0);
    w64(LIT_BASE + 16, reg_thr0);
    w64(LIT_BASE + 24, reg_claim);

    // x5=t0, x6=t1, x7=t2
    uint64_t pc = MAIN_PC;
    w32(pc, 0x40000293u);
    pc += 4; // addi t0, x0, 0x400
    w32(pc, 0x0002b303u);
    pc += 4; // ld   t1, 0(t0)   -> prio reg ptr
    w32(pc, 0x00700393u);
    pc += 4; // addi t2, x0, 7
    w32(pc, 0x00732023u);
    pc += 4; // sw   t2, 0(t1)
    w32(pc, 0x0082b303u);
    pc += 4; // ld   t1, 8(t0)   -> enable reg ptr
    w32(pc, 0x00200393u);
    pc += 4; // addi t2, x0, 2   (enable source 1)
    w32(pc, 0x00732023u);
    pc += 4; // sw   t2, 0(t1)
    w32(pc, 0x0102b303u);
    pc += 4; // ld   t1, 16(t0)  -> threshold reg ptr
    w32(pc, 0x00032023u);
    pc += 4; // sw   x0, 0(t1)
    w32(pc, 0x20000293u);
    pc += 4; // addi t0, x0, TRAP_PC (0x200)
    w32(pc, 0x30529073u);
    pc += 4; // csrrw x0, mtvec, t0
    w32(pc, 0x00100393u);
    pc += 4; // addi t2, x0, 1
    w32(pc, 0x00b39393u);
    pc += 4; // slli t2, t2, 11  -> MIE.MEIE
    w32(pc, 0x3043a073u);
    pc += 4; // csrrs x0, mie, t2
    w32(pc, 0x00800393u);
    pc += 4; // addi t2, x0, 8   -> MSTATUS.MIE
    w32(pc, 0x3003a073u);
    pc += 4; // csrrs x0, mstatus, t2
    w32(pc, 0x10500073u);
    pc += 4; // wfi

    // Trap @ TRAP_PC: claim, record ID @ 0x8, complete, set 0x0 done, mret.
    pc = TRAP_PC;
    w32(pc, 0x40000293u);
    pc += 4; // addi t0, x0, LIT_BASE
    w32(pc, 0x0182b303u);
    pc += 4; // ld   t1, 24(t0)
    w32(pc, 0x00032e03u);
    pc += 4; // lw   t3, 0(t1)   claim ID (32-bit; PLIC rejects ld)
    w32(pc, 0x03c02c23u);
    pc += 4; // sw   t3, PLIC_FLAG_CLAIM(x0)
    w32(pc, 0x01c32023u);
    pc += 4; // sw   t3, 0(t1)   complete
    w32(pc, 0x00100e13u);
    pc += 4; // addi t3, x0, 1
    w32(pc, 0x01c02023u);
    pc += 4; // sw   t3, 0(x0)
    w32(pc, 0x30200073u);
    pc += 4; // mret
}
#endif  // SMC_HAVE_PLIC

// ---------------------------------------------------------------------------
// Scratchpad SRAM stub (TC-CPU-004 / TC-CPU-005)
// Models auto-zero on reset and drives cluster INIT_MEM_DONE via set_init_mem_done().
// ---------------------------------------------------------------------------
class ScratchpadSramStub : public sc_core::sc_module
{
public:
    static constexpr uint64_t BASE = 0xC0060000ULL;
    static constexpr size_t   SIZE = 4096;

    sc_core::sc_in<bool> rst_n_i{"rst_n_i"};

    SC_HAS_PROCESS(ScratchpadSramStub);
    explicit ScratchpadSramStub(sc_core::sc_module_name n, smc::smc_cpu_cluster& cluster)
        : sc_core::sc_module(n)
        , cluster_(cluster)
        , mem_(SIZE, 0xA5)
    {
        SC_METHOD(reset_method);
        sensitive << rst_n_i;
        dont_initialize();
        SC_THREAD(init_thread);
    }

    uint8_t peek(size_t off) const
    {
        return (off < mem_.size()) ? mem_[off] : 0;
    }

    void seed(uint8_t pattern)
    {
        std::fill(mem_.begin(), mem_.end(), pattern);
    }

private:
    smc::smc_cpu_cluster&   cluster_;
    std::vector<uint8_t>    mem_;
    sc_core::sc_event       release_ev_;
    bool                    arm_release_ = false;

    void reset_method()
    {
        if (!rst_n_i.read()) {
            cluster_.set_init_mem_done(false);
            arm_release_ = true;
        } else if (arm_release_) {
            arm_release_ = false;
            release_ev_.notify(sc_core::SC_ZERO_TIME);
        }
    }

    void init_thread()
    {
        while (true) {
            sc_core::wait(release_ev_);
            if (cluster_.disable_sram_autoinit() != 0u) {
                continue;
            }
            std::fill(mem_.begin(), mem_.end(), 0);
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
            cluster_.set_init_mem_done(true);
        }
    }
};

// ---------------------------------------------------------------------------
// Test bench root (one elaboration, one sc_start — same pattern as plic_tb)
// ---------------------------------------------------------------------------
struct cluster_tb_top : sc_core::sc_module
{
    static constexpr unsigned NHARTS   = 4;
    static constexpr uint64_t RESET_PC = 0x80;

    smc::smc_cpu_cluster::config cfg_;
    smc::smc_cpu_cluster           cluster;
    smc_test::TlmRamStub           bus_data;
    InspectingRamStub              bus_mmio;
    smc_test::TlmRamStub           bus_ifetch;
    smc_test::ctrl_initiator       ctrl;

    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_sw;
    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_timer;
    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_ext;

    smc_test::Watchdog wd;

    ScratchpadSramStub           scratchpad;
    sc_core::sc_signal<bool>     sram_rst_n;

#ifdef SMC_HAVE_PLIC
    static constexpr unsigned PLIC_NSRC = 8;
    static constexpr unsigned PLIC_NCTX = 1;
    smc::plic                                    plic_dut;
    MmioPlicRouter                               mmio_router;
    sc_core::sc_signal<bool>                     rst_n;
    sc_core::sc_vector<sc_core::sc_signal<bool>> src_sigs;
    sc_core::sc_vector<sc_core::sc_signal<bool>> ctx_sig;
#endif

    SC_HAS_PROCESS(cluster_tb_top);

    explicit cluster_tb_top(sc_core::sc_module_name n)
        : sc_module(n)
        , cfg_(smc_test::make_default_cluster_cfg(NHARTS, RESET_PC))
        , cluster("cluster", cfg_)
        , bus_data("bus_data")
        , bus_mmio("bus_mmio")
        , bus_ifetch("bus_ifetch")
        , ctrl("ctrl")
        , sig_sw("sig_sw", NHARTS)
        , sig_timer("sig_timer", NHARTS)
        , sig_ext("sig_ext", NHARTS)
        , wd("wd", sc_core::sc_time(120, sc_core::SC_MS), "cluster_tb")
        , scratchpad("scratchpad", cluster)
        , sram_rst_n("sram_rst_n")
#ifdef SMC_HAVE_PLIC
        , plic_dut("plic", [] {
              smc::plic_cfg c;
              c.num_sources  = PLIC_NSRC;
              c.num_contexts = PLIC_NCTX;
              return c;
          }())
        , mmio_router("mmio_router")
        , rst_n("rst_n")
        , src_sigs("src_sigs", PLIC_NSRC)
        , ctx_sig("ctx_sig", PLIC_NCTX)
#endif
    {
        cluster.data.bind(bus_data.socket);
#ifdef SMC_HAVE_PLIC
        cluster.mmio.bind(mmio_router.tgt);
        mmio_router.plic_init.bind(plic_dut.reg_socket);
        mmio_router.stub_init.bind(bus_mmio.socket);
#else
        cluster.mmio.bind(bus_mmio.socket);
#endif
        cluster.ifetch.bind(bus_ifetch.socket);
        ctrl.socket.bind(cluster.ctrl);

        scratchpad.rst_n_i.bind(sram_rst_n);
        sram_rst_n.write(true);

        for (unsigned i = 0; i < NHARTS; ++i) {
            sig_sw[i].write(false);
            sig_timer[i].write(false);
            sig_ext[i].write(false);
            cluster.irq_sw[i].bind(sig_sw[i]);
            cluster.irq_timer[i].bind(sig_timer[i]);
        }
        // irq_ext[0] reserved for PLIC ctx_out when integration is enabled.
        for (unsigned i = 1; i < NHARTS; ++i) {
            cluster.irq_ext[i].bind(sig_ext[i]);
        }
#ifndef SMC_HAVE_PLIC
        cluster.irq_ext[0].bind(sig_ext[0]);
#endif

#ifdef SMC_HAVE_PLIC
        rst_n.write(true);
        plic_dut.rst_n_i.bind(rst_n);
        for (unsigned i = 0; i < PLIC_NSRC; ++i) {
            src_sigs[i].write(false);
            plic_dut.src_in[i].bind(src_sigs[i]);
        }
        for (unsigned c = 0; c < PLIC_NCTX; ++c) {
            plic_dut.ctx_out[c].bind(ctx_sig[c]);
        }
        cluster.irq_ext[0].bind(ctx_sig[0]);
#endif

        // Harts start stepping at elaboration; seed benign code before sc_start.
        for (unsigned i = 0; i < NHARTS; ++i) {
            cluster.hart(i).mem_write(0x0, 4, smc_test::OP_J_SELF);
            cluster.hart(i).mem_write(RESET_PC, 4, smc_test::OP_J_SELF);
        }

        SC_THREAD(run);
    }

    void run()
    {
        std::cout << "==== SMC CPU Cluster TB ====\n";

        // Unit tests drive hart 0 only; park the others to avoid stray TLM traffic.
        ctrl.write32(0x040, 0x1u);

        // --- 4-hart construction ------------------------------------------------
        EXPECT_EQ(cluster.num_harts(), NHARTS);
        EXPECT_EQ(cluster.whisper_system().hartCount(), NHARTS);
        for (unsigned i = 0; i < NHARTS; ++i) {
            EXPECT_EQ(cluster.hart(i).read_csr(smc_test::CSR_MHARTID), i);
            EXPECT_FALSE(cluster.hart(i).is_wfi());
        }
        pass("4-hart construction and binding");

        // --- reset-state matrix -------------------------------------------------
        for (unsigned i = 0; i < NHARTS; ++i) cluster.hart(i).reset();
        for (unsigned i = 0; i < NHARTS; ++i) {
            const auto& h = cluster.hart(i);
            EXPECT_EQ(h.get_pc(), RESET_PC);
            EXPECT_EQ(h.read_csr(smc_test::CSR_MHARTID), i);
            EXPECT_NE(h.read_csr(smc_test::CSR_MISA), 0u);
            EXPECT_FALSE(h.is_wfi());
        }
        pass("reset-state matrix (4 harts)");

        // --- memory routing (hart 0) ------------------------------------------
        {
            constexpr uint64_t MMIO = 0x80000000ULL;
            cluster.hart(0).reset();
            smc_test::smc_master m(cluster.hart(0));

            ASSERT_TRUE(m.write8(0x100, 0xA5));
            EXPECT_EQ(m.read8(0x100), 0xA5u);
            ASSERT_TRUE(m.write16(0x110, 0xBEEF));
            EXPECT_EQ(m.read16(0x110), 0xBEEFu);
            ASSERT_TRUE(m.write32(0x120, 0xDEADBEEFu));
            EXPECT_EQ(m.read32(0x120), 0xDEADBEEFu);
            ASSERT_TRUE(m.write64(0x130, 0x0123456789ABCDEFULL));
            EXPECT_EQ(m.read64(0x130), 0x0123456789ABCDEFULL);

            ASSERT_TRUE(m.write8(MMIO + 0x40, 0x5A));
            EXPECT_EQ(bus_mmio.read_le(MMIO + 0x40, 1), 0x5Au);
            ASSERT_TRUE(m.write16(MMIO + 0x50, 0xFEED));
            EXPECT_EQ(bus_mmio.read_le(MMIO + 0x50, 2), 0xFEEDu);
            ASSERT_TRUE(m.write32(MMIO + 0x60, 0xCAFEBABEu));
            EXPECT_EQ(bus_mmio.read_le(MMIO + 0x60, 4), 0xCAFEBABEu);
            ASSERT_TRUE(m.write64(MMIO + 0x70, 0x0011223344556677ULL));
            EXPECT_EQ(bus_mmio.read_le(MMIO + 0x70, 8), 0x0011223344556677ULL);

            bus_mmio.seed(MMIO + 0x140, 0xC3, 1);
            bus_mmio.seed(MMIO + 0x150, 0xABCD, 2);
            bus_mmio.seed(MMIO + 0x160, 0xDEADBEEFULL, 4);
            bus_mmio.seed(MMIO + 0x170, 0x1122334455667788ULL, 8);
            EXPECT_EQ(m.read8(MMIO + 0x140), 0xC3u);
            EXPECT_EQ(m.read16(MMIO + 0x150), 0xABCDu);
            EXPECT_EQ(m.read32(MMIO + 0x160), 0xDEADBEEFu);
            EXPECT_EQ(m.read64(MMIO + 0x170), 0x1122334455667788ULL);
            pass("memory routing (fast-mem + MMIO)");
        }

        // --- smc_axi_extension (hart 0) ---------------------------------------
        {
            constexpr uint64_t MMIO = 0x80000000ULL;
            auto& h = cluster.hart(0);
            h.reset();
            smc_test::smc_master m(h);
            bus_mmio.saw_extension = false;
            bus_mmio.last_locked   = false;

            ASSERT_TRUE(m.write32(MMIO + 0x100, 0xCAFEBABEu));
            EXPECT_TRUE(bus_mmio.saw_extension);
            EXPECT_EQ(bus_mmio.last_source_id, cfg_.source_id);
            EXPECT_EQ(bus_mmio.last_axi_id, 0u);
            EXPECT_FALSE(bus_mmio.last_locked);

            constexpr uint32_t OP_ADDI_X1_55 = 0x05500093u;
            constexpr uint32_t OP_ADDI_X2_40 = 0x04000113u;
            constexpr uint32_t OP_AMOSWAP_W  = 0x0811202Fu;
            h.mem_write(0x80, 4, OP_ADDI_X1_55);
            h.mem_write(0x84, 4, OP_ADDI_X2_40);
            h.mem_write(0x88, 4, OP_AMOSWAP_W);
            h.mem_write(0x8C, 4, smc_test::OP_J_SELF);
            h.set_reset_pc(0x80);
            h.reset();
            bus_mmio.saw_extension = false;
            bus_mmio.last_locked   = false;
            h.step();
            h.step();
            h.step();
            ASSERT_FALSE(h.last_commit().trapped);
            ASSERT_EQ(h.last_commit().opcode, OP_AMOSWAP_W);
            ASSERT_TRUE(m.write32(MMIO + 0x200, 0xFEEDF00Du));
            EXPECT_TRUE(bus_mmio.last_locked);
            pass("smc_axi_extension on MMIO transactions");
        }

        // --- step / WFI (hart 0) ------------------------------------------------
        {
            auto& h = cluster.hart(0);
            const uint32_t program[] = {
                smc_test::OP_NOP, smc_test::OP_ADDI_X1_X0_5, smc_test::OP_ADDI_X2_X0_10,
                smc_test::OP_WFI,
            };
            for (unsigned k = 0; k < sizeof(program) / sizeof(program[0]); ++k) {
                ASSERT_TRUE(h.mem_write(RESET_PC + 4 * k, 4, program[k]));
            }
            h.reset();
            EXPECT_EQ(h.get_pc(), RESET_PC);
            h.step();
            EXPECT_EQ(h.get_pc(), RESET_PC + 0x04);
            h.step();
            EXPECT_EQ(h.get_pc(), RESET_PC + 0x08);
            h.step();
            EXPECT_EQ(h.get_pc(), RESET_PC + 0x0C);
            h.step();
            const uint64_t pc_after_wfi = h.get_pc();
            EXPECT_TRUE(h.is_wfi());
            h.step();
            EXPECT_EQ(h.get_pc(), pc_after_wfi);
            h.poke_mip(smc_test::MIP_MTIP);
            EXPECT_FALSE(h.is_wfi());
            pass("step() and WFI flag");
        }

        // --- IRQ aggregator (needs sc_start) ----------------------------------
        {
            ctrl.write32(0x040, 0xFu);
            auto& h0 = cluster.hart(0);
            h0.mem_write(RESET_PC + 0, 4, smc_test::OP_CSRW_MSTATUS_0);
            h0.mem_write(RESET_PC + 4, 4, smc_test::OP_CSRW_MIE_0);
            h0.mem_write(RESET_PC + 8, 4, smc_test::OP_NOP);
            h0.mem_write(RESET_PC + 12, 4, smc_test::OP_WFI);
            for (unsigned k = 0; k < 32; ++k) {
                h0.mem_write(RESET_PC + 16 + 4 * k, 4, smc_test::OP_NOP);
            }
            for (unsigned i = 0; i < NHARTS; ++i) cluster.hart(i).reset();

            wait(sc_core::sc_time(20, sc_core::SC_MS));
            for (unsigned i = 0; i < NHARTS; ++i) EXPECT_TRUE(cluster.hart(i).is_wfi());

            const uint64_t pc0_at_park = cluster.hart(0).get_pc();
            sig_timer[0].write(true);
            wait(sc_core::sc_time(2000, sc_core::SC_NS));
            EXPECT_NE(cluster.hart(0).read_csr(smc_test::CSR_MIP) & smc_test::MIP_MTIP, 0u);
            EXPECT_FALSE(cluster.hart(0).is_wfi());
            sig_timer[0].write(false);
            wait(sc_core::sc_time(100, sc_core::SC_NS));

            const uint64_t pc1_at_park = cluster.hart(1).get_pc();
            sig_sw[1].write(true);
            wait(sc_core::sc_time(2000, sc_core::SC_NS));
            EXPECT_NE(cluster.hart(1).read_csr(smc_test::CSR_MIP) & smc_test::MIP_MSIP, 0u);
            EXPECT_FALSE(cluster.hart(1).is_wfi());
            sig_sw[1].write(false);
            wait(sc_core::sc_time(100, sc_core::SC_NS));

            const uint64_t pc2_at_park = cluster.hart(2).get_pc();
            sig_ext[2].write(true);
            wait(sc_core::sc_time(2000, sc_core::SC_NS));
            EXPECT_NE(cluster.hart(2).read_csr(smc_test::CSR_MIP) & smc_test::MIP_MEIP, 0u);
            EXPECT_FALSE(cluster.hart(2).is_wfi());
            EXPECT_NE(cluster.hart(2).get_pc(), pc2_at_park);
            sig_ext[2].write(false);
            wait(sc_core::sc_time(100, sc_core::SC_NS));
            EXPECT_TRUE(cluster.hart(3).is_wfi());
            (void)pc0_at_park;
            (void)pc1_at_park;
            pass("IRQ aggregator and WFI wake");
        }

#ifdef SMC_HAVE_PLIC
        // --- CPU -> PLIC -> CPU (MMIO configure, irq_ext, ISR claim/complete) ---
        {
            ctrl.write32(0x040, 0x1u);
            auto& h0 = cluster.hart(0);
            h0.mem_write(PLIC_FLAG_DONE, 4, 0u);
            h0.mem_write(PLIC_FLAG_CLAIM, 4, 0u);
            load_plic_cpu_firmware(h0);
            h0.reset();
            EXPECT_EQ(h0.get_pc(), RESET_PC);

            bool parked = false;
            for (unsigned k = 0; k < 3000u && !parked; ++k) {
                wait(sc_core::sc_time(10, sc_core::SC_US));
                parked = h0.is_wfi();
            }
            EXPECT_TRUE(parked);
            EXPECT_EQ(plic_dut.dbg_priority(1), 7u);
            EXPECT_TRUE(plic_dut.dbg_enable(0, 1));

            src_sigs[0].write(true);

            bool done = false;
            for (unsigned k = 0; k < 2000u && !done; ++k) {
                wait(sc_core::sc_time(50, sc_core::SC_US));
                uint64_t flag = 0;
                if (!h0.mem_read(0x0, 4, flag)) continue;
                done = (flag == 1u);
            }
            EXPECT_TRUE(done);

            src_sigs[0].write(false);
            wait(sc_core::sc_time(2, sc_core::SC_MS));
            EXPECT_FALSE(plic_dut.dbg_pending(1));
            EXPECT_FALSE(ctx_sig[0].read());
            pass("CPU configures PLIC, handles MEIP (CPU->PLIC->CPU)");
        }
#else
        std::cout << "  [SKIP] PLIC integration (SMC_BUILD_PLIC_INTEGRATION=OFF)\n";
#endif

        // --- Scratchpad SRAM init handshake (TC-CPU-004 / TC-CPU-005) -----------
        {
            auto pulse_sram_reset = [&]() {
                sram_rst_n.write(false);
                wait(sc_core::sc_time(1, sc_core::SC_NS));
                sram_rst_n.write(true);
            };

            EXPECT_EQ(ctrl.read32(0x200), 0u);

            // TC-CPU-004: auto-init enabled -> INIT_MEM_DONE asserts after reset.
            ctrl.write32(0x204, 0u);
            EXPECT_EQ(ctrl.read32(0x204), 0u);
            pulse_sram_reset();
            bool done = false;
            for (unsigned k = 0; k < 500u && !done; ++k) {
                wait(sc_core::sc_time(10, sc_core::SC_US));
                done = (ctrl.read32(0x200) == 1u);
            }
            EXPECT_TRUE(done);
            EXPECT_EQ(scratchpad.peek(0), 0u);
            EXPECT_EQ(scratchpad.peek(128), 0u);

            // TC-CPU-005: auto-init disabled -> INIT_MEM_DONE never asserts.
            scratchpad.seed(0xA5u);
            ctrl.write32(0x204, 1u);
            EXPECT_EQ(ctrl.read32(0x204), 1u);
            pulse_sram_reset();
            wait(sc_core::sc_time(200, sc_core::SC_US));
            EXPECT_EQ(ctrl.read32(0x200), 0u);
            EXPECT_EQ(scratchpad.peek(0), 0xA5u);
            EXPECT_EQ(scratchpad.peek(256), 0xA5u);

            pass("scratchpad SRAM INIT_MEM_DONE handshake");
        }

        // --- CPU-Control register file ------------------------------------------
        {
            ctrl.write32(0x040, 0xFu);
            constexpr uint64_t ALT_VECTOR = 0x200ULL;
            cluster.hart(0).mem_write(RESET_PC, 4, smc_test::OP_J_SELF);
            cluster.hart(0).mem_write(ALT_VECTOR, 4, smc_test::OP_J_SELF);

            ctrl.write64(0x000, ALT_VECTOR);
            EXPECT_EQ(cluster.reset_vector_n(0), ALT_VECTOR);
            EXPECT_EQ(ctrl.read32(0x100), uint32_t(cfg_.local_base_default));

            cluster.set_init_mem_done(true);
            EXPECT_EQ(ctrl.read32(0x200), 1u);
            cluster.set_init_mem_done(false);
            EXPECT_EQ(ctrl.read32(0x204), 1u);

            EXPECT_EQ(ctrl.read32(0x040), 0xFu);
            wait(sc_core::sc_time(2, sc_core::SC_US));
            const uint64_t pc1_a = cluster.hart(1).get_pc();
            ctrl.write32(0x040, 0xDu);
            wait(sc_core::sc_time(5, sc_core::SC_US));
            EXPECT_EQ(cluster.hart(1).get_pc(), pc1_a);
            ctrl.write32(0x040, 0xFu);
            wait(sc_core::sc_time(5, sc_core::SC_US));
            pass("CPU-Control register file");
        }

        // --- ctrl negative paths + CSR edge cases (coverage) ------------------
        {
            ctrl.write32(0x040, 0xFu);

            EXPECT_EQ(ctrl.read32(0x050), 0u);
            ctrl.write32(0x050, 0xDEADBEEFu);
            EXPECT_EQ(ctrl.read32(0x050), 0u);

            (void)ctrl.read64(0x020);
            EXPECT_EQ(ctrl.last_response(), tlm::TLM_ADDRESS_ERROR_RESPONSE);

            EXPECT_EQ(ctrl.read64(0x000), cluster.reset_vector_n(0));
            EXPECT_EQ(cluster.reset_vector_n(99), 0u);

            ctrl.write32(0x104, 0x12345678u);
            ctrl.write32(0x108, 0x02000000u);
            EXPECT_EQ(ctrl.read32(0x104), 0x12345678u);
            EXPECT_EQ(ctrl.read32(0x108), 0x02000000u);

            const uint32_t lb = ctrl.read32(0x100);
            ctrl.write32(0x100, 0xFFFFFFFFu);
            EXPECT_EQ(ctrl.read32(0x100), lb);

            cluster.set_mem_repair_status(0x42u);
            EXPECT_EQ(ctrl.read32(0x208), 0x42u);

            cluster.inject_nmi(99);
            cluster.inject_nmi(0, 0x42u);

            auto& h0 = cluster.hart(0);
            {
                smc_test::stderr_guard quiet_whisper;
                EXPECT_FALSE(cluster.load_elf({"/nonexistent/smc_cluster_test.elf"}));
                EXPECT_FALSE(h0.load_elf({"/nonexistent/smc_cluster_test.elf"}));
            }
            h0.inject_nmi(0x11u);
            uint64_t junk = 0;
            EXPECT_FALSE(h0.mem_read(0, 3, junk));
            EXPECT_FALSE(h0.mem_write(0, 3, 0));

            ctrl.write32(0x040, 0x0u);
            wait(sc_core::sc_time(50, sc_core::SC_US));
            ctrl.write32(0x040, 0xFu);
            wait(sc_core::sc_time(50, sc_core::SC_US));

            pass("CPU-Control negative paths and CSR edge cases");
        }

        // --- batch step + data-path error (coverage) --------------------------
        {
            ctrl.write32(0x040, 0x1u);
            auto& h = cluster.hart(0);
            h.reset();
            smc_test::smc_master m(h);

            EXPECT_TRUE(h.step(10) >= 1u);

            bus_data.fail_next = true;
            EXPECT_FALSE(m.read32(0x90000000ULL));
            bus_data.fail_next = false;

            pass("batch step and data-path TLM error handling");
        }

        wd.cancel();
        if (g_failures == 0) {
            std::cout << "\nALL TESTS PASSED\n";
        } else {
            std::cout << '\n' << g_failures << " FAILURE(S)\n";
        }
        sc_core::sc_stop();
    }
};

int sc_main(int, char**)
{
    {
        WdRiscv::System<uint64_t> sys(1u, 1u, 1u, 1ull << 20, size_t(4096));
        if (sys.hartCount() != 1u) {
            std::cerr << "FAIL smoke: hartCount\n";
            return 1;
        }
        if (!sys.ithHart(0)) {
            std::cerr << "FAIL smoke: ithHart\n";
            return 1;
        }
        std::cout << "  [PASS] smoke: Whisper System constructs\n";
    }

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    cluster_tb_top top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}

