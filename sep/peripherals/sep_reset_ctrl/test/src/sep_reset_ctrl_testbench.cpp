// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl_testbench.cpp
 * @brief Frontdoor SystemC/TLM testbench for SEP Reset Controller
 *
 * Independent oracles use literal 0x7e / 0x7f (not DUT kMask/kReset aliases).
 * Outputs are active-low software resets gated by global_rst_ni:
 *   out_i = global_rst_ni AND SW_RESET_N[i]
 */

#include <systemc.h>
#include <tlm.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

#include "../../include/sep_reset_ctrl.h"
#include "sep_reset_ctrl_test.h"
#include "reg_param.h"
#include "tlm_probe.h"

namespace {

/// Spec literals — deliberately not aliases of sep_reset_ctrl_ip::SW_RESET_N_*.
constexpr uint64_t kSwResetMask  = 0x7full;
constexpr uint64_t kSwResetReset = 0x7eull;
constexpr unsigned kNumOutputs   = 7;
constexpr uint64_t kCsrOffset    = 0x0;
constexpr uint64_t kApertureBytes = 8;

}  // namespace

// ---------------------------------------------------------------------------
// Companion DUT constructed with global_rst_ni already low (RST-008).
// ---------------------------------------------------------------------------
struct startup_low_probe : sc_core::sc_module {
    SC_HAS_PROCESS(startup_low_probe);

    sep_reset_ctrl_ip        dut;
    // Unused initiator — SystemC requires the DUT target socket to be bound.
    tlm_utils::simple_initiator_socket<startup_low_probe> unused_init;
    sc_core::sc_signal<bool> global_rst_sig;
    sc_core::sc_signal<bool> km_rst_sig;
    sc_core::sc_signal<bool> otbn_rst_sig;
    sc_core::sc_signal<bool> aes_rst_sig;
    sc_core::sc_signal<bool> hmac_rst_sig;
    sc_core::sc_signal<bool> kmac_rst_sig;
    sc_core::sc_signal<bool> trng_rst_sig;
    sc_core::sc_signal<bool> abr_rst_sig;

    explicit startup_low_probe(sc_core::sc_module_name n)
        : sc_module(n)
        , dut("dut")
        , unused_init("unused_init")
        , global_rst_sig("global_rst_sig", false)  // initial value false — no edge
    {
        unused_init.bind(dut.target_socket);
        dut.global_rst_ni(global_rst_sig);
        dut.km_rst_ni(km_rst_sig);
        dut.otbn_rst_n(otbn_rst_sig);
        dut.aes_rst_ni(aes_rst_sig);
        dut.hmac_rst_ni(hmac_rst_sig);
        dut.kmac_rst_ni(kmac_rst_sig);
        dut.trng_rst_ni(trng_rst_sig);
        dut.abr_rst_ni(abr_rst_sig);
    }

    bool all_outputs_low() const
    {
        return !km_rst_sig.read() && !otbn_rst_sig.read() && !aes_rst_sig.read() &&
               !hmac_rst_sig.read() && !kmac_rst_sig.read() && !trng_rst_sig.read() &&
               !abr_rst_sig.read();
    }
};

// ---------------------------------------------------------------------------
// Main testbench
// ---------------------------------------------------------------------------
struct sep_reset_ctrl_testbench : sc_core::sc_module {
    SC_HAS_PROCESS(sep_reset_ctrl_testbench);

    sep_reset_ctrl_ip   dut;
    sep_reset_ctrl_test harness;
    startup_low_probe   startup_low;

    sc_core::sc_signal<bool> global_rst_sig;
    sc_core::sc_signal<bool> km_rst_sig;
    sc_core::sc_signal<bool> otbn_rst_sig;
    sc_core::sc_signal<bool> aes_rst_sig;
    sc_core::sc_signal<bool> hmac_rst_sig;
    sc_core::sc_signal<bool> kmac_rst_sig;
    sc_core::sc_signal<bool> trng_rst_sig;
    sc_core::sc_signal<bool> abr_rst_sig;

