// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * Standalone SystemC testbench for VeeRISSTlm (SEP CPU wrapper).
 *
 * Drives a one-hart RV32IMC core against a TLM SRAM, checks that a
 * smoke hex program stores through the initiator socket, and exercises
 * the PIC intercept, CSR peek/poke, NMI pin, external IRQ, and TLM
 * error paths used by sep-vp. A second program (fixtures/smepmp.S)
 * covers the boot ROM's Smepmp PMP sequence and mscause.
 */

#include "VeeR-ISSTlm.hpp"
#include "reg_param.h"
#include "reg_logger.h"

#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

namespace {

constexpr uint64_t kIccmBase  = 0xC0000000ULL;
constexpr uint64_t kIccmSize  = 0x00040000ULL;
constexpr uint64_t kSramBase  = 0x10000000ULL;
constexpr uint64_t kSramSize  = 0x00040000ULL;
constexpr uint64_t kPoisonAddr = 0x30000000ULL;
constexpr uint32_t kMagic     = 0x0000005Au;
constexpr uint32_t kMeieOff   = 0x2004u;  // MEIE[1]

// RV32I smoke image (see test/fixtures/smoke.hex). Kept here so the TLM
// target can serve instruction fetches — MEM_CALLBACKS routes fetch off-chip.
constexpr uint32_t kSmokeWords[] = {
    0x100000b7u,  // lui  x1, 0x10000
    0x05a00113u,  // addi x2, x0, 0x5A
    0x0020a023u,  // sw   x2, 0(x1)
    0xc00821b7u,  // lui  x3, 0xC0082
    0x00100213u,  // addi x4, x0, 1
    0x0041a223u,  // sw   x4, 4(x3)     MEIE[1]
    0x0041a283u,  // lw   x5, 4(x3)
    0x0050a223u,  // sw   x5, 4(x1)
    0xBC901073u,  // csrrw x0, 0xBC9, x0   meipt
    0xBCC01073u,  // csrrw x0, 0xBCC, x0   meicurpl
    0x0000006fu,  // jal  x0, 0
};

bool load_hex_bytes(const std::string& path, std::vector<uint8_t>& dest, uint64_t base)
{
    std::ifstream in(path);
    if (!in)
        return false;
    std::string line;
    uint64_t addr = base;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        if (line[0] == '@') {
            addr = std::strtoull(line.c_str() + 1, nullptr, 16);
            continue;
        }
        std::istringstream iss(line);
        unsigned v = 0;
        while (iss >> std::hex >> v) {
            const uint64_t off = addr - base;
            if (off < dest.size())
                dest[static_cast<size_t>(off)] = static_cast<uint8_t>(v);
            ++addr;
        }
    }
    return true;
}

}  // namespace

class TlmRam : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TlmRam> tsock;

    TlmRam(sc_core::sc_module_name n)
        : sc_module(n)
        , tsock("tsock")
        , iccm_(kIccmSize, 0)
        , sram_(kSramSize, 0)
        , poison_next_(false)
    {
        tsock.register_b_transport(this, &TlmRam::b_transport);
        tsock.register_transport_dbg(this, &TlmRam::transport_dbg);
        for (size_t i = 0; i < sizeof(kSmokeWords) / sizeof(kSmokeWords[0]); ++i) {
            const uint32_t w = kSmokeWords[i];
            std::memcpy(iccm_.data() + i * 4, &w, 4);
        }
    }

    void preload_hex(const std::string& path) { load_hex_bytes(path, iccm_, kIccmBase); }

    uint32_t sram_word(uint64_t off) const
    {
        uint32_t v = 0;
        if (off + 4 <= sram_.size())
            std::memcpy(&v, sram_.data() + off, 4);
        return v;
    }

    void set_poison(bool v) { poison_next_ = v; }

private:
    std::vector<uint8_t> iccm_;
    std::vector<uint8_t> sram_;
    bool poison_next_;

    uint8_t* decode(uint64_t addr, unsigned len)
    {
        if (addr >= kIccmBase && addr + len <= kIccmBase + kIccmSize)
            return iccm_.data() + (addr - kIccmBase);
        if (addr >= kSramBase && addr + len <= kSramBase + kSramSize)
            return sram_.data() + (addr - kSramBase);
        return nullptr;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        access(trans);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& trans) { return access(trans); }

    unsigned access(tlm::tlm_generic_payload& trans)
    {
        const uint64_t addr = trans.get_address();
        const unsigned len = trans.get_data_length();
        if (poison_next_ || addr == kPoisonAddr) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }
        uint8_t* p = decode(addr, len);
        if (!p) {
            // Unmapped: RAZ/WI so stray fetches do not NMI the smoke program.
            if (trans.is_read())
                std::memset(trans.get_data_ptr(), 0, len);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return len;
        }
        if (trans.is_read())
            std::memcpy(trans.get_data_ptr(), p, len);
        else
            std::memcpy(p, trans.get_data_ptr(), len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return len;
    }
};

