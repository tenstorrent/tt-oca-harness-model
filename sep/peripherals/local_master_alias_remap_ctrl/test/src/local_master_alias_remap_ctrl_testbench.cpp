// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file local_master_alias_remap_ctrl_testbench.cpp
 * @brief SystemC/TLM testbench for local_master_alias_remap_ctrl.
 *
 * Wiring:
 *   test_harness.initiator_socket ──► dut.target_socket     (CSR path)
 *   data_initiator                ──► dut.data_socket       (data path in)
 *   dut.remapped_socket           ──► stub.socket           (data path out)
 */

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../../include/local_alias_remap.h"
#include "local_alias_remap_test.h"
#include "sep_axi_extension.h"

#include <climits>
#include <cstring>
#include <iostream>
#include <string>

// =============================================================================
// Independent translation oracle (documented rule — not a copy of DUT code):
//   offset added only to bits [55:12]; low 12 bits preserved; 44-bit truncation.
// =============================================================================
static uint64_t expect_translated(uint64_t addr, uint64_t offset)
{
    constexpr uint64_t kLo12   = 0xFFFULL;
    constexpr uint64_t kUpper  = 0x00FFFFFFFFFFF000ULL; // bits [55:12]
    return (((addr & kUpper) + (offset & kUpper)) & kUpper) | (addr & kLo12);
}

// =============================================================================
// StubTarget — records downstream payload fields and accumulates delay.
// =============================================================================
struct StubTarget : sc_core::sc_module
{
    tlm_utils::simple_target_socket<StubTarget> socket;

    uint64_t              last_addr            = 0;
    tlm::tlm_command      last_cmd             = tlm::TLM_IGNORE_COMMAND;
    unsigned char*        last_data_ptr        = nullptr;
    unsigned int          last_len             = 0;
    unsigned int          last_streaming_width = 0;
    unsigned char*        last_be_ptr          = nullptr;
    unsigned int          last_be_len          = 0;
    sc_core::sc_time      last_delay_in        = sc_core::SC_ZERO_TIME;
    bool                  last_had_ext         = false;
    uint8_t               last_cache           = 0xFF; ///< AxCACHE[3:0] seen downstream; 0xFF = no extension
    uint8_t               last_source_id       = 0;
    uint8_t               last_group_id        = 0;
    bool                  last_is_ns           = false;

    /// Added to the incoming annotated delay on every b_transport.
    sc_core::sc_time add_delay{17, sc_core::SC_NS};
    tlm::tlm_response_status forced_status = tlm::TLM_OK_RESPONSE;
    unsigned int             dbg_return_len_override = UINT_MAX; // UINT_MAX = use data_length

    SC_CTOR(StubTarget) : socket("socket")
    {
        socket.register_b_transport(this,   &StubTarget::b_transport);
        socket.register_transport_dbg(this, &StubTarget::transport_dbg);
    }

    void capture(tlm::tlm_generic_payload& trans, sc_core::sc_time* delay_in)
    {
        last_addr            = trans.get_address();
        last_cmd             = trans.get_command();
        last_data_ptr        = trans.get_data_ptr();
        last_len             = trans.get_data_length();
        last_streaming_width = trans.get_streaming_width();
        last_be_ptr          = trans.get_byte_enable_ptr();
        last_be_len          = trans.get_byte_enable_length();
        last_delay_in        = delay_in ? *delay_in : sc_core::SC_ZERO_TIME;

        sep::sep_axi_extension* ext = trans.get_extension<sep::sep_axi_extension>();
        last_had_ext   = (ext != nullptr);
        last_cache     = ext ? ext->axi_cache : 0xFF;
        last_source_id = ext ? ext->source_id : 0;
        last_group_id  = ext ? ext->group_id  : 0;
        last_is_ns     = ext ? ext->is_ns     : false;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        capture(trans, &delay);
        delay += add_delay;
        trans.set_response_status(forced_status);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        capture(trans, nullptr);
        trans.set_response_status(forced_status);
        if (dbg_return_len_override != UINT_MAX)
            return dbg_return_len_override;
        return (forced_status == tlm::TLM_OK_RESPONSE) ? trans.get_data_length() : 0u;
    }
};

