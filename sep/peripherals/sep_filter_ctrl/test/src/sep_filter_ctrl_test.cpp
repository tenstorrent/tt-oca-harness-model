// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_filter_ctrl_test.h"
#include "tlm_probe.h"
#include <cassert>
#include <tlm.h>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace tlm;
using namespace sc_core;

static constexpr uint32_t CSR_STRIDE = 0x20;

// =============================================================================
// Raw register access
// =============================================================================

void sep_filter_ctrl_test::note_transport(const simtlm::access_result& r,
                                          const char* op, uint64_t offset)
{
    if (r.ok()) return;

    ++m_transport_failures;

    std::ostringstream oss;
    oss << op << " at offset 0x" << std::hex << offset
        << " returned " << simtlm::response_name(r.status);
    m_last_transport_error = oss.str();
    std::cout << "  [TRANSPORT] " << m_last_transport_error << std::endl;
}

void sep_filter_ctrl_test::clear_transport_failures()
{
    m_transport_failures = 0;
    m_last_transport_error.clear();
}

simtlm::access_result sep_filter_ctrl_test::probe(simtlm::defect d,
                                                  const simtlm::target_geometry& geo,
                                                  tlm::tlm_command cmd)
{
    return simtlm::probe_defect(initiator_socket, d, geo, cmd);
}

void sep_filter_ctrl_test::register_read_64(unsigned int offset, uint64_t& read_value)
{
    const auto r = simtlm::read_word<uint64_t>(initiator_socket, offset, read_value);
    note_transport(r, "register_read_64", offset);
}

void sep_filter_ctrl_test::register_write_64(unsigned int offset, uint64_t write_value)
{
    const auto r = simtlm::write_word<uint64_t>(initiator_socket, offset, write_value);
    note_transport(r, "register_write_64", offset);
}

void sep_filter_ctrl_test::register_read_8(unsigned int offset, uint8_t& read_value)
{
    const auto r = simtlm::read_word<uint8_t>(initiator_socket, offset, read_value);
    note_transport(r, "register_read_8", offset);
}

void sep_filter_ctrl_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    const auto r = simtlm::write_word<uint8_t>(initiator_socket, offset, write_value);
    note_transport(r, "register_write_8", offset);
}

// =============================================================================
// Per-instance CSR access
// =============================================================================

void sep_filter_ctrl_test::csr_write_64(uint32_t instance, uint32_t reg_offset, uint64_t value)
{
    const uint64_t addr = (instance * CSR_STRIDE) + reg_offset;
    const auto r = simtlm::write_word<uint64_t>(initiator_socket, addr, value);
    note_transport(r, "csr_write_64", addr);
}

uint64_t sep_filter_ctrl_test::csr_read_64(uint32_t instance, uint32_t reg_offset)
{
    const uint64_t addr = (instance * CSR_STRIDE) + reg_offset;
    uint64_t read_value = 0;
    const auto r = simtlm::read_word<uint64_t>(initiator_socket, addr, read_value);
    note_transport(r, "csr_read_64", addr);
    return read_value;
}

void sep_filter_ctrl_test::csr_write_32_pair(uint32_t instance, uint32_t reg_offset, uint64_t value)
{
    const uint32_t addr = (instance * CSR_STRIDE) + reg_offset;
    const uint32_t lo = static_cast<uint32_t>(value);
    const uint32_t hi = static_cast<uint32_t>(value >> 32);

    auto beat32 = [this](uint32_t byte_addr, uint32_t word) {
        const auto r = simtlm::write_word<uint32_t>(initiator_socket, byte_addr, word);
        note_transport(r, "csr_write_32_pair", byte_addr);
    };

    beat32(addr, lo);
    beat32(addr + 4, hi);
}

// =============================================================================
// Test methods
// =============================================================================

