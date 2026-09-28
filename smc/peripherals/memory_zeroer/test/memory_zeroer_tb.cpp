// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// memory_zeroer_tb.cpp -- self-checking test bench for the SMC memory zeroer.
//
// The DMA destination (`simple_mem`) is also a full transaction monitor: it
// records the command, address, length, streaming width, byte-enable pointer,
// initial response status, payload contents and AXI sideband of every outbound
// chunk.  Cases then assert the exact chunk sequence a job must produce rather
// than only inspecting the memory image afterwards.
//
// Cases are split into two kinds, and the split is deliberate:
//
//   ARCHITECTURAL cases drive the model only through its sockets and signals.
//   Only these may be cited as evidence about zeroer behaviour.
//
//   STRUCTURAL cases exercise the model's own debug getters and dump_state.
//   They keep the debug surface working but prove nothing architectural.
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
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "memory_zeroer.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

void fail_at(const char* file, int line, const std::string& what)
{
    std::cerr << "FAIL " << file << ":" << line << "  " << what << "\n";
    ++g_failures;
}

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::ostringstream _o;                                             \
            _o << "expected=" << _e << " actual=" << _a                        \
               << "  (" #expected " == " #actual ")";                          \
            fail_at(__FILE__, __LINE__, _o.str());                             \
        }                                                                      \
    } while (0)

/// EXPECT_EQ with a caller-supplied context string, so a failure inside a
/// sweep names the size / chunk / transaction it came from.
#define EXPECT_EQ_CTX(expected, actual, ctx)                                   \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::ostringstream _o;                                             \
            _o << (ctx) << ": expected=" << _e << " actual=" << _a;            \
            fail_at(__FILE__, __LINE__, _o.str());                             \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) fail_at(__FILE__, __LINE__, "expected TRUE: " #cond);     \
    } while (0)