    // Kick an SC_METHOD so transport_dbg runs without a thread process context
    // (the model's wait()-guard path).
    sc_core::sc_event dbg_kick_;
    uint64_t          dbg_write_value_ = 0;
    unsigned          dbg_bytes_       = 0;
    bool              dbg_ran_         = false;

    // Pin monitor for same-delta pulse observation (records lows across deltas).
    bool saw_all_low_ = false;
    bool saw_otbn_low_ = false;

    int failures_ = 0;

    SC_CTOR(sep_reset_ctrl_testbench)
        : dut("dut")
        , harness("harness")
        , startup_low("startup_low")
        , global_rst_sig("global_rst_sig")
    {
        harness.initiator_socket.bind(dut.target_socket);

        dut.global_rst_ni(global_rst_sig);
        dut.km_rst_ni(km_rst_sig);
        dut.otbn_rst_n(otbn_rst_sig);
        dut.aes_rst_ni(aes_rst_sig);
        dut.hmac_rst_ni(hmac_rst_sig);
        dut.kmac_rst_ni(kmac_rst_sig);
        dut.trng_rst_ni(trng_rst_sig);
        dut.abr_rst_ni(abr_rst_sig);

        // Existing tests: construct with global reset already deasserted.
        global_rst_sig.write(true);

        SC_THREAD(run_all_tests);

        SC_METHOD(dbg_write_method);
        sensitive << dbg_kick_;
        dont_initialize();

        SC_METHOD(pin_monitor);
        sensitive << km_rst_sig << otbn_rst_sig << aes_rst_sig << hmac_rst_sig
                  << kmac_rst_sig << trng_rst_sig << abr_rst_sig;
        dont_initialize();
    }

    int failures() const { return failures_; }

private:
    void check(bool cond, const std::string& what)
    {
        if (cond) {
            std::cout << "[PASS] " << what << std::endl;
        } else {
            std::cout << "[FAIL] " << what << std::endl;
            ++failures_;
        }
    }

    void expect_ok(const simtlm::access_result& r, const char* what, uint64_t offset)
    {
        if (r.ok())
            return;
        std::ostringstream oss;
        oss << what << " at 0x" << std::hex << offset
            << " returned " << simtlm::response_name(r.status);
        check(false, oss.str());
    }

    uint64_t read64()
    {
        uint64_t data = 0;
        const auto r = simtlm::read_word<uint64_t>(harness.initiator_socket, kCsrOffset, data);
        expect_ok(r, "read64", kCsrOffset);
        return data;
    }

    void write64(uint64_t value)
    {
        const auto r = simtlm::write_word<uint64_t>(harness.initiator_socket, kCsrOffset, value);
        expect_ok(r, "write64", kCsrOffset);
        // Callback yields one delta so update_rst_outputs runs and writes the
        // sc_out ports; signal values are only readable after the update phase.
        wait(sc_core::SC_ZERO_TIME);
    }

    /// Settle pin updates after a path that notifies without yielding (dbg).
    void settle_pins() { wait(sc_core::SC_ZERO_TIME); wait(sc_core::SC_ZERO_TIME); }

    void pin_monitor()
    {
        const auto p = read_pins();
        if (!p[0] && !p[1] && !p[2] && !p[3] && !p[4] && !p[5] && !p[6])
            saw_all_low_ = true;
        if (!p[1])
            saw_otbn_low_ = true;
    }

    std::array<bool, kNumOutputs> read_pins() const
    {
        return {km_rst_sig.read(),  otbn_rst_sig.read(), aes_rst_sig.read(),
                hmac_rst_sig.read(), kmac_rst_sig.read(), trng_rst_sig.read(),
                abr_rst_sig.read()};
    }

    /// Expected pin levels for a given CSR value while global_rst_ni is high.
    static std::array<bool, kNumOutputs> expected_pins(uint64_t sw, bool global_ni = true)
    {
        std::array<bool, kNumOutputs> out{};
        for (unsigned i = 0; i < kNumOutputs; ++i)
            out[i] = global_ni && ((sw >> i) & 1ull);
        return out;
    }