void sep_filter_ctrl_test::test_reset_values(uint32_t num_instances)
{
    std::cout << "\n=== Testing Reset Values (num_instances=" << num_instances << ") ===" << std::endl;

    uint32_t check_count = (num_instances < 4) ? num_instances : 4;
    for (uint32_t i = 0; i < check_count; ++i) {
        uint64_t config = csr_read_64(i, 0x00);
        std::cout << "  Instance " << i << " FILTER_CONFIG: 0x" << std::hex << config << std::dec << std::endl;
        // data_bus_width=3 at [14:12] is hw=w; stored value is 0 at reset but read callback returns 3
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always read as 3");
        assert(((config & ~0x7000ULL) == 0x0ULL) && "Other FILTER_CONFIG bits should reset to 0");

        uint64_t start = csr_read_64(i, 0x08);
        assert((start == 0x0ULL) && "START_ADDR should reset to 0");

        uint64_t end = csr_read_64(i, 0x10);
        assert((end == 0x7ULL) && "END_ADDR should reset to 0x7 (minimum 8-byte granularity)");
    }
    std::cout << "Reset values test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_register_access_basic()
{
    std::cout << "\n=== Testing Basic Register Access ===" << std::endl;

    uint32_t instance = 0;

    // Write basic fields (not data_bus_width which is hw=w)
    uint64_t test_config = 0x01F0013ULL; // read_allowed, write_allowed, entry_enabled, allow_ns, allow_burst
    csr_write_64(instance, 0x00, test_config);
    uint64_t read_config = csr_read_64(instance, 0x00);

    uint64_t basic_mask = 0x01F0013ULL;
    assert(((read_config & basic_mask) == test_config) && "FILTER_CONFIG basic fields failed");
    assert(((read_config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");

    uint64_t test_start = 0x12345000ULL;
    csr_write_64(instance, 0x08, test_start);
    assert((csr_read_64(instance, 0x08) == test_start) && "START_ADDR access failed");

    uint64_t test_end = 0x56789000ULL;
    csr_write_64(instance, 0x10, test_end);
    assert((csr_read_64(instance, 0x10) == test_end) && "END_ADDR access failed");

    std::cout << "Basic register access test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_woset_locked_field()
{
    std::cout << "\n=== Testing WOSET Locked Field ===" << std::endl;

    const uint32_t instance = LOCKED_ENTRY;
    uint64_t config = csr_read_64(instance, 0x00);
    assert(((config & (1ULL << 63)) == 0) && "Locked should start as 0");

    const uint64_t start_before = csr_read_64(instance, 0x08);
    const uint64_t end_before   = csr_read_64(instance, 0x10);

    // Program a recognisable configuration and lock it in the same store
    // (locked | src_id=5; entry_enabled stays 0 so the data-path suites that
    // follow are unaffected). The write that sets the lock is itself still
    // accepted — the entry is not locked yet when it is decoded.
    const uint64_t cfg_programmed = (1ULL << 63) | (5ULL << 16);
    csr_write_64(instance, 0x00, cfg_programmed);
    config = csr_read_64(instance, 0x00);
    assert(((config & (1ULL << 63)) != 0) && "Locked bit should be set");
    const uint64_t cfg_locked = config;

    // RTL #2480 (axi_filter_wrap.sv): once locked, every write to the entry's
    // FILTER_CONFIG / START_ADDR / END_ADDR is steered to the AXI error
    // subordinate and terminates with DECERR. The TLM equivalent is
    // TLM_ADDRESS_ERROR_RESPONSE, the same status a blocked data transaction
    // gets. Reads still return the locked configuration.
    const uint64_t base = instance * CSR_STRIDE;
    auto locked_write = [this, base](uint32_t reg_off, uint64_t v) {
        return simtlm::write_word<uint64_t>(initiator_socket, base + reg_off, v).status;
    };
    auto locked_write32 = [this, base](uint32_t byte_off, uint32_t v) {
        return simtlm::write_word<uint32_t>(initiator_socket, base + byte_off, v).status;
    };

    // Try to clear the lock → DECERR, and the lock (plus the rest of
    // FILTER_CONFIG) is untouched.
    assert((locked_write(0x00, 0x0ULL) == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
           "FILTER_CONFIG write to a locked entry must return DECERR");
    assert((csr_read_64(instance, 0x00) == cfg_locked) &&
           "FILTER_CONFIG must be unchanged after a DECERR'd write");

    // Re-asserting the lock is also a FILTER_CONFIG write → DECERR.
    assert((locked_write(0x00, 1ULL << 63) == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
           "re-locking a locked entry must return DECERR");

    // START/END full-width stores → DECERR, values frozen.
    assert((locked_write(0x08, 0x11110000ULL) == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
           "START_ADDR write to a locked entry must return DECERR");
    assert((locked_write(0x10, 0x22220000ULL) == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
           "END_ADDR write to a locked entry must return DECERR");
    assert((csr_read_64(instance, 0x08) == start_before) && "START_ADDR must freeze when locked");
    assert((csr_read_64(instance, 0x10) == end_before) && "END_ADDR must freeze when locked");

    // The RV32 wr64 path (two 32-bit beats) hits the same steering — every
    // 32-bit lane of the three registers is DECERR'd, low and high halves.
    for (uint32_t lane : {0x00u, 0x04u, 0x08u, 0x0Cu, 0x10u, 0x14u}) {
        assert((locked_write32(lane, 0xDEADBEEFu) == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
               "32-bit lane write to a locked entry must return DECERR");
    }
    assert((csr_read_64(instance, 0x00) == cfg_locked)   && "FILTER_CONFIG frozen after 32-bit lanes");
    assert((csr_read_64(instance, 0x08) == start_before) && "START_ADDR frozen after 32-bit lanes");
    assert((csr_read_64(instance, 0x10) == end_before)   && "END_ADDR frozen after 32-bit lanes");

    // A write that only touches the reserved +0x18 word is not one of the
    // three locked registers: register-file policy (write-ignored, OK).
    assert((locked_write(0x18, 0xFFFFFFFFFFFFFFFFULL) == tlm::TLM_OK_RESPONSE) &&
           "reserved +0x18 word of a locked entry keeps the WI/OK policy");
    assert((csr_read_64(instance, 0x18) == 0x0ULL) && "reserved word still reads as zero");
    // ...but a burst that starts in the reserved word of the *previous* entry
    // and spills into this locked entry's FILTER_CONFIG is steered.
    if (instance > 0) {
        unsigned char span[16] = {0};
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(base - 8);
        gp.set_data_ptr(span);
        gp.set_data_length(sizeof span);
        gp.set_streaming_width(sizeof span);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_byte_enable_length(0);
        gp.set_dmi_allowed(false);
        const auto r = simtlm::access(initiator_socket, gp);
        assert((r.status == tlm::TLM_ADDRESS_ERROR_RESPONSE) &&
               "multi-word write spilling into a locked register must return DECERR");
        assert((csr_read_64(instance, 0x00) == cfg_locked) && "spill write must not touch FILTER_CONFIG");
    }

    // Debug transport follows the same steering: 0 bytes transferred.
    {
        uint64_t junk = 0x3333000000000000ULL;
        const unsigned n = simtlm::debug_write(initiator_socket, base + 0x08,
                                               reinterpret_cast<unsigned char*>(&junk), 8);
        assert((n == 0u) && "transport_dbg write to a locked register must transfer 0 bytes");
        assert((csr_read_64(instance, 0x08) == start_before) && "dbg write must not touch START_ADDR");

        uint64_t rd = 0;
        const unsigned m = simtlm::debug_read(initiator_socket, base + 0x00,
                                              reinterpret_cast<unsigned char*>(&rd), 8);
        assert((m == 8u) && (rd == cfg_locked) && "transport_dbg read of a locked entry still works");
    }

    // Malformed payloads to a locked entry keep the register file's verdict —
    // the steering is only for well-formed writes the RTL would actually decode.
    {
        const simtlm::target_geometry geo{base + 0x08, 8, 8};
        assert((probe(simtlm::defect::ignore_command, geo, tlm::TLM_WRITE_COMMAND).status
                    == tlm::TLM_COMMAND_ERROR_RESPONSE) &&
               "IGNORE_COMMAND to a locked entry stays a command error");
        assert((probe(simtlm::defect::zero_length, geo, tlm::TLM_WRITE_COMMAND).status
                    == tlm::TLM_BURST_ERROR_RESPONSE) &&
               "zero-length write to a locked entry stays a burst error");
    }

    // The lock is per-entry: the neighbouring unlocked entry still accepts
    // writes with an OK response (recorded by csr_write_64 if not).
    const uint32_t other = instance + 1;
    const uint64_t other_cfg_before   = csr_read_64(other, 0x00);
    const uint64_t other_start_before = csr_read_64(other, 0x08);
    const uint64_t other_end_before   = csr_read_64(other, 0x10);
    csr_write_64(other, 0x08, 0x44440000ULL);
    csr_write_64(other, 0x10, 0x4444FFFFULL);
    assert((csr_read_64(other, 0x08) == 0x44440000ULL) && "unlocked neighbour START_ADDR still writable");
    assert((csr_read_64(other, 0x10) == 0x4444FFFFULL) && "unlocked neighbour END_ADDR still writable");
    assert((csr_read_64(other, 0x00) == other_cfg_before) && "neighbour FILTER_CONFIG untouched");
    // ...and on the debug path too: a well-formed dbg write to an unlocked
    // register is transferred in full and lands in the register file.
    {
        uint64_t dbg_val = 0x55550000ULL;
        const unsigned n = simtlm::debug_write(initiator_socket, other * CSR_STRIDE + 0x08,
                                               reinterpret_cast<unsigned char*>(&dbg_val), 8);
        assert((n == 8u) && "transport_dbg write to an unlocked register transfers 8 bytes");
        assert((csr_read_64(other, 0x08) == 0x55550000ULL) && "dbg write must land in START_ADDR");
    }
    // Leave the neighbour as we found it for the suites that follow.
    csr_write_64(other, 0x08, other_start_before);
    csr_write_64(other, 0x10, other_end_before);

    std::cout << "WOSET locked field test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_hw_readonly_data_bus_width()
{
    std::cout << "\n=== Testing HW-readonly data_bus_width ===" << std::endl;

    uint32_t instance = 2;
    uint64_t test_values[] = {0x0000ULL, 0x1000ULL, 0x2000ULL, 0x4000ULL, 0x7000ULL};
    for (auto test_val : test_values) {
        csr_write_64(instance, 0x00, test_val);
        uint64_t config = csr_read_64(instance, 0x00);
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");
    }
    std::cout << "HW-readonly data_bus_width test PASSED" << std::endl;
}

// Note: this only checks CSR readback (entry_enabled clears to 0). It does NOT
// mean the data path passes through when unconfigured — BlockByDefault=1
// means an unconfigured filter denies all data-path transactions (see A7 in
// sep_filter_ctrl_testbench.cpp).
void sep_filter_ctrl_test::test_passthrough_when_unconfigured(uint32_t num_instances)
{
    std::cout << "\n=== Testing CSR Clears to Unconfigured (entry_enabled=0) ===" << std::endl;

    uint32_t check_count = (num_instances < 8) ? num_instances : 8;
    for (uint32_t i = 0; i < check_count; ++i) {
        csr_write_64(i, 0x00, 0x0ULL);
        uint64_t config = csr_read_64(i, 0x00);
        assert(((config & (1ULL << 4)) == 0) && "entry_enabled should be 0 when cleared");
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");
    }
    std::cout << "CSR unconfigured state test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_comprehensive_filter_scenarios()
{
    std::cout << "\n=== Testing Comprehensive Filter Scenarios ===" << std::endl;

    // Entry 0: read+write, [0x1000, 0x2000)
    csr_write_64(0, 0x08, 0x1000ULL);
    csr_write_64(0, 0x10, 0x2000ULL);
    csr_write_64(0, 0x00, 0x00000013ULL); // read_allowed=1, write_allowed=1, entry_enabled=1
    assert((csr_read_64(0, 0x08) == 0x1000ULL) && "START_ADDR[0] failed");
    assert((csr_read_64(0, 0x10) == 0x2000ULL) && "END_ADDR[0] failed");
    uint64_t cfg0 = csr_read_64(0, 0x00);
    assert(((cfg0 & 0x13ULL) == 0x13ULL) && "FILTER_CONFIG[0] basic bits failed");
    assert(((cfg0 & 0x3000ULL) == 0x3000ULL) && "data_bus_width should remain 3");

    // Entry 1: read-only, [0x3000, 0x4000)
    csr_write_64(1, 0x08, 0x3000ULL);
    csr_write_64(1, 0x10, 0x4000ULL);
    csr_write_64(1, 0x00, 0x00000011ULL); // read_allowed=1, write_allowed=0, entry_enabled=1

    // Entry 2: write-only, [0x5000, 0x6000)
    csr_write_64(2, 0x08, 0x5000ULL);
    csr_write_64(2, 0x10, 0x6000ULL);
    csr_write_64(2, 0x00, 0x00000012ULL); // read_allowed=0, write_allowed=1, entry_enabled=1

    std::cout << "Comprehensive filter scenarios test PASSED" << std::endl;
}

// Malformed generic payloads on the CSR path. The filter must give every defect
// a decided response, must not crash, and must leave a neighbouring entry's
// registers alone. What each defect should *return* is decode policy, so only
// the "decided, non-destructive" part is asserted here.
void sep_filter_ctrl_test::test_malformed_payloads(uint32_t num_instances)
{
    std::cout << "\n=== Testing Malformed Generic Payloads ===" << std::endl;

    // Entry 3 is the target; entry 5 is the witness, far enough away that the
    // widest (9-byte) and unaligned defects cannot reach it.
    const uint32_t target_inst  = 3;
    const uint32_t witness_inst = (num_instances > 5) ? 5 : (num_instances - 1);
    assert((witness_inst != target_inst) && "need a distinct witness entry");

    const uint64_t witness_start = 0xABCD0000ULL;
    csr_write_64(witness_inst, 0x08, witness_start);

    const unsigned before = transport_failures();

    simtlm::target_geometry geo;
    geo.valid_address  = (target_inst * CSR_STRIDE) + 0x08;  // START_ADDR
    geo.word_bytes     = 8;
    geo.aperture_bytes = static_cast<uint64_t>(num_instances) * CSR_STRIDE;

    for (simtlm::defect d : simtlm::all_defects()) {
        for (tlm::tlm_command cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            const auto r = probe(d, geo, cmd);
            if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                std::cout << "  [FAIL] " << simtlm::defect_name(d) << " ("
                          << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                          << ") left the payload INCOMPLETE" << std::endl;
            }
            assert((r.status != tlm::TLM_INCOMPLETE_RESPONSE) &&
                   "malformed payload left unhandled by sep_filter_ctrl");
        }
    }

    // probe() does not record, so the count must not have moved.
    assert((transport_failures() == before) &&
           "probe() must not record deliberate rejections");

    assert((csr_read_64(witness_inst, 0x08) == witness_start) &&
           "malformed traffic corrupted a neighbouring entry");

    // Still usable afterwards.
    csr_write_64(target_inst, 0x08, 0x77770000ULL);
    assert((csr_read_64(target_inst, 0x08) == 0x77770000ULL) &&
           "CSR path unusable after malformed traffic");

    std::cout << "Malformed generic payloads test PASSED" << std::endl;
}