#define EXPECT_TRUE_CTX(cond, ctx)                                             \
    do {                                                                       \
        if (!(cond))                                                           \
            fail_at(__FILE__, __LINE__,                                        \
                    std::string(ctx) + ": expected TRUE: " #cond);             \
    } while (0)

#define EXPECT_FALSE(cond)                                                     \
    do {                                                                       \
        if (cond) fail_at(__FILE__, __LINE__, "expected FALSE: " #cond);       \
    } while (0)

#define EXPECT_TIME_EQ_CTX(expected, actual, ctx)                              \
    do {                                                                       \
        const sc_time _e = (expected);                                         \
        const sc_time _a = (actual);                                           \
        if (_e != _a) {                                                        \
            fail_at(__FILE__, __LINE__,                                        \
                    std::string(ctx) + ": expected=" + _e.to_string() +        \
                        " actual=" + _a.to_string());                          \
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

/// Scopes an expected SC_ERROR to one statement.  The old bench demoted
/// SC_ERROR globally for the whole run, which would have hidden any unrelated
/// error raised afterwards.
struct scoped_error_demotion {
    sc_core::sc_actions saved;
    scoped_error_demotion()
        : saved(sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                                        sc_core::SC_DISPLAY))
    {
    }
    ~scoped_error_demotion()
    {
        sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, saved);
    }
};

// ---------------------------------------------------------------------------
// DMA destination + outbound transaction monitor
// ---------------------------------------------------------------------------

/// Everything the bench needs to judge one outbound chunk.
struct dma_record {
    tlm::tlm_command cmd;
    uint64_t         addr;
    unsigned         len;
    unsigned         streaming_width;
    bool             had_byte_enable;
    bool             response_initialised;  ///< arrived as TLM_INCOMPLETE
    bool             all_zero;              ///< payload was all 0x00
    bool             had_extension;
    uint16_t         source_id;
    uint8_t          prot;
    bool             is_user;
    bool             is_secure;
    bool             is_fetch;
    bool             is_locked;
    sc_time          delay_in;  ///< delay already accumulated on arrival
};

struct simple_mem : sc_core::sc_module {
    tlm_utils::simple_target_socket<simple_mem> sock;
    std::vector<uint8_t>                        mem;
    std::vector<dma_record>                     log;

    /// Per-access delay this target annotates.
    sc_time access_delay{1, SC_NS};

    /// Fail the Nth write seen since the last `arm_fault` (0-based); -1 = off.
    int  fault_index = -1;
    int  seen_since_arm = 0;

    /// Invoked while servicing each chunk, before it completes.  Used to
    /// re-enter the zeroer's CSR port from inside its own job.
    std::function<void()> on_chunk;

    explicit simple_mem(sc_module_name n, std::size_t bytes)
        : sc_module(n), sock("sock"), mem(bytes, 0xA5)
    {
        sock.register_b_transport(this, &simple_mem::b_transport);
    }

    void fill(uint8_t v) { std::fill(mem.begin(), mem.end(), v); }
    void clear_log() { log.clear(); }
    void arm_fault(int idx) { fault_index = idx; seen_since_arm = 0; }
    void disarm_fault() { fault_index = -1; seen_since_arm = 0; }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        const uint64_t adr = gp.get_address();
        const unsigned len = gp.get_data_length();
        unsigned char* ptr = gp.get_data_ptr();

        dma_record r{};
        r.cmd                  = gp.get_command();
        r.addr                 = adr;
        r.len                  = len;
        r.streaming_width      = gp.get_streaming_width();
        r.had_byte_enable      = gp.get_byte_enable_ptr() != nullptr;
        r.response_initialised =
            gp.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE;
        r.delay_in             = delay;
        r.all_zero             = ptr != nullptr && len != 0 &&
                     std::all_of(ptr, ptr + len,
                                 [](unsigned char c) { return c == 0; });

        if (on_chunk) on_chunk();

        const auto* ext = gp.get_extension<smc::smc_axi_extension>();
        r.had_extension = ext != nullptr;
        if (ext != nullptr) {
            r.source_id = ext->source_id;
            r.prot      = ext->prot;
            r.is_user   = ext->is_user;
            r.is_secure = ext->is_secure;
            r.is_fetch  = ext->is_fetch;
            r.is_locked = ext->is_locked;
        }
        // Logged before the fault and range checks run, so a rejected chunk
        // still appears: the failure cases assert *which* chunk stopped the
        // job, which needs the failing transaction itself to be recorded.
        log.push_back(r);

        if (fault_index >= 0 && seen_since_arm++ == fault_index) {
            gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        if (ptr == nullptr || len == 0 || adr >= mem.size() ||
            len > mem.size() - adr) {
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
        delay += access_delay;
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    /// True iff every byte of [begin, end) equals @p v.
    bool region_is(uint64_t begin, uint64_t end, uint8_t v) const
    {
        for (uint64_t i = begin; i < end; ++i) {
            if (mem[static_cast<std::size_t>(i)] != v) return false;
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
// Register-bus driver
// ---------------------------------------------------------------------------

struct req {
    tlm::tlm_command cmd  = tlm::TLM_READ_COMMAND;
    uint64_t         addr = 0;
    unsigned         len  = 8;
    void*            data = nullptr;
    /// -1 => set streaming_width to len.
    int              streaming_width = -1;
    uint8_t*         be      = nullptr;
    unsigned         be_len  = 0;
    sc_time          delay_in = SC_ZERO_TIME;
    smc::smc_axi_extension* ext = nullptr;
};

struct rsp {
    tlm::tlm_response_status status      = tlm::TLM_INCOMPLETE_RESPONSE;
    sc_time                  delay_delta = SC_ZERO_TIME;
    bool                     dmi_allowed = true;
};

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    /// Detaches a bench-owned extension before the payload is destroyed:
    /// ~tlm_generic_payload calls free() -- delete -- on every extension slot.
    struct ext_detacher {
        tlm::tlm_generic_payload& gp;
        bool                      attached;
        ~ext_detacher()
        {
            if (attached) gp.clear_extension<smc::smc_axi_extension>();
        }
    };

    rsp send(const req& r)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = r.delay_in;

        gp.set_command(r.cmd);
        gp.set_address(r.addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(r.data));
        gp.set_data_length(r.len);
        gp.set_streaming_width(r.streaming_width < 0
                                   ? r.len
                                   : static_cast<unsigned>(r.streaming_width));
        gp.set_byte_enable_ptr(r.be);
        gp.set_byte_enable_length(r.be_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (r.ext != nullptr) gp.set_extension(r.ext);
        const ext_detacher detach{gp, r.ext != nullptr};

        sock->b_transport(gp, delay);

        rsp out;
        out.status      = gp.get_response_status();
        out.delay_delta = delay - r.delay_in;
        out.dmi_allowed = gp.is_dmi_allowed();
        return out;
    }

    uint64_t read64(uint64_t addr)
    {
        uint64_t data = 0;
        req r;
        r.cmd = tlm::TLM_READ_COMMAND;
        r.addr = addr;
        r.data = &data;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, send(r).status);
        return data;
    }

    void write64(uint64_t addr, uint64_t data)
    {
        req r;
        r.cmd = tlm::TLM_WRITE_COMMAND;
        r.addr = addr;
        r.data = &data;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, send(r).status);
    }

    tlm::tlm_response_status try_access(tlm::tlm_command cmd, uint64_t addr,
                                        unsigned len)
    {
        std::vector<uint8_t> buf(len ? len : 1, 0);
        req r;
        r.cmd = cmd;
        r.addr = addr;
        r.len = len;
        r.data = buf.data();
        return send(r).status;
    }

    /// Raw back-door access over transport_dbg.  Returns bytes transferred.
    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, unsigned len, void* data,
                 int streaming_width = -1, uint8_t* be = nullptr,
                 unsigned be_len = 0)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return sock->transport_dbg(gp);
    }

    uint64_t dbg_read64(uint64_t addr)
    {
        uint64_t v = 0;
        EXPECT_EQ(8u, dbg(tlm::TLM_READ_COMMAND, addr, 8, &v));
        return v;
    }

    bool dmi(uint64_t addr, tlm::tlm_dmi& dmi_data)
    {
        tlm::tlm_generic_payload gp;
        uint64_t scratch = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&scratch));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        return sock->get_direct_mem_ptr(gp, dmi_data);
    }
};

// ---------------------------------------------------------------------------
// Top-level TB
// ---------------------------------------------------------------------------

/// chunk_size preset in sc_main; every chunking expectation derives from it.
constexpr unsigned kChunk = 64u;
/// access_delay_ns preset in sc_main.
constexpr double kAccessDelayNs = 5.0;

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

    void settle() { for (int i = 0; i < 3; ++i) wait(SC_ZERO_TIME); }

    void pulse_reset()
    {
        rst_n.write(false);
        wait(10, SC_NS);
        rst_n.write(true);
        settle();
    }

    /// Program DEST/SIZE and trigger a job, returning the trigger's response.
    rsp start_job(uint64_t dest, uint64_t size, uint64_t ctrl,
                  sc_time delay_in = SC_ZERO_TIME)
    {
        using cfg = smc::memory_zeroer_cfg;
        drv.write64(cfg::OFF_DEST_ADDR, dest);
        drv.write64(cfg::OFF_SIZE, size);
        mem.clear_log();
        req r;
        r.cmd  = tlm::TLM_WRITE_COMMAND;
        r.addr = cfg::OFF_CTRL_STATUS;
        r.data = &ctrl;
        r.delay_in = delay_in;
        return drv.send(r);
    }

    /// Assert that the recorded chunk sequence is exactly what a job of
    /// @p size bytes at @p dest must emit.
    void check_chunks(uint64_t dest, uint64_t size, const std::string& ctx);

    void run();

    void t_registers_and_reset();
    void t_chunking();
    void t_sideband_and_payload();
    void t_faults();
    void t_mmio_contract();
    void t_debug_and_dmi();
    void t_timing();
    void t_irq_and_retrigger();
    void t_structural();
};