// =============================================================================
// Testbench
// =============================================================================
SC_MODULE(local_master_alias_remap_ctrl_testbench)
{
    SC_HAS_PROCESS(local_master_alias_remap_ctrl_testbench);

    static constexpr uint32_t NUM_REGIONS = local_alias_remap_ip::NUM_REGIONS;
    static constexpr uint32_t IDX_START   = local_alias_remap_ip::IDX_START;

    static constexpr uint64_t ADDR_MASK  = 0x00FFFFFFFFFFF000ULL; // [55:12]
    // offset[55:12] | cacheable[59:56] | valid[63]. Was 0xC0FF... when cacheable
    // was a single bit 62; tt-oca-hw #2464 widened it to the 4-bit AxCACHE field
    // at [59:56] and made [62:60] reserved.
    static constexpr uint64_t ATTRS_MASK = 0x8FFFFFFFFFFFF000ULL;

    sc_core::sc_signal<bool> rst_n_sig;
    local_alias_remap_ip     dut;
    local_alias_remap_test   test_harness;
    StubTarget               stub;

    tlm_utils::simple_initiator_socket<local_master_alias_remap_ctrl_testbench> data_initiator;

    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;

    local_master_alias_remap_ctrl_testbench(sc_module_name name)
        : sc_module(name)
        , dut("dut")
        , test_harness("test_harness")
        , stub("stub")
        , data_initiator("data_initiator")
    {
        rst_n_sig.write(true);
        dut.rst_ni(rst_n_sig);

        test_harness.initiator_socket.bind(dut.target_socket);
        data_initiator.bind(dut.data_socket);
        dut.remapped_socket.bind(stub.socket);

        SC_THREAD(run_tests);
    }

    // -------------------------------------------------------------------------
    // CSR helpers
    // -------------------------------------------------------------------------
    static unsigned csr_off(uint32_t region, unsigned reg_word)
    {
        // 4-word stride per region; reg_word 0=START, 1=END, 2=ATTRS
        return static_cast<unsigned>((region * 4u + reg_word) * 8u);
    }

    bool csr_write64(unsigned offset, uint64_t value)
    {
        return test_harness.register_write_64(offset, value) == tlm::TLM_OK_RESPONSE;
    }

    bool csr_read64(unsigned offset, uint64_t& value)
    {
        return test_harness.register_read_64(offset, value) == tlm::TLM_OK_RESPONSE;
    }

    bool csr_read64_ok(unsigned offset, uint64_t expected)
    {
        uint64_t got = 0xDEADBEEFDEADBEEFULL;
        if (!csr_read64(offset, got))
            return false;
        return got == expected;
    }

    // -------------------------------------------------------------------------
    // REGION_ATTRS layout (alias_remap.rdl after tt-oca-hw #2464):
    //   [11:0] RAZ/WI | offset[55:12] | cacheable[59:56] | [62:60] RAZ/WI | valid[63]
    //
    // Spelled out locally rather than reusing the DUT's constants so the
    // expected encoding is an independent statement of the RDL, not a copy of
    // the code under test. Bit 62 was the pre-#2464 single cacheable bit and is
    // now reserved — T10 pins that it is dropped on write.
    // -------------------------------------------------------------------------
    static constexpr uint64_t ATTRS_CACHEABLE_SHIFT = 56;
    static constexpr uint64_t ATTRS_VALID           = 1ULL << 63;

    static uint64_t attrs_word(uint64_t offset, uint8_t cacheable, bool valid = true)
    {
        return (offset & 0x00FFFFFFFFFFF000ULL)
             | (static_cast<uint64_t>(cacheable & 0xFu) << ATTRS_CACHEABLE_SHIFT)
             | (valid ? ATTRS_VALID : 0ULL);
    }

    void program_region(uint32_t r, uint64_t start, uint64_t end,
                        uint64_t offset, uint8_t cacheable, bool valid = true)
    {
        csr_write64(csr_off(r, 0), start);
        csr_write64(csr_off(r, 1), end);
        csr_write64(csr_off(r, 2), attrs_word(offset, cacheable, valid));
    }

    // -------------------------------------------------------------------------
    // Data-path access carrying a sep_axi_extension, so the tests can drive an
    // incoming AxCACHE and see what the region override does to it. Returns the
    // extension's value after the call, which is what the initiator would go on
    // to observe.
    //
    // The extension is stack-allocated and paired with clear_extension() rather
    // than set_auto_extension(), which would schedule a release of memory the
    // payload does not own.
    // -------------------------------------------------------------------------
    uint8_t data_access_ext(uint64_t addr, uint8_t cache_in, bool use_dbg)
    {
        tlm::tlm_generic_payload trans;
        sep::sep_axi_extension   ext;
        sc_core::sc_time         delay = sc_core::SC_ZERO_TIME;
        uint64_t                 buf   = 0;

        ext.axi_cache = cache_in;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        trans.set_extension(&ext);

        if (use_dbg)
            data_initiator->transport_dbg(trans);
        else
            data_initiator->b_transport(trans, delay);

        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);

        const uint8_t cache_after = ext.axi_cache;
        trans.clear_extension(&ext);

        return cache_after;
    }

    void clear_all_via_reset()
    {
        rst_n_sig.write(false);
        wait(10, sc_core::SC_NS);
        rst_n_sig.write(true);
        wait(10, sc_core::SC_NS);
        stub.forced_status = tlm::TLM_OK_RESPONSE;
        stub.add_delay = sc_core::sc_time(17, sc_core::SC_NS);
    }

    // -------------------------------------------------------------------------
    // Data-path helpers
    //
    // data_xfer() is the general form used by the protocol / delay / error
    // cases: it exposes length, streaming width, byte enables, an incoming
    // annotated delay, and an optional caller-owned extension. data_access_ext()
    // above is the narrow AxCACHE-only form used by T12-T17b.
    // -------------------------------------------------------------------------
    struct DataResult {
        tlm::tlm_response_status status = tlm::TLM_INCOMPLETE_RESPONSE;
        uint64_t                 addr_after = 0;
        sc_core::sc_time         delay_after = sc_core::SC_ZERO_TIME;
        uint8_t                  cache_after = 0xFF; ///< 0xFF when no extension was supplied
        unsigned int             dbg_bytes = 0;
    };

    DataResult data_xfer(tlm::tlm_command cmd, uint64_t addr, uint64_t& buf,
                         bool use_dbg, sep::sep_axi_extension* ext,
                         sc_core::sc_time delay_in = sc_core::SC_ZERO_TIME,
                         unsigned int len = 8, unsigned int sw = 8,
                         unsigned char* be = nullptr, unsigned int be_len = 0)
    {
        DataResult r;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = delay_in;

        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(len);
        trans.set_streaming_width(sw);
        if (be) {
            trans.set_byte_enable_ptr(be);
            trans.set_byte_enable_length(be_len);
        } else {
            trans.set_byte_enable_ptr(nullptr);
        }
        if (ext)
            trans.set_extension(ext);

        if (use_dbg)
            r.dbg_bytes = data_initiator->transport_dbg(trans);
        else
            data_initiator->b_transport(trans, delay);

        r.status = trans.get_response_status();
        r.addr_after = trans.get_address();
        r.delay_after = delay;
        if (ext)
            r.cache_after = ext->axi_cache;

        if (ext)
            trans.clear_extension(ext);
        return r;
    }

    // -------------------------------------------------------------------------
    // Test helper — report test result
    // -------------------------------------------------------------------------
    void report(const std::string& test_name, bool passed)
    {
        m_tests_run++;
        if (passed) {
            m_tests_passed++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            m_tests_failed++;
            std::cout << "[FAIL] " << test_name << std::endl;
        }
    }

    // =========================================================================
    void run_tests()
    {
        std::cout << "\n=== local_master_alias_remap_ctrl tests ===\n"
                  << std::dec
                  << "  NumRegions  = " << NUM_REGIONS << "\n"
                  << "  IdxStart    = " << IDX_START  << "\n\n";

        // ==================================================================
        // T1: Socket CSR reset readback (fresh DUT — all zeros + OK)
        // ==================================================================
        {
            bool pass = true;
            for (uint32_t i = 0; i < NUM_REGIONS; ++i) {
                // Frontdoor: all three words of every region read back 0, which
                // also covers cacheable[59:56] and valid[63] being clear.
                for (unsigned w = 0; w < 3; ++w) {
                    if (!csr_read64_ok(csr_off(i, w), 0)) {
                        pass = false;
                        break;
                    }
                }
                if (!pass) break;
            }
            report("T1: CSR socket reset readback all zeros", pass);
        }

        // ==================================================================
        // T2: Program region 0 via socket; socket readback (not get_region)
        // ==================================================================
        {
            const uint64_t start = 0x10000;
            const uint64_t end   = 0x11000;
            const uint64_t off   = 0x2000000;
            bool pass = csr_write64(csr_off(0, 0), start)
                     && csr_write64(csr_off(0, 1), end)
                     && csr_write64(csr_off(0, 2), (off & ADDR_MASK) | (1ULL << 63));
            uint64_t rs = 0, re = 0, ra = 0;
            pass = pass && csr_read64(csr_off(0, 0), rs) && (rs == (start & ADDR_MASK));
            pass = pass && csr_read64(csr_off(0, 1), re) && (re == (end & ADDR_MASK));
            pass = pass && csr_read64(csr_off(0, 2), ra)
                && (ra == ((off & ADDR_MASK) | (1ULL << 63)));
            // Secondary backdoor check (socket is the primary oracle above).
            auto region = dut.get_region(0);
            pass = pass && (region.start_addr == start) && (region.end_addr == end)
                       && (region.offset == off) && (region.cacheable == 0)
                       && region.valid;
            report("T2: Region 0 program and socket readback", pass);
        }

        // ==================================================================
        // T3: Hit remaps to literal expected address
        // ==================================================================
        {
            const uint64_t in  = 0x10100;
            const uint64_t exp = expect_translated(in, 0x2000000); // 0x2010100
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
            report("T3: Address remap region 0 hit",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   stub.last_addr == exp && r.addr_after == in);
        }

        // ==================================================================
        // T4: Miss is passthrough
        // ==================================================================
        {
            const uint64_t in = 0x20000;
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
            report("T4: Passthrough - no region match",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   stub.last_addr == in && r.addr_after == in);
        }

        // ==================================================================
        // T5: First-match priority (region 0 over overlapping region 1)
        // ==================================================================
        {
            program_region(1, 0x10800, 0x10900, 0x3000000, /*cacheable=*/0x0);
            const uint64_t in  = 0x10850;
            const uint64_t exp = expect_translated(in, 0x2000000); // region 0 wins
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
            report("T5: First-match priority (region 0 wins)",
                   r.status == tlm::TLM_OK_RESPONSE && stub.last_addr == exp);
        }

        // ==================================================================
        // T6: Lower 12 bits preserved
        // ==================================================================
        {
            const uint64_t in  = 0x10ABC;
            const uint64_t exp = expect_translated(in, 0x2000000);
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
            report("T6: Lower 12 bits preserved",
                   r.status == tlm::TLM_OK_RESPONSE && stub.last_addr == exp &&
                   (stub.last_addr & 0xFFFULL) == 0xABCULL);
        }

        // ==================================================================
        // T7: Write transaction remap
        // ==================================================================
        {
            const uint64_t in  = 0x10200;
            const uint64_t exp = expect_translated(in, 0x2000000);
            uint64_t buf = 0xDEADBEEF;
            auto r = data_xfer(tlm::TLM_WRITE_COMMAND, in, buf, false, nullptr);
            report("T7: Write transaction remap",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   stub.last_addr == exp &&
                   stub.last_cmd == tlm::TLM_WRITE_COMMAND);
        }

        // ==================================================================
        // T8: Address restoration after forward
        // ==================================================================
        {
            const uint64_t in = 0x10300;
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
            report("T8: Address restoration after forward",
                   r.status == tlm::TLM_OK_RESPONSE && r.addr_after == in);
        }

        // ==================================================================
        // T9: transport_dbg hit — byte count + translated address
        // ==================================================================
        {
            const uint64_t in  = 0x10400;
            const uint64_t exp = expect_translated(in, 0x2000000);
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, true, nullptr);
            report("T9: transport_dbg hit remap + byte count",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   stub.last_addr == exp && r.dbg_bytes == 8 &&
                   r.addr_after == in);
        }

        // ==================================================================
        // T10: Field masks — all-ones writes; reserved bits discarded.
        //      Sample first (0), middle (7), last (15).
        //
        //      Post tt-oca-hw #2464 the attrs word keeps offset[55:12],
        //      cacheable[59:56] and valid[63]; [62:60] and [11:0] are RAZ/WI,
        //      so an all-ones write must read back exactly ATTRS_MASK and
        //      cacheable must read 0xF (not a replicated single bit).
        // ==================================================================
        {
            bool pass = true;
            const uint32_t samples[] = {0u, 7u, 15u};
            for (uint32_t r : samples) {
                pass = pass && csr_write64(csr_off(r, 0), ~0ULL);
                pass = pass && csr_write64(csr_off(r, 1), ~0ULL);
                pass = pass && csr_write64(csr_off(r, 2), ~0ULL);
                uint64_t rs = 0, re = 0, ra = 0;
                pass = pass && csr_read64(csr_off(r, 0), rs) && (rs == ADDR_MASK);
                pass = pass && csr_read64(csr_off(r, 1), re) && (re == ADDR_MASK);
                pass = pass && csr_read64(csr_off(r, 2), ra) && (ra == ATTRS_MASK);
                // Explicitly: bits [11:0] and [63:56] are 0 on address regs.
                pass = pass && ((rs & 0xFFFULL) == 0) && ((rs >> 56) == 0);
                pass = pass && ((re & 0xFFFULL) == 0) && ((re >> 56) == 0);
                pass = pass && ((ra & 0xFFFULL) == 0);
                pass = pass && ((ra & ~ATTRS_MASK) == 0);
                // valid set, [62:60] dropped, cacheable all four bits set.
                pass = pass && ((ra >> 63) == 0x1ULL);
                pass = pass && (((ra >> 60) & 0x7ULL) == 0ULL);
                pass = pass && (((ra >> ATTRS_CACHEABLE_SHIFT) & 0xFULL) == 0xFULL);
                auto rr = dut.get_region(r);
                pass = pass && (rr.cacheable == 0xF) && rr.valid;
            }
            report("T10: Field masks discard reserved bits (entries 0/7/15)", pass);

            // ----------------------------------------------------------------
            // T10b: bit 62 was the pre-#2464 single cacheable bit. It is now
            //       reserved, so a write that still sets it must drop it while
            //       the real 4-bit field (0xA = 0b1010, not all-ones or
            //       all-zeros) round-trips intact.
            // ----------------------------------------------------------------
            const uint64_t attrs_in = (0x1000000ULL & ADDR_MASK)
                                    | (0xAULL << ATTRS_CACHEABLE_SHIFT)
                                    | (1ULL << 62)   // reserved — must vanish
                                    | ATTRS_VALID;
            bool pass_b = csr_write64(csr_off(2, 0), 0x30000)
                       && csr_write64(csr_off(2, 1), 0x31000)
                       && csr_write64(csr_off(2, 2), attrs_in);

            uint64_t raw = 0;
            pass_b = pass_b && csr_read64(csr_off(2, 2), raw)
                            && (raw == (attrs_in & ~(1ULL << 62)));

            auto r2 = dut.get_region(2);
            pass_b = pass_b && (r2.start_addr == 0x30000) && (r2.end_addr == 0x31000)
                            && (r2.offset == 0x1000000) && (r2.cacheable == 0xA)
                            && r2.valid;
            report("T10b: cacheable[59:56] round-trips as 4 bits; reserved bit 62 dropped",
                   pass_b);

            // Leave the sampled entries invalid so later tests start clean.
            for (uint32_t r : samples)
                csr_write64(csr_off(r, 2), 0);
            csr_write64(csr_off(2, 2), 0);
        }

        // ==================================================================
        // T11: Real rst_ni — CSR socket zeros + former-hit becomes passthrough
        // ==================================================================
        {
            // Leave a programmed hit address from T2/T10 state, then reset.
            // Reprogram entry 0 to a known hit first so post-reset traffic is meaningful.
            program_region(0, 0x10000, 0x11000, 0x2000000, /*cacheable=*/0x0);
            uint64_t buf = 0;
            data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, nullptr);
            const bool was_hit = (stub.last_addr == expect_translated(0x10100, 0x2000000));

            clear_all_via_reset();

            bool pass = was_hit;
            for (uint32_t i = 0; i < NUM_REGIONS; ++i) {
                // Frontdoor readback is the oracle; the decoded region is
                // checked too so a stale cacheable nibble cannot hide behind
                // the CSR read mask.
                for (unsigned w = 0; w < 3; ++w) {
                    if (!csr_read64_ok(csr_off(i, w), 0)) {
                        pass = false;
                        break;
                    }
                }
                auto region = dut.get_region(i);
                if (region.start_addr != 0 || region.end_addr != 0 ||
                    region.offset != 0 || region.cacheable != 0 || region.valid) {
                    pass = false;
                }
                if (!pass) break;
            }
            // Former hit address must now passthrough unchanged.
            data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, nullptr);
            pass = pass && (stub.last_addr == 0x10100);
            report("T11: rst_ni clears CSRs; former hit is passthrough", pass);
        }

        // ==================================================================
        // T12-T17: cacheable override on the data path
        //
        // axi_alias_remap.sv (tt-oca-hw #2464) drives aw.cache/ar.cache from
        // the matched region's 4-bit cacheable[59:56] field — a bit-for-bit
        // replacement of AxCACHE — and passes the incoming value through on a
        // miss. T10 only proves the field survives a CSR round-trip; these
        // check that the exact nibble reaches the fabric.
        //
        // Patterns are chosen so a replicated-bit or OR/AND implementation
        // fails: 0x6 = 0b0110 and 0x9 = 0b1001 are bitwise complements within
        // the nibble, and neither is 0x0 or 0xF.
        //
        // T11 left every region cleared, so reprogram from scratch here:
        //   region 0: [0x10000, 0x11000) +0x2000000, cacheable = 0x6
        //   region 1: [0x40000, 0x41000) +0x5000000, cacheable = 0x0
        //   region 2: [0x50000, 0x51000) +0x7000000, cacheable = 0x9
        // ==================================================================
        program_region(0, 0x10000, 0x11000, 0x2000000, /*cacheable=*/0x6);
        program_region(1, 0x40000, 0x41000, 0x5000000, /*cacheable=*/0x0);
        program_region(2, 0x50000, 0x51000, 0x7000000, /*cacheable=*/0x9);

        // T12: hit on region 0 replaces an incoming AxCACHE=0 with exactly 0x6
        //      (not 0xF — the old replicated-bit behaviour).
        {
            data_access_ext(0x10100, /*cache_in=*/0x0, /*use_dbg=*/false);
            report("T12: region cacheable=0x6 drives outgoing AxCACHE to exactly 0x6",
                   stub.last_had_ext && stub.last_cache == 0x6 &&
                   stub.last_addr == expect_translated(0x10100, 0x2000000));
        }

        // T13: hit on a cacheable=0 region clears every incoming bit — an
        //      override, not an OR; and region 2 flips every bit of 0x6 to 0x9
        //      — an override, not an AND.
        {
            data_access_ext(0x40100, /*cache_in=*/0xF, /*use_dbg=*/false);
            const bool clear_ok = stub.last_had_ext && stub.last_cache == 0x0 &&
                                  stub.last_addr == expect_translated(0x40100, 0x5000000);

            data_access_ext(0x50100, /*cache_in=*/0x6, /*use_dbg=*/false);
            const bool flip_ok = stub.last_had_ext && stub.last_cache == 0x9 &&
                                 stub.last_addr == expect_translated(0x50100, 0x7000000);

            report("T13: region field replaces AxCACHE bit-for-bit (0xF->0x0, 0x6->0x9)",
                   clear_ok && flip_ok);
        }

        // T14: on a miss the incoming attribute passes through untouched, for
        //      several encodings, alongside the unremapped address.
        {
            bool pass = true;
            for (uint8_t c : {uint8_t{0xF}, uint8_t{0x0}, uint8_t{0x5}, uint8_t{0xA}}) {
                data_access_ext(0x90000, c, /*use_dbg=*/false);
                pass = pass && stub.last_cache == c && stub.last_addr == 0x90000;
            }
            report("T14: passthrough on miss leaves AxCACHE unchanged (0xF/0x0/0x5/0xA)",
                   pass);
        }

        // T15: the override is restored after the forward, for the same reason
        //      T8 checks the address — in RTL these are downstream wires, not a
        //      mutation the initiator can observe.
        {
            const uint8_t after_hit  = data_access_ext(0x10100, 0x3, false);
            const uint8_t after_miss = data_access_ext(0x90000, 0xC, false);
            report("T15: initiator's AxCACHE restored after forward",
                   after_hit == 0x3 && after_miss == 0xC);
        }

        // T16: transport_dbg applies the same override as b_transport, so a GDB
        //      access cannot see a different attribute than the functional path.
        {
            data_access_ext(0x10100, /*cache_in=*/0x0, /*use_dbg=*/true);
            const bool hit_ok = stub.last_had_ext && stub.last_cache == 0x6 &&
                                stub.last_addr == expect_translated(0x10100, 0x2000000);

            data_access_ext(0x40100, /*cache_in=*/0xF, /*use_dbg=*/true);
            const bool clear_ok = stub.last_cache == 0x0 &&
                                  stub.last_addr == expect_translated(0x40100, 0x5000000);

            data_access_ext(0x50100, /*cache_in=*/0x6, /*use_dbg=*/true);
            const bool flip_ok = stub.last_cache == 0x9 &&
                                 stub.last_addr == expect_translated(0x50100, 0x7000000);

            report("T16: transport_dbg applies the same 4-bit cacheable override",
                   hit_ok && clear_ok && flip_ok);
        }

        // T17: transport_dbg passthrough on a miss — the debug path's own
        //      no-hit branch, distinct from T4/T14's functional-path miss.
        {
            const uint8_t after = data_access_ext(0x90000, /*cache_in=*/0xB,
                                                  /*use_dbg=*/true);
            report("T17: transport_dbg passthrough on miss",
                   stub.last_addr == 0x90000 && stub.last_cache == 0xB && after == 0xB);
        }

        // T17b: an un-extended payload on a hit is remapped but carries no
        //       AxCACHE to override; the stub must see no extension.
        {
            uint64_t dummy = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, 0x10100, dummy, false, nullptr);
            report("T17b: un-extended payload remapped with no cache attribute to override",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   !stub.last_had_ext && stub.last_cache == 0xFF &&
                   stub.last_addr == expect_translated(0x10100, 0x2000000));
        }

        // ==================================================================
        // T18: get_region out-of-range backdoor
        // ==================================================================
        {
            auto region = dut.get_region(NUM_REGIONS);
            report("T18: get_region rejects an out-of-range index",
                   region.start_addr == 0 && region.end_addr == 0 &&
                   region.offset == 0 && region.cacheable == 0 && !region.valid);
        }

        // ==================================================================
        // T19: Partial 32-bit high/low writes — untouched half remains
        // ==================================================================
        {
            clear_all_via_reset();
            // Seed a known pattern via full 64-bit write.
            // valid | reserved bit 62 (dropped on write) | offset page
            const uint64_t seed = 0xC000000012340000ULL;
            bool pass = csr_write64(csr_off(0, 2), seed);
            // Write only the low 32 bits via length-4 at offset+0.
            uint32_t low = 0x56780000u; // new offset low half within [55:12]
            auto st = test_harness.csr_transport(
                tlm::TLM_WRITE_COMMAND, csr_off(0, 2),
                reinterpret_cast<unsigned char*>(&low), 4, 4);
            pass = pass && (st == tlm::TLM_OK_RESPONSE);
            uint64_t got = 0;
            pass = pass && csr_read64(csr_off(0, 2), got);
            // High 32 bits should still hold seed's high half (masked).
            const uint64_t expect_high = seed & 0xFFFFFFFF00000000ULL;
            const uint64_t expect_low  = (static_cast<uint64_t>(low) & ATTRS_MASK);
            // Merged: low 32 written, high 32 from seed, then ATTRS mask on read.
            const uint64_t expect =
                ((expect_high | (static_cast<uint64_t>(low))) & ATTRS_MASK);
            pass = pass && (got == expect) &&
                   ((got & 0xFFFFFFFF00000000ULL) == (expect_high & ATTRS_MASK));
            (void)expect_low;

            // Write only high 32 bits at byte offset +4. Bits [31:24] of this
            // word are attrs[63:56], so 0x8F sets valid and cacheable = 0xF
            // while leaving reserved [62:60] clear.
            uint32_t high = 0x8F000000u;
            st = test_harness.csr_transport(
                tlm::TLM_WRITE_COMMAND, csr_off(0, 2) + 4,
                reinterpret_cast<unsigned char*>(&high), 4, 4);
            pass = pass && (st == tlm::TLM_OK_RESPONSE);
            uint64_t after_high = 0;
            pass = pass && csr_read64(csr_off(0, 2), after_high);
            // Low half from previous write must remain.
            pass = pass && ((after_high & 0xFFFFFFFFULL) == (got & 0xFFFFFFFFULL));
            pass = pass && ((after_high >> 63) == 0x1ULL);
            pass = pass && (((after_high >> ATTRS_CACHEABLE_SHIFT) & 0xFULL) == 0xFULL);
            pass = pass && (((after_high >> 60) & 0x7ULL) == 0ULL);

            // Byte-enable write: only byte 2 of REGION_START[0].
            pass = pass && csr_write64(csr_off(0, 0), 0x00FFFFFFFFFFF000ULL);
            uint8_t be_data[8] = {0, 0, 0xAB, 0, 0, 0, 0, 0};
            unsigned char be[8] = {0, 0, 0xFF, 0, 0, 0, 0, 0};
            st = test_harness.csr_transport(
                tlm::TLM_WRITE_COMMAND, csr_off(0, 0), be_data, 8, 8, be, 8);
            pass = pass && (st == tlm::TLM_OK_RESPONSE);
            uint64_t start_rd = 0;
            pass = pass && csr_read64(csr_off(0, 0), start_rd);
            // Only the enabled byte lane may change; result still masked to ADDR_MASK.
            pass = pass && ((start_rd & ~ADDR_MASK) == 0);

            report("T19: Partial 32-bit / byte-enable CSR writes", pass);
        }

        // ==================================================================
        // T20: CSR malformed payloads — exact Memory responses; entry unchanged
        // ==================================================================
        {
            clear_all_via_reset();
            program_region(3, 0xA0000, 0xB0000, 0x100000, /*cacheable=*/0xF);
            uint64_t pre_s = 0, pre_e = 0, pre_a = 0;
            bool pass = csr_read64(csr_off(3, 0), pre_s)
                     && csr_read64(csr_off(3, 1), pre_e)
                     && csr_read64(csr_off(3, 2), pre_a);

            auto expect_status = [&](const char* label, tlm::tlm_response_status want,
                                     tlm::tlm_command cmd, unsigned off,
                                     unsigned char* data, unsigned len, unsigned sw,
                                     unsigned char* be = nullptr, unsigned be_len = 0) {
                auto st = test_harness.csr_transport(cmd, off, data, len, sw, be, be_len);
                if (st != want) {
                    std::cout << "    malformed mismatch (" << label
                              << "): got " << st << " want " << want << "\n";
                    return false;
                }
                return true;
            };

            uint64_t scratch = 0;
            unsigned char* p = reinterpret_cast<unsigned char*>(&scratch);

            pass = pass && expect_status("IGNORE", tlm::TLM_COMMAND_ERROR_RESPONSE,
                                         tlm::TLM_IGNORE_COMMAND, csr_off(3, 0), p, 8, 8);
            pass = pass && expect_status("len0", tlm::TLM_BURST_ERROR_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), p, 0, 0);
            pass = pass && expect_status("null", tlm::TLM_GENERIC_ERROR_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), nullptr, 8, 8);
            pass = pass && expect_status("sw<len", tlm::TLM_BURST_ERROR_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), p, 8, 4);

            // Legal lengths 1/4/8 — OK; length 9 with matching SW — OK (spans words).
            pass = pass && expect_status("len1", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), p, 1, 1);
            pass = pass && expect_status("len4", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), p, 4, 4);
            pass = pass && expect_status("len8", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), p, 8, 8);
            uint8_t nine[9] = {};
            pass = pass && expect_status("len9", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0), nine, 9, 9);

            // Unmapped offset inside window hole (word 3 of region 0) and beyond map:
            // Memory policy is TLM_OK (reserved reads as zero / writes dropped).
            pass = pass && expect_status("hole", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(0, 3), p, 8, 8);
            pass = pass && expect_status("oob", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, 0x200, p, 8, 8);

            // Unaligned address is accepted by Memory (byte merge) → OK.
            pass = pass && expect_status("unaligned", tlm::TLM_OK_RESPONSE,
                                         tlm::TLM_READ_COMMAND, csr_off(3, 0) + 1, p, 8, 8);

            // Adversarial: BE pointer with length 0 → BYTE_ENABLE_ERROR.
            unsigned char be_dummy = 0xFF;
            pass = pass && expect_status("be_len0", tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                                         tlm::TLM_WRITE_COMMAND, csr_off(3, 0), p, 8, 8,
                                         &be_dummy, 0);

            // Previously programmed entry 3 must be unchanged.
            uint64_t post_s = 0, post_e = 0, post_a = 0;
            pass = pass && csr_read64(csr_off(3, 0), post_s) && (post_s == pre_s);
            pass = pass && csr_read64(csr_off(3, 1), post_e) && (post_e == pre_e);
            pass = pass && csr_read64(csr_off(3, 2), post_a) && (post_a == pre_a);

            report("T20: CSR malformed payloads + entry unchanged", pass);
        }

        // ==================================================================
        // T21: Range oracle — [0x10000, 0x12000), empty, reversed
        // ==================================================================
        {
            clear_all_via_reset();
            const uint64_t start = 0x10000;
            const uint64_t end   = 0x12000;
            const uint64_t off   = 0x2000000;
            program_region(0, start, end, off, /*cacheable=*/0x0);

            auto check_addr = [&](uint64_t in, uint64_t exp) {
                uint64_t buf = 0;
                auto r = data_xfer(tlm::TLM_READ_COMMAND, in, buf, false, nullptr);
                return r.status == tlm::TLM_OK_RESPONSE &&
                       stub.last_addr == exp && r.addr_after == in;
            };

            bool pass = check_addr(0x10000, expect_translated(0x10000, off))  // hit start
                     && check_addr(0x11fff, expect_translated(0x11fff, off))  // hit end-1
                     && check_addr(0x12000, 0x12000);                         // miss exact end

            // Empty range start==end → passthrough
            program_region(1, 0x30000, 0x30000, 0x111000, /*cacheable=*/0xF);
            pass = pass && check_addr(0x30000, 0x30000);

            // Reversed start>end → passthrough
            program_region(2, 0x50000, 0x40000, 0x222000, /*cacheable=*/0xF);
            pass = pass && check_addr(0x48000, 0x48000);

            report("T21: Range oracle start/end-1/end + empty/reversed", pass);
        }

        // ==================================================================
        // T22: 44-bit page-offset overflow wraps; low 12 bits preserved
        //
        // addr_upper = 0xFFFFFFFFFFE, offset_upper = 2 → sum overflows 44 bits
        // and truncates to 0; low 12 bits (0xABC) are preserved → 0xABC.
        // ==================================================================
        {
            clear_all_via_reset();
            const uint64_t hit_addr = 0x00FFFFFFFFFFEABCULL;
            const uint64_t offset   = 0x2000ULL; // +2 pages
            program_region(0, 0x00FFFFFFFFFFE000ULL, 0x00FFFFFFFFFFF000ULL, offset,
                           /*cacheable=*/0x0);
            const uint64_t exp = expect_translated(hit_addr, offset); // 0xABC
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, hit_addr, buf, false, nullptr);
            bool pass = (r.status == tlm::TLM_OK_RESPONSE) &&
                        (stub.last_addr == exp) &&
                        (exp == 0xABCULL) &&
                        ((stub.last_addr & 0xFFFULL) == 0xABCULL);
            report("T22: 44-bit offset overflow wraps; low 12 preserved", pass);
        }

        // ==================================================================
        // T23: Priority — three overlapping entries; invalidate 0 then 1
        // ==================================================================
        {
            clear_all_via_reset();
            const uint64_t start = 0x20000;
            const uint64_t end   = 0x21000;
            const uint64_t in    = 0x20500;
            // Distinct cacheable nibbles so each step proves *which* entry won
            // from the AxCACHE value alone, not just from the address.
            program_region(0, start, end, 0x100000, /*cacheable=*/0x3);
            program_region(1, start, end, 0x200000, /*cacheable=*/0xC);
            program_region(2, start, end, 0x300000, /*cacheable=*/0x5);

            bool pass = true;

            // Entry 0 wins
            data_access_ext(in, /*cache_in=*/0x0, /*use_dbg=*/false);
            pass = pass && (stub.last_addr == expect_translated(in, 0x100000))
                       && (stub.last_cache == 0x3);

            // Invalidate entry 0 → entry 1 wins
            program_region(0, start, end, 0x100000, 0x3, /*valid=*/false);
            data_access_ext(in, /*cache_in=*/0x0, /*use_dbg=*/false);
            pass = pass && (stub.last_addr == expect_translated(in, 0x200000))
                       && (stub.last_cache == 0xC);

            // Invalidate entry 1 → entry 2 wins
            program_region(1, start, end, 0x200000, 0xC, /*valid=*/false);
            data_access_ext(in, /*cache_in=*/0x0, /*use_dbg=*/false);
            pass = pass && (stub.last_addr == expect_translated(in, 0x300000))
                       && (stub.last_cache == 0x5);

            report("T23: Overlap priority with invalidate cascade", pass);
        }

        // ==================================================================
        // T24: Absent extension on hit; unrelated fields preserved;
        //      downstream error restores address/cacheable
        // ==================================================================
        {
            clear_all_via_reset();
            program_region(0, 0x10000, 0x11000, 0x2000000, /*cacheable=*/0x6);

            // Absent extension — still forwards translated address
            uint64_t buf = 0;
            auto r0 = data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, nullptr);
            bool pass = (r0.status == tlm::TLM_OK_RESPONSE) &&
                        (stub.last_addr == expect_translated(0x10100, 0x2000000)) &&
                        !stub.last_had_ext;

            // Unrelated extension fields preserved through hit: the remap must
            // touch axi_cache only, leaving source_id / group_id / is_ns alone
            // (sep_filter_ctrl downstream reads those).
            sep::sep_axi_extension ext;
            ext.source_id = sep::SMC_SOURCE_ID;
            ext.group_id  = 0xA;
            ext.is_ns     = true;
            ext.axi_cache = 0x9;
            auto r1 = data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, &ext);
            pass = pass && stub.last_had_ext
                       && (stub.last_source_id == sep::SMC_SOURCE_ID)
                       && (stub.last_group_id == 0xA)
                       && stub.last_is_ns
                       && (stub.last_cache == 0x6) // overridden to region value during call
                       && (ext.source_id == sep::SMC_SOURCE_ID)
                       && (ext.group_id == 0xA)
                       && ext.is_ns
                       && (r1.cache_after == 0x9); // restored

            // Downstream error — address and AxCACHE still restored
            stub.forced_status = tlm::TLM_ADDRESS_ERROR_RESPONSE;
            ext.axi_cache = 0x9;
            auto r2 = data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, &ext);
            pass = pass && (r2.status == tlm::TLM_ADDRESS_ERROR_RESPONSE)
                       && (r2.addr_after == 0x10100)
                       && (r2.cache_after == 0x9)
                       && (stub.last_addr == expect_translated(0x10100, 0x2000000));
            stub.forced_status = tlm::TLM_OK_RESPONSE;

            report("T24: Absent ext / field preserve / error restore", pass);
        }

        // ==================================================================
        // T25: Delay — stub adds 17 ns; initiator delay accumulated
        // ==================================================================
        {
            clear_all_via_reset();
            program_region(0, 0x10000, 0x11000, 0x2000000, /*cacheable=*/0x0);
            stub.add_delay = sc_core::sc_time(17, sc_core::SC_NS);

            uint64_t buf = 0;
            const sc_core::sc_time din(5, sc_core::SC_NS);
            auto r = data_xfer(tlm::TLM_READ_COMMAND, 0x10100, buf, false, nullptr, din);
            const sc_core::sc_time expect = din + stub.add_delay;
            bool pass = (r.status == tlm::TLM_OK_RESPONSE) &&
                        (stub.last_delay_in == din) &&
                        (r.delay_after == expect) &&
                        (stub.last_addr == expect_translated(0x10100, 0x2000000));

            // Also verify initiator-owned length/streaming/cmd observed downstream
            pass = pass && (stub.last_cmd == tlm::TLM_READ_COMMAND)
                       && (stub.last_len == 8)
                       && (stub.last_streaming_width == 8)
                       && (stub.last_data_ptr == reinterpret_cast<unsigned char*>(&buf));

            report("T25: Downstream +17ns delay accumulated; metadata intact", pass);
        }

        // ==================================================================
        // T26: transport_dbg miss byte count + address restoration
        // ==================================================================
        {
            uint64_t buf = 0;
            auto r = data_xfer(tlm::TLM_READ_COMMAND, 0x90000, buf, true, nullptr);
            report("T26: transport_dbg miss byte count + passthrough",
                   r.status == tlm::TLM_OK_RESPONSE &&
                   r.dbg_bytes == 8 &&
                   stub.last_addr == 0x90000 &&
                   r.addr_after == 0x90000);
        }

        // ------------------------------------------------------------------
        // Summary
        // ------------------------------------------------------------------
        std::cout << "\n=== Test Summary: "
                  << m_tests_passed << "/" << m_tests_run << " passed";
        if (m_tests_failed > 0)
            std::cout << " [" << m_tests_failed << " FAILED]";
        std::cout << " ===\n";

        sc_core::sc_stop();
    }
};

int sc_main(int, char**)
{
    local_master_alias_remap_ctrl_testbench tb("tb");
    sc_core::sc_start();
    return (tb.m_tests_failed == 0) ? 0 : 1;
}
