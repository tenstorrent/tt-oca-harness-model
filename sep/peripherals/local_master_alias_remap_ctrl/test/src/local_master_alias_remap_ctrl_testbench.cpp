// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file local_master_alias_remap_ctrl_testbench.cpp
 * @brief Comprehensive testbench for local_master_alias_remap_ctrl peripheral.
 *
 * Ports test cases from knowledge-base/axi_alias_remap_tb.cpp into the
 * regmodel-style testbench pattern (mirrors ap_output_remap_ctrl/test pattern).
 *
 * SEP local master alias remap configuration under test:
 *   NumRegions = 16, IdxStart = 12 (4 KB granularity)
 *   CSR base = 0x10A10000 (but accessed via regmodel flat layout)
 *   Range-based address matching with additive remapping
 *
 * Wiring:
 *   test_harness.initiator_socket ──► dut.target_socket     (CSR path via base)
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

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

// =============================================================================
// StubTarget — records the last remapped address, and the cache attribute the
// fabric side sees, so the tests can observe the region's cacheable override.
// =============================================================================
struct StubTarget : sc_core::sc_module
{
    tlm_utils::simple_target_socket<StubTarget> socket;
    uint64_t last_addr      = 0;
    uint8_t  last_cache     = 0xFF;   ///< AxCACHE[3:0] seen downstream; 0xFF = no extension
    bool     last_had_ext   = false;

    SC_CTOR(StubTarget) : socket("socket")
    {
        socket.register_b_transport(this,   &StubTarget::b_transport);
        socket.register_transport_dbg(this, &StubTarget::transport_dbg);
    }

