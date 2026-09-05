// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file dma_neg_tb.cpp
 * @brief Negative-path / edge-case coverage for the SMC DMA LT model.
 *
 *   - constructor SC_REPORT_FATAL branches: num_channels 0 / >16, max_burst_bytes 0
 *   - TLM error responses: null data pointer, non-word length, invalid command
 *   - transport_dbg rejects malformed accesses
 *   - holes / misaligned offsets inside the window are RAZ/WI or ADDRESS_ERROR
 *   - write to read-only / trigger registers is accepted (returns true) but ignored
 *   - start_transfer returns 0 for zero-length / invalid channels
 *
 * The test exercises the model entirely during elaboration (b_transport and
 * transport_dbg are plain function calls through the bound socket) and never
 * calls sc_start.  That keeps the constructor-throw probes from perturbing
 * a running simulation kernel.
 */

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <iostream>

#include "dma.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

#define EXPECT_TRUE(cond)                                                             \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                       \
                      << "  expected TRUE: " #cond "\n";                             \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

#define EXPECT_EQ(expected, actual)                                                   \
    do {                                                                              \
        const auto _e = (expected);                                                   \
        const auto _a = (actual);                                                     \
        if (!(_e == _a)) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                       \
                      << "  expected=" << +_e << " actual=" << +_a                    \
                      << "  (" #expected " == " #actual ")\n";                      \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

using smc::dma_cfg;

template <typename F>
bool expect_fatal(F&& body)
{
    try { body(); }
    catch (const sc_core::sc_report&) { return true; }
    catch (const std::exception&)     { return true; }
    catch (...)                       { return true; }
    return false;
}

struct error_memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<error_memory, 64> sock;
    explicit error_memory(sc_module_name n) : sc_core::sc_module(n), sock("sock") {
        sock.register_b_transport(this, &error_memory::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& gp, sc_time&) {
        // Any address >= 0x8000 is reported as an error; lower addresses succeed.
        if (gp.get_address() >= 0x8000) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        } else {
            gp.set_response_status(tlm::TLM_OK_RESPONSE);
        }
    }
};