    void expect_pins(uint64_t sw, bool global_ni, const char* label)
    {
        const auto got = read_pins();
        const auto exp = expected_pins(sw, global_ni);
        static const char* names[kNumOutputs] = {
            "km_rst_ni", "otbn_rst_n", "aes_rst_ni", "hmac_rst_ni",
            "kmac_rst_ni", "trng_rst_ni", "abr_rst_ni"};
        bool ok = true;
        std::ostringstream oss;
        oss << label << " pins";
        for (unsigned i = 0; i < kNumOutputs; ++i) {
            if (got[i] != exp[i]) {
                ok = false;
                oss << " [" << names[i] << " got " << got[i] << " want " << exp[i] << "]";
            }
        }
        if (ok) {
            oss << " match CSR=0x" << std::hex << (sw & kSwResetMask)
                << " global_rst_ni=" << global_ni
                << " {km,otbn,aes,hmac,kmac,trng,abr}=";
            for (unsigned i = 0; i < kNumOutputs; ++i)
                oss << (i ? "," : "") << exp[i];
        }
        check(ok, oss.str());
    }

    void snapshot_state(uint64_t& csr, std::array<bool, kNumOutputs>& pins)
    {
        csr  = read64();
        pins = read_pins();
    }

    bool state_unchanged(uint64_t csr_before, const std::array<bool, kNumOutputs>& pins_before)
    {
        return read64() == csr_before && read_pins() == pins_before;
    }

    // -----------------------------------------------------------------------
    // RST-001 / renamed: one-hot and inverse-one-hot pin mapping
    // -----------------------------------------------------------------------
    void test_reset_output_updates()
    {
        std::cout << "\n=== RST-001: reset output updates (pin mapping) ===" << std::endl;

        // Default mapping after power-on / restore.
        write64(kSwResetReset);
        check(read64() == kSwResetReset, "RST-001a: CSR reads 0x7e after restore write");
        expect_pins(kSwResetReset, true,
                    "RST-001a: default 0x7e (km=0, others=1)");

        for (unsigned bit = 0; bit < kNumOutputs; ++bit) {
            const uint64_t one_hot = 1ull << bit;
            write64(one_hot);
            check(read64() == one_hot, "RST-001b: one-hot CSR readback");
            {
                std::ostringstream oss;
                oss << "RST-001b: one-hot bit " << bit;
                expect_pins(one_hot, true, oss.str().c_str());
            }

            const uint64_t inv = kSwResetMask ^ one_hot;
            write64(inv);
            check(read64() == inv, "RST-001c: inverse-one-hot CSR readback");
            {
                std::ostringstream oss;
                oss << "RST-001c: inverse-one-hot bit " << bit;
                expect_pins(inv, true, oss.str().c_str());
            }
        }
    }

    // -----------------------------------------------------------------------
    // Basic CSR patterns (kept; independent literals)
    // -----------------------------------------------------------------------
    void test_register_access()
    {
        std::cout << "\n=== Register access patterns ===" << std::endl;

        write64(0x55ull);
        check(read64() == 0x55ull, "wrote 0x55, read 0x55");
        expect_pins(0x55ull, true, "0x55 pin map");

        write64(0x2aull);
        check(read64() == 0x2aull, "wrote 0x2a, read 0x2a");
        expect_pins(0x2aull, true, "0x2a pin map");
    }

    // -----------------------------------------------------------------------
    // RST-002: reserved bits masked
    // -----------------------------------------------------------------------
    void test_reserved_mask()
    {
        std::cout << "\n=== RST-002: reserved bits [63:7] ===" << std::endl;

        write64(~0ull);
        const uint64_t got = read64();
        check(got == kSwResetMask,
              "RST-002: write ~0ULL reads back 0x7f (reserved bits zero)");
        expect_pins(kSwResetMask, true, "RST-002: all bits released");
    }

