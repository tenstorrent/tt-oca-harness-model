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
 */

#include "sep_scratch_cold.h"
#include "sep_status_decoder.h"
#include "virt_console_decoder.h"
#include "reg_param.h"
#include "reg_logger.h"

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
    tlm_utils::simple_initiator_socket<sep_scratch_cold_testbench, 32> initiator_socket;
    RegLogger logger;

    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;

    explicit sep_scratch_cold_testbench(sc_core::sc_module_name n)
        : sc_module(n)
        , dut("dut")
        , initiator_socket("initiator_socket")
    {
        initiator_socket.bind(dut.target_socket);
        logger.setMaxVerbosity(dut.verbosity.get_param_value());
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
        SC_THREAD(run_tests);
    }

    // -------------------------------------------------------------------------
    // TLM helpers — 32-bit socket, byte-addressed
    // Each SCRATCH[n] occupies 8 bytes (64-bit register); lower 32 bits are r/w.
    // -------------------------------------------------------------------------
    void b_write(sc_dt::uint64 byte_addr, uint32_t value)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(byte_addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4u);
        trans.set_streaming_width(4u);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
    }

    uint32_t b_read(sc_dt::uint64 byte_addr)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        uint32_t data = 0u;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(byte_addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4u);
        trans.set_streaming_width(4u);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
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
    void report_test_start(const std::string& name)
    {
        std::cout << "\n[ RUN   ] " << name << std::endl;
    }

    void report_test_pass(const std::string& name)
    {
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
        dut.reset_all_registers();
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
    // FUNC-SCRATCH-002: Basic read/write
    // =========================================================================
    void test_basic_rw()
    {
        const std::string TEST = "FUNC-SCRATCH-002: Basic read/write";
        report_test_start(TEST);
        dut.reset_all_registers();
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
        dut.reset_all_registers();
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
        dut.reset_all_registers();

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
        dut.reset_all_registers();
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
    // FUNC-SCRATCH-006: VP ack — SCRATCH[4]=0x815 sets SCRATCH[5]=0x777
    // =========================================================================
    void test_vp_ack_scratch4()
    {
        const std::string TEST =
            "FUNC-SCRATCH-006: VP ack SCRATCH[4]=0x815 -> SCRATCH[5]=0x777";
        report_test_start(TEST);
        dut.reset_all_registers();
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
        dut.reset_all_registers();
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
        dut.reset_all_registers();
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
        dut.reset_all_registers();

        // OP_ASCII: 'A' at [15:8], '\n' at [23:16], NUL at [31:24]
        // bits[3:1]=0 → opcode=0; word = 0x000A4100
        // → append_char('A') → cur_line_="A"
        // → append_char('\n') → emit_line("A") → "[SIM_OUT] A\n" to stdout
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
        } else {
            report_test_pass(TEST);
        }
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
        dut.reset_all_registers();

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
        } else {
            report_test_pass(TEST);
        }
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

        sep_virt_console::VirtConsoleDecoder dec([&](const std::string& s) {
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
    // SC_THREAD entry point
    // =========================================================================
    void run_tests()
    {
        std::cout << "\n" << std::string(72, '=') << "\n"
                  << "SEP SCRATCH COLD TESTBENCH\n"
                  << std::string(72, '=') << std::endl;

        test_reset_values();
        test_basic_rw();
        test_register_independence();
        test_reserved_bits();
        test_vp_ack_scratch0();
        test_vp_ack_scratch4();
        test_vp_ack_scratch6();
        test_no_spurious_ack();
        test_virt_console_decode();
        test_status_decoder_types();
        test_cci_param_defaults();
        test_status_decoder_unit();
        test_virt_console_unit();

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
    sep_scratch_cold_testbench testbench("testbench");
    sc_core::sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(testbench.m_tests_failed > 0 ? 1 : 0);
    return 0;
}
