// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file dma_tb.cpp
 * @brief Self-checking test bench for the SMC DMA SystemC/TLM-2.0 LT model.
 */

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iostream>
#include <vector>

#include "dma.h"
#include "smc_axi_extension.h"

namespace {

int g_failures = 0;

#define EXPECT_EQ(actual, expected)                                                   \
    do {                                                                              \
        if ((actual) != (expected)) {                                                 \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ << " expected "    \
                      << #actual << " == " << (expected) << ", got " << (actual)    \
                      << "\n";                                                        \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

#define EXPECT_TRUE(expr)                                                             \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ << " " << #expr   \
                      << "\n";                                                        \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

} // anonymous namespace

// ---------------------------------------------------------------------------
// Simple memory target backing the DMA master socket.
// ---------------------------------------------------------------------------

struct memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<memory, 64> sock;
    std::vector<unsigned char> data;

    explicit memory(sc_core::sc_module_name n, size_t size)
        : sc_core::sc_module(n), sock("sock"), data(size, 0x00)
    {
        sock.register_b_transport(this, &memory::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        const uint64_t addr = gp.get_address();
        unsigned char* ptr = gp.get_data_ptr();
        const unsigned int len = gp.get_data_length();

        if (addr + len > data.size()) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (gp.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(ptr, &data[addr], len);
        } else if (gp.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&data[addr], ptr, len);
        }

        gp.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1.0, sc_core::SC_NS);
    }
};

// ---------------------------------------------------------------------------
// Driver with the DMA register target socket.
// ---------------------------------------------------------------------------

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_core::sc_module_name n) : sc_core::sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, delay);
        EXPECT_EQ(gp.get_response_status(), tlm::TLM_OK_RESPONSE);
        return data;
    }

    void write32(uint64_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, delay);
        EXPECT_EQ(gp.get_response_status(), tlm::TLM_OK_RESPONSE);
    }

    tlm::tlm_response_status raw_read(uint64_t addr, uint32_t& data)
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    tlm::tlm_response_status raw_write(uint64_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    bool raw_write_len(uint64_t addr, unsigned char* ptr, unsigned int len)
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(ptr);
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, delay);
        return gp.get_response_status() == tlm::TLM_OK_RESPONSE;
    }
};

// ---------------------------------------------------------------------------
// Test bench top-level.
// ---------------------------------------------------------------------------

using namespace smc;

struct tb : sc_core::sc_module {
    dma    dut;
    driver drv;
    memory mem;

    explicit tb(sc_core::sc_module_name n)
        : sc_core::sc_module(n)
        , dut("dut", dma_cfg{16, 2.0, 1.0, 16})
        , drv("drv")
        , mem("mem", 0x10000)
    {
        drv.sock.bind(dut.reg_socket);
        dut.mst_socket.bind(mem.sock);

        SC_THREAD(run);
    }

    void run()
    {
        test_reset_values();
        test_config_readback();
        test_address_length_assembly();
        test_simple_transfer();
        test_transfer_delay();
        test_status_and_done();
        test_two_channels();
        test_2d_transfer();
        test_transfer_error();
        test_decode_miss();
        test_invalid_access();
        test_transport_dbg();

        if (g_failures == 0) {
            std::cout << "\nALL TESTS PASSED\n";
        } else {
            std::cout << "\nFAIL: " << g_failures << " test assertion(s) failed\n";
        }
        sc_core::sc_stop();
    }