    // -----------------------------------------------------------------------
    // RST-003: byte enables / short writes
    // -----------------------------------------------------------------------
    void test_byte_enables_and_partial()
    {
        std::cout << "\n=== RST-003: byte enables / partial writes ===" << std::endl;

        const unsigned char EN  = simtlm::BYTE_ENABLED;
        const unsigned char DIS = simtlm::BYTE_DISABLED;

        // All implemented bits live in lane 0; disabling that lane must be a no-op.
        write64(0x55ull);
        {
            const unsigned char be[8] = {DIS, EN, EN, EN, EN, EN, EN, EN};
            expect_ok(simtlm::write_word_be<uint64_t>(harness.initiator_socket, kCsrOffset,
                                                      0x0ull, be, sizeof(be)),
                      "BE lane0 disabled", kCsrOffset);
            settle_pins();
            check(read64() == 0x55ull,
                  "RST-003a: BE with lane 0 disabled leaves CSR 0x55");
            expect_pins(0x55ull, true, "RST-003a");
        }

        // Enable only lane 0: clear implemented bits.
        {
            const unsigned char be[8] = {EN, DIS, DIS, DIS, DIS, DIS, DIS, DIS};
            expect_ok(simtlm::write_word_be<uint64_t>(harness.initiator_socket, kCsrOffset,
                                                      0x00ull, be, sizeof(be)),
                      "BE lane0 only", kCsrOffset);
            settle_pins();
            check(read64() == 0x00ull, "RST-003b: BE lane 0 write 0 clears CSR");
            expect_pins(0x00ull, true, "RST-003b");
        }

        // Length-1 write at offset 0 merges into byte 0.
        write64(0x7eull);
        {
            uint8_t byte = 0x01u;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(&byte);
            gp.set_data_length(1);
            gp.set_streaming_width(1);
            gp.set_byte_enable_ptr(nullptr);
            const auto r = simtlm::access(harness.initiator_socket, gp);
            expect_ok(r, "len1 write", 0);
            settle_pins();
            check(read64() == 0x01ull, "RST-003c: 1-byte write 0x01 at offset 0");
            expect_pins(0x01ull, true, "RST-003c");
        }

        // Length-1 write at offset 1 touches a reserved byte; implemented bits stay.
        write64(0x2aull);
        {
            uint8_t byte = 0xffu;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(1);
            gp.set_data_ptr(&byte);
            gp.set_data_length(1);
            gp.set_streaming_width(1);
            gp.set_byte_enable_ptr(nullptr);
            const auto r = simtlm::access(harness.initiator_socket, gp);
            expect_ok(r, "len1 write off1", 1);
            settle_pins();
            check(read64() == 0x2aull,
                  "RST-003d: 1-byte write at offset 1 leaves implemented bits");
            expect_pins(0x2aull, true, "RST-003d");
        }

        // Length-4 write updates low half (contains all implemented bits).
        {
            uint32_t word = 0x0000007fu;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(reinterpret_cast<unsigned char*>(&word));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_byte_enable_ptr(nullptr);
            const auto r = simtlm::access(harness.initiator_socket, gp);
            expect_ok(r, "len4 write", 0);
            settle_pins();
            check(read64() == 0x7full, "RST-003e: 4-byte write sets 0x7f");
            expect_pins(0x7full, true, "RST-003e");
        }
    }