void tb::check_chunks(uint64_t dest, uint64_t size, const std::string& ctx)
{
    const uint64_t expected_n = (size + kChunk - 1) / kChunk;
    EXPECT_EQ_CTX(expected_n, static_cast<uint64_t>(mem.log.size()),
                  ctx + " [chunk count]");
    if (mem.log.size() != expected_n) return;

    uint64_t offset = 0;
    for (std::size_t i = 0; i < mem.log.size(); ++i) {
        const dma_record& r = mem.log[i];
        std::ostringstream o;
        o << ctx << " [chunk " << i << "]";
        const std::string c = o.str();

        const uint64_t expect_len = std::min<uint64_t>(kChunk, size - offset);
        EXPECT_EQ_CTX(tlm::TLM_WRITE_COMMAND, r.cmd, c + " command");
        EXPECT_EQ_CTX(dest + offset, r.addr, c + " address");
        EXPECT_EQ_CTX(expect_len, static_cast<uint64_t>(r.len), c + " length");
        // A single-beat write: streaming width must equal the length.
        EXPECT_EQ_CTX(expect_len, static_cast<uint64_t>(r.streaming_width),
                      c + " streaming width");
        EXPECT_TRUE_CTX(!r.had_byte_enable, c + " no byte enables");
        EXPECT_TRUE_CTX(r.response_initialised, c + " response initialised");
        EXPECT_TRUE_CTX(r.all_zero, c + " payload all zero");
        offset += expect_len;
    }
    EXPECT_EQ_CTX(size, offset, ctx + " [total bytes]");
}

// ---------------------------------------------------------------------------
// Architectural: registers and reset
// ---------------------------------------------------------------------------
void tb::t_registers_and_reset()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: registers and reset ---\n";

    pulse_reset();
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_DEST_ADDR));
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_SIZE));
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
    EXPECT_FALSE(irq.read());
    std::cout << "  [PASS] reset clears registers and irq\n";

    // DEST_ADDR / SIZE are plain 64-bit RW: full-width patterns survive.
    for (uint64_t pat : {UINT64_C(0x100), ~UINT64_C(0),
                         UINT64_C(0x0123456789ABCDEF)}) {
        drv.write64(cfg::OFF_DEST_ADDR, pat);
        drv.write64(cfg::OFF_SIZE, pat);
        std::ostringstream o;
        o << "RW pattern 0x" << std::hex << pat;
        EXPECT_EQ_CTX(pat, drv.read64(cfg::OFF_DEST_ADDR), o.str() + " dest");
        EXPECT_EQ_CTX(pat, drv.read64(cfg::OFF_SIZE), o.str() + " size");
    }
    drv.write64(cfg::OFF_SIZE, 0);  // do not leave a job armed
    std::cout << "  [PASS] DEST_ADDR / SIZE are full-width RW\n";

    // CTRL_STATUS: int_en is RW, the busy bit is RO to software, and every
    // reserved bit reads as zero.
    drv.write64(cfg::OFF_CTRL_STATUS, ~UINT64_C(0));
    EXPECT_EQ(cfg::CTRL_INT_EN_MASK, drv.read64(cfg::OFF_CTRL_STATUS));
    drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_STATUS_MASK);
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
    std::cout << "  [PASS] CTRL_STATUS int_en RW, busy RO, reserved RAZ\n";

    // SIZE = 0 must not start a job at all.
    mem.fill(0xA5);
    mem.clear_log();
    drv.write64(cfg::OFF_DEST_ADDR, 0x100);
    drv.write64(cfg::OFF_SIZE, 0);
    drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
    settle();
    EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
    EXPECT_EQ(cfg::CTRL_INT_EN_MASK, drv.read64(cfg::OFF_CTRL_STATUS));
    EXPECT_TRUE(mem.region_is(0, mem.mem.size(), 0xA5));
    EXPECT_FALSE(irq.read());
    std::cout << "  [PASS] SIZE=0 starts no job and emits no DMA\n";

    // Reset while registers are dirty restores the whole image and drops irq.
    mem.fill(0xA5);
    start_job(0x100, 16, cfg::CTRL_INT_EN_MASK);
    settle();
    EXPECT_TRUE(irq.read());
    pulse_reset();
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_DEST_ADDR));
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_SIZE));
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
    EXPECT_FALSE(irq.read());
    // Repeated reset is idempotent.
    pulse_reset();
    pulse_reset();
    EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
    EXPECT_FALSE(irq.read());
    std::cout << "  [PASS] reset after a completed job, repeated reset\n";
}

