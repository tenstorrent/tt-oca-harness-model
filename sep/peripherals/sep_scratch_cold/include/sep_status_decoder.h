// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Pure (SystemC-free) decoder for the SEP bootcode "production" status path.
//
// report_status() (fw/sep/bootcode/include/errors.h) encodes a 32-bit status word:
//
//   [31:24] type   [23:16] firmware-id   [15:0] value
//
// and, for non-DEBUG types, pushes it into the SEP status ring buffer in SMC SRAM
// (fw/sep/bootcode/src/status_ring.c) that the SMC reads on silicon. The virtual
// platform observes each entry write and renders one human-readable line:
//
//   "<stage> <SEVERITY> 0x%04x <NAME>"   e.g. "BL0 INFO     0x0044 SEP_MSG_BOOTROM_START"
//
//   type  -> INFO (0x01) / WARN (0x08) / ERROR (0x0f) / INFO_EXT (0x81); other -> "T0x%02x"
//            (DEBUG 0x80 never reaches this path; firmware skips the ring for DEBUG)
//   fw_id -> BL0 (1) / BL1 (2); other -> "ID%u"
//   name  -> looked up in a value->name map loaded at run time from a TSV; a miss
//            renders "SEP_MSG_UNKNOWN" (the code is never dropped)
//
// The names map is loaded from the firmware's canonical meta/status/status_values.tsv
// at run time — there is NO compiled name table. Kept free of SystemC/CSML so the
// decode and TSV-parse logic are unit-testable in isolation.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <istream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace sep_status_report {

// Message types (fw/sep/bootcode/include/errors.h). DEBUG never reaches the ring.
enum StatusType : uint8_t {
    STATUS_TYPE_INFO     = 0x01,
    STATUS_TYPE_WARN     = 0x08,
    STATUS_TYPE_ERROR    = 0x0f,
    STATUS_TYPE_DEBUG    = 0x80,
    STATUS_TYPE_INFO_EXT = 0x81,
};

class StatusDecoder {
  public:
    using EmitFn = std::function<void(const std::string&)>;
    using NameMap = std::unordered_map<uint16_t, std::string>;

    explicit StatusDecoder(EmitFn emit = nullptr) : emit_(std::move(emit)) {}

    void set_emit(EmitFn emit) { emit_ = std::move(emit); }
    void set_enabled(bool en) { enabled_ = en; }
    bool enabled() const { return enabled_; }

    // Install the value->name lookup (e.g. parsed from the TSV).
    void set_names(NameMap names) { names_ = std::move(names); }
    std::size_t name_count() const { return names_.size(); }

    // Decode one packed 32-bit status word into a line and emit it.
    void on_word(uint32_t word) {
        if (!enabled_) {
            return;
        }
        const uint8_t  type  = static_cast<uint8_t>((word >> 24) & 0xFFu);
        const uint8_t  fw_id = static_cast<uint8_t>((word >> 16) & 0xFFu);
        const uint16_t value = static_cast<uint16_t>(word & 0xFFFFu);

        char buf[192];
        std::snprintf(buf, sizeof(buf), "%-3s %-8s 0x%04x %s",
                      stage_label(fw_id).c_str(),
                      type_label(type).c_str(),
                      static_cast<unsigned>(value),
                      name_for(value).c_str());
        emit_line(buf);
    }

    // Assemble a little-endian word from a bus-write payload, then decode it.
    // Sub-word / short writes (len < 4) and null pointers are ignored without crashing.
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

    // ---- Name-table parsing (pure; static so it is unit-testable without an instance) ----
    // Accepts two row shapes so the same parser handles both the firmware's canonical
    // TSV and a vendored C-header snapshot of the same data:
    //   TSV row:    NAME<whitespace>0xVALUE
    //   C-header:   #define<whitespace>NAME<whitespace>0xVALUE
    // /* */ block comments, // line comments, and blank lines are skipped; malformed
    // rows are skipped (not fatal). Last value wins on a duplicate name/value.
    static NameMap parse_tsv(std::istream& in) {
        NameMap map;
        std::string raw;
        bool in_block_comment = false;
        while (std::getline(in, raw)) {
            const std::string s = strip(raw);
            if (s.empty()) {
                continue;
            }
            if (in_block_comment) {
                if (s.find("*/") != std::string::npos) {
                    in_block_comment = false;
                }
                continue;
            }
            if (s.rfind("/*", 0) == 0) {                     // begins a block comment
                if (s.find("*/") == std::string::npos) {
                    in_block_comment = true;
                }
                continue;
            }
            if (s.rfind("//", 0) == 0 || s[0] == '*') {      // line comment / stray '*'
                continue;
            }
            std::istringstream ls(s);
            std::string tok1, tok2, tok3;
            if (!(ls >> tok1 >> tok2)) {                      // need at least two columns
                continue;
            }
            std::string name = tok1, val = tok2;
            if (tok1 == "#define") {                          // C-header row: shift by one token
                if (!(ls >> tok3)) {
                    continue;
                }
                name = tok2;
                val  = tok3;
            }
            uint32_t v = 0;
            if (!parse_hex(val, v)) {                        // value must be hex
                continue;
            }
            map[static_cast<uint16_t>(v & 0xFFFFu)] = name;
        }
        return map;
    }

    static NameMap parse_tsv_string(const std::string& text) {
        std::istringstream ss(text);
        return parse_tsv(ss);
    }

  private:
    static std::string type_label(uint8_t type) {
        switch (type) {
            case STATUS_TYPE_INFO:     return "INFO";
            case STATUS_TYPE_WARN:     return "WARN";
            case STATUS_TYPE_ERROR:    return "ERROR";
            case STATUS_TYPE_INFO_EXT: return "INFO_EXT";
            case STATUS_TYPE_DEBUG:    return "DEBUG";  // not expected on the ring path
            default: {
                char b[8];
                std::snprintf(b, sizeof(b), "T0x%02x", type);
                return b;
            }
        }
    }

    static std::string stage_label(uint8_t fw_id) {
        switch (fw_id) {
            case 1: return "BL0";
            case 2: return "BL1";
            default: {
                char b[12];
                std::snprintf(b, sizeof(b), "ID%u", static_cast<unsigned>(fw_id));
                return b;
            }
        }
    }

    std::string name_for(uint16_t value) const {
        const auto it = names_.find(value);
        return it != names_.end() ? it->second : std::string("SEP_MSG_UNKNOWN");
    }

    void emit_line(const std::string& s) {
        if (emit_) {
            emit_(s);
        }
    }

    static std::string strip(const std::string& s) {
        const size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) {
            return "";
        }
        const size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    // Parse a hex token (with or without "0x"); whole token must be consumed.
    static bool parse_hex(const std::string& tok, uint32_t& out) {
        if (tok.empty()) {
            return false;
        }
        try {
            size_t pos = 0;
            const unsigned long v = std::stoul(tok, &pos, 16);
            if (pos != tok.size()) {
                return false;
            }
            out = static_cast<uint32_t>(v);
            return true;
        } catch (...) {
            return false;
        }
    }

    EmitFn emit_;
    NameMap names_;
    bool enabled_ = true;
};

}  // namespace sep_status_report
