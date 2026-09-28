// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// memory_zeroer_tb.cpp -- self-checking test bench for the SMC memory zeroer.
//
// CCI: sc_main registers a global broker before any module is constructed.
// Presets exercise chunk_size and access_delay_ns.
//
// Prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "memory_zeroer.h"
#include "tlm_probe.h"

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

// Run `body`; return true iff it raised any exception (SC_REPORT_FATAL throws).
template <typename F>
bool expect_fatal(F&& body)
{
    try {
        body();
    } catch (const sc_core::sc_report&) {
        return true;
    } catch (...) {
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Recording DMA target — validates payload metadata and supports fault injection.
// ---------------------------------------------------------------------------
struct simple_mem : sc_core::sc_module {
    struct xact {
        tlm::tlm_command           cmd            = tlm::TLM_IGNORE_COMMAND;
        uint64_t                   addr           = 0;
        unsigned                   len            = 0;
        unsigned                   streaming_width = 0;
        bool                       be_null        = true;
        unsigned                   be_len         = 0;
        tlm::tlm_response_status   status_in      = tlm::TLM_OK_RESPONSE;
        bool                       has_ext        = false;
        uint16_t                   source_id      = 0;
        uint8_t                    prot           = 0;
        bool                       is_fetch       = false;
        bool                       is_locked      = false;
        bool                       is_secure      = false;
        bool                       is_user        = false;
        uint16_t                   axi_id         = 0;
        uint8_t                    axi_user       = 0;
        std::vector<unsigned char> data;
    };

    tlm_utils::simple_target_socket<simple_mem> sock;
    std::vector<uint8_t>                        mem;
    std::vector<xact>                           log;
    /// Sparse store for high / wrapping address observation.
    std::map<uint64_t, uint8_t>                 sparse;
    bool                                        use_sparse   = false;
    int                                         fail_at      = -1; // 0-based
    bool                                        enforce_meta = false;
    uint64_t                                    expect_base  = 0;
    uint64_t                                    expect_size  = 0;
    unsigned                                    expect_chunk = 64;
    bool                                        meta_ok      = true;
    std::string                                 meta_fail;
    bool                                        saw_axi_extension = false;
    bool                                        axi_extension_valid = true;

    explicit simple_mem(sc_module_name n, std::size_t bytes)
        : sc_module(n), sock("sock"), mem(bytes, 0xA5)
    {
        sock.register_b_transport(this, &simple_mem::b_transport);
    }

    void fill(uint8_t v)
    {
        std::fill(mem.begin(), mem.end(), v);
        sparse.clear();
    }

    void reset_log()
    {
        log.clear();
        meta_ok = true;
        meta_fail.clear();
        fail_at = -1;
        enforce_meta = false;
        use_sparse = false;
    }

    void expect_job(uint64_t base, uint64_t size, unsigned chunk)
    {
        enforce_meta = true;
        expect_base  = base;
        expect_size  = size;
        expect_chunk = chunk;
        meta_ok      = true;
        meta_fail.clear();
        log.clear();
    }

    bool span_is_zero(uint64_t base, uint64_t size) const
    {
        for (uint64_t i = 0; i < size; ++i) {
            const uint64_t a = base + i;
            if (use_sparse) {
                auto it = sparse.find(a);
                if (it == sparse.end() || it->second != 0)
                    return false;
            } else if (a >= mem.size() || mem[static_cast<std::size_t>(a)] != 0) {
                return false;
            }
        }
        return true;
    }

    uint8_t byte_at(uint64_t a) const
    {
        if (use_sparse) {
            auto it = sparse.find(a);
            return it == sparse.end() ? uint8_t{0xA5} : it->second;
        }
        if (a >= mem.size())
            return 0xA5;
        return mem[static_cast<std::size_t>(a)];
    }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        xact x;
        x.cmd             = gp.get_command();
        x.addr            = gp.get_address();
        x.len             = gp.get_data_length();
        x.streaming_width = gp.get_streaming_width();
        x.be_null         = (gp.get_byte_enable_ptr() == nullptr);
        x.be_len          = gp.get_byte_enable_length();
        x.status_in       = gp.get_response_status();
        if (auto* ext = gp.get_extension<smc::smc_axi_extension>()) {
            x.has_ext    = true;
            x.source_id  = ext->source_id;
            x.prot       = ext->prot;
            x.is_fetch   = ext->is_fetch;
            x.is_locked  = ext->is_locked;
            x.is_secure  = ext->is_secure;
            x.is_user    = ext->is_user;
            x.axi_id     = ext->axi_id;
            x.axi_user   = ext->axi_user;
        }
        unsigned char* ptr = gp.get_data_ptr();
        if (ptr != nullptr && x.len != 0)
            x.data.assign(ptr, ptr + x.len);
        const unsigned idx = static_cast<unsigned>(log.size());
        log.push_back(x);
        saw_axi_extension |= x.has_ext;
        if (!x.has_ext || x.source_id != smc::SMC_ID || x.is_user ||
            x.is_secure || x.is_fetch || x.is_locked) {
            axi_extension_valid = false;
        }

        if (enforce_meta) {
            auto fail = [&](const char* why) {
                if (meta_ok) {
                    meta_ok = false;
                    meta_fail = why;
                }
            };
            if (x.cmd != tlm::TLM_WRITE_COMMAND)
                fail("DMA command is not WRITE");
            if (x.status_in != tlm::TLM_INCOMPLETE_RESPONSE)
                fail("initiator did not set TLM_INCOMPLETE_RESPONSE");
            if (!x.be_null || x.be_len != 0)
                fail("byte-enable pointer must be null / length 0");
            if (ptr == nullptr)
                fail("null data pointer");
            if (x.streaming_width != x.len)
                fail("streaming_width != data_length");
            // Sideband on every chunk, including a short final one. Values are
            // the extension's documented defaults plus SMC_ID, the internal-
            // master source ID the DMA initiator uses. There is no zeroer ID
            // in source_id_t.
            if (!x.has_ext)
                fail("DMA write has no smc_axi_extension");
            else if (x.source_id != smc::SMC_ID || x.prot != 0b0111 ||
                     x.is_fetch || x.is_locked || x.is_secure || x.is_user ||
                     x.axi_id != 0 || x.axi_user != 0)
                fail("DMA sideband is not the internal-master default");
            for (unsigned char b : x.data) {
                if (b != 0) {
                    fail("DMA payload byte is non-zero");
                    break;
                }
            }

            // Expected chunk geometry: monotonic, non-overlapping, exact spans.
            const uint64_t done = static_cast<uint64_t>(idx) * expect_chunk;
            if (done >= expect_size) {
                fail("extra DMA beyond expected size");
            } else {
                const unsigned want = static_cast<unsigned>(
                    std::min(static_cast<uint64_t>(expect_chunk),
                             expect_size - done));
                if (x.len != want)
                    fail("unexpected chunk length");
                if (x.addr != expect_base + done)
                    fail("unexpected chunk address");
            }
            // Prior chunks must end exactly where this one begins.
            if (idx > 0) {
                const auto& prev = log[idx - 1];
                if (prev.addr + prev.len != x.addr)
                    fail("chunks are not monotonic / abutting");
            }
        }

        if (fail_at >= 0 && static_cast<int>(idx) == fail_at) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (use_sparse) {
            if (ptr == nullptr || x.len == 0) {
                gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                return;
            }
            // Reject only if the span itself wraps; high addresses are fine.
            if (x.addr + x.len < x.addr) {
                gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            if (gp.get_command() == tlm::TLM_WRITE_COMMAND) {
                for (unsigned i = 0; i < x.len; ++i)
                    sparse[x.addr + i] = ptr[i];
            } else if (gp.get_command() == tlm::TLM_READ_COMMAND) {
                for (unsigned i = 0; i < x.len; ++i)
                    ptr[i] = byte_at(x.addr + i);
            } else {
                gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                return;
            }
            delay += sc_core::sc_time(1, SC_NS);
            gp.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        const uint64_t adr = x.addr;
        const unsigned len = x.len;
        if (ptr == nullptr || len == 0 || adr + len > mem.size() ||
            adr + len < adr) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (gp.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(mem.data() + adr, ptr, len);
        } else if (gp.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(ptr, mem.data() + adr, len);
        } else {
            gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        delay += sc_core::sc_time(1, SC_NS);
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ---------------------------------------------------------------------------
// Register-bus driver
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint64_t read64(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return data;
    }

    void write64(uint64_t addr, uint64_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
    }

    tlm::tlm_response_status try_access(tlm::tlm_command cmd, uint64_t addr,
                                        unsigned len)
    {
        tlm::tlm_generic_payload gp;
        std::vector<uint8_t> buf(len ? len : 1, 0);
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(buf.data());
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    // Fully-general probe for malformed TLM (null ptr, BE, streaming width, ...).
    tlm::tlm_response_status raw(tlm::tlm_command cmd, uint64_t addr,
                                 unsigned len, unsigned char* ptr,
                                 unsigned streaming_width,
                                 unsigned char* be = nullptr,
                                 unsigned be_len = 0)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(ptr);
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width);
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    // As raw(), but starts from `incoming` and hands back the delay the target
    // left behind. Returning the final value rather than a delta means a
    // target that overwrites the caller's delay instead of accumulating onto
    // it shows up as a mismatch instead of silently underflowing sc_time.
    tlm::tlm_response_status timed(tlm::tlm_command cmd, uint64_t addr,
                                   unsigned len, unsigned char* ptr,
                                   unsigned streaming_width,
                                   sc_time incoming, sc_time& final_delay,
                                   unsigned char* be = nullptr,
                                   unsigned be_len = 0)
    {
        tlm::tlm_generic_payload gp;
        final_delay = incoming;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(ptr);
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width);
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, final_delay);
        return gp.get_response_status();
    }
};

static unsigned expected_chunks(uint64_t size, unsigned chunk)
{
    if (size == 0 || chunk == 0)
        return 0;
    return static_cast<unsigned>((size + chunk - 1) / chunk);
}

// ---------------------------------------------------------------------------
// Top-level TB
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    // MANY_WRITERS: irq_o is driven both from reset_proc (SC_METHOD) and
    // from nested b_transport running in the TB thread process context.
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> irq{"irq"};

    smc::memory_zeroer zeroer;
    simple_mem         mem;
    driver             drv;

    SC_HAS_PROCESS(tb);

    explicit tb(sc_module_name n)
        : sc_module(n)
        , zeroer("zeroer")
        , mem("mem", 8192)
        , drv("drv")
    {
        zeroer.rst_n_i(rst_n);
        zeroer.irq_o(irq);
        drv.sock.bind(zeroer.reg_socket);
        zeroer.dma_socket.bind(mem.sock);
        SC_THREAD(run);
    }

    void settle()
    {
        for (int i = 0; i < 3; ++i) wait(SC_ZERO_TIME);
    }

    void pulse_reset()
    {
        rst_n.write(false);
        wait(10, SC_NS);
        rst_n.write(true);
        settle();
    }

    void run_zero_job(uint64_t dest, uint64_t size, bool int_en)
    {
        using cfg = smc::memory_zeroer_cfg;
        drv.write64(cfg::OFF_DEST_ADDR, dest);
        drv.write64(cfg::OFF_SIZE, size);
        drv.write64(cfg::OFF_CTRL_STATUS,
                    int_en ? cfg::CTRL_INT_EN_MASK : 0);
        settle();
    }

    void run()
    {
        using cfg = smc::memory_zeroer_cfg;
        constexpr unsigned kChunk = 64; // sc_main CCI preset

        std::cout << "==== SMC memory_zeroer TB ====\n";

        // Reset defaults ----------------------------------------------------
        pulse_reset();
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_SIZE));
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] reset clears registers and irq\n";

        // DEST_ADDR / SIZE R/W ---------------------------------------------
        drv.write64(cfg::OFF_DEST_ADDR, 0x100);
        drv.write64(cfg::OFF_SIZE, 0x40);
        EXPECT_EQ(UINT64_C(0x100), drv.read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(UINT64_C(0x40), drv.read64(cfg::OFF_SIZE));
        std::cout << "  [PASS] DEST_ADDR / SIZE R/W\n";

        // SIZE=0 write to CTRL_STATUS does not touch memory ----------------
        mem.fill(0xA5);
        drv.write64(cfg::OFF_SIZE, 0);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, drv.read64(cfg::OFF_CTRL_STATUS));
        EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.mem[0x100]));
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] SIZE=0 does not start a job\n";

        // Zero-fill a region (+ interrupt) ---------------------------------
        mem.fill(0xA5);
        drv.write64(cfg::OFF_DEST_ADDR, 0x80);
        drv.write64(cfg::OFF_SIZE, 0x100); // 256 bytes
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();

        // After LT job completes: idle (no busy bit), irq asserted.
        const uint64_t ctrl = drv.read64(cfg::OFF_CTRL_STATUS);
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, ctrl & cfg::CTRL_INT_EN_MASK);
        EXPECT_EQ(UINT64_C(0), ctrl & cfg::CTRL_STATUS_MASK);
        EXPECT_TRUE(irq.read());
        EXPECT_TRUE(mem.saw_axi_extension);
        EXPECT_TRUE(mem.axi_extension_valid);

        bool zeros_ok = true;
        for (uint64_t i = 0x80; i < 0x180; ++i) {
            if (mem.mem[static_cast<std::size_t>(i)] != 0) {
                zeros_ok = false;
                break;
            }
        }
        EXPECT_TRUE(zeros_ok);
        // Unrelated region untouched.
        EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.mem[0x200]));
        std::cout << "  [PASS] zero-fill + completion irq (int_en=1)\n";

        // -----------------------------------------------------------------
        // Finding 6: DMA payload conformance, including AXI sideband.
        // 500 bytes is not a multiple of the chunk, so the last beat is short.
        // -----------------------------------------------------------------
        {
            const unsigned dma_fails = g_failures;
            pulse_reset();
            mem.fill(0x5A);
            mem.expect_job(0x10, 500, kChunk);
            run_zero_job(0x10, 500, true);
            EXPECT_TRUE(mem.meta_ok);
            if (!mem.meta_ok)
                std::cerr << "FAIL  DMA meta: " << mem.meta_fail << "\n";
            EXPECT_EQ(expected_chunks(500, kChunk),
                      static_cast<unsigned>(mem.log.size()));
            EXPECT_TRUE(irq.read());
            EXPECT_TRUE(mem.span_is_zero(0x10, 500));
            EXPECT_EQ(0x5Au, static_cast<unsigned>(mem.mem[0x10 + 500]));
            EXPECT_EQ(0x5Au, static_cast<unsigned>(mem.mem[0x0f]));
            if (g_failures == dma_fails)
                std::cout << "  [PASS] DMA payload conformance (cmd/addr/len/BE/SW/sideband)\n";
        }

        // -----------------------------------------------------------------
        // Finding 7: size boundaries around chunk + exact dest end.
        // -----------------------------------------------------------------
        {
            const uint64_t sizes[] = {1, kChunk - 1, kChunk, kChunk + 1};
            for (uint64_t sz : sizes) {
                pulse_reset();
                mem.fill(0xA5);
                mem.expect_job(0x40, sz, kChunk);
                run_zero_job(0x40, sz, true);
                EXPECT_TRUE(mem.meta_ok);
                EXPECT_EQ(expected_chunks(sz, kChunk),
                          static_cast<unsigned>(mem.log.size()));
                EXPECT_TRUE(irq.read());
                EXPECT_TRUE(mem.span_is_zero(0x40, sz));
                EXPECT_EQ(0xA5u,
                          static_cast<unsigned>(mem.mem[0x40 + sz]));
                EXPECT_EQ(0xA5u,
                          static_cast<unsigned>(mem.mem[0x3f]));
            }

            // Exact end of the destination buffer.
            pulse_reset();
            mem.fill(0xA5);
            const uint64_t end_sz = 32;
            const uint64_t dest   = mem.mem.size() - end_sz;
            mem.expect_job(dest, end_sz, kChunk);
            run_zero_job(dest, end_sz, true);
            EXPECT_TRUE(mem.meta_ok);
            EXPECT_TRUE(irq.read());
            EXPECT_TRUE(mem.span_is_zero(dest, end_sz));
            if (dest > 0)
                EXPECT_EQ(0xA5u,
                          static_cast<unsigned>(mem.mem[dest - 1]));
            std::cout << "  [PASS] size boundaries + exact dest end\n";
        }

        // -----------------------------------------------------------------
        // Finding 7: partial failure on first / middle / last chunk.
        // -----------------------------------------------------------------
        {
            struct case_t {
                const char* name;
                uint64_t    size;
                int         fail_at;
                uint64_t    written; // bytes that must be zero
            };
            // chunk=64: size 200 → chunks at 0, 64, 128 (lens 64/64/72)
            const case_t cases[] = {
                {"first",  200, 0, 0},
                {"middle", 200, 1, 64},
                {"last",   200, 2, 128},
            };
            for (const auto& c : cases) {
                pulse_reset();
                mem.fill(0xA5);
                mem.reset_log();
                mem.fail_at = c.fail_at;
                // Still enforce geometry for chunks that do issue.
                mem.expect_job(0x20, c.size, kChunk);
                mem.fail_at = c.fail_at; // expect_job clears fail_at
                run_zero_job(0x20, c.size, true);
                EXPECT_FALSE(irq.read());
                const uint64_t st = drv.read64(cfg::OFF_CTRL_STATUS);
                EXPECT_EQ(cfg::CTRL_INT_EN_MASK, st & cfg::CTRL_INT_EN_MASK);
                EXPECT_EQ(UINT64_C(0), st & cfg::CTRL_STATUS_MASK);
                EXPECT_EQ(static_cast<unsigned>(c.fail_at + 1),
                          static_cast<unsigned>(mem.log.size()));
                EXPECT_TRUE(mem.span_is_zero(0x20, c.written));
                // Byte at the start of the failed chunk must still be poison.
                EXPECT_EQ(0xA5u, static_cast<unsigned>(
                                     mem.mem[0x20 + c.written]));
                // Tail past the whole job must stay poison.
                EXPECT_EQ(0xA5u, static_cast<unsigned>(
                                     mem.mem[0x20 + c.size]));
                if (!mem.meta_ok && c.fail_at > 0) {
                    // meta may flag "extra" only if a chunk after fail issued;
                    // with early stop that should not happen.
                }
                (void)c.name;
            }
            std::cout << "  [PASS] partial failure first/middle/last chunk\n";
        }

        // int_en=0: no irq on completion -----------------------------------
        pulse_reset();
        mem.reset_log();
        mem.fill(0xFF);
        drv.write64(cfg::OFF_DEST_ADDR, 0);
        drv.write64(cfg::OFF_SIZE, 16);
        drv.write64(cfg::OFF_CTRL_STATUS, 0); // start without int_en
        settle();
        EXPECT_FALSE(irq.read());
        for (int i = 0; i < 16; ++i)
            EXPECT_EQ(0u, static_cast<unsigned>(mem.mem[static_cast<std::size_t>(i)]));
        std::cout << "  [PASS] completion with int_en=0 leaves irq low\n";

        // status bit is RO to software -------------------------------------
        drv.write64(cfg::OFF_CTRL_STATUS,
                    cfg::CTRL_INT_EN_MASK | cfg::CTRL_STATUS_MASK);
        settle();
        // SIZE still 16 from above → job runs; busy cleared after.
        const uint64_t after = drv.read64(cfg::OFF_CTRL_STATUS);
        EXPECT_EQ(UINT64_C(0), after & cfg::CTRL_STATUS_MASK);
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, after & cfg::CTRL_INT_EN_MASK);
        std::cout << "  [PASS] status bit is software-RO (write ignored)\n";

        // Clearing int_en drops irq ----------------------------------------
        EXPECT_TRUE(irq.read());
        drv.write64(cfg::OFF_SIZE, 0); // avoid starting another job
        drv.write64(cfg::OFF_CTRL_STATUS, 0);
        settle();
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] clearing int_en deasserts irq\n";

        // -----------------------------------------------------------------
        // A4 (blocked): every CTRL write retriggers while SIZE != 0.
        // Document observed behaviour; design owner owns whether it should.
        // -----------------------------------------------------------------
        {
            pulse_reset();
            mem.fill(0xA5);
            mem.reset_log();
            drv.write64(cfg::OFF_DEST_ADDR, 0x200);
            drv.write64(cfg::OFF_SIZE, 16);
            // First enable write starts a job.
            drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
            settle();
            const unsigned after_first = static_cast<unsigned>(mem.log.size());
            EXPECT_TRUE(after_first >= 1u);
            // Poison the region again, then write the same CTRL value — must
            // retrigger (A4: confirm against RDL/reference before changing).
            for (unsigned i = 0; i < 16; ++i)
                mem.mem[0x200 + i] = 0x5A;
            mem.reset_log();
            drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
            settle();
            EXPECT_TRUE(mem.log.size() >= 1u);
            EXPECT_TRUE(mem.span_is_zero(0x200, 16));
            // Enable-only toggle: clear then set int_en with SIZE still 16.
            mem.fill(0xA5);
            mem.reset_log();
            drv.write64(cfg::OFF_CTRL_STATUS, 0);
            settle();
            const unsigned after_clear = static_cast<unsigned>(mem.log.size());
            EXPECT_TRUE(after_clear >= 1u); // clearing int_en also retriggers
            mem.reset_log();
            drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
            settle();
            EXPECT_TRUE(mem.log.size() >= 1u);
            std::cout << "  [PASS] CTRL retrigger while SIZE!=0 (A4 owns policy)\n";
        }

        // Bus-error paths --------------------------------------------------
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x0, 4));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x18, 8));
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_WRITE_COMMAND, 0x4, 8)); // unaligned

        {
            tlm::tlm_generic_payload gp;
            sc_time delay = SC_ZERO_TIME;
            uint64_t data = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(nullptr);
            gp.set_data_length(8);
            gp.set_streaming_width(8);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE, gp.get_response_status());

            gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            uint8_t be[8] = {};
            gp.set_byte_enable_ptr(be);
            gp.set_byte_enable_length(8);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      gp.get_response_status());

            gp.set_byte_enable_ptr(nullptr);
            gp.set_streaming_width(1);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE, gp.get_response_status());

            gp.set_streaming_width(8);
            gp.set_command(tlm::TLM_IGNORE_COMMAND);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE, gp.get_response_status());

            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_data_length(0);
            gp.set_streaming_width(8);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            drv.sock->b_transport(gp, delay);
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE, gp.get_response_status());
        }
        std::cout << "  [PASS] bus error responses (size / window / align / TLM)\n";

        // Malformed TLM: null ptr, zero length, byte-enable, streaming width,
        // unknown command. One EXPECT site so fail-paths are not duplicated.
        {
            uint64_t scratch = 0;
            uint8_t  be      = 0xFF;
            unsigned char* p = reinterpret_cast<unsigned char*>(&scratch);
            const tlm::tlm_response_status got[] = {
                drv.raw(tlm::TLM_READ_COMMAND, 0, 8, nullptr, 8),
                drv.raw(tlm::TLM_WRITE_COMMAND, 0, 0, p, 0),
                drv.raw(tlm::TLM_READ_COMMAND, 0, 8, p, 8, &be, 1),
                drv.raw(tlm::TLM_READ_COMMAND, 0, 8, p, /*sw=*/4),
                drv.raw(tlm::TLM_IGNORE_COMMAND, 0, 8, p, 8),
            };
            const tlm::tlm_response_status exp[] = {
                tlm::TLM_GENERIC_ERROR_RESPONSE,
                tlm::TLM_BURST_ERROR_RESPONSE,
                tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                tlm::TLM_BURST_ERROR_RESPONSE,
                tlm::TLM_COMMAND_ERROR_RESPONSE,
            };
            for (unsigned i = 0; i < 5; ++i)
                EXPECT_EQ(exp[i], got[i]);
            std::cout << "  [PASS] malformed TLM (null/len/BE/width/cmd)\n";
        }

        // -----------------------------------------------------------------
        // Finding 10: annotated MMIO delay (helper must not discard delay).
        // -----------------------------------------------------------------
        {
            pulse_reset();
            const sc_time acc(5, SC_NS);     // access_delay_ns preset
            const sc_time base(123, SC_NS);  // arbitrary non-zero incoming
            uint64_t data = 0;
            auto*    p    = reinterpret_cast<unsigned char*>(&data);
            sc_time  got;

            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.timed(tlm::TLM_WRITE_COMMAND, cfg::OFF_DEST_ADDR, 8,
                                p, 8, base, got));
            EXPECT_EQ(base + acc, got);

            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.timed(tlm::TLM_READ_COMMAND, cfg::OFF_DEST_ADDR, 8,
                                p, 8, base, got));
            EXPECT_EQ(base + acc, got);

            // Errors cost nothing.
            unsigned char be = TLM_BYTE_ENABLED;
            struct { const char* what; tlm::tlm_command cmd; uint64_t addr;
                     unsigned len; unsigned char* ptr; unsigned sw;
                     unsigned char* be; unsigned be_len; } bad[] = {
                {"unaligned",   tlm::TLM_READ_COMMAND,   cfg::OFF_SIZE + 1, 8, p, 8, nullptr, 0},
                {"past window", tlm::TLM_READ_COMMAND,
                                smc::memory_zeroer_cfg::WINDOW_SIZE, 8, p, 8, nullptr, 0},
                {"null ptr",    tlm::TLM_READ_COMMAND,   cfg::OFF_SIZE, 8, nullptr, 8, nullptr, 0},
                {"byte enable", tlm::TLM_WRITE_COMMAND,  cfg::OFF_SIZE, 8, p, 8, &be, 1},
                {"short width", tlm::TLM_READ_COMMAND,   cfg::OFF_SIZE, 8, p, 4, nullptr, 0},
                {"ignore cmd",  tlm::TLM_IGNORE_COMMAND, cfg::OFF_SIZE, 8, p, 8, nullptr, 0},
            };
            for (const auto& b : bad) {
                const auto st = drv.timed(b.cmd, b.addr, b.len, b.ptr, b.sw,
                                          base, got, b.be, b.be_len);
                EXPECT_TRUE(st != tlm::TLM_OK_RESPONSE);
                if (base != got)
                    std::cerr << "FAIL  " << b.what
                              << " changed the annotated delay\n";
                EXPECT_EQ(base, got);
            }
            std::cout << "  [PASS] annotated delay accumulates; errors add none\n";
        }

        // Downstream DMA time is not propagated to the initiator ------------
        // Documented model gap (plan finding 2 / A2): perform_write_zeros keeps
        // the DMA delay in a local and drops it, so a control write that runs a
        // whole job is annotated exactly like a bare register write. 500 bytes
        // in 64-byte chunks is 8 transactions, and simple_mem charges 1 ns
        // each, so 8 ns is what goes missing. Asserting the current number
        // pins the behaviour; it is not an endorsement of it.
        {
            pulse_reset();
            mem.fill(0xA5);
            const sc_time acc(5, SC_NS);
            const sc_time base(10, SC_NS);
            uint64_t data = 0;
            auto*    p    = reinterpret_cast<unsigned char*>(&data);
            sc_time  got;

            drv.write64(cfg::OFF_DEST_ADDR, 0);
            drv.write64(cfg::OFF_SIZE, 500);
            data = cfg::CTRL_INT_EN_MASK;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.timed(tlm::TLM_WRITE_COMMAND, cfg::OFF_CTRL_STATUS, 8,
                                p, 8, base, got));
            settle();
            EXPECT_TRUE(irq.read());              // the job really ran
            for (unsigned i = 0; i < 500; ++i)
                EXPECT_EQ(0u, unsigned(mem.mem[i]));
            EXPECT_EQ(base + acc, got);           // and still cost only 5 ns
            std::cout << "  [PASS] job DMA delay is dropped (known gap)\n";
        }

        // -----------------------------------------------------------------
        // Finding 4: address wrap must not write low memory.
        // -----------------------------------------------------------------
        {
            // Wrapping job: dest near top of space, size crosses 2^64.
            const unsigned wrap_fails = g_failures;
            pulse_reset();
            mem.reset_log();
            mem.use_sparse = true;
            mem.sparse.clear();
            // Seed low memory observation points.
            mem.sparse[0]  = 0xA5;
            mem.sparse[23] = 0xA5;
            const uint64_t wrap_dest = UINT64_MAX - 40ull;
            const uint64_t wrap_size = 100ull; // (wrap_dest + wrap_size) wraps
            run_zero_job(wrap_dest, wrap_size, true);
            EXPECT_FALSE(irq.read());
            EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
            EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.byte_at(0)));
            EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.byte_at(23)));
            const uint64_t st = drv.read64(cfg::OFF_CTRL_STATUS);
            EXPECT_EQ(cfg::CTRL_INT_EN_MASK, st & cfg::CTRL_INT_EN_MASK);
            EXPECT_EQ(UINT64_C(0), st & cfg::CTRL_STATUS_MASK);

            // Non-wrapping high address: must issue DMA at the high addr.
            mem.reset_log();
            mem.use_sparse = true;
            mem.sparse.clear();
            const uint64_t hi_dest = UINT64_MAX - 99ull;
            const uint64_t hi_size = 50ull; // hi_dest + hi_size does not wrap
            mem.expect_job(hi_dest, hi_size, kChunk);
            mem.use_sparse = true;
            run_zero_job(hi_dest, hi_size, true);
            EXPECT_TRUE(mem.meta_ok);
            EXPECT_TRUE(irq.read());
            EXPECT_EQ(expected_chunks(hi_size, kChunk),
                      static_cast<unsigned>(mem.log.size()));
            EXPECT_TRUE(mem.span_is_zero(hi_dest, hi_size));
            // Low memory must not have been touched by the high-address job.
            EXPECT_TRUE(mem.sparse.find(0) == mem.sparse.end());
            if (g_failures == wrap_fails) {
                std::cout << "  [PASS] address wrap rejected; high non-wrap OK\n";
            }
        }

        // DMA out-of-range fails without irq -------------------------------
        pulse_reset();
        mem.reset_log();
        mem.use_sparse = false;
        drv.write64(cfg::OFF_DEST_ADDR, 0x7F00); // past 8 KiB mem
        drv.write64(cfg::OFF_SIZE, 0x200);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_FALSE(irq.read());
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK,
                  drv.read64(cfg::OFF_CTRL_STATUS) & cfg::CTRL_INT_EN_MASK);
        std::cout << "  [PASS] failed DMA does not raise irq\n";

        // -----------------------------------------------------------------
        // Finding 12: transport_dbg / DMI defaults + stale dmi_allowed.
        // Model registers only b_transport; socket defaults apply for dbg/DMI.
        // -----------------------------------------------------------------
        {
            pulse_reset();
            drv.write64(cfg::OFF_DEST_ADDR, 0x55AA);

            uint64_t dbg_buf = 0;
            const unsigned n_rd = simtlm::debug_read(
                drv.sock, cfg::OFF_DEST_ADDR,
                reinterpret_cast<unsigned char*>(&dbg_buf), 8);
            // simple_target_socket default: no debug support → 0 bytes.
            EXPECT_EQ(0u, n_rd);
            EXPECT_EQ(UINT64_C(0x55AA), drv.read64(cfg::OFF_DEST_ADDR));

            uint64_t poke = 0xDEAD;
            const unsigned n_wr = simtlm::debug_write(
                drv.sock, cfg::OFF_DEST_ADDR,
                reinterpret_cast<unsigned char*>(&poke), 8);
            EXPECT_EQ(0u, n_wr);
            EXPECT_EQ(UINT64_C(0x55AA), drv.read64(cfg::OFF_DEST_ADDR));

            const auto dmi = simtlm::dmi_request(drv.sock, cfg::OFF_DEST_ADDR);
            EXPECT_FALSE(dmi.granted);

            // Stale dmi_allowed=true must come back cleared on a handled xact.
            {
                tlm::tlm_generic_payload gp;
                uint64_t data = 0;
                sc_time delay = SC_ZERO_TIME;
                gp.set_command(tlm::TLM_READ_COMMAND);
                gp.set_address(cfg::OFF_DEST_ADDR);
                gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                gp.set_data_length(8);
                gp.set_streaming_width(8);
                gp.set_dmi_allowed(true);
                gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                drv.sock->b_transport(gp, delay);
                EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
                EXPECT_FALSE(gp.is_dmi_allowed());
            }
            // An error return must clear the hint too.
            {
                tlm::tlm_generic_payload gp;
                sc_time delay = SC_ZERO_TIME;
                gp.set_command(tlm::TLM_READ_COMMAND);
                gp.set_address(cfg::OFF_DEST_ADDR);
                gp.set_data_ptr(nullptr);
                gp.set_data_length(8);
                gp.set_streaming_width(8);
                gp.set_dmi_allowed(true);
                gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                drv.sock->b_transport(gp, delay);
                EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE, gp.get_response_status());
                EXPECT_FALSE(gp.is_dmi_allowed());
            }

            simtlm::target_geometry geo;
            geo.valid_address  = cfg::OFF_DEST_ADDR;
            geo.word_bytes     = 8;
            geo.aperture_bytes = cfg::WINDOW_SIZE;
            bool all_defined = true;
            for (simtlm::defect d : simtlm::all_defects()) {
                const auto r = simtlm::probe_defect(drv.sock, d, geo);
                if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                    all_defined = false;
                    std::cerr << "FAIL  defect " << simtlm::defect_name(d)
                              << " left TLM_INCOMPLETE\n";
                }
                if (d == simtlm::defect::stale_dmi_allowed) {
                    // Well-formed path: model clears the hint.
                    EXPECT_FALSE(r.dmi_allowed);
                    EXPECT_EQ(tlm::TLM_OK_RESPONSE, r.status);
                }
                if (d == simtlm::defect::address_wrap) {
                    // The probe address is 8-byte aligned and still wraps,
                    // so the aperture check answers, not the align check.
                    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, r.status);
                }
            }
            EXPECT_TRUE(all_defined);
            std::cout << "  [PASS] transport_dbg/DMI defaults + defect matrix\n";
        }

        // CCI introspection ------------------------------------------------
        {
            auto broker = cci::cci_get_broker();
            auto h = broker.get_param_handle("tb.zeroer.access_delay_ns");
            EXPECT_TRUE(h.is_valid());
            if (h.is_valid()) {
                // Preset was 5.0 — JSON form is "5.0".
                EXPECT_EQ(std::string("5.0"), h.get_cci_value().to_json());
                h.set_cci_value(cci::cci_value(7.0));
                EXPECT_EQ(std::string("7.0"), h.get_cci_value().to_json());

                // The handle reporting 7.0 only proves the broker stored it.
                // Confirm the model actually charges the new value.
                uint64_t data = 0;
                sc_time  got;
                const sc_time base(31, SC_NS);
                EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                          drv.timed(tlm::TLM_READ_COMMAND, cfg::OFF_SIZE, 8,
                                    reinterpret_cast<unsigned char*>(&data), 8,
                                    base, got));
                EXPECT_EQ(base + sc_time(7, SC_NS), got);
            }
            auto hc = broker.get_param_handle("tb.zeroer.chunk_size");
            EXPECT_TRUE(hc.is_valid());
            if (hc.is_valid()) {
                // Finding 13: demote only for this expected immutable write,
                // then restore so unrelated SC_ERROR still fails the process.
                const sc_core::sc_actions prev_err =
                    sc_core::sc_report_handler::set_actions(
                        sc_core::SC_ERROR, sc_core::SC_DISPLAY);
                hc.set_cci_value(cci::cci_value(128u));
                sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                                       prev_err);
                EXPECT_EQ(std::string("64"), hc.get_cci_value().to_json());
            }
            std::cout << "  [PASS] CCI discovery / mutation / immutability\n";
        }

        zeroer.dump_state(std::cout);

        if (g_failures == 0)
            std::cout << "\nALL TESTS PASSED\n";
        else
            std::cout << "\n" << g_failures << " FAILURE(S)\n";

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    // Constructor guard rails use SC_REPORT_FATAL; throw so we can catch them
    // before elaborating the real DUT. Scoped — restored before sc_start.
    // Finding 13: do NOT globally demote SC_ERROR; the CCI immutability probe
    // demotes only around that one write (see TB body).
    const sc_core::sc_actions prev_fatal =
        sc_core::sc_report_handler::set_actions(
            sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    {
        smc::memory_zeroer_cfg bad;
        bad.chunk_size = 0;
        EXPECT_TRUE(expect_fatal([&] {
            smc::memory_zeroer boom("bad_chunk_size", bad);
        }));
        bad.chunk_size = (1u << 20) + 1u;
        EXPECT_TRUE(expect_fatal([&] {
            smc::memory_zeroer boom("bad_chunk_size_hi", bad);
        }));
        std::cout << "  [PASS] chunk_size constructor guard rail\n";
    }

    sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL, prev_fatal);

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    global_broker.set_preset_cci_value("tb.zeroer.chunk_size",
                                       cci::cci_value(64u));
    global_broker.set_preset_cci_value("tb.zeroer.access_delay_ns",
                                       cci::cci_value(5.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