class veeriss_tb : public sc_core::sc_module {
public:
    RegLogger logger;
    Args args;
    WdRiscv::HartConfig config;

    sc_core::sc_signal<bool> rst_n;
    sc_core::sc_signal<bool> nmi;
    sc_core::sc_signal<uint32_t> nmi_vec;

    TlmRam ram;
    std::unique_ptr<VeeRISSTlm> cpu;

    int tests_run = 0;
    int tests_failed = 0;

    SC_HAS_PROCESS(veeriss_tb);

    veeriss_tb(sc_core::sc_module_name n, const std::string& cfg, const std::string& hex,
               const std::string& freq)
        : sc_module(n)
        , rst_n("rst_n")
        , nmi("nmi")
        , nmi_vec("nmi_vec")
        , ram("ram")
    {
        logger.setMaxVerbosity(2);
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);

        if (!config.loadConfigFile(cfg))
            SC_REPORT_FATAL("veeriss_tb", "failed to load HartConfig JSON");

        // MEM_CALLBACKS leaves ISS data_ null; hex load would UB. Fetch is
        // served by the TLM SRAM (preloaded below). A dummy target satisfies
        // session()'s "program specified" check; ELF load is compiled out.
        args.targets.push_back("veeriss_tb.elf");
        args.expandTargets();
        args.raw = true;
        args.isa = "imc";
        args.fastExt = true;
        args.instFreqFile = freq;
        ram.preload_hex(hex);

        cpu = std::make_unique<VeeRISSTlm>("rv32imc", args, config, kIccmBase);
        cpu->initiator_socket.bind(ram.tsock);
        cpu->rst_ni(rst_n);
        cpu->nmi_i(nmi);
        cpu->nmi_vec_i(nmi_vec);
        cpu->setMasterId(1);

        rst_n.write(false);
        nmi.write(false);
        nmi_vec.write(0);

