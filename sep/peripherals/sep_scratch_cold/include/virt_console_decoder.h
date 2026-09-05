// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Pure (SystemC-free) decoder for the SEP bootcode "virtual console" protocol.
//
// The bootcode `simput*` helpers (fw/sep/bootcode/include/rom_virt_console.h) emit
// status by writing a packed 32-bit word to SEP_SCRATCH_COLD_SCRATCH_2 (0x10802010):
//
//   [31:8] payload   [7:4] reserved   [3:1] opcode   [0] toggle
//
//   opcode 0 = ASCII : up to 3 bytes at [15:8],[23:16],[31:24], NUL-terminated
//   opcode 1 = HEX16 : 16-bit value in [23:8], rendered as 4 lowercase hex digits
//   opcode 2 = DEC24 : 24-bit value in [31:8], rendered as decimal
//
// Bit[0] is a duplicate-detection toggle the firmware flips when it re-sends an
// otherwise-identical word; it is NOT part of the opcode or payload and is masked
// out here. This decoder mirrors the cocotb reference decoder
// (dv/sep/tb/tb_uvm/cocotb_tests/common/sep_virt_console.py) so VP and RTL runs
// report the same messages.
//
// Kept free of SystemC/regmodel so the decode logic is unit-testable in isolation.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

namespace sep_virt_console {

// Opcode field = bits[3:1] of the packed word.
enum VConsoleOpcode : uint32_t {
    OP_ASCII = 0,
    OP_HEX16 = 1,
    OP_DEC24 = 2,
};

class VirtConsoleDecoder {
  public:
    using EmitFn = std::function<void(const std::string&)>;

    explicit VirtConsoleDecoder(EmitFn emit = nullptr) : emit_(std::move(emit)) {}

    void set_emit(EmitFn emit) { emit_ = std::move(emit); }
    void set_enabled(bool en) { enabled_ = en; }
    bool enabled() const { return enabled_; }

    // Decode one packed 32-bit word. Unknown opcodes are ignored (never abort).
    void on_word(uint32_t word) {
        if (!enabled_) {
            return;
        }
        const uint32_t op = (word >> 1) & 0x7u;  // bits[3:1]; toggle bit[0] masked out
        switch (op) {
            case OP_ASCII: decode_ascii(word); break;
            case OP_HEX16: decode_hex16(word); break;
            case OP_DEC24: decode_dec24(word); break;
            default: /* unknown/reserved opcode: no-op */ break;
        }
    }

    // Assemble a little-endian word from a bus-write payload and decode it.
    // Sub-word / short writes (len < 4) are ignored without crashing.
    void on_bytes(const uint8_t* data, unsigned len) {
        if (!enabled_ || data == nullptr || len < 4) {
            return;
        }
        const uint32_t word = static_cast<uint32_t>(data[0]) |
                              (static_cast<uint32_t>(data[1]) << 8) |
                              (static_cast<uint32_t>(data[2]) << 16) |
                              (static_cast<uint32_t>(data[3]) << 24);
        on_word(word);
    }

    // Emit any buffered, unterminated content. Call at end of simulation.
    void flush() {
        if (!cur_line_.empty()) {
            emit_line(cur_line_);
            cur_line_.clear();
        }
    }

  private:
    void append_char(char c) {
        if (c == '\n') {
            emit_line(cur_line_);  // one logical line per newline
            cur_line_.clear();
        } else {
            cur_line_ += c;
        }
    }

    void decode_ascii(uint32_t word) {
        // payload bytes at [15:8],[23:16],[31:24]; stop at the first NUL
        for (int shift = 8; shift <= 24; shift += 8) {
            const char c = static_cast<char>((word >> shift) & 0xFFu);
            if (c == '\0') {
                break;
            }
            append_char(c);
        }
    }

    void decode_hex16(uint32_t word) {
        const unsigned val = (word >> 8) & 0xFFFFu;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%04x", val);
        for (const char* p = buf; *p; ++p) {
            append_char(*p);
        }
    }

    void decode_dec24(uint32_t word) {
        const unsigned val = (word >> 8) & 0xFFFFFFu;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u", val);
        for (const char* p = buf; *p; ++p) {
            append_char(*p);
        }
    }

    void emit_line(const std::string& s) {
        if (emit_) {
            emit_(s);
        }
    }

    EmitFn emit_;
    std::string cur_line_;
    bool enabled_ = true;
};

}  // namespace sep_virt_console