    void capture(tlm::tlm_generic_payload& trans)
    {
        last_addr = trans.get_address();

        sep::sep_axi_extension* ext = trans.get_extension<sep::sep_axi_extension>();
        last_had_ext   = (ext != nullptr);
        last_cache     = ext ? ext->axi_cache : 0xFF;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        capture(trans);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay = sc_core::SC_ZERO_TIME;
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        capture(trans);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};

// =============================================================================
// Testbench
// =============================================================================
SC_MODULE(local_master_alias_remap_ctrl_testbench)
{
    SC_HAS_PROCESS(local_master_alias_remap_ctrl_testbench);

    // -------------------------------------------------------------------------
    // SEP local master alias remap configuration
    // -------------------------------------------------------------------------
    static constexpr uint32_t NUM_REGIONS = local_alias_remap_ip::NUM_REGIONS;
    static constexpr uint32_t IDX_START   = local_alias_remap_ip::IDX_START;

    // -------------------------------------------------------------------------
    // Component instances
    // -------------------------------------------------------------------------
    sc_core::sc_signal<bool>  rst_n_sig;    ///< Drive rst_ni high (deasserted) throughout
    local_alias_remap_ip      dut;          ///< Device Under Test
    local_alias_remap_test    test_harness; ///< CSR accessor
    StubTarget                stub;         ///< Records remapped address

    /// Initiator socket for sending data-path transactions to dut.data_socket
    tlm_utils::simple_initiator_socket<local_master_alias_remap_ctrl_testbench> data_initiator;

    // -------------------------------------------------------------------------
    // Test statistics
    // -------------------------------------------------------------------------
    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;

    // =========================================================================
    // Constructor — wires sockets and registers SC_THREAD
    // =========================================================================
    local_master_alias_remap_ctrl_testbench(sc_module_name name)
        : sc_module(name)
        , dut("dut")
        , test_harness("test_harness")
        , stub("stub")
        , data_initiator("data_initiator")
    {
        rst_n_sig.write(true);  // deassert reset for entire testbench run
        dut.rst_ni(rst_n_sig);

        // CSR path: test_harness ──► dut.target_socket (inherited from base)
        test_harness.initiator_socket.bind(dut.target_socket);

        // Data path: data_initiator ──► dut.data_socket ──► stub
        data_initiator.bind(dut.data_socket);
        dut.remapped_socket.bind(stub.socket);

        SC_THREAD(run_tests);
    }

    // =========================================================================
    // TLM helper — data-path read (via data_initiator → dut.data_socket)
    // =========================================================================
    void data_read(uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        data_initiator->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    // =========================================================================
    // TLM helper — data-path write
    // =========================================================================
    void data_write(uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        data_initiator->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    // =========================================================================
    // TLM helper — data-path debug transport
    // =========================================================================
    void data_dbg(uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        data_initiator->transport_dbg(trans);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    // =========================================================================
    // TLM helper — data-path access carrying a sep_axi_extension, so the tests
    // can drive an incoming cache attribute and see what the region override
    // does to it. Returns the extension's value after the call, which is what
    // the initiator would go on to observe.
    //
    // The extension is stack-allocated and paired with clear_extension() rather
    // than set_auto_extension(), which would schedule a release of memory the
    // payload does not own.
    // =========================================================================
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

    // =========================================================================
    // Test helper — program one remap region via the CSR path
    // REGION_START[r] / REGION_END[r] / REGION_ATTRS[r] sit at words r*4 + 0/1/2
    // REGION_ATTRS = offset[55:12] | cacheable[59:56] | valid[63]
    // (alias_remap.rdl after tt-oca-hw #2464).
    // =========================================================================
    static constexpr uint64_t ATTRS_CACHEABLE_SHIFT = 56;
    static constexpr uint64_t ATTRS_VALID           = 1ULL << 63;
    static constexpr uint64_t ATTRS_RW_MASK         = 0x8FFFFFFFFFFFF000ULL;

    static uint64_t attrs_word(uint64_t offset, uint8_t cacheable)
    {
        return (offset & 0x00FFFFFFFFFFF000ULL)
             | (static_cast<uint64_t>(cacheable & 0xFu) << ATTRS_CACHEABLE_SHIFT)
             | ATTRS_VALID;
    }

    void program_region(uint32_t r, uint64_t start, uint64_t end,
                        uint64_t offset, uint8_t cacheable)
    {
        test_harness.register_write_64((r*4 + 0) * 8, start);
        test_harness.register_write_64((r*4 + 1) * 8, end);
        test_harness.register_write_64((r*4 + 2) * 8, attrs_word(offset, cacheable));
    }

    // =========================================================================
    // Test helper — report test result
    // =========================================================================
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
    // run_tests — SC_THREAD entry point
    // =========================================================================
    void run_tests()
    {
        std::cout << "\n=== local_master_alias_remap_ctrl smoke tests ===\n"
                  << std::dec
                  << "  NumRegions  = " << NUM_REGIONS << "\n"
                  << "  IdxStart    = " << IDX_START  << "\n\n";

        // ------------------------------------------------------------------
        // T1: Reset state — all region registers read back as 0
        // ------------------------------------------------------------------
        {
            bool pass = true;
            for (uint32_t i = 0; i < NUM_REGIONS; ++i) {
                auto region = dut.get_region(i);
                if (region.start_addr != 0 || region.end_addr != 0 || 
                    region.offset != 0 || region.cacheable != 0 || region.valid) {
                    pass = false;
                    break;
                }
            }
            report("T1: All region registers reset to 0", pass);
        }

        // ------------------------------------------------------------------
        // T2: Program region 0 and verify readback
        // regmodel interleaved layout: 4-word spacing between regions
        // ------------------------------------------------------------------
        {
            // Program region 0: [0x10000, 0x11000) -> +0x2000000
            test_harness.register_write_64(0 * 8, 0x10000);       // REGION_START[0] at word 0
            test_harness.register_write_64(1 * 8, 0x11000);       // REGION_END[0] at word 1
            // REGION_ATTRS[0]: offset[55:12] | valid[63] = (0x2000000 & 0xFFFFFFFFFFF000) | (1 << 63)
            test_harness.register_write_64(2 * 8, (0x2000000ULL & 0x00FFFFFFFFFFF000ULL) | (1ULL << 63));

            auto region = dut.get_region(0);
            bool pass = (region.start_addr == 0x10000) &&
                       (region.end_addr == 0x11000) &&
                       (region.offset == 0x2000000) &&
                       (region.cacheable == 0) &&
                       (region.valid);
            report("T2: Region 0 program and readback", pass);
        }

        // ------------------------------------------------------------------
        // T3: Address remap test — region 0 hit
        //     Input: 0x10100 (within [0x10000, 0x11000))
        //     Expected: 0x10100 + 0x2000000 = 0x2010100
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr    = 0x10100;
            const uint64_t expected_addr = 0x2010100;

            uint64_t dummy = 0;
            data_read(input_addr, dummy);
            report("T3: Address remap region 0 hit", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T4: Passthrough test — no region match
        //     Input: 0x20000 (outside all programmed regions)
        //     Expected: 0x20000 (unchanged)
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr    = 0x20000;
            const uint64_t expected_addr = 0x20000;

            uint64_t dummy = 0;
            data_read(input_addr, dummy);
            report("T4: Passthrough - no region match", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T5: First-match priority test
        //     Program overlapping region 1: [0x10800, 0x10900) -> +0x3000000
        //     Input: 0x10850 (matches both region 0 and 1)
        //     Expected: region 0 wins (lower index priority)
        // ------------------------------------------------------------------
        {
            // Program region 1 (overlaps with region 0)
            test_harness.register_write_64((1*4 + 0) * 8, 0x10800);          // REGION_START[1] at word 4
            test_harness.register_write_64((1*4 + 1) * 8, 0x10900);          // REGION_END[1] at word 5
            // REGION_ATTRS[1]: offset[55:12] | valid[63] = (0x3000000 & 0xFFFFFFFFFFF000) | (1 << 63)
            test_harness.register_write_64((1*4 + 2) * 8, (0x3000000ULL & 0x00FFFFFFFFFFF000ULL) | (1ULL << 63));

            const uint64_t input_addr    = 0x10850;
            const uint64_t expected_addr = 0x2010850;  // region 0 wins (+0x2000000)

            uint64_t dummy = 0;
            data_read(input_addr, dummy);
            report("T5: First-match priority (region 0 wins)", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T6: Lower bits preservation test (4KB page granularity)
        //     Input: 0x10ABC (lower 12 bits = 0xABC)
        //     Expected: remapped upper + 0xABC preserved
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr    = 0x10ABC;
            const uint64_t expected_addr = 0x2010ABC; // region 0: +0x2000000, lower 0xABC preserved

            uint64_t dummy = 0;
            data_read(input_addr, dummy);
            report("T6: Lower 12 bits preserved", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T7: Write transaction remap
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr    = 0x10200;
            const uint64_t expected_addr = 0x2010200;

            uint64_t dummy = 0xDEADBEEF;
            data_write(input_addr, dummy);
            report("T7: Write transaction remap", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T8: Address restoration after forward
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr = 0x10300;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            uint64_t dummy = 0;

            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_address(input_addr);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&dummy));
            trans.set_data_length(8);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_streaming_width(8);

            data_initiator->b_transport(trans, delay);
            report("T8: Address restoration after forward", trans.get_address() == input_addr);
        }

        // ------------------------------------------------------------------
        // T9: transport_dbg applies same remap
        // ------------------------------------------------------------------
        {
            const uint64_t input_addr    = 0x10400;
            const uint64_t expected_addr = 0x2010400;

            uint64_t dummy = 0;
            data_dbg(input_addr, dummy);
            report("T9: transport_dbg applies same remap", stub.last_addr == expected_addr);
        }

        // ------------------------------------------------------------------
        // T10: Field mask enforcement — 4-bit cacheable[59:56] (tt-oca-hw
        //      #2464). Bit 62, the pre-#2464 single cacheable bit, is now
        //      reserved and must be dropped on write.
        // ------------------------------------------------------------------
        {
            // Program region 2 with cacheable = 0xA (0b1010) AND a stale bit 62.
            test_harness.register_write_64((2*4 + 0) * 8, 0x30000);          // REGION_START[2] at word 8
            test_harness.register_write_64((2*4 + 1) * 8, 0x31000);          // REGION_END[2] at word 9
            const uint64_t attrs_in = (0x1000000ULL & 0x00FFFFFFFFFFF000ULL)
                                    | (0xAULL << ATTRS_CACHEABLE_SHIFT)
                                    | (1ULL << 62)                 // reserved — must vanish
                                    | ATTRS_VALID;
            test_harness.register_write_64((2*4 + 2) * 8, attrs_in);

            auto region = dut.get_region(2);
            bool pass = (region.start_addr == 0x30000) &&
                       (region.end_addr == 0x31000) &&
                       (region.offset == 0x1000000) &&
                       (region.cacheable == 0xA) &&
                       (region.valid);

            uint64_t raw = 0;
            test_harness.register_read_64((2*4 + 2) * 8, raw);
            pass = pass && (raw == (attrs_in & ~(1ULL << 62)));
            report("T10: cacheable[59:56] round-trips as 4 bits; reserved bit 62 dropped", pass);

            // T10b: every writable bit of REGION_ATTRS is exactly
            // offset[55:12] + cacheable[59:56] + valid[63]; [62:60] and [11:0]
            // are RAZ/WI.
            test_harness.register_write_64((3*4 + 2) * 8, 0xFFFFFFFFFFFFFFFFULL);
            test_harness.register_read_64((3*4 + 2) * 8, raw);
            const bool mask_ok = (raw == ATTRS_RW_MASK);
            auto r3 = dut.get_region(3);
            report("T10b: REGION_ATTRS RW mask is 0x8FFFFFFFFFFFF000 (cacheable reads 0xF)",
                   mask_ok && r3.cacheable == 0xF && r3.valid);
            test_harness.register_write_64((3*4 + 2) * 8, 0);   // leave region 3 invalid
        }

        // ------------------------------------------------------------------
        // T11: rst_ni actually clears programmed region state
        //
        // T1 only checked that freshly-constructed registers start at 0 —
        // that passes even if reset_handler()/reset() were never wired up,
        // since rst_n_sig is otherwise held high (deasserted) for the whole
        // run. This test drives a real reset pulse and confirms it clears
        // state that T2/T5/T10 deliberately left non-zero and valid.
        // ------------------------------------------------------------------
        {
            // Precondition: region 0 (from T2) is still programmed and valid.
            auto pre = dut.get_region(0);
            bool preconditions_ok = pre.valid && (pre.start_addr == 0x10000);

            rst_n_sig.write(false);   // assert reset
            wait(10, sc_core::SC_NS);
            rst_n_sig.write(true);    // deassert reset
            wait(10, sc_core::SC_NS);

            bool pass = preconditions_ok;
            for (uint32_t i = 0; i < NUM_REGIONS; ++i) {
                auto region = dut.get_region(i);
                if (region.start_addr != 0 || region.end_addr != 0 ||
                    region.offset != 0 || region.cacheable != 0 || region.valid) {
                    pass = false;
                    break;
                }
            }
            report("T11: rst_ni pulse clears previously-programmed regions", pass);
        }

        // ------------------------------------------------------------------
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
        // ------------------------------------------------------------------
        program_region(0, 0x10000, 0x11000, 0x2000000, /*cacheable=*/0x6);
        program_region(1, 0x40000, 0x41000, 0x5000000, /*cacheable=*/0x0);
        program_region(2, 0x50000, 0x51000, 0x7000000, /*cacheable=*/0x9);

        // T12: hit on region 0 replaces an incoming AxCACHE=0 with exactly 0x6
        //      (not 0xF — the old replicated-bit behaviour).
        {
            data_access_ext(0x10100, /*cache_in=*/0x0, /*use_dbg=*/false);
            report("T12: region cacheable=0x6 drives outgoing AxCACHE to exactly 0x6",
                   stub.last_had_ext && stub.last_cache == 0x6 &&
                   stub.last_addr == 0x2010100);
        }

        // T13: hit on a cacheable=0 region clears every incoming bit — an
        //      override, not an OR; and region 2 flips every bit of 0x6 to 0x9
        //      — an override, not an AND.
        {
            data_access_ext(0x40100, /*cache_in=*/0xF, /*use_dbg=*/false);
            const bool clear_ok = stub.last_had_ext && stub.last_cache == 0x0 &&
                                  stub.last_addr == 0x5040100;

            data_access_ext(0x50100, /*cache_in=*/0x6, /*use_dbg=*/false);
            const bool flip_ok = stub.last_had_ext && stub.last_cache == 0x9 &&
                                 stub.last_addr == 0x7050100;

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
                                stub.last_addr == 0x2010100;

            data_access_ext(0x40100, /*cache_in=*/0xF, /*use_dbg=*/true);
            const bool clear_ok = stub.last_cache == 0x0 && stub.last_addr == 0x5040100;

            data_access_ext(0x50100, /*cache_in=*/0x6, /*use_dbg=*/true);
            const bool flip_ok = stub.last_cache == 0x9 && stub.last_addr == 0x7050100;

            report("T16: transport_dbg applies the same 4-bit cacheable override",
                   hit_ok && clear_ok && flip_ok);
        }

        // ------------------------------------------------------------------
        // T17: transport_dbg passthrough on a miss — the debug path's own
        //      no-hit branch, distinct from T4/T14's functional-path miss.
        // ------------------------------------------------------------------
        {
            const uint8_t after = data_access_ext(0x90000, /*cache_in=*/0xB,
                                                  /*use_dbg=*/true);
            report("T17: transport_dbg passthrough on miss",
                   stub.last_addr == 0x90000 && stub.last_cache == 0xB && after == 0xB);
        }

        // ------------------------------------------------------------------
        // T17b: an un-extended payload on a hit is remapped but carries no
        //       AxCACHE to override; the stub must see no extension.
        // ------------------------------------------------------------------
        {
            uint64_t dummy = 0;
            data_read(0x10100, dummy);
            report("T17b: un-extended payload remapped with no cache attribute to override",
                   !stub.last_had_ext && stub.last_cache == 0xFF &&
                   stub.last_addr == 0x2010100);
        }

        // ------------------------------------------------------------------
        // T18: get_region() bounds guard — an out-of-range index returns a
        //      zeroed Region rather than reading past REGION_ATTRS[15].
        // ------------------------------------------------------------------
        {
            auto region = dut.get_region(NUM_REGIONS);
            report("T18: get_region rejects an out-of-range index",
                   region.start_addr == 0 && region.end_addr == 0 &&
                   region.offset == 0 && region.cacheable == 0 && !region.valid);
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

// =============================================================================
// sc_main
// =============================================================================
int sc_main(int, char**)
{
    local_master_alias_remap_ctrl_testbench tb("tb");
    sc_core::sc_start();
    return (tb.m_tests_failed == 0) ? 0 : 1;
}