// ---------------------------------------------------------------------------
// Architectural: chunking across every boundary
// ---------------------------------------------------------------------------
void tb::t_chunking()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: chunk boundaries ---\n";
    pulse_reset();

    // Sizes around the chunk boundary, including the short final chunk.
    const uint64_t sizes[] = {1, kChunk - 1, kChunk, kChunk + 1,
                              2 * kChunk - 1, 2 * kChunk, 500};
    const uint64_t dest = 0x400;
    for (uint64_t size : sizes) {
        std::ostringstream o;
        o << "size " << size;
        mem.fill(0xA5);
        start_job(dest, size, cfg::CTRL_INT_EN_MASK);
        settle();

        check_chunks(dest, size, o.str());
        // Exactly the requested span is zeroed -- not one byte more.
        EXPECT_TRUE_CTX(mem.region_is(dest, dest + size, 0x00),
                        o.str() + " [span zeroed]");
        EXPECT_TRUE_CTX(mem.region_is(dest - 1, dest, 0xA5),
                        o.str() + " [byte before untouched]");
        EXPECT_TRUE_CTX(mem.region_is(dest + size, dest + size + 1, 0xA5),
                        o.str() + " [byte after untouched]");
        EXPECT_TRUE_CTX(irq.read(), o.str() + " [completion irq]");
    }
    std::cout << "  [PASS] sizes 1, C-1, C, C+1, 2C-1, 2C and 500: exact "
                 "chunk spans\n";

    // Destination at the very end of the target: the last byte is writable,
    // one byte past it fails.
    {
        const uint64_t end = mem.mem.size();
        mem.fill(0xA5);
        start_job(end - kChunk, kChunk, cfg::CTRL_INT_EN_MASK);
        settle();
        check_chunks(end - kChunk, kChunk, "destination at memory end");
        EXPECT_TRUE(mem.region_is(end - kChunk, end, 0x00));
        EXPECT_TRUE(irq.read());
    }
    std::cout << "  [PASS] destination ending exactly at the memory end\n";

    // A span that would wrap the 64-bit address space must be refused before
    // any DMA is issued, rather than folding back into low memory.
    {
        mem.fill(0xA5);
        start_job(UINT64_MAX - 7, 64, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
        EXPECT_TRUE(mem.region_is(0, mem.mem.size(), 0xA5));
        EXPECT_FALSE(irq.read());
        EXPECT_EQ(UINT64_C(0),
                  drv.read64(cfg::OFF_CTRL_STATUS) & cfg::CTRL_STATUS_MASK);
    }
    std::cout << "  [PASS] 64-bit wrapping span is refused, no DMA issued\n";
}

// ---------------------------------------------------------------------------
// Architectural: outbound sideband and payload conformance
// ---------------------------------------------------------------------------
void tb::t_sideband_and_payload()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: outbound sideband ---\n";
    pulse_reset();

    // A multi-chunk job with a short final chunk, so the final chunk's
    // sideband is covered too.
    const uint64_t dest = 0x200;
    const uint64_t size = 2 * kChunk + 8;
    mem.fill(0xA5);
    start_job(dest, size, cfg::CTRL_INT_EN_MASK);
    settle();

    check_chunks(dest, size, "sideband job");
    EXPECT_EQ(3u, static_cast<unsigned>(mem.log.size()));

    for (std::size_t i = 0; i < mem.log.size(); ++i) {
        const dma_record& r = mem.log[i];
        std::ostringstream o;
        o << "sideband chunk " << i;
        const std::string c = o.str();
        EXPECT_TRUE_CTX(r.had_extension, c + " present");
        // Trusted internal master, privileged, non-secure, data, not exclusive.
        EXPECT_EQ_CTX(static_cast<uint16_t>(smc::SMC_ID), r.source_id,
                      c + " source_id");
        EXPECT_TRUE_CTX(!r.is_user, c + " privileged");
        EXPECT_TRUE_CTX(!r.is_secure, c + " non-secure");
        EXPECT_TRUE_CTX(!r.is_fetch, c + " data (not fetch)");
        EXPECT_TRUE_CTX(!r.is_locked, c + " not exclusive");
        // prot[0]=data, prot[1]=non-secure, prot[2]=privileged, prot[3]=lock.
        EXPECT_EQ_CTX(0x7u, static_cast<unsigned>(r.prot), c + " prot");
    }
    std::cout << "  [PASS] canonical sideband on every chunk incl. the short "
                 "final one\n";
}