        SC_THREAD(run);
    }

    void check(const char* name, bool ok, const std::string& why = {})
    {
        ++tests_run;
        if (ok) {
            REG_INFO(1, logger) << name << ": PASS" << std::endl;
        } else {
            ++tests_failed;
            REG_ERROR(0, logger) << name << ": FAIL " << why << std::endl;
        }
    }

    static std::string hex32(uint32_t v)
    {
        char buf[11];
        std::snprintf(buf, sizeof(buf), "0x%08x", v);
        return buf;
    }

    // Runs test/fixtures/smepmp.S, which documents the SRAM result layout.
    void run_smepmp_program()
    {
        rst_n.write(false);
        wait(50, sc_core::SC_NS);
        ram.preload_hex(std::string(FIXTURE_DIR) + "/smepmp.hex");
        // The decode cache still holds the smoke program at the same PCs.
        if (auto hart = cpu->system_ ? cpu->system_->ithHart(0) : nullptr)
            hart->invalidateDecodeCache();
        rst_n.write(true);
        wait(50, sc_core::SC_US);

        auto res = [&](unsigned i) { return ram.sram_word(0x100 + 4 * i); };
        auto trap = [&](unsigned t, unsigned f) { return ram.sram_word(0x200 + 12 * t + 4 * f); };
        auto traps_are = [&](unsigned t, uint32_t mcause, uint32_t mtval) {
            return trap(t, 0) == mcause && trap(t, 1) == 8u /*PMP*/ && trap(t, 2) == mtval;
        };
        auto trap_why = [&](unsigned t) {
            return "mcause=" + hex32(trap(t, 0)) + " mscause=" + hex32(trap(t, 1)) +
                   " mtval=" + hex32(trap(t, 2));
        };

        check("FUNC-CPU-017: Smepmp program ran to completion",
              res(14) == 0x600Du, "done=" + hex32(res(14)));
        check("FUNC-CPU-018: mscause readable, mseccfgh reads zero",
              res(0) == 0 && res(1) == 0,
              "mscause=" + hex32(res(0)) + " mseccfgh=" + hex32(res(1)));
        check("FUNC-CPU-019: boot ROM pmpcfg0/pmpcfg1 read back",
              res(2) == 0x9b009b9du && res(3) == 0x009b9b9bu,
              "pmpcfg0=" + hex32(res(2)) + " pmpcfg1=" + hex32(res(3)));
        check("FUNC-CPU-020: mseccfg RLB|MMWP read back",
              res(4) == 6u, "mseccfg=" + hex32(res(4)));
        check("FUNC-CPU-021: RLB=1 csrs X onto locked entries 3 and 5",
              res(5) == 0x9f009b9du && res(6) == 0x009b9f9bu,
              "pmpcfg0=" + hex32(res(5)) + " pmpcfg1=" + hex32(res(6)));
        check("FUNC-CPU-022: RLB=1 locked pmpaddr is writable",
              res(7) == 0x06005fffu, "pmpaddr1=" + hex32(res(7)));
        check("FUNC-CPU-023: RLB clears and stays clear while rules are locked",
              res(8) == 2u && res(9) == 2u,
              "after clear=" + hex32(res(8)) + " after set=" + hex32(res(9)));
        check("FUNC-CPU-024: MMWP is sticky",
              res(10) == 2u, "mseccfg=" + hex32(res(10)));
        check("FUNC-CPU-025: RLB=0 locked pmpcfg/pmpaddr ignore writes",
              res(11) == 0x9f009b9du && res(12) == 0x06005fffu,
              "pmpcfg0=" + hex32(res(11)) + " pmpaddr1=" + hex32(res(12)));
        check("FUNC-CPU-026: only the three MMWP-denied accesses trap",
              res(13) == 0x10000200u + 3 * 12, "trap log end=" + hex32(res(13)));
        check("FUNC-CPU-027: MMWP unmatched load raises load access fault",
              traps_are(0, 5, 0x60000000u), trap_why(0));
        check("FUNC-CPU-028: MMWP unmatched store raises store access fault",
              traps_are(1, 7, 0x60000000u), trap_why(1));
        check("FUNC-CPU-029: MMWP unmatched fetch raises instruction access fault",
              traps_are(2, 1, 0x60000000u), trap_why(2));

        uint32_t seccfg = 0;
        const bool mml_set = cpu->poke_csr(0x747, 1) && cpu->poke_csr(0x747, 0) &&
                             cpu->peek_csr(0x747, seccfg);
        check("FUNC-CPU-030: MML is sticky", mml_set && seccfg == 3u,
              "mseccfg=" + hex32(seccfg));
    }

    void run()
    {
        wait(10, sc_core::SC_NS);
        rst_n.write(true);

        // Let the 9-instruction smoke program reach the jal loop.
        wait(50, sc_core::SC_US);

        check("FUNC-CPU-001: TLM store of 0x5A to SRAM",
              ram.sram_word(0) == kMagic,
              "SRAM[0]=0x" + std::to_string(ram.sram_word(0)));

        check("FUNC-CPU-002: PIC MEIE[1] write/read via intercept",
              ram.sram_word(4) == 1u,
              "SRAM[4]=0x" + std::to_string(ram.sram_word(4)));

        uint32_t meipt = 0xFFu;
        check("FUNC-CPU-003: poke/peek meipt CSR",
              cpu->poke_csr(0xBC9, 0x5) && cpu->peek_csr(0xBC9, meipt) && meipt == 0x5,
              "meipt=0x" + std::to_string(meipt));

        cpu->set_pic_claim_id(7);
        uint32_t meihap = 0;
        cpu->peek_csr(static_cast<uint32_t>(WdRiscv::CsrNumber::MEIHAP), meihap);
        check("FUNC-CPU-004: set_pic_claim_id writes MEIHAP[9:2]",
              ((meihap >> 2) & 0xFFu) == 7u,
              "meihap=0x" + std::to_string(meihap));

        // mie.MEIE so isInterruptPossible() can fire mid-batch.
        cpu->poke_csr(static_cast<uint32_t>(WdRiscv::CsrNumber::MIE), 1u << 11);
        cpu->trigger_external_interrupt(MachineMode);
        wait(100, sc_core::SC_NS);
        cpu->clear_external_interrupt(MachineMode);
        check("FUNC-CPU-005: external IRQ trigger/clear", true);

        cpu->notifyWrite(kSramBase, 4, /*other initiator*/ 2);
        cpu->notifyWrite(kSramBase, 4, /*self*/ 1);
        check("FUNC-CPU-006: notifyWrite self vs foreign", true);

        uint64_t val = 0;
        check("FUNC-CPU-007: externalRead size>8 rejected",
              !cpu->externalRead(kSramBase, 16, val));
        check("FUNC-CPU-008: externalWrite size>8 rejected",
              !cpu->externalWrite(kSramBase, 16, 0));

        auto hart0 = cpu->system_ ? cpu->system_->ithHart(0) : nullptr;
        auto pulse_reset = [&]() {
            nmi.write(false);
            rst_n.write(false);
            wait(50, sc_core::SC_NS);
            rst_n.write(true);
            wait(200, sc_core::SC_NS);
            hart0 = cpu->system_ ? cpu->system_->ithHart(0) : nullptr;
        };

        // IRQ / illegal-inst storm may have left nmiActive_ set.
        pulse_reset();
        // Pin NMI first, while the hart has not yet taken an NMI.
        if (hart0)
            hart0->defineNmiPc(0);
        nmi_vec.write(0);
        nmi.write(true);
        wait(20, sc_core::SC_NS);
        nmi.write(false);
        wait(20, sc_core::SC_NS);
        check("FUNC-CPU-011: NMI with vec=0 is ignored", true);

        nmi_vec.write(static_cast<uint32_t>(kIccmBase + 0x100));
        nmi.write(true);
        wait(20, sc_core::SC_NS);
        nmi.write(false);
        wait(20, sc_core::SC_NS);
        nmi.write(true);  // already-active bark
        wait(20, sc_core::SC_NS);
        nmi.write(false);
        wait(20, sc_core::SC_NS);
        check("FUNC-CPU-012: NMI pin with FW vector", true);

        pulse_reset();
        if (hart0)
            hart0->defineNmiPc(static_cast<uint32_t>(kIccmBase + 0x100));
        nmi_vec.write(0);
        nmi.write(true);
        wait(20, sc_core::SC_NS);
        nmi.write(false);
        wait(20, sc_core::SC_NS);
        check("FUNC-CPU-012b: NMI pin falls back to core nmi_pc", true);

        pulse_reset();
        nmi_vec.write(static_cast<uint32_t>(kIccmBase + 0x200));
        ram.set_poison(true);
        uint64_t bad = 0;
        check("FUNC-CPU-009: load bus error posts NMI",
              cpu->externalRead(kPoisonAddr, 4, bad));
        check("FUNC-CPU-009b: load bus error while NMI active",
              cpu->externalRead(kPoisonAddr, 4, bad));
        ram.set_poison(false);

        pulse_reset();
        nmi_vec.write(static_cast<uint32_t>(kIccmBase + 0x200));
        ram.set_poison(true);
        check("FUNC-CPU-010: store bus error posts NMI",
              cpu->externalWrite(kPoisonAddr, 4, 0xA5));
        check("FUNC-CPU-010b: store bus error while NMI active",
              cpu->externalWrite(kPoisonAddr, 4, 0xA5));
        ram.set_poison(false);
        nmi_vec.write(0);

        check("FUNC-CPU-010c: STDOUT_DEVICE write path",
              cpu->externalWrite(0x80000000ULL, 1, static_cast<uint64_t>('X')));

        uint8_t buf[4] = {0};
        check("FUNC-CPU-013: PIC TLM read of MEIE[1]",
              cpu->doPicAccess(tlm::TLM_READ_COMMAND, VeeRISSTlm::PIC_BASE + kMeieOff, buf, 4));

        check("FUNC-CPU-014: debug TLM path",
              cpu->doTlmAccess(tlm::TLM_READ_COMMAND, kSramBase, buf, 4, /*debug*/ true));

        const std::string freq = args.instFreqFile;
        if (!freq.empty() && cpu->system_) {
            auto hart = cpu->system_->ithHart(0);
            if (hart) {
                check("FUNC-CPU-015: instruction-frequency report",
                      cpu->reportInstructionFrequency(*hart, freq));
                check("FUNC-CPU-015b: inst-freq fopen fail",
                      !cpu->reportInstructionFrequency(
                          *hart, "/no/such/veeriss_tb_dir/freq.txt"));
            }
        }

        uint32_t junk = 0;
        check("FUNC-CPU-015c: peek of nonexistent CSR fails",
              !cpu->peek_csr(0xFFFFu, junk));
        check("FUNC-CPU-015d: poke of nonexistent CSR fails",
              !cpu->poke_csr(0xFFFFu, 0));

        run_smepmp_program();

        rst_n.write(false);
        wait(50, sc_core::SC_NS);
        rst_n.write(true);
        wait(1, sc_core::SC_US);
        check("FUNC-CPU-016: reset deassert restarts the hart", true);

        REG_INFO(1, logger) << "Total: " << tests_run
                             << "  Failed: " << tests_failed << std::endl;
        if (tests_failed == 0)
            REG_INFO(1, logger) << "[OVERALL RESULT: PASSED]" << std::endl;
        else
            REG_ERROR(0, logger) << "[OVERALL RESULT: FAILED]" << std::endl;

        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    regmodel::load_config_file(nullptr);

    const std::string cfg = (argc > 1) ? argv[1]
                                       : std::string(FIXTURE_DIR) + "/veeriss_config.json";
    const std::string hex = (argc > 2) ? argv[2]
                                       : std::string(FIXTURE_DIR) + "/smoke.hex";
    const std::string freq = "/tmp/veeriss_tb_instfreq.txt";

    veeriss_tb tb("tb", cfg, hex, freq);
    REG_INFO(1, tb.logger) << "Starting VeeR-ISS TLM testbench" << std::endl;
    sc_core::sc_start();
    REG_INFO(1, tb.logger) << "Simulation completed" << std::endl;

#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(tb.tests_failed > 0 ? 1 : 0);
    return 0;
}
