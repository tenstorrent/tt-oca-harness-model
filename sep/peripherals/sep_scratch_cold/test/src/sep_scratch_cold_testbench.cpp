// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_scratch_cold_testbench.cpp
 * @brief Testbench for the SEP Scratch Cold peripheral.
 *
 * Tests:
 *   FUNC-SCRATCH-001  Reset values — all 8 registers read 0 after reset
 *   FUNC-SCRATCH-002  Basic read/write — write/read each register independently
 *   FUNC-SCRATCH-003  Register independence — simultaneous distinct values
 *   FUNC-SCRATCH-004  Reserved bits — upper 32-bit half of each register is masked out
 *   FUNC-SCRATCH-005  VP ack: SCRATCH[0]=0x12345678 -> SCRATCH[1]=0x87654321
 *   FUNC-SCRATCH-006  VP ack: SCRATCH[4]=0x815 -> SCRATCH[5]=0x777
 *   FUNC-SCRATCH-007  VP ack: SCRATCH[6]=0xA1E50006 -> SCRATCH[7]=0x00100001
 *   FUNC-SCRATCH-008  No spurious acks — non-magic writes leave adjacent registers unchanged
 *   FUNC-SCRATCH-009  CCI parameter defaults — verbosity, sim_out_enable, sep_status_enable
 *   FUNC-SCRATCH-010  Cold reset clears storage; warm-reset absence retains it
 *   FUNC-SCRATCH-012  Virtual console HEX16 opcode through SCRATCH[2]
 *   FUNC-SCRATCH-013  Decoder isolation — on_bytes, flush, disable, TSV parse edges
 */

#include "sep_scratch_cold.h"
#include "sep_status_decoder.h"
#include "virt_console_decoder.h"
#include "reg_param.h"
#include "reg_logger.h"
#include "tlm_probe.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// =============================================================================
// Testbench
// =============================================================================
struct sep_scratch_cold_testbench : sc_core::sc_module
{
    SC_HAS_PROCESS(sep_scratch_cold_testbench);

    sep_scratch_cold_ip dut;
    /// Second instance whose decoder CCI presets are false (COLD-004).
    sep_scratch_cold_ip quiet;
    tlm_utils::simple_initiator_socket<sep_scratch_cold_testbench, 32> initiator_socket;
    tlm_utils::simple_initiator_socket<sep_scratch_cold_testbench, 32> quiet_socket;
    sc_core::sc_signal<bool> cold_rst_n{"cold_rst_n"};
    sc_core::sc_signal<bool> quiet_rst_n{"quiet_rst_n"};
    RegLogger logger;

    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;

    explicit sep_scratch_cold_testbench(sc_core::sc_module_name n)
        : sc_module(n)
        , dut("dut")
        , quiet("quiet")
        , initiator_socket("initiator_socket")
        , quiet_socket("quiet_socket")
    {
        initiator_socket.bind(dut.target_socket);
        dut.cold_rst_ni(cold_rst_n);
        cold_rst_n.write(true);
        quiet_socket.bind(quiet.target_socket);
        quiet.cold_rst_ni(quiet_rst_n);
        quiet_rst_n.write(true);
        logger.setMaxVerbosity(dut.verbosity.get_param_value());
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
        SC_THREAD(run_tests);
    }

    // -------------------------------------------------------------------------
    // TLM helpers — 32-bit socket, byte-addressed
    // Each SCRATCH[n] occupies 8 bytes (64-bit register); lower 32 bits are r/w.
    // -------------------------------------------------------------------------
    // Signatures are unchanged so the scenarios read the same, but the response
    // status is recorded rather than discarded. report_test_pass() refuses to
    // pass any test that accumulated a transport failure.
    unsigned    m_transport_failures = 0;
    std::string m_last_transport_error;

    void note_transport(const simtlm::access_result& r, const char* op,
                        sc_dt::uint64 byte_addr)
    {
        if (r.ok())
            return;
        ++m_transport_failures;
        std::ostringstream oss;
        oss << op << " at 0x" << std::hex << byte_addr
            << " returned " << simtlm::response_name(r.status);
        m_last_transport_error = oss.str();
        std::cout << "[ TRANS ] " << m_last_transport_error << std::endl;
    }

    void b_write(sc_dt::uint64 byte_addr, uint32_t value)
    {
        const auto r = simtlm::write_word<uint32_t>(initiator_socket, byte_addr, value);
        note_transport(r, "b_write", byte_addr);
    }

    uint32_t b_read(sc_dt::uint64 byte_addr)
    {
        uint32_t data = 0u;
        const auto r = simtlm::read_word<uint32_t>(initiator_socket, byte_addr, data);
        note_transport(r, "b_read", byte_addr);
        return data;
    }

    // Write lower 32 bits of SCRATCH[idx] (byte offset = idx * 8)
    void scratch_write(unsigned idx, uint32_t value)
    {
        b_write(static_cast<sc_dt::uint64>(idx) * 8u, value);
    }

    // Read lower 32 bits of SCRATCH[idx]
    uint32_t scratch_read(unsigned idx)
    {
        return b_read(static_cast<sc_dt::uint64>(idx) * 8u);
    }

    // Write upper 32 bits of SCRATCH[idx] (Reserved0 field — should be masked to 0)
    void scratch_write_upper(unsigned idx, uint32_t value)
    {
        b_write(static_cast<sc_dt::uint64>(idx) * 8u + 4u, value);
    }

    // Read upper 32 bits of SCRATCH[idx] (should always be 0)
    uint32_t scratch_read_upper(unsigned idx)
    {
        return b_read(static_cast<sc_dt::uint64>(idx) * 8u + 4u);
    }

    // -------------------------------------------------------------------------
    // Reporting helpers
    // -------------------------------------------------------------------------
    // Clear the bank the way hardware does. The scenarios below used to call
    // dut.reset_all_registers() directly, which passes even for a model whose
    // cold_rst_ni is wired to nothing; going through the port means every test
    // that needs a clean bank also re-proves the reset path.
    void pin_reset()
    {
        cold_rst_n.write(false);
        wait(1, sc_core::SC_NS);
        cold_rst_n.write(true);
        wait(1, sc_core::SC_NS);
    }