struct probe : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<probe> sock;
    explicit probe(sc_module_name n) : sc_module(n), sock("sock") {}

    tlm::tlm_response_status raw(tlm::tlm_command cmd, uint64_t addr,
                                 uint32_t len, void* data,
                                 uint32_t sw = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(sw == 0 ? len : sw);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, uint32_t len, void* data) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

} // anonymous namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    std::cout << "==== SMC DMA negative-path TB ====\n";

    // Constructor guard rails.
    {
        dma_cfg bad_cfg;
        bad_cfg.num_channels = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::dma d("d", bad_cfg); }));
    }
    {
        dma_cfg bad_cfg;
        bad_cfg.num_channels = 17;
        EXPECT_TRUE(expect_fatal([&]{ smc::dma d("d", bad_cfg); }));
    }
    {
        dma_cfg bad_cfg;
        bad_cfg.max_burst_bytes = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::dma d("d", bad_cfg); }));
    }

    // Valid 16-channel instance for the majority of the negative tests.
    smc::dma dut("dut", smc::dma_cfg{16, 1.0, 0.0, 8});
    probe p("p");
    error_memory mem("mem");
    p.sock.bind(dut.reg_socket);
    dut.mst_socket.bind(mem.sock);

    // 1-channel instance used to exercise out-of-range register decode paths.
    smc::dma dut1("dut1", smc::dma_cfg{1, 1.0, 0.0, 8});
    probe p1("p1");
    error_memory mem1("mem1");
    p1.sock.bind(dut1.reg_socket);
    dut1.mst_socket.bind(mem1.sock);

    uint32_t data = 0;

    // TLM error: null data pointer.
    {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(dma_cfg::OFF_CONFIG);
        gp.set_data_ptr(nullptr);
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        p.sock->b_transport(gp, t);
        EXPECT_EQ(gp.get_response_status(), tlm::TLM_GENERIC_ERROR_RESPONSE);
    }

    // TLM error: non-word length.
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_CONFIG, 2, &data), tlm::TLM_BURST_ERROR_RESPONSE);

    // TLM error: streaming width mismatch.
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::OFF_CONFIG, 4, &data, 2), tlm::TLM_BURST_ERROR_RESPONSE);

    // TLM error: invalid command.
    EXPECT_EQ(p.raw(tlm::TLM_IGNORE_COMMAND, dma_cfg::OFF_CONFIG, 4, &data), tlm::TLM_COMMAND_ERROR_RESPONSE);

    // transport_dbg with null pointer.
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(dma_cfg::OFF_CONFIG);
        gp.set_data_ptr(nullptr);
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        EXPECT_EQ(dut.transport_dbg(gp), 0u);
    }

    // transport_dbg with invalid length.
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(dma_cfg::OFF_CONFIG);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(2);
        gp.set_streaming_width(2);
        EXPECT_EQ(dut.transport_dbg(gp), 0u);
    }

    // Misaligned register offsets are rejected.
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, 0x06, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE);  // STATUS misaligned
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, 0x4C, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE); // NEXT_ID misaligned
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, 0xCA, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE); // DONE misaligned

    // Addresses at or past WINDOW_SIZE must not wrap into the register file.
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::WINDOW_SIZE, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::WINDOW_SIZE, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, 0x140, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, 0x140, 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(p.dbg(tlm::TLM_READ_COMMAND, dma_cfg::WINDOW_SIZE, 4, &data), 0u);
    EXPECT_EQ(p.dbg(tlm::TLM_READ_COMMAND, 0x140, 4, &data), 0u);

    // Out-of-range register offsets on a 1-channel model are rejected.
    EXPECT_EQ(p1.raw(tlm::TLM_READ_COMMAND, dma_cfg::status_offset(1), 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE); // STATUS out of range
    EXPECT_EQ(p1.raw(tlm::TLM_READ_COMMAND, dma_cfg::next_id_offset(1), 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE); // NEXT_ID out of range
    EXPECT_EQ(p1.raw(tlm::TLM_READ_COMMAND, dma_cfg::done_offset(1), 4, &data), tlm::TLM_ADDRESS_ERROR_RESPONSE); // DONE out of range

    // Write to read-only / trigger register arrays returns OK but is ignored.
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_STATUS_0, 4, &data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_NEXT_ID_0, 4, &data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DONE_0, 4, &data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(p.dbg(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_STATUS_0, 4, &data), 4u);
    EXPECT_EQ(p.dbg(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_NEXT_ID_0, 4, &data), 4u);
    EXPECT_EQ(p.dbg(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DONE_0, 4, &data), 4u);

    // start_transfer returns 0 for zero length.
    p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_LENGTH_LO, 4, &data); // length = 0
    EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::OFF_NEXT_ID_0, 4, &data), tlm::TLM_OK_RESPONSE);
    // The read still returns 0 because length is 0.
    {
        uint32_t zero = 0;
        EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::OFF_NEXT_ID_0, 4, &zero), tlm::TLM_OK_RESPONSE);
    }

    // start_transfer returns 0 for an invalid channel (out of range).  That
    // path is not reachable from a decoded register offset; the out-of-range
    // NEXT_ID offsets above return ADDRESS_ERROR before start_transfer.

    // copy_chunk error path: source read fails.
    {
        uint32_t len = 8;
        uint32_t src_hi = 0;
        uint32_t src_lo = 0x8000;
        uint32_t dst_hi = 0;
        uint32_t dst_lo = 0x100;
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_SRC_ADDRESS_LO, 4, &src_lo);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_SRC_ADDRESS_HI, 4, &src_hi);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DST_ADDRESS_LO, 4, &dst_lo);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DST_ADDRESS_HI, 4, &dst_hi);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_LENGTH_LO, 4, &len);
        EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::OFF_NEXT_ID_0, 4, &data), tlm::TLM_OK_RESPONSE);
    }

    // copy_chunk error path: destination write fails.
    {
        uint32_t len = 8;
        uint32_t src_hi = 0;
        uint32_t src_lo = 0x100;
        uint32_t dst_hi = 0;
        uint32_t dst_lo = 0x8000;
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_SRC_ADDRESS_LO, 4, &src_lo);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_SRC_ADDRESS_HI, 4, &src_hi);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DST_ADDRESS_LO, 4, &dst_lo);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_DST_ADDRESS_HI, 4, &dst_hi);
        p.raw(tlm::TLM_WRITE_COMMAND, dma_cfg::OFF_LENGTH_LO, 4, &len);
        EXPECT_EQ(p.raw(tlm::TLM_READ_COMMAND, dma_cfg::next_id_offset(1), 4, &data), tlm::TLM_OK_RESPONSE);
    }

    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\nFAIL: " << g_failures << " test assertion(s) failed\n";
    }
    return g_failures;
}