// ---------------------------------------------------------------------------
// Architectural: DMA failure injection
// ---------------------------------------------------------------------------
void tb::t_faults()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: DMA failure handling ---\n";

    // A job of four chunks; fail each position in turn and check the job stops
    // there, leaves busy clear, and raises no completion interrupt.
    const uint64_t dest = 0x800;
    const uint64_t size = 4 * kChunk;
    for (int fault : {0, 1, 3}) {
        pulse_reset();
        mem.fill(0xA5);
        mem.arm_fault(fault);
        start_job(dest, size, cfg::CTRL_INT_EN_MASK);
        settle();
        mem.disarm_fault();

        std::ostringstream o;
        o << "fault at chunk " << fault;
        // The job stops at the failing chunk: no further transactions.
        EXPECT_EQ_CTX(static_cast<std::size_t>(fault) + 1u, mem.log.size(),
                      o.str() + " [stops at failure]");
        // Chunks before the failure landed; the failing chunk and everything
        // after it did not.
        EXPECT_TRUE_CTX(
            mem.region_is(dest, dest + static_cast<uint64_t>(fault) * kChunk,
                          0x00),
            o.str() + " [earlier chunks written]");
        EXPECT_TRUE_CTX(
            mem.region_is(dest + static_cast<uint64_t>(fault) * kChunk,
                          dest + size, 0xA5),
            o.str() + " [failing and later chunks not written]");
        // No completion interrupt, and busy is clear again.
        EXPECT_TRUE_CTX(!irq.read(), o.str() + " [no completion irq]");
        const uint64_t ctrl = drv.read64(cfg::OFF_CTRL_STATUS);
        EXPECT_EQ_CTX(UINT64_C(0), ctrl & cfg::CTRL_STATUS_MASK,
                      o.str() + " [busy clear]");
        EXPECT_EQ_CTX(cfg::CTRL_INT_EN_MASK, ctrl & cfg::CTRL_INT_EN_MASK,
                      o.str() + " [int_en retained]");
    }
    std::cout << "  [PASS] first / middle / final chunk failure: exact stop "
                 "point, no irq\n";

    // NOTE (open spec question, audit finding 8): a failed job leaves no
    // software-visible error status -- firmware cannot tell a failed job from
    // one that never ran.  The bench pins today's behaviour so a future error
    // CSR is a deliberate, reviewed change rather than an accident.
    {
        pulse_reset();
        mem.arm_fault(0);
        start_job(0x800, kChunk, cfg::CTRL_INT_EN_MASK);
        settle();
        mem.disarm_fault();
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, drv.read64(cfg::OFF_CTRL_STATUS));
        EXPECT_FALSE(irq.read());
    }
    // A destination the downstream target rejects behaves the same way.
    {
        pulse_reset();
        mem.fill(0xA5);
        start_job(0x7F00, 0x200, cfg::CTRL_INT_EN_MASK);  // past 8 KiB
        settle();
        EXPECT_FALSE(irq.read());
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK,
                  drv.read64(cfg::OFF_CTRL_STATUS) & cfg::CTRL_INT_EN_MASK);
    }
    std::cout << "  [PASS] failure leaves only int_en (no error CSR today)\n";
}

// ---------------------------------------------------------------------------
// Architectural: MMIO bus contract
// ---------------------------------------------------------------------------
void tb::t_mmio_contract()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: MMIO bus contract ---\n";
    pulse_reset();

    uint64_t scratch = 0;

    // Width and alignment: only naturally aligned 8-byte accesses.
    for (unsigned len : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 9u, 16u}) {
        std::ostringstream o;
        o << "length " << len;
        EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, 0, len), o.str());
    }
    for (uint64_t off : {UINT64_C(1), UINT64_C(2), UINT64_C(4), UINT64_C(7)}) {
        std::ostringstream o;
        o << "misaligned +" << off;
        EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, off, 8), o.str());
    }
    std::cout << "  [PASS] only naturally aligned 8-byte accesses are served\n";

    // Address space: the three registers, the interior holes, the last valid
    // register, the first address past the window, and 64-bit wrap shapes.
    for (uint64_t off : {cfg::OFF_DEST_ADDR, cfg::OFF_SIZE,
                         cfg::OFF_CTRL_STATUS}) {
        std::ostringstream o;
        o << "register @0x" << std::hex << off;
        EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, off, 8), o.str());
    }
    // 0x04 / 0x0c / 0x14 are inside the window but unaligned: burst errors.
    for (uint64_t hole : {UINT64_C(0x04), UINT64_C(0x0c), UINT64_C(0x14)}) {
        std::ostringstream o;
        o << "interior hole @0x" << std::hex << hole;
        EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, hole, 8), o.str());
    }
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, cfg::WINDOW_SIZE, 8));
    // `adr + len` wraps for these; a target checking the sum would accept them.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX - 7, 8));
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX, 8));
    std::cout << "  [PASS] window, interior holes and 64-bit wrap addresses\n";

    // Null pointer / zero length / unsupported command.
    {
        req r;
        r.addr = 0; r.len = 8; r.data = nullptr;
        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE, drv.send(r).status);
        req z;
        z.addr = 0; z.len = 0; z.data = &scratch; z.streaming_width = 0;
        EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE, drv.send(z).status);
        req c;
        c.cmd = tlm::TLM_IGNORE_COMMAND; c.addr = 0; c.data = &scratch;
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE, drv.send(c).status);
    }
    std::cout << "  [PASS] null pointer, zero length, unsupported command\n";

    // Byte enables are refused at every byte-enable length, including a fully
    // enabled mask; a null pointer with a stale length stays legal.
    {
        uint8_t be[8];
        std::fill(std::begin(be), std::end(be), uint8_t{0xFF});
        for (unsigned be_len : {0u, 1u, 2u, 4u, 8u}) {
            for (tlm::tlm_command cmd :
                 {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
                req r;
                r.cmd = cmd; r.addr = cfg::OFF_DEST_ADDR; r.data = &scratch;
                r.be = be; r.be_len = be_len;
                std::ostringstream o;
                o << "byte-enable len " << be_len;
                EXPECT_EQ_CTX(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                              drv.send(r).status, o.str());
            }
        }
        req ok;
        ok.addr = cfg::OFF_DEST_ADDR; ok.data = &scratch;
        ok.be = nullptr; ok.be_len = 8;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(ok).status);
    }
    std::cout << "  [PASS] byte enables refused at every byte-enable length\n";

    // Streaming width: single beat, so TLM-2.0 requires sw >= len.
    for (unsigned sw = 0; sw <= 16; ++sw) {
        req r;
        r.addr = cfg::OFF_DEST_ADDR;
        r.len = 8;
        r.data = &scratch;
        r.streaming_width = static_cast<int>(sw);
        const tlm::tlm_response_status exp =
            (sw < 8) ? tlm::TLM_BURST_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE;
        std::ostringstream o;
        o << "streaming_width " << sw;
        EXPECT_EQ_CTX(exp, drv.send(r).status, o.str());
    }
    std::cout << "  [PASS] streaming-width relations (0, <, ==, >)\n";

    // A malformed access must not disturb register state or start a job.
    {
        drv.write64(cfg::OFF_DEST_ADDR, 0x1234);
        drv.write64(cfg::OFF_SIZE, 0);
        mem.clear_log();
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_WRITE_COMMAND, cfg::OFF_DEST_ADDR, 4));
        EXPECT_EQ(UINT64_C(0x1234), drv.read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
    }
    std::cout << "  [PASS] a rejected access mutates nothing\n";

    // The canonical sideband is accepted and left untouched, and its absence
    // is equally acceptable: authorization is the fabric filter's job.
    {
        smc::smc_axi_extension ext;
        ext.source_id = smc::JTAG_ID;
        ext.axi_id    = 0x4242u;
        ext.axi_user  = 0x11u;
        ext.set_priv(false);
        ext.set_secure(true);
        const uint8_t prot_before = ext.prot;

        uint64_t data = 0;
        req r;
        r.addr = cfg::OFF_SIZE; r.data = &data; r.ext = &ext;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(r).status);
        EXPECT_EQ(static_cast<uint16_t>(smc::JTAG_ID), ext.source_id);
        EXPECT_EQ(static_cast<uint16_t>(0x4242u), ext.axi_id);
        EXPECT_EQ(static_cast<unsigned>(prot_before),
                  static_cast<unsigned>(ext.prot));
        EXPECT_EQ(static_cast<unsigned>(0x11u),
                  static_cast<unsigned>(ext.axi_user));
    }
    std::cout << "  [PASS] incoming sideband preserved; absent sideband OK\n";
}