    // Captures everything the decoders print for as long as it is in scope.
    //
    // The console and status callbacks emit to std::cout directly, so the only
    // way to assert what they produced is to take the stream buffer. Restoring
    // in the destructor matters: a scenario that fails early must not leave the
    // rest of the run writing into a dead stringstream.
    class cout_capture
    {
      public:
        cout_capture() : m_saved(std::cout.rdbuf(m_buf.rdbuf())) {}
        // Only restore if release() has not already done so: after release()
        // m_saved is null, and rdbuf(nullptr) would leave std::cout with no
        // stream buffer at all, silently discarding the rest of the run.
        ~cout_capture()
        {
            if (m_saved) std::cout.rdbuf(m_saved);
        }
        cout_capture(const cout_capture&)            = delete;
        cout_capture& operator=(const cout_capture&) = delete;

        /// Stop capturing early, so the caller can print its own diagnosis.
        std::string release()
        {
            if (m_saved) {
                std::cout.rdbuf(m_saved);
                m_saved = nullptr;
            }
            return m_buf.str();
        }

      private:
        std::ostringstream m_buf;
        std::streambuf*    m_saved;
    };

    void report_test_start(const std::string& name)
    {
        // Transport errors are attributed to the test that caused them.
        m_transport_failures = 0;
        m_last_transport_error.clear();
        std::cout << "\n[ RUN   ] " << name << std::endl;
    }

    void report_test_pass(const std::string& name)
    {
        // A scenario cannot pass on the strength of transactions the bank
        // refused. Gating here makes every existing test transport-sensitive
        // without restating the condition in any of them.
        if (m_transport_failures != 0) {
            std::ostringstream oss;
            oss << m_transport_failures << " transport error(s); last: "
                << m_last_transport_error;
            report_test_fail(name, oss.str());
            return;
        }
        m_tests_passed++;
        m_tests_run++;
        std::cout << "[ PASS  ] " << name << std::endl;
    }

    void report_test_fail(const std::string& name, const std::string& reason)
    {
        m_tests_failed++;
        m_tests_run++;
        std::cout << "[ FAIL  ] " << name << " -- " << reason << std::endl;
    }