    // -----------------------------------------------------------------------
    // RST-004: malformed TLM matrix — exact status, no output/CSR corruption
    // -----------------------------------------------------------------------
    void test_tlm_protocol_matrix()
    {
        std::cout << "\n=== RST-004: TLM protocol matrix ===" << std::endl;

        write64(0x55ull);
        uint64_t csr_before = 0;
        std::array<bool, kNumOutputs> pins_before{};
        snapshot_state(csr_before, pins_before);

        const simtlm::target_geometry geo{kCsrOffset, 8, kApertureBytes};

        struct Case {
            simtlm::defect            d;
            tlm::tlm_response_status  expect;
            const char*               name;
        };

        // Match regmodel::Memory::validate + reserved-hole policy (OK for
        // in-window holes / unaligned / short / oversized that still decode).
        const Case cases[] = {
            {simtlm::defect::ignore_command,           tlm::TLM_COMMAND_ERROR_RESPONSE, "IGNORE"},
            {simtlm::defect::null_data_ptr,            tlm::TLM_GENERIC_ERROR_RESPONSE, "null_ptr"},
            {simtlm::defect::zero_length,              tlm::TLM_BURST_ERROR_RESPONSE,   "len0"},
            {simtlm::defect::streaming_width_zero,     tlm::TLM_BURST_ERROR_RESPONSE,   "stream0"},
            {simtlm::defect::streaming_width_partial,  tlm::TLM_BURST_ERROR_RESPONSE,   "stream<len"},
            // Pass validate: short/odd/oversize/unaligned/OOB stay TLM_OK (Memory contract).
            {simtlm::defect::short_length,             tlm::TLM_OK_RESPONSE,            "len1"},
            {simtlm::defect::odd_length,               tlm::TLM_OK_RESPONSE,            "len3"},
            {simtlm::defect::oversized_length,         tlm::TLM_OK_RESPONSE,            "len9"},
            {simtlm::defect::unaligned_address,        tlm::TLM_OK_RESPONSE,            "unaligned"},
            {simtlm::defect::address_past_aperture,    tlm::TLM_OK_RESPONSE,            "oob"},
            {simtlm::defect::streaming_width_excess,   tlm::TLM_OK_RESPONSE,            "stream>len"},
        };

        for (const auto& c : cases) {
            // Restore known state before each reject / mutating probe.
            write64(0x55ull);
            snapshot_state(csr_before, pins_before);

            const auto r = simtlm::probe_defect(harness.initiator_socket, c.d, geo,
                                               tlm::TLM_WRITE_COMMAND);
            {
                std::ostringstream oss;
                oss << "RST-004 " << c.name << " status="
                    << simtlm::response_name(r.status)
                    << " expect " << simtlm::response_name(c.expect);
                check(r.status == c.expect, oss.str());
            }

            if (c.expect != tlm::TLM_OK_RESPONSE) {
                check(state_unchanged(csr_before, pins_before),
                      std::string("RST-004 ") + c.name + " left CSR/pins unchanged");
            }
        }

        // Explicit length-4 / length-8 control cases (well-formed).
        write64(0x55ull);
        {
            uint32_t w = 0x2au;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(reinterpret_cast<unsigned char*>(&w));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            const bool ok = simtlm::access(harness.initiator_socket, gp).ok();
            settle_pins();
            check(ok && read64() == 0x2aull, "RST-004 len4 write OK → 0x2a");
        }
        {
            write64(0x55ull);
            snapshot_state(csr_before, pins_before);
            uint64_t v = 0x55ull;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(0);
            gp.set_data_ptr(reinterpret_cast<unsigned char*>(&v));
            gp.set_data_length(8);
            gp.set_streaming_width(8);
            const bool ok = simtlm::access(harness.initiator_socket, gp).ok();
            settle_pins();
            check(ok && state_unchanged(csr_before, pins_before),
                  "RST-004 len8 rewrite same value: OK, pins stable");
        }

        // BE pointer with length 0 is illegal.
        write64(0x55ull);
        snapshot_state(csr_before, pins_before);
        {
            const unsigned char be[1] = {simtlm::BYTE_ENABLED};
            const auto r = simtlm::write_word_be<uint64_t>(
                harness.initiator_socket, kCsrOffset, 0x0ull, be, 0);
            check(r.status == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE &&
                      state_unchanged(csr_before, pins_before),
                  "RST-004 BE length 0 rejected; CSR/pins unchanged");
        }
    }

