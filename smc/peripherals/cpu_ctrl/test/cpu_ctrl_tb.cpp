// SPDX-License-Identifier: Apache-2.0
//
// cpu_ctrl_tb.cpp -- self-checking test bench for SMC CPU Control (LT + CCI).

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "cpu_ctrl.h"

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
                      << "  expected=" << _e << " actual=" << _a               \
                      << "  (" #expected " == " #actual ")\n";                \
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

constexpr uint64_t BASE = smc::cpu_ctrl_cfg::DEFAULT_BASE_ADDR;

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    tlm_utils::tlm_quantumkeeper                 qk;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock")
    {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    void access(uint64_t addr, tlm::tlm_command cmd, void* data, unsigned len)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_byte_enable_length(0);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        qk.inc(delay);
        EXPECT_TRUE(gp.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t v = 0;
        access(addr, tlm::TLM_READ_COMMAND, &v, 4);
        return v;
    }

    uint64_t read64(uint64_t addr)
    {
        uint64_t v = 0;
        access(addr, tlm::TLM_READ_COMMAND, &v, 8);
        return v;
    }

    void write32(uint64_t addr, uint32_t v)
    {
        access(addr, tlm::TLM_WRITE_COMMAND, &v, 4);
    }

    void write64(uint64_t addr, uint64_t v)
    {
        access(addr, tlm::TLM_WRITE_COMMAND, &v, 8);
    }

    unsigned dbg_read64(uint64_t addr, uint64_t& out)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&out));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }

    tlm::tlm_response_status raw_read(uint64_t addr, uint32_t& out)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&out));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    // Fully-general b_transport probe for exercising the malformed-transaction
    // error-response branches (bad command / burst / byte-enable).
    tlm::tlm_response_status raw_access(uint64_t addr, tlm::tlm_command cmd,
                                        void* data, unsigned len,
                                        unsigned streaming_width,
                                        unsigned char* be_ptr, unsigned be_len)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width);
        gp.set_byte_enable_ptr(be_ptr);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        qk.inc(delay);
        return gp.get_response_status();
    }

    // Fully-general transport_dbg probe (read/write/unknown command).
    unsigned dbg_access(uint64_t addr, tlm::tlm_command cmd, void* data,
                        unsigned len)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }

    unsigned dbg_write64(uint64_t addr, uint64_t val)
    {
        return dbg_access(addr, tlm::TLM_WRITE_COMMAND, &val, 8);
    }
};

struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::cpu_ctrl dut;
    driver        drv;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , dut("cpu_ctrl")
        , drv("drv")
    {
        drv.sock.bind(dut.reg_socket);
        SC_THREAD(run);
    }

    void run()
    {
        std::cout << "==== CPU Control TB ====\n";

        // 1. Power-on defaults (offset and absolute addressing).
        EXPECT_EQ(0xC000'0000ULL, dut.dbg_reg(smc::cpu_ctrl_cfg::OFF_LOCAL_BASE));
        EXPECT_EQ(0xC000'0000U, read32(BASE + smc::cpu_ctrl_cfg::OFF_LOCAL_BASE));
        for (unsigned i = 0; i < smc::CPU_CTRL_SCRATCH_COUNT; ++i)
            EXPECT_EQ(0u, dut.scratch(i));
        EXPECT_EQ(0xC004'0000ULL,
                  dut.dbg_reg(smc::cpu_ctrl_cfg::OFF_RESET_VECTOR));
        std::cout << "  [PASS] reset defaults\n";

        // 2. SCRATCH[16] R/W (32-bit firmware view).
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 2 * 8, 0xDEADBEEFu);
        EXPECT_EQ(0xDEADBEEFu, read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 16));
        write64(smc::cpu_ctrl_cfg::OFF_SCRATCH + 3 * 8, 0xAABB'CCDDu);
        EXPECT_EQ(0xAABB'CCDDu, dut.scratch(3));
        std::cout << "  [PASS] scratch R/W\n";

        // 3. Inter-stage handoff sequence (SMC ROM writer → SEP ROM reader).
        constexpr uint32_t manifest_off = 0x1000u;
        constexpr uint32_t status_off   = 0x2000u;
        constexpr uint32_t sep_off      = 0x6400u;
        constexpr uint32_t sep_size     = 0x8000u;

        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_MANIFEST_IDX * 8,
                manifest_off);
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_STATUS_BUFFER_IDX * 8,
                status_off);
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_SEP_SRAM_OFF_IDX * 8,
                sep_off);
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_SEP_SRAM_SIZE_IDX * 8,
                sep_size);

        uint32_t status = smc::CPU_CTRL_SEP_STATUS_SRAM_INIT_BIT |
                          smc::CPU_CTRL_SEP_STATUS_MANIFEST_READY_BIT |
                          smc::CPU_CTRL_SEP_STATUS_BUFFER_READY_BIT;
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_STATUS_TO_SEP_IDX * 8,
                status);
        write32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH +
                    smc::CPU_CTRL_SCRATCH_MEM_REPAIR_IDX * 8,
                smc::CPU_CTRL_MEM_REPAIR_STATUS_PASSED);

        EXPECT_EQ(manifest_off,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 8 * 8));
        EXPECT_EQ(status,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 9 * 8));
        EXPECT_EQ(status_off,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 11 * 8));
        EXPECT_EQ(sep_off,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 13 * 8));
        EXPECT_EQ(sep_size,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 14 * 8));
        EXPECT_EQ(smc::CPU_CTRL_MEM_REPAIR_STATUS_PASSED,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_SCRATCH + 15 * 8));
        std::cout << "  [PASS] inter-stage handoff scratch sequence\n";

        // 4. LOCAL_BASE is SW read-only.
        write32(BASE + smc::cpu_ctrl_cfg::OFF_LOCAL_BASE, 0x1111'1111u);
        EXPECT_EQ(0xC000'0000U, read32(BASE + smc::cpu_ctrl_cfg::OFF_LOCAL_BASE));
        std::cout << "  [PASS] LOCAL_BASE read-only\n";

        // 5. GLOBAL_BASE / REGION_SIZE R/W.
        write32(BASE + smc::cpu_ctrl_cfg::OFF_GLOBAL_BASE, 0x5000'0000u);
        EXPECT_EQ(0x5000'0000U, read32(BASE + smc::cpu_ctrl_cfg::OFF_GLOBAL_BASE));
        write32(BASE + smc::cpu_ctrl_cfg::OFF_REGION_SIZE, 0x0200'0000u);
        EXPECT_EQ(0x0200'0000U, read32(BASE + smc::cpu_ctrl_cfg::OFF_REGION_SIZE));
        std::cout << "  [PASS] GLOBAL_BASE / REGION_SIZE\n";

        // 6. HW-visible registers via backdoor + SW read.
        dut.set_smc_attributes(0x0000'0001'0203'0405ULL);
        EXPECT_EQ(0x0203'0405U, read32(BASE + smc::cpu_ctrl_cfg::OFF_SMC_ATTRIBUTES));
        dut.set_test_ctrl(0xCAFE'BEEFu);
        EXPECT_EQ(0xCAFE'BEEFu, read32(BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL));
        dut.set_wb_pc(1, 2, 0x0000'0000'8000'0123ULL);
        EXPECT_EQ(0x8000'0123U,
                  read32(BASE + smc::cpu_ctrl_cfg::OFF_WB_PC_CORE1 + 2 * 8));
        std::cout << "  [PASS] HW backdoor registers\n";

        // 7. MUTEX acquire / release.
        const uint64_t mtx0 = BASE + smc::cpu_ctrl_cfg::OFF_MUTEX;
        EXPECT_EQ(1u, read64(mtx0));
        EXPECT_EQ(0u, read64(mtx0));
        EXPECT_EQ(0u, read64(mtx0));
        write64(mtx0, 0);
        EXPECT_EQ(1u, read64(mtx0));
        std::cout << "  [PASS] mutex acquire/release\n";

        // 8. SEMA signed increment/decrement.
        const uint64_t sem0 = BASE + smc::cpu_ctrl_cfg::OFF_SEMA;
        write32(sem0, 5);
        EXPECT_EQ(5u, read32(sem0));
        write32(sem0, static_cast<uint32_t>(-2));
        EXPECT_EQ(3u, read32(sem0));
        std::cout << "  [PASS] semaphore inc/dec\n";

        // 9. WDT_TIMEOUT_RESET single-pulse (self-clearing).
        write32(BASE + smc::cpu_ctrl_cfg::OFF_WDT_TIMEOUT_RESET, 0xFu);
        EXPECT_EQ(0u, read32(BASE + smc::cpu_ctrl_cfg::OFF_WDT_TIMEOUT_RESET));
        std::cout << "  [PASS] WDT_TIMEOUT_RESET pulse\n";

        // 10. transport_dbg side-effect-free mutex peek.
        uint64_t dbg = 0;
        EXPECT_EQ(8u, drv.dbg_read64(BASE + smc::cpu_ctrl_cfg::OFF_MUTEX, dbg));
        EXPECT_EQ(0u, dbg);  // still held from test 7
        std::cout << "  [PASS] transport_dbg mutex peek\n";

        // 11. Out-of-window → address error.
        {
            uint32_t junk = 0;
            EXPECT_TRUE(
                drv.raw_read(BASE + smc::cpu_ctrl_cfg::WINDOW_SIZE, junk) ==
                tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        std::cout << "  [PASS] out-of-window address error\n";

        // 12. CCI access_delay_ns preset honoured.
        EXPECT_EQ(2.0, dut.access_delay_ns());
        std::cout << "  [PASS] CCI parameters\n";

        // 13. RESET_VECTOR write masking (bits [63:56] are WI, vector is 56-bit).
        {
            constexpr uint64_t addr =
                BASE + smc::cpu_ctrl_cfg::OFF_RESET_VECTOR + 2 * 8;
            write64(addr, 0xFFFF'FFFF'FFFF'FFFFULL);
            EXPECT_EQ(0x00FF'FFFF'FFFF'FFFFULL, read64(addr));
        }
        std::cout << "  [PASS] RESET_VECTOR write masking\n";

        // 14. RESET_CTRL: single-pulse request bits [7:4] self-clear.
        {
            constexpr uint64_t addr = BASE + smc::cpu_ctrl_cfg::OFF_RESET_CTRL;
            const uint32_t     before = read32(addr);
            write32(addr, before | 0xF0u);
            const uint32_t after = read32(addr);
            EXPECT_EQ(0u, after & 0xF0u);
            EXPECT_EQ(before & ~0xF0u, after & ~0xF0u);
        }
        std::cout << "  [PASS] RESET_CTRL self-clearing pulse bits\n";

        // 15. CORE_RESET_PULSE_COUNT: bits [35:32] (core_resets_done) are RO,
        //     forced high on every read and every write.
        {
            constexpr uint64_t addr =
                BASE + smc::cpu_ctrl_cfg::OFF_CORE_RESET_PULSE_COUNT;
            write64(addr, 0);
            EXPECT_EQ(0xFULL << 32, read64(addr) & (0xFULL << 32));
        }
        std::cout << "  [PASS] CORE_RESET_PULSE_COUNT RO bits\n";

        // 16. CLOCK_GATE_CONTROL plain R/W.
        {
            constexpr uint64_t addr =
                BASE + smc::cpu_ctrl_cfg::OFF_CLOCK_GATE_CONTROL;
            write32(addr, 0x0000'00FFu);
            EXPECT_EQ(0x0000'00FFu, read32(addr));
        }
        std::cout << "  [PASS] CLOCK_GATE_CONTROL R/W\n";

        // 17. REFERENCE_COUNTER plain R/W.
        {
            constexpr uint64_t addr =
                BASE + smc::cpu_ctrl_cfg::OFF_REFERENCE_COUNTER;
            write64(addr, 0x1234'5678'9ABC'DEF0ULL);
            EXPECT_EQ(0x1234'5678'9ABC'DEF0ULL, read64(addr));
        }
        std::cout << "  [PASS] REFERENCE_COUNTER R/W\n";

        // 18. WDT_TIMEOUT write masking (32-bit).
        {
            constexpr uint64_t addr = BASE + smc::cpu_ctrl_cfg::OFF_WDT_TIMEOUT;
            write64(addr, 0xFFFF'FFFF'FFFF'FFFFULL);
            EXPECT_EQ(0x0000'0000'FFFF'FFFFULL, read64(addr));
        }
        std::cout << "  [PASS] WDT_TIMEOUT write masking\n";

        // 19. DEBUG_CTRL (32-bit masked) / DEBUG_BUS_MUX (plain) R/W.
        {
            constexpr uint64_t dc_addr  = BASE + smc::cpu_ctrl_cfg::OFF_DEBUG_CTRL;
            constexpr uint64_t dbm_addr = BASE + smc::cpu_ctrl_cfg::OFF_DEBUG_BUS_MUX;
            write64(dc_addr, 0xFFFF'FFFF'FFFF'FFFFULL);
            EXPECT_EQ(0x0000'0000'FFFF'FFFFULL, read64(dc_addr));
            write32(dbm_addr, 0x5A5A'5A5Au);
            EXPECT_EQ(0x5A5A'5A5Au, read32(dbm_addr));
        }
        std::cout << "  [PASS] DEBUG_CTRL / DEBUG_BUS_MUX R/W\n";

        // 20. TEST_CTRL / SMC_ATTRIBUTES are SW read-only; SW writes are silently
        //     ignored -- the HW-backdoor values set in test 6 must stick.
        {
            constexpr uint64_t tc_addr = BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL;
            constexpr uint64_t sa_addr = BASE + smc::cpu_ctrl_cfg::OFF_SMC_ATTRIBUTES;
            write32(tc_addr, 0x0000'0000u);
            EXPECT_EQ(0xCAFE'BEEFu, read32(tc_addr));
            write32(sa_addr, 0x0000'0000u);
            EXPECT_EQ(0x0203'0405U, read32(sa_addr));
        }
        std::cout << "  [PASS] TEST_CTRL / SMC_ATTRIBUTES SW-write ignored\n";

        // 21. DUMMY_ROM_0..3 plain RW64.
        {
            constexpr uint64_t dr0 = BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_0;
            constexpr uint64_t dr1 = BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_1;
            constexpr uint64_t dr2 = BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_2;
            constexpr uint64_t dr3 = BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_3;
            write64(dr0, 0x1111'1111'1111'1111ULL);
            EXPECT_EQ(0x1111'1111'1111'1111ULL, read64(dr0));
            write64(dr1, 0x2222'2222'2222'2222ULL);
            EXPECT_EQ(0x2222'2222'2222'2222ULL, read64(dr1));
            write64(dr2, 0x3333'3333'3333'3333ULL);
            EXPECT_EQ(0x3333'3333'3333'3333ULL, read64(dr2));
            write64(dr3, 0x4444'4444'4444'4444ULL);
            EXPECT_EQ(0x4444'4444'4444'4444ULL, read64(dr3));
        }
        std::cout << "  [PASS] DUMMY_ROM_0..3 R/W\n";

        // 22. DUMMY_ROM_NULL[] plain RW64.
        {
            constexpr uint64_t addr =
                BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL + 8;
            write64(addr, 0xABCD'EF01'2345'6789ULL);
            EXPECT_EQ(0xABCD'EF01'2345'6789ULL, read64(addr));
        }
        std::cout << "  [PASS] DUMMY_ROM_NULL region R/W\n";

        // 23. WB_PC is SW read-only; a bus write must be a structural no-op.
        {
            constexpr uint64_t addr = BASE + smc::cpu_ctrl_cfg::OFF_WB_PC_CORE0;
            const uint64_t     before = read64(addr);
            write64(addr, 0xFFFF'FFFF'FFFF'FFFFULL);
            EXPECT_EQ(before, read64(addr));
        }
        std::cout << "  [PASS] WB_PC SW-write ignored\n";

        // 24. Unmapped offset in the middle of the window -> RAZ/WI hole.
        {
            constexpr uint64_t addr = BASE + 0x1100u;  // gap: SEMA .. DUMMY_ROM_0
            EXPECT_EQ(0u, read32(addr));
            write32(addr, 0xDEAD'BEEFu);
            EXPECT_EQ(0u, read32(addr));
        }
        std::cout << "  [PASS] unmapped hole RAZ/WI\n";

        // 25. API-level out-of-range guards (set_wb_pc / scratch / set_scratch),
        //     plus set_scratch()'s normal (in-range) write path.
        {
            dut.set_wb_pc(smc::cpu_ctrl_cfg::NUM_CORES, 0, 0xDEAD'BEEFULL);
            dut.set_wb_pc(0, smc::cpu_ctrl_cfg::WB_PC_PER_CORE, 0xDEAD'BEEFULL);
            EXPECT_EQ(0u, dut.scratch(smc::CPU_CTRL_SCRATCH_COUNT));
            dut.set_scratch(smc::CPU_CTRL_SCRATCH_COUNT, 0xDEAD'BEEFu);
            EXPECT_EQ(0xAABB'CCDDu, dut.scratch(3));  // untouched (set in test 2)

            dut.set_scratch(5, 0x9999'0000u);
            EXPECT_EQ(0x9999'0000u, dut.scratch(5));
        }
        std::cout << "  [PASS] API bounds guards (set_wb_pc / scratch)\n";

        // 26. dump_state() smoke test.
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            EXPECT_TRUE(oss.str().find("cpu_ctrl @") != std::string::npos);
            EXPECT_TRUE(oss.str().find("LOCAL_BASE") != std::string::npos);
        }
        std::cout << "  [PASS] dump_state()\n";

        // 27. transport_dbg: misalignment guard, write path, unknown command.
        {
            uint64_t out = 0;
            EXPECT_EQ(0u, drv.dbg_read64(BASE + 1, out));  // misaligned -> 0

            const uint64_t val = 0x1234'5678'9ABC'DEF0ULL;
            EXPECT_EQ(8u, drv.dbg_write64(
                              BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_1, val));
            EXPECT_EQ(val, dut.dbg_reg(smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_1));

            uint32_t junk = 0;
            EXPECT_EQ(0u, drv.dbg_access(BASE + smc::cpu_ctrl_cfg::OFF_DUMMY_ROM_1,
                                          tlm::TLM_IGNORE_COMMAND, &junk, 4));
        }
        std::cout << "  [PASS] transport_dbg misalignment / write / unknown command\n";

        // 28. b_transport malformed-transaction error responses.
        {
            uint32_t junk = 0;

            EXPECT_TRUE(drv.raw_access(BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL,
                                        tlm::TLM_IGNORE_COMMAND, &junk, 4, 4,
                                        nullptr, 0) ==
                        tlm::TLM_COMMAND_ERROR_RESPONSE);

            EXPECT_TRUE(drv.raw_access(BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL + 1,
                                        tlm::TLM_READ_COMMAND, &junk, 4, 4,
                                        nullptr, 0) ==
                        tlm::TLM_BURST_ERROR_RESPONSE);

            EXPECT_TRUE(drv.raw_access(BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL,
                                        tlm::TLM_READ_COMMAND, &junk, 4, 8,
                                        nullptr, 0) ==
                        tlm::TLM_BURST_ERROR_RESPONSE);

            unsigned char be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            EXPECT_TRUE(drv.raw_access(BASE + smc::cpu_ctrl_cfg::OFF_TEST_CTRL,
                                        tlm::TLM_READ_COMMAND, &junk, 4, 4, be,
                                        4) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        }
        std::cout << "  [PASS] b_transport malformed-transaction error responses\n";

        if (g_failures == 0)
            std::cout << "ALL TESTS PASSED\n";
        else
            std::cerr << g_failures << " TEST(S) FAILED\n";
        sc_core::sc_stop();
    }

    uint32_t read32(uint64_t addr) { return drv.read32(addr); }
    void     write32(uint64_t addr, uint32_t v) { drv.write32(addr, v); }
    uint64_t read64(uint64_t addr) { return drv.read64(addr); }
    void     write64(uint64_t addr, uint64_t v) { drv.write64(addr, v); }
};

}  // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.cpu_ctrl.access_delay_ns",
                                       cci::cci_value(2.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