    void test_reset_values()
    {
        std::cout << "test_reset_values\n";
        EXPECT_EQ(drv.read32(dma_cfg::OFF_CONFIG), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_STATUS_0), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_NEXT_ID_0), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DONE_0), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DST_ADDRESS_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DST_ADDRESS_HI), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_SRC_ADDRESS_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_SRC_ADDRESS_HI), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_LENGTH_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_LENGTH_HI), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DST_STRIDE_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DST_STRIDE_HI), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_SRC_STRIDE_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_SRC_STRIDE_HI), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_NUM_REPETITIONS_LO), 0u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_NUM_REPETITIONS_HI), 0u);
    }

    void test_config_readback()
    {
        std::cout << "test_config_readback\n";
        drv.write32(dma_cfg::OFF_CONFIG, 0x12345678u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_CONFIG), 0x12345678u);
    }

    void test_address_length_assembly()
    {
        std::cout << "test_address_length_assembly\n";
        const uint64_t src = 0x00000001AABBCCDDULL;
        const uint64_t dst = 0x0000000211223344ULL;
        const uint64_t len = 0x0000000000000100ULL;
        const uint64_t ss  = 0x0000000000000020ULL;
        const uint64_t ds  = 0x0000000000000040ULL;
        const uint64_t rep = 0x0000000000000003ULL;

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.write32(dma_cfg::OFF_SRC_STRIDE_LO, static_cast<uint32_t>(ss));
        drv.write32(dma_cfg::OFF_SRC_STRIDE_HI, static_cast<uint32_t>(ss >> 32));
        drv.write32(dma_cfg::OFF_DST_STRIDE_LO, static_cast<uint32_t>(ds));
        drv.write32(dma_cfg::OFF_DST_STRIDE_HI, static_cast<uint32_t>(ds >> 32));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_LO, static_cast<uint32_t>(rep));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_HI, static_cast<uint32_t>(rep >> 32));

        const uint64_t read_src = drv.read32(dma_cfg::OFF_SRC_ADDRESS_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_SRC_ADDRESS_HI)) << 32);
        const uint64_t read_dst = drv.read32(dma_cfg::OFF_DST_ADDRESS_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_DST_ADDRESS_HI)) << 32);
        const uint64_t read_len = drv.read32(dma_cfg::OFF_LENGTH_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_LENGTH_HI)) << 32);
        const uint64_t read_ss  = drv.read32(dma_cfg::OFF_SRC_STRIDE_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_SRC_STRIDE_HI)) << 32);
        const uint64_t read_ds  = drv.read32(dma_cfg::OFF_DST_STRIDE_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_DST_STRIDE_HI)) << 32);
        const uint64_t read_rep = drv.read32(dma_cfg::OFF_NUM_REPETITIONS_LO)
                                  | (static_cast<uint64_t>(drv.read32(dma_cfg::OFF_NUM_REPETITIONS_HI)) << 32);

        EXPECT_EQ(read_src, src);
        EXPECT_EQ(read_dst, dst);
        EXPECT_EQ(read_len, len);
        EXPECT_EQ(read_ss, ss);
        EXPECT_EQ(read_ds, ds);
        EXPECT_EQ(read_rep, rep);
    }

    void test_simple_transfer()
    {
        std::cout << "test_simple_transfer\n";
        const uint64_t src = 0x100;
        const uint64_t dst = 0x500;
        const uint64_t len = 32;

        for (uint64_t i = 0; i < len; ++i) {
            mem.data[src + i] = static_cast<unsigned char>(0xA0u + i);
        }
        std::memset(&mem.data[dst], 0x00, len);

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_LO, 0);
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_HI, 0);

        const uint32_t id = drv.read32(dma_cfg::OFF_NEXT_ID_0);
        EXPECT_EQ(id, 1u);

        // Allow the asynchronous transfer thread to complete.
        sc_core::wait(10, sc_core::SC_NS);

        for (uint64_t i = 0; i < len; ++i) {
            EXPECT_EQ(mem.data[dst + i], static_cast<unsigned char>(0xA0u + i));
        }
    }

    void test_transfer_delay()
    {
        std::cout << "test_transfer_delay\n";
        // One 16-byte chunk (max_burst_bytes=16): two master beats.  Each beat
        // annotates transfer_delay_ns (CCI 0.5) plus the memory target's 1 ns,
        // so the transfer thread consumes 3 ns before DONE increments.
        const uint64_t src = 0x180;
        const uint64_t dst = 0x580;
        const uint64_t len = 16;

        for (uint64_t i = 0; i < len; ++i) {
            mem.data[src + i] = static_cast<unsigned char>(0x50u + i);
        }
        std::memset(&mem.data[dst], 0x00, len);

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_LO, 0);
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_HI, 0);

        const uint32_t done_before = drv.read32(dma_cfg::done_offset(2));
        EXPECT_EQ(drv.read32(dma_cfg::next_id_offset(2)), 1u);

        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_EQ(drv.read32(dma_cfg::done_offset(2)), done_before);
        EXPECT_EQ(drv.read32(dma_cfg::status_offset(2)), 1u);

        sc_core::wait(5, sc_core::SC_NS);
        EXPECT_EQ(drv.read32(dma_cfg::done_offset(2)), done_before + 1);
        EXPECT_EQ(drv.read32(dma_cfg::status_offset(2)), 0u);

        for (uint64_t i = 0; i < len; ++i) {
            EXPECT_EQ(mem.data[dst + i], static_cast<unsigned char>(0x50u + i));
        }
    }

    void test_status_and_done()
    {
        std::cout << "test_status_and_done\n";
        const uint64_t src = 0x200;
        const uint64_t dst = 0x600;
        const uint64_t len = 16;

        for (uint64_t i = 0; i < len; ++i) {
            mem.data[src + i] = static_cast<unsigned char>(0xB0u + i);
        }

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));

        EXPECT_EQ(drv.read32(dma_cfg::OFF_DONE_0), 1u);
        drv.read32(dma_cfg::OFF_NEXT_ID_0);
        // After scheduling, the transfer thread needs a delta to finish.
        sc_core::wait(5, sc_core::SC_NS);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DONE_0), 2u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_STATUS_0), 0u);
    }

    void test_two_channels()
    {
        std::cout << "test_two_channels\n";
        const uint64_t src0 = 0x300;
        const uint64_t dst0 = 0x700;
        const uint64_t src1 = 0x400;
        const uint64_t dst1 = 0x800;
        const uint64_t len = 8;

        for (uint64_t i = 0; i < len; ++i) {
            mem.data[src0 + i] = static_cast<unsigned char>(0xC0u + i);
            mem.data[src1 + i] = static_cast<unsigned char>(0xD0u + i);
        }

        // Channel 0 transfer.
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src0));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src0 >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst0));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst0 >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.read32(dma_cfg::OFF_NEXT_ID_0);

        // Channel 1 transfer.
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src1));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src1 >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst1));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst1 >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.read32(dma_cfg::next_id_offset(1));

        sc_core::wait(10, sc_core::SC_NS);

        for (uint64_t i = 0; i < len; ++i) {
            EXPECT_EQ(mem.data[dst0 + i], static_cast<unsigned char>(0xC0u + i));
            EXPECT_EQ(mem.data[dst1 + i], static_cast<unsigned char>(0xD0u + i));
        }
        EXPECT_EQ(drv.read32(dma_cfg::done_offset(0)), 3u);
        EXPECT_EQ(drv.read32(dma_cfg::done_offset(1)), 1u);
    }

    void test_2d_transfer()
    {
        std::cout << "test_2d_transfer\n";
        const uint64_t src = 0x900;
        const uint64_t dst = 0xA00;
        const uint64_t len = 4;
        const uint64_t src_stride = 8;
        const uint64_t dst_stride = 8;
        const uint64_t reps = 2; // repetitions register value => 3 total rows

        // Fill three source rows at src, src+8, src+16.
        for (uint64_t r = 0; r <= reps; ++r) {
            for (uint64_t i = 0; i < len; ++i) {
                mem.data[src + r * src_stride + i] = static_cast<unsigned char>(0xE0u + r * 16u + i);
            }
        }

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.write32(dma_cfg::OFF_SRC_STRIDE_LO, static_cast<uint32_t>(src_stride));
        drv.write32(dma_cfg::OFF_SRC_STRIDE_HI, static_cast<uint32_t>(src_stride >> 32));
        drv.write32(dma_cfg::OFF_DST_STRIDE_LO, static_cast<uint32_t>(dst_stride));
        drv.write32(dma_cfg::OFF_DST_STRIDE_HI, static_cast<uint32_t>(dst_stride >> 32));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_LO, static_cast<uint32_t>(reps));
        drv.write32(dma_cfg::OFF_NUM_REPETITIONS_HI, static_cast<uint32_t>(reps >> 32));

        drv.read32(dma_cfg::OFF_NEXT_ID_0);
        sc_core::wait(10, sc_core::SC_NS);

        for (uint64_t r = 0; r <= reps; ++r) {
            for (uint64_t i = 0; i < len; ++i) {
                EXPECT_EQ(mem.data[dst + r * dst_stride + i],
                          static_cast<unsigned char>(0xE0u + r * 16u + i));
            }
        }
    }

    void test_transfer_error()
    {
        std::cout << "test_transfer_error\n";
        // Source address outside the memory target bounds triggers a read error.
        const uint64_t bad_src = 0x10000;
        const uint64_t dst = 0xB00;
        const uint64_t len = 8;

        drv.write32(dma_cfg::OFF_SRC_ADDRESS_LO, static_cast<uint32_t>(bad_src));
        drv.write32(dma_cfg::OFF_SRC_ADDRESS_HI, static_cast<uint32_t>(bad_src >> 32));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_LO, static_cast<uint32_t>(dst));
        drv.write32(dma_cfg::OFF_DST_ADDRESS_HI, static_cast<uint32_t>(dst >> 32));
        drv.write32(dma_cfg::OFF_LENGTH_LO, static_cast<uint32_t>(len));
        drv.write32(dma_cfg::OFF_LENGTH_HI, static_cast<uint32_t>(len >> 32));
        drv.read32(dma_cfg::OFF_NEXT_ID_0);
        sc_core::wait(10, sc_core::SC_NS);

        // A failed transfer must not be reported as completed.
        EXPECT_EQ(drv.read32(dma_cfg::OFF_DONE_0), 4u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_STATUS_0), 0u);
    }

    void test_decode_miss()
    {
        std::cout << "test_decode_miss\n";
        uint32_t data = 0;
        // 0x44 is a hole between STATUS_15 (0x40) and NEXT_ID_0 (0x48).
        EXPECT_EQ(drv.raw_read(0x44, data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_EQ(drv.raw_write(0x44, 0xDEADBEEFu), tlm::TLM_ADDRESS_ERROR_RESPONSE);

        // Past the window must not wrap (0x138 → CONFIG, 0x140 → STATUS_1).
        EXPECT_EQ(drv.raw_read(dma_cfg::WINDOW_SIZE, data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_EQ(drv.raw_write(dma_cfg::WINDOW_SIZE, 0xDEADBEEFu), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_EQ(drv.raw_read(0x140, data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_EQ(drv.raw_write(0x140, 0xDEADBEEFu), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    void test_invalid_access()
    {
        std::cout << "test_invalid_access\n";
        // Unsupported data length (must be 4 bytes).
        unsigned char buf[8] = {0};
        EXPECT_TRUE(!drv.raw_write_len(dma_cfg::OFF_CONFIG, buf, 8));

        // Invalid command.
        {
            tlm::tlm_generic_payload gp;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            uint32_t data = 0;
            gp.set_command(tlm::TLM_IGNORE_COMMAND);
            gp.set_address(dma_cfg::OFF_CONFIG);
            gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_byte_enable_ptr(nullptr);
            gp.set_dmi_allowed(false);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(gp.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
        }
    }

    void test_transport_dbg()
    {
        std::cout << "test_transport_dbg\n";
        drv.write32(dma_cfg::OFF_CONFIG, 0xABCD0000u);

        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(dma_cfg::OFF_CONFIG);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        EXPECT_EQ(dut.transport_dbg(gp), 4u);
        EXPECT_EQ(data, 0xABCD0000u);

        data = 0xDEADBEEFu;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(dma_cfg::OFF_CONFIG);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        EXPECT_EQ(dut.transport_dbg(gp), 4u);
        EXPECT_EQ(drv.read32(dma_cfg::OFF_CONFIG), 0xDEADBEEFu);

        // Unmapped / out-of-window debug access returns 0 bytes.
        gp.set_address(0x44);
        gp.set_command(tlm::TLM_READ_COMMAND);
        EXPECT_EQ(dut.transport_dbg(gp), 0u);
        gp.set_address(dma_cfg::WINDOW_SIZE);
        EXPECT_EQ(dut.transport_dbg(gp), 0u);
        gp.set_address(0x140);
        EXPECT_EQ(dut.transport_dbg(gp), 0u);
    }
};

// ---------------------------------------------------------------------------
// sc_main
// ---------------------------------------------------------------------------

int sc_main(int, char**)
{
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);
    cci::cci_originator cfg("platform_cfg");
    auto broker = cci::cci_get_global_broker(cfg);
    broker.set_preset_cci_value("tb.dut.num_channels", cci::cci_value(16u));
    broker.set_preset_cci_value("tb.dut.max_burst_bytes", cci::cci_value(16u));
    broker.set_preset_cci_value("tb.dut.access_delay_ns", cci::cci_value(1.0));
    broker.set_preset_cci_value("tb.dut.transfer_delay_ns", cci::cci_value(0.5));

    tb top("tb");
    sc_core::sc_start();
    return g_failures;
}