    // -----------------------------------------------------------------------
    // RST-005: same-delta assert then release (yield-one-delta rule)
    // -----------------------------------------------------------------------
    void test_same_delta_pulse()
    {
        std::cout << "\n=== RST-005: same-delta assert/release pulse ===" << std::endl;

        // Start released. write64 settles pins; clear the monitor afterwards.
        write64(kSwResetMask);
        expect_pins(kSwResetMask, true, "RST-005 setup all released");
        saw_all_low_  = false;
        saw_otbn_low_ = false;

        // Back-to-back zero-time writes with NO wait between them. The model's
        // yield-one-delta rule must let update_rst_outputs fire with the asserted
        // value before the release overwrites the event; the pin monitor records
        // the committed low level across the intervening update phase.
        {
            const auto r0 = simtlm::write_word<uint64_t>(
                harness.initiator_socket, kCsrOffset, 0x00ull);
            expect_ok(r0, "assert write", kCsrOffset);
            const auto r1 = simtlm::write_word<uint64_t>(
                harness.initiator_socket, kCsrOffset, kSwResetMask);
            expect_ok(r1, "release write", kCsrOffset);
        }
        settle_pins();
        check(saw_all_low_,
              "RST-005a: pin monitor saw all outputs low during assert→release pulse");
        expect_pins(kSwResetMask, true,
                    "RST-005b: after release settles, all outputs high");
        check(read64() == kSwResetMask, "RST-005b: CSR is 0x7f after pulse");

        // Single-bit pulse: clear OTBN then set it again with no wait between.
        write64(kSwResetMask);
        saw_otbn_low_ = false;
        {
            const auto r0 = simtlm::write_word<uint64_t>(
                harness.initiator_socket, kCsrOffset, kSwResetMask & ~(1ull << 1));
            expect_ok(r0, "clear OTBN", kCsrOffset);
            const auto r1 = simtlm::write_word<uint64_t>(
                harness.initiator_socket, kCsrOffset, kSwResetMask);
            expect_ok(r1, "restore OTBN", kCsrOffset);
        }
        settle_pins();
        check(saw_otbn_low_,
              "RST-005c: pin monitor saw otbn_rst_n low during bit pulse");
        expect_pins(kSwResetMask, true, "RST-005d: OTBN released again");
    }

    // -----------------------------------------------------------------------
    // RST-006: transport_dbg from SC_METHOD (no illegal wait) + DMI refuse
    // -----------------------------------------------------------------------
    void dbg_write_method()
    {
        dbg_bytes_ = simtlm::debug_write(
            harness.initiator_socket, kCsrOffset,
            reinterpret_cast<unsigned char*>(&dbg_write_value_),
            sizeof(dbg_write_value_));
        dbg_ran_ = true;
    }

    void test_debug_and_dmi()
    {
        std::cout << "\n=== RST-006: transport_dbg + DMI ===" << std::endl;

        write64(0x2aull);
        uint64_t dbg_rd = 0;
        const unsigned n = simtlm::debug_read(
            harness.initiator_socket, kCsrOffset,
            reinterpret_cast<unsigned char*>(&dbg_rd), sizeof(dbg_rd));
        check(n == sizeof(dbg_rd) && dbg_rd == 0x2aull,
              "RST-006a: transport_dbg read returns programmed 0x2a");

        // Debug write from SC_METHOD: process kind is METHOD → model must not wait().
        dbg_write_value_ = 0x7full;
        dbg_ran_         = false;
        dbg_kick_.notify(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);  // method runs
        check(dbg_ran_ && dbg_bytes_ == sizeof(dbg_write_value_),
              "RST-006b: SC_METHOD transport_dbg write transferred 8 bytes");

        // Debug path notifies without yielding; settle for event + signal update.
        settle_pins();
        check(read64() == 0x7full, "RST-006c: MMIO readback after dbg write is 0x7f");
        expect_pins(0x7full, true, "RST-006c: pins follow dbg write");

        const auto dmi = simtlm::dmi_request(harness.initiator_socket, kCsrOffset);
        check(!dmi.granted, "RST-006d: DMI refused (regmodel::Memory default)");
    }