// ---------------------------------------------------------------------------
// Architectural: debug transfer and DMI policy
// ---------------------------------------------------------------------------
void tb::t_debug_and_dmi()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: transport_dbg and DMI policy ---\n";
    pulse_reset();

    // Raw back door: bypasses the write mask, so a value no software write
    // could produce is observable, while the software read stays masked.
    {
        uint64_t raw = ~UINT64_C(0);
        EXPECT_EQ(8u, drv.dbg(tlm::TLM_WRITE_COMMAND, cfg::OFF_CTRL_STATUS, 8,
                              &raw));
        EXPECT_EQ(~UINT64_C(0), drv.dbg_read64(cfg::OFF_CTRL_STATUS));
        EXPECT_EQ(cfg::CTRL_RMASK, drv.read64(cfg::OFF_CTRL_STATUS));
    }
    pulse_reset();

    // A debug write to CTRL_STATUS must not start a job: that is what
    // "side-effect-free" has to mean for this model.
    {
        drv.write64(cfg::OFF_DEST_ADDR, 0x100);
        drv.write64(cfg::OFF_SIZE, kChunk);
        mem.fill(0xA5);
        mem.clear_log();
        uint64_t ctrl = cfg::CTRL_INT_EN_MASK;
        EXPECT_EQ(8u, drv.dbg(tlm::TLM_WRITE_COMMAND, cfg::OFF_CTRL_STATUS, 8,
                              &ctrl));
        settle();
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
        EXPECT_TRUE(mem.region_is(0x100, 0x100 + kChunk, 0xA5));
        EXPECT_FALSE(irq.read());
        // Debug reads are side-effect-free too.
        EXPECT_EQ(UINT64_C(0x100), drv.dbg_read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
    }
    pulse_reset();

    // Refusals mirror the b_transport contract and return 0 bytes.
    {
        uint64_t scratch = 0;
        uint8_t  be      = 0xFF;
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, cfg::WINDOW_SIZE, 8, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, UINT64_MAX - 7, 8, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0, 4, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0x04, 8, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0, 8, nullptr));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0, 0, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0, 8, &scratch, -1, &be, 1));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0, 8, &scratch, 4));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_IGNORE_COMMAND, 0, 8, &scratch));
    }
    std::cout << "  [PASS] transport_dbg raw back door and its refusals\n";

    // DMI is denied: a CTRL_STATUS write has DMA side effects a direct
    // pointer would bypass.
    {
        for (uint64_t off : {cfg::OFF_DEST_ADDR, cfg::OFF_SIZE,
                             cfg::OFF_CTRL_STATUS}) {
            tlm::tlm_dmi d;
            d.allow_read_write();
            EXPECT_TRUE(!drv.dmi(off, d));
            EXPECT_TRUE(!d.is_read_allowed());
            EXPECT_TRUE(!d.is_write_allowed());
        }
        uint64_t scratch = 0;
        req r;
        r.addr = cfg::OFF_DEST_ADDR;
        r.data = &scratch;
        EXPECT_TRUE(!drv.send(r).dmi_allowed);
        // Cleared on the error path too, so a stale hint cannot survive.
        req bad;
        bad.addr = cfg::WINDOW_SIZE;
        bad.data = &scratch;
        const rsp s = drv.send(bad);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, s.status);
        EXPECT_TRUE(!s.dmi_allowed);
    }
    std::cout << "  [PASS] DMI denied on hit and miss\n";
}