    // =========================================================================
    // FUNC-SCRATCH-001: Reset values
    // =========================================================================
    void test_reset_values()
    {
        const std::string TEST = "FUNC-SCRATCH-001: Reset values";
        report_test_start(TEST);
        pin_reset();
        bool ok = true;
        for (unsigned i = 0u; i < 8u && ok; ++i) {
            uint32_t v = scratch_read(i);
            if (v != 0u) {
                std::ostringstream oss;
                oss << "SCRATCH[" << i << "] = 0x" << std::hex << v
                    << " expected 0x0";
                report_test_fail(TEST, oss.str());
                ok = false;
            }
        }
        if (ok) report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-001b: a cold_rst_ni pulse clears programmed state
    //
    // Distinct from 001, which calls reset_all_registers() directly and so
    // passes for a model whose port is not wired to anything. This drives the
    // port, which is the half that matters: the ROM reads its warm-handler slot
    // out of this bank, and a bank that never re-clears hands it stale content.
    // =========================================================================
    void test_cold_reset_clears_state()
    {
        const std::string TEST = "FUNC-SCRATCH-001b: cold_rst_ni clears state";
        report_test_start(TEST);

        // Not a verdict code, which on scratch 0 would print a verdict line.
        for (unsigned i = 0u; i < 8u; ++i)
            scratch_write(i, 0x5A5AA5A5u);

        for (unsigned i = 0u; i < 8u; ++i) {
            if (scratch_read(i) != 0x5A5AA5A5u) {
                report_test_fail(TEST, "entries did not hold programmed values");
                return;
            }
        }

        // cold_reset_handler is an SC_METHOD on the port, so the scheduler has to
        // run for the edge to be delivered — hence real time either side.
        cold_rst_n.write(false);
        wait(1, sc_core::SC_NS);
        cold_rst_n.write(true);
        wait(1, sc_core::SC_NS);

        for (unsigned i = 0u; i < 8u; ++i) {
            uint32_t v = scratch_read(i);
            if (v != 0u) {
                std::ostringstream oss;
                oss << "SCRATCH[" << i << "] = 0x" << std::hex << v
                    << " after cold reset, expected 0x0";
                report_test_fail(TEST, oss.str());
                return;
            }
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-002: Basic read/write
    // =========================================================================
    void test_basic_rw()
    {
        const std::string TEST = "FUNC-SCRATCH-002: Basic read/write";
        report_test_start(TEST);
        pin_reset();
        // Patterns chosen to avoid triggering VP-ack side-effects
        const uint32_t patterns[8] = {
            0x00000001u,  // SCRATCH[0]: non-magic (magic = 0x12345678)
            0x00000000u,  // SCRATCH[1]: 0 → no status decoder stdout output
            0x00000002u,  // SCRATCH[2]: HEX16 opcode, buffered (no newline)
            0x55AA55AAu,  // SCRATCH[3]: plain storage
            0x00000816u,  // SCRATCH[4]: non-magic (magic = 0x815)
            0x12345678u,  // SCRATCH[5]: plain storage
            0x00000000u,  // SCRATCH[6]: 0 → no VP ack (magic = 0xA1E50006)
            0xFEDCBA98u,  // SCRATCH[7]: plain storage
        };
        bool ok = true;
        for (unsigned i = 0u; i < 8u && ok; ++i) {
            scratch_write(i, patterns[i]);
            uint32_t got = scratch_read(i);
            if (got != patterns[i]) {
                std::ostringstream oss;
                oss << "SCRATCH[" << i << "] wrote 0x" << std::hex << patterns[i]
                    << " read 0x" << got;
                report_test_fail(TEST, oss.str());
                ok = false;
            }
        }
        if (ok) report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-003: Register independence
    // =========================================================================
    void test_register_independence()
    {
        const std::string TEST = "FUNC-SCRATCH-003: Register independence";
        report_test_start(TEST);
        pin_reset();
        // Avoid magic values for idx 0, 4, 6
        const uint32_t vals[8] = {
            0x00000011u, 0x00000022u, 0x00000000u, 0x00000044u,
            0x00000055u, 0x00000066u, 0x00000000u, 0x00000088u,
        };
        for (unsigned i = 0u; i < 8u; ++i)
            scratch_write(i, vals[i]);
        bool ok = true;
        for (unsigned i = 0u; i < 8u && ok; ++i) {
            uint32_t got = scratch_read(i);
            if (got != vals[i]) {
                std::ostringstream oss;
                oss << "SCRATCH[" << i << "] = 0x" << std::hex << got
                    << " expected 0x" << vals[i];
                report_test_fail(TEST, oss.str());
                ok = false;
            }
        }
        if (ok) report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-004: Reserved bits (upper 32-bit half) are masked out
    //
    // SCRATCH_type write_bit_mask = 0xFFFFFFFF (covers bits[31:0] only).
    // Writing 0xFFFFFFFF to bytes [7:4] of a register slot should have no effect:
    //   handle_write(0xFFFFFFFF_00000000, 0xFFFFFFFF) → word_ref unchanged.
    // =========================================================================
    void test_reserved_bits()
    {
        const std::string TEST = "FUNC-SCRATCH-004: Reserved bits masked out";
        report_test_start(TEST);
        pin_reset();

        // Use SCRATCH[3] (no VP-ack callbacks, plain storage)
        scratch_write_upper(3u, 0xFFFFFFFFu);
        uint32_t upper = scratch_read_upper(3u);
        uint32_t lower = scratch_read(3u);

        if (upper != 0u || lower != 0u) {
            std::ostringstream oss;
            oss << "upper=0x" << std::hex << upper << " lower=0x" << lower
                << " expected both 0x0 (Reserved0 not writable)";
            report_test_fail(TEST, oss.str());
        } else {
            report_test_pass(TEST);
        }
    }

    // =========================================================================
    // FUNC-SCRATCH-005: VP ack — SCRATCH[0]=0x12345678 sets SCRATCH[1]=0x87654321
    // =========================================================================
    void test_vp_ack_scratch0()
    {
        const std::string TEST =
            "FUNC-SCRATCH-005: VP ack SCRATCH[0]=0x12345678 -> SCRATCH[1]=0x87654321";
        report_test_start(TEST);
        pin_reset();
        scratch_write(0u, 0x12345678u);
        uint32_t v = scratch_read(1u);
        if (v != 0x87654321u) {
            std::ostringstream oss;
            oss << "SCRATCH[1] = 0x" << std::hex << v << " expected 0x87654321";
            report_test_fail(TEST, oss.str());
        } else {
            report_test_pass(TEST);
        }
    }

    // =========================================================================
    // FUNC-SCRATCH-015: Terminal verdict on SCRATCH[0]
    //
    // The boot ROM and BL1 end a run by writing TEST_PASS_CODE or
    // TEST_FAIL_CODE to SCRATCH[0]; any other value, such as the remap
    // handshake, must not print a verdict.
    // =========================================================================
    void test_scratch0_verdict()
    {
        const std::string TEST = "FUNC-SCRATCH-015: Terminal verdict on SCRATCH[0]";
        report_test_start(TEST);

        const std::pair<uint32_t, std::string> cases[] = {
            {0xACAFACA1u, "\n[VP] SIMULATION OF THE TEST PASSED\n"},
            {0xDEADBEEFu, "\n[VP] SIMULATION OF THE TEST FAILED\n"},
            {0x12345678u, ""},
            {0xACAFACA0u, ""},
        };
        for (const auto& [code, expected] : cases) {
            pin_reset();
            std::string out;
            {
                cout_capture cap;
                scratch_write(0u, code);
                out = cap.release();
            }
            if (out != expected) {
                std::ostringstream oss;
                oss << "SCRATCH[0]=0x" << std::hex << code << " emitted "
                    << std::quoted(out) << ", expected " << std::quoted(expected);
                report_test_fail(TEST, oss.str());
                return;
            }
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-006: VP ack — SCRATCH[4]=0x815 sets SCRATCH[5]=0x777
    // =========================================================================
    void test_vp_ack_scratch4()
    {
        const std::string TEST =
            "FUNC-SCRATCH-006: VP ack SCRATCH[4]=0x815 -> SCRATCH[5]=0x777";
        report_test_start(TEST);
        pin_reset();
        scratch_write(4u, 0x00000815u);
        uint32_t v = scratch_read(5u);
        if (v != 0x00000777u) {
            std::ostringstream oss;
            oss << "SCRATCH[5] = 0x" << std::hex << v << " expected 0x777";
            report_test_fail(TEST, oss.str());
        } else {
            report_test_pass(TEST);
        }
    }

    // =========================================================================
    // FUNC-SCRATCH-007: VP ack — SCRATCH[6]=0xA1E50006 sets SCRATCH[7]=0x00100001
    // =========================================================================
    void test_vp_ack_scratch6()
    {
        const std::string TEST =
            "FUNC-SCRATCH-007: VP ack SCRATCH[6]=0xA1E50006 -> SCRATCH[7]=0x00100001";
        report_test_start(TEST);
        pin_reset();
        scratch_write(6u, 0xA1E50006u);
        uint32_t v = scratch_read(7u);
        if (v != 0x00100001u) {
            std::ostringstream oss;
            oss << "SCRATCH[7] = 0x" << std::hex << v << " expected 0x00100001";
            report_test_fail(TEST, oss.str());
        } else {
            report_test_pass(TEST);
        }
    }

    // =========================================================================
    // FUNC-SCRATCH-008: No spurious acks on non-magic writes
    // =========================================================================
    void test_no_spurious_ack()
    {
        const std::string TEST = "FUNC-SCRATCH-008: No spurious acks on non-magic writes";
        report_test_start(TEST);
        pin_reset();
        bool ok = true;

        // Non-magic to SCRATCH[0] (one off) — SCRATCH[1] must stay 0
        scratch_write(0u, 0x12345677u);
        uint32_t s1 = scratch_read(1u);
        if (s1 != 0u) {
            std::ostringstream oss;
            oss << "non-magic SCRATCH[0] set SCRATCH[1]=0x" << std::hex << s1;
            report_test_fail(TEST, oss.str());
            ok = false;
        }

        // Non-magic to SCRATCH[4] (one off) — SCRATCH[5] must stay 0
        if (ok) {
            scratch_write(4u, 0x00000816u);
            uint32_t s5 = scratch_read(5u);
            if (s5 != 0u) {
                std::ostringstream oss;
                oss << "non-magic SCRATCH[4] set SCRATCH[5]=0x" << std::hex << s5;
                report_test_fail(TEST, oss.str());
                ok = false;
            }
        }

        // Non-magic to SCRATCH[6] (one off) — SCRATCH[7] must stay 0
        if (ok) {
            scratch_write(6u, 0xA1E50007u);
            uint32_t s7 = scratch_read(7u);
            if (s7 != 0u) {
                std::ostringstream oss;
                oss << "non-magic SCRATCH[6] set SCRATCH[7]=0x" << std::hex << s7;
                report_test_fail(TEST, oss.str());
                ok = false;
            }
        }

        if (ok) report_test_pass(TEST);
    }

    void test_cold_reset()
    {
        const std::string TEST = "FUNC-SCRATCH-010: Cold reset domain";
        report_test_start(TEST);
        scratch_write(3u, 0xA5A55A5Au);
        wait(sc_core::SC_ZERO_TIME);
        if (scratch_read(3u) != 0xA5A55A5Au) {
            report_test_fail(TEST, "pre-reset value did not store");
            return;
        }

        // No warm-reset input exists by design, so ordinary time passage retains.
        wait(1, sc_core::SC_NS);
        if (scratch_read(3u) != 0xA5A55A5Au) {
            report_test_fail(TEST, "cold scratch did not retain without cold reset");
            return;
        }

        cold_rst_n.write(false);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        const bool cleared = scratch_read(3u) == 0u;
        cold_rst_n.write(true);
        wait(sc_core::SC_ZERO_TIME);
        if (!cleared)
            report_test_fail(TEST, "cold reset did not clear scratch storage");
        else
            report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-010: Virtual console decoder paths
    //
    // Exercises opcode branches and flush path in VirtConsoleDecoder:
    //   OP_ASCII with non-NUL text + '\n' flushes the emit lambda
    //   OP_DEC24 encodes a decimal value
    //   default opcode (3) is silently ignored
    //
    // Word encoding: bits[3:1] = opcode (toggle bit[0] masked out by decoder)
    //   OP_ASCII (0): payload at [15:8], [23:16], [31:24]; NUL terminates
    //   OP_HEX16 (1): 16-bit value at [23:8]
    //   OP_DEC24 (2): 24-bit value at [31:8]
    //   unknown  (3): (3 << 1) = 6 → ignored
    // =========================================================================
    void test_virt_console_decode()
    {
        const std::string TEST = "FUNC-SCRATCH-010: Virtual console decoder paths";
        report_test_start(TEST);
        pin_reset();

        // OP_ASCII: 'A' at [15:8], '\n' at [23:16], NUL at [31:24]
        // bits[3:1]=0 → opcode=0; word = 0x000A4100
        // → append_char('A') → cur_line_="A"
        // → append_char('\n') → emit_line("A") → "[SIM_OUT] - A\n" to stdout
        scratch_write(2u, 0x000A4100u);

        // OP_DEC24: bits[3:1]=2 → word bit2 set; 24-bit value=42 at [31:8]
        // 0x00002A04: opcode=(0x00002A04>>1)&7=(0x1502)&7=2; val=(0x2A04>>8)=0x2A=42
        // → decode_dec24 → append "42" to cur_line_
        scratch_write(2u, 0x00002A04u);

        // OP_ASCII '\n' to flush the buffered "42" from DEC24 above
        // 0x00000A00: opcode=0; byte[15:8]=0x0A='\n' → emit_line("42")
        scratch_write(2u, 0x00000A00u);

        // Default opcode (3): bits[3:1]=3 → (3<<1)=6 in word → ignored silently
        // 0x00000006: opcode=(6>>1)&7=3 → default: break
        scratch_write(2u, 0x00000006u);

        // No register state change from console writes — SCRATCH[2] just holds last value
        uint32_t v = scratch_read(2u);
        if (v != 0x00000006u) {
            std::ostringstream oss;
            oss << "SCRATCH[2] = 0x" << std::hex << v << " expected 0x00000006";
            report_test_fail(TEST, oss.str());
            return;
        }

        // The point of the console is the text it prints, which the checks
        // above cannot see at all: a decoder emitting nothing, the wrong
        // characters, or the same line twice would still leave 0x00000006 in
        // the register. Replay the same sequence with stdout captured.
        {
            pin_reset();
            cout_capture cap;
            scratch_write(2u, 0x000A4100u);   // 'A' then '\n' -> emits "A"
            scratch_write(2u, 0x00002A04u);   // DEC24 42, buffered
            scratch_write(2u, 0x00000A00u);   // '\n' -> emits "42"
            scratch_write(2u, 0x00000006u);   // unknown opcode -> emits nothing
            const std::string out = cap.release();

            if (out != "[SIM_OUT] - A\n[SIM_OUT] - 42\n") {
                std::ostringstream oss;
                oss << "console emitted " << std::quoted(out)
                    << ", expected \"[SIM_OUT] - A\\n[SIM_OUT] - 42\\n\"";
                report_test_fail(TEST, oss.str());
                return;
            }
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-011: Status decoder type and stage labels
    //
    // Exercises all type_label and stage_label branches in StatusDecoder
    // by writing status words with varying type and fw_id fields to SCRATCH[1].
    //
    // Word encoding: [31:24]=type, [23:16]=fw_id, [15:0]=value
    // =========================================================================
    void test_status_decoder_types()
    {
        const std::string TEST = "FUNC-SCRATCH-011: Status decoder type and stage labels";
        report_test_start(TEST);
        pin_reset();

        // All writes go to SCRATCH[1] and trigger the status decoder callback.
        // These just produce stdout output; the register stores the last value.
        // INFO (0x01) — baseline, also checks BL0 stage (fw_id=1)
        scratch_write(1u, (0x01u << 24) | (0x01u << 16) | 0x0001u);  // 0x01010001
        // WARN (0x08)
        scratch_write(1u, (0x08u << 24) | (0x01u << 16) | 0x0001u);  // 0x08010001
        // ERROR (0x0f)
        scratch_write(1u, (0x0fu << 24) | (0x01u << 16) | 0x0001u);  // 0x0f010001
        // INFO_EXT (0x81)
        scratch_write(1u, (0x81u << 24) | (0x01u << 16) | 0x0001u);  // 0x81010001
        // DEBUG (0x80) — firmware skips ring for DEBUG but decoder still fires here
        scratch_write(1u, (0x80u << 24) | (0x01u << 16) | 0x0001u);  // 0x80010001
        // BL1 stage (fw_id=2)
        scratch_write(1u, (0x01u << 24) | (0x02u << 16) | 0x0001u);  // 0x01020001
        // Unknown fw_id (3) → "ID3" label
        scratch_write(1u, (0x01u << 24) | (0x03u << 16) | 0x0001u);  // 0x01030001
        // Unknown type → "T0x07" label
        scratch_write(1u, (0x07u << 24) | (0x01u << 16) | 0x0001u);  // 0x07010001

        // Last value written should be readable back
        uint32_t v = scratch_read(1u);
        if (v != 0x07010001u) {
            std::ostringstream oss;
            oss << "SCRATCH[1] = 0x" << std::hex << v << " expected 0x07010001";
            report_test_fail(TEST, oss.str());
            return;
        }

        // The labels are the behaviour: every write above leaves the same
        // register value shape, so only the emitted line distinguishes INFO
        // from ERROR or BL0 from an unknown stage. Format is
        // "%-3s %-8s 0x%04x %s" over stage, type, value, name.
        {
            pin_reset();
            cout_capture cap;
            scratch_write(1u, (0x01u << 24) | (0x01u << 16) | 0x0001u);
            scratch_write(1u, (0x08u << 24) | (0x01u << 16) | 0x0001u);
            scratch_write(1u, (0x0fu << 24) | (0x01u << 16) | 0x0001u);
            scratch_write(1u, (0x01u << 24) | (0x03u << 16) | 0x0001u);
            scratch_write(1u, (0x07u << 24) | (0x01u << 16) | 0x0001u);
            const std::string out = cap.release();

            std::vector<std::string> lines;
            for (std::size_t b = 0; b < out.size();) {
                const std::size_t e = out.find('\n', b);
                if (e == std::string::npos) break;
                lines.push_back(out.substr(b, e - b));
                b = e + 1;
            }

            if (lines.size() != 5u) {
                std::ostringstream oss;
                oss << "status decoder emitted " << lines.size()
                    << " line(s), expected 5: " << std::quoted(out);
                report_test_fail(TEST, oss.str());
                return;
            }

            // Each write differs from the previous one only in the type or the
            // fw_id field, so checking the corresponding label is what proves
            // the decode rather than the storage.
            const std::pair<std::size_t, std::string> expect[] = {
                {0u, "INFO"}, {1u, "WARN"}, {2u, "ERROR"},
                {3u, "ID3"},                  // unknown fw_id -> synthesised stage
                {4u, "T0x07"},                // unknown type  -> synthesised label
            };
            for (const auto& [idx, token] : expect) {
                if (lines[idx].find(token) == std::string::npos) {
                    std::ostringstream oss;
                    oss << "line " << idx << " " << std::quoted(lines[idx])
                        << " does not contain " << std::quoted(token);
                    report_test_fail(TEST, oss.str());
                    return;
                }
            }

            // Value field is fixed-width lower-case hex; a %d or %x slip here
            // would still contain every label token checked above.
            if (lines[0].find("0x0001") == std::string::npos) {
                std::ostringstream oss;
                oss << "value not formatted as 0x0001 in " << std::quoted(lines[0]);
                report_test_fail(TEST, oss.str());
                return;
            }
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-012: HEX16 opcode through the model callback
    //
    // Complements FUNC-SCRATCH-010 (ASCII / DEC24 / unknown) by driving
    // VirtConsoleDecoder::decode_hex16 via the SCRATCH[2] post-write hook.
    // Word: bits[3:1]=1 → (1<<1)=2; 16-bit value 0xABCD at [23:8].
    // =========================================================================
    void test_virt_console_hex16()
    {
        const std::string TEST = "FUNC-SCRATCH-012: Virtual console HEX16 opcode";
        report_test_start(TEST);
        pin_reset();

        std::string out;
        {
            cout_capture cap;
            // 0x00ABCD02: opcode=(0x02>>1)&7=1 (HEX16); val=(0xABCD02>>8)=0xABCD
            scratch_write(2u, 0x00ABCD02u);
            // Flush the buffered "abcd" with an ASCII newline
            scratch_write(2u, 0x00000A00u);
            out = cap.release();
        }

        uint32_t v = scratch_read(2u);
        if (v != 0x00000A00u) {
            std::ostringstream oss;
            oss << "SCRATCH[2] = 0x" << std::hex << v << " expected 0x00000A00";
            report_test_fail(TEST, oss.str());
            return;
        }

        // Lower case and exactly four digits: the hex formatting is the whole
        // behaviour under test, and the register readback cannot see it.
        if (out != "[SIM_OUT] - abcd\n") {
            std::ostringstream oss;
            oss << "HEX16 emitted " << std::quoted(out)
                << ", expected \"[SIM_OUT] - abcd\\n\"";
            report_test_fail(TEST, oss.str());
            return;
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-013: Decoder isolation — uncovered header paths
    //
    // The decoders are header-only and designed to be unit-tested without
    // SystemC. Exercise on_bytes, flush, disabled early-outs, TSV parse
    // edge cases, and null emit callbacks that the TLM path never hits.
    // =========================================================================
    void test_decoder_isolation()
    {
        const std::string TEST = "FUNC-SCRATCH-013: Decoder isolation paths";
        report_test_start(TEST);
        bool ok = true;

        std::vector<std::string> lines;
        auto capture = [&](const std::string& s) { lines.push_back(s); };

        // --- VirtConsoleDecoder ---
        {
            virt_console::VirtConsoleDecoder vc(capture);

            // Disabled: on_word and on_bytes must be no-ops
            vc.set_enabled(false);
            if (vc.enabled()) {
                report_test_fail(TEST, "VirtConsoleDecoder::enabled() true after set_enabled(false)");
                ok = false;
            }
            if (ok) {
                vc.on_word(0x000A4100u);
                uint8_t payload[4] = {0x00, 0x41, 0x0A, 0x00};
                vc.on_bytes(payload, 4);
                if (!lines.empty()) {
                    report_test_fail(TEST, "disabled VirtConsoleDecoder emitted a line");
                    ok = false;
                }
            }

            if (ok) {
                vc.set_enabled(true);
                // on_bytes: null pointer and short write ignored
                vc.on_bytes(nullptr, 4);
                uint8_t short_wr[2] = {0x41, 0x0A};
                vc.on_bytes(short_wr, 2);

                // Valid HEX16 via on_bytes: word 0x00AB0C02 (opcode=1, val=0xAB0C)
                uint8_t hex_word[4] = {0x02, 0x0C, 0xAB, 0x00};
                vc.on_bytes(hex_word, 4);
                vc.flush();  // emit unterminated "ab0c"
                if (lines.empty() || lines.back() != "ab0c") {
                    report_test_fail(TEST, "HEX16 on_bytes+flush did not emit \"ab0c\"");
                    ok = false;
                }
            }

            if (ok) {
                // flush() on an empty buffer is a no-op
                const size_t before = lines.size();
                vc.flush();
                if (lines.size() != before) {
                    report_test_fail(TEST, "empty flush emitted a line");
                    ok = false;
                }
            }

            if (ok) {
                // Null emit: decode + flush must not crash
                virt_console::VirtConsoleDecoder silent;
                silent.on_word(0x00004100u);  // 'A', no newline
                silent.flush();
            }
        }

        // --- StatusDecoder ---
        if (ok) {
            lines.clear();
            sep_status_report::StatusDecoder st(capture);

            st.set_enabled(false);
            if (st.enabled()) {
                report_test_fail(TEST, "StatusDecoder::enabled() true after set_enabled(false)");
                ok = false;
            }
            if (ok) {
                st.on_word(0x01010001u);
                if (!lines.empty()) {
                    report_test_fail(TEST, "disabled StatusDecoder emitted a line");
                    ok = false;
                }
            }

            if (ok) {
                st.set_enabled(true);
                st.on_bytes(nullptr, 4);
                uint8_t short_wr[2] = {0x01, 0x00};
                st.on_bytes(short_wr, 2);

                // TSV + C-header + comments + malformed rows
                const std::string tsv =
                    "\n"
                    "// line comment\n"
                    "/* one-line block */\n"
                    "/*\n"
                    " * multi-line block\n"
                    " */\n"
                    "* stray star\n"
                    "ONLYONE\n"
                    "#define INCOMPLETE\n"
                    "TSV_NAME 0x0044\n"
                    "#define HDR_NAME 0x0055\n"
                    "BADHEX 0xZZ\n"
                    "TRAIL 0x12zz\n"
                    "\n";
                auto names = sep_status_report::StatusDecoder::parse_tsv_string(tsv);
                if (names.size() != 2 || names[0x0044] != "TSV_NAME" || names[0x0055] != "HDR_NAME") {
                    std::ostringstream oss;
                    oss << "parse_tsv_string expected 2 names, got " << names.size();
                    report_test_fail(TEST, oss.str());
                    ok = false;
                } else {
                    st.set_names(std::move(names));
                    if (st.name_count() != 2) {
                        report_test_fail(TEST, "name_count() mismatch after set_names");
                        ok = false;
                    }
                }
            }

            if (ok) {
                // Little-endian INFO/BL0/0x0044 via on_bytes
                uint8_t word[4] = {0x44, 0x00, 0x01, 0x01};
                st.on_bytes(word, 4);
                if (lines.empty() || lines.back().find("TSV_NAME") == std::string::npos) {
                    report_test_fail(TEST, "StatusDecoder on_bytes missed TSV name lookup");
                    ok = false;
                }
            }

            if (ok) {
                sep_status_report::StatusDecoder silent;
                silent.on_word(0x01010001u);  // null emit must not crash
            }
        }

        if (ok) report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-009: CCI parameter defaults are accessible and correct
    // =========================================================================
    void test_cci_param_defaults()
    {
        const std::string TEST = "FUNC-SCRATCH-009: CCI parameter defaults";
        report_test_start(TEST);
        bool ok = true;

        int v = dut.verbosity.get_param_value();
        if (v < 1) {
            std::ostringstream oss;
            oss << "verbosity=" << v << " expected >= 1";
            report_test_fail(TEST, oss.str());
            ok = false;
        }
        if (ok && !dut.sim_out_enable.get_param_value()) {
            report_test_fail(TEST, "sim_out_enable expected true by default");
            ok = false;
        }
        if (ok && !dut.sep_status_enable.get_param_value()) {
            report_test_fail(TEST, "sep_status_enable expected true by default");
            ok = false;
        }
        if (ok) report_test_pass(TEST);
    }

    // =========================================================================
    // COLD-004: both decoder enables preset false
    //
    // The instance is constructed with sim_out.enable and sep_status.enable
    // preset to false. Scratch storage must still read back, and neither
    // decoder may print.
    // =========================================================================
    void test_decoders_disabled()
    {
        const std::string TEST = "COLD-004: both decoders disabled";
        report_test_start(TEST);

        if (quiet.sim_out_enable.get_param_value() ||
            quiet.sep_status_enable.get_param_value()) {
            report_test_fail(TEST, "CCI presets did not disable both decoders");
            return;
        }

        quiet_rst_n.write(false);
        wait(1, sc_core::SC_NS);
        quiet_rst_n.write(true);
        wait(1, sc_core::SC_NS);

        cout_capture cap;
        // SCRATCH[2] is the virtual-console tap; SCRATCH[1] is the status tap.
        // Neither value is a VP-ack sentinel.
        const auto w_con = simtlm::write_word<uint32_t>(quiet_socket, 16, 0x00004102u);
        const auto w_sts = simtlm::write_word<uint32_t>(quiet_socket, 8, 0x01010001u);
        uint32_t con = 0;
        uint32_t sts = 0;
        const auto r_con = simtlm::read_word<uint32_t>(quiet_socket, 16, con);
        const auto r_sts = simtlm::read_word<uint32_t>(quiet_socket, 8, sts);
        const std::string emitted = cap.release();

        if (!w_con.ok() || !w_sts.ok() || !r_con.ok() || !r_sts.ok()) {
            report_test_fail(TEST, "scratch access failed with decoders disabled");
            return;
        }
        if (con != 0x00004102u || sts != 0x01010001u) {
            std::ostringstream oss;
            oss << "readback con=0x" << std::hex << con << " sts=0x" << sts;
            report_test_fail(TEST, oss.str());
            return;
        }
        if (!emitted.empty()) {
            report_test_fail(TEST, "decoder emitted: " + emitted);
            return;
        }
        report_test_pass(TEST);
    }

    // =========================================================================
    // FUNC-SCRATCH-012: StatusDecoder unit paths not reached via SCRATCH[1]
    //
    // Construction of the DUT only parse_tsv()'s the vendored C header, so
    // TSV rows, comments, malformed lines, on_bytes, disable, and a null
    // emitter never run. Drive those here against a standalone decoder.
    // =========================================================================
    void test_status_decoder_unit()
    {
        const std::string TEST = "FUNC-SCRATCH-012: StatusDecoder unit paths";
        report_test_start(TEST);
        bool ok = true;
        std::string reason;
        std::vector<std::string> lines;

        sep_status_report::StatusDecoder dec([&](const std::string& s) {
            lines.push_back(s);
        });

        // Known name + unknown name (SEP_MSG_UNKNOWN).
        dec.set_names({{0x0044, "SEP_MSG_BOOTROM_START"}});
        if (dec.name_count() != 1) {
            ok = false; reason += "name_count != 1; ";
        }
        dec.on_word((0x01u << 24) | (0x01u << 16) | 0x0044u);
        dec.on_word((0x01u << 24) | (0x01u << 16) | 0xFFFFu);
        if (lines.size() != 2 ||
            lines[0].find("SEP_MSG_BOOTROM_START") == std::string::npos ||
            lines[1].find("SEP_MSG_UNKNOWN") == std::string::npos) {
            ok = false; reason += "name lookup failed; ";
        }

        // Disabled: on_word / on_bytes must be silent.
        lines.clear();
        dec.set_enabled(false);
        if (dec.enabled()) {
            ok = false; reason += "enabled() true after set_enabled(false); ";
        }
        dec.on_word(0x01010001u);
        const uint8_t word_bytes[4] = {0x01, 0x00, 0x01, 0x01};
        dec.on_bytes(word_bytes, 4);
        if (!lines.empty()) {
            ok = false; reason += "emit while disabled; ";
        }

        // Re-enable and exercise on_bytes: valid 4-byte LE, short write, null ptr.
        dec.set_enabled(true);
        lines.clear();
        dec.on_bytes(word_bytes, 4);
        dec.on_bytes(word_bytes, 3);
        dec.on_bytes(nullptr, 4);
        if (lines.size() != 1) {
            ok = false; reason += "on_bytes accepted short/null or missed valid; ";
        }

        // Null emitter: decode still runs, just no callback.
        dec.set_emit(nullptr);
        dec.on_word(0x01010001u);

        // parse_tsv: TSV rows, #define rows, comments, blanks, malformed, hex.
        const char* tsv =
            "\n"
            "  \t  \n"
            "/* block comment start\n"
            " still in block\n"
            " end of block */\n"
            "/* one-line block */\n"
            "// line comment\n"
            "* stray star\n"
            "onlyone\n"
            "SEP_MSG_FOO 0x10\n"
            "SEP_MSG_BAR 0x11 extra\n"
            "#define SEP_MSG_BAZ 0x12\n"
            "#define INCOMPLETE\n"
            "SEP_MSG_BAD nothex\n"
            "SEP_MSG_PARTIAL 0x13xyz\n"
            "SEP_MSG_DUP 0x10\n";
        auto map = sep_status_report::StatusDecoder::parse_tsv_string(tsv);
        if (map.find(0x10) == map.end() || map[0x10] != "SEP_MSG_DUP") {
            ok = false; reason += "TSV last-value-wins failed; ";
        }
        if (map.find(0x11) == map.end() || map[0x11] != "SEP_MSG_BAR") {
            ok = false; reason += "TSV row missed; ";
        }
        if (map.find(0x12) == map.end() || map[0x12] != "SEP_MSG_BAZ") {
            ok = false; reason += "#define row missed; ";
        }
        if (map.find(0x13) != map.end()) {
            ok = false; reason += "partial hex token accepted; ";
        }

        if (ok)
            report_test_pass(TEST);
        else
            report_test_fail(TEST, reason);
    }

    // =========================================================================
    // FUNC-SCRATCH-013: VirtConsoleDecoder unit paths
    //
    // The TLM path only hits OP_ASCII and OP_DEC24. HEX16, on_bytes, flush,
    // disable, and a null emitter are covered here.
    // =========================================================================
    void test_virt_console_unit()
    {
        const std::string TEST = "FUNC-SCRATCH-013: VirtConsoleDecoder unit paths";
        report_test_start(TEST);
        bool ok = true;
        std::string reason;
        std::vector<std::string> lines;

        virt_console::VirtConsoleDecoder dec([&](const std::string& s) {
            lines.push_back(s);
        });

        // OP_HEX16: bits[3:1]=1 → word bit 1 set; value 0xABCD at [23:8].
        // 0x00ABCD02 → append "abcd"; then ASCII '\n' flushes.
        dec.on_word(0x00ABCD02u);
        dec.on_word(0x00000A00u);
        if (lines.size() != 1 || lines[0] != "abcd") {
            ok = false; reason += "HEX16 expected \"abcd\" got \"" +
                (lines.empty() ? std::string() : lines[0]) + "\"; ";
        }

        // flush() of an unterminated buffer.
        lines.clear();
        dec.on_word(0x00004100u);  // ASCII 'A', no newline
        dec.flush();
        if (lines.size() != 1 || lines[0] != "A") {
            ok = false; reason += "flush missed buffered ASCII; ";
        }
        dec.flush();  // empty buffer: no-op

        // Disabled + on_bytes guards.
        lines.clear();
        dec.set_enabled(false);
        if (dec.enabled()) {
            ok = false; reason += "enabled() true after disable; ";
        }
        dec.on_word(0x00000A00u);
        const uint8_t hex_bytes[4] = {0x02, 0xCD, 0xAB, 0x00};  // same HEX16 word
        dec.on_bytes(hex_bytes, 4);
        if (!lines.empty()) {
            ok = false; reason += "emit while disabled; ";
        }

        dec.set_enabled(true);
        lines.clear();
        dec.on_bytes(hex_bytes, 4);
        dec.on_bytes(hex_bytes, 2);
        dec.on_bytes(nullptr, 4);
        dec.on_word(0x00000A00u);  // flush "abcd"
        if (lines.size() != 1 || lines[0] != "abcd") {
            ok = false; reason += "on_bytes HEX16 failed; ";
        }

        // Null emitter: decode still runs.
        dec.set_emit(nullptr);
        dec.on_word(0x00000A00u);
        dec.flush();

        if (ok)
            report_test_pass(TEST);
        else
            report_test_fail(TEST, reason);
    }

    // =========================================================================
    // FUNC-SCRATCH-014: Malformed generic payloads
    //
    // SCRATCH[3] is the only slot with no ack or console side effect, so it is
    // the one slot the matrix can hammer without triggering unrelated model
    // behaviour. SCRATCH[5] is the witness: it sits at byte 40, clear of the
    // 24..33 range the widest and unaligned defects can reach.
    // =========================================================================
    void test_malformed_payloads()
    {
        const std::string name = "FUNC-SCRATCH-014: Malformed generic payloads";
        report_test_start(name);

        constexpr unsigned SLOT_BYTES   = 8;
        constexpr unsigned NUM_SLOTS    = 8;
        constexpr uint32_t WITNESS_VAL  = 0x5EED5EEDu;

        scratch_write(3, 0x11112222u);
        scratch_write(5, WITNESS_VAL);

        simtlm::target_geometry geo;
        geo.valid_address  = 3u * SLOT_BYTES;
        geo.word_bytes     = SLOT_BYTES;
        geo.aperture_bytes = NUM_SLOTS * SLOT_BYTES;

        for (simtlm::defect d : simtlm::all_defects()) {
            for (tlm::tlm_command cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
                const auto r = simtlm::probe_defect(initiator_socket, d, geo, cmd);
                if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                    std::ostringstream oss;
                    oss << simtlm::defect_name(d) << " ("
                        << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                        << ") left the payload INCOMPLETE";
                    report_test_fail(name, oss.str());
                    return;
                }
            }
        }

        const uint32_t witness = scratch_read(5);
        if (witness != WITNESS_VAL) {
            std::ostringstream oss;
            oss << "SCRATCH[5] corrupted: expected 0x" << std::hex << WITNESS_VAL
                << ", read 0x" << witness;
            report_test_fail(name, oss.str());
            return;
        }

        // The bank must still be usable after being fed bad payloads.
        scratch_write(3, 0x33334444u);
        const uint32_t after = scratch_read(3);
        if (after != 0x33334444u) {
            std::ostringstream oss;
            oss << "SCRATCH[3] unusable after malformed traffic: read 0x"
                << std::hex << after;
            report_test_fail(name, oss.str());
            return;
        }

        report_test_pass(name);
    }

    // =========================================================================
    // SC_THREAD entry point
    // =========================================================================
    void run_tests()
    {
        std::cout << "\n" << std::string(72, '=') << "\n"
                  << "SEP SCRATCH COLD TESTBENCH\n"
                  << std::string(72, '=') << std::endl;

        test_reset_values();
        test_cold_reset_clears_state();
        test_basic_rw();
        test_register_independence();
        test_reserved_bits();
        test_vp_ack_scratch0();
        test_scratch0_verdict();
        test_vp_ack_scratch4();
        test_vp_ack_scratch6();
        test_no_spurious_ack();
        test_cold_reset();
        test_virt_console_decode();
        test_virt_console_hex16();
        test_status_decoder_types();
        test_decoder_isolation();
        test_cci_param_defaults();
        test_decoders_disabled();
        test_status_decoder_unit();
        test_virt_console_unit();
        test_malformed_payloads();

        std::cout << "\n=== Summary: " << m_tests_passed << "/" << m_tests_run
                  << " passed";
        if (m_tests_failed > 0)
            std::cout << " [" << m_tests_failed << " FAILED]";
        std::cout << " ===" << std::endl;

        sc_core::sc_stop();
    }
};

// =============================================================================
// sc_main
// =============================================================================
int sc_main(int argc, char* argv[])
{
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);
    cci::cci_originator platform_cfg("platform_cfg");
    auto broker = cci::cci_get_global_broker(platform_cfg);
    broker.set_preset_cci_value("testbench.quiet.sim_out.enable",
                                cci::cci_value(false));
    broker.set_preset_cci_value("testbench.quiet.sep_status.enable",
                                cci::cci_value(false));
    sep_scratch_cold_testbench testbench("testbench");
    sc_core::sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(testbench.m_tests_failed > 0 ? 1 : 0);
    return 0;
}