    // -----------------------------------------------------------------------
    // RST-007 / RST-009: global reset behavior (actual pin drive)
    // -----------------------------------------------------------------------
    void test_global_reset_behavior()
    {
        std::cout << "\n=== RST-007/009: global_rst_ni behavior ===" << std::endl;

        write64(kSwResetMask);
        expect_pins(kSwResetMask, true, "RST-007 setup all released");

        global_rst_sig.write(false);
        wait(sc_core::SC_ZERO_TIME);  // signal update
        wait(sc_core::SC_ZERO_TIME);  // reset_handler + update_rst_outputs

        expect_pins(/*sw=*/kSwResetReset, /*global=*/false,
                    "RST-007a: while global low, all outputs forced low");
        check(read64() == kSwResetReset,
              "RST-007a: low edge restores CSR to 0x7e");

        // Writes while held in global reset must not leave a stale output high.
        write64(kSwResetMask);
        wait(sc_core::SC_ZERO_TIME);
        expect_pins(kSwResetMask, false,
                    "RST-007b: write 0x7f during global reset — pins stay low");
        // Shadow may accept the write (callback still runs); on release the
        // low-level reset_handler has already restored 0x7e, but a write after
        // that restore updates the shadow. Re-assert reset edge to clear again.
        global_rst_sig.write(true);
        wait(sc_core::SC_ZERO_TIME);
        global_rst_sig.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(read64() == kSwResetReset,
              "RST-007c: re-asserting global reset restores CSR 0x7e");
        expect_pins(kSwResetReset, false, "RST-007c: pins still low while held");

        // Release: pins must match reset CSR 0x7e (km held, others released).
        global_rst_sig.write(true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(read64() == kSwResetReset, "RST-009: CSR still 0x7e after release");
        expect_pins(kSwResetReset, true,
                    "RST-009: after release pins match 0x7e "
                    "(km=0, otbn=1, aes=1, hmac=1, kmac=1, trng=1, abr=1)");
    }

    // -----------------------------------------------------------------------
    // RST-008: time-zero with global already low / already high
    // -----------------------------------------------------------------------
    void test_startup_levels()
    {
        std::cout << "\n=== RST-008: startup levels ===" << std::endl;

        // Main DUT was constructed with global_rst_ni=true; after t=0 init the
        // default CSR 0x7e drives the documented pin map. (May have been
        // overwritten by earlier tests — restore and check the mapping.)
        write64(kSwResetReset);
        expect_pins(kSwResetReset, true,
                    "RST-008a: global high → pins match reset CSR 0x7e");

        // Companion DUT: sc_signal initial value false. update_rst_outputs runs
        // at elaboration/t=0 with global low → all outputs low.
        check(startup_low.all_outputs_low(),
              "RST-008b: DUT constructed with global_rst_ni=0 has all outputs low at t=0+");
    }

    void run_all_tests()
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "SEP RESET CONTROLLER COMPREHENSIVE TESTBENCH" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        // Let time-zero SC_METHODs commit before the first check.
        wait(sc_core::SC_ZERO_TIME);

        check(read64() == kSwResetReset,
              "power-on: SW_RESET_N reads 0x7e");
        expect_pins(kSwResetReset, true, "power-on pin map");

        test_startup_levels();
        test_register_access();
        test_reset_output_updates();
        test_reserved_mask();
        test_byte_enables_and_partial();
        test_tlm_protocol_matrix();
        test_same_delta_pulse();
        test_debug_and_dmi();
        test_global_reset_behavior();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        if (failures_ == 0) {
            std::cout << "ALL TESTS PASSED" << std::endl;
        } else {
            std::cout << "TESTS FAILED: " << failures_ << " check(s)" << std::endl;
        }
        std::cout << std::string(80, '=') << std::endl;
        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);
    sep_reset_ctrl_testbench testbench("testbench");
    sc_core::sc_start();
    const int fails = testbench.failures();
    std::cout << "\nSEP Reset Controller testbench completed"
              << (fails ? " with failures." : " successfully!") << std::endl;
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    if (fails != 0)
        return 1;
    std::quick_exit(0);
    return 0;
}