// ---------------------------------------------------------------------------
// Architectural: annotated delay
// ---------------------------------------------------------------------------
void tb::t_timing()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: annotated delay and CCI ---\n";
    pulse_reset();

    auto broker = cci::cci_get_broker();
    auto h = broker.get_param_handle("tb.zeroer.access_delay_ns");
    EXPECT_TRUE(h.is_valid());

    const sc_time in{7, SC_NS};  // non-zero incoming delay

    auto reg_delay = [&](uint64_t off) {
        uint64_t scratch = 0;
        req r;
        r.addr = off;
        r.data = &scratch;
        r.delay_in = in;
        const rsp s = drv.send(r);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, s.status);
        return s.delay_delta;
    };

    // A plain register access costs exactly access_delay_ns.
    EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs, SC_NS),
                       reg_delay(cfg::OFF_DEST_ADDR), "read DEST_ADDR");
    EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs, SC_NS),
                       reg_delay(cfg::OFF_CTRL_STATUS), "read CTRL_STATUS");
    std::cout << "  [PASS] incoming delay preserved, one access delay added\n";

    // A trigger write costs the access delay plus every chunk's downstream
    // delay -- the job's cost is annotated, not discarded.
    {
        drv.write64(cfg::OFF_SIZE, 0);
        mem.access_delay = sc_time(3, SC_NS);
        const uint64_t size = 2 * kChunk + 1;  // 3 chunks
        const rsp s = start_job(0x100, size, cfg::CTRL_INT_EN_MASK, in);
        settle();
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, s.status);
        EXPECT_EQ(3u, static_cast<unsigned>(mem.log.size()));
        EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs + 3 * 3.0, SC_NS),
                           s.delay_delta, "trigger write with 3 chunks");
        // Each chunk saw the delay accumulated by its predecessors.
        for (std::size_t i = 0; i < mem.log.size(); ++i) {
            std::ostringstream o;
            o << "chunk " << i << " incoming delay";
            EXPECT_TIME_EQ_CTX(sc_time(3.0 * static_cast<double>(i), SC_NS),
                               mem.log[i].delay_in, o.str());
        }
    }
    // A write that starts no job carries no job delay.
    {
        drv.write64(cfg::OFF_SIZE, 0);
        uint64_t ctrl = cfg::CTRL_INT_EN_MASK;
        req r;
        r.cmd = tlm::TLM_WRITE_COMMAND;
        r.addr = cfg::OFF_CTRL_STATUS;
        r.data = &ctrl;
        r.delay_in = in;
        EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs, SC_NS), drv.send(r).delay_delta,
                           "trigger write with SIZE=0");
    }
    // A DMA chunk that loops back into this CSR window re-enters b_transport
    // while the outer job is still accumulating.  The nested access must not
    // discard what the outer job has accrued so far.
    {
        drv.write64(cfg::OFF_SIZE, 0);
        mem.access_delay = sc_time(3, SC_NS);
        unsigned nested = 0;
        mem.on_chunk = [&] {
            ++nested;
            drv.read64(cfg::OFF_SIZE);  // re-entrant MMIO during the job
        };
        const uint64_t size = 2 * kChunk + 1;  // 3 chunks
        const rsp s = start_job(0x100, size, cfg::CTRL_INT_EN_MASK, in);
        settle();
        mem.on_chunk = nullptr;

        EXPECT_EQ(tlm::TLM_OK_RESPONSE, s.status);
        EXPECT_EQ(3u, nested);
        EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs + 3 * 3.0, SC_NS),
                           s.delay_delta, "re-entrant MMIO during a job");
    }
    mem.access_delay = sc_time(1, SC_NS);
    std::cout << "  [PASS] job cost is annotated onto the triggering write, "
                 "even under re-entrant MMIO\n";

    // Live CCI mutation changes the annotated delay immediately.
    {
        h.set_cci_value(cci::cci_value(11.0));
        EXPECT_EQ(std::string("11.0"), h.get_cci_value().to_json());
        EXPECT_TIME_EQ_CTX(sc_time(11.0, SC_NS), reg_delay(cfg::OFF_DEST_ADDR),
                           "after CCI mutation");
        h.set_cci_value(cci::cci_value(kAccessDelayNs));
        EXPECT_TIME_EQ_CTX(sc_time(kAccessDelayNs, SC_NS),
                           reg_delay(cfg::OFF_DEST_ADDR), "after restore");
    }
    // Error paths annotate nothing, and transport_dbg is untimed.
    {
        uint64_t scratch = 0;
        req r;
        r.addr = cfg::WINDOW_SIZE;
        r.data = &scratch;
        r.delay_in = in;
        const rsp s = drv.send(r);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, s.status);
        EXPECT_TIME_EQ_CTX(SC_ZERO_TIME, s.delay_delta, "decode miss");
    }
    std::cout << "  [PASS] live CCI mutation; error paths annotate no delay\n";

    // chunk_size is immutable after construction.  The expected SC_ERROR is
    // demoted for exactly this statement, not for the whole run.
    {
        auto hc = broker.get_param_handle("tb.zeroer.chunk_size");
        EXPECT_TRUE(hc.is_valid());
        {
            const scoped_error_demotion demote;
            hc.set_cci_value(cci::cci_value(128u));
        }
        EXPECT_EQ(std::string("64"), hc.get_cci_value().to_json());
    }
    std::cout << "  [PASS] chunk_size is immutable (scoped expected error)\n";
}

// ---------------------------------------------------------------------------
// Architectural: interrupt and retrigger semantics
// ---------------------------------------------------------------------------
void tb::t_irq_and_retrigger()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Architectural: interrupt and retrigger ---\n";

    // int_en = 0: a successful job completes with irq low.
    pulse_reset();
    mem.fill(0xFF);
    start_job(0, 16, 0);
    settle();
    EXPECT_FALSE(irq.read());
    EXPECT_TRUE(mem.region_is(0, 16, 0x00));
    std::cout << "  [PASS] completion with int_en=0 leaves irq low\n";

    // Enabling int_en afterwards does not retroactively raise the interrupt;
    // it is raised by the job that the enabling write itself starts.
    {
        drv.write64(cfg::OFF_SIZE, 0);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_FALSE(irq.read());
    }
    std::cout << "  [PASS] enabling int_en alone does not assert irq\n";

    // A successful job with int_en set raises irq; clearing int_en drops it.
    pulse_reset();
    mem.fill(0xFF);
    start_job(0, 16, cfg::CTRL_INT_EN_MASK);
    settle();
    EXPECT_TRUE(irq.read());
    drv.write64(cfg::OFF_SIZE, 0);            // do not start another job
    drv.write64(cfg::OFF_CTRL_STATUS, 0);     // clear int_en
    settle();
    EXPECT_FALSE(irq.read());
    std::cout << "  [PASS] completion asserts irq; clearing int_en drops it\n";

    // Retrigger semantics: a CTRL_STATUS write restarts the job whenever SIZE
    // is non-zero, including a write whose only intent is to change int_en and
    // a write of the same value.  This is level, not edge, behaviour.
    //
    // NOTE (open spec question, audit finding 9): the RDL/reference should
    // confirm whether an int_en-only write is meant to re-run the job.  The
    // bench pins today's behaviour so a change is deliberate.
    {
        pulse_reset();
        mem.fill(0xA5);
        start_job(0x300, kChunk, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(1u, static_cast<unsigned>(mem.log.size()));

        // Same value again -> another full job.
        mem.fill(0xA5);
        mem.clear_log();
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(1u, static_cast<unsigned>(mem.log.size()));
        EXPECT_TRUE(mem.region_is(0x300, 0x300 + kChunk, 0x00));

        // int_en-only change (set -> clear) also retriggers, and because
        // int_en is now 0 the completing job raises no interrupt.
        mem.fill(0xA5);
        mem.clear_log();
        drv.write64(cfg::OFF_CTRL_STATUS, 0);
        settle();
        EXPECT_EQ(1u, static_cast<unsigned>(mem.log.size()));
        EXPECT_TRUE(mem.region_is(0x300, 0x300 + kChunk, 0x00));
        EXPECT_FALSE(irq.read());

        // Clearing SIZE is the only way to stop the retriggering.
        drv.write64(cfg::OFF_SIZE, 0);
        mem.clear_log();
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
    }
    std::cout << "  [PASS] every CTRL write retriggers while SIZE != 0\n";

    // Writing DEST/SIZE does not itself start a job.
    {
        pulse_reset();
        mem.clear_log();
        drv.write64(cfg::OFF_DEST_ADDR, 0x40);
        drv.write64(cfg::OFF_SIZE, kChunk);
        settle();
        EXPECT_EQ(0u, static_cast<unsigned>(mem.log.size()));
        EXPECT_FALSE(irq.read());
    }
    std::cout << "  [PASS] DEST/SIZE writes alone start no job\n";
}

// ---------------------------------------------------------------------------
// Structural: model debug surface.  Not architectural evidence.
// ---------------------------------------------------------------------------
void tb::t_structural()
{
    using cfg = smc::memory_zeroer_cfg;
    std::cout << "\n--- Structural: debug surface (not architectural) ---\n";
    pulse_reset();

    drv.write64(cfg::OFF_DEST_ADDR, 0x1230);
    drv.write64(cfg::OFF_SIZE, 0x40);
    drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
    settle();

    // Debug getters agree with what the bus reports.
    EXPECT_EQ(drv.read64(cfg::OFF_DEST_ADDR), zeroer.dbg_dest_addr());
    EXPECT_EQ(drv.read64(cfg::OFF_SIZE), zeroer.dbg_size());
    EXPECT_EQ(drv.read64(cfg::OFF_CTRL_STATUS), zeroer.dbg_ctrl_status());
    EXPECT_TRUE(zeroer.dbg_int_en());
    // Busy is never observable from outside the job (see memory_zeroer.h).
    EXPECT_FALSE(zeroer.dbg_busy());

    std::ostringstream oss;
    zeroer.dump_state(oss);
    const std::string s = oss.str();
    EXPECT_TRUE(s.find("memory_zeroer state") != std::string::npos);
    EXPECT_TRUE(s.find("DEST_ADDR") != std::string::npos);
    EXPECT_TRUE(s.find("CTRL_STATUS") != std::string::npos);
    std::cout << "  [PASS] debug getters and dump_state (structural)\n";
}

void tb::run()
{
    std::cout << "==== SMC memory_zeroer TB ====\n";

    t_registers_and_reset();
    t_chunking();
    t_sideband_and_payload();
    t_faults();
    t_mmio_contract();
    t_debug_and_dmi();
    t_timing();
    t_irq_and_retrigger();
    t_structural();

#ifdef MZ_UB_CANARY
    // Built only by `run_tests.sh --ubsan-canary`.  Proves the UBSan build
    // really does report undefined behaviour, so a clean --asan run means
    // something.  volatile keeps the shift out of the optimiser's hands.
    {
        volatile int shift = 33;
        volatile int value = 1;
        std::cout << "  [UB CANARY] " << (value << shift) << "\n";
    }
#endif

    if (g_failures == 0)
        std::cout << "\nALL TESTS PASSED\n";
    else
        std::cout << "\n" << g_failures << " FAILURE(S)\n";

    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char**)
{
    // Constructor guard rails use SC_REPORT_FATAL; throw so we can catch them
    // before elaborating the real DUT (same pattern as the sister-IP neg TBs).
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

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    global_broker.set_preset_cci_value("tb.zeroer.chunk_size",
                                       cci::cci_value(kChunk));
    global_broker.set_preset_cci_value("tb.zeroer.access_delay_ns",
                                       cci::cci_value(kAccessDelayNs));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
